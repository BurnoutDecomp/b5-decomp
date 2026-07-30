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
// COMPONENT BOUNDARY (half closed): BrnPhotoBoothComponent.cpp IS now reconstructed and
// mounted, so every photo-booth call below is the REAL component (Construct / SetVisualStyle
// / SetCachePointer / SetProfilePointer / EnsureResourcesAreLoaded / OnLoad / ShowComponent /
// HideComponent / SetButtonPromptVisible / Select / Cancel /
// HandleCompressedStillImageEvent / SendPlayerPictureEvent / ReleaseResources) and its leaves
// are gone. BrnLicenseComponent.cpp's presentation slice is still deferred, so the LICENSE
// calls still go through the FLAG'd leaves in the anonymous namespace below; each names the
// X360 address it stands in for and is deleted when that slice lands.
//
// ⚠ 8 -> 9 IS A CONTROLLER PRESS and the PC input bridge delivers the accept as action 45,
// not the console's 49 -- see KI_ACTION_CONFIRM_PC below. Recognising only 49 parks the state
// in PHOTOBOOTH forever (boot-measured).
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnIntro.h"

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

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // CgsDev::Log::WriteToLog (the state-transition diagnostic)

#include <cstring>   // std::strcmp (the X360 inlines the apt-trigger name compares)
#include <cstdio>    // std::snprintf (the state-transition diagnostic)

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
        // FLAG PC-platform input bridge (the same one BrnBootProfile.cpp:44 documents): the
        // console A-button "select" arrives as action 49, but the PC input reconstruction
        // (CgsInput::InputPadsPC) maps the accept key (ENTER / SPACE / pad-A) to action 45 --
        // the action the title SelectionMenu consumes. Boot-measured: with only 49 recognised,
        // BrnGui::Intro parks in PHOTOBOOTH forever because the confirm press it is waiting for
        // is delivered as 45. Recognised alongside 49; behaviour is otherwise identical.
        const s32 KI_ACTION_CONFIRM_PC = 45;

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
        // CgsSound::Playback::Name::MakeHash is unreconstructed, so
        // the PC build posts id 0 (the sound module ignores an unknown id). The sequence's
        // TIMING is unaffected -- the 2 s dwells are gated on the 466/467 events, which the
        // sound module raises for whatever it decides to play.
        // FLAG PC-platform leaf: see the note above.
        u32 VoiceOverNameHash(const char* /*lpacVoiceOverName*/) { return 0u; }

        // Mirror of BrnGui::GuiEventAudioVoiceOver (real home BrnGuiDemangledEventTypes.h,
        // "id 466 size 4"). That header cannot be included here: it redefines
        // BrnGui::GuiEventRunFsm and BrnGui::GuiAudioTriggerEvent, which this TU already has
        // from their real home BrnGuiEventTypeDefs.h. Mirroring the payload is the same
        // remedy BrnGuiDemangledEventTypes.h itself documents for GuiEventAudioTraxUpdate.
        // (CgsModule::Event is empty, so the base keeps sizeof == 4, which is the payload
        // size the X360 wrapper header records.)
        struct GuiEventAudioVoiceOver : public CgsModule::Event
        {
            u8 maData[4];
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
            GuiEventAudioVoiceOver lEvent;
            const u32 luNameHash = VoiceOverNameHash(lpacVoiceOverName);
            lEvent.maData[0] = static_cast<u8>((luNameHash >> 24) & 0xFF);
            lEvent.maData[1] = static_cast<u8>((luNameHash >> 16) & 0xFF);
            lEvent.maData[2] = static_cast<u8>((luNameHash >>  8) & 0xFF);
            lEvent.maData[3] = static_cast<u8>( luNameHash        & 0xFF);
            lpInterface->OutputGuiEvent(lEvent);
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

        // GuiCache +0x13B5E, set to 1 by Intro::OnLeave and also written by
        // CarSelect{Livery,Vehicle}::SetupComponents / CarSelectLivery::OnLeave+Update and
        // GuiCache::Construct. Its role is NOT settled -- do not guess it.
        // FLAG PC-platform leaf: no-op boundary until the GuiCache TU names the byte.
        void CacheSetIntroLeftFlag(GuiCache* /*lpCache*/) {}

        // ================================================================================
        // Component boundaries. These stand in for REAL X360 functions whose bodies are
        // recovered but not yet reconstructed into their own TUs (the ledger's
        // BrnLicenseComponent.cpp / BrnPhotoBoothComponent.cpp slices). Each leaf names the
        // X360 address so it can be deleted the moment that TU lands. Nothing here invents
        // behaviour: where a leaf returns a value it returns the console's own result for
        // this configuration.
        // ================================================================================

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::OnLoad @0x82440AC0.
        void LicenseOnLoad(LicenseComponent& /*lrLicense*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::ShowLicense @0x82440C98.
        void LicenseShowLicense(LicenseComponent& /*lrLicense*/, bool /*lbUpgraded*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::SetVisible @0x82440E38.
        void LicenseSetVisible(LicenseComponent& /*lrLicense*/, bool /*lbVisible*/) {}

        // BrnGui::LicenseComponent::SetPlayerInfo @0x8243C380 --
        // pushes the driver name (a language-database string id) plus the rank / percentage
        // set the X360 passes as six zeroes from this call site.
        // FLAG PC-platform leaf: see the note above.
        void LicenseSetPlayerInfo(LicenseComponent& /*lrLicense*/, const char* /*lpacPlayerName*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::HandleAptLoadTriggers @0x8241A848.
        void LicenseHandleAptLoadTriggers(LicenseComponent& /*lrLicense*/,
                                          const CgsGui::GuiEventAptTriggerPayload* /*lpTrigger*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::HandleAptTransitionTriggers @0x8243BE20.
        void LicenseHandleAptTransitionTriggers(LicenseComponent& /*lrLicense*/,
                                                const CgsGui::GuiEventAptTriggerPayload* /*lpTrigger*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::ReleaseResources @0x82440BC0.
        void LicenseReleaseResources(LicenseComponent& /*lrLicense*/) {}

        // BrnGui::LicenseComponent::SendPlayerPictureEvent @0x8243CB90
        // -- posts event 258 carrying the profile's licence picture, and posts NOTHING when
        // the profile has no picture, which is the PC configuration.
        // FLAG PC-platform leaf: see the note above.
        void LicenseSendPlayerPictureEvent(LicenseComponent& /*lrLicense*/) {}

        // BrnGui::LicenseComponent::Construct @0x8241A610 and
        // BrnGui::PhotoBoothComponent::Construct @0x8241ABC0. Neither TU can be linked yet --
        // see the block comment in tools/build/build_game_exe.bat for the four link gaps.
        // FLAG PC-platform leaf: see the note above.
        // PARTIAL, not empty: both X360 bodies OPEN with the same call --
        //   IconComponent::Construct(this, lpacName, lpStateInterface, NULL, lpacParentName)
        // -- and BrnIcon.cpp IS linked, so that first statement is reproduced for real. It is
        // the statement that matters to this state: it fills the component's macName /
        // muHashedName, which AppendExpectedComponents hands to the GUI cache as the apt
        // components the intro waits on. Only the per-component tails (the licence's six
        // TextFields + scalars, the photo booth's two HelpItems + NetworkTexture + enums)
        // are deferred.
        void LicenseConstruct(LicenseComponent& lrLicense, const char* lpacName,
                              CgsGui::StateInterface* lpStateInterface,
                              const char* lpacParentName)
        {
            lrLicense.IconComponent::Construct(lpacName, lpStateInterface, 0, lpacParentName);
        }

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::SetCachePointer @0x824B31E8.
        void LicenseSetCachePointer(LicenseComponent& /*lrLicense*/, GuiCache* /*lpCache*/) {}

        // FLAG PC-platform leaf: BrnGui::LicenseComponent::SetProfilePointer @0x824B3248.
        void LicenseSetProfilePointer(LicenseComponent& /*lrLicense*/,
                                      BrnProgression::Profile* /*lpProfile*/) {}

        // BrnGui::LicenseComponent::EnsureResourcesAreLoaded @0x824B3300 and
        // BrnGui::PhotoBoothComponent::EnsureResourcesAreLoaded @0x824B34F0 stream the
        // licence-rank / photo-booth GUIAPT bundles through the cache. With their TUs
        // unlinked the components own no resources, so reporting "loaded" is the honest
        // reading of "nothing outstanding" -- and it is what lets the state's OWN resources
        // (maResourcesToLoad, which the cache really does load) gate the sequence.
        // FLAG PC-platform leaf: see the note above.
        bool LicenseEnsureResourcesAreLoaded(LicenseComponent& /*lrLicense*/) { return true; }

        // BrnProgression::Profile::SetLicenceIssuedDateAsNow @0x8235A0E0 and
        // GetLicenceIssuedDate both live in BrnProfile.cpp, which is not mounted in this
        // build. The date the licence card shows therefore stays at the profile's stored
        // value instead of "now"; the month/day/year fields still receive it.
        // FLAG PC-platform leaf: see the note above.
        void ProfileSetLicenceIssuedDateAsNow(BrnProgression::Profile* /*lpProfile*/) {}

        // FLAG PC-platform leaf: BrnProgression::Profile::GetLicenceIssuedDate (as above).
        CgsSystem::DateAndTime ProfileGetLicenceIssuedDate(const BrnProgression::Profile* /*lpProfile*/)
        {
            // Update() == "now", which is exactly what SetLicenceIssuedDateAsNow would have
            // stored a frame earlier, so the month/day/year fields still get a valid date
            // (and the cpp:443/444 month asserts stay quiet).
            CgsSystem::DateAndTime lDate;
            lDate.Update();
            return lDate;
        }

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
        // X360: LicenseComponent::Construct @0x8241A610 (partial on PC -- see its body).
        LicenseConstruct(mLicenseComponent, KAC_LICENSECOMP_NAME, mpStateInterface, 0);

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
        CacheSetIntroLeftFlag(mpGuiCache);   // X360: stb 1, mpGuiCache+0x13B5E

        // X360: the inlined GuiEventPlayAptMovie record {8, 18, 12, "", 3} on channel 41,
        // 20 bytes -- unmount the intro apt movie at level 3.
        mpStateInterface->PlayAptMovie("", 3);

        LicenseReleaseResources(mLicenseComponent);
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
            LicenseSetVisible(mLicenseComponent, false);
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
            LicenseShowLicense(mLicenseComponent, false);
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

        LicenseReleaseResources(mLicenseComponent);
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
                    LicenseHandleAptLoadTriggers(mLicenseComponent, lpTrigger);
                }
                else if (lpTrigger->meEventType ==
                         CgsGui::GuiEventAptTrigger::E_APT_EVENT_TRANSITION_COMPLETE)
                {
                    LicenseHandleAptTransitionTriggers(mLicenseComponent, lpTrigger);
                }
                break;
            }

            case KI_EVENT_GUI_CACHE:
                if (mpGuiCache == 0)
                {
                    mpGuiCache = reinterpret_cast<const GuiEventCache*>(lpEvent)->mpGuiCache;
                    LicenseSetCachePointer(mLicenseComponent, mpGuiCache);
                    LicenseSetPlayerInfo(mLicenseComponent, mpGuiCache->GetPlayerName());
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
                    ProfileSetLicenceIssuedDateAsNow(mpProfile);
                    LicenseSetProfilePointer(mLicenseComponent, mpProfile);
                    mPhotoBoothComponent.SetProfilePointer(mpProfile);

                    const CgsSystem::DateAndTime lLicenceDate = ProfileGetLicenceIssuedDate(mpProfile);

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
            if (liVideoAction == KI_ACTION_CONFIRM || liVideoAction == KI_ACTION_CONFIRM_PC)
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

            if (liAction == KI_ACTION_CONFIRM || liAction == KI_ACTION_CONFIRM_PC)
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
                LicenseEnsureResourcesAreLoaded(mLicenseComponent) &&
                mPhotoBoothComponent.EnsureResourcesAreLoaded())
            {
                // off_82F27B2C[0] == "BrnIntro", apt level 3.
                mpStateInterface->PlayAptMovie("BrnIntro", 3);
                LicenseOnLoad(mLicenseComponent);
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

        LicenseSendPlayerPictureEvent(mLicenseComponent);
        mPhotoBoothComponent.SendPlayerPictureEvent();
    }
}
