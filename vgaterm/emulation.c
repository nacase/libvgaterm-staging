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
 *  ANSI/vt100/Avatar/TextFX emulation for the VGA terminal.
 *
 *  This module basically extends the VGATerm widget with new output
 *  methods.  Based on ideas from Iniquity BBS's emulator, some of which
 *  originally came from Turbo Pascal SWAG.  The ANSI and TextFX support
 *  are most complete (since they're what really matters).
 */

#include "emulation.h"

#define TFX_NUM_UPALS	3

/* Attribute flags for vt100 */
#define AVT_DEFAULT 0
#define AVT_BOLD 1
#define AVT_LOWINT 2
#define AVT_ULINE 4
#define AVT_BLINK 8
#define AVT_REVERSE 16
#define AVT_INVIS 32


typedef guchar PalData[192];

typedef struct
{
	/* Individual emulation enablers */
	gboolean ansi, vt100, avatar, textfx;
	
	int tfx_stage;
	guchar tfx_param[4096];
	guchar tfx_cmd;
	int tfx_num;		/* param length for tfx_cmd */
	guchar tfx_def_attr;
	guchar tfx_save_x, tfx_save_y, tfx_save_attr;
	VGAPalette * tfx_user_pal[TFX_NUM_UPALS];

	GString * ansi_code;
	guchar ansi_save_x, ansi_save_y;
	guchar ansi_esc;
	
	guchar avt_cmd, avt_stage, avt_par1, avt_par2;

	GString * vt_code;
	guchar vt_stage, vt_cmd, vt_save_x, vt_save_y, vt_save_attr;
	guchar vt_attr;
	guchar vt_buf[3];

} EmuData;

	
static void vt_init(EmuData *data);
static void ansi_init(EmuData *data);
static void ansi_detect_reply(VGATerm *term);

void vga_term_emu_init(VGATerm *term)
{
	EmuData * emu;
	int i;
	/* Initialize extended widget properties */
	emu = g_malloc(sizeof(EmuData));
	g_object_set_data(G_OBJECT(term), "emu_data", emu);

	emu->tfx_stage = -1;
	emu->tfx_def_attr = 0x07;
	emu->tfx_save_x = 1;
	emu->tfx_save_y = 1;
	emu->tfx_save_attr = emu->tfx_def_attr;
	emu->textfx = TRUE;
	
	for (i = 0; i < TFX_NUM_UPALS; i++)
		emu->tfx_user_pal[i] = VGA_PALETTE(vga_palette_new());

	vt_init(emu);
	ansi_init(emu);
}

/* Get a palette object pointer from the character given */
/* Return NULL on error */
static
VGAPalette * tfx_get_pal(VGATerm *term, guchar c)
{
	EmuData * data;
	VGAText *vga;
	data = g_object_get_data(G_OBJECT(term), "emu_data");
	vga = VGA_TEXT(term);

	if (c >= '1' && c < ('1' + TFX_NUM_UPALS))
		return data->tfx_user_pal[c-'1'];
	else
	if (c >= 'A' && c <= 'E')
	{
		switch (c)
		{
			case 'A':
				return vga_palette_stock(PAL_WHITE);
			case 'B':
				return vga_palette_stock(PAL_BLACK);
			case 'C':
				return vga_get_palette(vga);
			case 'D':
				return vga_palette_stock(PAL_DEFAULT);
			case 'E':
				return vga_palette_stock(PAL_GREYSCALE);
		}
	}
	return NULL;
}

static
void tfx_command(VGATerm *term, EmuData * data, guchar cmd)
{
	guchar x, z, c;
	VGAFont *font;
	VGAText *vga;
	VGAPalette *pal, *p;
	gboolean b = FALSE;
	gchar *s;

	vga = VGA_TEXT(term);

	switch (cmd)
	{
		case 'a':
			vga_term_gotoxy(term, vga_term_wherex(term),
					vga_term_wherey(term) - 1);
			break;
		case 'A':
			vga_term_gotoxy(term, vga_term_wherex(term),
				vga_term_wherey(term) - data->tfx_param[0]);
			break;
		case 'b':
			vga_term_gotoxy(term, vga_term_wherex(term),
					vga_term_wherey(term) + 1);
			break;
		case 'B':
			vga_term_gotoxy(term, vga_term_wherex(term),
				vga_term_wherey(term) + data->tfx_param[0]);
			break;
		case 'c':
			vga_term_gotoxy(term, vga_term_wherex(term) + 1,
					vga_term_wherey(term));
			break;
		case 'C':
			vga_term_gotoxy(term,
				vga_term_wherex(term) + data->tfx_param[0],
				vga_term_wherey(term));
			break;
		case 'd':
			vga_term_gotoxy(term, vga_term_wherex(term) - 1,
					vga_term_wherey(term));
			break;
		case 'D':
			vga_term_gotoxy(term,
				vga_term_wherex(term) - data->tfx_param[0],
				vga_term_wherey(term));
			break;
		case 'E':
			/* We would send <esc>ENVgtermix v1.00<null> */
			/* I think we should simulate keypresses here
			 * on the widget */
			break;
		case 'F':
			font = vga_get_font(vga);
			if (vga_font_load(font, data->tfx_param, 8, 16))
			{
				vga_refresh_font(vga);
				vga_refresh(vga);
			}
			else
				g_error("Unable to load TextFX font");
			data->tfx_stage = -1;
			break;
		case 'G':
			font = vga_get_font(vga);
			if (vga_font_set_chars(font, &(data->tfx_param[2]),
				data->tfx_param[0], data->tfx_param[1]+1))
			{
				vga_refresh_font(vga);
				vga_refresh(vga);
			}
			else
				g_error("Unable to load TextFX font");
			break;
		case 'h':
			vga_term_gotoxy(term, 1, 1);
			break;
		case 'H':
			vga_term_gotoxy(term, data->tfx_param[0],
					data->tfx_param[1]);
			break;
		case 'i':
			vga_term_set_attr(term, data->tfx_save_attr);
			break;
		case 'I':
			data->tfx_save_attr = term->textattr;
			break;
		case 'j':
			vga_term_set_attr(term, data->tfx_def_attr);
			vga_term_clrscr(term);
			break;
		case 'J':
			vga_term_clrscr(term);
			break;
		case 'k':
			vga_term_set_attr(term, data->tfx_def_attr);
			vga_term_clreol(term);
			break;
		case 'K':
			vga_term_clreol(term);
			break;
		case 'l':
			data->tfx_def_attr = data->tfx_param[0];
			break;
		case 'M':
			vga_term_set_attr(term, data->tfx_param[0]);
			break;
		case 'n':
			vga_cursor_set_visible(vga, FALSE);
			break;
		case 'N':
			vga_cursor_set_visible(vga, TRUE);
			break;
		case 'p':
			pal = vga_get_palette(vga);
			p = tfx_get_pal(term, data->tfx_param[1]);
			if (p && p != pal)
			{
				g_object_unref(pal);
				vga_set_palette(vga,
						vga_palette_dup(p));
				vga_refresh(vga);
			}
			break;
		case 'P':
			vga_palette_load(vga_get_palette(vga),
					data->tfx_param, 192);
			vga_refresh(vga);
			break;
		case 'Q':
			c = data->tfx_param[0];
			if (c >= '1' && c < ('1' + TFX_NUM_UPALS))
			{
				pal = vga_get_palette(vga);
				g_free(data->tfx_user_pal[c-'1']);
				data->tfx_user_pal[c-'1'] =
					vga_palette_dup(pal);
				g_debug("Current palette saved to User Palette %c (stored at %p)", c, data->tfx_user_pal[c-'1']);
			}
			break;
		case 'r':
			g_debug("TextFX: repeat command.  char %d, %d times",
					data->tfx_param[0], data->tfx_param[1]);
			s = g_strnfill(data->tfx_param[1], data->tfx_param[0]);
			vga_term_write(term, s);
			g_free(s);
			/*
			for (z = 0; z < data->tfx_param[1]; z++)
				vga_term_writec(term, data->tfx_param[0]);
			*/
			break;
		case 'R':
			pal = vga_get_palette(vga);
			vga_palette_set_reg(pal, data->tfx_param[0],
					data->tfx_param[1],
					data->tfx_param[2],
					data->tfx_param[3]);
			vga_refresh(vga);
			break;
		case 's':
			vga_term_gotoxy(term, data->tfx_save_x,
					data->tfx_save_y);
			break;
		case 'S':
			data->tfx_save_x = vga_term_wherex(term);
			data->tfx_save_y = vga_term_wherey(term);
			break;
		case 't':
			vga_term_scroll_down(term, vga_term_wherey(term),
					1);
			break;
		case 'T':
			vga_term_scroll_down(term, vga_term_wherey(term),
					data->tfx_param[0]);
			break;
		case 'u':
			vga_term_scroll_up(term, vga_term_wherey(term),
					1);
			break;
		case 'U':
			vga_term_scroll_up(term, vga_term_wherey(term),
					data->tfx_param[0]);
			break;
		case 'V':
			// would put string <esc>TFX<#2>
			break;
		case 'W':
			vga_term_window(term, data->tfx_param[0],
					data->tfx_param[1],
					data->tfx_param[2],
					data->tfx_param[3]);
			break;
		case 'X':
			g_debug("Morph from %c to %c", data->tfx_param[0],
					data->tfx_param[1]);
			
			/* Start pal */
			pal = tfx_get_pal(term, data->tfx_param[0]);
			
			/* End pal */
			p = tfx_get_pal(term, data->tfx_param[1]);
			if (data->tfx_param[2])
				x = MAX(63 / data->tfx_param[2], 1);
			else x = 0;
			g_debug("Morphing from %p to %p", pal, p);
			if (p && pal && x > 0)
			{
				vga_palette_copy_from(vga_get_palette(vga),
						pal);
				vga_refresh(vga);
				pal = vga_get_palette(vga);
				for (z = 0; z < 63; z++)
				{
					vga_palette_morph_to_step(pal, p);
					if (z % x == 0)
						vga_refresh(vga);
				}
			}
			break;
		case 'z':
			if (data->tfx_param[0])
				vga_term_window(term, 1, 1, 80, 25);
			if (data->tfx_param[1])
			{
				vga_palette_load_default(
						vga_get_palette(vga));
				b = TRUE; /* refresh */
			}
			if (data->tfx_param[2])
			{
				vga_font_load_default(vga_get_font(vga));
				vga_refresh_font(vga);
				b = TRUE; /* refresh */
			}
			if (b)
				vga_refresh(vga);
			break;
		case 'Z':
			vga_term_window(term, 1, 1, 80, 25);
			vga_set_icecolor(vga, TRUE);
			vga_term_set_attr(term, data->tfx_def_attr);
			vga_palette_load_default(vga_get_palette(vga));
			vga_term_clrscr(term);
			vga_font_load_default(vga_get_font(vga));
			vga_refresh_font(vga);
			break;
	}
	data->tfx_stage = -1;
}

static
void tfx_out(VGATerm *term, EmuData * data, guchar c)
{
	if (data->tfx_stage == -1)
	{
		if (c == 27)
			data->tfx_stage = 0;
		else
			vga_term_writec(term, c);
	}
	else
	if (data->tfx_stage == 0)
	{
		data->tfx_cmd = c;
		switch (c)
		{
			case 'a':case 'b':case 'c':case 'd':case 'E':
			case 'h':case 'i':case 'I':case 'j':case 'J':
			case 'k':case 'K':case 'n':case 'N':case 's':
			case 'S':case 't':case 'u':case 'V':case 'Z':
				data->tfx_num = 0;
				break;
			case 'A':case 'B':case 'C':case 'D':case 'l':
			case 'M':case 'p':case 'Q':case 'T':case 'U':
				data->tfx_num = 1;
				break;
			case 'G':
			case 'H':
			case 'r':
				data->tfx_num = 2;
				break;
			case 'z':
			case 'X':
				data->tfx_num = 3;
				break;
			case 'R':
			case 'W':
				data->tfx_num = 4;
				break;
			case 'P':
				data->tfx_num = 192;
				break;
			case 'F':
				data->tfx_num = 4096;
				break;
			default:
				data->tfx_stage = -1;
				vga_term_writec(term, c);
		}
		if (data->tfx_stage == data->tfx_num)
			tfx_command(term, data, data->tfx_cmd);
		else data->tfx_stage++;
	} /* stage == 0 */
	else
	{
		data->tfx_param[data->tfx_stage - 1] = c;
		if (data->tfx_stage == data->tfx_num)
			tfx_command(term, data, data->tfx_cmd);
		else data->tfx_stage++;
	}
}

static
void vt_init(EmuData * data)
{
	data->vt100 = TRUE;
	data->vt_code = g_string_new(NULL);
	data->vt_stage = 0;
	data->vt_cmd = 0;
	data->vt_save_x = 1;
	data->vt_save_y = 1;
	data->vt_buf[0] = data->vt_buf[1] = data->vt_buf[2] = 0;
	data->vt_save_attr = 0x07;
	data->vt_attr = AVT_DEFAULT;
	/* need to set the terminal text attr here? */
}

/**
 * Get a regular VGA textmode attribute given a VT attribute byte.
 * Note: The VT attribute byte is my own creation to manage the flags and
 * isn't really part of any standard
 */
static
guchar get_vt_color_attr(guchar at)
{
	guchar new_attr = 0x00;
	
	if (at & AVT_BLINK)
		new_attr = BLINK(new_attr);
	if (at & AVT_REVERSE)
	{
		new_attr = SETBG(new_attr, 1);
		if (at & AVT_ULINE && at & AVT_BOLD)
			new_attr = SETBG(new_attr, 15);
		else
		if (at & AVT_BOLD)
			new_attr = SETBG(new_attr, 7);
		else
		if (at & AVT_ULINE)
			new_attr = SETBG(new_attr, 9);
	}
	else
	{
		if (at & AVT_ULINE && at & AVT_BOLD)
			new_attr = SETFG(new_attr, 11);
		else
		if (at & AVT_BOLD)
			new_attr = SETFG(new_attr, 15);
		else
		if (at & AVT_ULINE)
			new_attr = SETFG(new_attr, 8);
		else
			new_attr = SETFG(new_attr, 7);
	}

	return new_attr;
}

static
void vt_reset(EmuData * data)
{
	g_string_truncate(data->vt_code, 0);
	data->vt_cmd = 0;
}

/* like the pascal val() function, basically atoi() with error code. */
/* code is the 1-based index of where an invalid character is found */
static void val(char * s, long * i, int * code)
{
	int x;
	*i = atoi(s);
	*code = 0;
   
	/* if (i > 0) return; */

	for(x = 0; x < strlen(s); x++)
	{
		if (!g_ascii_isdigit(s[x]))
		{
			*code = x + 1;
			break;
		}
	}
}


static
guchar emu_parse_num(GString * code)
{
	long i;
	int j;
	GString * temp;
	
	val(code->str, &i, &j);
	if (j == 0)
		g_string_truncate(code, 0);
	else
	{
		/* Delete numeric-valid portion of code, use it. */
		temp = g_string_new(code->str);
		g_string_truncate(temp, j - 1);
		g_string_erase(code, 0, j);
		val(temp->str, &i, &j);
		g_string_free(temp, TRUE);
	}
	return i;
}

static
guchar vt_num(EmuData * data)
{
	return emu_parse_num(data->vt_code);
}

/* Go to the next tab position */
static
void vt_tab(VGATerm *term)
{
	EmuData * data;
	int cols;
	guchar x = vga_term_wherex(term) + 1;
	
	data = g_object_get_data(G_OBJECT(term), "emu_data");
	cols = vga_term_cols(term);
	if (x > cols)
		x = cols;
	else
	while (x < cols && ((x-1) % 8 != 0))
		x++;
	vga_term_gotoxy(term, x, vga_term_wherey(term));
}

static
void vt_process_attr(VGATerm *term, EmuData * data, guchar c)
{
	switch (c)
	{
		case 0:
			vga_term_set_attr(term, 0x07);
			data->vt_attr = AVT_DEFAULT;
			break;
		case 1:
			vga_term_set_attr(term,
					BRIGHT(vga_term_get_attr(term)));
			data->vt_attr = data->vt_attr | AVT_BOLD;
			if (AVT_REVERSE & data->vt_attr ||
				       AVT_ULINE & data->vt_attr)
			{
				vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			}
			break;
		case 2:
			data->vt_attr = data->vt_attr | AVT_LOWINT;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 4:
			data->vt_attr = data->vt_attr | AVT_ULINE;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 5:
			data->vt_attr = data->vt_attr | AVT_BLINK;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 7:
			data->vt_attr = data->vt_attr | AVT_REVERSE;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 8:
			data->vt_attr = data->vt_attr | AVT_INVIS;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 30:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+0);
			break;
		case 31:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+4);
			break;
		case 32:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+2);
			break;
		case 33:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+6);
			break;
		case 34:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+1);
			break;
		case 35:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+5);
			break;
		case 36:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+3);
			break;
		case 37:
			vga_term_set_attr(term,
					(vga_term_get_attr(term) & 0xF8)+7);
			break;
		case 40:
			vga_term_set_bg(term, 0);
			break;
		case 41:
			vga_term_set_bg(term, 4);
			break;
		case 42:
			vga_term_set_bg(term, 2);
			break;
		case 43:
			vga_term_set_bg(term, 6);
			break;
		case 44:
			vga_term_set_bg(term, 1);
			break;
		case 45:
			vga_term_set_bg(term, 5);
			break;
		case 46:
			vga_term_set_bg(term, 3);
			break;
		case 47:
			vga_term_set_bg(term, 7);
			break;
	}
}

