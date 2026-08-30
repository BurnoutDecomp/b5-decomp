// ===================================================================================
// BrnGui::Intro  -- implementation
//   GameSource/Gui/Flow/Screen/States/BrnIntro.cpp
//
// The first-boot intro presentation state (welcome text -> photo booth -> driver licence
// -> fly-by). Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX; every address,
// constant and assert line number below was read from the image (raw asm for all 13
// functions, plus the .rdata/.data statics at 0x8206670C / 0x82066730.. / 0x82F272E0 /
// 0x82F272F0). See the header for the address table and the member offsets.
//
// LIVE STATE MACHINE (the ARTIST build):
//   OnEnter -> NONE(0)
//   Update case 0  : assert mpGuiCache; StateInterface::StopLoadingScreen();
//                    -> LOADINGRESOURCES(5) and FALL THROUGH into case 5 the same frame
//   case 5         : gate on GuiCache::EnsureResourcesAreLoaded(maResourcesToLoad, 2)
//                    && LicenseComponent::EnsureResourcesAreLoaded()
//                    && PhotoBoothComponent::EnsureResourcesAreLoaded();
//                    then PlayAptMovie("BrnIntro", 3), both components' OnLoad(),
//                    GuiCache::ClearExpectedAptComponentList(SCREEN),
//                    AppendExpectedComponents()      -> WAITINGFORCOMPONENTS(6)
//   case 6         : GuiCache::AreAllAptComponentsInitialised(SCREEN)
//                                                    -> WELCOMETEXT(7) + SetupComponents()
//   7 -> 8         : HandleStateTransitions, a 2 s dwell (held while a voice-over plays)
//   8 -> 9         : a CONTROLLER PRESS (49 confirm / 50 back), not a timer
//   9 -> 3         : a 2 s dwell AND a signed-in user
//   case 3         : mfFlybyTimer = 0; post command 477 (fly-by START) -> WAIT(4)
//   case 4         : mfFlybyTimer += dt; at 7.6666665 s post command 478 (fly-by END),
//                    voice-over "Intro_Show_Car", then
//                    SendStateEvent(mbIsNewProfile ? "ADVANCE" : "GO_BACK")
//   every frame    : both components' SendPlayerPictureEvent()
//
// DEAD-BUT-REPRODUCED vestiges of the removed intro VIDEO (both verified image-wide):
//   * states PLAYING_VIDEO(1) / WAITING_FOR_VIDEO(2): Update's jump table @0x824DF0F0
//     routes 1 and 2 to the "Unhandled IntroState" assert, and nothing in the image ever
//     stores 1 into meIntroState -- so HandleControllerInput's case-1 "press A to skip"
//     branch (which is the only writer of 2) is unreachable. Kept as the console has it.
//   * event 510 ("the video that was playing has finished", posted by
//     BrnGui::GuiModule::Update @0x8252A208): it IS in maiEventToObserve and it IS
//     genuinely dropped -- 510 misses every case in HandleIncomingEvents' >350 switch and
//     lands on the "Unhandled event" assert (cpp:479). Retail strips the assert, so it is
//     a silent no-op there. Reproduced rather than "fixed".
//
// COMPONENT BOUNDARY: CLOSED (2026-07-30). BrnPhotoBoothComponent.cpp AND
// BrnLicenseComponent.cpp are both reconstructed and mounted, so every component call below is
// the real component. The FLAG'd License* / Profile* leaves this file used to carry are DELETED
// -- BrnProfile.cpp (GetLicenceIssuedDate / SetLicenceIssuedDateAsNow) is mounted too.
// What remains in the anonymous namespace is only the two un-named GuiCache far members.
//
// ⚠ 8 -> 9 IS A CONTROLLER PRESS. The PC input bridge used to deliver the accept as action
// 45 rather than the console's 49 GUI_SELECT, which parked this state in PHOTOBOOTH forever
// (boot-measured). That is fixed at the source now -- CgsInputPadsPC's KA_BINDINGS -- and the
// compensating 45 arm this file carried is deleted; see the note at KI_ACTION_CONFIRM below.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnIntro.h"
#include "GameSource/Input/GameInputActions.h"                       // EGameInputActions (the controller action vocabulary)

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                  // CgsDev::StrStream (the streamed unhandled-event assert)
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h" // CgsDev::Assert::Begin/Fire/EndAssert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                       // CgsCore::SnPrintf
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"              // the state in-queue
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"      // StateInterface / OutputGuiEvent
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h"  // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"        // CgsSystem::DateAndTime
#include "GameSource/GameState/Progression/BrnProfile.h"                      // BrnProgression::Profile
#include "GameSource/Gui/BrnGuiCache.h"                                       // BrnGui::GuiCache (+ GetCamStatus)
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"               // ButtonIconComponent::EPadButton (the photo-booth Construct args)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                               // BrnGui::GuiFlow
#include "GameSource/Gui/BrnGuiMovieManager.h"                                // MovieManager::VideoDefinition
#include "GameSource/Gui/BrnGuiVideoEvents.h"                                 // BrnGui::GuiEventStopVideo
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::WriteToLog (the state-transition diagnostic)

#include <cstring>   // std::strcmp (the X360 inlines the apt-trigger name compares)
#include <cstdio>    // std::snprintf (the state-transition diagnostic)
#include <cstddef>   // offsetof (the x64 payload-offset word of the voice-over record)

namespace BrnGui
{
    // ================================================================================
    // statics -- every value read from BURNOUT_X360_ARTIST.XEX
    // ================================================================================

