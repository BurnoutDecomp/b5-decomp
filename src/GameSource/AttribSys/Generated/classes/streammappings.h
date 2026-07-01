#pragma once

// Attrib::Gen::streammappings — generated AttribSys class (the "stream mappings"
// attribute schema). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::streammappings::streammappings @ 0x8269E518
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only streammappings function in the ledger — this is therefore a
// minimal, X360-faithful recon (class identity + ctor). Unlike some sibling Gen classes
// (e.g. surfacelist/languagestreamconfiguration), this ctor does NOT call
// DefaultDataArea() — the asm never touches mpAttributeData; every path returns r31 (==
// this) after only validating the class tag. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class streammappings : private Instance
    {
    public:
        explicit streammappings(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, then assert the collection's class is
    // ClassName::streammappings (skipping the check when GetClass() == 0, i.e. an
    // instance with no attribute data yet). No DefaultDataArea call in this ctor.
    inline streammappings::streammappings(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_STREAMMAPPINGS_CLASS = static_cast<int>(3558142457u); // Attrib::ClassName::streammappings (0xD414F1F9)
        if (GetClass() != KI_STREAMMAPPINGS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_STREAMMAPPINGS_CLASS, GetCollection());
    }
}
}
