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

        // These accessors are inlined in ARTIST.  Their consumer instructions read the
        // generated layout directly (for example HybridExhaustControl::Attach
        // @0x82699848 and DualGinsuEffect::UpdateParams @0x826B37A0); there is no
        // Collection::GetData/hash lookup on this path.
        const char* LoopModel() const { return Text(0x15Cu); }
        const char* GinsuFileDecel() const { return Text(0x160u); }
        const char* GinsuFileAccel() const { return Text(0x164u); }
        const char* SweetenersAssetName() const { return Text(0x140u); }

        u64 SweetenersAsset() const
        {
            if (!mpAttributeData)
                return 0;
            u64 luAsset = 0;
            const u8* lpData = static_cast<const u8*>(mpAttributeData);
            std::memcpy(&luAsset, lpData + 0x148u, sizeof(luAsset));
            return luAsset;
        }

        f32 DecelMinRpm() const { return Float(0x1DCu); }
        f32 MinRpm() const { return Float(0x1E0u); }
        f32 EQ_Peaking_Q() const { return Float(0x1E4u); }
        f32 EQ_LowShelf_Gain() const { return Float(0x1E8u); }
        f32 EQ_LowShelf_Freq() const { return Float(0x1ECu); }
        f32 EQ_HighShelf_Gain() const { return Float(0x1F0u); }
        f32 EQ_HighShelf_Freq() const { return Float(0x1F4u); }
        f32 EQ_Peaking_Freq() const { return Float(0x188u); }
        f32 MasterGain() const { return Float(0x190u); }
        f32 MasterCarVolume() const { return Float(0x194u); }
        f32 EQ_Peaking_Gain() const { return Float(0x210u); }

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