static
void vt_out(VGATerm *term, EmuData * data, guchar c)
{
	int x;

	if (data->vt_cmd == 1)
		switch (c)
		{
			case '[':
				data->vt_cmd = 2;
				break;
			case 'c':
				vt_init(data);
				break;
			case 'D':
				vga_term_scroll_down(term,
						vga_term_wherey(term), 1);
				vt_reset(data);
				break;
			case 'M':
				vga_term_scroll_up(term,
						vga_term_wherey(term), 1);
				vt_reset(data);
				vga_term_gotoxy(term, 1,1);
				break;
			case 'E':
				vga_term_writec(term, 10);
				break;
			case '7':
				data->vt_save_x = vga_term_wherex(term);
				data->vt_save_y = vga_term_wherey(term);
				data->vt_save_attr = vga_term_get_attr(term);
				vt_reset(data);
				break;
			case '8':
				vga_term_gotoxy(term, data->vt_save_x,
						data->vt_save_y);
				vga_term_set_attr(term, data->vt_save_attr);
				vt_reset(data);
				break;
			case 'A':
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term)-1);
				vt_reset(data);
				break;
			case 'B':
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term)+1);
				vt_reset(data);
				break;
			case 'C':
				vga_term_gotoxy(term,
						vga_term_wherex(term)+1,
						vga_term_wherey(term));
				vt_reset(data);
				break;
			/*case 'D':   not sure if this is standard
				vga_term_gotoxy(term,
						vga_term_wherex(term)-1,
						vga_term_wherey(term));
				vt_reset(data);
				break;
				*/
			case 'H':
				vga_term_gotoxy(term, 1, 1);
				vt_reset(data);
				break;
			case 'K':
				vga_term_clreol(term);
				vt_reset(data);
				break;
			case '(':
				data->vt_cmd = 3;
				break;
			case ')':
				data->vt_cmd = 4;
				break;
			default:
				vt_reset(data);
		}
	else
	if (data->vt_cmd == 2)
	{
		if ((c >= '0' && c <= '9') || c == ';')
			g_string_append_c(data->vt_code, c);
		else
		switch (c)
		{
			case 'm':
				if (data->vt_code->len == 0)
					g_string_assign(data->vt_code, "0");
				while (data->vt_code->len > 0)
				{
					vt_process_attr(term, data,
							vt_num(data));
					vt_reset(data);
				}
				break;
			case 'A':
				x = vt_num(data);
				if (x == 0)
					x = 1;
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term)-x);
				vt_reset(data);
				break;
			case 'B':
				x = vt_num(data);
				if (x == 0)
					x = 1;
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term)+x);
				vt_reset(data);
				break;
			case 'C':
				x = vt_num(data);
				if (x == 0)
					x = 1;
				vga_term_gotoxy(term,
						vga_term_wherex(term)+x,
						vga_term_wherey(term));
				vt_reset(data);
				break;
			case 'D':
				x = vt_num(data);
				if (x == 0)
					x = 1;
				vga_term_gotoxy(term,
						vga_term_wherex(term)-x,
						vga_term_wherey(term));
				vt_reset(data);
				break;
			case 'H':
			case 'f':
				x = vt_num(data);
				if (x == 0)
					vga_term_gotoxy(term, 1, 1);
				else
					vga_term_gotoxy(term,
							vt_num(data), x);
				vt_reset(data);
				break;
			case 'J':
				switch (vt_num(data))
				{
					case 0: 
						vga_term_clrdown(term);
						break;
					case 1: 
						vga_term_clrup(term);
						break;
					case 2:
						vga_term_clrscr(term);
						break;
				}
				vt_reset(data);
				break;
			case 'K':
				if (vt_num(data) == 0)
					vga_term_clreol(term);
				vt_reset(data);
				break;
			case 'r':
				vga_term_window(term, 1,
						vt_num(data), 80,
						vt_num(data));
				vga_term_gotoxy(term, 1, 1);
				vt_reset(data);
				break;
			default:
				vt_reset(data);
		}
	}
	else
	/* Keyboard/character set codes */
	if (data->vt_cmd == 3 || data->vt_cmd == 4)
	{
		vt_reset(data);
		/* unfinished?  check spec */
	}
	else
	switch (c)
	{
		case 27:
			data->vt_cmd = 1;
			break;
		case 9:
			vt_tab(term);
			break;
		case 12:
			vga_term_clrscr(term);
			break;
		case '[':
			data->vt_buf[2] = '[';
			vga_term_writec(term, c);
			break;
		case 15:
			/* ??? */
			break;
		case 2:
			/* Toggle bold attribute */
			data->vt_attr = data->vt_attr ^ AVT_BOLD;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 22:
			/* Toggle reverse video attribute */
			data->vt_attr = data->vt_attr ^ AVT_REVERSE;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		case 31:
			/* Toggle underline attribute */
			data->vt_attr = data->vt_attr ^ AVT_ULINE;
			vga_term_set_attr(term,
					get_vt_color_attr(data->vt_attr));
			break;
		default:
			vga_term_writec(term, c);
	}
}

