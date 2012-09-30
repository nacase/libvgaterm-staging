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


#ifndef __TERMINAL_H__
#define __TERMINAL_H__

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include "emulation.h"
#include "vgaterm.h"

void		terminal_flush_input		(void);
void		terminal_new_connection		(void);
gboolean	terminal_key_pressed		(void);
void		terminal_process_output_char	(guchar c);
void		terminal_process_output		(guchar * s, int bytes);
void		terminal_process_input_key	(GdkEventKey * event);
/*
void		terminal_process_input_char	(guchar c);
*/
void		terminal_process_input		(guchar * s);
void		terminal_dump_file		(VGATerm *term, gchar *fname);

#endif	/* __TERMINAL_H__ */
