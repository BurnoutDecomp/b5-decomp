#pragma once

// Attrib::Gen::debrisparams — generated AttribSys class (debris-burst parameters).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::debrisparams::debrisparams @ 0x8227E9A0
//
// class-sourced (no Feb-2007 leak / DWARF for this TU) — same generated-ctor pattern as
// surfacelist. The X360 build inlines the generated accessor / `using` API away, so the
// constructor is the only debrisparams function in the ledger (minimal X360-faithful
// recon). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class debrisparams : private Instance
    {
    public:
        explicit debrisparams(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::debrisparams,
    // then give the instance a default data area (0xE0 bytes) if it has none.
    inline debrisparams::debrisparams(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_DEBRISPARAMS_CLASS = 651697503; // Attrib::ClassName::debrisparams
        if (GetClass() != KI_DEBRISPARAMS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_DEBRISPARAMS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xE0u);
    }
}
}
