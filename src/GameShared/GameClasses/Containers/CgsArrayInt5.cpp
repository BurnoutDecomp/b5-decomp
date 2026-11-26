// Per-instantiation .cpp for Array<u32,5>. The generic Array<T,N> body (Append /
// Contains / FindFirstInstanceOf + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<unsigned int,5>::Append              @ 0x82448C28  (BrnGui::CrashNavIconRenderer::Construct)
//   Array<unsigned int,5>::Contains            @ 0x8244FE48  (BrnGui::CrashNavIconRenderer::GetIconInformation)
//   Array<unsigned int,5>::FindFirstInstanceOf @ 0x824494B8  (Array<unsigned int,5>::Contains)
// The element is unsigned: DWARF BrnCrashNavIconRenderer.h:366 `Array<uint32_t,5u>
// mOnlineStartpointsToIgnore`, and FindFirstInstanceOf compares with cmplw (@0x8244951C).
// IDA's "Array<int,5>" names are front-truncated.
// Layout: maElements[5] (20B) + miCount @ +0x14, matching the X360 result[5]/*(a1+20) count word
// (Append reads result[5], Contains/FindFirstInstanceOf read *(a1+20)==result[5]; -1 sentinel).
//
// uint32_t == u32 (types.hpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class CgsContainers::Array<u32, 5>;
