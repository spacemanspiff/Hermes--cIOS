/*   
	IOS Tester, tests communication with custom IOS module for Wii.
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
#include <string.h>
#include <setjmp.h>
#include "syscalls.h"
#include "ios_usbstorage.h"
#define swab32(x) ((u32)(                                     \
                                   (((u32)(x) & (u32)0x000000ffUL) << 24) | \
                                   (((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
                                   (((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
                                   (((u32)(x) & (u32)0xff000000UL) >> 24)))
#define read_le32_unaligned(x) ((x)[0]|((x)[1]<<8)|((x)[2]<<16)|((x)[3]<<24))
#define WII_DISC_MAGIC	0x5D1C9EA3
int wii_lba = 0;
void decode_mbr(u8*mbr)
{
  int i;
  u32 lba;
  u32 nblocks;
  u8*ptr = mbr+0x1be;
     for(i=0;i<4;i++,ptr+=16)
     {
          if(ptr[4]==0x83)//first unformatted partition
          {
               wii_lba = read_le32_unaligned(ptr+0x8);
               break;
          }
     }
}
/*******************************************************************************
 *
 * main.c - IOS test code main
 *
 *******************************************************************************
 *
 *
 * v1.0 - 26 July 2008				- initial release by neimod
 * v1.1 - 5 September 2008			- prepared for public release
 *
 */
int main(void)
{
     int i;
     u8 *buf = (u8*)0x1250000; // use some buffer inside MEM2..
     u32 *buf32 = (u32*)0x1250000; // use some buffer inside MEM2..
     /* here, we can call the ums api from haxx module */
     ums_init();
     ums_read_sectors(0,1,buf);
     decode_mbr(buf);
     for(i=0;i<10;i++){
          debug_printf("try %d\n",wii_lba+i);
          ums_read_sectors(wii_lba+i,1,buf);
          if (buf32[6] == WII_DISC_MAGIC) {
               debug_printf("found wii magic @ %d\n",i);
          }

     }
     ums_close();
     return 666;
}
