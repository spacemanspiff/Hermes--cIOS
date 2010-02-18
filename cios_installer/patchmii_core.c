/*  patchmii_core -- low-level functions to handle the downloading, patching
    and installation of updates on the Wii

    Copyright (C) 2008 bushing / hackmii.com
    Copyright (C) 2008 WiiGator
	Copyright (C) 2009 Hermes

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 2.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <gccore.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <network.h>
#include <sys/errno.h>
#include <fat.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

#include "patchmii_core.h"
#include "sha1.h"
#include "debug.h"
#include "http.h"
#include "haxx_certs.h"

#define  _DEBUG_PRINTF_H_ 1

#include "debug_printf.h"

/*
NOTE: i don�t necessary
#ifdef ADD_DIP_PLUGIN
#include "add_dip_plugin.h"
#include "patch_handle_di_cmd_raw.h"
#endif
*/

#define VERSION "0.2"

// These parameters will download IOS31, modify it, and install it as IOS222

#if IOS36
#define INPUT_TITLEID_H 1
#define INPUT_TITLEID_L 36
#define INPUT_VERSION 1042

#elif IOS38
#define INPUT_TITLEID_H 1
#define INPUT_TITLEID_L 38
#define INPUT_VERSION 3610
#else
#error "Hey! i need  IOS36 or IOS38 defined!"
#endif

#define OUTPUT_TITLEID_H 1
#ifdef ADD_DIP_PLUGIN
#ifdef DEBUG
#define OUTPUT_TITLEID_L 223
#else
#define OUTPUT_TITLEID_L 222
#endif
#else
#ifdef DEBUG
#define OUTPUT_TITLEID_L 203
#else
#define OUTPUT_TITLEID_L 202
#endif
#endif

#define OUTPUT_VERSION 1

#if 0
// to get modules
#define SAVE_DECRYPTED 1
#undef INPUT_TITLEID_H
#undef INPUT_TITLEID_L
#undef INPUT_VERSION
#define INPUT_TITLEID_H 1
#define INPUT_TITLEID_L 60
#define INPUT_VERSION 6174
#endif

#define ALIGN(a,b) ((((a)+(b)-1)/(b))*(b))
#define round_up(x,n) (-(-(x) & -(n)))

int http_status = 0;
int useSd = 1;
int tmd_dirty = 0, tik_dirty = 0;



#ifdef IOS36
u32 DIP_patch1_pos=0x6800;
u32 DIP_DVD_enable_orig_pos1=0x964;
u32 DIP_DVD_enable_orig_pos2=0x9F0;

u32 DIP_handle_di_cmd=0x112c;

unsigned char patch_handle_di_cmd[12] = {
	0x4B, 0x01, 0x68, 0x1B, 0x47, 0x18, 0x00, 0x00,/*addr to get handle_di_cmd*/ 0x20, 0x20, 0x90, 0x40 
};

u32 DIP_handle_di_cmd_reentry=0x8248;

// handle_di_cmd_reentry= 0x20209030 (default)
u8 handle_di_cmd_reentry[24] = {
	0x20, 0x20, 0x90, 0x44+1,
	0xB5, 0xF0, 0x46, 0x5F, 0x46, 0x56, 0x46, 0x4D, 0x46, 0x44, 0xB4, 0xF0, 0x4B, 0x00, 0x47, 0x18, 
	/* handle_di_cmd_reentry */ 0x20, 0x20, 0x10, 0x10+1 // (Thumb)
};


// patch sub_201000CC (ES_ioctvl)
#ifdef ADD_ES_PLUGIN

u32 ES_ioctvl_patch_pos=0x12ab0;

u8 ES_patch_ioctvl[8] = {
	0x49, 0x00, 0x47, 0x08, /* addr in mload.elf */ 0x13, 0x8c, 0x00, 0x10+1 // (Thumb)
};

	//138C0010
#endif

#endif

#ifdef IOS38
u32 DIP_patch1_pos=0x6494;
u32 DIP_DVD_enable_orig_pos1=0x68c;
u32 DIP_DVD_enable_orig_pos2= 0x718;
u32 DIP_handle_di_cmd= 0xe54;

unsigned char patch_handle_di_cmd[12] = {
	0x4B, 0x01, 0x68, 0x1B, 0x47, 0x18, 0x00, 0x00,/*addr to get handle_di_cmd*/ 0x20, 0x20, 0x80, 0x30
};

u32 DIP_handle_di_cmd_reentry=0x7ecc;

// handle_di_cmd_reentry= 0x20208030 (default)
u8 handle_di_cmd_reentry[24] = {
	0x20, 0x20, 0x80, 0x34+1,
	0xB5, 0xF0, 0x46, 0x5F, 0x46, 0x56, 0x46, 0x4D, 0x46, 0x44, 0xB4, 0xF0, 0x4B, 0x00, 0x47, 0x18, 
	/* handle_di_cmd_reentry */  0x20, 0x20, 0x0D, 0x38+1 // (Thumb)
};

// patch sub_201000CC (ES_ioctvl)
#ifdef ADD_ES_PLUGIN

u32 ES_ioctvl_patch_pos=0x12ab0 ;// 0x12b3c; NOTE: remember you that i am using ES from IOS36...
u8 ES_patch_ioctvl[8] = {
	0x49, 0x00, 0x47, 0x08,  /* addr in mload.elf */ 0x13, 0x8c, 0x00, 0x10+1 // (Thumb)
};
#endif

#endif

