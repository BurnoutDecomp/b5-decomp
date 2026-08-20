// ChallengeList.cpp
// BrnResource::ChallengeList -- Construct / Destruct.
//
// Reconstructed from the X360 ARTIST build:
//   ChallengeList::Construct @ 0x82677D00  (executed in the boot trace)
//   ChallengeList::Destruct  @ 0x82677D78
//
// Both bodies in the binary are pointer-walk loops (the compiler strength-reduced
// the indexed member access into a marching pointer); they are re-rolled here into
// clean indexed loops over named members, which is semantic parity. The X360
// "return" of each function is the last assignment result -- a fastcall register
// artifact, not a real return value; both methods are void.
//
// The X360 inlined BaseResourcePtr::CreateFromHandle(&maStaticDataLists[i],
// &sentinel) at each iteration; the DecFIGS DWARF for both bodies shows the
// pre-inline call as ResourcePtr<ChallengeListResource>::operator=(...), i.e. the
// source was `maStaticDataLists[i] = skInvalidHandle;` (assign-from-ResourceHandle,
// which resets the ResourcePtr to that handle). That public assignment is the
// faithful source-level form and exactly the observable operation; reconstructed
// as such rather than calling the protected CreateFromHandle.

#include "SharedClasses/DataLists/ChallengeList.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"  // CgsResource::ResourceHandle (used by value below)
#include "SharedClasses/DataLists/ChallengeListResourceType.h"          // ChallengeListResource (complete: operator-> + mpEntries)
#include "SharedClasses/DataLists/ChallengeListEntry.h"                 // ChallengeListEntry (complete: mChallengeID, content-bought type)
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

namespace BrnResource
{

namespace
{
    // X360: &dword_82FFB25C -- the invalid/default resource-handle sentinel used to
    // reset each ResourcePtr to "no resource". PS3 DecFIGS resolves this sentinel:
    // ChallengeList::Construct/Destruct (0x811BF8 / 0x811AE4) call
    // BaseResourcePtr::CreateFromHandle(slot, &CgsResource::NULLResourcePtr.mHandle),
    // i.e. the X360 dword_82FFB25C IS CgsResource::NULLResourcePtr.mHandle -- the
    // engine's canonical null/invalid ResourceHandle. Modeled here as a file-local
    // default-constructed (zero) ResourceHandle, which is that null handle's value.
    // (CgsResource::NULLResourcePtr is not yet a defined global in this port; once it
    //  lands it should be referenced directly instead of this local.)
    const CgsResource::ResourceHandle skInvalidHandle = {};
}

// ChallengeList::Construct @ 0x82677D00
void ChallengeList::Construct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle.
    for ( s32 liIndex = 0; liIndex < KI_MAX_CHALLENGE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }

    // X360: each slot's bought-flag <- 0, both indices <- -1.
    for ( s32 liIndex = 0; liIndex < KI_MAX_FREEBURN_CHALLENGES; ++liIndex )
    {
        maSlots[ liIndex ].mbBought     = false;
        maSlots[ liIndex ].miListIndex  = -1;
        maSlots[ liIndex ].miEntryIndex = -1;
    }

    // X360: a1[3256] = 0; a1[3257] = 0;
    miCount     = 0;
    miListCount = 0;
}

// ChallengeList::Destruct @ 0x82677D78
void ChallengeList::Destruct()
{
    // X360: 32x reset of each static-data-list ResourcePtr to the invalid handle.
    for ( s32 liIndex = 0; liIndex < KI_MAX_CHALLENGE_LISTS; ++liIndex )
    {
        maStaticDataLists[ liIndex ] = skInvalidHandle;
    }
}

