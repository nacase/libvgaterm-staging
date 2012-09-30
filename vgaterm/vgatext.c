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
 *****************************************************************************
 * 
 *  This file implements the Gtk+ widget for the VGA Text Mode display
 *  emulator.  It provides a display (default 80x25 characters) with text
 *  rendered by VGA fonts, VGA palette capabilities, as well as other
 *  display details commonly used in DOS programs (such as toggling ICE
 *  color mode to get high-intensity backgrounds without blinking).
 *
 *  The interface for writing to this emulated display is by using the
 *  character cell manipulation methods, or by writing to the video buffer
 *  display region (similar to 0xB800 segment from DOS).  Writing to the
 *  video buffer directly requires that you call the appropriate refresh
 *  method before any drawing actually takes place.
 *  for performance reasons]
 *
 *  This is *NOT* a terminal widget.  See VGATerm for a terminal widget
 *  implemented using this (VGAText).
 *
 *  All XY coordinates are 0-based.
 *
 *  This widget does not do any key input handling.
 *
 *  Many ideas and some code taken from the VTE widget, so a lot of credit
 *  for this goes to Nalin Dahyabhai from Red Hat.
 *
 */

#include "vgatext.h"
#include <pthread.h>


/* FIXME: temp before release */
#ifdef ENABLE_DEBUG
#define VGA_DEBUG
#endif
#define VGA_DEBUG

#define PIXEL_TO_COL(x, vgafont)	((x) / (vgafont)->width)
#define PIXEL_TO_ROW(y, vgafont)	((y) / (vgafont)->height)

#define CURSOR_BLINK_PERIOD_MS	229
#define BLINK_PERIOD_MS		498

typedef struct _VGAScreen VGAScreen;

/* Widget private data */
struct _VGATextPrivate {
	/* int keypad? */
	int rows;
	int cols;

	/*
	 * We maintain two buffers, dubbed primary (video_buf) and
	 * secondary (sec_buf).
	 *
	 * The primary buffer (video_buf) is ALWAYS the target for
	 * any kind of write/output/manipulation operations.
	 *
	 * The secondary buffer is conditionally used only for rendering
	 * purposes.  The renderer can temporarily switch to displaying
	 * the contents of the secondary buffer instead of the primary
	 * buffer.  For example, this can be used to implement a scrollback
	 * feature, in which the primary buffer is still used for any
	 * screen manipulation that occurs while the scrollback buffer
	 * is being displayed.
	 *
	 * For now, it is expected that any manipulation of the secondary
	 * buffer is done manually in raw format.  It might be a better
	 * idea to just have two "equal" buffers, and each operation
	 * has an argument to indicate which buffer it should manipulate,
	 * or use some kind of context to determine that.  This is similar
	 * in concept to virtual consoles in Linux.
	 *
	 * We don't really want the concept of a widget-wide "current"
	 * buffer, because we need to support displaying one while
	 * modifying another.
	 */
	int video_buf_len;
	vga_charcell *video_buf;
	vga_charcell *sec_buf;
	gboolean render_sec_buf;

	guchar *line_buf;	/* Length == # of columns */
	cairo_glyph_t *glyphs;

	char *dirty_buf;	/* 1 or 0 for each cell in video_buf */
	gboolean *dirty_line_buf;	/* 1 or 0 for each line in video_buf */
				/* Yes, it's redundant with dirty_buf.. but
				 * only indicates line status for speed */
	VGAFont * font;
	VGAPalette * pal;
	gboolean icecolor;
	gboolean cursor_visible;
	int cursor_x;		/* 0-based */
	int cursor_y;

#ifdef USE_DEPRECATED_GDK
	GdkBitmap * glyphs;
	GdkGC * gc;
#endif
	cairo_surface_t *surface_buf;
	guchar fg, bg;	/* Local copy of gc text attribute state */
	
	gboolean cursor_blink_state;
	guint cursor_timeout_id;
	guint render_timeout_id;

	gboolean blink_state;
	guint blink_timeout_id;	/* -1 when no blinking chars on screen */

	pthread_t thread;
	gboolean render_enabled;
};

/* static function prototypes */
static void vga_paint_region(GtkWidget * widget,
			int top_left_x, int top_left_y,
			int cols, int rows);

GtkWidget * vga_text_new(void)
{
	return GTK_WIDGET(g_object_new(vga_get_type(), NULL));
}

static void
vga_invalidate_cells(VGAText * vga, glong col_start, gint col_count,
			glong row_start, gint row_count)
{
	GdkRectangle rect;
	GtkWidget * widget;
	
	g_return_if_fail(VGA_IS_VGATEXT(vga));
	widget = GTK_WIDGET(vga);
	if (!GTK_WIDGET_REALIZED(widget))
		return;

	/* Convert the col/row start and end to pixel values by multiplying
	 * by the size of a character cell. */
	rect.x = col_start * vga->pvt->font->width;
	rect.width = col_count * vga->pvt->font->width;
	rect.y = row_start * vga->pvt->font->height;
	rect.height = row_count * vga->pvt->font->height;

	gdk_window_invalidate_rect(widget->window, &rect, TRUE);
}

static void
vga_invalidate_all(VGAText *vga)
{
	vga_invalidate_cells(vga, 0, vga->cols, 0, vga->rows);
}

/* FIXME: This doesn't seem to be used by anything */
/* Scroll a rectangular region up or down by a fixed number of lines. */
static void
vga_scroll_region(VGAText *vga,
			   long row, long count, long delta)
{
	GtkWidget *widget;
	gboolean repaint = TRUE;

	if ((delta == 0) || (count == 0))
		return;

	/* We only do this if we're scrolling the entire window. */
	if (row == 0 && count == vga->rows)
	{
		widget = GTK_WIDGET(vga);
		gdk_window_scroll(widget->window, 0, delta * vga->pvt->font->height);
		repaint = FALSE;
	}

	if (repaint)
	{
		/* We have to repaint the entire area. */
		vga_invalidate_cells(vga, 0, vga->cols, row, count);
	}
}


/* Emit a "contents_changed" signal. */
static void
vga_emit_contents_changed(VGAText *vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `contents-changed'.\n");
#endif
	g_signal_emit_by_name(vga, "contents-changed");
}

/* Emit a "cursor_moved" signal. */
static void
vga_emit_cursor_moved(VGAText *vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `cursor-moved'.\n");
#endif
	g_signal_emit_by_name(vga, "cursor-moved");
}

/* Emit a "refresh-window" signal. */
static void
vga_emit_refresh_window(VGAText *vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `refresh-window'.\n");
#endif
	g_signal_emit_by_name(vga, "refresh-window");
}

/* Emit a "move-window" signal. */
static void
vga_emit_move_window(VGAText *vga)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "Emitting `move-window'.\n");
#endif
	g_signal_emit_by_name(vga, "move-window");
}

