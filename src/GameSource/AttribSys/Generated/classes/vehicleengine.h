#pragma once

// Attrib::Gen::vehicleengine — generated AttribSys class (vehicle engine attributes).
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Attrib::Gen::vehicleengine::vehicleengine @ 0x82696F38
//
// class-sourced (no Feb-2007 partial source / DWARF for this TU) — same generated-ctor
// pattern as debrisparams/surfacelist/physicsvehiclebaseattribs. The X360 build inlines
// the generated accessor / `using Instance::…` API away, so the constructor is the only
// vehicleengine function in the ledger (minimal X360-faithful recon). Derives from
// Attrib::Instance.
#include "SDKs/Packages/AttribSys/1.2.1.2/AttribSys/runtime/common/attribinstance.h"
#include "BrnCommonTypes.h"

#include <cstring>

namespace Attrib
{
namespace Gen
{
    class vehicleengine : private Instance
    {
    public:
        explicit vehicleengine(Collection* lpCollection = nullptr, void* lpOwner = nullptr);

        void Change(Collection* lpCollection) { Instance::Change(lpCollection); }
        u64 CollectionKey() const { return Instance::GetCollection(); }

        // These accessors are inlined in ARTIST.  Their consumer instructions read the
        // generated layout directly (for example HybridExhaustControl::Attach
        // @0x82699848 and DualGinsuEffect::UpdateParams @0x826B37A0); there is no
        // Collection::GetData/hash lookup on this path.
        const char* LoopModel() const { return Text(0x15Cu); }
        const char* GinsuFileDecel() const { return Text(0x160u); }
        const char* GinsuFileAccel() const { return Text(0x164u); }
        const char* SweetenersAssetName() const { return Text(0x140u); }
        const char* WhineAssetName() const { return Text(0x150u); }
        const char* TurboAssetName() const { return Text(0x154u); }
        const char* BoostBank() const { return Text(0x158u); }

        u64 SweetenersAsset() const
        {
            if (!mpAttributeData)
                return 0;
            u64 luAsset = 0;
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            std::memcpy(&luAsset, lpData + 0x148u, sizeof(luAsset));
            return luAsset;
        }

        // ARTIST vehicleengine BinData layout.  These offsets are consumed directly
        // by the vehicle-audio code (for example DualGinsuExhaustEffect::UpdateParams
        // @ 0x826B3770 and HybridExhaustControl::UpdateMix @ 0x826CC878).
        Matrix44 VolumeOverRPM() const { return Matrix(0x000u); }
        Matrix44 PhysicsRpmMap() const { return Matrix(0x040u); }

        Vector4 INF_TimeMulti() const { return Vector(0x0E0u); }
        Vector4 INF_TimeBeforeReachingMaxSpeed() const { return Vector(0x0F0u); }
        Vector4 INF_RpmDropPercentage() const { return Vector(0x100u); }
        Vector4 INF_RpmDrop() const { return Vector(0x110u); }
        Vector4 INF_MaxRpmBeforeStart() const { return Vector(0x120u); }
        Vector4 INF_MaxRpmBeforeShift() const { return Vector(0x130u); }

        // ARTIST SweetenersEffect::UpdateSweetenerInfo @ 0x826F33C0 loads the
        // two four-element count vectors and their matching gain vectors directly
        // from these generated-layout offsets.
        Vector4 SweetenerVolumes1() const { return Vector(0x090u); }
        Vector4 SweetenerVolumes0() const { return Vector(0x0A0u); }
        Vector4 SweetenerCounts1() const { return Vector(0x0C0u); }
        Vector4 SweetenerCounts0() const { return Vector(0x0D0u); }

        f32 RotationVolRear() const { return Float(0x174u); }
        f32 RotationVolFront() const { return Float(0x178u); }
        f32 RotationMixRear() const { return Float(0x17Cu); }
        f32 RotationMixFront() const { return Float(0x180u); }

        s32 ShiftPatternType() const { return Int32(0x170u); }

