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

//int ehc_loop(void);

u8 heap_space2[0xc000] __attribute__ ((aligned (32)));

/* USB timer */


int timer1_queuehandle=-1;
int timer1_id=-1;

void ehci_usleep(u32 time)
{
u32 message;

	os_message_queue_send(timer1_queuehandle, 0x555, 0);
	os_restart_timer(timer1_id, time);
    while(1)
		{
		os_message_queue_receive(timer1_queuehandle,(void *) &message, 0);
		if(message==0x555) break;
		}
	os_message_queue_receive(timer1_queuehandle,(void *) &message, 0);
	os_stop_timer(timer1_id);

}


void udelay(int usec)
{
	ehci_usleep((u32) usec);
}

void ehci_msleep(int msec)
{
	ehci_usleep(((u32) msec)*1000);
}


int ehc_loop(void);

int heaphandle=-1;
unsigned int heapspace[0x4000] __attribute__ ((aligned (32)));

int main(void)
{
heaphandle = os_heap_create(heapspace, sizeof(heapspace));

void* timer1_queuespace = os_heap_alloc(heaphandle, 0x40);

timer1_queuehandle = os_message_queue_create(timer1_queuespace, 16);
timer1_id=os_create_timer(1000*1000, 1000, timer1_queuehandle, 0x666);
os_stop_timer(timer1_id);


	//os_unregister_event_handler(DEV_EHCI);
	
	/*
	// don´t work (may be EHCI irq is not supported)
	if(os_register_event_handler(DEV_EHCI, timer1_queuehandle, 0x0)!=0) return -1;
	os_software_IRQ(DEV_EHCI);
	*/

    if(tiny_ehci_init()<0) return -1;


	//os_thread_set_priority(0x20);
	
  
    ehc_loop();

	return 0;
}