static void
vga_realize(GtkWidget *widget)
{
	GdkWindowAttr attributes;
	gint attributes_mask;
	//GdkCursor * cursor;
	VGAText *vga;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_realize()\n");
#endif
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));

	vga = VGA_TEXT(widget);

	/* Set the realized flag. */
	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	/* Main widget window */
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = 0;
	attributes.y = 0;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) |
				GDK_EXPOSURE_MASK |
				GDK_BUTTON_PRESS_MASK |
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK |
				GDK_BUTTON1_MOTION_MASK |
				GDK_KEY_PRESS_MASK |
				GDK_KEY_RELEASE_MASK;
	attributes_mask = GDK_WA_X |
			  GDK_WA_Y |
			  GDK_WA_VISUAL |
			  GDK_WA_COLORMAP;
	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget),
					&attributes,
					attributes_mask);
	gdk_window_move_resize(widget->window,
			       widget->allocation.x,
			       widget->allocation.y,
			       widget->allocation.width,
			       widget->allocation.height);
	gdk_window_set_user_data(widget->window, widget);

	gdk_window_show(widget->window);


#ifdef USE_DEPRECATED_GDK
	/* Initialize private data that depends on the window */
	if (vga->pvt->gc == NULL)
	{
		vga->pvt->glyphs = vga_font_get_bitmap(vga->pvt->font,
				widget->window);
		vga->pvt->gc = gdk_gc_new(widget->window);
		// not needed i guess?
		//gdk_gc_set_colormap(vga->pvt->gc, attributes.colormap);
		gdk_gc_set_rgb_fg_color(vga->pvt->gc,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->fg));
		gdk_gc_set_rgb_bg_color(vga->pvt->gc,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->bg));
		gdk_gc_set_stipple(vga->pvt->gc, vga->pvt->glyphs);
		gdk_gc_set_fill(vga->pvt->gc, GDK_OPAQUE_STIPPLED);
	}
#else
#if 0
	if (vga->pvt->cr == NULL) {
		vga->pvt->cr = gdk_cairo_create(widget->window);
		cairo_set_source_rgb(vga->pvt->cr, 0.5, 0.5, 0.0);
		cairo_set_operator(vga->pvt->cr, CAIRO_OPERATOR_SOURCE);
		cairo_paint(vga->pvt->cr);
		cairo_set_font_face(vga->pvt->cr, vga->pvt->font->face);
		cairo_set_font_size(vga->pvt->cr, 1.0);
		cairo_move_to(vga->pvt->cr, 10.0, 370.0);
		cairo_set_source_rgb(vga->pvt->cr, 1.0, 1.0, 1.0);
		cairo_show_text(vga->pvt->cr, "Testing");
#endif
#if 0
		cairo_select_font_face (vga->pvt->cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
	                               CAIRO_FONT_WEIGHT_BOLD);
		cairo_set_font_size(vga->pvt->cr, 12.0);
	}
#endif
#endif

	/* create a gdk window?  is that what we really want? */
	gtk_widget_grab_focus(widget);
}

/* The window is being destroyed. */
static void
vga_unrealize(GtkWidget *widget)
{
	VGAText * vga;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_unrealize()\n");
#endif

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	if (GTK_WIDGET_MAPPED(widget))
	{
		gtk_widget_unmap(widget);
	}

	/* Remove the GDK Window */
	if (widget->window != NULL)
	{
		gdk_window_destroy(widget->window);
		widget->window = NULL;
	}
	
	/* Mark that we no longer have a GDK window */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
}


static void
vga_paint_cursor(VGAText *vga, VGAFont * font, gboolean state,
			int x, int y)
{
	/*
	 * Don't show cursor when showing secondary buffer
	 * (e.g., scrolling back).
	 */
	if (vga->pvt->render_sec_buf)
		return;

#ifdef USE_DEPRECATED_GDK
	gdk_draw_rectangle(widget->window,
		state ? widget->style->white_gc : widget->style->black_gc,
		TRUE,	/* filled */
		x, y,
		font->width, font->height / 8);
#else
	cairo_t *cr;
	cr = gdk_cairo_create(GTK_WIDGET(vga)->window);

	/* Set clip region for speed */
	cairo_rectangle(cr, x, y, font->width, font->height / 8);
	cairo_clip(cr);

	if (state)
		gdk_cairo_set_source_color(cr,
			vga_palette_get_color(vga->pvt->pal, 15));
	else
		gdk_cairo_set_source_color(cr,
			vga_palette_get_color(vga->pvt->pal, 0));
	cairo_rectangle(cr, x, y, font->width, font->height / 8);
	cairo_fill(cr);
	cairo_destroy(cr);
#endif
}

/*
 * For regions of VGA buffer that are 'dirty', render them onto the
 * Cairo off-screen surface buffer.  This function is invoked
 * as a periodic timer.
 */
static gboolean
vga_render_buf(gpointer data)
{
	int y;
	GtkWidget *widget = (GtkWidget *) data;
	VGAText *vga = VGA_TEXT(widget);

	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(data)))
		return TRUE;
	
	vga = VGA_TEXT(data);

	for (y = 0; y < vga->pvt->rows; y++) {
		if (vga->pvt->dirty_line_buf[y]) {
			/* For now, just to get this working, we ignore the per-cell
			 * dirty buf and just handle the entire line.  Wasteful but
			 * easy */
			vga_render_region(vga, 0, y, vga->pvt->cols, 1);
			/* Mark as clean */
			vga->pvt->dirty_line_buf[y] = 0;
			/* Now paint that region.. not sure if we should
			 * be doing this here as it could slow things down */
			//vga_paint_region(widget, 0, y, vga->pvt->cols, 1);
			//
			/* 
			 * Invalidate the region to queue up an expose event
			 * to the widget.  This is basically the same as doing
			 * a gdk_window_invalidate_rect()
			 */
			gtk_widget_queue_draw_area(widget, 0,
					y * vga->pvt->font->height,
					vga->pvt->font->width * vga->pvt->cols,
					vga->pvt->font->height);
		}
	}

	/* Return TRUE to keep timer enabled */
	return TRUE;
}

static gboolean
vga_blink_cursor(gpointer data)
{
	VGAText * vga;

	if (!GTK_WIDGET_REALIZED(GTK_WIDGET(data)))
		return TRUE;
	
	vga = VGA_TEXT(data);

	/* Don't do anything if we're already how we want it */
	if (!vga->pvt->cursor_visible && !vga->pvt->cursor_blink_state)
		return TRUE;
	
	vga->pvt->cursor_blink_state = !vga->pvt->cursor_blink_state;
	vga_paint_cursor(vga, vga->pvt->font,
				vga->pvt->cursor_blink_state,
				vga->pvt->cursor_x * vga->pvt->font->width,
				(vga->pvt->cursor_y + 1) *
					vga->pvt->font->height -
					(vga->pvt->font->height / 8) );

	return TRUE;

	/*
	vga->pvt->cursor_blink_id = g_timeout_add(CURSOR_BLINK_PERIOD_MS,
							vga_blink_cursor,
							data);

	return FALSE;
	*/
	
}
		
