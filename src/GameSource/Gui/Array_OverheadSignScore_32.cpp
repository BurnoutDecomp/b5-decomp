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
//   AppendArray<32> @ 0x822E5348 -- append every live element of a same-instantiation source
//                              Array onto this one (BrnWorld::PropEntityModule::PreSceneUpdate,
//                              WorldModule::BridgePropToOutput_PreScene). The X360 body carries
//                              the constructed asserts (this @ CgsArray.h:245, source GetLength
//                              @ :336), the combined out-of-space assert (:246, "Array container
//                              out of space appending an array"), then Appends source[0..len) in
//                              order -- matching the committed generic AppendArray body. miCount
//                              sits at +0x400 (32 * 0x20-byte elements), matching the asm's
//                              `lwz r11,0x400(rN)` count reads.
// Per-function (not `template class`): OverheadSignScore has no operator==, so a whole-class
// instantiation would force the equality-based members (Contains/FindFirstInstanceOf/
// CountInstancesOf/EraseInstancesOf) which do not compile for this element type.
template void                       Array<BrnGui::OverheadSignScore, 32>::Append(const BrnGui::OverheadSignScore&);
template BrnGui::OverheadSignScore& Array<BrnGui::OverheadSignScore, 32>::operator[](u32);
template BrnGui::OverheadSignScore* Array<BrnGui::OverheadSignScore, 32>::Grow();
template bool                       Array<BrnGui::OverheadSignScore, 32>::IsFull() const;
template void                       Array<BrnGui::OverheadSignScore, 32>::AppendArray(const Array<BrnGui::OverheadSignScore, 32>&);
