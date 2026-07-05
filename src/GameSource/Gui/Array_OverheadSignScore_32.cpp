#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in
// CgsArray.h) for the BrnGui::OverheadSignScore,32 leaf instantiation -- the committed
// Array_/EventQueue_ explicit-instantiation pattern. The X360 overhead-sign score
// bookkeeping uses this instantiation:
//   Append     @ 0x822AE5D0 -- push one 0x20-byte OverheadSignScore (the `slwi ...,5` element
//                              shift), with the constructed / out-of-space asserts.
//   operator[] @ 0x822AFCC8 -- bounds-checked indexed accessor returning &maElements[i]
//                              (stride 0x20); the non-const overload (mutable element).
//   Grow       @ 0x822ABF70 -- reserve-next-free-slot accessor returning &maElements[miCount],
//                              then post-increment the count.
//   IsFull     @ 0x822C6F10 -- true when the live-element count has reached N (== 32).
template void                       Array<BrnGui::OverheadSignScore, 32>::Append(const BrnGui::OverheadSignScore&);
template BrnGui::OverheadSignScore& Array<BrnGui::OverheadSignScore, 32>::operator[](u32);
template BrnGui::OverheadSignScore* Array<BrnGui::OverheadSignScore, 32>::Grow();
template bool                       Array<BrnGui::OverheadSignScore, 32>::IsFull() const;
