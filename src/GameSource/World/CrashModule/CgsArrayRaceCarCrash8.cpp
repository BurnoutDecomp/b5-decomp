// Per-instantiation .cpp for Array<BrnWorld::RaceCarCrash, 8>. The generic Array<T,N> body
// (EraseFast / GetItem / Grow + siblings) is fully inline in CgsArray.h, so this TU is just
// the explicit class instantiation (the X360 emits one out-of-line copy per using-TU). The
// three methods the X360 emitted for this instance:
//   Array<RaceCarCrash,8>::EraseFast @ 0x827B5410  (BrnWorld::CrashModule::ResetRaceCarFromCrashIndex)
//   Array<RaceCarCrash,8>::GetItem   @ 0x827BA3B0  (BrnWorld::CrashModule::OnContactFromNetworkPlayer,
//                                                   TickCrashes, ResetRaceCarFromCrashIndex,
//                                                   OnNetworkPlayerDisconnected, ClearupCrashes,
//                                                   HandleGameActions)
//   Array<RaceCarCrash,8>::Grow      @ 0x827B52F8  (BrnWorld::CrashModule::ProcessCrashedRaceCarEvents)
//
// Layout: maElements[8] (8 * 24 = 192B; RaceCarCrash is 24 bytes -- see BrnRaceCarCrash.h SIZE
// banner) + miCount @ +0xC0 (192), matching the X360 count word read/written at *(this+192)
// (`lwz/stw 0xC0(rN)`), the index*24 element addressing (`slwi r,1; add r,r; slwi r,3` == *24 in
// GetItem/Grow/EraseFast), and EraseFast's whole-element copy as three 8-byte loads/stores
// (`ld/std` at +0, +8, +0x10 == 24 bytes).
//
// The X360 EraseFast/GetItem/Grow carry the CgsArray.h bounds/used-before-Construct asserts
// (lines 405/406, 556/557, 289/290) -- supplied by the committed generic CGS_ASSERT body. Grow
// is the DWARF name (CgsArray.h:279) for the reserve-next-slot accessor (committed alias of
// AddNew). The streamed dynamic out-of-space/out-of-bounds messages are kept as the static
// strings in the generic body (parity note recorded in CgsArrayInt8.cpp).
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnWorld::RaceCarCrash,8u>. operator== on
// RaceCarCrash (additive in BrnRaceCarCrash.h) satisfies the equality-based generic members the
// explicit instantiation forces; the 24-byte element copy in EraseFast uses the implicit
// memberwise copy-assign (== the three-qword `ld/std` block).
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/World/CrashModule/BrnRaceCarCrash.h"

template class Array<BrnWorld::RaceCarCrash, 8>;
