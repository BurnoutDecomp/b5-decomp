#include "GameSource/Gui/BrnGuiPerfmons.h"
#include "GameSource/Gui/BrnGuiModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // original movie acquire request
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h" // collision-world event 493
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h" // collision-world swap requests/replies
#include "GameSource/Resource/BrnResourceAllocator.h" // console-memory markers
#include "SDKs/EA/GameTalk/GameTalk.h"
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"   // [H3b] MapTransform (the sat-nav view-rect install)

#include <cstdio>                                                         // std::snprintf (log formatting)
#include <xtl.h>                                                      // [gate] GetEnvironmentVariableA (BRN_FLAPT_AFTER_DISPATCH)
#include <chrono>   // the PC frame clock for the view time-step event (FLAG: wall clock)
#include <cstddef>  // offsetof for the native event-22 payload
#include <cstring>  // std::strcmp (ARTIST GUI-audio action-name table)

#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h"// CgsResource::BundleLoader ([PC IO] FSM loads)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h"// CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"    // E_RESOURCETYPE_LUACODE / E_MEMTYPE_*
#include "GameShared/GameClasses/Fsm/Resources/CgsLuaCodeResource.h"      // CgsResource::LuaCodeResource
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> (channel command records)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::GuiEventPlayAptMovie (channel-41 payload)
#include "GameShared/GameClasses/Gui/Model/CgsEventInterpreterModule.h"   // priority removal/blocking event ids
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification / GuiEventLoadRequest
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiAudioTriggerEvent
#include "GameSource/GameState/Progression/BrnProfile.h"                   // BrnProgression::Profile::Construct (the PC progression block seed)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"        // AlwaysAvailableComponentsManager + free accessor (bodied below)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (flow interface wiring)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"       // CgsGui::AptAuxPointer (the AptAux singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::AptCommunicator (the per-frame trigger publish)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash (event-155 keys)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // FLApt live-instance allocator
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/rwcore_general_alloc.h"                                   // actual public allocator derivation
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::Input/OutputBuffer (the colour-calibration screen's IO)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // AllocatorList::GetRWLinearResourceAllocator (mGuiConfig.mpTextureAllocator)
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h" // GetScriptedLoadGameData{Input,Output} (the PC stand-in for the scheduler's GameData IO pair)
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"      // BrnNetwork::LiveRevengeProfile (the PC live-revenge block)

// Existing PC platform providers in System/PC/CgsXboxLivePC.cpp.
extern "C" void* XNotifyCreateListener(unsigned long long luAreas);
extern "C" int XNotifyGetNext(void* lhListener, unsigned long luFilter,
    unsigned long* lpuId, unsigned long* lpuParam);

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
// commands 19/20 on the GUI-out channel and the game module's BridgeGuiToGame writes the
// dispatch write buffer's command (BrnDispatchThreadInputBuffer.h).
extern bool gBrnInitialLoadingComplete;    // set by the game-flow when the load stages finish
extern bool gBrnGuiDrivesLoadingScreen;    // we set this while the HUD flow FSM is live
// The in-game flow-state latch (BrnGameMainFlowInGameState.cpp) -- gates the PC
// ignition-event stand-in in the per-frame view drive below.
namespace BrnGameMainFlowController { extern bool gBrnInGameStateActive; }
extern bool gbLogGuiAudioTriggers; // X360 byte_82FB5098; defined by CgsAptCommunicator.cpp.

namespace
{
    // ARTIST event 22's unwrapped inner body is 100 bytes, with no GuiEvent header.
    // Keep a payload-only view here because the current CgsGui::GuiEventSoundTrigger
    // carries a 12-byte host header ahead of these fields.
    struct GuiEventSoundTriggerPayload22
    {
        char macTypeName[32];
        char macActionName[32];
        char macLabel[32];
        s32 miLayer;
    };
    static_assert(sizeof(GuiEventSoundTriggerPayload22) == 100 &&
                  offsetof(GuiEventSoundTriggerPayload22, miLayer) == 96,
                  "ARTIST event-22 payload reads its layer at byte 96");

    // ARTIST file-global Tool.GUI request owners at82FB5060/5088/5089.
    // The native image starts these bytes at zero. Construct clears only the
    // start flag; native Update consumes start before stop. Names are host projections.
    char gacGuiToolHookName[32] = {};
    bool gbGuiToolStartHookRequested = false;
    bool gbGuiToolStopHookRequested = false;

    // Host extraction of the callback's duplicated assert/copy block. Keep
    // assertion continuation and strncpy's original fixed32 behavior.
    void CopyGuiToolHookName(const char* lpcContent, s32 liOriginalLine)
    {
        if (!lpcContent)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpcContent != NULL",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", liOriginalLine);
            CgsDev::Assert::EndAssert();
        }
        if (std::strlen(lpcContent) >= 32)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "String too long: " << (lpcContent ? lpcContent : "<NULLSTRING>");
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 55);
            CgsDev::Assert::EndAssert();
        }
        std::strncpy(gacGuiToolHookName, lpcContent, 32);
    }

    // Backing for the per-flow FSM bundle pools (3 mem types each; the boot FSM scripts
    // are tiny single-state bundles ~1.5 KB -- BRNSCREENFSM is the largest at ~68 KB --
    // and each pool reserves 64 KB for its own management structures). One backing per
    // flow slot: each flow's ScriptedFsm holds its LuaCode resource while live.
    const u32 KU_FSM_POOL_BYTES = 256u * 1024u;
    u8 s_fsmPoolBacking[BrnGui::E_GUIFLOW_COUNT][CgsResource::E_MEMTYPE_NUMTYPES][KU_FSM_POOL_BYTES];
    u8 s_fsmLuaHeapBuffer[KU_FSM_LUA_HEAP_BYTES];

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

    u8 s_profileLinearBacking[2u * 1024u * 1024u];

    static_assert(sizeof(BrnNetwork::LiveRevengeProfile) == 30016,
                  "the live-revenge block is the 30016-byte segment the profile manager copies");

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

// ============================================================================
// THE APT BRING-UP (transplanted from the retired BrnGuiAptRuntime.cpp): the
// GuiModule owns the Apt bring-up driver; render callbacks use the frame owner's buffers, as the
// console GuiModule::Prepare owns the view/apt prepare chain. Every FLAG below
// is carried over unchanged; the remaining stand-in is the [PC IO] language
// string-table load (its console replacement, the CgsLanguage::Sku pump, is
// reconstructed and awaits wiring -- see the retirement plan).
// ============================================================================

#include <cstdio>    // std::snprintf (probe logging)
#include <cstring>   // std::strncpy
#include <chrono>    // steady_clock (the faithful timeline pacing's elapsed-ms source)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

// ---- the Apt text render-data hooks (dynamic-text draw/release) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCallbackRender.h"   // AptCallbackRender::DrawString / DeallocateString + AptMaskRenderOperation

// ---- the Apt host adaptor + render handler (the render bridge) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"          // CgsGui::AptAux / AptAuxPointer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"// CgsGui::AptImRendererSet / AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"                // CgsGui::ViewModule (real Apt/text owner)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                       // CgsGuiModuleIO::ImRendererSet
#include "GameShared/GameClasses/Module/CgsModuleUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameSource/Gui/BrnGuiPerfmons.h"

// ---- the Apt engine leaves that ARE bodied (the bring-up drives these) ------
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"            // AptValueGC_PoolManager
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpGCPoolManager / gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"     // AptActionInterpreter + AptInitParmsT
#include "SDKs/EATech/include/Apt/AptTarget.h"                // AptTarget + gpAptTarget singletons
#include "SDKs/EATech/include/Apt/AptString/EAString.h"       // EAStringC (loader file name)
#include "SDKs/EATech/include/Apt/AptLoader.h"                // AptLoader / GetTarget / AptFilePtr

// ---- the original 2D command-buffer types used by the Apt render callbacks ----
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (AptAux render-set only)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"  // CgsGraphics::Im2d (the PROVEN immediate render path)
#include "rw/rwcore_structs.h"                                // rw::ResourceAllocatorRegistry::GetDefaultAllocator

// ---- the synchronous bundle load + the Apt-data resource -> geometry path ----
#include "GameShared/GameClasses/System/Resource/CgsResourcePool.h"         // CgsResource::Pool / Entry
#include "GameShared/GameClasses/System/Resource/CgsResourceBundleLoader.h" // CgsResource::BundleLoader
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistry.h" // CgsResource::ResolveResourceType
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeRegistration.h" // RegisterAllResourceTypes
#include "GameShared/GameClasses/System/Resource/CgsSmallResourcePS3.h"     // E_MEMTYPE_* / E_MEMTYPE_NUMTYPES
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptDataHeader.h"  // CgsGui::AptDataHeader (relocated movie header)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // AptCommunicator (the framework-bootstrap ordering gate)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiGeometryObjects.h" // CgsResource::GuiGeometryObject / File
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"  // CgsGui::GuiEventLoadNotification (the load-notification records)

// ---- STEP 1 FAITHFUL INSTANTIATION (gated; the direct-geometry render stays the fallback) ----
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation::Fixup (FixupTranscode/InPlace)
#include "SDKs/EATech/include/Apt/AptConstFile.h"             // AptConstFile (pointer-size dispatch)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile (synthesised loaded-file handle for instantiation)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"       // AptGetAnimationAtLevel (the root level-0 CIH)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"// MakeCharacterAnimationInst (loaded movie -> live inst)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the root display-object node)
#include "SDKs/EATech/include/Apt/AptMovie.h"                 // AptMovie::doFrameControls / resolve (timeline driver)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mDisplayList (the root CIH's child display list)
#include "SDKs/EATech/include/Apt/AptCharacterTextInst.h"     // SetTextValue/ClearStateFlags (the apt_labeltxt bridge)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // AptAnimationTarget::GetRootDisplayList (the director's root list)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // AptDisplayListState::mpFirst (the placed-node chain)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCurrentRenderTreeManager / gnCurrUpdateTick (StopMovie's removal op)
#include "SDKs/EATech/include/Apt/AptRenderTreeManager.h"     // AptRenderTreeManager::Update_ItemRemoved (StopMovie's park)

// ---- the Apt TEXT system (fonts + glyph batcher + localisation) --------------
// The PC stand-in for CgsGui::ViewModule's owned text sub-objects (GetFontCollection() /
// GetTextRenderer() / GetLanguageManager()): the bring-up constructs + loads them itself and
// hands them to AptAux::Construct so the Apt dynamic-text path lays out + draws visible glyphs.
#include "GameShared/GameClasses/Gui/View/CgsGuiFontCollection.h"   // CgsGui::FontCollection (typeface set)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"   // CgsGraphics::TextRenderer (glyph batcher)
#include "GameShared/GameClasses/Fonts/CgsFont.h"                   // CgsResource::Font (loaded typeface)
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h" // E_RESOURCETYPE_FONT
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"     // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Memory/CgsHeapMalloc.h"            // CgsMemory::HeapMalloc (language allocator)
#include <cstdlib>   // malloc (font pool backing, language file block)

// ---- the Apt allocator-pool globals (X360 off_8324D808 aliases) -------------
// Declared extern in the Apt TUs (AptGlobals.cpp defines them, null). Re-declared
// here (legal redundant declarations) so this TU can WIRE them off our pools -- the
// X360 AptInit that wires them is un-reconstructed, so the bring-up does its job.
// (gpNonGCPoolManager / gpGCPoolManager come from AptDefine.h, already included.)
extern DOGMA_PoolManager* gpAptOperandStackPool;   // off_8324D808 (operand-stack arrays)
extern DOGMA_PoolManager* gpAptPseudoDataPool;     // off_8324D808
extern DOGMA_PoolManager* gpAptRenderManagerPool;  // off_8324D808
extern DOGMA_PoolManager* gpAptSharedPtrPool;      // off_8324D808
extern DOGMA_PoolManager* gpAptSingleListPool;     // off_8324D808
// gpAptValueGCPool extern RETIRED (2026-08-11): off_8324D834's three homes were
// unified onto AptDefine.h's gpGCPoolManager; this never-referenced alias
// declaration pointed at the deleted void* view.

// The interpreter VM singleton (X360 &dword_8324E760) -- defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// The faithful per-frame dynamic-text refresh pass (AptCIHBehaviour.cpp): install
// AptCIH::ProcessTextInst as the generalised-process callback + walk the root subtree,
// so every dynamic-text node re-resolves its bound text + (re)lays it out through
// EnsureStringAllocated. The console runs this inside AptUpdate (sub_82B0D608).
struct AptCIH;
void AptRunGeneralisedTextProcess(AptCIH* pRoot);

// The Apt input-recorder/replay gate (X360 byte_82F733F7; defined in AptGlobals.cpp, default 0).
// When SHUT (0) a freshly-placed clip's play-head does NOT auto-advance (deterministic replay);
// when OPEN (1) fresh clips step + run doFrameControls, placing their nested content. Normal play
// runs with the gate OPEN -- the console opens it outside replay recording. The bring-up opens it
// so the imported sprite CONTAINERS recurse + place their nested shapes/images. // FLAG: set to the
// normal-play value (the replay-recorder subsystem that would toggle it is out of this slice).
extern unsigned char gbAptRecorderGate;   // byte_82F733F7

namespace
{
    using namespace BrnGui;

    // ---- run-state flags --------------------------------------------------------
    bool s_bAllocatorReady   = false;   // DOGMA + AptValueGC pools constructed + wired
    bool s_bInterpreterReady = false;   // AptActionInterpreter::initialize done
    bool s_bAuxReady         = false;   // AptAux::Construct done (gAptFuncs render slots live)
    bool s_bTargetReady      = false;   // an AptTarget context exists (FLAG: see below)
    bool s_bRuntimeReady     = false;   // the whole bring-up succeeded
   // ran the (idempotent) bring-up at least once

    CgsGui::ViewModule* s_pViewModule = nullptr;
    CgsMemory::LinearMalloc* s_pFlaptLinear = nullptr;   // the FLApt instance allocator (GuiModule-owned)

    // Apt render callbacks retain the view's canonical renderer set. Each
    // dispatch publishes the original main-renderer buffers into that set.

    // ---- the pending load-notification queue (the GuiResourceModule OUTPUT-BUFFER stand-in) ----
    // Every bundle the host's [PC IO] leaf loads queues one GuiEventLoadNotification per carried
    // resource here; GuiModule's frame bridge drains them into the view input buffer as view
    // events (14), and the REAL CgsGui::ViewModule::ProcessIncomingLoadNotification @0x8285BD30
    // performs the registration (AddAptData / LoadStringTable / AddFont) -- exactly the console
    // notification flow, with only the bundle IO itself host-side. FIFO ring; the records point
    // at RESIDENT pool entries, so a queued handle stays valid until drained.
    const u32 KU_MAX_PENDING_LOAD_NOTIFICATIONS = 96u;   // PERSISTENTAPT alone carries 61 AptData resources
    CgsGui::GuiEventLoadNotification s_aPendingLoadNotifications[KU_MAX_PENDING_LOAD_NOTIFICATIONS];
    u32 s_uPendingLoadNotificationWrite = 0;
    u32 s_uPendingLoadNotificationRead  = 0;
    u32 s_uNextLoadRequestId            = 0;

    // The language allocator and staged resources remain in this loader bridge for now;
    // the objects they prepare are owned by CgsGui::ViewModule.
    CgsMemory::HeapMalloc        s_AptLanguageAllocator; // backs the manager's element allocs
    CgsResource::Pool            s_AptLanguagePool;      // the resident LANGUAGE bundle pool
    // The AptAux data-handler allocator (AptAlloc/AptFree service the engine's
    // pfnMemFree + the data-handler loads through it). Host heap for the faithful
    // AptAux::Prepare drive (step 5). Host-sized: 256K OOM'd composing
    // Title_Screen02 + B5HelperComponents once the audited real allocation paths
    // went live (AptAlloc 16384-byte failure, boot-measured 2026-08-10); 8M holds
    // the full title-flow set with headroom.
    unsigned char                s_aAptDataHeap[8 * 1024 * 1024];
    CgsMemory::HeapMalloc        s_AptDataAllocator;
    bool s_bTextSystemReady = false;                     // fonts + strings loaded (one-shot)

    // The language allocator's backing heap: ~4.7K hash elements (24B each) + heap overhead.
    const u32 KU_LANGUAGE_HEAP_BYTES = 512u * 1024u;
    u8 s_aLanguageHeap[KU_LANGUAGE_HEAP_BYTES];

    // The language string-table bundle. FLAG: langid selection is host-static (0002 ==
    // langid 8, the clean English table -- verified offline: TITLES_PRESS_START ->
    // "Press START"); the console picks the bundle from the SKU/dash language.
    const char* const KC_APT_LANGUAGE_BUNDLE = "LANGUAGE/0002.bundle";

    // (The PER-MOVIE SLOTS -- framework/flow/persist/aux AptMovieSlot machinery, the
    // BF_LEGAL park latch and the per-slot tick pacing -- are RETIRED, slice 2 of the
    // runtime retirement: every movie mounts through the ENGINE chain now. BootPreload
    // plays "main" @ level 0 over channel-41 exactly like the console; AptLoadAnimation
    // -> AptLinker::Load / AptLinker::Update own the load + mount + unload, and the
    // PERSISTENTAPT component library stays a registered-data import library (the
    // GuiResourceModule's up-front bank), not a mounted movie.)

