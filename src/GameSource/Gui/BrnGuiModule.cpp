#include "GameSource/Gui/BrnGuiModule.h"
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"   // [H3b] MapTransform (the sat-nav view-rect install)

#include <cstdio>                                                         // std::snprintf (log formatting)
#include <windows.h>                                                      // [gate] GetEnvironmentVariableA (BRN_FLAPT_AFTER_DISPATCH)
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
#include "GameSource/GameState/Progression/BrnProfile.h"                   // BrnProgression::Profile::Construct (the PC progression block seed)
#include "GameSource/Gui/BrnGuiAlwaysAvailableComponentsManager.h"        // AlwaysAvailableComponentsManager + free accessor (bodied below)
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (flow interface wiring)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"       // CgsGui::AptAuxPointer (the AptAux singleton)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::AptCommunicator (the per-frame trigger publish)
#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"             // CgsSystem::MenuMusicPC (the menu-stream music player)
#include "GameShared/GameClasses/System/PC/CgsGuiSoundPC.h"               // CgsSystem::GuiSoundPC (the GUI presentation blips)
#include "GameShared/GameClasses/Sound/Playback/CgsCommon.h"              // CgsSound::Playback::Name::MakeHash (event-155 keys)
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // FLApt live-instance allocator
#include "GameSource/Resource/BrnGameDataModuleIO.h"                      // BrnResource::GameDataIO::Input/OutputBuffer (the colour-calibration screen's IO)
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"        // AllocatorList::GetRWLinearResourceAllocator (mGuiConfig.mpTextureAllocator)
#include "GameSource/GameFlowController/TopLevel/BrnGameMainFlowStates.h" // GetScriptedLoadGameData{Input,Output} (the PC stand-in for the scheduler's GameData IO pair)

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
    //
    // SIZE TRAP (intro wave, 2026-07-30): the progression block must be sizeof(Profile),
    // NOT KI_PROGRESSION_PROFILE_SIZE_BYTES (118064). Those are two different numbers --
    // 118064 is the SERIALISED segment width inside BrnGuiSaveLoad::Profile, while the live
    // object is 120840 bytes on X360 (BrnProfile.h:151) and wider still on the x64 target.
    // The array used to be 118064, which was harmless only while nothing ever wrote the live
    // object; the moment Profile::Construct() runs over it, an 118064-byte array overruns by
    // thousands of bytes into the next static (an access violation a few frames later).
    alignas(16) u8 s_pcProgressionProfileBacking[sizeof(BrnProgression::Profile)];
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

// ============================================================================
// THE APT BRING-UP (transplanted from the retired BrnGuiAptRuntime.cpp): the
// GuiModule owns the Apt bring-up driver + the PC render buffer, exactly as the
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
#include "GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.h"             // DispatchAptIm2dRenderBufferPC (slice-5 step A)
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"          // CgsGui::AptAux / AptAuxPointer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"// CgsGui::AptImRendererSet / AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"                // CgsGui::ViewModule (real Apt/text owner)
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h"                       // CgsGuiModuleIO::ImRendererSet

// ---- the Apt engine leaves that ARE bodied (the bring-up drives these) ------
#include "SDKs/EATech/Apt/DogmaAllocator.h"                   // DOGMA_PoolManager
#include "SDKs/EATech/Apt/AptValueGCPoolManager.h"            // AptValueGC_PoolManager
#include "SDKs/EATech/include/Apt/AptDefine.h"                // gpGCPoolManager / gpNonGCPoolManager
#include "SDKs/EATech/include/Apt/AptActionInterpreter.h"     // AptActionInterpreter + AptInitParmsT
#include "SDKs/EATech/include/Apt/AptTarget.h"                // AptTarget + gpAptTarget singletons
#include "SDKs/EATech/include/Apt/AptString/EAString.h"       // EAStringC (loader file name)
#include "SDKs/EATech/include/Apt/AptLoader.h"                // AptLoader / GetTarget / AptFilePtr

