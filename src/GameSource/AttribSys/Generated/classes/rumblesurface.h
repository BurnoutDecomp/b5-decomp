#pragma once

// Attrib::Gen::rumblesurface — generated AttribSys class (a road surface's pad-rumble envelope:
// the heavy LEFT / light RIGHT motor ADSR pair, the speed band the volume scales across and the
// rumble priority). Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::rumblesurface::rumblesurface(const RefSpec&, owner) @ 0x82364828
//
// ⭐ FX-RUMBLE3 2026-09-24 (crash-parity G10-D6): the ctor at 0x82364828 is the REFSPEC overload
// (DWARF rumblesurface.h:21) -- its first call is sub_8280A248, Instance(const RefSpec&, owner),
// and its callers hand it `surfaceLayout + 0x28` (the surface's RumbleSurface ref; see surface.h).
// The earlier note here attributed that address to the Collection* overload. The class check is the
// 64-bit key 0x540C6D17_14E37D72 (`insrdi` + `cmpld` @0x82364844..0x82364858); this tree compares the
// low word, as every sibling generated class does (surface.h, audiosurface.h).
//
// THE LAYOUT (0x3C bytes -- DefaultDataArea(0x3C) @0x823648B4). The DWARF (rumblesurface.h:73..:174)
// names fifteen attributes, fourteen Float and one Int32; their offsets are read off the console's
// only reader, RumbleManager::UpdateSurfaceRumble @0x82378AE0, which copies them lane by lane into a
// CgsInput::InputIO::JoltEffect {mLowFreqJoltData, mHighFreqJoltData}, each {attack, decay,
// sustain time, release, peak, sustain speed} (0x82378F18..0x82378F88 and 0x82379250..0x823792D4):
//     low  (left, heavy motor):  +0x38 +0x34 +0x24 +0x2C +0x30 +0x28
//     high (right, light motor): +0x18 +0x14 +0x04 +0x0C +0x10 +0x08
//     +0x00 `lwz` -> the event's priority (and the `blt` gate at 0x82378D2C);
//     +0x1C / +0x20 -> the lower / upper bound of the |speed| Clamp (the volume's speed band).
// The generator lays the fields out in REVERSE alphabetical order of their names, which this reading
// reproduces exactly (RumblePriority, RightMotor{SustainTime, SustainSpeed, ReleaseTime, PeakSpeed,
// DecayTime, AttackTime}, MinSpeedForRumble, MaxSpeedForRumble, LeftMotor{...}) -- every lane agrees.
// These are serialised data offsets: they do not widen on the x64 host.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"

#include <cstddef>   // offsetof (the layout pins below)

namespace Attrib
{
namespace Gen
{
    class rumblesurface : private Instance
    {
    public:
        struct _LayoutStruct
        {
            s32 miRumblePriority;          // +0x00  RumblePriority          (Int32)
            f32 mfRightMotorSustainTime;   // +0x04  RightMotorSustainTime
            f32 mfRightMotorSustainSpeed;  // +0x08  RightMotorSustainSpeed
            f32 mfRightMotorReleaseTime;   // +0x0C  RightMotorReleaseTime
            f32 mfRightMotorPeakSpeed;     // +0x10  RightMotorPeakSpeed
            f32 mfRightMotorDecayTime;     // +0x14  RightMotorDecayTime
            f32 mfRightMotorAttackTime;    // +0x18  RightMotorAttackTime
            f32 mfMinSpeedForRumble;       // +0x1C  MinSpeedForRumble
            f32 mfMaxSpeedForRumble;       // +0x20  MaxSpeedForRumble
            f32 mfLeftMotorSustainTime;    // +0x24  LeftMotorSustainTime
            f32 mfLeftMotorSustainSpeed;   // +0x28  LeftMotorSustainSpeed
            f32 mfLeftMotorReleaseTime;    // +0x2C  LeftMotorReleaseTime
            f32 mfLeftMotorPeakSpeed;      // +0x30  LeftMotorPeakSpeed
            f32 mfLeftMotorDecayTime;      // +0x34  LeftMotorDecayTime
            f32 mfLeftMotorAttackTime;     // +0x38  LeftMotorAttackTime
        };

