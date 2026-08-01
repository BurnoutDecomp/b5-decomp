#pragma once

// Attrib::Gen::aftertouchcam -- generated AttribSys class (aftertouch-camera behaviour
// parameters). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::aftertouchcam::aftertouchcam @ 0x82206680
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling generated classes debrisparams / surfacelist. The X360 build
// inlines the generated accessor / `using` API away, so the constructor is the only
// aftertouchcam function in the ledger (minimal X360-faithful recon). Derives from
// Attrib::Instance. Instantiated by
// AbstractPool<...>::AllocateVoid<BehaviourAftertouchCam>() (Camera::BehaviourAftertouchCam's
// pool allocator), per the dossier's "called by".
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class aftertouchcam : private Instance
    {
    public:
        // The FULL 64-bit class key, as the AttribSys class registry hashes it (the whole
        // doubleword). Recorded here 2026-08-01 because DirectorResourceManager::Prepare
        // @0x8225E3FC builds a bare Attrib::RefSpec over it -- `lis 0x6323 / ori 0x88D6 /
        // lis 0x75E6 / ori 0x2FC1 / insrdi` -- to seed its mAfterTouchCam member; the
        // low word alone would MISS, exactly like the shotgroup/cameradefaults keys.
        static const u64 KU_AFTERTOUCHCAM_CLASS_KEY = 0x75E62FC1632388D6ULL;

        explicit aftertouchcam(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::aftertouchcam,
    // then give the instance a default data area (0x18 bytes) if it has none.
    inline aftertouchcam::aftertouchcam(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        // X360 asm stages a 64-bit compare (cmpld) built from two lis/ori halves combined
        // via insrdi: high word 0x75E62FC1 (dead -- GetClass() returns a 32-bit int so only
        // the low word can ever match), low word 0x632388D6 == 1663273174, the actual class
        // id compared (matches Hex-Rays' own literal in the pseudocode).
        static const int KI_AFTERTOUCHCAM_CLASS = 1663273174; // Attrib::ClassName::aftertouchcam
        if (GetClass() != KI_AFTERTOUCHCAM_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_AFTERTOUCHCAM_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x18u);
    }
}
}
