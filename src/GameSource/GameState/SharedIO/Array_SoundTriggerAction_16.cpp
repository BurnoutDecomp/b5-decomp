#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameActions.h"   // BrnGameState::GameStateModuleIO::SoundTriggerAction (element home)

// Explicit instantiation of the generic Array<T,N> container method (inline in CgsArray.h)
// for the SoundTriggerAction,16 leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
// (Resolves the batch-2 16-of-32 deferral: the generic Append copies the FULL 32-byte element.)
template void Array<BrnGameState::GameStateModuleIO::SoundTriggerAction, 16>::Append(
    const BrnGameState::GameStateModuleIO::SoundTriggerAction&);
