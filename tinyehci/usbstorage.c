/*-------------------------------------------------------------

usbstorage.c -- Bulk-only USB mass storage support

Copyright (C) 2008
Sven Peter (svpe) <svpe@gmx.net>

quick port to ehci/ios: Kwiirk

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

#define ROUNDDOWN32(v)				(((u32)(v)-0x1f)&~0x1f)

//#define	HEAP_SIZE			(32*1024)
//#define	TAG_START			0x1//BADC0DE
#define	TAG_START			0x22112211

#define	CBW_SIZE			31
#define	CBW_SIGNATURE			0x43425355
#define	CBW_IN				(1 << 7)
#define	CBW_OUT				0


#define	CSW_SIZE			13
#define	CSW_SIGNATURE			0x53425355

#define	SCSI_TEST_UNIT_READY		0x00
#define	SCSI_INQUIRY			0x12
#define	SCSI_REQUEST_SENSE		0x03
#define	SCSI_START_STOP			0x1b
#define	SCSI_READ_CAPACITY		0x25
#define	SCSI_READ_10			0x28
#define	SCSI_WRITE_10			0x2A

#define	SCSI_SENSE_REPLY_SIZE		18
#define	SCSI_SENSE_NOT_READY		0x02
#define	SCSI_SENSE_MEDIUM_ERROR		0x03
#define	SCSI_SENSE_HARDWARE_ERROR	0x04

#define	USB_CLASS_MASS_STORAGE		0x08
#define USB_CLASS_HUB				0x09

#define	MASS_STORAGE_RBC_COMMANDS		0x01
#define	MASS_STORAGE_ATA_COMMANDS		0x02
#define	MASS_STORAGE_QIC_COMMANDS		0x03
#define	MASS_STORAGE_UFI_COMMANDS		0x04
#define	MASS_STORAGE_SFF8070_COMMANDS	0x05
#define	MASS_STORAGE_SCSI_COMMANDS		0x06

#define	MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET		0xFF

#define	USB_ENDPOINT_BULK		0x02

#define	USBSTORAGE_CYCLE_RETRIES	10


#define MAX_TRANSFER_SIZE			4096

#define DEVLIST_MAXSIZE    8

static int ums_init_done = 0;

extern int handshake_mode; // 0->timeout force -ENODEV 1->timeout return -ETIMEDOUT

static bool first_access=true;

static usbstorage_handle __usbfd;
static u8 __lun = 16;
static u8 __mounted = 0;
static u16 __vid = 0;
static u16 __pid = 0;

//0x1377E000

//#define MEM_PRINT 1

#ifdef MEM_PRINT

// this dump from 0x13750000 to 0x13770000 log messages 

char mem_cad[32];

char *mem_log=(char *) 0x13750000;

#include <stdarg.h>    // for the s_printf function

extern int verbose;

void int_char(int num)
{
int sign=num<0;
int n,m;

	if(num==0)
		{
		mem_cad[0]='0';mem_cad[1]=0;
		return;
		}

	for(n=0;n<10;n++)
		{
		m=num % 10;num/=10;if(m<0) m=-m;
		mem_cad[25-n]=48+m;
		}

	mem_cad[26]=0;

	n=0;m=16;
	if(sign) {mem_cad[n]='-';n++;}

	while(mem_cad[m]=='0') m++;

	if(mem_cad[m]==0) m--;

	while(mem_cad[m]) 
	 {
	 mem_cad[n]=mem_cad[m];
	 n++;m++;
	 }
	mem_cad[n]=0;

}

void hex_char(u32 num)
{
int n,m;

	if(num==0)
		{
		mem_cad[0]='0';mem_cad[1]=0;
		return;
		}

	for(n=0;n<8;n++)
		{
		m=num & 15;num>>=4;
		if(m>=10) m+=7;
		mem_cad[23-n]=48+m;
		}

	mem_cad[24]=0;

	n=0;m=16;
    
	mem_cad[n]='0';n++;
	mem_cad[n]='x';n++;

	while(mem_cad[m]=='0') m++;

	if(mem_cad[m]==0) m--;

	while(mem_cad[m]) 
	 {
	 mem_cad[n]=mem_cad[m];
	 n++;m++;
	 }
	mem_cad[n]=0;

}


void s_printf(char *format,...)
{
 va_list	opt;
 char *t;
 int pos=0;

 int val,n;

 char *s;

// if(!verbose) return;

 if(mem_log>(char *) 0x13770000) return;

 t=mem_log;
 va_start(opt, format);

 while(format[0])
	{
	if(format[0]!='%') {*mem_log++=*format++;pos++;}
	else
		{
		format++;
		switch(format[0])
			{
			case 'd':
			case 'i':
				val=va_arg(opt,int);
				int_char(val);
				n=strlen(mem_cad);
				memcpy(mem_log,mem_cad,n);
				mem_log+=n;pos+=n;
				break;

			case 'x':
				val=va_arg(opt,int);
                hex_char((u32) val);
				n=strlen(mem_cad);
				memcpy(mem_log,mem_cad,n);
				mem_log+=n;pos+=n;
				break;

			case 's':
				s=va_arg(opt,char *);
			    n=strlen(s);
				memcpy(mem_log,s,n);
				mem_log+=n;pos+=n;
				break;

			}
		 format++;
		}
	
	}
   
	va_end(opt);

	os_sync_after_write((void *) t, pos);
	
	
}

void log_status(char *s)
{
u32 status=ehci_readl( &ehci->regs->status);
u32 statusp=ehci_readl(&ehci->regs->port_status[0]);

s_printf("    log_status  (%s)\n",s);
s_printf("    status: %x %s%s%s%s%s%s%s%s%s%s\n",
		status,
		(status & STS_ASS) ? " Async" : "",
		(status & STS_PSS) ? " Periodic" : "",
		(status & STS_RECL) ? " Recl" : "",
		(status & STS_HALT) ? " Halt" : "",
		(status & STS_IAA) ? " IAA" : "",
		(status & STS_FATAL) ? " FATAL" : "",
		(status & STS_FLR) ? " FLR" : "",
		(status & STS_PCD) ? " PCD" : "",
		(status & STS_ERR) ? " ERR" : "",
		(status & STS_INT) ? " INT" : ""
		);

 s_printf("    status port: %x\n", statusp);
}
			
#endif


static s32 __usbstorage_reset(usbstorage_handle *dev,int hard_reset);
static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun);
static s32 __usbstorage_start_stop(usbstorage_handle *dev, u8 lun, u8 start_stop);

// ehci driver has its own timeout.
static s32 __USB_BlkMsgTimeout(usbstorage_handle *dev, u8 bEndpoint, u16 wLength, void *rpData)
{
	return USB_WriteBlkMsg(dev->usb_fd, bEndpoint, wLength, rpData);
}

static s32 __USB_CtrlMsgTimeout(usbstorage_handle *dev, u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, u16 wLength, void *rpData)
{
	return USB_WriteCtrlMsg(dev->usb_fd, bmRequestType, bmRequest, wValue, wIndex, wLength, rpData);
}

static s32 __send_cbw(usbstorage_handle *dev, u8 lun, u32 len, u8 flags, const u8 *cb, u8 cbLen)
{
	s32 retval = USBSTORAGE_OK;

	if(cbLen == 0 || cbLen > 16 || !dev->buffer)
		return -EINVAL;
	memset(dev->buffer, 0, CBW_SIZE);

	((u32*)dev->buffer)[0]=cpu_to_le32(CBW_SIGNATURE);
	((u32*)dev->buffer)[1]=cpu_to_le32(dev->tag);
	((u32*)dev->buffer)[2]=cpu_to_le32(len);
	dev->buffer[12] = flags;
	dev->buffer[13] = lun;
	// linux usb/storage/protocol.c seems to say only difference is padding
        // and fixed cw size
        if(dev->ata_protocol)
                dev->buffer[14] = 12; 
        else
                dev->buffer[14] = (cbLen > 6 ? 0x10 : 6);
        //debug_printf("send cb of size %d\n",dev->buffer[14]);
	memcpy(dev->buffer + 15, cb, cbLen);
        //hexdump(dev->buffer,CBW_SIZE);
	retval = __USB_BlkMsgTimeout(dev, dev->ep_out, CBW_SIZE, (void *)dev->buffer);

	if(retval == CBW_SIZE) return USBSTORAGE_OK;
	else if(retval >= 0) return USBSTORAGE_ESHORTWRITE;

	return retval;
}

static s32 __read_csw(usbstorage_handle *dev, u8 *status, u32 *dataResidue)
{
	s32 retval = USBSTORAGE_OK;
	u32 signature, tag, _dataResidue, _status;

	memset(dev->buffer, 0xff, CSW_SIZE);

	retval = __USB_BlkMsgTimeout(dev, dev->ep_in, CSW_SIZE, dev->buffer);
        //print_hex_dump_bytes("csv resp:",DUMP_PREFIX_OFFSET,dev->buffer,CSW_SIZE);

	if(retval >= 0 && retval != CSW_SIZE) return USBSTORAGE_ESHORTREAD;
	else if(retval < 0) return retval;

	signature = le32_to_cpu(((u32*)dev->buffer)[0]);
	tag = le32_to_cpu(((u32*)dev->buffer)[1]);
	_dataResidue = le32_to_cpu(((u32*)dev->buffer)[2]);
	_status = dev->buffer[12];
        //debug_printf("csv status: %d\n",_status);
	if(signature != CSW_SIGNATURE) {
               // BUG();
                return USBSTORAGE_ESIGNATURE;
        }

	if(dataResidue != NULL)
		*dataResidue = _dataResidue;
	if(status != NULL)
		*status = _status;

	if(tag != dev->tag) return USBSTORAGE_ETAG;
	dev->tag++;

	return USBSTORAGE_OK;
}

extern u32 usb_timeout;

static int is_read_write=0;

static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue, s8 retries)
{
	s32 retval = USBSTORAGE_OK;

	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;
    u32 pstatus;
  
	u8 *buffer2=buffer;
	u32 len2=len;

//	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

	do
	{

	//pi if(retval==-ENODEV) {unplug_device=1;return -ENODEV;}
	
	

    if(retval < 0)
		{
		u8 conf;
		int old_usb_timeout,h_flag;
       /* retval=__usbstorage_reset(dev,0);// WARNING! you cannot perform here a hardware reset
	   */
	  
        ehci_msleep(10);
		pstatus = ehci_readl(&ehci->regs->port_status[dev->usb_fd->port]);

		// device unplugged
		if (/*!(PORT_CONNECT & status)*/ (pstatus & 0x3901)!=0x1001) 
			{
			unplug_device=1;
			
			return -ENODEV;
			}
        h_flag=handshake_mode;
		handshake_mode=1;
		old_usb_timeout=usb_timeout;
		usb_timeout=1000*1000;
		retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);
		if(retval >=0)
			retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);
		if(retval >=0)
			retval=USB_GetConfiguration(dev->usb_fd, &conf);
		handshake_mode=h_flag;
		usb_timeout=old_usb_timeout;

		ehci_msleep(60);
		pstatus = ehci_readl(&ehci->regs->port_status[dev->usb_fd->port]);
		
		// device unplugged
		if (/*!(PORT_CONNECT & status)*/(pstatus & 0x3901)!=0x1001) 
			{
			unplug_device=1;
			
			return -ENODEV;
			}

		

		/////

		//if(retval==-ENODEV) {unplug_device=1;return -ENODEV;}
		}
	retries--;
	if(retval<0) continue; // nuevo

	buffer=buffer2;
	len=len2;

		if(write)
		{

			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			if(retval < 0)
				continue;//reset+retry
			while(len > 0)
			{
                thisLen=len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, buffer);
           
		        /*
				pi
				if(retval==-ENODEV || retval==-ETIMEDOUT) break;

				if(retval < 0)
					continue;//reset+retry

				*/
				if(retval<0) break; //pi

				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					break; // pi continue;//reset+retry
				}
				len -= retval;
				buffer += retval;
			}

			if(retval < 0)
				continue;
		}
		else
		{
			retval = __send_cbw(dev, lun, len, CBW_IN, cb, cbLen);

			if(retval < 0)
				continue; //reset+retry
			while(len > 0)
			{
                thisLen=len;
				
				retval = __USB_BlkMsgTimeout(dev, dev->ep_in, thisLen, buffer);
				
				/*
				pi
				if(retval==-ENODEV || retval==-ETIMEDOUT) break;
				
				if(retval < 0)
					continue; //reset+retry
                                //hexdump(buffer,retval);
								*/
				if(retval<0) break; //pi
               
				len -= retval;
				buffer += retval;

				if(retval != thisLen)
                                {
                                        retval = -1;
                                        break; // pi continue; //reset+retry
                                }
			}

			if(retval < 0)
				continue;
		}

		retval = __read_csw(dev, &status, &dataResidue);
		
		if(retval>=0 && is_read_write!=0 && status!=0) retval = USBSTORAGE_ESTATUS; // if read/write bad status repeat the read/write

		if(retval < 0)
			continue;

		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);

    // force unplug
	if(retval < 0 && retries <= 0 && handshake_mode==0) {unplug_device=1;return retval;/*-ENODEV*/;}

