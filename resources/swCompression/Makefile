CC = gcc
CFLAGS = -O2
INCDIR = ../../../hal/public

.SUFFIXES: .c

.c.o:
	$(CC) -c $(CFLAGS) $<

all: compress decompress

clean:
	rm -f compress decompress

compress: compress.c compressEvent.c engFormat.c engFormat.h
	$(CC) $(CFLAGS) -I $(INCDIR) -o compress compress.c compressEvent.c engFormat.c

decompress: decompress.c decompressEvent.c engFormat.c engFormat.h
	$(CC) $(CFLAGS) -I $(INCDIR) -o decompress decompress.c decompressEvent.c engFormat.c

