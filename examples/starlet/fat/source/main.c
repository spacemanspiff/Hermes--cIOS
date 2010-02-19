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
#include "fat.h"
#include "fatfile.h"
#include "syscalls.h"

#define IOS_OPEN				0x01
#define IOS_CLOSE				0x02
#define IOS_READ				0x03
#define IOS_WRITE				0x04
#define IOS_SEEK				0x05
#define IOS_IOCTL				0x06
#define IOS_IOCTLV				0x07



#define get_timer()  (*(((volatile u32*)0x0D800010)))

int timer1_queuehandle=-1;
int timer1_id=-1;

void usleep(u32 time)
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
	usleep((u32) usec);
}

void msleep(int msec)
{
	usleep(((u32) msec)*1000);
}


static unsigned int heaphandle;
unsigned heapspace[0x100] __attribute__ ((aligned (32)));

int queuehandle=-1;


void * my_malloc(int size)
{
	return os_heap_alloc(0, size);
}

void my_free(void * mem)
{
	os_heap_free(0, mem);
}


struct _reent my_reent;
FILE_STRUCT my_filestr[5];
int file_table[5][2];


#include "fcntl.h"

#define DEVICE "/dev/fat"

// directory list

#include <sys/dir.h>
#include "fatdir.h"
#include <sys/stat.h>

DIR_ITER dir_internal;
DIR_STATE_STRUCT dir_state_internal;

struct stat filestat;

