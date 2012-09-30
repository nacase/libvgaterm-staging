/*
 *  Copyright (C) 2002 Nate Case 
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 *  This file implements the VGA terminal Gtk+ widget, based on the VGA
 *  Text Mode widget.  It provides convienence methods to give a terminal-like
 *  API (for things like printing strings, while knowing to scroll the screen).
 *  This is similar to what some of the BIOS functions would implement.
 *
 *  Cursor positions in this widget are both 1-based and relative to the
 *  current window area (default entire area).  This differs from the VGAText
 *  widget, where they are 0-based and absolute.
 */

#include <gdk/gdk.h>
#include "vgaterm.h"
#include "scrollbuf.h"
#include "marshal.h"

static void vga_term_class_init		(VGATermClass * klass);
static void vga_term_init		(VGATerm *term);
static void vga_term_finalize		(GObject *gobject);
static void
vga_term_set_scroll_adjustments		(GtkWidget *widget,
					 GtkAdjustment *hadjustment,
					 GtkAdjustment *vadjustment);
static void
vga_term_set_vadjustment		(VGATerm *term,
					 GtkAdjustment *adjustment);
static gboolean
vga_term_scroll				(GtkWidget *widget,
					 GdkEventScroll *event);
static void
vga_term_emit_pending_signals		(VGATerm *term);


static void
vga_term_scrollbuf_add_lines(VGATerm *term, int start_y, int count);

/* Terminal private data */
#define VGA_TERM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VGA_TYPE_TERM, VGATermPrivate))

/* TODO: Move most member data inside here */
struct _VGATermPrivate {
	ScrollBuf *sbuf;
	int scroll_line;	/* scroll line visible at top line, or 0
				    for scrollback not shown */
	GtkAdjustment *adjustment;
	gboolean adjustment_changed_pending;
	gboolean adjustment_value_changed_pending;
};

G_DEFINE_TYPE(VGATerm, vga_term, VGA_TYPE_TEXT);

