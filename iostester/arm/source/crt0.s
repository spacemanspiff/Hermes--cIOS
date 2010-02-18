/*   
	IOS Tester, tests communication with custom IOS module for Wii.
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
	.section ".init"
	.global _start

	.align	4
	.arm

/*******************************************************************************
 *
 * crt0.s - IOS test code startup
 *
 *******************************************************************************
 *
 *
 * v1.0 - 26 July 2008				- initial release by neimod
 * v1.1 - 5 September 2008			- prepared for public release
 *
 */
	

_start:	
	// Fix GOT table based on entry PC, to create a complete position independent program
	
	stmfd	sp!,{r10, r12, lr}
	adr		r12,_start
	ldr		r3,=main
	add		r3,r3,r12
	ldr		r2,=__GOT_START
	ldr		r1,=__GOT_END
	add		r0,r2,r12			@r0 = entry + gotstart
	add		r1,r1,r12			@r1 = entry + gotend
	add		r10,r12,r2			@r10= entry + gotstart
	
	
fixmoregot:
	cmp		r0,r1
	ldrcc	r2,[r0]
	addcc	r2,r2,r12
	strcc	r2,[r0],#4
	bcc		fixmoregot
	
	// Execute main program 
	mov		r0, #0						@ int argc
	mov		r1, #0						@ char *argv[]
	adr		lr, ret
	bx		r3
ret:
	ldmfd	sp!,{r10, r12,lr}
	bx		lr
	


	.align
	.pool
_startend:
	.end
