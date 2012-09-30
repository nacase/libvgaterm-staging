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

#include "vgafont.h"


/* Function prototypes for Cairo user font callbacks */
static cairo_status_t
vga_font_cairo_init		(cairo_scaled_font_t *scaled_font,
				 cairo_t *cr,
				 cairo_font_extents_t *extents);
static cairo_status_t
vga_font_cairo_render_glyph	(cairo_scaled_font_t *scaled_font,
				 unsigned long glyph,
				 cairo_t *cr,
				 cairo_text_extents_t *extents);
static cairo_status_t
vga_font_cairo_unicode_to_glyph	(cairo_scaled_font_t *scaled_font,
				 unsigned long unicode,
				 unsigned long *glyph);
static cairo_status_t
vga_font_cairo_text_to_glyphs	(cairo_scaled_font_t *scaled_font,
				 const char *utf8,
				 int utf8_len,
				 cairo_glyph_t **glyphs,
				 int *num_glyphs,
				 cairo_text_cluster_t **clusters,
				 int *num_clusters,
				 cairo_text_cluster_flags_t *cluster_flags);

static const cairo_user_data_key_t vgafont_closure_key;


static void vga_font_class_init	(VGAFontClass *klass);
static void vga_font_init	(VGAFont *font);
static void vga_font_dispose	(GObject *gobject);
static void vga_font_finalize	(GObject *gobject);


/* Private data */
#define VGA_FONT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), VGA_TYPE_FONT, VGAFontPrivate))
struct _VGAFontPrivate {
	guchar *data;
};

G_DEFINE_TYPE(VGAFont, vga_font, G_TYPE_OBJECT)

static void vga_font_class_init(VGAFontClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	g_type_class_add_private(klass, sizeof(VGAFontPrivate));

	obj_class->dispose = vga_font_dispose;
	obj_class->finalize = vga_font_finalize;
}

static void vga_font_init(VGAFont *font)
{
	VGAFontPrivate *pvt;

	font->pvt = pvt = VGA_FONT_GET_PRIVATE(font);
	pvt->data = NULL;
	font->width = -1;
	font->height = -1;
	font->bytes_per_glyph = -1;

	font->face = cairo_user_font_face_create();

	/* Register Cairo font face callback functions */
	cairo_user_font_face_set_init_func(font->face,
					   vga_font_cairo_init);
	cairo_user_font_face_set_render_glyph_func(font->face,
					   vga_font_cairo_render_glyph);
	cairo_user_font_face_set_unicode_to_glyph_func(font->face,
					   vga_font_cairo_unicode_to_glyph);
#if 0
	cairo_user_font_face_set_text_to_glyphs_func(font->face,
					   vga_font_cairo_text_to_glyphs);
#endif
	
	/*
	 * Associate the face with our VGAFont object so we can access
	 * it from callback functions
	 */
	cairo_font_face_set_user_data(font->face, &vgafont_closure_key,
				      (void *) font, NULL);
}

/**
 * vga_font_new:
 * 
 * Create a new VGA Font object.
 *
 * Returns: a new VGA font object.
 */
GObject *vga_font_new(void)
{
fprintf(stderr, "NAC: vga_font_new(): enter\n");
	return G_OBJECT(g_object_new(vga_font_get_type(), NULL));
}

static void
vga_font_dispose(GObject *gobject)
{
	VGAFont *font = VGA_FONT(gobject);

	/*
	 * Unref everything we are using.  Note that this may be called
	 * multiple times (e.g., in circular reference situations).
	 * Also, other methods may be called after dispose() is called
	 * but before finalize() is called, so we should handle this
	 * case gracefully.
	 */
	if (font->face) {
		/* Cairo font faces ref counts are decreased via destroy */
		cairo_font_face_destroy(font->face);
		font->face = NULL;
	}

	/* Chain up to the parent class */
	G_OBJECT_CLASS(vga_font_parent_class)->dispose(gobject);
}

static void
vga_font_finalize(GObject *gobject)
{
	VGAFont *font = VGA_FONT(gobject);

	/* Finish up object destruction.  This will only be called once. */
	if (font->pvt->data)
		g_free(font->pvt->data);

	/* Chain up to the parent class */
	G_OBJECT_CLASS (vga_font_parent_class)->finalize(gobject);
}

/**
 * vga_font_set_chars:
 * @font: the VGA font object
 * @data: raw VGA font data for [partial] font
 * @start_c: first character @data describes
 * @end_c: last character @data describes
 *
 * Load the [partial] font into the font object.  A font must already be
 * loaded before calling this function (unless you know what you are doing).
 * The @data buffer must be large enough to account the character range
 * given.
 */
gboolean vga_font_set_chars(VGAFont * font, guchar *data, guchar start_c,
		       	guchar end_c)
{
	int bytes;
	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(font->pvt->data != NULL, FALSE);
	g_assert(font->height > 0 && font->width > 0);
	bytes = font->bytes_per_glyph * (end_c - start_c + 1);
	memcpy(font->pvt->data + start_c, data, bytes);

	return TRUE;
}