static void vga_term_class_init(VGATermClass * klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass * widget_class;

	widget_class = (GtkWidgetClass *) klass;

	g_type_class_add_private(klass, sizeof(VGATermPrivate));

	obj_class->finalize = vga_term_finalize;

	/* Hackish way of making a scrollable widget in Gtk+ 2.x */
	klass->set_scroll_adjustments =	vga_term_set_scroll_adjustments;
	widget_class->set_scroll_adjustments_signal =
		g_signal_new("set-scroll-adjustments",
			     G_TYPE_FROM_CLASS(klass),
			     G_SIGNAL_RUN_LAST,
			     G_STRUCT_OFFSET(VGATermClass,
					     set_scroll_adjustments),
			     NULL, NULL,
			     _vga_term_marshal_VOID__OBJECT_OBJECT,
			     G_TYPE_NONE, 2,
			     GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

	widget_class->scroll_event = vga_term_scroll;

	/* widget_class->size_request = vga_term_size_request; */
	/* same for size_allocate */
}

static void vga_term_init(VGATerm *term)
{
	VGATermPrivate *pvt;
	
	/* Initialize public fields */
	term->textattr = 0x07;
	term->win_top_left_x = 1;
	term->win_top_left_y = 1;
	term->win_bot_right_x = 80;
	term->win_bot_right_y = 25;

	/* Initialize private data */
	pvt = term->pvt = VGA_TERM_GET_PRIVATE(term);

	/* FIXME: Configurable scroll buffer size */
	pvt->sbuf = scrollbuf_new(VGA_TERM_DEFAULT_SCROLLBUF_BYTES,
				  VGA_TERM_DEFAULT_SCROLLBUF_LINES);
	pvt->scroll_line = 0;

	pvt->adjustment = NULL;
	vga_term_set_vadjustment(term, NULL);

}


GtkWidget *vga_term_new(void)
{
	return GTK_WIDGET(g_object_new(vga_term_get_type(), NULL));
}

static void vga_term_finalize(GObject *gobject)
{
	VGATerm *term = VGA_TERM(gobject);

	if (term->pvt->sbuf)
		scrollbuf_destroy(term->pvt->sbuf);
	
	/* Chain up to the parent class */
	G_OBJECT_CLASS (vga_term_parent_class)->finalize(gobject);
}

static void
vga_term_handle_scroll(VGATerm *term)
{
	long dy, adj;

	adj = round(gtk_adjustment_get_value(term->pvt->adjustment));
	printf("NAC: handle_scroll: adj = %ld\n", adj);
	// FIXME: We need to emit a text-scrolled signal here
	// or something? maybe even scroll ?
	//vga_term_set_scroll(term, adj);
}

static void
vga_term_emit_adjustment_changed(VGATerm *term)
{
	gdouble current;
	glong v, current_v;
	gboolean changed = FALSE;

	if (term->pvt->adjustment_changed_pending) {
		g_object_freeze_notify(G_OBJECT(term->pvt->adjustment));
		current = gtk_adjustment_get_lower(term->pvt->adjustment);
		v = 10;
		/* do some crap */
		gtk_adjustment_set_lower(term->pvt->adjustment, 0);
		gtk_adjustment_set_upper(term->pvt->adjustment, 100);
		changed = TRUE;

		g_object_thaw_notify(G_OBJECT(term->pvt->adjustment));

		term->pvt->adjustment_changed_pending = FALSE;	
	}

	if (term->pvt->adjustment_value_changed_pending) {
		/* Emit adjustment_value_changed */
		term->pvt->adjustment_value_changed_pending = FALSE;
		v = round(gtk_adjustment_get_value(term->pvt->adjustment));
		if (v != term->pvt->scroll_line) {
			/* Weird but necessary? */
			current_v = term->pvt->scroll_line;
			term->pvt->scroll_line = v;
			gtk_adjustment_set_value(term->pvt->adjustment, current_v);
		}
	}
}

static void
vga_term_set_vadjustment(VGATerm *term, GtkAdjustment *adjustment)
{
	printf("NAC: vga_term_set_vadjustment\n");
	if (adjustment != NULL && adjustment == term->pvt->adjustment)
		return;
	if (adjustment == NULL && term->pvt->adjustment != NULL)
		return;
	
	if (adjustment == NULL)
		adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0,0,0,0,0,0));
	else
		g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
	
	printf("NAC: vga_term_set_vadjustment: add reference to object\n");
	/* Add a reference to the new adjustment object */
	g_object_ref_sink(adjustment);
	printf("NAC: vga_term_set_vadjustment: get rid of old \n");
	/* Get rid of the old adjustment object */
	if (term->pvt->adjustment != NULL) {
		/* Disconnect our signal handlers from this object */
		g_signal_handlers_disconnect_by_func(term->pvt->adjustment,
						     vga_term_handle_scroll,
						     term);
		g_object_unref(term->pvt->adjustment);
	}

	/* Set the new adjustment object */
	term->pvt->adjustment = adjustment;

	printf("NAC: vga_term_set_vadjustment: connect signal\n");
	g_signal_connect_swapped(term->pvt->adjustment, "value-changed",
				 G_CALLBACK(vga_term_handle_scroll),
				 term);
	printf("NAC: vga_term_set_vadjustment: done\n");
}

/* Set the adjustment objects used by the terminal widget */
#if !GTK_CHECK_VERSION (2, 91, 2)
static void
vga_term_set_scroll_adjustments(GtkWidget *widget,
				GtkAdjustment *hadjustment G_GNUC_UNUSED,
				GtkAdjustment *vadjustment)
{
fprintf(stderr, "NAC: vga_term_set_scroll_adjustments(): enter\n");
	vga_term_set_vadjustment(VGA_TERM(widget), vadjustment);
fprintf(stderr, "NAC: vga_term_set_scroll_adjustments(): return\n");
}
#endif /* GTK 2.x */

/* Handle a scroll event */
static gboolean
vga_term_scroll (GtkWidget *widget, GdkEventScroll *event)
{
	GtkAdjustment *adj;
	VGATerm *term;
	gdouble v;

	term = VGA_TERM(widget);
	
	/*
	 * TODO: Support mouse-aware applications and map it to button
	 * press in app (see vte source for reference).
	 */
	
	adj = term->pvt->adjustment;
	v = MAX(1., ceil(gtk_adjustment_get_page_increment(adj) / 10.));
	switch (event->direction) {
	case GDK_SCROLL_UP:
		break;
	case GDK_SCROLL_DOWN:
		v = -v;
		break;
	default:
		return FALSE;
	}

	v += term->pvt->scroll_line;
	v = MAX(v, 0);
	printf("v = %6.2f\n", v);
	vga_term_set_scroll(term, v);
	//vga_term_set_scroll(term, term->pvt->scroll_line);

	return TRUE;
}

