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
 */

#include "vgapalette.h"
#include "def_palette.h"



static void vga_palette_class_init	(VGAPaletteClass *klass);
static void vga_palette_init		(VGAPalette *pal);
static void vga_palette_finalize	(GObject *gobject);

/* Private data */
#define VGA_PALETTE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VGA_TYPE_PALETTE, VGAPalettePrivate))

/* Private data */
struct _VGAPalettePrivate {
	GdkColor color[PAL_REGS];
};

G_DEFINE_TYPE(VGAPalette, vga_palette, G_TYPE_OBJECT)

static void vga_palette_class_init(VGAPaletteClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
fprintf(stderr, "NAC: vga_palette_class_init\n");

	g_type_class_add_private(klass, sizeof(VGAPalettePrivate));

	obj_class->finalize = vga_palette_finalize;
}

static void vga_palette_init(VGAPalette *pal)
{
	VGAPalettePrivate *pvt;

	pal->pvt = pvt = VGA_PALETTE_GET_PRIVATE(pal);
}


/**
 * vga_palette_new:
 *
 * Create a new VGA Palette object.
 * 
 * Returns: a new VGA palette object.
 */
GObject * vga_palette_new(void)
{
GType blah;
fprintf(stderr, "NAC: vga_palette_new(): enter\n");
fprintf(stderr, "NAC: vga_palette_new(): call get_type\n");
blah = vga_palette_get_type();
fprintf(stderr, "NAC: vga_palette_new(): call g_object_new\n");
	return G_OBJECT(g_object_new(vga_palette_get_type(), NULL));
}


/**
 * vga_palette_stock:
 * @id: Stock Palette ID enum.
 * 
 * Get a stock palette object.  Valid stock palette IDs are PAL_DEFAULT,
 * PAL_BLACK, PAL_WHITE, PAL_GREYSCALE.  These objects should not be destroyed
 * or modified.  Use vga_palette_dup() to get a copy of a palette object
 * which you can modify. Note that vga_palette_stock(PAL_DEFAULT) is
 * functionally equivilent to using vga_palette_load_default(vga_palette_new());
 *
 * Returns: A stock palette object
 */ 
VGAPalette * vga_palette_stock(StockPalette id)
{
	static VGAPalette * def = NULL;
	static VGAPalette * black = NULL;
	static VGAPalette * white = NULL;
	static VGAPalette * greyscale = NULL;
	int i;
	long avg;
	guint16 value;

	/*
	 * Lazy allocation: Palette objects don't really exist until they're
	 * requested.
	 */
	switch (id)
	{
	case PAL_DEFAULT:
		if (def == NULL)
		{
		fprintf(stderr, "NAC: allow new palette\n");
			def = VGA_PALETTE(vga_palette_new());
		fprintf(stderr, "NAC: load default on it\n");
			vga_palette_load_default(def);
		fprintf(stderr, "NAC: return new palette\n");
		}
		return def;
	case PAL_WHITE:
		if (white == NULL)
		{
			white = VGA_PALETTE(vga_palette_new());
			value = TO_GDK_RGB(63);
				
			for (i = 0; i < PAL_REGS; i++)
			{
				white->pvt->color[i].red =
				white->pvt->color[i].green =
				white->pvt->color[i].blue = value;
			}
		}
		return white;
	case PAL_BLACK:
		if (black == NULL)
		{
			black = VGA_PALETTE(vga_palette_new());
			memset(black->pvt->color,
				0,
				PAL_REGS * sizeof(GdkColor));
		}
		return black;
	case PAL_GREYSCALE:
		/* Greyscaling is done by averaging the RGB values from
		 * the default palette */
		
		if (greyscale == NULL)
		{
			greyscale = VGA_PALETTE(vga_palette_new());
			vga_palette_load_default(greyscale);
			
			for (i = 0; i < PAL_REGS; i++)
			{
				avg =	(greyscale->pvt->color[i].red +
					greyscale->pvt->color[i].green +
					greyscale->pvt->color[i].blue) / 3;
				greyscale->pvt->color[i].red =
				greyscale->pvt->color[i].green =
				greyscale->pvt->color[i].blue = avg;
			}
		}
		return greyscale;
	default:
		g_error("Unknown stock palette id (%d)", id);
	}
	
	return NULL;	/* Humor the compiler */
}

/**
 * vga_palette_dup:
 * @pal: the VGA Palette object to clone
 *
 * Duplicate/copy a palette object.
 *
 * Returns: A newly allocated copy of @pal.
 */
VGAPalette * vga_palette_dup(VGAPalette * pal)
{
	VGAPalette * tmp;
	fprintf(stderr, "NAC: vga_palette_dup(): new alloc\n");
	tmp = VGA_PALETTE(vga_palette_new());
	fprintf(stderr, "NAC: vga_palette_dup(): memcpy\n");
	memcpy(tmp->pvt->color, pal->pvt->color,
			PAL_REGS * sizeof(GdkColor));
	fprintf(stderr, "NAC: vga_palette_dup(): return\n");
	return tmp;
}


/**
 * vga_palette_copy_from:
 * @pal: the VGA Palette object to populate
 * @srcpal: the VGA Palette to copy data from
 *
 * Copy data from a Palette object into the given palette
 *
 * Returns: The @pal object.
 */
VGAPalette * vga_palette_copy_from(VGAPalette * pal, VGAPalette * srcpal)
{
	memcpy(pal->pvt->color, srcpal->pvt->color,
			PAL_REGS * sizeof(GdkColor));
	return pal;
}


