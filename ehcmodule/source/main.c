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


/*******************************************************************************
 *
 * main.c - IOS module main code
 *
 *******************************************************************************
 *
 *
 * v1.0 - 26 July 2008				- initial release by neimod
 * v1.1 - 5 September 2008			- prepared for public release
 *
 */



#include <stdio.h>
#include <string.h>
#include "syscalls.h"

int tiny_ehci_init(void);

int ehc_loop(void);

u8 heap_space2[0xb000] __attribute__ ((aligned (32)));

int main(void)
{


        tiny_ehci_init();

	os_thread_set_priority(1);
	
      //  debug_printf("Custom EHC! stack at %X\n",&a);
	
   

        ehc_loop();

	return 0;
}