static void
vga_term_emit_pending_signals(VGATerm *term)
{
	GObject *object;
	
	object = G_OBJECT(term);

	g_object_freeze_notify(object);

	vga_term_emit_adjustment_changed(term);

	/* Here we could check for other conditions */

	g_object_thaw_notify(object);
}


/**
 * vga_term_process_char:
 * @widget: VGA Terminal widget
 * @c: Character to process
 *
 * Update display in memory for printing a character but don't actually
 * refresh the physical display.
 */
static
void __deprecated_vga_term_process_char(VGATerm *term, guchar c)
{
	static guchar lc = '\0';		/* Last character */
	gboolean cursor_vis;
	int x = -1, y = -1;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	switch (c)
	{
		case 10:
			x = vga_term_wherex(term);
			y = vga_term_wherey(term) + 1;
			if (lc != 13)
				x = 1;
			break;
		case 13:
			x = 1;
			y = vga_term_wherey(term);
			break;
		case 8:
			x = vga_term_wherex(term) - 1;
			y = vga_term_wherey(term);
			break;
		default:
			//buf = vga_get_video_buf(term);
			/*
			ofs = buf + (vga_cursor_y(term) *
					vga_get_cols(term) +
					vga_cursor_x(term)) * 2;
			*(ofs) = c;
			*(ofs + 1) = term->textattr;
			*/
			x = vga_cursor_x(VGA_TEXT(term));
			y = vga_cursor_y(VGA_TEXT(term));
			vga_put_char(VGA_TEXT(term), c, term->textattr, x, y);

			/* Go to next line? */
			if ((x+1) == term->win_bot_right_x)
			{
				x = 1;
				/* Need to scroll? */
				if ((y+1) == term->win_bot_right_y)
				{
					cursor_vis =
						vga_cursor_is_visible(VGA_TEXT(term));
					if (cursor_vis)
						vga_cursor_set_visible(VGA_TEXT(term),
								FALSE);
					vga_cursor_move(VGA_TEXT(term),
						term->win_top_left_x - 1,
						term->win_top_left_y - 1);
					vga_term_delline(term);
					x = term->win_top_left_x;
					y = term->win_bot_right_y;
					vga_cursor_move(VGA_TEXT(term), x-1, y-1);
					if (cursor_vis)
						vga_cursor_set_visible(VGA_TEXT(term),
								TRUE);
					/* Don't need to update cursor */
					x = -1;
					y = -1;
				}
				else
					y++;
			}
			else
			{
				x++;
				y = vga_term_wherey(term);
			}
	}
	lc = c;
	if (x != -1 && y != -1)
	{
		vga_cursor_move(VGA_TEXT(term), x - 1, y - 1);
		g_print("[x->%d, y->%d]", x-1, y-1);
	}
}

void vga_term_writec(VGATerm *term, guchar c)
{
	static guchar lc = '\0';		/* Last character */
	gboolean cursor_vis;
	int x = -1, y = -1, cx, cy;
	VGAText *vga;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));
	vga = VGA_TEXT(term);

	//g_print("%c", c);
	switch (c)
	{
		case 10:
			x = vga_term_wherex(term);
			y = vga_term_wherey(term) + 1;
			/* I Don't know about this.
			 * Can't some ansi files and boards depend on this
			 * behavior?  what if they send JUST a newline
			 * because that's what they really want?
			 *
			 * I had this problem with some .tfx files
			 * that depended on this behavior
			 *
			 * Decision: make this an option.
			 * Translate LF->CR+LF.  Off by default..
			 *
			 * Should be settable for directory entries
			 */
			if (lc != 13)
				x = 1;
			break;
		case 13:
			x = 1;
			y = vga_term_wherey(term);
			break;
		case 7:
			gdk_beep();
			break;
		case 8:
			x = MAX(vga_term_wherex(term) - 1, 1);
			y = vga_term_wherey(term);
			break;
		default:
			/*buf = vga_get_video_buf(term);
			
			ofs = buf + (vga_cursor_y(term) *
					vga_get_cols(term) +
					vga_cursor_x(term)) * 2;
			*(ofs) = c;
			*(ofs + 1) = term->textattr;
			*/
			x = vga_term_wherex(term);
			y = vga_term_wherey(term);
			cx = vga_cursor_x(vga);
			cy = vga_cursor_y(vga);
			/*
			if (c == '[')
				g_print("@<%d,%d>", cx, cy);
			*/
			vga_put_char(vga, c, term->textattr, cx, cy);

			/* Go to next line? */
			if (cx+1 == term->win_bot_right_x)
			{
				x = 1;
				y++;
			}
			else
				x++;
	}
	lc = c;
	/* Check if we need to scroll down */
	if (y > (term->win_bot_right_y - term->win_top_left_y + 1))
	{
		cursor_vis = vga_cursor_is_visible(vga);
		if (cursor_vis)
			vga_cursor_set_visible(vga, FALSE);
		vga_cursor_move(vga,	term->win_top_left_x - 1,
					term->win_top_left_y - 1);
		vga_term_delline(term);
		x = term->win_top_left_x;
		y = term->win_bot_right_y;
		vga_cursor_move(vga, x-1, y-1);
		if (cursor_vis)
			vga_cursor_set_visible(vga, TRUE);
		/* Don't need to update cursor */
		x = -1;
		y = -1;
	}
	else
	if (x != -1 && y != -1)
	{
		vga_term_gotoxy(term, x, y);
	}
}

