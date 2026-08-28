// ===================================================================================
// BrnGui::CrashNavDriverDetails -- the OFFLINE IN-GAME PAUSE SCREEN (the "Driver
// Details" screen the console opens on START during free burn). See the header for the
// entry chain and the measured member layout.
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   CrashNavDriverDetails       @0x825001E0  (vtable-only ctor -- the implicit default
//                                             reproduces it; see the header)
//   GetResourcesToLoad          @0x82500308  (inline in the header)
//   OnEnter                     @0x824CEC80
//   OnLeave                     @0x824CEF18
//   Update                      @0x824DEB68
//   UpdateInitSetup             @0x824CF038
//   UpdateSetupLicense          @0x824C1C58
//   UpdateLoadResources         @0x824CF1E0
//   UpdateWFInit                @0x824BFE40
//   UpdateRunning               @0x824B83E0
//   UpdatePermanent             @0x824D9C18
//   ShowMenu                    @0x824BD0A0
//   HandleTriggers              @0x824B8548
//   HandleControllerInputPressed@0x824CF3B8
//   HandleStatData              @0x824B8618
//   UpdateStatsPanel            @0x824B8B80
//   HideTickerMessage           -- X360-INLINED into OnLeave (DWARF cpp:1205), de-inlined
//
// Every .rdata table below was read out of the image with the verified reader (the same
// one the 2026-08-27 GetResourcesToLoad sweep used), NOT guessed:
//   maiEventToObserve   @0x820664F0  {6,21,64,436,438,350,569,538}
//   maResourcesToLoad   @0x82066518  {{144,4},{63,4}}, count @0x82066528 = 2
//     -- type word 4 == E_GUI_RESOURCETYPE_APT (read from the image, NOT assumed:
//        an earlier draft of this file guessed FLAPT_HD_BUNDLE, which is 7)
//   KAPC_STAT_TEXTFIELD_NAMES        @0x82F271B8 (33 pointers)
//   KAPC_DISTRICT_{BILLBOARD,JUMPS,SMASHES}_TEXTFIELD_NAMES @0x82F2723C/50/64 (5 each)
//   KAPC_MENU_TEXT      @0x82F2701C  {"$DDETAILS_LICENSE","$DDETAILS_RECORDS","$DDETAILS_DISCOVER"}
//   the apt movie name  @0x82F27B20  "BrnCrashNavDriverDetails"
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCrashNavDriverDetails.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                     // CgsCore::SnPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"                // CgsDev::StrStreamBase (the two <<-asserts)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"             // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                         // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"    // StateInterface / GuiEventNetworkSuspension / PlayAptMovie
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<18432,16> (in-queue view)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload / AptEventType
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"             // ButtonIconComponent::EPadButton
#include "GameSource/Gui/BrnGuiCache.h"                                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                             // GuiEventActivateCrashNav / GuiFlow
#include "GameSource/Gui/BrnGuiWorldDataController.h"                       // WorldDataController::GetProgressionData
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                       // GuiEventStatsResponse (event 436)
#include "GameSource/Gui/Events/BrnGuiEventRankProgressResponse.h"          // GuiEventRankProgressResponse (event 438)
#include "SharedClasses/Progression/BrnProgressionData.h"                   // ProgressionData::GetProgressionRankCount
#include "GameSource/GameState/Progression/BrnProfile.h"                           // BrnProgression::Profile (mpProfile / the finished-game byte)

namespace BrnGui
{
// ---- .rdata statics ---------------------------------------------------------------
// @0x820664F0. 6 == controller-action, 21 == apt trigger, 64 == the per-frame GuiCache
// event, 436 == the stats response, 438 == the rank-progress response, 350 == the
// profile-pointer event, 569 == the compressed still image, 538 == (observed, no arm in
// this state's dispatch -- the console registers it and never switches on it).
const s32 CrashNavDriverDetails::maiEventToObserve[8] = { 6, 21, 64, 436, 438, 350, 569, 538 };
const s32 CrashNavDriverDetails::miNumEventsObserved  = 8;

// @0x82066518 / @0x82066528. Ids resolve through BrnGuiCache.cpp's
// gGuiResourceIdentifier: 144 == "BrnCrashNavDriverDetails" (the screen's own apt movie,
// GUIAPT/BRNCRASHNAVDRIVERDETAILS.bundle) and 63 == "B5HelperComponents".
const CgsGui::sResourceTuple CrashNavDriverDetails::maResourcesToLoad[] =
{
    { 144u, CgsGui::E_GUI_RESOURCETYPE_APT },   // BrnCrashNavDriverDetails (the screen's movie)
    {  63u, CgsGui::E_GUI_RESOURCETYPE_APT },   // B5HelperComponents
};
const u32 CrashNavDriverDetails::muNumResourcesToLoad = 2;

namespace
{
    // ---- out-queue channels (the AddEvent selector word) --------------------------
    const s32 KI_CHANNEL_GUI_OUT   = 40;   // GuiEventOut
    const s32 KI_CHANNEL_VIEW_STATE = 41;  // OutputViewState (the apt-movie record)

    // ---- observed event ids (the table above, by name at the dispatch sites) ------
    const s32 KI_EVENT_CONTROLLER_PRESSED = 6;
    const s32 KI_EVENT_APT_TRIGGER        = 21;
    const s32 KI_EVENT_GUI_CACHE          = 64;
    const s32 KI_EVENT_PROFILE_POINTER    = 350;
    const s32 KI_EVENT_STATS_RESPONSE     = 436;
    const s32 KI_EVENT_RANK_PROGRESS      = 438;
    const s32 KI_EVENT_STILL_IMAGE        = 569;

    // ---- controller action sub-ids (payload word +4). Roles are this switch's; the
    //      ids are the console vocabulary (GameInputActions.h: 41/42 the menu pair,
    //      45 GUI_START, 49 GUI_SELECT, 50 GUI_CANCEL, 54/55 the shoulder pair). ----
    const s32 KI_ACTION_MENU_41    = 41;   // -> menu vtable +0x38 == HighlightPrevious
    const s32 KI_ACTION_MENU_42    = 42;   // -> menu vtable +0x34 == HighlightNext
    const s32 KI_ACTION_START      = 45;   // -> leave the pause screen (resume)
    const s32 KI_ACTION_SELECT     = 49;   // -> photo-booth confirm
    const s32 KI_ACTION_CANCEL     = 50;   // -> leave the pause screen (resume) / photo cancel
    const s32 KI_ACTION_TAKE_PHOTO = 52;   // -> open the photo booth
    const s32 KI_ACTION_TOGGLE_L   = 54;   // -> "TOGGLE_LEFT"  (the CrashNav tab shuffle)
    const s32 KI_ACTION_TOGGLE_R   = 55;   // -> "TOGGLE_RIGHT"

    // ---- the menu rows the licence/records/discover tabs drive (@0x82F2701C) ------
    const s32 KI_NUM_MENU_ROWS = 3;
    const char* const KAPC_MENU_TEXT[KI_NUM_MENU_ROWS] =
    {
        "$DDETAILS_LICENSE", "$DDETAILS_RECORDS", "$DDETAILS_DISCOVER",
    };

    // The apt view-state each menu row transitions the stats panel to (the three
    // literals UpdateStatsPanel pushes at "apt_Transition").
    const char* const KPC_STATS_PANEL_LICENSE  = "License";
    const char* const KPC_STATS_PANEL_RECORDS  = "Records";
    const char* const KPC_STATS_PANEL_DISCOVER = "Discover";

    // ---- apt component names (@0x82F271B8 and the three district tables) ----------
    const char* const KAPC_STAT_TEXTFIELD_NAMES[CrashNavDriverDetails::KI_NUM_STAT_TEXTFIELDS] =
    {
        "hoursPlayed_cpt",        "percentageComplete_cpt", "totalMileage_cpt",
        "carsWon_cpt",            "carsToShutdown_cpt",     "roadsRuledAmount_cpt",
        "roadRulesTime_cpt",      "roadRulesShowtime_cpt",  "eventsFound_cpt",
        "driveThrusFound_cpt",    "smashes_cpt",            "stunts_cpt",
        "jumps_cpt",              "racesOne_cpt",           "roadRagesOne_cpt",
        "markedManOne_cpt",       "challengesOne_cpt",      "stuntRunsOne_cpt",
        "bestShowtime_cpt",       "bestStuntRun_cpt",       "totalTakedowns_cpt",
        "bestDrift_cpt",          "bestBoostChain_cpt",     "bestOncoming_cpt",
        "bestAirTime_cpt",        "bestSpin_cpt",           "bestBarrelRoll_cpt",
        "bestPowerParking_cpt",   "bestRoadRage_cpt",       "bodyShopsFound_cpt",
        "gasStationsFound_cpt",   "paintShopsFound_cpt",    "carParksFound_cpt",
    };

    const char* const KAPC_DISTRICT_BILLBOARD_TEXTFIELD_NAMES[CrashNavDriverDetails::KI_NUM_DISTRICTS] =
        { "bbPBH_cpt",  "bbSL_cpt",  "bbHT_cpt",  "bbWM_cpt",  "bbDTP_cpt"  };
    const char* const KAPC_DISTRICT_JUMPS_TEXTFIELD_NAMES[CrashNavDriverDetails::KI_NUM_DISTRICTS] =
        { "jmpPBH_cpt", "jmpSL_cpt", "jmpHT_cpt", "jmpWM_cpt", "jmpDTP_cpt" };
    const char* const KAPC_DISTRICT_SMASHES_TEXTFIELD_NAMES[CrashNavDriverDetails::KI_NUM_DISTRICTS] =
        { "smhPBH_cpt", "smhSL_cpt", "smhHT_cpt", "smhWM_cpt", "smhDTP_cpt" };

    const char KAC_MENU_COMPONENT[]         = "MenuItem";                 // DWARF cpp:56
    const char KAC_LICENSECOMP_NAME[]       = "License_cpt";              // DWARF cpp:58
    const char KAC_PHOTOBOOTHCOMP_NAME[]    = "PhotoBooth_cpt";           // DWARF cpp:64
    const char KAC_STATSPANEL_ANIM_NAME[]   = "statsPanelAnimator_cpt";   // DWARF cpp:66
    const char KAC_TAKEPHOTO_PROMPT_NAME[]  = "TakePhotoPrompt_cpt";      // DWARF cpp:148
    const char KAC_ACHIEVEMENTS_PROMPT_NAME[] = "AchievementPrompt_cpt";  // DWARF cpp:68

    // The screen's own apt movie and the level it plays on (@0x82F27B20 / `li 3`).
    const char KPC_DRIVER_DETAILS_MOVIE[] = "BrnCrashNavDriverDetails";
    const s32  KI_APT_MOVIE_LEVEL         = 3;

    // ⚠️ MEASURED, NOT ASSUMED. KV2_LICENSE_POSITION / KV2_LICENSE_POSITION_SD (DWARF
    // cpp:61/:62) live at X360 .bss unk_82FB4A90 / unk_82FB4C00 -- both read all-zero in
    // the image, and a whole-image scan of the export set finds exactly ONE reference to
    // either address: this file's UpdateWFInit. So the console selects between two
    // zero vectors here; the branch is faithful and the values are the image's, not a
    // placeholder. (A non-zero const Vector2 would have been constant-folded into
    // .rdata; landing zeroed in .bss is the signature of a zero initialiser.)
    // FLAG measured-zero: if a writer is ever found, correct BOTH here.
    // The console loads each with a single `lvx128 v1` -- one 16-byte VMX quad, which is
    // what a by-value rw Vector2 is -- and passes it straight to LicenseComponent::SetPosition.
    const Vector2 KV2_LICENSE_POSITION    = { 0.0f, 0.0f, 0.0f, 0.0f };   // unk_82FB4A90 (HD)
    const Vector2 KV2_LICENSE_POSITION_SD = { 0.0f, 0.0f, 0.0f, 0.0f };   // unk_82FB4C00 (SD)

    // The HelpItem button-icon slot the console passes for "no button" (`li 15`); the
    // empty text is X360 unk_820046A7, the image's shared "" literal.
    const char* const KPC_EMPTY_STRING = "";

    // The take-photo prompt's localisation key and its button slot (`li 7`).
    const char* const KPC_TAKE_PHOTO_STRINGID = "$CAPS_BUTTON_TAKE_PHOTO";

    // ---- the state in-queue (the committed BrnCrashNavStats / BrnCredits idiom) ----
    typedef CgsModule::VariableEventQueue<18432, 16> DriverDetailsInQueue;

    // The event-64 payload view (the queue delivers the header-stripped payload; the
    // member name is the X360 assert's: "lpCacheEvent->mpCachePointer").
    struct GuiEventCache : public CgsModule::Event
    {
        GuiCache* mpCachePointer;
    };

    // The event-350 payload view -- one profile pointer (the X360 reads `*i` and hands
    // it to LicenseComponent::SetProfilePointer / PhotoBoothComponent::SetProfilePointer).
    struct GuiEventProfilePointer : public CgsModule::Event
    {
        BrnProgression::Profile* mpProfile;
    };

    // ---- the small command records the screen posts ------------------------------
    // { 1, 539, 12 } ch 40 / 16 bytes -- posted when the photo booth is cancelled.
    struct GuiEventPhotoBoothCancelled : public CgsGui::GuiEvent<539>
    {
        u32 muReserved;   // +0x0C (X360 leaves the gap word uninitialised; record size 16)
        GuiEventPhotoBoothCancelled() : CgsGui::GuiEvent<539>(1, 12), muReserved(0) {}
    };

    // { 1, 533, 12 } ch 40 / 16 bytes -- the "front-end screen closed" record, the same
    // one BrnPauseScreen and CrashNavMapMain post on the way back into the game.
    struct GuiEventScreenClosed : public CgsGui::GuiEvent<533>
    {
        u32 muReserved;   // +0x0C
        GuiEventScreenClosed() : CgsGui::GuiEvent<533>(1, 12), muReserved(0) {}
    };

    // { 1, 435, 12 } and { 1, 437, 12 } ch 40 / 16 bytes -- the two data requests the
    // screen fires the frame it gets the cache (the stats + rank-progress queries whose
    // 436 / 438 responses UpdatePermanent and UpdateSetupLicense consume).
    struct GuiEventStatsRequest : public CgsGui::GuiEvent<435>
    {
        u32 muReserved;
        GuiEventStatsRequest() : CgsGui::GuiEvent<435>(1, 12), muReserved(0) {}
    };
    struct GuiEventRankProgressRequest : public CgsGui::GuiEvent<437>
    {
        u32 muReserved;
        GuiEventRankProgressRequest() : CgsGui::GuiEvent<437>(1, 12), muReserved(0) {}
    };

    // { 2, 536, 12, 0 } ch 40 / 16 bytes -- HideTickerMessage's record.
    struct GuiEventHideTickerMessage : public CgsGui::GuiEvent<536>
    {
        u16 mu16Reserved;   // +0x0C (the X360 stores a halfword 0 here)
        u16 mu16Pad;
        GuiEventHideTickerMessage() : CgsGui::GuiEvent<536>(2, 12), mu16Reserved(0), mu16Pad(0) {}
    };

    // The channel-41 apt-movie record UpdateLoadResources posts. The X360 stack-builds it
    // inline rather than calling StateInterface::PlayAptMovie, but the bytes are
    // byte-identical to that helper's ({ 8, 18, 12, name, level } at 20 bytes on 41), so
    // it is de-inlined back to the named record type.
    typedef CgsGui::GuiEventPlayAptMovie GuiEventPlayAptMovieRecord;

    // The stats-response payload (event 436). BrnGuiDemangledEventTypes.h models it as an
    // opaque 420-byte blob, so this is the same file-local boundary reader the committed
    // BrnCrashNavStats.cpp:99 uses over the SAME event -- an external-record boundary,
    // documented inline, not an offset hack on a C++ class.
    struct StatsReader
    {
        const u8* mpBase;
        s32 Word(u32 luOffset) const { return *reinterpret_cast<const s32*>(mpBase + luOffset); }
    };
}

// ---------------------------------------------------------------- OnEnter @0x824CEC80
// Register the 8 observed ids, POST THE PAUSE (the deactivate pair), build every apt
// component the screen owns, then put the per-visit state back to its cold start and
// fire the stats query.
void CrashNavDriverDetails::OnEnter()
{
    mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // ⭐ THE PAUSE. Byte-identical to CrashNavMapMain::OnEnter @0x824CCA44/4C/50:
    //     { 8, 191, 12, 0, 0 } ch 40 / 20  == GuiEventActivateCrashNav(false)
    //     { 4,  45, 12, 1    } ch 40 / 16  == GuiEventNetworkSuspension(true)
    // GameBridgeGUIToX_GameState's case 191 turns the 191{0} into game event 93 payload 1
    // -> RequestPause(4) -> action 86 -> mbSimPaused, and ConstructUpdateSetFromFsm then
    // ORs bit 0x1 into the in-game update set.
    GuiEventActivateCrashNav lDeactivate(false);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lDeactivate), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(GuiEventActivateCrashNav)));

    CgsGui::GuiEventNetworkSuspension lSuspend(true);
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lSuspend), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(CgsGui::GuiEventNetworkSuspension)));

    // The 33 stat fields, then the three district columns in lock-step (the X360 runs one
    // loop over the stat names and a second, 5-iteration loop that constructs one field
    // from each district table per pass). Each Construct is the TextField vtable slot 0.
    for (s32 liField = 0; liField < KI_NUM_STAT_TEXTFIELDS; ++liField)
        maStatTextfields[liField].Construct(KAPC_STAT_TEXTFIELD_NAMES[liField], mpStateInterface, 0);

    for (s32 liDistrict = 0; liDistrict < KI_NUM_DISTRICTS; ++liDistrict)
    {
        maDistrictBillboardsTextfields[liDistrict].Construct(
            KAPC_DISTRICT_BILLBOARD_TEXTFIELD_NAMES[liDistrict], mpStateInterface, 0);
        maDistrictJumpsTextfields[liDistrict].Construct(
            KAPC_DISTRICT_JUMPS_TEXTFIELD_NAMES[liDistrict], mpStateInterface, 0);
        maDistrictSmashesTextfields[liDistrict].Construct(
            KAPC_DISTRICT_SMASHES_TEXTFIELD_NAMES[liDistrict], mpStateInterface, 0);
    }

    // The menu: 3 rows, no parent, no apt id (`li r6,3; li r7,0; li r8,-1`).
    mMenuComponent.Construct(KAC_MENU_COMPONENT, mpStateInterface, KI_NUM_MENU_ROWS, 0,
                             static_cast<u64>(-1));

    mStatsPanelAnimator.Construct(KAC_STATSPANEL_ANIM_NAME, mpStateInterface, 0);
    mLicenseComponent.Construct(KAC_LICENSECOMP_NAME, mpStateInterface, 0);

    mbShowLicense = false;                                       // stb 0, +0x50DC
    // `li r6,5; li r7,4; li r8,1; li r9,1; li r10,0` == (back = B, confirm = A,
    // take-photo string = TAKEPHOTO, back string = BACK, no parent).
    mPhotoBoothComponent.Construct(KAC_PHOTOBOOTHCOMP_NAME, mpStateInterface,
                                   ButtonIconComponent::E_PADBUTTON_BACK,
                                   ButtonIconComponent::E_PADBUTTON_SELECT,
                                   PhotoBoothComponent::E_TAKEPHOTOSTRING_TAKEPHOTO,
                                   PhotoBoothComponent::E_BACKSTRING_BACK, 0);
    mTakePhotoPrompt.Construct(KAC_TAKEPHOTO_PROMPT_NAME, mpStateInterface, 0);

    mbRefreshTakePhotoPrompt = true;                             // stb 1, +0x56C0
    mbIsCameraConnected      = false;                            // stb 0, +0x56C1
    mAchievementsPrompt.Construct(KAC_ACHIEVEMENTS_PROMPT_NAME, mpStateInterface, 0);

    mbStatsDataReceived       = false;                           // stb 0, +0x4908
    mpGuiCache                = 0;                               // stw 0, +0x490C
    meAchievementsTickerState = E_ACHIEVEMENTS_TICKER_INACTIVE;   // stw 0, +0x56C4

    // { 1, 539, 12 } -- the screen's own "opened" record (the same id the photo-booth
    // cancel arm posts; the console posts it here unconditionally).
    GuiEventPhotoBoothCancelled lOpened;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lOpened), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(lOpened)));

    meInternalState = E_INTERNALSTATE_GETCACHE;                  // stw 0, +0x3844
}