/*	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev,0) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}
*/
	if(retval>=0) unplug_device=0;

	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	return retval;
}

static s32 __usbstorage_start_stop(usbstorage_handle *dev, u8 lun, u8 start_stop)
{
	#if 0
	s32 retval;
	u8 cmd[16];
	
	u8 status = 0;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_START_STOP;
	cmd[1] = (lun << 5) | 1;
	cmd[4] = start_stop & 3;
	cmd[5] = 0;
	//memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
	retval = __cycle(dev, lun, NULL, 0, cmd, 6, 0, &status, NULL, USBSTORAGE_CYCLE_RETRIES);
//	if(retval < 0) goto error;

/*	
	if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) 
					retval = USBSTORAGE_ESENSE;*/
//error:
	return retval;
	#else
	return 0;
	#endif
}


static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 *sense= USB_Alloc(SCSI_SENSE_REPLY_SIZE);
	u8 status = 0;
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;
	//int n;

	if(!sense) return -ENOMEM;
    


/*		for(n=0;n<5;n++)
			{*/
			
			retval = __cycle(dev, lun, NULL, 0, cmd, 6, 1, &status, NULL, USBSTORAGE_CYCLE_RETRIES);

			#ifdef MEM_PRINT
			s_printf("    SCSI_TEST_UNIT_READY ret %i\n", retval);
			#endif
		
		//	if(retval==-ENODEV) goto error;

			
		//	if(retval==0) break;
		//	}
		
		if(retval<0) goto error;
		

	if(status != 0)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL, USBSTORAGE_CYCLE_RETRIES);

		#ifdef MEM_PRINT
		s_printf("    SCSI_REQUEST_SENSE ret %i\n", retval);
		#endif

		if(retval < 0) goto error;
		
		status = sense[2] & 0x0F;

		#ifdef MEM_PRINT
		s_printf("    SCSI_REQUEST_SENSE status %x\n", status);
		#endif

		
		if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) 
                        retval = USBSTORAGE_ESENSE;
	}
