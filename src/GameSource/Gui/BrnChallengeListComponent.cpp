// BrnChallengeListComponent.cpp
// BrnGui::ChallengeListComponent -- GUI list view over a runtime BrnResource::ChallengeList.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ChallengeListComponent::GetChallengeStyle  @ 0x8248FB58
//
// The single in-scope function resolves a challenge id to its entry through the embedded
// list and forwards to the entry's style accessor. Call signatures settled from the
// call-site register setup (Hex-Rays dropped the index arg of GetChallengeData -- the
// asm leaves the GetChallengeIndex result live in r4 across the branch):
//
//   0x8248FB68  lwz r31, 0x130(r3)              ; v2 = this->mpChallengeList
//   0x8248FB6C  mr  r3, r31
//   0x8248FB70  bl  ChallengeList::GetChallengeIndex   ; (v2, r4 == lID carried from entry)
//   0x8248FB74  mr  r4, r3                       ; r4 = index
//   0x8248FB78  cmpwi cr6, r4, 0
//   0x8248FB7C  blt cr6, loc_8248FB8C            ; index < 0 -> entry = 0
//   0x8248FB80  mr  r3, r31
//   0x8248FB84  bl  ChallengeList::GetChallengeData    ; (v2, r4 == index)
//   0x8248FB88  b   loc_8248FB90
//   0x8248FB8C  li  r3, 0                         ; entry = 0
//   0x8248FB90  bl  ChallengeListEntry::GetChallengeStyle   ; (entry)  -- @0x823542A0
//
// GetChallengeData's committed signature is GetChallengeData(s32 liIndex) const, matching
// the index in r4.  GetChallengeStyle is an inline accessor in the committed entry header.

#include "GameSource/Gui/BrnChallengeListComponent.h"
#include "SharedClasses/DataLists/ChallengeList.h"          // BrnResource::ChallengeList (complete)
#include "SharedClasses/DataLists/ChallengeListEntry.h"     // BrnResource::ChallengeListEntry (complete)

namespace BrnGui
{

s32 ChallengeListComponent::GetChallengeStyle( CgsID lID ) const
{
    const BrnResource::ChallengeList* lpList = mpChallengeList;

    const s32 liIndex = lpList->GetChallengeIndex( lID );

    const BrnResource::ChallengeListEntry* lpEntry;
    if ( liIndex < 0 )
    {
        lpEntry = 0;
    }
    else
    {
        lpEntry = lpList->GetChallengeData( liIndex );
    }

    return static_cast<s32>( lpEntry->GetChallengeStyle() );
}

} // namespace BrnGui
