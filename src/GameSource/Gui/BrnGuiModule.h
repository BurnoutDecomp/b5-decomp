#ifndef BRN_GUI_MODULE_H
#define BRN_GUI_MODULE_H

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"      // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"  // CgsGui::ViewIO Input/OutputBuffer (the per-frame bridge pair)
#include "GameSource/Gui/BrnGuiMovieManager.h"                          // BrnGui::MovieManager (embedded)
#include "GameSource/Gui/BrnGuiColourCalibrationScreen.h"               // BrnGui::ColourCalibrationScreen (embedded; DWARF BrnGuiModule.h:506)
#include "GameSource/Gui/BrnGuiViewModule.h"                             // BrnGui::ViewModule (embedded)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                  // CgsGui::CgsGuiModuleIO::InputBuffer (the inbound GUI event buffer)
#include "GameShared/GameClasses/Gui/Model/CgsModelModuleIO.h"          // CgsGui::ModelIO Input/OutputBuffer (the FSM controller's IO pair)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModule.h" // CgsGui::GuiResourceModule (+ IO buffers) -- the REAL FSM/resource loader
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // CgsModule::VariableEventQueue
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"     // CgsResource::Pool (holds the loaded FSM bundle)
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"               // CgsMemory::HeapMalloc (the FSM Lua VM heap)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"             // CgsMemory::LinearMalloc (the HUD state pool)
#include "GameSource/Gui/BrnGuiFsmController.h"                         // BrnGui::GuiFsmController (the flow FSM controller)
#include "GameSource/Gui/Flow/HUD/BrnHudFlow.h"                         // BrnGui::BrnHudFlow (the 14-state HUD flow)
#include "GameSource/Gui/Flow/Overlay/BrnOverlayFlow.h"                 // BrnGui::BrnOverlayFlow (the 15-popup-state overlay flow)
#include "GameSource/Gui/Flow/Screen/BrnScreenFlow.h"                   // BrnGui::BrnScreenFlow (the 61-state front-end SCREEN flow)
#include "GameSource/Gui/BrnGuiProfile.h"                               // BrnGui::ProfileManager (module-owned; REAL)
#include "GameShared/GameClasses/Gui/CgsGuideIntegration.h"             // CgsGui::SystemUserProfile (module-owned; X360 +949152)
#include "GameSource/Gui/BrnGuiCache.h"                                 // BrnGui::GuiCache (the flow states' cache)
#include "GameSource/Gui/SatNav/BrnMapIconManager.h"   // [H3b] BrnGui::MapIconManager (by-value member)
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h" // BrnGui::FreeburnChallengeManager (module-owned; X360 +309584)
#include "GameSource/Gui/BrnGuiHudMessageDirector.h"                    // BrnGui::HudMessageDirector (module-owned; X360 +639264)
#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"                    // BrnGui::HudMessageAnalyzer (module-owned; X360 +660992)
#include "GameSource/Gui/BrnGuiWorldDataController.h"                   // BrnGui::WorldDataController (module-owned; X360 +307836)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"     // module-owned permanent FLApt components
#include "GameSource/Gui/BrnCustomRendererManager.h"                    // BrnGui::CustomRendererManager (module-owned; X360 +311952)

