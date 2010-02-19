#include "syscalls.h"
#include <string.h>
#include "ehci_types.h"
#include "usb.h"
#include "ehci.h"
static  int heap;

void ehci_usleep(int usec);
void ehci_msleep(int msec);

extern u8 heap_space2[0xc000];

int usb_os_init(void)
{
        heap = os_heap_create(heap_space2, 0xc000);
		//heap = os_heap_create((void*)0x13890000, 0x8000);
        if(heap<0)
        {
                debug_printf("\n\nunable to create heap :( %d\n\n",heap);
        }
        return 0;
}


dma_addr_t ehci_virt_to_dma(void *a)
{

        return (dma_addr_t)a;
}
dma_addr_t ehci_dma_map_to(void *buf,size_t len)
{
        os_sync_after_write(buf, len);
        return (dma_addr_t)buf;

}
dma_addr_t ehci_dma_map_from(void *buf,size_t len)
{
        os_sync_after_write(buf, len);
        return (dma_addr_t)buf;
}
dma_addr_t ehci_dma_map_bidir(void *buf,size_t len)
{
        //debug_printf("sync_after_write %p %x\n",buf,len);
 
        os_sync_after_write(buf, len);
        return (dma_addr_t)buf;
}
void ehci_dma_unmap_to(dma_addr_t buf,size_t len)
{
        os_sync_before_read((void*)buf, len);
}
void ehci_dma_unmap_from(dma_addr_t buf,size_t len)
{
        os_sync_before_read((void*)buf, len);
}
void ehci_dma_unmap_bidir(dma_addr_t buf,size_t len)
{
        os_sync_before_read((void*)buf, len);
}

void my_sprint(char *cad, char *s);

void *USB_Alloc(int size)
{
  void * ret = 0;
  ret= os_heap_alloc(heap, size);
  if(ret==0)
	{debug_printf("not enough memory! need %d\n",size);
    my_sprint("USB Alloc: not enough memory!", NULL);
    while(1) ehci_msleep(100);
	}
  return ret;
}
void USB_Free(void *ptr)
{
        return os_heap_free(heap, ptr);
}

