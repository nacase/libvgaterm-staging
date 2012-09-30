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


#include "terminal.h"

static void terminal_stuff_key_input(guchar * s, gint length);
static void terminal_process_control_key(guint keyval);

static gboolean term_key_pressed = FALSE;

/* FIXME: TODO: TEMP HACK FOR LINK ERRORS */
/* I WANT TO DECOUPLE VGATERM FROM THE TELNET STUFF */
#if 1
#define telnet_reset(x) while (0) { x }
#define telnet_send_negotiations(x) while (0) { x }
#define telnet_process_char(x) while (0) { }
#endif

void terminal_new_connection(void)
{
	telnet_reset();
	telnet_send_negotiations();
}

/* Process data received from the socket to be outputted on the terminal */
void terminal_process_output_char(guchar c)
{
	telnet_process_char(c);
}

void terminal_process_output(guchar * s, int bytes)
{
	int i = 0;
	while (i < bytes)
		telnet_process_char(s[i++]);
}


static void terminal_stuff_key_input(guchar * s, gint length)
{
	if (length > 0) term_key_pressed = TRUE;

/* TODO: FIXME: */
#if 0
	if (termix_connect_status() == STATUS_CONNECTED)
		termix_send_data(s, length);
#endif
}

void terminal_flush_input(void)
{
	term_key_pressed = FALSE;
}

gboolean terminal_key_pressed(void)
{
	gboolean result;
	result = term_key_pressed;
	term_key_pressed = FALSE;
	return result;
}


static void terminal_process_control_key(guint keyval)
{
	guchar s[2];
	
	/* Handle ctrl-a through ctrl-z as ascii #1 to ascii #26 */
	if (keyval >= GDK_a && keyval <= GDK_z)
	{
		s[0] = keyval - GDK_a + 1;
		s[1] = '\0';
	}
	terminal_stuff_key_input(s, 1);
}

/* i just cut n pasted the old handler in here and made minor changes..
 * not done yet.  so the real keyboard input handler will just pass
 * it off to this.  i think.*/
void terminal_process_input_key(GdkEventKey * event)
{
	GdkModifierType modifiers;
	gboolean modifier = FALSE;

	

	if (event->type == GDK_KEY_PRESS)
	{
		/* Read the modifiers */
		if (gdk_event_get_state((GdkEvent *)event, 
					&modifiers) == FALSE)
		{
			modifiers = 0;
		}

		g_debug("Modifier state = %x", event->state);

		switch (event->keyval)
		{	/*
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
				g_debug("Modifier press, keyval=%d", event->keyval);
				break;
			*/
			case GDK_Up:
				terminal_stuff_key_input("\033[A", 3);
				break;
			case GDK_Down:
				terminal_stuff_key_input("\033[B", 3);
				break;
			case GDK_Left:
				terminal_stuff_key_input("\033[D", 3);
				break;
			case GDK_Right:
				terminal_stuff_key_input("\033[C", 3);
				break;
			case GDK_Return:
				/* Assume binary mode - send CR */
				terminal_stuff_key_input("\r", 1);
				break;
			case GDK_BackSpace:
				terminal_stuff_key_input("\010", 1);
				break;
			case GDK_Delete:
				terminal_stuff_key_input("\x7f", 1);
				break;
			case GDK_Home:
				terminal_stuff_key_input("\033[H", 3);
				break;
			case GDK_End:
				terminal_stuff_key_input("\033[K", 3);
				break;
			case GDK_Insert:
				terminal_stuff_key_input("\026", 1);
				break;
			default:
				modifier = FALSE;
				g_debug("Key press: (keyval=%d): %s",
						event->keyval, event->string);
				
				/* CTRL key pressed? */
				/*if (event->state & GDK_CONTROL_MASK)
					terminal_process_control_key(event->keyval);
				else*/
					terminal_stuff_key_input(event->string,
						event->length);
				break;
		}

		//return TRUE;
	}
	//return FALSE;
}

void terminal_process_input(guchar * s)
{
	GdkEventKey evt;
	int i;

	/* Simulate a key press event structure */
	evt.type = GDK_KEY_PRESS;
	evt.string = g_strdup(s);
	evt.length = strlen(s);
	/* Convert LFs to CRs */
	for (i = 0; i < evt.length; i++)
	{
		if (evt.string[i] == '\n')
			evt.string[i] = '\r';
	}
	
	terminal_process_input_key(&evt);
	g_free(evt.string);
}


void terminal_dump_file(VGATerm *term, gchar * fname)
{
	FILE * f;
	guchar buf[4096];
	int n, i;

	f = fopen(fname, "rb");
	if (f == NULL)
		return;

	n = fread(buf, 1, 4096, f);
	while (n > 0)
	{
		for (i = 0; i < n; i++)
			vga_term_emu_writec(term, buf[i]);
		n = fread(buf, 1, 4096, f);
	}
	fclose(f);
}
