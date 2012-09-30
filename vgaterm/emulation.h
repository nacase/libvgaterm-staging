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
 *  methods.  Based on ideas from Iniquity BBS's emulator.
 */

#ifndef __EMULATION_H__
#define __EMULATION_H__

#include <stdlib.h>
#include "vgatext.h"
#include "vgaterm.h"


void vga_term_emu_init		(VGATerm *term);
void vga_term_emu_writec	(VGATerm *term, guchar c);
void vga_term_emu_write		(VGATerm *term, gchar *s);
gchar * vga_term_emu_vtkey	(VGATerm *term, guchar c);

#endif	/* __EMULATION_H__ */