// ----------------------------------------------------------- HideTickerMessage (inlined)
// DWARF cpp:1205. The console folds it into OnLeave's head; the record is
// { 2, 536, 12, 0 } on channel 40 at 16 bytes.
void CrashNavDriverDetails::HideTickerMessage()
{
    GuiEventHideTickerMessage lHide;
    mpStateInterface->GetOutputEventQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lHide), KI_CHANNEL_GUI_OUT,
        static_cast<s32>(sizeof(lHide)));
}

// ---------------------------------------------------------------- OnLeave @0x824CEF18
void CrashNavDriverDetails::OnLeave()
{
    if (meAchievementsTickerState == E_ACHIEVEMENTS_TICKER_SHOWING)
        HideTickerMessage();

    mMenuComponent.Clear();                       // component vtable slot 6 (+0x18)
    mLicenseComponent.ReleaseResources();
    mPhotoBoothComponent.ReleaseResources();

    if (mpGuiCache != 0)
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

    mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

    // Stop the screen's apt movie: the same { 8, 18, 12, name, level } record
    // UpdateLoadResources posts, with the EMPTY name and level 3 -- exactly what
    // StateInterface::PlayAptMovie("", 3) builds (CrashNavStats::OnLeave posts the
    // identical pair through the named helper).
    mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

    meInternalState = E_INTERNALSTATE_LEAVING;
}

