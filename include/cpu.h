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
#ifndef	_CPU_H_
#define	_CPU_H_

#define MAXCPUUNITS             500000
#define MINCPUUNITS             8

/** Data structure for cpu parameters.
 */
typedef struct {
	unsigned long *limit;	/**< CPU usage for the VPS in percents. */
	unsigned long *weight;
	unsigned long *units;	/**< CPU weight in units for the VPS. */
} cpu_param;

/**  Apply cpu parameters on running VPS.
 *
 * @param h		VPS handler.
 * @param veid		VPS id.
 * @param cpu		cpu parameters.
 * @return		0 on success.
 */
int vps_set_cpu(vps_handler *h, envid_t veid, cpu_param *cpu);

/**  Apply cpu parameters on Host system.
 *
 * @param cpu		cpu parameters.
 * @return		0 on success.
 */
int hn_set_cpu(cpu_param *cpu);
#endif
