/*   
	IOS Tester, tests communication with custom IOS module for Wii.
    Copyright (C) 2008 neimod.
    Copyright (C) 2009 Kwiirk

	Startup code from devkitPPC template example.

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
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

static int haxx_fd = -1;

#define IOCTL_CALL				0xDB
#define IOCTL_JUMP				0xDC
#define IOCTL_TELL				0xDD


#include <stdarg.h>
void debug_printf(const char *fmt, ...) {
  char buf[1024];
  int len;
  va_list ap;
  usb_flush(1);
  va_start(ap, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (len <= 0 || len > sizeof(buf)) printf("Error: len = %d\n", len);
  else usb_sendbuffer(1, buf, len);
  printf("%s",buf);
}


int HAXX_Init() 
{ 
    if (haxx_fd >= 0) 
		return 1;

    haxx_fd = IOS_Open("/dev/haxx", 0);

    if (haxx_fd < 0) 
	{
		return -1;
	}

    return 0;
}

int HAXX_Close() 
{
	return IOS_Close(haxx_fd);
}

int HAXX_Read(unsigned char* buf, unsigned int size) 
{
	return IOS_Read(haxx_fd, buf, size);
}

int HAXX_Write(const unsigned char* buf, unsigned int size) 
{
	return IOS_Write(haxx_fd, buf, size);
}

int HAXX_Seek(int offset, int origin) 
{
	return IOS_Seek(haxx_fd, offset, origin);
}


unsigned int HAXX_Tell() 
{
	unsigned long bufferio;

	IOS_Ioctl(haxx_fd, IOCTL_TELL, 0, 0, &bufferio, sizeof(bufferio));

	return bufferio;
}

unsigned int HAXX_Execute(void* bin, unsigned int binsize) 
{
	debug_printf("Executing arm code..\n");
	unsigned int result =  IOS_Ioctl(haxx_fd, IOCTL_CALL, bin, binsize, 0, 0);
	debug_printf("Execute result: %d\n", result);

	return result;
}

void my_exit(int i)
{
  printf("exiting with code %d\n",i);
  if(i==0) exit(0);

}

void usb_test(void);
#include "arm_bin.h"
//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Reload to IOS which has the custom IOS module installed, in my case, IOS 202
        //VIDEO_WaitVSync();
	//IOS_ReloadIOS(202);
	IOS_ReloadIOS(222);

	// Initialise the video system
	VIDEO_Init();
	
	// This function initialises the attached controllers
	WPAD_Init();
	
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	
	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	
	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);
	
	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);
	
	// Make the display visible
	VIDEO_SetBlack(FALSE);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();


	int dohaxx = HAXX_Init();

        if (dohaxx >= 0) // if the cios_usb installed is Haxx, run our code.
        {
                // Let IOS module execute ARM test code
                HAXX_Execute((void*)arm_bin, arm_bin_size);
        }
        usb_test();

	//IOS_ReloadIOS(31);
        //exit(0);
        debug_printf("waiting for home key pressed to ensure bluetooth still works..\n");
        while(1) 
          {
            // Call WPAD_ScanPads each loop, this reads the latest controller states
            WPAD_ScanPads();

            // WPAD_ButtonsDown tells us which buttons were pressed in this loop
            // this is a "one shot" state which will not fire again until the button has been released
            u32 pressed = WPAD_ButtonsDown(0);

            // We return to the launcher application via exit
            if ( pressed & WPAD_BUTTON_HOME )
              {
                //SYS_ResetSystem(SYS_RESTART,0,0);
                exit(0);
              }
            // Wait for the next frame
            VIDEO_WaitVSync();
          }

	return 0;
}
