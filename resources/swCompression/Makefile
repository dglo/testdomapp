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

compressEvent.o: ../../private/dataAccess/compressEvent.c \
	     ../../private/dataAccess/compressEvent.h
	$(CC) $(CFLAGS) -I $(INCDIR) -I ../../private -I ../../private/dataAccess -c ../../private/dataAccess/compressEvent.c 

compress: compress.c engFormat.o compressEvent.o
	$(CC) $(CFLAGS) -I $(INCDIR) -I ../../private/dataAccess -o compress compress.c engFormat.o compressEvent.o

engFormat.o: ../../private/dataAccess/engFormat.c \
	     ../../private/dataAccess/engFormat.h
	$(CC) $(CFLAGS) -I $(INCDIR) -c ../../private/dataAccess/engFormat.c 

decompress: decompress.c decompressEvent.c engFormat.o
	$(CC) $(CFLAGS) -I $(INCDIR) -I ../../private/dataAccess -o decompress \
	   decompress.c decompressEvent.c engFormat.o
