#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/Progression/BrnProfile.h"   // BrnProgression::ProfileEvent element home
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert::Begin/Fire/EndAssert (explicit, baked file/line)

// X360 CgsContainers::Array<BrnProgression::ProfileEvent,175>::GetItem(u32) @ 0x824489F8.
// The generic GetItem in the committed CgsArray.h is a plain no-assert inline, so -- exactly
// like the committed Array_ChainableMultiplierInfo_8.cpp::GetCount precedent -- this
// instantiation needs a FULL MEMBER SPECIALIZATION carrying the X360-baked asserts. The
// element-buffer count word miCount lives at +1400 (175 * 8B stride); the X360
// `return 8 * index + this` is &maElements[index]. Called by BrnGui::GuiCache::GetProfileEvent.
//
// The asserts use explicit BeginAssert/FireAssert(expr,file,line)/EndAssert with the X360-baked
// literal path "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h" and lines 538/539
// verbatim -- NOT CGS_ASSERT (which would re-bake this repo's __FILE__/__LINE__). The dynamic
// "Array index out of bounds. Index: <n>, length: <len>" StrStream message is collapsed to the
// static expression string the bounds assert reports (the streamed numerics are not
// byte-reproducible from the dossier); baked lines kept verbatim.
template<>
BrnProgression::ProfileEvent& Array<BrnProgression::ProfileEvent, 175>::GetItem(u32 luIndex)
{
    if (miCount == KI_UNCONSTRUCTED)                       // X360: *(a1 + 1400) == -1
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array used before Construct/Clear was called",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            538);
        CgsDev::Assert::EndAssert();
    }
    if (luIndex >= static_cast<u32>(miCount))             // X360: a2 >= *(a1 + 1400)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "Array index out of bounds",
            "..\\..\\..\\GameShared\\GameClasses\\Containers/CgsArray.h",
            539);
        CgsDev::Assert::EndAssert();
    }
    return maElements[luIndex];                            // X360: return 8 * a2 + a1
}