u8 DIP_orig1[] =  { 0x00, 0x01, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
u8 DIP_patch1[] = { 0x7e, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

u8 ES_orig1[] =  { 0x99, 0x02, 0x22, 0x14, 0x4b, 0x0f, 0x47, 0x98, 0x28, 0x00, 0xd0, 0x00, 0x20, 0x07, 0x23, 0xa2 };
u8 ES_patch1[] = { 0x99, 0x02, 0x22, 0x14, 0x4b, 0x0f, 0x47, 0x98, 0x28, 0x00, 0xe0, 0x00, 0x20, 0x07, 0x23, 0xa2 };
u8 ES_orig2[] =  { 0x88, 0x13, 0x42, 0x99, 0xd2, 0x01, 0x4e, 0x56, 0xe0, 0x87, 0x21, 0xc6, 0x00, 0x49, 0x18, 0x6b };
u8 ES_patch2[] = { 0x88, 0x13, 0x42, 0x99, 0xe0, 0x01, 0x4e, 0x56, 0xe0, 0x87, 0x21, 0xc6, 0x00, 0x49, 0x18, 0x6b };
u8 ES_orig3[] =  { 0x42, 0xa3, 0xd1, 0x2a, 0x1c, 0x39, 0x1c, 0x30, 0x46, 0x42, 0xf0, 0x03, 0xf8, 0x45, 0x1e, 0x05 };
u8 ES_patch3[] = { 0x42, 0xa3, 0x46, 0xc0, 0x1c, 0x39, 0x1c, 0x30, 0x46, 0x42, 0xf0, 0x03, 0xf8, 0x45, 0x1e, 0x05 };
u8 ES_orig4[] =  { 0x42, 0x99, 0xd8, 0x00, 0x4a, 0x04, 0x1c, 0x10, 0xbc, 0x10, 0xbc, 0x02, 0x47, 0x08, 0x00, 0x00 };
u8 ES_patch4[] = { 0x42, 0x99, 0xe0, 0x00, 0x4a, 0x04, 0x1c, 0x10, 0xbc, 0x10, 0xbc, 0x02, 0x47, 0x08, 0x00, 0x00 };

u32 ES_patch1_pos=0x5820;
u32 ES_patch2_pos=0x150f0;
u32 ES_patch3_pos=0x17940;
u32 ES_patch4_pos=0x19fd0;

u8 ES_ioctlv_orig[12] = {0xB5, 0x70, 0xB0, 0x88, 0x68, 0x85, 0x1C, 0x01, 0x31, 0x0C, 0x22, 0xC0};

/*
IOS 38
u32 ES_patch1_pos=NO GOOD;
u32 ES_patch2_pos=0x1518c;
u32 ES_patch3_pos=0x179dc;
u32 ES_patch4_pos=0x1a06c;

I use ES from IOS 36
*/

u8 DIP_DVD_enable_orig[] = { 0x20, 0x01 };
u8 DIP_DVD_enable_patch[] = { 0x20, 0x00 };
u8 DIP_handle_di_cmd_orig[] = { 0xb5, 0xf0, 0x46, 0x5f, 0x46, 0x56, 0x46, 0x4d, 0x46, 0x44, 0xb4, 0xf0 };

static int patchmii(void);

int replace_ios_modules(u8 **decrypted_buf,  u32 *content_size);
int add_custom_modules(tmd *p_tmd);


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

char ascii(char s) {
  if(s < 0x20) return '.';
  if(s > 0x7E) return '.';
  return s;
}

void hexdump(void *d, int len) {
  u8 *data;
  int i, off;
  data = (u8*)d;
  for (off=0; off<len; off += 16) {
    debug_printf("%08x  ",off);
    for(i=0; i<16; i++)
      if((i+off)>=len) debug_printf("   ");
      else debug_printf("%02x ",data[off+i]);

    debug_printf(" ");
    for(i=0; i<16; i++)
      if((i+off)>=len) debug_printf(" ");
      else debug_printf("%c",ascii(data[off+i]));
    debug_printf("\n");
  }
}

char *spinner_chars="/-\\|";
int spin = 0;

void spinner(void) {
  printf("\b%c", spinner_chars[spin++]);
  if(!spinner_chars[spin]) spin=0;
}

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

void printvers(void) {
  debug_printf("IOS Version: %08x\n", *((u32*)0xC0003140));
}

void console_setup(void) {
  VIDEO_Init();
  PAD_Init();
  WPAD_Init();
  
  rmode = VIDEO_GetPreferredMode(NULL);

  xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
  VIDEO_ClearFrameBuffer(rmode,xfb,COLOR_BLACK);
  VIDEO_Configure(rmode);
  VIDEO_SetNextFramebuffer(xfb);
  VIDEO_SetBlack(FALSE);
  VIDEO_Flush();
  VIDEO_WaitVSync();
  if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();
  CON_InitEx(rmode,20,30,rmode->fbWidth - 40,rmode->xfbHeight - 60);
}

static char buf[128];

int get_nus_object(u32 titleid1, u32 titleid2, u32 version, char *content, u8 **outbuf, u32 *outlen) {
  
  int retval;
  u32 http_status;
  static int netInit = 0;

FILE *fd;
	
  if (useSd) {

	snprintf(buf, 128, "fat0:/%08x/%08x/v%d/%s", titleid1, titleid2, version, content);


	fd = fopen(buf, "rb");
	if (!fd) {
		debug_printf("from Internet: ");
	} else {
		debug_printf("from SD: ");
		fseek(fd, 0, SEEK_END);
		*outlen = ftell(fd);
		fseek(fd, 0, SEEK_SET);

		*outbuf = malloc(*outlen);
		if (*outbuf == NULL) {
			debug_printf("Out of memory size %d\n", *outlen);
			return 2;
		}

		if (fread(*outbuf, *outlen, 1, fd) != 1) {
			fclose(fd);
			return 3;
		} else {
			fclose(fd);
			return 0;
		}
	}
  }
  if (!netInit)
  {
  	printf("Initializing network."); fflush(stdout);
  	while (1) {
  		retval = net_init ();
 		if (retval < 0) {
			if (retval != -EAGAIN) {
				debug_printf ("net_init failed: %d\n", retval);
				return 4;
			}
    	}
		if (!retval) break;
		usleep(100000);
		printf("."); fflush(stdout);
  	}
    sleep(1);
  	printf("Done!\n");
    netInit = 1;
  }
  snprintf(buf, 128, "http://nus.cdn.shop.wii.com/ccs/download/%08x%08x/%s",
	   titleid1, titleid2, content);

  debug_printf("\nwget -O sd/%08x/%08x/v%d/%s %s\n", titleid1, titleid2, version, content,buf);

	{int retry=10;
	while(1)
		{
		  retval = http_request(buf, (u32) (1 << 31));
		  if (!retval) {
			  retry--;
			debug_printf("Error making http request\n");
			sleep(1);
			if(retry<0) return 1;
		  }
		else break;
		}
	}
  retval = http_get_result(&http_status, outbuf, outlen);
	snprintf(buf, 128, "fat0:/%08x/%08x/v%d/%s", titleid1, titleid2, version, content);	

	if (useSd)
	{
	fd = fopen(buf, "wb");
	if (fd) {
			fwrite(*outbuf, *outlen, 1, fd);
			fclose(fd);
			}
	}

  if (((int)*outbuf & 0xF0000000) == 0xF0000000) {

	
	return (int) *outbuf;
  }

  return 0;
}

void decrypt_buffer(u16 index, u8 *source, u8 *dest, u32 len) {
  static u8 iv[16];
  if (!source) {
	debug_printf("decrypt_buffer: invalid source paramater\n");
	exit(1);
  }
  if (!dest) {
	debug_printf("decrypt_buffer: invalid dest paramater\n");
	exit(1);
  }

  memset(iv, 0, 16);
  memcpy(iv, &index, 2);
  aes_decrypt(iv, source, dest, len);
}

static u8 encrypt_iv[16];
void set_encrypt_iv(u16 index) {
  memset(encrypt_iv, 0, 16);
  memcpy(encrypt_iv, &index, 2);
}
  
void encrypt_buffer(u8 *source, u8 *dest, u32 len) {
  aes_encrypt(encrypt_iv, source, dest, len);
}

int create_temp_dir(void) {
  int retval;
  retval = ISFS_CreateDir ("/tmp/patchmii", 0, 3, 1, 1);

  if (retval) debug_printf("ISFS_CreateDir(/tmp/patchmii) returned %d\n", retval);
  return retval;
}

u32 save_nus_object (u16 index, u8 *buf, u32 size) {
  char filename[256];
  static u8 bounce_buf[1024] ATTRIBUTE_ALIGN(0x20);
  u32 i;

  int retval, fd;
  snprintf(filename, sizeof(filename), "/tmp/patchmii/%08x", index);
  
  retval = ISFS_CreateFile (filename, 0, 3, 1, 1);

  if (retval != ISFS_OK) {
    debug_printf("ISFS_CreateFile(%s) returned %d\n", filename, retval);
    return retval;
  }
  
  fd = ISFS_Open (filename, ISFS_ACCESS_WRITE);

  if (fd < 0) {
    debug_printf("ISFS_OpenFile(%s) returned %d\n", filename, fd);
    return retval;
  }

  for (i=0; i<size;) {
    u32 numbytes = ((size-i) < 1024)?size-i:1024;
    memcpy(bounce_buf, buf+i, numbytes);
    retval = ISFS_Write(fd, bounce_buf, numbytes);
    if (retval < 0) {
      debug_printf("ISFS_Write(%d, %p, %d) returned %d at offset %d\n", 
		   fd, bounce_buf, numbytes, retval, i);
      ISFS_Close(fd);
      return retval;
    }
    i += retval;
  }
  ISFS_Close(fd);
  return size;
}

s32 install_nus_object (tmd *p_tmd, u16 index) {
  char filename[256];
  static u8 bounce_buf1[1024] ATTRIBUTE_ALIGN(0x20);
  static u8 bounce_buf2[1024] ATTRIBUTE_ALIGN(0x20);
  u32 i;
  const tmd_content *p_cr = TMD_CONTENTS(p_tmd);
  int rindex = p_cr[index].index;
  //  debug_printf("install_nus_object(%p, %lu)", p_tmd, rindex);
  
  int retval, fd, cfd, ret;
  snprintf(filename, sizeof(filename), "/tmp/patchmii/%08x", p_cr[index].cid);
  
  fd = ISFS_Open (filename, ISFS_ACCESS_READ);
  
  if (fd < 0) {
    debug_printf("ISFS_OpenFile(%s) returned %d\n", filename, fd);
    return fd;
  }
  set_encrypt_iv(rindex);
  //  debug_printf("ES_AddContentStart(%016llx, %x)\n", p_tmd->title_id, rindex);

  cfd = ES_AddContentStart(p_tmd->title_id, p_cr[index].cid);
  if(cfd < 0) {
    debug_printf(":\nES_AddContentStart(%016llx, %x) failed: %d\n",p_tmd->title_id, index, cfd);
    ES_AddTitleCancel();
    return -1;
  }
  debug_printf(" (cfd %d): ",cfd);
  for (i=0; i<p_cr[index].size;) {
    u32 numbytes = ((p_cr[index].size-i) < 1024)?p_cr[index].size-i:1024;
    numbytes = ALIGN(numbytes, 32);
    retval = ISFS_Read(fd, bounce_buf1, numbytes);
    if (retval < 0) {
      debug_printf("ISFS_Read(%d, %p, %d) returned %d at offset %d\n", 
		   fd, bounce_buf1, numbytes, retval, i);
      ES_AddContentFinish(cfd);
      ES_AddTitleCancel();
      ISFS_Close(fd);
      return retval;
    }
    
    encrypt_buffer(bounce_buf1, bounce_buf2, sizeof(bounce_buf1));
    ret = ES_AddContentData(cfd, bounce_buf2, retval);
    if (ret < 0) {
      debug_printf("ES_AddContentData(%d, %p, %d) returned %d\n", cfd, bounce_buf2, retval, ret);
      ES_AddContentFinish(cfd);
      ES_AddTitleCancel();
      ISFS_Close(fd);
      return ret;
    }
    i += retval;
  }

  debug_printf("  done! (0x%x bytes)\n",i);
  ret = ES_AddContentFinish(cfd);
  if(ret < 0) {
    printf("ES_AddContentFinish failed: %d\n",ret);
    ES_AddTitleCancel();
    ISFS_Close(fd);
    return -1;
  }
  
  ISFS_Close(fd);
  
  return 0;
}

int get_title_key(signed_blob *s_tik, u8 *key) {
  static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
  static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
  static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);
  int retval;

  const tik *p_tik;
  p_tik = (tik*)SIGNATURE_PAYLOAD(s_tik);
  u8 *enc_key = (u8 *)&p_tik->cipher_title_key;
  memcpy(keyin, enc_key, sizeof keyin);
  memset(keyout, 0, sizeof keyout);
  memset(iv, 0, sizeof iv);
  memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);
  
  retval = ES_Decrypt(ES_KEY_COMMON, iv, keyin, sizeof keyin, keyout);
  if (retval) debug_printf("ES_Decrypt returned %d\n", retval);
  memcpy(key, keyout, sizeof keyout);
  return retval;
}

