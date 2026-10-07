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

    // ARTIST 0x82857468..0x82857740, recovered from raw image instructions.
    // Function-local initialization caches hold aspect 1.0 (8305F268), near
    // 0.1 (8305F264, source 820E07D8), far 1000 (8305F260, source 820E07DC),
    // and the LookAt vectors below. Their guard word is 8305F26C.
    CgsGraphics::Camera GetGuiCamera()
    {
        static const f32 KF_GUI_ASPECT_RATIO = 1.0f; // 82001C98 -> 8305F268
        static const f32 KF_GUI_NEAR_CLIP = 0.1f;    // 820E07D8 -> 8305F264
        static const f32 KF_GUI_FAR_CLIP = 1000.0f;  // 820E07DC -> 8305F260
        CgsGraphics::Camera lCamera;

        switch (gCurrentGuiCamera)
        {
        case E_GUICAMERA_NORMAL:
        {
            lCamera.Construct(0.9f, KF_GUI_ASPECT_RATIO, KF_GUI_NEAR_CLIP, KF_GUI_FAR_CLIP); // 820E3E70
            static const Vector3 sEye = {-0.4f, 0.0f, 2.0f, 0.0f};    // 8305F250; 82012EF8/82001D9C
            static const Vector3 sUp = {0.0f, 1.0f, 0.0f, 0.0f};     // 8305F240; vector 82181510
            static const Vector3 sTarget = {-0.08f, 0.0f, 0.0f, 0.0f}; // 8305F230; 8200D5B0
            lCamera.LookAt(sEye, sUp, sTarget);
            break;
        }
        case E_GUICAMERA_FULLSCREENMAP:
        {
            // DecFIGS names selector 0 ORTHO, but ARTIST calls the same perspective
            // constructor with the narrow 0.02-radian FOV. No orthogonal setter runs.
            lCamera.Construct(0.02f, KF_GUI_ASPECT_RATIO, KF_GUI_NEAR_CLIP, KF_GUI_FAR_CLIP); // 820E3E6C
            static const Vector3 sEye = {0.0f, 0.0f, 100.0f, 0.0f}; // 8305F220; 820049E0
            static const Vector3 sUp = {0.0f, 1.0f, 0.0f, 0.0f};   // 8305F210; vector 82181510
            static const Vector3 sTarget = {0.0f, 0.0f, 0.0f, 0.0f}; // 8305F200
            lCamera.LookAt(sEye, sUp, sTarget);
            break;
        }
        default:
            // The console asserts then returns its untouched result storage.
            // It does not choose a different selector or construct a fallback camera.
            CGS_ASSERT(false, "Bad GuiCamera type\n"); // 820E3E74, CgsGuiShared.cpp:233
            break;
        }
        return lCamera;
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
