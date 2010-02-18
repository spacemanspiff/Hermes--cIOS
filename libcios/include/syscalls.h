#ifndef IOS_SYSCALLS_H
#define IOS_SYSCALLS_H

/* Data types */
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned long		u32;
typedef unsigned long long	u64;

typedef signed char		s8;
typedef signed short		s16;
typedef signed int		s32;
typedef signed long long	s64;

typedef struct _ioctlv
{
	void *data;
	unsigned long len;
} ioctlv;
typedef struct ipcmessage
{
	unsigned int command;			// 0
	unsigned int result;			// 4
        unsigned int fd;			// 8
	union 
	{
		struct
		{
			char *device;			// 12
			unsigned int mode;		// 16
			unsigned int resultfd;	// 20
		} open;
	
		struct 
		{
			void *data;
			unsigned int length;
		} read, write;
		
		struct 
		{
			int offset;
			int origin;
		} seek;
		
		struct 
		{
			unsigned int command;

			unsigned int *buffer_in;
			unsigned int length_in;
			unsigned int *buffer_io;
			unsigned int length_io;
		} ioctl;
		struct 
		{
			unsigned int command;

			unsigned int num_in;
			unsigned int num_io;
			ioctlv *vector;
		} ioctlv;
	};
} __attribute__((packed)) ipcmessage;

unsigned int os_thread_create( unsigned int (*entry)(void* arg), void* arg, void* stack, unsigned int stacksize, unsigned int priority, int autostart);
void os_thread_set_priority(unsigned int priority);
unsigned int os_message_queue_create(void* ptr, unsigned int id);
unsigned int os_message_queue_receive(unsigned int queue, unsigned int* message, unsigned int flags);
unsigned int os_heap_create(void* ptr, unsigned int size);
unsigned int os_heap_destroy(unsigned int heap);
void* os_heap_alloc(unsigned int heap, unsigned int size);
void os_heap_free(unsigned int heap, void* ptr);
unsigned int os_device_register(const char* devicename, unsigned int queuehandle);
void os_message_queue_ack(void* message, int result);
void os_sync_before_read(void* ptr, unsigned int size);
void os_sync_after_write(void* ptr, unsigned int size);
void os_syscall_50(unsigned int unknown);
void os_puts(char *str);

int os_open(char* device, int mode);
int os_close(int fd);
int os_ioctlv(int fd, int request, int bytes_in, int bytes_out, ioctlv *vector);


#ifdef DEBUG
void debug_printf(const char *fmt, ...);
void hexdump(void *d, int len);
#else
#define debug_printf(a...) do{}while(0)
#endif
#endif // IOS_SYSCALLS_H
