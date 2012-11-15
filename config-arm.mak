CFLAGS += -I../include/arm
CFLAGS += -Wa,-mcpu=cortex-a15 -I lib
CFLAGS += -ffreestanding

cstart := arm/cstart.o

cflatobjs += \
	lib/arm/io.o \
	lib/arm/div.o \
	lib/arm/lib1funcs.o

$(libcflat): LDFLAGS += -nostdlib

# these tests do not use libcflat
simpletests := \
	arm/8loop.bin

# theses tests use cstart.o, libcflat, and libgcc
tests := \
	arm/exit.bin \
	arm/helloworld.bin

all: kvmtrace $(libcflat) $(simpletests) $(tests)

$(simpletests): %.bin: %.o
	$(CC) -nostdlib $^ -Wl,-T,flat.lds -o $@

$(tests): %.bin: $(cstart) %.o $(libcflat)
	$(CC) -nostdlib $(libgcc) $^ -Wl,-T,flat.lds -o $@

arch_clean:
	$(RM) $(simpletests) $(tests) $(cstart)
	$(RM) $(patsubst %.bin, %.elf, $(simpletests) $(tests))
	$(RM) $(patsubst %.bin, %.o, $(simpletests) $(tests))
