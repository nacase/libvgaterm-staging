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
 *  Generic scrollback buffer that supports lines of varying lengths,
 *  and takes up a fixed amount of memory.
 */

#ifndef __SCROLLBUF_H__
#define __SCROLLBUF_H__

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "cbuf.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
	int max_bytes;
	int max_lines;

	int line_count;		/* Current line count */

	CircBuf *index_buf;	/* Line records */
	CircBuf *buf;		/* Log data itself */
} ScrollBuf;

typedef struct {
	int bytes;
	int offset;
} LineRecord;

ScrollBuf *	scrollbuf_new		(int max_bytes, int max_lines);
void		scrollbuf_destroy	(ScrollBuf *sbuf);
int		scrollbuf_line_count	(ScrollBuf *sbuf);
void		scrollbuf_add_line	(ScrollBuf *sbuf, unsigned char *line,
					 int bytes);
unsigned char *	scrollbuf_get_line	(ScrollBuf *sbuf, int index,
					 int *bytes);
void		scrollbuf_check_clean	(ScrollBuf *sbuf);
void		scrollbuf_dump		(ScrollBuf *sbuf);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __SCROLLBUF_H__ */
