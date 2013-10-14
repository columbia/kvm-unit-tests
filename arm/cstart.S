
#define CR_B	(1 << 7)	/* Big endian */

.arm

.section .init

.globl start
start:
	/* bootloader params are in r0-r2 */
	ldr	sp, =stacktop

	mrc	p15, 0, r8, c1, c0, 0	@r8 = sctrl
	ands	r3, r8, #CR_B		@set BE, if necessary
	ldrne	r3, =cpu_is_be
	movne	r4, #1
	strne	r4, [r3]
	bl	setup			@complete setup

	/* start the test */
	ldr	r0, =__argc
	ldr	r0, [r0]
	ldr	r1, =__argv
	bl	main
	bl	exit
	b	halt

.text

.globl halt
halt:
1:	wfi
	b	1b

.data

.globl cpu_is_be
cpu_is_be:	.word 0
