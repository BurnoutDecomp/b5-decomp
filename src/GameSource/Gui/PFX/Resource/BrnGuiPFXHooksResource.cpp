#include "GameSource/Gui/PFX/Resource/BrnGuiPFXHooksResource.h"
#include "rw/rwcore_structs.h"   // rw::Resource complete for the bodies
#include <cstring>
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT (ValidateBundleOffsets)

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

#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"   // THE BrnGui::PFXHook / PFXHookBundle -- this TU
                                                //   carried a private copy of both until 2026-09-17
                                                //   (an ODR fork; see the note in BrnGuiPFXHooks.cpp)

namespace BrnGui
{
    template <typename T>
    static T* PointerFromU32(u32 luAddress)
    {
        return reinterpret_cast<T*>(static_cast<uintptr_t>(luAddress));
    }
}

namespace CgsResource
{
    using BrnGui::PFXHook;
    using BrnGui::PFXHookBundle;
    using BrnGui::PointerFromU32;

    static const uint32_t KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID = 49;

    // ---- the console relocation, kept for the record --------------------------------------
    //   FixUpBundle   @0x8250B038: hooks/groups tables += base; each hook slot += base, then
    //                 PFXHook::FixUp; each group slot += base.
    //   FixDownBundle @0x8250B128: the exact inverse, in the inverse order.
    // [FLAG PC bring-up] NEITHER RUNS ON THE x64 HOST. The slots are 32-bit and the GUI
    // resource bank sits above 4 GB (measured 2026-09-17: base 0x1668CD23AE0; the truncated
    // rebase wrote through 0x00000000423AB2D0 and the first registered boot crashed here),
    // so the blob keeps the file-relative offsets the converter wrote and PFXHookBundle's
    // accessors add the bundle's own address at read time -- see the banner in
    // GameSource/Gui/PFX/BrnGuiPFXHooks.h. What is left of FixUp on PC is the console's
    // implicit contract, checked: the tables and the recorded size lie inside the blob.
    static void ValidateBundleOffsets(const PFXHookBundle* lpBundle)
    {
        CGS_ASSERT(lpBundle->mpaHooks + 4u * static_cast<u32>(lpBundle->miHookCount) <= lpBundle->mSizeOfBundle,
                   "PFX hook table lies inside the bundle");
        CGS_ASSERT(lpBundle->mpaGroups + 4u * static_cast<u32>(lpBundle->miGroupCount) <= lpBundle->mSizeOfBundle,
                   "PFX group table lies inside the bundle");
    }

    uint32_t PFXHookBundleResourceType::GetTypeID() const
    {
        return KU_PFX_HOOK_BUNDLE_RESOURCE_TYPE_ID;
    }

    void PFXHookBundleResourceType::FixUp(void* lpResource, const rw::Resource& /*lrResource*/) const
    {
        ValidateBundleOffsets(static_cast<const PFXHookBundle*>(lpResource));   // offsets stay (banner above)
    }

    void PFXHookBundleResourceType::FixDown(void* /*lpResource*/, const rw::Resource& /*lrResource*/) const
    {
        // nothing to un-relocate: the blob never leaves its file-relative form on PC
    }

    void* PFXHookBundleResourceType::Serialise(const void* lpResource, const rw::Resource& lrDest) const
    {
        // @0x82512478 fixes the source DOWN, copies, and fixes both back UP; with the blob
        // held file-relative on PC the copy IS the serialisation.
        const PFXHookBundle* lpBundle = static_cast<const PFXHookBundle*>(lpResource);
        void*                lpDest   = lrDest.m_baseResources[0];
        std::memcpy(lpDest, lpBundle, lpBundle->mSizeOfBundle);
        return lpDest;
    }
}
