#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h" // BrnTraffic::KU_INVALID_HULL
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                           // CGS_ASSERT + MessageAt

// BrnTraffic::BrnTrafficIO::TrafficNetworkInputInterface::ActivateHull @ 0x82558AB8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Validates the incoming hull-activation request
// (three non-gating asserts, verbatim X360 strings), packs an ActivateHullEvent on the stack and
// AddEventSafe's it onto the embedded queue. The X360 builds the 12-byte event store-for-store:
//   v9  = leActiveRaceCarIndex  (s32 @ +0)
//   v10 = (u16)luNewActiveHull  (sth   @ +4)
//   v11 = luUpdateFrame         (s32 @ +8)
// then calls AddEventSafe(&mActivateHullQueue, &event). The interface's `this` IS the embedded
// queue (mActivateHullQueue at offset 0), so passing `&mActivateHullQueue` matches the X360.

static const char KAC_ARTIST_ASSERT_FILE[] =
    "..\\..\\..\\GameSource\\World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h";

namespace BrnTraffic
{
namespace BrnTrafficIO
{
    // DWARF declares ActivateHull at BrnTrafficNetworkInterfaces.h:83 and ARTIST bakes the body's
    // header lines 218/219/220 into its asserts (li r5 at 0x82558AEC/0x82558B10/0x82558B34), so
    // the original defined it inline in the header; the asserts carry that file and those lines.
    void TrafficNetworkInputInterface::ActivateHull(
        EActiveRaceCarIndex leActiveRaceCarIndex, u32 luNewActiveHull, u32 luUpdateFrame)
    {
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   CGS_ASSERT_MESSAGE_AT("leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", KAC_ARTIST_ASSERT_FILE, 218));
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   CGS_ASSERT_MESSAGE_AT("leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", KAC_ARTIST_ASSERT_FILE, 219));
        CGS_ASSERT(luNewActiveHull < BrnTraffic::KU_INVALID_HULL,
                   CGS_ASSERT_MESSAGE_AT("luNewActiveHull < BrnTraffic::KU_INVALID_HULL", KAC_ARTIST_ASSERT_FILE, 220));

        ActivateHullEvent lEvent;
        lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;
        lEvent.muNewActiveHull      = static_cast<u16>(luNewActiveHull);
        lEvent.muUpdateFrame        = luUpdateFrame;

        mActivateHullQueue.AddEventSafe(lEvent);
    }

    // GetActivateHullQueue / SetDiverged / HasDiverged (DWARF h:88/:92/:95) are header inline:
    // ARTIST has no out-of-line copy and InputBuffer_PreScene::SetTrafficNetworkInputInterface
    // 0x827ACD28 inlines the queue and mbDiverged (+0x6C) accesses.
    // No out-of-line console copy: the network module's Construct and the network output buffer's
    // Construct both inline it as the queue's Construct followed by a zero byte store just past the
    // queue (mbDiverged).
    void TrafficNetworkInputInterface::Construct()
    {
        mActivateHullQueue.Construct();
        mbDiverged = false;
    }
}
}
