// Per-instantiation .cpp for Array<rw::math::vpu::Vector3, 10>. The generic Array<T,N>
// body (Append + operator[]/GetItem) is fully inline in CgsArray.h; the X360 emits one
// out-of-line copy of each used member per using-TU. This TU is the explicit per-member
// instantiation only (do NOT re-define the generic):
//   Array<rw::math::vpu::Vector3,10>::Append  @ 0x822AD2C8  (BrnWorld::PlaceOnTrackManager::PrePhysicsUpdate)
//   Array<rw::math::vpu::Vector3,10>::GetItem @ 0x822AEAF8  (BrnWorld::RaceCarEntityModuleDebugComponent::RenderWorld)
//
// Layout/asm parity: each element is a 16-byte, 16-aligned VMX register (rw::math::vpu::Vector3
// from rw/math/vpu/types.h). The live-element count word sits at byte offset 0xA0 == 160 ==
// 10 * 16 == the end of the inline maElements[10] buffer, confirming the count follows the
// element buffer (matches the committed Array<T,N> layout, count at offset N*sizeof(T)).
//   Append  @0x822AD2C8: asserts Construct/Clear'd (count @+0xA0 != -1 sentinel, CgsArray.h:225),
//     asserts room (unsigned count >= 0xA/10 streams "Array container out of space ..." , :226),
//     then `lvx128 v0,(r26)` / `stvx128 v0,(r29 + 16*count)` (slwi r11,r11,4 = stride 16) into
//     maElements[count], then ++count -- exactly the committed generic Append for a 16-byte
//     trivially-copyable element.
//   GetItem @0x822AEAF8: asserts Construct/Clear'd (CgsArray.h:556) + bounds-checks index against
//     count ("Array index out of bounds. Index/length", :557), then returns `16*index + base`
//     == &maElements[index] -- the committed generic GetItem(u32) (non-const operator[]).
//
// Instantiate ONLY the used members (NOT `template class`): the element rw::math::vpu::Vector3 is
// a SIMD register aggregate that defines no operator==, so forcing the whole class would
// instantiate FindFirstInstanceOf/Contains and fail to compile (same per-member technique as
// CgsArrayVpuVector3_5120.cpp). The dynamic out-of-space/out-of-bounds messages stay the shared
// static CGS_ASSERT strings.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "rw/math/vpu/types.h"

template void Array<rw::math::vpu::Vector3, 10>::Append(const rw::math::vpu::Vector3&);
template rw::math::vpu::Vector3& Array<rw::math::vpu::Vector3, 10>::GetItem(u32);
