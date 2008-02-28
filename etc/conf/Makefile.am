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

include $(top_srcdir)/paths.am

VE0CONFIG = 0.conf
VECONFIG  = ve-light.conf-sample ve-vps.basic.conf-sample

if ARCH_IA64
VECONFIG_EXT = .ia64
else
if ARCH_X86_64
VECONFIG_EXT = .x86_64
else
VECONFIG_EXT = .x86
endif
endif


EXTRA_DIST = $(VE0CONFIG:%=%.x86) \
             $(VE0CONFIG:%=%.ia64) \
             $(VE0CONFIG:%=%.x86_64) \
             $(VECONFIG:%=%.x86) \
             $(VECONFIG:%=%.ia64) \
             $(VECONFIG:%=%.x86_64)

%: %$(VECONFIG_EXT)
	$(INSTALL) $< $@

vpsconf_DATA = \
	$(VE0CONFIG) \
	$(VECONFIG)

CLEANFILES = $(VE0CONFIG) $(VECONFIG)
