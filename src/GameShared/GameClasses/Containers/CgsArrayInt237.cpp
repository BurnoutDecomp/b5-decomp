// Per-instantiation .cpp for Array<s32,237>. The generic Array<T,N> body (Append / Erase /
// EraseInstancesOf / GetItem + siblings) is fully inline in CgsArray.h, so this TU is just the
// explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,237>::Append           @ 0x824F66F0  (BrnGui::StateLoadingHelper::EnsureResourceIsLoaded,
//                                                    BrnGui::StateLoadingHelper::UnloadResource)
//   Array<int,237>::Erase            @ 0x824F71D0  (Array<int,237>::EraseInstancesOf)
//   Array<int,237>::EraseInstancesOf @ 0x824FBD30  (StateLoadingHelper::EnsureResourceIsLoaded,
//                                                    StateLoadingHelper::UnloadResource)
//   Array<int,237>::GetItem          @ 0x824F7280  (BrnGui::StateLoadingHelper::Update)
// Layout: maElements[237] (948B) + miCount @ +0x3B4, matching the X360 *(a1+0x3B4)==*(a1+948)
// count word (237*4 == 948 == 0x3B4) and the `4*index + a1` (== &maElements[index]) bounds-checked
// GetItem return. Element stride 4 (slwi ,,2), element type int == s32 (types.hpp).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,237u>. A primitive element needs no
// element_home include. This is the StateLoadingHelper request-dirty-list instantiation.
//
// NOTE (generic-body parity gap, NOT re-forked here, same as CgsArrayInt28/50): the X360
// Array<int,237>::Append/GetItem stream the DYNAMIC out-of-space / out-of-bounds messages
// ("Array container out of space, Length: <n>, Capacity: 237" / "Array index out of bounds.
// Index: <i>, length: <n>"). The committed generic bodies keep the unconstructed + bounds
// checks as static CGS_ASSERT strings; matching the dynamic-message form must GROW the shared
// CgsArray.h body so every instantiation re-verifies, so it is not specialized in this thin TU.
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 237>;
