// Per-instantiation .cpp for Array<s32,4>. The generic Array<T,N> body (Append / Contains /
// FindFirstInstanceOf + the checked operator[]/GetItem accessor + siblings) is fully inline in
// CgsArray.h, so this TU is just the explicit class instantiation (the X360 emits one out-of-line
// copy per using-TU). Instantiated by the NearMiss vein (BrnWorld::NearMissManager::AddNearRaceCar/
// AddNearTraffic/Update and BrnWorld::NearMissData<4,8>/<4,7>::BackupNearArray), a small int index list:
//
//   Array<int,4>::Append              @ 0x822AEC00  (NearMissManager::AddNearRaceCar/AddNearTraffic)
//   Array<int,4>::Contains            @ 0x822CAC60  (NearMissManager::AddNearRaceCar/AddNearTraffic)
//   Array<int,4>::FindFirstInstanceOf @ 0x822B0258  (Array<int,4>::Contains, NearMissManager::Update)
//   Array<int,4>::GetItem             @ 0x822B02F0  (NearMissData<4,8>/<4,7>::BackupNearArray, Update)
//
// Layout: maElements[4] (16B) + miCount @ +0x10, matching the X360 count word read at 0x10(this)
// (the -1 sentinel gates the 'Array used before Construct/Clear was called' assert; N==4 from the
// out-of-space compare `cmplwi ...,4` @ 0x822AEC48 and the stride-4 store `slwi r11,r11,2` @ 0x822AED00
// fixing sizeof(int)==4). GetItem is the checked const operator[] (asserts constructed + `a2 >= count`
// bounds -> 'Array index out of bounds. Index: ... , length: ...', then returns `4*a2 + this` ==
// &maElements[a2]); Contains double-asserts then FindFirstInstanceOf(x) != -1; FindFirstInstanceOf
// scans [0,count) for *a2, returning the index or -1.
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<int,4u>. int == s32 (types.hpp). A primitive
// element needs no element_home include (CgsArray.h already pulls types.hpp).
#include "GameShared/GameClasses/Containers/CgsArray.h"

template class Array<s32, 4>;