// ----------------------------------------------------------------- Update @0x824DEB68
// The fall-through ladder: each rung that reports "done" advances the state AND runs the
// next rung in the same frame (the X360's chained `goto`s), and every rung except
// LEAVING is followed by the permanent per-frame pass.
void CrashNavDriverDetails::Update()
{
    bool lbRunPermanent = true;

    switch (meInternalState)
    {
    case E_INTERNALSTATE_GETCACHE:
        if (!UpdateInitSetup())
            break;
        // fall through
    case E_INTERNALSTATE_SETUPLICENSE:
        meInternalState = E_INTERNALSTATE_SETUPLICENSE;
        if (!UpdateSetupLicense())
            break;
        // fall through
    case E_INTERNALSTATE_LOADRESOURCES:
        meInternalState = E_INTERNALSTATE_LOADRESOURCES;
        if (!UpdateLoadResources())
            break;
        // fall through
    case E_INTERNALSTATE_WFINIT:
        meInternalState = E_INTERNALSTATE_WFINIT;
        if (!UpdateWFInit())
            break;
        // fall through
    case E_INTERNALSTATE_RUNNING:
        meInternalState = E_INTERNALSTATE_RUNNING;
        UpdateRunning();
        break;

    case E_INTERNALSTATE_LEAVING:
        lbRunPermanent = false;
        break;

    default:
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "Invalid internal state : "
                                       << static_cast<s32>(meInternalState) << "\n";
        CGS_ASSERT(false, "Invalid internal state");   // cpp:358
        break;
    }

    // [ddetails] change-only ladder trace.
    {
        static s32 s_iLastState = -1;
        if (static_cast<s32>(meInternalState) != s_iLastState)
        {
            s_iLastState = static_cast<s32>(meInternalState);
            if (CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[ddetails] internal state -> " << s_iLastState << "\n";
        }
    }

    if (lbRunPermanent)
        UpdatePermanent();

    // The console clears its in-queue at the end of every Update, whatever the rung did.
    reinterpret_cast<DriverDetailsInQueue*>(mpInGuiEventQueue)->Clear();
}

// -------------------------------------------------------- UpdateInitSetup @0x824CF038
// Wait for the per-frame GuiCache event, latch the cache into this state and both
// data-owning components, and fire the two profile queries.
bool CrashNavDriverDetails::UpdateInitSetup()
{
    CGS_ASSERT(mpGuiCache == 0, "NULL == mpGuiCache");   // cpp:394

    bool lbGotCache = false;

    DriverDetailsInQueue* lpInQueue = reinterpret_cast<DriverDetailsInQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId != KI_EVENT_GUI_CACHE)
            continue;

        const GuiEventCache* lpCacheEvent = reinterpret_cast<const GuiEventCache*>(lpEvent);
        CGS_ASSERT(lpCacheEvent->mpCachePointer != 0,
                   "NULL != lpCacheEvent->mpCachePointer");      // cpp:404

        lbGotCache = true;
        mpGuiCache = lpCacheEvent->mpCachePointer;

        GuiEventStatsRequest lStatsRequest;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lStatsRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lStatsRequest)));

        GuiEventRankProgressRequest lRankRequest;
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRankRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRankRequest)));

        // The two component-side latches (each carries its own assert on the console --
        // BrnLicenseComponent.h:366 and BrnPhotoBoothComponent.h:262).
        mLicenseComponent.SetCachePointer(mpGuiCache);
        mPhotoBoothComponent.SetCachePointer(mpGuiCache);
        break;
    }

    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:428
    return lbGotCache;
}