    // PHASE 2: the per-slot movie pool backings + the host bundle-IO leaf (AptLoadMovieSlot)
    // are RETIRED -- every movie slot's bundle now loads through the real GuiResourceModule's
    // [PC] servicer (CgsGuiResourceModulePC.cpp owns the streamed-apt bank), so the host no
    // longer carves its own resident movie pools here.

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).

    // ====================================================================================
    // FAITHFUL LOAD-PATH STATE.
    //
    // The load flow is the console notification chain with only the bundle IO host-side:
    //   AptLoadMovieSlot ([PC IO] BundleLoader) -> queued GuiEventLoadNotification(s) ->
    //   GuiModule's bridge drains them as view events (14) -> the REAL ViewModule::
    //   ProcessIncomingLoadNotification @0x8285BD30 -> AptDataHandler::AddAptData; then
    //   the play-movie event drives DriveFaithfulLoad: FindAptData -> AptLoader::Load ->
    //   AptCallbackFile::LoadAnimation -> AptCompleteAnimationAsyncLoad -> CompleteLoad
    //   -> Resolve -> Fixup -> AptGetAnimationAtLevel -> MakeCharacterAnimationInst.
    // The movie root locates inside CompleteLoad (const chunk movieOffset @+0x18, the
    // XB1-attested native-8 formula); the resolve64 bounds derive inside LoadAnimation
    // from the header's size slot. The old byte-pattern root locator, the
    // gAptLoadAnimRootOverride / span pokes, and the direct AddAptData host calls are
    // RETIRED (2026-07-09, the step-5 load-ownership move).
    //
    // DEFENSIVE: the whole path is heavily guarded; ANY failure leaves the slot's
    // mbInstantiated false and the game up.
    // ====================================================================================

    // (The faithful-load state -- char-anim def base, AptFile handle, root CIH,
    //  attempted/instantiated flags -- moved into AptMovieSlot above: each of the two
    //  movies owns its own copy, keyed off the slot the load path is driven with.)
    // STAGED VM EXECUTION (2026-07-01): the deferred-action DRAIN (AptAnimationTarget::
    // RunActions after each tick). The full chain is wired + verified live -- with this
    // false the queue fills faithfully (289 enqueues to frame 100, boot green); with it
    // true the first real bytecode EXECUTES (45 ops traced: pushes, Stop, a taken
    // BranchIfTrue) but the run dies at frame ~2 in the byte-push family: a sub-clip
    // stream indexes the movie string DICTIONARY (interp mpRegisters) before any
    // DefineDictionary (0x88) stream has installed it. On the console the ordering
    // guarantee comes from the clip-event queue chain (AptCIH_queueClipEvents -- still
    // the deferred link-cluster stub) + init-action sequencing. Flip to true once that
    // cluster is homed. // (RESOLVED 2026-07-01 -- see below; the switch is retired.)
    //
    // STATUS 2026-07-01 (FINAL): the drain is PERMANENTLY ON. The frame-3 crash was a
    // straddled (never-relocated) tag-1 stream slot from a deep-nested movie (the
    // converter 4-byte-straddle family) dereferenced into a wild runStream PC -- now
    // guarded at both the enqueue (queueFrameActions) and the drain (RunActions). With
    // the CONVERTER-FIXED bundles (branch offsets corrected, 2026-07-01) the full VM
    // runs steady-state: 1243 tick+drain frames, 0 asserts, the executed stop()s hold
    // the clips faithfully.
    bool   s_bDisableRunActions    = false;

    // ---- the faithful timeline PACING (console AptUpdate/sub_82B0D608) ------------------
    // The console accumulates the ELAPSED MILLISECONDS into the root anim inst (charInst+36)
    // and ticks the display list once per movie msPerFrame (character+44 -- TITLE_SCREEN02
    // authors 33ms == 30fps), catch-up looping while the accumulator holds a full frame;
    // RunActions runs per TICK and the generalised text/mask process per UPDATE. Without
    // this the movie ticked once per RENDER frame (~170fps) and every transition played
    // ~5.7x too fast (a blink instead of an animation). The pacing state (msPerFrame,
    // accumulator, last-update clock, tick counter) is PER SLOT (AptMovieSlot above):
    // each movie ticks on its OWN authored clock, exactly as the console paces each
    // level's root anim inst separately.

    // (The import content-load registry + AptLoaderStartAsyncLoad -- the PC body
    // GameShared/GameClasses/Gui/PC/CgsAptStreamLoaderPC.cpp.)

    // NOTE: the Apt allocator/interpreter/target bring-up (the invented Steps 1/2/5 that
    // used to live here -- a local AptAllocatorInitialize + WireAllocatorGlobals + the
    // interpreter init + AptCreateTargetInstance) is RETIRED: it is now the faithful
    // CgsGui::AptAux::InitializeApt @0x82848E50 (CgsAptAux.cpp), which chains the homed
    // AptAllocatorInitialize/AptUpdateInitialize/AptRenderInitialize/AptCreateTargetInstance/
    // AptChangeTargetInstance (SDKs/EATech/Apt/AptInit.cpp). PrepareRuntime() below drives
    // it after AptAux::Construct has bound the canonical view renderer set.
}

namespace BrnGui
{
    // Forward declarations (definitions are `static` later in this BrnGui namespace; called
    // from PlayMovie / Update which appear before their definitions). Must be in BrnGui (not
    // the anon namespace) so the in-namespace calls bind to the static definitions.
    // (The old x64 byte-pattern root locator is RETIRED (2026-07-09):
    // AptLoader::CompleteLoad's faithful native-8 root location -- the const chunk's
    // movieOffset @const+0x18, the XB1-attested formula -- covers every bundle, so the
    // locator and its gAptLoadAnimRootOverride poke are gone. The DumpResourceBytes hex
    // probes went with them.)
    // (STEP 4 nested-content dirty propagation retired 2026-07-04 -- the AptCIH ctor births
    // fresh sprite/animation children dirty, so no host propagation pass is needed. See the
    // per-frame tick in UpdateRuntime.)

    // (AptLoadOneGuiFont RETIRED, slice 4a: the locale fonts load through the REAL
    // cache/module chain -- GuiModule::Prepare stage 13's font table {17,16},{18,16},
    // {19,16} -> GuiCache -> GuiResourceModule font bank -> type-16 notifications ->
    // ViewModule::AddFont. QueueLoadNotification below still carries the language
    // string-table record until the language request path is recovered.)

    // Queue one load notification for a loaded pool entry (see the ring's note above).
    // liRequestType carries the X360 ARTIST request-type numeric the view module's dispatch
    // switches on (12 = the localised-text bundle -- the one remaining host-queued record).
    static void QueueLoadNotification(CgsResource::Entry* lpEntry, s32 liRequestType)
    {
        if (s_uPendingLoadNotificationWrite - s_uPendingLoadNotificationRead
                >= KU_MAX_PENDING_LOAD_NOTIFICATIONS)
        {
            CgsDev::Log::WriteToLog("[AptRT] notify: pending load-notification ring FULL -- "
                                    "record dropped (FLAG).\n");
            return;
        }
        CgsGui::GuiEventLoadNotification& lrEvent = s_aPendingLoadNotifications[
            s_uPendingLoadNotificationWrite % KU_MAX_PENDING_LOAD_NOTIFICATIONS];
        lrEvent.mResourceHandle.mpResourceMemory =
            &lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        lrEvent.mResourceHandle.mpSourceEntry = lpEntry;
        lrEvent.meRequestType   = static_cast<CgsGui::ResourceRequestTypes>(liRequestType);
        lrEvent.muLoadRequestId = ++s_uNextLoadRequestId;
        ++s_uPendingLoadNotificationWrite;
    }

    // -------------------------------------------------------------------------
    // AptLoadLanguageBundle -- load the staged LANGUAGE string-table bundle through the
    // REAL resource chain: BundleLoader -> the Language (0x27) resource -> the registered
    // LanguageResourceType::FixUp relocation -> a queued load notification (request
    // type 12), which the real ViewModule::ProcessIncomingLoadNotification routes to
    // LanguageManager::LoadStringTable @0x828664B8. Only the synchronous bundle IO is
    // host-side ([PC IO] -- the X360 streams it via the GUI resource module). The pool
    // stays resident (the manager stores the relocated string POINTERS).
    // -------------------------------------------------------------------------
    static bool AptLoadLanguageBundle(const char* lpcBundlePath, CgsResource::Pool* lpPool)
    {
        const u32 KU_LANG_POOL_BYTES = 4u * 1024u * 1024u;
        void* lpMainMem = malloc(KU_LANG_POOL_BYTES);
        if (lpMainMem == 0)
            return false;

        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId    = 6;
        lOptions.mpcName = "AptLanguage";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 64u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_LANG_POOL_BYTES - 64u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = lpMainMem;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_LANG_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 16u;
        lOptions.muMaxImports           = 16u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        lpPool->InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lpcBundlePath, lpPool, CgsResource::ResolveResourceType);
        char lac[224];
        if (liLoaded <= 0)
        {
            std::snprintf(lac, sizeof(lac), "[AptRT] text: language bundle '%s' FAILED to load.\n",
                          lpcBundlePath);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        s32 liIndex = -1;
        CgsResource::Entry* lpEntry = lpPool->FindFirstResourceOfType(0x27u, &liIndex);
        if (lpEntry == 0 ||
            lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY] == 0)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] text: '%s' has no Language (0x27) resource.\n", lpcBundlePath);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        QueueLoadNotification(lpEntry, 12);
        std::snprintf(lac, sizeof(lac),
            "[AptRT] text: language '%s' loaded -- string-table notification queued.\n",
            lpcBundlePath);
        CgsDev::Log::WriteToLog(lac);
        return true;
    }

    // -------------------------------------------------------------------------
    // PrepareRuntime -- the once-only host bring-up (idempotent). Mirrors the
    // X360 CgsGui::AptAux::InitializeApt @0x82848E50 + AptAllocatorInitialize
    // @0x82ADD118, but only the pieces whose engine bodies exist; every step that
    // crosses an un-homed engine routine is // FLAG'd and skipped defensively.
    // -------------------------------------------------------------------------
    bool PrepareAptRuntime()
    {
        if (s_pViewModule == nullptr)
            return false;
        if (s_bRuntimeReady)
            return true;
        // Idempotent: each step is guarded by its own *Ready flag, so re-entry only runs
        // the preparation steps that have not yet succeeded. Log "begin" only on the
        // first attempt to avoid per-frame spam.
        CgsDev::Log::WriteToLog("[AptRT] bring-up: begin.\n");

        // ---- STEP 4: AptAux::Construct (the host callback table + render handler) -
        // Build the ImRendererSet (3D buffers arrive from the frame renderer) and
        // hand it to AptAux::Construct, which seeds the render handler + installs the
        // gAptFuncs render-callback family (the engine's render dispatch reaches our
        // AptRenderHandler::Render through it).
        if (!s_bAuxReady)
        {
            // ViewModule::Construct already gave AptAux the address of its own
            // renderer set. Its original null slots remain until Render publishes
            // the owner-supplied buffers; runtime preparation needs no private buffer.
            // Bring the TEXT system up first (fonts + language + glyph batcher) so the
            // handler's text-layout inputs are live from the start. One-shot; device-gated.

            CgsGui::AptAux* lpAptAux = s_pViewModule->GetAptAux();
            s_bAuxReady = (CgsGui::AptAuxPointer::mpAptAuxInst == lpAptAux);

            char lac[200];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step4 aux: Construct done. singleton=%p renderer-set=%p\n",
                (void*)CgsGui::AptAuxPointer::mpAptAuxInst,
                (void*)s_pViewModule->GetImRendererSet());
            CgsDev::Log::WriteToLog(lac);
        }

        // ---- STEP 5: the FAITHFUL Apt runtime bring-up (AptAux::InitializeApt) ----
        // This retires the invented Steps 1/2/5 (allocator wiring, interpreter init,
        // target create) with the single faithful CgsGui::AptAux::InitializeApt @0x82848E50,
        // homed in CgsAptAux.cpp. It runs, in the console's exact order:
        //   AptAllocatorInitialize(0x10000,0x4000,0x10000,0x4000)  (the pools + wiring)
        //   AptUpdateInitialize(v8, 0)                             (config + AS interpreter)
        //   AptRenderInitialize(updated)                           (clip stack + render pool)
        //   AptCreateTargetInstance(v7) + AptChangeTargetInstance  (the live director context)
        // (the ext-object phase is FLAG'd inside InitializeApt -- it needs the deferred
        //  AptValueInitialize singletons). Matches the X360 lifecycle: AptAux::Construct
        //  (above, at "ctor" time) then AptAux::Prepare -> InitializeApt.
        //
        // Requires AptAux::Construct to have published the singleton (step 4). Idempotent:
        // once a live GetTarget() exists the runtime is already up (do not re-run).
        if (!s_bAllocatorReady)
        {
            if (!s_bAuxReady)
            {
                CgsDev::Log::WriteToLog("[AptRT] step5 InitializeApt: DEFERRED -- AptAux::Construct not done "
                                        "(view construction not complete).\n");
            }
            else if (GetTarget() != nullptr)
            {
                // Already initialised (a prior pass / another caller). Mark up.
                s_bAllocatorReady   = true;
                s_bInterpreterReady = true;
                s_bTargetReady      = true;
                CgsDev::Log::WriteToLog("[AptRT] step5 InitializeApt: already up (GetTarget() live).\n");
            }
            else
            {
                CgsDev::Log::WriteToLog("[AptRT] step5 InitializeApt: calling AptAux::InitializeApt "
                                        "(alloc + update + render + target) ...\n");
                // Drive the faithful AptAux::Prepare state machine (@0x828503E0): the
                // data-handler prepare (giving AptAlloc/AptFree a real heap), then
                // InitializeApt, then the miState0=3 seed AptAux::Update asserts on.
                // Driven through the REAL staged CgsGui::ViewModule::Prepare
                // (retirement slice 6; the mbIsNewModule store makes the base
                // stage fall through, matching the console new-module contract).
                s_AptDataAllocator.Construct(s_aAptDataHeap,
                                             static_cast<s32>(sizeof(s_aAptDataHeap)));
                s_AptLanguageAllocator.Construct(s_aLanguageHeap,
                                                 static_cast<s32>(KU_LANGUAGE_HEAP_BYTES));
                while (!s_pViewModule->Prepare(&s_AptDataAllocator, nullptr,
                                               &s_AptLanguageAllocator, s_pFlaptLinear))
                {
                }

                // Reflect the faithful init into the facade's step flags (for the channel-41
                // readiness gate + the log). InitializeApt wires the pools + interpreter and
                // publishes the target into GetTarget().
                s_bAllocatorReady   = (gpAptOperandStackPool != nullptr);
                s_bInterpreterReady = s_bAllocatorReady;
                AptTarget* lpNow    = GetTarget();
                s_bTargetReady      = (lpNow != nullptr);
                char lac[220];
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] step5 InitializeApt: done. pools=%s GetTarget()=%p%s\n",
                    s_bAllocatorReady ? "wired" : "NULL(FLAG)", (void*)lpNow,
                    s_bTargetReady
                        ? " (director+loader+linker live)"
                        : " (target null -- FLAG; see the InitializeApt/AptTarget path)");
                CgsDev::Log::WriteToLog(lac);
            }
        }

        // The runtime is "ready" (for channel-41 routing) once the allocator and
        // AptAux host are up through the original view preparation. The
        // target being live lets PlayMovie + the per-frame tick reach a real GetTarget().
        s_bRuntimeReady = s_bAllocatorReady && s_bAuxReady;
        CgsDev::Log::WriteToLog(s_bRuntimeReady
            ? (s_bTargetReady
                ? "[AptRT] bring-up: READY (InitializeApt: alloc+interp+render+TARGET up; GetTarget() live).\n"
                : "[AptRT] bring-up: READY (InitializeApt: alloc+interp+render up; target FLAG'd).\n")
            : "[AptRT] bring-up: INCOMPLETE -- see step probes above.\n");
        return s_bRuntimeReady;
    }

    // (EnsureFrameworkMovie / PlayRuntimeMovie / DriveFaithfulLoad RETIRED, slice 2:
    // the engine linker mounts every movie -- see the slice note at the top. The
    // registered-data/bundle-IO completion (LoadImportBundle) lives on as the
    // body of the real AptLoaderStartAsyncLoad platform hook.)

    // PropagateDirtyToChildren (STEP 4 dirty-propagation stand-in) was RETIRED 2026-07-04: the real
    // mechanism is the AptCIH ctor (@0x82B00638) dirtying a freshly-created sprite/animation node
    // (SetDirtyState(true,true), AptCIH.cpp), so the root tick's own child-list recursion composes the
    // whole subtree per frame. The batch-dirty walk that used to live here is no longer needed.

    // -------------------------------------------------------------------------
    // RETIRED (2026-07-01): the invented direct-geometry render (RenderLoadedGeometryDirect +
    // ResolveMeshTexture) is GONE -- the movie renders through the real display-list ->
    // render-tree -> AptRenderHandler::Render -> ImRenderBuffer -> D3D9 path.
    // RETIRED (2026-07-09): the host render DRIVE (RenderRuntime's per-slot AptRender walk +
    // its s_bMovieStopped layer gating) moved to the real chain -- GuiModule::Render ->
    // CgsGui::ViewModule::Render @0x82858810 -> RenderInternal @0x82858AF8 -> AptAux::Render
    // @0x82848FB8 -> AptRenderTarget @0x82AF4ED0 (every layer, the engine's consumed-tick
    // bank). The main renderer publishes and consumes the original Im2d bank.
    // -------------------------------------------------------------------------

    // RETIRED (2026-07-09, step 6): the component view-state/key-value bridge, the
    // title help-item defaults, and their clip-walk helpers are GONE -- the REAL
    // component framework drives the menu (onLoad -> BuildName -> RegisterComponent
    // -> AddNewAptComponent; per-frame AptAux::UpdateComponents -> AptCommunicator::
    // UpdateAllComponents -> the movie AS UpdateAll -> GetComponentData).

    // Pop one queued load notification (the language string-table record). The
    // module bridge drains this ahead of the view Update. False when empty.
    bool PopPendingAptLoadNotification(CgsGui::GuiEventLoadNotification* lpOut)
    {
        if (s_uPendingLoadNotificationRead == s_uPendingLoadNotificationWrite)
            return false;
        *lpOut = s_aPendingLoadNotifications[
            s_uPendingLoadNotificationRead % KU_MAX_PENDING_LOAD_NOTIFICATIONS];
        ++s_uPendingLoadNotificationRead;
        return true;
    }

    bool AptFlowMovieLive()
    {
        // FLOW semantics: BootLegal drives this query off its channel-41 movie at
        // display level 1. Engine-native since the AptLoadAnimation retirement: the
        // linker mounts the flow movie's anim inst onto the level-1 node -- live ==
        // that node exists with a bound character inst (SetCharacterInst ran).
        AptCIH* lpNode = AptFindAnimationAtLevel(1);
        return lpNode != nullptr && lpNode->GetCharacterInst() != nullptr;
    }

    // True once the movie has COMPOSED: the root clip's first paced tick has run its
    // frame-0 place commands, so the child display list is populated (the PLACE-named
    // clips the view-state bridge targets exist). The console's equivalent gate is the
    // GuiCache apt-component handshake (AreAllAptComponentsInitialised): a component
    // reports initialised only once its clip is placed.
    bool AptFlowMovieComposed()
    {
        // FLOW semantics (BootLegal's compose gate): composed == the mounted level-1
        // movie's first paced tick ran its frame-0 place commands (child display list
        // non-empty). Engine-native read of the same node IsRuntimeMovieLive probes.
        AptCIH* lpRoot = AptFindAnimationAtLevel(1);
        if (lpRoot == nullptr)
            return false;
        AptCharacterInst* lpCI = lpRoot->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return false;
        AptDisplayListState* lpState =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        return lpState != nullptr && lpState->mpFirst != nullptr;
    }
}

