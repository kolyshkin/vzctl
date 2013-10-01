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
#ifndef	_UB_H_
#define	_UB_H_

#include "types.h"
#include "vzerror.h"
#include "vzsyscalls.h"

#define PROCUBC		"/proc/user_beancounters"
#define PROC_BC_RES	"/proc/bc/resources"

#ifndef UB_KMEMSIZE
#define UB_KMEMSIZE	0	/* Unswappable kernel memory size including
				 * struct task, page directories, etc.
				 */
#endif
#ifndef UB_LOCKEDPAGES
#define UB_LOCKEDPAGES	1	/* Mlock()ed pages. */
#endif
#ifndef UB_PRIVVMPAGES
#define UB_PRIVVMPAGES	2	/* Total number of pages, counting potentially
				 * private pages as private and used.
				 */
#endif
#ifndef UB_SHMPAGES
#define UB_SHMPAGES	3	/* IPC SHM segment size. */
#endif
#ifndef UB_ZSHMPAGES
#define UB_ZSHMPAGES	4	/* Anonymous shared memory. */
#endif
#ifndef UB_NUMPROC
#define UB_NUMPROC	5	/* Number of processes. */
#endif
#ifndef UB_PHYSPAGES
#define UB_PHYSPAGES	6	/* All resident pages, for swapout guarantee. */
#endif
#ifndef UB_VMGUARPAGES
#define UB_VMGUARPAGES	7	/* Guarantee for memory allocation,
				 * checked against PRIVVMPAGES.
				 */
#endif
#ifndef UB_OOMGUARPAGES
#define UB_OOMGUARPAGES	8	/* Guarantees against OOM kill.
				 * Only limit is used, no accounting.
				 */
#endif
#ifndef UB_NUMTCPSOCK
#define UB_NUMTCPSOCK	9	/* Number of TCP sockets. */
#endif
#ifndef UB_NUMFLOCK
#define UB_NUMFLOCK	10	/* Number of file locks. */
#endif
#ifndef UB_NUMPTY
#define UB_NUMPTY	11	/* Number of PTYs. */
#endif
#ifndef UB_NUMSIGINFO
#define UB_NUMSIGINFO	12	/* Number of siginfos. */
#endif
#ifndef UB_TCPSNDBUF
#define UB_TCPSNDBUF	13	/* Total size of tcp send buffers. */
#endif
#ifndef UB_TCPRCVBUF
#define UB_TCPRCVBUF	14	/* Total size of tcp receive buffers. */
#endif
#ifndef UB_OTHERSOCKBUF
#define UB_OTHERSOCKBUF	15	/* Total size of other socket
				 * send buffers (all buffers for PF_UNIX).
				 */
#endif
#ifndef UB_DGRAMRCVBUF
#define UB_DGRAMRCVBUF	16	/* Total size of other socket
				 * receive buffers.
				 */
#endif
#ifndef UB_NUMOTHERSOCK
#define UB_NUMOTHERSOCK	17	/* Number of other sockets. */
#endif
#ifndef UB_DCACHESIZE
#define UB_DCACHESIZE	18	/* Size of busy dentry/inode cache. */
#endif
#ifndef UB_NUMFILE
#define UB_NUMFILE	19	/* Number of open files. */
#endif
#ifndef UB_IPTENTRIES
#define UB_IPTENTRIES	23	/* Number of iptables rules */
#endif
#ifndef UB_SWAPPAGES
#define UB_SWAPPAGES	24
#endif
#define UB_DUMMY	255

/** Data structure for UBC parameter.
 */
typedef struct {
	int res_id;		/**< UBC resource id. */
	unsigned long limit[2];	/**< UBC resource barrier:limit. */
} ub_res;

/** Data structure for UBC parameters.
 */
struct ub_struct {
	unsigned long *kmemsize;
	unsigned long *lockedpages;
	unsigned long *privvmpages;
	unsigned long *shmpages;
	unsigned long *numproc;
	unsigned long *physpages;
	unsigned long *vmguarpages;
	unsigned long *oomguarpages;
	unsigned long *numtcpsock;
	unsigned long *numflock;
	unsigned long *numpty;
	unsigned long *numsiginfo;
	unsigned long *tcpsndbuf;
	unsigned long *tcprcvbuf;
	unsigned long *othersockbuf;
	unsigned long *dgramrcvbuf;
	unsigned long *numothersock;
	unsigned long *numfile;
	unsigned long *dcachesize;
	unsigned long *numiptent;
	unsigned long *avnumproc;
	unsigned long *swappages;
	float *vm_overcommit;
};
typedef struct ub_struct ub_param;

#define FOR_ALL_UBC(func)	\
	func(kmemsize)		\
	func(lockedpages)	\
	func(privvmpages)	\
	func(shmpages)		\
	func(numproc)		\
	func(physpages)		\
	func(vmguarpages)	\
	func(oomguarpages)	\
	func(numtcpsock)	\
	func(numflock)		\
	func(numpty)		\
	func(numsiginfo)	\
	func(tcpsndbuf)		\
	func(tcprcvbuf)		\
	func(othersockbuf)	\
	func(dgramrcvbuf)	\
	func(numothersock)	\
	func(dcachesize)	\
	func(numfile)		\
	func(numiptent)		\
	func(swappages)

/** Apply UBC resources.
 *
 * @param h		CT handler.
 * @param veid		CT ID.
 * @param ubc		UBC parameters
 * @return		0 on success
 */
int vps_set_ublimit(vps_handler *h, envid_t veid, ub_param *ubc);
int set_ublimit(vps_handler *h, envid_t veid, ub_param *ubc);

/** Check that all required parameters are specified in ub.
 *
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int check_ub(vps_handler *h, ub_param *ub);

/** Add UBC resource in ub_res format to UBC struct
 *
 * @param ub		UBC parameters.
 * @param res		UBC resource in ub_res format.
 * @return		0 on success.
 */
int add_ub_param(ub_param *ub, ub_res *res);

/** Read UBC resources current usage from /proc/user_beancounters
 *
 * @param veid		CT ID.
 * @param ub		UBC parameters.
 * @return		0 on success.
 */
int vps_read_ubc(envid_t veid, ub_param *ub);
int get_ub_resid(char *name);
void add_ub_limit(struct ub_struct *ub, int res_id, unsigned long *limit);
int fill_vswap_ub(ub_param *cfg, ub_param *cmd);
void free_ub_param(ub_param *ub);
void merge_ub(ub_param *dst, ub_param *src);
int is_vswap_config(const ub_param *param);
#endif
