/*   
	Custom IOS module for Wii.
    Copyright (C) 2008 neimod.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

	.section ".text"
	.align	4
	.arm


/*******************************************************************************
 *
 * syscalls.s - IOS syscalls assembly file
 *
 *******************************************************************************
 *
 *
 * v1.0 - 26 July 2008				- initial release by neimod
 * v1.1 - 5 September 2008			- prepared for public release
 *
 */


 
 	.global syscall_0
syscall_0:
	.long 0xE6000010
	bx		lr

	.global syscall_9
syscall_9:
	.long 0xE6000130
	bx		lr

	.global syscall_a
syscall_a:
	.long 0xE6000150
	bx		lr

	.global syscall_e	
syscall_e:
	.long 0xE60001D0
	bx		lr
	
	.global syscall_16
syscall_16:
	.long 0xE60002D0
	bx		lr
	
	.global syscall_17
syscall_17:
	.long 0xE60002F0
	bx		lr
	
	.global syscall_18
syscall_18:
	.long 0xE6000310
	bx		lr

	.global syscall_1a
syscall_1a:	
	.long 0xE6000350
	bx		lr

	.global syscall_1b
syscall_1b:
	.long 0xE6000370
	bx		lr

	.global syscall_1c
syscall_1c:
	.long 0xE6000390
	bx		lr

	.global syscall_1d
syscall_1d:
	.long 0xE60003b0
	bx		lr

	.global syscall_22
syscall_22:
	.long 0xE6000450
	bx		lr
	
	.global syscall_2a
syscall_2a:
	.long	0xE6000550
	bx		lr

	.global syscall_3f
syscall_3f:
	.long 0xE60007F0
	bx		lr

	.global syscall_40
syscall_40:
	.long 0xE6000810
	bx		lr



	.global syscall_50
syscall_50:
	.long	0xE6000A10
	bx		lr

	.global svc_4
svc_4:
	mov R2,lr
	adds r1,r0,#0
	movs R0,#4
	svc 0xAB
	bx r2
	.pool
	.end


	.pool
	.end