// ----------------------------------------------------- UpdateSetupLicense @0x824C1C58
// Wait for the rank-progress response and hand everything the licence card shows to the
// LicenseComponent, then pick the DMV photo-booth style.
bool CrashNavDriverDetails::UpdateSetupLicense()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:451

    bool lbSetUp = false;

    DriverDetailsInQueue* lpInQueue = reinterpret_cast<DriverDetailsInQueue*>(mpInGuiEventQueue);
    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        if (liEventId != KI_EVENT_RANK_PROGRESS)
            continue;

        const GuiEventRankProgressResponse* lpRankEvent =
            reinterpret_cast<const GuiEventRankProgressResponse*>(lpEvent);

        // The console reads the response's rank word INLINE here (lwz +0x20) rather than
        // through GetPlayerRank, because the sentinel is exactly what it is testing for.
        const bool lbFinishedLastRank = lpRankEvent->HasPlayerFinishedLastRank();

        s32 liRank;
        s32 liPointsToNextRank;
        if (lbFinishedLastRank)
        {
            // No online rank in the response: fall back to the progression table's last
            // rank index (`*(progressionData + 0x14) - 1`).
            liRank = static_cast<s32>(
                mpGuiCache->GetWorldDataController()->GetProgressionData()
                    ->GetProgressionRankCount()) - 1;
            liPointsToNextRank = 0;
        }
        else
        {
            liRank             = lpRankEvent->GetPlayerRank();
            liPointsToNextRank = static_cast<s32>(mpGuiCache->GetLicencePointsToNextRank());
        }

        const BrnProgression::Profile* lpProgressionProfile = mpGuiCache->GetProfile();
        CGS_ASSERT(lpProgressionProfile != 0, "lpProgressionProfile");   // cpp:477

        // lbElite is the SENTINEL ITSELF (the X360 computes it as cntlzw(rank+1)>>5, i.e.
        // "the response carried -1"); lbFinishedGame is the profile's completion byte.
        mLicenseComponent.SetPlayerInfo(mpGuiCache->GetPlayerName(),
                                        lbFinishedLastRank,
                                        lpProgressionProfile->AreGoldCarsUnlocked(),
                                        liRank, liPointsToNextRank,
                                        /*lbShowUpgradePending*/ false,
                                        /*lbShowPoints*/ true);

        // `stw 0x5B, +0x5174` -- the photo booth's resource id, i.e. the inlined
        // SetVisualStyle(E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE) (that case stores exactly
        // E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV == 91).
        mPhotoBoothComponent.SetVisualStyle(PhotoBoothComponent::E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE);

        lbSetUp = true;
        break;
    }

    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:499
    return lbSetUp;
}

