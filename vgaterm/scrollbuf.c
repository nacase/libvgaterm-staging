/*
 *  Copyright (C) 2011 Nate Case 
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

#include <stdio.h>
#include <stdlib.h>
#include "scrollbuf.h"
#include "cbuf.h"

#define DEBUG

/*
 * Scroll buffer uses two circular buffers:
 *   Log cbuf: Contains raw character cell byte data.
 *             Never 'read', only written.  So the cbuf 'get'
 *             pointer remains at the start of the buffer (index 0).
 *             Element size: sizeof(vga_charcell)
 *   Index cbuf: Contains row records indicating size and index into
 *               the log cbuf.
 *		 Not normally 'read'.  'get' pointer represents
 *		 oldest line record.  'put' pointer represents
 *               the most recently added line (+1).
 *             Element size: sizeof(RowRecord)
 *
 *   Data is accessed by peeking into the log cbuf.
 *   We must clean up the index cbuf by deleting old
 *   records when we find newer ones that overlap with
 *   it.  This can happen when the log cbuf overwrites.
 *   (we can't guarantee anything because the lines lengths
 *   can vary depending on how big the terminal was in the
 *   past).
 *
 *   We might want to consider changing the index cbuf into
 *   a doubly linked list.  The only problem with this is
 *   the lack of O(1) seek time.
 *   Maybe a simple array is best?  Then again, a cbuf is
 *   basically an array with some helpful indices.
 *
 *   To start out with, we can ignore this problem, and live
 *   with the fact that the very oldest end of the scrollback
 *   will probably be recycled / garbage lines :)
 */ 

/*
 * Create a scrollbuffer of size @max_bytes and a maximum number of
 * lines of @max_lines.  Since the size of a line can vary, either
 * the @max_bytes or @max_lines property can be the overall limiting
 * factor.
 */
ScrollBuf *scrollbuf_new(int max_bytes, int max_lines)
{
	ScrollBuf *sbuf;
	sbuf = malloc(sizeof(ScrollBuf));

	sbuf->max_bytes = max_bytes;
	sbuf->max_lines = max_lines;
	sbuf->line_count = 0;
	sbuf->buf = cbuf_new(1, max_bytes);
	sbuf->index_buf = cbuf_new(sizeof(LineRecord), max_lines);

	return sbuf;
}

void scrollbuf_destroy(ScrollBuf *sbuf)
{
	cbuf_destroy(sbuf->buf);
	cbuf_destroy(sbuf->index_buf);
	free(sbuf);
}

int scrollbuf_line_count(ScrollBuf *sbuf)
{
	return sbuf->line_count;
}

void scrollbuf_add_line(ScrollBuf *sbuf, unsigned char *line, int bytes)
{
	LineRecord r;

	r.bytes = bytes;
	r.offset = sbuf->buf->put - sbuf->buf->buf;
	cbuf_force_put(sbuf->buf, line, bytes);
	cbuf_force_put(sbuf->index_buf, &r, 1);
	if (sbuf->line_count < sbuf->max_lines)
		sbuf->line_count++;
#ifdef DEBUG
	printf("Add line (%d bytes, ofs=%d): ", bytes, r.offset);
	int i;
	for (i = 0; i < bytes; i += 2) {
		printf("%c", line[i]);
	}
	printf("\n");
	//printf("Put index = %d, line_count = %d\n", sbuf->buf->puti, sbuf->line_count);
	scrollbuf_get_line(sbuf, 0, NULL);
#endif
}

/*
 * Get pointer to a line within the scroll buffer.
 * @index is a number from 0..max lines, where 0 represents
 * @bytes gets set to the number of bytes in the line
 * the most recent line (bottom) of the buffer
 */
unsigned char *
scrollbuf_get_line(ScrollBuf *sbuf, int index, int *bytes)
{
	int ofs;
	LineRecord *rec;

	if (index >= sbuf->line_count)
		return NULL;

	rec = (LineRecord *) cbuf_peek_back(sbuf->index_buf, index+1);
	ofs = rec->offset;
#ifdef DEBUG
	printf("Get line %d: Offset = %d, size = %d, line count = %d: ", index, ofs, rec->bytes, sbuf->line_count);
	int i;
	unsigned char *line = sbuf->buf->buf + ofs;
	for (i = 0; i < rec->bytes; i += 2) {
		printf("%c", line[i]);
	}
	printf("\n");
	//printf("Line ptr: %p\n", (unsigned char *) sbuf->buf->buf + ofs);
	//printf("buf put ptr: %p\n", (unsigned char *) sbuf->buf->put);
#endif
	if (bytes != NULL)
		*bytes = rec->bytes;
	return (unsigned char *) sbuf->buf->buf + ofs;
}

void scrollbuf_check_clean(ScrollBuf *sbuf)
{
	/* Check to see if oldest line records are overlapped with
	by newer ones, and purge those line records */
}

/*
 * Debug function to dump the scroll buffer to stdout
 */
void scrollbuf_dump(ScrollBuf *sbuf)
{
	int i, j;
	char *line;
	unsigned char *linebuf;
	int bytes;

	line = malloc(4096);

	for (i = 0; i < sbuf->line_count; i++) {
		linebuf = scrollbuf_get_line(sbuf, i, &bytes);
		for (j = 0; j < bytes; j += 2) {
			line[j/2] = linebuf[j];
		}
		line[j/2-1] = '\0';
		printf("Line %d (size %d): %s\n", i, bytes, line);
	}
	free(line);
}
