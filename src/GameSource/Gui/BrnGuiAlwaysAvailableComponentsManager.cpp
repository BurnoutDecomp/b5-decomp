// ===================================================================================
// BrnGui::AlwaysAvailableComponentsManager
//   GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.cpp
//
//   AlwaysAvailableComponentsManager::Construct       @ 0x824F3628
//   AlwaysAvailableComponentsManager::Prepare         @ 0x824F3760
//   AlwaysAvailableComponentsManager::PrepareFlapt    @ 0x824F3858
//   AlwaysAvailableComponentsManager::SetInEventQueue @ 0x824F3920  (virtual)
//   AlwaysAvailableComponentsManager::Update          @ 0x82509338  (virtual)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX for SEMANTIC PARITY. All five methods are
// non-static members (asm: r3 = this). Member access is BY NAME throughout; the X360
// Begin/Fire/End dev-assert sequences fold into CGS_ASSERT(cond,"msg") per house style.
//
// This manager owns the GUI components that are always loaded (the in-game EATrax track
// banner, the achievement pop-up, the online-invite message bar, the save-icon spinner
// and the "Showtime!" banner). Construct builds them in place; Prepare drives a two-phase
// prepare/register state machine; PrepareFlapt binds each to its named clip in a flapt
// movie file; SetInEventQueue latches the input event queue Update pumps.
//
// Update (@0x82509338) is the large per-frame event pump, switching on ~15 distinct GUI
// event-payload types read by field off the live event record.
//
// ⭐ 2026-09-16 -- THE "BLOCKED" BANNER THAT USED TO SIT HERE WAS STALE, AND IT WAS THE ONLY
// THING KEEPING THE IN-GAME EATRAX CHYRON OFF THE SCREEN. It said the EATrax cases could not
// be written because EaTraxHelper, OptionsDataProfile and GuiEventTimeInfo::GetTime had "no
// reconstructable type home". Every one of them has a home with bodies today:
//   * BrnSound::Module::Io::EaTraxHelper   -> BrnPreUpdateSharedIo.h:80   (mounted)
//   * BrnGui::OptionsDataProfile           -> BrnGuiOptionsDataProfile.h  (mounted)
//   * CgsGui::GuiEventTimeInfo::GetTime    -> CgsGuiEventTypeDefs.cpp:14  (TU was simply not
//     MOUNTED, which is a build-list gap, not a missing body -- now mounted)
// The cache far members the banner listed are just the profile reached positionally:
// cache+0xB878 IS GetOptionsDataProfile() (BrnGuiCache.h:1001/:2116), and profile+0x7344 is
// miLastPlayedSongIndex. Cases 26 and 502 are now written from the asm; see each in place.
//
// STILL DEFERRED, and honestly so: the online-invite family (175/43/105), whose
// OnlineInviteMessageComponent::ShowMessage is genuinely not committed, and case 503 (the
// CLASSICAL chyron -- a different overlay, DWARF GuiClassicalChyronEvent, carrying two
// 48-char text ids and writing cache+0x12BC0 == the profile's picture-paradise index).
// ===================================================================================
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::RegisterForEvents
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                     // CgsGui::GuiAccessPointers
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"                        // BrnFlapt::FileRef
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // the in-queue Update walks
#include "GameSource/Gui/BrnGuiCache.h"                                  // BrnGui::GuiCache (event 64 connect)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"              // CgsGui::GuiEventTimeInfo (event 26)
#include "GameSource/Gui/Events/BrnGuiEventAudioTrax.h"                   // GuiEATraxNewTrackEvent (event 502)
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile (case 502 tail)
#include "GameSource/Sound/Module/SharedIO/BrnPreUpdateSharedIo.h"        // Io::EaTraxHelper (case 502)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // [DIAG] gpDebugPrint witness
#include <cstdlib>                                                       // [DIAG] getenv

