/* SWI service from dev/mload (c) 2009 Hermes / www.elotrolado.net */

#ifndef IOS_SWI_MLOAD_H
#define IOS_SWI_MLOAD_H

#include "types.h"
#include "starlet.h"

/* NOTE: stack for SWI services is 0x900 bytes */
/* WARNING: Interrupt are disabled in SWI functions and you cannot call syscalls */


/***************************************************************************************************************************************************/
/*
	swi_mload_add_handler: add a new service handler for SWI 
	
	svc_code: for example 0xab for svc 0xab
	
	func: function for the new SWI service

	return: none

*/

void swi_mload_add_handler(u8 svc_code, int (*func) (u32 arg0, u32 arg1, u32 arg2, u32 arg3));


/***************************************************************************************************************************************************/
/*
	swi_mload_EHCI_data: gets the EHCI struct from dev/mload

	return: the struct pointer

*/

void * swi_mload_EHCI_data(void);


/***************************************************************************************************************************************************/
/*
	swi_mload_get_syscall_base: gets the syscall base address to call directly

	return: the address

*/

u32 swi_mload_get_syscall_base(void);


/***************************************************************************************************************************************************/
/*
	swi_mload_get_ios_base: gets the ios base (FFS, ES, IOSP) used to create this cIOS

	return: the IOS base version

*/

u32 swi_mload_get_ios_base(void);


/***************************************************************************************************************************************************/
/*
	swi_mload_memcpy: memcpy from supervisor mode
	
	dst: destination address
	
	src: source address

	len: datas to move

	return: none

*/

void swi_mload_memcpy(void * dst, void * src, int len);

/***************************************************************************************************************************************************/
/*
	swi_mload_memcpy_from_uncached: memcpy from supervisor mode. Copies from uncached source RAM to cached RAM
	
	dst: destination address
	
	src: source address uncached

	len: datas to move

	return: none

*/

void swi_mload_memcpy_from_uncached(void * dst, void * src, int len);


/***************************************************************************************************************************************************/
/*
	swi_mload_get_register: function thinking to read 32 bits registers from supervisor mode
	
	addr: register address

	return: value

*/

u32 swi_mload_get_register(u32 addr);


/***************************************************************************************************************************************************/
/*
	swi_mload_put_register: function thinking to write 32 bits registers from supervisor mode
	
	addr: register address

	val: new value for register

	return: none

*/

void swi_mload_put_register(u32 addr, u32 val);


/***************************************************************************************************************************************************/
/*
	swi_mload_set_register: function thinking to set bits to 1 (with OR) in 32 bits registers from supervisor mode
	
	addr: register address

	val: bits to set must be 1 (operation reg|=val)

	return: none

*/

void swi_mload_set_register(u32 addr, u32 val);


/***************************************************************************************************************************************************/
/*
	swi_mload_clr_register: function thinking to clear bits to 0 (with AND) in 32 bits registers from supervisor mode
	
	addr: register address

	val: bits to clear must be 1 (operation register &=~value) 

	return: none

*/

void swi_mload_clr_register(u32 addr, u32 val);


/***************************************************************************************************************************************************/
/*
	swi_mload_call_func: call to one function in Supervisor Mode 
	
	in: pointer to data in (received as argument 0 in the function)
	
	out: pointer to data out (received as argument 0 in the function)

	return: result from the function

*/

int swi_mload_call_func(int (*func) (void *in,  void *out), void *in, void *out);

/***************************************************************************************************************************************************/
/*
	swi_mload_led_on: frontal LED ON

	return: none

*/

void swi_mload_led_on(void);


/***************************************************************************************************************************************************/
/*
	swi_mload_led_off: frontal LED OFF

	return: none

*/

void swi_mload_led_off(void);


/***************************************************************************************************************************************************/
/*
	swi_mload_led_blink: frontal LED Blinking (note: it use a XOR function, so must call some times for blinking :P). It is thinking to see some activity...

	return: none

*/

void swi_mload_led_blink(void);

/***************************************************************************************************************************************************/
/*
	swi_mload_os_software_IRQ_func: function to call from os_software_IRQ(9) in system mode
	
	system_mode_func: function address

	return: none

*/

void swi_mload_os_software_IRQ9_func( int (*system_mode_func)(void));


/***************************************************************************************************************************************************/
/*
	swi_mload_log_func: control the log buffer for os_puts() 
	
	mode: 0-> return log buffer 1-> clear log buffer 2-> set a new log buffer

	buffer_log: new buffer log address (only with mode==2)

	maxsize_log: max size for log (only with mode==2)

	return: current buffer_log (by default 4KB) the string finish with '\0' code

*/

void * swi_mload_log_func(u32 mode, void *buffer_log, int maxsize_log);


/***************************************************************************************************************************************************/



#endif

