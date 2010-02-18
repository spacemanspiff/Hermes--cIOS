#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ogcsys.h>
#include <gccore.h>

#include "unistd.h"

#include "mload.h"
#include "example1_elf.h"
//#include "ehcmodule_elf.h"

GXRModeObj *rmode;		// Graphics Mode Object
u32 *xfb = NULL;		// Framebuffer  


int exit_by_reset=0;

void reset_call() {exit_by_reset=1;}

data_elf my_data_elf;

int my_thread_id=0;


static const char ehc_fs[] ATTRIBUTE_ALIGN(32) = "/dev/usb/ehc";
s32 hid2 = -1;

int main(int argc, char **argv) 
{
int ret;

	VIDEO_Init();                                        //Inicialización del Vídeo.
                                      
	rmode = VIDEO_GetPreferredMode(NULL);                //mediante esta función rmode recibe el valor de tu modo de vídeo.
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));  //inicialización del buffer.
	console_init(xfb,20,20,rmode->fbWidth,rmode->        //inicialización de la consola.
	xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);  
	VIDEO_Configure(rmode);                              //configuración del vídeo.
	VIDEO_SetNextFramebuffer(xfb);                       //Configura donde guardar el siguiente buffer .
	VIDEO_SetBlack(FALSE);                               //Hace visible el display  .                     
	VIDEO_Flush();
	VIDEO_WaitVSync();                                   
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	SYS_SetResetCallback(reset_call);

	IOS_ReloadIOS(222);
	sleep(1);

	/* secuencias de escape 

	\33[2J -> borra pantalla y posiciona en 1,1
	\33[1;1H -> posiciona en 1,1
	\33[42m -> color de fondo verde (0->negro, 1->rojo, 2->verde, 3-> amarillo, 4->azul, 5->magenta 6->cyan 7->blanco )
	\33[32m -> color de letras verde

	*/

	printf("\33[2J\n\n\n \33[42m dev/mload Test \33[40m \n\n\n\n");

	ret=mload_init();
	if(ret<0)
		{
		printf("fail to get dev/mload\n");
		
		goto out;
		}
	else
		{
		u32 addr;
		int len;

		printf("Hello!: my name is dev/mload and my thread id is %i\n", mload_get_thread_id());
		mload_get_load_base(&addr, &len);
		printf("You have from 0x%x to 0x%x to work\n\n", addr, addr+len-1);
		}
	/*
	mload_elf((void *) ehcmodule_elf, &my_data_elf);
	my_thread_id=mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);
	*/

	printf("Loading example1.elf (Starlet)\n");

	if(((u32) example1_elf) & 3) {printf("Unaligned elf!\n"); goto out;}

	mload_elf((void *) example1_elf, &my_data_elf);

	printf("Running... at 0x%x\n", (u32) my_data_elf.start);

	my_thread_id=mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);

    if(my_thread_id<0) {printf("fail to init the starlet thread: %i\n", my_thread_id); goto out;}

	printf("Module thread id: %i Priority in elf: 0x%x\n", my_thread_id, my_data_elf.prio);

	printf("The example Module counts seconds and report the thread id and the priority\n");

	{
	#if 0
		int n,m;

		sleep(1);
		#define UMS_BASE			(('U'<<24)|('M'<<16)|('S'<<8))
		#define USB_IOCTL_UMS_INIT	        (UMS_BASE+0x1)
		
		s32 hid2 = -1;
		hid2 = iosCreateHeap(1024);
		n=IOS_Open(ehc_fs, 0);
		if(n<0) printf("Error: device not found\n");
		else {
			printf("OK\n");
		     /* Initialize USB storage */
		     m=IOS_IoctlvFormat(hid2, n, USB_IOCTL_UMS_INIT, ":");

			 }

    #endif	
	}

	while(1)
		{
		static u32 data[8] ATTRIBUTE_ALIGN(32);

		if(exit_by_reset) break;

		mload_seek(0x13740000, SEEK_SET);
		mload_read(&data,32);
		printf("\33[16;16H Starlet activity: %i Thid: %i Prio: 0x%x    \n",data[0],data[1],data[2]);
		
		VIDEO_WaitVSync();
		}

	// you can stops the Starlet thread using this function:
	 mload_stop_thread(my_thread_id);
out:
	mload_close();
VIDEO_WaitVSync();
sleep(4);

return 0;
}

