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
 *  This doesn't do anything interesting.  I considered not even having this,
 *  but in the end the consistency with the VGA Font stuff gives a valuable
 *  level of elegance.
 *
 */

#ifndef __VGA_PALETTE_H__
#define __VGA_PALETTE_H__

#include <stdio.h>
#include <gtk/gtk.h>
#include <math.h>
#include <string.h>


#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define VGA_TYPE_PALETTE		(vga_palette_get_type())
#define VGA_PALETTE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), VGA_TYPE_PALETTE, VGAPalette))
#define VGA_PALETTE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), VGA_TYPE_PALETTE, VGAPaletteClass))
#define VGA_IS_PALETTE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE((obj), VGA_TYPE_PALETTE))
#define IS_VGA_PALETTE		VGA_IS_PALETTE
#define VGA_IS_PALETTE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VGA_TYPE_PALETTE))
#define VGA_PALETTE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), VGA_TYPE_PALETTE, VGAPaletteClass))


#define PAL_REGS	256
#define TO_GDK_RGB(vga_rgb)     (guint16) floor((vga_rgb * 1040.23809) + 0.5)

typedef enum
{
	PAL_DEFAULT, PAL_WHITE, PAL_BLACK, PAL_GREYSCALE
} StockPalette;


typedef struct _VGAPalette	VGAPalette;
typedef struct _VGAPaletteClass VGAPaletteClass;
typedef struct _VGAPalettePrivate VGAPalettePrivate;

/* 
 * The VGA Palette structure here contains 256 registers.  However, only
 * only 64 of them are commonly used in VGA applications.
 */
struct _VGAPalette
{
	GObject parent_instance;

	/* <private>  */
	struct _VGAPalettePrivate *pvt;
};

struct _VGAPaletteClass
{
	GObjectClass parent_class;

	/* class members */
};

/*
 * Note: In VGA/EGA color table terms, a "register" or "index" is a
 * value from 0..63 that indexes into the raw palette data in memory.
 * VGA actually supports a palette of 256 entries instead of EGA's limit
 * of 64.  But, we only support 64 anyway since that's all TextFX supports.
 *
 * We also limit ourselves to 16 colors shown at a time, like EGA.
 * So really, we are emulating EGA palettes more than VGA :/
 * I'm ignoring this since the target applications (terminals, BBSes)
 * only care about EGA anyway.
 *
 * A "palette index" is a number from 0..15 that refers to the color
 * attribute components that are referenced in video memory.
 * That is, in the video buffer, they only refer to 0..15 as either
 * the foreground or background color, since only 16 can be shown at a
 * time in EGA.
 *
 * These palette index numbers follow the same color scheme as used by
 * BBS pipe codes (e.g., "|09H|01ello |0AW|02orld!"), where the upper
 * nibble is the background palette index / attribute, and the lower nibble
 * is the foreground color.
 */

GObject *	vga_palette_new			(void);
VGAPalette *	vga_palette_stock		(StockPalette id);
VGAPalette *	vga_palette_dup			(VGAPalette *pal);
VGAPalette * 	vga_palette_copy_from		(VGAPalette *pal,
							VGAPalette *srcpal);
gboolean	vga_palette_load		(VGAPalette *pal,
							guchar * data,
							int size);
gboolean	vga_palette_load_from_file	(VGAPalette *pal,
							guchar * fname);
void		vga_palette_set_reg		(VGAPalette *pal, guchar reg,
							guchar r, guchar g,
							guchar b);
GdkColor *	vga_palette_get_reg		(VGAPalette *pal, guchar reg);
GdkColor *	vga_palette_get_color		(VGAPalette *pal, guchar pal_index);
void		vga_palette_load_default	(VGAPalette *pal);
void		vga_palette_morph_to_step	(VGAPalette *pal,
							VGAPalette * srcpal);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __VGA_PALETTE_H__ */
