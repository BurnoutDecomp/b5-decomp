#include "GameSource/Replays/Serialisers/BrnReplayTrafficSerialiserFrame.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"

namespace BrnReplays
{
    TrafficSerialiserFrame* TrafficSerialiserFrame::Clear()
    {
        mVehicleUpdateBits.UnSetAll();
        mPhysicsBits.UnSetAll();
        muLocalPlayerHull = BrnTraffic::KU_INVALID_HULL;
        mActiveHullsForLocalPlayer.Clear();
        return this;
    }
}
