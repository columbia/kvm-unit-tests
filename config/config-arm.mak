PROCESSOR = cortex-a15
mach = mach-virt
iodevs = pl011 virtio_mmio
phys_base = 0x40000000

cstart.o = $(TEST_DIR)/cstart.o
bits = 32
ldarch = elf32-littlearm
kernel_offset = 0x10000
CFLAGS += -D__arm__

all: test_cases

cflatobjs += \
	lib/$(TEST_DIR)/iomaps.gen.o \
	lib/heap.o \
	lib/iomaps.o \
	lib/libio.o \
	lib/virtio.o \
	lib/virtio-testdev.o \
	lib/test_util.o \
	lib/arm/io.o \
	lib/arm/processor.o \
	lib/arm/setup.o

libeabi := lib/arm/libeabi.a
eabiobjs += \
	lib/arm/eabi_compat.o

$(libcflat) $(libeabi): LDFLAGS += -nostdlib
$(libcflat) $(libeabi): CFLAGS += -ffreestanding -I lib

CFLAGS += -Wextra
CFLAGS += -marm
CFLAGS += -mcpu=$(PROCESSOR)
CFLAGS += -O2

libgcc := $(shell $(CC) -m$(ARCH) --print-libgcc-file-name)
start_addr := $(shell printf "%x\n" $$(( $(phys_base) + $(kernel_offset) )))

FLATLIBS = $(libcflat) $(libgcc) $(libeabi)
%.elf: %.o $(FLATLIBS) arm/flat.lds
	$(CC) $(CFLAGS) -nostdlib -o $@ \
		-Wl,-T,arm/flat.lds,--build-id=none,-Ttext=$(start_addr) \
		$(filter %.o, $^) $(FLATLIBS)

$(libeabi): $(eabiobjs)
	$(AR) rcs $@ $^

%.flat: %.elf
	$(OBJCOPY) -O binary $^ $@

tests-common = $(TEST_DIR)/boot.flat $(TEST_DIR)/vmexit.flat

tests_and_config = $(TEST_DIR)/*.flat $(TEST_DIR)/unittests.cfg

test_cases: $(tests-common) $(tests)

$(TEST_DIR)/%.o scripts/arm/%.o: CFLAGS += -std=gnu99 -ffreestanding -I lib

$(TEST_DIR)/boot.elf: $(cstart.o) $(TEST_DIR)/boot.o
$(TEST_DIR)/vmexit.elf: $(cstart.o) $(TEST_DIR)/vmexit.o

lib/$(TEST_DIR)/iomaps.gen.c: lib/$(TEST_DIR)/$(mach).dts
	scripts/gen-devtree-iomaps.pl $^ $(iodevs) > $@

lib/$(TEST_DIR)/mach-virt.dts: dtb = $(subst .dts,.dtb,$@)
lib/$(TEST_DIR)/mach-virt.dts:
	$(QEMU_BIN) -kernel /dev/null -M virt -machine dumpdtb=$(dtb)
	dtc -I dtb -O dts $(dtb) > $@

.PHONY: asm-offsets

asm-offsets: scripts/arm/asm-offsets.flat
	$(QEMU_BIN) -device virtio-testdev -display none -serial stdio \
		-M virt -cpu $(PROCESSOR) \
		-kernel $^ > lib/arm/asm-offsets.h || true
scripts/arm/asm-offsets.elf: $(cstart.o) scripts/arm/asm-offsets.o

arch_clean:
	$(RM) $(TEST_DIR)/*.o $(TEST_DIR)/*.flat $(TEST_DIR)/*.elf \
	$(libeabi) $(eabiobjs) $(TEST_DIR)/.*.d lib/arm/.*.d \
	lib/$(TEST_DIR)/iomaps.gen.c lib/$(TEST_DIR)/mach-virt.* \
	scripts/arm/.*.d scripts/arm/*.o scripts/arm/*.flat scripts/arm/*.elf
