ESBMCDIR = $(shell pwd)
include $(ESBMCDIR)/config.inc

all: esbmc

###############################################################################

util: big-int

infrastructure: util langapi solvers goto-symex pointer-analysis \
		goto-programs

languages: ansi-c cpp

# Ansi-c builds its library using infrastructure facilities.
ansi-c: infrastructure

###############################################################################

DIRS= big-int util langapi solvers goto-symex pointer-analysis goto-programs \
      ansi-c esbmc cpp

NJOBS := $(shell if [ -f /proc/cpuinfo ]; \
                    then echo `cat /proc/cpuinfo | grep 'processor' | wc -l`; \
                    else echo 1; fi)
$(DIRS):
	$(MAKE) -j$(NJOBS) -C $@

clean:
	for dir in $(DIRS); do \
		$(MAKE) -C $$dir clean; \
	done
	-rm $(OBJDIR)/.depends

.PHONY: $(DIRS) clean

esbmc: $(OBJDIR)/.depends infrastructure languages

doxygen:
	doxygen .doxygen

ctags:
	ctags -o .ctags -R --exclude=regression --extra=fq --fields=afikKlmnsSzt

# Some regexing required to not cscope-up the regression directory.
cscope:
	find . | egrep '(\.cpp|\.h)$$' | egrep -v regression | cscope -f.cscope -b -I. -Iutil -R  -i -

###############################################################################

STRICTCOMPILE=1

include $(ESBMCDIR)/common