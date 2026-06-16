#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameActions.h"   // BrnGameState::GameStateModuleIO::TrophyUnlockAction (element home)

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the TrophyUnlockAction,12 leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::GameStateModuleIO::TrophyUnlockAction, 12>::Append(
    const BrnGameState::GameStateModuleIO::TrophyUnlockAction&);
template void Array<BrnGameState::GameStateModuleIO::TrophyUnlockAction, 12>::Erase(u32);
