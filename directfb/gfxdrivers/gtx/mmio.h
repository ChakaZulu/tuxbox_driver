#ifndef __GTX__MMIO_H__
#define __GTX__MMIO_H__

#include <asm/types.h>


static inline void
gtx_out8(volatile __u8 *mmioaddr, __u32 reg, __u8 value)
{
     *((volatile __u8*)(mmioaddr+reg)) = value;
}

static inline void
gtx_out16(volatile __u8 *mmioaddr, __u32 reg, __u16 value)
{
     *((volatile __u16*)(mmioaddr+reg)) = value;
}

static inline void
gtx_out32(volatile __u8 *mmioaddr, __u32 reg, __u32 value)
{
     *((volatile __u32*)(mmioaddr+reg)) = value;
}

static inline volatile __u8
gtx_in8(volatile __u8 *mmioaddr, __u32 reg)
{
     return *((volatile __u8*)(mmioaddr+reg));
}

static inline volatile __u16
gtx_in16(volatile __u8 *mmioaddr, __u32 reg)
{
     return *((volatile __u16*)(mmioaddr+reg));
}

static inline volatile __u32
gtx_in32(volatile __u8 *mmioaddr, __u32 reg)
{
     return *((volatile __u32*)(mmioaddr+reg));
}

#endif