    // @0x82F272E0 (.data). The two GUI resources the intro streams. The ids index the
    // 237-entry GuiCache resource-name table (BrnGuiCache.cpp kapcGuiResourceNames):
    //   147 == "BrnIntro"   59 == "B5HelpItem".
    // These are NOT handed to the generic loader: GetResourcesToLoad is the ICF fold that
    // returns {NULL, 0}. Update case 5 drives this array through the cache itself.
    const CgsGui::sResourceTuple Intro::maResourcesToLoad[2] =
    {
        { 147u, CgsGui::E_GUI_RESOURCETYPE_APT },   // "BrnIntro"
        {  59u, CgsGui::E_GUI_RESOURCETYPE_APT },   // "B5HelpItem"
    };
    const u32 Intro::muNumResourcesToLoad = 2;

    // @0x8206670C (.rdata) -- the 8 observed event ids, in table order. 510 is subscribed
    // and deliberately unhandled (see the file note).
    const s32 Intro::maiEventToObserve[8] =
    {
        6,      // controller input           -> HandleControllerInput (vtable slot 9)
        21,     // apt trigger                -> the text fields + LicenseComponent
        64,     // GuiCache pointer delivery
        350,    // BrnProgression::Profile pointer delivery
        569,    // compressed still image     -> PhotoBoothComponent
        510,    // video finished             -> UNHANDLED (removed intro-video vestige)
        466,    // voice-over started
        467,    // voice-over finished
    };
    const s32 Intro::miNumEventsObserved = 8;

    // apt component names @0x82066730.. (.rdata); the array widths are the DWARF's.
    const char Intro::KAC_SCREENANIM_NAME[15]        = "ScreenAnim_cpt";
    const char Intro::KAC_LICENSECOMP_NAME[12]       = "License_cpt";
    const char Intro::KAC_PHOTOBOOTHCOMP_NAME[15]    = "PhotoBooth_cpt";
    const char Intro::KAC_BUTTONPROMPTCOMP_NAME[10]  = "Prompt_mc";
    const char Intro::KAC_WELCOMETEXT1COMP_NAME[18]  = "WelcomeText1_anim";
    const char Intro::KAC_WELCOMETEXT2COMP_NAME[18]  = "WelcomeText2_anim";
    const char Intro::KAC_MONTHTEXTFIELD_NAME[14]    = "MonthText_cpt";
    const char Intro::KAC_DAYTEXTFIELD_NAME[12]      = "DayText_cpt";
    const char Intro::KAC_YEARTEXTFIELD_NAME[13]     = "YearText_cpt";

    // @0x82066790 / @0x820667AC. Present in .rdata and DWARF-declared, but NO recovered
    // Intro function references either: in this build the photo-booth prompt strings are
    // chosen inside PhotoBoothComponent (GetTakePhotoStringID and its own KA*_STRINGID
    // tables). Kept because they are this state's own attested statics.
    const char Intro::KAC_GOTOPHOTOBOOTH_STRINGID[25] = "$CAPS_BUTTON_PHOTO_BOOTH";
    const char Intro::KAC_CONTINUE_STRINGID[25]       = "$GENERAL_OPTION_CONTINUE";

    // @0x82F272F0 (.data) -- the "apt_Transition" frame label SetupComponents pushes at
    // mScreenAnim, indexed by meIntroState. Slots 0..6 are the empty string @0x820046A7.
    const char* const Intro::KAPC_SCREEN_FRAMENAMES[Intro::E_INTROSTATE_COUNT] =
    {
        "", "", "", "", "", "", "",
        "WelcomeText1",   // [7] WELCOMETEXT
        "Photobooth",     // [8] PHOTOBOOTH
        "WelcomeText2",   // [9] LICENCE
    };

    // DWARF BrnIntro.cpp:90/91.
    const f32 KF_INTRO_FLYBY_DURATION   = 7.6666665f;   // flt_820667F4 (0x40F55555)
    const f32 KF_INTRO_TRANSITION_PAUSE = 2.0f;         // @0x820667F8

    namespace
    {
        // ---- observed / posted event ids ------------------------------------------
        const s32 KI_EVENT_CONTROLLER        = 6;
        const s32 KI_EVENT_APT_TRIGGER       = 21;
        const s32 KI_EVENT_GUI_CACHE         = 64;
        const s32 KI_EVENT_PROFILE           = 350;
        const s32 KI_EVENT_VOICEOVER_STARTED = 466;
        const s32 KI_EVENT_VOICEOVER_STOPPED = 467;
        const s32 KI_EVENT_CAM_PIC           = 569;

        // Controller action sub-ids (the action event's payload word @+4). Roles from
        // Intro's own switch: 49 confirms, 50 backs out. Same producer vocabulary the
        // committed BrnPauseScreen / BrnInGame slices use.
        const s32 KI_ACTION_CONFIRM = 49;
        const s32 KI_ACTION_BACK    = 50;
        // + ROOT CAUSE FIXED ELSEWHERE, COMPENSATING ARM DELETED (input-vocabulary wave,
        // 2026-08-29). The local `KI_ACTION_CONFIRM_PC` (45) used to sit here because
        // KA_BINDINGS bound the accept key to 45 GUI_START rather than 49 GUI_SELECT. Both
        // console bodies were checked and NEITHER handles 45:
        //   Intro::HandleControllerInput            @0x824D1988 -- state 1 tests `== 49` only
        //   Intro::HandleControllerPressedPhotoBooth @0x824D1B98 -- tests 49 then 50 only
        // KA_BINDINGS now puts Enter/Space/pad-A on 49, so PHOTOBOOTH advances on the
        // console's own id and 50 GUI_CANCEL (Escape / pad B) reaches PhotoBoothComponent::
        // Cancel for the first time on PC.

