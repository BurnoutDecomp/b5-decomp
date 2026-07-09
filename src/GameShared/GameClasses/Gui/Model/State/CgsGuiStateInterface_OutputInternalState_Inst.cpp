// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface_OutputInternalState_Inst.cpp
//
// The X360-attested explicit instantiations of
// CgsGui::StateInterface::OutputInternalState<T> (the channel-42 "internal state" output
// GUI-event publisher). The single shared body lives in CgsGuiStateInterface.h; each
// instantiation compiles to the identical stack-build-a-GuiEventWrapper<T,42> +
// VariableEventQueue<65536,16>::AddEvent(&wrapper, 42, sizeof(wrapper)) sequence,
// differing only in the per-T sizeof / GetEventType() constants that fall out of T.
// Reuses the homed GUI-event payload types (their home is #included below).
//
// Per-instance X360 facts (channel 42; record = 12B header + sizeof(T), 4-aligned unless noted):
//   OutputInternalState<GuiEventPerformOnlineMainMenuOption> @0x82436A30  id 284 size 4  rec 16
//   OutputInternalState<GuiEventPerformOnlinePauseOption>    @0x82436A80  id 283 size 4  rec 16
//   OutputInternalState<GuiEventSetInspectedEventIcon>       @0x82493DE8  id 558 size 4  rec 16
//   OutputInternalState<GuiEventShowHideHud>                 @0x82493C98  id 148 size 1  rec 16
//   OutputInternalState<GuiEventShowHideSatNav>              @0x82476E38  id 213 size 12 rec 24
// ============================================================================

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"

namespace CgsGui
{
    template int StateInterface::OutputInternalState<BrnGui::GuiEventPerformOnlineMainMenuOption>(BrnGui::GuiEventPerformOnlineMainMenuOption&);  // 0x82436A30
    template int StateInterface::OutputInternalState<BrnGui::GuiEventPerformOnlinePauseOption>(BrnGui::GuiEventPerformOnlinePauseOption&);  // 0x82436A80
    template int StateInterface::OutputInternalState<BrnGui::GuiEventSetInspectedEventIcon>(BrnGui::GuiEventSetInspectedEventIcon&);  // 0x82493DE8
    template int StateInterface::OutputInternalState<BrnGui::GuiEventShowHideHud>(BrnGui::GuiEventShowHideHud&);  // 0x82493C98
    template int StateInterface::OutputInternalState<BrnGui::GuiEventShowHideSatNav>(BrnGui::GuiEventShowHideSatNav&);  // 0x82476E38
}
