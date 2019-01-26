#
# DPA/RP Makefile
#


#
#  SCO defines
#
#CFLAGS = -O2 -DSCO -w3 $(INCLUDES)
#CFLAGS = -g -DSCO -w3 $(INCLUDES)
#LIBS = -lpanel -lcurses -lsocket

#
#  Solaris defines
#
#CFLAGS = -O2 -DSOLARIS -W -Wall $(INCLUDES)
#CFLAGS = -g -DSOLARIS -W -Wall $(INCLUDES)
#LIBS = -lpanel -lcurses -lsocket -lxnet

#
#  Cygwin defines
#
#CFLAGS = -O2 -W -Wall -DCYGWIN $(INCLUDES)
#CFLAGS = -g -W -Wall -DCYGWIN $(INCLUDES)
#LIBS = -lpanel -lncurses


HELP_FILES= help/dpa_info1 \
            help/dpa_info2 \
            help/dpa_info3 \
            help/dpa_info4


all: aix
build: aix

aix:
	make -f os/Makefile.AIX all

linux:
	make -f os/Makefile.Linux all

solaris:
	make -f os/Makefile.Solaris all


clean:
	rm -rf *.o os/*.o help_files.c

clobber: clean
	rm -rf dparp dparp-sparc dparp-i386

#----------------------------
#
#  Custom targets
#
#----------------------------
help_files.c: $(HELP_FILES)
	for f in $(HELP_FILES) ; \
	do \
		./encode_txtfile.sh $$f `basename $$f` ; \
	done > help_files.c
