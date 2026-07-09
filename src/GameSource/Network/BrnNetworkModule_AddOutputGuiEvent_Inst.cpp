// ============================================================================
// b5-decomp/src/GameSource/Network/BrnNetworkModule_AddOutputGuiEvent_Inst.cpp
//
// The 15 X360-attested explicit instantiations of
// BrnNetwork::BrnNetworkModule::AddOutputGuiEvent<T>. The single shared body lives in
// BrnNetworkModule.h; each instantiation compiles to the identical assert-updating +
// GetOutputGuiEventQueue()->AddEvent(&event, T::GetEventType(), sizeof(T)) sequence over the
// VariableEventQueue<4096,16> output queue, differing only in the per-T (id,size) constants.
// ============================================================================

#include "GameSource/Network/BrnNetworkModule.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"

namespace BrnNetwork
{
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventCamStatus>(const BrnGui::GuiEventCamStatus&);  // 0x82595788
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkCustomMatchResults>(const BrnGui::GuiEventNetworkCustomMatchResults&);  // 0x82565E88
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkLeavingGameFailed>(const BrnGui::GuiEventNetworkLeavingGameFailed&);  // 0x82565AF0
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkNewsAndTOS>(const BrnGui::GuiEventNetworkNewsAndTOS&);  // 0x82566220
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkPlayerImage>(const BrnGui::GuiEventNetworkPlayerImage&);  // 0x82565DD0
    template int BrnNetworkModule::AddOutputGuiEvent<BrnGui::GuiEventNetworkPlayerStats>(const BrnGui::GuiEventNetworkPlayerStats&);  // 0x82565C60
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEvent<46>>(const CgsGui::GuiEvent<46> &);  // 0x82565F40
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEvent<53>>(const CgsGui::GuiEvent<53> &);  // 0x82565A38
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventAddLocalisedTextPointer>(const CgsGui::GuiEventAddLocalisedTextPointer&);  // 0x825658C8
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkConnected>(const CgsGui::GuiEventNetworkConnected&);  // 0x82565FF8
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkDisconnected>(const CgsGui::GuiEventNetworkDisconnected&);  // 0x825660B0
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkInGame>(const CgsGui::GuiEventNetworkInGame&);  // 0x82565980
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkInGameFailed>(const CgsGui::GuiEventNetworkInGameFailed&);  // 0x82565D18
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventNetworkLaunched>(const CgsGui::GuiEventNetworkLaunched&);  // 0x82565BA8
    template int BrnNetworkModule::AddOutputGuiEvent<CgsGui::GuiEventShowLoginQuestion>(const CgsGui::GuiEventShowLoginQuestion&);  // 0x825956D0
}
