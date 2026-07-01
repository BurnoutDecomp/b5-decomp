#pragma once

// Attrib::Gen::presentationcomponent -- generated AttribSys class (presentation
// component attributes). The generated accessor / `using` API is inlined away at
// the call sites, so the constructor is the only presentationcomponent function in
// the build (a minimal generated-ctor recon, same shape as the sibling generated
// classes surfacelist / debrisparams / iceanim). Derives from Attrib::Instance.
// Only known caller: BrnSound::Logic::HUDEffect::HUDEffect.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class presentationcomponent : private Instance
    {
    public:
        explicit presentationcomponent(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::presentationcomponent (skipping the assert when the class is
    // unset/0), then give the instance a default data area (0xE30 bytes) if it
    // has none.
    inline presentationcomponent::presentationcomponent(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PRESENTATIONCOMPONENT_CLASS = -802641548; // Attrib::ClassName::presentationcomponent
        if (GetClass() != KI_PRESENTATIONCOMPONENT_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PRESENTATIONCOMPONENT_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0xE30u);
    }
}
}
