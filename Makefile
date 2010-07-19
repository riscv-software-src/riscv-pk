CFLAGS := -O2 -std=gnu99 -G 0
CC  := riscv-gcc $(CFLAGS)

TARGETS := pk

all: $(TARGETS)

pk: boot.o entry.o pk.o syscall.o file.o pk.ld
	$(CC) -o pk entry.o pk.o syscall.o file.o -T pk.ld

%.o: %.c *.h
	$(CC) -c $<

%.o: %.S *.h
	$(CC) -c $<

clean:
	rm -f *.o $(TARGETS)