// ---------------------------------------------------- UpdateLoadResources @0x824CF1E0
// Hold until the screen's two resources AND both components' resources are in, then
// re-arm the expected-apt-component bookkeeping for everything this screen owns and
// START THE SCREEN'S APT MOVIE.
bool CrashNavDriverDetails::UpdateLoadResources()
{
    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:519

    // [ddetails] change-only bring-up trace: which of the three resource gates is still
    // holding the ladder. Cheap and change-only, in the tree's dev-trace idiom.
    const bool lbScreenIn  = (mpGuiCache != 0)
        && mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad);
    const bool lbLicenceIn = lbScreenIn && mLicenseComponent.EnsureResourcesAreLoaded();
    const bool lbPhotoIn   = lbLicenceIn && mPhotoBoothComponent.EnsureResourcesAreLoaded();
    {
        static s32 s_iLastGates = -1;
        const s32 liGates = (lbScreenIn ? 1 : 0) | (lbLicenceIn ? 2 : 0) | (lbPhotoIn ? 4 : 0);
        if (liGates != s_iLastGates)
        {
            s_iLastGates = liGates;
            if (CgsDev::Log::gpDebugPrint != 0)
                *CgsDev::Log::gpDebugPrint << "[ddetails] resource gates: screen=" << (lbScreenIn ? 1 : 0)
                                           << " licence=" << (lbLicenceIn ? 1 : 0)
                                           << " photo=" << (lbPhotoIn ? 1 : 0) << "\n";
        }
    }
    if (!lbPhotoIn)
        return false;

    mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);

    // Every text field, the stats-panel animator and the take-photo prompt register their
    // apt component name with the cache (the X360 passes each component's macName to the
    // name-hashing GuiCache::AppendExpectedAptComponent overload).
    for (s32 liField = 0; liField < KI_NUM_STAT_TEXTFIELDS; ++liField)
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, maStatTextfields[liField].GetName());

    for (s32 liDistrict = 0; liDistrict < KI_NUM_DISTRICTS; ++liDistrict)
    {
        mpGuiCache->AppendExpectedAptComponent(
            E_GUIFLOW_SCREEN, maDistrictBillboardsTextfields[liDistrict].GetName());
        mpGuiCache->AppendExpectedAptComponent(
            E_GUIFLOW_SCREEN, maDistrictJumpsTextfields[liDistrict].GetName());
        mpGuiCache->AppendExpectedAptComponent(
            E_GUIFLOW_SCREEN, maDistrictSmashesTextfields[liDistrict].GetName());
    }

    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mStatsPanelAnimator.GetName());
    mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTakePhotoPrompt.GetName());

    mMenuComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
    mLicenseComponent.AppendExpectedAptComponent(E_GUIFLOW_SCREEN);
    mPhotoBoothComponent.AppendExpectedAptComponents(E_GUIFLOW_SCREEN);

    mLicenseComponent.OnLoad();
    mPhotoBoothComponent.OnLoad();

    // ⭐ THE SCREEN DRAWS FROM HERE: { 8, 18, 12, "BrnCrashNavDriverDetails", 3 } on the
    // view-state channel, i.e. the record StateInterface::PlayAptMovie builds, which the
    // X360 stack-builds inline at this site.
    mpStateInterface->PlayAptMovie(KPC_DRIVER_DETAILS_MOVIE, KI_APT_MOVIE_LEVEL);
    return true;
}

