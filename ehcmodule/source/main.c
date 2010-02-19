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
static u32 message;
//int n;

	//os_message_queue_send(timer1_queuehandle, 0x555, 0);
	//os_restart_timer(timer1_id, time);
	timer1_id=os_create_timer(time, 1000*1000*10, timer1_queuehandle, 0x0);
    os_message_queue_receive(timer1_queuehandle,(void *) &message, 0);
	os_stop_timer(timer1_id);
	os_destroy_timer(timer1_id);

}

void ehci_msleep(int msec)
{
	ehci_usleep(((u32) msec)*1000);
}


#define get_timer()  (*(((volatile u32*)0x0D800010)))

void ehci_udelay(int usec)
{
        u32 tmr,temp;
		u32 time_usec;

        tmr = get_timer();
        time_usec=2*usec;
		
        while (1) {temp=get_timer()-tmr;if(((int) temp)<0) tmr = get_timer(); if(((int)temp) > time_usec) break;}
		
}
void ehci_mdelay(int msec)//@todo not really sleeping..
{
        u32 tmr,temp;
		u32 time_usec;

        tmr = get_timer();
        time_usec=2048*msec;

        while (1) {temp=get_timer()-tmr;if(((int) temp)<0) tmr = get_timer(); if(((int)temp) > time_usec) break;}
		

}




int ehc_loop(void);

int heaphandle=-1;
unsigned int heapspace[0x4000*2] __attribute__ ((aligned (32)));


int main(void)
{

heaphandle = os_heap_create(heapspace, sizeof(heapspace));

void* timer1_queuespace = os_heap_alloc(heaphandle, 0x80);

timer1_queuehandle = os_message_queue_create(timer1_queuespace, 32);
//timer1_id=os_create_timer(1000*1000, 1000, timer1_queuehandle, 0x666);
//os_stop_timer(timer1_id);


	//os_unregister_event_handler(DEV_EHCI);
	
	/*
	// don´t work (may be EHCI irq is not supported)
	if(os_register_event_handler(DEV_EHCI, timer1_queuehandle, 0x0)!=0) return -1;
	os_software_IRQ(DEV_EHCI);
	*/

    if(tiny_ehci_init()<0) return -1;
	

	os_thread_set_priority(os_get_thread_id(), 0x78);
  
    ehc_loop();

	return 0;
}
