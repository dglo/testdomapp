.SUFFIXES: .c .o .S .elf .srec .xml .html .tab .bin

.c.o:
	$(CC) -c $(CFLAGS) $<

.S.o:
	$(CPP) $(CPPFLAGS) -o $*.i $<
	$(AS) $(AFLAGS) -o $*.o $*.i

.elf.bin:
	$(OBJCOPY) -O binary $*.elf $*.bin
	$(CPP) $(CPPFLAGS) -DBINFILE=\"$*.bin\" -o $*-raw.i $(RAWS)
	$(AS) $(AFLAGS) -o $*-raw.o $*-raw.i
	$(LD) --script=$(RAWX) -o $*-raw.elf $*-raw.o
	$(OBJCOPY) -O binary $*-raw.elf $*.bin

OBJS  = DOMdataCompression.o \
	message.o             compressEvent.o \
	commonServices.o      domSControlRoutines.o  messageBuffers.o \
	dataAccess.o          moniDataAccess.o       engFormat.o  \
	dataAccessRoutines.o  expControl.o           msgHandler.o \
	domSControl.o         genericMsgSendRecv.o   lbm.o

all: ../bin/domapp.bin.gz

clean:
	rm -f *.o *.i *.bin *.elf

KOBJS = ../lib/crt0.o ../lib/libkernel.a

domapp.elf: domapp.o $(OBJS) $(LIBHAL)
	$(LD) --script=$(KERNELX) -o domapp.elf \
		$(KOBJS) domapp.o $(OBJS) $(LIBHAL) $(SYSLIBS)

../bin/%.bin.gz: %.bin; gzip -c $< > $@





