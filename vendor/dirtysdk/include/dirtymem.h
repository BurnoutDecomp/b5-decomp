#ifndef DIRTYSDK_DIRTYMEM_H
#define DIRTYSDK_DIRTYMEM_H

#include "types.hpp"

// DirtySDK 5.5.3 - DirtySock memory interface.
// DirtyMemAlloc / DirtyMemFree are provided by the game (the CGS DirtySock server
// interface defines them); every DirtySDK module allocates through them, tagged with
// a four-character module id and the current memory group. The group is a small
// stack the lobby pushes around allocations it makes on a caller's behalf.
// Group bodies: ../src/dirtymem.cpp.

#ifdef __cplusplus
extern "C" {
#endif

// Game-provided allocator hooks.
void* DirtyMemAlloc(s32 iSize, s32 iMemModule, s32 iMemGroup);
void  DirtyMemFree(void* pMem, s32 iMemModule, s32 iMemGroup);

// Push / pop the current memory group; query the group on top of the stack
// (the bottom entry is 'dflt').
void DirtyMemGroupEnter(s32 iGroup);
void DirtyMemGroupLeave(void);
s32  DirtyMemGroupQuery(void);

#ifdef __cplusplus
}
#endif

#endif // DIRTYSDK_DIRTYMEM_H
