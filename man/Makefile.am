#  Copyright (C) 2000-2010, Parallels, Inc. All rights reserved.
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

man_MANS = \
 vzctl.8 \
 vzlist.8 \
 vzcpucheck.8 \
 vzmemcheck.8 \
 vzcfgvalidate.8 \
 vzcalc.8 \
 vzsplit.8 \
 vzubc.8 \
 vzmigrate.8 \
 vzcptcheck.8 \
 vzfsync.8 \
 vznnc.8 \
 vztmpl-dl.8 \
 arpsend.8 \
 ndsend.8 \
 vzpid.8 \
 vzifup-post.8 \
 vzeventd.8 \
 $(extra_MANS) \
 vz.conf.5 \
 ctid.conf.5

SRCS=$(man_MANS:%=%.in)
PSS=$(man_MANS:%=%.ps)
PDFS=$(man_MANS:%=%.pdf)
WIKIS=$(man_MANS:%=%.wiki)

GROFF=groff
PAPER=$(shell paperconf || echo letter)
GROFF_OPTS=-Tps -t -dpaper=$(PAPER) -P-p$(PAPER) -man -msafer -rC1 -rD1 -rS11
GROFF_HTML_OPTS=-Thtml -t -man -msafer

EXTRA_DIST = $(SRCS) toc.man.in toc.sh html2wiki.sh macros.tmac

%.8: %.8.in macros.tmac
	$(AM_V_GEN)cat $(srcdir)/macros.tmac $< | $(pathsubst) > $@

%.5: %.5.in macros.tmac
	$(AM_V_GEN)cat $(srcdir)/macros.tmac $< | $(pathsubst) > $@

check-man-dates:
	for FILE in $(SRCS); do \
		DATE="$$(LANG=C git log -1 --format='format:%cD' \
			$${FILE} | awk '{print $$2, $$3, $$4}')" && \
		echo $$FILE: $$DATE && \
		sed -e "1,20s%\(^\.TH \w[a-z.]* [0-9] \"\)[0-9]* \w\w* [0-9][0-9][0-9][0-9]%\1$${DATE}%" -i $${FILE}; \
	done
	git diff $(SRCS)
.PHONY: check-man-dates

%.5.ps:	%.5
	$(GROFF) $(GROFF_OPTS) $^ > $@
%.8.ps:	%.8
	$(GROFF) $(GROFF_OPTS) $^ > $@

all.ps: $(man_MANS)
	 $(GROFF) $(GROFF_OPTS) $^ > $@

toc.man: toc.man.in toc.sh all.ps
	sed -ne '/^__TOC__$$/,$$!p' $< | \
		sed 's/__PKG__/$(PACKAGE_STRING)/' > $@
	$(srcdir)/toc.sh all.ps >> $@
	sed -ne '1,/^__TOC__$$/!p' $< >> $@

manual.ps: toc.man $(man_MANS)
	$(GROFF) $(GROFF_OPTS) $^ > $@

# to create a PDF from a PS
%.pdf: %.ps
	ps2pdf $< $@

pdfs: manual.pdf $(PDFS)
.PHONY: pdfs

%.8.wiki: %.8 html2wiki.sh
	sed 's/^\.SS \(.*$$\)/.in 0\n=== \1 ===/' $< | \
	sed 's/^\.SS3 \(.*$$\)/.in 0\n==== \1 ====/' | \
	$(GROFF) $(GROFF_HTML_OPTS) | \
		MANS="$(man_MANS)" $(srcdir)/html2wiki.sh > $@
	rm -f grohtml-*.png
%.5.wiki: %.5 html2wiki.sh
	sed 's/^\.SS\(.*$$\)/.in 0\n=== \1 ===/' $< | \
	$(GROFF) $(GROFF_HTML_OPTS) | \
		MANS="$(man_MANS)" $(srcdir)/html2wiki.sh > $@
	rm -f grohtml-*.png
toc.wiki: $(man_MANS) wikitoc.sh
	$(srcdir)/wikitoc.sh $(man_MANS) > $@

wikis: $(WIKIS) toc.wiki
.PHONY: wikis

CLEANFILES=all.ps all.pdf toc.man toc.wiki \
	   manual.ps manual.pdf $(PSS) $(PDFS) $(WIKIS) $(man_MANS)