gint vga_term_write(VGATerm *term, guchar * s)
{
	int i = 0;
	while (s[i] != '\0')
		vga_term_writec(term, s[i++]);
	return i;
}

gint vga_term_writeln(VGATerm *term, guchar * s)
{
	return vga_term_write(term, s) + vga_term_write(term, "\r\n");
}


int vga_term_print(VGATerm *term, const gchar * format, ...)
{
	va_list args;
	gchar * string;

	g_return_val_if_fail(format != NULL, 0);

	va_start(args, format);
	string = g_strdup_vprintf(format, args);
	va_end(args);

	vga_term_write(term, string);

	g_free(string);
	return strlen(string);
}


/* 
 * these input functions may be removed.  i'm thinking it will just
 * be handled best in the application level with key_press events
 */
gboolean vga_term_kbhit(VGATerm *term)
{
	return '\0';
}

guchar vga_term_readkey(VGATerm *term)
{
	return '\0';
}

void vga_term_window(VGATerm *term, int x1, int y1, int x2, int y2)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	g_assert(x1 > 0 && x1 <= x2);
	g_assert(y1 > 0 && y1 <= y2);
	g_assert(x2 <= vga_get_cols(VGA_TEXT(term)));
	g_assert(y2 <= vga_get_rows(VGA_TEXT(term)));
			
	term->win_top_left_x = x1;
	term->win_top_left_y = y1;
	term->win_bot_right_x = x2;
	term->win_bot_right_y = y2;

	vga_term_gotoxy(term, 1, 1);
}

void vga_term_gotoxy(VGATerm *term, int x, int y)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	/* Boundary coercions */
	x = MAX(1, x);
	y = MAX(1, y);
	x = MIN(term->win_bot_right_x - term->win_top_left_x + 1, x);
	y = MIN(term->win_bot_right_y - term->win_top_left_y + 1, y);

	/* Adjust for window offsets */	
	x += term->win_top_left_x - 1;
	y += term->win_top_left_y - 1;

	vga_cursor_move(VGA_TEXT(term), x - 1, y - 1);
}

int vga_term_wherex(VGATerm *term)
{
	g_return_val_if_fail(term != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(term), -1);

	return vga_cursor_x(VGA_TEXT(term)) - term->win_top_left_x + 2;
}


int vga_term_wherey(VGATerm *term)
{
	g_return_val_if_fail(term != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(term), -1);

	return vga_cursor_y(VGA_TEXT(term)) - term->win_top_left_y + 2;
}

int vga_term_cols(VGATerm *term)
{
	g_return_val_if_fail(term != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(term), -1);

	return term->win_bot_right_x - term->win_top_left_x + 1;
}

int vga_term_rows(VGATerm *term)
{
	g_return_val_if_fail(term != NULL, -1);
	g_return_val_if_fail(VGA_IS_TERM(term), -1);

	return term->win_bot_right_y - term->win_top_left_y + 1;
}