int change_ticket_title_id(signed_blob *s_tik, u32 titleid1, u32 titleid2) {
	static u8 iv[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyin[16] ATTRIBUTE_ALIGN(0x20);
	static u8 keyout[16] ATTRIBUTE_ALIGN(0x20);
	int retval;

	tik *p_tik;
	p_tik = (tik*)SIGNATURE_PAYLOAD(s_tik);
	u8 *enc_key = (u8 *)&p_tik->cipher_title_key;
	memcpy(keyin, enc_key, sizeof keyin);
	memset(keyout, 0, sizeof keyout);
	memset(iv, 0, sizeof iv);
	memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);

	retval = ES_Decrypt(ES_KEY_COMMON, iv, keyin, sizeof keyin, keyout);
	p_tik->titleid = (u64)titleid1 << 32 | (u64)titleid2;
	memset(iv, 0, sizeof iv);
	memcpy(iv, &p_tik->titleid, sizeof p_tik->titleid);
	
	retval = ES_Encrypt(ES_KEY_COMMON, iv, keyout, sizeof keyout, keyin);
    if (retval) debug_printf("ES_Decrypt returned %d\n", retval);
	memcpy(enc_key, keyin, sizeof keyin);
	tik_dirty = 1;

    return retval;
}

void change_tmd_title_id(signed_blob *s_tmd, u32 titleid1, u32 titleid2) {
	tmd *p_tmd;
	u64 title_id = titleid1;
	title_id <<= 32;
	title_id |= titleid2;
	p_tmd = (tmd*)SIGNATURE_PAYLOAD(s_tmd);
	p_tmd->title_id = title_id;
	tmd_dirty = 1;
}

