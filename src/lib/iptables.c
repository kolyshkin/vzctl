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

#include <asm/timex.h>
#include <linux/vzcalluser.h>

#include "iptables.h"

unsigned long get_ipt_mask(unsigned long mask)
{
	if (!mask)
		return VE_IP_DEFAULT;
	return mask; 
}
