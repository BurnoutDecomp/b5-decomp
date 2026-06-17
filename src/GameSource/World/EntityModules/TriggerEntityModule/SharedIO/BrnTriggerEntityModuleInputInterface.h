// ===== Owning header: GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h =====
// DWARF home: BrnTriggerEntityModuleInputInterface.h:92.
// Minimal slice: only the InRemoveTriggerEvent element struct (the element type of the
// EventQueue<...,256> the TriggerEntityModuleIO TU instantiates). The sibling Add/LineTest
// events and the TriggerManagementInputInterface / TriggerQueryInputInterface aggregates that
// also live in this header are NOT reconstructed here (declared-only / left for their own TUs).
#pragma once

#include "BrnCommonTypes.h"                                       // u32
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::Event (empty event base)

namespace BrnWorld
{
namespace TriggerEntityModuleIO
{
    // 32-bit trigger handle (DWARF spells it `TriggerId`, home BrnTriggerTypes.h). No
    // committed `TriggerId` typedef exists yet; the X360 element stride is exactly 4 bytes
    // (4 * miLength addressing in AddEvent/Append) and the LightTriggerId==u32 precedent
    // (BrnGameModeParams.h) fixes the width. Provisional minimal slice -- promote to its
    // BrnTriggerTypes.h home when that TU lands.
    typedef u32 TriggerId;

    // BrnTriggerEntityModuleInputInterface.h:92 (DecFIGS DWARF). Element record of the
    // module's "remove trigger" input queue. Derives from the empty CgsModule::Event base
    // (EBO -> sizeof == 4, matching the X360 4-byte element stride). Capacity of the owning
    // queue is KI_TRIGGER_ENTITY_REMOVE_TRIGGER_QUEUE_SIZE == 256.
    struct InRemoveTriggerEvent : public CgsModule::Event
    {
        TriggerId mTriggerID;   // BrnTriggerEntityModuleInputInterface.h:94
    };
}
}