// BrnGui::GuiModule -- the GUI module (a dispatched CgsModule, like BrnRendererModule). The X360 module
// (Construct 0x82518028 / Prepare 0x82518D68 / Update 0x82527A58 / Render 0x825146B8) builds the entire
// GUI subsystem (model + view + the HUD/Screen flows) and EMBEDS the MovieManager (X360 +301600), driving
// it each frame via UpdateAndRenderMovieManager (0x82511240).
//
// THE BOOT/MENU FLOW IS THE REAL CONTROLLER CHAIN (2026-07-10): the module owns the real
// BrnGui::GuiFsmController + BrnHudFlow (the 14-state pool BF_PRELOAD..PRE_FLY_BY in ONE
// CgsGui::StateMachine), sequenced exactly as the X360 GuiModule::Update does --
//   * GuiEventRunFsm posts (event 144, from the game module's BridgeGameToGui) ->
//     GuiFsmController::RunFsm;
//   * the controller's load machine posts GuiEventLoadRequest records into the ModelIO
//     input buffer; the FSM LuaCode bundle loads land as GuiEventLoadNotification (14)
//     records on the ModelIO output buffer; PrepareLua enters the script's state;
//   * the flow's states register their observed events (records 34/35 on the state
//     interface output queue); the module fans matching inbound GUI events into the
//     flow's in-queue (the EventInterpreterModule observer-subscription dispatch);
//   * each boot state posts command 70 (channel 40) at phase end; the game module's
//     BridgeGuiToGame consumes it and the game main flow requests the next FSM stage.
// The [PC IO] leaf that remains module-side is the synchronous FSM-bundle load standing
// in for the GuiResourceModule module dispatch (ServiceFsmBundleRequests).
namespace BrnGui
{
    class GuiModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // [gateui r4] X360 `BrnGui::GuiModule::Construct` @0x82518028 is NOT the module
        // virtual -- it is a direct, argument-taking call the OWNER makes
        // (BrnGameModule::Construct), and the pure-virtual `Construct()` slot stays
        // `CgsModule::ModuleSingleBuffered::Construct`, exactly as `CgsGui::GuiModule::
        // Construct` @0x82856FA8 (which likewise takes `lpViewModule`) chains into it.
        // Round 4 restores the console's first argument: `a2` == the owner's
        // `&mGameDataModule.mHudMessageController` (X360 gm+0x65A1D0), asserted
        // "lpHudMessageController" BrnGuiModule.cpp:229 and then stored into the cache at
        // line 368 (`*(gm + 1021872) = a2`, the inlined GuiCache::SetHudMessageController
        // with assert "lpController" BrnGuiCache.h:2405). Without it
        // GuiCache::mpHudMessageController had ZERO writers and every HUD message died at
        // HudMessageDirector::FilterAndSendOffMessage's `mpController` assert.
        //
        // The console's remaining arguments are NOT reconstructed here: `lpPopupController`
        // (gm+0x65A1F4, assert BrnGuiModule.cpp:230 -> `*(gm + 1021880)`) has no
        // reconstructed type in this tree, and the trailing aspect/bool pair is already
        // sourced inside the body. Named, not fabricated.
        void Construct(const BrnResource::HudMessageController* lpHudMessageController);

        bool Prepare() override;
        bool Release() override;
        void Destruct() override;
        void Update() override;

        // ⭐ 2026-08-16 (boot audit F-P1-1). Prepare no longer runs from BrnGameModule::Construct;
        // it runs from loading stage 2, with the loading screen already up, exactly as the
        // console's LoadingScriptedState::LoadGUIModule @0x823EF310 pumps it (vtable+0x58 per
        // frame). This latch is what everything that used to be able to assume "the GUI module
        // was prepared before the frame loop started" now tests -- above all the game module's
        // per-frame Update() drive, which on the console does not exist until the module
        // scheduler has the module prepared.
        bool IsPrepared() const { return mbPrepared; }

