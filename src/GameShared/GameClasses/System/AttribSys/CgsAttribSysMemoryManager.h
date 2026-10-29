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
// Native lifecycle receiver with shared allocator state (DWARF home
// GameShared/GameClasses/System/AttribSys/CgsAttribSysMemoryManager.h). Prepare()
// adopts the engine's LinearMalloc and three HeapMallocs and Construct()s/Prepare()s
// the three AttribSysPackageAllocators (one each for AttribSys, GameTalk, EASTL);
// the Get*Allocator() accessors hand those package allocators back to the EA
// AttribSys vendor library. The sbHasLinearAllocator flag records that Prepare()
// has run and gates the accessors.
//
// Native Prepare @0x828043B8 receives this in r3 and the four allocators in
// r4-r7. The module calls it on its mMemoryManager member at +0x228; allocator
// state remains shared in the original file-scope storage.
class AttribSysMemoryManager
{
public:
    // Alignment each package is prepared with (DWARF CgsAttribSysMemoryManager.h:48-53).
    static const s32 KI_LINEAR_ALLOC_ALIGNMENT    = 16;
    static const s32 KI_ATTRIBSYS_ALLOC_ALIGNMENT = 16;
    static const s32 KI_GAMETALK_ALLOC_ALIGNMENT  = 4;
    static const s32 KI_EASTL_ALLOC_ALIGNMENT     = 4;

    void Construct();
    bool Prepare(CgsMemory::LinearMalloc* lpLinearAllocator,
                        CgsMemory::HeapMalloc* lpAttribSysHeap,
                        CgsMemory::HeapMalloc* lpGameTalkHeap,
                        CgsMemory::HeapMalloc* lpEaStlHeap);
    bool Release();
    void Destruct();

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
