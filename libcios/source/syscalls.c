/*   
	Custom IOS module for Wii.
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
 * syscalls.c - IOS syscalls
 *
 *******************************************************************************
 *
 *
 * v1.0 - 26 July 2008				- initial release by neimod
 * v1.1 - 5 September 2008			- prepared for public release
 *
 */



/*
   NOTE: Syscalls are different across different IOS versions. Current syscalls are for IOS31.

   Best would be to first check what current IOS is running, and then map the OS functions
   to the right syscall number.
 */
#include "syscalls.h"
extern void syscall_0(void);
extern void syscall_9(void);
extern void syscall_a(void);
extern void syscall_e(void);
extern void syscall_16(void);
extern void syscall_17(void);
extern void syscall_18(void);
extern void syscall_1a(void);
extern void syscall_1b(void);
extern void syscall_1c(void);
extern void syscall_1d(void);
extern void syscall_22(void);
extern void syscall_2a(void);
extern void syscall_3f(void);
extern void syscall_40(void);
extern void syscall_50(void);
extern void svc_4(void);

unsigned int os_thread_create( unsigned int (*entry)(void* arg), void* arg, void* stack, unsigned int stacksize, unsigned int priority, int autostart)
{
	unsigned int (*syscall)( unsigned int (*)(void*), void*, void*, unsigned int, unsigned int, int ) = (unsigned int (*)( unsigned int (*)(void*), void*, void*, unsigned int, unsigned int, int))syscall_0;

	return syscall(entry, arg, stack, stacksize, priority, autostart);
}


void os_thread_set_priority(unsigned int priority)
{
	void (*syscall)(unsigned int) = (void (*)(unsigned int))syscall_9;

	syscall(priority);
}

unsigned int os_message_queue_create(void* ptr, unsigned int id)
{
	unsigned int (*syscall)(void*, unsigned int) = (unsigned int (*)(void*, unsigned int))syscall_a;

	return syscall(ptr, id);
}

unsigned int os_message_queue_receive(unsigned int queue, unsigned int* message, unsigned int flags)
{
	unsigned int (*syscall)(unsigned int, unsigned int*, unsigned int) = (unsigned int (*)(unsigned int, unsigned int*, unsigned int))syscall_e;

	return syscall(queue, message, flags);
}


unsigned int os_heap_create(void* ptr, unsigned int size)
{
	unsigned int (*syscall)(void*, unsigned int) = (unsigned int (*)(void*, unsigned int))syscall_16;
        while(((unsigned int)ptr)&31) //loosy attempt to align the heap..
        {
                ptr = (void*)((int)ptr+1);
                size--;
        }
        size &= ~31;
	return syscall(ptr, size);
}
unsigned int os_heap_destroy(unsigned int heap)
{
	unsigned int (*syscall)( unsigned int) = (unsigned int (*)(unsigned int))syscall_17;
        return syscall(heap);
}

void* os_heap_alloc(unsigned int heap, unsigned int size)
{
	void* (*syscall)(unsigned int, unsigned int) = (void* (*)(unsigned int, unsigned int))syscall_18;

	return syscall(heap, size);
}

void os_heap_free(unsigned int heap, void* ptr)
{
	void* (*syscall)(unsigned int, void*) = (void* (*)(unsigned int, void*))syscall_1a;

	if (ptr)
		syscall(heap, ptr);
}



unsigned int os_device_register(const char* devicename, unsigned int queuehandle)
{
	unsigned int (*syscall)(const char*, unsigned int) = (unsigned int (*)(const char*, unsigned int))syscall_1b;

	return syscall(devicename, queuehandle);
}


void os_message_queue_ack(void* message, int result)
{
	void (*syscall)(void*, int) = (void (*)(void*, int))syscall_2a;

	syscall(message, result);
}


void os_sync_before_read(void* ptr, unsigned int size)
{
	void (*syscall)(void*, unsigned int) = (void (*)(void*, unsigned int))syscall_3f;

	syscall(ptr, size);
}

void os_sync_after_write(void* ptr, unsigned int size)
{
	void (*syscall)(void*, unsigned int) = (void (*)(void*, unsigned int))syscall_40;

	syscall(ptr, size);
}

int os_open(char* device, int mode)
{
        int (*syscall)(char* , int ) = (int (*)(char* , int ) )syscall_1c;
	return syscall(device,mode);
}
int os_close(int fd)
{
        int (*syscall)(int ) = (int (*)(int ) )syscall_1d;
	return syscall(fd);
}


int os_ioctlv(int fd, int request, int bytes_in, int bytes_out, ioctlv *vector)
{
        int (*syscall)(int , int , int, int, ioctlv *) = (int (*)(int , int , int, int, ioctlv *)) syscall_22;
        return syscall(fd,request,bytes_in,bytes_out,vector);
}



void os_syscall_50(unsigned int unknown)
{
	void (*syscall)(unsigned int) = (void (*)(unsigned int))syscall_50;
	syscall(unknown);
}


void os_puts(char *str)
{
	void (*syscall)(char*) = (void (*)(char*))svc_4;
	syscall(str);
}
