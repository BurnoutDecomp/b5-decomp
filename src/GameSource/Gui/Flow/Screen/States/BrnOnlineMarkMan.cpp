// ===================================================================================
// BrnGui::OnlineMarkMan  -- the online "mark man" screen state (expected-component slice)
//   class:BrnGui::OnlineMarkMan
//
//   SetExpectedComponent   @ 0x82483AC0
//   ClearExpectedComponent @ 0x82483BA8
// Reconstructed store-for-store from the X360 asm; byte-identical twin of
// BrnGui::RaceMainHudState::SetExpectedComponent.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineMarkMan.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsHash.h"    // CgsContainers::CgsHash::CalculateHash

namespace BrnGui
{
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
}
