// Per-instantiation .cpp for Array<BrnTraffic::PhysicalRequestInfo, 25>.
// The generic Array<T,N> body (Append / GetLength / ...) is fully inline in CgsArray.h;
// the X360 emits one out-of-line copy of each USED member per using-TU. This TU is the
// explicit per-member instantiation only (do NOT re-define the generic). The members the
// X360 emitted out-of-line for this instance (both from
// BrnTraffic::UpdateVehiclesJob::RequestNewPhysicalVehicle):
//   Array<PhysicalRequestInfo,25>::Append    @ 0x82918B98
//   Array<PhysicalRequestInfo,25>::GetLength  @ 0x82709748
//
// Layout/asm parity: maElements[25] (25 * 8 = 200B; PhysicalRequestInfo is an 8-byte POD --
// u16 muVehicle @0, i8 miReason @2, pad @3, EntityId mTargetEntityId @4) + miCount @ +0xC8
// (== 25*8), matching the X360 count word read/written at *(this+0xC8) and the slwi r11,r11,3
// (index*8) element addressing with the two-word (stw@+0, stw@+4) element copy in Append.
//   * Append    @ 0x82918B98: constructed-assert (count != -1, CgsArray.h:225) + out-of-space
//     assert (count < 25u, cmplwi 0x19, CgsArray.h:226, dynamic StrStream 'Array container out
//     of space, Length: <n>, Capacity: 25' kept as the static CGS_ASSERT string), writes the
//     8-byte element then post-increments the count.
//   * GetLength  @ 0x82709748: constructed-assert (count != -1, CgsArray.h:336) then returns
//     the u32 count word.
//
// PhysicalRequestInfo is a plain POD with no operator==, so ONLY the used members are
// instantiated (NOT `template class`), avoiding forcing FindFirstInstanceOf/Contains --
// same per-member technique as the committed CgsArrayStaticPassbyRecord5.cpp. Spelled
// unqualified to match the committed Array<T,N> convention; the DWARF spells the type
// CgsContainers::Array<BrnTraffic::PhysicalRequestInfo,25u>. The PhysicalRequestInfo type and
// the PhysicalRequestInfoList : Array<PhysicalRequestInfo,25> alias are homed in
// BrnTrafficMiscRuntimeClasses.h; this file only forces the explicit instantiation.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h"

template void Array<BrnTraffic::PhysicalRequestInfo, 25>::Append(const BrnTraffic::PhysicalRequestInfo&);
template u32  Array<BrnTraffic::PhysicalRequestInfo, 25>::GetLength() const;
