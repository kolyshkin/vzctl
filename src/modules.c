/*
 *  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
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
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>

#include "modules.h"
#include "logger.h"

#define SH_LIB_EXT	".so"

static int add_mod(struct mod_action *action, struct mod_info *mod_info,
	char *fname)
{
	struct mod *mod;

	if (action->mod_list == NULL)
		action->mod_list = malloc(sizeof(*action->mod_list));
	else
		action->mod_list = realloc(action->mod_list,
			sizeof(*action->mod_list) * (action->mod_count + 1));
	if (action->mod_list == NULL)
		return -1;
	mod = &action->mod_list[action->mod_count];
	if (mod_info->alloc_data != NULL)
		mod->data = mod_info->alloc_data();
	else
		mod->data = NULL;
	mod->mod_info = mod_info;
	mod->fname = strdup(fname);
	if (mod_info->init != NULL)
		mod_info->init(mod->data);
	action->mod_count++;

	return 0;
}

void free_modules(struct mod_action *action)
{
	int i;
	struct mod *mod;

	if (action->mod_list == NULL)
		return;
	for (i = 0, mod = action->mod_list; i < action->mod_count; i++, mod++) {
		free(mod->fname);
		if (mod->mod_info != NULL && mod->mod_info->free_data != NULL)
			mod->mod_info->free_data(mod->data);
	}
	free(action->mod_list);
	action->mod_count = 0;
	action->mod_list = NULL;
}

static void *open_dlib(char *fname)
{
	return dlopen(fname, RTLD_NOW);
}

static void *find_dsym(void *h, char *symbol)
{
	return dlsym(h, symbol);
}

static void close_dlib(void *h)
{
	dlclose(h);
}

static int add_module(char *fname, struct mod_action *action, const char *name)
{
	void *h;
	void *(*fn) (void);

	struct mod_info *mod_info;

	if ((h = open_dlib(fname)) == NULL) {
		logger(-1, 0, "Unable to open %s: %s", fname, dlerror());
		return -1;
	}
	if ((fn = find_dsym(h, MOD_INFO_SYM)) != NULL) {
		mod_info = fn();
		if (is_mod_action(mod_info, name)) {
			logger(2, 0, "Add module %s", fname);
			mod_info->handle = h;
			add_mod(action, mod_info, fname);
		} else {
			close_dlib(h);
		}
	} else {
		logger(2, 0, "Warning: no " MOD_INFO_SYM " found in %s", fname);
		close_dlib(h);
	}
	return 0;
}

void init_modules(struct mod_action *action, const char *name)
{
	char *pext;
	DIR *dir;
	struct dirent *ent;
	struct stat st;
	char fname[256];

	dir = opendir(MODULESDIR);
	if (!dir) {
		return;
	}
	while ((ent = readdir(dir)) != NULL)  {
		snprintf(fname, sizeof(fname), "%s/%s", MODULESDIR, ent->d_name);
		if (stat(fname, &st))
			continue;
		/* FIxme: handle symlink
		 * add dup detection
		 */
		if (!S_ISREG(st.st_mode))
			continue;
		if ((pext = strrchr(ent->d_name, '.')) != NULL &&
			!strcmp(pext, SH_LIB_EXT))
		{
			add_module(fname, action, name);
		}
	}
	closedir(dir);
}
