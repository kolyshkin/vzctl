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

#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <stdlib.h>
#include <string.h>

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))
#endif
#ifndef MIN
#define MIN(a, b)		((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b)		((a) > (b) ? (a) : (b))
#endif

#define BITS_PER_BYTE		8
#define BITS_PER_LONG		(BITS_PER_BYTE * sizeof(long))

#define BITS_TO_LONGS(nr)	DIV_ROUND_UP(nr, BITS_PER_LONG)

#define BIT_MASK(nr)		(1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)		((nr) / BITS_PER_LONG)

static inline unsigned long *alloc_bitmap(int nmaskbits)
{
	return malloc(BITS_TO_LONGS(nmaskbits) * sizeof(long));
}

static inline void bitmap_zero(unsigned long *maskp, int nmaskbits)
{
	memset(maskp, 0, BITS_TO_LONGS(nmaskbits) * sizeof(long));
}

static inline void bitmap_set_all(unsigned long *maskp, int nmaskbits)
{
	memset(maskp, -1, BITS_TO_LONGS(nmaskbits) * sizeof(long));
}

static inline int bitmap_test_bit(int nr, const unsigned long *maskp)
{
	return (maskp[BIT_WORD(nr)] & BIT_MASK(nr)) != 0;
}

static inline void bitmap_set_bit(int nr, unsigned long *maskp)
{
	maskp[BIT_WORD(nr)] |= BIT_MASK(nr);
}

static inline void bitmap_clear_bit(int nr, unsigned long *maskp)
{
	maskp[BIT_WORD(nr)] &= ~BIT_MASK(nr);
}

int bitmap_find_first_zero_bit(const unsigned long *maskp, int nmaskbits);
int bitmap_parse(const char *str, unsigned long *maskp, int nmaskbits);
int bitmap_snprintf(char *buf, unsigned int buflen,
		    const unsigned long *maskp, int nmaskbits);

#endif /* _BITMAP_H_ */
