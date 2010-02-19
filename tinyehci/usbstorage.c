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
#define	TAG_START			0x0BADC0DE

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
#define	MASS_STORAGE_ATA_COMMANDS	0x02
#define	MASS_STORAGE_QIC_COMMANDS	0x05
#define	MASS_STORAGE_SCSI_COMMANDS	0x06
#define	MASS_STORAGE_BULK_ONLY		0x50

#define USBSTORAGE_GET_MAX_LUN		0xFE
#define USBSTORAGE_RESET		0xFF

#define	USB_ENDPOINT_BULK		0x02

#define USBSTORAGE_CYCLE_RETRIES	10

#define MAX_TRANSFER_SIZE			4096

#define DEVLIST_MAXSIZE    8

static int ums_init_done = 0;

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


static s32 __cycle(usbstorage_handle *dev, u8 lun, u8 *buffer, u32 len, u8 *cb, u8 cbLen, u8 write, u8 *_status, u32 *_dataResidue)
{
	s32 retval = USBSTORAGE_OK;

	u8 status = 0;
	u32 dataResidue = 0;
	u32 thisLen;

	s8 retries = USBSTORAGE_CYCLE_RETRIES + 1;

	do
	{

	if(retval==-ENODEV) {unplug_device=1;return -ENODEV;}
	
	

                if(retval < 0)
                        retval=__usbstorage_reset(dev,retries < USBSTORAGE_CYCLE_RETRIES);
				if(retval<0) continue; // nuevo


		retries--;
		if(write)
		{

			retval = __send_cbw(dev, lun, len, CBW_OUT, cb, cbLen);
			if(retval < 0)
				continue;//reset+retry
			while(len > 0)
			{
                                thisLen=len;
				retval = __USB_BlkMsgTimeout(dev, dev->ep_out, thisLen, buffer);
           
			if(retval==-ENODEV) break;

				if(retval < 0)
					continue;//reset+retry

				if(retval != thisLen && len > 0)
				{
					retval = USBSTORAGE_EDATARESIDUE;
					continue;//reset+retry
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
				if(retval < 0)
					continue; //reset+retry
                                //hexdump(buffer,retval);
                if(retval==-ENODEV) break;
				len -= retval;
				buffer += retval;

				if(retval != thisLen)
                                {
                                        retval = -1;
                                        continue; //reset+retry
                                }
			}

			if(retval < 0)
				continue;
		}

		retval = __read_csw(dev, &status, &dataResidue);

		if(retval < 0)
			continue;

		retval = USBSTORAGE_OK;
	} while(retval < 0 && retries > 0);

    // force unplug
	if(retval < 0 && retries <= 0) {unplug_device=1;return -ENODEV;}

	if(retval < 0 && retval != USBSTORAGE_ETIMEDOUT)
	{
		if(__usbstorage_reset(dev,0) == USBSTORAGE_ETIMEDOUT)
			retval = USBSTORAGE_ETIMEDOUT;
	}


	if(_status != NULL)
		*_status = status;
	if(_dataResidue != NULL)
		*_dataResidue = dataResidue;

	return retval;
}

static s32 __usbstorage_start_stop(usbstorage_handle *dev, u8 lun, u8 start_stop)
{
	s32 retval;
	u8 cmd[16];
	
	u8 status = 0;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_START_STOP;
	cmd[1] = (lun << 5) | 1;
	cmd[4] = start_stop & 3;
	cmd[5] = 0;
	//memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
	retval = __cycle(dev, lun, NULL, 0, cmd, 6, 0, &status, NULL);
	if(retval < 0) goto error;

	
	if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) 
					retval = USBSTORAGE_ESENSE;
error:
	return retval;
}

static s32 __usbstorage_clearerrors(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[16];
	u8 *sense= USB_Alloc(SCSI_SENSE_REPLY_SIZE);
	u8 status = 0;
	memset(cmd, 0, sizeof(cmd));
	cmd[0] = SCSI_TEST_UNIT_READY;

	if(!sense) return -ENOMEM;
    
	/*while(1)
		{*/
		retval = __cycle(dev, lun, NULL, 0, cmd, 1, 1, &status, NULL);
		if(retval < 0) goto error; // Hermes: bug fixed (free(sense))
		/*if(status==0 || status!=0x8) break;
		ehci_msleep(10);
		}*/

	if(status != 0)
	{
		cmd[0] = SCSI_REQUEST_SENSE;
		cmd[1] = lun << 5;
		cmd[4] = SCSI_SENSE_REPLY_SIZE;
		cmd[5] = 0;
		memset(sense, 0, SCSI_SENSE_REPLY_SIZE);
		retval = __cycle(dev, lun, sense, SCSI_SENSE_REPLY_SIZE, cmd, 6, 0, NULL, NULL);
		if(retval < 0) goto error;

		status = sense[2] & 0x0F;
		if(status == SCSI_SENSE_NOT_READY || status == SCSI_SENSE_MEDIUM_ERROR || status == SCSI_SENSE_HARDWARE_ERROR) 
                        retval = USBSTORAGE_ESENSE;
	}
error:
        USB_Free(sense);
	return retval;
}

