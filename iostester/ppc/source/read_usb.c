/*   
    Q&D test file for cios_usb2

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
#include <sys/types.h>
#include <ogc/lwp_watchdog.h>

#include <dirent.h>
#include <string.h>
#include <fat.h>
#include <malloc.h>
#include <ogc/usbstorage.h>

void debug_printf(const char *fmt, ...);
#define MAXPATHLEN 1024

void read_files(char *curdir,int step)
{
        DIR_ITER* dir = diropen (curdir); 
        struct stat st;
        char pathname[MAXPATHLEN];
        char* filename;
        char tab[8*4];
        int i;
	if(step>2)
                return;
        for (i=0;i<step;i++)
        {
                tab[i]='\t';
        }
        tab[i]='\0';
        strcpy(pathname,curdir);
        i = strlen(pathname);
        pathname[i] = '/';
        filename = pathname+i+1;
        if (dir) {
                while (!dirnext(dir, filename, &st)) {
                        if(st.st_mode & S_IFDIR){
                                if(strcmp(filename, ".") && strcmp(filename, "..")){
                                        debug_printf("%s:%s\n",tab,pathname);
                                        read_files(pathname,step+1);
                                }
                        }
                        else{
                                debug_printf("%s:%s\n",tab,filename);
                        }
                }
        }else{
                debug_printf("d %s:?\?!!\n",tab);
        }
}


void fat_find(void)
{
        debug_printf("read dir..\n");
        if (__io_usbstorage.isInserted())
        {
                read_files("usb://",0);
        }

}
void perf_test(void)
{
        static s32 hId = -1;
        if(hId==-1) hId = iosCreateHeap(0x9000);
        if (__io_usbstorage.isInserted())
        {
                u8 *buffer = iosAlloc(hId,0x8000); /* 32k transfers */
                int i,j;
                u64 start = gettime();
                int mb = 50;
                float sec = 0;
                if(!buffer){ debug_printf("unable to allocate memory\n");return;}

                for( j = 0; j < mb/4;j++){
                        for( i = 0; i < 512;i++){
                                int ret = __io_usbstorage.readSectors(i,0x8000/512,buffer);
                                if(ret<0)
                                        debug_printf("error:)..\n");
                        }
                        debug_printf(".");
                }
                sec = ticks_to_millisecs(diff_ticks(start,gettime()))/1000.;
                debug_printf("\ntransfered %dMB in %.2f s %.2fMB/s..\n",mb,sec,mb/sec);
        }

}
void usb_test(void) {
        if(!fatInitDefault()){
                debug_printf("unable to init fat..\n");
        }
//        fat_find();
        perf_test();
	return ;
}
