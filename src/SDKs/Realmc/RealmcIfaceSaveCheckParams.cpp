#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"

#include "SDKs/Realmc/RealmcCore.h"  // RealmcCore::AllocateMem / FreeMemSize

#include <new>  // placement new (each slot's SaveReq copy)

// ===========================================================================
// RealmcIface::SaveCheckParams -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm (see
// RealmcIfaceSaveCheckParams.h for the layout). Reproduced store-for-store /
// branch-for-branch; the do/while loops are de-optimized to for-loops with the
// identical bounds and side effects. Each slot holds a SaveReq copy-constructed
// in a block from the Realmc allocator (SaveReq's copy ctor, RealmcSaveReq.cpp).
//
// Host widths: the slot array holds host pointers, so it is sized and freed as
// sizeof(SaveReq*) per slot (the console's 4); a SaveReq is 0x164 bytes on both.
// ===========================================================================

namespace RealmcIface
{

static_assert(sizeof(SaveReq) == 0x164, "SaveReq is 0x164 bytes on the host too");

// ---------------------------------------------------------------------------
// SaveCheckParams::SaveCheckParams @ 0x82B51E38
//
//   stw  r4, 0(r31)                 -> mCount = nCount        (a2)
//   stw  r11(=0), 4(r31)            -> mppReqs = nullptr
//   beq  cr6 ...                    -> if ( nCount )
//     bl RealmcCore__AllocateMem("SaveReq array", nCount<<2)
//     stw r3, 4(r31)               ->   mppReqs = AllocateMem("SaveReq array", 4*nCount)
//   loop while ( i < mCount ):
//     r3 = RealmcCore__AllocateMem("SaveReq", 0x164)
//     if ( r3 ) r3 = sub_82B51DC0(r3, paSources[i]) else r3 = 0
//     mppReqs[i] = r3
// ---------------------------------------------------------------------------
SaveCheckParams::SaveCheckParams(s32 nCount, SaveReq* const* paSources)
{
    mCount  = nCount;                                             // +0x000
    mppReqs = nullptr;                                            // +0x004
    if (nCount)
    {
        mppReqs = static_cast<SaveReq**>(RealmcCore::AllocateMem(
            "SaveReq array", sizeof(SaveReq*) * static_cast<std::size_t>(nCount)));
    }

    for (s32 i = 0; i < mCount; ++i)
    {
        void* pMem = RealmcCore::AllocateMem("SaveReq", sizeof(SaveReq));
        mppReqs[i] = pMem ? new (pMem) SaveReq(*paSources[i]) : nullptr;
    }
}

// ---------------------------------------------------------------------------
// SaveCheckParams::~SaveCheckParams @ 0x82B51F88
//
//   loop while ( i < mCount ):
//     RealmcCore__FreeMemSize(mppReqs[i], 0x164)
//   r3 = mppReqs
//   if ( r3 ) RealmcCore__FreeMemSize(mppReqs, mCount<<2)
//
// The X360 dtor passes mppReqs[i] straight to FreeMemSize without a null guard
// (it frees whatever the slot holds); reproduced faithfully.
// ---------------------------------------------------------------------------
SaveCheckParams::~SaveCheckParams()
{
    for (s32 i = 0; i < mCount; ++i)
    {
        RealmcCore::FreeMemSize(mppReqs[i], sizeof(SaveReq));
    }

    if (mppReqs)
    {
        RealmcCore::FreeMemSize(mppReqs, static_cast<u32>(sizeof(SaveReq*)) * static_cast<u32>(mCount));
    }
}

} // namespace RealmcIface
