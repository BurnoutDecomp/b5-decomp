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
#include "GameSource/AttribSys/Generated/attrib_private.h"
#include "GameSource/AttribSys/Generated/attrib_findcollection.h"
#include "BrnCommonTypes.h"

#include <cstring>

namespace Attrib
{
namespace Gen
{
    class crashbin : private Instance
    {
    public:
        static const u64 KU_CLASS_KEY = 0x3DFA53FAFE5BD9D7ull;
        static const u32 KU_LAYOUT_SIZE = 0x190u;
        static const u32 KU_OFFSET_ARRAY_SMALL_HEADER = 0x48u;
        static const u32 KU_OFFSET_ARRAY_SMALL = 0x50u;
        static const u32 KU_OFFSET_ARRAY_MEDIUM_HEADER = 0xA0u;
        static const u32 KU_OFFSET_ARRAY_MEDIUM = 0xA8u;
        static const u32 KU_OFFSET_ARRAY_LARGE_HEADER = 0xF8u;
        static const u32 KU_OFFSET_ARRAY_LARGE = 0x100u;
        static const u32 KU_OFFSET_NUM_SMALL = 0x160u;
        static const u32 KU_OFFSET_NUM_MEDIUM = 0x164u;
        static const u32 KU_OFFSET_NUM_LARGE = 0x168u;

        explicit crashbin(Collection* lpCollection = nullptr, void* lpOwner = nullptr);
        crashbin(u64 luCollectionKey, void* lpOwner)
            : Instance(FindCollectionWithDefault(KU_CLASS_KEY, luCollectionKey), lpOwner)
        {
            if (!mpAttributeData)
                mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
        }

        void ChangeWithDefault(const RefSpec& lrRefSpec)
        {
            RefSpec& lrMutable = const_cast<RefSpec&>(lrRefSpec);
            Change(const_cast<Collection*>(lrMutable.GetCollectionWithDefault()));
        }

        const s32& mCollisionsSmall(u32 luIndex) const
        {
            return ArrayElementAt(KU_OFFSET_ARRAY_SMALL_HEADER,
                                  KU_OFFSET_ARRAY_SMALL, luIndex);
        }

        const s32& mCollisionsMedium(u32 luIndex) const
        {
            return ArrayElementAt(KU_OFFSET_ARRAY_MEDIUM_HEADER,
                                  KU_OFFSET_ARRAY_MEDIUM, luIndex);
        }

        const s32& mCollisionsLarge(u32 luIndex) const
        {
            return ArrayElementAt(KU_OFFSET_ARRAY_LARGE_HEADER,
                                  KU_OFFSET_ARRAY_LARGE, luIndex);
        }

        const s32& mNumCollisionsSmall() const { return ScalarAt(KU_OFFSET_NUM_SMALL); }

        const s32& mNumCollisionsMedium() const
        {
            return ScalarAt(KU_OFFSET_NUM_MEDIUM);
        }
        const s32& mNumCollisionsLarge() const { return ScalarAt(KU_OFFSET_NUM_LARGE); }

        const Vector3& Volumes() const { return VectorAt(0x000u); }
        const Vector3& Pitch() const { return VectorAt(0x010u); }
        const Vector3& IntensityThreshold() const { return VectorAt(0x020u); }
        const char* mSpliceBankAsset() const { return TextAt(0x030u); }
        const u64& mMaterialB() const { return QwordAt(0x038u); }
        const u64& mMaterialA() const { return QwordAt(0x040u); }
        f32 Priority() const { return FloatAt(0x150u); }
        f32 PhysicsImpulseNormalization_MIN() const { return FloatAt(0x154u); }
        f32 PhysicsImpulseNormalization_MAX() const { return FloatAt(0x158u); }
        u32 mOrientation() const { return DwordAt(0x15Cu); }
        s32 MixerSlider() const { return static_cast<s32>(DwordAt(0x16Cu)); }
        u32 mImpactTime() const { return DwordAt(0x170u); }
        u32 mGameModes() const { return DwordAt(0x174u); }
        u32 mFatalityFlag() const { return DwordAt(0x178u); }
        u32 mCameras() const { return DwordAt(0x17Cu); }
        u32 mAction() const { return DwordAt(0x180u); }
        f32 DistanceFactor_Min() const { return FloatAt(0x184u); }
        f32 DistanceFactor_Max() const { return FloatAt(0x188u); }
        using Instance::IsValid;

    private:
        const u8* LayoutBytes() const
        {
            return static_cast<const u8*>(GetLayoutPointer());
        }
        const s32& ScalarAt(u32 luOffset) const
        {
            return *reinterpret_cast<const s32*>(LayoutBytes() + luOffset);
        }
        u32 DwordAt(u32 luOffset) const
        {
            return *reinterpret_cast<const u32*>(LayoutBytes() + luOffset);
        }
        f32 FloatAt(u32 luOffset) const
        {
            return *reinterpret_cast<const f32*>(LayoutBytes() + luOffset);
        }
        const u64& QwordAt(u32 luOffset) const
        {
            return *reinterpret_cast<const u64*>(LayoutBytes() + luOffset);
        }
        const Vector3& VectorAt(u32 luOffset) const
        {
            return *reinterpret_cast<const Vector3*>(LayoutBytes() + luOffset);
        }
        const char* TextAt(u32 luOffset) const
        {
            u32 luAddress = 0;
            std::memcpy(&luAddress, LayoutBytes() + luOffset, sizeof(luAddress));
            return reinterpret_cast<const char*>(static_cast<uintptr_t>(luAddress));
        }
        const s32& ArrayElementAt(u32 luHeaderOffset, u32 luElementsOffset,
                                  u32 luIndex) const
        {
            const Private* lpHeader = reinterpret_cast<const Private*>(
                LayoutBytes() + luHeaderOffset);
            if (luIndex >= lpHeader->GetLength())
                return *static_cast<const s32*>(DefaultDataArea(sizeof(s32)));
            return reinterpret_cast<const s32*>(
                LayoutBytes() + luElementsOffset)[luIndex];
        }
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
            mpAttributeData = DefaultDataArea(KU_LAYOUT_SIZE);
    }
}
}