        const s32 KI_CHANNEL_GUI_OUT      = 40;  // GuiEventOut
        const s32 KI_CHANNEL_GUI_INTERNAL = 42;  // internal/HUD-component channel

        // The two fly-by commands the intro hands the director through
        // BrnGame::BrnGameModule::BridgeGuiToDirector (@0x823CBF70 on X360; case 477 sets
        // DirectorIO::InputBuffer+31422, case 478 sets +31423).
        const s32 KI_COMMAND_FLYBY_START = 477;
        const s32 KI_COMMAND_FLYBY_END   = 478;

        // DWARF BrnIntro.cpp:443/444 -- the month string-id table bound the asserts use.
        const s32 KI_NUM_MONTH_STRINGIDS = 12;

        // @0x82F27E8C (.data) -- the localisation ids for the licence-issued month.
        const char* const KAPC_MONTH_STRINGIDS[KI_NUM_MONTH_STRINGIDS] =
        {
            "MONTH_SHORT_01", "MONTH_SHORT_02", "MONTH_SHORT_03", "MONTH_SHORT_04",
            "MONTH_SHORT_05", "MONTH_SHORT_06", "MONTH_SHORT_07", "MONTH_SHORT_08",
            "MONTH_SHORT_09", "MONTH_SHORT_10", "MONTH_SHORT_11", "MONTH_SHORT_12",
        };

        // The LanguageManager::ParameterFormatType SetLocalisedText is called with (the
        // X360 passes the literal 9 for the month id).
        const CgsLanguage::LanguageManager::ParameterFormatType KE_MONTH_TEXT_FORMAT =
            static_cast<CgsLanguage::LanguageManager::ParameterFormatType>(9);

        // The day / year fields are filled through a 31-char scratch buffer the X360 always
        // NUL-terminates at index 31 after SnPrintf.
        const s32 KI_DATE_SCRATCH_LEN = 31;

        // The state's in-event queue (State +0x18) is the DWARF InputBuffer::GuiEventQueue,
        // an incomplete alias for this concrete instantiation the X360 walks by name.
        typedef CgsModule::VariableEventQueue<18432, 16> InGuiEventQueue;

        // ---- in-queue payload views (the queue delivers the header-stripped payload;
        //      the committed consumer idiom -- see BrnInGame.cpp) ---------------------
        struct GuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };

        struct GuiEventProfilePointer : public CgsModule::Event
        {
            BrnProgression::Profile* mpProfile;
        };

        // The controller action event: the action sub-id is the payload word @+4.
        struct GuiEventControllerAction : public CgsModule::Event
        {
            s32 miPad0;
            s32 miAction;
        };

        // ---- out-queue wire record: the 16-byte GuiEvent<N> command {1, N, 12} + flag,
        //      byte-identical to the committed BrnInGame.cpp helper. --------------------
        template <s32 N>
        struct GuiCommandEvent16 : public CgsGui::GuiEvent<N>
        {
            u8 mu8Flag;
            u8 maPad[3];
            GuiCommandEvent16(u8 lu8Flag = 0) : CgsGui::GuiEvent<N>(1, 12), mu8Flag(lu8Flag)
            { maPad[0] = maPad[1] = maPad[2] = 0; }
        };

        template <s32 N>
        void PostCommand16(CgsGui::StateInterface* lpInterface, s32 liChannel, u8 lu8Flag = 0)
        {
            GuiCommandEvent16<N> lEvent(lu8Flag);
            lpInterface->GetOutputEventQueue()->AddEvent(&lEvent, liChannel, sizeof(lEvent));
        }

        // ---- voice-over ------------------------------------------------------------
        // The X360 posts {4, 466, 12, <name hash>} on channel 40, which is exactly
        // GuiEventWrapper<BrnGui::GuiEventAudioVoiceOver, 40> (payload size 4 / type 466 /
        // payload offset 12, then the 4-byte hash) -- i.e.
        // OutputGuiEvent<GuiEventAudioVoiceOver>, whose explicit instantiation @0x824C3178
        // is already committed.
        //
        // The six ids are CgsSound::Playback::Name::MakeHash results computed by static
        // initialisers (X360 sub_82C54E20 / ..E50 / ..E80 / ..EB0 / ..EE0 / ..F10 ->
        // dword_82FB4A80 / ..4A38 / ..4A84 / ..4A3C / ..4A8C / ..4C18).
        //
        // ⚠ CORRECTION (2026-07-30): the previous revision of this comment said
        // "CgsSound::Playback::Name::MakeHash is unreconstructed, so the PC build posts id 0".
        // THAT WAS STALE -- MakeHash is fully bodied (GameShared/GameClasses/Sound/Playback/
        // CgsCommon.cpp:148, faithful to X360 @0x82689A50) and is already used at runtime by
        // BrnGuiModule's menu-music table. The hash is therefore computed for real here, which
        // is what the console posts.
        u32 VoiceOverNameHash(const char* lpacVoiceOverName)
        {
            return static_cast<u32>(CgsSound::Playback::Name::MakeHash(lpacVoiceOverName));
        }

        // The wire record for BrnGui::GuiEventAudioVoiceOver (real home
        // BrnGuiDemangledEventTypes.h, "id 466 size 4"; that header cannot be included here --
        // it redefines BrnGui::GuiEventRunFsm and BrnGui::GuiAudioTriggerEvent, which this TU
        // already has from their real home BrnGuiEventTypeDefs.h).
        //
        // ⚠ FIXED 2026-07-30: this used to be posted through the generic
        // StateInterface::OutputGuiEvent<T>, whose committed body queues the record under
        // lrEvent.GetEventType() -- i.e. under id 466 -- and GuiModule::Update's out-queue
        // switch has no arm for that id, so the request never reached
        // BrnGameModule::BridgeGuiToGame at all. The X360's OutputGuiEvent<T> builds a
        // GuiEventWrapper<T, 40> and posts it on CHANNEL 40 (@0x824C3178 --
        // "{4, 466, 12, <name hash>}, channel 40, 16 bytes"). The local payload tag below
        // is boxed through that canonical wrapper, preserving the original structure.
        struct GuiEventAudioVoiceOverPayload : public CgsModule::Event
        {
            u32 muNameHash;

