# Modify these two lines to choose your compiler and compile time
# flags.

CC=gcc
CFLAGS=-Wall -O2 -DUSE_INTERP_RESULT -Wno-deprecated-declarations -Werror

##################################################
# You shouldn't need to modify anything below here
##################################################

LIBS= -lm
YAS = ../misc/yas

all: cache isa pcsim

cache: cache.c cache.h 
	$(CC) $(CFLAGS) -c cache.c

isa: isa.c isa.h
	$(CC) $(CFLAGS) -c  isa.c

# This rule builds the PIPE simulator
pcsim: cache isa pcsim.c
	$(CC) $(CFLAGS) -o pcsim pcsim.c isa.o cache.o $(LIBS)

# These are implicit rules for assembling .yo files from .ys files.
.SUFFIXES: .ys .yo
.ys.yo:
	$(YAS) $*.ys


clean:
	rm -f pcsim *.o *.exe *~ 


