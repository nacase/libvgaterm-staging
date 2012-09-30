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
 *  This extends GtkWidget
 *
 */


#ifndef __VGA_TEXT_H__
#define __VGA_TEXT_H__

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <gtk/gtk.h>
#include "vgafont.h"
#include "vgapalette.h"


/* Standard byte-representation helpers for VGA text-mode attributes */

#define PURPLE MAGENTA
#define GRAY GREY
typedef enum
{
        BLACK, BLUE, GREEN, CYAN, RED, MAGENTA, BROWN, GREY
} base_color;

/* VGA text attribute byte manipulation */
#define GETFG(attr) (attr & 0x0F)
#define GETBG(attr) ((attr & 0x70) >> 4)
#define GETBLINK(attr) (attr >> 7)
#define BRIGHT(col) (col | 0x08)
#define SETFG(attr, fg) ((attr & 0xF0) | fg)
#define SETBG(attr, bg) ((attr & 0x8F) | (bg << 4))
#define BLINK(col) (col | 0x80)
#define ATTR(fg, bg) ((bg << 4) | fg)


G_BEGIN_DECLS

/* The widget itself */
typedef struct _VGAText
{
	/* Parent instance */
	GtkWidget widget;
	GtkAdjustment *adjustment;	/* Scrolling adjustment */

	/* Metric and sizing data */
	long rows, cols;
	
	/*< private >*/
	struct _VGATextPrivate * pvt;
} VGAText;

/* The widget's class structure */
typedef struct _VGATextClass
{
	/*< public >*/
	/* Inherited parent class */
	GtkWidgetClass parent_class;

	/*< private >*/
	/* Signals we might emit */
	guint contents_changed_signal;
	guint refresh_window_signal;
	guint move_window_signal;

	gpointer reserved1;
	gpointer reserved2;
	gpointer reserved3;
	gpointer reserved4;
} VGATextClass;

/*
 * This is the raw memory format used traditionally.
 * You should probably not modiify this struct.
 */
typedef struct
{
	guchar c;		/* The ASCII character */
	guchar attr;		/* The text attribute */
} vga_charcell;

GtkType vga_get_type(void);

#define VGA_TYPE_TEXT	               (vga_get_type())
#define VGA_TEXT(obj)               (GTK_CHECK_CAST((obj),\
                                                        VGA_TYPE_TEXT,\
                                                        VGAText))
#define VGA_TEXT_CLASS(klass)       GTK_CHECK_CLASS_CAST((klass),\
                                                             VGA_TYPE_TEXT,\                                                             VGATextClass)
#define VGA_IS_TEXT	VGA_IS_VGATEXT
#define VGA_IS_VGATEXT(obj)            GTK_CHECK_TYPE((obj),\
                                                       VGA_TYPE_TEXT)
#define VGA_TEXT_IS_VGA_CLASS(klass)    GTK_CHECK_CLASS_TYPE((klass),\
                                                             VGA_TYPE_TEXT)
#define VGA__GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), VGA_TYPE_TEXT, VGATextClass))


GtkWidget *	vga_text_new		(void);
void		vga_cursor_set_visible	(VGAText *vga, gboolean visible);
gboolean	vga_cursor_is_visible	(VGAText *vga);

void		vga_cursor_move		(VGAText *vga, int x, int y);
void		vga_set_palette		(VGAText *vga,
					 VGAPalette * palette);
VGAPalette *	vga_get_palette		(VGAText *vga);
VGAFont *	vga_get_font		(VGAText *vga);
void		vga_refresh_font	(VGAText *vga);
void		vga_set_font		(VGAText *vga, VGAFont *font);
void		vga_set_icecolor	(VGAText *vga, gboolean status);
gboolean	vga_get_icecolor	(VGAText *vga);
void		vga_put_char		(VGAText *vga, guchar c,
					 guchar attr,
					int col, int row);
void		vga_put_string		(VGAText *vga, guchar *s,
					 guchar attr, int col, int row);
guchar *	vga_get_video_buf	(VGAText *vga);
guchar *	vga_get_sec_buf		(VGAText *vga);
int		vga_video_buf_size	(VGAText *vga);

int		vga_cursor_x		(VGAText *vga);
int		vga_cursor_y		(VGAText *vga);
void		vga_render_region	(VGAText *vga,
					int top_left_x, int top_left_y,
					int cols, int rows);
void		vga_mark_region_dirty	(VGAText *vga,
					int top_left_x, int top_left_y,
					int cols, int rows);
void		vga_refresh		(VGAText *vga);
int		vga_get_rows		(VGAText *vga);
int		vga_get_cols		(VGAText *vga);
void		vga_clear_area		(VGAText *vga, guchar attr,
					 int top_left_x,
					 int top_left_y, int cols, int rows);
void		vga_video_buf_clear	(VGAText *vga);
void		vga_show_secondary	(VGAText *vga, gboolean enabled);

G_END_DECLS

#endif	/* __VGA_TEXT_H__ */
