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

#include <stdlib.h>
#include "cbuf.h"

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

/*
 * Note: We stupidly maintain two indices - one in pointer form and one
 * in 0-based integer form.  The integer form was added after the fact
 * to make certain uses more natural.  We should consider getting
 * rid of the pointer index completely since most things are simpler
 * without the pointer arithmetic
 */
CircBuf * cbuf_new(size_t elem_size, size_t nmemb)
{
	CircBuf *cbuf;
	
	cbuf = malloc(sizeof(CircBuf));
	if (cbuf == NULL)
		return NULL;

	cbuf->nmemb = nmemb;
	cbuf->elem_size = elem_size;
	cbuf->size = elem_size * nmemb;

	cbuf->buf = malloc(cbuf->size);
	if (cbuf->buf == NULL)
		return NULL;

	cbuf->put = cbuf->get = cbuf->buf;
	cbuf->puti = cbuf->geti = 0;

	return cbuf;
}

void cbuf_destroy(CircBuf *cbuf)
{
	if (cbuf == NULL)
		return;
	
	if (cbuf->buf)
		free(cbuf->buf);

	free(cbuf);
}

/*
 * Calculate how much unread space (in elements) is in the buffer
 */
int cbuf_unread(CircBuf *cbuf)
{
	return ((cbuf->size + (cbuf->put - cbuf->get)) % cbuf->size) /
		cbuf->elem_size;
}

/*
 * Calculate how much unwritten space (in elements) is in the buffer
 */
int cbuf_unwritten(CircBuf *cbuf)
{
	return ((cbuf->size - (cbuf->put - cbuf->get) - 1) % cbuf->size) /
		 cbuf->elem_size;
}

/*
 * Read @count elements from circular buffer.
 * @count is not necessarily the number of bytes read
 */
int cbuf_get(CircBuf *cbuf, void *dest, int count)
{
	int num_to_read;
	int bytes_read = 0;
	int room_to_end;

	num_to_read = MIN(cbuf_unread(cbuf), count);

	if (num_to_read == 0)
		return 0;

	room_to_end = (cbuf->buf + cbuf->size - cbuf->get) / cbuf->elem_size;

	if (num_to_read > room_to_end) {
		memcpy(dest, cbuf->get, room_to_end * cbuf->elem_size);
		memcpy(dest + (room_to_end*cbuf->elem_size), cbuf->buf,
		       (num_to_read-room_to_end) * cbuf->elem_size);
		cbuf->get = cbuf->buf +
			    (num_to_read-room_to_end) * cbuf->elem_size;
		cbuf->geti = num_to_read - room_to_end;
	} else {
		memcpy(dest, cbuf->get, num_to_read * cbuf->elem_size);
		cbuf->get += (num_to_read * cbuf->elem_size);
		cbuf->geti += num_to_read;
	}
	
	return num_to_read;
}

/*
 * Peek at data at current get pointer + the given offset without updating
 * the internal indexing pointers
 * @ofs and @count arein elements (not bytes)
 */
int cbuf_peek(CircBuf *cbuf, int ofs, void *dest, int count)
{
	int num_to_read;
	int bytes_read = 0;
	int room_to_end;
	int savei;
	unsigned char *save;

	/* Save get pointer */
	save = cbuf->get;
	savei = cbuf->geti;

	cbuf->get = cbuf->buf + (((cbuf->get - cbuf->buf) +
		    ofs*cbuf->elem_size) % cbuf->size);
	cbuf->geti = (cbuf->geti + ofs) % cbuf->nmemb;

	num_to_read = MIN(cbuf_unread(cbuf), count);

	if (num_to_read == 0) {
		cbuf->get = save;
		cbuf->geti = savei;
		return 0;
	}

	room_to_end = (cbuf->buf + cbuf->size - cbuf->get) / cbuf->elem_size;

	if (num_to_read > room_to_end) {
		memcpy(dest, cbuf->get, room_to_end * cbuf->elem_size);
		memcpy(dest + room_to_end * cbuf->elem_size, cbuf->buf,
		       (num_to_read-room_to_end) * cbuf->elem_size);
		cbuf->get = cbuf->buf + ((num_to_read-room_to_end) * cbuf->elem_size);
		cbuf->geti = num_to_read - room_to_end;
	} else {
		memcpy(dest, cbuf->get, num_to_read * cbuf->elem_size);
		cbuf->get += (num_to_read * cbuf->elem_size);
		cbuf->geti += num_to_read;
	}

	/* Restore get pointer */
	cbuf->get = save;
	cbuf->geti = savei;
	return num_to_read;
}

