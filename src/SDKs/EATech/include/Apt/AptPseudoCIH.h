#pragma once

// =====================================================================
//  AptPseudoCIH.h  --  EA APT (ActionScript Player Technology) middleware
//
//  AptPseudoCIH_t ("pseudo character-instance header") is the small
//  display-list node APT builds while interpreting a frame's place-object
//  commands. It wraps an optional AptPseudoData_t snapshot (built only when
//  the source character-info record is a placeable display object, tag == 3)
//  plus a couple of list-link slots.
//
//  NO DWARF exists for this class (X360-only; not in the PS3 DecFIGS dump nor
//  in the EATech public Apt.h). All shape below is derived STRICTLY from the
//  X360 binary:
//      AptPseudoCIH_t::AptPseudoCIH_t   @ 0x82AE47C0  (ctor)
//      AptPseudoCIH_t::~AptPseudoCIH_t  @ 0x82AE5AA0  (dtor)
//
//  LAYOUT (20 bytes, dword offsets reproduced from the ctor/dtor stores):
//      +0x00  mpSource     source character-info record (ctor arg a2)
//      +0x04  mpPseudoData AptPseudoData_t* snapshot (pool-allocated) or null
//      +0x08  mpContext    caller context (ctor arg a4)
//      +0x0C  mpNext       list link (zeroed by ctor)
//      +0x10  mpPrev       list link (zeroed by ctor)
//
//  The AptPseudoData_t (28 bytes) is allocated from the shared Apt DOGMA pool
//  (gpAptPseudoDataPool == X360 off_8324D808, the same pool the other Apt
//  display-list nodes use) and freed back to it by the dtor.
// =====================================================================

#include "types.hpp"

#include "SDKs/EATech/include/Apt/AptPseudoData.h"   // AptPseudoData_t, AptPlaceObjectInfo_t
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager

// ---------------------------------------------------------------------------
// off_8324D808 is the DOGMA_PoolManager* that backs the fixed-size Apt
// display-list node allocations (AptPseudoData_t here, and the
// AptSharedPtr<AptFile>/AptValue families elsewhere -- all reference the same
// off_8324D808 slot). Defined in AptGlobals.cpp; constructed + wired by
// AptAllocatorInitialize @0x82ADD118 (AptInit.cpp). (Sibling TUs declare the
// same global under their own name; the single underlying object is shared.)
// ---------------------------------------------------------------------------
extern DOGMA_PoolManager* gpAptPseudoDataPool;   // off_8324D808

// ---------------------------------------------------------------------------
// The source character-info record AptPseudoCIH_t wraps. The ctor reads only
// its leading 32-bit type tag (must equal 3 -- the placeable display-object
// type -- for a snapshot to be built); the AptPseudoData_t ctor then consumes
// the rest of the record as an AptPlaceObjectInfo_t. The leading tag aliases
// AptPlaceObjectInfo_t::maPad00 (offset 0), so the same record type serves
// both reads.
// ---------------------------------------------------------------------------
struct AptCharacterInfo_t : public AptPlaceObjectInfo_t
{
    // Type tag lives at offset 0 (== AptPlaceObjectInfo_t::maPad00[0..3]). The
    // value 3 selects the placeable display-object path.
    u32 GetTypeTag() const { return *reinterpret_cast<const u32*>(maPad00); }

    enum { KU_TypePlaceableDisplayObject = 3 };
};

struct AptPseudoCIH_t
{
    // ---- layout (20 bytes; offsets verified against the X360 ctor/dtor) ----
    AptCharacterInfo_t* mpSource;      // [0x00] source char-info record (ctor a2)
    AptPseudoData_t*    mpPseudoData;  // [0x04] pool-allocated snapshot, or null
    void*               mpContext;     // [0x08] caller context (ctor a4)
    void*               mpNext;        // [0x0C] display-list link (ctor-zeroed)
    void*               mpPrev;        // [0x10] display-list link (ctor-zeroed)

    // ctor @ 0x82AE47C0. Stores the source/context, and -- iff the source is a
    // placeable display object (tag == 3) -- pool-allocates and constructs an
    // AptPseudoData_t snapshot from it. li16CharacterId / lpData feed the
    // snapshot. Returns *this in the X360 fastcall convention.
    AptPseudoCIH_t(AptCharacterInfo_t* lpSource,
                   s16                 li16CharacterId,
                   void*               lpContext,
                   void*               lpData);

    // dtor @ 0x82AE5AA0. Zeroes the source/link slots, and if a snapshot exists
    // clears its first three dwords and frees it back to the pool.
    ~AptPseudoCIH_t();
};
