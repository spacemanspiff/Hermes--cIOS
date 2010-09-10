#ifndef __UTILS_H__
#define __UTILS_H__

#include "types.h"

#define swab32(x) ((u32)(                                     \
                                   (((u32)(x) & (u32)0x000000ffUL) << 24) | \
                                   (((u32)(x) & (u32)0x0000ff00UL) <<  8) | \
                                   (((u32)(x) & (u32)0x00ff0000UL) >>  8) | \
                                   (((u32)(x) & (u32)0xff000000UL) >> 24)))
#define swab16(x) ((u16)(                                   \
                (((u16)(x) & (u16)0x00ffU) << 8) |          \
                (((u16)(x) & (u16)0xff00U) >> 8)))

# define ATTRIBUTE_PACKED         __attribute__((packed))
# define ATTRIBUTE_ALIGN(v)       __attribute__((aligned(v)))


void dip_memset(u8 *buf, u32 c, u32 size);

void *VirtToPhys(void *address);
void *PhysToVirt(void *address);

#endif
