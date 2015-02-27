#  Copyright (C) 2000-2008, Parallels, Inc. All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

include $(top_srcdir)/pathsubst.am

script_SCRIPTS = \
	vps-create \
	vps-functions \
	vps-net_add \
	vps-netns_dev_add \
	vps-netns_dev_del \
	vps-net_del \
	vzevent-stop \
	vzevent-reboot \
	vps-pci	\
	vps-prestart \
	vps-cpt \
	vps-rst \
	vps-rst-env \
	vz-postinstall

EXTRA_DIST = \
	$(script_SCRIPTS:%=%.in)

CLEANFILES = \
	$(script_SCRIPTS)