namespace BrnGui
{
    GuiModule*      gpActiveGuiModule      = 0;

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
    void GuiModule::Construct(const BrnResource::HudMessageController* lpHudMessageController,
                              const BrnResource::PopupController* lpPopupController,
                              bool lbHighDef)
    {
        // ARTIST 0x82518054: register GUI profiling handles before any view updates.
        // The registry is static; the initializer does not use its receiver.
        GuiPerfmons().Initialise();

        // X360 GuiModule::Construct @0x82518028, pseudocode lines 327-332 -- the console's
        // own argument assert, fired before anything is built.
        CGS_ASSERT(lpHudMessageController != 0, "lpHudMessageController");   // BrnGuiModule.cpp:229
        CGS_ASSERT(lpPopupController != 0, "lpPopupController");

        // Route through the real BrnGui::ViewModule::Construct @0x824F13B8 with the X360
        // caller's recovered args: the view flapt count (7), a 16:9 aspect, and the real
        // alternate-text-colours table + count 8 (see the static above). The
        // X360 passes a null debug name here; the descriptive "BrnGuiView" is a harmless
        // non-null label the base accepts.
        mViewModule.Construct(this, "BrnGuiView", 7, 1280.0f / 720.0f,
                              KA_ALTERNATE_TEXT_COLOURS, 8);
        // ARTIST825184E4: construct the actual shared Model and stored-event
        // owner. Native preparation/update selection remains with its caller.
        CgsGui::GuiModule::Construct(&mViewModule, lbHighDef);
        mViewModule.GetFlaptManager()->SetSoundTriggerHandler(
            &GuiModule::FlaptSoundTriggerCallback, this);
        // X360 GuiModule::Construct @0x82518B18: EffectsArbitrator::Construct just ahead of
        // MovieManager::Construct + ColourCalibrationScreen::Construct.
        muFrameUpdateSet = 0;
        mEffectsArbitrator.Construct();
        mMovieManager.Construct();
        // ARTIST82518B88 initializes the actual boot-movie owner byte.
        mbBootMovieMode = true;
        // ARTIST82518858: distinct from primary Prepare and release stages.
        mePrepareStage = E_PREPARE_START;
        muPrepare2State = 0;
        meReleaseStage = E_RELEASE_DONE;
        // X360 GuiModule::Construct @0x82518B18-24: MovieManager::Construct is immediately
        // followed by ColourCalibrationScreen::Construct (gm+301600 then gm+306752), with
        // EffectsArbitrator::Construct just ahead of the pair.
        mColourCalibrationScreen.Construct();
        // ARTIST82518028: these values seed the collision-world and car-pool validators.
        meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalid = false;
        meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
        mbCollisionWorldInvalidateForNetworking = false;
        mbCollisionWorldValidateForNetworking = false;

        // ---- [profile-save] THE AUTOSAVE LATCH'S OWN INITIALISERS -------------------------
        // X360 GuiModule::Construct @0x82518028, the run that immediately follows
        // ColourCalibrationScreen::Construct (pseudocode lines 413..421):
        //     *(gm + 1628216..1628220) = 0;      // the four sibling bytes (not landed here)
        //     *(gm + 1628224)          = 0.0;    // mfLastProfileAutosaveTime
        //     *(gm + 1628228)          = 0;      // mbProfileAutosavePending
        // ⛔ These two were MISSING from the pair's landing commit, which left the tail's
        // `if (mbProfileAutosavePending)` reading an indeterminate byte on the first Update.
        mfLastProfileAutosaveTime = 0.0f;
        mbProfileAutosavePending  = false;
        // The console keeps the FORCE flag in a per-Update stack byte it zeroes at the top of
        // Update (@0x82527A58, `v142 = 0` before the event walk). This build carries it as a
        // member (the walk and the tail are two functions), so it gets the same defined start
        // here; the tail clears it again at the end of every Update.
        mbForceProfileAutosave    = false;

        mAlwaysAvailableComponentsManager.Construct();

        // X360 GuiModule::GuiModule @0x827E5B28 constructs the custom-renderer manager as a
        // by-value member; GuiModule::Construct @0x82518028 then drives its Construct.
        // ⭐ This is the object the Apt custom-control render callback reaches through
        // AptRenderHandler::mpCustomRendererManager -- until this leg NOTHING in the PC
        // build instantiated it, so `_type='PlayerImage'` on the licence card (and every
        // other custom control) had nowhere to resolve to.
        // ARTIST82518BB0..BD0: bind this receiver's own backing with 1024/16, then Clear.
        mEventReceiverQueue.Construct();
        // ARTIST82518BD4..C0C: the collision validator has its own 1024/16 receiver.
        mCollisonPoolValidationReceiverQueue.Construct();
        // ARTIST82518BF0..C0C: the real receiver owns its backing storage.
        mGDMReceiverQueue.Construct();
        gbGuiToolStartHookRequested = false; // ARTIST82518C38..82518C44
        mhSysListenerHandle = 0; // ARTIST82518C48, before serialiser Construct
        mGuiModuleSerialiser.Construct(); // ARTIST82518C4C, before manager Construct
        mCustomRendererManager.Construct();

        // ⭐ X360 GuiModule::Construct's LAST TWO ACTIONS, @0x82518D2C-54:
        //     addis r30, r31, 5 ; addi r30, r30, -0x3D70   ; r30 = gm + 311952 (the manager)
        //     mr r3, r30 ; lwz r11,0(r30) ; lwz r11,0(r11) ; mtctr r11 ; bctrl   ; its Construct
        //     addis r4, r31, 5 ; mr r3, r30 ; addi r4, r4, -0x6E80
        //     bl BrnGui__CustomRendererManager__SetFlaptRenderer
        // The argument is gm + 327680 - 28352 - 64... i.e. +299392, which is
        // mViewModule (gm+132224) + mFlaptManager (view +0x28CC0 == +167104) + mRenderer
        // (+0x40): 132224 + 167104 + 64 == 299392 exactly. (The sibling constant at
        // @0x82518C6C, gm+299328, is the FlaptManager itself -- the `this` the
        // SetSoundTriggerHandler call above uses.)
        // ⛔ THIS CALL WAS MISSING, and the setter it drives was writing the wrong field
        // (see BrnCustomRenderer.cpp), so NetworkPlayerImageRenderer::mpFlaptRenderer had
        // ZERO writers in this build. It is read by SwapBuffers, which the newly-wired
        // GuiModule::EndOfFrame runs every frame -- without this line that is a
        // "mpFlaptRenderer" assert per frame, not a fix.
        mCustomRendererManager.SetFlaptRenderer(&mViewModule.GetFlaptManager()->mRenderer);

        mpGuiEventInputBuffer = 0;
        mpOutputBuffer = 0;
        mGuiConfig.mpTextureAllocator = 0;   // retained selected PC initialization
        mGuiConfig.mpRendererAllocator = 0;  // retained selected PC initialization
        mbCustomRenderersPrepared = false;   // [licence-icon] the manager staged-prepare latch

        // The flow-controller chain (the X360 Construct's flow set): the cache Construct
        // (the watcher reset @0x82505860 -> 0x824FD978), then the profile manager (X360
        // GuiModule::Construct @0x82518028 hands it the cache, the sign-in watcher, and
        // the view module's language manager), then the flows against the cache; the
        // controller starts UNLOADED on every flow slot.
        // ⭐⭐ THE SAT-NAV TRACKER, Constructed BEFORE the cache exactly as the console does.
        // X360 GuiModule::Construct @0x82518028 emits the tracker's inlined Construct at
        // @0x82518874..@0x825188C8 (off r29 == gm+1131872) and only then calls
        // GuiCache::Construct @0x82518948 with `mr r4, r29`. Order matters here for the same
        // reason it does on the console: the cache's own "Invalid tracker pointer" assert
        // (BrnGuiCache.cpp:1226) fires on a pointer that is handed over before its object is
        // initialised, and every reader downstream (GetGuiTracker) gets the live one.
        mGuiTracker.Construct();

        // X360 @0x8251893C..@0x82518948, verbatim:
        //     mr r5, r26   ; the module's CgsGui::SystemUserProfile @gm+949152
        //     mr r4, r29   ; the module's GuiTracker              @gm+1131872
        //     mr r3, r27   ; the GuiCache                         @gm+1005376
        //     bl BrnGui::GuiCache::Construct
        // ⭐ This is the WRITER of GuiCache::mpGuiTracker (+0x4054) the tree never had.
        mGuiCache.Construct(&mGuiTracker, &mSystemUserProfile);

        // X360 GuiModule::Construct @0x82518028, the two lines that follow GuiCache::Construct
        // and MapIconManager::Construct: Construct the module's own WorldDataController (the
        // seven inlined stores at gm+307836..+309028 + the receiver-queue Clear) and bind it
        // into the cache (`*(gm + 1021860) = gm + 307836`, guarded by the "lpController"
        // assert at BrnGuiCache.h:2310). This is the pointer every GUI component that resolves
        // a car / landmark / event through GuiCache::GetWorldDataController reaches.
        mWorldDataController.Construct();
        mGuiCache.SetWorldDataController(&mWorldDataController);

        // [H3b] X360 GuiModule::Construct: MapIconManager::Construct(gm+1088304, cache)
        // right after the cache/world-data wiring, then the cache binding + the sat-nav
        // view-rect pick + the mask matrix build (the @0x82518A2C..@0x82518A64 run).
        mMapIconManager.Construct(&mGuiCache);
        mGuiCache.SetMapIconManager(&mMapIconManager);
        // ⭐ THE HIGH-DEFINITION BYTE (issue #11). X360 @0x82518A24: `*(gm + 1024649) = a6`
        // -- 1024649 - 1005376 == 19273 == GuiCache +0x4B49, the byte BootLegal::Update,
        // MainMapComponent::Construct, CrashNavMapMain, CrashNavDriverDetails and
        // RoadSignIconManager all read to choose between their HD and SD constants. This
        // store is the byte's ONLY writer in the image; without it the cache said SD.
        mGuiCache.SetIsHighDefinition(lbHighDef);
        {
            std::snprintf(lacLine, sizeof(lacLine),
                          "[GuiModule] high-definition=%d (GuiCache +0x4B49; display %dx%d)\n",
                          lbHighDef ? 1 : 0,
                          static_cast<int>(renderengine::gDisplayWidth),
                          static_cast<int>(renderengine::gDisplayHeight));
            CgsDev::Log::WriteToLog(lacLine);
        }
        // @0x82518A2C..A64: the sat-nav view rect pick, `a6 ? unk_82FB30A0 : unk_82FB3130`
        // stored to the live rect @0x82FB36A0.
        MapTransform::SetSatNavRect(lbHighDef ? MapTransform::GetSatNavViewRectHD()
                                              : MapTransform::GetSatNavViewRectSD());
        SetMaskAspectCorrectionMatrix(&mGuiCache);

        // ⭐ [stuntrace] THE FREEBURN-CHALLENGE MANAGER HAND-OFF -- the missing writer of
        // GuiCache::mpChallengeManager. X360 GuiModule::Construct @0x82518028, the two lines
        // that follow the mask-matrix build and the BurnoutSkillsManager pair, verbatim:
        //     BrnGui::FreeburnChallengeManager::Construct(gm + 309584, gm + 1005376);
        //     if (gm == -309584) { ... FireAssert("lpChallengeManager",
        //                          "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2374); ... }
        //     *(gm + 1021868) = gm + 309584;          // == GuiCache +0x406C
        // (1021868 - 1005376 == 16492 == 0x406C, the member BrnGuiCache.h:1105 pins.) The
        // setter carries the console's own assert, so nothing is restated here.
        //
        // This is the GUI-OBJECT half of the freeburn-challenge chain and is SEPARATE from the
        // challenge-LIST data half (GameDataModule streams OnlineChallenges.bndl and
        // WorldDataController publishes the list GuiCache::GetFreeburnChallengeList returns).
        // The list landed first; this pointer had no producer at all, which is what made
        // RaceMainHudState::UpdateRunning's case-6 arm (BrnRaceMainHudState_wS3.cpp:261, an
        // UNCONDITIONAL GetFreeburnChallengeManager per controller-input event) fire
        // "mpChallengeManager" once per input frame for the whole of a live stunt race.
        //
        // The console's BurnoutSkillsManager pair sits between the mask matrix and this one:
        // the module's skills manager is constructed against the cache and bound as
        // GuiCache::mpSkillsManager (the "lpSkillsManager" assert is the setter's).
        mBurnoutSkillsManager.Construct(&mGuiCache);
        mGuiCache.SetSkillsManager(&mBurnoutSkillsManager);
        mFreeburnChallengeManager.Construct(&mGuiCache);
        mGuiCache.SetChallengeManager(&mFreeburnChallengeManager);

        // X360 GuiModule::Construct wires the shared state-access bundle here, before
        // any flow is allowed to run: ViewModule::GetAptAux/GetLanguageManager,
        // ViewModule::GetFlaptManager, and this GuiCache are the four live owners.
        // AptAux itself is installed during Prepare below because the PC runtime host
        // constructs that singleton there; the other three owners already exist.
        mGuiAccessPointers.Construct();
        mGuiAccessPointers.mpLanguageManager = mViewModule.GetLanguageManager();
        mGuiAccessPointers.SetFlaptManager(mViewModule.GetFlaptManager());
        mGuiAccessPointers.SetGuiCache(&mGuiCache);
        // ARTIST82518CE8/D24: publish the actual contained receiver and serialiser.
        // The serialiser's static buffer is allocated by the replay owner later.
        mGuiAccessPointers.SetGDMReceiverQueue(&mGDMReceiverQueue);
        mGuiAccessPointers.SetSerialiser(&mGuiModuleSerialiser);

        // ARTIST82518798/A0/A8: the profile watcher owns these three initial stores.
        mSystemUserProfile.Construct();
        mProfileHost.GetProfileManager().Construct(mGuiCache, mSystemUserProfile,
                                  mViewModule.GetLanguageManager());
        // ARTIST825187CC/D0/D4: real contained keyboard and listener host.
        mGuiKeyboard.Construct();
        mKeyboardHost.Construct();
        mScreenFlow.Construct(&mGuiCache);
        mHudFlow.Construct(&mGuiCache);
        mOverlayFlow.Construct(&mGuiCache);
        // ARTIST82518570/580/590/5A0: the four real observer owners, event0.
        mGuiModel.AddGuiEventObserver(&mHudFlow, 0);
        mGuiModel.AddGuiEventObserver(&mScreenFlow, 0);
        mGuiModel.AddGuiEventObserver(&mOverlayFlow, 0);
        mGuiModel.AddGuiEventObserver(&mAlwaysAvailableComponentsManager, 0);
        mFsmController.Construct();

        // ARTIST82518720 and the inlined overlay Construct: both directors receive the real contained
        // Model. Their original publishers are restored together (CP16).
        mHudMessageDirector.Construct(&mGuiModel, &mGuiCache);
        mHudMessageAnalyzer.Construct(&mHudMessageDirector);
        mOverlaysDirector.Construct(&mGuiModel);

        // ARTIST825187D8..82518818: actual debug subobjects, after their real director owner.
        // Their C++ constructors installed only vtables/queue flags; this is their data phase.
        mDebugComponent.Construct(this);
        mHudMessageDebugComponent.Construct(&mHudMessageDirector);
        mSatNavDebugComponent.Construct(this);
        mGuiCacheDebugComponent.Construct(this);

        // ⭐ [gateui r4] THE CONTROLLER HAND-OFF -- the missing writer of
        // GuiCache::mpHudMessageController. X360 GuiModule::Construct @0x82518028
        // pseudocode lines 362-368, IMMEDIATELY before the director store:
        //     if ( !a2 ) { BeginAssert; FireAssert("lpController",
        //                  "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2405); EndAssert; }
        //     *(a1 + 1021872) = a2;                     // == GuiCache +0x4070, mpHudMessageController
        //     if ( a1 == -639264 ) { ... "lpDirector", BrnGuiCache.h, 2433 ... }
        //     *(a1 + 1021876) = a1 + 639264;            // == GuiCache +0x4074, mpHudMessageDirector
        // (1021872 - 1005376 == 16496 == 0x4070 and 1021876 - 1005376 == 16500 == 0x4074,
        // the two members BrnGuiCache.h:917/918 pin -- so the console's order really is
        // controller THEN director.) The setter carries the console's assert already, so the
        // CGS_ASSERT below is the ARGUMENT one (BrnGuiModule.cpp:229) re-stated at the point
        // of use rather than a second copy of BrnGuiCache.h:2405.
        CGS_ASSERT(lpHudMessageController != 0,
                   "lpHudMessageController (GuiCache::mpHudMessageController would stay null "
                   "and every HUD message would die at FilterAndSendOffMessage's mpController "
                   "assert)");
        mGuiCache.SetHudMessageController(lpHudMessageController);

        mGuiCache.SetHudMessageDirector(&mHudMessageDirector);
        // The popup controller: the console's third argument, stored right after the
        // director (GuiCache +0x4078).
        mGuiCache.SetPopupController(lpPopupController);

        // The REAL GUI resource-loading module + its persistent IO pair (replaces the
        // host FSM-bundle stand-in). Construct the IO buffers (their embedded queues come
        // up here) and the module. The HD flag is the console's own argument (the same a6
        // the cache byte above took): it picks GuiApt\ vs GuiAptSD\, FLAPTHUD vs FLAPTHUDSD
        // and Fonts\ vs FontsSD\ (the FSM bundle path itself is HD-independent). Construct
        // seeds the module counters/stages + marks it a new-module type (its base Prepare
        // then skips the old-module IO-structure lock path -- no assert).
        mResourceInputBuffer.Construct();
        mResourceOutputBuffer.Construct();
        mGuiResourceModule.Construct(lbHighDef);

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
        // ⭐ THE EVENT-64 CACHE HAND-OFF -- the console's own tail of GuiModule::Construct,
        // two lines before its GuiAccessPointers block:
        //     v102[0] = a1 + 1005376;                              // the GuiCache
        //     BrnGui::GuiTracker::RecEvent(a1 + 1131872, v102, 64, 4);
        // (The 4 is the X360's 32-bit pointer size; RecEvent never reads the size argument,
        // so the host passes sizeof the record it actually built.) The arm latches
        // mpGuiCache and seeds mPlayersTrackerInfo from the cache's world-camera lane
        // (GuiCache +0x4AE0), which is the tracker's own view of where the player is.
        // ⚠️ The console re-posts 64 on every cache update; this build posts it once, at
        // construction -- so the tracker's player record does not follow the camera yet.
        // Named as the known gap (see the RecEvent banner in BrnGuiTracker.h), not papered
        // over: the 232 publish path that draws the route line does not depend on it.
        {
            GuiEventCachePointer lCachePointerEvent;
            lCachePointerEvent.mpCachePointer = &mGuiCache;
            mGuiTracker.RecEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCachePointerEvent), 64,
                static_cast<s32>(sizeof(lCachePointerEvent)));
        }

        mbResourcesReadyFed = false;
        mbPrepared          = false;
    }

    // ARTIST82518E9C..82519024, extracted from the original inline memory
    // stage. No selected host caller invokes this helper yet.
    void GuiModule::PopulateNativeGuiConfig(
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (!lpGameDataOutput->GetAllocatorList())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Invalid allocator list passed into BrnGui::GuiModule::Prepare";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 367);
            CgsDev::Assert::EndAssert();
        }
        mGuiConfig.miAptPersistentBunldePoolId = 8;
        mGuiConfig.miAptStreamedbundlePoolId = 9;
        mGuiConfig.miFontBundlePoolId = 0;
        mGuiConfig.miGlobalTexturePoolId = 10;
        mGuiConfig.miFSMBunldePoolId = 11;
        mGuiConfig.miLanguageBundlePoolId = 11;
        mGuiConfig.miTexturesBundlePoolId = 12;
        mGuiConfig.mpAptAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(32);
        mGuiConfig.mpFSMAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpRendererAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWGeneralResourceAllocator(31);
        mGuiConfig.mpLanguageAllocator = lpGameDataOutput->GetAllocatorList()->GetHeapAllocator(38);
        mGuiConfig.mpGuiStateAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(44);
        mGuiConfig.mpTextureAllocator =
            lpGameDataOutput->GetAllocatorList()->GetRWLinearResourceAllocator(42);
        mGuiConfig.mpFlaptAllocator = lpGameDataOutput->GetAllocatorList()->GetLinearAllocator(66);
        mGuiConfig.mpGuiStateAllocator->SetAlignment(16);
        mGuiConfig.mpFlaptAllocator->SetAlignment(16);
    }

    // ARTIST825193C8..82519444. Counts and tuple words are recovered
    // directly from the three original tables; enum ordinals are target values.
    // This extracted block has no selected host caller.
    bool GuiModule::EnsureNativeGuiResources()
    {
        static const CgsGui::sResourceTuple maEuropeanFontResourcesToLoad[3] =
        {
            { 17u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 18u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 19u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maJapaneseFontResourcesToLoad[2] =
        {
            { 20u, static_cast<CgsGui::ResourceRequestTypes>(16) },
            { 21u, static_cast<CgsGui::ResourceRequestTypes>(16) },
        };
        static const CgsGui::sResourceTuple maBasicResourcesToLoad[2] =
        {
            { 125u, static_cast<CgsGui::ResourceRequestTypes>(7) },
            { 196u, static_cast<CgsGui::ResourceRequestTypes>(10) },
        };
        const CgsGui::sResourceTuple* lpFonts;
        u32 luFontCount;
        if (mGuiCache.GetLanguage() == CgsLanguage::E_LANGUAGE_JAPANESE)
        {
            lpFonts = maJapaneseFontResourcesToLoad;
            luFontCount = 2;
        }
        else
        {
            lpFonts = maEuropeanFontResourcesToLoad;
            luFontCount = 3;
        }
        if (!mGuiCache.EnsureResourcesAreLoaded(lpFonts, luFontCount))
            return false;
        return mGuiCache.EnsureResourcesAreLoaded(maBasicResourcesToLoad, 2);
    }

    // ARTIST824F10D8. GameTalk's actual MessageHandler contract is void.
    // Registration belongs to native Prepare and remains unselected here.
    void GuiModule::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
    {
        const char* lpcKey = lpMessage->GetKey(0);
        if (_strnicmp(lpcKey, "StartHook", 9) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StartHook"), 3554);
            gbGuiToolStartHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopHook", 8) == 0)
        {
            CopyGuiToolHookName(lpMessage->GetKeyContent("StopHook"), 3564);
            gbGuiToolStopHookRequested = true;
        }
        else if (_strnicmp(lpcKey, "StopMenuHook", 12) == 0)
        {
            std::strncpy(gacGuiToolHookName, "", 32);
            gbGuiToolStopHookRequested = true;
        }
    }

    bool GuiModule::Prepare()
    {
        mGuiConfig.mpRendererAllocator = 0;
        {
            BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
                BrnGameMainFlowController::GetScriptedLoadGameDataOutput();
            if (lpGameDataOutput != 0)
            {
                // (IsBufferLockedForReading is protected; GetAllocatorList's own read assert
                // is the check.)
                const BrnResource::GameDataIO::AllocatorList* lpAllocators =
                    lpGameDataOutput->GetAllocatorList();
                if (lpAllocators != 0)
                {
                    mGuiConfig.mpTextureAllocator = lpAllocators->GetRWLinearResourceAllocator(42);
                    mGuiConfig.mpRendererAllocator =
                        lpAllocators->GetRWGeneralResourceAllocator(31);
                }
            }
        }

        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and
        // publish the manager so the renderer draws the active movie each frame (interim render
        // bridge; the X360 renders it through the GUI's own ViewIO ImRenderers).
        mMovieManager.Prepare(0);
        PrepareNativeDebugComponents();
        gpActiveMovieManager = &mMovieManager;
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
        // callback table) BEFORE the flow prepares, so the flow
        // states' access pointers can reach the AptAux singleton. Idempotent + defensive.
        // The host drives the REAL staged (virtual) ViewModule::Prepare, whose FLAPT
        // stage prepares the FlaptManager with this linear -- the console prepare shape
        // (the separate FlaptManager::Prepare call is retired with it).
        s_pViewModule  = &mViewModule;
        s_pFlaptLinear = &s_flaptLinear;
        PrepareAptRuntime();

        // Complete the shared access bundle with the AptAux singleton created by the
        // PC runtime host above.  Construct already installed the language, Flapt, and
        // GuiCache owners exactly as the X360 GuiModule::Construct does.
        mGuiAccessPointers.mpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;

        // ---- X360 GuiModule::Prepare @0x82518D68, STAGE 7 -------------------------------
        //   v27 = (*(*(v3+311952) + 4))(v3+311952, rwGeneralResource, rwLinearResource);
        //   CgsGui::ViewModule::SetCustomRendererManager(v3+132224, v3+311952, 10, v3+1629284);
        //   if (!v27) goto fail;
        // Order matters and is reproduced: the manager is Prepared FIRST, installed SECOND,
        // and the prepare result is only gated on afterwards -- so the view module gets the
        // manager pointer even on a not-yet-finished staged prepare.
        //
        // ⚠️ It MUST be installed after PrepareAptRuntime() above, because
        // ViewModule::SetCustomRendererManager mirrors the pointer into
        // mAptAux.mRenderHandler and AptAux::Construct clears that same slot to 0.
        //
        // ⭐ [licence-icon] 2026-08-24: the allocator FLAG that stood here is PAID -- the call
        // now passes the console's own pair (stage 1 above fills both mGuiConfig rows: slot 31
        // general -> mGuiConfig.mpRendererAllocator, slot 42 linear -> mGuiConfig.mpTextureAllocator; the X360 stage-7
        // call is `Prepare(*(gm+311924), *(gm+311932))` @0x82518D68). One call advances the
        // staged prepare as far as it can this frame; the console re-enters Prepare until every
        // stage passes, so Update owns the PC pump (see the [licence-icon] block there).
        const bool lbCustomRenderersPrepared =
            mCustomRendererManager.Prepare(mGuiConfig.mpRendererAllocator, mGuiConfig.mpTextureAllocator);
        mbCustomRenderersPrepared = lbCustomRenderersPrepared;
        mViewModule.SetCustomRendererManager(&mCustomRendererManager, 10,
                                             &mGuiModuleSerialiser);
        (void)lbCustomRenderersPrepared;   // the console's `if (!v27) return 0;` -- see FLAG
        {
            char lacProbe[192];
            std::snprintf(lacProbe, sizeof(lacProbe),
                          "[custrend] manager installed: prepared=%d numComponents=%d "
                          "slot0(PlayerImage) id=%016llX renderable=%d\n",
                          lbCustomRenderersPrepared ? 1 : 0,
                          mCustomRendererManager.GetNumComponents(),
                          static_cast<unsigned long long>(
                              mCustomRendererManager.GetComponentID(BrnGui::E_NETWORK_PLAYER_IMAGE)),
                          mCustomRendererManager.GetComponentRenderable(
                              BrnGui::E_NETWORK_PLAYER_IMAGE) ? 1 : 0);
            CgsDev::Log::WriteToLog(lacProbe);
        }

        // The REAL flow bring-up: base prepare (access pointers into the StateInterface)
        // + the 14-state pool carve, then the flow's single in-queue. (allocator: the
        // flagged NULL died 2026-08-29 -- MapManager dereferences it now; see the
        // lpFlowAllocator stand-in below.) FLAG (ProfileManager): un-reconstructed; BF_PROFILE's
        // manager-gated calls are boundary no-ops (see BrnBootProfile.cpp).
        // The profile manager's Prepare precedes the flows': it attaches the sign-in
        // listener, prepares the embedded save/load system, and carves the mugshot
        // circular buffer (X360 hands the 0x26 game-data heap + a module linear; the
        // PC module owns dedicated backing regions -- see the FLAG at the statics).
        // ARTIST82519264..82519300: retain call order and assertion continuation.
        if (!mSystemUserProfile.Prepare())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mSystemUserProfile.Prepare()",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 519);
            CgsDev::Assert::EndAssert();
        }
        if (!mGuiKeyboard.Prepare(&mSystemUserProfile))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mGuiKeyboard.Prepare(&mSystemUserProfile )",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 523);
            CgsDev::Assert::EndAssert();
        }
        if (!mKeyboardHost.Prepare(&mGuiKeyboard))
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mKeyboardHost.Prepare(&mGuiKeyboard)",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 525);
            CgsDev::Assert::EndAssert();
        }
        mProfileHeap.Construct(s_profileHeapBacking, static_cast<s32>(sizeof(s_profileHeapBacking)));
        mProfileLinear.Construct();
        mProfileLinear.Create(s_profileLinearBacking, sizeof(s_profileLinearBacking));
        mProfileHost.GetProfileManager().Prepare(&mProfileHeap, &mProfileLinear);

        // Install the PC-boundary blank LIVE-REVENGE block (see the static above): the
        // console's network module does this via SetLiveRevengeProfile (GUI event 351);
        // without it Bootup->ReadProfileData faults on the null pointer. Clear() makes it a
        // fresh, unsaved profile of the current version, so the saves written from it pass
        // the live-revenge validation (older PC saves carrying version 0 fail it and reset).
        //
        // ⛔⛔ [one-profile wave 2026-08-28] THE PROGRESSION PAIR IS NO LONGER INSTALLED HERE,
        // and this was the whole defect. There is exactly ONE BrnProgression::Profile on the
        // console -- GameStateModule::mProgressionManager::mProfile, embedded at GSM+48288 --
        // and GameStateModule::PreWorldUpdate posts its address as game action 193 every
        // pre-world update; ProfileManager::SetProgressionProfile @0x824ECC98 has exactly one
        // xref in the whole image and it is GuiModule::Update's case-350 arm consuming that
        // record. We had no case-350 arm, so the manager could never be told the live address
        // and this Prepare installed a module static instead. The result was measurable and
        // permanent: Profile::Deserialise wrote the SAVE into the static while every gameplay
        // reader (ProgressionManager::AreRoadRulesAvailable, the medal/event/car tables, the
        // TrainingManager flags) read mProgressionManager.mProfile -- which was Construct()ed
        // and never loaded into, so it answered mbIsNewProfile = 1 on every boot, for ever.
        // The case-350 arm now does the console's install and the static is gone.
        s_pcLiveRevengeProfile.Clear();
        mProfileHost.GetProfileManager().SetLiveRevengeProfile(&s_pcLiveRevengeProfile);

        // X360 GuiModule::Prepare @0x82518D68 STAGE 3, line 104: `*(gm + 660992) = gm +
        // 1005332`, i.e. the analyzer adopts the shared GuiAccessPointers block (the same
        // block the three flows are prepared against, one line below). The console inlines
        // the store; the DWARF names the method (BrnGuiHudMessageAnalyzer.h:75) and it is
        // bodied as the de-inlined form in BrnGuiHudMessageAnalyzer.cpp. Without it
        // mpAccessPointers is never written and TriggerChallengeEndedMessage /
        // HandleStuntPerformed dereference it (mpAccessPointers->mpLanguageManager).
        mHudMessageAnalyzer.SetAccessPointers(&mGuiAccessPointers);

        mHudFlow.Prepare(&mGuiAccessPointers, lpFlowAllocator, &mHudStatePool,
                         &mProfileHost.GetProfileManager());
        mHudInQueue.Construct();
        mHudFlow.SetInEventQueue(&mHudInQueue);

        // The overlay flow: the 15-popup-state pool + its own in-queue (the X360 module
        // prepares all three flows here).
        mOverlayStatePool.Construct();
        mOverlayStatePool.Create(s_overlayStatePoolBacking, sizeof(s_overlayStatePoolBacking));
        mOverlayFlow.Prepare(&mGuiAccessPointers, lpFlowAllocator, &mOverlayStatePool);
        mOverlayInQueue.Construct();
        mOverlayFlow.SetInEventQueue(&mOverlayInQueue);

        // The SCREEN flow: the 61-state front-end pool + its own in-queue. The X360
        // Prepare threads the module's ProfileManager through BY REFERENCE (the CN_PROFILE
        // state's Construct consumes it; the manager is a shell until reconstructed).
        mScreenStatePool.Construct();
        mScreenStatePool.Create(s_screenStatePoolBacking, sizeof(s_screenStatePoolBacking));
        if (mScreenFlow.Prepare(&mGuiAccessPointers, lpFlowAllocator, &mScreenStatePool,
                                mProfileHost.GetProfileManager()))
        {
            // ARTIST true-return path at 0x825190E8 seeds the SCREEN flow to CURRENT.
            mScreenFlow.SetStreamingMode(BrnBaseFlow::E_STREAMING_CURRENT);
        }
        mScreenInQueue.Construct();
        mScreenFlow.SetInEventQueue(&mScreenInQueue);

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
            &mGuiModel, &mFsmLuaHeap);
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
        mOverlayLoopBackQueue.Construct();
        mOverlayLoopBackQueue.Clear();

        // The view-module IO pair the per-frame bridge fills.
        mViewInputBuffer.Construct();
        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.GetViewStateQueue()
            .CgsModule::VariableEventQueue<65536, 16>::Construct();
        mViewInputBuffer.UnlockForWrite();
        mViewOutputBuffer.Construct();
        miLastViewFrameMs = -1;

        {
            CgsGui::GuiEventLoadNotification lNotification;
            while (PopPendingAptLoadNotification(&lNotification))
            {
                mViewInputBuffer.GetViewStateQueue()
                    .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNotification), 14,
                        static_cast<s32>(sizeof(lNotification)));
            }
        }
        mViewInputBuffer.UnlockForWrite();

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

        mViewModule.Update(0, 0, &mViewInputBuffer, &mViewOutputBuffer);
        BridgeFromViewToOutput();
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
        BridgeFromViewToOutput();
        mViewInputBuffer.LockForWrite();
        mViewInputBuffer.GetViewStateQueue()
            .CgsModule::VariableEventQueue<65536, 16>::Clear();
        mViewInputBuffer.UnlockForWrite();

        // The HUD flow FSM chain is live: the GUI owns the loading-screen visual through
        // the real 19/20 command protocol (BridgeGuiToGame consumes them).
        gBrnGuiDrivesLoadingScreen = true;

        CgsDev::Log::WriteToLog(
            "[GuiModule] flow controller live (HUD flow registered; awaiting GuiEventRunFsm).\n");
        mbPrepared = true;
        return true;
    }

    // ARTIST825194B8: the enclosing second-prepare owner. Keep the unsigned
    // stage tests and original wait/continuation behavior; no readiness surrogate.
    bool GuiModule::Prepare2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (muPrepare2State < 2)
        {
            muPrepare2State = 1;
            if (!mWorldDataController.Prepare2(lpGameDataInput))
                return false;
        }
        else if (muPrepare2State >= 3)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid prepare state", "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 658);
            CgsDev::Assert::EndAssert();
            return false;
        }
        muPrepare2State = 2;
        return true;
    }

    // X360 GuiModule::Prepare @0x82518D68 STAGE 14, split out to its own entry point because the
    // PC has no module scheduler to hand GuiModule::Prepare the GameData IO pair. See the FLAG on
    // the declaration; the machine itself is the console's, unaltered.
    bool GuiModule::PrepareWorldData(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (lpGameDataInput == 0)
            return false;
        return mWorldDataController.Prepare(lpGameDataInput);
    }

    // ⭐ [event-starts wave 2026-08-27] X360 GuiModule::Prepare2 @0x825194B8's WorldDataController
    // leg, split out for exactly the same reason as PrepareWorldData above (the PC has no module
    // scheduler to hand GuiModule::Prepare2 the GameData IO pair). The machine itself is the
    // console's, unaltered -- see WorldDataController::Prepare2.
    //
    // ⛔ THE CALLER MUST NOT PUMP THIS AND PrepareWorldData IN THE SAME PHASE. Both machines drain
    // the SAME mReceiverQueue and neither checks what the one queued reply belongs to, so
    // overlapping them makes each eat the other's answer -- and on this build that is not
    // hypothetical: Prepare parks forever at stage 9 waiting on a GetFreeburnChallengeList reply
    // that no PC producer sends, so a Prepare2 reply arriving while Prepare is still pumped would
    // be consumed as the challenge list AND falsely advance meState to WFPLAYERCARCOLOURS.
    // BrnGameModule's driver enforces the exclusion; see the ⛔ at its call site.
    bool GuiModule::PrepareWorldData2(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
    {
        if (lpGameDataInput == 0)
            return false;
        return mWorldDataController.Prepare2(lpGameDataInput);
    }

    // Original stage12 has no failure return after this assertion.
    // Selecting the full typed Prepare will own the stage transition/call.
    void GuiModule::PrepareNativeSystemNotifications()
    {
        mhSysListenerHandle = XNotifyCreateListener(1);
        if (!mhSysListenerHandle)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
            lStream << "Failed to create an X360 system listener";
            CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 557);
            CgsDev::Assert::EndAssert();
        }
    }

    // ARTIST82529CA8..82529DD0: dequeue at most one notification,
    // then publish the actual four-byte event516 to the fresh Model input.
    void GuiModule::UpdateNativeSystemNotifications(
        CgsGui::ModelIO::InputBuffer* lpInputBuffer)
    {
        unsigned long luNotificationId;
        unsigned long luNotificationParam;
        if (mhSysListenerHandle &&
            XNotifyGetNext(mhSysListenerHandle, 9, &luNotificationId, &luNotificationParam))
        {
            if (luNotificationId == 0x02000002)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::StrStream lStream(lMessage.GetBuffer(), lMessage.GetSize());
                lStream << "Invite notification processed by GuiModule. This could break cross game invites\n";
                CgsDev::Assert::FireAssert(lMessage.GetBuffer(),
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 2918);
                CgsDev::Assert::EndAssert();
            }
            const u32 luUiVisible = luNotificationParam != 1;
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 238);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->LockForWrite();
            lpInputBuffer->GetEventQueueNonConst()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&luUiVisible), 516, sizeof(luUiVisible));
            if (!lpInputBuffer)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpInputBuffer",
                    "..\\..\\..\\GameShared\\GameClasses\\Module/CgsModuleUtils.h", 248);
                CgsDev::Assert::EndAssert();
            }
            lpInputBuffer->UnlockForWrite();
        }
    }

    // Original Release closes before its stage switch, including failed close.
    // The assertion continues and clears the original member in either case.
    void GuiModule::ReleaseNativeSystemNotifications()
    {
        if (mhSysListenerHandle)
        {
            if (!CloseHandle(mhSysListenerHandle))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("CloseHandle( mhSysListenerHandle )",
                    "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 681);
                CgsDev::Assert::EndAssert();
            }
            mhSysListenerHandle = 0;
        }
    }

    bool GuiModule::Release()
    {
        mScreenFlow.Release();       // staged: current state OnLeave + ScriptedFsm release
        mHudFlow.Release();
        mOverlayFlow.Release();
        // Leave retry-sensitive GUI owners intact until the movie pool releases successfully.
        if (!mMovieManager.Release())
            return false;
        mProfileHost.GetProfileManager().Release();   // detach the sign-in listener + release the SLS
        // ARTIST824F92A4..824F930C: profile, keyboard, then keyboard listener host.
        if (!mSystemUserProfile.Release())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mSystemUserProfile.Release()",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 710);
            CgsDev::Assert::EndAssert();
        }
        mGuiKeyboard.Release();
        if (!mKeyboardHost.Release())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mKeyboardHost.Release()",
                "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 712);
            CgsDev::Assert::EndAssert();
        }
        mFsmLuaHeap.Destruct();
        if (gpActiveGuiModule == this)
            gpActiveGuiModule = 0;
        gpActiveMovieManager = 0;
        return true;
    }

    // Extracted ARTIST8251A4D4..8251A538. Only the future typed PreWorld phase calls this block.
    void GuiModule::PublishNativeMovieResourceRequest(CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput)
    {
        const CgsResource::ID lResourceId = mMovieManager.PendingVideoDataResourceRequest();
        if (lResourceId.GetHash() == 0)
            return;

        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser = mMovieManager.GetRecieverQueue();
        lRequest.miEventId = MovieManager::KI_VIDEO_DATA_RESOURCE_EVENT_ID;
        lRequest.miPoolId = 11;
        lRequest.mResourceId = lResourceId;
        lpGuiOutput->LockForWrite();
        lpGuiOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 4);
        lpGuiOutput->UnlockForWrite();
    }

    // ARTIST8251A3D0; original call order and locks are retained. The active PC GameMain
    // still does not select this virtual phase; that remains a separate frame-wiring increment.
    void GuiModule::PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                   CgsModule::IOBufferStack* lpOutputBufferStack,
                                   CgsGui::CgsGuiModuleIO::OutputBuffer* lpGuiOutput,
                                   CgsGui::ModelIO::OutputBuffer* lpModelOutputBuffer)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
        CGS_ASSERT(lpGuiOutput != 0, "lpGuiOutput != NULL");
        CGS_ASSERT(lpModelOutputBuffer != 0, "lpModelOutputBuffer != NULL");
        CGS_ASSERT(lpInputBufferStack != 0, "lpInputBufferStack != NULL");
        CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");

        lpGuiOutput->LockForWrite();
        UpdateCollisionWorldValidation(lpGuiOutput);
        UpdateCarPoolValidation(lpGuiOutput);
        mProfileHost.GetProfileManager().LoadProfileUpgradeData(lpGuiOutput, &mEventReceiverQueue);
        lpGuiOutput->UnlockForWrite();

        PublishNativeMovieResourceRequest(lpGuiOutput);
        CgsGui::GuiModule::PreWorldUpdate(lpInputBufferStack, lpOutputBufferStack,
                                          lpGuiOutput, lpModelOutputBuffer);
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModulePreWorldUpdate);
    }

    // ARTIST82519578. Pending* calls consume their flags/state even when an
    // assertion is disabled, so keep every call outside its assertion expression.
    void GuiModule::UpdateCollisionWorldValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        const auto ConsumeNetworkingInvalidate = [this]() {
            const bool lbPending = mbCollisionWorldInvalidateForNetworking;
            mbCollisionWorldInvalidateForNetworking = false;
            return lbPending;
        };
        const auto ConsumeNetworkingValidate = [this]() {
            const bool lbPending = mbCollisionWorldValidateForNetworking;
            mbCollisionWorldValidateForNetworking = false;
            return lbPending;
        };
        const auto NoNetworkingInvalidate = [&]() {
            const bool lbPending = ConsumeNetworkingInvalidate();
            CGS_ASSERT(!lbPending, "!PendingCollisionWorldInvalidateForNetworking()");
        };
        const auto NoMovieInvalidate = [this]() {
            const bool lbPending = mMovieManager.PendingCollisionWorldInvalidate();
            CGS_ASSERT(!lbPending, "!mMovieManager.PendingCollisionWorldInvalidate()");
        };
        const auto NoProfileInvalidate = [this]() {
            const bool lbPending = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
            CGS_ASSERT(!lbPending, "!mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate()");
        };
        const auto SwapOut = [this, lpOutput]() {
            BrnResource::GameDataIO::SwapOutCollisionWorldRequest lRequest;
            lRequest.miEventId = 0;
            lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
            lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 67);
        };
        const auto SwapIn = [this, lpOutput]() {
            BrnResource::GameDataIO::SwapInCollisionWorldRequest lRequest;
            lRequest.miEventId = 0;
            lRequest.mpReceiverQueue = &mCollisonPoolValidationReceiverQueue;
            lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 68);
        };
        const auto AskWorld = [lpOutput](GuiEventRequestCollisionWorldEvent::EEventType leType) {
            GuiEventRequestCollisionWorldEvent lRequest;
            lRequest.meEventType = leType;
            lpOutput->AddGuiOutEvent(lRequest);
        };
        const auto ReadResponse = [this](s32 liExpectedType) {
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            const s32 liType = mCollisonPoolValidationReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
            CGS_ASSERT(mCollisonPoolValidationReceiverQueue.GetLength() == 1,
                "mCollisonPoolValidationReceiverQueue.GetLength() == 1");
            if (liExpectedType == 67)
                CGS_ASSERT(liType == 67, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_OUT_COLLISION_WORLD");
            else
                CGS_ASSERT(liType == 68, "liEventId == BrnResource::GameDataIO::EVENT_SWAP_IN_COLLISION_WORLD");
            return lpEvent;
        };

        switch (meCollisionWorldStateValidationState)
        {
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE:
        {
            const bool lbMovie = mMovieManager.PendingCollisionWorldInvalidate();
            const bool lbNetworking = ConsumeNetworkingInvalidate();
            if (mbBootMovieMode)
            {
                const bool lbProfile = mProfileHost.GetProfileManager().PendingCollisionWorldInvalidate();
                if (lbMovie || lbProfile)
                {
                    BrnResource::PrintConsoleMemory(0);
                    mCollisonPoolValidationReceiverQueue.Clear();
                    SwapOut();
                    meCollisionWorldStateValidationState = lbMovie
                        ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT
                        : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD;
                }
            }
            else if (lbMovie || lbNetworking)
            {
                CGS_ASSERT(!mbCollisionWorldInvalid, "!mbCollisionWorldInvalid");
                CGS_ASSERT((lbMovie && !lbNetworking) || (!lbMovie && lbNetworking),
                    "( lbMoviePendingInvalidate && !lbNetworkingPendingInvalidate) || (!lbMoviePendingInvalidate && lbNetworkingPendingInvalidate)");
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE);
                meCollisionWorldStateValidationState = lbMovie
                    ? E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE
                    : E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING;
            }
            break;
        }
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_BOOT:
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                const auto* lpResponse = static_cast<const BrnResource::GameDataIO::SwapOutCollisionWorldResponse*>(ReadResponse(67));
                mMovieManager.FreeCollisionPoolMemory(lpResponse);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_BOOT:
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                SwapIn();
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_BOOT:
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(68);
                BrnResource::PrintConsoleMemory(0);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(67);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = true;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mProfileHost.GetProfileManager().PendingCollisionWorldValidate())
            {
                SwapIn();
                mCollisonPoolValidationReceiverQueue.Clear();
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_SAVELOAD:
            NoMovieInvalidate();
            NoNetworkingInvalidate();
            if (mCollisonPoolValidationReceiverQueue.GetLength() > 0)
            {
                ReadResponse(68);
                BrnResource::PrintConsoleMemory(0);
                mbCollisionWorldInvalid = false;
                mProfileHost.GetProfileManager().SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            if (mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(false);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            CGS_ASSERT(mbCollisionWorldInvalid, "mbCollisionWorldInvalid");
            if (mMovieManager.PendingCollisionWorldValidate())
            {
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            NoProfileInvalidate();
            NoNetworkingInvalidate();
            if (!mbCollisionWorldInvalid)
            {
                mMovieManager.SetCollisionWorldValid(true);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALIDATING_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING;
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (ConsumeNetworkingValidate())
            {
                AskWorld(GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE);
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING;
            }
            break;
        case E_COLLISIONWORLDSTATEVALIDATIONSTATE_VALIDATING_FOR_NETWORKING:
            NoProfileInvalidate();
            NoMovieInvalidate();
            if (!mbCollisionWorldInvalid)
                meCollisionWorldStateValidationState = E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE;
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    // ARTIST82514228. Pool15 is positively attested here. Invalidate IN_USE
    // retries at WAITING_FOR_POOL_TO_EMPTY; it never fabricates completion.
    void GuiModule::UpdateCarPoolValidation(CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        switch (meCarPoolStateValidationState)
        {
        case E_CARPOOLSTATEVALIDATIONSTATE_IDLE:
            if (mMovieManager.PendingCarPoolInvalidate())
            {
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = true;
                else
                {
                    const bool lbEmpty = true;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    mbTrafficPoolEmptied = false;
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY:
            if (mbTrafficPoolEmptied)
            {
                CgsResource::Events::InvalidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 7);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() > 0)
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 7, "liEventId == CgsResource::ResourceIO::EVENT_INVALIDATEPOOL");
                const auto* lpResponse = reinterpret_cast<const CgsResource::Events::InvalidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpInvalidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear(); // Original Clear preserves payload storage.
                if (lpResponse->GetResult() != CgsResource::Events::InvalidatePoolResponse::E_RESULT_SUCCESS)
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_EMPTY;
                else
                {
                    mMovieManager.SetCarPoolValid(false, lpResponse->GetResource(), lpResponse->GetDescriptor());
                    meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE;
                }
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_INVALID_FOR_MOVIE:
            if (mMovieManager.PendingCarPoolValidate())
            {
                CgsResource::Events::ValidatePoolRequest lRequest;
                lRequest.mpUser = &mEventReceiverQueue;
                lRequest.miEventId = BrnResource::E_POOL_TRAFFIC;
                lRequest.miPoolId = BrnResource::E_POOL_TRAFFIC;
                lpOutput->GetGuiResourceRequestQueue()->AddEvent(&lRequest, 8);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE;
            }
            break;
        case E_CARPOOLSTATEVALIDATIONSTATE_VALIDATING_FOR_MOVIE:
            if (mEventReceiverQueue.GetLength() <= 0)
                break;
            {
                CGS_ASSERT(mEventReceiverQueue.GetLength() == 1, "mEventReceiverQueue.GetLength() == 1");
                const CgsModule::Event* lpEvent = 0;
                s32 liSize = 0;
                const s32 liEventId = mEventReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
                CGS_ASSERT(liEventId == 8, "liEventId == CgsResource::ResourceIO::EVENT_VALIDATEPOOL");
                const auto* lpResponse = reinterpret_cast<const CgsResource::Events::ValidatePoolResponse*>(lpEvent);
                CGS_ASSERT(lpResponse->miEventId == BrnResource::E_POOL_TRAFFIC,
                    "lpValidatePoolResponse->GetEventId() == BrnResource::E_POOL_TRAFFIC");
                mEventReceiverQueue.Clear();
                if (mbBootMovieMode)
                    mbTrafficPoolEmptied = false;
                else
                {
                    const bool lbEmpty = false;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lbEmpty), 13, sizeof(lbEmpty));
                    CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                }
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL;
            }
            [[fallthrough]];
        case E_CARPOOLSTATEVALIDATIONSTATE_WAITING_FOR_POOL_TO_FILL:
            if (!mbTrafficPoolEmptied)
            {
                CgsResource::SmallResource lResource;
                CgsResource::Entry::ResourceDescriptor lDescriptor;
                for (s32 liType = 0; liType < 3; ++liType)
                {
                    lResource.m_baseResources[liType] = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_size = 0;
                    lDescriptor.m_baseResourceDescriptors[liType].m_alignment = 1;
                }
                mMovieManager.SetCarPoolValid(true, lResource, lDescriptor);
                meCarPoolStateValidationState = E_CARPOOLSTATEVALIDATIONSTATE_IDLE;
            }
            break;
        default:
            CGS_ASSERT(false, "Unhandled state");
            break;
        }
    }

    // ARTIST GuiModule::Prepare stage 10 (0x82519218..0x82519254): register the four
    // native debug components and clear the event receiver queue at the original stage.
    void GuiModule::PrepareNativeDebugComponents()
    {
        mDebugComponent.Register();
        mHudMessageDebugComponent.Register();
        mSatNavDebugComponent.Register();
        mGuiCacheDebugComponent.Register();
        mEventReceiverQueue.Clear();
    }

void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
        // ARTIST825076D0..825077A4: four actual derived teardown owners, original order.
        mDebugComponent.Destruct();
        mHudMessageDebugComponent.Destruct();
        mSatNavDebugComponent.Destruct();
        mGuiCacheDebugComponent.Destruct();
        // X360 GuiModule::Destruct @0x82507690: ColourCalibrationScreen::Destruct is the LAST
        // sub-object destruct, immediately before the base CgsGui::GuiModule::Destruct.
        // ARTIST825077D0/D4: retain the original host/profile pointer teardown.
        mKeyboardHost.Destruct();
        mGuiKeyboard.Destruct();
        mColourCalibrationScreen.Destruct();
        // ARTIST825077F4: the shared queue/base lifecycle follows the final
        // game subobject. Existing Movie/APT lifetime owners above are retained.
        CgsGui::GuiModule::Destruct();
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
        // ARTIST sub_824FB9A8 emits this diagnostic after Construct for any action >= 14.
        if (liAction >= 14 && ::gbLogGuiAudioTriggers &&
            (CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            const char* lpcLogComponent = lpcComponentName != 0 ? lpcComponentName : "<NULLSTRING>";
            const char* lpcLogAction = lpcActionName != 0 ? lpcActionName : "<NULLSTRING>";
            const char* lpcLogLabel = lpcLabel != 0 ? lpcLabel : "<NULLSTRING>";
            const char* lpcLogMovie = lpcSwfName != 0 ? lpcSwfName : "<NULLSTRING>";
            *CgsDev::Log::gpDebugPrint
                << "\nGUI AUDIO TRIGGERS: "
                << "Received invalid sound action. Component: " << lpcLogComponent
                << " Action: " << lpcLogAction
                << " Label: " << lpcLogLabel
                << " Movie: " << lpcLogMovie
                << "\n";
        }
        lpThis->mpOutputBuffer->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAudioEvent),
            lAudioEvent.GetEventType(), static_cast<s32>(sizeof(lAudioEvent)));
    }

    namespace
    {
        // The four player-name string ids UpdatePlayerName reads/writes. The read side
        // ("DEFAULTPLAYERNAME"/"DEFAULTPLAYERNAMEQUOTED", X360 off_82F278B4/off_82F278B8)
        // are language-DATABASE keys whose entries ship in the LANGUAGE bundles ("You" /
        // '"You"'); the write side (off_82F278AC/off_82F278B0) are the LIVE ids every
        // GUI consumer resolves (GuiCache::GetPlayerName returns the first verbatim).
        const char* const KAPC_UPN_PLAYER_NAME_ID        = "PLAYER_NAME_STRING_ID";      // @0x8206E7DC
        const char* const KAPC_UPN_PLAYER_NAME_QUOTED_ID = "PLAYER_NAME_STRING_ID_Q";    // @0x8206E7C4
        const char* const KAPC_UPN_DEFAULT_NAME_ID       = "DEFAULTPLAYERNAME";          // off_82F278B4
        const char* const KAPC_UPN_DEFAULT_QUOTED_ID     = "DEFAULTPLAYERNAMEQUOTED";    // off_82F278B8
    }

    // @ 0x824F0D30 -- publish the live player name into the language database (see the
    // header note). Runs when GuiModule::Update sees event 507 -- the command record
    // BootProfile::OnLeave posts as the profile boot completes -- so every subsequent
    // "PLAYER_NAME_STRING_ID" lookup (the licence card's playerName field, the HUD
    // message analyzer's name substitution) resolves to a real name instead of the id.
    //
    // [PC] XUserGetName has no host equivalent and no user is ever signed in, so this
    // always takes the console's XUserGetName-FAILED branch: copy the database's own
    // DEFAULT entries ("You" / '"You"') under the live ids -- byte-identical to an X360
    // with no profile signed in. The signed-in branch (gamertag + the "''%s''" quoted
    // form) needs the platform user account and stays X360-only.
    void GuiModule::UpdatePlayerName()
    {
        CgsLanguage::LanguageManager* lpLanguageManager = mViewModule.GetLanguageManager();
        if (lpLanguageManager == 0)
            return;   // [PC] Prepare not finished yet; the console cannot reach 507 this early.

        const u8* lpcDefaultName = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_NAME_ID);
        CGS_ASSERT(lpcDefaultName != 0,
                   "Couldn't find default player name in string database.");             // cpp:1216
        if (lpcDefaultName == 0)
            return;   // [PC] the console strncpy's from NULL here; entries ship in every LANGUAGE bundle
        CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultName)) < 0x1B,
                   "String too long: ");                                                  // CgsStringUtils.h:55
        char lacName[32];
        std::strncpy(lacName, reinterpret_cast<const char*>(lpcDefaultName), 27);
        lacName[27] = 0;
        lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_ID,
                                     reinterpret_cast<const u8*>(lacName));

        const u8* lpcDefaultQuoted = lpLanguageManager->FindString(KAPC_UPN_DEFAULT_QUOTED_ID);
        CGS_ASSERT(lpcDefaultQuoted != 0,
                   "Couldn't find default player name quoted in string database.");      // cpp:1222
        if (lpcDefaultQuoted == 0)
            return;   // [PC] see above
        CGS_ASSERT(std::strlen(reinterpret_cast<const char*>(lpcDefaultQuoted)) < 0x1F,
                   "String too long: ");                                                  // CgsStringUtils.h:55
        char lacQuoted[32];
        std::strncpy(lacQuoted, reinterpret_cast<const char*>(lpcDefaultQuoted), 31);
        lacQuoted[31] = 0;   // the console's explicit v35 = 0 store
        lpLanguageManager->AddString(KAPC_UPN_PLAYER_NAME_QUOTED_ID,
                                     reinterpret_cast<const u8*>(lacQuoted));
    }

    // X360 0x8251A568: preserve an image file when autosave is disabled, otherwise autosave
    // it as a one-record ImageFileInfo request. The current PC Update keeps its documented
    // sign-in-gate deviation, but retains the original autosave-enabled and image-copy branches.
    void GuiModule::AutosaveProfile(const CgsGui::ImageFileInfo* lpImageFileInfo)
    {
        if (lpImageFileInfo != 0 && !mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().CopyImageToBuffer(lpImageFileInfo);
        }

        if (mProfileHost.GetProfileManager().AutosaveIsEnabled())
        {
            mProfileHost.GetProfileManager().Autosave(&mProfileHost,
                                     lpImageFileInfo != 0 ? 1 : 0, lpImageFileInfo);
            mbProfileAutosavePending  = false;
            mfLastProfileAutosaveTime = mGuiCache.GetTime();
        }
    }

    // The real GuiModule::Update event dispatch (X360 0x82527A58's switch): consume the
    // module-level events, forward the load notifications, and fan the rest to the flow.
    const CgsModule::Event* GuiModule::DispatchInboundGuiEvents(
        bool& rbHasSaveImageEvent, CgsGui::ImageFileInfo& lrSaveImageInfo,
        const CgsModule::Event* lpAfterEvent)
    {
        if (mpGuiEventInputBuffer == 0)
            return lpAfterEvent;

        mpGuiEventInputBuffer->LockForRead();
        const CgsModule::VariableEventQueue<32768, 16>* lpInQueue =
            static_cast<const CgsGui::CgsGuiModuleIO::InputBuffer*>(mpGuiEventInputBuffer)->GetGuiEvents();

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        const CgsModule::Event* lpLastDispatchedEvent = lpAfterEvent;
        s32 liId = lpAfterEvent != 0
            ? lpInQueue->GetNextEvent(lpAfterEvent, &lpEvent, &liSize)
            : lpInQueue->GetFirstEvent(&lpEvent, &liSize);
        while (liId >= 0 && lpEvent != 0)
        {
            switch (liId)
            {
                case 141:   // ARTIST82528ACC..AE0: reply with this module's actual keyboard.
                {
                    lResponse.lpKeyboard = &mGuiKeyboard;
                    if (mGuiOutQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                             lResponse.GetEventType(), static_cast<s32>(sizeof(lResponse))))
                        RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                         lResponse.GetEventType(), static_cast<s32>(sizeof(lResponse)));
                    break;
                }

                case 143:   // ARTIST 82528AE8..AF8: user-index word, then sign-in refresh.
                    mSystemUserProfile.SetUserIndex(*reinterpret_cast<const u32*>(lpEvent));
                    mSystemUserProfile.UpdateUserSigninState();
                    break;

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

                case 221:  // ARTIST 82528B28: the same bar size also gates HUD messages.
                    mHudMessageDirector.SetBlackBarSize(*reinterpret_cast<const f32*>(lpEvent));
                    break;

                case 26:    // GuiEventTimeInfo -- the per-frame { delta, now } pair
                    // The cache LEADS with this pair and every GUI-side timer reads it
                    // straight off mpGuiCache+0 (see GuiCache::RecTimeInfo's note). Without
                    // this latch mfTimeStep stays 0 and every GUI dwell/tick in the game is
                    // frozen -- which is exactly what parked BrnGui::Intro in WELCOMETEXT.
                    mGuiCache.RecTimeInfo(
                        reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent));
                    break;

                case 504:   // localized audio ready
                case 508:   // play video
                case 513:   // (movie family)
                    mMovieManager.RecvEvent(lpEvent, liId);
                    break;

                case 493:   // World collision invalidate/validate completion
                {
                    const auto* lpCollisionWorldEvent =
                        reinterpret_cast<const GuiEventRequestCollisionWorldEvent*>(lpEvent);
                    if (lpCollisionWorldEvent->meEventType ==
                        GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATED)
                        mbCollisionWorldInvalid = true;
                    else if (lpCollisionWorldEvent->meEventType ==
                             GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATED)
                        mbCollisionWorldInvalid = false;
                    break;
                }

                case 512:   // Traffic-pool emptied/refilled latch
                {
                    const bool lbTrafficPoolEmpty =
                        reinterpret_cast<const GuiEventTrafficPoolEmptied*>(lpEvent)->mbTrafficPoolEmpty;
                    if (lbTrafficPoolEmpty)
                    {
                        CGS_ASSERT(!mbTrafficPoolEmptied, "!mbTrafficPoolEmptied");
                        mbTrafficPoolEmptied = true;
                    }
                    else
                    {
                        CGS_ASSERT(mbTrafficPoolEmptied, "mbTrafficPoolEmptied");
                        mbTrafficPoolEmptied = false;
                    }
                    break;
                }

                case 350:   // GuiEventProgressionProfileData -- the live-profile handoff
                    // ⭐⭐⭐ [one-profile wave 2026-08-28] THE MISSING HALF OF CASE 350, and
                    // the reason this build owned TWO BrnProgression::Profile objects where the
                    // console owns one. X360 BrnGui::GuiModule::Update @0x82527A58:
                    //     case 350:
                    //       BrnGui::ProfileManager::SetProgressionProfile(a1 + 681696, *v61, v61[1]);
                    // i.e. the event's two payload words ARE the manager's mpProgressionProfile /
                    // mpProgressionData, and this is the ONLY writer of either on the console
                    // (SetProgressionProfile @0x824ECC98 has exactly one xref, and it is this).
                    // We routed 350 to the GuiCache only, so the manager never learned the live
                    // profile's address and BrnGuiModule::Prepare installed a module-static
                    // stand-in instead -- a SECOND Profile that Deserialise wrote into and no
                    // gameplay code ever read. The static and its install are deleted with this
                    // arm; the console's own chain is now the only installer.
                    {
                        const BrnGui::GuiEventProgressionProfileData* lpProfileData =
                            reinterpret_cast<const BrnGui::GuiEventProgressionProfileData*>(lpEvent);
                        mProfileHost.GetProfileManager().SetProgressionProfile(lpProfileData->mpProfile,
                                                              lpProfileData->mpProgressionData);
                    }
                    // fall through -- the record also feeds GuiCache::mpProfile (case 350 below)
                case 27:    // Actual Sku notification -> the cache enum owner.
                case 29:    // Actual language notification -> the cache enum owner.
                case 501:   // Original PFX enumeration -> actual cache owner.
                case 330:   // GuiEventSetRoadRuleScoreMode -- the road-rule panel mode word
                case 335:   // GuiEventRoadRuleBegin -- the rule's score type marked active
                case 336:   // GuiEventRoadRuleEnd   -- the rule's score type marked inactive
                case 169:   // GuiEventChangeDistrict -- the district-marker source words
                case 147:   // [H3b] GuiEventUpdateHud -- the player {speed,rpm,gear} words
                case 199:   // [H3b] GuiEventUpdateSatNav -- the icon array (player position arm)
                case 204:   // [H3b] the sat-nav event-filter pair (the ch40 mirror)
                case 207:   // [H3b] GuiRaceCarInfoEvent -- the mRaceCarInfo SoA feed
                case 208:   // Traffic score targets for AboveCarRenderer.
                case 210:   // Visible overhead-sign score targets.
                case 376:   // [H3b] GuiPlayerRaceCarIdEvent -- the player index pair (case-199 gate)
                case 377:   // GuiPlayerCrashingStateChangeEvent -- the crash-bar state latch + the
                            // gameplay-HUD-active drop (RecEvent @0x8250F58C; was misfiled as 132)
                case 379:   // [reveal gate] GuiPlayerEngineEvent -- the ignition latch (+0x4B20)
                case 238:   // Race positions, finished flags and per-slot validity.
                case 380:   // Player entered or left a shortcut.
                case 492:   // [E1] GuiEventCurrentStatus  -- distance driven + player-team table
                case 424:   // [E1] GuiEventScoreUpdate    -- THE EVENT TIMER (mfEventTime/mfTargetTime)
                case 428:   // [E1] GuiAttackScoreUpdate   -- THE STUNT SCORE (current/target/combo/multiplier)
                case 434:   // [showtime] GuiCrashScoreUpdate -- THE SHOWTIME SCORE (cars crashed + distance)
                case 426:   // [road-rage] GuiRoadRageScoreUpdate -- THE TAKEDOWN COUNT (current/target)
                case 194:   // Specific preset races; the cache adopts the per-mode table.
                case 203:   // [event-starts] GuiEventUpdateEventStarts -- THE EVENT-START TABLE
                case 93:    // [A9] GuiEventPrepareForModeStart -- THE MODE-TYPE SEED (meGameModeType)
                case 159:   // Pre-race messages and the in-event map colouring gate.
                case 162:   // Clear the pre-race message count.
                case 170:   // Landmark race list and its navigation tracker.
                case 289:   // [results] GuiEventOfflinePostEvent -- THE OFFLINE RESULT RECORD
                case 318:   // Online post-event record, adopted by the existing cache arm.
                case 292:   // [results] post-event teardown (clears the record + suppress byte)
                case 307:   // Medal totals and wins remaining on the licence.
                case 304:   // [results] raises the presentation-suppressed byte
                case 237:   // [event-start 2026-09-10] GuiGameModeStarted -- drops mbEventPreparedForModeStart
                case 321:   // [event-end 2026-09-10] GuiEventFinishedModeResults -- GuiCache's shared LABEL_232 tail
                case 322:   // [event-end 2026-09-10] GuiEventStopMode -- game mode back to NONE, tracker cleared
                case 556:   // [event-state 2026-09-10] GuiEventEventStateResponse -- the DISCOVERED-event array
                case 373:   // [FX-FLOW 2026-09-24, G13-X5] GuiShutdownEvent -- mShutdownCarID (RecEvent 0x8250FFA8)
                case 374:   // [FX-FLOW 2026-09-24, G13-X5] GuiShutdownFinishedEvent -- mbCarUnlockPending (0x8250FFC4)
                case 44:    // [net] GuiEventNetworkDisconnected -- the local player marked disconnected
                case 58:    // [net] the game launched -- disconnected / eliminated tables cleared
                case 108:   // [net] GuiEventOnlineTimeout -- the online timeout pending byte
                case 239:   // [net] GuiEventRaceDistanceRemaining -- the eliminated bytes
                case 240:   // [net] GuiEventRaceDistanceToCheckpoint -- the checkpoint distance
                case 243:   // [net] GuiEventNetworkPlayerList -- the pre-game roster
                case 244:   // [net] GuiEventNetworkLobbyPlayerList -- the lobby roster
                case 245:   // [net] GuiEventNetworkPlayerStatus -- the in-game roster + host byte
                case 257:   // [net] GuiEventNetworkGameParams -- the params mirror (online game mode)
                case 273:   // [net] GuiEventNetworkLeftGame -- the params security reset
                case 320:   // [net] online showtime completed
                case 79:    // GuiEnteredJunkyard -- the in-junkyard byte
                case 201:   // GuiEventHideDriveThru -- a closed drive-thru's map row hidden
                case 375:   // GuiEventTrophyCarUnlock -- the award screen's unlock type and car
                case 415:   // GuiChangeCarEvent -- current and original car ids for event icons
                    // [H1 wave 2026-08-25] On the console EVERY module-input event reaches
                    // GuiCache::RecEvent (its ~180-case switch consumes what it wants);
                    // this build's pump routes selectively, so the two cache-consumed ids
                    // this wave landed handlers for are forwarded here explicitly. 350 is
                    // what fills GuiCache::mpProfile (the odometer asserts it per frame);
                    // 169 is what fills the district-marker words. ⚠️ Do NOT blanket-route
                    // the whole stream: 14/16 are already re-consumed off the notification
                    // queue below and would double-deliver.
                    // [E1 event-status wave 2026-08-26] 492/424/428 join the list on the same
                    // terms: BridgeGameStateToGui's stunt slice posts them and GuiCache::RecEvent
                    // now has the matching three arms. None of the three is re-consumed off the
                    // notification queue, so forwarding them here delivers exactly once.
                    // ⭐⭐ [results-screen wave 2026-08-29] 289/292/304 join on identical terms,
                    // and they were THE last link in the offline results chain. Measured: with
                    // GuiCache::RecEvent's case-289 arm landed but this forward missing, the
                    // bridge posted the 192-byte record (its action-37 arm builds and pushes it
                    // unconditionally on the offline path), the cache never saw it, and
                    // InstantResultsState copied 192 ZERO bytes -- so its own reconstructed
                    // SetupComponents printed the console's "Unhandled game mode! (0)" and the
                    // screen stayed blank. A selective pump means a landed producer AND a landed
                    // consumer still add up to nothing until the id is on this list; that is
                    // three separate places to check, and the log looked identical at each.
                    // None of the three is re-consumed off the notification queue.
                    // [showtime score wave 2026-08-29] 434 joins on IDENTICAL terms and completes
                    // that set for modes 2/16: its only producer is the same bridge's showtime arm
                    // (@0x823EEE18), GuiCache::RecEvent's case-434 arm is its only consumer, and
                    // nothing else in the pump touches it. It is the SOLE writer of
                    // miShowTimeCarsCrashed / miShowTimeComboMultiplier /
                    // mfShowTimeDistanceTravelled -- the three words
                    // EventInfoComponent::UpdateCrash renders.
                    // ⚠️ THE ID IS 434 (AddGuiEvent<GuiCrashScoreUpdate> @0x823D10D0 `li r5, 0x1B2`),
                    // NOT the DecFIGS DWARF's 429. Forwarding 429 here would forward an id nothing
                    // posts, and RecEvent has no arm for it either.
                    // [A9 mode-type arm 2026-08-27] 93 joins on exactly the same terms and it is
                    // the one that makes the other three mean anything: BrnGame::
                    // TranslateEventFlowGameActionToGuiEvent's case-23 arm posts it (mounted +
                    // called), GuiCache::RecEvent's case-93 arm above is its only consumer, and
                    // it is the SOLE writer of meGameModeType -- the switch variable case 424
                    // and case 428 assert against and EventInfoComponent::Update switches on.
                    // ⭐ SELECTIVE, NOT BLANKET (the gateui-campaign house rule): 93 is not
                    // observed by any flow (RouteEventToFlow drops it) and is not re-consumed
                    // off the notification queue, so this forward delivers it exactly once.
                    // [event-starts wave 2026-08-27] 203 joins on identical terms: its only
                    // producer is BridgeGameStateToGui's event-start arm (landed as
                    // GameBridgeGameStateToX_EventStartsGuiEvents.cpp), GuiCache::RecEvent's
                    // case-203 arm is its only consumer, and nothing else in the pump touches
                    // it -- so this forward delivers it exactly once. It is the SOLE writer of
                    // GuiCache::maEventStarts, the table GetProfileEventDisplayInfo walks.
                    mGuiCache.RecEvent(lpEvent, liId);
                    break;

                case 343:   // GuiEventRoadRuleChangeMode -- the active road rule
                    // The console's module switch hands the new rule to the skills manager and,
                    // like every event, then gives the record to GuiCache::RecEvent, which keeps
                    // it for GetActiveRoadRule (the ticker's road-rules mode reads that).
                    mBurnoutSkillsManager.SetRoadRuleMode(
                        reinterpret_cast<const GuiEventRoadRuleChangeMode*>(lpEvent)->meScoreType);
                    mGuiCache.RecEvent(lpEvent, liId);
                    break;

                // The free-burn challenge tracker's event arms, in the console's case order.
                // Only the online free-burn lobby produces these records (the action->GUI
                // translator's 153/154/155/157 arms and the network bridge's 583).
                case 544:
                    mFreeburnChallengeManager.SelectNext();
                    break;
                case 574:
                    mFreeburnChallengeManager.StartChallenge(
                        reinterpret_cast<const GuiChallengeStartEvent*>(lpEvent));
                    break;
                case 576:
                    mFreeburnChallengeManager.TriggerChallenge(
                        reinterpret_cast<const GuiChallengeTriggerResponse*>(lpEvent));
                    break;
                case 577:
                    mFreeburnChallengeManager.HandleNewData(
                        reinterpret_cast<const GuiChallengeUpdateEvent*>(lpEvent));
                    break;
                case 578:
                    mFreeburnChallengeManager.StartResults();
                    break;
                case 579:
                    mFreeburnChallengeManager.FinishChallenge();
                    break;
                case 581:
                    mFreeburnChallengeManager.HandleCompletionStatus(
                        reinterpret_cast<const GuiEventFburnChallengeEveryPlayerStatus*>(lpEvent));
                    break;
                case 583:
                    mFreeburnChallengeManager.StartNotActiveChallenge(
                        reinterpret_cast<const GuiChallengeNotActiveStartEvent*>(lpEvent));
                    break;
                case 584:
                    mFreeburnChallengeManager.EndNotActiveChallenge();
                    break;

                case 358:
                    // X360 copies the four image-save words into a frame-local record, raises
                    // the force/pending latches, then lets Update call AutosaveProfile. The host
                    // record is 24 bytes because the buffer pointer widened; the unwritten
                    // trailing word remains byte-for-byte residue.
                    std::memcpy(&lrSaveImageInfo, lpEvent, sizeof(lrSaveImageInfo));
                    rbHasSaveImageEvent = true;
                    mbForceProfileAutosave = true;
                    mbProfileAutosavePending = true;
                    break;

                case 359:   // Image-file records loaded by ProfileManager (ARTIST GuiModule::Update case 359)
                    mProfileHost.GetProfileManager().LoadImageFiles(
                        &mProfileHost,
                        reinterpret_cast<const CgsGui::ImageFileInfo*>(lpEvent));
                    break;

                case 356:   // [profile-save] GuiAutosaveRequestEvent -- THE AUTOSAVE LATCH
                {
                    // ⭐⭐ X360 GuiModule::Update's event walk @0x82529054 (jumptable
                    // 0x82528C5C case 0 == GUI id 356), instruction for instruction:
                    //     lbz  r11, 0(r30)          ; the request's one payload byte
                    //     lbz  r10, var_430(r1)     ; v142, the per-Update FORCE byte
                    //     or   r11, r11, r10
                    //     stb  r11, var_430(r1)     ; v142 |= payload
                    //     li   r11, 1
                    //     stbx r11, r26, r15        ; *(module + 0x18D844) = 1  (pending)
                    //
                    // ⛔⛔ THIS ARM IS THE ONE LINK THE AUTOSAVE COMMIT DID NOT LAND. Its
                    // message claims "case 356 raises the pending byte", but the diff only
                    // added the CONSUMER (Update's tail) -- `mbProfileAutosavePending` had a
                    // reader and a clearer and NO WRITER, so the latch could never rise and
                    // ProfileManager::Autosave was unreachable. Measured 2026-08-27: a full
                    // junkyard->car-select->driving run logged
                    // `[profile-save] action 55 -> gui 356 (flag 0)` and then never wrote
                    // Memcard\Profile.sav, because the byte on the other side of this arm
                    // stayed down. The 1-byte record is the AutosaveRequestWire356 both
                    // GameBridgeGameStateToX_{Stunt,EventFlow}GuiEvents.cpp post.
                    //
                    // ⚠️ The producers in this build post a payload of ZERO (CarSelectManager's
                    // exit post, BrnCarSelectManager.cpp:1185), so the force byte stays clear
                    // and the tail's 60-second throttle is what actually times the save --
                    // exactly as on the console. Only the paused-game arm (case 505) and the
                    // mugshot arm (case 358) force it.
                    mbForceProfileAutosave =
                        mbForceProfileAutosave || (*reinterpret_cast<const u8*>(lpEvent) != 0);
                    mbProfileAutosavePending = true;
                    break;
                }

                default:
                    // The other module-level consumers (profile/skills/overlays/keyboard/
                    // language...) are subsystem follow-ons; their events pass through to
                    // the flow filter below.
                    break;
            }

            // The console's per-event tail, after GuiCache::RecEvent: the set-tracker, landmark
            // reached, route-information, cache-pointer and regenerate-route ids go to the
            // tracker (GuiCache ignores all five).
            if (liId == 232 || liId == 165 || liId == 211 || liId == 64 || liId == 233)
            {
                mGuiTracker.RecEvent(lpEvent, liId, liSize);
            }

            // ⭐ [tut-ticker] THE CUSTOM-RENDERER MANAGER FEED (2026-08-24). On the console
            // every module-input GUI event reaches BrnGui::CustomRendererManager::RecvEvent
            // through the view hop (BridgeFromInputToView -> ViewModule::
            // ProcessIncomingViewEvents' per-event hook @0x8285FCE8 tail). This build feeds
            // the view queue selectively, so the manager forward sits HERE, on the full
            // module-input stream -- exactly-once for everything that transits this queue
            // (537 the tutorial ticker, 64 the GuiCache bind, 258/571 the player image...).
            // The seat deviation is flagged at the console hook in CgsGuiViewModule.cpp.
            mCustomRendererManager.RecvEvent(lpEvent, liId);

            // The observer-subscription fan-out (EventInterpreterModule::ProcessInEvents).
            RouteEventToFlow(lpEvent, liId, liSize);

            lpLastDispatchedEvent = lpEvent;
            const CgsModule::Event* lpNext = 0;
            liId = lpInQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }
        mpGuiEventInputBuffer->UnlockForRead();
        return lpLastDispatchedEvent;
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

        // 4. Bridge request completions to ModelIO, where both the FSM controller and
        //    GuiCache observe them. Apt movie load notifications (request types 4..7) also
        //    go to the VIEW input buffer as
        //      view event 14, where the REAL CgsGui::ViewModule::ProcessIncomingLoadNotification
        //      @0x8285BD30 registers each header (AddAptData). Phase 2 routes the movie-slot
        //      bundle IO through the module, so these replace the AptRuntimeHost's
        //      PopPendingLoadNotification ring for the flow movie.
        //    The dual delivery is the ARTIST contract for actual request completions.
        //    PC-only id-zero bundled-clip registrations belong to the view alone.
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
                bool lbAptMovie   = false;
                bool lbAptUnload  = false;
                if (liId == 14)
                {
                    const s32 liReqType = static_cast<s32>(
                        reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent)->meRequestType);
                    lbAptMovie = (liReqType >= 4 && liReqType <= 7) || liReqType == 10;
                }
                else if (liId == 15)
                {
                    // ⭐⭐ [driver-details pause wave 2026-08-28] THE MISSING OTHER HALF of the
                    // line above, and the reason a released apt movie was never un-registered.
                    // GuiResourceModule::UnloadBundle posts a type-15 unload-REQUEST notification
                    // (OutputBuffer::AddUnloadRequestNotification @0x8285AC08, `li r5,0xF`), whose
                    // console consumer is ViewModule::ProcessIncomingUnloadRequestNotification
                    // @0x828586E8 -> AptDataHandler::RemoveAptData. This bridge forwarded ONLY the
                    // type-14 loads to the view, so the removal end of the pair could never run --
                    // AddAptData is idempotent by name, so every reload of a released movie handed
                    // the Apt linker the STALE header. Same apt request-type gate as the load side
                    // (the view's own arm re-tests 4..7, which is the console's gate).
                    const s32 liReqType = static_cast<s32>(
                        reinterpret_cast<const CgsGui::GuiEventUnloadRequestNotification*>(
                            lpEvent)->meRequestType);
                    lbAptUnload = (liReqType >= 4 && liReqType <= 7);
                }
                if (!lbViewOnlyRegistration)
                    mModelOutputBuffer.GetLoadNotificationsNonConst()->AddEvent(
                        lpEvent, liId, liSize);
                // Dynamic components observe completion directly (ARTIST
                // LargeCarComponent 0x8241B7A8/0x8241B868). These local module
                // notifications bypass DispatchInboundGuiEvents, so fan them
                // out here as well as delivering them to the cache/controller.
                // The PC loader also emits id-zero Apt registrations for every
                // bundled clip (CgsGuiResourceModulePC.cpp); those are view data,
                // not completions for a component's named resource request.
                if ((liId == 14 && (!lbAptMovie
                         || reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(
                                lpEvent)->muLoadRequestId != 0))
                    || liId == 16)
                    RouteEventToFlow(lpEvent, liId, liSize);
                if (lbAptMovie)
                    mViewInputBuffer.GetViewStateQueue()
                        .CgsModule::VariableEventQueue<65536, 16>::AddEvent(lpEvent, 14, liSize);
                else if (lbAptUnload)
                    mViewInputBuffer.GetViewStateQueue()
                        .CgsModule::VariableEventQueue<65536, 16>::AddEvent(lpEvent, 15, liSize);
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

    // ARTIST82507800; the native caller invokes this immediately after
    // CgsGui::GuiModule::Update, while the same module OutputBuffer is still live.
    void GuiModule::HandleEventsPostBaseModuleUpdate(
        CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
    {
        lpOutput->LockForRead();

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        s32 liEventType = lpOutput->GetOutEventQueue()->GetFirstEvent(&lpEvent, &liEventSize);
        while (lpEvent != 0)
        {
            switch (liEventType)
            {
            case 493:
            {
                const GuiEventRequestCollisionWorldEvent* lpRequest =
                    reinterpret_cast<const GuiEventRequestCollisionWorldEvent*>(lpEvent);
                switch (lpRequest->meEventType)
                {
                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_INVALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_IDLE",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1729);
                        CgsDev::Assert::EndAssert();
                    }
                    // ARTIST stbx to GuiModule +0x18D83B.
                    mbCollisionWorldInvalidateForNetworking = true;
                    break;

                case GuiEventRequestCollisionWorldEvent::E_COLLISON_WORLD_VALIDATE:
                    if (meCollisionWorldStateValidationState !=
                        E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "meCollisionWorldStateValidationState == E_COLLISIONWORLDSTATEVALIDATIONSTATE_INVALID_FOR_NETWORKING",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1737);
                        CgsDev::Assert::EndAssert();
                    }
                    // ARTIST stbx to GuiModule +0x18D83C.
                    mbCollisionWorldValidateForNetworking = true;
                    break;

                default:
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "Invalid collsion world request output event type",
                        "..\\..\\..\\GameSource\\Gui/BrnGuiModule.cpp", 1745);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                break;
            }

            case 508:
            case 509:
                mMovieManager.RecvEvent(lpEvent, liEventType);
                break;

            case 514:
            case 515:
                mColourCalibrationScreen.RecvEvent(lpEvent, liEventType);
                break;

            default:
                break;
            }

            liEventType = lpOutput->GetOutEventQueue()->GetNextEvent(
                lpEvent, &lpEvent, &liEventSize);
        }

        lpOutput->UnlockForRead();
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

                case 39:   // StateInterface::RequestResource (dynamic car artwork, etc.)
                    // ARTIST EventInterpreterModule::ProcessOutEvents 0x8285E1D0
                    // forwards these records to its resource-output queue. Feed the
                    // same native request to the resource module alongside cached loads.
                    mModelInputBuffer.LockForWrite();
                    mModelInputBuffer.GetLoadRequests()->AddEvent(lpEvent, liId, liSize);
                    mModelInputBuffer.UnlockForWrite();
                    break;

                case KI_GUIEVENT_PLAY_VIDEO:   // 508
                case KI_GUIEVENT_STOP_VIDEO:   // 509
                    mMovieManager.GetReceiverQueue()->AddEvent(lpEvent, liId, liSize);
                    break;

                case 514:   // colour-calibration screen SHOW
                case 515:   // colour-calibration screen HIDE
                    // X360 GuiModule::HandleEventsPostBaseModuleUpdate @0x82507800 -- the
                    // function that runs immediately after the base module update
                    // (@0x8252A330) and walks the module OUT-event queue, dispatching
                    // 493 -> the collision-world latches, 508/509 -> MovieManager::RecvEvent
                    // and 514/515 -> ColourCalibrationScreen::RecvEvent
                    // (`bl 0x824471D0` @0x825079A8, with r5 still holding the event id).
                    // THIS DRAIN IS THE PC STAND-IN FOR THAT FUNCTION -- it already carries
                    // its 508/509 arm -- so the calibration arm belongs here, at the same
                    // point in the frame (after the flows have ticked and posted).
                    // This is the RE-KEYED form: the console's base module rewraps each
                    // channel-40 GuiEventOut record by its inner event type before the
                    // out-queue is walked, so HandleEventsPostBaseModuleUpdate sees 514/515
                    // directly. The PC queue does not re-key, so the CHANNEL-40 form is
                    // handled in `case 40` below; both are the same console wire.
                    mColourCalibrationScreen.RecvEvent(lpEvent, liId);
                    // The console does NOT consume it: BridgeGuiToDirector @0x823CBF70 walks
                    // the SAME out-event queue afterwards and turns 514/515 into
                    // SetGotColourCalibration{Shown,Hidden}Event. Forward it on.
                    mGuiOutQueue.AddEvent(lpEvent, liId, liSize);
                    break;

                case 40:   // channel 40: GuiEventOut command records -> the game bridge
                {

                    // Commands 514 / 515 -- the colour-calibration screen show/hide requests.
                    // BrnGui::CrashNavColourCalibrate::ShowCalibrationCard @0x824CE7A8 posts
                    // them on CHANNEL 40 as { muHeader0 = 1, muEventType = 514/515,
                    // muHeader2 = 12 } 16-byte records (`li r5,0x28` = channel 40,
                    // `li r6,0x10` = 16, `li r11,0x202`/`0x203`), NOT as type-keyed events.
                    // On the console the base module's out-event rewrap turns them into
                    // plain 514/515 before HandleEventsPostBaseModuleUpdate @0x82507800 sees
                    // them; the PC queue keeps them on their channel, so the arm is applied
                    // here too. NOT consumed -- BridgeGuiToDirector decodes the same record
                    // out of mGuiOutQueue (BrnGameModule.cpp `case 514:` / `case 515:`), so
                    // it must still be forwarded below.
                    if (liSize >= 8)
                    {
                        const u32 luCalibrationCommand =
                            reinterpret_cast<const u32*>(lpEvent)[1];
                        if (luCalibrationCommand == 514u || luCalibrationCommand == 515u)
                        {
                            const CgsModule::Event* lpCalibrationPayload = lpEvent;
                            if (liSize >= 12)
                            {
                                const u32 luOffset = reinterpret_cast<const u32*>(lpEvent)[2];
                                if (luOffset >= 12u && static_cast<s32>(luOffset) < liSize)
                                {
                                    lpCalibrationPayload =
                                        reinterpret_cast<const CgsModule::Event*>(
                                            reinterpret_cast<const u8*>(lpEvent) + luOffset);
                                }
                            }
                            mColourCalibrationScreen.RecvEvent(
                                lpCalibrationPayload, static_cast<s32>(luCalibrationCommand));
                        }
                    }

                    // ⭐⭐ INNER ID 144 -- GuiEventRunFsm posted BY A FLOW STATE. This is the
                    // seat the offline results screen's exit needed and did not have.
                    //
                    // The console's chain: InstantResultsState::TriggerExitResults @0x824D58A8
                    // ends with OutputGuiEvent<GuiEventRunFsm> @0x824938D0, which stack-builds
                    // {sizeof=0x18, type=0x90, offset=0x10} + the 24-byte record and AddEvents
                    // it on CHANNEL 40. EventInterpreterModule::ProcessOutEvents' case '('
                    // then UNWRAPS it -- @0x8285E568 `lwz r11,8(r22)` (offset) / `lwz r6,0(r22)`
                    // (payload size) / `lwz r5,4(r22)` (inner type) -> AddEvent into the module's
                    // out-event buffer -- and the module bus fans that channel back around as
                    // in-events, where GuiModule::Update's case 144 hands it to
                    // GuiFsmController::RunFsm. (That loop-back is the same one the profile
                    // manager's out-queue pump below already models, and it is how the
                    // autosave-icon flag reaches the always-available manager.)
                    //
                    // This build routes channel 40 to the game bridge only, so without this arm
                    // a state-posted RunFsm is a SILENT DROP: BridgeGuiToGameState has no
                    // case 144 either (its X360 switch does not carry one -- checked), so the
                    // record simply evaporates and the results screen ADVANCEs into nothing.
                    // Answered at the drain, one hop earlier than the console's bus round-trip
                    // -- the identical accommodation the 507 arm above already makes, and for
                    // the identical reason. NOT consumed: the record is still forwarded below,
                    // exactly as the console still puts it on the out buffer.
                    // DELETE-WHEN the out-buffer -> in-events bus loop-back is modelled
                    // generically; then this becomes a plain forward.
                    if (liSize >= 12)
                    {
                        const u32* lpuRecord = reinterpret_cast<const u32*>(lpEvent);
                        const u32  luInnerType   = lpuRecord[1];
                        const u32  luInnerOffset = lpuRecord[2];
                        if (luInnerType == 144u && luInnerOffset >= 12u &&
                            static_cast<s32>(luInnerOffset) + static_cast<s32>(sizeof(GuiEventRunFsm))
                                <= liSize)
                        {
                            mFsmController.RunFsm(
                                reinterpret_cast<const GuiEventRunFsm*>(
                                    reinterpret_cast<const u8*>(lpEvent) + luInnerOffset));
                        }
                    }

                    // EventInterpreterModule::ProcessOutEvents @0x8285E568 unwraps a
                    // channel-40 GuiEventWrapper before it publishes the GuiModule output:
                    // word 0 is the raw payload size, word 1 is the event type, and word 2
                    // is the payload offset.  Every game-module bridge consumes that
                    // canonical, type-keyed queue; leaving the wrapper keyed as 40 makes
                    // BridgeGuiToSound miss events such as voice-over request 466.
                    const u32* const lpuWrapper = reinterpret_cast<const u32*>(lpEvent);
                    const u32 luPayloadSize   = lpuWrapper[0];
                    const u32 luEventType     = lpuWrapper[1];
                    const u32 luPayloadOffset = lpuWrapper[2];
                    const CgsModule::Event* const lpRawEvent =
                        reinterpret_cast<const CgsModule::Event*>(
                            reinterpret_cast<const u8*>(lpEvent) + luPayloadOffset);

                    mGuiOutQueue.AddEvent(
                        lpRawEvent, static_cast<s32>(luEventType),
                        static_cast<s32>(luPayloadSize));

                    if (luEventType == 141u)
                    {
                        lResponse.lpKeyboard = &mGuiKeyboard;
                        if (mGuiOutQueue.AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                                 lResponse.GetEventType(), static_cast<s32>(sizeof(lResponse))))
                            RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                             lResponse.GetEventType(), static_cast<s32>(sizeof(lResponse)));
                    }

                    // ProcessOutEvents also returns the unwrapped event to the interpreter's
                    // observer fan-out.  This is why a state that posts voice-over event 466
                    // observes 466 as "started" on the following GUI update, while sound later
                    // posts the distinct completion event 467 through its pre-update output.
                    // ARTIST GuiModule::Update 0x828602C8 appends the output events
                    // to mLoadNotifications for the next input/view pass. The PC
                    // pump already delivers these to observers here; include the
                    // renderer observer too, as DispatchInboundGuiEvents does.
                    // Without it, screen-posted ticker messages never reach the view.
                    mCustomRendererManager.RecvEvent(lpRawEvent, static_cast<s32>(luEventType));
                    RouteEventToFlow(
                        lpRawEvent, static_cast<s32>(luEventType),
                        static_cast<s32>(luPayloadSize));

                    break;
                }

                case 41:   // channel 41: GuiOutViewState records -> the view input queue
                {
                    const CgsGui::GuiEventPlayAptMovie* lpPlay =
                        reinterpret_cast<const CgsGui::GuiEventPlayAptMovie*>(lpEvent);
                    if (lpPlay->muEventType == 18)   // PlayAptMovie {name, level}
                    {
                        PrepareAptRuntime();   // idempotent

                        struct { const char* mpacMovieName; s32 miLevelNum; } lBody =
                            { lpPlay->mpacMovieName, lpPlay->miLevelNum };
                        mViewInputBuffer.GetViewStateQueue()
                            .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lBody), 18,
                                static_cast<s32>(sizeof(lBody)));
                    }
                    break;
                }

                case 155:  // menu-music request (0 = stop)
                    // The model channel is 155, while the typed sound event carried
                    // by it is GuiEventPlayMusicOnMenuStream (23). Publish the typed
                    // id so BridgeGuiToSound reaches MusicEffect::Notify.
                    CGS_ASSERT(mpOutputBuffer != 0, "mpOutputBuffer");
                    mpOutputBuffer->AddEvent(lpEvent, 23, liSize);
                    break;

                case 201:  // GUI audio trigger -> the module output event channel
                    CGS_ASSERT(mpOutputBuffer != 0, "mpOutputBuffer");
                    mpOutputBuffer->AddEvent(lpEvent, liId, liSize);
                    break;

                case 42:
                {
                    // ⭐⭐ CHANNEL 42 IS *NOT* A DEAD-END, AND THE [FLAG] THAT STOOD HERE
                    // ("consumers are module-internal follow-ons") THREW THE WHOLE CHANNEL
                    // AWAY. It is the console's OutputInternalState<T> channel -- the twin
                    // of channel 40's OutputGuiEvent<T>, differing only in the AddEvent
                    // selector (`li r5,0x2A` vs `li r5,0x28`; compare
                    // StateInterface::OutputInternalState<GuiEventShowHideHud> @0x82493C98
                    // with OutputGuiEvent<GuiEventShowHideHud> @0x82493988 -- byte for byte
                    // the same 16-byte {1,148,12,flag} record).
                    //
                    // The console consumes it in EventInterpreterModule::ProcessOutEvents
                    // @0x8285E1D0, case '*' (== 42) @0x8285E6C8:
                    //     lwz  r11, 8(r22)      ; headerOffset  (record[2] == 12)
                    //     addi r3, r18, 0x15C0  ; the module's OWN internal in-queue
                    //     lwz  r6, 0(r22)       ; payload size  (record[0])
                    //     add  r4, r11, r22     ; payload = record + headerOffset (BYTES)
                    //     lwz  r5, 4(r22)       ; event type    (record[1])
                    //     bl   VariableEventQueue<18432,16>::AddEvent
                    // i.e. it UNWRAPS the record and re-posts the inner event -- exactly as
                    // the sibling cases '(' (40) and ')' (41) unwrap theirs -- into a queue
                    // that EventInterpreterModule::Update @0x8285F670 splices into the
                    // inbound stream at the top of the NEXT frame:
                    //     EventQueue->Append(this + 0x15C0); (this + 0x15C0)->Clear();
                    //     ProcessInEvents(this, EventQueue);
                    // so an internal-state event is fanned to every registered observer
                    // one frame later, like any other GUI event.
                    //
                    // WHAT THE DROP COST, MEASURED: the in-game HUD stayed drawn ON TOP of
                    // the pause screen. InGame::OpenDriverDetails @0x824DA700 (and
                    // OpenMainMap / OpenEventMap / ShutDownHudComponents) post exactly this
                    // record -- GuiEventShowHideHud{0} -- and it is what
                    // FBurnMainHudState::UpdatePermenant's case 148 turns into
                    // SendStateEvent("PAUSE"), and what PausedHudState::Update's case 148
                    // (payload byte non-zero, from InGame::OnEnter's flag=1 twin) turns
                    // back into "UNPAUSE". Producers, consumers and the HUD flow's PAUSED
                    // slot were all already here; only this hop was missing.
                    //
                    // RouteEventToFlow is this build's ProcessInEvents fan-out, and this
                    // drain runs AFTER the flows have ticked and after the per-frame
                    // in-queue Clear -- so posting here lands the event for the next
                    // frame's flow Update, which is the console's one-frame defer exactly.
                    const u32 luHeaderSize = reinterpret_cast<const u32*>(lpEvent)[2];
                    const s32 liInnerId    =
                        static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[1]);
                    const s32 liPayload    =
                        static_cast<s32>(reinterpret_cast<const u32*>(lpEvent)[0]);
                    // :958 "Invalid raw event in EventInterpreterModule::ProcessOutEvents"
                    CGS_ASSERT(luHeaderSize >= 12u,
                               "Invalid raw event in EventInterpreterModule::ProcessOutEvents");
                    if (luHeaderSize >= 12u &&
                        static_cast<s32>(luHeaderSize) + liPayload <= liSize)
                    {
                        const u8* lpu8Body =
                            reinterpret_cast<const u8*>(lpEvent) + luHeaderSize;
                        RouteEventToFlow(
                            reinterpret_cast<const CgsModule::Event*>(lpu8Body),
                            liInnerId, liPayload);
                    }
                    break;
                }

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

    // The original call at ARTIST 82529AE4..82529B04 borrows the caller's
    // GameData pair. Keep its selected-port call together without an inner lock.
    void GuiModule::UpdateCalibrationScreen(
        BrnResource::GameDataIO::InputBuffer* lpGameDataInput,
        const BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput)
    {
        if (lpGameDataInput != 0)
        {
            mColourCalibrationScreen.Update(lpGameDataInput, lpGameDataOutput,
                                            &mGuiOutQueue, mGuiConfig.mpTextureAllocator);
        }
    }

    // ⭐⭐⭐ THE GUI END-OF-FRAME NOTIFY, restored 2026-09-07. X360 @0x824F1008, verbatim:
    //     addis r3, r3, 5 ; addi r3, r3, -0x3D70 ; b BrnGui__CustomRendererManager__EndOfFrame
    // -- +327680-15728 == +311952 == mCustomRendererManager, tail-called. That in turn is
    //     addis r3, r3, 2 ; addi r3, r3, -0x49C0 ; b BrnGui__NetworkPlayerImageRenderer__SwapBuffers
    // -- +131072-18880 == +112192 == the embedded NetworkPlayerImageRenderer. So the whole
    // notify is one thing: SWAP THE PLAYER-IMAGE TRIPLE BUFFER.
    //
    // ⛔ IT WAS DECLARED NOWHERE, DEFINED NOWHERE AND CALLED BY NOTHING -- which links in
    // total silence. Both bodies below it were already correct and had NEVER EXECUTED. This
    // is the exact sibling of ParticleModule::EndOfFrame (c227a165, issue #17), named by the
    // same stale banner line in BrnGameModule::OnEndOfUpdateFrame.
    //
    // ⭐ WHAT ITS ABSENCE COSTS, from the console's own arithmetic (no eyeballing):
    //   NetworkPlayerImageRenderer::Construct @0x82445A50 seeds `*(this+736) = 1` and
    //   `*(this+740) = 2` -- miCurrentRenderTexture = 1, miCurrentCopyToTexture = 2 -- and
    //   SwapBuffers @0x82445E58 is the ONLY writer of either word in the whole image (both
    //   `(x+1) % 3`, via the 0x55555556 magic-multiply pair at 0x82445E9C/0x82445EA4).
    //   RecvEvent @0x82449CA0 case 258 copies an arriving picture into slot `v4[185]` ==
    //   miCurrentCopyToTexture and flags `maabRender*Texture[idx][thatSlot]`; GetRenderOutput
    //   @0x82445CC0 reads slot `miCurrentRenderTexture` and consults the flags at THAT slot.
    //   With the cursors frozen at 1 and 2, every picture is written to slot 2 and every read
    //   asks slot 1, whose flags no code path can ever set. ⇒ A RECEIVED PLAYER PICTURE CAN
    //   NEVER BE DISPLAYED. The two type-258 producers are single-player:
    //   LicenseComponent::SendPlayerPictureEvent and PhotoBoothComponent (see the bridge note
    //   in DispatchInboundGuiEvents) -- the licence card and the photo booth.
    //   Secondary: SwapBuffers is also the only per-frame writer of the Flapt special texture
    //   ("CustomComponentTexture.tif", FlaptFile::SetSpecialTexture @0x8246D750) and of
    //   FlaptRenderer::miSpecialTextureShaderProgram, so both stayed at whatever
    //   NetworkPlayerImageRenderer::Prepare @0x82451560 left on the frame it ran.
    //
    // ⚠️ NOT a fix for GitHub #9 (minimap blips) or #11 (in-game event UI): this chain is
    // three tail calls long and touches nothing but the player-image buffer set. Measured,
    // not assumed -- see the wave report.
    void GuiModule::EndOfFrame()
    {
        mCustomRendererManager.EndOfFrame();
    }

    // ARTIST 0x825146B8 plus its CgsGui::GuiModule::Render core @0x8285AF38.
    // That core is inlined here while the canonical base/model class hierarchy
    // remains unrecovered; this preserves its operations without a proxy base.
    // The scheduler supplies the real view/module inputs and renderer output.
    // Preparation gates the entire original pass; no renderer pointer is selected here.
    // Perfmon aliases: dword_82F2767C = miGuiModuleRender, dword_82F330B8 = miGuiRender.
    void GuiModule::Render(CgsGui::ViewIO::InputBuffer* lpViewInput,
                           CgsGui::CgsGuiModuleIO::InputBuffer* lpInput,
                           RendererIO::OutputBuffer* lpRenderOutput)
    {
        CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiModuleRender);
        CGS_ASSERT(lpInput != nullptr, "lpInput");
        CGS_ASSERT(lpRenderOutput != nullptr, "lpRenderOutput");
        if (mbPrepared && mDebugComponent.IsGuiRenderingEnabled())
        {
            CgsDev::PerfMonCpu::StartMonitor(GuiPerfmons::miGuiRender);
            // 0x823B6FE0/7060 carry these non-gating checks around the bridge.
            CGS_ASSERT(lpViewInput != nullptr, "lpInputBuffer");
            CGS_ASSERT(lpInput != nullptr, "lpOutputBuffer0");
            CgsModule::LockBuffersForIO(lpViewInput, lpInput);
            lpViewInput->SetImRenderers(lpInput->GetImRenderers());
            CGS_ASSERT(lpViewInput != nullptr, "lpInputBuffer");
            CGS_ASSERT(lpInput != nullptr, "lpOutputBuffer0");
            CgsModule::UnlockBuffersForIO(lpViewInput, lpInput);
            mViewModule.Render(lpViewInput);
            CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiRender);

            UpdateAndRenderMovieManager(lpViewInput);
            mEffectsArbitrator.GenerateEffectFrameEvents(lpRenderOutput);
        }
        CgsDev::PerfMonCpu::StopMonitor(GuiPerfmons::miGuiModuleRender);
    }

    // BridgeFromViewToOutput @0x8285DE10 -- an export hole, read with tools/re/ppcdis.py:
    //     Append<18432,16>(this + 111644, ViewIO::OutputBuffer::GetGuiEventQueue(viewOut));
    //     OutputBuffer::AddGuiOutEvents(moduleOut, *(view + 57352) + 12);
    //     OutputBuffer::AddGuiOutEvents(moduleOut, view + 56768);   // ViewModule::mOutputEventQueue
    // (called from CgsGui::GuiModule::Update @0x82860548, right after the view Update).
    // The first two carry the view OUTPUT buffer's queue -- the Apt trigger records 21/22
    // that Update above already drains by hand. The third had NO PC seat: the view module's
    // OWN out queue, where ProcessIncomingAptEvent @0x8285EAE8 posts 33
    // GuiEventLoadingScreenState {visible} for PlayAptLoadingMovie 19 / StopAptLoadingMovie
    // 20. The sound logic module drains the module out queue (BridgeGuiToSound @0x823C0A58)
    // and turns 33 into bit 1 of its dispatch flags word -- half of MusicEffect::
    // UpdateParams' stream-pause gate -- so on PC that gate could never hold while the
    // loading screen was up, and HUDEffect::Notify's loading-screen test never tripped.
    void GuiModule::BridgeFromViewToOutput()
    {
        CGS_ASSERT(mpOutputBuffer != 0, "mpOutputBuffer");
        mpOutputBuffer->Append(mViewModule.GetOutputEventQueue());
    }

    // @ 0x82511240 -- pump the movie manager (the movie pass of the GUI render).
    // ARTIST 0x82511240: movie update precedes the view-input read bracket.
    // MoviePlayer::Render records into the same supplied Im2d command buffer.
    void GuiModule::UpdateAndRenderMovieManager(CgsGui::ViewIO::InputBuffer* lpViewInput)
    {
        mMovieManager.Update();
        lpViewInput->LockForRead();
        mMovieManager.Render(lpViewInput->GetImRenderers().mpIm2dRenderBuffer);
        lpViewInput->UnlockForRead();
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