static gboolean
vga_blink_char(gpointer data)
{
	int x, y;
	GtkWidget * widget;
	VGAText * vga;

	widget = GTK_WIDGET(data);
	if (!GTK_WIDGET_REALIZED(widget))
		return TRUE;

	vga = VGA_TEXT(data);
	
	if (vga->pvt->icecolor)
		return TRUE;
	
	vga->pvt->blink_state = !vga->pvt->blink_state;

	for (y = 0; y < vga->pvt->rows; y++)
	{
		for (x = 0; x < vga->pvt->cols; x++)
		{
			/* 
			 * If you encounter a blink bit, refresh it
			 * and the rest of the line, then move on to the
			 * next line
			 */
			if (GETBLINK(vga->pvt->video_buf[y*vga->pvt->cols + x].attr))
			{
				g_print("blink bit encountered on line %d\n", y);
				vga_mark_region_dirty(vga, x, y,
				//vga_render_region(widget, x, y,
							vga->pvt->cols - x,
							1);
				break;
			}
		}
	}

	return TRUE;
}


static
void vga_start_blink_timer(VGAText *vga)
{
	vga->pvt->blink_timeout_id = g_timeout_add(BLINK_PERIOD_MS,
			vga_blink_char, vga);
}

/*
 * vga_set_textattr:
 * @vga: VGAtext object
 * @textattr: VGA text attribute byte
 *
 * Set the VGAText's graphics context to use the text attribute given using
 * the current VGA palette.  May not actually result in a call to the
 * graphics server, since redundant calls may be optimized out.
 */
static void
vga_set_textattr(VGAText *vga, guchar textattr)
{
	guchar fg, bg;
	
	/* 
	 * Blink logic for text attributes
	 * -----------------------------------
	 * 
	 * has no blink bit: normal (fg = fg, bg = bg)
	 * has blink bit, icecolor: high intensity bg (bg = hi(bg))
	 * has blink bit, no icecolor, blink_state off: fg = bg [hide]
	 * has blink bit, no icecolor, blink_state on: normal (fg = fg)
	 */ 
	if (!GETBLINK(textattr))
	{
		fg = GETFG(textattr);
		bg = GETBG(textattr);
	}
	else if (vga->pvt->icecolor)
	{	/* High intensity background / iCEColor */
		fg = GETFG(textattr);
		bg = BRIGHT(GETBG(textattr));
	}
	else if (vga->pvt->blink_state)
	{	/* Blinking, but in on state so it appears normal */
		fg = GETFG(textattr);
		bg = GETBG(textattr);

		if (vga->pvt->blink_timeout_id == -1)
			vga_start_blink_timer(vga);
	}
	else
	{	/* Hide, blink off state */
		fg = GETBG(textattr);
		bg = GETBG(textattr);

		if (vga->pvt->blink_timeout_id == -1)
			vga_start_blink_timer(vga);
	}

	if (vga->pvt->fg != fg)
	{
#ifdef USE_DEPRECATED_GDK
		gdk_gc_set_rgb_fg_color(vga->pvt->gc,
			vga_palette_get_color(vga->pvt->pal, fg));
#else
		/* Nothing we can really do like this */
#endif
		vga->pvt->fg = fg;
	}
	if (vga->pvt->bg != bg)
	{
#ifdef USE_DEPRECATED_GDK
		gdk_gc_set_rgb_bg_color(vga->pvt->gc,
			vga_palette_get_color(vga->pvt->pal, bg));
#else
#endif
		vga->pvt->bg = bg;
	}
}

/* Deprecated?  Depend strictly on refresh?   This won't work for blinking
 * and friends
 * x and y are in pixel units */
static void
vga_paint_charcell(GtkWidget *da, VGAText *vga,
				vga_charcell cell, int x, int y)
{
#ifdef USE_DEPRECATED_GDK
	vga_set_textattr(vga, cell.attr);
	gdk_gc_set_ts_origin(vga->pvt->gc, x, y-(16 * cell.c));
	gdk_draw_rectangle(da->window, vga->pvt->gc, TRUE, x, y,
				vga->pvt->font->width, vga->pvt->font->height);
#else
	printf("vga_paint_charcel() called -- why?\n");
#if 0
	char text[2];
	vga_set_textattr(vga, cell.attr);
	gdk_cairo_set_source_color(vga->pvt->cr,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->bg));
	cairo_move_to(vga->pvt->cr, x, y);
	cairo_rectangle(vga->pvt->cr, x, y,
			vga->pvt->font->width, vga->pvt->font->height);
	cairo_fill(vga->pvt->cr);
	gdk_cairo_set_source_color(vga->pvt->cr,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->fg));
	/* FIXME: Don't use toy text API.  Use cairo_show_glyphs() instead */
	text[0] = cell.c;
	text[1] = '\0';
	cairo_show_text(vga->pvt->cr, text);
#endif
#endif
}

/* FIXME: TEMP PROTOTYPE FOR STATIC FUNCTION */
guchar *
vga_font_get_glyph_data(VGAFont *font, int glyph);


/*
 * Paint a block of characters all in the same attribute.
 * You should have already determined that these characters have the
 * same attribute before calling this.
 */
