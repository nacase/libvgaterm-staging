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
 *  Circular / ring buffer implementation that uses a pointer for each
 *  element.
 */

#ifndef __CBUF_H__
#define __CBUF_H__

#include <stdio.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
	unsigned char *get, *put;
	int geti, puti;
	size_t nmemb;			/* max # elements */
	size_t elem_size;		/* bytes per element */
	int size;			/* elem-size * nmemb */
	unsigned char *buf;
} CircBuf;

CircBuf *	cbuf_new	(size_t elem_size, size_t nmemb);
void		cbuf_destroy	(CircBuf *cbuf);
int		cbuf_unread	(CircBuf *cbuf);
int		cbuf_unwritten	(CircBuf *cbuf);
int		cbuf_get	(CircBuf *cbuf, void *dest, int count);
int		cbuf_force_put	(CircBuf *cbuf, void *src, int count);
int		cbuf_put	(CircBuf *cbuf, void *src, int count);
int		cbuf_peek	(CircBuf *cbuf, int ofs, void *dest,
				 int count);
void *		cbuf_peek_back	(CircBuf *cbuf, int n);
int		cbuf_unput	(CircBuf *cbuf, int count);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif	/* __CBUF_H__ */
