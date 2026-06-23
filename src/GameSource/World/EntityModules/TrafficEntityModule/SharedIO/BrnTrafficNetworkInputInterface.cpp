#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // BrnTraffic::KU_INVALID_HULL
#include "GameShared/GameClasses/Core/CgsAssert.h"                                    // CGS_ASSERT

// BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface::ActivateHull @ 0x82558AB8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Validates the incoming hull-activation request
// (three non-gating asserts, verbatim X360 strings), packs an ActivateHullEvent on the stack and
// AddEventSafe's it onto the embedded queue. The X360 builds the 12-byte event store-for-store:
//   v9  = leActiveRaceCarIndex  (s32 @ +0)
//   v10 = (u16)luNewActiveHull  (sth   @ +4)
//   v11 = luUpdateFrame         (s32 @ +8)
// then calls AddEventSafe(&mActivateHullQueue, &event). The interface's `this` IS the embedded
// queue (mActivateHullQueue at offset 0), so passing `&mActivateHullQueue` matches the X360.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    void TrafficNetworkInputInterface::ActivateHull(
        EActiveRaceCarIndex leActiveRaceCarIndex, u32 luNewActiveHull, u32 luUpdateFrame)
    {
        // BrnTrafficNetworkInterfaces.h:218/219/220 (non-gating tripwires).
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(luNewActiveHull < BrnTraffic::KU_INVALID_HULL,
                   "luNewActiveHull < BrnTraffic::KU_INVALID_HULL");

        ActivateHullEvent lEvent;
        lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;
        lEvent.muNewActiveHull      = static_cast<u16>(luNewActiveHull);
        lEvent.muUpdateFrame        = luUpdateFrame;

        mActivateHullQueue.AddEventSafe(lEvent);
    }
}
}
