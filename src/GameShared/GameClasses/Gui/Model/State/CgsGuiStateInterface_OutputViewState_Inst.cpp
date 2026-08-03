// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface_OutputViewState_Inst.cpp
//
// The X360-attested explicit instantiations of
// CgsGui::StateInterface::OutputViewState<T> (the channel-41 "view state" output
// GUI-event publisher). The single shared body lives in CgsGuiStateInterface.h; each
// instantiation compiles to the identical stack-build-a-GuiEventWrapper<T,41> +
// VariableEventQueue<65536,16>::AddEvent(&wrapper, 41, sizeof(wrapper)) sequence,
// differing only in the per-T sizeof / GetEventType() constants that fall out of T.
// Reuses the homed GUI-event payload types (their home is #included below).
//
// GuiEventRenderMainMap's real field-layout home is BrnMainMap.h, whose transitive
// includes (BrnMapManager / CgsGuiComponent / BrnCommonTypes) do not belong in this
// demangled-payload TU -- that one instantiation lives in its own sibling _Inst file
// (CgsGuiStateInterface_OutputViewState_RenderMainMap_Inst.cpp).
//
// Per-instance X360 facts (channel 41; record = header + sizeof(T)):
//   OutputViewState<GuiEventFilterEventIcons>    @0x824C2FA0  id 557 size 4  off12 rec 16
//   OutputViewState<GuiEventNetworkPlayerImage>  @0x82436CF0  id 258 size 8  off12 rec 20
//   OutputViewState<GuiEventSetHoveredEventIcon> @0x824C2EE8  id 559 size 24 off16 rec 40 (8-aligned)
//   OutputViewState<GuiEventSetInspectedEventIcon>@0x82493D98 id 558 size 4  off12 rec 16
//   OutputViewState<GuiEventShowHideBoostBar>    @0x82476B60  id 214 size 1  off12 rec 16
//   OutputViewState<GuiEventShowHideSatNav>      @0x82476DD8  id 213 size 12 off12 rec 24
// ============================================================================

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"

namespace CgsGui
{
    template void StateInterface::OutputViewState<BrnGui::GuiEventFilterEventIcons>(BrnGui::GuiEventFilterEventIcons&);  // 0x824C2FA0
    template void StateInterface::OutputViewState<BrnGui::GuiEventNetworkPlayerImage>(BrnGui::GuiEventNetworkPlayerImage&);  // 0x82436CF0
    template void StateInterface::OutputViewState<BrnGui::GuiEventSetHoveredEventIcon>(BrnGui::GuiEventSetHoveredEventIcon&);  // 0x824C2EE8
    template void StateInterface::OutputViewState<BrnGui::GuiEventSetInspectedEventIcon>(BrnGui::GuiEventSetInspectedEventIcon&);  // 0x82493D98
    template void StateInterface::OutputViewState<BrnGui::GuiEventShowHideBoostBar>(BrnGui::GuiEventShowHideBoostBar&);  // 0x82476B60
    template void StateInterface::OutputViewState<BrnGui::GuiEventShowHideSatNav>(BrnGui::GuiEventShowHideSatNav&);  // 0x82476DD8
}
