/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Copyright IBM Corp. 2008
 *
 * Authors: Hollis Blanchard <hollisb@us.ibm.com>
 */

#define OUTPUT_VADDR 0xf0000000
#define OUTPUT_PADDR 0xf0000000

.globl _start
_start:
	/* In the future we might need to assign a stack and zero BSS here. */

	/* Virtual address mappings? */
	@ set r3=vaddr and r4=paddr?
	@bl	map

	/* Call main() and pass return code to exit(). */
	bl	main
	bl	exit

	b	.
