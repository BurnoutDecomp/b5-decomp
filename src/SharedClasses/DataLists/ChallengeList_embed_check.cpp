// Compile-only embedder check for the BrnResource::ChallengeList TU.
// Not part of the shipping build: it embeds a ChallengeList by value (forcing the full
// declared layout -- maStaticDataLists[32] of ResourcePtr<ChallengeListResource>, the
// maSlots table and the two counts -- to instantiate) and exercises each reconstructed
// accessor by name, the way the GUI / challenge-manager callers invoke them.
#include "SharedClasses/DataLists/ChallengeList.h"
#include "SharedClasses/DataLists/ChallengeListResourceType.h"   // ChallengeListResource complete
#include "SharedClasses/DataLists/ChallengeListEntry.h"          // ChallengeListEntry complete
#include "BrnCommonTypes.h"                                       // CgsID

namespace BrnResource
{

struct ChallengeListEmbedderCheck
{
    ChallengeList mChallengeList;
};

void ChallengeListEmbedderCheck_Exercise( ChallengeListEmbedderCheck& lrEmbedder, CgsID lID )
{
    const ChallengeList& lrList = lrEmbedder.mChallengeList;

    // index -> entry pointer (range-guarded resolve through the list ResourcePtr).
    (void)lrList.GetChallengeData( 0 );

    // id -> index linear scan.
    (void)lrList.GetChallengeIndex( lID );

    // index -> DLC entitlement gate.
    (void)lrList.IsChallengeContentBought( 0 );
}

} // namespace BrnResource
