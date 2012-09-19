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
#ifndef _CAP_H_
#define _CAP_H_

#include <linux/types.h>
#include "types.h"

struct env_param;
typedef __u32 cap_t;

#undef CAP_TO_MASK
#define CAP_TO_MASK(x) (1 << ((x) & 31)) /* mask for indexed __u32 */
#define cap_raise(c, flag) (c |=  CAP_TO_MASK(flag))
#define cap_lower(c, flag) (c &=  CAP_TO_MASK(flag))

/* Data structure for capability mask see /usr/include/linux/capability.h
 */
typedef struct {
	unsigned long on;
	unsigned long off;
} cap_param;

/** Add capability name to capability mask.
 *
 * @param name		capability name.
 * @param mask		capability mask.
 * @return		0 on success.
 */
int get_cap_mask(char *name, unsigned long *mask);

/** Apply capability mask to CT.
 *
 * @param veid		CT ID.
 * @param env		CT environment parameters.
 * @param cap		capability mask.
 * @param is_upstream	set to 1 for non-OpenVZ kernel.
 * @return		0 on success.
 */
int vps_set_cap(envid_t veid, struct env_param *env, cap_param *cap,
		int is_upstream);

/** Merge capabilities and return in string format.
 *
 * @param new		new capability mask.
 * @param old		old capamility mask.
 * @param buf		merged capabilities in string format.
 * @return
 */
void build_cap_str(cap_param *new, cap_param *old, const char *delim,
		char *buf, int len);

#endif
