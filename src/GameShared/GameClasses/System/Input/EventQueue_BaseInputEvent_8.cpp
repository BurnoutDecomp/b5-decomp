#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::BaseInputEvent, 8>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (8) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::BaseInputEvent, 8>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::BaseInputEvent> methods (bodies inline in CgsBaseEventQueue.h).
//   AddEvent  @ 0x82367E98 (called by PostWorldInputBuffer::Post{Bind,Unbind}Request, InputPads::Update,
//                           GameStateInviteManager::UpdatePrepareForInvite)
//   Append    @ 0x82369B10 (called by GameStateInviteManager::Update)
//   GetEvent  @ 0x823ABCD0 (CONST overload; ledger "Base"; callers Bridge*/Process*)
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::BaseInputEvent>::AddEvent(const CgsInput::InputIO::BaseInputEvent&);
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::BaseInputEvent>::Append(const CgsModule::BaseEventQueue<CgsInput::InputIO::BaseInputEvent>&);
template const CgsInput::InputIO::BaseInputEvent& CgsModule::BaseEventQueue<CgsInput::InputIO::BaseInputEvent>::GetEvent(s32) const;
