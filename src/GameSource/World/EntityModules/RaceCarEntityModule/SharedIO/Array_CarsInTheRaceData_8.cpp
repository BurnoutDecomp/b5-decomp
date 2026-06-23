#include "GameShared/GameClasses/Containers/CgsArray.h"            // Array<T,N>::Append (inline generic)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // CarsInTheRaceData element

// =============================================================================
// Array<BrnWorld::RaceCarEntityModuleIO::CarsInTheRaceData, 8>::Append
//   @ 0x822AC710   (ledger id: class:BrnWorld::RaceCarEntityModuleIO::CarsInTheRaceData,8>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// Push one CarsInTheRaceData onto the inline maElements buffer. The generic
// Array<T,N>::Append body is inline in CgsArray.h (one body covers every
// instantiation); this TU is the explicit instantiation the X360 emitted out-of-line
// for the CarsInTheRaceData/8 specialisation. Called by
// RCEntityActiveRaceCarOutputInterface::AddCarToRace (which does maCarsInTheRace.Append).
//
// X360 store-for-store (asm at 0x822AC710):
//   * miCount lives at owner+0x200 (== 8 * 64), i.e. immediately past maElements[8].
//   * assert miCount != -1 ("Array used before Construct/Clear was called",
//     CgsArray.h:225) -- the X360 spells the file as
//     "..\..\..\GameShared\GameClasses\Containers/CgsArray.h".
//   * assert (unsigned) miCount < 8 (CgsArray.h:226), streaming the dynamic
//     "Array container out of space, Length: <n>, Capacity: 8" message.
//   * element copy: `slwi r10, r10, 6` (index * 64) + base, then `mtctr 8` with a
//     `ld`/`std` loop -> 8 qwords == 64 bytes copied unconditionally. CarsInTheRaceData
//     is exactly 64 bytes (three alignas(16) Vector3 @0/16/32 = 48, then
//     EActiveRaceCarIndex@48 + EGlobalRaceCarIndex@52 + bool mbIsPlayer@56, padded to
//     the Vector3 16-byte alignment -> 64). This matches the X360 *64 stride 1:1.
//   * ++miCount (stw to owner+0x200).
// The generic Append (CGS_ASSERT constructed, CGS_ASSERT room, maElements[miCount] =
// lrElement, ++miCount) reproduces this exactly.
// =============================================================================
template void
Array<BrnWorld::RaceCarEntityModuleIO::CarsInTheRaceData, 8u>::Append(
    const BrnWorld::RaceCarEntityModuleIO::CarsInTheRaceData&);
