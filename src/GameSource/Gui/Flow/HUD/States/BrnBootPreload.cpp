#include "GameSource/Gui/Flow/HUD/States/BrnBootPreload.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // CgsModule::VariableEventQueue (the in-queue)
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"                                   // GuiCache::EnsureResourcesAreLoaded (the stage gates)

// BrnGui::BootPreload -- reconstructed from BURNOUT_X360_ARTIST.XEX
//   (OnEnter @0x82473A10, OnLeave @0x82473A60, Update @0x82477F28,
//    GetResourcesToLoad @0x82508070 -- inline in the header).
//
// BF_PRELOAD is the FIRST HUD-flow state: it waits for the GUI cache's preload
// resource set, plays the AS FRAMEWORK movie "main" at display level 0 (the movie
// whose frame-0 DoAction runs `new AptCommunicator` + the Object.registerClass
// bootstrap -- the asm string xref @0x82478110 pins the name), raises the loading
// screen, and signals phase-complete: command 70 on the GUI-out channel (40) --
// the game main flow's generic "GUI phase done" -- plus the preload-done command
// 72 on channels 42 and 40.
namespace BrnGui
{
    namespace
    {
        const s32 KI_EVENT_GUI_CACHE = 64;   // the per-frame cache event (GuiCache* payload)

        const s32 KI_CHANNEL_GUI_OUT  = 40;  // GuiEventOut
        const s32 KI_CHANNEL_INTERNAL = 42;  // internal command channel

        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

