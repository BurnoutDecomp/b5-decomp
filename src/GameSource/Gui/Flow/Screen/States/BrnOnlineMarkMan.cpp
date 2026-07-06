// ===================================================================================
// BrnGui::OnlineMarkMan  -- the online "mark man" screen state (expected-component slice)
//   class:BrnGui::OnlineMarkMan
//
//   SetExpectedComponent        @ 0x82483AC0
//   ClearExpectedComponent      @ 0x82483BA8
//   SetExpectedAptComponentList @ 0x82487260
//   UpdateWFInit                @ 0x82487190
//   (GetResourcesToLoad @0x825006B8 is header-inline)
// Reconstructed store-for-store from the X360 asm; the expected-component / apt-list /
// init-poll surface is the byte-identical twin of BrnGui::ImageGalleryState (and
// RaceMainHudState::SetExpectedComponent for the hash helper).
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash

namespace BrnGui
{
    // The state's one-APT resource list (XEX .rodata @0x8205F8FC; the paired count word
    // @0x8205F904 == 1). FLAG: the tuple's id + type bytes are not attested in scope --
    // the DWARF only fixes maResourcesToLoad[1] / muNumResourcesToLoad == 1. Every sibling
    // state resource list is a single {apt-id, E_GUI_RESOURCETYPE_APT} tuple, so that shape
    // is reproduced here; the concrete id is a placeholder pending the .rodata read.
    const CgsGui::sResourceTuple OnlineMarkMan::KA_RESOURCES_TO_LOAD[1] =
    {
        { 0u, CgsGui::E_GUI_RESOURCETYPE_APT },   // FLAG: id + type unattested in scope
    };

    // FLAG boundary no-op: the apt-component watcher half of the cache the X360 reaches is
    // not on the committed GuiCache public API. Faithful default is a no-op; mirrors the
    // committed BrnGui::BootLegalCacheBoundary::ClearExpectedAptComponentList body. GROW when
    // the GuiCache apt-component watcher lands.
    namespace OnlineMarkManCacheBoundary
    {
        void ClearExpectedAptComponentList(GuiCache* /*lpCache*/, s32 /*liFlow*/)
        {
        }
    }

    // ---- SetExpectedComponent @ 0x82483AC0 -------------------------------------
    // Append the hash of an expected APT-component name to the table. Asserts there
    // is room (count < 9), inline-strlens the name (length EXCLUDES the NUL terminator,
    // the byte count CgsHash::CalculateHash expects), stores the hash at
    // mauExpectedComponentIds[count] (X360 +0x40 + count*4), and increments the count.
    // The X360 returns the computed hash in r3 (twin RaceMainHudState::SetExpectedComponent
    // is homed u32; DWARF declares void, the sole caller ignores the return).
    u32 OnlineMarkMan::SetExpectedComponent(const char* lpcName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        const char* lpc = lpcName;
        while (*lpc)
        {
            ++lpc;
        }
        u32 luHash = CgsContainers::CgsHash::CalculateHash(
            const_cast<char*>(lpcName), static_cast<int>(lpc - lpcName));   // length excludes the NUL

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
        return luHash;
    }

    // ---- ClearExpectedComponent @ 0x82483BA8 -----------------------------------
    // Reset the expected-APT-component table: zero all nine hash slots
    // (mauExpectedComponentIds, X360 +0x40) and the live count
    // (muNumExpectedComponents, +0x64), then hand the (now empty) list to the cache's
    // apt-component watcher (flow 0, E_GUIFLOW_SCREEN). The X360 clears the array +
    // count BEFORE asserting the cache handle, so that order is preserved.
    void OnlineMarkMan::ClearExpectedComponent()
    {
        for (u32 luSlot = 0; luSlot < KU_MAX_INIT_COMPONENTS_NUM; ++luSlot)
        {
            mauExpectedComponentIds[luSlot] = 0;
        }
        muNumExpectedComponents = 0;

        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        OnlineMarkManCacheBoundary::ClearExpectedAptComponentList(mpGuiCache, 0);
    }

    // ---- SetExpectedAptComponentList @ 0x82487260 ------------------------------
    // Rebuild the expected-APT-component list and hand it to the cache (flow layer 0).
    // Clears the current list, registers the single time-field APT component by name
    // (the name buffer at X360 +0x194), asserts the count fits the bound (non-gating),
    // then pushes the {hash array, count} to the cache. Twin of
    // ImageGalleryState::SetExpectedAptComponentList, minus the tab loop.
    void OnlineMarkMan::SetExpectedAptComponentList()
    {
        ClearExpectedComponent();
        SetExpectedComponent(macTimeComponentName);
        CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                   "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");   // cpp:503 (non-gating)
        mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_SCREEN, mauExpectedComponentIds,
                                                muNumExpectedComponents);
    }

    // ---- UpdateWFInit @ 0x82487190 ---------------------------------------------
    // The per-frame init poll: once the cache reports every expected component
    // initialised (flow layer 0), clear the list and report done; otherwise wait.
    // Twin of ImageGalleryState::UpdateWFInit.
    bool OnlineMarkMan::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }
        ClearExpectedComponent();
        return true;
    }
}
