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
#ifndef	_UB_H_
#define	_UB_H_

#include "types.h"

#define PROCUBC		"/proc/user_beancounters"

#ifndef UB_IPTENTRIES
#define UB_IPTENTRIES	23 /* Number of iptables rules */
#endif
#define UB_DUMMY	255

/** Data structure for UBC parameter.
 */
typedef struct {
        int res_id;		/**< UBC resource id. */
        unsigned long limit[2];	/**< UBC resource barrier:limit. */
} ub_res;

/** Data structure for array of UBC parameters.
 */
typedef struct {
        int num_res;		/**< Number of resources in array. */
        ub_res *ub;		/**< pointer on first element. */
} ub_param;

/** Static UBC structure
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
};

/** Apply UBC resources.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
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
int check_ub(ub_param *ub);
int get_ub_resid(char *name);
ub_res *get_ub_res(ub_param *ub, int res_id);
const char *get_ub_name(int res_id);
int add_ub_param(ub_param *ub, ub_res *res);
void free_ub_param(ub_param *ub);
void merge_ub(ub_param *dst, ub_param *src);
int vps_read_ubc(envid_t veid, ub_param *ub);
void ub2ubs(ub_param *ub, struct ub_struct *ubs);
void add_ubs_limit(struct ub_struct *ubs, int res_id, unsigned long *limit);
#endif
