// Embed-check for BaseEventQueue<BrnTraffic::BrnTrafficIO::ActivateHullEvent>.
// Embeds the 8-deep fixed-capacity queue, exercises AddEvent / AddEventSafe / Append,
// and reads back via GetEvent so the template bodies and the 12-byte element stride are
// instantiated. Not a unit test -- a compile/link tripwire for this TU's 3 funcs.
#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"

namespace
{
    using ActivateHullEvent = BrnTraffic::BrnTrafficIO::ActivateHullEvent;
    using ActivateHullQueue = CgsModule::EventQueue<ActivateHullEvent, 8>;

    static_assert(sizeof(ActivateHullEvent) == 12, "ActivateHullEvent must be a 12-byte element (X360 stride)");

    int ExerciseActivateHullQueue()
    {
        ActivateHullQueue lQueue;
        lQueue.Construct();

        ActivateHullEvent lEvent;
        lEvent.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
        lEvent.muNewActiveHull      = 7u;
        lEvent.muUpdateFrame        = 1234u;

        lQueue.AddEvent(lEvent);

        lEvent.muNewActiveHull = 9u;
        bool lbStored = lQueue.AddEventSafe(lEvent);

        ActivateHullQueue lOther;
        lOther.Construct();
        lOther.Append(lQueue);

        const ActivateHullEvent& lReadBack = lOther.GetEvent(0);
        return static_cast<int>(lReadBack.muNewActiveHull) + (lbStored ? 1 : 0) + lOther.GetLength();
    }

    volatile int gActivateHullEmbedSink = ExerciseActivateHullQueue();
}
