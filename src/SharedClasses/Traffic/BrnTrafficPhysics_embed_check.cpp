// Tiny embed check for the Traffic-Physics group: instantiate/exercise the two
// reconstructed surfaces so the gate compiles the LaneRung layout + GetRightVector
// body and the BaseEventQueue<PropUpdateNotification>::Append generic over the
// 64-byte PropUpdateNotification record. Not part of any shipped TU.
#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

namespace
{

// PropUpdateNotification is block-copied with a 64-byte stride in Append; pin the
// reconstructed record to that size so the generic byte count matches the asm.
static_assert(sizeof(BrnPhysics::Props::PropUpdateNotification) == 64,
              "PropUpdateNotification stride == 64 (Append slwi r,r,6)");

// LaneRung is two 16-byte Vector3 lanes; maPoints[1] sits at +0x10 (lvx128 r4,0x10).
static_assert(sizeof(BrnTraffic::LaneRung) == 32, "LaneRung == 2 * Vector3");

Vector3 EmbedCheckLaneRung(const BrnTraffic::LaneRung& rRung)
{
    return rRung.GetRightVector();
}

bool EmbedCheckPropQueue(
    CgsModule::BaseEventQueue<BrnPhysics::Props::PropUpdateNotification>&       rDst,
    const CgsModule::BaseEventQueue<BrnPhysics::Props::PropUpdateNotification>& rSrc)
{
    return rDst.Append(rSrc);
}

} // namespace
