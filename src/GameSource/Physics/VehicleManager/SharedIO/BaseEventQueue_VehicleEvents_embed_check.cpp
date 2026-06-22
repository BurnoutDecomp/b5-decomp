// Embed-check for the Vehicle-events group's two BaseEventQueue instantiations:
//   BaseEventQueue<BrnPhysics::Vehicle::VehicleAddedForCollisionEvent>
//   BaseEventQueue<BrnPhysics::Vehicle::TrafficCrashedEvent>
// Embeds a fixed-capacity queue per type, exercises AddEvent / AddEventSafe / Append, and
// reads back via GetEvent so the template bodies and the 16-byte element stride are
// instantiated. Not a unit test -- a compile/link tripwire for this group's funcs.
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"

namespace
{
    using VehicleAddedForCollisionEvent = BrnPhysics::Vehicle::VehicleAddedForCollisionEvent;
    using TrafficCrashedEvent           = BrnPhysics::Vehicle::TrafficCrashedEvent;

    static_assert(sizeof(VehicleAddedForCollisionEvent) == 16,
                  "VehicleAddedForCollisionEvent must be a 16-byte element (X360 stride)");
    static_assert(sizeof(TrafficCrashedEvent) == 16,
                  "TrafficCrashedEvent must be a 16-byte element (X360 stride)");

    int ExerciseVehicleAddedForCollisionQueue()
    {
        CgsModule::EventQueue<VehicleAddedForCollisionEvent, 8> lQueue;
        lQueue.Construct();

        VehicleAddedForCollisionEvent lEvent;
        lEvent.mRaceCarVolumeInstanceId.muId = 0x1234u;
        lEvent.mbAdded                        = true;
        lQueue.AddEvent(lEvent);

        lEvent.mbAdded = false;
        bool lbStored = lQueue.AddEventSafe(lEvent);

        CgsModule::EventQueue<VehicleAddedForCollisionEvent, 8> lOther;
        lOther.Construct();
        lOther.Append(lQueue);

        const VehicleAddedForCollisionEvent& lReadBack = lOther.GetEvent(0);
        return (lReadBack.mbAdded ? 1 : 0) + (lbStored ? 1 : 0) + lOther.GetLength();
    }

    int ExerciseTrafficCrashedQueue()
    {
        CgsModule::EventQueue<TrafficCrashedEvent, 20> lQueue;
        lQueue.Construct();

        TrafficCrashedEvent lEvent;
        lEvent.mTrafficVolumeInstanceID.muId = 0x5678u;
        lEvent.mCrasherEntityID.muValue      = 42u;
        lQueue.AddEvent(lEvent);

        lEvent.mCrasherEntityID.muValue = 7u;
        bool lbStored = lQueue.AddEventSafe(lEvent);

        CgsModule::EventQueue<TrafficCrashedEvent, 20> lOther;
        lOther.Construct();
        lOther.Append(lQueue);

        const TrafficCrashedEvent& lReadBack = lOther.GetEvent(0);
        return static_cast<int>(lReadBack.mCrasherEntityID.muValue) + (lbStored ? 1 : 0) + lOther.GetLength();
    }

    volatile int gVehicleAddedForCollisionEmbedSink = ExerciseVehicleAddedForCollisionQueue();
    volatile int gTrafficCrashedEmbedSink           = ExerciseTrafficCrashedQueue();
}
