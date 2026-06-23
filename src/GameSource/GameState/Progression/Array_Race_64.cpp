#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "SharedClasses/Progression/BrnRace.h"          // BrnProgression::Race element home (120-byte stride)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

// Explicit instantiation(s)/specialisation(s) of the generic Array<T,N> container methods
// (inline in CgsArray.h) for the BrnProgression::Race,64 leaf instantiation -- reconstructed
// from BURNOUT_X360_ARTIST.XEX:
//   Array<BrnProgression::Race,64>::Append  @0x8235E0C8
//   Array<BrnProgression::Race,64>::GetItem @0x82360108
//
// The element stride is PROVEN 120 bytes (0x78): Append's `memcpy(120 * miCount + this, src, 120)`
// and the live-count word miCount landing at +7680 == 64 * 120. sizeof(BrnProgression::Race) is
// 120 on the console (its 8-byte-aligned CgsID member pads the 0x71-byte body up to 0x78).

// X360 0x8235E0C8. Append is the generic Array<T,N>::Append (committed inline in CgsArray.h):
// the constructed-assert (miCount != -1, CgsArray.h:225), the room assert (miCount < 64,
// CgsArray.h:226), the element copy (`memcpy(&maElements[miCount], src, 120)` == the generic's
// memberwise `maElements[miCount] = lrElement` for the 120-byte POD-shaped Race) and `++miCount`.
// Emitted as an explicit instantiation of that generic body. Called by
// BrnProgression::ProgressionManager::ProcessLoadedPresetRaces.
template void Array<BrnProgression::Race, 64>::Append(const BrnProgression::Race&);

// X360 0x82360108. The bounds-checked indexed accessor. The generic GetItem in CgsArray.h is a
// plain no-assert inline forwarding to operator[], so -- exactly like the committed
// Array_ProfileEvent_175.cpp::GetItem precedent -- this instantiation carries the X360 asserts
// via a FULL MEMBER SPECIALIZATION. The X360 `return 120 * index + this` is &maElements[index]
// (120-byte stride). Called by BrnProgression::ProgressionManager::GetRacesAtLandmark and
// ::LandmarkHasAvailableRaces.
//
// The asserts use the project CGS_ASSERT macro (it supplies __FILE__/__LINE__; the X360-baked
// file path + line are discarded per project convention). The dynamic "Array index out of
// bounds. Index: <n>, length: <len>" StrStream message is collapsed to the static expression
// string the bounds assert reports.
template<>
BrnProgression::Race& Array<BrnProgression::Race, 64>::GetItem(u32 luIndex)
{
    // X360: *(a1 + 7680) == -1
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    // X360: a2 >= *(a1 + 7680)
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];                            // X360: return 120 * a2 + a1
}
