#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the ChainableMultiplierInfo leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template BrnGameState::GameStateModuleIO::ChainableMultiplierInfo* Array<BrnGameState::GameStateModuleIO::ChainableMultiplierInfo, 8>::QSort(int (*)(const void*, const void*));