int main(void)
{
int fd,n,mounted=0;
int inited=0;
ipcmessage* message;


	memset(&my_reent, 0, sizeof(struct _reent));
	memset(my_filestr, 0, sizeof(FILE_STRUCT)*5);
	memset(file_table, 0, 4*2*5);

	memset(&dir_state_internal,0,sizeof(DIR_STATE_STRUCT));

	dir_internal.device=0;
	dir_internal.dirStruct=&dir_state_internal;

	

	heaphandle = os_heap_create(heapspace, sizeof(heapspace));

	void* queuespace = os_heap_alloc(heaphandle, 0x20);

	queuehandle = os_message_queue_create(queuespace, 8);

	 void* timer1_queuespace = os_heap_alloc(heaphandle, 0x40);

	timer1_queuehandle = os_message_queue_create(timer1_queuespace, 16);
	timer1_id=os_create_timer(1000*1000, 1, timer1_queuehandle, 0x666);
	os_stop_timer(timer1_id);

	 
	extern DISC_INTERFACE __io_wiisd;
	inited=__io_wiisd.startup();
	msleep(100);

	os_device_register(DEVICE, queuehandle);
	while(1)
		{
		int result = 1;
		int ack = 1;

		os_message_queue_receive(queuehandle,(void *) &message, 0);
		
		switch( message->command )
		{
		
	    case IOS_OPEN:
			{
			result = -6;

                                //debug_printf("%s try open %sfor fd %d\n",DEVICE,message->open.device,message->open.resultfd);
				// Checking device name
				if (0 == strcmp(message->open.device, DEVICE))
                                  {
									result = message->open.resultfd;        
                                  }
				else
				if (0 == strcmp(message->open.device, DEVICE"/log"))
                                  {
								  if(inited && !mounted) 
										mounted=fatMountSimple("sd", &__io_wiisd);

								  if(mounted)
									{
									static char log_name[]="sd:/log000.txt";
									for(n=0;n<5;n++) if(my_filestr[n].inUse==0) break;
									fd=-1;
									if(n!=5)
										{
										my_reent._errno=0;
										fd=_FAT_open_r(&my_reent, &my_filestr[n], log_name, O_CREAT | O_TRUNC | O_RDWR, 0);
										log_name[9]++;
										if(log_name[9]>='9') {log_name[9]='0';log_name[8]++;}
										if(log_name[8]>='9') {log_name[8]='0';log_name[7]++;}
										if(log_name[7]>='9') {log_name[7]='0';log_name[8]='0';log_name[9]='0';}

										}
									
									if(fd<0) result=-1; 
									else 
										{
										result=file_table[n][0]=message->open.resultfd;
										file_table[n][1]= fd;
										}
									}
                        
                                  }
				else if (0 == memcmp(message->open.device, DEVICE"/", sizeof(DEVICE)))
                                        
				{
				if(inited && !mounted) 
					mounted=fatMountSimple("sd", &__io_wiisd);

				if(mounted)
					{
					for(n=0;n<5;n++) if(my_filestr[n].inUse==0) break;
					fd=-1;
					if(n!=5)
						{
						my_reent._errno=0;
						fd=_FAT_open_r(&my_reent, &my_filestr[n], message->open.device+sizeof(DEVICE),
							message->open.mode, 0);	
							
						}

					if(fd<0) result=-1; 
					else 
						{
						result=file_table[n][0]=message->open.resultfd;
						file_table[n][1]= fd;
						}
					}
				}
			
					
			}	
			break;

		case IOS_IOCTLV:
			{
			result = -1;

			ioctlv *vec = message->ioctlv.vector;

                                int i,in = message->ioctlv.num_in,io= message->ioctlv.num_io;
                               
                                os_sync_before_read( vec, (in+io)*sizeof(ioctlv));

                                for(i=0;i<in+io;i++){
                                        os_sync_before_read( vec[i].data, vec[i].len);
                                      
                                }

			#if 0
			// list the directory content to directory.txt
			switch(message->ioctl.command )
				{
				
				case 0xcacafea:
				if(inited && !mounted) 
						mounted=fatMountSimple("sd", &__io_wiisd);

				if(mounted)
					{
					my_reent._errno=0;
					
					result=-666;
					if(_FAT_diropen_r(&my_reent, &dir_internal, "sd:/"))
						{
						static char log_name2[]="sd:/directory.txt";
						char filename[768];

						result = 0;
						my_reent._errno=0;
						fd=_FAT_open_r(&my_reent, &my_filestr[4], log_name2, O_CREAT | O_TRUNC | O_RDWR, 0);
						my_reent._errno=0;
						while(!_FAT_dirnext_r (&my_reent, &dir_internal, filename, &filestat))
							{
							my_reent._errno=0;
							
							if(fd>=0)
								{
								if(filestat.st_mode & S_IFDIR)_FAT_write_r (&my_reent, fd, "[", 1); // es directorio
								_FAT_write_r (&my_reent, fd, filename, strlen(filename));
								if(filestat.st_mode & S_IFDIR)_FAT_write_r (&my_reent, fd, "]", 1); // es directorio
								_FAT_write_r (&my_reent, fd, "\n", 1);
								
								}
							
							}
							my_reent._errno=0;
							if(fd>=0) _FAT_close_r (&my_reent, fd);fd=-1;

						  _FAT_dirclose_r(&my_reent, &dir_internal);
						}

					
					}
				break;
				}
			#endif
				for(i=in;i<in+io;i++){
											os_sync_after_write( vec[i].data, vec[i].len);
									}
			
			}
			break;

		case IOS_CLOSE:
			{
			result = -1;
			if(mounted)
				{
				my_reent._errno=0;
				for(n=0;n<5;n++)
					{
					if(message->fd==file_table[n][0])
						{
						fd=file_table[n][1];
						file_table[n][0]=file_table[n][1]=0;
						_FAT_close_r (&my_reent, fd);
						result = 0;
						break;
						}
					}

				for(n=0;n<5;n++) if(my_filestr[n].inUse!=0) break;
					  
				if(n==5) {fatUnmount ("sd");mounted=0;}
				
				}
				else result=-6;
				
			}	
			break;

		case IOS_READ:
			{
			
			result=-1;
			if(mounted)
				{
				my_reent._errno=0;
				for(n=0;n<5;n++)
					{
					if(message->fd==file_table[n][0])
						{
						fd=file_table[n][1];
						result=_FAT_read_r (&my_reent, fd, message->write.data, message->write.length);
						}
					}
				}
			os_sync_after_write( message->write.data, message->write.length);
			}	
			break;

		case IOS_WRITE:
			{
		
			os_sync_before_read( message->write.data, message->write.length);

			result=-1;
			if(mounted)
				{
				my_reent._errno=0;
				for(n=0;n<5;n++)
					{
					if(message->fd==file_table[n][0])
						{
						fd=file_table[n][1];
						result=_FAT_write_r (&my_reent, fd, message->write.data, message->write.length);
						}
					}
				}		
			}	
			break;

		case IOS_SEEK:
			{
			result=-1;
			if(mounted)
				{
				my_reent._errno=0;
				for(n=0;n<5;n++)
					{
					if(message->fd==file_table[n][0])
						{
						fd=file_table[n][1];
						result=_FAT_seek_r (&my_reent, fd, message->seek.offset, message->seek.origin);
						}
					}
				}		
			}
			break;

		default:
			result=-1;
			break;

		}

		if (ack)
			os_message_queue_ack( (void*)message, result );

		}

	return 0;
}
