#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::UnBindResult, 8>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::UnBindResult, 8>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult> methods (bodies inline in CgsBaseEventQueue.h).
//   GetEvent  @ 0x8235D158 (NON-const overload; ledger "UnBindResult"; caller GameStateInviteManager::ProcessUnbindResults)
//   GetEvent  @ 0x823ABE28 (CONST overload;     ledger "UnBind";       caller BrnGameModule::BridgeInputToGame)
//   AddEvent  @ 0x828EAAB8 (caller CgsInput::InputModule::ProcessUnbindRequestQueue)
//   Append    @ 0x82369CE0 (callers BrnGameState::GameStateModule::PreWorldUpdate,
//                           BrnGame::BrnGameModule::BridgeControllerToGameState,
//                           CgsInput::InputModule::PreWorldUpdate)
template CgsInput::InputIO::UnBindResult& CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::GetEvent(s32);
template const CgsInput::InputIO::UnBindResult& CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::GetEvent(s32) const;
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::AddEvent(const CgsInput::InputIO::UnBindResult&);
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::Append(const CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>&);
