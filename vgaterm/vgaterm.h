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
 *  This extends the parent vgatext object.
 */


#ifndef __VGA_TERM_H__
#define __VGA_TERM_H__

#include <gdk/gdk.h>
#include "vgatext.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VGA_TYPE_TERM		(vga_term_get_type())
#define VGA_TERM(obj)		(GTK_CHECK_CAST((obj), VGA_TYPE_TERM, VGATerm))
#define VGA_TERM_CLASS(klass)	(GTK_CHECK_CLASS_CAST((klass), VGA_TYPE_TERM, VGATermClass))
#define VGA_IS_TERM(obj)	(GTK_CHECK_TYPE((obj), VGA_TYPE_TERM))
#define IS_VGA_TERM		VGA_IS_TERM
#define VGA_IS_TERM_CLASS(klass) (GTK_CHECK_CLASS_TYPE((klass), VGA_TYPE_TERM))
#define VGA_TERM_GET_CLASS(obj)	(GTK_CHECK_GET_CLASS((obj), VGA_TYPE_TERM, VGATermClass))

#define VGA_TERM_DEFAULT_SCROLLBUF_LINES	2000
#define VGA_TERM_DEFAULT_SCROLLBUF_BYTES	320000

typedef struct _VGATerm	VGATerm;
typedef struct _VGATermClass VGATermClass;
typedef struct _VGATermPrivate VGATermPrivate;

struct _VGATerm
{
	/* Parent instance */
	VGAText vga;

	/* <public> -- though should be read-only really */

	guchar textattr;
	int win_top_left_x, win_top_left_y;
	int win_bot_right_x, win_bot_right_y;
	
	/* <private> */
	VGATermPrivate *pvt;
};

struct _VGATermClass
{
	VGATextClass parent_class;

#if !GTK_CHECK_VERSION (2, 91, 2)
	void (* set_scroll_adjustments) (GtkWidget *widget,
					 GtkAdjustment *hadjustment,
					 GtkAdjustment *vadjustment);
#endif
};

GType		vga_term_get_type	(void)	G_GNUC_CONST;
GtkWidget * 	vga_term_new		(void);
void		vga_term_writec		(VGATerm *widget, guchar c);
gint		vga_term_write		(VGATerm *widget, guchar *s);
gint		vga_term_writeln	(VGATerm *widget, guchar *s);
int		vga_term_print		(VGATerm *widget,
						const gchar *format, ...);

gboolean	vga_term_kbhit		(VGATerm *widget);
guchar		vga_term_readkey	(VGATerm *widget);
void		vga_term_window		(VGATerm *widget, int x1, int y1,
						int x2, int y2);

void		vga_term_gotoxy		(VGATerm *widget, int x, int y);
int		vga_term_wherex		(VGATerm *widget);
int		vga_term_wherey		(VGATerm *widget);
int		vga_term_cols		(VGATerm *widget);
int		vga_term_rows		(VGATerm *widget);
void		vga_term_clrscr		(VGATerm *widget);
void		vga_term_clrdown	(VGATerm *widget);
void		vga_term_clrup		(VGATerm *widget);
void		vga_term_clreol		(VGATerm *widget);
void		vga_term_scroll_up	(VGATerm *widget, int start_row,
							int lines);
void		vga_term_scroll_down	(VGATerm *widget, int start_row,
							int lines);
void		vga_term_insline	(VGATerm *widget);
void		vga_term_delline	(VGATerm *widget);
void		vga_term_set_attr	(VGATerm *widget, guchar textattr);
guchar		vga_term_get_attr	(VGATerm *widget);
void		vga_term_set_fg		(VGATerm *widget, guchar fg);
void		vga_term_set_bg		(VGATerm *widget, guchar bg);
void		vga_term_set_scroll	(VGATerm *term, int line);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __VGA_TERM_H__ */