// ---- the D3D9 2D immediate render buffer the Apt rasteriser fills + we flush --
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V> (AptAux render-set only)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"  // CgsGraphics::Im2d (the PROVEN immediate render path)
#include "pc/gcm/renderengine/texture.h"                      // renderengine::Texture (mesh texture binding -> mpD3DTexture)
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
#include "pc/gcm/renderengine/device.h"                             // renderengine::gDevice (font atlas gate)

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
    bool s_bBringUpAttempted = false;   // ran the (idempotent) bring-up at least once

    CgsGui::ViewModule* s_pViewModule = nullptr;
    CgsMemory::LinearMalloc* s_pFlaptLinear = nullptr;   // the FLApt instance allocator (GuiModule-owned)

    // ---- the Apt render buffer the engine's render callbacks fill --------------
    // AptRenderHandler::GetIm2dRendererType() returns mpImRenderers->mpIm2dRenderer
    // (an AptIm2dRenderBuffer*), and AptRenderHandler::Render appends its draw
    // commands to that buffer's mCommandBuffer (a real CgsGraphics::ImRenderBuffer<V>).
    // We OWN that buffer here and flush it to D3D9 each frame via Dispatch() -- this is
    // how Apt geometry would reach the screen. (Reuse note: this is the SAME
    // ImRenderBuffer<Basic2dColouredTexturedVertex> family the loading screen + debug
    // HUD draw through; the Dispatch() path is the fully-reconstructed PC D3D9 flush.)
    CgsGui::AptIm2dRenderBuffer s_AptRenderBuffer;
    bool                        s_bRenderBufferReady = false;

    // A non-null 3D-renderer sentinel so AptRenderHandler::Render's
    // `mpImRenderers->mp3dRenderer != 0` assert passes. The Apt boot/title movies are
    // 2D-only, so the 3D renderer is never dereferenced on this path; a sentinel keeps
    // the assert quiet without dragging in the (out-of-scope) 3D render set.
    // FLAG: sentinel only -- a real Im3dRenderBuffer is out of this slice's scope (the
    // title movie draws 2D). If a 3D Apt path is ever exercised this must become real.
    int s_i3dRendererSentinel = 0;

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
    s32         s_iRenderFrame           = 0;       // render-walk per-frame trace counter
    bool        s_bFlushProbed           = false;   // emitted the one-shot render-flush probe yet

    // PHASE 2: the per-slot movie pool backings + the host bundle-IO leaf (AptLoadMovieSlot)
    // are RETIRED -- every movie slot's bundle now loads through the real GuiResourceModule's
    // [PC] servicer (CgsGuiResourceModulePC.cpp owns the streamed-apt bank), so the host no
    // longer carves its own resident movie pools here.

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).

    // ---- a static-backed rw resource allocator for the Apt render buffer -------------------
    // FLAG (PC bring-up): the RenderWare DEFAULT resource allocator's DoAllocate(256KB) returns
    // null at Apt bring-up time (its heap has no room for the render buffer's 4x(256KB+128)
    // streams -- verified at runtime: "DoAllocate(256KB)=0"), so the ImRenderBuffer's Prepare
    // carve fails and BeginRendering AVs on the null command buffer. We back the Apt render
    // buffer with a dedicated static bump pool instead (a small IResourceAllocator over BSS
    // storage). This does NOT touch the ImRenderBuffer/Im2d/D3D9 leaf -- it only supplies the
    // buffer's backing memory (the host owns the render-buffer's storage, exactly as the console
    // does through its own render-heap). 2 MB covers the 4 streams with headroom.
    // 4 MB (was 2 MB): the 128 KB carves were sized for the boot/title movies; the
    // IN-GAME HUD walk plus the layer-2 custom renderers (the tutorial ticker's glyph
    // strips) need larger streams -- see the Prepare call's sizing note.
    const u32 KU_APT_RB_POOL_BYTES = 4u * 1024u * 1024u;
    u8  s_aAptRenderBufferPool[KU_APT_RB_POOL_BYTES];

    struct AptRenderBufferAllocator : public rw::IResourceAllocator
    {
        u32 muUsed = 0;

        rw::Resource DoAllocate(const rw::ResourceDescriptor& lrDescriptor, const char* /*lpcName*/) override
        {
            rw::Resource lResult;
            for (u32 lu = 0; lu < 4; ++lu)
                lResult.m_baseResources[lu] = nullptr;

            const u32 luSize  = lrDescriptor.m_baseResourceDescriptors[0].m_size;
            u32       luAlign = lrDescriptor.m_baseResourceDescriptors[0].m_alignment;
            if (luAlign < 1u) luAlign = 1u;

            // Bump-align the cursor, then hand out [cursor, cursor+size) if it fits.
            const u32 luOffset = (muUsed + luAlign - 1u) & ~(luAlign - 1u);
            if (luOffset + luSize <= KU_APT_RB_POOL_BYTES)
            {
                lResult.m_baseResources[0] = &s_aAptRenderBufferPool[luOffset];
                muUsed = luOffset + luSize;
            }
            return lResult;
        }
    };
    AptRenderBufferAllocator s_AptRenderBufferAllocator;

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
    // of the console's async .apt stream -- are RE-HOMED to the marked PC leaf TU
    // GameShared/GameClasses/Gui/PC/CgsAptStreamLoaderPC.cpp.)

    // NOTE: the Apt allocator/interpreter/target bring-up (the invented Steps 1/2/5 that
    // used to live here -- a local AptAllocatorInitialize + WireAllocatorGlobals + the
    // interpreter init + AptCreateTargetInstance) is RETIRED: it is now the faithful
    // CgsGui::AptAux::InitializeApt @0x82848E50 (CgsAptAux.cpp), which chains the homed
    // AptAllocatorInitialize/AptUpdateInitialize/AptRenderInitialize/AptCreateTargetInstance/
    // AptChangeTargetInstance (SDKs/EATech/Apt/AptInit.cpp). PrepareRuntime() below drives
    // it after the (host-adaptor) render buffer + AptAux::Construct are up.
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

    static bool IsRuntimeReady() { return s_bRuntimeReady; }

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
    // AptBringUpTextSystem -- one-shot: load the 3 typefaces + the language string table
    // and arm the text system the AptAux render handler hands to the Apt string path.
    // Gated on the D3D device (the font atlas FixUp creates D3D textures); retried by the
    // (idempotent) bring-up until the device exists.
    // FLAG stand-in for the un-homed CgsGui::ViewModule::Prepare text/font bring-up (Phase 4c retires
    // this; NOT a permanent PC leaf).
    // -------------------------------------------------------------------------
    static void AptBringUpTextSystem()
    {
        if (s_bTextSystemReady)
            return;
        if (renderengine::gDevice == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] text: device not up yet -- font/language load deferred.\n");
            return;
        }

        CgsResource::RegisterAllResourceTypes();   // idempotent (Font 0x21 + raster handlers)


        // The language MANAGER is prepared by the real staged ViewModule::Prepare
        // (slice 6: its LANGUAGE stage runs mLanguageManager.Prepare; formatting
        // strings derive at LoadStringTable). Only the string-table bundle IO +
        // its queued type-12 notification remain host-side.
        const bool lbStrings = AptLoadLanguageBundle(KC_APT_LANGUAGE_BUNDLE, &s_AptLanguagePool);

        s_bTextSystemReady = lbStrings;   // fonts ride the module chain now (slice 4a)
        char lac[160];
        std::snprintf(lac, sizeof(lac), "[AptRT] text: system %s (fonts=%d strings=%s).\n",
                      s_bTextSystemReady ? "READY" : "INCOMPLETE", 0, lbStrings ? "ok" : "MISSING");
        CgsDev::Log::WriteToLog(lac);
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
        // the steps that have not yet succeeded (e.g. the render buffer waiting on the rw
        // allocator). Log "begin" only on the first attempt to avoid per-frame spam.
        if (!s_bBringUpAttempted)
            CgsDev::Log::WriteToLog("[AptRT] bring-up: begin.\n");
        s_bBringUpAttempted = true;

        // ---- STEP 3: the Apt render buffer (the D3D9 2D buffer the engine fills) --
        // Construct + Prepare the ImRenderBuffer<V> that AptRenderHandler::Render
        // appends to (and we flush via Dispatch each frame). Prepare carves its
        // command + vertex storage from the RenderWare default resource allocator.
        if (!s_bRenderBufferReady)
        {
            s_AptRenderBuffer.mu32Head = 0;
            s_AptRenderBuffer.mCommandBuffer.Construct();

            // Back the render buffer with the dedicated static bump pool (the RW DEFAULT
            // allocator's DoAllocate returns null at this bring-up point -- see the allocator
            // note above). 512 KB command stream + 512 KB vertex stream per buffer (x2 = 4
            // carves, 2 MB of the 4 MB pool). GROWN 2026-08-24 from 128 KB: that sizing was
            // "generous for a single boot/title movie", but the IN-GAME HUD walk plus the
            // layer-2 custom renderers (the tutorial ticker's glyph strips) overflowed it,
            // and failGracefully's rewind DROPS every later append in the frame -- an
            // invisible ticker with every diagnostic green (see the [tut-ticker] OVERFLOW
            // rung in CgsImRenderBufferTemplate.cpp). failGracefully=true so an overflow
            // still rewinds instead of asserting.
            rw::IResourceAllocator* lpAllocator = &s_AptRenderBufferAllocator;
            const bool lbOk = s_AptRenderBuffer.mCommandBuffer.Prepare(
                512u * 1024u, 512u * 1024u, lpAllocator, /*failGracefully*/ true);
            s_bRenderBufferReady = lbOk;
            char lacp[160];
            std::snprintf(lacp, sizeof(lacp),
                "[AptRT] step3 renderbuffer: Construct+Prepare %s (static pool, 512KB cmd / 512KB vtx, used=%u).\n",
                lbOk ? "ok" : "FAILED", s_AptRenderBufferAllocator.muUsed);
            CgsDev::Log::WriteToLog(lacp);
        }

        // ---- STEP 4: AptAux::Construct (the host callback table + render handler) -
        // Build the ImRendererSet (slot0 = our render buffer, 3d slot = sentinel) and
        // hand it to AptAux::Construct, which seeds the render handler + installs the
        // gAptFuncs render-callback family (the engine's render dispatch reaches our
        // AptRenderHandler::Render through it).
        if (!s_bAuxReady)
        {
            // Seed the view module's renderer set for the bring-up window (before the
            // first frame; from then on the real ViewModule::Render @0x82858810 copies
            // the set in from the view input buffer each frame -- GuiModule::Render
            // publishes these same values -- and re-nulls it after RenderInternal).
            CgsGui::ImRendererSet* lpImRenderers = s_pViewModule->GetImRendererSet();
            lpImRenderers->mpIm2dRenderBuffer            = &s_AptRenderBuffer;
            lpImRenderers->mpReserved04                  = nullptr;
            lpImRenderers->mpIm3dRenderBufferUntex       = nullptr;
            lpImRenderers->mpIm3dRenderBufferRacePosition = nullptr;
            lpImRenderers->mpIm3dRenderBufferMenusAndHud = &s_i3dRendererSentinel;

            // Bring the TEXT system up first (fonts + language + glyph batcher) so the
            // handler's text-layout inputs are live from the start. One-shot; device-gated.
            AptBringUpTextSystem();

            CgsGui::AptAux* lpAptAux = s_pViewModule->GetAptAux();
            s_bAuxReady = (CgsGui::AptAuxPointer::mpAptAuxInst == lpAptAux);

            char lac[200];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step4 aux: Construct done. singleton=%p im2d=%p (== &renderbuf %p)\n",
                (void*)CgsGui::AptAuxPointer::mpAptAuxInst,
                (void*)lpImRenderers->mpIm2dRenderBuffer, (void*)&s_AptRenderBuffer);
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
                                        "(render buffer waiting on rw allocator).\n");
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

        // The runtime is "ready" (for channel-41 routing) once the allocator + render
        // buffer + AptAux host are up (all done by/through InitializeApt + step 3/4). The
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
    // bank). Only the DispatchRenderBuffer platform leaf below remains host-side.
    // -------------------------------------------------------------------------

    // RETIRED (2026-07-09, step 6): the component view-state/key-value bridge, the
    // title help-item defaults, and their clip-walk helpers are GONE -- the REAL
    // component framework drives the menu (onLoad -> BuildName -> RegisterComponent
    // -> AddNewAptComponent; per-frame AptAux::UpdateComponents -> AptCommunicator::
    // UpdateAllComponents -> the movie AS UpdateAll -> GetComponentData).

    // -------------------------------------------------------------------------
    // DispatchRenderBuffer -- the PC-platform dispatch leaf that remains after the
    // render DRIVE moved to the real chain (GuiModule::Render -> CgsGui::ViewModule::
    // Render @0x82858810 -> RenderInternal @0x82858AF8 -> AptAux::Render @0x82848FB8
    // -> AptRenderTarget @0x82AF4ED0 -> the AptRender walk). The walk fills
    // s_AptRenderBuffer.mCommandBuffer (the renderer set GuiModule::Render publishes
    // into the view input buffer each frame) inside RenderInternal's Begin/End block;
    // this freezes + flushes it to D3D9 afterwards: Swap -> Clear -> Dispatch (the
    // same ImRenderBuffer<V> path the loading screen/debug HUD dispatch). On the
    // console the render THREAD consumes the filled buffers through the custom-
    // renderer-manager bracket RenderInternal notifies; this leaf is the PC's
    // single-threaded equivalent of that consumption.
    //
    // Hard-gated on the bring-up flags: never swaps a buffer RenderInternal did not
    // just fill (the view render itself is gated by GuiModule::Render on IsReady()).
    // -------------------------------------------------------------------------
    // [gate] BRN_FLAPT_AFTER_DISPATCH -- flush the Apt/GUI command buffer EARLIER, inside
    // BrnGui::ViewModule::RenderInternal and ahead of mFlaptManager.Render(), so the immediate
    // FLAPT channel paints OVER the deferred channel the way the console's single recorded
    // buffer replays it. See BrnGui::DrainAptRenderResidueBeforeFlapt at the foot of this file
    // for the full mechanism.
    //
    // ⭐ DEFAULT IS **ON** (2026-08-27): this is the shipped 2D pixel order, and it is what
    // makes the sat-nav player arrow visible at all. `BRN_FLAPT_AFTER_DISPATCH=0` restores the
    // OLD order on the SAME binary -- deliberately kept, because it is the falsification
    // control for this fix: the arrow must vanish again under =0 and come back under =1
    // without recompiling. Any other value (or unset) means ON.
    //
    // Evidence the reorder is safe (measured, not argued -- risk R1, "is the HUD hidden in
    // menus by a real FLAPT hide or merely by being painted over?"): across the car-select Apt
    // menu (73 deferred draws with 7 opaque FLAPT quads underneath) and the full-screen title
    // surface + black clear, the A/B pixel difference INSIDE every FLAPT quad rect is exactly
    // ZERO, while the same runs' frame-to-frame animation noise is thousands of pixels. The
    // FLAPT quads that survive into menu states carry empty textures, so promoting them above
    // the Apt surfaces changes no pixel. If a future menu makes them opaque, this is the knob
    // that isolates the regression.
    bool FlaptAfterDispatchEnabled()
    {
        static int siState = -1;
        if (siState < 0)
        {
            char lacBuf[8] = { 0 };
            const DWORD luLen = GetEnvironmentVariableA("BRN_FLAPT_AFTER_DISPATCH", lacBuf, sizeof(lacBuf));
            // Unset => ON. Set => ON unless it is explicitly "0".
            siState = (luLen > 0 && lacBuf[0] == '0' && lacBuf[1] == '\0') ? 0 : 1;
        }
        return siState == 1;
    }

    void DispatchAptRenderResidue()
    {
        if (!s_bRuntimeReady || !s_bAuxReady || !s_bRenderBufferReady)
            return;

        // The flush body is re-homed to the PC backend TU (slice 5 step A): the
        // Swap -> Clear -> Dispatch consumption + its one-shot probe live in
        // GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.cpp.
        CgsGui::DispatchAptIm2dRenderBufferPC(&s_AptRenderBuffer);
        ++s_iRenderFrame;
    }


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
    void GuiModule::Construct(const BrnResource::HudMessageController* lpHudMessageController)
    {
        // X360 GuiModule::Construct @0x82518028, pseudocode lines 327-332 -- the console's
        // own argument assert, fired before anything is built.
        CGS_ASSERT(lpHudMessageController != 0, "lpHudMessageController");   // BrnGuiModule.cpp:229

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
        // X360 GuiModule::Construct @0x82518B18-24: MovieManager::Construct is immediately
        // followed by ColourCalibrationScreen::Construct (gm+301600 then gm+306752), with
        // EffectsArbitrator::Construct just ahead of the pair.
        mColourCalibrationScreen.Construct();
        mAlwaysAvailableComponentsManager.Construct();

        // X360 GuiModule::GuiModule @0x827E5B28 constructs the custom-renderer manager as a
        // by-value member; GuiModule::Construct @0x82518028 then drives its Construct.
        // ⭐ This is the object the Apt custom-control render callback reaches through
        // AptRenderHandler::mpCustomRendererManager -- until this leg NOTHING in the PC
        // build instantiated it, so `_type='PlayerImage'` on the licence card (and every
        // other custom control) had nowhere to resolve to.
        mCustomRendererManager.Construct();

        mpGuiEventInputBuffer = 0;
        mpOutputBuffer = 0;
        mpTextureAllocator = 0;   // filled by Prepare (X360 GuiModule::Prepare @0x82518DE0)
        mpGuiHeapAllocator = 0;   // [licence-icon] filled by Prepare (mGuiConfig row +311924, bank 31)
        mbCustomRenderersPrepared = false;   // [licence-icon] the manager staged-prepare latch

        // The flow-controller chain (the X360 Construct's flow set): the cache Construct
        // (the watcher reset @0x82505860 -> 0x824FD978), then the profile manager (X360
        // GuiModule::Construct @0x82518028 hands it the cache, the sign-in watcher, and
        // the view module's language manager), then the flows against the cache; the
        // controller starts UNLOADED on every flow slot.
        mGuiCache.Construct();

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
        // isHighDef == true on this host (the HD apt path) -> the HD rect @0x82FB30A0.
        // MapUtils' static default IS that rect; the explicit install keeps the console's
        // HD/SD pick visible (the SD alt {0.750781238079071, y0, 0.9039062261581421, y1}).
        MapTransform::SetSatNavRect(MapTransform::GetSatNavViewRect());
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
        // The console's BurnoutSkillsManager pair that sits between the mask matrix and this
        // one (BurnoutSkillsManager::Construct(gm + 309032) + the "lpSkillsManager" assert at
        // BrnGuiCache.h:2341 -> `*(gm + 1021864)`) is NOT landed here: GuiCache::mpSkillsManager
        // is still writer-less. It costs no assert -- GetBurnoutSkillsManager
        // (BrnGuiCache.h:799) is a plain inline read with no guard -- and its readers
        // (PlayerPositionSingleComponent::RenderValue's today's-best arm) are game-mode 15/16
        // only, so no offline event reaches them. Named as the known gap, not fabricated.
        mFreeburnChallengeManager.Construct(&mGuiCache);
        mGuiCache.SetChallengeManager(&mFreeburnChallengeManager);

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

        // ---- the HUD-message pair (gateui wave, round 2) --------------------------------
        // X360 GuiModule::Construct @0x82518028, lines 277-278 + 376, verbatim order:
        //     HudMessageDirector::Construct(gm + 639264, gm + 552, gm + 1005376);
        //     HudMessageAnalyzer ::Construct(gm + 660992, gm + 639264);
        //     ... *(gm + 1021876) = gm + 639264;   // GuiCache::SetHudMessageDirector
        //
        // ⚠ FLAG PC bring-up (argument sourcing only -- both bodies are real
        // reconstructions): the console's first Construct argument is its embedded
        // CgsGui::ModelModule (gm+552 == the CgsGui::GuiModule base's +0x228). This tree's
        // BrnGui::GuiModule derives straight from CgsModule::ModuleSingleBuffered and there
        // is no reconstructed CgsGui::ModelModule anywhere, so there is nothing to pass and
        // NOTHING IS FABRICATED HERE -- the argument is NULL and the console's own
        // "lpModelModule" assert (BrnGuiHudMessageDirector.cpp:80) fires ONCE per boot as
        // the honest marker for that gap. It costs no behaviour: mpModelModule is
        // write-only in the whole recovered surface (the console reaches the model IO
        // buffer through ModelModule::AddGuiEvents @0x8285DF50, which never touches its own
        // `this`), and HudMessageDirector::Update takes the buffer as its own argument.
        // DELETE-WHEN CgsGui::ModelModule is reconstructed (or BrnGui::GuiModule is given
        // its CgsGui::GuiModule base).
        mHudMessageDirector.Construct(0, &mGuiCache);
        mHudMessageAnalyzer.Construct(&mHudMessageDirector);

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
        mbPrepared          = false;
    }

    bool GuiModule::Prepare()
    {
        // ---- X360 GuiModule::Prepare @0x82518D68, STAGE 1 (the mGuiConfig fill) -----------
        // The console's stage 1 reads seven pool ids and seven allocators out of the GameData
        // OUTPUT buffer's AllocatorList. The ONE row this build needs is
        //     *(gm + 311932) = AllocatorList::GetRWLinearResourceAllocator(list, 42)
        // (0x82518DE0's `ori r21, r11, 0xC27C`), the allocator ColourCalibrationScreen::Update
        // carves its texture-state resource from. Bank 42 is "Network Image Allocator" and is
        // a real RW-LINEAR bank on this build (BrnMemoryMapData.h:109), created by
        // GameDataModule::CreateAllocators.
        // FLAG PC drive point: the console's module scheduler hands GuiModule::Prepare the
        // GameData IO pair; on PC the pair is the one the whole scripted load already uses.
        // The ordering that makes this safe is proven, not assumed: BrnGameModule::GamePrepare
        // runs `mGameDataModule.Prepare(0,0)` to completion (returning false until done) and
        // the loading flow -- which is what calls GuiModule::Prepare -- only runs once
        // GamePrepare has returned done, so CreateAllocators has already registered bank 42.
        // Read once here, exactly as the console reads it once in Prepare.
        // NO LOCK PAIR OF ITS OWN (step-11 verifier catch): the only caller,
        // InitialLoadingScreen's stage-2 leg in BrnGameMainFlowStates.cpp (:691 LockForRead ..
        // :1073 Prepare() .. :789 UnlockForRead), already holds this buffer's read lock, and
        // CgsModule::IOBuffer's lock is a single status BIT (CgsIOBuffer.h:26/:42), not a count --
        // an inner Unlock would clear the caller's lock and its own UnlockForRead would then assert
        // "Not locked for read". GetAllocatorList's read assert is satisfied by the caller's bracket.
        mpTextureAllocator = 0;
        mpGuiHeapAllocator = 0;
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
                    mpTextureAllocator = lpAllocators->GetRWLinearResourceAllocator(42);
                    // [licence-icon] the console's sibling stage-1 row:
                    //   *(gm + 311924) = AllocatorList::GetRWGeneralResourceAllocator(list, 31)
                    // -- the heap allocator the custom-renderer manager Prepare receives.
                    // (reinterpret: rw::core::GeneralResourceAllocator is forward-declared
                    // opaque in this TU; on the rwcore ABI it IS an IResourceAllocator -- the
                    // console stores the same pointer in the IResourceAllocator config row.)
                    mpGuiHeapAllocator = reinterpret_cast<rw::IResourceAllocator*>(
                        lpAllocators->GetRWGeneralResourceAllocator(31));
                }
            }
        }

        // Load VIDEOS\VIDEOLIST.BUNDLE synchronously (English; see MovieManager::Prepare) and
        // publish the manager so the renderer draws the active movie each frame (interim render
        // bridge; the X360 renders it through the GUI's own ViewIO ImRenderers).
        mMovieManager.Prepare(0);
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
        // callback table + the render buffer) BEFORE the flow prepares, so the flow
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
        s_GuiAccessPointers.mpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;

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
        // general -> mpGuiHeapAllocator, slot 42 linear -> mpTextureAllocator; the X360 stage-7
        // call is `Prepare(*(gm+311924), *(gm+311932))` @0x82518D68). One call advances the
        // staged prepare as far as it can this frame; the console re-enters Prepare until every
        // stage passes, so Update owns the PC pump (see the [licence-icon] block there).
        const bool lbCustomRenderersPrepared =
            mCustomRendererManager.Prepare(mpGuiHeapAllocator, mpTextureAllocator);
        mbCustomRenderersPrepared = lbCustomRenderersPrepared;
        mViewModule.SetCustomRendererManager(&mCustomRendererManager, 10,
                                             /*lpReplaySerialiser*/ 0);
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
        // ...then run the REAL BrnProgression::Profile::Construct @0x823708A8 over the
        // progression block. On the console the live profile is a member of the GameState
        // module and its Construct is what seeds the empty-profile state -- including
        // mbIsNewProfile = true, which is the byte BrnGui::InGame::Update tests to enter
        // the licence/photo INTRO on a first boot. A raw memset leaves that byte 0, i.e.
        // "an old profile", which is NOT the console's fresh-profile state.
        reinterpret_cast<BrnProgression::Profile*>(s_pcProgressionProfileBacking)->Construct();
        mProfileManager.SetProgressionProfile(
            reinterpret_cast<BrnProgression::Profile*>(s_pcProgressionProfileBacking),
            reinterpret_cast<const BrnProgression::ProgressionData*>(s_pcProgressionManifestBacking));
        mProfileManager.SetLiveRevengeProfile(
            reinterpret_cast<BrnNetwork::LiveRevengeProfile*>(s_pcLiveRevengeProfileBacking));

        // X360 GuiModule::Prepare @0x82518D68 STAGE 3, line 104: `*(gm + 660992) = gm +
        // 1005332`, i.e. the analyzer adopts the shared GuiAccessPointers block (the same
        // block the three flows are prepared against, one line below). The console inlines
        // the store; the DWARF names the method (BrnGuiHudMessageAnalyzer.h:75) and it is
        // bodied as the de-inlined form in BrnGuiHudMessageAnalyzer.cpp. Without it
        // mpAccessPointers is never written and TriggerChallengeEndedMessage /
        // HandleStuntPerformed dereference it (mpAccessPointers->mpLanguageManager).
        mHudMessageAnalyzer.SetAccessPointers(&s_GuiAccessPointers);

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
            while (PopPendingAptLoadNotification(&lNotification))
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
        mbPrepared = true;
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

    bool GuiModule::Release()
    {
        mProfileManager.Release();   // detach the sign-in listener + release the SLS
        mScreenFlow.Release();       // staged: current state OnLeave + ScriptedFsm release
        mHudFlow.Release();
        mOverlayFlow.Release();
        mFsmLuaHeap.Destruct();
        if (gpActiveGuiModule == this)
            gpActiveGuiModule = 0;
        gpActiveMovieManager = 0;
        mMovieManager.Release();
        return true;
    }

    void GuiModule::Destruct()
    {
        mMovieManager.Destruct();
        // X360 GuiModule::Destruct @0x82507690: ColourCalibrationScreen::Destruct is the LAST
        // sub-object destruct, immediately before the base CgsGui::GuiModule::Destruct.
        mColourCalibrationScreen.Destruct();
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

                case 350:   // GuiEventProgressionProfileData -- the live-profile handoff
                case 169:   // GuiEventChangeDistrict -- the district-marker source words
                case 147:   // [H3b] GuiEventUpdateHud -- the player {speed,rpm,gear} words
                case 199:   // [H3b] GuiEventUpdateSatNav -- the icon array (player position arm)
                case 204:   // [H3b] the sat-nav event-filter pair (the ch40 mirror)
                case 207:   // [H3b] GuiRaceCarInfoEvent -- the mRaceCarInfo SoA feed
                case 376:   // [H3b] GuiPlayerRaceCarIdEvent -- the player index pair (case-199 gate)
                case 379:   // [reveal gate] GuiPlayerEngineEvent -- the ignition latch (+0x4B20)
                case 492:   // [E1] GuiEventCurrentStatus  -- distance driven + player-team table
                case 424:   // [E1] GuiEventScoreUpdate    -- THE EVENT TIMER (mfEventTime/mfTargetTime)
                case 428:   // [E1] GuiAttackScoreUpdate   -- THE STUNT SCORE (current/target/combo/multiplier)
                case 203:   // [event-starts] GuiEventUpdateEventStarts -- THE EVENT-START TABLE
                case 93:    // [A9] GuiEventPrepareForModeStart -- THE MODE-TYPE SEED (meGameModeType)
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

                default:
                    // The other module-level consumers (profile/skills/overlays/keyboard/
                    // language...) are subsystem follow-ons; their events pass through to
                    // the flow filter below.
                    break;
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
                    // Command 507 = "refresh the player name" (the {1, 507, 12} record
                    // BootProfile::OnLeave posts as the profile boot completes). On the
                    // console it round-trips through the game/network side and comes back
                    // as GUI in-event 507, whose Update arm (0x82527A58 case 507) calls
                    // UpdatePlayerName. [FLAG PC-platform seam] the network module that
                    // reflects it does not exist on PC, so the record is answered at the
                    // drain -- the same call at the same point in the frame, one hop
                    // earlier. DELETE-WHEN the network sign-in manager lands and posts
                    // in-event 507; then this becomes a plain forward and case 507 moves
                    // to DispatchInboundGuiEvents.
                    if (liSize >= 8 &&
                        reinterpret_cast<const u32*>(lpEvent)[1] == 507u)
                    {
                        UpdatePlayerName();
                        break;   // consumed, exactly as the console's Update arm consumes it
                    }

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
                    mGuiOutQueue.AddEvent(lpEvent, liId, liSize);
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
                    else if (lpPlay->muEventType == 25)   // GuiEventClearScreenSet {mode, alpha}
                    {
                        // The view's black-backdrop control: BootLegal enables it (alpha
                        // 1.0) under the title, BootLegal::OnLeave / BootProfile::OnEnter
                        // disable it so the save/load prompt composes over the
                        // loading-screen background (ViewModule case 25 @0x8285FCE8).
                        const CgsGui::GuiEventClearScreenSet* lpClear =
                            reinterpret_cast<const CgsGui::GuiEventClearScreenSet*>(lpEvent);
                        struct { s32 miMode; f32 mfAlpha; } lBody =
                            { lpClear->meClearScreen, lpClear->mfAlpha };
                        mViewInputBuffer.GetViewStateQueue()
                            .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lBody), 25,
                                static_cast<s32>(sizeof(lBody)));
                    }
                    // ⭐ [boost-bar + H3b reconcile 2026-08-25] the custom-renderer record
                    // family -- 213 (SatNav/MainMap show/hide, {s32 mode, f32 fade, u8
                    // enable}), 214/215 (the BoostBar/AboveCar enables), 204 (the sat-nav
                    // event-starts display command) and 212 (the per-frame RenderSatNav
                    // payload) -- bridged BODY-ONLY into the view-state queue, exactly as
                    // the case-18/25 bridges above strip theirs. ONE delivery path: the
                    // view module's custom-renderer manager forward
                    // (ViewModule::ProcessIncomingViewEvents cases 204/212/213/214/215 ->
                    // CustomRendererManager::RecvEvent), the console's loop-tail seat. The
                    // console routes the WHOLE channel onto the view queue; this build
                    // still bridges selectively, one record type at a time, as each
                    // consumer lands. The header size comes from the record's own head[2]
                    // ({payloadSize, type, headerSize}) rather than a hardcoded 12: the
                    // alignas(16) 212 record carries a 16-byte head, its siblings 12.
                    else if (lpPlay->muEventType == 204 ||
                             lpPlay->muEventType == 212 ||
                             lpPlay->muEventType == 213 ||
                             lpPlay->muEventType == 214 ||
                             lpPlay->muEventType == 215)
                    {
                        const u32 luPayloadOffset =
                            reinterpret_cast<const u32*>(lpEvent)[2];
                        if (luPayloadOffset >= 12u &&
                            static_cast<s32>(luPayloadOffset) <= liSize)
                        {
                            const u8* lpu8Body =
                                reinterpret_cast<const u8*>(lpEvent) + luPayloadOffset;
                            mViewInputBuffer.GetViewStateQueue()
                                .CgsModule::VariableEventQueue<65536, 16>::AddEvent(
                                    reinterpret_cast<const CgsModule::Event*>(lpu8Body),
                                    static_cast<s32>(lpPlay->muEventType),
                                    liSize - static_cast<s32>(luPayloadOffset));
                        }
                    }
                    // The remaining view-state records (311/415/556 and the rest) ride the
                    // AptCommunicator component path on PC. [FLAG: the raw channel-41
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
            // The per-frame "gameplay HUD data ready" trio the gameplay HUD components gate on
            // (BoostBarRenderer::Update @0x82451CA4 reads cache+0x4B54/0x4B56/0x4B58 as one
            // gate). The console publishes +0x4B54 from the frame's update set
            // (`(lUpdateSet & 8) != 0`, PS3 GuiModule::Update); the other two bytes' producers
            // are unrecovered. [FLAG PC stand-in] the update set is not threaded into this
            // PC-shaped Update and the two producers are unknown, so all three follow the
            // gameplay-HUD flag the cache already maintains -- the same "in gameplay" condition
            // the update-set bit models. DELETE-WHEN the update-set threading + the two
            // producers are recovered.
            mGuiCache.SetGameplayHudReady(mGuiCache.IsGameplayHudActive());
            GuiEventCache lCacheEvent;
            lCacheEvent.mpGuiCache = &mGuiCache;
            // Delivery to every subscriber -- the three flows AND the always-available
            // manager (its real 19-id table includes 64; it latches the GuiCache its
            // Prepare state machine waits on) -- rides the one subscription filter.
            RouteEventToFlow(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64,
                             static_cast<s32>(sizeof(lCacheEvent)));

            // ⭐ [licence-icon] the CUSTOM-RENDERER manager is the event's other console
            // consumer (its RecvEvent case 64 fans the GuiCache to every component --
            // NetworkPlayerImageRenderer latches mpGuiCache off it, the pointer its staged
            // Prepare's resource wait needs). This synthesized post never transits the
            // module-input queue, so the DispatchInboundGuiEvents feed cannot carry it.
            mCustomRendererManager.RecvEvent(
                reinterpret_cast<const CgsModule::Event*>(&lCacheEvent), 64);

            // ⭐ [licence-icon] THE PREPARE PUMP (PC seat of the console's staged-Prepare
            // re-entry -- see the mbCustomRenderersPrepared note in the header). Advances the
            // manager's staged prepare each frame until the PlayerImage component reaches
            // E_PREPARESTAGE_DONE (its GuiCache wait needs the cache bind above first, which
            // is why the pump sits after it). One-shot log on completion.
            if (!mbCustomRenderersPrepared)
            {
                mbCustomRenderersPrepared =
                    mCustomRendererManager.Prepare(mpGuiHeapAllocator, mpTextureAllocator);
                if (mbCustomRenderersPrepared)
                {
                    CgsDev::Log::WriteToLog("[custrend] manager prepare DONE (pump)\n");

                    // ⭐ [licence-icon] THE OFFLINE DEFAULT-IMAGE ARM (PC stand-in, FLAG).
                    // ASM-RECOVERED ANSWER to "what selects the default photo when none
                    // exists": the console has exactly ONE writer of the renderer's
                    // mbUseDefaultTexture -- GUI event 571, posted ONLY by
                    // TranslateNetworkEventsToGuiEvents @0x823E0900 case 73 (the network
                    // module's "no mugshot for this player" report; even an offline console
                    // boot runs that module). There is NO non-571 default path in the image.
                    // This build has no network module, so the report's one observable --
                    // one 571 into the manager once the component is prepared -- is
                    // synthesized here, at the same exactly-once seat as the prepare latch.
                    // A real transmitted image (event 258 with a texture) still clears the
                    // flag, exactly as on console. DELETE-WHEN the network module +
                    // TranslateNetworkEventsToGuiEvents land.
                    const u8 lacUseDefault[4] = { 0, 0, 0, 0 };   // CgsGui::GuiEvent<571>: payload unread
                    mCustomRendererManager.RecvEvent(
                        reinterpret_cast<const CgsModule::Event*>(lacUseDefault), 571);
                    CgsDev::Log::WriteToLog(
                        "[licence-icon] default player image armed (571 stand-in)\n");
                }
            }
        }

        // ---- 2b. boot-resources-ready feedback (event 567; bring-up FLAG) -------------
        // The console GUI cache posts 567 when the title's expected apt components have
        // initialised, which arms BootLegal's press-start path. The cache watcher isn't
        // reconstructed; post it once when the apt movie is live.
        if (!mbResourcesReadyFed && AptFlowMovieLive())
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

        // ---- 3c. the colour-calibration screen (X360 GuiModule::Update @0x82529B04) -----
        // The console ticks it here: after the profile/overlay pumps and BEFORE the base
        // module update ticks the flows, with the module's GUI OUT buffer held write-locked
        // across the whole update. Its four arguments are recovered in the header note; on PC
        // the GameData IO pair is the scripted-load pair (the one place it is live and
        // pumped, the same source BrnGameModule::ResourceUpdateThread reads) and the GUI OUT
        // buffer's mOutEvents stand-in is mGuiOutQueue.
        // FLAG PC bring-up (argument sourcing only -- the screen body is the real
        // reconstruction): DELETE-WHEN a module scheduler hands GuiModule::Update its IO set.
        // [FLAG PC bring-up TEST HOOK -- OFF BY DEFAULT] BRN_POSTFX_CALIB_SCREEN_TEST=<n>: 300
        // updates before the n-th GuiModule::Update ensure the CN_COLOUR state's two APT resources
        // are loaded (the same GuiCache::EnsureResourcesAreLoaded call BrnBaseFlow::UpdateStreaming
        // makes on state entry, over the same {143 APT, 34 APT} list CrashNavColourCalibrate::
        // maResourcesToLoad carries -- 143 = BRNCRASHNAVCOLOURCALIBRATE, the bundle that holds the
        // calibration card raster the screen acquires from pool 9), on the n-th feed the screen a
        // 514 (show), and 600 updates later a 515 (hide). Stands in for the flow entering CN_COLOUR
        // + CrashNavColourCalibrate::SetupComponents / OnLeave, all reconstructed but UNREACHABLE on
        // this build (the FSM's only inbound edge to CN_COLOUR is "TO_COLOUR", posted by
        // CrashNavSettings, which has no TU). Exercises the whole chain: APT load -> screen acquire
        // -> 546 -> BridgeGuiToGame -> dispatch buffer -> the composite's calibration ramp override.
        // Without the resource load the acquire returns the null handle and the screen's own
        // "!= NULLResourceHandle" assert (cpp:147) fires -- measured. DELETE-WHEN CrashNavSettings lands.
        {
            static s32  siTestShowAt = -2;   // -2 = not read yet, -1 = knob absent
            static u32  suUpdates    = 0u;
            if (siTestShowAt == -2)
            {
                const char* lpcEnv = std::getenv("BRN_POSTFX_CALIB_SCREEN_TEST");
                siTestShowAt = (lpcEnv != 0 && lpcEnv[0] != '\0') ? std::atoi(lpcEnv) : -1;
                if (siTestShowAt >= 0)
                    CgsDev::Log::WriteToLog("[calib-screen] TEST HOOK armed: load APT at N-300, show at update N, hide at N+600\n");
            }
            ++suUpdates;
            if (siTestShowAt >= 300)
            {
                CgsModule::Event lEvent;
                if (suUpdates == static_cast<u32>(siTestShowAt) - 300u)
                {
                    static const CgsGui::sResourceTuple kaCalibrateResources[] =
                        { { 143u, CgsGui::E_GUI_RESOURCETYPE_APT }, { 34u, CgsGui::E_GUI_RESOURCETYPE_APT } };
                    mGuiCache.EnsureResourcesAreLoaded(kaCalibrateResources, 2u);
                    CgsDev::Log::WriteToLog("[calib-screen] TEST HOOK: CN_COLOUR APT resources {143,34} requested\n");
                }
                else if (suUpdates == static_cast<u32>(siTestShowAt))
                    mColourCalibrationScreen.RecvEvent(&lEvent, 514);
                else if (suUpdates == static_cast<u32>(siTestShowAt) + 600u)
                    mColourCalibrationScreen.RecvEvent(&lEvent, 515);
            }
        }
        {
            BrnResource::GameDataIO::InputBuffer* lpGameDataInput =
                BrnGameMainFlowController::GetScriptedLoadGameDataInput();
            BrnResource::GameDataIO::OutputBuffer* lpGameDataOutput =
                BrnGameMainFlowController::GetScriptedLoadGameDataOutput();
            if (lpGameDataInput != 0)
            {
                // The screen's PREPARE_TO_SHOW arm pushes an AcquireResourceRequest onto the
                // request interface, which asserts the buffer is locked for WRITING -- the
                // same bracket PrepareWorldData is driven under.
                lpGameDataInput->LockForWrite();
                mColourCalibrationScreen.Update(lpGameDataInput, lpGameDataOutput,
                                                &mGuiOutQueue, mpTextureAllocator);
                lpGameDataInput->UnlockForWrite();
            }
        }

        // ---- 3d. THE HUD-MESSAGE PUMP (gateui wave, round 2) --------------------------
        // X360 GuiModule::Update @0x82527A58, reproduced in the console's own order:
        //
        //   910  v153 = gm + 1005376;
        //   911  VariableEventQueue<32768,16>::AddEvent(v156, &v153, 64, 4);
        //          -- v156 == sub_8284F238(a5) == CgsGuiModuleIO::InputBuffer::GetGuiEvents(),
        //             i.e. the console PUSHES the GuiCache pointer (event 64) back into the
        //             very queue the analyzer is about to drain. That record is how
        //             HudMessageAnalyzer::mpGuiCache gets set: Construct @0x82509060 leaves
        //             it NULL and Update's case 64 latches it (_wB_12). Without this push
        //             the analyzer's first `mpGuiCache->...` is a null deref.
        //   912-929 the mpHudMessageController assert (BrnGuiCache.h:2418) + SetController.
        //   1015 LockForWrite(a8)                        -- the VIEW input buffer
        //   1022 v114 = ViewIO::InputBuffer::GetViewStateQueue(a8)
        //   1024 HudMessageAnalyzer::Update(gm + 660992, v115, v114)
        //   1031 UnlockForWrite(a8)
        //   1035 HudMessageDirector::Update(gm + 639264, v150)   -- v150 == the ModelIO
        //          input buffer, the same one GuiCache::Update is handed at line 1048.
        //
        // The console runs the analyzer between ColourCalibrationScreen::Update (1007) and
        // GuiOverlaysDirector::Update (1032); this build has no overlays director, so the
        // block sits immediately after the calibration screen and before the flow ticks --
        // the same slot.
        if (mpGuiEventInputBuffer != 0)
        {
            mpGuiEventInputBuffer->LockForWrite();
            {
                CgsGui::CgsGuiModuleIO::InputBuffer::GuiEventInputQueue* lpGuiEvents =
                    mpGuiEventInputBuffer->GetGuiEvents();

                // console line 910-911 (the X360 size literal is 4 -- its pointer width;
                // the host record is 8, so the push rides sizeof, as every other event
                // producer in this TU does).
                GuiEventCache lCacheEvent;
                lCacheEvent.mpGuiCache = &mGuiCache;
                lpGuiEvents->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCacheEvent),
                                      64, static_cast<s32>(sizeof(lCacheEvent)));

                // console lines 911-928, and the SHAPE below is the CONSOLE'S OWN, not a
                // PC gate: it fires a NON-GATING `mpHudMessageController` assert
                // (BrnGuiCache.h:2418) and then wraps the SetController call in
                // `if ( *(a1 + 1021872) )` -- i.e. it too declines to publish a null
                // controller.
                //
                // ⭐ [gateui r4] THE ROUND-3 PARK IS RETIRED. Its DELETE-WHEN ("a
                // HudMessageController producer lands") is met twice over:
                // GameDataModule::PrepareHudMessages fills mHudMessageController, and
                // GuiModule::Construct now receives it as the console's `a2` and stores it
                // through GuiCache::SetHudMessageController. So this gate is expected to be
                // TRUE from the first frame and the assert below is a tripwire, not a
                // standing gap report.
                //
                // The assert fires ONCE rather than per frame: a per-pump assert storm is
                // what the 440-assert perf incident was, and the console reaches this line
                // once per frame too.
                if (!mGuiCache.HasHudMessageController())
                {
                    static bool sbNoControllerAssertFired = false;
                    if (!sbNoControllerAssertFired)
                    {
                        sbNoControllerAssertFired = true;
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert(
                            "mpHudMessageController",
                            "..\\..\\..\\GameSource\\Gui/BrnGuiCache.h", 2418);
                        CgsDev::Assert::EndAssert();
                    }
                }
                else
                {
                    mHudMessageDirector.SetController(mGuiCache.GetHudMessageController());
                }

                // console lines 1015-1031: the analyzer runs with the VIEW input buffer
                // write-locked across it (it appends view-state records through
                // mpViewOutputQueue).
                mViewInputBuffer.LockForWrite();
                mHudMessageAnalyzer.Update(lpGuiEvents, &mViewInputBuffer.GetViewStateQueue());
                mViewInputBuffer.UnlockForWrite();
            }
            mpGuiEventInputBuffer->UnlockForWrite();
        }

        // console line 1035. The director drains its own published messages (event 154)
        // into the model module's input GUI-event queue.
        mHudMessageDirector.Update(&mModelInputBuffer);

        // FLAG PC bring-up (dispatch seam, NOT a behaviour change): on the console the
        // records the director just appended are consumed by the embedded
        // CgsGui::ModelModule's own update -- the EventInterpreterModule fans them out to
        // every registered observer, which is how event 154 reaches
        // BrnFBurnMainHudState::RecvEvent. This build models that fan-out with
        // RouteEventToFlow (see DispatchInboundGuiEvents), so the same walk is run here
        // over the model input buffer's queue. It is ALSO what keeps that queue bounded:
        // nothing else drains or clears it, and a GuiHudMessage is ~840 bytes against a
        // 32768-byte queue -- roughly 39 messages to an overflow assert.
        // DELETE-WHEN CgsGui::ModelModule is reconstructed and owns this dispatch.
        {
            mModelInputBuffer.LockForRead();
            const CgsGui::ModelIO::InputBuffer::GuiEventInputQueue* lpModelEvents =
                static_cast<const CgsGui::ModelIO::InputBuffer&>(mModelInputBuffer).GetEventQueue();
            const CgsModule::Event* lpEvent = 0;
            s32 liSize = 0;
            s32 liId = lpModelEvents->GetFirstEvent(&lpEvent, &liSize);
            while (liId >= 0 && lpEvent != 0)
            {
                RouteEventToFlow(lpEvent, liId, liSize);
                const CgsModule::Event* lpNext = 0;
                liId = lpModelEvents->GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            mModelInputBuffer.UnlockForRead();

            mModelInputBuffer.LockForWrite();
            mModelInputBuffer.GetEventQueueNonConst()->Clear();
            mModelInputBuffer.UnlockForWrite();
        }

        // ⭐ [stuntrace] the freeburn-challenge tracker's per-frame tick. X360
        // GuiModule::Update @0x82527A58 runs it in exactly this seat -- the two manager ticks
        // immediately before the base module update that drives the flows:
        //     BrnGui::BurnoutSkillsManager::Update(gm + 309032);
        //     BrnGui::FreeburnChallengeManager::Update(gm + 309584);
        //     CgsGui::GuiModule::Update(gm, ...);      // == the flow ticks below
        // The body is the AUTO_ROTATE page timer and is entirely behind
        // `meInternalState != E_INTERNAL_STATE_OFF`, so on an offline event it costs one
        // compare per frame. (The skills-manager twin above it is not landed -- see the gap
        // note at the Construct hand-off.)
        //
        // The manager's OTHER console drives are its GuiModule::Update EVENT arms, which are
        // NOT landed: 544 -> SelectNext, 574 -> StartChallenge, 576 -> TriggerChallenge,
        // 577 -> HandleNewData, 578 -> meInternalState = RESULTS, 579 -> FinishChallenge,
        // 581 -> memcpy(manager + 184 /*mCompletedData*/, event, 2104), 583 ->
        // StartNotActiveChallenge, 584 -> meInternalState = OFF. Every one of them is a
        // FREEBURN-challenge record; none is produced on this build and none can reach an
        // offline event, so the manager correctly rests in E_INTERNAL_STATE_OFF for the whole
        // of a stunt race and every consumer's IsActive/IsNotActive/IsRunning/
        // IsShowingResults gate reads false. Landing them is the follow-up that turns the
        // challenge ticker on, and it needs the freeburn producers first.
        mFreeburnChallengeManager.Update();

        // ---- 4. the flow ticks (each current state's PreUpdate/Update/PostUpdate) -----
        mScreenFlow.Update();
        mHudFlow.Update();
        mOverlayFlow.Update();
        // ...then RESET each observer's per-frame queue, exactly as the always-available
        // manager's queue is reset above. The console's EventInterpreterModule fills an
        // observer's in-queue from the subscription filter, runs UpdateObservers, and the
        // queue starts the next frame empty -- an observer sees an event once.
        //
        // These three were never cleared, so every routed event stayed in the queue and was
        // re-delivered on every subsequent frame, growing without bound until the 18 KB
        // VariableEventQueue overflowed (a boot reached "Event Type 64 has 223 entries" and
        // then asserted "Queue overflow. Write Pos=18431"). Past the overflow the walk reads
        // whatever is at the tail, which is how BrnGui::Intro started tripping its
        // unhandled-event assert on ids 0 and 256 -- ids nothing ever posts. It stayed
        // invisible while only a handful of events were routed per frame; the INTRO path
        // routes a profile record every sub-step and hit the wall in seconds.
        mScreenInQueue.Clear();
        mHudInQueue.Clear();
        mOverlayInQueue.Clear();

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
            while (PopPendingAptLoadNotification(&lNotification))
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
        // (MovieManager::Update advances from the render pass now -- the console's
        // UpdateAndRenderMovieManager @0x82511240 -- so the finished check below reads
        // the flag that pass raised.)
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

            // ⭐ [boost-bar gate 2026-08-25] THE PER-FRAME COMPONENT PUMP. The console
            // drives CustomRendererManager::Update @0x82450908 once per frame from the
            // view chain (the call is virtual, so the exact console seat is not name-
            // recoverable; this PC seat follows the manager's Prepare-pump precedent
            // above). It must run AFTER the view dispatch just above (so this frame's
            // RecvEvent payloads -- the 206 boost info, 212 sat-nav render, 213/214
            // show-hides -- are already latched) and BEFORE GuiModule::Render (so
            // BoostBarRenderer::Update's visibility machine processes the allowed-to-
            // boost edge before RenderComponent's state-consistency asserts read it).
            // Without this call NO custom-render component ever ticked: the boost bar's
            // interpolators never re-keyed toward the live boost amount and its fade
            // machine froze at the Construct-seeded FULL -- the user-reported "asserts
            // when there is no boost" ("Visibility is full but we are not allowed to
            // boost", :1080) and "bar doesn't update with the correct values".
            mCustomRendererManager.Update();

            // [PC diagnostic] log the level-1 flow-movie state on CHANGE only (live /
            // composed flips) -- the mount/unmount/remount observability line.
            {
                static s32 s_iPrevLevel1State = -1;
                const s32 liLevel1State =
                    (AptFlowMovieLive() ? 1 : 0) | (AptFlowMovieComposed() ? 2 : 0);
                if (liLevel1State != s_iPrevLevel1State)
                {
                    char lacState[96];
                    std::snprintf(lacState, sizeof(lacState),
                                  "[AptRT] level-1 state -> live=%d composed=%d\n",
                                  liLevel1State & 1, (liLevel1State >> 1) & 1);
                    CgsDev::Log::WriteToLog(lacState);
                    s_iPrevLevel1State = liLevel1State;
                }

                // [hud reveal gate 2026-08-25] DELETED: the "world-load stand-in" that fed a
                // fabricated engine-on 379 the frame the in-game HUD movie composed. It had
                // no console counterpart at all -- GuiModule::Update @0x828602C8 posts no GUI
                // event of its own -- and it was the direct cause of the user-reported "the
                // HUD is always visible, even in the Junkyard": it fired the ignition latch
                // ~3800 log lines before the player's engine actually started, so the HUD's
                // master "EventHud_Animator" ran 'transin' during car select.
                // The REAL producer is the console's own edge latch inside
                // BrnGameModule::BridgeWorldVehicleDataToGui @0x823E5930..0x823E59AC, landed
                // this wave in GameBridgeWorldToGui.cpp -- it reads the world's published
                // EActiveRaceCarEngineState and posts 379 only on a genuine off/on flip.
                // ⛔ DO NOT RE-ADD a stand-in here: with the world side live, a second
                // producer would double-post and re-open exactly this bug.
            }

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
    // UpdateAndRenderMovieManager (fullscreen movies present INSIDE this pass, over the
    // view content) + the effects arbitrator (data-gated). lpIm2dRenderBuffer is the
    // movie player's presentation surface (the console reaches it through the input
    // buffer's renderer set).
    // FLAG PC-ABI adapter: gates on the Apt bring-up (the console's prepared byte).
    void GuiModule::Render(CgsGraphics::Im2dRenderBuffer* lpIm2dRenderBuffer)
    {
        if (!IsRuntimeReady())
        {
            // The movie manager still presents while the apt runtime is warming up (the
            // EA/Criterion logos play before the GUI view composes anything).
            UpdateAndRenderMovieManager(lpIm2dRenderBuffer);
            return;
        }

        CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
            s_bRenderBufferReady ? &s_AptRenderBuffer : nullptr;
        if (lpAptBuffer == nullptr)
        {
            UpdateAndRenderMovieManager(lpIm2dRenderBuffer);
            return;
        }

        // CgsGui::GuiModule::Render @0x8285AF38 core: publish the active renderer set
        // into the view input buffer. Slot 0 is the Apt Im2d command buffer the engine's
        // render callbacks fill; the MenusAndHud 3D slot carries the host's non-null
        // stand-in (AptRenderHandler::Render asserts it; the 2D-only boot path never
        // dereferences it). The camera is FLAG-deferred with the ViewModule camera member.
        CgsGui::ViewIO::ImRendererSet lRendererSet = {};
        lRendererSet.mpIm2dRenderBuffer            = lpAptBuffer;
        lRendererSet.mpIm3dRenderBufferMenusAndHud = &s_i3dRendererSentinel;

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
        DispatchAptRenderResidue();

        // Fullscreen movies present over the view content, inside this pass -- the X360
        // Render's UpdateAndRenderMovieManager call (@0x825146B8). The states manage the
        // loading screen around videos through the real protocol (BootLoading::OnLeave /
        // PostTitleScreenLoad post StopAptLoadingMovie before playing and re-raise it
        // after), so nothing else needs to hide for the video's duration.
        UpdateAndRenderMovieManager(lpIm2dRenderBuffer);
    }

    // @ 0x82511240 -- pump the movie manager (the movie pass of the GUI render).
    // FLAG PC-platform presentation split: the console body also calls
    // MoviePlayer::Render here (through the view input buffer's renderer set, under the
    // read lock) -- and the X360 XMV presentation then owns the screen ABOVE the whole
    // 2D frame: the boot logos play over the still-latched loading screen (BootVideos
    // @0x82478778 posts no hide; nothing does until BootLegal::OnEnter's 20). The PC
    // FFmpeg substitute draws immediate D3D9 quads, so its call position IS its pixel
    // order -- to reproduce the console's "video above everything" layering, the
    // presentation draw runs at the renderer's frame tail (BrnRendererModule::Render,
    // after the loading-screen foreground), not here.
    void GuiModule::UpdateAndRenderMovieManager(CgsGraphics::Im2dRenderBuffer* /*lpIm2dRenderBuffer*/)
    {
        mMovieManager.Update();
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

    // ---- DrainAptRenderResidueBeforeFlapt (free hook, declared in BrnGuiViewModule.h) ----
    // ⭐⭐ THE 2D PIXEL-ORDER FIX. Console: EVERY 2D submitter -- FLAPT included -- records
    // into ONE CgsGraphics::Im2dRenderBuffer, and Im2dRenderBuffer::Dispatch @0x827F9BA0
    // replays them in RECORD order, so FLAPT's records (which sit at the tail) paint OVER
    // the sat-nav map. PC has TWO backends: FlaptRenderSet::mpIm2dRenderBuffer is typed
    // CgsGraphics::Im2d*, so FLAPT binds Im2dBase<V>::BatchTransformTextureBlendRenderStatic
    // (CgsIm2d.cpp:484), which reaches D3D9 IMMEDIATELY, while every other 2D submitter
    // records a command. The single flush -- DispatchAptRenderResidue() -- runs AFTER
    // mViewModule.Render() returns. Net effect: CALL order is map->arrow but PIXEL order is
    // arrow->map, every frame, and the opaque map erases the HUD drawn under it.
    //
    // ⛔ The "more faithful" repair -- routing FLAPT through the command buffer -- is NOT
    // this change. The two PC backends disagree on POSITION SPACE (Im2dBase folds
    // transform->NDC->logical; Dispatch's RENDER_PRIMITIVES treats the transform output as
    // logical already, and the PC producers were adapted to Dispatch on purpose) and on
    // COLOUR-SCALE IDENTITY (FoldIm2dColourChannel identity 1.0 vs DispatchColourScaleOnly
    // identity 255) while SHARING opcodes 16/2, so Dispatch cannot tell an Apt record from a
    // FLAPT one without a producer-side marker we would have to invent. Recording FLAPT
    // today would put the HUD in a 2x2-pixel blob at the origin at 1/255 brightness.
    // Unifying the conventions is a whole-GUI campaign, not this fix.
    //
    // So: MOVE THE FLUSH, NOT THE SUBMITTER. Draining here -- between the base view render
    // and mFlaptManager.Render() -- reproduces the console's RECORDED order as the PC's
    // PIXEL order. It touches no convention, adds no opcode and adds no buffer.
    // ⭐ Any reorder that keeps BOTH submissions inside the recording phase is a pixel
    //   NO-OP; that is why swapping the statements at BrnGuiViewModule.cpp:167-169 cannot
    //   work, and why the flush -- which is in a DIFFERENT TU -- is the thing that moves.
    //
    // GuiModule::Render's own DispatchAptRenderResidue() call is deliberately LEFT IN PLACE.
    // It is a proven no-op once this drain has run: Dispatch bounds its walk by the dispatch
    // buffer's muCommandBufferWritePos (GetFirstCommand/GetNextCommand), and this drain's
    // Swap->Clear zeroed that buffer's write position, so the second flush walks an EMPTY
    // buffer. Keeping it means the residue path still covers anything recorded after the
    // view render, and it keeps the OFF path byte-identical to today.
    void DrainAptRenderResidueBeforeFlapt()
    {
        if (!FlaptAfterDispatchEnabled())
            return;
        DispatchAptRenderResidue();
    }
}
