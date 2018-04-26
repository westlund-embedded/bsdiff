CC=gcc 

all: bsdiff bspatch
bsdiff: bsdiff.c libtinf.a
bspatch: bspatch.c libtinf.a
clean: 
	rm bsdiff bspatch


