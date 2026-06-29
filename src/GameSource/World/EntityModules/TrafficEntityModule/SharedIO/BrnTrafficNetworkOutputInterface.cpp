#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"  // BrnTraffic::KU_MAX_HULLS, KU_MAX_HULLS_IN_PVS
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"  // BrnTraffic::KU_INVALID_HULL
#include "GameShared/GameClasses/Core/CgsAssert.h"            // CGS_ASSERT
#include "GameSource/BurnoutConstants.h"                      // E_ACTIVE_RACE_CAR_INDEX_COUNT (== KU_MAX_HULLS_IN_PVS == 8)

// BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The local sim fills this each frame so the network
// layer can publish the traffic snapshot to peers. The interface's `this` IS the embedded
// ActivateHull event queue (mActivateHullQueue at offset 0), so ActivateHull forwards `this`
// straight to the queue's AddEvent. Byte offsets confirmed against the X360 stores/loads:
//   queue @0x00 .. table mau16ActiveHulls[8] @0x6C .. mbHashValid @0x7C ..
//   mbActiveHullsValid @0x7D .. mbHullSyncDivergence @0x7E .. muHash(u16) @0x80 ..
//   muHashUpdateFrame(u32) @0x84.

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // X360 0x82592830 (EXECUTED in the boot-trace milestone). Constructs the embedded event queue,
    // clears the two "valid" tripwire flags and the hull-sync divergence flag, and seeds the
    // per-active-race-car active-hull table to KU_INVALID_HULL (the X360 stores 0xFFFF as a
    // halfword into all KU_MAX_HULLS_IN_PVS == 8 entries).
    void TrafficNetworkOutputInterface::Construct()
    {
        mActivateHullQueue.Construct();

        mbHashValid        = false;   // 0x7C
        mbActiveHullsValid = false;   // 0x7D

        for (u32 luIndex = 0; luIndex < KU_MAX_HULLS_IN_PVS; ++luIndex)
        {
            mau16ActiveHulls[luIndex] = KU_INVALID_HULL;   // 0x6C + 2*luIndex, 0xFFFF
            // The X360 walks this table with the inlined operator++(EActiveRaceCarIndex)
            // (BurnoutConstants.h:39), which fires this non-gating per-iteration assert.
            CGS_ASSERT(luIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        mbHullSyncDivergence = false;   // 0x7E
    }

    // X360 0x8271D238. Validates the request (three non-gating tripwire asserts, verbatim X360
    // strings) then packs an ActivateHullEvent on the stack (s32 race-car index, u16 hull, u32
    // frame) and AddEvent's it onto the embedded queue. Unlike the INPUT interface (AddEventSafe),
    // the output interface appends unconditionally via AddEvent.
    void TrafficNetworkOutputInterface::ActivateHull(
        EActiveRaceCarIndex leActiveRaceCarIndex, u32 luNewActiveHull, u32 luUpdateFrame)
    {
        // BrnTrafficNetworkInterfaces.h:310/311/312 (non-gating).
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        CGS_ASSERT(luNewActiveHull < KU_MAX_HULLS,
                   "luNewActiveHull < BrnTraffic::KU_MAX_HULLS");

        ActivateHullEvent lEvent;
        lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;
        lEvent.muNewActiveHull      = static_cast<u16>(luNewActiveHull);
        lEvent.muUpdateFrame        = luUpdateFrame;

        mActivateHullQueue.AddEvent(lEvent);
    }

    // X360 0x82542438. Asserts the hash has been published this frame (mbHashValid, non-gating)
    // then returns it. The X360 reads muHash with a halfword load (lhz), matching the u16 type.
    u16 TrafficNetworkOutputInterface::GetDataHash() const
    {
        CGS_ASSERT(mbHashValid, "mbHashValid");   // BrnTrafficNetworkInterfaces.h:407
        return muHash;
    }

    // X360 0x82542490. Asserts the hash has been published this frame (mbHashValid, non-gating)
    // then returns the frame number the hash was taken on (muHashUpdateFrame, word load).
    u32 TrafficNetworkOutputInterface::GetUpdateFrameForHash() const
    {
        CGS_ASSERT(mbHashValid, "mbHashValid");   // BrnTrafficNetworkInterfaces.h:421
        return muHashUpdateFrame;
    }

    // X360 0x825424E8. Asserts the active-hull table has been filled this frame
    // (mbActiveHullsValid, non-gating) then returns a pointer to the table (this + 0x6C).
    const u16* TrafficNetworkOutputInterface::GetActiveHulls() const
    {
        CGS_ASSERT(mbActiveHullsValid, "mbActiveHullsValid");   // BrnTrafficNetworkInterfaces.h:434
        return mau16ActiveHulls;
    }
}
}
