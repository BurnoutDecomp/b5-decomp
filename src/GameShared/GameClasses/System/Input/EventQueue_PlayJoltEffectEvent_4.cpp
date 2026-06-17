#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h"

// CgsModule::EventQueue<CgsInput::InputIO::PlayJoltEffectEvent, 4>::Construct
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (4) event-queue
// instantiation: points the base queue at its inline maEvents buffer, sets the max
// length, and clears the live count.
template void CgsModule::EventQueue<CgsInput::InputIO::PlayJoltEffectEvent, 4>::Construct();

// CgsModule::BaseEventQueue<CgsInput::InputIO::PlayJoltEffectEvent> methods (bodies inline in CgsBaseEventQueue.h).
//   GetEvent  @ 0x8235CF50 (NON-const overload; ledger "PlayJ"; caller BrnGameState::RumbleManager::BridgeRumbleToInput)
template CgsInput::InputIO::PlayJoltEffectEvent& CgsModule::BaseEventQueue<CgsInput::InputIO::PlayJoltEffectEvent>::GetEvent(s32);