void display_tag(u8 *buf) {
  debug_printf("Firmware version: %s      Builder: %s\n",
	       buf, buf+0x30);
}

void display_ios_tags(u8 *buf, u32 size) {
  u32 i;
  char *ios_version_tag = "$IOSVersion:";

  if (size == 64) {
    display_tag(buf);
    return;
  }

  for (i=0; i<(size-64); i++) {
    if (!strncmp((char *)buf+i, ios_version_tag, 10)) {
      char version_buf[128], *date;
      while (buf[i+strlen(ios_version_tag)] == ' ') i++; // skip spaces
      strlcpy(version_buf, (char *)buf + i + strlen(ios_version_tag), sizeof version_buf);
      date = version_buf;
      strsep(&date, "$");
      date = version_buf;
      strsep(&date, ":");
      debug_printf("%s (%s)\n", version_buf, date);
      i += 64;
    }
  }
}

void print_tmd_summary(const tmd *p_tmd) {
  const tmd_content *p_cr;
  p_cr = TMD_CONTENTS(p_tmd);

  u32 size=0;

  u16 i=0;
  for(i=0;i<p_tmd->num_contents;i++) {
    size += p_cr[i].size;
  }

  debug_printf("Title ID: %016llx\n",p_tmd->title_id);
  debug_printf("Number of parts: %d.  Total size: %uK\n", p_tmd->num_contents, (u32) (size / 1024));
}

void zero_sig(signed_blob *sig) {
  u8 *sig_ptr = (u8 *)sig;
  memset(sig_ptr + 4, 0, SIGNATURE_SIZE(sig)-4);
}

void brute_tmd(tmd *p_tmd) {
  u16 fill;
  for(fill=0; fill<65535; fill++) {
    p_tmd->fill3=fill;
    sha1 hash;
    //    debug_printf("SHA1(%p, %x, %p)\n", p_tmd, TMD_SIZE(p_tmd), hash);
    SHA1((u8 *)p_tmd, TMD_SIZE(p_tmd), hash);;
  
    if (hash[0]==0) {
      //      debug_printf("setting fill3 to %04hx\n", fill);
      return;
    }
  }
  printf("Unable to fix tmd :(\n");
  exit(4);
}

void brute_tik(tik *p_tik) {
  u16 fill;
  for(fill=0; fill<65535; fill++) {
    p_tik->padding=fill;
    sha1 hash;
    //    debug_printf("SHA1(%p, %x, %p)\n", p_tmd, TMD_SIZE(p_tmd), hash);
    SHA1((u8 *)p_tik, sizeof(tik), hash);
  
    if (hash[0]==0) return;
  }
  printf("Unable to fix tik :(\n");
  exit(5);
}
    
void forge_tmd(signed_blob *s_tmd) {
  debug_printf("forging tmd sig\n");
  zero_sig(s_tmd);
  brute_tmd(SIGNATURE_PAYLOAD(s_tmd));
}

void forge_tik(signed_blob *s_tik) {
  debug_printf("forging tik sig\n");
  zero_sig(s_tik);
  brute_tik(SIGNATURE_PAYLOAD(s_tik));
}

s32 install_ticket(const signed_blob *s_tik, const signed_blob *s_certs, u32 certs_len) {
  u32 ret;

  debug_printf("Installing ticket...\n");
  ret = ES_AddTicket(s_tik,STD_SIGNED_TIK_SIZE,s_certs,certs_len, NULL, 0);
  if (ret < 0) {
      debug_printf("ES_AddTicket failed: %d\n",ret);
      return ret;
  }
  return 0;
}