/**
 * vga_font_load:
 * @font: the VGA font object
 * @data: raw VGA font data for entire font
 * @width: width, in pixels, of the font
 * @height: height, in pixels, of the font
 *
 * Load the given font data/parameters into the font object.
 *
 * Returns: TRUE on success.
 */
gboolean vga_font_load(VGAFont * font, guchar * data, int width, int height)
{
	font->height = height;
	font->width = width;
	font->bytes_per_glyph = width * height / 8;
	if (font->pvt->data)
	{
		g_free(font->pvt->data);
	}
	font->pvt->data = g_malloc(width * height * 32);
	return vga_font_set_chars(font, data, 0, 255);
}

/* Determine the dimensions of the characters given the font image length */
/* I haven't proved it mathematically but it should only be able to get one */
/* correct result with this algorithm. */
static void
determine_char_size(int fontlen, int *width, int *height)
{
	int bits_per_char = (fontlen / 256) * 8;
	int h, w;
	for (h = 1; h < 65; h++)
		for (w = 1; w < 65; w++)
		{
			if ((w * h) == bits_per_char && (2 * w == h))
			{
				*height = h;
				*width = w;
				return;
			}
		}
}

guchar *
vga_font_get_glyph_data(VGAFont *font, int glyph)
{
	g_return_val_if_fail(font != NULL, NULL);
	g_return_val_if_fail(font->pvt->data != NULL, NULL);
	g_assert(font->height > 0 && font->width > 0);
	
	return (font->pvt->data + (font->bytes_per_glyph * glyph));
}

/**
 * vga_font_load_from_file:
 * @font: the VGA font object
 * @fname: the filename of the VGA font file to load.
 *
 * Load a VGA font from a raw VGA font data file.
 */
gboolean vga_font_load_from_file(VGAFont * font, gchar * fname)
{
	int filesize, height = 0, width = 0;
	FILE * f;
	guchar * buf;
	gboolean result;
	size_t count;

	/* Determine font parameters and read file into memory buffer */
	f = fopen(fname, "rb");
	fseek(f, 0, SEEK_END);
	filesize = ftell(f);
	fseek(f, 0, SEEK_SET);
	determine_char_size(filesize, &width, &height);
	if (height == 0 || width == 0)
	{
		fclose(f);
		return FALSE;
	}
	buf = g_malloc(filesize);
	count = fread(buf, filesize, 1, f);
	fclose(f);

	if (count != 1)
		return FALSE;

	result = vga_font_load(font, buf, width, height);
	g_free(buf);

	return result;
}

/**
 * vga_font_load_default:
 * @font: the VGA font object
 *
 * Load the default font into VGA font object.
 */
void vga_font_load_default(VGAFont * font)
{
	vga_font_load(font, default_font, 8, 16);
}


/**
 * vga_font_pixels:
 * @font: the VGA font object
 * 
 * Returns: The number of pixels per character
 */
int vga_font_pixels(VGAFont * font)
{
	return font->width * font->height;
}


#ifdef USE_DEPRECATED_GDK
/* Bit flip/mirror a byte -- maybe there's a better way to do this */
static guchar bitflip(guchar c)
{
	int i;
	guchar y = 0, filter = 128;

	for (i = 7; i > 0; i -= 2)
	{
		y = (filter & c) >> i | y;
		filter = filter >> 1;
	}

	for (i = 1; i < 8; i += 2)
	{
		y = (filter & c) << i | y;
		filter = filter >> 1;
	}

	return y;
}

/**
 * vga_font_get_bitmap:
 * @font: the VGA font object
 *
 * Create a XBM representation of the font, with all characters in one column
 * with the same width as the character width of the font (so, for a standard
 * 8x16 VGA font, the returned bitmap is 8x4096).
 * 
 * Returns: The new GdkBitmap object representing the rendered font
 */
GdkBitmap * vga_font_get_bitmap(VGAFont * font, GdkWindow * win)
{
	unsigned char * xbm;
	int i, size;
	/* The XBM format is pretty similar to the raw VGA font, just
	 * mirrored horizontally.  So we flip/mirror the bits */
	/* We only really support 8-bit width fonts */
	g_assert(font->width == 8);

	size = font->height * 256;
	xbm = g_malloc(size);
	/* bitflip each byte */
	for (i = 0; i < size; i++)
		*(xbm + i) = bitflip(*(font->pvt->data + i));
	
	return gdk_bitmap_create_from_data(win, xbm, font->width, size);
}
#endif

/*
 * Called by Cairo when a scaled-font needs to be created for a user font-face.
 * @cr: a cairo context, in font space (not used by caller)
 * @extents: font extens to fill in, in font space. 
 * Returns: CAIRO_STATUS_SUCCESS upon success, or an error status on error.
 */
