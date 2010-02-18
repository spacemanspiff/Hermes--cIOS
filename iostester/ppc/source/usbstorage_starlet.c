/*-------------------------------------------------------------

usbstorage_starlet.c -- USB mass storage support, inside starlet
Copyright (C) 2008 Kwiirk

If this driver is linked before libogc, this will replace the original 
usbstorage driver by svpe from libogc

CIOS_usb2 must be loaded!

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <gccore.h>

#include <ogc/lwp_heap.h>
#include <malloc.h>
#include <ogc/disc_io.h>
#include <stdio.h>
#include <string.h>
#define UMS_BASE (('U'<<24)|('M'<<16)|('S'<<8))
#define USB_IOCTL_UMS_INIT	        	(UMS_BASE+0x1)
#define USB_IOCTL_UMS_GET_CAPACITY      	(UMS_BASE+0x2)
#define USB_IOCTL_UMS_READ_SECTORS      	(UMS_BASE+0x3)
#define USB_IOCTL_UMS_WRITE_SECTORS		(UMS_BASE+0x4)
#define USB_IOCTL_UMS_READ_STRESS		(UMS_BASE+0x5)
#define USB_IOCTL_UMS_SET_VERBOSE		(UMS_BASE+0x6)

#define UMS_HEAPSIZE					0x10000
#define UMS_MAXPATH 16
 
static s32 hId = -1;
static u32 fd=-1;
static u32 sector_size;
void debug_printf(const char *fmt, ...) ;

s32 USBStorage_Get_Capacity(u32*_sector_size)
{
        if(fd>0){
                s32 ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_GET_CAPACITY,":i",&sector_size);
                if(_sector_size)
                        *_sector_size = sector_size;
                debug_printf("got a masstorage of %d sectors of %d bytes: %.2f GB\n",ret,sector_size,(float)ret*sector_size/(1024.*1024*1024));
                return ret;
        }
        else
                return IPC_ENOENT;
}

s32 USBStorage_Init(int verbose)
{
	s32 _fd = -1;
	s32 ret = USB_OK;
	char *devicepath = NULL;
        if(fd!=-1)
                return 1;
	if(hId==-1) hId = iosCreateHeap(UMS_HEAPSIZE);
	if(hId<0) return IPC_ENOMEM;

	devicepath = iosAlloc(hId,UMS_MAXPATH);
        if(devicepath==NULL)return IPC_ENOMEM;

	snprintf(devicepath,USB_MAXPATH,"/dev/usb/ehc");

	_fd = IOS_Open(devicepath,0);
	if(_fd<0) ret = _fd;
        else
                fd = _fd;

	iosFree(hId,devicepath);
        if(fd>0){
                if(verbose)
                        ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_SET_VERBOSE,":");
                ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_INIT,":");
                ret = USBStorage_Get_Capacity(NULL);
        }
	return 1;
}
s32 USBStorage_Read_Stress(u32 sector, u32 numSectors, void *buffer)
{
        if(fd>0){
                return IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_STRESS,"ii:d",sector,numSectors,buffer,sector_size*numSectors);
        }
        else
                return IPC_ENOENT;
}

inline int is_MEM2_buffer(const void *buffer)
{
        u32 high_addr = ((u32)buffer)>>24;
        return (high_addr == 0x90) ||(high_addr == 0xD0);
}
s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
        if(fd>0){
                if (!is_MEM2_buffer(buffer)){ //libfat is not providing us good buffers :-(
                        s32 ret;
                        u8 *_buffer = iosAlloc(hId,sector_size*numSectors);
                        if(!_buffer)
                        {
                                debug_printf("unable to alloc %d %d bytes on hid:%d..\n",sector_size,numSectors,hId);
                                return IPC_ENOMEM;
                        }
                        ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,numSectors,_buffer,sector_size*numSectors);
                        memcpy(buffer,_buffer,sector_size*numSectors);
                        iosFree(hId,_buffer);
                        return ret;
                }
                else
                        return  IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_READ_SECTORS,"ii:d",sector,numSectors,buffer,sector_size*numSectors);
        }
        else
                return IPC_ENOENT;
}

s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
        if(fd>0){
                if (!is_MEM2_buffer(buffer)){ // libfat is not providing us good buffers :-(
                        s32 ret;
                        u8 *_buffer = iosAlloc(hId,sector_size*numSectors);
                        if(!_buffer)
                        {
                                debug_printf("unable to alloc %d %d bytes..\n",sector_size,numSectors);
                                return IPC_ENOMEM;
                        }
                        memcpy(_buffer,buffer,sector_size*numSectors);
                        ret = IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,numSectors,_buffer,sector_size*numSectors);
                        iosFree(hId,_buffer);
                        return ret;
                }
                else
                        return  IOS_IoctlvFormat(hId,fd,USB_IOCTL_UMS_WRITE_SECTORS,"ii:d",sector,numSectors,buffer,sector_size*numSectors);
        }
        else
                return IPC_ENOENT;
}

static bool __usbstorage_Startup(void)
{
        return USBStorage_Init(0);
}

static bool __usbstorage_IsInserted(void)
{
        if(fd>0){
        return USBStorage_Get_Capacity(NULL)>0;
        }
        else
                return IPC_ENOENT;
}

static bool __usbstorage_ClearStatus(void)
{
        return true;
}

static bool __usbstorage_Shutdown(void)
{
   return true;
}

const DISC_INTERFACE __io_usbstorage = {
   DEVICE_TYPE_WII_USB,
   FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_USB,
   (FN_MEDIUM_STARTUP)&__usbstorage_Startup,
   (FN_MEDIUM_ISINSERTED)&__usbstorage_IsInserted,
   (FN_MEDIUM_READSECTORS)&USBStorage_Read_Sectors,
   //(FN_MEDIUM_READSECTORS)&USBStorage_Read_Stress,
   (FN_MEDIUM_WRITESECTORS)&USBStorage_Write_Sectors,
   (FN_MEDIUM_CLEARSTATUS)&__usbstorage_ClearStatus,
   (FN_MEDIUM_SHUTDOWN)&__usbstorage_Shutdown
};
