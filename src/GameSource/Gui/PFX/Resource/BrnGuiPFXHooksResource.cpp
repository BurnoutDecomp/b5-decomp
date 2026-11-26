#include "GameSource/Gui/PFX/Resource/BrnGuiPFXHooksResource.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::PFXHookBundleResourceType::Serialise @ 0x82512478
//   CgsResource::PFXHookBundleResourceType::FixUp     @ 0x8250B038 (the private (void*, uint32_t) one)
//   CgsResource::PFXHookBundleResourceType::FixDown   @ 0x8250B128 (the private (void*, uint32_t) one)
//   CgsResource::PFXHookBundleResourceType::GetTypeID @ 0x824F5B90
//
// FixUp/FixDown (un)relocate the hook/group pointer tables and each hook's nodes by a
// base value. The virtuals take the base from the rw::Resource (0x825124E8 / 0x825124F0:
// `lwz r5, 0(r5)` and a tail branch); Serialise un-relocates the source against its own
// address, copies the bundle into the destination, relocates the copy through the
// virtual FixUp and the source back (DWARF BrnGuiPFXHooksResource.h:43, cpp:43..190).

#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"

namespace CgsResource
{
    using BrnGui::PFXHook;
    using BrnGui::PFXGroup;
    using BrnGui::PFXHookBundle;

    static const uint32_t KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID = 49;

    // cpp:43
    void* PFXHookBundleResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        PFXHookBundle* lpBundle = static_cast<PFXHookBundle*>(const_cast<void*>(lpResource));
        const size_t luSize = lpBundle->mSizeOfBundle;
        void* lpDest = lrDest.m_baseResources[0];

        FixDown(lpBundle, reinterpret_cast<uint32_t>(lpBundle));
        std::memcpy(lpDest, lpBundle, luSize);
        FixUp(lpDest, lrDest);
        FixUp(lpBundle, reinterpret_cast<uint32_t>(lpBundle));

        return lrDest.m_baseResources[0];
    }

    // cpp:92
    uint32_t PFXHookBundleResourceType::GetTypeID() const
    {
        return KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID;
    }

    // cpp:108
    void PFXHookBundleResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        FixUp(lpResource, reinterpret_cast<uint32_t>(lrResource.m_baseResources[0]));
    }

    // cpp:118
    void PFXHookBundleResourceType::FixUp(void* lpResource, uint32_t luBaseValue) const
    {
        PFXHookBundle* lpBundle = static_cast<PFXHookBundle*>(lpResource);

        lpBundle->mpaHooks  = reinterpret_cast<PFXHook**>(reinterpret_cast<uint32_t>(lpBundle->mpaHooks) + luBaseValue);
        lpBundle->mpaGroups = reinterpret_cast<PFXGroup**>(reinterpret_cast<uint32_t>(lpBundle->mpaGroups) + luBaseValue);

        for (int32_t liIndex = 0; liIndex < lpBundle->miHookCount; ++liIndex)
        {
            lpBundle->mpaHooks[liIndex] =
                reinterpret_cast<PFXHook*>(reinterpret_cast<uint32_t>(lpBundle->mpaHooks[liIndex]) + luBaseValue);
            lpBundle->mpaHooks[liIndex]->FixUp(luBaseValue);
        }

        for (int32_t liIndex = 0; liIndex < lpBundle->miGroupCount; ++liIndex)
        {
            lpBundle->mpaGroups[liIndex] =
                reinterpret_cast<PFXGroup*>(reinterpret_cast<uint32_t>(lpBundle->mpaGroups[liIndex]) + luBaseValue);
        }
    }

    // cpp:180
    void PFXHookBundleResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        FixDown(lpResource, reinterpret_cast<uint32_t>(lrResource.m_baseResources[0]));
    }

    // cpp:190
    void PFXHookBundleResourceType::FixDown(void* lpResource, uint32_t luBaseValue) const
    {
        PFXHookBundle* lpBundle = static_cast<PFXHookBundle*>(lpResource);

        for (int32_t liIndex = 0; liIndex < lpBundle->miHookCount; ++liIndex)
        {
            lpBundle->mpaHooks[liIndex]->FixDown(luBaseValue);
            lpBundle->mpaHooks[liIndex] =
                reinterpret_cast<PFXHook*>(reinterpret_cast<uint32_t>(lpBundle->mpaHooks[liIndex]) - luBaseValue);
        }

        for (int32_t liIndex = 0; liIndex < lpBundle->miGroupCount; ++liIndex)
        {
            lpBundle->mpaGroups[liIndex] =
                reinterpret_cast<PFXGroup*>(reinterpret_cast<uint32_t>(lpBundle->mpaGroups[liIndex]) - luBaseValue);
        }

        lpBundle->mpaHooks  = reinterpret_cast<PFXHook**>(reinterpret_cast<uint32_t>(lpBundle->mpaHooks) - luBaseValue);
        lpBundle->mpaGroups = reinterpret_cast<PFXGroup**>(reinterpret_cast<uint32_t>(lpBundle->mpaGroups) - luBaseValue);
    }
}
