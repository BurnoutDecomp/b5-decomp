// Compile-only embedder check for BrnProgression::ProgressionData's lookup/relocation methods.
// Not part of the shipping build: it embeds a ProgressionData by value and drives the four
// methods bodied in BrnProgressionData.cpp by this batch (FindRivalIndexFromId / FindRival /
// FindCarOpponentSet / FixDown) the way ModeManager::SetupOpponentData, the progression manager
// and the resource-type FixDown forwarder do.
#include "SharedClasses/Progression/BrnProgressionData.h"
#include "SharedClasses/Progression/BrnRival.h"          // complete Rival (rival lookups)
#include "SharedClasses/Progression/BrnOpponentData.h"   // complete CarOpponentSet (FindCarOpponentSet)
#include "BrnCommonTypes.h"                              // CgsID

namespace BrnProgression
{

// Element strides the lookups/relocation depend on (X360-faithful; CgsID is 8-byte aligned).
static_assert(sizeof(Rival) == 56, "Rival is a 56-byte record");
static_assert(sizeof(CarOpponentSet) == 144, "CarOpponentSet is a 144-byte record");

struct ProgressionDataEmbedderCheck
{
    ProgressionData mData;
};

void ProgressionDataEmbedderCheck_Exercise(ProgressionDataEmbedderCheck& lrEmbedder,
                                           CgsID lRivalId, CgsID lCarModelId, s32 liRank, int liDelta)
{
    const ProgressionData& lrData = lrEmbedder.mData;

    const s32 liRivalIndex = lrData.FindRivalIndexFromId(lRivalId);
    (void)liRivalIndex;

    const Rival* lpRival = lrData.FindRival(lRivalId);
    (void)lpRival;

    CarOpponentSet* lpSet = lrData.FindCarOpponentSet(lCarModelId, liRank);
    (void)lpSet;

    const int liResult = lrEmbedder.mData.FixDown(liDelta);
    (void)liResult;
}

}
