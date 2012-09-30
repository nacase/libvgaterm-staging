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
 */

#include "ui_callbacks.h"
#include <gdk/gdkkeysyms.h>
#include <stdio.h>

gint key_pressed(GtkWidget * widget, GdkEventKey * event)
{
	GdkModifierType modifiers;
	gboolean modifier = FALSE;
	FILE * f;
	guchar buf[11000];
	int i, n;
//	vga_term_writec(widget, 

	f = fopen("test.tfx", "rb");
	n = fread(buf, 1, 11000, f);
	fclose(f);
	g_print("read %d bytes\n", n);
	for (i = 0; i < n; i++)
		vga_term_emu_writec(widget, buf[i]);

	return FALSE;
	
	if (event->type == GDK_KEY_PRESS)
	{
		/* Read the modifiers */
		if (gdk_event_get_state((GdkEvent *)event, 
					&modifiers) == FALSE)
		{
			modifiers = 0;
		}

		/* Determine if this is just a modifier key */
		switch (event->keyval)
		{
			case GDK_Alt_L:
			case GDK_Alt_R:
			case GDK_Caps_Lock:
			case GDK_Control_L:
			case GDK_Control_R:
			case GDK_Eisu_Shift:
			case GDK_Hyper_L:
			case GDK_Hyper_R:
			case GDK_ISO_First_Group_Lock:
			case GDK_ISO_Group_Lock:
			case GDK_ISO_Group_Shift:
			case GDK_ISO_Last_Group_Lock:
			case GDK_ISO_Level3_Lock:
			case GDK_ISO_Level3_Shift:
			case GDK_ISO_Lock:
			case GDK_ISO_Next_Group_Lock:
			case GDK_ISO_Prev_Group_Lock:
			case GDK_Kana_Lock:
			case GDK_Kana_Shift:
			case GDK_Meta_L:
			case GDK_Meta_R:
			case GDK_Num_Lock:
			case GDK_Scroll_Lock:
			case GDK_Shift_L:
			case GDK_Shift_Lock:
			case GDK_Shift_R:
			case GDK_Super_L:
			case GDK_Super_R:
				modifier = TRUE;
				break;
			default:
				modifier = FALSE;
				break;
		}

		/* VTE hides the pointer unless it's a modifier here */

		/* Map the key to a sequence name? */
		/* some other processing probably */
		vga_term_emu_write(widget, event->string);
		return TRUE;
	}
	return FALSE;
}

void exec_clicked(GtkWidget * widget, gpointer user_data)
{
	GtkWidget * vga;
	int intensity, i, regs;
	guint16 step;
	VGAPalette * pal, * srcpal;
	int x, y;
	static guchar c = 65;
	long lxpy1, lxpy2;
	
	vga = GTK_WIDGET(user_data);
//	vga_set_icecolor(vga, !vga_get_icecolor(vga));
//	vga_put_string(vga, "Fading", 0x07, 37, 22);
	
/*	x = vga_cursor_x(vga);
	y = vga_cursor_y(vga);
	x++;
	if (x > 79)
	{
		y++;
		x = 0;
	}
	vga_cursor_move(vga, x, y);
	return;*/
//	vga_term_print(vga, "ABCDEFGHIJKLMNOPQRSTUVWXYZ_1234567890_!@#$%^&*() ");
	vga_term_print(vga, "%c", c++);
	return;
	pal = vga_get_palette(vga);
		
	srcpal = vga_palette_stock(PAL_BLACK);
	for (regs = 0; regs < 64; regs++)
	{
		vga_palette_morph_to_step(pal, srcpal);
		vga_refresh(vga);
	}
	srcpal = vga_palette_stock(PAL_DEFAULT);
	for (regs = 0; regs < 64; regs++)
	{
		vga_palette_morph_to_step(pal, srcpal);
		vga_refresh(vga);
	}
	
	vga_put_string(vga, "      ", 0x07, 37, 22);
	
	return;
	
	step = TO_GDK_RGB(1);
	for (regs = 0; regs < 63; regs++)
	{
		for (i = 0; i < PAL_REGS; i++)
		{
			pal->color[i].red = MAX(pal->color[i].red - step,0);
			pal->color[i].green = MAX(pal->color[i].green - step,0);
			pal->color[i].blue = MAX(pal->color[i].blue - step,0);
		}
		vga_refresh(vga);
	}
/* NEED a generic "pal_morph_to()" method that takes a single step toward
 * a destination palette.  then fading out would just be the specific case
 * of morph_to() where the destination palette is all black.  of course,
 * i totally don't have a problem with keeping a fade_out_step() function
 * either, to avoid storing a black palette in memory.  but we will anyway
 * for textfx.  all fading in textfx is done with palette morphing i think.
 * so that would cover everything we need.
 */	
#if 0			
	for (lxpy1 = 1; lxpy1 <= 65535; lxpy1++)
	{
		/* wrt */
		for (lxpy2 = 1; lxpy2 < 255; lxpy2++)
		{
			if (pal->color[lxpy2].red > 0)
				pal->color[lxpy2].red--;
			if (pal->color[lxpy2].green > 0)
				pal->color[lxpy2].green--;
			if (pal->color[lxpy2].blue > 0)
				pal->color[lxpy2].blue--;
		}
		vga_refresh(vga);
	}
#endif
#if 0
	for (intensity = 63; intensity >= 0; intensity--)
	{
		for (i = 0; i < PAL_REGS; i++)
		{
			pal->color[i].red =
			       MIN(TO_GDK_RGB(pal->color[i].red),
					       TO_GDK_RGB(intensity));
			pal->color[i].green =
			       MIN(TO_GDK_RGB(pal->color[i].green),
					       TO_GDK_RGB(intensity));
			pal->color[i].blue =
			       MIN(TO_GDK_RGB(pal->color[i].blue),
					       TO_GDK_RGB(intensity));
		}
		vga_refresh(vga);
	}
#endif
	
}