error:
        USB_Free(sense);
	return retval;
}

extern int handshake_us;

static s32 __usbstorage_reset(usbstorage_handle *dev,int hard_reset)
{
	s32 retval;
	u32 old_usb_timeout=usb_timeout; 
	usb_timeout=1000*1000;
    //int retry = hard_reset;
    int retry = 0;  //first try soft reset
	int h_mode=handshake_mode;
	
	int hard_reset_done=0;

	u32 status;
	u8 conf;
		//
 retry:
	 handshake_mode=1;
        if ( hard_reset==1 || (hard_reset==2 && retry>=1)){
                
				hard_reset_done=1;
                #ifdef MEM_PRINT
				s_printf("usbstorage hard reset retry: %i\n", retry);
				
				#endif
                retval = ehci_reset_device(dev->usb_fd);
				if(retval<0) 
					{
					ehci_msleep(100);
					retval=ehci_reset_port2(dev->usb_fd->port);
					if(retval<0)
						{
						ehci_msleep(100);
						retval = ehci_reset_device(dev->usb_fd);
						}
					}

				ehci_msleep(100);
				
				status = ehci_readl(&ehci->regs->port_status[dev->usb_fd->port]);

				#ifdef MEM_PRINT
				s_printf("usbstorage hard reset ret %i status: %x\n",retval, status);
				
				#endif
				if (retval<0 && !(PORT_CONNECT & status)) 
						{
						handshake_mode=h_mode;
                        unplug_device=1;
						usb_timeout=old_usb_timeout;
						return -ENODEV;
						}
				
                if(retval < 0)
                        goto end;
				 handshake_mode=1;
                // reset configuration
				retval=USB_GetConfiguration(dev->usb_fd, &conf);
				#ifdef MEM_PRINT
				s_printf("usbstorage hard reset: GetConfiguration ret %i\n",retval);
				#endif
                if(retval < 0) goto end;

				retval=USB_SetConfiguration(dev->usb_fd, dev->configuration);
				#ifdef MEM_PRINT
				s_printf("usbstorage hard reset: SetConfiguration ret %i\n",retval);
				#endif
                if(retval < 0) goto end;
                if(dev->altInterface != 0)
					{ 
					retval=USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface);
                     
					#ifdef MEM_PRINT
					s_printf("usbstorage hard reset: SetAlternativeInterface %i\n",retval);
					#endif
				    if(retval < 0) goto end;
					}
				
        }
        debug_printf("usbstorage reset..\n");

	status = ehci_readl(&ehci->regs->port_status[dev->usb_fd->port]);

	// device unplugged
	if (!(PORT_CONNECT & status)) 
		{
		handshake_mode=h_mode;
        unplug_device=1;
		usb_timeout=old_usb_timeout;
		return -ENODEV;
		}
	 handshake_mode=2;
	
	usb_timeout=1000*1000;
	//retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);
	retval=0;


	#ifdef MEM_PRINT
	s_printf("usbstorage reset: Reset ret %i time: %i (us)\n",retval, handshake_us);
	
	#endif

    usb_timeout=1000*1000;

	/* FIXME?: some devices return -7004 here which definitely violates the usb ms protocol but they still seem to be working... */
        if(retval < 0 && retval != -ETIMEDOUT)
                goto end;
				
	/* gives device enough time to process the reset */
	ehci_msleep(20);
	

        debug_printf("clear halt on bulk ep..\n");
	
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);

	#ifdef MEM_PRINT
	s_printf("usbstorage reset: clearhalt in ret %i\n",retval);
	
	#endif
	if(retval < 0)
		goto end;
	ehci_msleep(10);
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);

	#ifdef MEM_PRINT
	s_printf("usbstorage reset: clearhalt in ret %i\n",retval);
	
	#endif
	if(retval < 0)
		goto end;
	//if(retval >=0)
	ehci_msleep(10);
	retval=USB_GetConfiguration(dev->usb_fd, &conf);
	if(retval < 0)
		goto end;

	if(__lun != 16 && hard_reset && hard_reset_done)
		{
		if(USBStorage_MountLUN(&__usbfd, __lun) < 0)
        goto end;			
		__mounted = 1;
		
		}

    unplug_device=0;

    ehci_msleep(50);
    usb_timeout=old_usb_timeout;
	handshake_mode=h_mode;
	return retval;

