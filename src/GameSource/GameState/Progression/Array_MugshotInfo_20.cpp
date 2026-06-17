#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/Progression/BrnProfile.h"   // BrnProgression::MugshotInfo element home
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

// Per-instantiation .cpp for Array<BrnProgression::MugshotInfo, 20>. The generic Array<T,N>
// body (Append / Erase / IsFull / GetItem + siblings) is fully inline in CgsArray.h. N==20
// (not the DWARF's PS3 30u) and the 56-byte stride / +1120 count word are X360-authoritative:
//   Append  @ 0x8235BE80  (BrnProgression::Profile::AddMugshot)
//   Erase   @ 0x8235BFB8  (BrnProgression::Profile::AddMugshot / ::DeleteMugshot)
//   GetItem @ 0x8235ED28  (Profile::AddMugshot/DeleteMugshot/LockOrUnlockMugshot/GetMugshotInfo,
//                          BrnGameState::GameStateImageManagerBase::ProcessDeleteRequest)
//   IsFull  @ 0x823679B8  (BrnProgression::Profile::AddMugshot)
//
// Spelled unqualified to match the committed Array<T,N> container convention (CgsArray.h);
// the DWARF spells the type CgsContainers::Array<BrnProgression::MugshotInfo,30u>.

// Append/Erase/IsFull are PLAIN explicit instantiations: the committed generic bodies already
// carry the exact X360 asserts (unconstructed + bounds), with the X360 dynamic StrStream
// "Length:/Capacity:" message collapsed to the static CGS_ASSERT expression string per
// project convention.
template void Array<BrnProgression::MugshotInfo, 20>::Append(const BrnProgression::MugshotInfo&);
template void Array<BrnProgression::MugshotInfo, 20>::Erase(u32);
template bool Array<BrnProgression::MugshotInfo, 20>::IsFull() const;

// GetItem needs a FULL MEMBER SPECIALIZATION (mirroring Array_ProfileEvent_175.cpp): the
// committed generic GetItem is a plain no-assert inline, but the X360 Array<MugshotInfo,20>
// ::GetItem @0x8235ED28 carries full bounds asserts. The count word miCount lives at +1120
// (20 * 56B stride); the X360 `return 56 * index + this` is &maElements[index]. The dynamic
// "Array index out of bounds. Index: <n>, length: <len>" StrStream message is collapsed to
// the static expression string the bounds assert reports.
template<>
BrnProgression::MugshotInfo& Array<BrnProgression::MugshotInfo, 20>::GetItem(u32 luIndex)
{
    // X360: *(a1 + 1120) == -1
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    // X360: a2 >= *(a1 + 1120)
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];                            // X360: return 56 * a2 + a1
}
