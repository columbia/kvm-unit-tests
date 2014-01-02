#include "libcflat.h"
#include "libio.h"
#include "heap.h"
#include "arm/processor.h"
#include "arm/sysinfo.h"
#include "arm/psci.h"
#include "arm/mmu.h"
#include "arm/setup.h"

#define FDT_SIG			0xd00dfeed

#define KERNEL_OFFSET		0x00010000
#define ATAG_OFFSET		0x00000100

#define ATAG_CORE		0x54410001
#define ATAG_MEM		0x54410002
#define ATAG_CMDLINE		0x54410009

extern void start(void);
extern unsigned long stacktop;
extern char *__args;

extern void io_init_early(void);
extern void io_init(void);
extern void __setup_args(void);

u32 mach_type_id;
struct tag_core core;
struct tag_mem32 mem32;

static void read_atags(u32 id, u32 *info)
{
	u32 *p = info;

	if (!p) {
		printf("Can't find bootinfo. mach-type = %x\n", id);
		exit(ENOEXEC);
	}

	/*
	 * p[0]	count of words for the tag
	 * p[1]	tag id
	 * p[2..]	tag data
	 */
	for (; p[0] != 0; p += p[0])
		switch (p[1]) {
		case ATAG_CORE:
			core.flags = p[2];
			core.pagesize = p[3];
			core.rootdev = p[4];
			break;
		case ATAG_MEM:
			mem32.size = p[2];
			mem32.start = p[3];
			break;
		case ATAG_CMDLINE:
			__args = (char *)&p[2];
			break;
		}
}

static void read_bootinfo(u32 id, u32 *info)
{
	u32 *atags = NULL;

	mach_type_id = id;

	if (info[0] == be32_to_cpu(FDT_SIG)) {
		/*
		 * fdt reading is not [yet?] implemented. So calculate
		 * the ATAGS addr to read that instead.
		 */
		atags = (u32 *)(start - KERNEL_OFFSET + ATAG_OFFSET);
	} else if (info[1] == ATAG_CORE)
		atags = info;

	read_atags(id, atags);
}

void setup(u32 arg __unused, u32 id, u32 *info)
{
	io_init_early();
	read_bootinfo(id, info);
	__setup_args();
	heap_init(&stacktop,
		  mem32.size - (ptr_to_compat(&stacktop) - mem32.start),
		  core.pagesize);
	io_init();
	psci_init();
	enable_mmu();
}

extern unsigned long exception_stacks;

void init_cpu_thread_info(struct cpu_thread_info *thread_info, int cpu_id)
{
	unsigned long estacks = (unsigned long)&exception_stacks;
	int i;

	estacks += cpu_id * PAGE_SIZE;

	thread_info->cpu_id = cpu_id;
	thread_info->exception_stacks = (void *)estacks;
	thread_info->exception_handlers = (void *)(estacks + PAGE_SIZE
		- sizeof(*thread_info->exception_handlers));
	for (i = 0; i < EXCPTN_MAX; i ++)
		*(thread_info->exception_handlers)[i] = NULL;
}