// ----------------------------------------------------------- UpdateWFInit @0x824BFE40
// Wait for the apt components to initialise, bring the menu up, place the licence card,
// and (if the stats already arrived) re-push every field's stored text now that its clip
// exists.
bool CrashNavDriverDetails::UpdateWFInit()
{
    CGS_ASSERT(mpGuiCache != 0, "NULL != mpGuiCache");   // cpp:581

    bool lbInitialised = false;

    if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
    {
        ShowMenu();

        mbShowLicense = true;
        mLicenseComponent.SetPosition(mpGuiCache->IsHighDefinition()
                                          ? KV2_LICENSE_POSITION
                                          : KV2_LICENSE_POSITION_SD);

        if (mbStatsDataReceived)
        {
            // field.SetText(field.GetText()) -- the console's `SetText(field, field+0xA4)`
            // re-push idiom (the text was formatted before the clip existed).
            for (s32 liField = 0; liField < KI_NUM_STAT_TEXTFIELDS; ++liField)
                maStatTextfields[liField].SetText(maStatTextfields[liField].GetText());

            for (s32 liDistrict = 0; liDistrict < KI_NUM_DISTRICTS; ++liDistrict)
            {
                maDistrictBillboardsTextfields[liDistrict].SetText(
                    maDistrictBillboardsTextfields[liDistrict].GetText());
                maDistrictJumpsTextfields[liDistrict].SetText(
                    maDistrictJumpsTextfields[liDistrict].GetText());
                maDistrictSmashesTextfields[liDistrict].SetText(
                    maDistrictSmashesTextfields[liDistrict].GetText());
            }
        }

        lbInitialised = true;
    }

    // Unconditional, outside the gate (the X360 runs it on the fall-through too): blank
    // the achievements prompt.
    // `li r5,15; li r6,15` -- E_PADBUTTON_INVISIBLE on both slots, i.e. text only.
    mAchievementsPrompt.SetItem(KPC_EMPTY_STRING,
                                ButtonIconComponent::E_PADBUTTON_INVISIBLE,
                                ButtonIconComponent::E_PADBUTTON_INVISIBLE);
    return lbInitialised;
}

// ---------------------------------------------------------- UpdateRunning @0x824B83E0
// The steady state: keep the take-photo prompt in step with the camera, and keep the
// licence card's visibility in step with the photo booth's.
bool CrashNavDriverDetails::UpdateRunning()
{
    if (mbRefreshTakePhotoPrompt)
    {
        // With a menu row other than "License" highlighted, or no camera attached, the
        // prompt is blank; otherwise it offers the take-photo button.
        const bool lbOfferTakePhoto =
            (mMenuComponent.miHighlightedIndex == 0) && (mpGuiCache->GetCamStatus() != 0);

        // `li r5,7` / `li r5,15` -- E_PADBUTTON_OPTION1 offers the glyph, and
        // E_PADBUTTON_INVISIBLE blanks it.
        mTakePhotoPrompt.SetItem(
            lbOfferTakePhoto ? KPC_TAKE_PHOTO_STRINGID : KPC_EMPTY_STRING,
            lbOfferTakePhoto ? ButtonIconComponent::E_PADBUTTON_OPTION1
                             : ButtonIconComponent::E_PADBUTTON_INVISIBLE,
            ButtonIconComponent::E_PADBUTTON_INVISIBLE);

        mbRefreshTakePhotoPrompt = false;
    }

    // Camera hot-plug: any change in "a camera is attached" re-arms the prompt next frame.
    const bool lbCameraConnected = (mpGuiCache->GetCamStatus() != 0);
    if (mbIsCameraConnected != lbCameraConnected)
        mbRefreshTakePhotoPrompt = true;

    const bool lbPhotoBoothWasVisible = mPhotoBoothComponent.IsActive();
    mbIsCameraConnected = lbCameraConnected;

    // The camera went away while the booth was open: shut the booth and put the licence
    // back on screen.
    if (lbPhotoBoothWasVisible && !lbCameraConnected)
    {
        mPhotoBoothComponent.HideComponent(false);
        mbShowLicense = true;
    }

    if (mbShowLicense)
    {
        if (!mLicenseComponent.IsVisible())
        {
            if (mLicenseComponent.IsFirstResourceLoaded())
                mLicenseComponent.ShowLicense(false);
        }
    }
    else if (mLicenseComponent.IsVisible() && !mLicenseComponent.IsHiding())
    {
        mLicenseComponent.HideLicense();
    }

    mPhotoBoothComponent.SendPlayerPictureEvent();
    mLicenseComponent.SendPlayerPictureEvent();
    return true;
}

// -------------------------------------------------------- UpdatePermanent @0x824D9C18
// The per-frame event pass that runs under every rung of the ladder.
bool CrashNavDriverDetails::UpdatePermanent()
{
    DriverDetailsInQueue* lpInQueue = reinterpret_cast<DriverDetailsInQueue*>(mpInGuiEventQueue);

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
         lpEvent != 0;
         liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
    {
        switch (liEventId)
        {
        case KI_EVENT_CONTROLLER_PRESSED:
            if (meInternalState == E_INTERNALSTATE_RUNNING)
                HandleControllerInputPressed(lpEvent);
            break;

        case KI_EVENT_APT_TRIGGER:
            HandleTriggers(lpEvent);
            break;

        case KI_EVENT_PROFILE_POINTER:
        {
            const GuiEventProfilePointer* lpProfileEvent =
                reinterpret_cast<const GuiEventProfilePointer*>(lpEvent);
            mLicenseComponent.SetProfilePointer(lpProfileEvent->mpProfile);
            CGS_ASSERT(lpProfileEvent->mpProfile != 0, "NULL != lpProfile");   // PhotoBooth h:280
            mPhotoBoothComponent.SetProfilePointer(lpProfileEvent->mpProfile);
            break;
        }

        case KI_EVENT_STATS_RESPONSE:
        {
            mbStatsDataReceived = true;
            const GuiEventStatsResponse* lpStatsEvent =
                reinterpret_cast<const GuiEventStatsResponse*>(lpEvent);
            HandleStatData(lpStatsEvent);
            // The percentage-complete word the licence card shows sits at +0xC4 of the
            // same record (the console reads `i[49]`).
            const StatsReader lReader = { reinterpret_cast<const u8*>(lpEvent) };
            mLicenseComponent.SetPercentageComplete(lReader.Word(0xC4));
            break;
        }

        case KI_EVENT_STILL_IMAGE:
            mPhotoBoothComponent.HandleCompressedStillImageEvent(lpEvent);
            break;

        default:
            break;
        }
    }

    mMenuComponent.Update();   // component vtable slot 5 (+0x14) == SelectableGroup::Update
    return true;
}

