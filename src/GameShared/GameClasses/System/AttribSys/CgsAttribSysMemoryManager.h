#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysPackageAllocator.h"

namespace CgsMemory
{
class LinearMalloc;
class HeapMalloc;
}

namespace CgsAttribSys
{
// CgsAttribSys::AttribSysMemoryManager -- the AttribSys memory front-end.
//
// A namespace-of-statics manager (an all-static "class" in the DWARF home
// GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h). Prepare()
// adopts the engine's LinearMalloc and three HeapMallocs and Construct()s/Prepare()s
// the three AttribSysPackageAllocators (one each for AttribSys, GameTalk, EASTL);
// the Get*Allocator() accessors hand those package allocators back to the EA
// AttribSys vendor library. The sbHasLinearAllocator flag records that Prepare()
// has run and gates the accessors.
//
// Member set + the alignment constants are recovered from the DecFIGS DWARF
// (CgsAttribSysMemoryManager.h / .cpp). Only GetAttribSysAllocator() is bodied in
// this TU's .cpp (X360 0x821F02A0); the other members are declared so callers
// compile and are bodied in their own TUs.
class AttribSysMemoryManager
{
public:
    // Alignment each package is prepared with (DWARF CgsAttribSysMemoryManager.h:48-53).
    static const s32 KI_LINEAR_ALLOC_ALIGNMENT    = 16;
    static const s32 KI_ATTRIBSYS_ALLOC_ALIGNMENT = 16;
    static const s32 KI_GAMETALK_ALLOC_ALIGNMENT  = 4;
    static const s32 KI_EASTL_ALLOC_ALIGNMENT     = 4;

    static void Construct();
    static bool Prepare(CgsMemory::LinearMalloc* lpLinearAllocator,
                        CgsMemory::HeapMalloc* lpAttribSysHeap,
                        CgsMemory::HeapMalloc* lpGameTalkHeap,
                        CgsMemory::HeapMalloc* lpEaStlHeap);
    static bool Release();
    static void Destruct();

    // X360 0x821F02A0 -- the AttribSys package allocator. Asserts the manager has
    // been Prepare'd (sbHasLinearAllocator) then returns &sAttribSysAllocator.
    static AttribSysPackageAllocator* GetAttribSysAllocator();

    static AttribSysPackageAllocator* GetGameTalkAllocator();
    static AttribSysPackageAllocator* GetEaStlAllocator();

    static bool HasMemoryBuffer();

private:
    static CgsMemory::LinearMalloc*  spLinearAllocator;
    static bool                      sbHasLinearAllocator;
    static AttribSysPackageAllocator sAttribSysAllocator;
    static AttribSysPackageAllocator sGameTalkAllocator;
    static AttribSysPackageAllocator sEaStlAllocator;
};
}
