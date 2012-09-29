
CC=g++
CPPFLAGS=-g -Wall -I. -fPIC -D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS -O3 -ffast-math

# g++ -shared  -Wl,-soname,libzita-convolver.so.1 -o libzita-convolver.so.1.0.0 zita-convolver.o -lfftw3f -lpthread

all: zita-convolver.o

clean:
	rm -f zita-convolver.o
