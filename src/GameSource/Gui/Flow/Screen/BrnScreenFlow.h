#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"     // CgsID
#include "GameSource/Gui/Flow/BrnBaseFlow.h"       // BrnGui::BrnBaseFlow (base)

namespace CgsGui    { struct GuiAccessPointers; struct State; }
namespace CgsMemory { class  LinearMalloc; }
namespace rw        { struct IResourceAllocator; }

// BrnGui::BrnScreenFlow - the front-end SCREEN GUI flow (E_GUIFLOW_SCREEN). A BrnBaseFlow that
// owns the pool of 61 front-end screen states -- intro/car-select, the CrashNav menu screens,
// pause/video, the ON_* online screens, the POST_*/PE_* post-event screens, the RE_* replay
// screens and the debug screen -- and installs them into its embedded CgsGui::StateMachine
// under their script ids. The BRNSCREENFSM Lua script then drives SetState by those ids.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Prepare @0x82523E50   PrintStateSizes @0x824F2150
// (Construct/Update have no TU-local X360 export -- they fold with the identical
// BrnHudFlow bodies; reconstructed to the DWARF declarations via the sibling idiom.)
// Class shape/member names from the DecFIGS DWARF (BrnScreenFlow.h) gated on the X360
// Prepare: the DWARF's PS3-only mpStateCrashNavAchievements / mpStateReplay (CrashNavReplay)
// slots are ABSENT from the X360 build, which instead carries OnlineTeamSelection ON_TEAMS
// and the eight RE_* replay states -- the X360 ledger decides what exists. The 61 members
// are the X360 fields flow+0x1024C..+0x1033C (one state pointer each, in the X360 declared
// order); on x64 the offsets differ (8-byte pointers) but the member order is faithful.
namespace BrnGui
{
    class GuiCache;         // GameSource/Gui/BrnGuiCache.h (held by pointer in the base)
    class ProfileManager;   // threaded by reference to CN_PROFILE's wider Construct (never
                            // dereferenced by the flow itself; may be an un-reconstructed shell)

    // -- committed screen states (class-key matches each definition) ----------------------
    struct Intro;                          // States/BrnIntro.h
    struct CarSelectVehicle;               // States/BrnCarSelectVehicle.h
    struct CarSelectOnlineEnd;             // States/BrnCarSelectOnlineEnd.h
    struct ScreenLoading;                  // States/BrnScreenLoading.h
    struct InGame;                         // States/BrnInGame.h
    struct CrashNavStats;                  // States/BrnCrashNavStats.h
    struct CrashNavSettings;               // States/BrnCrashNavSettings.h
    class  CrashNavOptions;                // States/BrnCrashNavOptions.h
    struct CrashNavAccountManagement;      // States/BrnCrashNavAccountManagement.h
    struct CrashNavTrax;                   // States/BrnCrashNavTrax.h
    struct Credits;                        // States/BrnCredits.h
    struct CrashNavColourCalibrate;        // States/BrnCrashNavColourCalibrate.h
    struct CrashNavDriverDetails;          // States/BrnCrashNavDriverDetails.h
    struct CrashNavEnterOnlineFull;        // States/BrnCrashNavEnterOnlineMod.h
    struct CrashNavEnterOnlineNoTitle;     // States/BrnCrashNavEnterOnlineMod.h
    struct PauseScreen;                    // States/BrnPauseScreen.h
    struct Video;                          // States/BrnVideo.h
    class  ImageGalleryState;              // States/BrnImageGallery.h
    struct OnlinePlay;                     // States/BrnOnlinePlay.h
    struct OnlineCustomMatch;              // States/BrnOnlineCustomMatch.h
    struct OnlineCreateFreeburn;           // States/BrnOnlineCreateFreeburn.h
    struct OnlineSelectRoute;              // States/BrnOnlineSelectRoute.h
    struct OnlineGameOptions;              // States/BrnOnlineGameOptions.h
    struct OnlineGameOptionsSummary;       // States/BrnOnlineGameOptionsSummary.h
    struct OnlineScoreboards;              // States/BrnOnlineScoreboards.h
    struct OnlineStats;                    // States/BrnOnlineStats.h
    struct OnlineQuickMatch;               // States/BrnOnlineQuickMatch.h
    struct OnlineNews;                     // States/BrnOnlineNews.h
    struct OnlineRivals;                   // States/BrnOnlineRivals.h
    struct OnlineQuickCustomCreate;        // States/BrnOnlineQuickCustomCreate.h
    struct OnlineFBurnQuickCustomCreate;   // States/BrnOnlineFBurnQuickCustomCreate.h
    struct OnlineViewChallenges;           // States/BrnOnlineViewChallenges.h
    struct OnlineLoading;                  // States/BrnOnlineLoading.h
    struct OnlinePause;                    // States/BrnOnlinePause.h
    struct OnlineMarkMan;                  // States/BrnOnlineMarkMan.h
    struct OnlinePreEvent;                 // States/BrnOnlinePreEvent.h
    struct OnlineYouWin;                   // States/OnlineYouWin.h
    struct GenericForwardState;            // States/BrnGenericForwardState.h
    struct ReplayLoading;                  // States/BrnReplayLoading.h
    struct ReplayMain;                     // States/BrnReplayMain.h
    struct ReplayOutro;                    // States/BrnReplayOutro.h
    struct BrnDebug;                       // States/BrnBrnDebug.h
    struct InstantResultsState;            // ../PostEvent/States/Offline/BrnOfflineInstantResults.h
    struct CompletedGame;                  // ../PostEvent/States/Offline/BrnCompletedGame.h
    struct OfflineRivalShutdown;           // ../PostEvent/States/Offline/BrnOfflineRivalShutdown.h
    struct OfflineTrophyCarUnlock;         // ../PostEvent/States/Offline/BrnOfflineTrophyCarUnlock.h
    struct OnlineInstantResultsState;      // ../PostEvent/States/Online/BrnOnlineInstantResults.h
    struct ShowtimeInstantResultsState;    // ../PostEvent/States/Showtime/BrnShowtimeInstantResults.h

