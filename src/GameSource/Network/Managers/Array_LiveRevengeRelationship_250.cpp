// Per-instantiation .cpp for Array<BrnNetwork::LiveRevengeRelationship, 250>::Append.
//
//   Array<BrnNetwork::LiveRevengeRelationship,250>::Append  @ 0x8254DE90
//     (called by BrnNetwork::LiveRevengeManager::AddNewTableEntry)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::Append body is fully
// inline in CgsArray.h, so this TU is just the explicit member instantiation (the X360 emits
// one out-of-line copy per using-TU). The element type is the committed
// BrnNetwork::LiveRevengeRelationship (120-byte record), reused by name.
//
// Layout / byte-parity check against the X360 body:
//   miCount lives at +0x7530 (== 250 * 120 == N * sizeof(T)), i.e. immediately after the
//   inline maElements[250] buffer, exactly as the committed generic Array<T,N> places it.
//     lwz r11, 0x7530(this); cmpwi -1   -> "Array used before Construct/Clear was called"
//                                           (KI_UNCONSTRUCTED sentinel guard)
//     lwz r11, 0x7530(this); cmplwi 0xFA -> bounds guard against capacity 250 (0xFA == N),
//                                           unsigned so the -1 sentinel also fails it
//     memcpy(120*miCount + this, src, 120) -> maElements[miCount] = element  (stride 0x78 == 120)
//     stw miCount+1, 0x7530(this)          -> ++miCount
// The X360 build streams the dynamic "Array container out of space, Length: <n>, Capacity: 250"
// message via the StrStream/BasePriorityQueue::Clear path; the committed generic Append keeps
// that as the static CGS_ASSERT string "Array container out of space" -- the documented
// generic-body parity gap (fixing the dynamic-message form must GROW the shared CgsArray.h
// body so every instantiation re-verifies; it is deliberately not specialized in this thin TU).
//
// The element is a plain aggregate (no user-declared copy-assignment), so its element copy is
// the compiler-generated member-wise copy -- matching the X360's 120-byte block move.
//
// Only Append is instantiated (the single X360-emitted member for this instance). A whole-class
// `template class` instantiation is deliberately NOT used: it would force the equality-based
// members (FindFirstInstanceOf/Contains), which require T::operator==; LiveRevengeRelationship
// is a plain aggregate with no operator== and the X360 never emits those members for it.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeRelationship.h"  // BrnNetwork::LiveRevengeRelationship (120B element)

template void Array<BrnNetwork::LiveRevengeRelationship, 250>::Append(const BrnNetwork::LiveRevengeRelationship&);

// const operator[]  @ 0x824F7388  (checked indexed accessor; called by ~15 LiveRevengeManager
// sites, e.g. GetNonConstRevengeRelation/OnRoundFinish/FindPlayerInTableByName/AddNewTableEntry).
// The X360 body is the generic const operator[] (CgsArray.h:556/557): asserts miCount != the -1
// sentinel ("Array used before Construct/Clear was called"), unsigned bounds-checks the index
// against miCount @ +0x7530 (== 250 * 120 == N * sizeof(T)), then returns &maElements[i] (element
// stride 0x78 == 120). The dynamic "Array index out of bounds. Index: <i>, length: <n>" StrStream
// message is the documented generic-body parity gap (kept as the static CGS_ASSERT string).
template const BrnNetwork::LiveRevengeRelationship&
    Array<BrnNetwork::LiveRevengeRelationship, 250>::operator[](u32) const;
