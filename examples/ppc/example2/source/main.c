#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include <ogcsys.h>
#include <gccore.h>

#include "unistd.h"

#include "mload.h"
#include "fat_elf.h"


GXRModeObj *rmode;		// Graphics Mode Object
u32 *xfb = NULL;		// Framebuffer  


int exit_by_reset=0;

void reset_call() {exit_by_reset=1;}

data_elf my_data_elf;

int my_thread_id=0;

static const char fat_fs[] ATTRIBUTE_ALIGN(32) = "/dev/fat/log";
static const char fat_fs_log[] ATTRIBUTE_ALIGN(32) = "/dev/fat/log";
static const char fat_fs2[] ATTRIBUTE_ALIGN(32) = "/dev/fat/sd:/my_file.txt";
//static const char fat_fs2[] ATTRIBUTE_ALIGN(32) = "/dev/fat/my_file.txt"; // alternative form
static char dump_datas[256] ATTRIBUTE_ALIGN(32);

static int fd=-1;

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
	

	printf("Loading fat.elf\n");

	if(((u32) fat_elf) & 3) {printf("Unaligned elf!\n"); goto out;}

	mload_elf((void *) fat_elf, &my_data_elf);

	printf("Running... at 0x%x\n", (u32) my_data_elf.start);

	my_thread_id=mload_run_thread(my_data_elf.start, my_data_elf.stack, my_data_elf.size_stack, my_data_elf.prio);

    if(my_thread_id<0) {printf("fail to init the starlet thread: %i\n", my_thread_id); goto out;}

	printf("Module thread id: %i Priority in elf: 0x%x\n", my_thread_id, my_data_elf.prio);

	sleep(2); // Wait a moment

	printf("The example Module provides FAT/FAT32 access to the SD from the Starlet\n\n");

	#if 0
	// list root directory to sd:/directory.txt
	fd=IOS_Open(fat_fs, 0);

	if(fd>=0)
		{
		ret= IOS_IoctlvFormat(iosCreateHeap(0x800), fd, 0xcacafea, ":");
		IOS_Close(fd);
		printf("ioctlv %i\n\n", ret);
		}
	 #endif



    printf("Writing a Log message in the SD (file log000.txt surely)\n\n");

	fd=IOS_Open(fat_fs_log, 0);
	if(fd>=0)
		{
		strcpy(dump_datas,"Hello guy");
		ret=IOS_Write(fd, dump_datas,strlen(dump_datas));
		if(ret==strlen(dump_datas)) printf("Ok. Writed %i bytes (fd = %i)\n",ret, fd); 
		else printf("Error Writing the SD Writed %i bytes (fd = %i)\n",ret, fd);
		IOS_Close(fd);
		}
	else printf("Error Writing the SD\n");

#include <fcntl.h> // O_CREAT, O_RDWR and others flags


	printf("\nCreate a file in the SD (my_file.txt)\n\n");

	fd=IOS_Open(fat_fs2, O_CREAT | O_TRUNC | O_WRONLY);
	if(fd>=0)
		{
		int n;
		strcpy(dump_datas,"blah blah blah ");
		n=IOS_Write(fd, dump_datas,strlen(dump_datas));
		n+=IOS_Write(fd, dump_datas,strlen(dump_datas)); // repeat

		ret=IOS_Seek(fd,0, SEEK_END);
		
		if(n!=ret) printf("Error!: Only %i bytes writed\n", ret);
			printf("Writed %i bytes (fd = %i)\n",ret, fd);

		IOS_Close(fd);
		}
	else printf("Error Writing the SD\n");
	
	printf("\nPress RESET to exit\n");

	while(1)
		{
		

		if(exit_by_reset) break;


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

