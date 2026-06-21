#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"   // GameStateModuleIO::GuiInterfaceEvent element home

// Explicit instantiation of the generic CgsModule::BaseEventQueue<T>::GetEvent(s32) const accessor
// (inline in CgsBaseEventQueue.h) for the BaseEventQueue<GuiInterfaceEvent> leaf instantiation --
// the committed Array_/EventQueue_ explicit-instantiation pattern.
//
// X360 0x823AC338 (class:BrnGameState catch-all TU, ledger "BrnGameState::G") = the const indexed
// accessor. The X360 body is the "mpEvents != NULL" (CgsBaseEventQueue.h:272) + "liIndex < GetLength()"
// (274) + "liIndex >= 0" (275) sequence returning &mpEvents[i] (stride 12 == sizeof(GuiInterfaceEvent)).
// Drained by BrnGameModule::TranslateGuiInterfaceToGuiEvents.
template const BrnGameState::GameStateModuleIO::GuiInterfaceEvent&
    CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::GuiInterfaceEvent>::GetEvent(s32) const;
