#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/Progression/BrnProfile.h"   // BrnProgression::ProfileEvent element home
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT

// X360 CgsContainers::Array<BrnProgression::ProfileEvent,175>::GetItem(u32) @ 0x824489F8.
// The generic GetItem in the committed CgsArray.h is a plain no-assert inline, so -- exactly
// like the committed Array_ChainableMultiplierInfo_8.cpp::GetCount precedent -- this
// instantiation needs a FULL MEMBER SPECIALIZATION carrying the X360 asserts. The
// element-buffer count word miCount lives at +1400 (175 * 8B stride); the X360
// `return 8 * index + this` is &maElements[index]. Called by BrnGui::GuiCache::GetProfileEvent.
//
// The asserts use the project CGS_ASSERT macro (it supplies __FILE__/__LINE__; the X360-baked
// file path + line are discarded per project convention). The dynamic "Array index out of
// bounds. Index: <n>, length: <len>" StrStream message is collapsed to the static expression
// string the bounds assert reports (the streamed numerics are not byte-reproducible from the
// dossier).
template<>
BrnProgression::ProfileEvent& Array<BrnProgression::ProfileEvent, 175>::GetItem(u32 luIndex)
{
    // X360: *(a1 + 1400) == -1
    CGS_ASSERT(miCount != KI_UNCONSTRUCTED, "Array used before Construct/Clear was called");
    // X360: a2 >= *(a1 + 1400)
    CGS_ASSERT(luIndex < static_cast<u32>(miCount), "Array index out of bounds");
    return maElements[luIndex];                            // X360: return 8 * a2 + a1
}

// X360 Array<BrnProgression::ProfileEvent,175>::Append(const ProfileEvent&) @0x8235CB40.
// The X360 body is the generic Array<T,N>::Append (committed inline in CgsArray.h): the
// constructed-assert (miCount != -1, "Array used before Construct/Clear was called",
// CgsArray.h:225), the room assert (miCount < 175, "Array container out of space",
// CgsArray.h:226), then the two-dword element copy (`*v13 = *a2; v13[1] = a2[1]` with
// v13 = &maElements[miCount] via the 8-byte stride) and `++miCount`. Emitted as an explicit
// instantiation of that generic body. The element-buffer count word miCount lives at +1400
// (175 * 8B stride). Called by BrnGameState::GameStateModule::ProcessGameEvents.
template void Array<BrnProgression::ProfileEvent, 175>::Append(const BrnProgression::ProfileEvent&);
