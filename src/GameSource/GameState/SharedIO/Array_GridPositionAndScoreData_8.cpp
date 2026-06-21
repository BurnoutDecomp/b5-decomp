#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the GridPositionAndScoreData leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::GridPositionAndScoreData, 8>::Append(const BrnGameState::GridPositionAndScoreData&);
// X360 0x8231B270 (class:BrnGameState catch-all TU) = the bounds-checked indexed accessor of this
// same instantiation. Hex-Rays attributes the asserts to CgsArray.h:556/557 -- the CONST operator[]
// overload (the const-vs-nonconst body line map is pinned in CgsArray.h: 538/539 non-const, 556/557
// const). "Array used before Construct/Clear" + "Array index out of bounds" returning &maElements[i]
// (stride 8). The BubbleSort/Shuffle/SetupOnlineStartingGrid callers reach it through a const path.
template const BrnGameState::GridPositionAndScoreData& Array<BrnGameState::GridPositionAndScoreData, 8>::operator[](u32) const;