        f32 MinRpm() const { return Float(0x188u); }
        f32 MaxRpm() const { return Float(0x18Cu); }
        f32 MasterGain() const { return Float(0x190u); }
        f32 MasterCarVolume() const { return Float(0x194u); }
        f32 LoopModelDecelSmallRpmGain() const { return Float(0x198u); }
        f32 LoopModelDecelLargeRpmGain() const { return Float(0x19Cu); }
        f32 LoopModelDecelGain() const { return Float(0x1A0u); }
        f32 LoopModelAccelSmallRpmGain() const { return Float(0x1A4u); }
        f32 LoopModelAccelLargeRpmGain() const { return Float(0x1A8u); }
        f32 LoopModelAccelGain() const { return Float(0x1ACu); }
        f32 IdleRpm() const { return Float(0x1B0u); }
        f32 IdleGain() const { return Float(0x1B4u); }
        s32 GinsuSampleRate() const { return Int32(0x1B8u); }
        f32 GinsuDecelSmallRpmGain() const { return Float(0x1BCu); }
        f32 GinsuDecelLargeRpmGain() const { return Float(0x1C0u); }
        f32 GinsuDecelGain() const { return Float(0x1C4u); }
        f32 GinsuAccelSmallRpmGain() const { return Float(0x1C8u); }
        f32 GinsuAccelNegSmallRpmGain() const { return Float(0x1CCu); }
        f32 GinsuAccelNegLargeRpmGain() const { return Float(0x1D0u); }
        f32 GinsuAccelLargeRpmGain() const { return Float(0x1D4u); }
        f32 GinsuAccelGain() const { return Float(0x1D8u); }
        f32 EQ_Peaking_Q() const { return Float(0x1DCu); }
        f32 EQ_Peaking_Gain() const { return Float(0x1E0u); }
        f32 EQ_Peaking_Freq() const { return Float(0x1E4u); }
        f32 EQ_LowShelf_Gain() const { return Float(0x1E8u); }
        f32 EQ_LowShelf_Freq() const { return Float(0x1ECu); }
        f32 EQ_HighShelf_Gain() const { return Float(0x1F0u); }
        f32 EQ_HighShelf_Freq() const { return Float(0x1F4u); }
        f32 DecelMinRpm() const { return Float(0x210u); }
        f32 DecelMaxRpm() const { return Float(0x214u); }
        s32 DecelGinsuSampleRate() const { return Int32(0x218u); }
        f32 DecelDeltaRpmThreshold() const { return Float(0x21Cu); }
        f32 AccelDeltaRpmThreshold() const { return Float(0x220u); }

    private:
        const char* Text(u32 auOffset) const
        {
            if (!mpAttributeData)
                return nullptr;

            // The ARTIST vehicleengine data area retains its 32-bit generated layout on
            // disk.  Vault::Initialize resolves each PtrN entry into one of these four-byte
            // pointer slots.  The PC resource arena is deliberately allocated below 4 GiB;
            // widen the resolved slot only after reading it, so adjacent generated fields
            // keep their original offsets.
            u32 luAddress = 0;
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            std::memcpy(&luAddress, lpData + auOffset, sizeof(luAddress));
            return reinterpret_cast<const char*>(static_cast<uintptr_t>(luAddress));
        }

        f32 Float(u32 auOffset) const
        {
            if (!mpAttributeData)
                return 0.0f;
            f32 lfValue = 0.0f;
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            std::memcpy(&lfValue, lpData + auOffset, sizeof(lfValue));
            return lfValue;
        }

        s32 Int32(u32 auOffset) const
        {
            if (!mpAttributeData)
                return 0;
            s32 liValue = 0;
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            std::memcpy(&liValue, lpData + auOffset, sizeof(liValue));
            return liValue;
        }

        Matrix44 Matrix(u32 auOffset) const
        {
            Matrix44 lValue;
            lValue.SetZero();
            if (mpAttributeData)
            {
                const u8* lpData = static_cast<const u8*>(mpAttributeData);
                std::memcpy(&lValue, lpData + auOffset, sizeof(lValue));
            }
            return lValue;
        }

        Vector4 Vector(u32 auOffset) const
        {
            Vector4 lValue;
            lValue.SetZero();
            if (mpAttributeData)
            {
                const u8* lpData = static_cast<const u8*>(mpAttributeData);
                std::memcpy(&lValue, lpData + auOffset, sizeof(lValue));
            }
            return lValue;
        }
    };

    // Chain the Instance ctor, assert the collection's class is
    // ClassName::vehicleengine, then give the instance a default data area
    // (0x230 bytes) if construction left it without one.
    inline vehicleengine::vehicleengine(Collection* lpCollection, void* lpOwner)
        : Instance(lpCollection, lpOwner)
    {
        static const int KI_VEHICLEENGINE_CLASS = 1210889151; // 0x482CB3BF, Attrib::ClassName::vehicleengine
        if (GetClass() != KI_VEHICLEENGINE_CLASS && GetClass() != 0)
            AssertOnClassCheck(GetClass(), KI_VEHICLEENGINE_CLASS, GetCollection());
        if (!mpAttributeData)
            mpAttributeData = DefaultDataArea(0x230u);
    }
}
}
