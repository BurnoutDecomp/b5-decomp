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

    // ⭐ ADDED 2026-08-10 (pre-physics bridge wave). The three DWARF-declared accessors
    // (BrnTrafficNetworkInterfaces.h:88/:92/:95) had no bodies anywhere in the tree; mounting
    // BrnTrafficEntityModuleIO.cpp for the pre-physics OutputBuffer produced three LNK2019s for
    // them, referenced from the already-committed
    // InputBuffer_PreScene::SetTrafficNetworkInputInterface @0x827ACD28. Classic mount-gap find:
    // the consumer was written months ago against an API nobody had defined, and only a LINK
    // could see it.
    // No out-of-line X360 symbol exists for any of the three (name-indexed sweep of all 30,084
    // exports returns only ActivateHull @0x82558AB8 and the InputBuffer_PreScene setter) -- the
    // console inlines them, which is exactly what its caller shows: the setter touches
    // `stw 0, 8(this+0x32A0)` (the queue's miLength, i.e. Clear through the queue this accessor
    // hands back) and `lbz/stb 0x6C` (mbDiverged, right after the 108-byte
    // EventQueue<ActivateHullEvent,8>). The shape is therefore fully determined by the member
    // list; bodied here, in the interface's own home, rather than as header inlines so the
    // ledger keeps one definition site.
    const TrafficNetworkInputInterface::ActivateHullQueue&
    TrafficNetworkInputInterface::GetActivateHullQueue() const
    {
        return mActivateHullQueue;
    }

    void TrafficNetworkInputInterface::SetDiverged(bool lbDiverged)
    {
        mbDiverged = lbDiverged;
    }

    bool TrafficNetworkInputInterface::HasDiverged() const
    {
        return mbDiverged;
    }
}
}
