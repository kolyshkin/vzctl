INSTALL = install

PREFIX=/usr
MANDIR=$(PREFIX)/share/man

MAN8 = vzctl.8 arpsend.8 vzsplit.8 vzcfgvalidate.8 vzmemcheck.8 vzlist.8 \
	vzcalc.8 vzpid.8 vzcpucheck.8
MAN5 = vz.5 vps.conf.5 
MANS = $(MAN8) $(MAN5)

PSS = $(patsubst %, %.ps, $(MANS))
PDFS = $(patsubst %, %.pdf, $(MANS))
PCLS = $(patsubst %, %.pcl, $(MANS))
TXTS = $(patsubst %, %.txt, $(MANS))
GROFF = groff
#GROFF_OPTIONS = -Tps -t -msafer -man
# below is for A4 page size, requires a4.tmac
GROFF_OPTIONS = -Tps -t -ma4 -msafer -man -rC1 -rD1 -rS11
GROFF_TEXT_OPTIONS = -Tlatin1 -t -ma4 -msafer -man -rC1 -rD1 -rS11
GROFF_PCL_OPTIONS = -Tlj4 -t -ma4 -msafer -man -rC1 -rD1 -rS11 -P-pa4 -P-d1
PS2PDF = ps2pdf

all:

ps: $(PSS)

all.ps: $(MANS)
	$(GROFF) $(GROFF_OPTIONS) $(MANS) > all.ps

all.pcl: $(MANS)
	$(GROFF) $(GROFF_PCL_OPTIONS) $(MANS) > all.pcl

pdf: $(PDFS)

txt: $(TXTS)

clean: clean-ps clean-pdf clean-pcl clean-txt
	rm -f all.ps 

clean-pdf:
	rm -f $(PDFS)

clean-ps:
	rm -f $(PSS)

clean-pcl:
	rm -f $(PCLS)

clean-txt:
	rm -f $(TXTS)


%.5.ps: %.5
	$(GROFF) $(GROFF_OPTIONS) $< > $@

%.8.ps: %.8
	$(GROFF) $(GROFF_OPTIONS) $< > $@

%.pdf: %.ps
	$(PS2PDF) $<

# pcl - for HP LaserJet and compatible printers
%.5.pcl: %.5
	$(GROFF) $(GROFF_PCL_OPTIONS) $< > $@

%.8.pcl: %.8
	$(GROFF) $(GROFF_PCL_OPTIONS) $< > $@

# text with overstriking
%.5.txt.os: %.5
	$(GROFF) $(GROFF_TEXT_OPTIONS) $< > $@
%.8.txt.os: %.8
	$(GROFF) $(GROFF_TEXT_OPTIONS) $< > $@

# to remove overstrike
%.txt: %.txt.os
	sed 's/.//g' < $< > $@ && rm -f $<

install:
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man5
	$(INSTALL) -d $(DESTDIR)$(MANDIR)/man8
	for file in $(MAN8); do \
		$(INSTALL) -m 644 $$file $(DESTDIR)$(MANDIR)/man8/$$file; \
	done
	for file in $(MAN5); do \
		$(INSTALL) -m 644 $$file $(DESTDIR)$(MANDIR)/man5/$$file; \
	done


.PHONY: clean clean-ps clean-pdf
