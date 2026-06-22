#ifndef BRN_CHALLENGE_LIST_COMPONENT_H
#define BRN_CHALLENGE_LIST_COMPONENT_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID (typedef u64)

// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//   BrnGui::ChallengeListComponent::GetChallengeStyle  @ 0x8248FB58
//   (Hex-Rays truncated the symbol to "GetChalle"; the @0x823542A0 callee it ends in
//    is BrnResource::ChallengeListEntry::GetChallengeStyle, so the component accessor
//    is the matching "GetChallengeStyle".)
//
// The component is a GUI list view over a loaded BrnResource::ChallengeList. The single
// in-scope ledger function resolves a challenge id to its entry through the embedded
// list, then forwards to the entry's GetChallengeStyle accessor (already reconstructed
// inline in the committed ChallengeListEntry.h).
//
// MINIMAL OWNING SLICE: the real guest object is a full GUI component (CgsGui base,
// apt-interface state, layout/scroll state, etc. -- all uncommitted). This models ONLY
// the one member the ledger function touches: the pointer to the runtime ChallengeList
// (guest +0x130). FLAG: minimal-slice class -- the GUI base, the vtable and every other
// component member are intentionally OMITTED (uncommitted dependencies, none in scope).
// Offsets are NOT reproduced (host is 64-bit; guest offsets are not load-bearing).

namespace BrnResource
{
    class  ChallengeList;        // committed: SharedClasses/DataLists/ChallengeList.h
    struct ChallengeListEntry;   // committed: SharedClasses/DataLists/ChallengeListEntry.h
}

namespace BrnGui
{

class ChallengeListComponent
{
public:
    // GetChallengeStyle @ 0x8248FB58.
    //   v2 = this->mpChallengeList            (guest lwz r31, 0x130(r3))
    //   if ( v2->GetChallengeIndex(lID) < 0 ) entry = 0
    //   else                                  entry = v2->GetChallengeData(<index>)
    //   return entry->GetChallengeStyle()     (guest bl ChallengeListEntry::GetChallengeStyle)
    //
    // Return is BrnResource::ChallengeListEntry::EFreeburnChallengeStyle; the X360 pseudo
    // types it int and may call the entry accessor on a null entry when the id is not in
    // the list (the asm passes r3 == 0 straight through). Reconstructed faithfully: the
    // not-found path forwards a null entry, exactly as the guest does.
    s32 GetChallengeStyle( CgsID lID ) const;

private:
    // Guest +0x130: the runtime challenge list this component views. Held BY NAME (no
    // raw offset cast). FLAG: only member modelled in this minimal slice.
    BrnResource::ChallengeList* mpChallengeList;
};

} // namespace BrnGui

#endif // BRN_CHALLENGE_LIST_COMPONENT_H