namespace BrnGui
{
    // The GUI events the manager registers to observe: the X360 passes the real game-data
    // table dword_8206F760 with a hard-coded count of 19 (asm @0x824F37FC `li r5,0x13`;
    // DWARF `maiEventToObserve[19]` / `miNumEventsObserved == 19`). ALL 19 literal values
    // below are ATTESTED: dumped directly from the ARTIST IDA database at 0x8206F760
    // (2026-07-16; the table sits right after the "ShowtimeMsg_cpt" string literal), in
    // the authored order. The Update switch @0x82509338 handles fifteen of them; 21 / 94 /
    // 96 / 192 are observed-but-unhandled there (online-invite family).
    const s32 maiEventToObserve[19] =
    {
        26, 21, 43, 44, 9, 64, 105, 175, 502, 586,
        503, 72, 94, 192, 96, 392, 516, 191, 355,
    }; // ARTIST dword_8206F760 (IDA-dump attested)

    namespace
    {
        const s32 KI_NUM_EVENTS_OBSERVED = 19;   // asm @0x824F37FC: li r5, 0x13

        // The movie-clip / component names the X360 binds each always-available component to.
        const char* const KAC_EATRAX_COMPONENT_MOVIE_CLIP        = "EATrax_mc";
        const char* const KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME   = "AchievementPopup_mc";
        const char* const KAC_ONLINE_NOTIFICATION_COMPONENT_NAME = "OnlineInvite_mc";
        const char* const KAC_SAVE_ICON_COMPONENT_NAME           = "SaveIcon_mc";
        const char* const KAC_SHOWTIME_MESSAGE_CPT_NAME          = "ShowtimeMsg_cpt";
    }

    // @0x824F3628 -- construct the EventObserver base and the five always-available
    // components in place, then clear the manager's flags / pointers.
    void AlwaysAvailableComponentsManager::Construct()
    {
        CgsGui::EventObserver::Construct();

        // The components take the manager's StateInterface (the EventObserver base member,
        // guest +4) as their channel to the rest of the GUI.
        CgsGui::StateInterface* lpStateInterface = &mStateInterface;

        mEATraxInGameComponent.Construct(KAC_EATRAX_COMPONENT_MOVIE_CLIP, lpStateInterface, 0);
        mAchievementPopupComponent.Construct(KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME, lpStateInterface, 0);
        mOnlineNotificationMessageComponent.Construct(KAC_ONLINE_NOTIFICATION_COMPONENT_NAME,
                                                      lpStateInterface, 0);
        // The save-icon and showtime components are polymorphic; the X360 dispatches their
        // Construct through the vtable (slot 0).
        mSaveIconComponent.Construct(KAC_SAVE_ICON_COMPONENT_NAME, lpStateInterface, 0);
        mShowtimeMessageComponent.Construct(KAC_SHOWTIME_MESSAGE_CPT_NAME, lpStateInterface, 0);

        mePrepareStage = E_PREPARE_START;

        mbExternalResourcesDependenciesLoaded = false;
        mbGameLoadStateCompleted              = false;
        mbContainerMovieClipPlaying           = false;
        mbShowNewsNotification                = false;

        mbFlaptPrepared = false;
        mpGuiCache      = 0;
    }

    // @0x824F3760 -- advance the prepare/register state machine one step. Each call drops
    // into its current stage and falls through to the next, so a single call can walk
    // START -> REGISTERFOREVENTS -> LOADRESOURCES; it returns true only once the GuiCache has
    // been latched (mpGuiCache != null, set by the connect event in Update) and the stage
    // reaches DONE.
    bool AlwaysAvailableComponentsManager::Prepare(CgsGui::GuiAccessPointers* lpGuiAccessPointers)
    {
        switch (mePrepareStage)
        {
        case E_PREPARE_START:
            mePrepareStage = E_PREPARE_START;
            // Prepare the EventObserver base (its StateInterface) with the access pointers
            // and a null resource allocator (X360 passes 0 for the allocator).
            CGS_ASSERT(CgsGui::EventObserver::Prepare(lpGuiAccessPointers, 0),
                       "CgsGui::EventObserver::Prepare(lpGuiAccessPointers)");
            // fall through

        case E_PREPARE_REGISTERFOREVENTS:
            mePrepareStage = E_PREPARE_REGISTERFOREVENTS;
            mStateInterface.RegisterForEvents(maiEventToObserve, KI_NUM_EVENTS_OBSERVED);
            // fall through

        case E_PREPARE_LOADRESOURCES:
            mePrepareStage = E_PREPARE_LOADRESOURCES;
            // Only advance to DONE once the GuiCache pointer has been latched.
            if (mpGuiCache == 0)
            {
                return false;
            }
            // fall through

        case E_PREPARE_DONE:
            mePrepareStage = E_PREPARE_DONE;
            return true;

        default:
            return false;
        }
    }

