#include "syscalls.h"
#include <string.h>
#include "ehci_types.h"
#include "usb.h"
#include "ehci.h"
static  int heap;

extern u8 heap_space2[0xb000];

int usb_os_init(void)
{
        //heap = 4;//*((unsigned int*)0x138a71f8);//os_heap_create((void*)0x138ab1f8, 0x2000);
        heap = os_heap_create((u8 *)((((u32) heap_space2+4095) & ~4095)+0x2000)/*(void*)0x13892000*/, 0x8000);
		//heap = os_heap_create((void*)0x13890000, 0x8000);
        if(heap<0)
        {
                debug_printf("\n\nunable to create heap :( %d\n\n",heap);
        }
        return 0;
}
static u8* aligned_mem = 0;
static u8* aligned_base = 0;
/* @todo hum.. not that nice.. */
void*ehci_maligned(int size,int alignement,int crossing)
{
        if (!aligned_mem )
        {
                aligned_mem=aligned_base =  (u8 *)((((u32) heap_space2+4095) & ~4095));//(void*)0x13890000;
        }
        u32 addr=(u32)aligned_mem;
        alignement--;
        addr += alignement;
        addr &= ~alignement;
        if (((addr +size-1)& ~(crossing-1)) != (addr&~(crossing-1)))
                addr = (addr +size-1)&~(crossing-1);
        aligned_mem = (void*)(addr + size);
        if (aligned_mem>aligned_base + 0x2000) 
        {
                debug_printf("not enough aligned memory!\n");
		while(1) msleep(1);
        }
        memset((void*)addr,0,size);
        return (void*)addr;
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


void *USB_Alloc(int size)
{
  void * ret = 0;
  ret= os_heap_alloc(heap, size);
  if(ret==0)
	{debug_printf("not enough memory! need %d\n",size);
    while(1) msleep(100);
	}
  return ret;
}
void USB_Free(void *ptr)
{
        return os_heap_free(heap, ptr);
}

