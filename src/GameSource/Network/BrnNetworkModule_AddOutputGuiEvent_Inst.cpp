// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule_AddOutputGuiEvent_Inst.cpp
//
// The 15 X360-attested explicit instantiations of
// BrnNetwork::BrnNetworkModule::AddOutputGuiEvent<T>. The single shared body lives in
// BrnNetworkModule.h; each instantiation compiles to the identical assert-updating +
// GetOutputGuiEventQueue()->AddGuiEvent(event) sequence over the GuiEventQueueBase<4096,16>
// output queue, differing only in the per-T (id,size) constants.
// ============================================================================

#include "GameSource/Network/BrnNetworkModule.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"

namespace BrnNetwork
{
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventCamStatus>(BrnGui::GuiEventCamStatus&);  // 0x82595788
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkCustomMatchResults>(BrnGui::GuiEventNetworkCustomMatchResults&);  // 0x82565E88
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkLeavingGameFailed>(BrnGui::GuiEventNetworkLeavingGameFailed&);  // 0x82565AF0
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkNewsAndTOS>(BrnGui::GuiEventNetworkNewsAndTOS&);  // 0x82566220
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkPlayerImage>(BrnGui::GuiEventNetworkPlayerImage&);  // 0x82565DD0
    template void BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkPlayerStats>(BrnGui::GuiEventNetworkPlayerStats&);  // 0x82565C60
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEvent<46>>(CgsGui::GuiEvent<46> &);  // 0x82565F40
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEvent<53>>(CgsGui::GuiEvent<53> &);  // 0x82565A38
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventAddLocalisedTextPointer>(CgsGui::GuiEventAddLocalisedTextPointer&);  // 0x825658C8
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkConnected>(CgsGui::GuiEventNetworkConnected&);  // 0x82565FF8
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkDisconnected>(CgsGui::GuiEventNetworkDisconnected&);  // 0x825660B0
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkInGame>(CgsGui::GuiEventNetworkInGame&);  // 0x82565980
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkInGameFailed>(CgsGui::GuiEventNetworkInGameFailed&);  // 0x82565D18
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkLaunched>(CgsGui::GuiEventNetworkLaunched&);  // 0x82565BA8
    template void BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventShowLoginQuestion>(CgsGui::GuiEventShowLoginQuestion&);  // 0x825956D0
}
