#pragma once

// Attrib::Gen::proceduralshot -- generated AttribSys class (procedural-shot
// director/camera parameters). No recovered source or type information for this TU;
// same generated-ctor pattern as the sibling generated classes debrisparams /
// surfacelist / worldemitter. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only proceduralshot function in
// the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::proceduralshot::proceduralshot @ 0x822091E8
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class proceduralshot : private Instance
    {
    public:
        explicit proceduralshot(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        // The 64-bit class-key tag identifying a proceduralshot ShotList element
        // (ShotSelector::GetCrashShot @0x82239900 builds 0x9B2E3C86_E02737B0 with
        // insrdi and cmpld-compares the RefSpec's leading qword against it; the low
        // word is the 32-bit ClassName::proceduralshot key the ctor asserts).
        static s64 ClassKey() { return static_cast<s64>(0x9B2E3C86E02737B0ULL); }

        // ADDITIVE GROW (ShotSelector::GetCrashShot @0x82239928): construct over a
        // ShotList RefSpec element. ⭐ THIS IS THE OVERLOAD THE CONSOLE ACTUALLY HAS: the
        // single proceduralshot ctor symbol in the image chains
        // Attrib::Instance(const RefSpec&, void*), not the Collection* overload above, and
        // both call sites (ShotSelector::GetCrashShot and BehaviourManager::NewBehaviour)
        // hand it a shot attribute block, which is a RefSpec. Bodied below 2026-09-11.
        proceduralshot(const Attrib::RefSpec& lrRefSpec, void* lpOwner);

        // ADDITIVE GROW (ShotSelector::GetCrashShot @0x8223992C..34): the generated
        // per-attribute reads on the resolved layout block -- SuitableFor (event-flag
        // mask, layout +0x00) and ShotProperties (property mask, layout +0x08; the
        // proceduralshot layout differs from iceanim's). DWARF spells both as
        // generated accessors; modelled as plain u32 reads per the minimal-recon
        // convention.
        u32 SuitableFor() const     { return reinterpret_cast<const u32*>(GetLayoutPointer())[0]; }
        u32 ShotProperties() const  { return reinterpret_cast<const u32*>(GetLayoutPointer())[2]; }

        // ADDITIVE GROW 2026-09-11 (the behaviour factory's procedural arm): the shot-type
        // selector word, layout +0x04 -- read straight off the resolved layout block, the
        // same plain-u32 shape as the two reads above. The factory switches on it to pick
        // which gyro-cam parameter block the allocated behaviour adopts; anything outside
        // the three known values trips the console's "Unsupported Procedural Shot Type".
        s32 ShotType() const        { return reinterpret_cast<const s32*>(GetLayoutPointer())[1]; }
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::proceduralshot (skipping the assert when the class is
    // unset/0), then give the instance a default data area (0x10 bytes) if it
    // has none. The X360 asm de-inlines the class-key check as a 64-bit
    // register-pair compare (insrdi builds 0x9B2E3C86E02737B0, cmpld cr6);
    // per the committed generated-class convention the low word 0xE02737B0 =
    // -534300752 is the 32-bit ClassName::proceduralshot key and the high word
    // is incidental -- matching the sibling debrisparams/worldemitter recons
    // and the Hex-Rays pseudocode's own `GetClass() != -534300752`.
    inline proceduralshot::proceduralshot(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROCEDURALSHOT_CLASS = -534300752; // Attrib::ClassName::proceduralshot (0xE02737B0)
        if (GetClass() != KI_PROCEDURALSHOT_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROCEDURALSHOT_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }

    // The reference-spec overload. Same guard order, same class constant and the same
    // 0x10-byte default data area as the sibling above, so the two agree.
    inline proceduralshot::proceduralshot(const Attrib::RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_PROCEDURALSHOT_CLASS = -534300752; // Attrib::ClassName::proceduralshot (0xE02737B0)
        if (GetClass() != KI_PROCEDURALSHOT_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROCEDURALSHOT_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
