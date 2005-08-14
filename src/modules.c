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
		if (mod->fname != NULL)
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

int add_module(char *fname, struct mod_action *action, const char *name)
{
	void *h;
	void *(*fn) (void);

	struct mod_info *mod_info;

	if ((h = open_dlib(fname)) == NULL) {
		logger(0, 0, "Unable to open %s: %s", fname, dlerror());
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

	dir = opendir(MOD_DIR);
	if (!dir) {
		logger(2, errno, "Unable to open " MOD_DIR);
		return;
	}
	while ((ent = readdir(dir)) != NULL)  {
		snprintf(fname, sizeof(fname), "%s/%s", MOD_DIR, ent->d_name);
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
