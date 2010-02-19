/*   
	Custom IOS module for Wii.
        wbfs glue
    Copyright (C) 2009 kwiirk.
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
 * oh0_loop.c - IOS module main code
 * even if we are "ehc" driver, we still pretend to be "oh0"
 * and implement "standard" ios oh0 usb api
 *
 *******************************************************************************
 *
 */



#include <stdio.h>
#include <string.h>
#include "syscalls.h"
#include "libwbfs.h"

u32 n_sec,sec_size;





static int read_sector(void *ign,u32 lba,u32 count,void*buf)
{
        int ret;
		
	
	    os_sync_after_write(buf, count*sec_size);

	/*	do
		{*/
        ret = USBStorage_Read_Sectors(lba,count, buf);
		/*}*/
        if(!ret) return 1;

        os_sync_before_read(buf, count*sec_size);
        return 0;
}

wbfs_disc_t * wbfs_init_with_partition(u8*discid, int partition)
{
        static wbfs_t *p=NULL;
		static wbfs_disc_t *d=NULL;
		static u8 old_discid[6]="";
		
		// opens the hd only is is not opened
		if(!p)
			{
			USBStorage_Init();
			n_sec =  USBStorage_Get_Capacity(&sec_size);
			//debug_printf("hd found n_sec:%x sec_size %x\n",n_sec,sec_size);
			if (n_sec==0)
                return NULL; //no hd
			p = wbfs_open_hd(read_sector, 0, 0, sec_size, n_sec,partition, 0); 
			if(!p) // no partition
                return NULL;
			}
		// close previously disc opened except if discid is equal
		if(d) 
			{
			
			if(!memcmp(old_discid,discid,6)) return d;
			
			wbfs_close_disc(d);d=NULL;
			}

        // open the disc
        d=wbfs_open_disc(p, discid);

		if(d) memcpy(old_discid,discid,6);

        return d;
}
