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

} // namespace BrnResource
