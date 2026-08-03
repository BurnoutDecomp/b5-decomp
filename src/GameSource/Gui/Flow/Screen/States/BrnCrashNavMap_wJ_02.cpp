// ===================================================================================
// BrnGui::CrashNavMap -- wave-J partfile 02: the apt-component registration pair and the
// icon-manager (re)acquisition.
//   AppendExpectedAptComponents @0x824B67E0  (cpp:537,  virtual, vtable slot +44)
//   SetupComponents             @0x824D8C40  (cpp:556,  virtual, vtable slot +48)
//   ResetIconManager            @0x824B6BE0  (cpp:1408, Update's id-64 arm)
//
// All three bodies are landed, read store-for-store off the raw X360 assembly with
// Hex-Rays arbitrated against it. The shared-header declarations this group filed requests
// for -- GuiCache::AppendExpectedAptComponentList / GetMapIconManager, MapIconManager::
// AppendExpectedAptComponents / SetupComponent / SetOwnerParameters and its
// meIconFilterMode / mbRotateSatNav / meIconSizeMode members (reached through
// `friend struct CrashNavMap`), CrashNavPanel::AppendExpectedAptComponents / SetupComponent
// and GuiEventDrawEventIcons::EIconDisplayType -- have all since been applied, so the
// bodies below spell them as the committed headers do.
//
// The nine-parameter SetOwnerParameters signature is verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/SatNav/BrnMapIconManager.h:191) and matches
// the X360 call site argument for argument, including the two stack parameters Hex-Rays
// dropped. GameSource/Gui/Flow/PreEvent/States/BrnPreRaceFlyBy_wJ_02.cpp is the other
// consumer of GetMapIconManager / SetOwnerParameters / E_ICON_DISPLAY_TYPE_COUNT; it is
// landed and compiles (selfcheck STATUS=pass, 2026-08-03). An earlier revision of this
// banner claimed that file "does not compile today either" -- that was wrong when written
// (the file was then a comment-only stub) and is wrong now; the claim is retracted, along
// with the cross-file ledger warning it was attached to.
//
// Everything else in the reconstruction -- the enum spellings, the GameStateModuleIO
// game-mode compares, the CgsDev::StrStream assert chain, the gpDebugPrint stream chain,
// the id-64 payload view and the GuiComponent::GetName() calls -- checks out clean under
// the compile gate.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"       // GuiComponent::GetName
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiFlow
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    namespace
    {
        // ---- shared group-2 file statics (include ONCE when consolidating) ------------

        // The log-category bit every debug print in this TU is gated on
        // (`ld` + `clrldi r11,r11,63`).
        const u64 KX_MESSAGE_FILTER_BIT = 1;

        // The crash-nav map's per-icon apt component budget (X360 `li r6,0x32` at both
        // 0x824B67F8 and 0x824B6D60).
        const s32 KI_CRASHNAVMAP_NUMICONS = 50;

    }

    // X360 .bss @0x82FB4890 (DWARF BrnCrashNavMap.cpp:45, uint32_t[50]). Construct fills it
    // with CalculateHash(SPrintf("%s%d", macSatNavIconBaseName, i)) for i in [0, 50).
    //
    // DECLARED, NOT DEFINED, HERE. The single object lives in the partfile that owns
    // Construct (BrnCrashNavMap_wJ_04.cpp); wave J lands the partfiles as separate
    // translation units, so a second (anonymous-namespace) definition here would be a
    // private, never-written copy and this function would register 50 zeros.
    extern u32 mauComponentHashIds[];

    // ================================================================================
    //  AppendExpectedAptComponents  @ 0x824B67E0  (cpp:537, vtable slot +44)
    //
    //  Tell the cache every apt component this screen is waiting on before it will call
    //  itself loaded: the 50 sat-nav icon clips (as one pre-hashed list), whatever the
    //  shared icon manager and the crash-nav panel need, and the screen's own two
    //  animation clips.
    // ================================================================================
    void CrashNavMap::AppendExpectedAptComponents()
    {
        // The 50 hashes Construct precomputed ("SatNavIcon0".."SatNavIcon49"). This runs
        // BEFORE the mpIconManager assert on the console; the order is kept.
        mpGuiCache->AppendExpectedAptComponentList(E_GUIFLOW_SCREEN, mauComponentHashIds,
                                                   KI_CRASHNAVMAP_NUMICONS);

        // cpp:537 -- non-fatal on the X360; the manager is dereferenced either way,
        // exactly as the console does.
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");

        if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "MAPICONMANAGER: CrashNavMap is calling AppendExpectedAptComponents.\n";
        }

        mpIconManager->AppendExpectedAptComponents();
        mCrashNavPanel.AppendExpectedAptComponents(E_GUIFLOW_SCREEN, mpGuiCache);

        // X360 `addi r5, r31, 0x5F58` / `0x5FE4` == the two AnimationComponents' macName
        // buffers (component +4), i.e. the components' own names.
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mTitleButtonsAnimation.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                               mButtonPromptsAnimation.GetName());
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT / Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStream (the streamed assert)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::Event
#include "GameSource/GameState/BrnGameStateSharedIO.h"                    // GSM::EGameModeType
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventDrawEventIcons
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    namespace GSM = BrnGameState::GameStateModuleIO;

    namespace
    {
        // ---- shared group-2 file statics (keep ONE copy when consolidating) ------------

        // TU static, rodata (DWARF macSatNavIconBaseName): the stem the 50 per-icon apt
        // component names are built from, and the base name the manager is handed.
        const char macSatNavIconBaseName[] = "SatNavIcon";

        // The X360 assert-site file string, verbatim (aGamesourceGuiF_63).
        const char KAC_ASSERT_FILE[] =
            "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnCrashNavMap.cpp";
    }

    // ================================================================================
    //  ResetIconManager  @ 0x824B6BE0  (cpp:1408)
    //
    //  Update's id-64 (GuiCache arrived) arm: latch the shared MapIconManager out of the
    //  cache that just turned up and (re)claim it for the crash-nav map, handing it this
    //  screen's icon-clip base name, capacity, selection policy and -- only while no real
    //  game mode is running -- its event-icon display set. When the manager actually
    //  changes hands, reset the three display knobs the previous owner may have moved.
    // ================================================================================
    void CrashNavMap::ResetIconManager(const CgsModule::Event* lpEvent)
    {
        // The in-queue hands the state the HEADER-STRIPPED payload; id 64's payload is a
        // bare GuiCache pointer (X360 `lwz r11,0(r26)`).
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpGuiCache;   // +0x00
        };
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        if (lpPayload->mpGuiCache == 0)
        {
            // Streamed diagnostic: the X360 composes it into CgsDev::Assert::
            // gpcMessageBuffer through a StrStream, then fires it (Begin first, exactly as
            // ordered here). Non-fatal -- the payload cache is dereferenced below anyway.
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "Invalid cache in GenericHudState::Update";
            CgsDev::Assert::FireAssert(lacMessage, KAC_ASSERT_FILE, 1408);
            CgsDev::Assert::EndAssert();
        }

        // The X360 re-reads the PAYLOAD's cache here, and stores the manager before it
        // tests it.
        mpIconManager = lpPayload->mpGuiCache->GetMapIconManager();
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");   // cpp:1411 (non-fatal)

        // ...whereas the display-set gate reads the STATE's own cache (`lwz r11,0x48`),
        // which Update latched from this same event just before calling in.
        GuiEventDrawEventIcons::EIconDisplayType leIconDisplayType =
            static_cast<GuiEventDrawEventIcons::EIconDisplayType>(meEventIconDisplayType);

        const s32 liGameMode = mpGuiCache->GetGameMode();
        if (liGameMode != GSM::E_MODE_NONE &&
            liGameMode != GSM::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            // Any actually-running mode gets the one-past-the-end sentinel, i.e. no
            // event-icon display set at all.
            leIconDisplayType = GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT;
        }

        const u32 luPreviousOwnerId = mIconManagerOwnerId;

        if ((CgsDev::Message::gxMessageFilterFlags & KX_MESSAGE_FILTER_BIT) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "MAPICONMANAGER: CrashNavMap is calling SetOwnerParameters with OwnerID "
                << static_cast<s32>(MapIconManager::E_CRASHNAV_MAP)
                << "(E_CRASHNAV_MAP).\n";
        }

        // The manager hands back the owner id it actually granted, which is what the
        // release path later quotes -- hence the assignment through the call.
        mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
            mpStateInterface,
            macSatNavIconBaseName,              // X360 "SatNavIcon"
            KI_CRASHNAVMAP_NUMICONS,            // X360 `li r6,0x32` == 50
            MapIconManager::E_CRASHNAV_MAP,     // X360 `li r7,2`
            mbUseRoadSigns,                     // X360 lbz +0x6081
            mbDrawDriveThrus,                   // X360 lbz +0x6082
            mbSelectDriveThrus,                 // X360 lbz +0x6083
            leIconDisplayType,                  // X360 stack parameter 8 (sp+0x54)
            0);                                 // X360 stack parameter 9 (sp+0x5C) == NULL

        if (mIconManagerOwnerId != luPreviousOwnerId)
        {
            // Console stores in this order: stb +0xAA1C, stw +0xAA08, stw +0xAA04. There is
            // no setter for any of the three -- the state writes them directly.
            mpIconManager->mbRotateSatNav   = false;
            mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;
            mpIconManager->meIconSizeMode   = MapIconManager::E_ICONSIZE_LARGE;
        }
    }
}

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"                      // BrnGui::MapIconManager

namespace BrnGui
{
    // ================================================================================
    //  SetupComponents  @ 0x824D8C40  (cpp:556, vtable slot +48)
    //
    //  Once the cache reports every expected component initialised, hand the freshly
    //  created apt objects to the pieces that drive them, then re-apply the panel's
    //  current icon filter to the manager.
    // ================================================================================
    void CrashNavMap::SetupComponents()
    {
        // cpp:556 -- non-fatal on the X360 (the manager is dereferenced regardless).
        CGS_ASSERT(mpIconManager != 0, "mpIconManager");

        mpIconManager->SetupComponent();
        mCrashNavPanel.SetupComponent();
        SetFilterFromPanel();
    }
}