static void vga_block_paint(VGAText *vga, cairo_t *cr,
			    int col, int row, int chars)
{
	int x, y;
	int i, ci;
	vga_charcell *cell;
	vga_charcell *video_buf;

//printf("vga_block_paint(col=%d, row=%d, chars=%d)\n", col, row, chars);
	if (chars == 0)
		return;

	x = col * vga->pvt->font->width;
	y = row * vga->pvt->font->height;

	cairo_move_to(cr, x, y);

	if (vga->pvt->render_sec_buf)
		video_buf = vga->pvt->sec_buf;
	else
		video_buf = vga->pvt->video_buf;

#if 1
	/* Draw background rectangle */
	gdk_cairo_set_source_color(cr,
		vga_palette_get_color(vga->pvt->pal, vga->pvt->bg));
	cairo_rectangle(cr, x, y, vga->pvt->font->width * chars,
			vga->pvt->font->height);
	cairo_fill(cr);
#endif

	/* Draw characters */
	cairo_move_to(cr, x, y);
	gdk_cairo_set_source_color(cr,
		vga_palette_get_color(vga->pvt->pal, vga->pvt->fg));
	/* Build up NULL-terminated line_buf string for cairo_show_text() */
	ci = 0;
	for (i = 0; i < chars; i++) {
		cell = &(video_buf[row * 80 + col + i]);
		vga->pvt->glyphs[i].index = cell->c;
		vga->pvt->glyphs[i].x = x + (vga->pvt->font->width * i);
		vga->pvt->glyphs[i].y = y;
#if 1
		if (cell->c < 128) {
			vga->pvt->line_buf[ci++] = (cell->c == '\0' ? ' ' : cell->c);
		} else {
			/* 
			 * We have to encode our extended ASCII characters
			 * into the Unicode PUA so that Cairo doesn't think
			 * they are real UTF-8 bytes.  Then, in the vgafont
			 * user font, we translate PUA area glyphs back
			 * into the regular extended ASCII glyphs
			 */

			/* IBM 8-bit extended ASCII, use Unicode PUA (U+E000) */
			/* 
			 * Encoded in UTF-8 as:
			 *     1110xxxx 10xxxxxx 10xxxxxx
			 *     ^^^ ^^^^   ^^^^^^   ^^^^^^
			 *      |    |       |        |
			 *      |     \______|________|
			 *      |            |
			 *      |        Remaining bits concatenated to
			 *      |        form Unicode code point value
			 *       \
			 *         "3 bytes in sequence"
			 *
			 * So for U+E000 we use:
			 *    11101110 100000xx 10xxxxxx
			 */
			vga->pvt->line_buf[ci++] = 0xee;
			vga->pvt->line_buf[ci++] = 0x80 | (cell->c >> 6);
			vga->pvt->line_buf[ci++] = 0x80 | (cell->c & 0x3f);
			//printf("extended char: 0x%02x\n", cell->c);
		}
#endif
	}
	vga->pvt->line_buf[ci] = '\0';
	//printf("%s\n", vga->pvt->line_buf);

	//cairo_show_text(cr, (const char *) vga->pvt->line_buf);
	cairo_show_glyphs(cr, vga->pvt->glyphs, chars);

	//printf("show_text(x,y=%d,%d col,row=%d,%d,fg=%d,bg=%d): %s\n", x, y, col, row, vga->pvt->fg, vga->pvt->bg, vga->pvt->line_buf);
}

/*
 * vga_render_area:
 * @vga: VGAText structure pointer
 * @area: Area to refresh
 *
 * For the given rectangular area, render the contents of the VGA buffer
 * onto the surface buffer (NOT on-screen).
 */
static void
vga_render_area(VGAText *vga, GdkRectangle * area)
{
	int x, y, x2, y2;
	int char_x, char_y, x_drawn, y_drawn, row, col;
	x2 = area->x + area->width;	/* Last column in area + 1 */
	y2 = area->y + area->height;	/* Last row in area + 1 */
	x2 = MIN(x2, vga->pvt->font->width * vga->pvt->cols);
	y2 = MIN(y2, vga->pvt->font->height * vga->pvt->rows);
	y = area->y;
	vga_charcell * cell;
	char text[2];
	cairo_t *cr;
	guchar attr;
	int cols_sameattr;
	int col_topaint;
	int num_cols;
	int last_col;
	vga_charcell *video_buf;

//printf("vga_render_area(): x,y = (%d,%d), width=%d, height=%d\n", area->x, area->y, area->width, area->height);
	/* We must create/destroy the context in each expose event */
	//cr = gdk_cairo_create(da->window);
	cr = cairo_create(vga->pvt->surface_buf);
#if 1
	/* Set clip region for speed */
	cairo_rectangle(cr, area->x, area->y, area->width, area->height);
	cairo_clip(cr);
#endif

	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	//cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
#if 1
	cairo_set_font_face(cr, vga->pvt->font->face);
	cairo_set_font_size(cr, 1.0);
#endif

	if (vga->pvt->render_sec_buf)
		video_buf = vga->pvt->sec_buf;
	else
		video_buf = vga->pvt->video_buf;

#define NEW_WAY
#ifdef NEW_WAY
	while (y < y2) {
	//printf("while loop: y (%d) < y2 (%d)\n", y, y2);
		row = PIXEL_TO_ROW(y, vga->pvt->font);
		char_y = row * vga->pvt->font->height;
		y_drawn = (char_y + vga->pvt->font->height) - y;

		col = PIXEL_TO_COL(area->x, vga->pvt->font);
		num_cols = PIXEL_TO_COL(x2-1, vga->pvt->font) - col + 1;
		cols_sameattr = 0;
		x = col * vga->pvt->font->width;
		col_topaint = col;
		cell = &(video_buf[row * 80 + col]);
		attr = cell->attr;
		last_col = col + num_cols - 1;
		//printf("col_topaint = %d, last_col = %d, num_cols=%d\n", col_topaint, last_col, num_cols);
		while (col <= last_col) {
			cell = &(video_buf[row * 80 + col]);
			//printf("CHAR: '%c', cell attr = 0x%02x, old attr was 0x%02x\n", cell->c, cell->attr, attr);
			if (cell->attr == attr) {
				cols_sameattr++;
			} else {
				vga_set_textattr(vga, attr);
				//printf("vga_block_paint() - mid line, cols_sameattr=%d\n", cols_sameattr);
				vga_block_paint(vga, cr, col_topaint,
						row, cols_sameattr);
				attr = cell->attr;
				col_topaint += cols_sameattr;
				cols_sameattr = 1;
			}
			col++;
		}
		/* Paint last chunk of row */
		vga_set_textattr(vga, cell->attr);
		//printf("vga_block_paint() - last chunk, col_to_paint=%d, cols=%d\n", col_topaint, last_col-col_topaint);
		vga_block_paint(vga, cr, col_topaint, row,
				last_col - col_topaint + 1);
		y += vga->pvt->font->height;
	}
#else	/* !NEW_WAY */
	while (y < y2)
	{
		row = PIXEL_TO_ROW(y, vga->pvt->font);
		char_y = row * vga->pvt->font->height;
		y_drawn = (char_y + vga->pvt->font->height) - y;

		x = area->x;
		while (x < x2)
		{
		/*
		 * x       : pixel position to start drawing at (top left)
		 * y       : pixel position to start drawing at (top left)
		 * row     : row of character (usually 0-24)
		 * col     : column of character (usually 0-79)
		 * char_x  : pixel coordinate of beginning of whole character
		 * char_y  : pixel coordinate of beginning of whole character
		 * x_drawn : number of columns to draw of character
		 * y_drawn : number of rows to draw of character
		 */
			col = PIXEL_TO_COL(x, vga->pvt->font);
			char_x = col * vga->pvt->font->width;
			x_drawn = (char_x + vga->pvt->font->width) - x;
		
			cell = &(video_buf[row * 80 + col]);
	
#ifdef USE_DEPRECATED_GDK
			vga_set_textattr(vga, cell->attr);
			gdk_gc_set_ts_origin(vga->pvt->gc, char_x,
					char_y-(16 * cell->c));
			gdk_draw_rectangle(da->window, vga->pvt->gc, TRUE, x, y,
					x_drawn, y_drawn);
#else

/* NEW WAY, TESTING TO SEE HOW MUCH CAIRO FONTS ARE SLOWING ME DOWN */
/* SPeed seems similar... although things aren't getting wiped now */
#if 1
	guchar *data; int row, col, i, j;
	vga_set_textattr(vga, cell->attr);

	/* Background */
	//printf("Char at (%d, %d)\n", x, y);
	cairo_translate(cr, x, y);
	gdk_cairo_set_source_color(cr,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->bg));
	cairo_rectangle(cr, 0, 0,
			vga->pvt->font->width, vga->pvt->font->height);
	cairo_fill(cr);

	/* Foreground character */
	data = vga_font_get_glyph_data(vga->pvt->font, cell->c);
	gdk_cairo_set_source_color(cr,
			vga_palette_get_color(vga->pvt->pal, vga->pvt->fg));
	for (i = 0; i < vga->pvt->font->bytes_per_glyph; i++) {
		row = i;
		for (j = 0; j < 8; j++) {
			if (data[i] & (1 << j)) {
				col = 7 - j;
				cairo_rectangle(cr, col * 1.0, row,
						1.0, 1.0);
				cairo_fill(cr);
			}
		}
	}
#endif

#endif
			x += x_drawn;
		}
		y += y_drawn;
	}
