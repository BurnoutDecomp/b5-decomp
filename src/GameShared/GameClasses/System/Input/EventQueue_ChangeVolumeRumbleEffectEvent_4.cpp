#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::ChangeVolumeRumbleEffectEvent, 4>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (4) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::ChangeVolumeRumbleEffectEvent, 4>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::ChangeVolumeRumbleEffectEvent> methods (bodies inline in CgsBaseEventQueue.h).
//   AddEvent  @ 0x82367C00 (called by BrnGameState::RumbleManager::UpdateSurfaceRumble /
//                           PreWorldInputBuffer::PostChangeVolumeRumbleEffectByPlayer)
template bool CgsModule::BaseEventQueue<CgsInput::InputIO::ChangeVolumeRumbleEffectEvent>::AddEvent(const CgsInput::InputIO::ChangeVolumeRumbleEffectEvent&);
