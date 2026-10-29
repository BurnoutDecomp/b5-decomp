#include "GameSource/Replays/Serialisers/BrnReplayTrafficSerialiserFrame.h"

namespace BrnReplays
{
    // ARTIST 0x826533D8: eleven zero qwords at +0xA0..+0xF0, hull -1, count zero.
    // The asset count/ids and hull element bytes are deliberately retained.
    TrafficSerialiserFrame* TrafficSerialiserFrame::Clear()
    {
        mVehicleUpdateBits.UnSetAll();
        mPhysicsBits.UnSetAll();
        muLocalPlayerHull = 0xFFFF;
        mHeader.Clear();
        return this;
    }
}
