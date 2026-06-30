// Per-instantiation .cpp for Array<s32,5>. The generic Array<T,N> body (Append /
// Contains / FindFirstInstanceOf + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<int,5>::Append              @ 0x82448C28  (BrnGui::CrashNavIconRenderer::Construct)
//   Array<int,5>::Contains            @ 0x8244FE48  (BrnGui::CrashNavIconRenderer::GetIconInformation)
//   Array<int,5>::FindFirstInstanceOf @ 0x824494B8  (Array<int,5>::Contains)
// Layout: maElements[5] (20B) + miCount @ +0x14, matching the X360 result[5]/*(a1+20) count word
// (Append reads result[5], Contains/FindFirstInstanceOf read *(a1+20)==result[5]; -1 sentinel).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,5u>. int == s32 (types.hpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 5>;