// ---------------------------------------------------------------- ShowMenu @0x824BD0A0
void CrashNavDriverDetails::ShowMenu()
{
    mMenuComponent.SetupMenu(KI_NUM_MENU_ROWS, /*lbWrap*/ true);
    for (s32 liRow = 0; liRow < KI_NUM_MENU_ROWS; ++liRow)
        mMenuComponent.SetText(liRow, KAPC_MENU_TEXT[liRow]);
}

// ---------------------------------------------------------- HandleTriggers @0x824B8548
// Route the apt trigger to the licence card: type 1 is a load trigger, type 4 a
// transition trigger. Everything else is ignored.
void CrashNavDriverDetails::HandleTriggers(const CgsModule::Event* lpAptTrigger)
{
    CGS_ASSERT(lpAptTrigger != 0,
               "Invalid event in CrashNavDriverDetails::::HandleTriggers");   // cpp:1023

    const CgsGui::GuiEventAptTriggerPayload* lpPayload =
        reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpAptTrigger);

    // The X360 tests the payload's leading word against 1 and 4, which the DWARF's own
    // AptEventType names E_APT_EVENT_ONLOAD and E_APT_EVENT_TRANSITION_COMPLETE.
    if (lpPayload->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        mLicenseComponent.HandleAptLoadTriggers(lpPayload);
    else if (lpPayload->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
        mLicenseComponent.HandleAptTransitionTriggers(lpPayload);
}

// ---------------------------------------- HandleControllerInputPressed @0x824CF3B8
// ⭐ THE RESUME LIVES HERE. Cases 45 (GUI_START) and 50 (GUI_CANCEL) post the exact
// mirror of OnEnter's pause pair and send "GO_BACK", which the BRNSCREENFSM turns into
// Transition_111CN_D_DETAIL_4INGAME.
void CrashNavDriverDetails::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
{
    const s32 liAction = *reinterpret_cast<const s32*>(
        reinterpret_cast<const u8*>(lpEvent) + 4);

    switch (liAction)
    {
    case KI_ACTION_MENU_41:
        // Menu vtable +0x38 == MenuComponent::HighlightPrevious (@0x824E4DE8, which tail-
        // calls SelectableGroup::HighlightPrevious with lbQuiet false). Verified against
        // the vtable at off_82074068, NOT against slot order.
        if (!mPhotoBoothComponent.IsActive())
        {
            mMenuComponent.HighlightPrevious();
            mbRefreshTakePhotoPrompt = true;
            UpdateStatsPanel();
        }
        break;

    case KI_ACTION_MENU_42:
        // Menu vtable +0x34 == HighlightNext (@0x824E4DE0).
        if (!mPhotoBoothComponent.IsActive())
        {
            mMenuComponent.HighlightNext();
            mbRefreshTakePhotoPrompt = true;
            UpdateStatsPanel();
        }
        break;

    case KI_ACTION_START:
    case KI_ACTION_CANCEL:
        if (mPhotoBoothComponent.IsActive())
        {
            // Inside the photo booth these two back OUT of the booth, not out of the
            // screen.
            if (mLicenseComponent.IsFirstResourceLoaded() && mPhotoBoothComponent.Cancel())
            {
                GuiEventPhotoBoothCancelled lCancelled;
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lCancelled), KI_CHANNEL_GUI_OUT,
                    static_cast<s32>(sizeof(lCancelled)));
                mbShowLicense = true;
            }
        }
        else
        {
            // ⭐ THE RESUME, in the console's own order.
            CgsGui::GuiEventNetworkSuspension lResume(false);
            mpStateInterface->OutputGuiEvent(lResume);

            GuiEventScreenClosed lClosed;
            mpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lClosed), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lClosed)));

            GuiEventActivateCrashNav lActivate(true);
            mpStateInterface->OutputGuiEvent(lActivate);

            SendStateEvent("GO_BACK");             // CN_D_DETAIL(111) -> INGAME(4)
        }
        break;

    case KI_ACTION_SELECT:
        if (mPhotoBoothComponent.IsActive()
            && mLicenseComponent.IsFirstResourceLoaded()
            && mPhotoBoothComponent.Select())
        {
            mbShowLicense = true;
        }
        break;

    case KI_ACTION_TAKE_PHOTO:
        if (mMenuComponent.miHighlightedIndex == 0
            && !mPhotoBoothComponent.IsActive()
            && mpGuiCache->GetCamStatus() != 0)
        {
            mbShowLicense = false;
            mPhotoBoothComponent.ShowComponent(false);
        }
        break;

    case KI_ACTION_TOGGLE_L:
        SendStateEvent("TOGGLE_LEFT");
        break;

    case KI_ACTION_TOGGLE_R:
        SendStateEvent("TOGGLE_RIGHT");
        break;

    default:
        break;
    }
}

// ------------------------------------------------------- UpdateStatsPanel @0x824B8B80
// The highlighted menu row picks the stats-panel apt view state, and row 0 ("License")
// is the only one that shows the licence card.
void CrashNavDriverDetails::UpdateStatsPanel()
{
    const s32 liHighlighted = mMenuComponent.miHighlightedIndex;

    const char* lpacViewState = 0;
    if (liHighlighted == 0)
    {
        mbShowLicense = true;
        lpacViewState = KPC_STATS_PANEL_LICENSE;
    }
    else if (liHighlighted == 1)
    {
        mbShowLicense = false;
        lpacViewState = KPC_STATS_PANEL_RECORDS;
    }
    else if (liHighlighted == 2)
    {
        mbShowLicense = false;
        lpacViewState = KPC_STATS_PANEL_DISCOVER;
    }
    else
    {
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint << "Unhandled menu option index " << liHighlighted
                                       << " in CrashNavDriverDetails::UpdateStatsPanel()\n";
        CGS_ASSERT(false, "Unhandled menu option index");   // cpp:1200
        return;
    }

    // FLAG: the apt-view boundary -- AddOutputAptViewState hands the transition to the
    // apt engine (a CgsGui::GuiComponent boundary).
    mStatsPanelAnimator.AddOutputAptViewState("apt_Transition", lpacViewState, false);
}

