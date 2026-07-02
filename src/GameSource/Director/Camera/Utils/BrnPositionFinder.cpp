#include "GameSource/Director/Camera/Utils/BrnPositionFinder.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"        // BehaviourSharedInfo (accessor slice)
#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"             // BrnDirector::WorldMap

// BrnDirector::Camera::Utils::PositionFinder -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Director/Camera/Utils/BrnPositionFinder.cpp):
//   PositionFinder::FindPosition @0x821F8E68
//   PositionFinder::Update       @0x8223FCD8
// Both are called by BrnDirector::Camera::BehaviourBystanderCam::Update.

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// @ 0x821F8E68
void PositionFinder::FindPosition(Vector3 lTarget, Vector3 lDisplacement)
{
    // Non-gating tripwire (cpp:75).
    CGS_ASSERT(mbConstructed, "mbConstructed");

    mTarget          = lTarget;
    mDisplacement    = lDisplacement;
    mbIsInitialised  = true;
    mbFoundPosition  = false;
}

// @ 0x8223FCD8
void PositionFinder::Update(const BehaviourSharedInfo& lrSharedInfo)
{
    // Non-gating tripwire (cpp:90).
    CGS_ASSERT(mbConstructed, "mbConstructed");

    if (mbIsInitialised && !mbFoundPosition)
    {
        // The world map lives in the per-frame shared info (X360 sharedInfo+0x5DC);
        // the safe-position query writes straight into mPosition (the X360 passes
        // &mPosition == this+0 in r4).
        mbFoundPosition = lrSharedInfo.GetWorldMap()->GetSafePositionNearestPointWithDisplacement(
            mTarget, mDisplacement, mPosition);
    }
}

}
}
}
