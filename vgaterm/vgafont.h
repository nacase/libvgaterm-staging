/*
 *  Copyright (C) 2002-2011 Nate Case 
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
 */
#ifndef __VGA_FONT_H__
#define __VGA_FONT_H__

#include <stdio.h>

#include "def_font.h"
#include <gtk/gtk.h>
#include <string.h>

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VGA_TYPE_FONT		(vga_font_get_type())
#define VGA_FONT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), VGA_TYPE_FONT, VGAFont))
#define VGA_FONT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), VGA_TYPE_FONT, VGAFontClass))
#define VGA_IS_FONT(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), VGA_TYPE_FONT))
#define IS_VGA_FONT		VGA_IS_FONT
#define VGA_IS_FONT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VGA_TYPE_FONT))
#define VGA_FONT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), VGA_TYPE_FONT, VGAFontClass))

typedef struct _VGAFont	VGAFont;
typedef struct _VGAFontClass VGAFontClass;
typedef struct _RenderedChar	RenderedChar;
typedef struct _VGAFontPrivate VGAFontPrivate;

struct _VGAFont
{
	GObject parent_instance;

	/* instance members */
	int width, height;		/* Pixel sizes, usually 8x16 */
	int bytes_per_glyph;		/* Normally width*height / 8 */
	cairo_font_face_t *face;

	/* <private> */
	VGAFontPrivate *pvt;
};

struct _VGAFontClass
{
	GObjectClass parent_class;

	/* class members */
};

GType		vga_font_get_type	(void);

/* Method definitions */
GObject	*	vga_font_new		(void);
gboolean	vga_font_set_chars	(VGAFont *font, guchar *data,
						guchar start_c, guchar end_c);
gboolean	vga_font_load		(VGAFont *font, guchar *data,
						int height, int width);
gboolean	vga_font_load_from_file	(VGAFont *font, gchar *fname);
void		vga_font_load_default	(VGAFont *font);
guchar *	vga_font_get_glyph_data	(VGAFont *font, int glyph);
int		vga_font_pixels		(VGAFont *font);
#if 0
GdkBitmap *	vga_font_get_bitmap	(VGAFont *font, GdkWindow *win);
#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __VGA_FONT_H__ */
