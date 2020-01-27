include Makemodules
CFLAGS += -O3

all:	vmei.o

clean:
	rm -f *.o

vmei.o: vmei.c Makefile
	$(CC) -c $(INCLUDE) $(CFLAGS) -o $@ $<

