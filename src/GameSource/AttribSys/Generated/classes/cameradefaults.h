#pragma once

// Attrib::Gen::cameradefaults -- generated AttribSys class (default camera parameters
// used by BrnDirector::DirectorResourceManager::Prepare). The generated accessor /
// `using Instance::...` API is inlined away at the call site, so the constructor is the
// only cameradefaults function in the X360 ledger (same minimal-recon convention as the
// sibling generated classes shotgroup / iceanim / surfacelist / debrisparams). Derives
// from Attrib::Instance.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::cameradefaults::cameradefaults @ 0x82208770
//
//   * the ctor resolves the cameradefaults class collection via
//     FindCollection(1595961137 /* 0x5F206F31 */, owner), chains Instance(Collection,
//     owner) over it, then (unlike the class-checked siblings) gives the instance a
//     default data area (0x38 bytes) if construction left it without one -- no
//     AssertOnClassCheck is emitted in this ctor's asm.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"   // Attrib::FindCollection (canonical)

namespace Attrib
{
namespace Gen
{
    class cameradefaults : private Instance
    {
    public:
        // The cameradefaults class key, LOW word (0x5F206F31 == 1595961137).
        static const int KI_CAMERADEFAULTS_CLASS = 1595961137;

        // The FULL 64-bit class key Attrib::FindCollection resolves against. RECOVERED
        // 2026-07-31 from the ctor @0x82208770: `lis 0x5F20 / ori 0x6F31 / lis 0x095B /
        // ori 0x375E / insrdi r3,r11,32,0` -> 0x095B375E_5F206F31. (This one was not
        // previously recorded anywhere in the tree.)
        static const u64 KU_CAMERADEFAULTS_CLASS_KEY = 0x095B375E5F206F31ULL;

        // Construct over the cameradefaults collection named by luCollectionKey. lpOwner
        // is the optional owning object threaded through to Attrib::Instance.
        //
        // ⚠️ ARITY FIXED 2026-07-31. The X360 symbol is (this, key, owner) -- r5 is the
        // owner and r4 (the key) is never written by the ctor, so it passes straight
        // through to FindCollection as the collection key, exactly like shotgroup. This
        // declaration used to take only the owner, which meant the caller's key was
        // dropped and the owner pointer was handed to FindCollection in its place.
        explicit cameradefaults(Attrib::Key luCollectionKey = 0, void* lpOwner = nullptr);
    };

    // X360 ctor @0x82208770: Collection = FindCollection(0x095B375E_5F206F31, r4); chain
    // the Instance ctor over it; then give the instance a default data area (0x38 bytes)
    // if it has none (lwz r11,4(r31) / cmplwi / bne-skip / DefaultDataArea(0x38) /
    // stw r11,4(r31)). No class-check assert appears in this ctor's asm (unlike the
    // shotgroup/iceanim/surfacelist/debrisparams siblings).
    inline cameradefaults::cameradefaults(Attrib::Key luCollectionKey, void* lpOwner)
        : Instance(FindCollection(KU_CAMERADEFAULTS_CLASS_KEY, luCollectionKey), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x38u);
    }
}
}