static cairo_status_t
vga_font_cairo_init		(cairo_scaled_font_t *scaled_font,
				 cairo_t *cr,
				 cairo_font_extents_t *extents)
{
	cairo_font_face_t *user_font;
	VGAFont *vf;
	cairo_status_t res = CAIRO_STATUS_SUCCESS;
	
	user_font = cairo_scaled_font_get_font_face(scaled_font);
	vf = cairo_font_face_get_user_data(user_font, &vgafont_closure_key);

	/* This monospace font has no distance between baselines */
	extents->ascent = 0.0;
	extents->descent = 0.0;
	extents->height = 0.0;
	extents->max_x_advance = vf->width;
	extents->max_y_advance = 0;
	printf("vga_font_cairo_init()\n");
	
	return res;
}

/*
 * Called by Cairo when a user scaled-font needs to render a glyph.
 * This callback should draw the glyph with the code @glyph to the
 * cairo context @cr.
 * See http://cairographics.org/manual/cairo-User-Fonts.html#cairo-user-scaled-font-render-glyph-func-t
 */
static cairo_status_t
vga_font_cairo_render_glyph	(cairo_scaled_font_t *scaled_font,
				 unsigned long glyph,
				 cairo_t *cr,
				 cairo_text_extents_t *extents)
{
	cairo_font_face_t *user_font;
	VGAFont *vf;
	guchar *data;
	cairo_glyph_t cairo_glyph;
	int i, j;
	int row, col;
	cairo_status_t res = CAIRO_STATUS_SUCCESS;
	printf("vga_font_cairo_render_glyph(glyph='%c')\n", (guchar) glyph);

	cairo_glyph.index = glyph;
	cairo_glyph.x = 0;
	cairo_glyph.y = 0;

	user_font = cairo_scaled_font_get_font_face(scaled_font);
	vf = cairo_font_face_get_user_data(user_font, &vgafont_closure_key);

	extents->x_bearing = 0.0;
	/*extents->y_bearing = -vf->height;*/
	extents->y_bearing = 0.0;
	extents->width = vf->width * 1.0;
	extents->height = vf->height * 1.0;
	extents->x_advance = vf->width;
	extents->y_advance = 0.0;

	data = vga_font_get_glyph_data(vf, glyph);
	/*
	 * Currently we do a stupid obvious solution to this.
	 * Consider optimizing later (may not be a huge deal since
	 * these glyphs are supposed to get cached anyway).
	 *
	 * We draw 1.0x1.0 squares for each pixel in the font.
	 * For a 8x16 font the space used will be from X=[0.0, 8.0],
	 * and Y=[0.0, 16.0].
	 */
	for (i = 0; i < vf->bytes_per_glyph; i++) {
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
	
	return res;
}

/*
 * Called by Cairo to convert an input Unicode character into a single glyph.
 * This is used by the cairo_show_text() operation.
 * It's an easier-to-use version of the text_to_glyphs callback.
 */
static cairo_status_t
vga_font_cairo_unicode_to_glyph	(cairo_scaled_font_t *scaled_font,
				 unsigned long unicode,
				 unsigned long *glyph)
{
	cairo_status_t res = CAIRO_STATUS_SUCCESS;

	//printf("vga_font_cairo_unicode_to_glyph(unicode=%d=0x%x)\n", unicode, unicode);
	//if (unicode > 127)
	//	*glyph = 0x3f;
	if (unicode < 256)
		*glyph = unicode;
	else if (unicode >= 0xe000 && unicode <= 0xe0ff) {
		*glyph = unicode & 0x00ff;
	}
	else
		*glyph = 0x3f;	/* Default to question mark */
	
	return res;
}

/*
 * Called by Cairo to convert input text to an array of glyphs.
 * This is used by the cairo_show_text() operation.
 * User fonts have full control on glyphs and their positions, so
 * things like kerning is permitted.  We don't need that though since
 * we're 100% fixed width.
 *
 * @utf8: String of text encoded in UTF-8
 * @utf8_len: length of utf8 in bytes
 * @glyphs: Pointer to array of glyphs to fill, in font space
 * @num_glyphs: Pointer to number of glyphs
 * @clusters: Pointer to array of cluster mapping information to fill, or NULL
 * @num_clusters: Pointer to number of clusters
 * @cluster_flags: Pointer to location to store cluster flags corresponding
 *                 to the output @clusters
 */
static cairo_status_t
vga_font_cairo_text_to_glyphs	(cairo_scaled_font_t *scaled_font,
				 const char *utf8,
				 int utf8_len,
				 cairo_glyph_t **glyphs,
				 int *num_glyphs,
				 cairo_text_cluster_t **clusters,
				 int *num_clusters,
				 cairo_text_cluster_flags_t *cluster_flags)
{
	cairo_font_face_t *user_font;
	VGAFont *vf;
	cairo_status_t res = CAIRO_STATUS_SUCCESS;

	user_font = cairo_scaled_font_get_font_face(scaled_font);
	vf = cairo_font_face_get_user_data(user_font, &vgafont_closure_key);

	return res;
}
