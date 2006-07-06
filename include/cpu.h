/*
 *  Copyright (C) 2000-2006 SWsoft. All rights reserved.
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
	unsigned long *vcpus;	/**< number of CPUs available in the running VPS*/
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