// ---------------------------------------------------------------------------
// File-scope DLC entitlement state (FLAG -- foreign home).
//
// IsChallengeContentBought reads a small block of downloadable-content state that the
// X360 binary lays out from base byte_82FFA7F0:
//   dword_82FFA7F4   -- the owned/entitlement bitmask          (+0x04)
//   dword_82FFA7F8[] -- per-content required-bit values        (+0x08, indexed)
//   dword_82FFA80C   -- index into the array for content-type 1 (+0x1C)
//   dword_82FFA810   -- index into the array for content-type 2 (+0x20)
//   byte_82FFA870    -- "type-1 content bought" flag            (+0x80)
//   byte_82FFA871    -- "type-2 content bought" flag            (+0x81)
// These belong to the DLC subsystem (BrnResource::DLCManager / its translation unit),
// NOT to ChallengeList.
//
// ⭐ [gateui r4] THEY ARE NOW REAL ZERO-INITIALISED DEFINITIONS, NOT `extern` DECLARATIONS.
// The round-3 form left six UNDEF externals in ChallengeList.obj that NOTHING in the tree
// defines -- survivable only under `/Gy + /OPT:REF` and only while every caller stayed
// unmounted, which this wave changes. ChallengeList.cpp takes the HOST SEAT for them until
// the DLC manager is reconstructed; the moment it lands, it becomes the owner and these
// definitions move there (they stay non-static precisely so that hand-over is an
// `extern` away and the linker catches a duplicate).
//
// ⛔ FLAG -- THE CONSOLE'S RUNTIME WRITER IS THE DLC MANAGER, AND THERE IS NO DLC MANAGER
// IN THIS BUILD. Nothing here writes any of these six; on the X360 they are .data that the
// downloadable-content subsystem fills after the entitlement query. So they hold zero for
// the whole process.
//
// POLARITY SANITY-CHECKED AGAINST THE EXPORT, not assumed. IsChallengeContentBought
// @0x8267BC08, verbatim:
//     if ( v2 == 1 ) { if ( (dword_82FFA7F4 & dword_82FFA7F8[dword_82FFA80C])
//                            != dword_82FFA7F8[dword_82FFA80C] ) return 0;
//                      v4 = byte_82FFA870; }
//     else           { if ( v2 != 2 ) return 1; ... v4 = byte_82FFA871; }
//     return v4 != 0;
// With every global zero: the required-bits value is 0, so `(0 & 0) != 0` is FALSE and the
// mask test does NOT early-out; the answer is then `byte_82FFA870/871 != 0` == FALSE.
// So zero yields "DLC-A and DLC-B challenges are NOT bought", while type 0 (the base-game
// challenges) still returns true unconditionally. That IS the asm's no-DLC behaviour, and
// it is the safe direction -- a build with no entitlement data must not unlock paid content.
//
// Extents of dword_82FFA7F8[]: the next named global in the block sits at +0x1C
// (dword_82FFA80C), so the array occupies +0x08..+0x1B == FIVE dwords. It is indexed only
// by the two index globals, both of which are zero here, so only element 0 is ever read in
// this build. The size is documentation of the console block, not a load-bearing bound.
u32  gauChallengeDlcOwnedMask       = 0;        // byte_82FFA7F4  (+0x04)
u32  gauChallengeDlcRequiredBits[5] = { 0, 0, 0, 0, 0 };  // byte_82FFA7F8[] (+0x08..+0x1B)
u32  guChallengeDlcIndexTypeA       = 0;        // byte_82FFA80C  (+0x1C)
u32  guChallengeDlcIndexTypeB       = 0;        // byte_82FFA810  (+0x20)
u8   gbChallengeContentBoughtTypeA  = 0;        // byte_82FFA870  (+0x80)
u8   gbChallengeContentBoughtTypeB  = 0;        // byte_82FFA871  (+0x81)

// ChallengeList::GetChallengeData(s32) @ 0x82326080
// X360: range-guard the index against miCount ("Index out of range", ChallengeList.h:162,
// non-fatal), then resolve the entry as
//   maStaticDataLists[ maSlots[liIndex].miListIndex ]->mpEntries[ maSlots[liIndex].miEntryIndex ]
// The list ResourcePtr is dereferenced through operator-> (the X360 BrnResource::ChallengeL
// helper @0x82324D20 == ResourcePtr<ChallengeListResource>::operator-> const, baked assert
// CgsResourcePtr.h:563), and the entry is reached by indexing mpEntries (216-byte stride).
const ChallengeListEntry* ChallengeList::GetChallengeData( s32 liIndex ) const
{
    CGS_ASSERT( liIndex >= 0 && liIndex < miCount, "Index out of range\n" );

    const ChallengeSlot& lrSlot = maSlots[ liIndex ];

    const ChallengeListResource* lpResource = maStaticDataLists[ lrSlot.miListIndex ].operator->();

    return &lpResource->mpEntries[ lrSlot.miEntryIndex ];
}