end:
	
    if(retry < (5/*+15*(hard_reset==2)*/)){
                
				//if(hard_reset==2) ehci_msleep(1000);
				//else 
				ehci_msleep(100);
           
                retry ++;
                goto retry;
        }        
    
	usb_timeout=old_usb_timeout;
	handshake_mode=h_mode;

	unplug_device=1; // force the unplug method

	return retval;
}



int my_memcmp(char *a, char *b, int size_b)
{
int n;

for(n=0;n<size_b;n++) 
	{
	if(*a!=*b) return 1;
	a++;b++;
	}
return 0;
}

static usb_devdesc _old_udd; // used for device change protection

s32 try_status=0;

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd)
{
	s32 retval = -1;
	u8 conf;//,*max_lun = NULL;
	u32 iConf, iInterface, iEp;
	static usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;
	
	__lun= 16; // select bad LUN

	//max_lun = USB_Alloc(1);
	//if(max_lun==NULL) return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	dev->tag = TAG_START;
    dev->usb_fd = fd;

    udd.configurations=NULL;

	retval = USB_GetDescriptors(dev->usb_fd, &udd);

	#ifdef MEM_PRINT
	s_printf("USBStorage_Open(): USB_GetDescriptors %i\n",retval);

	log_status("after USB_GetDescriptors");
	
	#endif

	if(retval < 0)
		goto free_and_return;

	// test device changed without unmount (prevent device write corruption)
	if(ums_init_done)
		{
		if(my_memcmp((void *) &_old_udd, (void *) &udd, sizeof(usb_devdesc)-4)) 
			{
			//USB_Free(max_lun);
			USB_FreeDescriptors(&udd);
			#ifdef MEM_PRINT
			s_printf("USBStorage_Open(): device changed!!!\n");
			
			#endif
			return -ENODEV;
			}
		}
	
	_old_udd= udd;
	try_status=-128;
	for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
	{
		ucd = &udd.configurations[iConf];		
		for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
		{
			uid = &ucd->interfaces[iInterface];
                  //      debug_printf("interface %d, class:%x subclass %x protocol %x\n",iInterface,uid->bInterfaceClass,uid->bInterfaceSubClass, uid->bInterfaceProtocol);
			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
			   (uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_RBC_COMMANDS
                || uid->bInterfaceSubClass == MASS_STORAGE_ATA_COMMANDS
                || uid->bInterfaceSubClass == MASS_STORAGE_QIC_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_UFI_COMMANDS
				|| uid->bInterfaceSubClass == MASS_STORAGE_SFF8070_COMMANDS) &&
			   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY && uid->bNumEndpoints>=2)
			{
				
				dev->ata_protocol = 0;
                if(uid->bInterfaceSubClass != MASS_STORAGE_SCSI_COMMANDS || uid->bInterfaceSubClass != MASS_STORAGE_RBC_COMMANDS)
                                        dev->ata_protocol = 1;

				#ifdef MEM_PRINT
				s_printf("USBStorage_Open(): interface subclass %i ata_prot %i \n",uid->bInterfaceSubClass, dev->ata_protocol);
				
				#endif
				dev->ep_in = dev->ep_out = 0;
				for(iEp = 0; iEp < uid->bNumEndpoints; iEp++)
				{
					ued = &uid->endpoints[iEp];
					if(ued->bmAttributes != USB_ENDPOINT_BULK)
						continue;

					if(ued->bEndpointAddress & USB_ENDPOINT_IN)
						dev->ep_in = ued->bEndpointAddress;
					else
						dev->ep_out = ued->bEndpointAddress;
				}
				if(dev->ep_in != 0 && dev->ep_out != 0)
				{
					dev->configuration = ucd->bConfigurationValue;
					dev->interface = uid->bInterfaceNumber;
					dev->altInterface = uid->bAlternateSetting;
					
					goto found;
				}
			}
		else
			{


			if(uid->endpoints != NULL)
						USB_Free(uid->endpoints);uid->endpoints= NULL;
			if(uid->extra != NULL)
						USB_Free(uid->extra);uid->extra=NULL;

			if(uid->bInterfaceClass == USB_CLASS_HUB)
				{
				retval = USBSTORAGE_ENOINTERFACE;
				try_status= -20000;

				USB_FreeDescriptors(&udd);
        
				goto free_and_return;
				}

			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
			   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY && uid->bNumEndpoints>=2)
					{
					try_status= -(10000+uid->bInterfaceSubClass);
					}
			}
		}
	}
    
	#ifdef MEM_PRINT
	s_printf("USBStorage_Open(): cannot find any interface!!!\n");
	
	#endif
	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
        debug_printf("cannot find any interface\n");
	goto free_and_return;