            explicit GuiEventAudioVoiceOverPayload(u32 luNameHash) : muNameHash(luNameHash) {}
            s32 GetEventType() const { return KI_EVENT_VOICEOVER_STARTED; }
        };

        const char* const KAC_VO_INTRO_NEED_PICTURE   = "Intro_Need_Picture";    // -> 0x82FB4A80
        const char* const KAC_VO_INTRO_NO_CAM         = "Intro_No_Cam";          // -> 0x82FB4A38
        const char* const KAC_VO_INTRO_TAKE_PHOTO     = "Intro_Take_Photo";      // -> 0x82FB4A84
        const char* const KAC_VO_INTRO_LEARNER_PERMIT = "Intro_Learner_Permit";  // -> 0x82FB4A3C
        const char* const KAC_VO_INTRO_GO_TO_JUNKYARD = "Intro_Go_To_Junkyard";  // -> 0x82FB4A8C
        const char* const KAC_VO_INTRO_SHOW_CAR       = "Intro_Show_Car";        // -> 0x82FB4C18

        void PostVoiceOver(CgsGui::StateInterface* lpInterface, const char* lpacVoiceOverName)
        {
            // The hash word is stored NATIVELY (the console's big-endian bytes are that
            // platform's native order; the consumer reads a u32, not a byte string).
            GuiEventAudioVoiceOverPayload lEvent(VoiceOverNameHash(lpacVoiceOverName));
            CgsGui::GuiEventWrapper<GuiEventAudioVoiceOverPayload, KI_CHANNEL_GUI_OUT>
                lWrapper(lEvent);
            lpInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrapper), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWrapper)));
        }

        // ================================================================================
        // GuiCache far-member boundaries. All three live inside the committed
        // BrnGuiCache.h padding holes (mPad_4B34[0xC] and mPad_13B58[60]), so they are
        // reached through named FLAG'd leaves here -- the same discipline BrnInGame.cpp
        // uses -- until the GuiCache TU grows the members.
        // ================================================================================

        // GuiCache +0x4B38 is the signed-in Xbox `dwUserIndex`: 24 image-wide readers, of
        // which 9 feed it straight into XUser*/XShow* as their dwUserIndex argument
        // (OnlinePlay::ShowFriendsMenu, OnlineQuickCustomCreate::Update,
        // CrashNavEnterOnlineX360::Update, CrashNavSettings::HandleControllerInput);
        // GuiCache::Construct and GuiCache::RecEvent are the writers. Intro compares it
        // against -1, i.e. "is anybody signed in".
        // FLAG PC-platform leaf: PC has no Xbox sign-in; the local player is always present.
        bool CacheIsUserSignedIn(const GuiCache* lpCache) { return lpCache != 0; }

    }

    // Intro-state transition log -- the [BootLegal] / [BootProfile] / [FBurnMainHud]-style
    // diagnostic every boot/flow state in this tree carries. PC-side only; the X360 has no
    // equivalent (its retail build strips all of these).
    static void LogIntroState(s32 liFrom, s32 liTo, const char* lpacWhere)
    {
        char lac[96];
        std::snprintf(lac, sizeof(lac), "[Intro] state %d -> %d (%s)\n", liFrom, liTo, lpacWhere);
        CgsDev::Log::WriteToLog(lac);
    }

    // The voice-over gate diagnostic: HandleStateTransitions holds both timed transitions
    // while a voice-over is playing, so a stuck mbVoiceOverPlaying stalls the whole sequence.
    static void LogIntroVoiceOver(bool lbPlaying, s32 liState)
    {
        char lac[96];
        std::snprintf(lac, sizeof(lac), "[Intro] voice-over %s (state %d)\n",
                      lbPlaying ? "STARTED" : "FINISHED", liState);
        CgsDev::Log::WriteToLog(lac);
    }

    // ================================================================================
    // @ 0x824FFF58 -- the compiler-emitted constructor. The X360 writes the state's own
    // vtable (+0x000, off_820740E0) plus ~20 embedded sub-object vtable pointers; with the
    // sub-objects now modelled as real members, constructing the base and the members
    // reproduces exactly that.
    // ================================================================================
    Intro::Intro()
        : CgsGui::State()
        , meIntroState(E_INTROSTATE_NONE)
        , mpProfile(0)
        , mpGuiCache(0)
        , mfFlybyTimer(0.0f)
        , mfPauseTimer(0.0f)
        , mbVoiceOverPlaying(false)
    {
    }

    // ================================================================================
    // @ 0x824B8FE8
    // ================================================================================
    void Intro::Construct(CgsID lId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");   // cpp:111

        CgsGui::State::Construct(lId, lpFsm);

        meIntroState = E_INTROSTATE_NONE;   // +0x38
        mpGuiCache   = 0;                   // +0x40
    }

    // ================================================================================
    // @ 0x824D1488
    // ================================================================================
    void Intro::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The internal-channel command 148 the state announces itself with (the record
        // {1, 148, 12} plus one zero byte, channel 42, 16 bytes).
        PostCommand16<148>(mpStateInterface, KI_CHANNEL_GUI_INTERNAL, 0);

        mScreenAnim.Construct(KAC_SCREENANIM_NAME, mpStateInterface, 0);
        // X360: LicenseComponent::Construct @0x8241A610 (its own virtual override of
        // CgsGui::GuiComponent::Construct -- it seeds the six embedded TextFields).
        mLicenseComponent.Construct(KAC_LICENSECOMP_NAME, mpStateInterface, 0);

        // X360: PhotoBoothComponent::Construct(this+0x89C, "PhotoBooth_cpt",
        //   mpStateInterface, 5, 4, 2, 3, 0) -- back button 5, confirm button 4,
        //   take-photo string E_TAKEPHOTOSTRING_CONTINUE(2), back string E_BACKSTRING_CANCEL(3),
        //   no parent name. The two button ids are ButtonIconComponent::EPadButton values.
        //   back button 5, confirm button 4, take-photo string E_TAKEPHOTOSTRING_CONTINUE(2),
        //   back string E_BACKSTRING_CANCEL(3), no parent name.
        mPhotoBoothComponent.Construct(KAC_PHOTOBOOTHCOMP_NAME, mpStateInterface,
                                       ButtonIconComponent::E_PADBUTTON_BACK,
                                       ButtonIconComponent::E_PADBUTTON_SELECT,
                                       PhotoBoothComponent::E_TAKEPHOTOSTRING_CONTINUE,
                                       PhotoBoothComponent::E_BACKSTRING_CANCEL,
                                       0);

        mWelcomeText1Anim.Construct(KAC_WELCOMETEXT1COMP_NAME, mpStateInterface, 0);
        mWelcomeText2Anim.Construct(KAC_WELCOMETEXT2COMP_NAME, mpStateInterface, 0);
        mMonthTextField.Construct(KAC_MONTHTEXTFIELD_NAME, mpStateInterface, 0);
        mDayTextField.Construct(KAC_DAYTEXTFIELD_NAME, mpStateInterface, 0);
        mYearTextField.Construct(KAC_YEARTEXTFIELD_NAME, mpStateInterface, 0);

        mfPauseTimer       = 0.0f;                 // +0xDEC
        meIntroState       = E_INTROSTATE_NONE;    // +0x38
        mpGuiCache         = 0;                    // +0x40
        mpProfile          = 0;                    // +0x3C
        mbVoiceOverPlaying = false;                // +0xDF0
    }

    // ================================================================================
    // @ 0x824D1640
    // ================================================================================
    void Intro::OnLeave()
    {
        // X360: `stbx 1, mpGuiCache, 0x13B5E` (unconditional, cache is live whenever this
        // state can be left). Arms the one-shot GuiCache gate the junkyard car-select pair
        // consumes: CarSelectVehicle::SetupComponents skips its "transin" transition and
        // CarSelectLivery::Update auto-accepts the selection on its first interactive frame,
        // which is why the intro's junkyard visit never shows the colour-select screen.
        // CarSelectLivery::OnLeave clears it again. (This used to be a no-op leaf while the
        // byte's role was unsettled -- root-caused 2026-08-24 when the colour-select screen
        // appeared in the intro flow.)
        mpGuiCache->SetCarSelectTransitionAlreadyShown(true);

        // X360: the inlined GuiEventPlayAptMovie record {8, 18, 12, "", 3} on channel 41,
        // 20 bytes -- unmount the intro apt movie at level 3.
        mpStateInterface->PlayAptMovie("", 3);

        mLicenseComponent.ReleaseResources();
        mPhotoBoothComponent.ReleaseResources();

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        meIntroState = E_INTROSTATE_NONE;   // +0x38
        mpGuiCache   = 0;                   // +0x40

        // Clear the profile's "never played" flag so the intro runs exactly once.
        if (mpProfile != 0)
        {
            mpProfile->SetIsNewProfile(false);   // stb 0, mpProfile+118033
            mpProfile = 0;
        }
    }

    // ================================================================================
    // vtable slot 8 -- the ICF fold @0x825151A8. Writes {NULL, 0} into both out params.
    // ================================================================================
    void Intro::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                   u32* lpuNumberOfResources) const
    {
        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ================================================================================
    // @ 0x824B90A0 -- register the seven apt components the intro screen waits on. The
    // licence component registers its own four
    // (LicenseComponent::AppendExpectedAptComponent @0x8241A790), which is why it is
    // absent from this list. The X360 passes each component's macName (component+0x04).
    // ================================================================================
    void Intro::AppendExpectedComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:595

        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mScreenAnim.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mPhotoBoothComponent.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mWelcomeText1Anim.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mWelcomeText2Anim.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mMonthTextField.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDayTextField.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mYearTextField.GetName());
    }

    // ================================================================================
    // @ 0x824D1718 -- push the presentation for the state we have just entered.
    // ================================================================================
    void Intro::SetupComponents()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:617

        switch (meIntroState)
        {
        case E_INTROSTATE_NONE:
        case E_INTROSTATE_START_FLYBY:
        case E_INTROSTATE_WAIT_FOR_FLYBY_FINISH:
        case E_INTROSTATE_LOADINGRESOURCES:
        case E_INTROSTATE_WAITINGFORCOMPONENTS:
            break;

        case E_INTROSTATE_WELCOMETEXT:
            PostVoiceOver(mpStateInterface, KAC_VO_INTRO_NEED_PICTURE);
            mLicenseComponent.SetVisible(false);
            mPhotoBoothComponent.HideComponent(true);
            mWelcomeText1Anim.AddOutputAptViewState("apt_Transition", "transin", false);
            mScreenAnim.AddOutputAptViewState("apt_Transition",
                                              KAPC_SCREEN_FRAMENAMES[meIntroState], false);
            break;

        case E_INTROSTATE_PHOTOBOOTH:
            mWelcomeText1Anim.AddOutputAptViewState("apt_Transition", "transout", false);
            mPhotoBoothComponent.ShowComponent(false);
            mScreenAnim.AddOutputAptViewState("apt_Transition",
                                              KAPC_SCREEN_FRAMENAMES[meIntroState], false);
            break;

        case E_INTROSTATE_LICENCE:
            PostVoiceOver(mpStateInterface, KAC_VO_INTRO_LEARNER_PERMIT);
            mLicenseComponent.ShowLicense(false);
            mWelcomeText2Anim.AddOutputAptViewState("apt_Transition", "transin", false);
            mScreenAnim.AddOutputAptViewState("apt_Transition",
                                              KAPC_SCREEN_FRAMENAMES[meIntroState], false);
            break;

        default:   // PLAYING_VIDEO(1) / WAITING_FOR_VIDEO(2) land here too -- see the file note.
            CGS_ASSERT(false, "Unhandled introstate in Intro::SetupComponents()");   // cpp:685
            break;
        }
    }

    // ================================================================================
    // @ 0x824D1B00 -- WELCOMETEXT -> PHOTOBOOTH. The voice-over depends on whether a
    // camera is attached ("Intro_Take_Photo" vs "Intro_No_Cam").
    // ================================================================================
    void Intro::HandleTransitionFromWelcomeText()
    {
        LogIntroState(E_INTROSTATE_WELCOMETEXT, E_INTROSTATE_PHOTOBOOTH, "HandleTransitionFromWelcomeText");
        meIntroState = E_INTROSTATE_PHOTOBOOTH;
        SetupComponents();

        PostVoiceOver(mpStateInterface,
                      mpGuiCache->GetCamStatus() != 0 ? KAC_VO_INTRO_TAKE_PHOTO
                                                      : KAC_VO_INTRO_NO_CAM);
    }

    // ================================================================================
    // @ 0x824D1C18 -- LICENCE -> START_FLYBY. Unmounts the intro apt movie, releases both
    // components' resources and starts the "go to the junkyard" voice-over.
    // ================================================================================
    void Intro::HandleTransitionFromLicense()
    {
        CGS_ASSERT(mpProfile != 0, "mpProfile");   // cpp:860

        // X360: the inlined GuiEventPlayAptMovie record {8, 18, 12, "", 3} on channel 41.
        mpStateInterface->PlayAptMovie("", 3);

        LogIntroState(E_INTROSTATE_LICENCE, E_INTROSTATE_START_FLYBY, "HandleTransitionFromLicense");
        meIntroState = E_INTROSTATE_START_FLYBY;

        mLicenseComponent.ReleaseResources();
        mPhotoBoothComponent.ReleaseResources();

        PostVoiceOver(mpStateInterface, KAC_VO_INTRO_GO_TO_JUNKYARD);
    }

    // ================================================================================
    // @ 0x824DAA48 -- the two timed transitions. Both are held while a voice-over plays;
    // the licence one additionally waits for a signed-in user.
    // ================================================================================
    void Intro::HandleStateTransitions()
    {
        if (meIntroState == E_INTROSTATE_WELCOMETEXT)
        {
            if (!mbVoiceOverPlaying)
            {
                mfPauseTimer += mpGuiCache->GetTimeStep();
                if (mfPauseTimer >= KF_INTRO_TRANSITION_PAUSE)
                {
                    HandleTransitionFromWelcomeText();
                }
            }
        }
        else if (meIntroState == E_INTROSTATE_LICENCE && !mbVoiceOverPlaying)
        {
            mfPauseTimer += mpGuiCache->GetTimeStep();
            if (mfPauseTimer >= KF_INTRO_TRANSITION_PAUSE && CacheIsUserSignedIn(mpGuiCache))
            {
                HandleTransitionFromLicense();
            }
        }
    }

    // ================================================================================
    // @ 0x824C1F68 -- drain the state in-queue.
    // ================================================================================
    void Intro::HandleIncomingEvents()
    {
        InGuiEventQueue* lpInQueue = reinterpret_cast<InGuiEventQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liEventType = lpInQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case KI_EVENT_CONTROLLER:
                // X360: virtual dispatch through vtable slot 9.
                HandleControllerInput(lpEvent, KI_EVENT_CONTROLLER);
                break;

            case KI_EVENT_APT_TRIGGER:
            {
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);

                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
                {
                    // A reloaded apt clip has dropped the text it was showing: re-push each
                    // date field's own cached string (the X360 compares the trigger's
                    // component name against each field's macName, then calls
                    // TextField::SetText(field, field+0xA4) -- i.e. its own macText).
                    if (std::strcmp(lpTrigger->mpacComponentName, mMonthTextField.GetName()) == 0)
                    {
                        mMonthTextField.SetText(mMonthTextField.GetText());
                    }
                    else if (std::strcmp(lpTrigger->mpacComponentName, mDayTextField.GetName()) == 0)
                    {
                        mDayTextField.SetText(mDayTextField.GetText());
                    }
                    else if (std::strcmp(lpTrigger->mpacComponentName, mYearTextField.GetName()) == 0)
                    {
                        mYearTextField.SetText(mYearTextField.GetText());
                    }
                    else
                    {
                        // No field matched: the X360 skips straight past the load-trigger
                        // forward (LABEL_27 jumps over it only on the name-mismatch chain's
                        // fallthrough), but the licence component is still offered the
                        // trigger on every matched and unmatched name alike.
                    }
                    mLicenseComponent.HandleAptLoadTriggers(lpTrigger);
                }
                else if (lpTrigger->meEventType ==
                         CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
                {
                    mLicenseComponent.HandleAptTransitionTriggers(lpTrigger);
                }
                break;
            }

            case KI_EVENT_GUI_CACHE:
                if (mpGuiCache == 0)
                {
                    mpGuiCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    mLicenseComponent.SetCachePointer(mpGuiCache);
                    // X360: SetPlayerInfo(name, false, false, 0, 0, false, false) -- a
                    // brand-new profile's licence: rank 0, no points line, no upgrade
                    // pending. The name is a language-database string id (GetPlayerName
                    // returns "PLAYER_NAME_STRING_ID", not the gamertag text).
                    mLicenseComponent.SetPlayerInfo(mpGuiCache->GetPlayerName(),
                                                    false, false, 0, 0, false, false);
                    // X360: an INLINED `stw 92, photoBooth+0x94` -- the component's photo
                    // resource id. 92 == E_GUI_RESOURCEID_APT_COMPONENT_PHOTOBOOTH_DMV
                    // ("B5PhotoBoothComponentDMV"), i.e. the DMV_FULL_PAGE visual style; the
                    // setter is PhotoBoothComponent::SetVisualStyle (DWARF h:296), a header
                    // inline the compiler folded into this store. The three sibling call
                    // sites in the image store 91 / 93 / (93|91) from the same slot, one per
                    // EPhotoBoothStyle, which is what identifies it.
                    mPhotoBoothComponent.SetVisualStyle(
                        PhotoBoothComponent::E_PHOTOBOOTH_STYLE_DMV_FULL_PAGE);
                    mPhotoBoothComponent.SetCachePointer(mpGuiCache);
                }
                break;

            case KI_EVENT_PROFILE:
                if (mpProfile == 0)
                {
                    mpProfile = reinterpret_cast<const GuiEventProfilePointer*>(lpEvent)->mpProfile;
                    mpProfile->SetLicenceIssuedDateAsNow();
                    mLicenseComponent.SetProfilePointer(mpProfile);
                    mPhotoBoothComponent.SetProfilePointer(mpProfile);

                    const CgsSystem::DateAndTime lLicenceDate = mpProfile->GetLicenceIssuedDate();

                    const s32 liMonth = lLicenceDate.GetMonth() - 1;
                    CGS_ASSERT(liMonth >= 0, "liMonth >= 0");                                  // cpp:443
                    CGS_ASSERT(liMonth < KI_NUM_MONTH_STRINGIDS, "liMonth < KI_NUM_MONTH_STRINGIDS"); // cpp:444
                    mMonthTextField.SetLocalisedText(KAPC_MONTH_STRINGIDS[liMonth], KE_MONTH_TEXT_FORMAT);

                    char lacScratch[KI_DATE_SCRATCH_LEN + 1];
                    CgsCore::SnPrintf(lacScratch, KI_DATE_SCRATCH_LEN, "%d", lLicenceDate.GetDay());
                    lacScratch[KI_DATE_SCRATCH_LEN] = 0;
                    mDayTextField.SetText(lacScratch);

                    CgsCore::SnPrintf(lacScratch, KI_DATE_SCRATCH_LEN, "%d", lLicenceDate.GetYear());
                    lacScratch[KI_DATE_SCRATCH_LEN] = 0;
                    mYearTextField.SetText(lacScratch);
                }
                break;

            case KI_EVENT_VOICEOVER_STARTED:
                LogIntroVoiceOver(true, meIntroState);
                mbVoiceOverPlaying = true;
                if (meIntroState == E_INTROSTATE_PHOTOBOOTH)
                {
                    mPhotoBoothComponent.SetButtonPromptVisible(false);
                }
                break;

            case KI_EVENT_VOICEOVER_STOPPED:
                LogIntroVoiceOver(false, meIntroState);
                mfPauseTimer       = 0.0f;
                mbVoiceOverPlaying = false;
                if (meIntroState == E_INTROSTATE_PHOTOBOOTH)
                {
                    mPhotoBoothComponent.SetButtonPromptVisible(true);
                }
                break;

            case KI_EVENT_CAM_PIC:
                mPhotoBoothComponent.HandleCompressedStillImageEvent(lpEvent);
                break;

            default:
            {
                // Event 510 ("the video finished") lands HERE, deliberately: it is
                // registered in maiEventToObserve and has no case anywhere in the X360
                // function. Retail compiles the assert out.
                //
                // The X360 message is STREAMED, not a fixed literal (LABEL_42 of
                // HandleIncomingEvents @0x824C1F68): "Unhandled event " << liEventType <<
                // " in Intro::Update()\n". Reproduced -- an unhandled id is useless
                // without the id, and this is the assert a boot actually trips.
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unhandled event " << liEventType << " in Intro::Update()\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);   // cpp:479
                CgsDev::Assert::EndAssert();
                break;
            }
            }

            liEventType = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
        }
    }

    // ================================================================================
    // @ 0x824D1988 -- vtable slot 9.
    // ================================================================================
    void Intro::HandleControllerInput(const CgsModule::Event* lpEvent, s32 leEventType)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");   // cpp:704

        if (leEventType != KI_EVENT_CONTROLLER)
            return;

        switch (meIntroState)
        {
        case E_INTROSTATE_NONE:
        case E_INTROSTATE_WAITING_FOR_VIDEO:
        case E_INTROSTATE_START_FLYBY:
        case E_INTROSTATE_WAIT_FOR_FLYBY_FINISH:
        case E_INTROSTATE_LOADINGRESOURCES:
        case E_INTROSTATE_WAITINGFORCOMPONENTS:
        case E_INTROSTATE_WELCOMETEXT:
        case E_INTROSTATE_LICENCE:
            break;

        case E_INTROSTATE_PLAYING_VIDEO:
        {
            // UNREACHABLE in this build (nothing ever sets PLAYING_VIDEO) -- the removed
            // "press A to skip the intro video" path, kept because the console kept it.
            const s32 liVideoAction =
                reinterpret_cast<const GuiEventControllerAction*>(lpEvent)->miAction;
            if (liVideoAction == KI_ACTION_CONFIRM)
            {
                GuiEventStopVideo lStopVideo;
                mpStateInterface->OutputGuiEvent(lStopVideo);
                meIntroState = E_INTROSTATE_WAITING_FOR_VIDEO;
            }
            break;
        }

        case E_INTROSTATE_PHOTOBOOTH:
            // X360: virtual dispatch through vtable slot 10.
            HandleControllerPressedPhotoBooth(lpEvent);
            break;

        default:
            CGS_ASSERT(false, "Unhandled intro state in Intro::HandleControllerInput()");   // cpp:758
            break;
        }
    }

    // ================================================================================
    // @ 0x824D1B98 -- vtable slot 10. PHOTOBOOTH -> LICENCE on an accepted press. Input is
    // swallowed entirely while a voice-over is playing.
    // ================================================================================
    bool Intro::HandleControllerPressedPhotoBooth(const CgsModule::Event* lpEvent)
    {
        bool lbAdvance = false;

        if (!mbVoiceOverPlaying)
        {
            const s32 liAction =
                reinterpret_cast<const GuiEventControllerAction*>(lpEvent)->miAction;

            if (liAction == KI_ACTION_CONFIRM)
            {
                lbAdvance = mPhotoBoothComponent.Select();
            }
            else if (liAction == KI_ACTION_BACK)
            {
                lbAdvance = mPhotoBoothComponent.Cancel();
            }

            if (lbAdvance)
            {
                LogIntroState(meIntroState, E_INTROSTATE_LICENCE, "HandleControllerPressedPhotoBooth");
                meIntroState = E_INTROSTATE_LICENCE;
                SetupComponents();
            }
        }

        return lbAdvance;
    }

    // ================================================================================
    // @ 0x824DF0B0
    // ================================================================================
    void Intro::Update()
    {
        HandleIncomingEvents();
        HandleStateTransitions();

        switch (meIntroState)
        {
        case E_INTROSTATE_NONE:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:212
            mpStateInterface->StopLoadingScreen();
            LogIntroState(E_INTROSTATE_NONE, E_INTROSTATE_LOADINGRESOURCES, "Update");
            meIntroState = E_INTROSTATE_LOADINGRESOURCES;
            // FALL THROUGH -- the X360 case 0 runs straight on into case 5 (no branch).

        case E_INTROSTATE_LOADINGRESOURCES:
            if (mpGuiCache != 0 &&
                mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad) &&
                mLicenseComponent.EnsureResourcesAreLoaded() &&
                mPhotoBoothComponent.EnsureResourcesAreLoaded())
            {
                // off_82F27B2C[0] == "BrnIntro", apt level 3.
                mpStateInterface->PlayAptMovie("BrnIntro", 3);
                mLicenseComponent.OnLoad();
                mPhotoBoothComponent.OnLoad();
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                AppendExpectedComponents();
                LogIntroState(E_INTROSTATE_LOADINGRESOURCES, E_INTROSTATE_WAITINGFORCOMPONENTS, "Update");
                meIntroState = E_INTROSTATE_WAITINGFORCOMPONENTS;
            }
            break;

        case E_INTROSTATE_WAITINGFORCOMPONENTS:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:248
            if (mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                LogIntroState(E_INTROSTATE_WAITINGFORCOMPONENTS, E_INTROSTATE_WELCOMETEXT, "Update");
                meIntroState = E_INTROSTATE_WELCOMETEXT;
                SetupComponents();
            }
            break;

        case E_INTROSTATE_START_FLYBY:
            if (CacheIsUserSignedIn(mpGuiCache))
            {
                mfFlybyTimer = 0.0f;
                LogIntroState(E_INTROSTATE_START_FLYBY, E_INTROSTATE_WAIT_FOR_FLYBY_FINISH, "Update");
                meIntroState = E_INTROSTATE_WAIT_FOR_FLYBY_FINISH;
                PostCommand16<KI_COMMAND_FLYBY_START>(mpStateInterface, KI_CHANNEL_GUI_OUT);
            }
            break;

        case E_INTROSTATE_WAIT_FOR_FLYBY_FINISH:
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // cpp:295
            if (CacheIsUserSignedIn(mpGuiCache))
            {
                mfFlybyTimer += mpGuiCache->GetTimeStep();
                if (mfFlybyTimer >= KF_INTRO_FLYBY_DURATION)
                {
                    PostCommand16<KI_COMMAND_FLYBY_END>(mpStateInterface, KI_CHANNEL_GUI_OUT);
                    PostVoiceOver(mpStateInterface, KAC_VO_INTRO_SHOW_CAR);
                    SendStateEvent(mpProfile->GetIsNewProfile() ? "ADVANCE" : "GO_BACK");
                }
            }
            break;

        case E_INTROSTATE_WELCOMETEXT:
        case E_INTROSTATE_PHOTOBOOTH:
        case E_INTROSTATE_LICENCE:
            break;

        default:   // PLAYING_VIDEO(1) / WAITING_FOR_VIDEO(2) -- see the file note.
            CGS_ASSERT(false, "Unhandled IntroState in Intro::Update()");   // cpp:330
            break;
        }

        mLicenseComponent.SendPlayerPictureEvent();
        mPhotoBoothComponent.SendPlayerPictureEvent();
    }
}
