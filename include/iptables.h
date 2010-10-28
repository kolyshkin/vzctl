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
#ifndef	_IPTABLES_H_
#define _IPTABLES_H_

struct iptables_s {
	char *name;
	unsigned long long id;
	unsigned long long mask;
};

struct iptables_s *find_ipt(const char *name);
void ipt_mask2str(unsigned long long mask, char *buf, int size);
unsigned long long get_ipt_mask(unsigned long long mask);

#endif //_IPTABLES_H_
