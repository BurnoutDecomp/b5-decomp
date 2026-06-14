#include "GameSource/Gui/PFX/Resource/BrnGuiPFXHooksResource.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PFXHookBundleResourceType::Serialise @ 0x82512478
//   CgsResource::PFXHookBundleResourceType::FixUp     @ 0x8250B038
//   CgsResource::PFXHookBundleResourceType::FixDown   @ 0x8250B128
//   CgsResource::PFXHookBundleResourceType::GetTypeID @ 0x824F5B90
//
// FixUp/FixDown (un)relocate the hook/group pointer tables and each hook's nodes by a
// base value (the rw::Resource's load base). Serialise un-relocates the source to
// file-relative pointers, copies the bundle to the destination resource's buffer,
// then re-relocates both. The worker logic is shared between the virtuals (base from
// the rw::Resource) and Serialise (base = the buffer's own address).

namespace BrnGui
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }

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
        u32  muId;
        s32  miPriority;
        u32  meTransitionMode;
        f32  mfTransitionTime;
        u8   mbIsMenu;
        u8   maPad53[3];
        u32  mpaNodes;
        s32  miNodeCount;
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
            PointerFromU32<PFXHookNode>(lpaNodeAddresses[liIndex])->mpGroup += luBaseValue;
        }
    }

    void PFXHook::FixDown(u32 luBaseValue)
    {
        u32* lpaNodeAddresses = PointerFromU32<u32>(mpaNodes);
        for (s32 liIndex = 0; liIndex < miNodeCount; ++liIndex)
        {
            PointerFromU32<PFXHookNode>(lpaNodeAddresses[liIndex])->mpGroup -= luBaseValue;
            lpaNodeAddresses[liIndex] -= luBaseValue;
        }
        mpaNodes -= luBaseValue;
    }
}

namespace CgsResource
{
    using BrnGui::PFXHook;
    using BrnGui::PFXHookBundle;
    using BrnGui::PointerFromU32;

    static const uint32_t KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID = 49;

    static u32 AddressFromPointer(const void* lpPointer)
    {
        return static_cast<u32>(reinterpret_cast<uintptr_t>(lpPointer));
    }

    static void FixUpBundle(PFXHookBundle* lpBundle, u32 luBaseValue)
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
            lpaGroupAddresses[liIndex] += luBaseValue;
    }

    static void FixDownBundle(PFXHookBundle* lpBundle, u32 luBaseValue)
    {
        u32* lpaHookAddresses = PointerFromU32<u32>(lpBundle->mpaHooks);
        for (s32 liIndex = 0; liIndex < lpBundle->miHookCount; ++liIndex)
        {
            PointerFromU32<PFXHook>(lpaHookAddresses[liIndex])->FixDown(luBaseValue);
            lpaHookAddresses[liIndex] -= luBaseValue;
        }

        u32* lpaGroupAddresses = PointerFromU32<u32>(lpBundle->mpaGroups);
        for (s32 liIndex = 0; liIndex < lpBundle->miGroupCount; ++liIndex)
            lpaGroupAddresses[liIndex] -= luBaseValue;

        lpBundle->mpaHooks -= luBaseValue;
        lpBundle->mpaGroups -= luBaseValue;
    }

    uint32_t PFXHookBundleResourceType::GetTypeID() const
    {
        return KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID;
    }

    void PFXHookBundleResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        FixUpBundle(static_cast<PFXHookBundle*>(lpResource), CgsResource::GetLoadBase(lrResource));
    }

    void PFXHookBundleResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        FixDownBundle(static_cast<PFXHookBundle*>(lpResource), CgsResource::GetLoadBase(lrResource));
    }

    void* PFXHookBundleResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        PFXHookBundle* lpBundle = const_cast<PFXHookBundle*>(static_cast<const PFXHookBundle*>(lpResource));
        void*          lpDest   = lrDest.m_baseResources[0];

        FixDownBundle(lpBundle, AddressFromPointer(lpBundle));
        std::memcpy(lpDest, lpBundle, lpBundle->mSizeOfBundle);
        FixUpBundle(static_cast<PFXHookBundle*>(lpDest), AddressFromPointer(lpDest));
        FixUpBundle(lpBundle, AddressFromPointer(lpBundle));

        return lpDest;
    }
}
