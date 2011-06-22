/*
 *  Copyright (C) 2010-2011, Parallels, Inc. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "bitmap.h"

static int bitmap_find_first_bit(const unsigned long *maskp, int nmaskbits)
{
	int i, n;
	int nmasklongs = BITS_TO_LONGS(nmaskbits);

	for (i = 0; i < nmasklongs; i++) {
		if (maskp[i] != 0)
			break;
	}
	if (i == nmasklongs)
		return nmaskbits;
	i *= BITS_PER_LONG;
	n = i + BITS_PER_LONG;
	n = MIN(n, nmaskbits);
	do {
		if (bitmap_test_bit(i, maskp))
			break;
	} while (++i < n);
	return i;
}

static int bitmap_find_next_bit(const unsigned long *maskp, int nmaskbits, int offset)
{
	int n;

	if (offset % BITS_PER_LONG != 0) {
		n = (BIT_WORD(offset) + 1) * BITS_PER_LONG;
		n = MIN(n, nmaskbits);
		while (offset < n) {
			if (bitmap_test_bit(offset, maskp))
				return offset;
			offset++;
		}
	}
	if (offset >= nmaskbits)
		return nmaskbits;
	return offset + bitmap_find_first_bit(
		maskp + BIT_WORD(offset), nmaskbits - offset);
}

int bitmap_find_first_zero_bit(const unsigned long *maskp, int nmaskbits)
{
	int i, n;
	int nmasklongs = BITS_TO_LONGS(nmaskbits);

	for (i = 0; i < nmasklongs; i++) {
		if (~maskp[i] != 0)
			break;
	}
	if (i == nmasklongs)
		return nmaskbits;
	i *= BITS_PER_LONG;
	n = i + BITS_PER_LONG;
	n = MIN(n, nmaskbits);
	do {
		if (!bitmap_test_bit(i, maskp))
			break;
	} while (++i < n);
	return i;
}

static int bitmap_find_next_zero_bit(const unsigned long *maskp,
			      int nmaskbits, int offset)
{
	int n;

	if (offset % BITS_PER_LONG != 0) {
		n = (BIT_WORD(offset) + 1) * BITS_PER_LONG;
		n = MIN(n, nmaskbits);
		while (offset < n) {
			if (!bitmap_test_bit(offset, maskp))
				return offset;
			offset++;
		}
	}
	if (offset >= nmaskbits)
		return nmaskbits;
	return offset + bitmap_find_first_zero_bit(
		maskp + BIT_WORD(offset), nmaskbits - offset);
}

static inline int parse_range(const char *str, int *a, int *b, char **endptr)
{
	if (!isdigit(*str))
		return -1;
	*a = *b = strtol(str, endptr, 10);
	if (**endptr == '-') {
		str = *endptr + 1;
		if (!isdigit(*str))
			return -1;
		*b = strtol(str, endptr, 10);
		if (*a > *b)
			return -1;
	}
	return 0;
}

/**
 * bitmap_parse - parse bitmap stored as a comma-separated list of decimal
 * numbers and ranges in the string @str and write it to the bitmap @maskp
 * containing at most @nmaskbits.
 * A range of bits is shown as two hyphen-separated decimal numbers with
 * the first number being the smallest and the second one being the largest
 * bit numbers set in the range.
 *
 * Returns 0 on success. On invalid input strings, -1 is returned,
 * and @errno is set appropriately.
 * Errors:
 * %EINVAL: invalid character in string or
 *          second number in range smaller than first
 * %ERANGE: bit number specified too large for mask
 */
int bitmap_parse(const char *str, unsigned long *maskp, int nmaskbits)
{
	int a, b;
	char *endptr;

	bitmap_zero(maskp, nmaskbits);
	do {
		if (parse_range(str, &a, &b, &endptr) != 0) {
			errno = EINVAL;
			return -1;
		}
		if (b >= nmaskbits) {
			errno = ERANGE;
			return -1;
		}
		for (; a <= b; a++)
			bitmap_set_bit(a, maskp);
		if (*endptr == ',')
			endptr++;
		str = endptr;
	} while (*str != '\0');
	return 0;
}

static inline int print_range(char *buf, unsigned int buflen, int a, int b)
{
	if (a == b)
		return snprintf(buf, buflen, "%d", a);
	return snprintf(buf, buflen, "%d-%d", a, b);
}

/**
 * bitmap_snprintf - convert bitmap to string.
 * @buf: buffer to store string
 * @buflen: size of @buf, in bytes
 * @maskp: bitmap to convert
 * @nmaskbits: size of bitmap, in bits
 *
 * For description of output format, see bitmap_parse().
 *
 * Returns the number of characters printed, excluding the trailing '\0'.
 * If the output was truncated due to the limit on the size of @buf than
 * the return value is the number of characters which would have been written
 * to the final string if enough space had been available.
 */
int bitmap_snprintf(char *buf, unsigned int buflen,
		    const unsigned long *maskp, int nmaskbits)
{
	int a, b;
	unsigned int len = 0;

	if (buflen > 0)
		buf[0] = '\0';
	a = bitmap_find_first_bit(maskp, nmaskbits);
	while (a < nmaskbits) {
		b = bitmap_find_next_zero_bit(maskp, nmaskbits, a + 1) - 1;
		if (len > 0)
			len += snprintf(buf + len,
					buflen > len ? buflen - len : 0, ",");
		len += print_range(buf + len,
				   buflen > len ? buflen - len : 0, a, b);
		a = bitmap_find_next_bit(maskp, nmaskbits, b + 1);
	}
	return len;
}