        // The event-64 record: the GUI module posts the cache pointer each frame.
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        // 16-byte GuiEvent<N> command with header { 1, N, 12 } and one trailing flag byte
        // (the same record + helper shape BrnBootLegal.cpp uses for its channel commands).
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;    // +0x0C (the X360 stores a single byte then leaves the record at 16)
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel)
        {
            GuiCommandEvent16<N> lEvent(0);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lEvent), liChannel, 16);
        }
    }

    // The one event this state observes (X360 .rdata table the OnEnter registration
    // points at; the Update body dispatches only 64).
    const s32 BootPreload::maiEventToObserve[] = { KI_EVENT_GUI_CACHE };
    const s32 BootPreload::miNumEventsObserved = 1;

    // ⭐ RECOVERED 2026-08-16 (boot audit F-P8b-11). This table was empty behind a
    // "FLAG (unrecovered .rdata)" note; the premise was wrong. It is a STATIC .data table
    // at 0x82F25D10 with its count at 0x82F25D0C (= 68), read straight out of the
    // decrypted XEX, and nothing populates it at runtime -- only GetResourcesToLoad
    // @0x82508070 and Update @0x82477F28 reference it. The names come from the id->name
    // table off_82F278E0.
    //
    // What it IS: the ENTIRE in-game HUD component / mask / FSM / pfx set, preloaded
    // BEHIND the loading screen before the title. BF_PRELOAD parks on this set: the
    // console does not signal its phase-complete volley until every one of these is
    // resident, which is why the whole HUD exists the instant gameplay starts instead of
    // being faulted in lazily.
    const CgsGui::sResourceTuple BootPreload::maSecondPhaseResourcesToLoad[68] =
    {
        { 202u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // PreRaceBackgroundMask
        { 203u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // MainMapBackgroundMask
        { 204u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // Icons_EventIcon_NotAttempted_Anim
        { 205u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // Icons_EventIcon_Completed_Anim
        { 206u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // Icons_CrashNavIcon
        { 235u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // RoadSigns_0
        { 199u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // SatNavMap
        { 201u, CgsGui::E_GUI_RESOURCETYPE_LOCALISED_TEXT },   // SatNavMask
        {  31u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MultiTextField
        { 192u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5RaceHud
        { 193u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5CrashedHud
        { 194u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5CrashedStuntHud
        { 195u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5IdleHud
        {  40u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5CustomComponentTexture
        {  16u, CgsGui::E_GUI_RESOURCETYPE_PFX_BUNDLE },       // BRNEVENTFSM
        {  56u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5Triggers
        {  60u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // BoostMessage
        {  22u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ComponentUnity
        {  23u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5NorthIndicatorComponent
        {  24u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5CompassComponent
        {  27u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // CountdownIcon
        {  37u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5HudMessage
        {  38u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5CrashedHudMessages
        {  39u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5PreRaceMessageComponent
        { 200u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5SatNavComponent
        {  26u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // SatNavStatic
        {  25u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // SatNavDistance
        {  29u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // TextField
        {  32u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // Timer
        {  33u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5RoadRuleComponent
        {  62u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5PositionIndicatorComponent
        {  36u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MenuItem
        {  58u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ScrollableSelection
        {  59u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5HelpItem
        {  41u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MapCursor
        {  44u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ProgressBar
        {  45u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MugShot
        {  46u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // RoadRuleShot
        {  47u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // Ticker
        { 131u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // CrashNavTitleBar
        {  34u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MenuToggle
        {  57u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // Toggle
        {  61u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ControllerButtons
        {  63u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5HelperComponents
        {  64u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // DistrictIcon
        {  65u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // DistrictMarker
        {  69u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ColourSelector
        {  73u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5RaceEventInfo
        {  74u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5SatNavOverlay
        {  75u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5PositionTableComponent
        {  76u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5FriendList
        {  77u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5FriendListChangeIcon
        {  81u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5PaybackComponent
        {  82u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // PlayerStatsBar
        {  84u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5RoadSigns
        {  85u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5RoadRulerIcon
        {  87u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ShowTimeBar
        {  88u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5ShowtimeComponents
        {  71u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5MedalIcon
        {  89u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5JunctionInfoComponent
        {  90u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5VersionTextComponent
        {  95u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5SkipCrashPrompt
        {  96u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5AchievementIcons
        { 198u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5AlwaysAvailableContainer
        {  78u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5EATraxInGameComponent
        {  79u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5OnlineInviteComponent
        {  80u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },  // B5SaveIconComponent
        { 228u, CgsGui::E_GUI_RESOURCETYPE_PFX_COLOURCUBE_DICTIONARY },  // pfxhooks
    };
    const u32 BootPreload::muSecondPhaseNumResourcesToLoad = 68;

    // @ 0x82473A10 -- seed the wait-cache stage, register for the cache event, clear the cache.
    void BootPreload::OnEnter()
    {
        meUpdateStage = E_PRELOAD_WAIT_CACHE;
        if (mpStateInterface != 0)
            mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpGuiCache = 0;
    }

    // @ 0x82473A60 -- unregister the observed set.
    void BootPreload::OnLeave()
    {
        if (mpStateInterface != 0)
            mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
    }

    // ⭐ CORRECTED 2026-08-16 (boot audit F-P8b-11). Every stage gated on `cache != 0` --
    // a null check, not a residency check -- so BF_PRELOAD signalled its phase-complete
    // volley a couple of frames after the cache arrived with NOTHING preloaded, and the
    // whole in-game HUD set then faulted in lazily later. The console gates each stage on
    // GuiCache::EnsureResourcesAreLoaded(cache, <.rdata tuple table>, <count>) and parks
    // the state until the set is resident (@0x824780D4/F0/12C/5C/88/D0).
    //
    // The per-stage tables, recovered from the decrypted XEX with the second-phase set:
    //   stage WAIT_CACHE     unk_82F25F3C = {125 "main", 7}                     count 1
    //   (font pair, language-selected: [GuiCache+0xA7F8]==16 -> {20 DFHEIC,16}
    //    {21 JAMA,16} count 2, else {17,18,19 WesternB5*,16} count 3)
    //   stage LOADING_SCREEN unk_82F25F30 = {196 "FLAPTHUD", 10}                count 1
    //   stage SIGNAL_DONE    maSecondPhaseResourcesToLoad                       count 68
    namespace
    {
        const CgsGui::sResourceTuple KA_PRELOAD_MAIN[1] =
        {
            { 125u, CgsGui::E_GUI_RESOURCETYPE_FLAPT_HD_BUNDLE },      // "main" (AS framework movie)
        };
        const CgsGui::sResourceTuple KA_PRELOAD_FONTS_WESTERN[3] =
        {
            {  17u, CgsGui::E_GUI_RESOURCETYPE_FSM_BUNDLE },           // WesternB5Header_70
            {  18u, CgsGui::E_GUI_RESOURCETYPE_FSM_BUNDLE },           // WesternB5Body_35
            {  19u, CgsGui::E_GUI_RESOURCETYPE_FSM_BUNDLE },           // WesternB5DotMat_35
        };
        const CgsGui::sResourceTuple KA_PRELOAD_FONTS_CJK[2] =
        {
            {  20u, CgsGui::E_GUI_RESOURCETYPE_FSM_BUNDLE },           // DFHEIC
            {  21u, CgsGui::E_GUI_RESOURCETYPE_FSM_BUNDLE },           // JAMA
        };
        const CgsGui::sResourceTuple KA_PRELOAD_FLAPTHUD[1] =
        {
            { 196u, CgsGui::E_GUI_RESOURCETYPE_TEXTURE },              // FLAPTHUD
        };
    }

    // The console's gate: hand the table to the cache's StateLoadingHelper and only pass
    // when every tuple reports loaded. A null cache is "not ready" (the console cannot
    // reach these stages without one -- OnLeave asserts it @0x82473A7C, line 0x182).
    static bool PreloadResourcesReady(GuiCache* lpGuiCache,
                                      const CgsGui::sResourceTuple* lpResources,
                                      u32 luCount)
    {
        if (lpGuiCache == 0)
            return false;
        // GuiCache::EnsureResourcesAreLoaded @0x824FEB58 is itself just the forwarder
        // (`addi r3,r3,8; b StateLoadingHelper::EnsureResourcesAreLoaded` -- the helper
        // lives at cache+8).
        return lpGuiCache->EnsureResourcesAreLoaded(lpResources, luCount);
    }

    // The language-selected font pair (X360 selector [GuiCache+0xA7F8] == 16 -> CJK).
    static bool PreloadFontsReady(GuiCache* lpGuiCache)
    {
        if (lpGuiCache == 0)
            return false;
        // [FLAG PC bring-up] the language slot the console reads at GuiCache+0xA7F8 is not
        // homed by name yet, and the PC boot is the western SKU (langid 8), so the western
        // trio is selected unconditionally. The CJK arm is kept beside it so the selector
        // is a one-line change the day that member lands (boot audit B-P8a-2).
        return PreloadResourcesReady(lpGuiCache, KA_PRELOAD_FONTS_WESTERN, 3);
    }

    // @ 0x82477F28 -- consume the cache event, then walk the preload stage machine.
    void BootPreload::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        if (lpInQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            if (liEventId == KI_EVENT_GUI_CACHE)
            {
                if (mpGuiCache == 0)
                {
                    GuiCache* lpCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    CGS_ASSERT(lpCache != 0, "Invalid cache in BrnBootPreload::Update");
                    mpGuiCache = lpCache;
                }
            }
            else
            {
                CGS_ASSERT(false, "Unexpected event in IntroHudState::Update");
            }
        }

        switch (meUpdateStage)
        {
        case E_PRELOAD_WAIT_CACHE:
            // Wait for the cache AND its preload set (@0x824780D4/F0: the "main" movie
            // plus the language-selected font faces), then play the AS framework movie
            // "main" at display level 0 (the level-0 root the component classes live in).
            if (!PreloadResourcesReady(mpGuiCache, KA_PRELOAD_MAIN, 1) ||
                !PreloadFontsReady(mpGuiCache))
                break;
            mpStateInterface->PlayAptMovie("main", 0);
            meUpdateStage = E_PRELOAD_LOADING_SCREEN;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_LOADING_SCREEN:
            // @0x8247812C -- the Flapt HUD texture bundle is gated before the loading
            // screen goes up.
            if (!PreloadResourcesReady(mpGuiCache, KA_PRELOAD_FLAPTHUD, 1))
                break;
            mpStateInterface->PlayLoadingScreen();
            meUpdateStage = E_PRELOAD_SETTLE;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_SETTLE:
            meUpdateStage = E_PRELOAD_SIGNAL_DONE;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_SIGNAL_DONE:
            // @0x82478188-D0 -- THE preload park: the phase-complete volley below does not
            // go out until the ENTIRE second-phase set (the whole in-game HUD component /
            // mask / FSM / pfx list) is resident. This is the gate that makes the console's
            // HUD exist the instant gameplay starts.
            if (!PreloadResourcesReady(mpGuiCache, maSecondPhaseResourcesToLoad,
                                       muSecondPhaseNumResourcesToLoad))
                break;
            // Phase complete: 70 on the GUI-out channel (the game main flow advances on
            // it), then the preload-done command 72 on the internal + GUI-out channels.
            PostCommand16<70>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            PostCommand16<72>(mpStateInterface, KI_CHANNEL_INTERNAL);
            PostCommand16<72>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            meUpdateStage = E_PRELOAD_DRAIN;
            lpInQueue->Clear();
            return;

        case E_PRELOAD_DRAIN:
            meUpdateStage = E_PRELOAD_DONE;
            break;

        case E_PRELOAD_DONE:
        default:
            break;
        }

        lpInQueue->Clear();
    }
}
