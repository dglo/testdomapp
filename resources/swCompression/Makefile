CC = gcc
CFLAGS = -O2
INCDIR = ../../../hal/public

.SUFFIXES: .c

.c.o:
	$(CC) -c $(CFLAGS) $<

test: decompress

all: compress decompress

clean:
	rm -f compress decompress *.o

compress: compress.c compressEvent.c engFormat.c engFormat.h
	$(CC) $(CFLAGS) -I $(INCDIR) -o compress compress.c compressEvent.c engFormat.c

engFormat.o: ../../private/dataAccess/engFormat.c \
	     ../../private/dataAccess/engFormat.h
	$(CC) $(CFLAGS) -I $(INCDIR) -c ../../private/dataAccess/engFormat.c 

decompress: decompress.c decompressEvent.c engFormat.o
	$(CC) $(CFLAGS) -I $(INCDIR) -I ../../private/dataAccess -o decompress \
	   decompress.c decompressEvent.c engFormat.o


