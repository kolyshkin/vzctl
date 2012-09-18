/*
 *  Copyright (C) 2000-2009, Parallels, Inc. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>

#include "readelf.h"

#define EI_NIDENT	16
#define ELFMAG		"\177ELF"
#define OLFMAG		"\177OLF"

#define ELFCLASSNONE	0
#define ELFCLASS32	1
#define ELFCLASS64	2


struct elf_hdr_s {
	uint8_t ident[EI_NIDENT];
	uint16_t type;
	uint16_t machine;
};

static inline int check_elf_magic(const uint8_t *buf)
{
	if (memcmp(buf, ELFMAG, 4) &&
		memcmp(buf, OLFMAG, 4))
	{
		return -1;
	}
	return 0;
}

int get_arch_from_elf(const char *file)
{
	int fd, nbytes, class;
	struct stat st;
	struct elf_hdr_s elf_hdr;

	fd = open(file, O_RDONLY|O_NOCTTY);
	if (fd < 0)
		return -1;
	if (fstat(fd, &st)) {
		close(fd);
		return -1;
	}
	if (!S_ISREG(st.st_mode)) {
		close(fd);
		return -1;
	}
	nbytes = read(fd, (void *) &elf_hdr, sizeof(elf_hdr));
	close(fd);
	if (nbytes < (int)sizeof(elf_hdr))
		return -1;
	if (check_elf_magic(elf_hdr.ident))
		return -1;
	class = elf_hdr.ident[4];
	switch (class) {
	case ELFCLASS32:
		return elf_32;
	case ELFCLASS64:
		return elf_64;
	}
	return elf_none;
}