#endif	/* NEW_WAY */
	cairo_destroy(cr);
}

/* Draw part of the widget by blitting surface buffer to window */
static void
vga_paint(GtkWidget *widget, GdkRectangle *area)
{
	VGAText * vga;

	/* Sanity checks */
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	g_return_if_fail(area != NULL);
	vga = VGA_TEXT(widget);
	if (!GTK_WIDGET_DRAWABLE(widget))
	{
		fprintf(stderr, "vga_paint(): widget not drawable!\n");
		return;
	}

printf("vga_paint(): area x,y = %d,%d, width=%d, height=%d\n", area->x, area->y, area->width, area->height);
#if 0
	vga_render_area(widget, vga, area);
#else
	cairo_t *cr;
	cr = gdk_cairo_create(widget->window);
	/* Set clip region for speed */
	cairo_rectangle(cr, area->x, area->y, area->width, area->height);
	cairo_clip(cr);
	cairo_set_source_surface(cr, vga->pvt->surface_buf, 0, 0);
	cairo_paint(cr);
	cairo_destroy(cr);
#endif
	
	/* 
	 * FIXME: The algorithm seems to work great.  I watch the debug output
	 * as it runs trying various things and it always calculates the
	 * right starting/stopping row/col.  However, still sometimes things
	 * are not drawn, but I'm lead to believe right now that the problem
	 * lies elsewhere.
	 * UPDATE: Correct.  We now use vga_render_area() which is a
	 * pixel-level based refresh rather than character level.  It
	 * works fine.  Commenting out approach below.
	 *
	 */
#if 0
	row_start = area->y / vga->pvt->font->height; /* verified */
	
	num_rows = area->height / vga->pvt->font->height + 1;
	row_stop = MIN(row_start + num_rows, vga->pvt->rows);
	
	num_cols = area->width / vga->pvt->font->width + 1;
	col_start = area->x / vga->pvt->font->width;
	col_stop = MIN(col_start + num_cols, vga->pvt->cols);
#ifdef VGA_DEBUG
	fprintf(stderr, "area->y = %d, area->height = %d\n", area->y, area->height);
	fprintf(stderr, "area->x = %d, area->width = %d\n", area->x, area->width);
	fprintf(stderr, "row_start = %d, row_stop = %d\n", row_start, row_stop);
	fprintf(stderr, "col_start = %d, col_stop = %d\n", col_start, col_stop);
	fprintf(stderr, "\tnum_cols = %d\n", num_cols);

#endif

	for (y = row_start; y < row_stop; y++)
		for (x = col_start; x < col_stop; x++)
		{
			vga_paint_charcell(widget, vga,
					vga->pvt->video_buf[y*80+x],
					x*vga->pvt->font->width,
					y*vga->pvt->font->height);
		}
#endif
	
}

/*
 * Same as vga_paint(), but works on regions of cell rows/columns
 * instead of a pixel area.
 */
static void
vga_paint_region(GtkWidget *widget,
			int top_left_x, int top_left_y,
			int cols, int rows)
{
	VGAText * vga;
	GdkRectangle area;
	g_return_if_fail(widget != NULL);
	if (!GTK_WIDGET_REALIZED(widget))
		return;

	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	area.x = top_left_x * vga->pvt->font->width;
	area.y = top_left_y * vga->pvt->font->height;
	area.width = cols * vga->pvt->font->width;
	area.height = rows * vga->pvt->font->height;
	vga_paint(widget, &area);
}

static gint
vga_expose(GtkWidget *widget, GdkEventExpose * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_expose()\n");
#endif

	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	if (event->window == widget->window)
	{
		vga_paint(widget, &event->area);
	}
	else
		g_assert_not_reached();
	return TRUE;
}


/* Perform final cleanups for the widget before it's free'd */
static void
vga_finalize(GObject *object)
{
	VGAText * vga;
	//GtkWidget * toplevel;
	GtkWidgetClass * widget_class;
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_finalize()\n");
#endif
	
	g_return_if_fail(VGA_IS_TEXT(object));
	vga = VGA_TEXT(object);
	widget_class = g_type_class_peek(GTK_TYPE_WIDGET);

	vga->pvt->render_enabled = FALSE;
	pthread_join(vga->pvt->thread, NULL);

	/* Remove the blink timeout functions */
	if (vga->pvt->cursor_timeout_id != -1)
		g_source_remove(vga->pvt->cursor_timeout_id);
	if (vga->pvt->blink_timeout_id != -1)
		g_source_remove(vga->pvt->blink_timeout_id);
#if 0
	if (vga->pvt->render_timeout_id != -1)
		g_source_remove(vga->pvt->render_timeout_id);
#endif

	/* Destroy its font object */
	g_object_unref(vga->pvt->font);

	/* Destroy palette */
	g_object_unref(vga->pvt->pal);

	/* Free up private widget memory allocations */
	g_free(vga->pvt->video_buf);
	g_free(vga->pvt->sec_buf);
	g_free(vga->pvt->line_buf);
	g_free(vga->pvt->glyphs);
	g_free(vga->pvt->dirty_buf);
	g_free(vga->pvt->dirty_line_buf);

	/* Call the inherited finalize() method. */
	if (G_OBJECT_CLASS(widget_class)->finalize)
	{
		(G_OBJECT_CLASS(widget_class))->finalize(object);
	}
}

static gint
vga_focus_in(GtkWidget * widget, GdkEventFocus * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_focus_in()\n");
#endif
	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	GTK_WIDGET_SET_FLAGS(widget, GTK_HAS_FOCUS);
	//gtk_im_context_focus_in((VGA_TEXT(widget))->pvt->im_context);
	/* invalidate cursor once? */
	return TRUE;
}

