/*   
	Example of IOS module (C) 2009 Hermes.

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




#include <stdio.h>
#include <string.h>
#include "syscalls.h"

#define get_timer()  (*(((volatile u32*)0x0D800010)))

void udelay(int usec)
{
        u32 tmr,temp;
		u32 time_usec;

        tmr = get_timer();
        time_usec=2*usec;
		
        while (1) {temp=get_timer()-tmr;if(temp > time_usec) break;}
		
}

void msleep(int msec)//@todo not really sleeping..
{
        u32 tmr,temp;
		u32 time_usec;

        tmr = get_timer();
        time_usec=2048*msec;

        while (1) {temp=get_timer()-tmr;if(temp > time_usec) break;}
		

}


static unsigned int heaphandle;
unsigned heapspace[0x100/4] __attribute__ ((aligned (32)));

#define THREAD_STACK 1024

u8 thread_stack[THREAD_STACK] __attribute__ ((aligned (32)));


int queuehandle;

int thread_start(void)
{
int thread_queuehandle = os_message_queue_create( os_heap_alloc(heaphandle, 0x20), 8);



os_create_timer(1000*1000*10, 1000*1000*10, thread_queuehandle, 0x555);

while(1)
	{
    u32 message;	
 
	os_message_queue_receive(thread_queuehandle, (void*)&message, 0);

	os_message_queue_send(queuehandle,0x555, 0); // send 0x555 message to main thread to reset counter (count 10 seconds and resets)
    
	}


return 0;
}

int main(void)
{
int n=0x0;
int tid=0;
u32 message;


	heaphandle = os_heap_create(heapspace, sizeof(heapspace));

	void* queuespace = os_heap_alloc(heaphandle, 0x20);

	queuehandle = os_message_queue_create(queuespace, 8);

	os_sync_before_read((void *) 0x13740000, 16 );
	
	 tid=os_create_timer(1000*1000, 1000*1000*1, queuehandle, 0x666);
	 
	 // count to 0
	 *((u32 *) 0x13740000)=0;
	 n=0;

	 
    // create a thread to reset the counter 
	int my_thread_id=os_thread_create( (void *) thread_start, NULL, &thread_stack[1024], 1024, 0x28, 1);

	if(my_thread_id>=0) os_thread_continue(my_thread_id);
	
	while(1)
		{

		n++; // counter ++

		os_message_queue_receive(queuehandle,(void *) &message, 0);
		// here i can receive 0x555 (from thread_start timer) or 0x666 message (from tid timer)
	
		if(message==0x555) n=0;

		os_sync_before_read((void *) 0x13740000, 16 );

		*((u32 *) 0x13740000)=n;
		*((u32 *) 0x13740004)= os_get_thread_id();
		*((u32 *) 0x13740008)= os_thread_get_priority();

		os_sync_after_write((void *) 0x13740000, 16 );

	
		//os_stop_timer(tid);
		//os_restart_timer(tid,1000*500);
		}

	return 0;
}
