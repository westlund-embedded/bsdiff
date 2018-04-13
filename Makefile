CC=gcc 

all: bsdiff bspatch
bsdiff: bsdiff.c -lbz2 libtinf.a
bspatch: bspatch.c -lbz2 libtinf.a
clean: 
	rm bsdiff bspatch