static gint
vga_focus_out(GtkWidget * widget, GdkEventFocus * event)
{
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_focus_out()\n");
#endif
	g_return_val_if_fail(VGA_IS_TEXT(widget), 0);
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_HAS_FOCUS);
	//gtk_im_context_focus_out((VGA_TEXT(widget))->pvt->im_context);
	/* invalidate cursor once? */
	return TRUE;
}

/* Tell GTK+ how much space we need. */
static void
vga_size_request(GtkWidget * widget, GtkRequisition *req)
{
	VGAText * vga;
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_size_request()\n");
#endif
	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	req->width = vga->pvt->font->width * vga->pvt->cols;
	req->height = vga->pvt->font->height * vga->pvt->rows;

#ifdef VGA_DEBUG
	fprintf(stderr, "Size request is %dx%d.\n",
			req->width, req->height);
#endif
}

/* Accept a given size from GTK+. */
static void
vga_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	VGAText * vga;
	glong width, height;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	width = allocation->width / vga->pvt->font->width;
	height = allocation->height / vga->pvt->font->height;

#ifdef VGA_DEBUG
	fprintf(stderr, "Sizing window to %dx%d (%ldx%ld).\n",
		allocation->width, allocation->height,
		width, height);
#endif

	/* Set our allocation to match the structure. */
	widget->allocation = *allocation;

	/* Set the size of the pseudo-terminal. */
	//vte_terminal_set_size(terminal, width, height);

	/* Adjust scrollback buffers to ensure that they're big enough. */
	//vte_terminal_set_scrollback_lines(terminal,
	//				  MAX(terminal->pvt->scrollback_lines,
//					      height));

	/* Resize the GDK window. */
	if (widget->window != NULL) {
		gdk_window_move_resize(widget->window,
				       allocation->x,
				       allocation->y,
				       allocation->width,
				       allocation->height);
		//vte_terminal_queue_background_update(terminal);
	}

	/* Adjust the adjustments. */
	//vte_terminal_adjust_adjustments(terminal);
}


static void
vga_class_init(VGATextClass *klass, gconstpointer data)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;

#ifdef VGA_DEBUG
	fprintf(stderr, "vga_class_init()\n");
#endif

	// bindtextdomain(PACKAGE, LOCALEDIR);
	
	gobject_class = G_OBJECT_CLASS(klass);
	widget_class = GTK_WIDGET_CLASS(klass);

	/* Override some of the default handlers */
	gobject_class->finalize = vga_finalize;
	widget_class->realize = vga_realize;
	widget_class->expose_event = vga_expose;
	widget_class->focus_in_event = vga_focus_in;
	widget_class->focus_out_event = vga_focus_out;
	widget_class->unrealize = vga_unrealize;
	widget_class->size_request = vga_size_request;
	widget_class->size_allocate = vga_size_allocate;
	//widget_class->get_accessible = vga_get_accessible;

	/* Register some signals of our own */
	klass->contents_changed_signal =
		g_signal_new("contents-changed",
			G_OBJECT_CLASS_TYPE(klass),
			G_SIGNAL_RUN_LAST,
			0,
			NULL,
			NULL,
			g_cclosure_marshal_VOID__VOID,
			G_TYPE_NONE, 0);
	
}

#define RENDER_PERIOD_MS	33
static void
render_thread(void *ptr)
{
	GtkWidget * widget = (GtkWidget *) ptr;
	VGAText * vga;
	GTimer *timer;
	long elapsed_ms;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(VGA_IS_TEXT(widget));
	vga = VGA_TEXT(widget);

	timer = g_timer_new();

	g_timer_start(timer);
	while (vga->pvt->render_enabled) {
		gdk_threads_enter();
		vga_render_buf((gpointer) widget);
		gdk_threads_leave();
		elapsed_ms = (long) g_timer_elapsed(timer, NULL) * 1000;
		if (elapsed_ms > RENDER_PERIOD_MS) {
			printf("Took longer than %d ms to render, or gtk main"
			    "blocked for a long time.\n", RENDER_PERIOD_MS);
		}  else {
			g_usleep((RENDER_PERIOD_MS*1000) -
				 (elapsed_ms*1000));
		}
		g_timer_start(timer);
	}
}

/*
 *  Initialize the VGA text widget after the base widget stuff is initialized.
 */
static void
vga_init(VGAText *vga, gpointer *klass)
{
	struct _VGATextPrivate * pvt;
	GtkWidget * widget;

	/*tmp*/ int i;
#ifdef VGA_DEBUG
	fprintf(stderr, "vga_init()\n");
#endif

fprintf(stderr, "NAC: vga_init(): return if fail\n");
	g_return_if_fail(VGA_IS_TEXT(vga));
fprintf(stderr, "NAC: vga_init(): cast widget\n");
	widget = GTK_WIDGET(vga);
fprintf(stderr, "NAC: vga_init(): set flags\n");
	GTK_WIDGET_SET_FLAGS(widget, GTK_CAN_FOCUS);

	/*
	 * We are painting using an off-screen image (cairo surface buffer),
	 * so we don't need GTK+'s double buffering.
	 */
	GTK_WIDGET_UNSET_FLAGS(widget, GTK_DOUBLE_BUFFERED);

	/* Initialize private data */
fprintf(stderr, "NAC: vga_init(): malloc\n");
	pvt = vga->pvt = g_malloc0(sizeof(*vga->pvt));
fprintf(stderr, "NAC: vga_init(): new font\n");
	pvt->font = VGA_FONT(vga_font_new());
fprintf(stderr, "NAC: vga_init(): load default font\n");
	vga_font_load_default(pvt->font);
	//vga_font_load_from_file(pvt->font, "dump.fnt");

fprintf(stderr, "NAC: vga_init(): palette dup\n");
	pvt->pal = vga_palette_dup(vga_palette_stock(PAL_DEFAULT));

	//vga_palette_load_default(pvt->pal);
	/*if (vga_palette_load_from_file(pvt->pal, "dos.pal"))
		g_message("Loaded palette file successfully");
	else
	{
		g_message("Failed to load palette file, falling back to default"); */
	/*}*/

fprintf(stderr, "NAC: vga_init(): set pvt fields\n");
	pvt->rows = 25;
	pvt->cols = 80;

	pvt->fg = 0x07;
	pvt->bg = 0x00;

	pvt->cursor_visible = TRUE;

	/* These are initialized if needed in vga_realize() for now */
#ifdef USE_DEPRECATED_GDK
	pvt->glyphs = NULL;
	pvt->gc = NULL;
#endif

fprintf(stderr, "NAC: vga_init(): video buf\n");
	pvt->video_buf_len = sizeof(vga_charcell) * pvt->rows * pvt->cols;
	pvt->video_buf = g_malloc0(pvt->video_buf_len);
	pvt->sec_buf = g_malloc0(pvt->video_buf_len);
	pvt->render_sec_buf = FALSE;
#if 0
	/* populate our buffer with the ASCII table */
	for (i = 0; i < 80*25; i++)
	{
		pvt->video_buf[i].c = i % 256;
		pvt->video_buf[i].attr = i % 256;
	}
#endif
	/*
	pvt->video_buf[0].c = '!';
	pvt->video_buf[0].attr = 0x09;
	pvt->video_buf[100].c = '@';
	pvt->video_buf[100].attr = 0x2A; */
	/* FIXME: Destroy this on destroy */
	pvt->line_buf = g_malloc0(sizeof(guchar) * pvt->cols * 3 + 1);
	/* FIXME: Destroy this on destroy */
	pvt->glyphs = g_malloc0(sizeof(cairo_glyph_t) * pvt->cols + 1);

	pvt->dirty_buf = g_malloc0(sizeof(char) * pvt->rows * pvt->cols);
	pvt->dirty_line_buf = g_malloc0(sizeof(gboolean) * pvt->rows);

fprintf(stderr, "NAC: vga_init(): cairo\n");
	/* FIXME: Destroy this on destroy */
	pvt->surface_buf = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
				pvt->font->width * vga->pvt->cols,
				pvt->font->height * vga->pvt->rows);