static
void ansi_init(EmuData * data)
{
	data->ansi_code = g_string_new(NULL);
	data->ansi_save_x = 1;
	data->ansi_save_y = 1;
	data->ansi_esc = 0;
	data->avt_cmd = 0;
	data->avt_stage = 0;
	data->avt_par1 = 0;
	data->avt_par2 = 0;
	data->tfx_stage = -1;
	data->vt100 = FALSE;
}

static
void ansi_detect_reply(VGATerm *term)
{
	guchar str[8];
	g_snprintf(str, 8, "\033[%d;%dR",
			vga_term_wherey(term), vga_term_wherex(term));
/* TODO: FIXME: */
#if 0
	termix_send_data(str, strlen(str));
#endif
}


static
void ansi_cmd(VGATerm *term, EmuData *data, guchar c)
{
	int col, y;
	guchar attr;
	
	if ((c >= '0' && c <= '9') || c == ';')
		g_string_append_c(data->ansi_code, c);
	else
	switch (c)
	{
		case '?':
			break;
		case 'h':
			data->ansi_esc = 0;
			break;
		case 'm':
			data->ansi_esc = 0;
			attr = vga_term_get_attr(term);
			if (data->ansi_code->len == 0)
				g_string_assign(data->ansi_code, "0");
			while (data->ansi_code->len > 0)
			{
				col = emu_parse_num(data->ansi_code);
				switch (col)
				{
					case 0:
						attr = 0x07;
						break;
					case 1:
						attr = BRIGHT(attr);
						break;
					case 5:
						attr = BLINK(attr);
						break;
					case 7: /* reverse video */
						y = attr;
						attr = (attr << 4) & 0x70;
						attr = attr | (y >> 4);
						break;
					case 30:
						attr = attr & 0xF8;
						break;
					case 31:
						attr = (attr & 0xF8) + RED;
						break;
					case 32:
						attr = (attr & 0xF8) + GREEN;
						break;
					case 33:
						attr = (attr & 0xF8) + BROWN;
						break;
					case 34:
						attr = (attr & 0xF8) + BLUE;
						break;
					case 35:
						attr = (attr & 0xF8) + MAGENTA;
						break;
					case 36:
						attr = (attr & 0xF8) + CYAN;
						break;
					case 37:
						attr = (attr & 0xF8) + GREY;
						break;
					case 40:
						attr = SETBG(attr, BLACK);
						break;
					case 41:
						attr = SETBG(attr, RED);
						break;
					case 42:
						attr = SETBG(attr, GREEN);
						break;
					case 43:
						attr = SETBG(attr, BROWN);
						break;
					case 44:
						attr = SETBG(attr, BLUE);
						break;
					case 45:
						attr = SETBG(attr, MAGENTA);
						break;
					case 46:
						attr = SETBG(attr, CYAN);
						break;
					case 47:
						attr = SETBG(attr, GREY);
						break;
				}
			}
			vga_term_set_attr(term, attr);
			break;
		case 'H':
		case 'f':
			data->ansi_esc = 0;
			y = emu_parse_num(data->ansi_code);
			vga_term_gotoxy(term, 
					emu_parse_num(data->ansi_code), y);
			break;
		case 'A':
			data->ansi_esc = 0;
			y = emu_parse_num(data->ansi_code);
			if (y == 0)
				y = 1;
			y = vga_term_wherey(term) - y;
			vga_term_gotoxy(term,
					vga_term_wherex(term), y);
			break;
		case 'B':
			data->ansi_esc = 0;
			y = emu_parse_num(data->ansi_code);
			if (y == 0)
				y = 1;
			y += vga_term_wherey(term);
			vga_term_gotoxy(term, vga_term_wherex(term), y);
			break;
		case 'C':
			data->ansi_esc = 0;
			y = emu_parse_num(data->ansi_code);
			if (y == 0)
				y = 1;
			y += vga_term_wherex(term);
			vga_term_gotoxy(term, y, vga_term_wherey(term));
			break;
		case 'D':
			data->ansi_esc = 0;
			y = emu_parse_num(data->ansi_code);
			if (y == 0)
				y = 1;
			y = vga_term_wherex(term) - y;
			vga_term_gotoxy(term, y, vga_term_wherey(term));
			break;
		case 's':
			data->ansi_esc = 0;
			data->ansi_save_x = vga_term_wherex(term);
			data->ansi_save_y = vga_term_wherey(term);
			break;
		case 'u':
			data->ansi_esc = 0;
			vga_term_gotoxy(term, data->ansi_save_x,
					data->ansi_save_y);
			break;
		case 'J':
			data->ansi_esc = 0;
			vga_term_clrscr(term);
			break;
		case 'K':
			data->ansi_esc = 0;
			vga_term_clreol(term);
			break;
		case 'n':
			data->ansi_esc = 0;
			ansi_detect_reply(term);
			break;
		default:
			data->ansi_esc = 0;
	}
}

