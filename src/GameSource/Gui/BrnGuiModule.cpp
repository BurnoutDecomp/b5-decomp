#include "GameSource/Gui/BrnGuiModule.h"

#include <cstdio>                                                         // std::snprintf (log formatting)
#include <chrono>   // the PC frame clock for the view time-step event (FLAG: wall clock)
#include <cstring>  // std::strcmp (ARTIST GUI-audio action-name table)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"// CgsResource::BundleLoader ([PC IO] FSM loads)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> (channel command records)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::GuiEventPlayAptMovie (channel-41 payload)
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"   // priority removal/blocking event ids
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification / GuiEventLoadRequest
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiAudioTriggerEvent
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"        // AlwaysAvailableComponentsManager + free accessor (bodied below)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (flow interface wiring)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"       // CgsGui::AptAuxPointer (the AptAux singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::AptCommunicator (the per-frame trigger publish)
#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"             // CgsSystem::MenuMusicPC (the menu-stream music player)
#include "GameShared/GameClasses/System/PC/CgsGuiSoundPC.h"               // CgsSystem::GuiSoundPC (the GUI presentation blips)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash (event-155 keys)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // FLApt live-instance allocator

// DecFIGS types GuiModule::Construct's alternate-text palette as const RGBA*.
// ARTIST's eight packed words at 0x82F27F84 are the complete table.
struct RGBA
{
    u32 mPacked;
};

// ============================================================================
// BrnGui::GuiModule -- the GUI module. X360 GuiModule::Construct (0x82518028) builds the
// whole GUI subsystem; Update (0x82527A58) dispatches the inbound GUI events, drives the
// GuiFsmController + the flows, the MovieManager and the view chain. This PC module
// reconstructs that spine with the REAL controller chain:
//
//   BridgeGameToGui (game side) posts GuiEventRunFsm (event 144)
//     -> DispatchInboundGuiEvents -> GuiFsmController::RunFsm
//     -> the controller's load machine posts a GuiEventLoadRequest into the ModelIO input
//     -> ServiceFsmBundleRequests ([PC IO] GuiResourceModule stand-in) loads
//        FSM/<NAME>.BUNDLE and posts the GuiEventLoadNotification (14) back
//     -> GuiFsmController::Update -> BrnBaseFlow::PrepareLua -> the script SetState()s the
//        matching CgsGui::State in BrnHudFlow's 14-state pool
//   The states run under mHudFlow.Update(); everything they post drains through
//   DrainFlowOutputQueue (subscriptions 34/35, movie 508/509, view-state 41, GUI-out 40,
//   music 155 / triggers 201); each boot state posts command 70 at phase end, which the
//   game module's BridgeGuiToGame consumes to advance the game main flow.
// ============================================================================

// The loading-screen visual signal (BrnRendererModule::Render shows the loading screen
// while it's set). Driven by the REAL protocol now: the states post the loading-screen
// commands 19/20 on the GUI-out channel and the game module's BridgeGuiToGame writes this
// signal (the console's dispatch-buffer render-state equivalent).
extern bool gBrnLoadingScreenShouldShow;   // defined in BrnGameMainFlowStates.cpp
extern bool gBrnInitialLoadingComplete;    // set by the game-flow when the load stages finish
extern bool gBrnGuiDrivesLoadingScreen;    // we set this while the HUD flow FSM is live

namespace
{
    // Backing for the per-flow FSM bundle pools (3 mem types each; the boot FSM scripts
    // are tiny single-state bundles ~1.5 KB -- BRNSCREENFSM is the largest at ~68 KB --
    // and each pool reserves 64 KB for its own management structures). One backing per
    // flow slot: each flow's ScriptedFsm holds its LuaCode resource while live.
    const u32 KU_FSM_POOL_BYTES = 256u * 1024u;
    u8 s_fsmPoolBacking[BrnGui::E_GUIFLOW_COUNT][CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_POOL_BYTES];
    u8 s_fsmLuaHeapBuffer[512u * 1024u];

    // Backing for the HUD flow's 14-state pool (BrnHudFlow::Prepare carves the state
    // objects out of this linear region; BootLegal is the largest at a few KB).
    u8 s_hudStatePoolBacking[512u * 1024u];

    // Backing for the overlay flow's 15-popup-state pool (X360 sizes 0x40/0x50/13x0x148;
    // sized generously for the x64 member inflation).
    u8 s_overlayStatePoolBacking[256u * 1024u];

    // Backing for the SCREEN flow's 61-state pool (X360 total ~638 KB with 4-byte
    // pointers; the big real states -- ON_GAME_ROOM 86 KB, ON_CUST_MAT 57 KB -- widen
    // on x64, so the region carries 2x headroom).
    u8 s_screenStatePoolBacking[2u * 1024u * 1024u];

    // The view's FLApt timeline tree is one linear allocation, reset when the GUI
    // module is rebuilt. ARTIST receives the GUI module's LinearAllocator here;
    // the PC owner supplies an equivalent dedicated region.
    alignas(16) u8 s_flaptLinearBacking[16u * 1024u * 1024u];
    CgsMemory::LinearMalloc s_flaptLinear;

    // Backing for the profile manager's allocators (FLAG PC stand-in: the console hands
    // the 0x26 game-data heap + a module linear; the PC module owns dedicated regions).
    // The heap carves the 3x9608 mugshot circular buffer + the SLS callback block; the
    // LINEAR carves the save/load system's mugshot image buffer
    // (miExtraFilesSizeBytes 9600 * mugshotsPerType 20 * types 5 == 960000 bytes) + the
    // content-info file buffer -- so it must clear ~1 MB (the 64 KB it had returned null
    // from SaveLoadSystem::Prepare's Malloc and tripped the mpMugshotBufferData assert).
    u8 s_profileHeapBacking[192u * 1024u];
    u8 s_profileLinearBacking[2u * 1024u * 1024u];

    // FLAG PC-platform leaf: the live progression + live-revenge profile blocks the
    // console's progression/network modules install on the ProfileManager via GuiModule
    // events (SetProgressionProfile / SetLiveRevengeProfile, event 351). Those subsystems
    // are not wired on PC, so the manager's mpProgressionProfile/mpProgressionData/
    // mpLiveRevengeProfile stay null and ProfileManager::Bootup->ReadProfileData faults
    // (memcpy from mpLiveRevengeProfile; ValidateProfiles derefs mpProgressionData as the
    // ExpectedManifest). These zeroed stand-ins are a blank first-boot profile -- exactly
    // what the console holds before any save loads -- installed in Prepare below. Sized to
    // the real segment widths (BrnGuiProfile.h): live-revenge is memcpy'd 30016 B, the
    // manifest is dereferenced by value, the progression profile is only read by the
    // (FLAG'd no-op) serialiser.
    alignas(16) u8 s_pcProgressionProfileBacking[118064];   // KI_PROGRESSION_PROFILE_SIZE_BYTES
    alignas(16) u8 s_pcProgressionManifestBacking[4096];    // ExpectedManifest (generous)
    alignas(16) u8 s_pcLiveRevengeProfileBacking[30016];    // KI_LIVEREVENGE_PROFILE_SIZE_BYTES

    // The shared access-pointer bundle the HUD flow's state interface hands its GUI
    // components (Prepare'd in GuiModule::Prepare once the Apt bring-up publishes the
    // AptAux singleton). The console's view module owns the equivalent module-shared
    // GuiAccessPointers instance.
    CgsGui::GuiAccessPointers s_GuiAccessPointers;

    // FLAG PC stand-in: the real CgsGui::ModelModule (the GUI model dispatcher) is not
    // yet instantiable on PC (its reconstructed ctor initialises X360 byte offsets over
    // an unmodelled ~0x18000-byte layout). GuiFsmController::Prepare only STORES and
    // null-checks the pointer (no dereference on any reconstructed path), so a sentinel
    // non-null stands in until the model module lands.
    u8 s_ModelModuleSentinel;

    // The subscription record the states post through StateInterface::RegisterForEvents
    // (X360 wire records 34/35: { s32 miEventType; EventObserver* } -- only the leading
    // event-type word matters to the dispatch table; the pointer is the posting observer).
    struct RegisterEventRecord
    {
        s32 miEventType;
    };

    // StateInterface::PriorityRegisterForEvent's ARTIST wire prefix. The observer
    // pointer follows at +0x968 on PPC; GuiModule already knows the posting flow, so
    // the native pointer is deliberately not part of this parser.
    struct PriorityRegisterRecordPrefix
    {
        s32 miEventType;
        s32 maiEventTypeOverridden[600];
        u32 muOverrideCount;
    };

    // The event-64 record: the module posts the GuiCache pointer each frame (the X360
    // AddEvent(&cachePtr, 64, ptr-size) in GuiModule::Update -- the states' "cache" feed).
    struct GuiEventCache : public CgsModule::Event
    {
        BrnGui::GuiCache* mpGuiCache;
    };
}

namespace BrnGui
{
    AptRuntimeHost* gpActiveAptRuntimeHost = 0;
    GuiModule*      gpActiveGuiModule      = 0;

    // The current menu-music stream hash (X360 dword_830082A8; 0 == silence). The
    // menu-music consumer below keeps it current; the post-title intro reads it.
    s32 gCurrentMenuMusicHash = 0;

