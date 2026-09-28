#pragma once
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerQueryId.h"
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h"

namespace BrnWorld { namespace TriggerEntityModuleIO {
// DWARF h:62..68. ARTIST82386D80..82386D88:8-byte prefix, followed by
// miNumTriggers packed handles. ProcessLineTestFineResult822D9FF8 writes it.
struct OutLineTestResultEvent : public CgsModule::Event
{
    BrnWorld::TriggerQueryId mQueryID;
    s32 miNumTriggers;
    const TriggerId* GetTriggerIds() const { return reinterpret_cast<const TriggerId*>(this + 1); }
};
static_assert(sizeof(OutLineTestResultEvent) == 8, "trigger result prefix");
}}
