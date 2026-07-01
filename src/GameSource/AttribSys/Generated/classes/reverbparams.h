#pragma once

// Attrib::Gen::reverbparams — generated AttribSys class (reverb-effect parameters).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::reverbparams::reverbparams @ 0x8269BD30
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams / propscrashbinlist / surfacelist. The X360 build inlines the
// generated accessor / `using` API away, so the constructor is the only reverbparams
// function in the ledger (minimal X360-faithful recon). Derives from Attrib::Instance.
// Used by BrnSound::Vehicles::Environment::ReverbEffect::UpdateParams.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class reverbparams : private Instance
    {
    public:
        explicit reverbparams(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::reverbparams,
    // then give the instance a default data area (0x10 bytes) if it has none.
    inline reverbparams::reverbparams(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_REVERBPARAMS_CLASS = 1672882824; // Attrib::ClassName::reverbparams
        if (GetClass() != KI_REVERBPARAMS_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_REVERBPARAMS_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