#if 0
	pvt->render_timeout_id = g_timeout_add(33 /* ~30fps */,
			vga_render_buf, widget);
#endif

	/* Add our cursor timeout event to make it blink */
	/* 229 ms is about how often the cursor blink is toggled in DOS */
	pvt->cursor_timeout_id = g_timeout_add(CURSOR_BLINK_PERIOD_MS,
			vga_blink_cursor, vga);

	/* Leave this at -1 until we actually have characters blinking */
	pvt->blink_timeout_id = -1;

	pvt->blink_state = TRUE;
	pvt->cursor_blink_state = TRUE;
	pvt->icecolor = TRUE;

/* FIXME: Don't knwo if this is needed */
#if 1
	gtk_widget_set_colormap(widget, gdk_screen_get_rgb_colormap(gtk_widget_get_screen(widget)));
#endif

	pvt->render_enabled = TRUE;
fprintf(stderr, "NAC: vga_init(): pthread_create\n");
	pthread_create(&pvt->thread, NULL,
		       (void *) render_thread, (void *) vga);
fprintf(stderr, "NAC: vga_init(): done\n");
}

GtkType
vga_get_type(void)
{
	static GtkType vga_type = 0;
	static const GTypeInfo vga_info =
	{
		sizeof(VGATextClass),
		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vga_class_init,
		(GClassFinalizeFunc)NULL,
		(gconstpointer)NULL,

		sizeof(VGAText),
		0,
		(GInstanceInitFunc)vga_init,

		(GTypeValueTable*)NULL,
	};

	if (vga_type == 0)
	{
		vga_type = g_type_register_static(GTK_TYPE_WIDGET,
						"VGAText",
						&vga_info,
						0);
	}

	return vga_type;
}



/*************************************
 * Public methods
 *************************************/


VGAPalette *
vga_get_palette(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(vga), NULL);

	return vga->pvt->pal;
}


VGAFont *
vga_get_font(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(vga), NULL);

	return vga->pvt->font;
}

/* Override the default VGA palette */
void
vga_set_palette(VGAText *vga, VGAPalette * palette)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	g_object_unref(vga->pvt->pal);
	vga->pvt->pal = palette;

	vga_refresh(vga);
}


/* Re-render the internal font data so that changes will be reflected on
 * the next display refresh. */
void
vga_refresh_font(VGAText *vga)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

#ifdef USE_DEPRECATED_GDK
	g_object_unref(vga->pvt->glyphs);
	vga->pvt->glyphs = vga_font_get_bitmap(vga->pvt->font,
			GTK_WIDGET(vga)->window);
	gdk_gc_set_stipple(vga->pvt->gc, vga->pvt->glyphs);
#else
	/* FIXME: ??? */
#endif
}

/* Override the default VGA font.  Refreshes the display. */
void
vga_set_font(VGAText *vga, VGAFont *font)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	if (vga->pvt->font != font)
	{
		g_object_unref(vga->pvt->font);
		vga->pvt->font = font;
	}

	vga_refresh_font(vga);
	vga_refresh(vga);
}

/*
 * Enable or disable iCECOLOR (use 'high intensity backgrounds' rather than
 * 'blink' for the blink bit of the text attribute.
 */
void
vga_set_icecolor(VGAText *vga, gboolean status)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	vga->pvt->icecolor = status;
}

gboolean vga_get_icecolor(VGAText *vga)
{
	g_assert(vga != NULL);
	g_assert(VGA_IS_TEXT(vga));

	return vga->pvt->icecolor;
}


/* Put a character on the screen */
void
vga_put_char(VGAText *vga, guchar c, guchar attr, int col, int row)
{
	int ofs;
	GdkRectangle area;

	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	/* Update video buffer */
	ofs = vga->pvt->cols * row + col;
	vga->pvt->video_buf[ofs].c = c;
	vga->pvt->video_buf[ofs].attr = attr;

#if 0
	/* Refresh charcell */
	area.x = col * vga->pvt->font->width;
	area.y = row * vga->pvt->font->height;
	area.width = vga->pvt->font->width;
	area.height = vga->pvt->font->height;
	vga_render_area(vga, &area);
#else
	vga_mark_region_dirty(vga, col, row, 1, 1);
#endif
}

/* Put a string on the screen.  String will be truncated if exceeds screen
 * width, so it's not a real print function.  Intended for quick and dirty
 * uses, and for optimized string writes. */
void
vga_put_string(VGAText *vga, guchar * s, guchar attr, int col, int row)
{
	int ofs, i, len;
	GdkRectangle area;

	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));
	g_return_if_fail(s != NULL);

	len = strlen(s);
	if (len > (vga->pvt->cols - col))
		/* Truncate string */
		len = vga->pvt->cols - col;

	/* Update video buffer */
	ofs = vga->pvt->cols * row + col;
	for (i = 0; i < len; i++)
	{
		vga->pvt->video_buf[ofs].c = s[i];
		vga->pvt->video_buf[ofs++].attr = attr;
	}

#if 0
	/* Refresh charcells */
	area.x = col * vga->pvt->font->width;
	area.y = row * vga->pvt->font->height;
	area.width = vga->pvt->font->width * len;
	area.height = vga->pvt->font->height;
	vga_render_area(vga, &area);
#else
	vga_mark_region_dirty(vga, col, row, len, 1);
#endif
}



/* Get a pointer to the screen internal video buffer */
guchar *
vga_get_video_buf(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(vga), NULL);

	return (guchar *) vga->pvt->video_buf;
}

/*
 * Get a pointer to the secondary video buffer.
 * Manipulate this buffer on your own, and toggle displaying it
 * in this widget via vga_show_secondary().
 */
