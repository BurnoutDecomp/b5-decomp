#pragma once

// Attrib::Gen::proceduralshake -- generated AttribSys class (procedural camera-shake
// attributes: Pitch/Roll/Yaw Frequency+Scale + ShakeMethod). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::proceduralshake::proceduralshake @ 0x8220AE30
//
// The X360 build inlines the generated accessor / `using Instance::...` API away, so the
// constructor is the only proceduralshake function in the ledger (minimal, X360-faithful
// recon), same shape as the sibling generated classes debrisparams / iceanim /
// surfacelist. Derives from Attrib::Instance. Consumed by
// BrnDirector::KeyAnimShakeController::Update.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

namespace Attrib
{
namespace Gen
{
    class proceduralshake : private Instance
    {
    public:
        explicit proceduralshake(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
    };

    // Chain the Instance ctor, assert the collection's class is ClassName::proceduralshake
    // (skipping the assert when the class matches or is unset/0), then give the instance a
    // default data area (0x1C bytes) if it has none.
    inline proceduralshake::proceduralshake(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_PROCEDURALSHAKE_CLASS = -1191313409; // 0xB8FDFFFF, Attrib::ClassName::proceduralshake
        if (GetClass() != KI_PROCEDURALSHAKE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_PROCEDURALSHAKE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x1Cu);
    }
}
}
