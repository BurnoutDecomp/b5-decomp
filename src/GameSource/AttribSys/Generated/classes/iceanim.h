#pragma once

// Attrib::Gen::iceanim -- generated AttribSys class (ICE camera-take animation
// attributes). The generated accessor / `using` API is inlined away at the call
// sites, so the constructor is the only iceanim entry point in the build (a
// minimal generated-ctor recon, same shape as the sibling generated classes
// surfacelist / debrisparams). Derives from Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class iceanim : private Instance
    {
    public:
        explicit iceanim(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::iceanim
    // (skipping the assert when the class is unset/0), then give the instance a
    // default data area (0x10 bytes) if it has none.
    inline iceanim::iceanim(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_ICEANIM_CLASS = -1449672210; // Attrib::ClassName::iceanim
        if (GetClass() != KI_ICEANIM_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_ICEANIM_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }
}
}