found:
	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;

	
	try_status=-1201;

    #ifdef MEM_PRINT
	s_printf("USBStorage_Open(): conf: %x altInterface: %x\n", dev->configuration, dev->altInterface);
	
	#endif

	if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
		goto free_and_return;
	try_status=-1202;

	#ifdef MEM_PRINT
	log_status("after USB_GetConfiguration");
	#endif

	#ifdef MEM_PRINT
	if(conf != dev->configuration)
		s_printf("USBStorage_Open(): changing conf from %x\n", conf);
	
	#endif

//usb_timeout=2000*1000;
	if(conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0) // antes forzaba el cambio de configuracion
		goto free_and_return;

	try_status=-1203;
	if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
		goto free_and_return;

//usb_timeout=1000*1000;
	try_status=-1204;
	
	#ifdef MEM_PRINT
	log_status("Before USBStorage_Reset");
	#endif
	retval = USBStorage_Reset(dev);
    //retval=0;
	//retval =__usbstorage_reset(dev, 1);
	#ifdef MEM_PRINT
	log_status("After USBStorage_Reset");
	#endif
	if(retval < 0)
		goto free_and_return;


/*	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_GET_MAX_LUN, 0, dev->interface, 1, max_lun);
	if(retval < 0 )
		dev->max_lun = 1;
	else
		dev->max_lun = (*max_lun+1);
	

	if(retval == USBSTORAGE_ETIMEDOUT)*/

	/* NOTE: from usbmassbulk_10.pdf "Devices that do not support multiple LUNs may STALL this command." */
		dev->max_lun = 8; // max_lun can be from 1 to 16, but some devices do not support lun 
	
	retval = USBSTORAGE_OK;

	/*if(dev->max_lun == 0)
		dev->max_lun++;*/

	/* taken from linux usbstorage module (drivers/usb/storage/transport.c) */
	/*
	 * Some devices (i.e. Iomega Zip100) need this -- apparently
	 * the bulk pipes get STALLed when the GetMaxLUN request is
	 * processed.   This is, in theory, harmless to all other devices
	 * (regardless of if they stall or not).
	 */
	//USB_ClearHalt(dev->usb_fd, dev->ep_in);
	//USB_ClearHalt(dev->usb_fd, dev->ep_out);

	if(dev->buffer == NULL)
		dev->buffer = USB_Alloc(MAX_TRANSFER_SIZE+16);
	
	if(dev->buffer == NULL) {retval = -ENOMEM;try_status=-1205;}
	else retval = USBSTORAGE_OK;

