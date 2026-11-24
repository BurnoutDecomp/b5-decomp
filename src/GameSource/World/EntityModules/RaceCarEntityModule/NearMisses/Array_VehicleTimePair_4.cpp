#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::Append/Erase/GetItem (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/NearMisses/BrnNearMissData.h" // NearMissData<A,B>::VehicleTimePair (8-byte element)

// The Array<NearMissData<A,B>::VehicleTimePair, 4> instances (the two A-sized record lists).
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic bodies are inline in CgsArray.h;
// this TU is the thin explicit instantiation only (do NOT re-define the generic).
// VehicleTimePair is nested in each NearMissData instantiation (DWARF BrnNearMissData.h:122),
// so ARTIST has one instance per outer type, byte-identical apart from their callers:
//
//                         NearMissData<4,8>   NearMissData<4,7>
//   Append                0x822AED20          0x822AF0E0
//   Erase                 0x822AEE48          0x822AF208
//   GetItem (non-const)   0x822B03F8          0x822B0818   asserts CgsArray.h:556/557 (h:546)
//   GetItem const         0x822B0500          0x822B0920   asserts CgsArray.h:538/539 (h:528)
//
// Append asserts Construct/Clear'd (count @ +0x20 != -1, CgsArray.h:225) and room (h:226),
// stores the 8-byte pair at &maElements[count] (slwi r11,count,3) and bumps the count; the count
// word at +0x20 == 4 * sizeof(VehicleTimePair). Erase asserts h:380/381, decrements the count
// and shifts the tail down one slot. The non-const GetItem serves UpdateTimers /
// RememberNearMiss / AddContacted, the const one IsRecentNearMiss / IsRecentContacted (operator[]
// forwards to GetItem and is inlined).
template void CgsContainers::Array<BrnWorld::NearMissData<4, 7>::VehicleTimePair, 4>::Append(const BrnWorld::NearMissData<4, 7>::VehicleTimePair&);
template void CgsContainers::Array<BrnWorld::NearMissData<4, 7>::VehicleTimePair, 4>::Erase(u32);
template void CgsContainers::Array<BrnWorld::NearMissData<4, 8>::VehicleTimePair, 4>::Append(const BrnWorld::NearMissData<4, 8>::VehicleTimePair&);
template void CgsContainers::Array<BrnWorld::NearMissData<4, 8>::VehicleTimePair, 4>::Erase(u32);

template BrnWorld::NearMissData<4, 8>::VehicleTimePair&
CgsContainers::Array<BrnWorld::NearMissData<4, 8>::VehicleTimePair, 4>::GetItem(u32);
template BrnWorld::NearMissData<4, 7>::VehicleTimePair&
CgsContainers::Array<BrnWorld::NearMissData<4, 7>::VehicleTimePair, 4>::GetItem(u32);
template const BrnWorld::NearMissData<4, 8>::VehicleTimePair&
CgsContainers::Array<BrnWorld::NearMissData<4, 8>::VehicleTimePair, 4>::GetItem(u32) const;
template const BrnWorld::NearMissData<4, 7>::VehicleTimePair&
CgsContainers::Array<BrnWorld::NearMissData<4, 7>::VehicleTimePair, 4>::GetItem(u32) const;
