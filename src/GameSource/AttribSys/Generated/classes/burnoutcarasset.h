#pragma once

// Attrib::Gen::burnoutcarasset -- generated AttribSys class (per-car "burnout car
// asset" attribute schema: the top-level attribute block referencing a car's model,
// physics, sound, and gameplay sub-attributes). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::burnoutcarasset::burnoutcarasset @ 0x822048F0
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) -- same generated-ctor
// pattern as the sibling generated classes debrisparams / surfacelist / iceanim. The
// X360 build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only burnoutcarasset function in the ledger (minimal X360-faithful
// recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class burnoutcarasset : private Instance
    {
    public:
        explicit burnoutcarasset(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::burnoutcarasset
    // (skipping the assert when the class is unset/0), then give the instance a default
    // data area (0x228 bytes) if it has none.
    inline burnoutcarasset::burnoutcarasset(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_BURNOUTCARASSET_CLASS = -206702987; // Attrib::ClassName::burnoutcarasset
        if (GetClass() != KI_BURNOUTCARASSET_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_BURNOUTCARASSET_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x228u);
    }
}
}
