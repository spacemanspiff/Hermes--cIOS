/*   
	dev/mload: Custom IOS module for Wii, to load ios elfs, initialize USB 2.0 and others uses
	This module is derived from haxx.elf
	Copyright (C) 2009 Hermes.
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
 * v1.2 - march  2008				- added some IOTCL, put it into its own module, by kwiirk
 *
 */


#include <stdio.h>
#include <string.h>
#include "syscalls.h"


#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07

#define MLOAD_MLOAD_THREAD_ID	0x4D4C4400

#define MLOAD_LOAD_MODULE		0x4D4C4480
#define MLOAD_RUN_MODULE		0x4D4C4481
#define MLOAD_RUN_THREAD        0x4D4C4482

#define MLOAD_STOP_THREAD		0x4D4C4484
#define MLOAD_CONTINUE_THREAD   0x4D4C4485

#define MLOAD_GET_LOAD_BASE     0x4D4C4490
#define MLOAD_MEMSET			0x4D4C4491

#define MLOAD_GET_EHCI_DATA		0x4D4C44A0

#define MLOAD_SET_ES_IOCTLV		0x4D4C44B0

#define MLOAD_GETW				0x4D4C44C0
#define MLOAD_GETH				0x4D4C44C1
#define MLOAD_GETB				0x4D4C44C2
#define MLOAD_SETW				0x4D4C44C3
#define MLOAD_SETH				0x4D4C44C4
#define MLOAD_SETB				0x4D4C44C5

#define DEVICE "/dev/mload"


extern int ES_ioctlv_ret(void *);

unsigned ES_ioctlv_vect=((unsigned) ES_ioctlv_ret);

unsigned int heapspace[0x100/4] __attribute__ ((aligned (32)));

// from IOS ELF stripper of neimod

typedef struct 
{
        u32		ident0;
		u32		ident1;
		u32		ident2;
		u32		ident3;
        u32		machinetype;
        u32		version;
        u32		entry;
        u32     phoff;
        u32     shoff;
        u32		flags;
        u16     ehsize;
        u16     phentsize;
        u16     phnum;
        u16     shentsize;
        u16     shnum;
        u16     shtrndx;
} elfheader;

typedef struct 
{
       u32      type;
       u32      offset;
       u32      vaddr;
       u32      paddr;
       u32      filesz;
       u32      memsz;
       u32      flags;
       u32      align;
} elfphentry;

#define ioctlv_u8(a) (*((u8*)(a).data))
#define ioctlv_u16(a) (*((u16*)(a).data))
#define ioctlv_u32(a) (*((u32*)(a).data))
#define ioctlv_voidp(a) (a).data



extern u8 *mem_exe; // size 0x80000 (see crt0.s)


struct _data_elf
{
	void *start;
	int prio;
	void *stack;
	int size_stack;
}
data_elf;

#define getbe32(x) ((adr[x]<<24) | (adr[x+1]<<16) | (adr[x+2]<<8) | (adr[x+3]))


int load_elf(u32 elf)
{
int n,m;
int p;
u8 *adr;

elfheader *head=(void *) elf;
elfphentry *entries;

if(head->ident0!=0x7F454C46) return -1;
if(head->ident1!=0x01020161) return -1;
if(head->ident2!=0x01000000) return -1;

p=head->phoff;

data_elf.start=(void *)  head->entry;

for(n=0; n<head->phnum; n++)
	{
	entries=(void *) (elf+p);
	p+=sizeof(elfphentry);

	if(entries->type == 4)
		{
		adr=(void *) (elf + entries->offset);

        if(getbe32(0)!=0) return -2; // bad info (sure)

		for(m=4; m < entries->memsz; m+=8)
			{
			switch(getbe32(m))
				{
				case 0x9:
					data_elf.start= (void *) getbe32(m+4);
					break;
				case 0x7D:
					data_elf.prio= getbe32(m+4);
					break;
				case 0x7E:
					data_elf.size_stack= getbe32(m+4);
					break;
				case 0x7F:
					data_elf.stack= (void *) (getbe32(m+4));
					break;
				
				}

			}

		}
    else
	if(entries->type == 1  && entries->memsz != 0 && entries->vaddr!=0)
		{
	
		os_sync_before_read((void *) entries->vaddr, entries->memsz );

		memset((void *) entries->vaddr, 0, entries->memsz);
		memcpy((void *) entries->vaddr, (void *) (elf + entries->offset), entries->filesz);

		os_sync_after_write((void *) entries->vaddr, entries->memsz );
			
		}
	}

return 0;
}

