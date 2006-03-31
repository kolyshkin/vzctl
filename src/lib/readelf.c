/*
 * Copyright (C) 2000-2005 SWsoft. All rights reserved.
 *
 * This file may be distributed under the terms of the Q Public License
 * as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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

	if (stat(file, &st))
		return -1;
	if (!S_ISREG(st.st_mode))
		return -1;	
	fd = open(file, O_RDONLY);
	if (fd < 0)
		return -1;
	nbytes = read(fd, (void *) &elf_hdr, sizeof(elf_hdr));
	close(fd);
	if (nbytes < sizeof(elf_hdr))
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
