#pragma once
// BrnWorld::PropEntityIO::PropToTrafficInterface (DWARF BrnPropToTrafficInterface.h:102) --
// the prop->traffic hand-off interface: two event queues the prop module fills to ask the
// traffic system to knock down / restore traffic lights. NOT an IOBuffer (plain struct).
// The element types (TrafficLightKnockDownEvent / TrafficLightRestoreEvent, both 4-byte) are
// the already-committed structs in BrnPropEntityModuleIO.h -- reused, not redeclared.
// (DWARF homes those events in this header with member muInstanceID + Construct/GetInstanceID;
//  the committed tree instead homes them in BrnPropEntityModuleIO.h as { u32 muPayload } and
//  several EventQueue/BaseEventQueue TUs already reference that home. Reuse it to avoid a
//  duplicate/redefinition; a later slice may reconcile the muPayload->muInstanceID naming.)
#include "types.hpp"                                                              // u32
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // TrafficLightKnockDownEvent/RestoreEvent

namespace BrnWorld
{
namespace PropEntityIO
{
    // DWARF :102. Members :130/:131 (queue order). Capacities from DWARF: 32 / 80.
    struct PropToTrafficInterface
    {
        void Construct();                                    // :111
        // X360 0x822CD378 (this TU): assert luInstanceID != 0, then AddEvent a knock-down.
        void RequestTrafficLightKnockDown(u32 luInstanceID); // :117
        void RequestTrafficLightRestore(u32 luInstanceID);   // :122
        const CgsModule::EventQueue<TrafficLightKnockDownEvent, 32>* GetTrafficLightKnockDownQueue() const; // :125
        const CgsModule::EventQueue<TrafficLightRestoreEvent, 80>*   GetTrafficLightRestoreQueue()   const; // :126

    private:
        CgsModule::EventQueue<TrafficLightKnockDownEvent, 32> mTrafficLightKnockDownQueue;  // :130 (+0x00)
        CgsModule::EventQueue<TrafficLightRestoreEvent, 80>   mTrafficLightRestoreQueue;     // :131
    };
}
}
