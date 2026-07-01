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
        // The cameradefaults class key the ctor resolves its collection against
        // (0x5F206F31 == 1595961137).
        static const int KI_CAMERADEFAULTS_CLASS = 1595961137;

        // Construct over the cameradefaults collection. lpOwner is the optional owning
        // object the AttribSys collection resolve threads through.
        explicit cameradefaults(void* lpOwner = nullptr);
    };

    // X360 ctor @0x82208770: Collection = FindCollection(1595961137, owner); chain the
    // Instance ctor over it; then give the instance a default data area (0x38 bytes) if
    // it has none (lwz r11,4(r31) / cmplwi / bne-skip / DefaultDataArea(0x38) /
    // stw r11,4(r31)). No class-check assert appears in this ctor's asm (unlike the
    // shotgroup/iceanim/surfacelist/debrisparams siblings).
    inline cameradefaults::cameradefaults(void* lpOwner)
        : Instance(FindCollection(KI_CAMERADEFAULTS_CLASS, lpOwner), lpOwner)
    {
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x38u);
    }
}
}
