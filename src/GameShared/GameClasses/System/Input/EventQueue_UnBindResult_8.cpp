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
template CgsInput::InputIO::UnBindResult& CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::GetEvent(s32);
template const CgsInput::InputIO::UnBindResult& CgsModule::BaseEventQueue<CgsInput::InputIO::UnBindResult>::GetEvent(s32) const;