void vga_term_emu_writec(VGATerm *term, guchar c)
{
	EmuData *data;
	gchar *s;
	data = g_object_get_data(G_OBJECT(term), "emu_data");

	/*
	if (c > 31 && c < 127)
		g_print("%c", c);
	else
		g_print("[#%d]", c);
	*/
	
/*	if (c == '[')
	{
		g_print("<stg %d>", data->ansi_esc);
	} */

	/* vt100 is exclusive */
	if (data->vt100)
		vt_out(term, data, c);
	else
	if (data->tfx_stage > 0)
		tfx_out(term, data, c);
	else
	if (data->avt_cmd == 100)
	{
		if (data->avt_stage == 1)
		{
			data->avt_par1 = c;
			data->avt_stage++;
		}
		else
		if (data->avt_stage == 2)
		{
			s = g_strnfill(c, data->avt_par1);
			vga_term_write(term, s);
			g_free(s);
			data->avt_cmd = 0;
		}
	}
	else
	if (data->ansi_esc > 0)
	{
		switch (data->ansi_esc)
		{
			case 1:
				if (c == '[')
				{
					data->ansi_esc = 2;
					g_string_assign(data->ansi_code, "");
				}
				else
				if (data->textfx)
				{
					data->ansi_esc = 0;
					data->tfx_stage = 0;
					tfx_out(term, data, c);
				}
				else
					data->ansi_esc = 0;
				break;
			case 2:
				ansi_cmd(term, data, c);
				break;
			default:
				data->ansi_esc = 0;
				g_string_assign(data->ansi_code, "");
		}
	}
	else
	if (data->avt_cmd > 1)
	{
		switch (data->avt_cmd)
		{
			case 2:
				vga_term_set_attr(term, c);
				data->avt_cmd = 0;
				break;
			case 3:
				switch (data->avt_stage)
				{
					case 1:
						data->avt_par1 = c;
						data->avt_stage++;
						break;
					case 2:
						data->avt_par2 = c;
						vga_term_gotoxy(term,
								data->avt_par1,
								data->avt_par2);
						data->avt_cmd = 0;
					default:
						data->avt_cmd = 0;
				}
			default:
				data->avt_cmd = 0;
		}
	}
	else
	if (data->avt_cmd == 1)
	{
		switch (c)
		{
			case 1:
				data->avt_cmd = 2;
				data->avt_stage = 1;
				break;
			case 2:
				vga_term_set_attr(term,
					BLINK(vga_term_get_attr(term)));
				data->avt_cmd = 0;
				break;
			case 3:
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term) - 1);
				data->avt_cmd = 0;
				break;
			case 4:
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term) + 1);
				data->avt_cmd = 0;
				break;
			case 5:
				vga_term_gotoxy(term,
						vga_term_wherex(term) - 1,
						vga_term_wherey(term));
				data->avt_cmd = 0;
				break;
			case 6:
				vga_term_gotoxy(term,
						vga_term_wherex(term),
						vga_term_wherey(term) + 1);
				data->avt_cmd = 0;
				break;
			case 7:
				vga_term_clreol(term);
				data->avt_cmd = 0;
				break;
			case 8:
				data->avt_cmd = 3;
				data->avt_stage = 1;
				break;
			default:
				data->avt_cmd = 0;
		}
	}
	else
	{
		switch (c)
		{
			/* Avatar/0 commands */
			/* 
			 * The 'Repeat' command has been disabled because
			 * too many BBSes like to use this character literally
			 * as an arrow.  Figure out a better solution later
			 * (unfortunately this command is not escaped in
			 * Avatar).
			 */
		/*	case 25:
				data->avt_cmd = 100;
				data->avt_stage = 1;
				break;
		*/
			case 22:
				data->avt_cmd = 1;
				break;
			case 27:
				data->ansi_esc = 1;
				break;
			case 9:
				vt_tab(term);
				break;
			case 12:
				vga_term_clrscr(term);
				break;
			default:
				//g_print("vga_term_writec(term, %c); ", c);
				vga_term_writec(term, c);
				//g_print("done\n");
		}
	}
}

void vga_term_emu_write(VGATerm *term, gchar * s)
{
	int i = 0;
	/* FIXME: Could optimize out some unnecessary cursor movement */
	while (s[i])
		vga_term_emu_writec(term, s[i++]);
}

void vga_term_emu_writeln(VGATerm *term, gchar *s)
{
	vga_term_emu_write(term, s);
	vga_term_emu_writec(term, 13);
	vga_term_emu_writec(term, 10);
}

gchar * vga_term_emu_vtkey(VGATerm *term, guchar c)
{
	/*
	 * f1 = ^[A
	 * f2 = ^[B
	 * ..
	 * f10 = ^[J
	 * up = ^[A
	 * down = ^[B
	 * right = ^[C
	 * left = ^[D
	 * del = ^D
	 * else s = ""
	 */
	return "<FIXME: SPECIAL KEY>";
}