    // ---- BF_LEGAL-era audio consumers (events 155 / 201; PC sound leaves) -------------
    // The console consumers are BrnSound::Logic::MusicStream (the menu stream, fed through
    // SndStream) and the AEMS GUI sound logic (the trigger patches) -- both deferred
    // behavioural clusters. These PC leaves reproduce the OBSERVABLES on the same event
    // protocol:
    //   155 (GuiEventPlayMusicOnMenuStream): miHash @+0x0C. A known sound-name hash
    //        (CgsSound::Playback::Name::MakeHash -- homed) -> play/loop that stream;
    //        hash 0 -> stop (the X360 posts 0 before the attract video).
    //   201 (GuiAudioTriggerEvent): resolved through the presentationactionlist data to a
    //        splice in the presentation Splicer bank (CgsGuiSoundPC).
    static void HandleMenuMusicEvent(s32 liHash)
    {
        // Event name -> ContentSpec name. FLAG (the MusicEffect data layer): the
        // console maps the posted event name to a StreamsRegistry ContentSpec via
        // the music database (MusicEffect::GetEventStartContentSpec @0x8269CFC0
        // reads it from game data); that table is not reconstructed, so the one
        // title-screen pairing is carried here. The SPEC then resolves through
        // the real registry chain (CgsSystem::StreamHeadersPC) -- the .SNS file
        // and its SNR header both come from the ORIGINAL X360 bundles.
        struct MenuStreamKey { const char* lpacName; const char* lpacSpecName; };
        static const MenuStreamKey KA_MENU_STREAMS[] =
        {
            // The title screen's menu stream (BootLegal E_STAGE_START_MOVIE posts it).
            { "GunsAndRoses", "Guns_And_Roses" },
        };

        gCurrentMenuMusicHash = liHash;
        if (liHash == 0)
        {
            if (CgsSystem::MenuMusicPC::IsActive())
            {
                CgsDev::Log::WriteToLog("[GuiModule] menu-music 155 hash 0 -> stop.\n");
                CgsSystem::MenuMusicPC::Stop();
            }
            return;
        }
        for (u32 lu = 0; lu < sizeof(KA_MENU_STREAMS) / sizeof(KA_MENU_STREAMS[0]); ++lu)
        {
            const s32 liKey = static_cast<s32>(
                CgsSound::Playback::Name::MakeHash(KA_MENU_STREAMS[lu].lpacName));
            if (liHash == liKey)
            {
                char lac[160];
                std::snprintf(lac, sizeof(lac), "[GuiModule] menu-music 155 '%s' -> spec '%s'\n",
                              KA_MENU_STREAMS[lu].lpacName, KA_MENU_STREAMS[lu].lpacSpecName);
                CgsDev::Log::WriteToLog(lac);
                CgsSystem::MenuMusicPC::PlaySpec(KA_MENU_STREAMS[lu].lpacSpecName);
                return;
            }
        }
        {
            char lac[120];
            std::snprintf(lac, sizeof(lac),
                          "[GuiModule] menu-music 155 hash 0x%08X unknown -- no stream mapped (FLAG).\n",
                          static_cast<u32>(liHash));
            CgsDev::Log::WriteToLog(lac);
        }
    }

    // The exact eight packed colours passed to ViewModule::Construct by ARTIST
    // (0x82F27F84, count 8).
    const RGBA KA_ALTERNATE_TEXT_COLOURS[8] =
    {
        { 0xFF000000u },
        { 0xFF00CCFFu },
        { 0xFFFFFFFFu },
        { 0xFF2864B7u },
        { 0xFFA68C4Au },
        { 0xFF0F0F9Cu },
        { 0xFF6B8A57u },
        { 0xFF33B6E6u },
    };

    // X360 GuiModule::Construct (0x82518028) builds the whole GUI subsystem. This slice
    // constructs the view module, the movie manager, and the real flow-controller chain
    // (cache + HUD flow + FSM controller).
    void GuiModule::Construct()
    {
        // Route through the real BrnGui::ViewModule::Construct @0x824F13B8 with the X360
        // caller's recovered args: the view flapt count (7), a 16:9 aspect, and the real
        // alternate-text-colours table + count 8 (see the static above). The
        // X360 passes a null debug name here; the descriptive "BrnGuiView" is a harmless
        // non-null label the base accepts.
        mViewModule.Construct(this, "BrnGuiView", 7, 1280.0f / 720.0f,
                              KA_ALTERNATE_TEXT_COLOURS, 8);
        mViewModule.GetFlaptManager()->SetSoundTriggerHandler(
            &GuiModule::FlaptSoundTriggerCallback, this);
        mMovieManager.Construct();
        mAlwaysAvailableComponentsManager.Construct();
        mpGuiEventInputBuffer = 0;
        mpOutputBuffer = 0;

        // The flow-controller chain (the X360 Construct's flow set): the cache Construct
        // (the watcher reset @0x82505860 -> 0x824FD978), then the profile manager (X360
        // GuiModule::Construct @0x82518028 hands it the cache, the sign-in watcher, and
        // the view module's language manager), then the flows against the cache; the
        // controller starts UNLOADED on every flow slot.
        mGuiCache.Construct();

        // X360 GuiModule::Construct wires the shared state-access bundle here, before
        // any flow is allowed to run: ViewModule::GetAptAux/GetLanguageManager,
        // ViewModule::GetFlaptManager, and this GuiCache are the four live owners.
        // AptAux itself is installed during Prepare below because the PC runtime host
        // constructs that singleton there; the other three owners already exist.
        s_GuiAccessPointers.Construct();
        s_GuiAccessPointers.mpLanguageManager = mViewModule.GetLanguageManager();
        s_GuiAccessPointers.SetFlaptManager(mViewModule.GetFlaptManager());
        s_GuiAccessPointers.SetGuiCache(&mGuiCache);

        mProfileManager.Construct(mGuiCache, mSystemUserProfile,
                                  mViewModule.GetLanguageManager());
        mScreenFlow.Construct(&mGuiCache);
        mHudFlow.Construct(&mGuiCache);
        mOverlayFlow.Construct(&mGuiCache);
        mFsmController.Construct();

        // The REAL GUI resource-loading module + its persistent IO pair (replaces the
        // host FSM-bundle stand-in). Construct the IO buffers (their embedded queues come
        // up here) and the module. HighDef == true: matches the HD apt/flapt path the
        // boot uses (the FSM bundle path itself is HD-independent). Construct seeds the
        // module counters/stages + marks it a new-module type (its base Prepare then skips
        // the old-module IO-structure lock path -- no assert).
        mResourceInputBuffer.Construct();
        mResourceOutputBuffer.Construct();
        mGuiResourceModule.Construct(true);

        for (s32 lf = 0; lf < KI_NUM_EVENT_OBSERVERS; ++lf)
        {
            for (s32 li = 0; li < KI_MAX_OBSERVED_EVENT_ID; ++li)
                mabObservedEventIds[lf][li] = false;
            mabPriorityBlocking[lf] = false;
            for (s32 lc = 0; lc < KI_MAX_PRIORITY_CLAIMS_PER_FLOW; ++lc)
            {
                PriorityClaim& lrClaim = maPriorityClaims[lf][lc];
                lrClaim.mbActive   = false;
                lrClaim.miEventType = -1;
                for (s32 li = 0; li < KI_MAX_OBSERVED_EVENT_ID; ++li)
                    lrClaim.mabOverriddenEventIds[li] = false;
            }
        }
        mbResourcesReadyFed = false;
    }

