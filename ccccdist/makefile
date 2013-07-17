# Top level makefile for CCCC

# This distribution is a compilation of code, some of which comes from
# different sources, some of which builds different (e.g. Win32 DLL) kinds
# of targets.
# I would like to make it less inconsistent, but the first stage is to make 
# it work...

.PHONY : pccts cccc test install

all : pccts cccc test # install

pccts :
	cd pccts && $(MAKE) $(MAKEFLAGS)

cccc : 
	cd cccc && $(MAKE) $(MAKEFLAGS) -f posixgcc.mak

clean :
	cd cccc && $(MAKE) $(MAKEFLAGS) -f posixgcc.mak clean

test :
	cd test && $(MAKE) $(MAKEFLAGS) -f posix.mak

install : 
	cd install && su root -c "make -f install.mak"

