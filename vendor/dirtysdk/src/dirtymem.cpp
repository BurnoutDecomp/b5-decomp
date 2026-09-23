// DirtySDK dirtysock -- memory group stack (core/source/dirtysock/dirtymem.c).
//
// Allocations are tagged with the group on top of this stack. The lobby pushes a
// caller's group around work it does on that caller's behalf; the bottom entry is the
// default group 'dflt'. DirtyMemAlloc / DirtyMemFree themselves are game-provided.

#include "dirtymem.h"
#include "dirtylib.h"   // NetPrintf

// Group stack: _DirtyMem_iGroup indexes the current top. The SDK's overflow test lets
// the index reach 16 before it refuses a push, one past its 16-entry stack; the extra
// slot keeps that seventeenth push inside this array instead of the next variable.
static s32 _DirtyMem_iGroup = 0;
static s32 _DirtyMem_iGroupStack[16 + 1] =
{
    ('d' << 24) | ('f' << 16) | ('l' << 8) | 't',
};

extern "C" void DirtyMemGroupEnter(s32 iGroup)
{
    if (_DirtyMem_iGroup >= 16)
    {
        NetPrintf(("dirtymem: group stack overflow\n"));
        return;
    }
    _DirtyMem_iGroupStack[++_DirtyMem_iGroup] = iGroup;
}

extern "C" void DirtyMemGroupLeave(void)
{
    if (_DirtyMem_iGroup <= 0)
    {
        NetPrintf(("dirtymem: group stack underflow\n"));
        return;
    }
    --_DirtyMem_iGroup;
}

extern "C" s32 DirtyMemGroupQuery(void)
{
    return _DirtyMem_iGroupStack[_DirtyMem_iGroup];
}