    // -- not-yet-reconstructed screen states (placeholders in States/BrnScreenStatesLinkStubs.h)
    struct NullState;
    struct CarSelectUnlock;
    struct CarSelectLivery;
    struct CrashNavMapEvent;
    struct CrashNavMapMain;
    struct CrashNavProfile;
    struct OnlineGameRoomPlayerInfoState;   // placeholder name; see the stub header's FLAG note
    struct OnlineTeamSelection;
    struct ReplayClips;
    struct ReplayClipsOnline;
    struct ReplayOptions;
    struct ReplayIntro;
    struct ReplayCredits;

    struct BrnScreenFlow : public BrnBaseFlow
    {
        static const s32 KI_NUM_SCREEN_STATES = 61;   // X360 Prepare's SetStates(..., 61)

        // BrnScreenFlow.cpp:42 (DWARF) -- chain to BrnBaseFlow::Construct (stash the GUI
        // cache + reset the streaming/release bookkeeping). No TU-local X360 export
        // (folds with BrnHudFlow::Construct @0x824F1E78, the identical tail-chain).
        virtual void Construct(GuiCache* lpGuiCache);

        // @ 0x82523E50 -- base-prepare, the PrintStateSizes dev dump, then build + install
        // the 61-state screen pool. The wider overload (adds the linear allocator the states
        // are carved from + the profile manager CN_PROFILE's wider Construct needs);
        // distinct vtable slot from BrnBaseFlow::Prepare(access, allocator).
        virtual bool Prepare(CgsGui::GuiAccessPointers* lpAccessPointers,
                             rw::IResourceAllocator* lpAllocator,
                             CgsMemory::LinearMalloc* lpLinearMalloc,
                             ProfileManager& lrProfileManager);

        // BrnScreenFlow.h:226 (DWARF; declared with an inline header body the X360 build
        // emitted no out-of-line copy of) -- is the current state one of the two enter-online
        // sign-in screens? Bodied in the .cpp on PC because the state types are only
        // forward-declared here.
        bool IsInEnterOnlineState();

        // BrnScreenFlow.cpp:293 (DWARF) -- BrnBaseFlow::Update wrapped in the screen-flow
        // CPU perf monitor. No TU-local X360 export (folds with BrnHudFlow::Update
        // @0x82508620, the identical wrap).
        virtual void Update();

    private:
        // @ 0x824F2150 -- dev-only dump of every state's name + sizeof (behind the
        // CgsDev::Message filter). Prepare virtually dispatches it right after the base
        // prepare (@0x82523E94, vtbl +0x18).
        virtual void PrintStateSizes();