s32 install(const signed_blob *s_tmd, const signed_blob *s_certs, u32 certs_len) {
  u32 ret, i;
  tmd *p_tmd = SIGNATURE_PAYLOAD(s_tmd);
  debug_printf("Adding title...\n");

  ret = ES_AddTitleStart(s_tmd, SIGNED_TMD_SIZE(s_tmd), s_certs, certs_len, NULL, 0);

  if(ret < 0) {
    debug_printf("ES_AddTitleStart failed: %d\n",ret);
    ES_AddTitleCancel();
    return ret;
  }

  for(i=0; i<p_tmd->num_contents; i++) {
    debug_printf("Adding content ID %08x", i);
    ret = install_nus_object((tmd *)SIGNATURE_PAYLOAD(s_tmd), i);
    if (ret) return ret;
  }

  ret = ES_AddTitleFinish();
  if(ret < 0) {
    printf("ES_AddTitleFinish failed: %d\n",ret);
    ES_AddTitleCancel();
    return ret;
  }

  printf("Installation complete!\n");
  return 0;

}


void fun_exit()
{
sleep(5);
}
int main(int argc, char **argv) {
	int rv;
	s32 pressed;
	
	atexit(fun_exit);
	console_setup();
	printf("This program is a modification of patchmii, and is unsupported and not condoned by the original authors of it.\n");
	printf("The backup loader modification is solely the work of WiiGator.\n");
	printf("This version includes optimizations made by Waninkoko and Hermes\n");
        printf("USB2/wbfs support by Kwiirk\n");
	printf("\n");
	printf("cIOS installer v%d by WiiGator.\n", OUTPUT_VERSION);
	printf("If you get an error, you need to downgrade your Wii first.\n");
	printf("\n");
	printf("USE ON YOUR OWN RISK!\n");
	printf("\n");
	printf("Press A to continue (TAKE THE RISK).\n");
	printf("Switch off if you have any fear.\n");
#if 1
	while(1)
	{
		WPAD_ScanPads();
		pressed = WPAD_ButtonsDown(0);

		if(pressed) {
			if (pressed == WPAD_BUTTON_A) {
				break;
			} else {
				printf("Wrong button pressed, exiting...\n");
				return 0;
			}
		}
		VIDEO_WaitVSync();
	}
#endif
	if (fatInitDefault()) {
		chdir ("fat0:/");
	}
	else useSd=0;

	rv = patchmii();

	//fatUnmount(PI_DEFAULT);

	return rv;
}

int apply_patch(u8 *data, u32 offset, u8 *orig, u32 orig_size, u8 *patch, u32 patch_size)
{
	if (memcmp(&data[offset], orig, orig_size) == 0) {
		memcpy(&data[offset], patch, patch_size);
		return -1;
	} else {
		return 0;
	}
}

#define INPUT2_TITLEID_H 1
#define INPUT2_TITLEID_L 36
#define INPUT2_VERSION 1042

u8 *ES_decrypted_buf=NULL;
u32 ES_content_size=0;