void vga_term_clrscr(VGATerm *term)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	/* If full clear screen, save current screen to scroll buffer */
	if (term->win_top_left_x == 1 &&
	    term->win_top_left_y == 1 &&
	    term->win_bot_right_x == vga_get_cols(VGA_TEXT(term)) &&
	    term->win_bot_right_y == vga_get_rows(VGA_TEXT(term))) {
	    printf("Saving all lines due to clrscr\n");
		vga_term_scrollbuf_add_lines(term, 1,
					     vga_get_rows(VGA_TEXT(term)));
	}

	vga_clear_area(VGA_TEXT(term), SETBG(0x00, GETBG(term->textattr)),
			term->win_top_left_x - 1,
			term->win_top_left_y - 1,
			term->win_bot_right_x - term->win_top_left_x + 1,
			term->win_bot_right_y - term->win_top_left_y + 1);

	vga_cursor_move(VGA_TEXT(term), term->win_top_left_x - 1,
				term->win_top_left_y - 1);

#if 0
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, rows, cols, win_rows, win_cols, y;

	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));
	term = VGA_TERM(term);

	rows = vga_get_rows(term);
	cols = vga_get_cols(term);
	
	vga_cursor_move(term, 0, 0);  // wrong??
	
	video_buf = vga_get_video_buf(term);
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	win_rows = term->win_bot_right_y - term->win_top_left_y + 1;
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	/* Special case optimization */
	if (win_cols == cols)
	{
		ofs = (term->win_top_left_y - 1) * cols * 2;
		memsetword(video_buf + ofs, *cellword, cols * rows); 
	}
	else
	{
		for (y = term->win_top_left_y - 1; y < win_rows; y++)
		{
			ofs = (y * cols + term->win_top_left_x - 1) * 2;
			memsetword(video_buf + ofs, *cellword, win_cols);
		}
	}
	vga_refresh_region(term, term->win_top_left_x, term->win_top_left_y,
					win_cols, win_rows);
#endif
}

/* clears down using 0x00 attr, NOT current textattr */
/* untested */
void vga_term_clrdown(VGATerm *term)
{
	int y;

	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	y = vga_cursor_y(VGA_TEXT(term));
	vga_clear_area(VGA_TEXT(term), 0x00,
			term->win_top_left_x - 1,
			y,
			term->win_bot_right_x - term->win_top_left_x + 1,
			term->win_bot_right_y - y);
}

/* clears up using 0x00 attr, NOT current textattr */
/* untested */
void vga_term_clrup(VGATerm *term)
{
	int y;

	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	y = vga_cursor_y(VGA_TEXT(term));
	vga_clear_area(VGA_TEXT(term), 0x00,
			term->win_top_left_x - 1,
			term->win_top_left_y - 1,
			term->win_bot_right_x - term->win_top_left_x + 1,
			y + 1);
}

void vga_term_clreol(VGATerm *term)
{
	VGAText *vga;
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));
	vga = VGA_TEXT(term);

	vga_clear_area(vga, SETBG(0x00, GETBG(term->textattr)),
			vga_cursor_x(vga), vga_cursor_y(vga),
			(term->win_bot_right_x - term->win_top_left_x + 1) -
				vga_term_wherex(term) + 1, 1);
			
#if 0
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, cols, win_cols, x, y, clrcols;

	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	cols = vga_get_cols(term);  // wrong!?
	
	vga_cursor_move(term, 0, 0);
	
	video_buf = vga_get_video_buf(term);
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	x = vga_cursor_x(term);
	y = vga_cursor_y(term);
	ofs = (y * cols + x) * 2;
	clrcols = win_cols - vga_term_wherex(term) + 1;
	memsetword(video_buf + ofs, *cellword, clrcols);
	vga_refresh_region(term, x, vga_cursor_y(term), clrcols, 1);
#endif
}

/**
 * vga_term_scroll_up:
 * @term: VGATerm to operate on
 * @top_row: Top row (relative to current window) of scroll region
 * @lines: Number of lines to scroll up
 *
 * Scroll the current window buffer up starting at @top_row for @lines
 * lines.
 */