        // The 61-state pool, in the X360 declared order (flow+0x1024C..+0x1033C). Comments:
        // registered script id + the X360 (4-byte-pointer) sizeof Prepare/PrintStateSizes carry.
        NullState*                     mpStateNull;                        // NULL          (56)
        Intro*                         mpStateIntro;                       // INTRO         (4464)
        CarSelectUnlock*               mpStateCarSelectUnlock;             // CS_UNLOCK     (1216)
        CarSelectVehicle*              mpStateCarSelectVehicle;            // CS_VEHICLE    (16720)
        CarSelectLivery*               mpStateCarSelectLivery;             // CS_LIVERY     (46096)
        CarSelectOnlineEnd*            mpStateCarSelectOnlineEnd;          // CS_ON_END     (6832)
        ScreenLoading*                 mpStateLoading;                     // LOADING       (72)
        InGame*                        mpStateInGame;                      // INGAME        (96)
        CrashNavMapEvent*              mpStateCrashNavMapEvent;            // CN_MAP_EVENT  (25104)
        CrashNavMapMain*               mpStateCrashNavMapMain;             // CN_MAP_MAIN   (24944)
        CrashNavStats*                 mpStateCrashNavStats;               // CN_STATS      (10728)
        CrashNavSettings*              mpStateCrashNavSettings;            // CN_SETTINGS   (4784)
        CrashNavProfile*               mpStateCrashNavProfile;             // CN_PROFILE    (5896)
        CrashNavOptions*               mpStateCrashNavOptions;             // CN_OPTIONS    (16040)
        CrashNavAccountManagement*     mpStateCrashNavAccountManagement;   // CN_ACCT_MAN   (12160)
        CrashNavTrax*                  mpStateCrashNavTrax;                // CN_TRAX       (568)
        Credits*                       mpStateCredits;                     // CN_CREDITS    (64)
        CrashNavColourCalibrate*       mpStateColourCalibration;           // CN_COLOUR     (4248)
        CrashNavDriverDetails*         mpStateDriverDetails;               // CN_D_DETAIL   (22648)
        CrashNavEnterOnlineFull*       mpStateCrashNavEnterOnlineFull;     // CN_ENTER_ON   (14328)
        CrashNavEnterOnlineNoTitle*    mpStateCrashNavEnterOnlineNoTitle;  // CN_ENTER_ONT  (14328)
        PauseScreen*                   mpStatePause;                       // PAUSED        (4352)
        Video*                         mpStateVideo;                       // FMV_VIDEO     (64)
        ImageGalleryState*             mpStateImageGallery;                // ON_IMG_GAL    (12072)
        OnlinePlay*                    mpStateOnlinePlay;                  // ON_PLAY       (9256)
        OnlineGameRoomPlayerInfoState* mpStateOnlineGameRoom;              // ON_GAME_ROOM  (87968)
        OnlineCustomMatch*             mpStateOnlineCustomMatch;           // ON_CUST_MAT   (57952)
        OnlineCreateFreeburn*          mpStateOnlineCreateFreeburn;        // ON_CREATE_FB  (64)
        OnlineSelectRoute*             mpStateOnlineSelectRoute;           // ON_SEL_ROUTE  (23056)
        OnlineGameOptions*             mpStateOnlineGameOptions;           // ON_GAME_OPT   (42272)
        OnlineGameOptionsSummary*      mpStateOnlineGameOptionsSummary;    // ON_GAME_SUM   (21344)
        OnlineScoreboards*             mpStateOnlineScoreboards;           // ON_SCOREB     (20336)
        OnlineStats*                   mpStateOnlineStats;                 // ON_STATS      (1864)
        OnlineQuickMatch*              mpStateOnlineQuickMatch;            // ON_QWK_MAT    (4936)
        OnlineNews*                    mpStateOnlineNews;                  // ON_NEWS       (680)
        OnlineRivals*                  mpStateOnlineRivals;                // ON_RIVALS     (64)
        OnlineTeamSelection*           mpStateOnlineTeamSelection;         // ON_TEAMS      (4792)
        OnlineQuickCustomCreate*       mpStateOnlineQuickCustomCreate;     // ON_QK_CST_CR  (4504)
        OnlineFBurnQuickCustomCreate*  mpStateOnlineFBurnQuickCustCreate;  // ON_FB_QKCTCR  (4504)
        OnlineViewChallenges*          mpStateOnlineViewChallenges;        // ON_VIW_CHL    (6528)
        OnlineLoading*                 mpStateOnlineLoading;               // ON_LOADING    (19696)
        OnlinePause*                   mpStateOnlinePause;                 // ON_PAUSE      (4352)
        OnlineMarkMan*                 mpStateOnlineMarkMan;               // ON_MARK_MAN   (552)
        OnlinePreEvent*                mpStateOnlinePreEvent;              // ON_PRE_EVENT  (1104)
        InstantResultsState*           mpStateOfflineInstantResults;       // POST_INST     (9024)
        CompletedGame*                 mpStateOfflineCompletedGame;        // PE_COMPLETED  (4056)
        OfflineRivalShutdown*          mpStateOfflineRivalShutdown;        // POST_RIVAL    (1152)
        OfflineTrophyCarUnlock*        mpStateOfflineTrophyCarUnlock;      // TRPHY_UNLOCK  (1152)
        OnlineInstantResultsState*     mpStateOnlineInstantResults;        // POST_ON_IR    (49320)
        OnlineYouWin*                  mpStateOnlineYouWin;                // ON_YOU_WIN    (800)
        ShowtimeInstantResultsState*   mpStateShowtimeInstantResults;      // POST_ST_IR    (2928)
        GenericForwardState*           mpStateGenericForward;              // GEN_FORWARD   (56)
        ReplayClips*                   mpStateReplayClips;                 // RE_CLIPS      (4496)
        ReplayClipsOnline*             mpStateReplayClipsOnline;           // RE_CLIPS_ON   (4496)
        ReplayOptions*                 mpStateReplayOptions;               // RE_OPTIONS    (19320)
        ReplayLoading*                 mpStateReplayLoading;               // RE_LOADING    (64)
        ReplayIntro*                   mpStateReplayIntro;                 // RE_INTRO      (72 alloc; see .cpp note)
        ReplayMain*                    mpStateReplayMain;                  // RE_MAIN       (1096)
        ReplayOutro*                   mpStateReplayOutro;                 // RE_OUT        (64)
        ReplayCredits*                 mpStateReplayCredits;               // RE_CREDITS    (72)
        BrnDebug*                      mpStateDebug;                       // DEBUG         (56)
    };
}