/**
 * vga_palette_finalize:
 * @pal: the VGA Palette object
 *
 * Destroy a VGA Palette object.
 */
static void
vga_palette_finalize(GObject *gobject)
{
	VGAPalette *pal = VGA_PALETTE(gobject);

	fprintf(stderr, "NAC: vga_palette_finalize: enter\n");
	/* Finish up object destruction.  This will only be called once. */

	/* Chain up to the parent class */
	G_OBJECT_CLASS (vga_palette_parent_class)->finalize(gobject);
}

/**
 * vga_palette_load:
 * @pal: The VGA palette object
 * @data: raw VGA palette data
 * @size: the size (in bytes) of the raw VGA palette data
 **/
gboolean vga_palette_load(VGAPalette * pal, guchar * data, int size)
{
	int i, n = 0;
	g_return_val_if_fail(size < PAL_REGS*3, FALSE);

	for (i = 0; i < size; i += 3)
	{
		pal->pvt->color[n].pixel = -1;
		pal->pvt->color[n].red = TO_GDK_RGB(*((guchar *) (data + i)));
		pal->pvt->color[n].green = TO_GDK_RGB(*((guchar *) (data + i + 1)));
		pal->pvt->color[n++].blue = TO_GDK_RGB(*((guchar *) (data + i + 2)));
	}

	return TRUE;
}


/**
 * vga_palette_load_from_file:
 * @pal: the VGA palette object
 * @fname: the filename of the VGA palette file to load.
 *
 * Load a VGA palette from a raw VGA palette data file.
 */
gboolean vga_palette_load_from_file(VGAPalette * pal, guchar * fname)
{
	int filesize;
	FILE * f;
	guchar * buf;
	gboolean result;
	size_t count;

	/* Determine palette parameters and read file into memory buffer */
	f = fopen(fname, "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		filesize = ftell(f);
		fseek(f, 0, SEEK_SET);
		if ((filesize > PAL_REGS * 3) || (filesize % 3 != 0))
		{
			/* Invalid palette file */
			fclose(f);
			return FALSE;
		}
	
		buf = g_malloc(filesize);
		count = fread(buf, filesize, 1, f);
		fclose(f);
		if (count != 1)
			return FALSE;
	}
	else
		return FALSE;

	result = vga_palette_load(pal, buf, filesize);
	g_free(buf);

	return result;
}

/**
 * vga_palette_set_reg:
 * @pal: the VGA Palette object
 * @reg: the register number to set
 * @r: Red value
 * @g: Green value
 * @b: Blue value
 *
 * Set a single register of the VGA Palette.
 */
void vga_palette_set_reg(VGAPalette * pal, guchar reg, guchar r, guchar g,
		guchar b)
{
	pal->pvt->color[reg].red = TO_GDK_RGB(r);
	pal->pvt->color[reg].green = TO_GDK_RGB(g);
	pal->pvt->color[reg].blue = TO_GDK_RGB(b); 
}

GdkColor *vga_palette_get_reg(VGAPalette *pal, guchar reg)
{
	return &pal->pvt->color[reg];
}

/*
 * Standard EGA palette index -> palette register # mapping 
 * Map the standard 0..15 color attributes to their proper 0..63 register
 */
static int pal_map[16] = {0,1,2,3,4,5,20,7,56,57,58,59,60,61,62,63};

GdkColor *vga_palette_get_color(VGAPalette *pal, guchar pal_index)
{
	return &pal->pvt->color[pal_map[pal_index]];
}

/**
 * vga_palette_load_default:
 * @pal: the VGA palette object
 *
 * Load the default palette into VGA palette object.
 */
void vga_palette_load_default(VGAPalette * pal)
{
	/* FIXME: Hardcoded default palette size */
	vga_palette_load(pal, default_palette, 192);
}


/**
 * vga_palette_morph_to_step:
 * @pal: The palette object to morph
 * @srcpal: The source palette to use.  This is the desired palette you want
 * the pal to approach.
 *
 * Morph @pal toward @srcpal.  Performs one step in the palette morphing
 * process.  You should call this function 63 times to form a complete
 * morph (and update the display in between calls to this).
 */
void vga_palette_morph_to_step(VGAPalette * pal, VGAPalette * srcpal)
{
	int i;
	guint16 step;

	step = TO_GDK_RGB(1);
	for (i = 0; i < PAL_REGS; i++)
	{
		if (pal->pvt->color[i].red > srcpal->pvt->color[i].red)
			pal->pvt->color[i].red = MAX(pal->pvt->color[i].red - step,
						srcpal->pvt->color[i].red);
		else
			pal->pvt->color[i].red = MIN(pal->pvt->color[i].red + step,
						srcpal->pvt->color[i].red);
		if (pal->pvt->color[i].green > srcpal->pvt->color[i].green)
			pal->pvt->color[i].green = MAX(pal->pvt->color[i].green - step,
						srcpal->pvt->color[i].green);
		else
			pal->pvt->color[i].green = MIN(pal->pvt->color[i].green + step,
						srcpal->pvt->color[i].green);
		if (pal->pvt->color[i].blue > srcpal->pvt->color[i].blue)
			pal->pvt->color[i].blue = MAX(pal->pvt->color[i].blue - step,
						srcpal->pvt->color[i].blue);
		else
			pal->pvt->color[i].blue = MIN(pal->pvt->color[i].blue + step,
						srcpal->pvt->color[i].blue);

	}
}
