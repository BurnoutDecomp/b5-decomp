// Per-instantiation .cpp for Array<rw::math::vpu::Vector3, 5120>. The generic Array<T,N>
// body (Append + siblings) is fully inline in CgsArray.h, so this TU is just the explicit
// class instantiation (the X360 emits one out-of-line copy per using-TU):
//   Array<rw::math::vpu::Vector3,5120>::Append  @ 0x82448B00
//                                                 (BrnGui::GuiTracker::GenerateRouteData)
//
// Layout/asm parity: each element is a 16-byte, 16-aligned VMX register (rw::math::vpu::Vector3
// from rw/math/vpu/types.h). The X360 Append asserts the array was Construct/Clear'd
// (count word == -1 sentinel, CgsArray.h:225) and has room (count >= 0x1400/5120,
// CgsArray.h:226), then does `lvx128 v0,(r25)` / `stvx128 v0,(r26 + 16*count)` -- a whole
// 16-byte register copy into maElements[miCount] (stride 16 = slwi r11,r11,4) followed by
// ++miCount. That is exactly the committed generic Append (maElements[miCount]=lrElement;
// ++miCount) for a 16-byte trivially-copyable element; no per-instantiation specialisation
// is needed and the dynamic out-of-space message stays the shared static CGS_ASSERT string.
//
// The DWARF spells the type CgsContainers::Array<...>; we follow the committed unqualified
// Array<T,N> container convention (CgsArray.h). The 16-byte element comes from the canonical
// RenderWare math home so the stored register width matches the asm.
//
// Instantiate ONLY Append per-member (NOT `template class`): the element rw::math::vpu::Vector3
// is a SIMD register aggregate that defines no operator==, so forcing the whole class would
// instantiate FindFirstInstanceOf/Contains and fail to compile. The same per-member technique
// the ObjectPool<IceMovie,20,s32> instantiation TU uses for the same reason.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "rw/math/vpu/types.h"

template void Array<rw::math::vpu::Vector3, 5120>::Append(const rw::math::vpu::Vector3&);