    // @0x824F3858 -- bind every always-available component to its named movie clip in the
    // supplied flapt file, initialise the EATrax + achievement components, and mark the
    // container clip playing / the flapt prepared.
    void AlwaysAvailableComponentsManager::PrepareFlapt(const BrnFlapt::FileRef& lFile)
    {
        mEATraxInGameComponent.Prepare(KAC_EATRAX_COMPONENT_MOVIE_CLIP, lFile);
        mAchievementPopupComponent.Prepare(KAC_ACHIEVEMENT_POPUP_COMPONENT_NAME, lFile);
        mOnlineNotificationMessageComponent.Prepare(KAC_ONLINE_NOTIFICATION_COMPONENT_NAME, lFile);
        mSaveIconComponent.Prepare(KAC_SAVE_ICON_COMPONENT_NAME, lFile);
        mShowtimeMessageComponent.Prepare(KAC_SHOWTIME_MESSAGE_CPT_NAME, lFile);

        mbContainerMovieClipPlaying = true;

        mEATraxInGameComponent.Initialize();
        mAchievementPopupComponent.Initialize();

        mbFlaptPrepared = true;
    }

    // @0x824F3920 -- latch the input GUI event queue the manager pumps each frame in Update.
    void AlwaysAvailableComponentsManager::SetInEventQueue(CgsModule::VariableEventQueue<18432, 16>* lpInGuiEventQueue)
    {
        mpInGuiEventQueue = lpInGuiEventQueue;
    }

    namespace
    {
        // The event-64 connect payload (the GuiCache pointer), same shape BootProfile reads.
        struct AacGuiEventCache : public CgsModule::Event
        {
            GuiCache* mpGuiCache;
        };
    }

    // @0x82509338 -- per-frame event pump: walk the in-queue and drive the always-available
    // overlays. Reconstructed from the X360 body. The two in-game EATrax rich-presence cases
    // (502/503) reach BrnSound::Module::Io::EaTraxHelper + BrnGui::OptionsDataProfile /
    // GuiCache FAR members that have no committed type home; those specific accessor calls are
    // FLAG'd deferrals (documented below) -- the event DISPATCH and every other case (the
    // save-icon 355, the connect 64, showtime, achievement, the load flags) are faithful.
    void AlwaysAvailableComponentsManager::Update()
    {
        // The console gates the whole pump on the flapt being bound (this+65988): with no
        // clips bound there is nothing to drive.
        if (!mbFlaptPrepared || mpInGuiEventQueue == 0)
            return;

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liId)
            {
            case 64:   // connect: latch the GuiCache (the far members the in-game cases read)
                CGS_ASSERT(lpEvent != 0, "Invalid cache in AlwaysAvailableComponentsManager::Update");
                mpGuiCache = static_cast<const AacGuiEventCache*>(lpEvent)->mpGuiCache;
                break;

            case 26:   // per-frame time-step: stamp the timed (in-game) overlays
                // ⭐ RESTORED 2026-09-16. The old FLAG said GuiEventTimeInfo::GetTime()
                // "is not committed to the link". Its body was in the tree all along
                // (CgsGuiEventTypeDefs.cpp:14) -- the TU simply was not MOUNTED, so the
                // flag was stale rather than true. Mounted, and the console's two stamps
                // restored. Without them mfCurrentGameTime_Seconds never advances, so the
                // EATrax chyron's anim-out timer can never expire and the overlay could
                // never hide itself even once something showed it.
                //   82509840  stbx r10, r28, 0x101B9   ; mbGameLoadStateCompleted = true
                //   82509844  bl 0x8240E328            ; GuiEventTimeInfo::GetTime
                //   82509848  add r3, r28, r27         ; r27 == 0x10020, the EATrax chyron
                //   8250984C  bl 0x82415C18            ; EATraxInGameComponent::SetTime
                //   82509854  bl 0x8240E328            ; GetTime again (the console re-reads)
                //   82509858  add r3, r28, r31         ; r31 == 0x10060, the achievement popup
                //   8250985C  bl 0x82415D60            ; AchievementPopupComponent::SetTime
                mbGameLoadStateCompleted = true;
                {
                    // GuiEventTimeInfo is a PLAIN 8-byte payload, not GuiEvent<N>-derived,
                    // so the queue pointer IS the record -- the console passes r25 straight
                    // to GetTime. Same reinterpret the tree already uses at
                    // BrnGuiModule.cpp:1936 and BrnOnlineScoreboards_wI_06.cpp:328.
                    const CgsGui::GuiEventTimeInfo* lpTimeInfo =
                        reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent);
                    // The console calls GetTime once per consumer (826E: two separate bl's);
                    // it is a pure accessor, so the two reads are the same value.
                    mEATraxInGameComponent.SetTime(
                        static_cast<f32>(lpTimeInfo->GetTime()));
                    mAchievementPopupComponent.SetTime(
                        static_cast<f32>(lpTimeInfo->GetTime()));
                }
                break;