guchar *
vga_get_sec_buf(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, NULL);
	g_return_val_if_fail(VGA_IS_TEXT(vga), NULL);

	return (guchar *) vga->pvt->sec_buf;
}

int
vga_video_buf_size(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(vga), -1);

	return vga->pvt->video_buf_len;
}

void
vga_video_buf_clear(VGAText *vga)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	memset(vga_get_video_buf(vga), 0, vga_video_buf_size(vga));
	vga_mark_region_dirty(vga, 0, 0, vga->pvt->cols, vga->pvt->rows);
}

/* Show or hide the cursor */
void
vga_cursor_set_visible(VGAText *vga, gboolean visible)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	vga->pvt->cursor_visible = visible;
}

gboolean
vga_cursor_is_visible(VGAText *vga)
{
	g_assert(vga != NULL);
	g_assert(VGA_IS_TEXT(vga));

	return vga->pvt->cursor_visible;
}


/* Change the cursor position */
void
vga_cursor_move(VGAText *vga, int x, int y)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	if (vga->pvt->cursor_visible)
		//vga_render_region(vga, vga->pvt->cursor_x,
		vga_mark_region_dirty(vga, vga->pvt->cursor_x,
				vga->pvt->cursor_y, 1, 1);

	vga->pvt->cursor_x = x;
	vga->pvt->cursor_y = y;

	if (vga->pvt->cursor_visible)
		//vga_render_region(vga, x, y, 1, 1);
		vga_mark_region_dirty(vga, x, y, 1, 1);
}

int
vga_cursor_x(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(vga), -1);

	return vga->pvt->cursor_x;
}

int
vga_cursor_y(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(vga), -1);

	return vga->pvt->cursor_y;
}

/*
 * Mark cells as dirty.
 *
 * In this case, "dirty" means that the VGA buffer contents do not
 * match the contents that have been rendered onto the off-screen
 * surface buffer.  Generally you would call this after modifying
 * the VGA display buffer directly.
 */
void
vga_mark_region_dirty(VGAText *vga,
			int top_left_x, int top_left_y,
			int cols, int rows)
{
	int i, x, y;

	g_return_if_fail(vga != NULL);
/* We shouldn't care if vga is realized or not for this */
#if 0
	if (!GTK_WIDGET_REALIZED(vga))
		return;
#endif

	g_return_if_fail(VGA_IS_TEXT(vga));

	for (y = top_left_y; y < (top_left_y + rows); y++) {
//printf("vga_mark_region_dirty(): line %d dirty\n", y);
		vga->pvt->dirty_line_buf[y] = 1;
		for (x = top_left_x; x < (top_left_x + cols); x++) {
			i = y * vga->pvt->cols + x;
			vga->pvt->dirty_buf[i] = 1;
		}
	}
}

/*
 * This is the same as vga_render_area() in that it renders the contents
 * of the VGA buffer onto the off-screen surface buffer.
 *
 * However, the rectangular area is specified in terms of cell rows/columns
 * (e.g., 0..79,0..24), instead of pixels.
 */
void
vga_render_region(VGAText *vga,
			int top_left_x, int top_left_y,
			int cols, int rows)
{
	GdkRectangle area;
	g_return_if_fail(vga != NULL);
	if (!GTK_WIDGET_REALIZED(vga))
		return;

	g_return_if_fail(VGA_IS_TEXT(vga));

//printf("vga_render_region(): top_left_x = %d, top_left_y = %d, cols=%d, rows=%d\n", top_left_x, top_left_y, cols, rows);
	area.x = top_left_x * vga->pvt->font->width;
	area.y = top_left_y * vga->pvt->font->height;
	area.width = cols * vga->pvt->font->width;
	area.height = rows * vga->pvt->font->height;
	vga_render_area(vga, &area);
}
		
/* Re-render and re-paint entire screen to refresh the entire
 * screen */
void
vga_refresh(VGAText *vga)
{
	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));

	/*
	 * We force a repaint here instead of scheduling it because the
	 * caller probably intends for the current state to be shown.
	 * This ensures things like palette morphing work, since otherwise
	 * only the final palette is displayed since gtk interprets the
	 * entire thing as a single invalidate
	 */
	vga_mark_region_dirty(vga, 0, 0, vga->pvt->cols, vga->pvt->rows);
	/* Lame hack that ensures we wait for the rendering thread
	 * to be updated with the dirty region.... */
	printf("uslep(%d)\n", RENDER_PERIOD_MS * 1000 * 2);
	//g_usleep(RENDER_PERIOD_MS * 1000 * 2);
	//g_usleep(40000);
	/* Now explicitly repaint it.. */
	/* This step is SLOOWWWW... */
	//vga_paint_region(vga, 0, 0, vga->pvt->cols, vga->pvt->rows);
	gtk_main_iteration();
	printf("vga_refresh(): done\n");
}

int vga_get_rows(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(vga), -1);

	return vga->pvt->rows;
}

int vga_get_cols(VGAText *vga)
{
	g_return_val_if_fail(vga != NULL, -1);
	g_return_val_if_fail(VGA_IS_TEXT(vga), -1);

	return vga->pvt->cols;
}

void vga_show_secondary(VGAText *vga, gboolean enabled)
{
	vga->pvt->render_sec_buf = enabled;
}

/**
 * memsetword:
 * @s: Pointer to the start of the area
 * @c: The word to fill the area with
 * @count: The size of the area
 *
 * Fill a memory area with a 16-byte word value
 */
static void * memsetword(void * s, gint16 c, size_t count)
{
	/* Simple portable way of doing it */
	gint16 * xs = (gint16 *) s;

	while (count--)
		*xs++ = c;

	return s;
}

void vga_clear_area(VGAText *vga, guchar attr, int top_left_x,
		int top_left_y, int cols, int rows)
{
	gint16 cellword;
	int ofs, y, endrow;

	g_return_if_fail(vga != NULL);
	g_return_if_fail(VGA_IS_TEXT(vga));
	/* FIXME: Endianness.  No << 8 for big endian */
	cellword = 0x0000 | ((gint16) attr << 8);
	/* Special case optimization */
	if (cols == vga->pvt->cols)
	{
		ofs = top_left_y * cols;
		memsetword(vga->pvt->video_buf + ofs, cellword, cols * rows);
	}
	else
	{
		endrow = top_left_y + rows;
		for (y = top_left_y; y < endrow; y++)
		{
			ofs = (y * vga->pvt->cols + top_left_x);
			memsetword(vga->pvt->video_buf + ofs, cellword, cols);
		}
	}
	vga_mark_region_dirty(vga, top_left_x, top_left_y, cols, rows);
}

/* Clear screen / eol will be done in the terminal widget since it is
 * based on the screen 'textattr'. */

/* Delete a line, scrolling .. hmm question.. should we have the 'window'
 * concept in here?  i think the BIOS might have actually had something
 * like that but i doubt it now that i think about it. */
