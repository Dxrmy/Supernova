#ifndef OS_H
#define OS_H

#include <stdarg.h>
#include "revolution/types.h"
#include "revolution/os/OSAlarm.h"
#include "revolution/os/OSMessage.h"
#include "revolution/os/OSMutex.h"
#include "revolution/os/OSThread.h"
#include "revolution/os/OSTime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSHalt(msg) OSPanic(__FILE__, __LINE__, msg)

#ifndef OS_CORE_CLOCK
#define OS_CORE_CLOCK 729000000u
#endif
#ifndef OS_BUS_CLOCK
#define OS_BUS_CLOCK 243000000u
#endif
#ifndef OS_TIMER_CLOCK
#define OS_TIMER_CLOCK (OS_BUS_CLOCK / 4)
#endif

#define OSNanosecondsToTicks(nsec) (((nsec) * (OS_TIMER_CLOCK / 125000)) / 8000)
#define OSTicksToSeconds(ticks) ((ticks) / OS_TIMER_CLOCK)
#define OSTicksToMilliseconds(ticks) ((ticks) / (OS_TIMER_CLOCK / 1000))
#define OSTicksToMicroseconds(ticks) (((ticks)*8) / (OS_TIMER_CLOCK / 125000))
#define OSSecondsToTicks(sec) ((sec)*OS_TIMER_CLOCK)
#define OSMillisecondsToTicks(msec) ((msec) * (OS_TIMER_CLOCK / 1000))
#define OSMicrosecondsToTicks(usec) (((usec) * (OS_TIMER_CLOCK / 125000)) / 8)

#define OSRoundUp32B(x) (((uintptr_t)(x) + 32 - 1) & ~(uintptr_t)(32 - 1))
#define OSRoundDown32B(x) (((uintptr_t)(x)) & ~(uintptr_t)(32 - 1))
#define OSDiffTick(tick1, tick0) ((s32)(tick1) - (s32)(tick0))
#define OS_TICKS_DELTA(x, y) ((s32)(x) - (s32)(y))

typedef u16 OSError;
typedef struct OSContext OSContext;

u32 OSGetSoundMode(void);
void OSSetSoundMode(u32 mode);
u32 OSGetConsoleType(void);
void OSRegisterVersion(const char*);

void OSReport(const char*, ...);
void OSVReport(const char*, va_list);
void OSPanic(const char*, int, const char*, ...);

void* OSGetArenaHi(void);
void* OSGetArenaLo(void);
void OSSetArenaHi(void*);
void OSSetArenaLo(void*);
void OSSetMEM2ArenaHi(void*);
void* OSGetMEM1ArenaLo(void);
void* OSGetMEM1ArenaHi(void);
void* OSGetMEM2ArenaLo(void);
void* OSGetMEM2ArenaHi(void);
void* OSAllocFromMEM1ArenaLo(u32, u32);
u32 OSGetPhysicalMem2Size(void);
void OSProtectRange(u32, void*, u32, u32);
void* OSAllocFromHeap(int, u32);

BOOL OSDisableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL);
void OSInitMessageQueue(OSMessageQueue*, OSMessage*, s32);
BOOL OSSendMessage(OSMessageQueue*, OSMessage, s32);
BOOL OSReceiveMessage(OSMessageQueue*, OSMessage*, s32);

void DCFlushRange(void*, u32);
void DCInvalidateRange(void*, u32);
void DCStoreRange(void*, u32);
void ICInvalidateRange(void*, u32);

#ifdef __cplusplus
}
#endif

#endif  // SMG2_COMPAT_REVOLUTION_OS_H
