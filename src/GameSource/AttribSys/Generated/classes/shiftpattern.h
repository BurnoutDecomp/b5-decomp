#pragma once

// Attrib::Gen::shiftpattern — generated AttribSys class (vehicle shift-pattern
// attributes). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::shiftpattern::shiftpattern @ 0x82696468
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as surfacelist/debrisparams. The X360 build inlines the generated accessor /
// `using` API away, so the constructor is the only shiftpattern function in the ledger
// (minimal X360-faithful recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class shiftpattern : private Instance
    {
    public:
        explicit shiftpattern(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::shiftpattern,
    // then give the instance a default data area (0xF0 bytes) if it has none.
    inline shiftpattern::shiftpattern(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_SHIFTPATTERN_CLASS = 1819927603; // Attrib::ClassName::shiftpattern (0x6C79E433)
        if (GetClass() != KI_SHIFTPATTERN_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_SHIFTPATTERN_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xF0u);
    }
}
}
