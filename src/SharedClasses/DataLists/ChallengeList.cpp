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
    // reset each ResourcePtr to "no resource". Modeled as a file-local default
    // (zero) ResourceHandle: {nullptr, nullptr}.
    // FLAG: the exact .rodata bytes of dword_82FFB25C are not recovered; an all-zero
    // / invalid handle is the modeled value.
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
// NOT to ChallengeList. They are declared here as honest extern placeholders so the
// observable read pattern is faithful; the real definitions land with the DLC manager
// pass. The exact .rodata extents of dword_82FFA7F8[] are not recovered (it is indexed
// by the two index globals); modeled as an extern unbounded array. Do NOT treat these
// names/semantics as a committed home -- they are placeholders to be re-homed.
extern u32  gauChallengeDlcOwnedMask;        // byte_82FFA7F4
extern u32  gauChallengeDlcRequiredBits[];   // byte_82FFA7F8[]
extern u32  guChallengeDlcIndexTypeA;        // byte_82FFA80C
extern u32  guChallengeDlcIndexTypeB;        // byte_82FFA810
extern u8   gbChallengeContentBoughtTypeA;   // byte_82FFA870
extern u8   gbChallengeContentBoughtTypeB;   // byte_82FFA871

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