void vga_term_scroll_up(VGATerm *term, int top_row, int lines)
{
	guchar * video_buf;
	//struct { guchar c; guchar attr; } cell;
	//gint16 * cellword;
	int ofs, cols, win_cols, y, start_y, end_y;
	VGAText *vga;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));
	vga = VGA_TEXT(term);
	cols = vga_get_cols(vga);

	video_buf = vga_get_video_buf(vga);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	// start_y = relative_to_absolute(top_row)
	start_y = term->win_top_left_y + top_row - 2;
	end_y = term->win_bot_right_y - lines;
	
	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		printf("NAC: add line because of scroll_up\n");
		vga_term_scrollbuf_add_lines(term, top_row, lines);
		ofs = start_y * cols * 2;
		memmove(video_buf + ofs, video_buf + ofs + cols*2*lines,
				cols*2*(term->win_bot_right_y - lines));
	}
	else
	{
		for (y = start_y; y < end_y; y++)
		{
			/* Source is line below y at column of window start */
			ofs = (y*cols + term->win_top_left_x - 1) * 2;
			/* Shift line up */
			memmove(video_buf + ofs, video_buf + ofs + cols*2,
					win_cols*2);
		}
	}
	
	/* Now clear the free'd up lines at the bottom */
	vga_clear_area(vga, SETBG(0x00, GETBG(term->textattr)),
			term->win_top_left_x - 1, end_y, win_cols, lines);
		
/*	
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = (end_y*cols + term->win_top_left_x-1) * 2;
	for (y = 0; y < lines; y++)
	{
		memsetword(video_buf + ofs, *cellword, win_cols);
		ofs += (2*cols);
	}
	*/

#if 1
	vga_mark_region_dirty(vga, term->win_top_left_x-1, term->win_top_left_y-1, win_cols,
				term->win_bot_right_y - term->win_top_left_y + 1);
#else
	/* FIXME: Use actual font height?  Or some other way of knowing
	 pixels per line */
	gdk_window_scroll(GTK_WIDGET(term)->window, 0, -lines * 16);
	/* gdk_window_scroll automatically invalidates the areas that
	   are cleared out from the scrolling, so we don't need to call
	   a refresh here */

	/* OK, turns out it automatically refreshes the last line
	properly... but not  second to last line? odd... let's work
	around that */
	vga_refresh_region(vga, 0, end_y-1, cols, lines);
#endif
}


void vga_term_scroll_down(VGATerm *term, int top_row, int lines)
{
	guchar * video_buf;
	//struct { guchar c; guchar attr; } cell;
	//gint16 * cellword;
	int ofs, cols, win_cols, y, start_y;
	VGAText *vga;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));
	vga = VGA_TEXT(term);

	cols = vga_get_cols(vga);
	
	video_buf = vga_get_video_buf(vga);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	// start_y = relative_to_absolute(top_row)
	start_y = term->win_top_left_y + top_row - 2;

	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		ofs = start_y * cols * 2;
		memmove(video_buf + ofs + cols*2*lines, video_buf + ofs,
				cols*2*(term->win_bot_right_y - start_y - 1));
	}
	else
	{
		/* Start from the bottom */
		for (y = term->win_bot_right_y - 1; y > start_y; y--)
		{
			/* Source is line above y at column of window start */
			ofs = ((y-1)*cols + term->win_top_left_x - 1) * 2;
			/* Shift line down */
			memmove(video_buf + ofs + win_cols*2, video_buf + ofs,
					win_cols*2);
		}
	}
	
	/* Now clear the gap lines */
	vga_clear_area(vga, SETBG(0x00, GETBG(term->textattr)),
		term->win_top_left_x - 1, start_y, win_cols, lines);
			
#if 0	
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = (start_y * cols + term->win_top_left_x - 1) * 2;
	for (y = 0; y < lines; y++)
	{
		memsetword(video_buf + ofs, *cellword, win_cols);
		ofs += (2*cols);
	}
#endif

#if 1
	vga_mark_region_dirty(vga, term->win_top_left_x, term->win_top_left_y, win_cols,
				term->win_bot_right_y - term->win_top_left_y + 1);
#else
	/* FIXME: Use actual font height?  Or some other way of knowing
	 pixels per line */
	gdk_window_scroll(GTK_WIDGET(term)->window, 0, lines * 16);
	/* gdk_window_scroll automatically invalidates the areas that
	   are cleared out from the scrolling, so we don't need to call
	   a refresh here */
#endif
	
}



/*
 * vga_term_insline:
 * @term: VGATerm widget to use
 *
 * From the current cursor's row, shift all lines from it and below down
 * one row.  The last row in the display is truncated, and the current
 * line becomes a blank line.
 **/
