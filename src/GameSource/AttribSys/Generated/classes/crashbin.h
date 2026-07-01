#pragma once

// Attrib::Gen::crashbin — generated AttribSys class (crash-bin attribute schema).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::crashbin::crashbin @ 0x82697108
//
// The X360 build inlines the generated accessor / `using Instance::…` API away, so the
// constructor is the only crashbin function in the ledger — this is therefore a
// minimal, X360-faithful recon (class identity + ctor), same generated-ctor pattern as
// debrisparams / propscrashbinlist / surfacelist. Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class crashbin : private Instance
    {
    public:
        explicit crashbin(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::crashbin,
    // then give the instance a default data area (0x190 bytes) if it has none.
    inline crashbin::crashbin(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_CRASHBIN_CLASS = -27534889; // Attrib::ClassName::crashbin (0xFE5BD9D7)
        if (GetClass() != KI_CRASHBIN_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_CRASHBIN_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x190u);
    }
}
}
