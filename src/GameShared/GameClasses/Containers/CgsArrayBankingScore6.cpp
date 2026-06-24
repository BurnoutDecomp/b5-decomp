// Per-instantiation .cpp for Array<BrnGui::BankingScore, 6>. The generic Array<T,N> body
// (Append / EraseFast / GetItem / IsFull + siblings) is fully inline in CgsArray.h, so this
// TU is just the explicit class instantiation (the X360 emits one out-of-line copy per
// using-TU). The four methods the X360 emitted for this instance:
//   Array<BankingScore,6>::Append    @ 0x82448ED8  (BrnGui::AboveCarRenderer::RecvEvent)
//   Array<BankingScore,6>::EraseFast @ 0x82449018  (BrnGui::AboveCarRenderer::RenderBankingScores)
//   Array<BankingScore,6>::GetItem   @ 0x82449658  (BrnGui::AboveCarRenderer::RenderBankingScores)
//   Array<BankingScore,6>::IsFull    @ 0x8244FEC8  (BrnGui::AboveCarRenderer::RecvEvent)
//
// Layout: maElements[6] (6 * 48 = 288B; BankingScore is 48 bytes, 16-aligned -- see
// BrnAboveCarRenderer.h) + miCount @ +0x120 (288), matching the X360 count word read/written
// at *(this+0x120) (`lwz/stw 0x120(rN)`), the index*48 element addressing
// (`slwi r,1; add r,r; slwi r,4` == *48 in GetItem/Append/EraseFast), and the 6-qword
// (`ld/std` x6 == 48-byte) element copy in Append/EraseFast.
//
// Faithful-body parity per function:
//   Append    @0x82448ED8 -> assert constructed (CgsArray.h:225, count != -1), assert
//                            count < 6 (CgsArray.h:226 "out of space"), maElements[count] = e,
//                            ++count.
//   EraseFast @0x82449018 -> assert constructed (CgsArray.h:405), assert index < count
//                            (CgsArray.h:406 "Trying to erase an unused element"),
//                            maElements[index] = maElements[count-1], --count.
//   GetItem   @0x82449658 -> assert constructed (CgsArray.h:556), assert index < count
//                            (CgsArray.h:557 "out of bounds"), return &maElements[index].
//   IsFull    @0x8244FEC8 -> assert constructed (CgsArray.h:336), return count == 6.
// The X360 Append/GetItem stream the dynamic out-of-space / out-of-bounds messages
// (BasePriorityQueue::Clear + StrStream integer append); the committed generic body keeps
// those as static CGS_ASSERT strings (parity note recorded in CgsArrayInt8.cpp). The
// per-element copy in Append/EraseFast uses the implicit memberwise copy-assign of the
// 48-byte BankingScore (== the six-qword ld/std block).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnGui::BankingScore,6u>.
//
// PER-METHOD (not whole-class) instantiation: the X360 emitted only the four methods above
// for this instance. BankingScore has no operator== (the DWARF struct is a plain aggregate),
// so the equality-based generic members (FindFirstInstanceOf/Contains) are deliberately left
// un-instantiated -- matching the X360 ledger and avoiding a fabricated operator==. This
// mirrors the per-method instantiation precedent in VariableEventQueue_18432_16.cpp.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Gui/CustomRenderer/Renderers/BrnAboveCarRenderer.h"

template void              Array<BrnGui::BankingScore, 6>::Append(const BrnGui::BankingScore&);
template void              Array<BrnGui::BankingScore, 6>::EraseFast(u32);
template BrnGui::BankingScore& Array<BrnGui::BankingScore, 6>::GetItem(u32);
template bool              Array<BrnGui::BankingScore, 6>::IsFull() const;
