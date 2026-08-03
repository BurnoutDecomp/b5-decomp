// ============================================================================
// b5-decomp/src/GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface_OutputViewState_RenderMainMap_Inst.cpp
//
// The single X360-attested instantiation
//   CgsGui::StateInterface::OutputViewState<BrnGui::GuiEventRenderMainMap> @0x82465E50
// (channel 41; id 223, sizeof(T)==48 on the console, payload 8-aligned so off16 rec 64).
//
// Isolated in its own _Inst TU because GuiEventRenderMainMap's canonical field-layout home is
// BrnMainMap.h -- whose transitive includes (BrnMapManager / CgsGuiComponent / BrnCommonTypes)
// pull in the whole sat-nav component and must not be co-included with the flat
// BrnGuiDemangledEventTypes.h payload homes the other OutputViewState instantiations use.
// The shared template body lives in CgsGuiStateInterface.h; the per-T size/id/offset constants
// fall out of the real BrnGui::GuiEventRenderMainMap type.
// ============================================================================

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameSource/Gui/SatNav/BrnMainMap.h"   // BrnGui::GuiEventRenderMainMap (real field-layout home)

namespace CgsGui
{
    template void StateInterface::OutputViewState<BrnGui::GuiEventRenderMainMap>(BrnGui::GuiEventRenderMainMap&);  // 0x82465E50
}