// to get ES title from IOS 36
static int patchmii2(void)
{


// ******* WARNING *******
// Obviously, if you're reading this, you're obviously capable of disabling the
// following checks.  If you put any of the following titles into an unusuable state, 
// your Wii will fail to boot:
//
// 1-1 (BOOT2), 1-2 (System Menu), 1-30 (IOS30, currently specified by 1-2's TMD)
// Corrupting other titles (for example, BC or the banners of installed channels)
// may also cause difficulty booting.  Please do not remove these safety checks
// unless you have performed extensive testing and are willing to take on the risk
// of bricking the systems of people to whom you give this code.  -bushing

	if ((OUTPUT_TITLEID_H == 1) && (OUTPUT_TITLEID_L == 2)) {
		printf("Sorry, I won't modify the system menu; too dangerous. :(\n");
		while(1);
  	}

	if ((OUTPUT_TITLEID_H == 1) && (OUTPUT_TITLEID_L == 30)) {
		printf("Sorry, I won't modify IOS30; too dangerous. :(\n");
		while(1);
  	}

	printvers();
  
	int retval;


  	signed_blob *s_tmd = NULL, *s_tik = NULL, *s_certs = NULL;

  	u8 *temp_tmdbuf = NULL, *temp_tikbuf = NULL;

  	static u8 tmdbuf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(0x20);
  	static u8 tikbuf[STD_SIGNED_TIK_SIZE] ATTRIBUTE_ALIGN(0x20);
  
  	u32 tmdsize;

	static char tmdname[32];

	if (useSd) 
		{
		snprintf(buf, 128, "fat0:/%08x", INPUT2_TITLEID_H);
		mkdir(buf,S_IREAD | S_IWRITE);
		snprintf(buf, 128, "fat0:/%08x/%08x", INPUT2_TITLEID_H, INPUT2_TITLEID_L);
		mkdir(buf,S_IREAD | S_IWRITE);
		snprintf(buf, 128, "fat0:/%08x/%08x/v%d", INPUT2_TITLEID_H, INPUT2_TITLEID_L, INPUT2_VERSION);
		mkdir(buf,S_IREAD | S_IWRITE);
		}

  	debug_printf("Downloading IOS%d metadata: ..", INPUT2_TITLEID_L);
	snprintf(tmdname, sizeof(tmdname),"tmd.%d", INPUT2_VERSION);
  	retval = get_nus_object(INPUT2_TITLEID_H, INPUT2_TITLEID_L, INPUT2_VERSION, tmdname, &temp_tmdbuf, &tmdsize);
  	if (retval<0) {
		debug_printf("get_nus_object(tmd) returned %d, tmdsize = %u\n", retval, tmdsize);
		return(1);
	}
	if (temp_tmdbuf == NULL) {
		debug_printf("Failed to allocate temp buffer for encrypted content, size was %u\n", tmdsize);
		return(1);
	}
  	memcpy(tmdbuf, temp_tmdbuf, MIN(tmdsize, sizeof(tmdbuf)));
	free(temp_tmdbuf);

	s_tmd = (signed_blob *)tmdbuf;
	if(!IS_VALID_SIGNATURE(s_tmd)) {
    	debug_printf("Bad TMD signature!\n");
		return(1);
  	}

  	debug_printf("\b ..tmd..");

	u32 ticketsize;
	retval = get_nus_object(INPUT2_TITLEID_H, INPUT2_TITLEID_L, INPUT2_VERSION,
						  "cetk", &temp_tikbuf, &ticketsize);
						
	if (retval < 0) debug_printf("get_nus_object(cetk) returned %d, ticketsize = %u\n", retval, ticketsize);
	memcpy(tikbuf, temp_tikbuf, MIN(ticketsize, sizeof(tikbuf)));
  
	s_tik = (signed_blob *)tikbuf;
	if(!IS_VALID_SIGNATURE(s_tik)) {
    	debug_printf("Bad tik signature!\n");
		return(1);
  	}
  
  	free(temp_tikbuf);

	s_certs = (signed_blob *)haxx_certs;
	if(!IS_VALID_SIGNATURE(s_certs)) {
    	debug_printf("Bad cert signature!\n");
		return(1);
  	}

	debug_printf("\b ..ticket..");

	u8 key[16];
	get_title_key(s_tik, key);
	aes_set_key(key);

	tmd *p_tmd;
	tmd_content *p_cr;
	p_tmd = (tmd*)SIGNATURE_PAYLOAD(s_tmd);
	p_cr = TMD_CONTENTS(p_tmd);

	if (p_tmd->title_version != INPUT2_VERSION) {
		printf("TMD Version wrong %d != %d.\n", p_tmd->title_version, INPUT2_VERSION);
		return 1;
	}
	/* Patch version number. */
	p_tmd->title_version = OUTPUT_VERSION;
        
	print_tmd_summary(p_tmd);

	debug_printf("Downloading contents: \n");
	static char cidstr[32];
	u16 i;
	for (i=0xe;i<0xf;i++) {
	   debug_printf("Downloading part %d/%d (%uK): ", i+1, 
					p_tmd->num_contents, p_cr[i].size / 1024);
	   sprintf(cidstr, "%08x", p_cr[i].cid);
   
	   u8 *content_buf, *decrypted_buf;
	   u32 content_size;

	   retval = get_nus_object(INPUT2_TITLEID_H, INPUT2_TITLEID_L, INPUT2_VERSION, cidstr, &content_buf, &content_size);
	   if (retval < 0) {
			debug_printf("get_nus_object(%s) failed with error %d, content size = %u\n", 
					cidstr, retval, content_size);
			return(1);
		}

		if (content_buf == NULL) {
			debug_printf("error allocating content buffer, size was %u\n", content_size);
			return(1);
		}

		if (content_size % 16) {
			debug_printf("ERROR: downloaded content[%hu] size %u is not a multiple of 16\n",
					i, content_size);
			free(content_buf);
			return(1);
		}

   		if (content_size < p_cr[i].size) {
			debug_printf("ERROR: only downloaded %u / %llu bytes\n", content_size, p_cr[i].size);
			free(content_buf);
			return(1);
   		} 

		decrypted_buf = malloc(content_size);
		if (!decrypted_buf) {
			debug_printf("ERROR: failed to allocate decrypted_buf (%u bytes)\n", content_size);
			free(content_buf);
			return(1);
		}

		decrypt_buffer(i, content_buf, decrypted_buf, content_size);

		sha1 hash;
		SHA1(decrypted_buf, p_cr[i].size, hash);

		if (!memcmp(p_cr[i].hash, hash, sizeof hash)) {
                  debug_printf("\b hash OK.\n");
			//display_ios_tags(decrypted_buf, content_size);

			
		
			}

	ES_decrypted_buf=decrypted_buf;
    ES_content_size=content_size;

	   	free(content_buf);
	}

  	debug_printf("Done \n");

	return(0);
}