extern void *ehci;
int tiny_ehci_init(void);

int main(void)
{
	ipcmessage* message;
    unsigned int offset = 0;

	
    
	mem_exe[0]=0; // don't remove this !!!!!
	

	tiny_ehci_init();

	unsigned int heaphandle = os_heap_create(heapspace, sizeof(heapspace));
	void* queuespace = os_heap_alloc(heaphandle, 0x20);

	unsigned int queuehandle = os_message_queue_create(queuespace, 8);

	os_device_register(DEVICE, queuehandle);
	
	while(1)
	{	
		int result = 1;
		int ack = 1;

		// Wait for message to arrive
		os_message_queue_receive(queuehandle, (void*)&message, 0);

		switch( message->command )
		{
			case IOS_OPEN:
			{
                                //debug_printf("%s try open %sfor fd %d\n",DEVICE,message->open.device,message->open.resultfd);
				// Checking device name
				if (0 == strcmp(message->open.device, DEVICE))
                                  {
									result = message->open.resultfd;        
                                  }
				
				else
					result = -6;
			}	
			break;

			case IOS_CLOSE:
			{
                            
				// do nothing
				result = 0;
			}	
			break;

			case IOS_READ:
			{
				// Read from Starlet memory

			/*
			    if (message->read.length == 4)
				{
				  *(volatile unsigned long*)(message->read.data) = *(volatile unsigned long*)offset;
				}
				else if (message->read.length == 2)
				{
					*(volatile unsigned short*)(message->read.data) = *(volatile unsigned short*)offset;
				}
				else if (message->read.length == 1)
				{
					*(volatile unsigned char*)(message->read.data) = *(volatile unsigned char*)offset;
				}
				else
				{
					memcpy(message->read.data, (void*)offset, message->read.length);
				}
				*/
				// NOTE: no aligned is better
				memcpy(message->read.data, (void*)offset, message->read.length);
				// Clean cache
				os_sync_after_write( message->read.data, message->read.length );
				offset += message->read.length;
			}	
			break;

			case IOS_WRITE:
			{
				// Write to Starlet memory
				// Invalidate cache
				os_sync_before_read( message->write.data, message->write.length );

				/*
				if (message->write.length == 4)
				{
					*(volatile unsigned long*)offset = *(volatile unsigned long*)(message->write.data);
				}
				else if (message->write.length == 2)
				{
					*(volatile unsigned short*)offset = *(volatile unsigned short*)(message->write.data);
				}
				else if (message->write.length == 1)
				{
					*(volatile unsigned char*)offset = *(volatile unsigned char*)(message->write.data);
				}
				else
				{
					memcpy((void*)offset, message->write.data, message->write.length);
				}
				*/
				memcpy((void*)offset, message->write.data, message->write.length);
				offset += message->write.length;
			}	
			break;

			case IOS_SEEK:
			{
				// Change current offset
				switch(message->seek.origin)
				{	
					case SEEK_SET:
					{
						offset = message->seek.offset;
						break;
					}

					case SEEK_CUR:
					{
						offset += message->seek.offset;
						break;
					}

					case SEEK_END:
					{
						offset = - message->seek.offset;
						break;
					}
				}
			result=offset;
			}	
			break;


			case IOS_IOCTL:
			{
			
                break;
            }

			case IOS_IOCTLV:
			{
                                ioctlv *vec = message->ioctlv.vector;

                                int i,in = message->ioctlv.num_in,io= message->ioctlv.num_io;
                               
                                os_sync_before_read( vec, (in+io)*sizeof(ioctlv));

                                for(i=0;i<in+io;i++){
                                        os_sync_before_read( vec[i].data, vec[i].len);
                                      
                                }

                                switch( message->ioctl.command )
                                {
								
								case MLOAD_MLOAD_THREAD_ID:
										
										result=os_get_thread_id();
										
										break;
								
								case MLOAD_GET_EHCI_DATA:

										result= (u32) ehci;
										break;

								case MLOAD_GET_LOAD_BASE:
									    
										result=0;
									    ioctlv_u32(vec[0])= 0x13700000;
										ioctlv_u32(vec[1])= 0x80000;
										break;
								
                                case MLOAD_LOAD_MODULE:

                                        result =  load_elf((u32) ioctlv_voidp(vec[0]));
                                        break;
								
								case MLOAD_RUN_MODULE:

										result=os_thread_create( data_elf.start, NULL, data_elf.stack, data_elf.size_stack, data_elf.prio, 0);
										if(result>=0) os_thread_continue(result);
										
										break;	

								case MLOAD_RUN_THREAD:

										result=os_thread_create((void *) ioctlv_u32(vec[0]), NULL, (void *) ioctlv_u32(vec[1]), ioctlv_u32(vec[2]), ioctlv_u32(vec[3]), 0);
										if(result>=0) os_thread_continue(result);
										
										break;

								case MLOAD_STOP_THREAD:
										
										result=os_thread_stop(ioctlv_u32(vec[0]));

										
										break;
								case MLOAD_CONTINUE_THREAD:
										
										result=os_thread_continue(ioctlv_u32(vec[0]));
										
										break;


								case MLOAD_MEMSET:
										result=0;
										os_sync_before_read((void *) ioctlv_u32(vec[0]), ioctlv_u32(vec[2]));
										memset((void *) ioctlv_u32(vec[0]), ioctlv_u32(vec[1]), ioctlv_u32(vec[2]));
										
										break;

								case MLOAD_SET_ES_IOCTLV: // changes the current vector for dev/es ioctl (put 0 to disable it)
										result=0;
										ES_ioctlv_vect=ioctlv_u32(vec[0]);
										os_sync_after_write( &ES_ioctlv_vect, 4);
										break;

								case MLOAD_GETW:
									result=0;
									ioctlv_u32(vec[1])=*((volatile u32*) ioctlv_u32(vec[0]));
									break;
								case MLOAD_GETH:
									result=0;
									ioctlv_u16(vec[1])=*((volatile u16*) ioctlv_u32(vec[0]));
									break;
								case MLOAD_GETB:
									result=0;
									ioctlv_u8(vec[1])=*((volatile u8*) ioctlv_u32(vec[0]));
									break;

								case MLOAD_SETW:
									result=0;
									*((volatile u32*) ioctlv_u32(vec[0]))=ioctlv_u32(vec[1]);
									break;
								case MLOAD_SETH:
									result=0;
									*((volatile u16*) ioctlv_u32(vec[0]))=ioctlv_u16(vec[1]);
									break;
								case MLOAD_SETB:
									result=0;
									*((volatile u8*) ioctlv_u32(vec[0]))=ioctlv_u8(vec[1]);
									break;



									/*
			    if (message->read.length == 4)
				{
				  *(volatile unsigned long*)(message->read.data) = *(volatile unsigned long*)offset;
				}
				else if (message->read.length == 2)
				{
					*(volatile unsigned short*)(message->read.data) = *(volatile unsigned short*)offset;
				}
				else if (message->read.length == 1)
				{
					*(volatile unsigned char*)(message->read.data) = *(volatile unsigned char*)offset;
				}
				else
				{
					memcpy(message->read.data, (void*)offset, message->read.length);
				}
				*/
							
                                }
                                for(i=in;i<in+io;i++){
                                        os_sync_after_write( vec[i].data, vec[i].len);
                                }

                                break;
                        }
			default:
				result = -1;
				//ack = 0;
			break;
		}
                //debug_printf("return %d\n",result);
		// Acknowledge message
		if (ack)
			os_message_queue_ack( (void*)message, result );
	}
   
	return 0;
}