// [gateui] ChallengeList::GetChallengeData(CgsID) -- the id-keyed overload.
// LEDGER IDENTITY: the X360 emitted it as the UNNAMED `sub_82337200` (the ledger's only
// `GetChallengeData` row, 0x82326080, is the s32 overload above). Its body is unambiguous:
//     0x82337214  bl    BrnResource__ChallengeList__GetChallengeIndex
//     0x82337218  mr    r4, r3
//     0x8233721C  cmpwi cr6, r4, 0
//     0x82337220  blt   -> li r3, 0 ; return NULL
//     0x82337228  bl    BrnResource__ChallengeList__GetChallengeData   (r3 = this, r4 = index)
// and its seven callers are exactly the id-keyed sites (ChallengeManager::ProcessEvent
// @0x8233D6A8, four FriendsListComponent entries, OnlineGameRoomPlayerInfo
// @0x824A4B60, and HudMessageAnalyzer::TriggerChallengeEndedMessage @0x82520078, whose
// baked assert text is literally "lpChallengeList->GetChallengeData( mCha...").
// Note it does NOT re-range-check: the s32 overload's own "Index out of range" assert is
// what guards the second hop, and a negative index short-circuits before it.
const ChallengeListEntry* ChallengeList::GetChallengeData( CgsID lID ) const
{
    const s32 liIndex = GetChallengeIndex( lID );
    if ( liIndex < 0 )
    {
        return 0;
    }
    return GetChallengeData( liIndex );
}

// ChallengeList::GetChallengeIndex(CgsID) @ 0x82326168
// X360: linear scan [0, miCount); return the first slot whose challenge data has
// GetChallengeData(i)->mChallengeID == lID (the 8-byte CgsID compared at entry +0xC0),
// else -1.
s32 ChallengeList::GetChallengeIndex( CgsID lID ) const
{
    for ( s32 liIndex = 0; liIndex < miCount; ++liIndex )
    {
        if ( GetChallengeData( liIndex )->GetChallengeID() == lID )
        {
            return liIndex;
        }
    }
    return -1;
}

// ChallengeList::IsChallengeContentBought(s32) @ 0x8267BC08
// X360: read the challenge's content-bought type (entry byte @0xD6). Type 1 / 2 each
// gate the challenge behind a downloadable-content entitlement: the content counts as
// "bought" only if (a) the owned-mask has ALL the required bits for that content's index
// AND (b) the matching per-type bought flag is set. Any other type value (0, ...) means
// the content is always available (returns true).
bool ChallengeList::IsChallengeContentBought( s32 liIndex ) const
{
    const u8 luType = GetChallengeData( liIndex )->GetContentBoughtType();

    if ( luType == ChallengeListEntry::E_CONTENT_BOUGHT_DLC_A )
    {
        const u32 luRequired = gauChallengeDlcRequiredBits[ guChallengeDlcIndexTypeA ];
        if ( ( gauChallengeDlcOwnedMask & luRequired ) != luRequired )
        {
            return false;
        }
        return gbChallengeContentBoughtTypeA != 0;
    }

    if ( luType == ChallengeListEntry::E_CONTENT_BOUGHT_DLC_B )
    {
        const u32 luRequired = gauChallengeDlcRequiredBits[ guChallengeDlcIndexTypeB ];
        if ( ( gauChallengeDlcOwnedMask & luRequired ) != luRequired )
        {
            return false;
        }
        return gbChallengeContentBoughtTypeB != 0;
    }

    return true;
}

} // namespace BrnResource
