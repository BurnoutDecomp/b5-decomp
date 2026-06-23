#include "SDKs/EATech/include/Apt/AptPseudoCIH.h"

#include <new>   // placement new

// ===========================================================================
//  AptPseudoCIH_t -- ctor/dtor.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX:
//      AptPseudoCIH_t::AptPseudoCIH_t   @ 0x82AE47C0
//      AptPseudoCIH_t::~AptPseudoCIH_t  @ 0x82AE5AA0
//  X360 asm is authoritative. See AptPseudoCIH.h for the member layout.
//
//  The X360 ctor (registers r3=this, r4=source, r5=charId, r6=context,
//  r7=data) does, store-for-store:
//      *(this+8) = context;                     // stw r6, 8(r30)
//      *this     = source;                      // stw r31(source), 0(r30)
//      if (source && *source == 3) {            // cmpwi *source, 3
//          v9 = Allocate(off_8324D808, 0x1C);   // 28-byte AptPseudoData_t
//          this[1] = v9 ? AptPseudoData_t(v9, source, charId, data) : 0;
//      } else {
//          this[1] = 0;
//      }
//      this[3] = 0;                             // mpNext
//      this[4] = 0;                             // mpPrev
//
//  The dtor (@0x82AE5AA0):
//      v2 = this[1];                            // mpPseudoData
//      *this    = 0;                            // mpSource
//      this[3]  = 0;                            // mpNext
//      this[4]  = 0;                            // mpPrev
//      if (v2) {
//          v2[0] = v2[1] = v2[2] = 0;           // clear the snapshot's first 3 dwords
//          Deallocate(off_8324D808, v2, 0x1C);  // free 28 bytes back to the pool
//          this[1] = 0;                         // mpPseudoData
//      }
//
//  The snapshot's first-three-dword clear (mpData / mpMatrix / mpColorTransform)
//  before the free is reproduced explicitly.
// ===========================================================================

// The X360 ctor/dtor pass the literal pool-block size 0x1C (28) -- the byte size
// of an AptPseudoData_t in the 32-bit X360 ABI. Here the allocation is sized by
// the type (sizeof AptPseudoData_t), which is the semantic intent ("one
// AptPseudoData_t"); on a 64-bit host the byte count widens with the wider
// pointers but the meaning is identical. (No fixed-28 static_assert: that would
// only hold for the 32-bit target, not the host x64 gate build.)

// ctor @ 0x82AE47C0
AptPseudoCIH_t::AptPseudoCIH_t(AptCharacterInfo_t* lpSource,
                               s16                 li16CharacterId,
                               void*               lpContext,
                               void*               lpData)
{
    mpContext = lpContext;   // *(this+8) = a4
    mpSource  = lpSource;    // *this     = a2

    if (lpSource && lpSource->GetTypeTag() == AptCharacterInfo_t::KU_TypePlaceableDisplayObject)
    {
        void* lpBlock = gpAptPseudoDataPool->Allocate(sizeof(AptPseudoData_t));  // Allocate(off_8324D808, 0x1C)
        if (lpBlock)
        {
            // The X360 ctor returns the constructed object pointer in r3 and stores
            // it; placement-new on the pool block reproduces that.
            mpPseudoData = ::new (lpBlock) AptPseudoData_t(lpSource, li16CharacterId, lpData);
        }
        else
        {
            mpPseudoData = nullptr;
        }
    }
    else
    {
        mpPseudoData = nullptr;
    }

    mpNext = nullptr;   // this[3] = 0
    mpPrev = nullptr;   // this[4] = 0
}

// dtor @ 0x82AE5AA0
AptPseudoCIH_t::~AptPseudoCIH_t()
{
    AptPseudoData_t* lpSnapshot = mpPseudoData;  // v2 = this[1]

    mpSource = nullptr;   // *this    = 0
    mpNext   = nullptr;   // this[3]  = 0
    mpPrev   = nullptr;   // this[4]  = 0

    if (lpSnapshot)
    {
        // Clear the snapshot's first three dwords (mpData/mpMatrix/mpColorTransform)
        // exactly as the X360 dtor does before returning the block to the pool.
        lpSnapshot->mpData          = nullptr;  // *v2     = 0
        lpSnapshot->mpMatrix        = nullptr;  // v2[1]   = 0
        lpSnapshot->mpColorTransform = nullptr; // v2[2]   = 0

        gpAptPseudoDataPool->Deallocate(lpSnapshot, sizeof(AptPseudoData_t));  // Deallocate(off_8324D808, v2, 0x1C)
        mpPseudoData = nullptr;   // this[1] = 0
    }
}
