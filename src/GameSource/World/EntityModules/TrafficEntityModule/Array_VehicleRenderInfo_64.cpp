// Array<BrnTraffic::VehicleRenderInfo, 64>::Append    @ 0x8270A148  (TrafficEntityModule::PreDispatchUpdate)
// Array<BrnTraffic::VehicleRenderInfo, 64>::Get       @ 0x827BA2A0  (WorldModule::CalculateVehicleLODs)
// Array<BrnTraffic::VehicleRenderInfo, 64>::GetLength @ 0x8270A280  (TrafficEntityModule::GenerateDispatchLists)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N> bodies (Append /
// operator[] / GetLength) are already committed inline in CgsArray.h; this TU is the thin
// explicit instantiation only (the X360 emits one out-of-line copy per using-TU). All three
// X360 bodies match the generic store-for-store:
//
//   ::Append    asserts the array was Construct/Clear'd (count word @ +0x300 != the -1
//     sentinel, CgsArray.h:225), then asserts there is room (unsigned count < 0x40, the X360
//     `cmplwi 0x40; blt` fires "Array container out of space" once count>=64, CgsArray.h:226),
//     copies the 12-byte VehicleRenderInfo as three dwords (`stw muEntityIndex@+0`,
//     `stw mfDistanceSq@+4`, `stw mLOD@+8`) at &maElements[count] (12-byte stride
//     index*12 == `slwi r,1; add; slwi r,2`), then increments the count word.
//   ::Get       is the const checked accessor (the DWARF spelling of Array<>::Ge/GetItem):
//     asserts constructed (CgsArray.h:556) + index < count ("Array index out of bounds",
//     CgsArray.h:557) then returns 12*index + base (`slwi r11,r27,1; add; slwi r11,r11,2; add`).
//   ::GetLength asserts constructed (CgsArray.h:336) then returns the unsigned count word at
//     +0x300 -- exactly the generic GetLength().
//
// The live-count word sits at byte +0x300 == 64 * sizeof(VehicleRenderInfo), confirming
// sizeof(VehicleRenderInfo) == 12 (u32 muEntityIndex + f32 mfDistanceSq + 4-byte enum mLOD).
// The DWARF spells the type CgsContainers::Array<BrnTraffic::VehicleRenderInfo,64u>; spelled
// unqualified to match the committed Array<T,N> convention. VehicleRenderInfo is a 12-byte POD
// (implicit memberwise copy-assign == the three-dword store block).

#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::Append / ::operator[] / ::GetLength (inline generic)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h" // VehicleRenderInfo (12-byte element)

template void Array<BrnTraffic::VehicleRenderInfo, 64>::Append(const BrnTraffic::VehicleRenderInfo&);
template const BrnTraffic::VehicleRenderInfo& Array<BrnTraffic::VehicleRenderInfo, 64>::GetItem(u32) const;
template u32 Array<BrnTraffic::VehicleRenderInfo, 64>::GetLength() const;