#if 0
/*
 * Search the buffer for the specified pattern.  Update the read/get
 * pointer to point to the start of the pattern.  Return the number of
 * bytes from the current get pointer to the pattern offset, or -1
 * if the pattern was not found.
 */
int cbuf_find(CircBuf * cbuf, unsigned char * pattern, int len)
{
	unsigned char * tmp;
	int ofs = 0;

	tmp = malloc(len);

	while (1) {
		if (cbuf_peek(cbuf, ofs, tmp, len) != len) {
			/* Not found */
			free(tmp);
			return -1;
		}

		if (memcmp(tmp, pattern, len) == 0)
			break;

		ofs++;
	}

	/* Found it */
	free(tmp);
	cbuf->get = cbuf->buf + (((cbuf->get - cbuf->buf) + ofs) % cbuf->size);
	return ofs;
}
#endif

/*
 * Put elements in buffer, without regard to unread data.
 * This may overwrite 'old' contents.
 */
int cbuf_force_put(CircBuf *cbuf, void *src, int count)
{
	int room_to_end;

	room_to_end = (cbuf->buf + cbuf->size - cbuf->put) / cbuf->elem_size;

//printf("cbuf_force_put(elem_size=%d): put = %p\n", cbuf->elem_size, cbuf->put);
	if (count > room_to_end) {
		memcpy(cbuf->put, src, room_to_end * cbuf->elem_size);
		memcpy(cbuf->buf, src + room_to_end * cbuf->elem_size,
		       (count-room_to_end) * cbuf->elem_size);
		cbuf->put = cbuf->buf +
			    (count - room_to_end) * cbuf->elem_size;
		cbuf->puti = count - room_to_end;
	} else {
		memcpy(cbuf->put, src, count * cbuf->elem_size);
		cbuf->put += (count * cbuf->elem_size);
		cbuf->puti += count;
	}

	return count;
}

/*
 * Put elements in buffer, but don't overwrite unread contents.
 */
int cbuf_put(CircBuf *cbuf, void *src, int count)
{
	int num_to_write;

	num_to_write = MIN(cbuf_unwritten(cbuf), count);
	return cbuf_force_put(cbuf, src, num_to_write);
}

/*
 * Remove @count elements most recently added to queue
 */
int cbuf_unput(CircBuf *cbuf, int count)
{
	if ((cbuf->put - cbuf->buf) < (count*cbuf->elem_size)) {
		count -= ((cbuf->put - cbuf->buf) / cbuf->elem_size);
		cbuf->put = (cbuf->buf + cbuf->size) - (count*cbuf->elem_size);
		cbuf->puti = cbuf->nmemb - count;
	} else {
		cbuf->put -= (count * cbuf->elem_size);
		cbuf->puti -= count;
	}
	return count;
}

/*
 * Peek back @n entries from the put pointer.  Pointer to
 * entry is returned (no data is copied).
 */
void * cbuf_peek_back(CircBuf *cbuf, int n)
{
	int i;

	i = (cbuf->puti - n) % cbuf->nmemb;

//printf("cbuf_peek_back(): %p\n", (void *) (cbuf->buf + (i * cbuf->elem_size)));
	return (void *) (cbuf->buf + (i * cbuf->elem_size));
}