// ----------------------------------------------------------- HandleStatData @0x824B8618
// Fill all 33 stat fields plus the three 5-district columns from the stats response.
//
// The record is the same opaque 420-byte GuiEventStatsResponse the committed
// BrnCrashNavStats.cpp reads, so the same file-local byte-boundary reader is used; every
// offset below is the X360 `lwz`'s, in the console's own order.
void CrashNavDriverDetails::HandleStatData(const GuiEventStatsResponse* lpStatsEvent)
{
    CGS_ASSERT(lpStatsEvent != 0, "lpStatsEvent");   // cpp:1050
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");       // cpp:1051

    typedef CgsLanguage::LanguageManager LM;
    const LM* lpLanguageManager = mpStateInterface->GetLanguageManager();

    const StatsReader lReader = { reinterpret_cast<const u8*>(lpStatsEvent) };

    char lacBuffer[128];
    lacBuffer[63] = 0;   // the X360's `stb 0, var_41` -- the formatter's own cap is 64

    // --- the plain single-value fields -------------------------------------------
    maStatTextfields[0].SetLocalisedText(static_cast<f32>(lReader.Word(0x20)),
                                         LM::E_FORMAT_HOURS_MINUTES_SECONDS);
    maStatTextfields[1].SetLocalisedText(lReader.Word(0xC4), LM::E_FORMAT_PERCENTAGE);
    maStatTextfields[2].SetLocalisedText(static_cast<f32>(lReader.Word(0x1C)),
                                         LM::E_FORMAT_AUTO_DISTANCE_LONG);

    // --- the "x of y" fields ------------------------------------------------------
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x24), lReader.Word(0x28), 64);
    maStatTextfields[3].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);

    maStatTextfields[4].SetLocalisedText(lReader.Word(0xC0), LM::E_FORMAT_INTEGER);

    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0xB4), lReader.Word(0xB8), 64);
    maStatTextfields[5].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0xAC), lReader.Word(0xB8), 64);
    maStatTextfields[6].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0xB0), lReader.Word(0xB8), 64);
    maStatTextfields[7].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x104), lReader.Word(0x108), 64);
    maStatTextfields[8].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x70), lReader.Word(0x74), 64);
    maStatTextfields[10].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x78), lReader.Word(0x7C), 64);
    maStatTextfields[11].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x68), lReader.Word(0x6C), 64);
    maStatTextfields[12].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);

    // --- the per-event-type "won" counts ------------------------------------------
    maStatTextfields[13].SetLocalisedText(lReader.Word(0xCC), LM::E_FORMAT_INTEGER);
    maStatTextfields[14].SetLocalisedText(lReader.Word(0xD0), LM::E_FORMAT_INTEGER);
    maStatTextfields[15].SetLocalisedText(lReader.Word(0xD4), LM::E_FORMAT_INTEGER);
    maStatTextfields[16].SetLocalisedText(lReader.Word(0xD8), LM::E_FORMAT_INTEGER);
    maStatTextfields[17].SetLocalisedText(lReader.Word(0xDC), LM::E_FORMAT_INTEGER);
    maStatTextfields[18].SetLocalisedText(lReader.Word(0xE0), LM::E_FORMAT_MONEY);

    // --- the "best" records, each a localisation key with one integer parameter ----
    CgsCore::SnPrintf(lacBuffer, 64, "%d", lReader.Word(0x100));
    maStatTextfields[19].SetLocalisedText("CV_PANEL_EVENTS_SCORE", LM::E_FORMAT_ID_LOOKUP,
                                          1, lacBuffer, LM::E_FORMAT_INTEGER);

    maStatTextfields[20].SetLocalisedText(lReader.Word(0x88), LM::E_FORMAT_INTEGER);
    maStatTextfields[21].SetLocalisedText(static_cast<f32>(lReader.Word(0xEC)),
                                          LM::E_FORMAT_AUTO_DISTANCE_LONG);

    CgsCore::SnPrintf(lacBuffer, 64, "%d", lReader.Word(0xE8));
    maStatTextfields[22].SetLocalisedText("STAT_BURNOUTS", LM::E_FORMAT_ID_LOOKUP,
                                          1, lacBuffer, LM::E_FORMAT_INTEGER);

    maStatTextfields[23].SetLocalisedText(static_cast<f32>(lReader.Word(0xF0)),
                                          LM::E_FORMAT_AUTO_DISTANCE_LONG);
    maStatTextfields[24].SetLocalisedText(static_cast<f32>(lReader.Word(0xF4)),
                                          LM::E_FORMAT_SECONDS_HUNDREDTHS_LONG);

    CgsCore::SnPrintf(lacBuffer, 64, "%d", lReader.Word(0xF8));
    maStatTextfields[25].SetLocalisedText("STAT_DEGREES", LM::E_FORMAT_ID_LOOKUP,
                                          1, lacBuffer, LM::E_FORMAT_INTEGER);

    CgsCore::SnPrintf(lacBuffer, 64, "%d", lReader.Word(0xFC));
    maStatTextfields[26].SetLocalisedText("x %1", LM::E_FORMAT_TEXT,
                                          1, lacBuffer, LM::E_FORMAT_INTEGER);

    maStatTextfields[27].SetLocalisedText(lReader.Word(0x2C), LM::E_FORMAT_PERCENTAGE);
    maStatTextfields[28].SetLocalisedText("CV_PANEL_EVENTS_TD_COUNT", LM::E_FORMAT_ID_LOOKUP,
                                          lReader.Word(0xE4), LM::E_FORMAT_INTEGER);

    // --- the three district columns, one district per pass ------------------------
    // The X360 walks a single word cursor at event+0x134 and reads the three "found"
    // counts at +0/+20/+40 and their three totals at +60/+80/+100.
    for (s32 liDistrict = 0; liDistrict < KI_NUM_DISTRICTS; ++liDistrict)
    {
        const u32 luCursor = 0x134u + static_cast<u32>(liDistrict) * 4u;

        lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(luCursor),
                                              lReader.Word(luCursor + 60), 64);
        maDistrictBillboardsTextfields[liDistrict].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);

        lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(luCursor + 20),
                                              lReader.Word(luCursor + 80), 64);
        maDistrictJumpsTextfields[liDistrict].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);

        lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(luCursor + 40),
                                              lReader.Word(luCursor + 100), 64);
        maDistrictSmashesTextfields[liDistrict].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    }

    // --- the four "found" tallies the console formats after the district loop ------
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x130), lReader.Word(0x12C), 64);
    maStatTextfields[9].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x110), lReader.Word(0x120), 64);
    maStatTextfields[30].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x114), lReader.Word(0x124), 64);
    maStatTextfields[31].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x10C), lReader.Word(0x11C), 64);
    maStatTextfields[29].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
    lpLanguageManager->FormatXoverYString(lacBuffer, lReader.Word(0x118), lReader.Word(0x128), 64);
    maStatTextfields[32].SetLocalisedText(lacBuffer, LM::E_FORMAT_TEXT);
}

}
