#ifndef DIRTYSDK_DIRTYLIB_X360_INTERNAL_H
#define DIRTYSDK_DIRTYLIB_X360_INTERNAL_H
#include "types.hpp"
#include <xtl.h>
struct NetCritT
{
    DWORD owner;
    u32 count;
    volatile LONG lock;
    CRITICAL_SECTION section;
};
CGS_STATIC_ASSERT((sizeof(NetCritT) == 40), "X360 NetCritT size");
extern "C" {
u32 NetTick(void);
void NetCritInit(NetCritT*, const char*);
s32 NetCritTry(NetCritT*);
void NetCritEnter(NetCritT*);
void NetCritLeave(NetCritT*);
}
#endif
