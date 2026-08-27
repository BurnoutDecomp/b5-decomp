#pragma once

// ChallengeList.h
// BrnResource::ChallengeList -- the runtime aggregation of loaded challenge-list
// resources plus the per-challenge "slot" table the profile uses to track which
// challenges exist and whether their content has been bought.
//
// Shape recovered from the DecFIGS DWARF (SharedClasses/DataLists/ChallengeList.h);
// behaviour of the two reconstructed methods (Construct/Destruct) verified against
// the X360 pseudocode @0x82677D00 / @0x82677D78. Only Construct + Destruct are
// reconstructed in this TU; every other method is declared-only and lands in its
// own TU.

#include "types.hpp"                                                        // s32, bool widths
#include "BrnCommonTypes.h"                                                 // CgsID (typedef u64)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // CgsResource::ResourcePtr<T>, BaseResourcePtr, ResourceHandle

namespace BrnResource
{

// ChallengeListResource is the element type of the static-data-list ResourcePtr
// array. It is NOT reconstructed in this TU; ResourcePtr<ChallengeListResource>
// embeds it by value but only needs the base ResourcePtr size (Construct/Destruct
// never dereference the element), so a forward declaration suffices. Deferred to
// its own TU.
class ChallengeListResource;

// ChallengeListEntry is referenced only by the declared-only accessors below (as a
// pointer return type). Forward-declared (its full layout lives in the committed
// ChallengeListEntry.h, intentionally not pulled in here to keep this TU minimal).
struct ChallengeListEntry;

// ChallengeList.h:49 -- one per-challenge runtime slot. POD; DWARF offsets:
// mbBought @+0, miListIndex @+4, miEntryIndex @+8. Namespace-scope sibling of
// ChallengeList (DWARF qualifies it BrnResource::ChallengeSlot, not nested).
struct ChallengeSlot
{
    bool mbBought;       // :51  @+0
    s32  miListIndex;    // :52  @+4
    s32  miEntryIndex;   // :53  @+8
};

// ChallengeList.h:65
class ChallengeList
{
public:
    static const s32 KI_MAX_CHALLENGE_LISTS     = 32;   // :70
    static const s32 KI_MAX_FREEBURN_CHALLENGES = 1000; // :73
    static const s32 KI_MAX_PROFILE_CHALLENGES  = 2000; // :74

    void Construct();   // :77  RECONSTRUCTED
    void Destruct();    // :80  RECONSTRUCTED

    // :92  RECONSTRUCTED @0x8267B598 (challenge-list wave 2026-08-27) -- registration +
    // the console's post-load fixup pass. Its only caller is
    // BrnResource::GameDataModule::PrepareFreeburnChallengeList @0x8266C088 (Prepare stage 10).
    void                       AddListResource(CgsResource::ResourcePtr<ChallengeListResource>& lrResource);
    // :95  RECONSTRUCTED (X360-inlined; replaces the return-0 gate that lived in
    // BrnFriendsListLinkGates.cpp until this wave made the table real).
    s32                        GetChallengeCount() const;

    // ---- declared-only (each reconstructed in its own TU) ----
    const ChallengeListEntry*  GetChallengeData(s32 liIndex) const;                  // :98
    const ChallengeListEntry*  GetChallengeData(CgsID lID) const;                    // :101
    s32                        GetChallengeIndex(CgsID lID) const;                   // :104
    bool                       IsChallengeInList(CgsID lID) const;                   // :107
    bool                       IsChallengeContentBought(s32 liIndex) const;          // :110
    bool                       IsChallengeContentBought(CgsID lID) const;            // :113
    void                       SetChallengeContentBought(s32 liIndex, bool lbBought); // :116
    void                       SetChallengeContentBought(CgsID lID, bool lbBought);   // :119

private:
    // ---- layout (DWARF offsets) ----
    CgsResource::ResourcePtr<ChallengeListResource> maStaticDataLists[KI_MAX_CHALLENGE_LISTS]; // :124
    ChallengeSlot                                   maSlots[KI_MAX_FREEBURN_CHALLENGES];       // :125
    s32                                             miCount;                                   // :126
    s32                                             miListCount;                               // :127
};

} // namespace BrnResource
