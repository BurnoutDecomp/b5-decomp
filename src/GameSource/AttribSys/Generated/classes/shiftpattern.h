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
#include "BrnCommonTypes.h"

#include <cstring>

namespace Attrib
{
namespace Gen
{
    class shiftpattern : private Instance
    {
    public:
        explicit shiftpattern(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        void ChangeWithDefault(void* lpRefSpec)
        {
            Instance::ChangeWithDefault(static_cast<RefSpec*>(lpRefSpec));
        }

        Matrix44 UpDisengageFallCurve() const { return Matrix(0x00u); }
        f32 UpEngageTime() const { return Float(0x80u); }
        f32 UpEngageRpm() const { return Float(0x84u); }
        f32 UpEngageAttackTime() const { return Float(0x88u); }
        f32 UpEngageAttackGain() const { return Float(0x8Cu); }
        f32 UpDisengageFallTime() const { return Float(0x90u); }
        f32 UpDisengageFallRpm() const { return Float(0x94u); }
        f32 RpmLfoFrequency() const { return Float(0x98u); }
        f32 RpmLfoDecayTime() const { return Float(0x9Cu); }
        f32 RpmLfoAmplitude(s32 liGear) const
        {
            const s32 liClamped = liGear < 1 ? 1 : (liGear > 6 ? 6 : liGear);
            return Float(0xA0u + static_cast<u32>(6 - liClamped) * 4u);
        }
        f32 VolLfoFrequency() const { return Float(0xB8u); }
        f32 VolLfoDecayTime() const { return Float(0xBCu); }
        f32 VolLfoAmplitude(s32 liGear) const
        {
            const s32 liClamped = liGear < 1 ? 1 : (liGear > 6 ? 6 : liGear);
            return Float(0xC0u + static_cast<u32>(6 - liClamped) * 4u);
        }
        f32 DownEngageRiseTime() const { return Float(0xD8u); }
        f32 DownEngageRiseRpm() const { return Float(0xDCu); }
        f32 DownEngageFallTime() const { return Float(0xE0u); }
        f32 DownEngageFallRpm() const { return Float(0xE4u); }
        f32 DownDisengageFallTime() const { return Float(0xE8u); }
        f32 DownDisengageFallRpm() const { return Float(0xECu); }

    private:
        f32 Float(u32 luOffset) const
        {
            if (!mpAttributeData)
                return 0.0f;
            f32 lfValue = 0.0f;
            std::memcpy(&lfValue, static_cast<const u8*>(mpAttributeData) + luOffset,
                        sizeof(lfValue));
            return lfValue;
        }

        Matrix44 Matrix(u32 luOffset) const
        {
            Matrix44 lValue;
            lValue.SetZero();
            if (mpAttributeData)
                std::memcpy(&lValue, static_cast<const u8*>(mpAttributeData) + luOffset,
                            sizeof(lValue));
            return lValue;
        }
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
