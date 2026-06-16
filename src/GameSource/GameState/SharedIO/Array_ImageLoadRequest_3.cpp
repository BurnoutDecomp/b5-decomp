#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the ImageLoadRequest leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::GameStateImageManagerBase::ImageLoadRequest, 3>::Append(const BrnGameState::GameStateImageManagerBase::ImageLoadRequest&);
template void Array<BrnGameState::GameStateImageManagerBase::ImageLoadRequest, 3>::Erase(u32);
