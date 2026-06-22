// Compile-only embedder check for the BrnGui::ChallengeListComponent TU.
// Not part of the shipping build: it embeds a ChallengeListComponent by value (forcing the
// minimal-slice layout to instantiate) and exercises the reconstructed accessor by name the
// way the OnlineGameRoomPlayerInfo challenges sub-state caller invokes it.
#include "GameSource/Gui/BrnChallengeListComponent.h"
#include "BrnCommonTypes.h"   // CgsID

namespace BrnGui
{

struct ChallengeListComponentEmbedderCheck
{
    ChallengeListComponent mComponent;
};

s32 ChallengeListComponentEmbedderCheck_Exercise(
    const ChallengeListComponentEmbedderCheck& lrEmbedder, CgsID lID )
{
    // id -> challenge style (resolves through the embedded ChallengeList).
    return lrEmbedder.mComponent.GetChallengeStyle( lID );
}

} // namespace BrnGui