static s32 __usbstorage_reset(usbstorage_handle *dev,int hard_reset)
{
	s32 retval;
        int retry=0 ;//hard_reset;

		//if(hard_reset>=1) retry=1;
 retry:
        if (retry >= 1){
                u8 conf;
                debug_printf("reset device..\n");
                retval = ehci_reset_device(dev->usb_fd);
				ehci_msleep(10);
				if(retval==-ENODEV) return retval;

                if(retval < 0 && retval != -7004)
                        goto end;
                // reset configuration
                if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
                        goto end;
                if(conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
                        goto end;
                if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
                        goto end;

        }
        debug_printf("usbstorage reset..\n");
	retval = __USB_CtrlMsgTimeout(dev, (USB_CTRLTYPE_DIR_HOST2DEVICE | USB_CTRLTYPE_TYPE_CLASS | USB_CTRLTYPE_REC_INTERFACE), USBSTORAGE_RESET, 0, dev->interface, 0, NULL);

	/* FIXME?: some devices return -7004 here which definitely violates the usb ms protocol but they still seem to be working... */
        if(retval < 0 && retval != -7004)
                goto end;


	/* gives device enough time to process the reset */
	ehci_msleep(10);
	

        debug_printf("clear halt on bulk ep..\n");
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_in);
	if(retval < 0)
		goto end;
	retval = USB_ClearHalt(dev->usb_fd, dev->ep_out);
	if(retval < 0)
		goto end;
        
	return retval;

end:
        if(retry < 20){
                ehci_msleep(100);
                debug_printf("retry with hard reset..\n");
                
                retry ++;
                goto retry;
        }
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

s32 USBStorage_Open(usbstorage_handle *dev, struct ehci_device *fd)
{
	s32 retval = -1;
	u8 conf,*max_lun = NULL;
	u32 iConf, iInterface, iEp;
	static usb_devdesc udd;
	usb_configurationdesc *ucd;
	usb_interfacedesc *uid;
	usb_endpointdesc *ued;

	max_lun = USB_Alloc(1);
	if(max_lun==NULL) return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	dev->tag = TAG_START;
        dev->usb_fd = fd;

	retval = USB_GetDescriptors(dev->usb_fd, &udd);
	if(retval < 0)
		goto free_and_return;

	// test device changed without unmount (prevent device write corruption)
	if(ums_init_done)
		{
		if(my_memcmp((void *) &_old_udd, (void *) &udd, sizeof(usb_devdesc)-4)) 
			{
			USB_Free(max_lun);
			USB_FreeDescriptors(&udd);
			return -ENODEV;
			}
		}
	
	_old_udd= udd;

	for(iConf = 0; iConf < udd.bNumConfigurations; iConf++)
	{
		ucd = &udd.configurations[iConf];		
		for(iInterface = 0; iInterface < ucd->bNumInterfaces; iInterface++)
		{
			uid = &ucd->interfaces[iInterface];
                        debug_printf("interface %d, class:%x subclass %x protocol %x\n",iInterface,uid->bInterfaceClass,uid->bInterfaceSubClass, uid->bInterfaceProtocol);
			if(uid->bInterfaceClass    == USB_CLASS_MASS_STORAGE &&
			   (uid->bInterfaceSubClass == MASS_STORAGE_SCSI_COMMANDS 
                            || uid->bInterfaceSubClass == MASS_STORAGE_ATA_COMMANDS
                            || uid->bInterfaceSubClass == MASS_STORAGE_QIC_COMMANDS)&&
			   uid->bInterfaceProtocol == MASS_STORAGE_BULK_ONLY)
			{
				if(uid->bNumEndpoints < 2)
					continue;
                                if(uid->bInterfaceSubClass != MASS_STORAGE_SCSI_COMMANDS)
                                        dev->ata_protocol = 1;
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
			}
		}
	}

	USB_FreeDescriptors(&udd);
	retval = USBSTORAGE_ENOINTERFACE;
        debug_printf("cannot find any interface\n");
	goto free_and_return;

found:
	USB_FreeDescriptors(&udd);

	retval = USBSTORAGE_EINIT;
	
	if(USB_GetConfiguration(dev->usb_fd, &conf) < 0)
		goto free_and_return;
	
	if(conf != dev->configuration && USB_SetConfiguration(dev->usb_fd, dev->configuration) < 0)
		goto free_and_return;
	if(dev->altInterface != 0 && USB_SetAlternativeInterface(dev->usb_fd, dev->interface, dev->altInterface) < 0)
		goto free_and_return;
	
	
	retval = USBStorage_Reset(dev);
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

	dev->buffer = USB_Alloc(MAX_TRANSFER_SIZE+16);
	
	if(dev->buffer == NULL) retval = -ENOMEM;
	else retval = USBSTORAGE_OK;

free_and_return:

	if(max_lun!=NULL) USB_Free(max_lun);

	if(retval < 0)
	{
		if(dev->buffer != NULL)
			USB_Free(dev->buffer);
		memset(dev, 0, sizeof(*dev));
		return retval;
	}
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

	if(lun >= dev->max_lun)
		return -EINVAL;
	usb_timeout=1000*1000;

	/*retval= __usbstorage_start_stop(dev, lun, 1);
    if(retval < 0)
		return retval;*/
	
	retval = __usbstorage_clearerrors(dev, lun);
	if(retval < 0)
		return retval;
	usb_timeout=1000*1000;
	retval = USBStorage_Inquiry(dev, lun);
	retval = USBStorage_ReadCapacity(dev, lun, &dev->sector_size[lun], &dev->n_sector[lun]);
	return retval;
}

s32 USBStorage_Inquiry(usbstorage_handle *dev, u8 lun)
{
	s32 retval;
	u8 cmd[] = {SCSI_INQUIRY, lun << 5,0,0,36,0};
	u8 *response = USB_Alloc(36);

	if(!response) return -ENOMEM;

	retval = __cycle(dev, lun, response, 36, cmd, 6, 0, NULL, NULL);
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

	retval = __cycle(dev, lun, response, 8, cmd, 2, 0, NULL, NULL);
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
    
	retval = __cycle(dev, lun, buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 0, &status, NULL);
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
	retval = __cycle(dev, lun, (u8 *)buffer, n_sectors * dev->sector_size[lun], cmd, sizeof(cmd), 1, &status, NULL);
	if(retval > 0 && status != 0)
		retval = USBSTORAGE_ESTATUS;
	return retval;
}

s32 USBStorage_Read(usbstorage_handle *dev, u8 lun, u32 sector, u16 n_sectors, u8 *buffer)
{
u32 max_sectors=n_sectors;
u32 sectors;
s32 ret=-1;

	if(n_sectors * dev->sector_size[lun]>32768) max_sectors= 32768/dev->sector_size[lun];

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

	if((n_sectors * dev->sector_size[lun])>32768) max_sectors=32768/dev->sector_size[lun];

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

static usbstorage_handle __usbfd;
static u8 __lun = 0;
static u8 __mounted = 0;
static u16 __vid = 0;
static u16 __pid = 0;

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

s32 try_status=0;

// temp function before libfat is available */
s32 USBStorage_Try_Device(struct ehci_device *fd)
{
        int maxLun,j,retval;
		int test_max_lun=1;

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

		   
           retval = USBStorage_MountLUN(&__usbfd, j);
		  
           if(retval == USBSTORAGE_ETIMEDOUT && test_max_lun==0)
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

	USBStorage_Close(&__usbfd);
    __mounted=0;
	ums_init_done=0;
	unplug_device=0;
}


s32 USBStorage_Init(void)
{
        int i,n;
        debug_printf("usbstorage init %d\n", ums_init_done);
        if(ums_init_done)
          return 0;

	 try_status=-1;      

        for(i = 0;i<ehci->num_port; i++){
                struct ehci_device *dev = &ehci->devices[i];
                if(dev->id != 0){
                        
						// reintenta 3 veces
						for(n=0;n<3;n++)
							{
							
							if(USBStorage_Try_Device(dev)==0) {ums_init_done = 1;unplug_device=0;return 0;}
							
							ehci_reset_port2(dev->port);
							ehci_msleep(1000);
							}
                }
				else
					{
				    u32 status;

					status = ehci_readl(&ehci->regs->port_status[0]);
					if(status & 1)
						{
						ehci_reset_port2(0);
						return -101;
						}
					else return -100;
					}
        }

	
        return try_status;
}

s32 USBStorage_Get_Capacity(u32*sector_size)
{
   if(__mounted == 1)
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
			
			if(__usbfd.usb_fd)
			   if(ehci_reset_port2(__usbfd.usb_fd->port)==0)	
				{
                if(__usbfd.buffer != NULL)
						USB_Free(__usbfd.buffer);
				__usbfd.buffer= NULL;

				if(USBStorage_Try_Device(__usbfd.usb_fd)==0) {retval=0;unplug_device=0;}

				}
			ehci_msleep(100);
			}

return retval;
}

s32 USBStorage_Read_Sectors(u32 sector, u32 numSectors, void *buffer)
{
   s32 retval=0;
   int retry;
   if(__mounted != 1)
       return false;
   
   for(retry=0;retry<16;retry++)
	{
	 if(retry>12) retry=12; // infinite loop
	//ehci_usleep(100);
	 if(!unplug_procedure())
		{
		 retval=0;
		}
	
		if(retval == USBSTORAGE_ETIMEDOUT && retry>0)
		   {
		   retval=__usbstorage_reset(&__usbfd,1);
		   if(retval>=0) retval=-666;
		   ehci_msleep(10);
		   }
		 if(unplug_device!=0 ) continue;
		 //if(retval==-ENODEV) return 0;
		 usb_timeout=1000*1000; // 4 seconds to wait
	    if(retval != USBSTORAGE_ETIMEDOUT)
		   retval = USBStorage_Read(&__usbfd, __lun, sector, numSectors, buffer);
		usb_timeout=1000*1000;
		if(unplug_device!=0 ) continue;
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

#if 0
s32 USBStorage_Read_Sectors_ingame(u32 sector, u32 numSectors, void *buffer, int speed_limit)
{
   s32 retval=0;
  
  //  int n;
  // u32 numSectors2;
   //u32 tmr=0,temp;
   //u32 time_us=0;
   
  
  
   if(__mounted != 1)
       return false;
   

  /* if(speed_limit)
	{
		
		// Speed 6x ==8.1 MB /s value 63 for 512 bytes. fixed to 5.0x
		time_us=numSectors*(2*63)*(__usbfd.sector_size[__lun]/512);

        tmr = get_timer();
	}
	*/
    


	while(1)
		{
		if(!unplug_procedure())
			{
			 retval=0;
			}

        usb_timeout=1000*1000; // 4 seconds to wait
	    if(retval != USBSTORAGE_ETIMEDOUT)
			retval = USBStorage_Read(&__usbfd, __lun, sector/*+n*/, numSectors, ((u8 *) buffer)/*+n*__usbfd.sector_size[__lun]*/);
		if(unplug_device!=0 ) continue;
		usb_timeout=1000*1000;
	   
		if(retval == USBSTORAGE_ETIMEDOUT)
		   {
			retval=__usbstorage_reset(&__usbfd,1);
			if(retval>=0) retval=-666;
			
		   }
		if(unplug_device!=0 ) continue;

		if(retval>=0) break;

		}
	

   
  
   /*if(speed_limit)
	{
	while (1) {temp=get_timer()-tmr;if(temp > time_us) break;} // 6x speed limit
	}
	*/
  

   if(retval < 0)
       return false;
   return true;
}
#endif 

s32 USBStorage_Write_Sectors(u32 sector, u32 numSectors, const void *buffer)
{
   s32 retval=0;
   int retry;

   if(__mounted != 1)
       return false;

    for(retry=0;retry<16;retry++)
	{
	 if(retry>12) retry=12; // infinite loop
	
	//ehci_usleep(100);

	  if(!unplug_procedure())
		{
		 retval=0;
		}

		if(retval == USBSTORAGE_ETIMEDOUT && retry>0)
		   {
		   retval=__usbstorage_reset(&__usbfd,1);
		   if(retval>=0) retval=-666;
		   }
		 usb_timeout=1000*1000; // 4 seconds to wait
	    if(retval != USBSTORAGE_ETIMEDOUT)
		   retval = USBStorage_Write(&__usbfd, __lun, sector, numSectors, buffer);
		usb_timeout=1000*1000;
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

