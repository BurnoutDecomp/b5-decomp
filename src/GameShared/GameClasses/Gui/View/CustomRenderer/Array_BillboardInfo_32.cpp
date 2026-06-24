#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsGuiBillboardInfo.h"

// Explicit instantiation(s) of the generic Array<T,N> container methods (inline in CgsArray.h)
// for the CgsGui::BillboardInfo,32 leaf instantiation -- the committed Array_/EventQueue_
// explicit-instantiation pattern. The X360 BrnGui::BoostBarRenderer render paths use this
// instantiation:
//   Append    @ 0x82448D48 -- push one 64-byte BillboardInfo (the 8-qword std copy loop), with
//                             the Construct/Clear-not-called and out-of-space (Length/Capacity)
//                             asserts (CgsArray.h:225/226).
//   GetItem   @ 0x82449550 -- bounds-checked indexed accessor returning &maElements[i]
//                             (`(i<<6)+this`, stride 64); the non-const overload (mutable element).
//                             Construct-not-called + index-out-of-bounds asserts (CgsArray.h:556/557).
//   GetLength @ 0x82448E80 -- the checked live-element count (Construct-not-called assert,
//                             CgsArray.h:336), reading miCount @ +0x800 (== 32 * 64).
template void                Array<CgsGui::BillboardInfo, 32>::Append(const CgsGui::BillboardInfo&);
template CgsGui::BillboardInfo& Array<CgsGui::BillboardInfo, 32>::GetItem(u32);
template u32                 Array<CgsGui::BillboardInfo, 32>::GetLength() const;
