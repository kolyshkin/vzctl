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
#ifndef	_IPTABLES_H_
#define _IPTABLES_H_

struct iptables_s {
	char *name;
	unsigned long id;
	unsigned long long mask;
};

struct iptables_s *find_ipt(const char *name);
void ipt_mask2str(unsigned long mask, char *buf, int size);
unsigned long long get_ipt_mask(unsigned long mask);

#endif //_IPTABLES_H_