            case 44:   // buddy notification cleared
                mbShowNewsNotification = false;
                break;

            case 72:   // external resource dependencies loaded
                mbExternalResourcesDependenciesLoaded = true;
                break;

            case 355:  // autosave icon: show (payload==1) / hide the top-left save spinner
                if (*reinterpret_cast<const u8*>(lpEvent) == 1)
                    mSaveIconComponent.ShowSaveIcon();
                else
                    mSaveIconComponent.HideSaveIcon();
                break;

            case 392:  // showtime banner: show / hide
                if (*reinterpret_cast<const u8*>(lpEvent) == 1)
                    mShowtimeMessageComponent.Show();
                else
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 191:  // showtime hide (immediate) when the payload flag is clear
                if (*reinterpret_cast<const u8*>(lpEvent) == 0)
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 9:    // fall-through hide: only once fully prepared
            case 516:
                if (mePrepareStage == E_PREPARE_DONE)
                    mShowtimeMessageComponent.Hide(true);
                break;

            case 586:  // new achievement unlocked
                mAchievementPopupComponent.DisplayNewAchievementNotification(
                    reinterpret_cast<const AchievementPopupComponent::AchievementsBitArray*>(lpEvent));
                break;

            // ⭐ RESTORED 2026-09-16 -- the EATrax "now playing" chyron. The old FLAG said
            // BrnSound::Module::Io::EaTraxHelper had "no committed type home"; that was true
            // when it was written and is not any more (the EA Trax wave gave it a home at
            // BrnPreUpdateSharedIo.h:80, with all three getters bodied and the TU mounted).
            // The stale flag was the only thing keeping the chyron off the screen.
            //
            // Console @0x82509908 (the 0x1F6 arm of the id dispatch at 0x82509884):
            //   82509910  bl 0x826B0420   ; EaTraxHelper::GetAlbumName (event->miSong)
            //   82509920  bl 0x826B0380   ; EaTraxHelper::GetSongName
            //   82509930  bl 0x826B03D0   ; EaTraxHelper::GetArtistName
            //   82509934  lbzx r11, r28, r23   ; r23 == 0x101BA, mbContainerMovieClipPlaying
            //   82509940  beq  -> 0x82509958   ; ...the chyron only shows while it is set
            //   82509944  li r7, 0             ; lbLocalised = false (raw metadata strings)
            //   82509950  add r3, r28, r27     ; r27 == 0x10020, mEATraxInGameComponent
            //   82509954  bl 0x82439F08        ; DisplayNewTrackNotification(artist,song,album)
            // Argument order is settled by the register assignment: r4 artist, r5 song,
            // r6 album -- note the GETTERS are called album/song/artist, the reverse.
            // The producer is already in the tree: MusicEffect posts this record as event
            // 502, 24 bytes, with miSong at +0x10 and the set flag at +0x14
            // (BrnMusicEffect.cpp:699-713), and 502 is already in this manager's
            // RegisterForEvents list above.
            case 502:
            {
                const GuiEATraxNewTrackEvent* lpNewTrack =
                    reinterpret_cast<const GuiEATraxNewTrackEvent*>(lpEvent);
                const BrnSound::Module::Io::EaTraxHelper lHelper;   // stateless (static key)
                const char* lpcAlbum  = lHelper.GetAlbumName(lpNewTrack->miSongIndex);
                const char* lpcSong   = lHelper.GetSongName(lpNewTrack->miSongIndex);
                const char* lpcArtist = lHelper.GetArtistName(lpNewTrack->miSongIndex);
                if (mbContainerMovieClipPlaying)
                    mEATraxInGameComponent.DisplayNewTrackNotification(
                        lpcArtist, lpcSong, lpcAlbum, false);

                // [DIAG] NOT IN THE X360 BINARY (BRN_MUSIC_DIAG=1). The producer's own
                // witness only proves the event was POSTED; this one proves it arrived,
                // survived the sound->GUI append, and that the chyron was actually asked to
                // draw with real metadata. Rare by construction (one per track change).
                if (getenv("BRN_MUSIC_DIAG") != 0 && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[trax] GUI 502 song " << lpNewTrack->miSongIndex
                        << " preview " << static_cast<s32>(lpNewTrack->mbPreview)
                        << " gate " << static_cast<s32>(mbContainerMovieClipPlaying)
                        << " artist '" << (lpcArtist ? lpcArtist : "<null>")
                        << "' song '" << (lpcSong ? lpcSong : "<null>")
                        << "' album '" << (lpcAlbum ? lpcAlbum : "<null>") << "'\n";
                }

                // 82509958..82509960 -- an AUDITION stops here; only a real track change
                // persists to the profile.
                if (lpNewTrack->mbPreview != 0)
                    break;

                // 8250996C..825099CC -- the profile tail. This was FLAG'd too, on the same
                // stale "OptionsDataProfile has no committed type home" claim; it has a home
                // (BrnGuiOptionsDataProfile.h) and GetOptionsDataProfile/SetTraxRemaining are
                // both bodied. The console reaches the profile as a GuiCache FAR member --
                // `addi r31, r31, -0x4788` on cache+0x10000 is cache+0xB878, which is exactly
                // where GetOptionsDataProfile() points (BrnGuiCache.h:1001/:2116).
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");                       // cpp:486
                OptionsDataProfile* lpOptionsDataProfile =
                    mpGuiCache->GetOptionsDataProfile();
                CGS_ASSERT(lpOptionsDataProfile != 0, "lpOptionsDataProfile");   // cpp:489
                // 825099C8  stw r11, 0x7344(r31) -- the setter, inlined by the console.
                lpOptionsDataProfile->SetLastPlayedSongIndex(lpNewTrack->miSongIndex);
                // 825099CC  bl 0x824F0180
                lpOptionsDataProfile->SetTraxRemaining(&lpNewTrack->mRemainingSongs);
                break;
            }

            // The online-invite chyron (175/43/105) and the EATrax rich-presence case (503)
            // drive in-game-only overlays that do not fire on the boot/front-end path. Their
            // console bodies reach OnlineInviteMessageComponent::ShowMessage (not yet
            // committed) and BrnGui::OptionsDataProfile / GuiCache far members with no
            // committed type home. The dispatch is kept; the un-homed accessor calls are
            // FLAG'd deferrals until those TUs land.
            // FLAG PC-platform leaf: in-game online-invite overlay + 503 (un-homed callees).
            case 175:
            case 43:
            case 105:
            case 503:
                break;

            default:
                break;
            }
        }
    }
}
