#pragma once

// Attrib::Gen::b4blurasset -- generated AttribSys class (the "b4blur" post-effect
// asset: BrnEffects blur pass parameters). The generated accessor / `using
// Instance::...` API is inlined away at the call site, so the constructor is the
// only b4blurasset entry point in the build (same minimal-recon convention as the
// sibling generated classes shotgroup / iceanim / surfacelist / debrisparams).
// Derives from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::b4blurasset::b4blurasset @ 0x8227FA30
//
//   * the ctor resolves the b4blurasset class collection via
//     FindCollection(1935857871) -- unlike shotgroup, the X360 call passes no
//     owner (r4 is left unset before the bl; the owner in r5=a3 is only threaded
//     into the later Instance ctor, not into FindCollection here) -- and chains
//     Instance(a1 /*this*/, Collection, a3 /*owner*/) over it.
//   * then gives the instance a default data area (0x60 bytes) if construction
//     left it without one. Note: unlike iceanim/surfacelist/debrisparams the X360
//     ctor has NO AssertOnClassCheck -- so none is emitted here.
//   * called by BrnEffects::EffectsModule::GenerateRenderRequests and
//     BrnEffects::BlurData::Construct (both still [todo] elsewhere in the queue).
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class b4blurasset : private Instance
    {
    public:
        // The b4blurasset class/collection key the ctor resolves via FindCollection
        // (0x7362D8CF == 1935857871 -- the low 32 bits of the 64-bit immediate
        // 0xEF9F6F04_7362D8CF the X360 stages in r3; Hex-Rays/the FindCollection(int)
        // shape surface the low half, matching the shotgroup sibling key-staging).
        static const int KI_B4BLURASSET_CLASS = 1935857871;

        // The FULL 64-bit class key Attrib::FindCollection resolves against -- the
        // doubleword the X360 ctor stages in r3 with lis/ori + insrdi. KI_KI_B4BLURASSET_CLASS
        // above is only its LOW word (which is what Hex-Rays surfaces, and what this
        // header used to pass to the old one-key FindCollection(int)); the class
        // registry is keyed by the whole doubleword, so the low word alone MISSES.
        static const u64 KU_B4BLURASSET_CLASS_KEY = 0xEF9F6F047362D8CFULL;

        explicit b4blurasset(void* lpOwner = nullptr);
    };

    // X360 ctor @0x8227FA30: Collection = FindCollection(1935857871); chain the
    // Instance ctor over it (Instance(this, Collection, lpOwner)); then give the
    // instance a default data area (0x60 bytes) if construction left it without one.
    inline b4blurasset::b4blurasset(void* lpOwner)
                // FLAG (collection key): the X360 ctor never writes r4, so the CALLER's key
        // argument passes straight through to FindCollection as the collection key.
        // This ctor does not model that parameter yet (no call site in this repo
        // supplies one), so it resolves the class's collection key 0 -- exactly what
        // the previous `FindCollection(KI_..., nullptr)` form did. Add the parameter
        // when a real call site needs a named collection.
    : Instance(FindCollection(KU_B4BLURASSET_CLASS_KEY, 0), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x60u);
    }
}
}
