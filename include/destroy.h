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
#ifndef	_DESTROY_H_
#define	_DESTROY_H_
#include "res.h"

int vps_destroy_dir(envid_t veid, char *dir, int layout);
int vps_destroy(vps_handler *h, envid_t veid, fs_param *fs,  cpt_param *cpt);
int del_dir(char *dir);
int destroydir(char *dir);
int destroy_dump(envid_t veid, const char *dumpdir);
#endif
