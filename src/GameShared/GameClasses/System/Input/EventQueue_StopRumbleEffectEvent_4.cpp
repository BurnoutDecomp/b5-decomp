#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::StopRumbleEffectEvent, 4>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (4) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::StopRumbleEffectEvent, 4>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::StopRumbleEffectEvent> methods (bodies inline in CgsBaseEventQueue.h).
//   AddEvent  @ 0x82367D40 (called by BrnGameState::RumbleManager::UpdateSurfaceRumble /
//                           PreWorldInputBuffer::PostStopRumbleEffectByPlayer)
//   GetEvent  @ 0x8235CFF8 (NON-const overload; ledger "Sto"; caller BrnGameState::RumbleManager::BridgeRumbleToInput)
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::StopRumbleEffectEvent>::AddEvent(const CgsInput::InputIO::StopRumbleEffectEvent&);
template CgsInput::InputIO::StopRumbleEffectEvent& CgsModule::BaseEventQueue<CgsInput::InputIO::StopRumbleEffectEvent>::GetEvent(s32);
