#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnOpponentData.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the OpponentData leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template const BrnGameState::OpponentData& Array<BrnGameState::OpponentData, 7>::GetItem(u32) const;
template void Array<BrnGameState::OpponentData, 7>::Append(const BrnGameState::OpponentData&);
