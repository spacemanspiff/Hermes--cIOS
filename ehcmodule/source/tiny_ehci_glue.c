/*   
	EHCI glue. A bit hacky for the moment. needs cleaning..

    Copyright (C) 2008 kwiirk.

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
#include <string.h>
#include <setjmp.h>
#include "syscalls.h"

#include "ehci_types.h"
#include "utils.h"
#define static
#define inline extern


#define readl(a) (*((volatile u32*)(a)))
#define writel(v,a) do{*((volatile u32*)(a))=(v);}while(0)
#define ehci_dbg(a...) debug_printf(a)
#define printk(a...) debug_printf(a)
#define get_timer()  (*(((volatile u32*)0x0D800010)))


void BUG(void)
{
        debug_printf("bug\n");
//        stack_trace();
      //  while(1);
}
#define BUG_ON(a) if(a)BUG()

void ehci_usleep(int usec);
void ehci_msleep(int msec);
/*
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
*/
extern u32 __exe_start_virt__;
extern u32 __ram_start_virt__;

extern u32 ios_thread_stack;

#define cpu_to_le32(a) swab32(a)
#define le32_to_cpu(a) swab32(a)
#define cpu_to_le16(a) swab16(a)
#define le16_to_cpu(a) swab16(a)
#define cpu_to_be32(a) (a)
#define be32_to_cpu(a) (a)
void print_hex_dump_bytes(char *header,int prefix,u8 *buf,int len)
{
        int i;
        if (len>0x100)len=0x100;
        debug_printf("%s  %08X\n",header,(u32)buf);
        for (i=0;i<len;i++){
                debug_printf("%02x ",buf[i]);
                if((i&0xf) == 0xf) 
                        debug_printf("\n");
        }
        debug_printf("\n");
                
}
#define DUMP_PREFIX_OFFSET 1
#include "ehci.h"
#define ehci_readl(a) ((*((volatile u32*)(a))))
//#define ehci_writel(e,v,a) do{msleep(40);debug_printf("writel %08X %08X\n",a,v);*((volatile u32*)(a))=(v);}while(0)
#define ehci_writel(v,a) do{*((volatile u32*)(a))=(v);}while(0)

struct ehci_hcd _ehci;
struct ehci_hcd *ehci = &_ehci;

#include "ehci.c"


int usb_os_init(void);

#define MLOAD_GET_EHCI_DATA		0x4D4C44A0



int tiny_ehci_init(void)
{

        ehci = &_ehci;

		
        if(usb_os_init()<0)
                return -1;
	
	if(1) 
	{ // From Hermes: ohci mem is readed from dev/mload: (ehci init is from here)
	int fd;
		fd = os_open("/dev/mload",1);
		if(fd<0) return -1;
		ehci= (struct ehci_hcd *) os_ioctlv(fd, MLOAD_GET_EHCI_DATA ,0,0,0);
		os_close(fd);

	//////////////////////////////////////////////////////////////////////////////////////////////
	/* WARNING: This ignore the port 1 (external) and 2,3 (internals) for USB 2.0 operations    */
	/* from cIOS mload 1.6 port 1 is forced to USB 1.1. Only port 0 can work as USB 2.0         */

	ehci->num_port=1;

	//writel (INTR_MASK, &ehci->regs->intr_enable); //try interrupt


    /////////////////////////////////////////////////////////////////////////////////////////////
	}

	return 0;
}