        // The per-frame GUI render drive (X360 BrnGui::GuiModule::Render @0x825146B8 ->
        // CgsGui::GuiModule::Render @0x8285AF38's core): publish the active renderer set
        // into the view input buffer (SetImRenderers), run the view module's render entry
        // (ViewModule::Render @0x82858810 -> the RenderInternal virtual -> AptAux::Render
        // -> the engine render walk), then flush the filled Apt command buffer to D3D9
        // (the host's PC dispatch leaf), then present the active fullscreen movie
        // (UpdateAndRenderMovieManager @0x82511240) over the view content, exactly the
        // console pass order. FLAG PC-ABI adapter: the console signature takes the
        // scheduler's view/GUI IO buffers + the render output buffer and gates on the
        // module-prepared byte (+949208); this PC drive owns its IO pair, gates on the
        // Apt bring-up, and receives the movie presentation surface as the argument.
        // Called from BrnRendererModule::Render (the PC render thread).
        void Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer);

        // @ 0x82511240 -- MovieManager::Update + the movie frame draw (the movie pass).
        void UpdateAndRenderMovieManager(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer);

        static void FlaptSoundTriggerCallback(void* lpUserData,
                                              const char* lpcComponentName,
                                              const char* lpcSwfName,
                                              const char* lpcActionName,
                                              const char* lpcLabel);

        MovieManager* GetMovieManager() { return &mMovieManager; }
        ViewModule* GetViewModule() { return &mViewModule; }
        AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager()
        {
            return &mAlwaysAvailableComponentsManager;
        }

        // (RequestAptMovieLoad RETIRED, slice 2 of the runtime retirement: the engine's
        // AptLoader owns movie data acquisition -- registered-data first, bundle-IO
        // fallback -- through the real AptLoaderStartAsyncLoad platform hook.)

        // Hand this sub-step's GUI module INPUT buffer (filled by BrnGameModule's
        // BridgeControllerToGui + BridgeGameToGui) to the update drive; Update dispatches
        // its inbound events (144 -> RunFsm, 14/16/481 -> the model notifications,
        // 504/508/513 -> the MovieManager) and fans the rest into the HUD flow's in-queue
        // per the observer subscriptions. FLAG (bridge stand-in): on the console the
        // buffer arrives through the module scheduler's IO set.
        void SetGuiEventInputBuffer(CgsGui::CgsGuiModuleIO::InputBuffer* lpBuffer)
        {
            mpGuiEventInputBuffer = lpBuffer;
        }

        // The module's GUI OUT event queue for this frame (the flow states' channel-40
        // command records and the module's own out events). BrnGameModule::BridgeGuiToGame
        // drains it after the module update -- the PC stand-in for the console GUI module
        // OUTPUT buffer's out-event queue the bridge reads. Cleared by the bridge.
        CgsModule::VariableEventQueue<18432, 16>* GetGuiOutQueue() { return &mGuiOutQueue; }

        // The module's profile manager (X360 module+681696). Exposed so the game module's
        // per-sub-step GUI bridge can read back the progression pair the console's
        // GameState module publishes as game action 193 -- see the FLAG in
        // BrnGameModule.cpp. The X360 reaches the embedded manager by offset from inside
        // GuiModule::Update itself, so no accessor is emitted there.
        ProfileManager&       GetProfileManager()       { return mProfileManager; }
        const ProfileManager& GetProfileManager() const { return mProfileManager; }

        // X360 GuiModule::Prepare @0x82518D68 STAGE 14: `if (!WorldDataController::Prepare(
        // guiModule + 307836, gameDataInputBuffer)) return 0;`. The controller is a GuiModule
        // member (Construct binds it into the cache) and its acquire machine talks to the
        // GAME DATA request queue, so it can only be pumped where that buffer pair is live.
        // FLAG PC drive point: the console's module scheduler hands GuiModule::Prepare the
        // GameData input/output pair; on PC the only place that pair exists and is pumped is
        // BrnGameModule::ResourceUpdateThread (the single-threaded stand-in for the console's
        // resource thread), which calls this immediately before GameDataModule::Update -- the
        // same "stage the request, then pump" order the console's Prepare has. Returns the
        // machine's own done flag.
        bool PrepareWorldData(BrnResource::GameDataIO::InputBuffer* lpGameDataInput);

        // ⭐ [event-starts wave 2026-08-27] X360 GuiModule::Prepare2 @0x825194B8's
        // WorldDataController leg -- WorldDataController::Prepare2 @0x82516CB8, the only writer of
        // mpProgressionData / mpStreetData. Same FLAG'd PC drive point as PrepareWorldData above.
        // ⛔ MUTUALLY EXCLUSIVE WITH PrepareWorldData: both machines drain the SAME receiver
        // queue. The body's banner spells out what overlapping them corrupts.
        bool PrepareWorldData2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput);

        // True once PrepareWorldData has reported done (or has parked on a request no PC
        // producer answers -- see the banner in BrnGuiWorldDataController.cpp). The driver
        // reads it only for its one-shot diagnostic.
        const WorldDataController& GetWorldDataController() const { return mWorldDataController; }

        // Is the SCREEN flow's live state currently subscribed to liEventId? The observer
        // table is the module's own record of the type-34/35 registration records each state
        // posts from OnEnter (RegisterForEvents), so this is the exact "a state is listening
        // for this event right now" signal -- and it goes false again when that state leaves.
        // Read by the game module's GameState->GUI car-select stand-in, which must not publish
        // into a queue nobody is draining. FLAG PC bring-up: on the console the producers are
        // driven by the game-state side, which never needs to ask.
        bool IsScreenFlowObserving(s32 liEventId) const
        {
            return liEventId >= 0 && liEventId < KI_MAX_OBSERVED_EVENT_ID
                && mabObservedEventIds[E_GUIFLOW_SCREEN][liEventId];
        }

    private:
        // Dispatch this sub-step's inbound GUI events (the real GuiModule::Update event
        // switch @0x82527A58: 144 -> RunFsm, 481 -> HandleHudStateLoadComplete + forward,
        // 14/16 -> the model-out notification queue, 504/508/513 -> MovieManager), and fan
        // every event the flow subscribed to (records 34/35) into the HUD flow's in-queue
        // -- the EventInterpreterModule observer dispatch.
        void DispatchInboundGuiEvents();

        // Drain one flow's StateInterface output queue -- the per-frame dispatch point
        // for everything its states post: subscription records (34/35, keyed into that
        // flow's observed-id table), movie play/stop (508/509), the channel-40 command
        // records (-> mGuiOutQueue for the game bridge), the channel-41 view-state
        // records (-> the view input queue), and the menu-music / audio-trigger events
        // (155/201, the PC sound leaves).
        void DrainFlowOutputQueue(s32 liFlow);

        // @0x824F0D30 -- refresh the two live player-name entries in the language
        // database ("PLAYER_NAME_STRING_ID" / "PLAYER_NAME_STRING_ID_Q"). The X360 asks
        // XUserGetName for the signed-in gamertag; when that FAILS it falls back to the
        // database's own "DEFAULTPLAYERNAME" / "DEFAULTPLAYERNAMEQUOTED" entries ("You" /
        // '"You"'). Triggered by GUI event 507 in GuiModule::Update's dispatch (the
        // record BrnGui::BootProfile::OnLeave posts when the profile boot completes).
        // (The console passes the view-state queue as a second argument; the body never
        // reads it, so it is dropped here.)
        void UpdatePlayerName();

        // The GUI resource-loading dispatch: hand the flow controller's FSM-bundle load
        // requests (GuiEventLoadRequest, queue type 39, out of mModelInputBuffer) to the
        // REAL CgsGui::GuiResourceModule, run it (its acquire machine + the [PC] platform
        // servicer that loads FSM\<NAME>.BUNDLE), and bridge its GuiEventLoadNotification
        // (14) records back into the ModelIO output buffer the controller reads. Replaces
        // the host stand-in ServiceFsmBundleRequests below (Phase 1 of retiring that path).
        void DispatchGuiResourceModule();

        // [PC IO] the ORIGINAL host FSM-bundle stand-in: drained the ModelIO input
        // buffer's load requests, loaded "FSM/<NAME>.BUNDLE" synchronously, and posted
        // the GuiEventLoadNotification (14) itself. SUPERSEDED by DispatchGuiResourceModule
        // (the real module now owns this); retained unused this phase (/OPT:REF strips it).
        void ServiceFsmBundleRequests();

        // Post one event into each live flow's in-queue if that flow subscribed to it
        // (the observer-subscription filter, per flow).
        void RouteEventToFlow(const CgsModule::Event* lpEvent, s32 liId, s32 liSize);

        // This sub-step's GUI module INPUT buffer (set by BrnGameModule each sub-step; the
        // buffer itself lives on the update IO stack and is re-created per sub-step).
        CgsGui::CgsGuiModuleIO::InputBuffer* mpGuiEventInputBuffer;

        // ARTIST's callback publishes GuiAudioTriggerEvent through GuiModule's current
        // output buffer. The PC scheduler exposes that channel as mGuiOutQueue, so retain
        // the same explicit producer pointer instead of bypassing the event route.
        CgsModule::VariableEventQueue<18432, 16>* mpOutputBuffer;

        ViewModule mViewModule;       // DecFIGS BrnGuiModule.h:441 (owns Apt/text/render state)

        // X360 +311952 -- the GUI CUSTOM-RENDERER SET manager. GuiModule::GuiModule
        // @0x827E5B28 constructs it by value and GuiModule::Prepare @0x82518D68 stage 7
        // does, in this order:
        //     v27 = (*(*(v3 + 311952) + 4))(v3 + 311952, rwGeneralRes, rwLinearRes);
        //     CgsGui::ViewModule::SetCustomRendererManager(v3 + 132224, v3 + 311952, 10,
        //                                                  v3 + 1629284);
        //     if (!v27) return 0;
        // -- i.e. Prepare the manager, install it into the view module (which mirrors it
        // into AptRenderHandler::mpCustomRendererManager, the pointer the Apt custom-control
        // callback reads), and only then gate on the prepare result.
        CustomRendererManager mCustomRendererManager;   // X360 +311952

        // The view-module IO pair the per-frame bridge fills (the input buffer carries the
        // view-state events -- frame time step 26, the play-movie events 18, the load
        // notifications 14 -- into CgsGui::ViewModule::Update). FLAG (bridge stand-in):
        // the console fills these through the module scheduler's IO stacks.
        CgsGui::ViewIO::InputBuffer  mViewInputBuffer;
        CgsGui::ViewIO::OutputBuffer mViewOutputBuffer;
        s64 miLastViewFrameMs;        // PC frame clock for the time-step event (FLAG: wall clock)
        MovieManager mMovieManager;   // X360 +301600 (drives the boot/attract videos)

        // X360 +306752 -- the full-screen colour/brightness calibration test card. DWARF
        // BrnGuiModule.h:506 places it between mMovieManager (h:504) and
        // mWorldDataController (h:509), which is exactly the order GuiModule::Construct
        // @0x82518B18-24 constructs them in (`MovieManager::Construct(gm+301600)` then
        // `ColourCalibrationScreen::Construct(gm+306752)`). Reached BY NAME here -- the
        // guest displacement is a note, not a layout instruction.
        ColourCalibrationScreen mColourCalibrationScreen;   // X360 +306752

        // X360 +311932 -- one row of the console's mGuiConfig (DWARF BrnGuiModule.h:516 ->
        // CgsGui::GuiModuleConfig, CgsGuiModule.h:73 `rw::IResourceAllocator*
        // mpTextureAllocator`). GuiModule::Prepare @0x82518DE0 fills it with
        // `GetAllocatorList(gameDataOut)->GetRWLinearResourceAllocator(42)` ("Network Image
        // Allocator") and GuiModule::Update @0x82529B00 hands it to
        // ColourCalibrationScreen::Update as r7. Only this one row of GuiModuleConfig is
        // modelled; the rest of the config block is the (unreconstructed) base-module
        // prepare's business.
        rw::IResourceAllocator* mpTextureAllocator;   // X360 +311932 (mGuiConfig.mpTextureAllocator)

        // ⭐ [licence-icon] X360 +311924 -- the sibling mGuiConfig row: Prepare stage 1 fills it
        // with GetAllocatorList(gameDataOut)->GetRWGeneralResourceAllocator(31) and the console
        // hands it to CustomRendererManager::Prepare as the heap allocator (`v26 = *(gm+311924)`
        // at the stage-7 call @0x82518D68). Modelled for the same one consumer.
        rw::IResourceAllocator* mpGuiHeapAllocator;   // X360 +311924 (mGuiConfig heap row, bank 31)

        // [licence-icon] the manager's staged-prepare latch. The console pumps the manager's
        // Prepare through GuiModule::Prepare's stage re-entry (`if (!v27) goto fail` + the module
        // scheduler re-calls Prepare until every stage passes); this build's Prepare runs once,
        // so Update owns the pump (see the seat note at the call).
        bool mbCustomRenderersPrepared;
        // (AptRuntimeHost RETIRED: the Apt bring-up + PC render buffer live in
        // BrnGuiModule.cpp's transplanted block -- the console GuiModule ownership.)
        AlwaysAvailableComponentsManager mAlwaysAvailableComponentsManager;

        // ---- the real flow-controller chain (X360 GuiModule members) --------------------
        // X360 +307836 -- the GUI-side world/progression/vehicle data front-end. Construct
        // Constructs it and binds it into the cache (SetWorldDataController); Prepare stage 14
        // runs its acquire machine. It MUST outlive every flow state that resolves a car.
        WorldDataController mWorldDataController;   // X360 +307836
        GuiCache          mGuiCache;        // X360 +1005376 (the flow states' cache; event-64 payload)
        // [H3b] the shared map-icon manager (X360 +1088304; ctor from GuiModule::GuiModule
        // @0x827E5D7C, Construct + GuiCache::SetMapIconManager from GuiModule::Construct).
        MapIconManager    mMapIconManager;

        // ⭐ [stuntrace] X360 +309584 -- THE GUI-SIDE FREEBURN-CHALLENGE TRACKER, and the
        // object GuiCache::mpChallengeManager points at. GuiModule::Construct @0x82518028
        // constructs it against the cache and binds it in (SetChallengeManager, the console's
        // `*(gm + 1021868) = gm + 309584`); GuiModule::Update @0x82527A58 ticks it once per
        // frame. It is the ONE owner in the image -- the manager is a plain by-value member of
        // this module, exactly like mWorldDataController/mMapIconManager above.
        //
        // Until this member existed, GuiCache::mpChallengeManager had ZERO writers and every
        // in-event HUD frame that touched a freeburn-challenge arm of RaceMainHudState fired
        // the "mpChallengeManager" assert (BrnGuiCache.h:2390) and then read off a null
        // pointer -- the user-blocking dialog on starting a stunt race.
        FreeburnChallengeManager mFreeburnChallengeManager;   // X360 +309584
        BrnScreenFlow     mScreenFlow;      // X360 mScreenFlow (SCREEN = E_GUIFLOW_SCREEN, the front-end)
        BrnHudFlow        mHudFlow;         // X360 +638904-adjacent flow set (HUD = E_GUIFLOW_HUD)
        BrnOverlayFlow    mOverlayFlow;     // X360 mOverlayFlow (OVERLAY = E_GUIFLOW_OVERLAY)
        GuiFsmController  mFsmController;   // X360 +638904 (the flow FSM controller)

        // ---- the HUD-message pair (gateui wave, round 2) --------------------------------
        // X360 GuiModule::Construct @0x82518028 lines 277-278 constructs them back to back
        // and in this order:
        //     HudMessageDirector::Construct(gm + 639264, gm + 552 /*ModelModule*/, gm + 1005376 /*GuiCache*/);
        //     HudMessageAnalyzer ::Construct(gm + 660992, gm + 639264 /*the director*/);
        // then line 376 publishes the director into the cache
        // (`*(gm + 1021876) = gm + 639264` == GuiCache::SetHudMessageDirector), and
        // Prepare @0x82518D68 stage 3 line 104 gives the analyzer the shared access-pointer
        // block (`*(gm + 660992) = gm + 1005332`).
        //
        // ⭐ THIS PAIR IS THE GUI END OF THE gateui CHAIN. Until it landed, the whole
        // HudMessageAnalyzer fan-out (25 partfiles) and the director's send path were
        // unreachable code: HudMessageAnalyzer::Update had no caller anywhere in the
        // program, so `[UI-gate] hud stunt-info ...` could never print no matter what the
        // GameState/bridge lanes did (round-1 verify WRONG-2).
        HudMessageDirector mHudMessageDirector;  // X360 +639264
        HudMessageAnalyzer mHudMessageAnalyzer;  // X360 +660992

        ProfileManager    mProfileManager;  // X360 +681696 (the REAL save/load manager)
        // ---- [profile-save 2026-08-27] the module's AUTOSAVE latch + throttle ------------
        // X360 GuiModule::Update tail @0x825295CC..0x825296A4:
        //   r30 = module + 0x18D844 (1628228) -- the autosave-PENDING byte, raised by the
        //     event walk's case 356 (GuiAutosaveRequestEvent), case 358 and case 43, cleared
        //     by the tail once the Autosave has been started;
        //   module + 0x18D840 (1628224) -- the f32 GuiCache::GetTime() stamp of the last
        //     autosave, the throttle's base (`lfsx f13,r26,r31` / `fadds f0,f13,flt_82004C6C`
        //     with flt_82004C6C == 60.0f).
        // GuiModule::AutosaveProfile @0x8251A568 and GuiDebugComponent::AutosaveProfile
        // @0x825238E0 write the same pair after their own Autosave calls.
        bool mbProfileAutosavePending;      // X360 +1628228
        f32  mfLastProfileAutosaveTime;     // X360 +1628224
        // The per-frame FORCE flag the console keeps in a stack byte across its single
        // Update (`v142`): case 356 ORs the request's own payload byte into it and case 505
        // (game paused) sets it outright, so a forced request bypasses the 60-second
        // throttle. This build splits the console's one Update into DispatchInboundGuiEvents
        // + Update, so the byte has to survive between them -- it is written by the event
        // walk and consumed (and cleared) by the tail in the same frame.
        bool mbForceProfileAutosave;        // X360 Update stack byte var_430 (v142)
        // The completion handler the autosave task reports to. On the console the argument is
        // the BrnGui::ProfileHost the module embeds at +681680 (the ProfileManager sits inside
        // it at +16), and ProfileHost::HandleProfileTaskResult @0x827E21E0 is a pure diagnostic
        // ("SaveLoad: Finished" under message-filter bit 0). This build embeds the
        // ProfileManager directly rather than through a ProfileHost, so the handler is a
        // module-owned object carrying that same body -- see BrnGuiProfileHost.cpp for the
        // console original. DELETE-WHEN the module embeds a real ProfileHost.
        class ProfileAutosaveResultHandler : public ProfileTaskResultHandler
        {
        public:
            virtual void HandleProfileTaskResult();
        };
        ProfileAutosaveResultHandler mProfileAutosaveResultHandler;
        CgsGui::SystemUserProfile mSystemUserProfile; // X360 +949152 (sign-in watcher the manager listens to)
        CgsMemory::HeapMalloc  mFsmLuaHeap;    // the FSM Lua VM heap (the controller's allocator)
        CgsMemory::HeapMalloc   mProfileHeap;   // the manager's heap (mugshot buffer + SLS callbacks)
        CgsMemory::LinearMalloc mProfileLinear; // the manager's linear (SLS Prepare)
        CgsMemory::LinearMalloc mHudStatePool; // the 14-state pool allocator (HudFlow::Prepare)
        CgsMemory::LinearMalloc mOverlayStatePool; // the 15-popup-state pool allocator (OverlayFlow::Prepare)
        CgsMemory::LinearMalloc mScreenStatePool;  // the 61-state pool allocator (ScreenFlow::Prepare)

        // The ModelIO pair the controller exchanges with the (module-dispatch) resource
        // loader: requests out through the input buffer, notifications back on the output
        // buffer. On the console these are per-frame stack buffers ("GUIModel"); the PC
        // module owns a persistent pair and clears the queues per frame as the real
        // Update does (Clear @ the update tail).
        CgsGui::ModelIO::InputBuffer  mModelInputBuffer;
        CgsGui::ModelIO::OutputBuffer mModelOutputBuffer;

        // The REAL GUI resource-loading module + its own per-frame IO pair. Wired in to
        // REPLACE the host FSM-bundle stand-in: the controller's GuiEventLoadRequest queue
        // (out of mModelInputBuffer) is handed to the module, which loads FSM\<NAME>.BUNDLE
        // through its acquire machine + the [PC] platform servicer and posts the
        // GuiEventLoadNotification (14) back -- DispatchGuiResourceModule bridges those into
        // mModelOutputBuffer for the controller. The console recreates the module IO
        // buffers each frame; the PC pair is persistent and cleared per frame.
        CgsGui::GuiResourceModule                 mGuiResourceModule;
        CgsGui::GuiResourceModuleIO::InputBuffer  mResourceInputBuffer;
        CgsGui::GuiResourceModuleIO::OutputBuffer mResourceOutputBuffer;

        // The per-flow in-queues (every observed event is fanned into the subscribing
        // flow's queue) + the module GUI-OUT queue the game bridge drains.
        CgsModule::VariableEventQueue<18432, 16> mScreenInQueue;
        CgsModule::VariableEventQueue<18432, 16> mHudInQueue;
        CgsModule::VariableEventQueue<18432, 16> mOverlayInQueue;
        CgsModule::VariableEventQueue<18432, 16> mGuiOutQueue;
        // The always-available components manager's in-queue: the subscription filter
        // (RouteEventToObserver) delivers the events the manager registered for (its real
        // 19-id table: save-icon 355, connect 64, showtime, ...) into it, then the module
        // pumps AlwaysAvailableComponentsManager::Update against it each frame -- the same
        // observer contract the console's EventInterpreterModule drives (deliver via
        // SetInEventQueue's queue, pump via UpdateObservers).
        CgsModule::VariableEventQueue<18432, 16> mAlwaysAvailInQueue;

        // The registered event OBSERVERS (the console EventInterpreterModule's observer
        // list): the three flows plus the always-available components manager -- the AAC
        // registers through its own StateInterface exactly like a flow state (its Prepare
        // posts the real 19-id table, ARTIST dword_8206F760, as type-34 records).
        static const s32 E_GUIOBSERVER_ALWAYSAVAILABLE = E_GUIFLOW_COUNT;
        static const s32 KI_NUM_EVENT_OBSERVERS        = E_GUIFLOW_COUNT + 1;

        // The observer-subscription tables (records 34/35 from each observer's state
        // interface output queue). One flag set per observer slot.
        static const s32 KI_MAX_OBSERVED_EVENT_ID = 1024;
        bool mabObservedEventIds[KI_NUM_EVENT_OBSERVERS][KI_MAX_OBSERVED_EVENT_ID];

        // EventInterpreterModule priority state. ARTIST permits ten priority keys
        // per observer; each key owns an override-event mask. A priority owner starts
        // blocking its override set after receiving its priority event and stops when
        // the state unregisters or posts the explicit stop-blocking record.
        static const s32 KI_MAX_PRIORITY_CLAIMS_PER_FLOW = 10;
        struct PriorityClaim
        {
            bool mbActive;
            s32  miEventType;
            bool mabOverriddenEventIds[KI_MAX_OBSERVED_EVENT_ID];
        };
        PriorityClaim maPriorityClaims[KI_NUM_EVENT_OBSERVERS][KI_MAX_PRIORITY_CLAIMS_PER_FLOW];
        bool          mabPriorityBlocking[KI_NUM_EVENT_OBSERVERS];

        // One resident pool per flow slot for the loaded FSM LuaCode bundle (request ids
        // 13/14/15 = SCREEN/HUD/OVERLAY; each flow's ScriptedFsm holds its LuaCode
        // resource while live, so the pools must not alias across flows).
        CgsResource::Pool mFsmBundlePool[E_GUIFLOW_COUNT];
        bool              mbResourcesReadyFed;   // fed BF_LEGAL its resources-ready (567) yet
        // The Prepare latch behind IsPrepared() (see the accessor's note). The console has no
        // such member because its stage word IS the latch -- CgsGui::GuiModule::Prepare
        // @0x82518D68 is a 16-stage resumable ladder whose gated stages return 0 until the
        // stage word reaches DONE (boot audit F-P8a-1). This build's Prepare is still the
        // one-shot synchronous collapse of that ladder, so a bool is the whole of the state;
        // it becomes the stage word when the ladder itself is adopted.
        bool              mbPrepared;
    };

    // Renderer bridge, matching gpActiveMovieManager: GuiModule publishes itself while
    // prepared; BrnRendererModule drives GuiModule::Render (the GUI render chain) through it.
    extern GuiModule* gpActiveGuiModule;

}

#endif
