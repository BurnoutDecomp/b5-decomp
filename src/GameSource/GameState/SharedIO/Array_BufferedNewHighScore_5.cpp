#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/SharedIO/BrnGameStateLeafContainers.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the BufferedNewHighScore leaf instantiation -- the committed Array_/EventQueue_ explicit-instantiation pattern.
template void Array<BrnGameState::BufferedNewHighScore, 5>::Append(const BrnGameState::BufferedNewHighScore&);
template void Array<BrnGameState::BufferedNewHighScore, 5>::Erase(u32);
// X360 0x823180C0 (class:BrnGameState catch-all TU, ledger "BrnGameState::BufferedNewHighScor") = the
// bounds-checked indexed accessor of this same instantiation. The X360 body is the "Array used before
// Construct/Clear" (CgsArray.h:556) + "Array index out of bounds" (557) sequence returning
// &maElements[i] (stride 32; the count word lives at owner+160 == 5*32). CONST overload (CgsArray.h
// line map: 538/539 non-const, 556/557 const -- this instance is attributed to 556/557).
template const BrnGameState::BufferedNewHighScore& Array<BrnGameState::BufferedNewHighScore, 5>::operator[](u32) const;
