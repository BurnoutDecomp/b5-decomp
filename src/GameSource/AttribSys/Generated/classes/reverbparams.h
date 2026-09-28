#pragma once
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/vechashmap.h"

namespace Attrib { namespace Gen {
// ARTIST 8269BD30 calls Instance(const RefSpec&, owner), compares the full
// 64-bit class key, and supplies a 16-byte default layout. Declaration names
// come from DecFIGS reverbparams.h; loads at 826D1634..826D1650 pin the fields.
class reverbparams : private Instance
{
public:
    struct _LayoutStruct {
        f32 Time;
        f32 SpaceSize;
        f32 Gain;
        f32 Brightness;
    };

    explicit reverbparams(const RefSpec& arRef, void* apOwner = nullptr)
        : Instance(arRef, apOwner)
    {
        static const u64 KU_CLASS = 0xA59AD4BD63B62A88ull;
        if (GetClass() != KU_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KU_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x10u);
    }

    // Generated GetClass (DWARF reverbparams.h:25), with the exact Instance
    // accessor inlined (ARTIST 82802F18). The legacy shared declaration still
    // returns int, so calling that truncates this 64-bit class identity.
    u64 GetClass() const
    {
        const Class* lpClass = mpCollection ? mpCollection->mpClass : nullptr;
        return lpClass ? lpClass->GetKey() : 0;
    }

    const f32& Time() const { return static_cast<const _LayoutStruct*>(mpAttributeData)->Time; }
    const f32& SpaceSize() const { return static_cast<const _LayoutStruct*>(mpAttributeData)->SpaceSize; }
    const f32& Gain() const { return static_cast<const _LayoutStruct*>(mpAttributeData)->Gain; }
    const f32& Brightness() const { return static_cast<const _LayoutStruct*>(mpAttributeData)->Brightness; }
};
}}
