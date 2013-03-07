/*
 *  Copyright (C) 2007-2013, Parallels, Inc. All rights reserved.
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
#ifndef _VZCTL_CLEANUP_H_
#define _VZCTL_CLEANUP_H_

#include "list.h"

typedef void (* cleanupFN)(void *data);

struct vzctl_cleanup_handler {
	list_elem_t list;
	cleanupFN f;
	void *data;
};

struct vzctl_cleanup_handler *add_cleanup_handler(cleanupFN f, void *data);
void run_cleanup(void);
void del_cleanup_handler(struct vzctl_cleanup_handler *h);
void free_cleanup(void);
void cleanup_kill_process(void *data);

#endif /* _VZCTL_CLEANUP_H_ */
