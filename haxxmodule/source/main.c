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

#define IOCTL_CALL				0xDB
#define IOCTL_JUMP				0xDC
#define IOCTL_TELL				0xDD
#define IOCTL_SYNC_BEFORE_READ			0xDE
#define IOCTL_SYNC_AFTER_WRITE		       	0xDF

#define IOCTL_READL                             0xE0
#define IOCTL_WRITEL                            0xE1


#define DEVICE "/dev/haxx"

unsigned int heapspace[0x1500] __attribute__ ((aligned (32)));


int main(void)
{
	unsigned int offset = 0;
	ipcmessage* message;


	//os_thread_set_priority(0x54);
	os_thread_set_priority(1);

	// Write something to memory for testing
	//*(volatile unsigned int*)(0x10019000) = 0xDEADB00B;
	//*(volatile unsigned int*)(0x10019004) = 0xCAFEF00D;

	unsigned int heaphandle = os_heap_create(heapspace, sizeof(heapspace));
	void* queuespace = os_heap_alloc(heaphandle, 0x20);

	unsigned int queuehandle = os_message_queue_create(queuespace, 8);

	os_device_register(DEVICE, queuehandle);
	
	debug_printf("haxx inited at:%s\n",DEVICE);

	// NOTE: Reading and writing only works from this modules allowed memory space.
	// The IOS system will crash if you don't obey these rules.
	// For unlimited access, you need to break free from usermode priviledges and switch into systemmode!
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
				int result;

				// Checking device name
				if (0 != strncmp(message->open.device, DEVICE, sizeof(DEVICE)))
				{
					result = -6;
				}
				else
				{
					result = message->open.resultfd;
				}
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
			}	
			break;

			case IOS_IOCTL:
			{
				switch( message->ioctl.command )
				{
					// Allocate memory, jump to it, and deallocate
					case IOCTL_CALL:
					{
						void* codeblock;


						// Invalidate cache
						os_sync_before_read( message->ioctl.buffer_in, message->ioctl.length_in );
						
						codeblock = os_heap_alloc(0, message->ioctl.length_in);
						if (codeblock == 0)
						{
							result = -1;
							break;
						}

						memcpy(codeblock, message->ioctl.buffer_in, message->ioctl.length_in);

						unsigned int (*func)(void) = ( unsigned int (*)(void) )(codeblock);

						result = func();

						if (codeblock)
						{
							os_heap_free(0, codeblock);
						}

						break;
					}

					// Execute code and return
					case IOCTL_JUMP:
					{
						void (*jump)(void) = ( void (*)(void) )(offset);

						jump();

						break;
					}

					case IOCTL_TELL:
					{
						*(volatile unsigned long*)(message->ioctl.buffer_io) = offset;
				
						// Clean cache
						os_sync_after_write( message->ioctl.buffer_io, 4 );
					}
					break;

				        case IOCTL_SYNC_BEFORE_READ:
				        {
					  os_sync_before_read( (char*)offset, message->ioctl.length_in );
					}
					break;
				        case IOCTL_SYNC_AFTER_WRITE:
					{
					        os_sync_after_write( (char*)offset, message->ioctl.length_in );
					}
					break;
				        case IOCTL_READL:
					{
					  os_sync_before_read( message->ioctl.buffer_in, message->ioctl.length_in );
					  message->ioctl.buffer_io[0] = (*(volatile unsigned long*)(message->ioctl.buffer_in[0]));
					  os_sync_after_write( message->ioctl.buffer_io, 4 );
					}
					break;
				        case IOCTL_WRITEL:
					{
					  os_sync_before_read( message->ioctl.buffer_in, message->ioctl.length_in );
					  *((volatile unsigned long*)(message->ioctl.buffer_in[0])) = message->ioctl.buffer_in[1];
					}
					break;
					default:
						result = -1;
						//ack = 0;
					break;
				}

				break;
			}
			

			default:
				result = -1;
				//ack = 0;
			break;
		}

		// Acknowledge message
		if (ack)
			os_message_queue_ack( (void*)message, result );
	}
   
	return 0;
}