free_and_return:

	//if(max_lun!=NULL) USB_Free(max_lun);

	if(retval < 0)
	{
		if(dev->buffer != NULL)
			USB_Free(dev->buffer);
		memset(dev, 0, sizeof(*dev));

		#ifdef MEM_PRINT
		s_printf("USBStorage_Open(): try_status %i\n",try_status);
		
		#endif
		return retval;
	}

	#ifdef MEM_PRINT
	s_printf("USBStorage_Open(): return 0\n");
	
	#endif

	return 0;
}


void my_sprint(char *cad, char *s);

s32 USBStorage_Close(usbstorage_handle *dev)
{
        if(dev->buffer != NULL)
                USB_Free(dev->buffer);
	memset(dev, 0, sizeof(*dev));

	my_sprint("USBStorage_Close()", NULL);
	return 0;
}

s32 USBStorage_Reset(usbstorage_handle *dev)
{
	s32 retval;

	retval = __usbstorage_reset(dev,0);

	return retval;
}

s32 USBStorage_GetMaxLUN(usbstorage_handle *dev)
{

	return dev->max_lun;
}


s32 USBStorage_MountLUN(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
    int f=handshake_mode;

	if(lun >= dev->max_lun)
		return -EINVAL;
	usb_timeout=5000*1000;
	handshake_mode=1;

	retval= __usbstorage_start_stop(dev, lun, 1);

	#ifdef MEM_PRINT
	   s_printf("    start_stop cmd ret %i\n",retval);
	#endif
    if(retval < 0)
		goto ret;
	
	retval = __usbstorage_clearerrors(dev, lun);
	if(retval < 0)
		goto ret;
	usb_timeout=1000*1000;

	retval = USBStorage_Inquiry(dev, lun);
	#ifdef MEM_PRINT
	   s_printf("    Inquiry ret %i\n",retval);
	#endif
	if(retval < 0)
		goto ret;
	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], &dev->n_sector[lun]);
	#ifdef MEM_PRINT
	   s_printf("    ReadCapacity ret %i\n",retval);
	#endif
ret:
	usb_timeout=1000*1000;
	handshake_mode=f;
	return retval;
}

s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[] = {SCSI_INQUIRY, lun << 5,0,0,36,0};
	u8 *response = USB_Alloc(36);

	if(!response) return -ENOMEM;

	retval = __cycle(dev, lun, response, 36, cmd, 6, 0, NULL, NULL, USBSTORAGE_CYCLE_RETRIES);
        //print_hex_dump_bytes("inquiry result:",DUMP_PREFIX_OFFSET,response,36);
    USB_Free(response);

	return retval;
}
s32 USBStorage_ReadCapacity(usbstorage_handle *dev, u8 lun, u32 *sector_size, u32 *n_sectors)
{
	s32 retval;
	u8 cmd[] = {SCSI_READ_CAPACITY, lun << 5};
	u8 *response = USB_Alloc(8);
        u32 val;
	if(!response) return -ENOMEM;

	retval = __cycle(dev, lun, response, 8, cmd, 2, 0, NULL, NULL, USBSTORAGE_CYCLE_RETRIES);
	if(retval >= 0)
	{
                
                memcpy(&val, response, 4);
		if(n_sectors != NULL)
                        *n_sectors = be32_to_cpu(val);
                memcpy(&val, response + 4, 4);
		if(sector_size != NULL)
			*sector_size = be32_to_cpu(val);
		retval = USBSTORAGE_OK;
	}
        USB_Free(response);
	return retval;
}



static s32 __USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_READ_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >>  8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
		};
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0 || !dev)
		return -EINVAL;
    is_read_write=1;
	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL, 6);
	is_read_write=0;
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

static s32 __USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
	u8 status = 0;
	s32 retval;
	u8 cmd[] = {
		SCSI_WRITE_10,
		lun << 5,
		sector >> 24,
		sector >> 16,
		sector >> 8,
		sector,
		0,
		n_sectors >> 8,
		n_sectors,
		0
		};
	if(lun >= dev->max_lun || dev->sector_size[lun] == 0)
		return -EINVAL;

	is_read_write=1;
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL,6);
	is_read_write=0;
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
u32 max_sectors=n_sectors;
u32 sectors;
s32 ret=-1;

	if(n_sectors * dev->sector_size[lun]>65536) max_sectors= 65536/dev->sector_size[lun];

	while(n_sectors>0)
		{
		sectors=n_sectors>max_sectors ? max_sectors: n_sectors;
		ret=__USBStorage_Read(dev, lun, sector, sectors, buffer);
		if(ret<0) return ret;
		
		n_sectors-=sectors;
		sector+=sectors;
		buffer+=sectors * dev->sector_size[lun];
		}

