#ifndef GXVERT_H
#define GXVERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "revolution/base/PPCWGPipe.h"

extern volatile PPCWGPipe GXWGFifo;

#define __GXCDEF(prfx, n, t) __GXCDEF##n(prfx##n##t, t, t)
#define __GXCDEFX(func, n, t) __GXCDEF##n(func, t, t)
#define __GXCDEF1(func, ts, td) static inline void func(const ts x) { GXWGFifo.td = (td)x; }
#define __GXCDEF2(func, ts, td) static inline void func(const ts x, const ts y) { GXWGFifo.td = (td)x; GXWGFifo.td = (td)y; }
#define __GXCDEF3(func, ts, td) static inline void func(const ts x, const ts y, const ts z) { GXWGFifo.td = (td)x; GXWGFifo.td = (td)y; GXWGFifo.td = (td)z; }
#define __GXCDEF4(func, ts, td) static inline void func(const ts x, const ts y, const ts z, const ts w) { GXWGFifo.td = (td)x; GXWGFifo.td = (td)y; GXWGFifo.td = (td)z; GXWGFifo.td = (td)w; }

__GXCDEF(GXCmd, 1, u8)
__GXCDEF(GXCmd, 1, u16)
__GXCDEF(GXCmd, 1, u32)
__GXCDEF(GXCmd, 1, f32)

__GXCDEF(GXPosition, 3, f32)
__GXCDEF(GXPosition, 3, u8)
__GXCDEF(GXPosition, 3, s8)
__GXCDEF(GXPosition, 3, u16)
__GXCDEF(GXPosition, 3, s16)
__GXCDEF(GXPosition, 2, f32)
__GXCDEF(GXPosition, 2, u8)
__GXCDEF(GXPosition, 2, s8)
__GXCDEF(GXPosition, 2, u16)
__GXCDEF(GXPosition, 2, s16)
__GXCDEF(GXPosition, 1, u16)

__GXCDEF(GXNormal, 3, f32)
__GXCDEF(GXNormal, 3, s16)
__GXCDEF(GXNormal, 3, s8)

__GXCDEF(GXColor, 4, u8)
__GXCDEF(GXColor, 1, u32)
__GXCDEF(GXColor, 1, u16)
__GXCDEF(GXColor, 3, u8)

__GXCDEF(GXTexCoord, 2, f32)
__GXCDEF(GXTexCoord, 2, s16)
__GXCDEF(GXTexCoord, 2, u16)
__GXCDEF(GXTexCoord, 2, s8)
__GXCDEF(GXTexCoord, 2, u8)
__GXCDEF(GXTexCoord, 1, f32)
__GXCDEF(GXTexCoord, 1, s16)
__GXCDEF(GXTexCoord, 1, u16)
__GXCDEF(GXTexCoord, 1, s8)
__GXCDEF(GXTexCoord, 1, u8)

static inline void GXPosition1x8(const u8 index) { GXWGFifo.u8 = index; }
static inline void GXNormal1x16(const u16 index) { GXWGFifo.u16 = index; }
static inline void GXNormal1x8(const u8 index) { GXWGFifo.u8 = index; }
static inline void GXColor1x8(const u8 index) { GXWGFifo.u8 = index; }
static inline void GXTexCoord1x8(const u8 index) { GXWGFifo.u8 = index; }
static inline void GXMatrixIndex1x8(const u8 index) { GXWGFifo.u8 = index; }

#ifdef __cplusplus
}
#endif

#endif  // GXVERT_H