        explicit rumblesurface(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        // @0x82364828 -- DWARF rumblesurface.h:21 `rumblesurface(const Attrib::RefSpec&, uint32_t)`.
        explicit rumblesurface(const RefSpec& lrRefSpec, void* lpOwner = nullptr);

        // The generated const accessors (DWARF rumblesurface.h:75..:173): each returns the layout
        // field by reference, as the X360's inlined reads do (a plain load off mpAttributeData).
        const s32& RumblePriority() const         { return Layout().miRumblePriority; }
        const f32& RightMotorSustainTime() const  { return Layout().mfRightMotorSustainTime; }
        const f32& RightMotorSustainSpeed() const { return Layout().mfRightMotorSustainSpeed; }
        const f32& RightMotorReleaseTime() const  { return Layout().mfRightMotorReleaseTime; }
        const f32& RightMotorPeakSpeed() const    { return Layout().mfRightMotorPeakSpeed; }
        const f32& RightMotorDecayTime() const    { return Layout().mfRightMotorDecayTime; }
        const f32& RightMotorAttackTime() const   { return Layout().mfRightMotorAttackTime; }
        const f32& MinSpeedForRumble() const      { return Layout().mfMinSpeedForRumble; }
        const f32& MaxSpeedForRumble() const      { return Layout().mfMaxSpeedForRumble; }
        const f32& LeftMotorSustainTime() const   { return Layout().mfLeftMotorSustainTime; }
        const f32& LeftMotorSustainSpeed() const  { return Layout().mfLeftMotorSustainSpeed; }
        const f32& LeftMotorReleaseTime() const   { return Layout().mfLeftMotorReleaseTime; }
        const f32& LeftMotorPeakSpeed() const     { return Layout().mfLeftMotorPeakSpeed; }
        const f32& LeftMotorDecayTime() const     { return Layout().mfLeftMotorDecayTime; }
        const f32& LeftMotorAttackTime() const    { return Layout().mfLeftMotorAttackTime; }

    private:
        const _LayoutStruct& Layout() const
        {
            return *static_cast<const _LayoutStruct*>(GetLayoutPointer());
        }
    };

    static_assert(sizeof(rumblesurface::_LayoutStruct) == 0x3C, "rumblesurface layout is 0x3C bytes (DefaultDataArea(0x3C) @0x823648B4)");
    static_assert(offsetof(rumblesurface::_LayoutStruct, mfMinSpeedForRumble) == 0x1C, "MinSpeedForRumble @+0x1C (lfs 0x1C @0x82378F18)");
    static_assert(offsetof(rumblesurface::_LayoutStruct, mfMaxSpeedForRumble) == 0x20, "MaxSpeedForRumble @+0x20 (lfs 0x20 @0x82378F80)");
    static_assert(offsetof(rumblesurface::_LayoutStruct, mfLeftMotorAttackTime) == 0x38, "LeftMotorAttackTime @+0x38 (lfs 0x38 @0x82378F1C)");

    // Chain the Instance ctor, assert the collection's class is ClassName::rumblesurface,
    // then give the instance a default data area (0x3C bytes) if it has none. (DWARF
    // rumblesurface.h:18; the generated-ctor pattern of the RefSpec form below.)
    inline rumblesurface::rumblesurface(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_RUMBLESURFACE_CLASS = static_cast<int>(0x14E37D72u); // Attrib::ClassName::rumblesurface (350453106)
        if (GetClass() != KI_RUMBLESURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_RUMBLESURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x3Cu);
    }

    // @0x82364828: Instance(const RefSpec&, owner) (sub_8280A248), the class check against
    // 0x540C6D17_14E37D72 (AssertOnClassCheck unless the class is it or 0), then the 0x3C-byte
    // default data area when the ref resolved to nothing (0x823648A8..0x823648C0).
    inline rumblesurface::rumblesurface(const RefSpec& lrRefSpec, void* lpOwner)
        : Instance(lrRefSpec, lpOwner)
    {
        static const int KI_RUMBLESURFACE_CLASS = static_cast<int>(0x14E37D72u); // low word of 0x540C6D1714E37D72
        if (GetClass() != KI_RUMBLESURFACE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_RUMBLESURFACE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x3Cu);
    }
}
}
