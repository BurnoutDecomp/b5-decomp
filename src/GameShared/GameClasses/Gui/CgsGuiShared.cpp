#include "GameShared/GameClasses/Gui/CgsGuiShared.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8240E388.

namespace CgsGui
{
    // @ 0x8240E388 - assert the gui-cache pointer has been wired up (read at +0x10)
    // and return it. Path/line from the baked assert string
    // (GameShared/GameClasses/Gui/CgsGuiShared.h:201).
    BrnGui::GuiCache* GuiAccessPointers::GetGuiCache()
    {
        CGS_ASSERT(mpGuiCache != nullptr, "mpGuiCache");
        return mpGuiCache;
    }

    // GetFlaptManager -- DWARF home CgsGuiShared.h:194 (X360 header-inline; no
    // standalone body was emitted -- the assert+load pair is carried inlined at
    // e.g. BrnGui::InvisibleOverlayState::OnEnter @0x824B1568 and
    // BrnGui::BaseOverlayState::Prepare @0x824B1F80). Same shape as GetGuiCache
    // above: assert the pointer has been wired up, then return it.
    BrnFlapt::FlaptManager* GuiAccessPointers::GetFlaptManager()
    {
        CGS_ASSERT(mpFlaptManager != nullptr, "NULL != mpFlaptManager");
        return mpFlaptManager;
    }

    // Header-inline access-pointer setters in the original build.  GuiModule::Construct
    // calls both before either the HUD or overlay flow can enter its first state.
    void GuiAccessPointers::SetFlaptManager(BrnFlapt::FlaptManager* lpFlaptManager)
    {
        mpFlaptManager = lpFlaptManager;
    }

    void GuiAccessPointers::SetGuiCache(BrnGui::GuiCache* lpGuiCache)
    {
        mpGuiCache = lpGuiCache;
    }

    // ---- the GUI camera selector -------------------------------------------------------
    // X360 dword_8305A6C4. Zero in the shipped image (E_GUICAMERA_FULLSCREENMAP), which is
    // also the C++ zero-init value, so no explicit seeding is needed.
    EGuiCameraType gCurrentGuiCamera = E_GUICAMERA_FULLSCREENMAP;

    // @0x82847658 (recovered from the image -- the export set has no entry for it; see the
    // instruction listing in CgsGuiShared.h). Assert the type is in range, then store it.
    // ⚠️ The store is UNCONDITIONAL on the console: the `blt` only skips the assert block,
    // it does not skip the write, so an out-of-range value is asserted AND stored. Kept.
    int SetGuiCamera(s32 liCameraType)
    {
        CGS_ASSERT(liCameraType < E_GUICAMERA_COUNT, "lCameraType<E_GUICAMERA_COUNT");
        gCurrentGuiCamera = static_cast<EGuiCameraType>(liCameraType);
        return liCameraType;
    }

    // Null every shared-resource pointer; the owners install each one as its
    // subsystem comes up (mpAptAux from the Apt bring-up, the flapt/cache/queue
    // pointers from their modules).
    void GuiAccessPointers::Construct()
    {
        mpAptAux           = nullptr;
        mpLanguageManager  = nullptr;
        mpFlaptFile        = nullptr;
        mpFlaptManager     = nullptr;
        mpGuiCache         = nullptr;
        mpGDMInput         = nullptr;
        mpGDMReceiverQueue = nullptr;
    }
}