static int patchmii(void)
{
#if SAVE_DECRYPTED
char name[256];
FILE *fd;
#endif
// ******* WARNING *******
// Obviously, if you're reading this, you're obviously capable of disabling the
// following checks.  If you put any of the following titles into an unusuable state, 
// your Wii will fail to boot:
//
// 1-1 (BOOT2), 1-2 (System Menu), 1-30 (IOS30, currently specified by 1-2's TMD)
// Corrupting other titles (for example, BC or the banners of installed channels)
// may also cause difficulty booting.  Please do not remove these safety checks
// unless you have performed extensive testing and are willing to take on the risk
// of bricking the systems of people to whom you give this code.  -bushing

	if ((OUTPUT_TITLEID_H == 1) && (OUTPUT_TITLEID_L == 2)) {
		printf("Sorry, I won't modify the system menu; too dangerous. :(\n");
		while(1);
  	}

	if ((OUTPUT_TITLEID_H == 1) && (OUTPUT_TITLEID_L == 30)) {
		printf("Sorry, I won't modify IOS30; too dangerous. :(\n");
		while(1);
  	}


#ifdef IOS38
// to get dev/es from IOS36 (Yes, to use with IOS38)
if(patchmii2() !=0 || ES_decrypted_buf==NULL)
	{
	perror("Failed to adquire IOS36 file ");
		return(1);
	}
#endif
	printvers();
 
	int retval;

	if (ISFS_Initialize() || create_temp_dir()) {
		perror("Failed to create temp dir: ");
		return(1);
	}

  	signed_blob *s_tmd = NULL, *s_tik = NULL, *s_certs = NULL;

  	u8 *temp_tmdbuf = NULL, *temp_tikbuf = NULL;

  	static u8 tmdbuf[MAX_SIGNED_TMD_SIZE] ATTRIBUTE_ALIGN(0x20);
  	static u8 tikbuf[STD_SIGNED_TIK_SIZE] ATTRIBUTE_ALIGN(0x20);
  
  	u32 tmdsize;
	int update_tmd;
	static char tmdname[32];

	if (useSd) 
		{
		snprintf(buf, 128, "fat0:/%08x", INPUT_TITLEID_H);
		mkdir(buf,S_IREAD | S_IWRITE);
		snprintf(buf, 128, "fat0:/%08x/%08x", INPUT_TITLEID_H, INPUT_TITLEID_L);
		mkdir(buf,S_IREAD | S_IWRITE);
		snprintf(buf, 128, "fat0:/%08x/%08x/v%d", INPUT_TITLEID_H, INPUT_TITLEID_L, INPUT_VERSION);
		mkdir(buf,S_IREAD | S_IWRITE);
		}

  	debug_printf("Downloading IOS%d metadata: ..", INPUT_TITLEID_L);
	snprintf(tmdname, sizeof(tmdname),"tmd.%d", INPUT_VERSION);
  	retval = get_nus_object(INPUT_TITLEID_H, INPUT_TITLEID_L, INPUT_VERSION, tmdname, &temp_tmdbuf, &tmdsize);
  	if (retval<0) {
		debug_printf("get_nus_object(tmd) returned %d, tmdsize = %u\n", retval, tmdsize);
		return(1);
	}
	if (temp_tmdbuf == NULL) {
		debug_printf("Failed to allocate temp buffer for encrypted content, size was %u\n", tmdsize);
		return(1);
	}
  	memcpy(tmdbuf, temp_tmdbuf, MIN(tmdsize, sizeof(tmdbuf)));
	free(temp_tmdbuf);

	s_tmd = (signed_blob *)tmdbuf;
	if(!IS_VALID_SIGNATURE(s_tmd)) {
    	debug_printf("Bad TMD signature!\n");
		return(1);
  	}

  	debug_printf("\b ..tmd..");

	u32 ticketsize;
	retval = get_nus_object(INPUT_TITLEID_H, INPUT_TITLEID_L, INPUT_VERSION,
						  "cetk", &temp_tikbuf, &ticketsize);
						
	if (retval < 0) debug_printf("get_nus_object(cetk) returned %d, ticketsize = %u\n", retval, ticketsize);
	memcpy(tikbuf, temp_tikbuf, MIN(ticketsize, sizeof(tikbuf)));
  
	s_tik = (signed_blob *)tikbuf;
	if(!IS_VALID_SIGNATURE(s_tik)) {
    	debug_printf("Bad tik signature!\n");
		return(1);
  	}
  
  	free(temp_tikbuf);

	s_certs = (signed_blob *)haxx_certs;
	if(!IS_VALID_SIGNATURE(s_certs)) {
    	debug_printf("Bad cert signature!\n");
		return(1);
  	}

	debug_printf("\b ..ticket..");

	u8 key[16];
	get_title_key(s_tik, key);
	aes_set_key(key);

	tmd *p_tmd;
	tmd_content *p_cr;
	p_tmd = (tmd*)SIGNATURE_PAYLOAD(s_tmd);
	p_cr = TMD_CONTENTS(p_tmd);

	if (p_tmd->title_version != INPUT_VERSION) {
		printf("TMD Version wrong %d != %d.\n", p_tmd->title_version, INPUT_VERSION);
		return 1;
	}
	/* Patch version number. */
	p_tmd->title_version = OUTPUT_VERSION;
        
	print_tmd_summary(p_tmd);

	debug_printf("Downloading contents: \n");
	static char cidstr[32];
	u16 i;
	for (i=0;i<p_tmd->num_contents;i++) {
	   debug_printf("Downloading part %d/%d (%uK): ", i+1, 
					p_tmd->num_contents, p_cr[i].size / 1024);
	   sprintf(cidstr, "%08x", p_cr[i].cid);
   
	   u8 *content_buf, *decrypted_buf;
	   u32 content_size;

	   retval = get_nus_object(INPUT_TITLEID_H, INPUT_TITLEID_L, INPUT_VERSION, cidstr, &content_buf, &content_size);
	   if (retval < 0) {
			debug_printf("get_nus_object(%s) failed with error %d, content size = %u\n", 
					cidstr, retval, content_size);
			return(1);
		}

		if (content_buf == NULL) {
			debug_printf("error allocating content buffer, size was %u\n", content_size);
			return(1);
		}

		if (content_size % 16) {
			debug_printf("ERROR: downloaded content[%hu] size %u is not a multiple of 16\n",
					i, content_size);
			free(content_buf);
			return(1);
		}

   		if (content_size < p_cr[i].size) {
			debug_printf("ERROR: only downloaded %u / %llu bytes\n", content_size, p_cr[i].size);
			free(content_buf);
			return(1);
   		} 

		decrypted_buf = malloc(content_size);
		if (!decrypted_buf) {
			debug_printf("ERROR: failed to allocate decrypted_buf (%u bytes)\n", content_size);
			free(content_buf);
			return(1);
		}

		decrypt_buffer(i, content_buf, decrypted_buf, content_size);

		sha1 hash;
		SHA1(decrypted_buf, p_cr[i].size, hash);

		if (!memcmp(p_cr[i].hash, hash, sizeof hash)) {
                  debug_printf("\b hash OK.\n");
			//display_ios_tags(decrypted_buf, content_size);
		

#if SAVE_DECRYPTED

sprintf(name,"fat0:/modulo_%s.elf",cidstr);
				fd = fopen(name, "wb");
				if (fd) {
				fwrite(decrypted_buf, content_size, 1, fd);
				fclose(fd);
				}
#else			

			update_tmd = 0;
			switch (p_cr[i].cid) {
			case 0x00000000:
				break;

			case 0x00000001: /* DIP */
#if defined(IOS36) || defined(IOS38)
printf("DIP Patch\n");

				if (!apply_patch(decrypted_buf, DIP_patch1_pos, DIP_orig1, sizeof(DIP_orig1), DIP_patch1, sizeof(DIP_patch1))) {
					printf("DIP patch 1 failed.\n");
					return 0;
				}
			
				if (!apply_patch(decrypted_buf, DIP_DVD_enable_orig_pos1, DIP_DVD_enable_orig, sizeof(DIP_DVD_enable_orig), DIP_DVD_enable_patch, sizeof(DIP_DVD_enable_patch))) {
					printf("DIP DVD enable patch 1 failed.\n");
					return 0;
				}

				if (!apply_patch(decrypted_buf, DIP_DVD_enable_orig_pos2, DIP_DVD_enable_orig, sizeof(DIP_DVD_enable_orig), DIP_DVD_enable_patch, sizeof(DIP_DVD_enable_patch))) {
					printf("DIP DVD enable patch 2 failed.\n");
					return 0;
				}


#ifdef ADD_DIP_PLUGIN
				/* Replace function handle DI command. */
				
				if (!apply_patch(decrypted_buf, DIP_handle_di_cmd, DIP_handle_di_cmd_orig, sizeof(DIP_handle_di_cmd_orig),
					patch_handle_di_cmd,sizeof (patch_handle_di_cmd))) {
					printf("DIP A8 patch failed.\n");
					return 0;
				}

				#if defined(IOS36) || defined(IOS38)
		
				// apply patch directly
				memcpy(&decrypted_buf[DIP_handle_di_cmd_reentry], handle_di_cmd_reentry,sizeof(handle_di_cmd_reentry));
			

				#else
				int rv = add_dip_plugin(&decrypted_buf);
				if (rv <= 0) {
					debug_printf("DIP additional patch failed! (rv = %d)\n", rv);
					return(1);
				}
				p_cr[i].size = rv;
				#endif
				content_size = round_up(p_cr[i].size, 0x40);
				
#endif
				debug_printf("Patched DIP.\n");
				update_tmd = 1;
#endif
				break;
        
		#ifdef IOS38
			case 0x00000011: /* FFS, ES, IOSP */
	
				printf("Patch ES\n");
                memcpy(decrypted_buf, ES_decrypted_buf, ES_content_size);
				free(ES_decrypted_buf);
		#endif
		#ifdef IOS36
			case  0x0000000e: /* FFS, ES, IOSP */
		#endif
				if (!apply_patch(decrypted_buf, ES_patch1_pos, ES_orig1, sizeof(ES_orig1), ES_patch1, sizeof(ES_patch1))) {
					printf("IOS patch 1 failed.\n");
					return 0;
				}
				if (!apply_patch(decrypted_buf, ES_patch2_pos, ES_orig2, sizeof(ES_orig2), ES_patch2, sizeof(ES_patch2))) {
					printf("IOS patch 2 failed.\n");
					return 0;
				}
				if (!apply_patch(decrypted_buf, ES_patch3_pos, ES_orig3, sizeof(ES_orig3), ES_patch3, sizeof(ES_patch3))) {
					printf("IOS patch 3 failed.\n");
					return 0;
				}
				if (!apply_patch(decrypted_buf, ES_patch4_pos, ES_orig4, sizeof(ES_orig4), ES_patch4, sizeof(ES_patch4))) {
					printf("IOS patch 4 failed.\n");
					return 0;
				}
				#ifdef  ADD_ES_PLUGIN

				if (!apply_patch(decrypted_buf, ES_ioctvl_patch_pos, ES_ioctlv_orig, sizeof(ES_ioctlv_orig), ES_patch_ioctvl, sizeof(ES_patch_ioctvl))) {
					printf("IOS patch Ioctlv failed.\n");
					return 0;
				}
				#endif
				
	

				update_tmd = 1;

				break;

			default:
				break;
			}
			if(update_tmd == 1) {
				debug_printf("Updating TMD.\n");
				SHA1(decrypted_buf, p_cr[i].size, hash);
				memcpy(p_cr[i].hash, hash, sizeof hash);
				if (p_cr[i].type == 0x8001) {
					p_cr[i].type = 1;
				}
				tmd_dirty=1;
			}

			retval = (int) save_nus_object(p_cr[i].cid, decrypted_buf, content_size);
			if (retval < 0) {
				debug_printf("save_nus_object(%x) returned error %d\n", p_cr[i].cid, retval);
				return(1);
			}
#endif   // save_decrypt

		} else {
			debug_printf("hash BAD\n");
			return(1);
		}
   
		free(decrypted_buf);
	   	free(content_buf);
	}

#ifndef SAVE_DECRYPTED
        if(add_custom_modules(p_tmd))
                tmd_dirty=1;

	if ((INPUT_TITLEID_H != OUTPUT_TITLEID_H) 
		|| (INPUT_TITLEID_L != OUTPUT_TITLEID_L)) {
		debug_printf("Changing titleid from %08x-%08x to %08x-%08x\n",
			INPUT_TITLEID_H, INPUT_TITLEID_L,
			OUTPUT_TITLEID_H, OUTPUT_TITLEID_L);
		change_ticket_title_id(s_tik, OUTPUT_TITLEID_H, OUTPUT_TITLEID_L);
		change_tmd_title_id(s_tmd, OUTPUT_TITLEID_H, OUTPUT_TITLEID_L);
	} 

	if (tmd_dirty) {
    	forge_tmd(s_tmd);
    	tmd_dirty = 0;
  	}

  	if (tik_dirty) {
    	forge_tik(s_tik);
    	tik_dirty = 0;
  	}

  	debug_printf("Download complete. Installing:\n");

  	retval = install_ticket(s_tik, s_certs, haxx_certs_size);
  	if (retval) {
    	debug_printf("install_ticket returned %d\n", retval);
		return(1);
  	}

  	retval = install(s_tmd, s_certs, haxx_certs_size);
#endif 
  	if (retval) {
    	debug_printf("install returned %d\n", retval);
    	return(1);
  	}

  	debug_printf("Done!\n");

	return(0);
}