void vga_term_insline(VGATerm *term)
{
	vga_term_scroll_down(term, vga_term_wherey(term), 1);
#if 0
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, cols, win_cols, x, y, start_y;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	cols = vga_get_cols(term);
	
	vga_cursor_move(term, 0, 0);
	
	video_buf = vga_get_video_buf(term);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	start_y = vga_cursor_y(term);

	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		ofs = vga_cursor_y(term) * cols * 2;
		memmove(video_buf + ofs + cols*2, video_buf + ofs,
				cols*2*(term->win_bot_right_y - start_y - 1));
	}
	else
	{
		/* Start from the bottom */
		for (y = term->win_bot_right_y - 1; y > start_y; y--)
		{
			/* Source is line above y at column of window start */
			ofs = ((y-1)*cols + term->win_top_left_x - 1) * 2;
			/* Shift line down */
			memmove(video_buf + ofs + win_cols*2, video_buf + ofs,
					win_cols*2);
		}
	}
	
	/* Now clear the starting line */
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = (start_y * cols + term->win_top_left_x - 1) * 2;
	memsetword(video_buf + ofs, *cellword, win_cols);

	/* FIXME: Refresh region only */
	vga_refresh(term);
#endif
}


/*
 * vga_term_delline:
 * @term: VGATerm widget to use
 *
 * From the current cursor's row, shift all lines from it and below up
 * one row.  The current cursor's line becomes overwritten with the contents
 * of the row under it.  The last row in the display becomes a blank line.
 **/
void vga_term_delline(VGATerm *term)
{
#if 0
	/* NAC: Not needed, because vga_term_scroll_up() already
	 * adds the lines to scrollback. */
	/*
	 * If top line, assume we are scrolling and save line to
	 * scrollback buffer
	 */
	if (vga_term_wherey(term) == 1) {
		printf("NAC: add line because of delline\n");
		vga_term_scrollbuf_add_lines(term, 1, 1);
	}
#endif

	vga_term_scroll_up(term, vga_term_wherey(term), 1);

#if 0
	guchar * video_buf;
	struct { guchar c; guchar attr; } cell;
	gint16 * cellword;
	int ofs, cols, win_cols, x, y, start_y, end_y;
	
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	cols = vga_get_cols(term);
	
	vga_cursor_move(term, 0, 0);
	
	video_buf = vga_get_video_buf(term);
	win_cols = term->win_bot_right_x - term->win_top_left_x + 1;
	start_y = vga_cursor_y(term);
	end_y = term->win_bot_right_y - 1;

	/* 
	 * In the case where the window is as wide as the display, we
	 * can optimize the shifting by using a single memmove() call
	 */
	if (win_cols == cols)
	{
		ofs = vga_cursor_y(term) * cols * 2;
		memmove(video_buf + ofs, video_buf + ofs + cols*2,
				cols*2*(term->win_bot_right_y - start_y - 1));
	}
	else
	{
		/* Start from the current row */
		for (y = start_y; y < end_y; y++)
		{
			/* Source is line below y at column of window start */
			ofs = ((y+1)*cols + term->win_top_left_x - 1) * 2;
			/* Shift line up */
			memmove(video_buf + ofs, video_buf + ofs + cols*2,
					win_cols*2);
		}
	}
	
	/* Now clear the bottom line */
	cell.c = 0x00;
	cell.attr = SETBG(0x00, GETBG(term->textattr));
	cellword = (gint16 *) &cell;
	ofs = ((term->win_bot_right_y-1) * cols + term->win_top_left_x-1) * 2;
	memsetword(video_buf + ofs, *cellword, win_cols);

	/* FIXME: Refresh region only */
	vga_refresh(term);
#endif
}

void vga_term_set_attr(VGATerm *term, guchar textattr)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	term->textattr = textattr;
}

guchar vga_term_get_attr(VGATerm *term)
{
	g_assert(term != NULL);
	g_assert(VGA_IS_TERM(term));

	return term->textattr;
}

void vga_term_set_fg(VGATerm *term, guchar fg)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	if (fg > 15)
		fg = (fg & 0x0F) | 0x80;
	term->textattr = (term->textattr & 0x70) | fg;
}

void vga_term_set_bg(VGATerm *term, guchar bg)
{
	g_return_if_fail(term != NULL);
	g_return_if_fail(VGA_IS_TERM(term));

	term->textattr = (term->textattr & 0x8F) | ((bg & 0x07) << 4);
}

/*
 * Add the specified lines from the current video buffer into the
 * scrollback buffer.  'start_y' is a 1-based line number where
 * 1 is the top line.
 */
