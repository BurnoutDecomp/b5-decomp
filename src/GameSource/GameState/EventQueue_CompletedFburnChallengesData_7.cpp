#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"

// CgsModule::EventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData, 7>::Construct  @ 0x82373B50
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Fixed-capacity (7) event queue
// instantiation: points the base queue at its inline maEvents buffer, sets the
// max length, and clears the live count.
template void CgsModule::EventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData, 7>::Construct();

// CgsModule::BaseEventQueue<...CompletedFburnChallengesData>::AddEvent  @ 0x8258C160
// CgsModule::BaseEventQueue<...CompletedFburnChallengesData>::Append    @ 0x823C46A8
// Explicit instantiations only -- the template bodies live inline in the committed
// CgsBaseEventQueue.h. Both bodies bake a 264-byte element stride (memcpy/XMemCpy of
// 264 * count), which == sizeof(CompletedFburnChallengesData) now that the element
// struct is corrected to the DWARF s32 + FastBitArray<2000> (264B) layout.
template bool CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData>::AddEvent(const BrnGameState::GameStateModuleIO::CompletedFburnChallengesData&);
template bool CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData>::Append(const CgsModule::BaseEventQueue<BrnGameState::GameStateModuleIO::CompletedFburnChallengesData>&);
