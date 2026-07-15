#include "SDKs/Realmc/RealmcIfaceSaveCheckParams.h"

#include "SDKs/Realmc/RealmcCore.h"  // RealmcCore::AllocateMem / FreeMemSize

// ===========================================================================
// RealmcIface::SaveCheckParams -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// No leak source / no DWARF: SHAPE and BODY both come from the X360 asm (see
// RealmcIfaceSaveCheckParams.h for the layout). Reproduced store-for-store /
// branch-for-branch; the do/while loops are de-optimized to for-loops with the
// identical bounds and side effects.
//
// The per-slot SaveReq copy is the X360 sub_82B51DC0 -- SaveReq's copy
// constructor, which is NOT homed in this tree (external/unknown in the ledger;
// a distinct routine from SaveReq's 6-arg ctor @ 0x82B51D60). It is declared
// below as an un-homed external helper so this TU reproduces the copy call
// across the TU boundary WITHOUT fabricating that routine's body.  <<< FLAG
// ===========================================================================

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// FLAG (un-homed dependency): the X360 SaveReq copy constructor, sub_82B51DC0.
//   int *__fastcall sub_82B51DC0(_DWORD *Dst, _DWORD *Src)  -- copy-construct a
//   356-byte SaveReq at Dst from the SaveReq at Src, returning Dst. Its body is
//   not attested in this TU's asm; declared extern here (mirrors the
//   RealmcCopyEntryContentParams boundary in RealmcSaveReq.h). Defined by the
//   SaveReq TU when that copy ctor is homed.
// ---------------------------------------------------------------------------
SaveReq* RealmcCopySaveReq(void* pDst, const void* pSrc);

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
        mppReqs = static_cast<SaveReq**>(
            RealmcCore::AllocateMem("SaveReq array", 4 * static_cast<std::size_t>(nCount)));
    }

    for (s32 i = 0; i < mCount; ++i)
    {
        void* pMem = RealmcCore::AllocateMem("SaveReq", 0x164);   // 356 bytes
        mppReqs[i] = pMem ? RealmcCopySaveReq(pMem, paSources[i]) // sub_82B51DC0
                          : nullptr;
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
        RealmcCore::FreeMemSize(mppReqs[i], 0x164);               // 356 bytes
    }

    if (mppReqs)
    {
        RealmcCore::FreeMemSize(mppReqs, 4 * static_cast<u32>(mCount));
    }
}

} // namespace RealmcIface
