// BrnWorld::CrashModule -- three out-of-line members the X360 ARTIST build emitted for the world
// crash module (World/CrashModule/BrnCrashModule.cpp). See BrnCrashModule.h for the class banner
// and the X360 member-offset map.
//
// Reconstructed methods (X360 spine authoritative):
//   CrashModule()                                   @ 0x827DE960
//   FindCrashForRaceCar(EActiveRaceCarIndex) const  @ 0x827C6AD8
//   WillTrafficVehicleBeRecycledNextFrame(u16)      @ 0x827BBB10
#include "GameSource/World/CrashModule/BrnCrashModule.h"

#include "GameSource/BurnoutConstants.h"                                   // E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT

namespace BrnWorld
{
    // 0x827DE960. The X360 ctor stores the base ModuleSingleBuffered vtable, constructs the two
    // base RWMutexes (mInputMutex @ +0x10, mOutputMutex @ +0x118), re-points the vtable to the
    // derived CrashModule slot, then stores -1 into the count word of each crash array
    // (mRaceCarCrashes @ +0x2F0, mTrafficCrashes @ +0x7F8) -- i.e. marks both arrays as
    // not-yet-constructed (the "Array used before Construct/Clear" sentinel). The base sub-object
    // init and vtable stores are emitted by the compiler-generated base ctor + vtable setup; the
    // CrashModule ctor body is just the two MarkUnconstructed() stores.
    CrashModule::CrashModule()
    {
        mRaceCarCrashes.MarkUnconstructed();
        mTrafficCrashes.MarkUnconstructed();
    }

    // 0x827C6AD8. Walk every live RaceCarCrash record (mRaceCarCrashes, count @ +0xC0) and return
    // the index of the one whose crashing-race-car volume-instance entity index matches the given
    // active-race-car slot; KU_INVALID_CRASH (-1) when none matches. The GetItem accessor asserts
    // the array was constructed; the per-element read asserts the embedded entity index is a valid
    // active-race-car slot (< E_ACTIVE_RACE_CAR_INDEX_COUNT == 8).
    u32 CrashModule::FindCrashForRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const
    {
        const u32 luCount = mRaceCarCrashes.GetLength();
        for (u32 luCrash = 0; luCrash < luCount; ++luCrash)
        {
            // The X360 inlines RaceCarCrash::GetOwner here: read the crashing-race-car volume
            // instance entity index and assert it is a valid active-race-car slot (< 8).
            const RaceCarCrash& lrCrash = mRaceCarCrashes.GetItem(luCrash);
            if (lrCrash.GetOwner() == static_cast<s32>(leActiveRaceCarIndex))
            {
                return luCrash;
            }
        }

        return KU_INVALID_CRASH;
    }

    // 0x827BBB10. True when the traffic vehicle luVehicle is queued in mRecycledTrafficQueue (the
    // vehicle-manager removed-traffic events mirrored for this frame), i.e. it will be recycled on
    // the next frame. Each queued TrafficRemovedEvent carries the removed vehicle's packed
    // EntityId; the X360 extracts its 14-bit Burnout entity index ((word >> 10) & 0x3FFF) and
    // compares it to luVehicle. Asserts luVehicle is a valid traffic index (< KU_MAX_TOTAL_TRAFFIC
    // == 600 == 0x258).
    bool CrashModule::WillTrafficVehicleBeRecycledNextFrame(u16 luVehicle)
    {
        CGS_ASSERT(luVehicle < 0x258u, "luVehicle < BrnTraffic::KU_MAX_TOTAL_TRAFFIC");

        const s32 liCount = mRecycledTrafficQueue.GetLength();
        for (s32 liEvent = 0; liEvent < liCount; ++liEvent)
        {
            const BrnPhysics::Vehicle::TrafficRemovedEvent& lrEvent = mRecycledTrafficQueue.GetEvent(liEvent);
            const u32 luEventVehicleIndex = (lrEvent.mRemovedVehicleEntityId.muValue >> 10) & 0x3FFFu;
            if (luEventVehicleIndex == static_cast<u32>(luVehicle))
            {
                return true;
            }
        }

        return false;
    }
}