    bool GuiModule::Prepare()
    {
        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and
        // publish the manager so the renderer draws the active movie each frame (interim render
        // bridge; the X360 renders it through the GUI's own ViewIO ImRenderers).
        mMovieManager.Prepare(0);
        gpActiveMovieManager = &mMovieManager;
        gpActiveAptRuntimeHost = &mAptRuntimeHost;
        gpActiveGuiModule = this;

        // The FSM Lua VM heap (the controller's allocator) + the HUD state pool.
        mFsmLuaHeap.Construct(s_fsmLuaHeapBuffer, static_cast<s32>(sizeof(s_fsmLuaHeapBuffer)));
        mHudStatePool.Construct();
        mHudStatePool.Create(s_hudStatePoolBacking, sizeof(s_hudStatePoolBacking));

        // The FLApt linear allocator must exist BEFORE the view-module prepare: the
        // staged BrnGui::ViewModule::Prepare's FLAPT stage seeds every
        // FlaptFileInstance from it (a null here leaves null instance allocators --
        // the state machine one-shots at DONE and never re-seeds).
        s_flaptLinear.Construct();
        s_flaptLinear.Create(s_flaptLinearBacking, sizeof(s_flaptLinearBacking));
        s_flaptLinear.SetAlignment(16);

        // Stand up the GUI-owned Apt runtime host (allocator + interpreter + AptAux host
        // callback table + the render buffer) BEFORE the flow prepares, so the flow
        // states' access pointers can reach the AptAux singleton. Idempotent + defensive.
        // The host drives the REAL staged (virtual) ViewModule::Prepare, whose FLAPT
        // stage prepares the FlaptManager with this linear -- the console prepare shape
        // (the separate FlaptManager::Prepare call is retired with it).
        mAptRuntimeHost.Prepare(&mViewModule, &s_flaptLinear);

        // Complete the shared access bundle with the AptAux singleton created by the
        // PC runtime host above.  Construct already installed the language, Flapt, and
        // GuiCache owners exactly as the X360 GuiModule::Construct does.
        s_GuiAccessPointers.mpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;

        // The REAL flow bring-up: base prepare (access pointers into the StateInterface)
        // + the 14-state pool carve, then the flow's single in-queue. FLAG (allocator):
        // the rw resource allocator the console threads through EventObserver::Prepare is
        // null until the GUI resource slice lands (no reconstructed state dereferences it
        // on the boot path). FLAG (ProfileManager): un-reconstructed; BF_PROFILE's
        // manager-gated calls are boundary no-ops (see BrnBootProfile.cpp).
        // The profile manager's Prepare precedes the flows': it attaches the sign-in
        // listener, prepares the embedded save/load system, and carves the mugshot
        // circular buffer (X360 hands the 0x26 game-data heap + a module linear; the
        // PC module owns dedicated backing regions -- see the FLAG at the statics).
        mSystemUserProfile.Prepare();   // X360: CGS_ASSERT'd @BrnGuiModule.cpp:519
        mProfileHeap.Construct(s_profileHeapBacking, static_cast<s32>(sizeof(s_profileHeapBacking)));
        mProfileLinear.Construct();
        mProfileLinear.Create(s_profileLinearBacking, sizeof(s_profileLinearBacking));
        mProfileManager.Prepare(&mProfileHeap, &mProfileLinear);

        // Install the PC-boundary blank profile blocks (see the statics above): the
        // console's progression + network modules do this via SetProgressionProfile /
        // SetLiveRevengeProfile; without them Bootup->ReadProfileData faults on the null
        // pointers. std::memset zeroes them (a fresh, unsaved profile).
        std::memset(s_pcProgressionProfileBacking, 0, sizeof(s_pcProgressionProfileBacking));
        std::memset(s_pcProgressionManifestBacking, 0, sizeof(s_pcProgressionManifestBacking));
        std::memset(s_pcLiveRevengeProfileBacking, 0, sizeof(s_pcLiveRevengeProfileBacking));
        mProfileManager.SetProgressionProfile(
            reinterpret_cast<BrnProgression::Profile*>(s_pcProgressionProfileBacking),
            reinterpret_cast<const BrnProgression::ProgressionData*>(s_pcProgressionManifestBacking));
        mProfileManager.SetLiveRevengeProfile(
            reinterpret_cast<BrnNetwork::LiveRevengeProfile*>(s_pcLiveRevengeProfileBacking));

        mHudFlow.Prepare(&s_GuiAccessPointers, /*lpAllocator*/ 0, &mHudStatePool,
                         &mProfileManager);
        mHudInQueue.Construct();
        mHudFlow.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mHudInQueue));

        // The overlay flow: the 15-popup-state pool + its own in-queue (the X360 module
        // prepares all three flows here).
        mOverlayStatePool.Construct();
        mOverlayStatePool.Create(s_overlayStatePoolBacking, sizeof(s_overlayStatePoolBacking));
        mOverlayFlow.Prepare(&s_GuiAccessPointers, /*lpAllocator*/ 0, &mOverlayStatePool);
        mOverlayInQueue.Construct();
        mOverlayFlow.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mOverlayInQueue));

        // The SCREEN flow: the 61-state front-end pool + its own in-queue. The X360
        // Prepare threads the module's ProfileManager through BY REFERENCE (the CN_PROFILE
        // state's Construct consumes it; the manager is a shell until reconstructed).
        mScreenStatePool.Construct();
        mScreenStatePool.Create(s_screenStatePoolBacking, sizeof(s_screenStatePoolBacking));
        mScreenFlow.Prepare(&s_GuiAccessPointers, /*lpAllocator*/ 0, &mScreenStatePool,
                            mProfileManager);
        mScreenInQueue.Construct();
        mScreenFlow.SetInEventQueue(reinterpret_cast<InputBuffer::GuiEventQueue*>(&mScreenInQueue));

        // The always-available components manager (save-icon spinner, EATrax/achievement/
        // showtime overlays): give it its own in-queue and latch it. The manager's Prepare
        // state machine + per-frame Update pump run from GuiModule::Update (matching the
        // console's GuiModule::Update @0x82527A58, which calls the manager's Prepare each
        // frame). PrepareFlapt (binding SaveIcon_mc etc.) is driven by the flapt-load
        // notification in ViewModule::ProcessIncomingLoadNotification.
        mAlwaysAvailInQueue.Construct();
        mAlwaysAvailableComponentsManager.SetInEventQueue(&mAlwaysAvailInQueue);

        // The controller: store the model-module pointer + the FSM allocator, then
        // register the three flow slots.
        mFsmController.Prepare(
            reinterpret_cast<CgsGui::ModelModule*>(&s_ModelModuleSentinel), &mFsmLuaHeap);
        mFsmController.AddFlow(E_GUIFLOW_SCREEN, &mScreenFlow);
        mFsmController.AddFlow(E_GUIFLOW_HUD, &mHudFlow);
        mFsmController.AddFlow(E_GUIFLOW_OVERLAY, &mOverlayFlow);

        // The ModelIO pair the controller exchanges with the loader: construct the
        // IOBuffer bases (the eStatusConstructed guard the lock methods assert on) and
        // the queues this module uses (the input event/request queues + the output
        // notifications).
        mModelInputBuffer.Construct();
        mModelOutputBuffer.Construct();
        mModelInputBuffer.LockForWrite();
        mModelInputBuffer.GetEventQueueNonConst()->Construct();
        mModelInputBuffer.GetLoadRequests()->Construct();
        mModelInputBuffer.UnlockForWrite();
        mModelOutputBuffer.LockForWrite();
        mModelOutputBuffer.GetLoadNotificationsNonConst()->Construct();
        mModelOutputBuffer.UnlockForWrite();

        // Prepare the GUI resource module with seven bank/pool ids (member order:
        // aptPersistent, aptStreamed, font, FSM, language, textures, globalTexture). On
        // the console these are the resource system's real bank handles the module routes
        // each request type to; on PC they are opaque routing tags -- the module only
        // COMPARES them, and the [PC] platform servicer materialises just the FSM bank
        // (id 4) while completing the other banks' requests without IO. They must be
        // DISTINCT so the FSM bank is uniquely identified against the START-stage
        // PERSISTENTAPT (bank 1) / GUITEXTURES.BIN (bank 6) loads.
        mGuiResourceModule.Prepare(/*aptPersistent*/ 1, /*aptStreamed*/ 2, /*font*/ 3,
                                   /*FSM*/ 4, /*language*/ 5, /*textures*/ 6,
                                   /*globalTexture*/ 7);

        mGuiOutQueue.Construct();
        mpOutputBuffer = &mGuiOutQueue;

        // The view-module IO pair the per-frame bridge fills.
        mViewInputBuffer.Construct();
        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.GetViewStateQueue()
            .CgsModule::VariableEventQueue<65536, 16>::Construct();
        mViewInputBuffer.UnlockForWrite();
        mViewOutputBuffer.Construct();
        miLastViewFrameMs = -1;

        // GuiModule::Prepare stage 13 FIRST blocks on the locale's font table -- the
        // ARTIST western-SKU set is exactly {17,16}, {18,16}, {19,16}
        // (WesternB5Header_70 / WesternB5Body_35 / WesternB5DotMat_35 as
        // E_FONT_RESOURCETYPE_FONTDATA). Drive it through the SAME cache/module pump
        // the second table below uses: the module's container path loads each
        // "Language\Fonts\<name>.font" bundle into the font bank and the notification
        // sweep emits the type-16 records; the view registers each font
        // (ProcessIncomingLoadNotification case 16 -> AddFont) BEFORE the second
        // table is allowed to instantiate FLAPTHUD's text fields -- the original
        // fonts-before-FLApt ordering, by the console's own mechanism.
        // (The language notification still rides the host bring-up's queue; it is
        // drained after the font pump, ahead of the same view Update.)
        {
            const CgsGui::sResourceTuple kaFontResources[3] =
            {
                { 17u, static_cast<CgsGui::ResourceRequestTypes>(16) },
                { 18u, static_cast<CgsGui::ResourceRequestTypes>(16) },
                { 19u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            };

            bool lbFontsReady = mGuiCache.EnsureResourcesAreLoaded(kaFontResources, 3);
            for (u32 luPass = 0; luPass < 64u && !lbFontsReady; ++luPass)
            {
                mModelInputBuffer.LockForWrite();
                mGuiCache.Update(&mModelInputBuffer);
                mModelInputBuffer.UnlockForWrite();

                DispatchGuiResourceModule();

                mModelOutputBuffer.LockForRead();
                {
                    const CgsGui::ModelIO::OutputBuffer::GuiNotificationQueue* lpNotifications =
                        mModelOutputBuffer.GetLoadNotifications();
                    const CgsModule::Event* lpNotification = 0;
                    s32 liNotificationSize = 0;
                    s32 liNotificationId =
                        lpNotifications->GetFirstEvent(&lpNotification, &liNotificationSize);
                    while (liNotificationId >= 0 && lpNotification != 0)
                    {
                        if (liNotificationId == 14 || liNotificationId == 16)
                        {
                            mGuiCache.RecEvent(lpNotification, liNotificationId);
                            // Bridge the font notification to the view (event 14) so
                            // ProcessIncomingLoadNotification collects it (AddFont).
                            if (liNotificationId == 14)
                            {
                                mViewInputBuffer.LockForWrite();
                                mViewInputBuffer.GetViewStateQueue()
                                    .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                                        lpNotification, 14, liNotificationSize);
                                mViewInputBuffer.UnlockForWrite();
                            }
                        }

                        const CgsModule::Event* lpNext = 0;
                        liNotificationId = lpNotifications->GetNextEvent(
                            lpNotification, &lpNext, &liNotificationSize);
                        lpNotification = lpNext;
                    }
                }
                mModelOutputBuffer.UnlockForRead();

                mModelOutputBuffer.LockForWrite();
                mModelOutputBuffer.GetLoadNotificationsNonConst()->Clear();
                mModelOutputBuffer.UnlockForWrite();

                lbFontsReady = mGuiCache.EnsureResourcesAreLoaded(kaFontResources, 3);
            }
            CGS_ASSERT(lbFontsReady, "GUI locale fonts failed to load");
        }

        // The host bring-up's remaining queued notification (the LANGUAGE string
        // table) drains here, ahead of the same view Update.
        mViewInputBuffer.LockForWrite();
        {
            CgsGui::GuiEventLoadNotification lNotification;
            while (mAptRuntimeHost.PopPendingLoadNotification(&lNotification))
            {
                mViewInputBuffer.GetViewStateQueue()
                    .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNotification), 14,
                        static_cast<s32>(sizeof(lNotification)));
            }
        }
        mViewInputBuffer.UnlockForWrite();
        mViewModule.Update(0, 0, &mViewInputBuffer, &mViewOutputBuffer);
        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.GetViewStateQueue()
            .CgsModule::VariableEventQueue<65536, 16>::Clear();
        mViewInputBuffer.UnlockForWrite();

        // The second ARTIST stage-13 table is this exact resource pair:
        // resource 125 "main" (persistent Apt, type 7) and resource 196
        // "FLAPTHUD" (persistent FLApt, type 10). The PC resource transport is
        // synchronous, so advance the same cache/module state machines here until
        // both completion notifications have arrived, then let the view consume them
        // before any flow state can enter InvisibleOverlayState.
        const CgsGui::sResourceTuple kaStartupResources[2] =
        {
            { 125u, static_cast<CgsGui::ResourceRequestTypes>(7) },
            { 196u, static_cast<CgsGui::ResourceRequestTypes>(10) },
        };

        bool lbStartupResourcesReady =
            mGuiCache.EnsureResourcesAreLoaded(kaStartupResources, 2);
        for (u32 luPass = 0; luPass < 64u && !lbStartupResourcesReady; ++luPass)
        {
            mModelInputBuffer.LockForWrite();
            mGuiCache.Update(&mModelInputBuffer);
            mModelInputBuffer.UnlockForWrite();

            DispatchGuiResourceModule();

            mModelOutputBuffer.LockForRead();
            {
                const CgsGui::ModelIO::OutputBuffer::GuiNotificationQueue* lpNotifications =
                    mModelOutputBuffer.GetLoadNotifications();
                const CgsModule::Event* lpNotification = 0;
                s32 liNotificationSize = 0;
                s32 liNotificationId =
                    lpNotifications->GetFirstEvent(&lpNotification, &liNotificationSize);
                while (liNotificationId >= 0 && lpNotification != 0)
                {
                    if (liNotificationId == 14 || liNotificationId == 16)
                        mGuiCache.RecEvent(lpNotification, liNotificationId);

                    const CgsModule::Event* lpNext = 0;
                    liNotificationId = lpNotifications->GetNextEvent(
                        lpNotification, &lpNext, &liNotificationSize);
                    lpNotification = lpNext;
                }
            }
            mModelOutputBuffer.UnlockForRead();

            mModelOutputBuffer.LockForWrite();
            mModelOutputBuffer.GetLoadNotificationsNonConst()->Clear();
            mModelOutputBuffer.UnlockForWrite();

            lbStartupResourcesReady =
                mGuiCache.EnsureResourcesAreLoaded(kaStartupResources, 2);
        }
        CGS_ASSERT(lbStartupResourcesReady,
                   "GUI startup resources main/FLAPTHUD failed to load");

        // Both type-7 and type-10 completion records were bridged in load order.
        // Processing the queue registers main with Apt and FLAPTHUD with FlaptManager.
        mViewModule.Update(0, 0, &mViewInputBuffer, &mViewOutputBuffer);
        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.GetViewStateQueue()
            .CgsModule::VariableEventQueue<65536, 16>::Clear();
        mViewInputBuffer.UnlockForWrite();

        // The HUD flow FSM chain is live: the GUI owns the loading-screen visual through
        // the real 19/20 command protocol (BridgeGuiToGame consumes them).
        gBrnGuiDrivesLoadingScreen = true;

        CgsDev::Log::WriteToLog(
            "[GuiModule] flow controller live (HUD flow registered; awaiting GuiEventRunFsm).\n");
        return true;
    }

    bool GuiModule::Release()
    {
        mProfileManager.Release();   // detach the sign-in listener + release the SLS
        mScreenFlow.Release();       // staged: current state OnLeave + ScriptedFsm release
        mHudFlow.Release();
        mOverlayFlow.Release();
        mFsmLuaHeap.Destruct();
        if (gpActiveAptRuntimeHost == &mAptRuntimeHost)
            gpActiveAptRuntimeHost = 0;
        if (gpActiveGuiModule == this)
            gpActiveGuiModule = 0;
        gpActiveMovieManager = 0;
        mMovieManager.Release();
        return true;
    }

    void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
    }

    // Post one event into each subscribing observer's in-queue (the EventInterpreterModule
    // observer-subscription filter the console applies in ProcessInEvents before handing
    // an observer its per-frame queue). The observer slots are the three flows plus the
    // always-available components manager.
    void GuiModule::RouteEventToFlow(const CgsModule::Event* lpEvent, s32 liId, s32 liSize)
    {
        if (liId < 0 || liId >= KI_MAX_OBSERVED_EVENT_ID)
            return;

        // IsPriorityEvent + IsEventBlocked from ARTIST's EventInterpreterModule.
        // The first registered priority key owns the event; a blocking owner removes
        // its override events from every other observer until it unregisters.
        s32 liPriorityOwner = -1;
        s32 liBlockingOwner = -1;
        for (s32 lf = 0; lf < KI_NUM_EVENT_OBSERVERS; ++lf)
        {
            for (s32 lc = 0; lc < KI_MAX_PRIORITY_CLAIMS_PER_FLOW; ++lc)
            {
                const PriorityClaim& lrClaim = maPriorityClaims[lf][lc];
                if (!lrClaim.mbActive)
                    continue;
                if (liPriorityOwner < 0 && lrClaim.miEventType == liId)
                    liPriorityOwner = lf;
                if (liBlockingOwner < 0 && mabPriorityBlocking[lf] &&
                    lrClaim.mabOverriddenEventIds[liId])
                {
                    liBlockingOwner = lf;
                }
            }
        }

        CgsModule::VariableEventQueue<18432, 16>* lapQueues[KI_NUM_EVENT_OBSERVERS] =
            { &mScreenInQueue, &mHudInQueue, &mOverlayInQueue, &mAlwaysAvailInQueue };
        for (s32 lf = 0; lf < KI_NUM_EVENT_OBSERVERS; ++lf)
        {
            if (!mabObservedEventIds[lf][liId])
                continue;

            if (liPriorityOwner >= 0)
            {
                if (lf == liPriorityOwner || liBlockingOwner < 0)
                {
                    lapQueues[lf]->AddEvent(lpEvent, liId, liSize);
                    if (lf == liPriorityOwner)
                        mabPriorityBlocking[lf] = true;
                }
                else if (mabObservedEventIds[lf][CgsGui::E_GUI_PRIORITY_REMOVAL])
                {
                    const s32 liRemovedEventId = liId;
                    lapQueues[lf]->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&liRemovedEventId),
                        CgsGui::E_GUI_PRIORITY_REMOVAL, static_cast<s32>(sizeof(liRemovedEventId)));
                }
                continue;
            }

            if (liBlockingOwner >= 0)
            {
                if (lf == liBlockingOwner)
                {
                    const s32 liBlockingEventId = liId;
                    lapQueues[lf]->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&liBlockingEventId),
                        CgsGui::E_GUI_PRIORITY_BLOCKING, static_cast<s32>(sizeof(liBlockingEventId)));
                    lapQueues[lf]->AddEvent(lpEvent, liId, liSize);
                }
                else if (mabObservedEventIds[lf][CgsGui::E_GUI_PRIORITY_REMOVAL])
                {
                    const s32 liRemovedEventId = liId;
                    lapQueues[lf]->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&liRemovedEventId),
                        CgsGui::E_GUI_PRIORITY_REMOVAL, static_cast<s32>(sizeof(liRemovedEventId)));
                }
                continue;
            }

            lapQueues[lf]->AddEvent(lpEvent, liId, liSize);
        }
    }

    // ARTIST @0x825112B0 converts the FLApt action string to a
    // GuiAudioTriggerEvent and publishes it through the module output buffer.
    void GuiModule::FlaptSoundTriggerCallback(void* lpUserData,
                                              const char* lpcComponentName,
                                              const char* lpcSwfName,
                                              const char* lpcActionName,
                                              const char* lpcLabel)
    {
        CGS_ASSERT(lpUserData != 0, "lpUserData");
        CGS_ASSERT(lpcComponentName != 0, "lpcComponentName");
        CGS_ASSERT(lpcSwfName != 0, "lpcSwfName");
        CGS_ASSERT(lpcActionName != 0, "lpcActionName");
        CGS_ASSERT(lpcLabel != 0, "lpcLabel");

        GuiModule* lpThis = static_cast<GuiModule*>(lpUserData);
        CGS_ASSERT(lpThis->mpOutputBuffer != 0, "mpOutputBuffer");

        // off_82F277A8..off_82F277E0: the fourteen authored presentation actions.
        static const char* const KAPC_ACTION_NAMES[14] = {
            "ON_ENTER", "ON_LEAVE", "ON_FOCUS", "ON_LOSE_FOCUS",
            "ON_ACCEPT", "ON_CANCEL", "ON_TICK", "ON_CHANGE",
            "ON_UP", "ON_DOWN", "ON_LEFT", "ON_LEFT_SWEEP",
            "ON_RIGHT", "ON_RIGHT_SWEEP"
        };

        s32 liAction = 14;
        for (s32 li = 0; li < 14; ++li)
        {
            if (std::strcmp(lpcActionName, KAPC_ACTION_NAMES[li]) == 0)
            {
                liAction = li;
                break;
            }
        }

        GuiAudioTriggerEvent lAudioEvent;
        lAudioEvent.Construct(liAction, lpcComponentName, lpcLabel, lpcSwfName);
        lpThis->mpOutputBuffer->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudioEvent),
            lAudioEvent.GetEventType(), static_cast<s32>(sizeof(lAudioEvent)));
    }

    // The real GuiModule::Update event dispatch (X360 0x82527A58's switch): consume the
    // module-level events, forward the load notifications, and fan the rest to the flow.
    void GuiModule::DispatchInboundGuiEvents()
    {
        if (mpGuiEventInputBuffer == 0)
            return;

        mpGuiEventInputBuffer->LockForRead();
        const CgsModule::VariableEventQueue<32768, 16>* lpInQueue =
            static_cast<const CgsGui::CgsGuiModuleIO::InputBuffer*>(mpGuiEventInputBuffer)->GetGuiEvents();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            switch (liId)
            {
                case 144:   // GuiEventRunFsm -- the flow-change request (BridgeGameToGui)
                    mFsmController.RunFsm(reinterpret_cast<const GuiEventRunFsm*>(lpEvent));
                    break;

                case 481:   // HUD-state load complete: notify the controller + forward
                    mFsmController.HandleHudStateLoadComplete();
                    // fall through -- the record also lands on the notification queue
                case 14:    // load notification    -> the ModelIO output notification queue
                case 16:    // unload notification  -> (the controller's Update consumes them)
                {
                    mModelOutputBuffer.LockForWrite();
                    mModelOutputBuffer.GetLoadNotificationsNonConst()->AddEvent(lpEvent, liId, liSize);
                    mModelOutputBuffer.UnlockForWrite();
                    break;
                }

                case 504:   // localized audio ready
                case 508:   // play video
                case 513:   // (movie family)
                    mMovieManager.RecvEvent(lpEvent, liId);
                    break;

                default:
                    // The other module-level consumers (profile/skills/overlays/keyboard/
                    // language...) are subsystem follow-ons; their events pass through to
                    // the flow filter below.
                    break;
            }

            // The observer-subscription fan-out (EventInterpreterModule::ProcessInEvents).
            RouteEventToFlow(lpEvent, liId, liSize);

            const CgsModule::Event* lpNext = 0;
            liId = lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        mpGuiEventInputBuffer->UnlockForRead();
    }

    // The REAL GuiResourceModule dispatch (replaces ServiceFsmBundleRequests). Runs the
    // reconstructed CgsGui::GuiResourceModule each frame against its own persistent IO
    // pair, and bridges the two queue ends to the flow controller's ModelIO buffers --
    // the same 39-in / 14-out contract the host stand-in served, now through the module:
    //   1. feed this frame's controller load requests (GuiEventLoadRequest, id 39) into
    //      the module input, then clear the controller queue (consumed);
    //   2. run the module -- it drains the requests into its bundle-load queue, advances
    //      the acquire state machine, and at its Update tail runs the [PC] platform
    //      servicer that loads FSM\<NAME>.BUNDLE synchronously; completed loads post
    //      GuiEventLoadNotification (14) into the module output;
    //   3. clear the module input's now-consumed request queue (the persistent PC buffer
    //      is not recreated per frame as the console's transient one is);
    //   4. bridge the module's load notifications into the ModelIO output notification
    //      queue the controller reads (what the host stand-in posted on completion);
    //   5. clear the module output's bridged notifications.
    // The module's acquire machine takes several frames per bundle (acquire-miss -> load
    // -> re-acquire -> notify); the controller's WFLOAD stage polls for the notification,
    // so the completion arriving 1+ frames after the request is safe. On the console the
    // module runs under the model scheduler between the controller's request-post and
    // notification-read; here it runs in that same slot, before the controller Update.
    void GuiModule::DispatchGuiResourceModule()
    {
        // 1. Feed the controller's requests (posted into mModelInputBuffer by the previous
        //    frame's GuiFsmController::Update) into the module input, then clear them.
        mModelInputBuffer.LockForWrite();
        mResourceInputBuffer.LockForWrite();
        CgsGui::GuiEventQueueSmall* lpControllerRequests = mModelInputBuffer.GetLoadRequests();
        mGuiResourceModule.AddResourceRequests(lpControllerRequests, &mResourceInputBuffer);
        lpControllerRequests->Clear();
        mResourceInputBuffer.UnlockForWrite();
        mModelInputBuffer.UnlockForWrite();

        // 2. Run the module for this frame (it locks its own IO pair internally; hold no
        //    lock here). The Update tail's ServicePlatformRequests loads the bundle files.
        mGuiResourceModule.Update(&mResourceInputBuffer, &mResourceOutputBuffer);

        // 3. Drop this frame's now-consumed input requests (ProcessIncomingLoadRequests
        //    reads but does not clear them; the persistent PC buffer must not re-queue).
        mResourceInputBuffer.LockForWrite();
        mResourceInputBuffer.GetLoadRequestsNonConst()->Clear();
        mResourceInputBuffer.UnlockForWrite();

        // 4. Bridge every notification to ModelIO, where both the FSM controller and
        //    GuiCache observe it. Apt movie load notifications (request types 4..7) also
        //    go to the VIEW input buffer as
        //      view event 14, where the REAL CgsGui::ViewModule::ProcessIncomingLoadNotification
        //      @0x8285BD30 registers each header (AddAptData). Phase 2 routes the movie-slot
        //      bundle IO through the module, so these replace the AptRuntimeHost's
        //      PopPendingLoadNotification ring for the flow movie.
        //    The dual delivery is the ARTIST contract: the cache owns resource state while
        //    ViewModule owns Apt registration.
        mResourceOutputBuffer.LockForRead();
        mModelOutputBuffer.LockForWrite();
        mViewInputBuffer.LockForWrite();
        {
            const CgsGui::GuiResourceModuleIO::InputBuffer::GuiEventQueue* lpNotifications =
                mGuiResourceModule.GetLoadedNotifications(&mResourceOutputBuffer);
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpNotifications->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                bool lbAptMovie = false;
                if (liId == 14)
                {
                    const s32 liReqType = static_cast<s32>(
                        reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent)->meRequestType);
                    lbAptMovie = (liReqType >= 4 && liReqType <= 7) || liReqType == 10;
                }
                mModelOutputBuffer.GetLoadNotificationsNonConst()->AddEvent(
                    lpEvent, liId, liSize);
                if (lbAptMovie)
                    mViewInputBuffer.GetViewStateQueue()
                        .CgsModule::VariableEventQueue<65536, 16>::AddEvent(lpEvent, 14, liSize);
                const CgsModule::Event* lpNext = 0;
                liId = lpNotifications->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
        }
        mViewInputBuffer.UnlockForWrite();
        mModelOutputBuffer.UnlockForWrite();
        mResourceOutputBuffer.UnlockForRead();

        // 5. The notifications are bridged; clear the module output queue for next frame.
        mResourceOutputBuffer.LockForWrite();
        mResourceOutputBuffer.GetLoadNotificationsNonConst()->Clear();
        mResourceOutputBuffer.UnlockForWrite();
    }

    // (RequestAptMovieLoad RETIRED, slice 2: no more host movie-slot requests --
    // the engine's AptLoader owns movie data acquisition.)

    // (RequestAptMovieLoadThroughModule RETIRED, slice 2: the engine's AptLoader
    // requests movie data itself -- registered-data first, bundle-IO fallback --
    // through the real AptLoaderStartAsyncLoad platform hook.)

    // [PC IO] the ORIGINAL host FSM-bundle stand-in: serviced the controller's FSM-bundle
    // load requests synchronously and posted the load notification it waits for. SUPERSEDED
    // by DispatchGuiResourceModule (the real CgsGui::GuiResourceModule now owns this path);
    // retained unused this phase -- /OPT:REF strips the unreferenced body from the exe.
    // (On the console the request queue reaches CgsGui::GuiResourceModule through the
    // module scheduler; ProcessIncomingLoadRequests + LoadBundle then post the
    // notification -- those bodies are reconstructed, but the module dispatch that runs
    // them was not, so the IO leaf lived here.)
    void GuiModule::ServiceFsmBundleRequests()
    {
        mModelInputBuffer.LockForWrite();
        CgsGui::GuiEventQueueSmall* lpRequests = mModelInputBuffer.GetLoadRequests();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpRequests->GetFirstEvent(&lpEvent, &liSize);
        bool lbAnyServed = false;
        while (liId >= 0 && lpEvent != 0)
        {
            if (liId == 39)   // GuiEventLoadRequest
            {
                const CgsGui::GuiEventLoadRequest* lpRequest =
                    reinterpret_cast<const CgsGui::GuiEventLoadRequest*>(lpEvent);
                if (lpRequest->meLoadUnload == CgsGui::E_GUI_RESOURCEREQUEST_LOAD &&
                    lpRequest->mpacFileToLoad != 0)
                {
                    lbAnyServed = true;

                    // The controller's request ids map onto the flow slots (13/14/15 =
                    // SCREEN/HUD/OVERLAY); each flow owns a resident pool so a load for
                    // one flow never drops another flow's live LuaCode.
                    s32 liFlow = E_GUIFLOW_HUD;
                    switch (lpRequest->muLoadRequestId)
                    {
                        case 13u: liFlow = E_GUIFLOW_SCREEN;  break;
                        case 14u: liFlow = E_GUIFLOW_HUD;     break;
                        case 15u: liFlow = E_GUIFLOW_OVERLAY; break;
                        default:  break;
                    }

                    // Re-init that flow's pool for the fresh bundle (the previous FSM's
                    // LuaCode was released by the flow's staged Release before this load).
                    CgsResource::Pool::InitOptions lOptions;
                    lOptions.miId    = 2;
                    lOptions.mpcName = "GuiFsm";
                    for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                    {
                        lOptions.maHeapInfo[lt].muMaxNodes       = 64u;
                        lOptions.maHeapInfo[lt].muHeapMemorySize = KU_FSM_POOL_BYTES - 64u * 1024u;
                        lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
                        lOptions.mResource.m_baseResources[lt]   = s_fsmPoolBacking[liFlow][lt];
                        lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_FSM_POOL_BYTES;
                        lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
                    }
                    lOptions.muMaxResources         = 64u;
                    lOptions.muMaxImports           = 64u;
                    lOptions.miRefCountThreshold    = 0;
                    lOptions.miNumDependencies      = 0;
                    lOptions.miBankId               = 0;
                    lOptions.mbAllowDefragmentation = false;
                    mFsmBundlePool[liFlow].InitPool(&lOptions);

                    char lacBundlePath[160];
                    std::snprintf(lacBundlePath, sizeof(lacBundlePath), "FSM/%s.BUNDLE",
                                  lpRequest->mpacFileToLoad);

                    CgsResource::BundleLoader lLoader;
                    const s32 liLoaded = lLoader.LoadBundle(lacBundlePath, &mFsmBundlePool[liFlow],
                                                            CgsResource::ResolveResourceType);
                    s32 liIndex = -1;
                    CgsResource::Entry* lpEntry = (liLoaded > 0)
                        ? mFsmBundlePool[liFlow].FindFirstResourceOfType(
                              CgsResource::E_RESOURCETYPE_LUACODE, &liIndex)
                        : 0;

                    char lac[200];
                    std::snprintf(lac, sizeof(lac),
                        "[GuiModule] FSM bundle '%s' -> %s (request id %u).\n",
                        lacBundlePath, lpEntry != 0 ? "loaded" : "MISSING",
                        lpRequest->muLoadRequestId);
                    CgsDev::Log::WriteToLog(lac);

                    if (lpEntry != 0)
                    {
                        // The notification the controller's WFLOAD stage waits for (the
                        // GuiResourceModule's AddLoadNotification record, queue type 14).
                        CgsGui::GuiEventLoadNotification lNotification;
                        lNotification.mResourceHandle.mpResourceMemory =
                            &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
                        lNotification.mResourceHandle.mpSourceEntry = lpEntry;
                        lNotification.meRequestType   = lpRequest->meRequestType;
                        lNotification.muLoadRequestId = lpRequest->muLoadRequestId;

                        mModelOutputBuffer.LockForWrite();
                        mModelOutputBuffer.GetLoadNotificationsNonConst()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lNotification), 14,
                            static_cast<s32>(sizeof(lNotification)));
                        mModelOutputBuffer.UnlockForWrite();
                    }
                }
                // Unload requests never reach this queue on the reconstructed controller
                // (its WFUNLOAD stage completes against the dummy notification).
            }
            const CgsModule::Event* lpNext = 0;
            liId = lpRequests->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        if (lbAnyServed || liId < 0)
            lpRequests->Clear();
        mModelInputBuffer.UnlockForWrite();
    }

    // Drain one observer's StateInterface output queue -- the per-frame dispatch point for
    // everything its states post (the ModelModule bridge + EventInterpreter
    // ProcessOutEvents roles). The 34/35 subscription records key into THAT observer's
    // observed-id table. The fourth slot is the always-available components manager,
    // whose Prepare posts its real 19-id registration through the same records.
    void GuiModule::DrainFlowOutputQueue(s32 liFlow)
    {
        CgsGui::GuiStackEventQueue::GuiEventQueueLarge* lpOutQueue = 0;
        switch (liFlow)
        {
            case E_GUIFLOW_SCREEN:  lpOutQueue = mScreenFlow.GetOutputEventQueue();  break;
            case E_GUIFLOW_HUD:     lpOutQueue = mHudFlow.GetOutputEventQueue();     break;
            case E_GUIFLOW_OVERLAY: lpOutQueue = mOverlayFlow.GetOutputEventQueue(); break;
            case E_GUIOBSERVER_ALWAYSAVAILABLE:
                lpOutQueue = mAlwaysAvailableComponentsManager.GetOutputEventQueue();
                break;
            default:                break;
        }
        if (lpOutQueue == 0)
            return;

        CgsModule::VariableEventQueue<65536, 16>* lpOutBase = lpOutQueue;
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        s32 liId = lpOutBase->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            switch (liId)
            {
                case 34:   // RegisterForEvents (the observer-subscription record)
                case 35:   // UnRegisterForEvents
                {
                    const s32 liType =
                        reinterpret_cast<const RegisterEventRecord*>(lpEvent)->miEventType;
                    if (liType >= 0 && liType < KI_MAX_OBSERVED_EVENT_ID)
                        mabObservedEventIds[liFlow][liType] = (liId == 34);
                    break;
                }
                case 36:   // PriorityRegisterForEvent
                {
                    if (liSize < static_cast<s32>(sizeof(PriorityRegisterRecordPrefix)))
                        break;
                    const PriorityRegisterRecordPrefix* lpRecord =
                        reinterpret_cast<const PriorityRegisterRecordPrefix*>(lpEvent);

                    PriorityClaim* lpClaim = 0;
                    for (s32 lc = 0; lc < KI_MAX_PRIORITY_CLAIMS_PER_FLOW; ++lc)
                    {
                        PriorityClaim& lrCandidate = maPriorityClaims[liFlow][lc];
                        if (lrCandidate.mbActive &&
                            lrCandidate.miEventType == lpRecord->miEventType)
                        {
                            lpClaim = &lrCandidate;
                            break;
                        }
                        if (lpClaim == 0 && !lrCandidate.mbActive)
                            lpClaim = &lrCandidate;
                    }
                    if (lpClaim == 0)
                        break;

                    lpClaim->mbActive    = true;
                    lpClaim->miEventType = lpRecord->miEventType;
                    for (s32 li = 0; li < KI_MAX_OBSERVED_EVENT_ID; ++li)
                        lpClaim->mabOverriddenEventIds[li] = false;
                    const u32 luCount = (lpRecord->muOverrideCount < 600u)
                        ? lpRecord->muOverrideCount : 600u;
                    for (u32 lu = 0; lu < luCount; ++lu)
                    {
                        const s32 liOverridden = lpRecord->maiEventTypeOverridden[lu];
                        if (liOverridden >= 0 && liOverridden < KI_MAX_OBSERVED_EVENT_ID)
                            lpClaim->mabOverriddenEventIds[liOverridden] = true;
                    }
                    break;
                }
                case 37:   // PriorityUnRegisterForEvent
                {
                    const s32 liPriority =
                        reinterpret_cast<const RegisterEventRecord*>(lpEvent)->miEventType;
                    for (s32 lc = 0; lc < KI_MAX_PRIORITY_CLAIMS_PER_FLOW; ++lc)
                    {
                        PriorityClaim& lrClaim = maPriorityClaims[liFlow][lc];
                        if (lrClaim.mbActive && lrClaim.miEventType == liPriority)
                        {
                            lrClaim.mbActive = false;
                            lrClaim.miEventType = -1;
                            break;
                        }
                    }
                    break;
                }
                case 38:   // StopPriorityEventBlocking
                    mabPriorityBlocking[liFlow] = false;
                    break;

                case KI_GUIEVENT_PLAY_VIDEO:   // 508
                case KI_GUIEVENT_STOP_VIDEO:   // 509
                    mMovieManager.GetReceiverQueue()->AddEvent(lpEvent, liId, liSize);
                    break;

                case 40:   // channel 40: GuiEventOut command records -> the game bridge
                    mGuiOutQueue.AddEvent(lpEvent, liId, liSize);
                    break;

                case 41:   // channel 41: GuiOutViewState records -> the view input queue
                {
                    const CgsGui::GuiEventPlayAptMovie* lpPlay =
                        reinterpret_cast<const CgsGui::GuiEventPlayAptMovie*>(lpEvent);
                    if (lpPlay->muEventType == 18)   // PlayAptMovie {name, level}
                    {
                        if (BrnGui::gpActiveAptRuntimeHost != nullptr)
                            BrnGui::gpActiveAptRuntimeHost->Prepare();   // idempotent

                        struct { const char* mpacMovieName; s32 miLevelNum; } lBody =
                            { lpPlay->mpacMovieName, lpPlay->miLevelNum };
                        mViewInputBuffer.GetViewStateQueue()
                            .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lBody), 18,
                                static_cast<s32>(sizeof(lBody)));
                    }
                    // The other view-state records (options 25, transins 214, ...) ride
                    // the AptCommunicator component path on PC. [FLAG: the raw channel-41
                    // bridge for them lands with the full view IO chain.]
                    break;
                }

                case 155:  // menu-music request (0 = stop)
                    HandleMenuMusicEvent(static_cast<s32>(
                        reinterpret_cast<const CgsGui::GuiEventPlayMusicOnMenuStream*>(
                            lpEvent)->muStreamNameHash));
                    break;

                case 201:  // GUI audio trigger -> the module output event channel
                    CGS_ASSERT(mpOutputBuffer != 0, "mpOutputBuffer");
                    mpOutputBuffer->AddEvent(lpEvent, liId, liSize);
                    break;

                case 42:   // internal command channel (preload-done 72 etc.) -- consumers
                    break; // are module-internal follow-ons. [FLAG]

                default:
                    // Resource requests (39) and the other state outputs are follow-ons.
                    break;
            }

            const CgsModule::Event* lpNext = 0;
            liId = lpOutBase->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        lpOutBase->Clear();
    }

    // X360 GuiModule::Update (0x82527A58): dispatch the inbound GUI events, drive the
    // FSM controller + the flows + the MovieManager, then the view chain.
    void GuiModule::Update()
    {
        // ---- 1. inbound GUI events (144 -> RunFsm; 14/16/481 -> notifications;
        //          504/508/513 -> MovieManager; subscription fan-out to the flow) -------
        DispatchInboundGuiEvents();

        // ---- 2. the per-frame cache event (64): the real Update posts the GuiCache
        //          pointer into the event queue each frame; the states read it as their
        //          "cache ready" feed (BootPreload/BootVideos/BootProfile all key on it).
        {
            GuiEventCache lCacheEvent;
            lCacheEvent.mpGuiCache = &mGuiCache;
            // Delivery to every subscriber -- the three flows AND the always-available
            // manager (its real 19-id table includes 64; it latches the GuiCache its
            // Prepare state machine waits on) -- rides the one subscription filter.
            RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64,
                             static_cast<s32>(sizeof(lCacheEvent)));
        }

        // ---- 2b. boot-resources-ready feedback (event 567; bring-up FLAG) -------------
        // The console GUI cache posts 567 when the title's expected apt components have
        // initialised, which arms BootLegal's press-start path. The cache watcher isn't
        // reconstructed; post it once when the apt movie is live.
        if (!mbResourcesReadyFed && mAptRuntimeHost.IsMovieLive())
        {
            CgsModule::Event lReady;
            RouteEventToFlow(&lReady, 567, static_cast<s32>(sizeof(lReady)));
            mbResourcesReadyFed = true;
            CgsDev::Log::WriteToLog("[GuiModule] apt movie live -> fed resources-ready (567).\n");
        }

        // ---- 3. the FSM-bundle load service (the REAL GuiResourceModule) then the
        //          controller update ---------------------------------------------------
        DispatchGuiResourceModule();
        mModelInputBuffer.LockForWrite();
        mModelOutputBuffer.LockForRead();

        // ARTIST GuiCache::RecEvent consumes load/unload completion before the cache
        // update publishes its next double-buffered request batch.
        {
            const CgsGui::ModelIO::OutputBuffer::GuiNotificationQueue* lpNotifications =
                mModelOutputBuffer.GetLoadNotifications();
            const CgsModule::Event* lpNotification = 0;
            s32 liNotificationSize = 0;
            s32 liNotificationId =
                lpNotifications->GetFirstEvent(&lpNotification, &liNotificationSize);
            while (liNotificationId >= 0 && lpNotification != 0)
            {
                if (liNotificationId == 14 || liNotificationId == 16)
                    mGuiCache.RecEvent(lpNotification, liNotificationId);

                const CgsModule::Event* lpNext = 0;
                liNotificationId = lpNotifications->GetNextEvent(
                    lpNotification, &lpNext, &liNotificationSize);
                lpNotification = lpNext;
            }
        }
        mGuiCache.Update(&mModelInputBuffer);
        mFsmController.Update(&mModelInputBuffer, &mModelOutputBuffer);
        mModelOutputBuffer.UnlockForRead();
        mModelInputBuffer.UnlockForWrite();

        // The real Update clears the notification queue at its tail (the per-frame IO
        // buffer lifecycle); the controller has consumed this frame's records.
        mModelOutputBuffer.LockForWrite();
        mModelOutputBuffer.GetLoadNotificationsNonConst()->Clear();
        mModelOutputBuffer.UnlockForWrite();

        // ---- 3b. the profile manager pump (X360 GuiModule::Update: the SLS Update at
        //          module+685896, the collision-world validate/invalidate swap
        //          @0x82519578, and the manager out-queue drained into the GUI out
        //          channel). The world-side pool free/restore around the swap is the
        //          un-reconstructed world collision pool -- FLAG'd absent (no world on
        //          the PC boot path); the manager's own state machine is driven fully. --
        mProfileManager.Update();
        if (mProfileManager.PendingCollisionWorldInvalidate())
        {
            // FLAG PC-platform leaf: the console frees the world collision pool here.
            mProfileManager.SetCollisionWorldValid(false);
        }
        if (mProfileManager.PendingCollisionWorldValidate())
        {
            // FLAG PC-platform leaf: the console restores the world collision pool here.
            mProfileManager.SetCollisionWorldValid(true);
        }
        {
            CgsGui::GuiEventQueueSmall& lrProfileOut = mProfileManager.GetOutEventQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lrProfileOut.GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mGuiOutQueue.AddEvent(lpEvent, liId, liSize);
                // The console publishes these on the module bus (AddGuiOutEvents onto the
                // out buffer's gui-events channel), where they come back around as in
                // events and reach every registered observer through the interpreter's
                // subscription filter (that is how the autosave-icon flag, id 355, reaches
                // the always-available manager). Model the loop with the same filter.
                RouteEventToFlow(lpEvent, liId, liSize);
                const CgsModule::Event* lpNext = 0;
                liId = lrProfileOut.GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lrProfileOut.Clear();
        }

        // Pump the always-available components manager (the top-left save-icon spinner + the
        // in-game EATrax/achievement/showtime overlays). The console GuiModule::Update
        // (@0x82527A58) advances its Prepare state machine each frame, and the interpreter's
        // UpdateObservers runs its Update against the queue the subscription filter filled
        // (RouteEventToFlow above delivers the ids its Prepare registered -- 64, 355, ...).
        mAlwaysAvailableComponentsManager.Prepare(&s_GuiAccessPointers);
        mAlwaysAvailableComponentsManager.Update();
        mAlwaysAvailInQueue.Clear();

        // ---- 4. the flow ticks (each current state's PreUpdate/Update/PostUpdate) -----
        mScreenFlow.Update();
        mHudFlow.Update();
        mOverlayFlow.Update();

        // ---- 5. drain the flow's output (subscriptions / movie / view / game / audio) --
        mViewInputBuffer.LockForWrite();
        {
            // Drain the pending LOAD NOTIFICATIONS into the view queue FIRST (event 14 --
            // the GuiResourceModule output-buffer stand-in): the real ViewModule::
            // ProcessIncomingLoadNotification @0x8285BD30 performs every registration
            // (AddAptData / LoadStringTable / AddFont) when the queue dispatches, BEFORE
            // any play-movie event (18) posted below consumes the registered data --
            // the console's notification-before-play ordering.
            CgsGui::GuiEventLoadNotification lNotification;
            while (mAptRuntimeHost.PopPendingLoadNotification(&lNotification))
            {
                mViewInputBuffer.GetViewStateQueue()
                    .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNotification), 14,
                        static_cast<s32>(sizeof(lNotification)));
            }

            DrainFlowOutputQueue(E_GUIFLOW_SCREEN);
            DrainFlowOutputQueue(E_GUIFLOW_HUD);
            DrainFlowOutputQueue(E_GUIFLOW_OVERLAY);
            // The always-available manager is the fourth registered observer: its
            // StateInterface out-queue carries the type-34 registration records its
            // Prepare posts (the real 19-id table) plus anything its components emit.
            DrainFlowOutputQueue(E_GUIOBSERVER_ALWAYSAVAILABLE);
        }
        mViewInputBuffer.UnlockForWrite();

        // ---- 6. the MovieManager pump (receiver -> RecvEvent -> Update -> 510 back) ---
        {
            CgsModule::VariableEventQueue<1024, 16>* lpRecv = mMovieManager.GetReceiverQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpRecv->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                mMovieManager.RecvEvent(lpEvent, liId);
                const CgsModule::Event* lpNext = 0;
                liId = lpRecv->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            lpRecv->Clear();
        }
        mMovieManager.Update();
        if (mMovieManager.HasFinishedReporting())
        {
            // Video finished -> feed 510 back to the flow (the real Update posts the
            // finished VideoDefinition as event 510 into the model input event queue;
            // the boot states key on the id alone). [FLAG: the 48-byte definition
            // payload rides along when the movie-definition slice lands.]
            CgsModule::Event lFinishedEvent;
            RouteEventToFlow(&lFinishedEvent, 510, static_cast<s32>(sizeof(lFinishedEvent)));
            mMovieManager.AcknowledgeFinishedAndReturnToIdle();
            CgsDev::Log::WriteToLog("[GuiModule] video finished -> fed 510 to the flow.\n");
        }

        // Per-frame: let the menu-music stream (re)claim the audio output once the
        // movie stream is idle (the attract/intro video borrows the single device voice).
        CgsSystem::MenuMusicPC::Update();

        // ---- 7. the view frame (the real per-frame owner) -----------------------------
        // Post the frame time step (view event 26) onto the view-state queue and run
        // CgsGui::ViewModule::Update -- which dispatches the view events (incl. the
        // bridged play-movie 18 + notifications 14), advances the view clock, and ticks
        // AptAux::Update (the component flush + the engine AptUpdateTarget frame pacer).
        // FLAG (PC time source): the console's step rides the module scheduler's clock;
        // the wall clock is the host stand-in.
        {
            const s64 liNowMs = static_cast<s64>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            f32 lfStepSeconds = 0.0f;
            if (miLastViewFrameMs >= 0)
                lfStepSeconds = static_cast<f32>(liNowMs - miLastViewFrameMs) * 0.001f;
            miLastViewFrameMs = liNowMs;

            // FLAG PC-platform leaf: the console scheduler supplies a bounded simulation
            // step, whereas this host stand-in measures wall time across synchronous file
            // and movie loads. Cap it to one 60 Hz simulation tick so the faithful Flapt
            // updater never receives an impossible multi-frame delta (which ARTIST asserts)
            // and Apt does not attempt wall-clock catch-up after a blocking host operation.
            const f32 kfMaxHostViewStep = 1.0f / 60.0f;
            if (lfStepSeconds > kfMaxHostViewStep)
                lfStepSeconds = kfMaxHostViewStep;

            mViewInputBuffer.LockForWrite();
            if (lfStepSeconds > 0.0f)
            {
                mViewInputBuffer.GetViewStateQueue()
                    .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lfStepSeconds), 26,
                        static_cast<s32>(sizeof(lfStepSeconds)));
            }
            mViewInputBuffer.UnlockForWrite();

            mViewModule.Update(0, 0, &mViewInputBuffer, &mViewOutputBuffer);

            // The view consumed this frame's bridged events; reset the queue for the
            // next frame's bridge fill.
            mViewInputBuffer.LockForWrite();
            mViewInputBuffer.GetViewStateQueue()
                .CgsModule::VariableEventQueue<65536, 16>::Clear();
            mViewInputBuffer.UnlockForWrite();

            // The console GuiModule::Update @0x828602C8 tail: publish this frame's
            // AptCommunicator trigger records (SendAptEvent 21 apt triggers /
            // SendAptSoundEvent 22 sound triggers) into the view OUTPUT buffer's GUI
            // event queue, then clear the communicator queue.
            mViewOutputBuffer.LockForWrite();
            CgsGui::AptCommunicator::FlushTriggerEventsTo(mViewOutputBuffer.GetGuiEventQueue());
            mViewOutputBuffer.UnlockForWrite();
            // Deliver this frame's SOUND triggers (event 22 -- the AS SendAptSoundEvent
            // records: type[32] action[32] label[32] + layer) to the GUI sound leaf --
            // the console route is the sound-logic message layer (blocked cluster);
            // GuiSoundPC keys the same presentationactionlist data (CgsGuiSoundPC.h).
            mViewOutputBuffer.LockForRead();
            {
                const CgsModule::VariableEventQueue<18432, 16>* lpTrigQueue =
                    static_cast<const CgsGui::ViewIO::OutputBuffer&>(mViewOutputBuffer).GetGuiEventQueue();
                const CgsModule::Event* lpTrig = 0;
                s32 liTrigSize = 0;
                s32 liTrigId = lpTrigQueue->GetFirstEvent(&lpTrig, &liTrigSize);
                while (liTrigId >= 0 && lpTrig != 0)
                {
                    if (liTrigId == 21)
                    {
                        // ARTIST GuiCache::RecEvent case 21 marks expected Apt
                        // components on ONLOAD, then the EventInterpreter fans the
                        // same trigger to any flow state observing event 21.
                        mGuiCache.RecEvent(lpTrig, liTrigId);
                        RouteEventToFlow(lpTrig, liTrigId, liTrigSize);
                    }
                    else if (liTrigId == 22 && liTrigSize >= 100)
                    {
                        // {type[32], action[32], label[32], layer}. Key rule (the
                        // trigger-resolve): string key = label unless 'uninitialised',
                        // then the component/type name; the enum parses from the AS
                        // action string ('ON_FOCUS' -> OnFocus).
                        const char* lpacT = reinterpret_cast<const char*>(lpTrig);
                        CgsSystem::GuiSoundPC::OnTrigger(lpacT + 64, lpacT + 32, lpacT, -1);
                    }
                    const CgsModule::Event* lpTrigNext = 0;
                    liTrigId = lpTrigQueue->GetNextEvent(lpTrig, &lpTrigNext, &liTrigSize);
                    lpTrig = lpTrigNext;
                }
            }
            mViewOutputBuffer.UnlockForRead();
            // [PC] the downstream consumer (BridgeFromViewToOutput -> the module output
            // -> the sound-logic/flow observers) is un-homed, and the console's view
            // output buffer is re-created per frame off the IO stack; reset the queue
            // here as that per-frame recreate's stand-in so it cannot overflow either.
            mViewOutputBuffer.LockForWrite();
            mViewOutputBuffer.GetGuiEventQueue()
                ->CgsModule::VariableEventQueue<18432, 16>::Clear();
            mViewOutputBuffer.UnlockForWrite();
        }

    }

    // The per-frame GUI render drive. X360 BrnGui::GuiModule::Render @0x825146B8 gates on
    // the module-prepared byte (+949208), runs CgsGui::GuiModule::Render @0x8285AF38 --
    // whose core copies the GUI input buffer's renderer set into the view input buffer
    // (SetImRenderers) and calls ViewModule::Render @0x82858810 -- then
    // UpdateAndRenderMovieManager + the effects arbitrator. This PC drive reproduces the
    // view-render core; the movie manager renders through the renderer's
    // gpActiveMovieManager hook, and the effects arbitrator is data-gated.
    // FLAG PC-ABI adapter: gates on the Apt bring-up (the console's prepared byte).
    void GuiModule::Render()
    {
        if (!mAptRuntimeHost.IsReady())
            return;

        // FLAG (presentation stand-in ordering): full-screen movies are presented through
        // the renderer's gpActiveMovieManager hook (drawn BEFORE this), not through the
        // GUI view's MovieVideoRenderer as on console -- so the view's black clear
        // (RenderBlackScreen) would paint over them. Skip the view render whenever a movie
        // presentation is active (covers the boot logos in BF_VIDEOS AND the intro montage
        // in BF_COMPLOAD/PostTitleScreenLoad, which is NOT a video-presentation state but
        // still plays a full-screen movie), plus the two pre-title states that clear before
        // any movie arrives. The gate dies when the movie presentation moves under the real
        // view IO chain.
        {
            if (mMovieManager.IsMoviePresentationActive())
                return;
            static const CgsID KI_STATE_PRELOAD = CgsIDCompress("BF_PRELOAD");
            static const CgsID KI_STATE_VIDEOS  = CgsIDCompress("BF_VIDEOS");
            CgsGui::State* lpCurrentState = mHudFlow.GetStateMachine().GetCurrentState();
            if (lpCurrentState == 0)
                return;
            const CgsID lStateId = lpCurrentState->GetId();
            if (lStateId == KI_STATE_PRELOAD || lStateId == KI_STATE_VIDEOS)
                return;
        }

        CgsGui::AptIm2dRenderBuffer* lpAptBuffer = mAptRuntimeHost.GetAptRenderBuffer();
        if (lpAptBuffer == nullptr)
            return;

        // CgsGui::GuiModule::Render @0x8285AF38 core: publish the active renderer set
        // into the view input buffer. Slot 0 is the Apt Im2d command buffer the engine's
        // render callbacks fill; the MenusAndHud 3D slot carries the host's non-null
        // stand-in (AptRenderHandler::Render asserts it; the 2D-only boot path never
        // dereferences it). The camera is FLAG-deferred with the ViewModule camera member.
        CgsGui::ViewIO::ImRendererSet lRendererSet = {};
        lRendererSet.mpIm2dRenderBuffer            = lpAptBuffer;
        lRendererSet.mpIm3dRenderBufferMenusAndHud = mAptRuntimeHost.Get3dRendererAssertSatisfier();

        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.SetImRenderers(lRendererSet);
        mViewInputBuffer.UnlockForWrite();

        // The view module's render entry (Render @0x82858810 -> the RenderInternal
        // virtual -> the black-screen clear + AptAux::Render -> the engine render walk
        // -> FlaptManager::Render, all filling the published command buffer).
        mViewModule.Render(&mViewInputBuffer);

        // PC dispatch leaf: freeze + flush the filled Apt command buffer to D3D9 (the
        // console render thread consumes the buffers via the custom-renderer-manager
        // bracket RenderInternal notifies).
        mAptRuntimeHost.DispatchRenderResidue();
    }
}

// ---- GetAlwaysAvailableComponentsManager (free accessor) ----------------------------
// Header-declared in BrnGuiAlwaysAvailableComponentsManager.h; homed here because this TU
// owns the GuiModule layout.
namespace BrnGui
{
    AlwaysAvailableComponentsManager* GetAlwaysAvailableComponentsManager(GuiModule* lpGuiModule)
    {
        return lpGuiModule->GetAlwaysAvailableComponentsManager();
    }
}
