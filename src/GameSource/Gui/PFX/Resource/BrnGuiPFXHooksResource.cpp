#include "types.hpp"

#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PFXHookBundleResourceType::Serialise @ 0x82512478
//   CgsResource::PFXHookBundleResourceType::FixUp     @ 0x8250B038
//   CgsResource::PFXHookBundleResourceType::FixDown   @ 0x8250B128
//   CgsResource::PFXHookBundleResourceType::GetTypeID @ 0x824F5B90

namespace BrnGui
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    struct PFXGroup
    {
    };

    struct PFXHookNode
    {
        f32 mfStartTime;
        u32 mpGroup;
    };

    struct PFXHook
    {
        void FixUp(u32 luBaseValue);
        void FixDown(u32 luBaseValue);

        char macName[32];
        u32 muId;
        s32 miPriority;
        u32 meTransitionMode;
        f32 mfTransitionTime;
        u8 mbIsMenu;
        u8 maPad53[3];
        u32 mpaNodes;
        s32 miNodeCount;
    };

    struct PFXHookBundle
    {
        s32 miHookCount;
        s32 miGroupCount;
        u32 mpaHooks;
        u32 mpaGroups;
        u32 mSizeOfBundle;
    };

    void PFXHook::FixUp(u32 luBaseValue)
    {
        mpaNodes += luBaseValue;
        u32* lpaNodeAddresses = PointerFromU32<u32>(mpaNodes);

        for (s32 liIndex = 0; liIndex < miNodeCount; ++liIndex)
        {
            lpaNodeAddresses[liIndex] += luBaseValue;
            PFXHookNode* lpNode = PointerFromU32<PFXHookNode>(lpaNodeAddresses[liIndex]);
            lpNode->mpGroup += luBaseValue;
        }
    }
}

namespace CgsResource
{
    static const u32 KI_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID = 49;

    using BrnGui::PFXGroup;
    using BrnGui::PFXHook;
    using BrnGui::PFXHookBundle;

    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

    static u32 AddressFromPointer(const void* lpPointer)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer));
    }

    class PFXHookBundleResourceType
    {
    public:
        void* Serialise(const PFXHookBundle* lpBundle, void** lppDestination) const;
        u32 GetTypeID() const;
        void FixUp(PFXHookBundle* lpBundle, u32 luBaseValue) const;
        void FixDown(PFXHookBundle* lpBundle, u32 luBaseValue) const;
    };

    void* PFXHookBundleResourceType::Serialise(
        const PFXHookBundle* lpBundle,
        void** lppDestination) const
    {
        void* lpDestination = *lppDestination;
        PFXHookBundle* lpMutableBundle = const_cast<PFXHookBundle*>(lpBundle);

        FixDown(lpMutableBundle, AddressFromPointer(lpMutableBundle));
        std::memcpy(lpDestination, lpBundle, lpBundle->mSizeOfBundle);

        FixUp(static_cast<PFXHookBundle*>(lpDestination), AddressFromPointer(lpDestination));
        FixUp(lpMutableBundle, AddressFromPointer(lpMutableBundle));

        return lpDestination;
    }

    u32 PFXHookBundleResourceType::GetTypeID() const
    {
        return KI_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID;
    }

    void PFXHookBundleResourceType::FixUp(PFXHookBundle* lpBundle, u32 luBaseValue) const
    {
        lpBundle->mpaHooks += luBaseValue;
        lpBundle->mpaGroups += luBaseValue;

        u32* lpaHookAddresses = PointerFromU32<u32>(lpBundle->mpaHooks);
        for (s32 liIndex = 0; liIndex < lpBundle->miHookCount; ++liIndex)
        {
            lpaHookAddresses[liIndex] += luBaseValue;
            PointerFromU32<PFXHook>(lpaHookAddresses[liIndex])->FixUp(luBaseValue);
        }

        u32* lpaGroupAddresses = PointerFromU32<u32>(lpBundle->mpaGroups);
        for (s32 liIndex = 0; liIndex < lpBundle->miGroupCount; ++liIndex)
        {
            lpaGroupAddresses[liIndex] += luBaseValue;
        }
    }

    void PFXHookBundleResourceType::FixDown(PFXHookBundle* lpBundle, u32 luBaseValue) const
    {
        u32* lpaHookAddresses = PointerFromU32<u32>(lpBundle->mpaHooks);
        for (s32 liIndex = 0; liIndex < lpBundle->miHookCount; ++liIndex)
        {
            PFXHook* lpHook = PointerFromU32<PFXHook>(lpaHookAddresses[liIndex]);
            lpHook->FixDown(luBaseValue);
            lpaHookAddresses[liIndex] -= luBaseValue;
        }

        u32* lpaGroupAddresses = PointerFromU32<u32>(lpBundle->mpaGroups);
        for (s32 liIndex = 0; liIndex < lpBundle->miGroupCount; ++liIndex)
        {
            lpaGroupAddresses[liIndex] -= luBaseValue;
        }

        lpBundle->mpaHooks -= luBaseValue;
        lpBundle->mpaGroups -= luBaseValue;
    }
}
