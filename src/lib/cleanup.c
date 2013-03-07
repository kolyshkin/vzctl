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

#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

#include "cleanup.h"

typedef list_head_t cleanup_ctx;
static cleanup_ctx *g_cleanup_ctx;

cleanup_ctx *get_cleanup_ctx(void)
{
	if (g_cleanup_ctx)
		return g_cleanup_ctx;
	g_cleanup_ctx = malloc(sizeof(cleanup_ctx));
	if (g_cleanup_ctx == NULL)
		return NULL;
	list_head_init(g_cleanup_ctx);
	return g_cleanup_ctx;
}

struct vzctl_cleanup_handler *add_cleanup_handler(cleanupFN f, void *data)
{
	struct vzctl_cleanup_handler *h;
	cleanup_ctx *ctx;

	h = malloc(sizeof(struct vzctl_cleanup_handler));
	if (h == NULL)
		return NULL;
	h->f = f;
	h->data = data;

	ctx = get_cleanup_ctx();
	list_add(&h->list, ctx);
	return h;
}

void del_cleanup_handler(struct vzctl_cleanup_handler *h)
{
	if (h == NULL)
		return;
	list_del(&h->list);
	free(h);
}

void run_cleanup(void)
{
	struct vzctl_cleanup_handler *it;
	cleanup_ctx *ctx;

	ctx = get_cleanup_ctx();
	list_for_each(it, ctx, list) {
		it->f(it->data);
	}
}

void free_cleanup()
{
	struct vzctl_cleanup_handler *it, *tmp;
	cleanup_ctx *ctx;

	ctx = get_cleanup_ctx();
	list_for_each_safe(it, tmp, ctx, list) {
		del_cleanup_handler(it);
	}
}

void cleanup_kill_process(void *data)
{
	int pid = *(int *) data;

	kill(-pid, SIGTERM);
}