static void
vga_term_scrollbuf_add_lines(VGATerm *term, int start_y, int count)
{
	int i;
	int cols;
	long ofs;
	guchar * video_buf;

	if (term->pvt->sbuf == NULL)
		return;

	cols = vga_get_cols(VGA_TEXT(term));
	video_buf = vga_get_video_buf(VGA_TEXT(term));
	ofs = (start_y-1) * cols * 2;

	for (i = 0; i < count; i++) {
		scrollbuf_add_line(term->pvt->sbuf, video_buf + ofs, cols*2);
		ofs += (cols * 2);
	}
}

/*
 * @line: Scrollback history line number to show at top of screen.
 *        0 = scrollback disabled, 1 = one line visible at top, 
 *        2 = line two shown at top, line one shown below it, etc.
 */
void vga_term_set_scroll(VGATerm *term, int line)
{
	guchar *video_buf, *sec_buf;
	guchar *linebuf;
	int cols, rows;
	int buf_size, line_bytes;
	int start_row;
	int i;
	long ofs;
	int scroll_i;

	if (term->pvt->scroll_line == line)
		return;	/* Nothing to do */

	term->pvt->scroll_line = line;

	if (line == 0) {
		vga_show_secondary(VGA_TEXT(term), FALSE);
		vga_refresh(VGA_TEXT(term));
		return;
	}

	if (line > scrollbuf_line_count(term->pvt->sbuf))
		line = scrollbuf_line_count(term->pvt->sbuf);

	//scrollbuf_dump(term->pvt->sbuf);
#if 0
guchar c, lc;
printf("set scroll (%d)\n", line);
printf("max bytes = %d\n", term->pvt->sbuf->max_bytes);
lc = 255;
for (i = 0; i < term->pvt->sbuf->max_bytes; i++) {
	c = *(term->pvt->sbuf->buf->buf + i);
	if (lc == 0 && c != 0) {
		printf(".. 00 ");
	}
	if (c >= 32 && c <= 126) {
		printf("%2c ", c);
	} else if (c == 0) {
		if (lc != 0)
			printf("%02x ", c);
	} else {
		//printf("%02x ", c);
		printf("*");
	}
	lc = c;
}
	printf("\n");
#endif

	video_buf = vga_get_video_buf(VGA_TEXT(term));
	sec_buf = vga_get_sec_buf(VGA_TEXT(term));
	cols = vga_get_cols(VGA_TEXT(term));
	rows = vga_get_rows(VGA_TEXT(term));
	buf_size = vga_video_buf_size(VGA_TEXT(term));

	if (line < rows) {
		/* Show partial primary buffer, partial scrollback */
		memcpy(sec_buf + 2*cols*line, video_buf,
		       buf_size - (2*cols*line));
		start_row = line - 1;
		scroll_i = 0;
	} else {
		/* Scrollback buffer visible only */
		start_row = rows - 1;
		scroll_i = line - rows;
#if 0
		for (i = 0; i < rows; i++) {
			line = scrollbuf_get_line(term->pvt->sbuf, scroll_i,
						  &line_bytes);
			ofs = (rows-i-1) * cols*2;
			memcpy(sec_buf + ofs, line, MIN(cols*2, line_bytes));
			scroll_i++;
		}
#endif
	}

	/*
	 * Copy row by row, starting at @start_row (val typ 0..24),
	 * and working 'up' to the top row 0.
	 *
	 * @scroll_i is the index of the scrollback buffer line to start
	 * with, and is incremented with each row used.
	 */
	//printf("NAC: start_row=%d, scroll_i=%d\n", start_row, scroll_i);
	for (i = start_row; i >= 0; i--) {
		linebuf = scrollbuf_get_line(term->pvt->sbuf, scroll_i,
					  &line_bytes);
		if (linebuf == NULL)
			continue;
		ofs = i*cols*2;
		memcpy(sec_buf + ofs, linebuf, MIN(cols*2, line_bytes));
		scroll_i++;
	}

#if 0
printf("NAC: Dumping secondary screen (chars only)\n");
int j;
for (i = 0; i < rows; i++) {
printf("Line %i: ", i);
	for (j = 0; j < (cols*2); j += 2)
		printf("%c", (unsigned char *) *(sec_buf + (i*cols*2 + j)));
printf("\n");
}
#endif

	vga_show_secondary(VGA_TEXT(term), TRUE);
	vga_refresh(VGA_TEXT(term));
}
