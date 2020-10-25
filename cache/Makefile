# Modify these two lines to choose your compiler and compile time
# flags.

CC=gcc
CFLAGS=-Wall -O2 -DUSE_INTERP_RESULT -Wno-deprecated-declarations -Werror

##################################################
# You shouldn't need to modify anything below here
##################################################

LIBS= -lm

all: csim test-cache 

csim: csim.c cache.c cache.h cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o csim csim.c cache.c cachelab.c -lm

test-cache: csim test-csim.c
	$(CC) $(CFLAGS) -o test-csim test-csim.c

clean:
	rm -f test-csim csim *.o *.exe *~ 