return ret;
}

s32 USBStorage_Write(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, const u8 *buffer)
{
u32 max_sectors=n_sectors;
u32 sectors;
s32 ret=-1;

	if((n_sectors * dev->sector_size[lun])>65536) max_sectors=65536/dev->sector_size[lun];

	while(n_sectors>0)
		{
		sectors=n_sectors>max_sectors ? max_sectors: n_sectors;
		ret=__USBStorage_Write(dev, lun, sector, sectors, buffer);
		if(ret<0) return ret;
		
		n_sectors-=sectors;
		sector+=sectors;
		buffer+=sectors * dev->sector_size[lun];
		}

return ret;
}
/*
The following is for implementing the ioctl interface inpired by the disc_io.h
as used by libfat

This opens the first lun of the first usbstorage device found.
*/



/* perform 512 time the same read */
s32 USBStorage_Read_Stress(u32 sector, u32 numSectors, void *buffer)
{
   s32 retval;
   int i;
   if(__mounted != 1)
       return false;
   
   for(i=0;i<512;i++){
           retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
           sector+=numSectors;
           if(retval == USBSTORAGE_ETIMEDOUT)
           {
                   __mounted = 0;
                   USBStorage_Close(&__usbfd);
           }
           if(retval < 0)
                   return false;
   }
   return true;

}



// temp function before libfat is available */
s32 USBStorage_Try_Device(struct ehci_device *fd)
{
        int maxLun,j,retval;
		int test_max_lun=1;

		__vid=0;
		__pid=0;
		__mounted = 0;
		__lun = 0;

		try_status=-120;
		if(USBStorage_Open(&__usbfd, fd) < 0)
			return -EINVAL;
	  
	/*
       maxLun = USBStorage_GetMaxLUN(&__usbfd);
       if(maxLun == USBSTORAGE_ETIMEDOUT)
			{
		    __mounted = 0;
		    USBStorage_Close(&__usbfd);
			return -EINVAL;
			}
*/
	 
	maxLun= 1;
    __usbfd.max_lun = 1;  

	j=0; 
      //for(j = 0; j < maxLun; j++)
	while(1)
       {

		   #ifdef MEM_PRINT
		   s_printf("USBStorage_MountLUN %i#\n", j);
		   #endif
           retval = USBStorage_MountLUN(&__usbfd, j);
		   #ifdef MEM_PRINT
		   s_printf("USBStorage_MountLUN: ret %i\n", retval);
		   #endif

		  
           if(retval == USBSTORAGE_ETIMEDOUT /*&& test_max_lun==0*/)
           { 
               //USBStorage_Reset(&__usbfd);
			   try_status=-121;
			   __mounted = 0;
               USBStorage_Close(&__usbfd); 
			   return -EINVAL;
             //  break;
           }

           if(retval < 0)
				{
				if(test_max_lun)
					{
					__usbfd.max_lun = 0;
					retval = __USB_CtrlMsgTimeout(&__usbfd, 
						(USB_CTRLTYPE_DIR_DEVICE2HOST | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), 
						USBSTORAGE_GET_MAX_LUN, 0, __usbfd.interface, 1, &__usbfd.max_lun);
					if(retval < 0 )
						__usbfd.max_lun = 1;
					else __usbfd.max_lun++;
					maxLun = __usbfd.max_lun;

					 #ifdef MEM_PRINT
					 s_printf("USBSTORAGE_GET_MAX_LUN ret %i maxlun %i\n", retval,maxLun);
					 #endif
					test_max_lun=0;
					}
				else j++;

				if(j>=maxLun) break;
				continue;
				}

           __vid=fd->desc.idVendor;
           __pid=fd->desc.idProduct;
           __mounted = 1;
           __lun = j;
		   usb_timeout=1000*1000;
		   try_status=0;
           return 0;
       }
	   try_status=-122;
	   __mounted = 0;
	   USBStorage_Close(&__usbfd);

	   #ifdef MEM_PRINT
	   s_printf("USBStorage_MountLUN fail!!!\n");
	   #endif
	  
       return -EINVAL;
}

void USBStorage_Umount(void)
{
if(!ums_init_done) return;
	
	if(__mounted && !unplug_device)
		{
		if(__usbstorage_start_stop(&__usbfd, __lun, 0x0)==0) // stop
		ehci_msleep(1000);
		}

	USBStorage_Close(&__usbfd);__lun= 16;
    __mounted=0;
	ums_init_done=0;
	unplug_device=0;
}


