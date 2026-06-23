// Per-instantiation .cpp for Array<CgsGui::AnimChannel,6>. The generic Array<T,N> body
// (operator[] / GetItem / Append + siblings) is fully inline in CgsArray.h, so this TU is
// just the explicit member instantiation the X360 emits out-of-line for this element type:
//   Array<AnimChannel,6>::GetItem (non-const) @ 0x8284DA48
//       (called by Animator::Update / SetAnimationChannel / SetAnimation / SetAptValues)
// Byte-parity check against the X360 GetItem pseudocode for this instantiation:
//   element stride 36 (sizeof AnimChannel == 0x24: Interpolator* +0, bool +4, f32 +8,
//   Interpolator +0xC..+0x24), miCount @ +0xD8 (216 == 6*36) -> matches the X360's
//   `*(a1+216)` count word and `36 * a2 + a1` element address.
//
// GetItem routes through operator[] (the X360 uses the const-path asserts CgsArray.h:556/557);
// instantiating GetItem pulls in operator[] only -- the comparison-dependent members
// (FindFirstInstanceOf/Contains) are NOT instantiated, so AnimChannel needs no operator==.
//
// Spelled unqualified Array<...> to match the committed Array<T,N> container convention
// (CgsArray.h / CgsArrayInt8.cpp); the DWARF spells the type CgsContainers::Array<CgsGui::AnimChannel,6u>.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAnimChannel.h"

// X360 element stride (GetItem @ 0x8284DA48 addresses element a2 at `36 * a2 + a1`):
// sizeof(AnimChannel) is 0x24 (36) on the 32-bit target -- pointer (mpCurrentInterpolator)
// +0, bool +4, f32 +8, then a 0x18-byte Interpolator subobject (4-byte vtable + 5 f32) @ +0xC.
// Not static_asserted: the host gate is 64-bit (vcvars64), so the pointer-bearing members
// widen; this is a semantic-parity reconstruction, not a byte-match, and the member offsets
// above (not the host sizeof) are the layout authority.

template CgsGui::AnimChannel& Array<CgsGui::AnimChannel, 6>::GetItem(u32);