#ifdef UNIT_TEST
/* Compile with: gcc cbuf.c -o cbuf-test -DUNIT_TEST */
#include <assert.h>
int main(void)
{
	CircBuf *cbuf;
	unsigned long val, val2;
	unsigned long vals[32];
	unsigned char c, c2;
	unsigned char s[10];
	int i, j;

	cbuf = cbuf_new(sizeof(val), 32);
	
	assert(cbuf != NULL);
	assert(cbuf->size == (sizeof(val) * 32));
	assert(cbuf_unread(cbuf) == 0);
	assert(cbuf_unwritten(cbuf) == 31);

	val = 0xaaaaaaaa;
	cbuf_put(cbuf, &val, 1);

	/* 
	 * Note:  unread() + unwritten() should always equal 31
	 * (one less than the size of the buffer)
	 */
	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 30);

	cbuf_peek(cbuf, 0, &val2, 1);
	
	assert(val2 == val);
	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 30);

	val = 0xdeadbeef;
	cbuf_put(cbuf, &val, 1);
	assert(cbuf_unread(cbuf) == 2);
	assert(cbuf_unwritten(cbuf) == 29);

	val2 = 0;
	cbuf_get(cbuf, &val2, 1);
	assert(val2 == 0xaaaaaaaa);
	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 30);

	val = 0xdeadbabe;
	cbuf_put(cbuf, &val, 1);
	val = 0x12345678;
	cbuf_put(cbuf, &val, 1);

	/* Write 10 elements */
	for (i = 0; i < 10; i++)
		vals[i] = i;
	cbuf_put(cbuf, vals, 10);

	assert(cbuf_unread(cbuf) == 13);
	assert(cbuf_unwritten(cbuf) == 18);

	val2 = 0;
	cbuf_get(cbuf, &val2, 1);
	assert(val2 == 0xdeadbeef);
	cbuf_get(cbuf, &val2, 1);
	assert(val2 == 0xdeadbabe);
	
	/* 
	 * Now let's try writing across the boundary.. we've written
	 * a total of 14 elements so far.  So let's write 18 now
	 * to make sure we're overwriting the first things we wrote.
	 */
	for (i = 0; i < 18; i++)
		vals[i] = 0xFFFF0000 | i;
	cbuf_put(cbuf, vals, 18);

	assert(cbuf_unread(cbuf) == 29);
	assert(cbuf_unwritten(cbuf) == 2);

	cbuf_get(cbuf, &val2, 1);
	assert(val2 == 0x12345678);

	cbuf_get(cbuf, vals, 10);
	for (i = 0; i < 10; i++)
		assert(vals[i] == i);
	assert(cbuf_unread(cbuf) == 18);
	assert(cbuf_unwritten(cbuf) == 13);

	/* Now read across boundary */
	cbuf_get(cbuf, vals, 18);
	for (i = 0; i < 18; i++)
		assert(vals[i] == (0xFFFF0000 | i));
	assert(cbuf_unread(cbuf) == 0);
	assert(cbuf_unwritten(cbuf) == 31);

	/* One more boundary test, writing 1 element at a time */
	for (i = 0; i < 31; i++) {
		val = (0xaaaa0000 | i);
		cbuf_put(cbuf, &val, 1);
	}

	assert(cbuf_unread(cbuf) == 31);
	assert(cbuf_unwritten(cbuf) == 0);

	for (i = 0; i < 31; i++) {
		cbuf_get(cbuf, &val, 1);
		assert(val == (0xaaaa0000 | i));
	}

	assert(cbuf_unread(cbuf) == 0);
	assert(cbuf_unwritten(cbuf) == 31);

	cbuf_destroy(cbuf);

	/* Test with character elements instead of longs */
	cbuf = cbuf_new(sizeof(c), 10);
	
	assert(cbuf != NULL);
	assert(cbuf->size == (sizeof(c) * 10));
	assert(cbuf_unread(cbuf) == 0);
	assert(cbuf_unwritten(cbuf) == 9);

	c = '@';
	cbuf_put(cbuf, &c, 1);

	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 8);

	cbuf_peek(cbuf, 0, &c2, 1);
	
	assert(c2 == c);
	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 8);

	c = 'a';
	cbuf_put(cbuf, &c, 1);
	assert(cbuf_unread(cbuf) == 2);
	assert(cbuf_unwritten(cbuf) == 7);

	c2 = '\0';
	cbuf_get(cbuf, &c2, 1);
	assert(c2 == '@');
	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 8);

	c = 'b';
	cbuf_put(cbuf, &c, 1);
	c = '#';
	cbuf_put(cbuf, &c, 1);

	cbuf_get(cbuf, &c, 1);
	assert(c == 'a');
	cbuf_get(cbuf, &c, 1);
	assert(c == 'b');

	assert(cbuf_unread(cbuf) == 1);
	assert(cbuf_unwritten(cbuf) == 8);

	/* Write 8 elements */
	for (i = 0; i < 8; i++)
		s[i] = (unsigned char) i;
	cbuf_put(cbuf, s, 8);

	assert(cbuf_unwritten(cbuf) == 0);
	assert(cbuf_unread(cbuf) == 9);

	c = '\0';
	cbuf_get(cbuf, &c, 1);
	assert(c == '#');

	for (i = 0; i < 8; i++)
		s[i] = '0';

	cbuf_get(cbuf, s, 8);
	for (i = 0; i < 8; i++)
		assert(s[i] == (unsigned char) i);


	assert(cbuf_unread(cbuf) == 0);
	assert(cbuf_unwritten(cbuf) == 9);

	cbuf_destroy(cbuf);	

	printf("Unit test PASSED\n");
	return 0;
}
#endif
