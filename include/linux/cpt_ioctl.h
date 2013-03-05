/*
 *  Copyright (C) 2000-2013, Parallels, Inc. All rights reserved.
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
#ifndef _CPT_IOCTL_H_
#define _CPT_IOCTL_H_ 1

#include <linux/types.h>
#include <linux/ioctl.h>

#define CPTCTLTYPE '-'
#define CPT_SET_DUMPFD	_IOW(CPTCTLTYPE, 1, int)
#define CPT_SET_STATUSFD _IOW(CPTCTLTYPE, 2, int)
#define CPT_SET_LOCKFD	_IOW(CPTCTLTYPE, 3, int)
#define CPT_SET_VEID	_IOW(CPTCTLTYPE, 4, int)
#define CPT_SUSPEND	_IO(CPTCTLTYPE, 5)
#define CPT_DUMP	_IO(CPTCTLTYPE, 6)
#define CPT_UNDUMP	_IO(CPTCTLTYPE, 7)
#define CPT_RESUME	_IO(CPTCTLTYPE, 8)
#define CPT_KILL	_IO(CPTCTLTYPE, 9)
#define CPT_JOIN_CONTEXT _IO(CPTCTLTYPE, 10)
#define CPT_GET_CONTEXT _IOW(CPTCTLTYPE, 11, unsigned int)
#define CPT_PUT_CONTEXT _IO(CPTCTLTYPE, 12)
#define CPT_SET_PAGEINFDIN _IOW(CPTCTLTYPE, 13, int)
#define CPT_SET_PAGEINFDOUT _IOW(CPTCTLTYPE, 14, int)
#define CPT_PAGEIND	_IO(CPTCTLTYPE, 15)
#define CPT_VMPREP	_IOW(CPTCTLTYPE, 16, int)
#define CPT_SET_LAZY	_IOW(CPTCTLTYPE, 17, int)
#define CPT_SET_CPU_FLAGS _IOW(CPTCTLTYPE, 18, unsigned int)
#define CPT_TEST_CAPS	_IOW(CPTCTLTYPE, 19, unsigned int)
#define CPT_TEST_VECAPS	_IOW(CPTCTLTYPE, 20, unsigned int)
#define CPT_SET_ERRORFD _IOW(CPTCTLTYPE, 21, int)
#define CPT_LINKDIR_ADD _IOW(CPTCTLTYPE, 24, int)
#define CPT_HARDLNK_ON  _IOW(CPTCTLTYPE, 25, int)
#define CPT_TEST_VERSION _IOW(CPTCTLTYPE, 26, int)
#define CPT_SET_LOCKFD2 _IOW(CPTCTLTYPE, 27, int)

#endif