s32 USBStorage_Init(void)
{
        int i;
		u32 status;
		int ret;

        debug_printf("usbstorage init %d\n", ums_init_done);
        if(ums_init_done)
          return 0;

	 try_status=-1;      

#ifdef MEM_PRINT
s_printf("\n***************************************************\nUSBStorage_Init()\n***************************************************\n\n");

#endif

        for(i = 0;i<1/*ehci->num_port*/; i++){
                struct ehci_device *dev = &ehci->devices[i];
				
				dev->port=i;

                if(dev->id != 0)
					{
		
					ret=ehci_reset_port(0);
					status=ehci_readl(&ehci->regs->port_status[0]);

					if(ret<0 || (status & 0x3905)!=0x1005)
						{
						ret=ehci_reset_port2(0);
						status=ehci_readl(&ehci->regs->port_status[0]);
						}
				
					handshake_mode=1;
					if(ret>=0 && (status & 0x3905)==0x1005 )
						{

						if(USBStorage_Try_Device(dev)==0) 
							{first_access=true;handshake_mode=0;ums_init_done = 1;unplug_device=0;
							#ifdef MEM_PRINT
							s_printf("USBStorage_Init() Ok\n");
							
							#endif

							return 0;
							}
						}
					}
				else
					{
				   

                    handshake_mode=1;
					status = ehci_readl(&ehci->regs->port_status[0]);

					#ifdef MEM_PRINT
					s_printf("USBStorage_Init() status %x\n",status);
					
					#endif
					
					if(status & 1)
						{
						ret=ehci_reset_port2(0);
						status=ehci_readl(&ehci->regs->port_status[0]);

							if(ret<0 || (status & 0x3905)!=0x1005)
								{
								ehci_msleep(100);
								ret=ehci_reset_port(0);
								//status=ehci_readl(&ehci->regs->port_status[0]);
								}
						

						return -101;
						}
					else 
						{
						return -100;
						}
					}
        }

	
        return try_status;
}

s32 USBStorage_Get_Capacity(u32*sector_size)
{
if(sector_size) *sector_size = 0;
   if(__mounted == 1 && __lun!=16)
   {
           if(sector_size){
                   *sector_size = __usbfd.sector_size[__lun];
           }
           return __usbfd.n_sector[__lun];
   }
   return 0;
}

int unplug_procedure(void)
{
int retval=1;

 if(unplug_device!=0 )
			{
	        if(__usbfd.buffer == NULL)
				{
				
				__usbfd.buffer = USB_Alloc(MAX_TRANSFER_SIZE+16);
				
				}
				if(__mounted!=0)
					__usbstorage_reset(&__usbfd,0); // perform hardware reset

               unplug_device=1;

			   if(unplug_device!=0 || __mounted==0)
				{
				 int ret;
				 u32 status;
				 ret=ehci_reset_port2(0);
				 ehci_msleep(60);

				 status=ehci_readl(&ehci->regs->port_status[0]);

				 /*if(ret<0 || (status & 0x3905)!=0x1005 )
					{
					ret=ehci_reset_port(0);
					ehci_msleep(60);
					
					status=ehci_readl(&ehci->regs->port_status[0]);
					}
					*/

				 if(ret>=0 && (status & 0x3905)==0x1005 )	
					{
			
					if(__usbfd.buffer != NULL)
						USB_Free(__usbfd.buffer);
					__usbfd.buffer= NULL;

					handshake_mode=1;
					if(USBStorage_Try_Device(&ehci->devices[0])==0) {retval=0;unplug_device=0;}
					handshake_mode=0;
					}
				}
			ehci_msleep(100);
			}

return retval;
}

int is_watchdog_read_sector=0;

s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
   s32 retval=0;
   int retry;
  /*
   if(__mounted != 1)
		{
		if(unplug_procedure()) 
			return false;
	    }
   */
   for(retry=0;retry<4;retry++)
	{
     if(!is_watchdog_read_sector) retry=0; // infinite loop
	//ehci_usleep(100);
	 if(!unplug_procedure())
		{
		 retval=0;
		}
	
		if(retval<0 || __mounted != 1)
		   {
		   unplug_device=1;
		   retval=-1;
		   }
		 if(unplug_device!=0 ) continue;
		 //if(retval==-ENODEV) return 0;
		 usb_timeout=1000*1000;
	    if(retval >= 0)
		   retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
		usb_timeout=1000*1000;
		if(retval<0) unplug_device=1;

		if(unplug_device!=0 || __mounted != 1) continue;
		 //if(retval==-ENODEV) return 0;
		if(retval>=0) break;

		
	}

  /* if(retval == USBSTORAGE_ETIMEDOUT)
   {
       __mounted = 0;
       USBStorage_Close(&__usbfd);
   }*/
   if(retval < 0)
       return false;
   return true;
}


s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
   s32 retval=0;
   int retry;
/*
  if(__mounted != 1)
		{
		if(unplug_procedure()) 
			return false;
	    }*/

    for(retry=0;retry<4;retry++)
	{
	 retry=0; // infinite loop
	
	//ehci_usleep(100);

	  if(!unplug_procedure())
		{
		 retval=0;
		}

		if(retval<0 || __mounted != 1)
		   {
		   unplug_device=1;
		   retval=-1;
		   }
		  if(unplug_device!=0 ) continue;
		 usb_timeout=1000*1000;
	    if(retval >=0)
		   retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);
		usb_timeout=1000*1000;
		if(retval<0) unplug_device=1;

		if(unplug_device!=0 ) continue;
		if(retval>=0) break;
	}


  /* retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);
   if(retval == USBSTORAGE_ETIMEDOUT)
   {
       __mounted = 0;
       USBStorage_Close(&__usbfd);
   }
   */


   if(retval < 0)
       return false;
   return true;
}

