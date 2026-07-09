#include "GameSource/Gui/BrnGuiAptRuntime.h"

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

#ifndef BRN_GUI_APT_RUNTIME_DIAGNOSTICS
#define BRN_GUI_APT_RUNTIME_DIAGNOSTICS 0
#endif

namespace
{
    constexpr bool kAptRuntimeDiagnostics = BRN_GUI_APT_RUNTIME_DIAGNOSTICS != 0;
}

#if BRN_GUI_APT_RUNTIME_DIAGNOSTICS
// (2026-07-09) The engine-side bring-up probe hooks (the Apt*Probe / CgsApt_*Probe /
// AptOpTrace* weak-no-op + /alternatename call sites in the SDKs/EATech Apt TUs) were
// removed once the Apt engine reconstruction stabilised, so the strong log sinks that
// lived in this block no longer had any callers and were deleted with them. The gate
// (default 0) is kept so future opt-in diagnostics have a home.
#endif

// =============================================================================
// THE APT RUNTIME BRING-UP / PER-FRAME DRIVER.
//
// This is the host facade that the X360 AptInit / AptUpdateInitialize / AptRender-
// Initialize / AptCreateTargetInstance chain (all UN-RECONSTRUCTED -- they exist
// only as comments in the tree) would normally provide. It stands up the engine
// pieces that DO exist and drives them each frame, logging [AptRT] probes and
// bailing cleanly the instant it crosses an un-homed piece, so the run-log reveals
// exactly how far BootLegal's Title_Screen02 render path gets.
//
// The init values (allocator sizes, interpreter stack sizes, the AptUpdate/Create
// param blocks) are transcribed from the REAL X360 CgsGui::AptAux::InitializeApt
// @0x82848E50 + AptAllocatorInitialize @0x82ADD118 (see the matching .ida-exports
// JSONs) so they are faithful, not invented.
// =============================================================================

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
extern void*              gpAptValueGCPool;        // off_8324D834 (type-erased GC-pool view)

// The interpreter VM singleton (X360 &dword_8324E760) -- defined in AptGlobals.cpp.
extern AptActionInterpreter gAptActionInterpreter;

// The faithful per-frame dynamic-text refresh pass (AptCIHBehaviour.cpp): install
// AptCIH::ProcessTextInst as the generalised-process callback + walk the root subtree,
// so every dynamic-text node re-resolves its bound text + (re)lays it out through
// EnsureStringAllocated. The console runs this inside AptUpdate (sub_82B0D608).
struct AptCIH;
void AptCIH_RunGeneralisedTextProcess(AptCIH* pRoot);

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
    // AptAux::Prepare drive (step 5).
    unsigned char                s_aAptDataHeap[256 * 1024];
    CgsMemory::HeapMalloc        s_AptDataAllocator;
    bool s_bTextSystemReady = false;                     // fonts + strings loaded (one-shot)
    CgsResource::Font* s_pAptBodyFont = nullptr;         // the FIRST loaded typeface (B5EAConDisS --
                                                         // the menu-label font; the AS-autofit measure)
    bool s_bHelpDefaultsPending = false;                 // retry the help-item defaults until the
                                                         // icon subtree is composed (see the FLAG)

    // The language allocator's backing heap: ~4.7K hash elements (24B each) + heap overhead.
    const u32 KU_LANGUAGE_HEAP_BYTES = 512u * 1024u;
    u8 s_aLanguageHeap[KU_LANGUAGE_HEAP_BYTES];

    // The 3 typeface bundles the collection holds (KI_MAX_FONTS == 3 slots). The title
    // movie's type-3 font chars name the "B5EAConDisS(Drop)" family == WESTERNB5BODY_35;
    // HEADER_70 ("MachineStd-Bold") and DOTMAT_35 ("B5DotMat") are the other two western
    // typefaces the GUI movies reference (and the FindFont fallback-table entries).
    const char* const KA_APT_FONT_BUNDLES[3] =
    {
        "Language/Fonts/WesternB5Body_35.font",
        "Language/Fonts/WesternB5Header_70.font",
        "Language/Fonts/WesternB5DotMat_35.font",
    };
    CgsResource::Pool s_aAptFontPools[3];   // one resident pool per typeface bundle

    // The language string-table bundle. FLAG: langid selection is host-static (0002 ==
    // langid 8, the clean English table -- verified offline: TITLES_PRESS_START ->
    // "Press START"); the console picks the bundle from the SKU/dash language.
    const char* const KC_APT_LANGUAGE_BUNDLE = "LANGUAGE/0002.bundle";

    // ---- the PER-MOVIE SLOTS (the framework movie + the channel-41 flow movie) --
    // TWO movies coexist on the director's root display list, exactly as the console
    // composes them: the AS FRAMEWORK movie ("MAIN" -- the BurnoutComponent /
    // gAptCommunicator class carrier) at display level 0, and the FLOW movie
    // (BootLegal's Title_Screen02) at display level 1 above it. Each slot owns its
    // own load state, resident pool, resource span, root AptCIH and paced-tick clock;
    // the load / instantiate / tick paths below all take a slot by reference, so both
    // movies travel the SAME faithful path (bundle load -> AddAptData -> AptLoader::
    // Load -> LoadAnimation -> AptGetAnimationAtLevel(slot.miLevel) -> instantiate).
    //
    // Each movie bundle (GUIAPT\<NAME>.bundle) is loaded SYNCHRONOUSLY into the slot's
    // pool via BundleLoader::LoadBundle (the same async-FS/DeviceManager path the FSM +
    // VIDEOLIST bundles use). Its single AptData resource (type 0x1E == 30) is the
    // relocated CgsGui::AptDataHeader; the header's pointer fields are RAW file-relative
    // offsets on x64 (see the FixUp note at the load site), so the slot keeps the FULL
    // 64-bit resource base + size and every offset resolves as (T*)(base64 + offset).
    struct AptMovieSlot
    {
        const char* mpcTag;                  // "framework" / "flow" (the [AptRT] log prefix)
        char        macName[64];             // the loaded movie name (load-once key)
        bool        mbRequested;             // a load request is in flight (load attempted)
        bool        mbLoaded;                // bundle loaded + the AptDataHeader resolved
        bool        mbInstantiated;          // root CIH bound to a live anim inst (ticking)
        bool        mbLoadAttempted;         // the one-shot faithful load path ran (any outcome)
        s32         miLevel;                 // display level (root display-list depth)
        CgsResource::Pool*     mpPool;       // the slot's resident movie pool
        CgsGui::AptDataHeader* mpHeader;     // the movie header (fields are RAW offsets on x64)
        uintptr_t   muResourceBase;          // the AptData resource 64-bit load base
        u32         muResourceSize;          // the AptData resource size (relocation bound)
        AptCharacterAnimation* mpCharAnim;   // the movie-root def base (post-Fixup)
        AptFile*    mpAptFile;               // the loaded AptFile handle (mpData = root)
        void*       mpRootCIH;               // the level-miLevel root AptCIH
        u32         muMsPerFrame;            // RETIRED (engine paces from AptMovieData); kept for layout stability
        double      mdTickAccumMs;           // the charInst+36 accumulator (host-held)
        s64         miLastUpdateQpcMs;       // wall-clock of the previous slot tick pass
        s32         miTickFrame;             // per-slot tick counter (the one-shot probes)
    };

    AptMovieSlot s_FrameworkSlot =
        { "framework", { 0 }, false, false, false, false, /*miLevel*/ 0,
          nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, 33u, 0.0, -1, 0 };
    AptMovieSlot s_FlowSlot =
        { "flow",      { 0 }, false, false, false, false, /*miLevel*/ 1,
          nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, 33u, 0.0, -1, 0 };
    // The PERSISTENT component library (PERSISTENTAPT.bundle -- the BurnoutComponent base +
    // the menu component classes: SelectionMenu / *AnimatorComponent). The console keeps it
    // resident alongside MAIN (§6.4). Placed at display level 2 (above the flow) so its own
    // timeline ticks (its embedded component movies' init/class registration run); MAIN stays
    // the level-0 AS core (new AptCommunicator + the 24-class registerClass bootstrap).
    AptMovieSlot s_PersistentSlot =
        { "persist",   { 0 }, false, false, false, false, /*miLevel*/ 2,
          nullptr, nullptr, 0, 0, nullptr, nullptr, nullptr, 33u, 0.0, -1, 0 };

    // The flow left BF_LEGAL (accept path) -- parks the FLOW slot only (tick + render);
    // the FRAMEWORK movie keeps ticking (the console keeps the persistent level-0 apt
    // composed across flow-state changes).
    bool        s_bMovieStopped          = false;
    s32         s_iFrameCounter          = 0;       // per-frame probe throttle
    s32         s_iRenderFrame           = 0;       // render-walk per-frame trace counter
    bool        s_bFlushProbed           = false;   // emitted the one-shot render-flush probe yet

    // The per-slot pool backings (3 mem types each), static BSS storage; each slot gets
    // its OWN CgsResource::Pool storage object. The FLOW movie (the title) is ~1.6 MB ->
    // keep the 8 MB/type backing; the FRAMEWORK movie (MAIN.bundle) is 236 KB -> 4 MB/type
    // is generous (the bundle's main-memory resources + the heap node overhead).
    const u32 KU_FLOW_POOL_BYTES      = 8u * 1024u * 1024u;
    // PERSISTENTAPT (the persistent component library) is an 11.5 MB, 101-resource package:
    // ~3.5 MiB main + ~8.13 MiB graphics per the console Gui_Persistent_Apt_Pool
    // ({0x369AF0, 0x820000, 0} in BrnMemoryMapData.h). 12 MiB/type covers its largest section.
    const u32 KU_FRAMEWORK_POOL_BYTES = 12u * 1024u * 1024u;
    u8 s_aFlowPoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_FLOW_POOL_BYTES];
    u8 s_aFrameworkPoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_FRAMEWORK_POOL_BYTES];
    // PERSISTENTAPT is the 11.5 MB library -> its own 12 MiB/type pool (as the framework's).
    u8 s_aPersistentPoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_FRAMEWORK_POOL_BYTES];
    CgsResource::Pool s_FlowPoolStorage;
    CgsResource::Pool s_FrameworkPoolStorage;
    CgsResource::Pool s_PersistentPoolStorage;

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).
    const u32 KU_APTDATA_RESOURCE_TYPE_ID = 30u;

    // ---- a static-backed rw resource allocator for the Apt render buffer -------------------
    // FLAG (PC bring-up): the RenderWare DEFAULT resource allocator's DoAllocate(256KB) returns
    // null at Apt bring-up time (its heap has no room for the render buffer's 4x(256KB+128)
    // streams -- verified at runtime: "DoAllocate(256KB)=0"), so the ImRenderBuffer's Prepare
    // carve fails and BeginRendering AVs on the null command buffer. We back the Apt render
    // buffer with a dedicated static bump pool instead (a small IResourceAllocator over BSS
    // storage). This does NOT touch the ImRenderBuffer/Im2d/D3D9 leaf -- it only supplies the
    // buffer's backing memory (the host owns the render-buffer's storage, exactly as the console
    // does through its own render-heap). 2 MB covers the 4 streams with headroom.
    const u32 KU_APT_RB_POOL_BYTES = 2u * 1024u * 1024u;
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
    // from the header's size slot. The old signature scan (LocateMovieRoot8), the
    // gAptLoadAnimRootOverride / span pokes, and the direct AddAptData host calls are
    // RETIRED (2026-07-09, the step-5 load-ownership move).
    //
    // DEFENSIVE: the whole path is heavily guarded; ANY failure leaves the slot's
    // mbInstantiated false and the game up.
    // ====================================================================================
        const bool KB_FAITHFUL_PATH_ENABLED = true;   // master gate for the faithful load path.

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

    // ====================================================================================
    // STEP 3 -- IMPORT CONTENT-LOAD (the cross-bundle imports the title's visible content lives in).
    //
    // TITLE_SCREEN02's 7 placed chars are type-5 SPRITE CONTAINERS whose content is 100% in 5
    // IMPORT bundles (B5MenuItem / B5HelperComponents / B5ControllerButtons / B5HelpItem). The
    // parent movie's Fixup pass-3 REGISTERS each import with the loader (AptLoader::Load -> a
    // "requested" AptFile, mnState==1) but does NOT stream its data -- on the console that is the
    // async .apt stream kicked off by AptLoader::Update state 1->2 (AptLoader_StartAsyncLoad), whose
    // completion (AptCompleteAnimationAsyncLoad -> CompleteLoad -> Resolve -> Fixup) fills the import
    // in. PC has no async stream, so AptLoader_StartAsyncLoad is HOMED below to load the import bundle
    // SYNCHRONOUSLY + drive that same completion. AptLoader::Update (the faithful console state
    // machine) then drives the whole recursive import graph bottom-up: leaf imports 3->4 (Link),
    // then the parent 3->4 -> AptCharacterAnimation_Link -> AptFile::FindExport -> the referenced
    // export char lands in the parent's charTable[importId]. Everything except StartAsyncLoad's
    // platform I/O is the faithful console flow.
    //
    // Each import bundle is itself a "1:7:8" apt movie with the SAME converter bugs as the title
    // (char[1] mis-align + mnDataRootOffset); they are byte-patched offline by
    // tools/assets/bundles/fix_apt_bundle.py (validated to reproduce fix_title_screen02.py). So the
    // synchronous load reads a corrected bundle -- the movie root locates + the char table is clean.
    //
    // The loaded import bundles must stay RESIDENT (the parent references their chars every frame),
    // so each gets a persistent pool + AptData span, kept in this small name-keyed registry (dedup:
    // an import referenced by >1 movie loads once). Static BSS storage (no runtime heap growth).
    // ====================================================================================
    const u32 KU_MAX_IMPORT_BUNDLES  = 16u;                       // PERSISTENTAPT's master movie has 13 imports
                                                                 // (was 8 for the title graph's ~6-8 distinct)
    const u32 KU_IMPORT_POOL_BYTES   = 2u * 1024u * 1024u;        // per-import pool (largest import ~282KB)

    struct ImportBundleSlot
    {
        char              macName[64];       // the movie name (e.g. "B5HelperComponents"), lower-cmp key
        CgsResource::Pool Pool;              // the resident pool holding the import bundle
        uintptr_t         luBase;            // the AptData resource base (the native-8 load base)
        u32               luSize;            // the AptData resource size (relocation bound)
        void*             lpHeader;          // the AptDataHeader (== the resource base)
        bool              lbUsed;
    };
    // STEP 4 gate: drive the nested-content dirty propagation (dirty the placed sprite containers so
    // they tick + place their nested shapes/images). ENABLED (2026-07-01): AptMovie::resolve64 now
    // relocates EVERY per-frame command record's pointer slots (tag-1 Action / tag-2 Label / tag-3
    // PlaceObject name+clipActions / tag-8 Morph) at the native-8 offsets, so a nested container's
    // doFrameControls / queueFrameActions read their records WITHOUT AV. The ActionScript VM
    // EXECUTION stays deferred (resolve64 relocates the action-stream POINTERS but does not parse or
    // run the bytecode; runStream is stubbed) -- the statically-placed nested shapes/images do not
    // need the VM to execute, only their records relocated. // FLAG: an un-run AS action queue is a
    // valid state (the movie just doesn't advance via script); dynamically script-attached content
    // is the deferred VM last-mile.

    ImportBundleSlot s_aImportBundles[KU_MAX_IMPORT_BUNDLES];
    // Per-import pool backing (one E_MEMTYPE_NUMTYPES x KU_IMPORT_POOL_BYTES block per slot).
    u8 s_aImportPoolBacking[KU_MAX_IMPORT_BUNDLES][CgsResource::E_MEMTYPE_NUMTYPES][KU_IMPORT_POOL_BYTES];
    u32  s_uImportBundleCount = 0;
    bool s_bImportReentryGuard = false;   // guard AptLoader::Update re-entrancy while a load is in flight

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
    // (LocateMovieRoot8, the x64 signature-scan root locator, is RETIRED (2026-07-09):
    // AptLoader::CompleteLoad's faithful native-8 root location -- the const chunk's
    // movieOffset @const+0x18, the XB1-attested formula -- covers every bundle, so the
    // scan and its gAptLoadAnimRootOverride poke are gone. The DumpResourceBytes hex
    // probes went with them.)
    static void DriveFaithfulLoad(AptMovieSlot& lrSlot);
    // The SHARED slot load path (both movies travel it) + the lazy framework-movie load.
    static bool AptLoadMovieSlot(AptMovieSlot& lrSlot, CgsResource::Pool* lpPoolStorage,
                                 u8* const* lapPoolBacking, u32 luPoolBytes);
    static void EnsureFrameworkMovie();
    // STEP 3 (import content-load): synchronously load the import bundle named `lpacMovieName`
    // (GuiApt\<NAME>.bundle) into a resident pool, register its AptDataHeader with the data handler,
    // and drive AptCompleteAnimationAsyncLoad on `lpFile` so the import's AptFile is fully
    // loaded+resolved (mpData = root, mnState = 3). Returns true on success (idempotent: a second
    // call for the same name is a no-op that still completes the handle). Called by the homed
    // AptLoader_StartAsyncLoad (the platform stream hook).
    static bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile);
    // (STEP 4 nested-content dirty propagation retired 2026-07-04 -- the AptCIH ctor births
    // fresh sprite/animation children dirty, so no host propagation pass is needed. See the
    // per-frame tick in UpdateRuntime.)

    static bool IsRuntimeReady() { return s_bRuntimeReady; }

    // -------------------------------------------------------------------------
    // AptLoadOneGuiFont -- load one typeface bundle into its resident pool and register
    // the Font with the Apt font collection. Mirrors the PROVEN CgsDev::LoadAndSetDebugFont
    // load shape (pool over malloc backing -> BundleLoader -> type-0x21 entry ->
    // CreateTextureState -> SafeResourceHandle), but the handle goes to the FontCollection
    // instead of the debug manager. Returns true when the typeface registered.
    // -------------------------------------------------------------------------
    // Queue one load notification for a loaded pool entry (see the ring's note above).
    // liRequestType carries the X360 ARTIST request-type numeric the view module's dispatch
    // switches on (4 = APT data, 12 = the localised-text bundle, 16 = a font).
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

    // Queue a notification for EVERY resource of one type in a just-loaded pool (a GUI apt
    // bundle carries one AptData per movie it embeds -- PERSISTENTAPT carries 61, the whole
    // import library, which is how the console resolves imports without per-import streams).
    static u32 QueueLoadNotificationsForPool(CgsResource::Pool* lpPool, u32 luTypeId,
                                             s32 liRequestType)
    {
        u32 luQueued = 0;
        const u32 luMax = lpPool->GetMaxResources();
        for (u32 lu = 0; lu < luMax; ++lu)
        {
            if (lpPool->GetEntryStatusDirect(static_cast<s32>(lu)) == 0)
                continue;
            CgsResource::Entry* lpEntry = const_cast<CgsResource::Entry*>(
                lpPool->GetEntryDirect(static_cast<s32>(lu)));
            if (lpEntry->mpResourceType == 0 || lpEntry->mpResourceType->GetTypeID() != luTypeId)
                continue;
            if (lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY] == 0)
                continue;
            QueueLoadNotification(lpEntry, liRequestType);
            ++luQueued;
        }
        return luQueued;
    }

    static bool AptLoadOneGuiFont(const char* lpcBundlePath, CgsResource::Pool* lpPool)
    {
        // Pool backing (the same generous fixed sizes the debug-font bring-up uses).
        const u32 KU_FONT_POOL_BYTES = 4u * 1024u * 1024u;
        void* lpMainMem   = malloc(KU_FONT_POOL_BYTES);
        void* lpGfxSysMem = malloc(KU_FONT_POOL_BYTES);
        void* lpGfxLclMem = malloc(KU_FONT_POOL_BYTES);
        if (lpMainMem == 0 || lpGfxSysMem == 0 || lpGfxLclMem == 0)
        {
            free(lpMainMem); free(lpGfxSysMem); free(lpGfxLclMem);
            return false;
        }

        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId    = 5;
        lOptions.mpcName = "AptFont";
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muMaxNodes       = 256u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muHeapMemorySize = 3u * 1024u * 1024u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_MAINMEMORY].muHeapAlignment  = 16u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muMaxNodes       = 256u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muHeapMemorySize = KU_FONT_POOL_BYTES - 64u * 1024u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM].muHeapAlignment  = 16u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muMaxNodes       = 256u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muHeapMemorySize = KU_FONT_POOL_BYTES - 64u * 1024u;
        lOptions.maHeapInfo[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL].muHeapAlignment  = 16u;
        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY]      = lpMainMem;
        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_GRAPHICS_SYSTEM] = lpGfxSysMem;
        lOptions.mResource.m_baseResources[CgsResource::E_MEMTYPE_GRAPHICS_LOCAL]  = lpGfxLclMem;
        for (u32 luMemType = 0; luMemType < CgsResource::E_MEMTYPE_NUMTYPES; ++luMemType)
        {
            lOptions.mDescriptor.m_baseResourceDescriptors[luMemType].m_size      = KU_FONT_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[luMemType].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 64u;
        lOptions.muMaxImports           = 64u;
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
            std::snprintf(lac, sizeof(lac), "[AptRT] text: font bundle '%s' FAILED to load.\n", lpcBundlePath);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        s32 liIndex = -1;
        CgsResource::Entry* lpEntry =
            lpPool->FindFirstResourceOfType(CgsResource::E_RESOURCETYPE_FONT, &liIndex);
        CgsResource::Font* lpFont = (lpEntry != 0)
            ? reinterpret_cast<CgsResource::Font*>(lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY])
            : 0;
        if (lpFont == 0)
        {
            std::snprintf(lac, sizeof(lac), "[AptRT] text: '%s' has no Font (0x21) resource.\n", lpcBundlePath);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        // Build the runtime texture state (binds atlas page 0) so the glyph batch can draw.
        lpFont->CreateTextureState();

        // The first registered typeface is the body font (B5EAConDisS) -- the menu-label
        // measure font for the AS-autofit observable (see the apt_labeltxt bridge).
        if (s_pAptBodyFont == nullptr)
            s_pAptBodyFont = lpFont;

        // Queue the font's load notification: the REAL ViewModule::ProcessIncomingLoadNotification
        // @0x8285BD30 (request type 16) validates + collects it into the font collection.
        QueueLoadNotification(lpEntry, 16);

        std::snprintf(lac, sizeof(lac),
            "[AptRT] text: font '%s' registered (family='%s' chars=%u heightPx=%u atlas=%s).\n",
            lpcBundlePath, lpFont->macTypefaceFamilyName, lpFont->muNumChars,
            lpFont->muFontHeightInPixels,
            (lpFont->mpapTextures != 0 && lpFont->muNumTexturePages > 0 && lpFont->mpapTextures[0] != 0
             && lpFont->mpapTextures[0]->mpD3DTexture != 0) ? "d3d-ok" : "NO-D3D");
        CgsDev::Log::WriteToLog(lac);
        return true;
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

        s32 liFonts = 0;
        for (u32 lu = 0; lu < 3u; ++lu)
        {
            if (AptLoadOneGuiFont(KA_APT_FONT_BUNDLES[lu], &s_aAptFontPools[lu]))
                ++liFonts;
        }

        // The language manager: allocator + faithful default formatting, then the string table.
        s_AptLanguageAllocator.Construct(s_aLanguageHeap, static_cast<s32>(KU_LANGUAGE_HEAP_BYTES));
        s_pViewModule->GetLanguageManager()->Prepare(&s_AptLanguageAllocator);
        s_pViewModule->GetLanguageManager()->PrepareDefaultFormattingStrings();
        const bool lbStrings = AptLoadLanguageBundle(KC_APT_LANGUAGE_BUNDLE, &s_AptLanguagePool);

        s_bTextSystemReady = (liFonts > 0);
        char lac[160];
        std::snprintf(lac, sizeof(lac), "[AptRT] text: system %s (fonts=%d strings=%s).\n",
                      s_bTextSystemReady ? "READY" : "INCOMPLETE", liFonts, lbStrings ? "ok" : "MISSING");
        CgsDev::Log::WriteToLog(lac);
    }

    // -------------------------------------------------------------------------
    // PrepareRuntime -- the once-only host bring-up (idempotent). Mirrors the
    // X360 CgsGui::AptAux::InitializeApt @0x82848E50 + AptAllocatorInitialize
    // @0x82ADD118, but only the pieces whose engine bodies exist; every step that
    // crosses an un-homed engine routine is // FLAG'd and skipped defensively.
    // -------------------------------------------------------------------------
    static bool PrepareRuntime()
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
            // note above). 128 KB command stream + 128 KB vertex stream per buffer (x2 = 4
            // carves) -- generous for a single boot/title movie's per-frame geometry, well
            // within the 2 MB pool. failGracefully=true so an overflow rewinds instead of
            // asserting.
            rw::IResourceAllocator* lpAllocator = &s_AptRenderBufferAllocator;
            const bool lbOk = s_AptRenderBuffer.mCommandBuffer.Prepare(
                128u * 1024u, 128u * 1024u, lpAllocator, /*failGracefully*/ true);
            s_bRenderBufferReady = lbOk;
            char lacp[160];
            std::snprintf(lacp, sizeof(lacp),
                "[AptRT] step3 renderbuffer: Construct+Prepare %s (static pool, 128KB cmd / 128KB vtx, used=%u).\n",
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
                s_AptDataAllocator.Construct(s_aAptDataHeap,
                                             static_cast<s32>(sizeof(s_aAptDataHeap)));
                while (!s_pViewModule->GetAptAux()->Prepare(&s_AptDataAllocator))
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

    // -------------------------------------------------------------------------
    // AptLoadMovieSlot -- the SHARED movie bundle-IO leaf (BOTH slots travel it): init
    // the slot's resident pool over the given backing, load "GuiApt\<NAME>.bundle"
    // ([PC IO] -- the X360 streams it via the GUI resource module), then queue one load
    // notification per carried AptData resource so the REAL view-module dispatch performs
    // every registration. Returns true when the bundle loaded (slot.mbLoaded);
    // INSTANTIATION is decoupled (DriveFaithfulLoad, off the play-movie event).    //
    // FAITHFULNESS (2026-06-30, user): the GUIAPT32/GUIAPT64 split was NON-FAITHFUL test
    // scaffolding -- the original game loads apt movies from GUIAPT\. Our PC build's GUIAPT
    // bundles carry the converted "Apt Data:1:7:8" (native 8-byte) data, so the path stays
    // faithful (GUIAPT) while the data content is the x64-converted form. Load through
    // BundleLoader::LoadBundle; the AptData resource (type 0x1E == 30) is FixUp'd by the
    // registered CgsResource::AptDataHeaderType handler.
    // -------------------------------------------------------------------------
    static bool AptLoadMovieSlot(AptMovieSlot& lrSlot, CgsResource::Pool* lpPoolStorage,
                                 u8* const* lapPoolBacking, u32 luPoolBytes)
    {
        char lac[224];
        CgsResource::RegisterAllResourceTypes();   // idempotent: ensure AptDataHeaderType (0x1E) is live

        // Build the FAITHFUL bundle path "GuiApt\<NAME>.bundle" -- the exact format string the X360
        // apt loader uses (verified @0x828504B0), name passed as-is (Windows resolves case/separator).
        // The loader returns <=0 if missing -> bails cleanly.
        char lacBundlePath[160];
        std::snprintf(lacBundlePath, sizeof(lacBundlePath), "GuiApt\\%s.bundle", lrSlot.macName);

        lrSlot.mpPool = lpPoolStorage;
        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId   = 4;
        lOptions.mpcName = "AptMovie";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 256u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = luPoolBytes - 128u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = lapPoolBacking[lt];
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = luPoolBytes;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 128u;   // PERSISTENTAPT packs 101 resources (61 AptData + ~40 tex)
        lOptions.muMaxImports           = 64u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        lrSlot.mpPool->InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lacBundlePath, lrSlot.mpPool,
                                                CgsResource::ResolveResourceType);
        std::snprintf(lac, sizeof(lac), "[AptRT] %s: load '%s' -> %d resources.\n",
                      lrSlot.mpcTag, lacBundlePath, liLoaded);
        CgsDev::Log::WriteToLog(lac);
        if (liLoaded <= 0)
        {
            // FLAG: bundle missing / not platform-4 / unreadable. Bail cleanly.
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: load: bundle missing/unreadable or not platform-4 -- bail (FLAG).\n",
                lrSlot.mpcTag);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        // Pull the single AptData (0x1E) resource; its bytes are the relocated AptDataHeader.
        s32 liIndex = -1;
        CgsResource::Entry* lpEntry =
            lrSlot.mpPool->FindFirstResourceOfType(KU_APTDATA_RESOURCE_TYPE_ID, &liIndex);
        if (lpEntry == nullptr)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: load: no AptData (type 0x1E) resource in bundle -- bail (FLAG).\n",
                lrSlot.mpcTag);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }
        void* lpRes = lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        if (lpRes == nullptr)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: load: AptData resource has null main-memory bytes -- bail (FLAG).\n",
                lrSlot.mpcTag);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }
        lrSlot.mpHeader = reinterpret_cast<CgsGui::AptDataHeader*>(lpRes);
        lrSlot.mbLoaded = true;

        // Queue one load notification per carried AptData resource: the REAL
        // ViewModule::ProcessIncomingLoadNotification @0x8285BD30 (request type 4) registers
        // each header with the Apt data handler (AddAptData) when GuiModule's bridge drains
        // the ring into the view queue -- the console notification flow. A framework bundle
        // carries its whole import library this way (PERSISTENTAPT: 61 AptData resources),
        // which is how imports later resolve WITHOUT per-import bundle IO. Instantiation is
        // decoupled: DriveFaithfulLoad runs off the play-movie event once the registration
        // has landed (FindAptData resolves).
        const u32 luQueued =
            QueueLoadNotificationsForPool(lrSlot.mpPool, KU_APTDATA_RESOURCE_TYPE_ID, 4);
        std::snprintf(lac, sizeof(lac),
            "[AptRT] %s: load: %u AptData notification(s) queued for registration.\n",
            lrSlot.mpcTag, luQueued);
        CgsDev::Log::WriteToLog(lac);
        return true;
    }

    // -------------------------------------------------------------------------
    // EnsureFrameworkMovie -- lazily load + instantiate the AS FRAMEWORK movie ("MAIN")
    // into s_FrameworkSlot at display level 0, through the SAME load path the flow movie
    // takes (AptLoadMovieSlot). Called by PlayRuntimeMovie BEFORE the flow movie loads,
    // so the framework core composes BENEATH it (level 0 under level 1) and its exported
    // classes are resident when the flow movie's imports link.
    // FLAG (host bring-up choice): the console composes PERSISTENTAPT (which imports MAIN)
    // persistently at level 0 via GuiResourceModule; the host starts with the MAIN framework
    // core itself (the 236 KB bundle; PERSISTENTAPT is the follow-on). One-shot: a failed
    // load is logged and NOT retried (the flow movie still loads + runs above the gap).
    // -------------------------------------------------------------------------
    // FLAG (bring-up gate, 2026-07-05): loading MAIN currently AVs inside
    // LoadAnimation's CompleteLoad/Resolve/Fixup (the log dies right after
    // "LoadAnimation('MAIN', ...)"; MAIN.BUNDLE IS frame-table-repaired, so the
    // fault is a record shape among its 89 chars the relocation walk has not met
    // before). Gate the framework load OFF until that walk is fixed so the title
    // flow keeps booting; flip to true to continue the stage-B bring-up.
    static const bool KB_LOAD_FRAMEWORK_MOVIE = true;

    static void EnsureFrameworkMovie()
    {
        if (!KB_LOAD_FRAMEWORK_MOVIE)
        {
            static bool sbLoggedOff = false;
            if (!sbLoggedOff)
            {
                sbLoggedOff = true;
                CgsDev::Log::WriteToLog("[AptRT] framework: GATED OFF (KB_LOAD_FRAMEWORK_MOVIE=false; "
                                        "MAIN Fixup AV under investigation).\n");
            }
            return;
        }

        char lac[160];

        // ---- bundle IO (one-shot; the [PC IO] leaf): load MAIN + PERSISTENTAPT and queue
        // their AptData registrations through the notification chain. --------------------
        if (!s_FrameworkSlot.mbRequested)
        {
            s_FrameworkSlot.mbRequested = true;
            s_FrameworkSlot.miLevel     = 0;   // the framework core sits at display level 0
            // MAIN is the AS core: its frame-0 DoAction runs `new AptCommunicator` + the 24-class
            // registerClass bootstrap. It carries no display (childNodes=0), so level 0 is free.
            std::strncpy(s_FrameworkSlot.macName, "MAIN", sizeof(s_FrameworkSlot.macName) - 1);
            s_FrameworkSlot.macName[sizeof(s_FrameworkSlot.macName) - 1] = '\0';
            CgsDev::Log::WriteToLog("[AptRT] framework: loading the AS core 'MAIN' at level 0 ...\n");
            u8* lapBacking[CgsResource::E_MEMTYPE_NUMTYPES];
            for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                lapBacking[lt] = s_aFrameworkPoolBacking[lt];
            AptLoadMovieSlot(s_FrameworkSlot, &s_FrameworkPoolStorage,
                             lapBacking, KU_FRAMEWORK_POOL_BYTES);
        }
        if (!s_PersistentSlot.mbRequested)
        {
            // §6.4 (2026-07-07): ALSO load PERSISTENTAPT -- the persistent component library that
            // defines the BurnoutComponent base + the menu component classes. The console keeps it
            // resident alongside MAIN (the GUI resource module's state-0 up-front load). Level 2 so
            // its own timeline ticks (its component-class init runs).
            s_PersistentSlot.mbRequested = true;
            std::strncpy(s_PersistentSlot.macName, "PERSISTENTAPT",
                         sizeof(s_PersistentSlot.macName) - 1);
            s_PersistentSlot.macName[sizeof(s_PersistentSlot.macName) - 1] = '\0';
            CgsDev::Log::WriteToLog("[AptRT] persist: loading the component library "
                                    "'PERSISTENTAPT' at level 2 ...\n");
            u8* lapPersistBacking[CgsResource::E_MEMTYPE_NUMTYPES];
            for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                lapPersistBacking[lt] = s_aPersistentPoolBacking[lt];
            AptLoadMovieSlot(s_PersistentSlot, &s_PersistentPoolStorage,
                             lapPersistBacking, KU_FRAMEWORK_POOL_BYTES);
        }

        // ---- instantiate (deferred until the queued registrations landed: DriveFaithfulLoad
        // resolves the header via FindAptData and defers on a miss; the per-frame channel-41
        // re-fire drives the retry -- the async-load observable). -------------------------
        if (s_FrameworkSlot.mbLoaded && !s_FrameworkSlot.mbInstantiated)
        {
            DriveFaithfulLoad(s_FrameworkSlot);
            if (s_FrameworkSlot.mbInstantiated)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] framework: 'MAIN' up (instantiated level=%d).\n",
                    s_FrameworkSlot.miLevel);
                CgsDev::Log::WriteToLog(lac);
            }
        }
        if (s_PersistentSlot.mbLoaded && !s_PersistentSlot.mbInstantiated)
        {
            DriveFaithfulLoad(s_PersistentSlot);
            if (s_PersistentSlot.mbInstantiated)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] persist: 'PERSISTENTAPT' up (instantiated level=%d).\n",
                    s_PersistentSlot.miLevel);
                CgsDev::Log::WriteToLog(lac);
            }
        }
    }

    // -------------------------------------------------------------------------
    // PlayRuntimeMovie -- consume a channel-41 GuiEventPlayAptMovie. Records the
    // movie name in the FLOW slot and ATTEMPTS to load it through the homed Apt
    // loader (after lazily standing the framework movie up at level 0). Defensive:
    // bails (logs) wherever the load path crosses an un-homed piece.
    // -------------------------------------------------------------------------
    static void PlayRuntimeMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
        {
            // null / empty channel-41 name: a spurious re-post (BootLegal interleaves empty events).
            // Ignore it -- do NOT clobber the loaded-movie name or trigger a (failed) reload.
            return;
        }

        const bool lbSameMovie =
            (s_FlowSlot.mbRequested || s_FlowSlot.mbLoaded) &&
            std::strncmp(s_FlowSlot.macName, lpacMovieName, sizeof(s_FlowSlot.macName) - 1) == 0;

        // LOAD-ONCE + RETRY: the channel-41 event re-fires EVERY frame (BootLegal keeps the
        // movie playing). Once the movie is fully up, re-fires are silent no-ops; while its
        // queued registration has not landed yet (the notification drains next frame), the
        // re-fire IS the retry that completes the load -- the async-load observable.
        if (lbSameMovie && s_FlowSlot.mbInstantiated)
            return;

        if (!lbSameMovie)
        {
            // FLOW-SLOT ROUTING: channel 41 drives the FLOW movie at display level 1 (above
            // the level-0 framework). BootLegal posts level 1; a different level is a protocol
            // surprise -- log it (assert-equivalent) but still route to the flow slot.
            if (liLevelNum != 1)
            {
                char lacLvl[160];
                std::snprintf(lacLvl, sizeof(lacLvl),
                    "[AptRT] PlayMovie: UNEXPECTED level %d (the flow movie is level 1) -- "
                    "routing to the flow slot anyway (FLAG).\n", liLevelNum);
                CgsDev::Log::WriteToLog(lacLvl);
            }
            std::strncpy(s_FlowSlot.macName, lpacMovieName, sizeof(s_FlowSlot.macName) - 1);
            s_FlowSlot.macName[sizeof(s_FlowSlot.macName) - 1] = '\0';
            s_FlowSlot.mbRequested = true;
            s_FlowSlot.miLevel     = liLevelNum;

            char lac[200];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] PlayMovie: consume channel-41 '%s' (level %d).\n",
                s_FlowSlot.macName, liLevelNum);
            CgsDev::Log::WriteToLog(lac);
        }

        if (!s_bRuntimeReady)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: runtime not ready -- load deferred.\n");
            s_FlowSlot.mbRequested = false;   // allow a retry next time the event fires
            return;
        }

        // Stand the AS FRAMEWORK movies up FIRST (level 0/2, lazily, one-shot IO + deferred
        // instantiate) so they compose beneath the flow movie and their exported classes are
        // resident when the flow movie's imports link.
        EnsureFrameworkMovie();

        // The flow movie's own bundle IO ([PC IO] leaf) + queued registration, then the
        // deferred instantiate (DriveFaithfulLoad resolves via FindAptData; defers until the
        // registration notification has been dispatched).
        if (!s_FlowSlot.mbLoaded)
        {
            u8* lapBacking[CgsResource::E_MEMTYPE_NUMTYPES];
            for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
                lapBacking[lt] = s_aFlowPoolBacking[lt];
            AptLoadMovieSlot(s_FlowSlot, &s_FlowPoolStorage, lapBacking, KU_FLOW_POOL_BYTES);
        }
        if (s_FlowSlot.mbLoaded && !s_FlowSlot.mbInstantiated)
            DriveFaithfulLoad(s_FlowSlot);
    }

    // =========================================================================
    // DriveFaithfulLoad -- drive the FAITHFUL Apt load + instantiate for one movie slot:
    //
    //   FindAptData(name)  ->  AptLoader::Load(name)  ->  AptCallbackFile::LoadAnimation(name,&h)
    //       -> AptCompleteAnimationAsyncLoad -> AptLoader::CompleteLoad -> Resolve -> Fixup
    //   then AptGetAnimationAtLevel(level) -> MakeCharacterAnimationInst(pFile) -> SetCharacterInst.
    //
    // The header is resolved through the DATA HANDLER (FindAptData) -- registration arrived
    // via the load-notification chain (ViewModule::ProcessIncomingLoadNotification ->
    // AddAptData), exactly the console flow. A FindAptData miss DEFERS (the registration
    // notification drains on a later frame; the per-frame channel-41 re-fire retries) --
    // the async-load observable. The movie root locates inside CompleteLoad from the const
    // chunk (the XB1-attested movieOffset formula); the resolve64 relocation bounds derive
    // inside LoadAnimation from the header. No host-side scans or pokes remain.
    //
    // On ANY hard failure past the resolve: bail cleanly (slot.mbInstantiated stays false),
    // leaving the game up (the other slot's movie still ticks/renders).
    //
    // PER SLOT (two-movie refactor): the whole chain is driven against the passed
    // AptMovieSlot -- its name and DISPLAY LEVEL (the instantiate step attaches at
    // AptGetAnimationAtLevel(slot.miLevel): level 0 = framework, level 1 = flow --
    // the console's root display-list arrangement).
    // =========================================================================
    static void DriveFaithfulLoad(AptMovieSlot& lrSlot)
    {
        if (!KB_FAITHFUL_PATH_ENABLED || lrSlot.mbLoadAttempted)
            return;

        char lac[224];
        // Need a live Apt context (the director + loader) + the GC pool (the root CIH allocates
        // from it). GetTarget()->mpLoader is the faithfully-initialised AptLoader.
        AptTarget* lpTarget = GetTarget();
        if (lpTarget == nullptr || lpTarget->mpAnimationTarget == nullptr || lpTarget->mpLoader == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: no Apt director/loader (GetTarget null) "
                                    "-- bail, fallback to direct geometry (FLAG).\n");
            return;
        }
        if (gpGCPoolManager == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool null -- bail, fallback (FLAG).\n");
            return;
        }

        // --- 1. RESOLVE the header through the data handler (registration arrived via the
        //        load-notification chain). A miss defers WITHOUT consuming the one-shot: the
        //        queued notification dispatches on a later frame and the re-fire retries. ---
        CgsGui::AptAux* lpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;
        if (lpAptAux == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: AptAux singleton null -- bail (FLAG).\n");
            return;
        }
        CgsGui::AptDataHeader* lpHeader = lpAptAux->mAptDataHandler.FindAptData(lrSlot.macName);
        if (lpHeader == nullptr && lrSlot.mpHeader != nullptr)
        {
            // The framework slots are keyed by the HOST bundle name (MAIN / PERSISTENTAPT),
            // but the handler registers each AptData under its AUTHORED movie name (the
            // header's own name field -- e.g. MAIN.bundle's movie is authored 'main').
            // Resolve the slot's own movie -- the bundle's leading AptData, captured at
            // load time -- by its authored name, and adopt that name for the load chain
            // (AptLoader::Load / LoadAnimation key on the registered name).
            const uintptr_t luHdr = reinterpret_cast<uintptr_t>(lrSlot.mpHeader);
            const char* lpacAuthored = reinterpret_cast<const char*>(
                luHdr + static_cast<uintptr_t>(*reinterpret_cast<const u64*>(luHdr)));  // serialized .apt header: name @+0x00
            lpHeader = lpAptAux->mAptDataHandler.FindAptData(lpacAuthored);
            if (lpHeader != nullptr)
            {
                std::strncpy(lrSlot.macName, lpacAuthored, sizeof(lrSlot.macName) - 1);
                lrSlot.macName[sizeof(lrSlot.macName) - 1] = '\0';
            }
        }
        if (lpHeader == nullptr)
        {
            // Registration not dispatched yet -- defer (retry on the next channel-41 fire).
            return;
        }
        lrSlot.mbLoadAttempted = true;   // the one-shot arms once the header resolves

        std::snprintf(lac, sizeof(lac), "[AptRT] %s: faithful: load-path begin ('%s' registered).\n",
                      lrSlot.mpcTag, lrSlot.macName);
        CgsDev::Log::WriteToLog(lac);

        // The AptData resource IS the AptDataHeader; the header sits at the resource base and
        // the converted 6-field header carries the span size @+0x28 (the resolve64 bounds
        // LoadAnimation publishes engine-side).
        const uintptr_t luBase = reinterpret_cast<uintptr_t>(lpHeader);
        const u32 luSize = static_cast<u32>(
            *reinterpret_cast<const u64*>(luBase + 0x28u));   // serialized .apt header: size @+0x28
        lrSlot.mpHeader       = lpHeader;
        lrSlot.muResourceBase = luBase;
        lrSlot.muResourceSize = luSize;
        if (luSize == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: header size slot zero -- bail (FLAG).\n");
            return;
        }

        // Peek the pointer size from the "Apt Data:1:7:N" signature. That signature lives in the
        // APT DATA chunk -- hdr field 2 (@+0x10) of the libapt2 6-field header [name, baseName,
        // aptData, const, geom, size].
        const u32 luAptDataOff = static_cast<u32>(*reinterpret_cast<const u64*>(luBase + 0x10u));
        if (luAptDataOff == 0 || luAptDataOff >= luSize)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: aptData offset out of range -- bail (FLAG).\n");
            return;
        }
        AptConstFile* lpConstFile = reinterpret_cast<AptConstFile*>(luBase + luAptDataOff);
        const int liPtrSize = lpConstFile->GetPointerSizeBytes();   // 8 (native) or 4 (console)
        std::snprintf(lac, sizeof(lac),
            "[AptRT] %s: faithful: header@0x%llX aptData@0x%X ptrSize=%d (%u bytes).\n",
            lrSlot.mpcTag, (unsigned long long)luBase, luAptDataOff, liPtrSize, luSize);
        CgsDev::Log::WriteToLog(lac);
        (void)lpConstFile;

        // --- 2. REGISTER / look up the AptFile handle (AptLoader::Load) --------------------------        // --- 3. REGISTER / look up the AptFile handle (AptLoader::Load) --------------------------
        // Load returns a handle owning ONE counted reference. We keep it (laOwned) and hand a
        // COPY to LoadAnimation (which consumes its copy, per the console by-value slot). The owned
        // handle survives, and after CompleteLoad it points at the loaded movie (mpData = root).
        EAStringC lNameStr(lrSlot.macName);
        AptFilePtr laOwned = lpTarget->mpLoader->Load(lNameStr);
        if (laOwned.pData == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: AptLoader::Load returned null handle -- bail (FLAG).\n");
            return;
        }

        AptFilePtr laForCallback;
        laForCallback.pData = laOwned.pData;
        AptSharedPtrIncRef(laForCallback.pData);   // the callback consumes this copy (keep laOwned alive)

        // --- 3. DRIVE the faithful load: LoadAnimation -> AsyncLoad -> CompleteLoad -> Resolve ->
        //        Fixup. The Fixup relocates the movie root in place (charCount/importCount set). ---
        std::snprintf(lac, sizeof(lac),
            "[AptRT] %s: faithful: LoadAnimation('%s', handle=%p) ...\n",
            lrSlot.mpcTag, lrSlot.macName, (void*)laForCallback.pData);
        CgsDev::Log::WriteToLog(lac);
        CgsGui::AptCallbackFile::LoadAnimation(lrSlot.macName, &laForCallback);

        // The owned handle's AptFile is now loaded (CompleteLoad set mpData = root, state = 3).
        AptFile* lpFile = laOwned.pData;
        if (lpFile == nullptr || lpFile->mpData == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: post-LoadAnimation AptFile not loaded "
                                    "(mpData null) -- bail (FLAG).\n");
            return;
        }
        lrSlot.mpAptFile  = lpFile;
        lrSlot.mpCharAnim = reinterpret_cast<AptCharacterAnimation*>(
            static_cast<char*>(lpFile->mpData) + ((liPtrSize == 8) ? 0x20 : 0x10));   // def base

        // "Fixup COMPLETED" marker (read the counts the Fixup walk populated at the def base).
        {
            const u32 luOffCC = (liPtrSize == 8) ? 0x18u : 0x0Cu;   // charCount
            const u32 luOffIC = (liPtrSize == 8) ? 0x34u : 0x20u;   // importCount
            const int liCC = *reinterpret_cast<const int*>(
                reinterpret_cast<char*>(lrSlot.mpCharAnim) + luOffCC);
            const int liIC = *reinterpret_cast<const int*>(
                reinterpret_cast<char*>(lrSlot.mpCharAnim) + luOffIC);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: Fixup COMPLETED. charCount@+0x18=%d importCount@+0x34=%d (via CompleteLoad)\n",
                lrSlot.mpcTag, liCC, liIC);
            CgsDev::Log::WriteToLog(lac);
            if (liCC <= 0 || liCC > 512 || liIC < 0 || liIC > 4096)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: post-fixup counts INSANE -- bail to fallback (FLAG).\n");
                return;
            }
        }

        // --- STEP 3: CONTENT-LOAD THE IMPORTS -----------------------------------------------------
        // The parent's Fixup pass-3 just REGISTERED each import (AptLoader::Load -> "requested"
        // AptFile). Drive the faithful console loader state machine (AptLoader::Update): the homed
        // AptLoader_StartAsyncLoad synchronously loads each import bundle + completes it (state 1->3),
        // then Update links the graph bottom-up (leaf imports 3->4, then the parent 3->4 ->
        // AptCharacterAnimation_Link -> AptFile::FindExport -> parent charTable[importId] populated).
        // After this, the parent's charTable holds the real imported characters, so the instantiation
        // + tick below place the title's visible content (not null slots). Guarded: a missing/
        // unconvertable import is left "requested" (an honest data boundary); Update skips it and the
        // parent still links (that one char stays null + the tick skips it safely).
        if (lpTarget->mpLoader != nullptr)
        {
            const int liImportCount = *reinterpret_cast<const int*>(
                reinterpret_cast<char*>(lrSlot.mpCharAnim) + 0x34);   // def+0x34 importCount
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: STEP3 content-load %d import(s) via AptLoader::Update ...\n",
                lrSlot.mpcTag, liImportCount);
            CgsDev::Log::WriteToLog(lac);
            lpTarget->mpLoader->Update();   // drives StartAsyncLoad (load) + Link for every import
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: STEP3 content-load done. import bundles resident=%u.\n",
                lrSlot.mpcTag, s_uImportBundleCount);
            CgsDev::Log::WriteToLog(lac);
        }

        // STEP 5 PROBE (blocker #1): confirm the ROOT movie's frame table is RELOCATED by
        // AptCharacterAnimation::Fixup case-9 -> AptMovie::resolve64. The root def-base IS
        // the root AptMovie (frameCount@+0x00, mpFrames@+0x08). Before the fix mpFrames held
        // the raw file offset 0x5180; after resolve64 it is a live pointer (base + 0x5180),
        // and frame[0].mnCommandCount should be 13 (verified vs TITLE_SCREEN02.bundle).
        {
            const char* lpRootMovie = reinterpret_cast<const char*>(lrSlot.mpCharAnim);
            const int liFrameCount = *reinterpret_cast<const int*>(lpRootMovie + 0x00);
            void** lpFrames = *reinterpret_cast<void* const*>(lpRootMovie + 0x08) ?
                *reinterpret_cast<void** const*>(lpRootMovie + 0x08) : nullptr;
            const unsigned long long luFramesRaw =
                *reinterpret_cast<const unsigned long long*>(lpRootMovie + 0x08);
            // The frame table is a relocated pointer iff it lands inside the resource span.
            const bool lbRelocated =
                (luFramesRaw >= lrSlot.muResourceBase &&
                 luFramesRaw <  lrSlot.muResourceBase + lrSlot.muResourceSize);
            int liFrame0Cmds = -1;
            if (lbRelocated && lpFrames != nullptr)
            {
                // frame[0]: {mnCommandCount@0, mpCommands@8} (native-8 stride 16).
                liFrame0Cmds = *reinterpret_cast<const int*>(lpFrames);
            }
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: STEP5 frame-table: frameCount=%d mpFrames=0x%016llX relocated=%s "
                "frame0.cmdCount=%d (want 13)\n",
                lrSlot.mpcTag, liFrameCount, luFramesRaw, lbRelocated ? "YES" : "NO(raw-offset)", liFrame0Cmds);
            CgsDev::Log::WriteToLog(lac);
        }

        // --- 5. the root level-N CIH on the director's root display list ---------------------------
        // AptGetAnimationAtLevel(slot.miLevel) searches the director's root display list for the
        // node at the SLOT's display level then (if absent) creates an AptCIH via the GC pool
        // (level 0 = the framework movie, level 1 = the flow movie -- the console's root
        // display-list arrangement). Self-test the GC pool first (probe the 40-byte carve).
        {
            void* lpGCTest = gpGCPoolManager->Allocate(40);
            if (lpGCTest == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool cannot carve 40 bytes -- bail (FLAG).\n");
                return;
            }
            gpGCPoolManager->Deallocate(lpGCTest, 40);
        }
        std::snprintf(lac, sizeof(lac),
            "[AptRT] %s: faithful: AptGetAnimationAtLevel(%d) (create root CIH) ...\n",
            lrSlot.mpcTag, lrSlot.miLevel);
        CgsDev::Log::WriteToLog(lac);
        AptCIH* lpRootCIH = AptGetAnimationAtLevel(lrSlot.miLevel);
        if (lpRootCIH == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: root CIH null (GC pool / display list) -- bail (FLAG).\n");
            return;
        }
        lrSlot.mpRootCIH = lpRootCIH;

        // --- 6. instantiate the movie + bind it to the root CIH -----------------------------------
        // The engine's MakeCharacterAnimationInst is the single faithful 64-bit path: its
        // AptCharacterAnimation::IncCharacterList walks the movie's embedded character table
        // (reached via KU_AptEmbeddedMovieOff = char+0x20) UNCONDITIONALLY, exactly like the console.
        //
        // RE-ENABLED (Steps 6+10, 2026-07-01): the converter char[1] data bug is FIXED -- the
        // GUIAPT/TITLE_SCREEN02 bundle is now uniformly 64-bit (every character carries signature
        // @+0x08; char[1] is a clean widened Image with mpAnimationFile@+0x18 == 0), via the libapt2
        // Movie::Write pointer-alignment fix / the fix_title_screen02.py byte-patch. So
        // IncCharacterList's `mpCharacterTable[1]->mpAnimationFile` reads a null slot and the walk is
        // safe. The struct/offset match (serialized-64 def-base: charCount@+0x18 / charTable@+0x20)
        // was already in place. This mirrors AptLinker::Update pass 2 (@0x82B0D028, AptLinker.cpp:558-584):
        // MakeCharacterAnimationInst(pFile) -> AptCIH::SetCharacterInst(animInst, moveRenderData) ->
        // seed the sprite-instance state (mnGotoFrame = -1 "none", mnClipActionFlags |= 0x80) the tick
        // reads. The initial in-line tick AptLinker::Update also does is driven per-frame by
        // UpdateRuntime (Step 3) instead. DEFENSIVE: guarded; a null animInst leaves the empty root
        // CIH in place (game stays up, geometry fallback still available).
        AptCharacterAnimationInst* lpAnimInst = MakeCharacterAnimationInst(lpFile);
        if (lpAnimInst != nullptr)
        {
            lpRootCIH->mFlagsA &= 0x9FFFFFFFu;                     // clear the transition state bits (v61[3] &= 0x9FFFFFFF)
            lpRootCIH->SetCharacterInst(reinterpret_cast<AptCharacterInst*>(lpAnimInst),
                                        /*bMoveRenderData*/ true); // install the anim inst on the root node
            // Seed the freshly-installed sprite instance's play state for the tick (AptLinker.cpp:583-584).
            AptCharacterSpriteInstBase* lpSprite =
                static_cast<AptCharacterSpriteInstBase*>(lpRootCIH->GetCharacterInst());
            if (lpSprite != nullptr)
            {
                lpSprite->mnGotoFrame        = -1;                 // *(v61[8]+16) = -1  (no pending goto)
                lpSprite->mnClipActionFlags |= 0x80u;             // *(v61[8]+20) |= 0x80
            }
            lrSlot.mbInstantiated = true;
            // Open the input-recorder/replay gate (normal-play value) so the freshly-placed imported
            // sprite CONTAINERS auto-advance + run doFrameControls, recursing to place their nested
            // shapes/images (the title's actual visible content lives inside those imported sprites).
            // FLAG: the replay-recorder subsystem that would drive this is out of scope; set once here.
            gbAptRecorderGate = 1;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: INSTANTIATED -- animInst %p bound to root CIH %p @level %d (movie root %p); "
                "IncCharacterList walked %d chars; sprite state seeded. dlParent(after bind)=%p\n",
                lrSlot.mpcTag, (void*)lpAnimInst, (void*)lpRootCIH, lrSlot.miLevel, lpFile->mpData,
                *reinterpret_cast<const int*>(reinterpret_cast<char*>(lrSlot.mpCharAnim) + 0x18),
                (void*)lpRootCIH->GetDisplayListParent());
            CgsDev::Log::WriteToLog(lac);
        }
        else
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] %s: faithful: MakeCharacterAnimationInst returned null -- root CIH stays empty "
                "(game up). (FLAG)\n", lrSlot.mpcTag);
            CgsDev::Log::WriteToLog(lac);
            lrSlot.mbInstantiated = false;
        }
    }

    // =========================================================================
    // LoadImportBundle -- content-load ONE import movie by name and drive its AptFile
    // through the faithful completion, the PC substitute for the console's async .apt
    // stream (the stream AptLoader_StartAsyncLoad kicks off).
    //
    // REGISTERED-DATA FIRST (the console shape): a framework bundle load registers EVERY
    // AptData it carries through the load-notification chain (PERSISTENTAPT alone carries
    // the 61-movie import library), so an import normally resolves straight from the data
    // handler -- FindAptData(name) hits and the faithful AptCallbackFile::LoadAnimation
    // completes the handle with NO per-import bundle IO.
    //
    // FALLBACK (FLAG, [PC IO]): an import NOT carried by any loaded bundle synchronously
    // loads its own GuiApt\<NAME>.bundle into a resident pool and registers its header
    // directly (the resolved-name AddAptData form -- the notification chain cannot help
    // mid-engine, the completion is needed in this call), then completes through the same
    // faithful LoadAnimation. Idempotent by name (the registry dedups).
    // =========================================================================
    static bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile)
    {
        char lac[256];
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
            return false;

        CgsGui::AptAux* lpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;
        if (lpAptAux == nullptr)
            return false;

        // The registered-data path: the import's AptData is already with the handler
        // (registered when its carrying bundle's notifications dispatched).
        if (lpAptAux->mAptDataHandler.FindAptData(lpacMovieName) != nullptr)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' resolves from the data handler (no bundle IO).\n",
                lpacMovieName);
            CgsDev::Log::WriteToLog(lac);
            CgsGui::AptCallbackFile::LoadAnimation(lpacMovieName, lpFile);
            return true;
        }

        // ---- FALLBACK ([PC IO] + FLAG): per-import bundle load ----------------------
        // Dedup: already resident?
        ImportBundleSlot* lpSlot = nullptr;
        for (u32 lu = 0; lu < s_uImportBundleCount; ++lu)        for (u32 lu = 0; lu < s_uImportBundleCount; ++lu)
        {
            if (s_aImportBundles[lu].lbUsed &&
                _stricmp(s_aImportBundles[lu].macName, lpacMovieName) == 0)
            {
                lpSlot = &s_aImportBundles[lu];
                break;
            }
        }

        if (lpSlot == nullptr)
        {
            // Fresh import: allocate a slot + load the bundle.
            if (s_uImportBundleCount >= KU_MAX_IMPORT_BUNDLES)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] import-load: registry full (%u) -- cannot load '%s' (FLAG).\n",
                    KU_MAX_IMPORT_BUNDLES, lpacMovieName);
                CgsDev::Log::WriteToLog(lac);
                return false;
            }
            lpSlot = &s_aImportBundles[s_uImportBundleCount];
            std::strncpy(lpSlot->macName, lpacMovieName, sizeof(lpSlot->macName) - 1);
            lpSlot->macName[sizeof(lpSlot->macName) - 1] = '\0';
            lpSlot->lbUsed = true;

            // Build "GuiApt\<NAME>.bundle" (Windows resolves case; the files are UPPERCASE).
            char lacPath[160];
            std::snprintf(lacPath, sizeof(lacPath), "GuiApt\\%s.bundle", lpacMovieName);

            // Init the resident pool (same options as the parent movie pool).
            CgsResource::Pool::InitOptions lOptions;
            lOptions.miId   = 4;
            lOptions.mpcName = "AptImport";
            u8 (*lpBacking)[KU_IMPORT_POOL_BYTES] = s_aImportPoolBacking[s_uImportBundleCount];
            for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
            {
                lOptions.maHeapInfo[lt].muMaxNodes       = 256u;
                lOptions.maHeapInfo[lt].muHeapMemorySize = KU_IMPORT_POOL_BYTES - 64u * 1024u;
                lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
                lOptions.mResource.m_baseResources[lt]   = lpBacking[lt];
                lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_IMPORT_POOL_BYTES;
                lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
            }
            lOptions.muMaxResources         = 64u;
            lOptions.muMaxImports           = 64u;
            lOptions.miRefCountThreshold    = 0;
            lOptions.miNumDependencies      = 0;
            lOptions.miBankId               = 0;
            lOptions.mbAllowDefragmentation = false;
            lpSlot->Pool.InitPool(&lOptions);

            CgsResource::BundleLoader lLoader;
            const s32 liLoaded = lLoader.LoadBundle(lacPath, &lpSlot->Pool,
                                                    CgsResource::ResolveResourceType);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' -> %d resources.\n", lacPath, liLoaded);
            CgsDev::Log::WriteToLog(lac);
            if (liLoaded <= 0)
            {
                CgsDev::Log::WriteToLog("[AptRT] import-load: bundle missing/unreadable -- import left "
                                        "'requested' (FLAG: honest data boundary).\n");
                lpSlot->lbUsed = false;
                return false;
            }

            s32 liIndex = -1;
            CgsResource::Entry* lpEntry =
                lpSlot->Pool.FindFirstResourceOfType(KU_APTDATA_RESOURCE_TYPE_ID, &liIndex);
            void* lpRes = (lpEntry != nullptr)
                ? lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY] : nullptr;
            if (lpRes == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] import-load: no AptData resource -- left 'requested' (FLAG).\n");
                lpSlot->lbUsed = false;
                return false;
            }
            lpSlot->lpHeader = lpRes;
            lpSlot->luBase   = reinterpret_cast<uintptr_t>(lpRes);
            lpSlot->luSize   =
                lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

            // Register the header with the data handler so the faithful FindAptData(name)
            // resolves it (the resolved-name form -- FLAG x64; see the fallback note above).
            lpAptAux->mAptDataHandler.AddAptData(
                reinterpret_cast<CgsGui::AptDataHeader*>(lpRes), lpSlot->macName);

            ++s_uImportBundleCount;            ++s_uImportBundleCount;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' resident base=0x%016llX size=%u (slot %u).\n",
                lpSlot->macName, (unsigned long long)lpSlot->luBase, lpSlot->luSize,
                s_uImportBundleCount - 1);
            CgsDev::Log::WriteToLog(lac);
        }

        // Complete through the same faithful callback as the registered path (it resolves the
        // header's chunks + publishes the resolve64 bounds itself).
        std::snprintf(lac, sizeof(lac),
            "[AptRT] import-load: complete '%s' from the freshly-registered fallback bundle.\n",
            lpSlot->macName);
        CgsDev::Log::WriteToLog(lac);
        CgsGui::AptCallbackFile::LoadAnimation(lpacMovieName, lpFile);
        return true;
    }

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

    // Forward declarations (bodies below, with the component view-state bridge).
    static AptCIH* AptFindClipByName(AptCIH* lpNode, const char* lpacName, int liDepth);
    static bool AptApplyTitleHelpItemDefaults(AptCIH* lpMenuClip);

    // -------------------------------------------------------------------------
    // ShimResidueUpdate -- the LAST shim-side per-frame drive: the title help-item
    // defaults retry (armed by the SelectionMenu transin; applied once the
    // StaticHelpItem + its ControllerButtons icon subtree have composed). The movie
    // TICK ownership moved to the real chain (GuiModule::Update ->
    // CgsGui::ViewModule::Update -> AptAux::Update -> the engine AptUpdateTarget
    // frame pacer in AptUpdate.cpp), so this residue no longer paces or ticks
    // anything. Deleted with the component shim.
    // -------------------------------------------------------------------------
    static void ShimResidueUpdate()
    {
        if (!s_bRuntimeReady)
            return;

        if (s_bHelpDefaultsPending && s_FlowSlot.mbLoaded && s_FlowSlot.mpRootCIH != nullptr)
        {
            static int s_iHelpAttempts = 0;
            AptCIH* lpFlowRoot = static_cast<AptCIH*>(s_FlowSlot.mpRootCIH);
            AptCIH* lpMenuClip2 = AptFindClipByName(lpFlowRoot, "SelectionMenu_mc", 0);
            if (lpMenuClip2 != nullptr && AptApplyTitleHelpItemDefaults(lpMenuClip2))
            {
                s_bHelpDefaultsPending = false;
            }
            else if ((++s_iHelpAttempts % 300) == 0)   // heartbeat ~every 10s
            {
                CgsDev::Log::WriteToLog(
                    "[AptRT] helpitem: defaults still pending (icon subtree not composed yet) -- retrying.\n");
            }
        }
    }

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
    static void DispatchRenderBuffer()
    {
        if (!s_bRuntimeReady || !s_bAuxReady || !s_bRenderBufferReady)
            return;

        const bool lbFirst = !s_bFlushProbed;

        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrBuffer =
            s_AptRenderBuffer.mCommandBuffer;

        lrBuffer.Swap();               // freeze the write buffer for dispatch
        // Reset the NEW write buffer's stream positions for the next frame (the faithful
        // ImRenderBuffer::Clear @0x1EA7D4 -- Swap alone does NOT reset them). Without this the
        // vertex stream (the text/mask AllocVertices consumer) fills permanently after ~60
        // text-drawing frames (RenderStart returns null forever -> the dynamic-text glyphs
        // silently stop landing) and the command stream degrades into rewind churn.
        lrBuffer.Clear();
        lrBuffer.Dispatch();           // re-issue every command to the D3D9 device (faithful PC path)
        ++s_iRenderFrame;

        if (lbFirst)
        {
            s_bFlushProbed = true;
            CgsDev::Log::WriteToLog(
                "[AptRT] render: ViewModule::Render chain -> D3D9 via ImRenderBuffer::Dispatch (OK; per-frame).\n");
        }
    }

    static bool IsRuntimeMovieLive()
    {
        // FLOW-slot semantics: BootLegal drives this query off its channel-41 movie
        // (the framework movie's liveness is internal to the runtime).
        return s_FlowSlot.mbInstantiated && s_FlowSlot.mpRootCIH != nullptr;
    }

    // -------------------------------------------------------------------------
    // StopRuntimeMovie -- the flow left BF_LEGAL (the accept path posted command
    // 70). On the console, leaving the state unloads the title movie (OnLeave's
    // channel-41 {"",1} post -> CgsAptAux unload), whose render-side observable is
    // the movie's render root leaving the tree. The async unload path is not homed,
    // so the stand-in parks the FLOW movie through the ENGINE'S OWN removal op:
    // deletion-mark its render root (AptRenderTreeManager::Update_ItemRemoved) so
    // Render_GetRoot / the sibling walk prune it -- the title disappears exactly as
    // on console. (The render drive walks every layer faithfully now, so the old
    // host-side per-layer render gate is gone with RenderRuntime.) Idempotent.
    // FLAG (follow-on): the faithful unload (resource release + target-instance
    // teardown) lands with the CgsAptAux unload chain.
    // -------------------------------------------------------------------------
    static void StopRuntimeMovie()
    {
        if (s_bMovieStopped)
            return;
        s_bMovieStopped = true;

        if (s_FlowSlot.mbInstantiated && s_FlowSlot.mpRootCIH != nullptr)
        {
            AptCIH* lpRoot = static_cast<AptCIH*>(s_FlowSlot.mpRootCIH);
            AptCharacterInst* lpCI = lpRoot->GetCharacterInst();
            if (lpCI != nullptr && (lpCI->GetTypeTag() == 5 || lpCI->GetTypeTag() == 9))
            {
                AptRenderItem* lpItem =
                    static_cast<AptCharacterSpriteInstBase*>(lpCI)->mpRenderItem;
                if (lpItem != nullptr)
                    AptCurrentRenderTreeManager()->Update_ItemRemoved(lpItem, gnCurrUpdateTick);
            }
        }

        CgsDev::Log::WriteToLog("[AptRT] StopMovie: BF_LEGAL left -- FLOW render root "
                                "deletion-marked (framework movie keeps running; unload deferred).\n");
    }

    // True once the movie has COMPOSED: the root clip's first paced tick has run its
    // frame-0 place commands, so the child display list is populated (the PLACE-named
    // clips the view-state bridge targets exist). The console's equivalent gate is the
    // GuiCache apt-component handshake (AreAllAptComponentsInitialised): a component
    // reports initialised only once its clip is placed.
    static bool IsRuntimeMovieComposed()
    {
        // FLOW-slot semantics (BootLegal's compose gate): the framework movie's own
        // composition is NOT part of this handshake.
        if (!IsRuntimeMovieLive())
            return false;
        AptCIH* lpRoot = reinterpret_cast<AptCIH*>(s_FlowSlot.mpRootCIH);
        AptCharacterInst* lpCI = lpRoot->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return false;
        AptDisplayListState* lpState =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        return lpState != nullptr && lpState->mpFirst != nullptr;
    }

    // -------------------------------------------------------------------------
    // AptRuntimeApplyComponentViewState -- PC bring-up shim for the GuiComponent apt-view
    // protocol (FLAG; see the header note). Finds the root movie's placed child clip by
    // its PLACE instance name and jumps it to the view-state frame label, playing.
    // The label lives in the clip's own embedded AptMovie label hash (registered by
    // resolve64's tag-2 pass); a clip or label miss is logged + false (clean no-op).
    // -------------------------------------------------------------------------
    // Try to jump ONE clip node to the labelled frame; when its own timeline lacks the
    // label, recurse into its child display list (the components are CONTAINER sprites --
    // the transition timeline lives in a nested clip, e.g. char[19] transin/transout).
    static bool AptViewStateGotoLabel(AptCIH* lpNode, const char* lpacLabel, int liDepth)
    {
        if (lpNode == nullptr || liDepth > 6)
            return false;
        AptCharacterInst* lpCI = lpNode->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return false;
        AptCharacterSpriteInstBase* lpSprite = static_cast<AptCharacterSpriteInstBase*>(lpCI);
        AptCharacter* lpChar = lpSprite->mpRenderItem ? lpSprite->mpRenderItem->mpCharacter : nullptr;
        if (lpChar != nullptr)
        {
            AptMovie* lpMovie = reinterpret_cast<AptMovie*>(
                reinterpret_cast<char*>(lpChar) + KU_AptEmbeddedMovieOff);
            EAStringC lLabel(lpacLabel);
            const int liFrame = lpMovie->labelToFrame(&lLabel);
            if (liFrame >= 0)
            {
                // The faithful gotoAndPlay tail (AptCIH::_gotoAndX @0x82B0D2F0, bPlay=1):
                // jumpToFrame + SET the auto-play state bit (mnClipActionFlags bit6 0x40) +
                // re-dirty the node so it keeps ticking. The clip then PLAYS the labelled
                // transition segment until its authored stop() frame action clears the play
                // bit (the interpreter Stop op) -- every Title_Screen02 state frame carries
                // one (verified in the 1:7:4 action streams). This is exactly what the
                // MAIN.bundle TransitionComponent AS does on the console (_parent.gotoAndPlay
                // (viewState) from gAptCommunicator.UpdateAll). The prior 0x80 "freshly
                // placed" seed was the WRONG bit: tick clears it after ONE step, freezing
                // every transition on its jump frame (the broken title-screen animations).
                lpNode->jumpToFrame(liFrame);
                lpSprite->mnClipActionFlags =
                    (lpSprite->mnClipActionFlags & 0xFFFFFFBFu) | 0x40u;
                lpNode->SetDirtyState(true, true);
                char lac[192];
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] viewstate:   clip %p depth %d -> frame %d ('%s')\n",
                    (void*)lpNode, liDepth, liFrame, lpacLabel);
                CgsDev::Log::WriteToLog(lac);
                return true;
            }
        }
        AptDisplayListState* lpKids = lpSprite->mDisplayList.AsState();
        if (lpKids == nullptr)
            return false;
        for (AptCIH* lpK = lpKids->mpFirst; lpK != nullptr; lpK = lpK->GetDisplayListNext())
        {
            if (AptViewStateGotoLabel(lpK, lpacLabel, liDepth + 1))
                return true;
        }
        return false;
    }

    // Find a placed clip by its PLACE instance name anywhere in the movie (the root's
    // display list, recursing into container sprites -- MenuItem_0/1 live INSIDE
    // SelectionMenu_mc, not on the root list). Sprite/animation nodes only.
    static AptCIH* AptFindClipByName(AptCIH* lpNode, const char* lpacName, int liDepth)
    {
        if (lpNode == nullptr || liDepth > 6)
            return nullptr;
        AptCharacterInst* lpCI = lpNode->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return nullptr;
        AptDisplayListState* lpKids =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        if (lpKids == nullptr)
            return nullptr;
        for (AptCIH* lpK = lpKids->mpFirst; lpK != nullptr; lpK = lpK->GetDisplayListNext())
        {
            AptCharacterInst* lpKCI = lpK->GetCharacterInst();
            if (lpKCI != nullptr && (lpKCI->GetTypeTag() == 5 || lpKCI->GetTypeTag() == 9))
            {
                const EAStringC& lrName = lpK->GetInstanceName();
                const char* lpcName = lrName.GetBuffer();
                if (lpcName != nullptr && _stricmp(lpcName, lpacName) == 0)
                    return lpK;
            }
            AptCIH* lpFound = AptFindClipByName(lpK, lpacName, liDepth + 1);
            if (lpFound != nullptr)
                return lpFound;
        }
        return nullptr;
    }

    // Find the clip subtree's dynamic-text field (char-inst type tag 2), preferring the
    // one PLACE-named 'label' (the B5MenuItem labelHolder->label field); else the first.
    static AptCIH* AptFindTextFieldIn(AptCIH* lpNode, int liDepth)
    {
        if (lpNode == nullptr || liDepth > 6)
            return nullptr;
        AptCharacterInst* lpCI = lpNode->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return nullptr;
        AptDisplayListState* lpKids =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        if (lpKids == nullptr)
            return nullptr;
        AptCIH* lpFirstText = nullptr;
        for (AptCIH* lpK = lpKids->mpFirst; lpK != nullptr; lpK = lpK->GetDisplayListNext())
        {
            AptCharacterInst* lpKCI = lpK->GetCharacterInst();
            if (lpKCI == nullptr)
                continue;
            if (lpKCI->GetTypeTag() == 2)
            {
                const EAStringC& lrName = lpK->GetInstanceName();
                const char* lpcName = lrName.GetBuffer();
                if (lpcName != nullptr && _stricmp(lpcName, "label") == 0)
                    return lpK;
                if (lpFirstText == nullptr)
                    lpFirstText = lpK;
                continue;
            }
            AptCIH* lpFound = AptFindTextFieldIn(lpK, liDepth + 1);
            if (lpFound != nullptr)
            {
                const EAStringC& lrName = lpFound->GetInstanceName();
                const char* lpcName = lrName.GetBuffer();
                if (lpcName != nullptr && _stricmp(lpcName, "label") == 0)
                    return lpFound;
                if (lpFirstText == nullptr)
                    lpFirstText = lpFound;
            }
        }
        return lpFirstText;
    }

    // The title help-prompt defaults (see the FLAG at the SelectionMenu transin apply):
    // find the StaticHelpItem instance inside the menu -- the clip whose DIRECT children
    // include a dynamic-text 'TextField' -- set the authored prompt text, and jump its
    // 'Icon' (the ControllerButtons import) to the authored 'select' glyph state.
    static bool AptApplyTitleHelpItemDefaults(AptCIH* lpMenuClip)
    {
        // depth-first: the StaticHelpItem lives one level under SelectionMenu_mc.
        struct Local
        {
            static AptCIH* FindHelpItem(AptCIH* lpNode, int liDepth)
            {
                if (lpNode == nullptr || liDepth > 5)
                    return nullptr;
                AptCharacterInst* lpCI = lpNode->GetCharacterInst();
                if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
                    return nullptr;
                AptDisplayListState* lpKids =
                    static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
                if (lpKids == nullptr)
                    return nullptr;
                for (AptCIH* lpK = lpKids->mpFirst; lpK != nullptr; lpK = lpK->GetDisplayListNext())
                {
                    AptCharacterInst* lpKCI = lpK->GetCharacterInst();
                    const EAStringC& lrName = lpK->GetInstanceName();
                    const char* lpcName = lrName.GetBuffer();
                    if (lpKCI != nullptr && lpKCI->GetTypeTag() == 2 &&
                        lpcName != nullptr && _stricmp(lpcName, "TextField") == 0)
                        return lpNode;   // the PARENT is the help-item state clip
                    AptCIH* lpFound = FindHelpItem(lpK, liDepth + 1);
                    if (lpFound != nullptr)
                        return lpFound;
                }
                return nullptr;
            }
        };

        AptCIH* lpHelp = Local::FindHelpItem(lpMenuClip, 0);
        if (lpHelp == nullptr)
            return false;   // not composed yet -- the per-update driver retries

        // The prompt text: '$CAPS_BUTTON_SELECT' (localised by the text pipeline). Applied
        // to EVERY dynamic-text field in the help-item subtree: the PERSISTENTAPT-carried
        // B5HelpItem places its prompt field behind a wrapper clip, so targeting only the
        // first-found field leaves the RENDERED one at its authored placeholder.
        struct LocalText
        {
            static void SetAll(AptCIH* lpNode, int liDepth)
            {
                if (lpNode == nullptr || liDepth > 5)
                    return;
                AptCharacterInst* lpCI = lpNode->GetCharacterInst();
                if (lpCI == nullptr)
                    return;
                if (lpCI->GetTypeTag() == 2)
                {
                    AptCharacterTextInst* lpTextInst =
                        static_cast<AptCharacterTextInst*>(lpCI);
                    EAStringC lValue("$CAPS_BUTTON_SELECT");
                    lpTextInst->SetTextValue(lValue);
                    lpTextInst->ClearStateFlags(1u);
                    return;
                }
                if (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9)
                    return;
                AptDisplayListState* lpKids =
                    static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
                for (AptCIH* lpK = lpKids ? lpKids->mpFirst : nullptr; lpK != nullptr;
                     lpK = lpK->GetDisplayListNext())
                    SetAll(lpK, liDepth + 1);
            }
        };
        AptCIH* lpText = AptFindTextFieldIn(lpHelp, 0);
        if (lpText != nullptr)
            LocalText::SetAll(lpHelp, 0);

        // The button glyph: the 'Icon' child -> the ControllerButtons 'select' state.
        AptCharacterInst* lpHCI = lpHelp->GetCharacterInst();
        AptDisplayListState* lpHKids = (lpHCI != nullptr)
            ? static_cast<AptCharacterSpriteInstBase*>(lpHCI)->mDisplayList.AsState() : nullptr;
        bool lbIcon = false;
        for (AptCIH* lpK = lpHKids ? lpHKids->mpFirst : nullptr; lpK != nullptr;
             lpK = lpK->GetDisplayListNext())
        {
            const EAStringC& lrName = lpK->GetInstanceName();
            const char* lpcName = lrName.GetBuffer();
            if (lpcName != nullptr && _stricmp(lpcName, "Icon") == 0)
            {
                lbIcon = AptViewStateGotoLabel(lpK, "select", 0);
                if (!lbIcon)
                {
                    // The Icon is the imported ControllerButtons PLATFORM wrapper
                    // ('xbox' f0 / 'ps3' f10 / 'invisible' f20). In this Nov-06 dev
                    // asset only the 'ps3' band PLACES the button board (the 'xbox'
                    // band is empty -- the console's component ActionScript attaches
                    // the platform board; that script is the AS-VM follow-on, FLAG).
                    // Arm the board's composition by jumping the wrapper to its only
                    // art-bearing band; the per-update retry then lands the 'select'
                    // jump once the board has placed. (Before the GUIAPT64 record
                    // repair this happened BY ACCIDENT: the wrapper's broken f0 Stop
                    // let it free-run into the 'ps3' band.)
                    AptViewStateGotoLabel(lpK, "ps3", 0);
                }
                break;
            }
        }
        if (lbIcon)
        {
            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] helpitem: prompt text set (%s), icon 'select' APPLIED.\n",
                lpText ? "ok" : "NO FIELD");
            CgsDev::Log::WriteToLog(lac);
        }
        return lbIcon;
    }

    // FLAG title-bring-up scaffolding (Phase 4d retires this whole cluster): AptRuntimeSetComponentViewState
    // / AptRuntimeSetComponentKeyValue / AptApplyTitleHelpItemDefaults reproduce the OBSERVABLE of the
    // un-run GuiComponent ActionScript (view-state transitions) via a hardcoded, title-screen-scoped
    // component->clip pair map -- this is behavioral scaffolding, NOT faithful decompiled code. Kept
    // (user decision 2026-07-04) so the title keeps animating; delete the whole cluster when the faithful
    // GuiComponent::FillAptViewMessage -> AptCommunicator AS routing is homed (Phase 4d).
    static bool AptRuntimeApplyComponentViewState(const char* lpacInstName, const char* lpacViewState)
    {
        char lac[224];
        if (lpacInstName == nullptr || lpacViewState == nullptr || !IsRuntimeMovieLive())
            return false;

        // The AnimatorComponent instances are CONTAINERS (the imported TransitionComponent);
        // the transition TIMELINE (transin/transout/... labels) lives in a PAIRED sibling
        // clip that the component's ActionScript drives. The AS pairing isn't running yet
        // (the communicator glue is the faithful follow-on), so pair them here explicitly
        // for the title screen (FLAG: title-screen-scoped table).
        struct PairMap { const char* pcComponent; const char* pcTargetClip; };
        static const PairMap KA_PAIRS[] =
        {
            { "HDCompAnimator_mc",             "HDComp_mc"       },
            { "esrb_anim",                     "esrb_mc"         },
            { "StartMessageAnimatorComponent", "ButtStart_mc"    },
            { "BackgroundAnimatorComponent",   "background_mc"   },
            { "SelectionMenuAnimatorComponent","SelectionMenu_mc"},
        };
        const char* lpcTarget = lpacInstName;
        for (u32 lu = 0; lu < sizeof(KA_PAIRS) / sizeof(KA_PAIRS[0]); ++lu)
        {
            if (_stricmp(KA_PAIRS[lu].pcComponent, lpacInstName) == 0)
            {
                lpcTarget = KA_PAIRS[lu].pcTargetClip;
                break;
            }
        }

        AptCIH* lpRoot = reinterpret_cast<AptCIH*>(s_FlowSlot.mpRootCIH);
        AptCharacterInst* lpRootCI = lpRoot->GetCharacterInst();
        if (lpRootCI == nullptr)
            return false;
        AptCharacterSpriteInstBase* lpRootSprite =
            static_cast<AptCharacterSpriteInstBase*>(lpRootCI);
        AptDisplayListState* lpState = lpRootSprite->mDisplayList.AsState();
        if (lpState == nullptr)
            return false;

        // Search the FLOW movie's display tree FIRST (the title clips live there), then
        // fall back to the FRAMEWORK movie's (a component clip exported by MAIN).
        AptCIH* lpClip = AptFindClipByName(lpRoot, lpcTarget, 0);
        if (lpClip == nullptr &&
            s_FrameworkSlot.mbInstantiated && s_FrameworkSlot.mpRootCIH != nullptr)
            lpClip = AptFindClipByName(
                static_cast<AptCIH*>(s_FrameworkSlot.mpRootCIH), lpcTarget, 0);
        if (lpClip != nullptr)
        {
            const bool lbApplied = AptViewStateGotoLabel(lpClip, lpacViewState, 0);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] viewstate: '%s' -> '%s' on clip '%s' (%s)\n",
                lpacInstName, lpacViewState, lpcTarget,
                lbApplied ? "APPLIED" : "no label in subtree (FLAG)");
            CgsDev::Log::WriteToLog(lac);

            // FLAG (title-scoped stand-in for the StaticHelpItem AS initialize): the
            // selection menu's help prompt is authored via the placed instance's clip-
            // action params (Title_Screen02 stream @0x6ff4: mText='$CAPS_BUTTON_SELECT',
            // mPromptType='select', mAlignment='IconOnLeft'). The StaticHelpItem class
            // AS (MAIN.bundle, not running) would apply them; reproduce the observable
            // once when the menu transitions in: set the prompt TextField's text (the
            // localised 'SELECT') and jump the Icon (the ControllerButtons import) to
            // its 'select' glyph state.
            if (lbApplied && _stricmp(lpcTarget, "SelectionMenu_mc") == 0)
                s_bHelpDefaultsPending = true;   // applied by the per-update driver once the
                                                 // icon subtree has composed (retried)
            return lbApplied;
        }

        std::snprintf(lac, sizeof(lac),
            "[AptRT] viewstate: '%s': target clip '%s' not found in the movie (FLAG).\n",
            lpacInstName, lpcTarget);
        CgsDev::Log::WriteToLog(lac);
        return false;
    }

    // -------------------------------------------------------------------------
    // AptRuntimeApplyComponentKeyValue -- the faithful KEY dispatch of the GuiComponent
    // apt-view protocol (GuiComponent::AddOutputAptViewState(key, value) -> FillAptView
    // Message -> AptAux::UpdateComponents -> AptCommunicator key-values -> the movie AS).
    // The AS framework movie (MAIN.bundle's gAptCommunicator/BurnoutComponent classes) is
    // not running yet, so this bridge reproduces each key's OBSERVABLE effect on the
    // component's clip, exactly as the AS would:
    //   apt_Transition  (TransitionComponent)   -> gotoAndPlay(value) on the PAIRED clip;
    //   apt_state       (B5MenuItem/ButtonIcon) -> gotoAndPlay(value) on the clip itself
    //                    (the item timelines carry the Selected/Unselected/Disabled/
    //                     Invisible state labels);
    //   apt_labeltxt    (B5MenuItem)            -> set the clip's nested dynamic-text
    //                    field ('labelHolder'->'label') through the faithful setters
    //                    (AptCharacterTextInst::SetTextValue + ClearStateFlags bit0);
    //                    the invalidate makes ProcessTextInst re-lay it out and
    //                    CgsAptString::Prepare localises the '$KEY' string;
    //   apt_updatestate (B5MenuItem)            -> no-op (the AS Update trigger; the
    //                    invalidate above already re-lays out).
    // -------------------------------------------------------------------------
    static bool AptRuntimeApplyComponentKeyValue(const char* lpacInstName, const char* lpacKey,
                                                 const char* lpacValue)
    {
        if (lpacInstName == nullptr || lpacKey == nullptr || lpacValue == nullptr)
            return false;

        if (_stricmp(lpacKey, "apt_Transition") == 0)
            return AptRuntimeApplyComponentViewState(lpacInstName, lpacValue);

        if (!IsRuntimeMovieLive())
            return false;

        char lac[224];
        AptCIH* lpRoot = reinterpret_cast<AptCIH*>(s_FlowSlot.mpRootCIH);

        if (_stricmp(lpacKey, "apt_updatestate") == 0)
            return true;   // the AS Update trigger; the labeltxt/state posts already applied

        // FLOW movie first (the title clips live there), then the FRAMEWORK movie's tree.
        AptCIH* lpClip = AptFindClipByName(lpRoot, lpacInstName, 0);
        if (lpClip == nullptr &&
            s_FrameworkSlot.mbInstantiated && s_FrameworkSlot.mpRootCIH != nullptr)
            lpClip = AptFindClipByName(
                static_cast<AptCIH*>(s_FrameworkSlot.mpRootCIH), lpacInstName, 0);
        if (lpClip == nullptr)
        {
            std::snprintf(lac, sizeof(lac),
                "[AptRT] kv: '%s' %s='%s': clip not found (FLAG).\n",
                lpacInstName, lpacKey, lpacValue);
            CgsDev::Log::WriteToLog(lac);
            return false;
        }

        if (_stricmp(lpacKey, "apt_state") == 0)
        {
            const bool lbApplied = AptViewStateGotoLabel(lpClip, lpacValue, 0);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] kv: '%s' apt_state='%s' (%s)\n",
                lpacInstName, lpacValue, lbApplied ? "APPLIED" : "no label (FLAG)");
            CgsDev::Log::WriteToLog(lac);
            return lbApplied;
        }

        if (_stricmp(lpacKey, "apt_labeltxt") == 0)
        {
            AptCIH* lpText = AptFindTextFieldIn(lpClip, 0);
            if (lpText == nullptr)
            {
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] kv: '%s' apt_labeltxt: no dynamic-text field in the clip subtree (FLAG).\n",
                    lpacInstName);
                CgsDev::Log::WriteToLog(lac);
                return false;
            }
            AptCharacterTextInst* lpTextInst =
                static_cast<AptCharacterTextInst*>(lpText->GetCharacterInst());
            EAStringC lValue(lpacValue);
            lpTextInst->SetTextValue(lValue);      // @0x82AE66C0 (the AS `label.text =` store)

            // ---- the AS-autofit observable (B5MenuItem::Update: shrink the label font
            // until the text fits -- `fmt.size = mnFontSize; while (_width > mnMaxWidth &&
            // size > mnMinFontSize) --size`). Reproduced with the engine's own faithful
            // shrink math (TextObject::CalculateAutosizing @0x827EEF58: keep when the box
            // holds the span, else scale by boxW/(span+5), floored) against the field's
            // AUTHORED box + font size, measuring the LOCALISED string with the body
            // typeface (B5EAConDisS -- the menu-label font; title-scoped FLAG).
            {
                AptRenderItemDynamicText* lpItem = static_cast<AptRenderItemDynamicText*>(
                    lpText->GetCharacterInst()->GetRenderItem());

                // The AUTHORED font size: cached on first touch (later posts re-fit from
                // the original, not from an already-shrunk size).
                struct AuthoredSize { void* pField; float fSize; };
                static AuthoredSize s_aAuthored[8] = {};
                float lfAuthored = lpItem->mFontSize;
                for (u32 luA = 0; luA < 8; ++luA)
                {
                    if (s_aAuthored[luA].pField == (void*)lpText)
                    { lfAuthored = s_aAuthored[luA].fSize; break; }
                    if (s_aAuthored[luA].pField == nullptr)
                    { s_aAuthored[luA].pField = (void*)lpText; s_aAuthored[luA].fSize = lfAuthored; break; }
                }

                const char* lpcResolved = lpacValue;
                if (lpacValue[0] == '$' || lpacValue[0] == '~')
                {
                    const u8* lpLoc = s_pViewModule->GetLanguageManager()->FindString(lpacValue + 1);
                    if (lpLoc != nullptr)
                        lpcResolved = reinterpret_cast<const char*>(lpLoc);
                }
                if (s_pAptBodyFont != nullptr)
                {
                    const f32 lfBoxW = lpItem->mBounds.fRight - lpItem->mBounds.fLeft;
                    const f32 lfWidthEm = s_pAptBodyFont->GetStringWidth(
                        reinterpret_cast<const CgsResource::CgsUtf8*>(lpcResolved));
                    const f32 lfSpan = lfWidthEm * lfAuthored;
                    f32 lfFit = lfAuthored;
                    if (lfBoxW > 0.0f && lfSpan > lfBoxW)
                    {
                        lfFit = lfAuthored / ((lfSpan + 5.0f) / lfBoxW);
                        if (lfFit < 15.0f)
                            lfFit = 15.0f;
                    }
                    lpTextInst->SetFontSize(lfFit);   // @0x82AE2020 (the AS fmt.size store)
                }
            }

            lpTextInst->ClearStateFlags(1u);       // invalidate -> ProcessTextInst re-lays out
            std::snprintf(lac, sizeof(lac),
                "[AptRT] kv: '%s' apt_labeltxt='%s' (text field %p invalidated)\n",
                lpacInstName, lpacValue, (void*)lpText);
            CgsDev::Log::WriteToLog(lac);
            return true;
        }

        std::snprintf(lac, sizeof(lac),
            "[AptRT] kv: '%s' %s='%s' (unhandled key -- FLAG).\n",
            lpacInstName, lpacKey, lpacValue);
        CgsDev::Log::WriteToLog(lac);
        return false;
    }

    bool AptRuntimeHost::Prepare(CgsGui::ViewModule* lpViewModule)
    {
        mpViewModule = lpViewModule;
        s_pViewModule = lpViewModule;
        return PrepareRuntime();
    }

    bool AptRuntimeHost::Prepare()
    {
        s_pViewModule = mpViewModule;
        return PrepareRuntime();
    }

    void AptRuntimeHost::PlayMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        PlayRuntimeMovie(lpacMovieName, liLevelNum);
    }

    void AptRuntimeHost::UpdateShimResidue()
    {
        ShimResidueUpdate();
    }

    CgsGui::AptIm2dRenderBuffer* AptRuntimeHost::GetAptRenderBuffer() const
    {
        return s_bRenderBufferReady ? &s_AptRenderBuffer : nullptr;
    }

    void* AptRuntimeHost::Get3dRendererAssertSatisfier() const
    {
        return &s_i3dRendererSentinel;
    }

    void AptRuntimeHost::DispatchRenderResidue()
    {
        DispatchRenderBuffer();
    }

    bool AptRuntimeHost::PopPendingLoadNotification(CgsGui::GuiEventLoadNotification* lpOut)
    {
        if (s_uPendingLoadNotificationRead == s_uPendingLoadNotificationWrite)
            return false;
        *lpOut = s_aPendingLoadNotifications[
            s_uPendingLoadNotificationRead % KU_MAX_PENDING_LOAD_NOTIFICATIONS];
        ++s_uPendingLoadNotificationRead;
        return true;
    }

    void AptRuntimeHost::StopMovie()
    {
        StopRuntimeMovie();
    }

    bool AptRuntimeHost::IsReady() const
    {
        return IsRuntimeReady();
    }

    bool AptRuntimeHost::IsMovieLive() const
    {
        return IsRuntimeMovieLive();
    }

    bool AptRuntimeHost::IsMovieComposed() const
    {
        return IsRuntimeMovieComposed();
    }

    bool AptRuntimeHost::SetComponentViewState(const char* lpacInstName, const char* lpacViewState)
    {
        return AptRuntimeApplyComponentViewState(lpacInstName, lpacViewState);
    }

    bool AptRuntimeHost::SetComponentKeyValue(const char* lpacInstName, const char* lpacKey,
                                              const char* lpacValue)
    {
        return AptRuntimeApplyComponentKeyValue(lpacInstName, lpacKey, lpacValue);
    }

}

// =============================================================================
// AptLoader_StartAsyncLoad (dword_8324E838) -- the platform "kick off the .apt stream" hook the
// faithful AptLoader::Update calls on the state 1->2 (requested -> loading) transition. On the
// console this starts an ASYNC stream whose completion posts AptCompleteAnimationAsyncLoad; PC has
// no such stream, so it is HOMED here to load the import bundle SYNCHRONOUSLY + drive that exact
// completion inline (the import AptFile ends at mnState == 3, resolved). This is the ONE genuinely
// platform-specific piece of the import content-load: the loader STATE MACHINE (AptLoader::Update)
// + the completion (AptCompleteAnimationAsyncLoad -> CompleteLoad -> Resolve -> Fixup) + the link
// (AptCharacterAnimation_Link -> FindExport) are all the faithful console flow.
//
// This STRONG definition overrides the FLAG link-stub previously in AptRenderLinkStubs.cpp (removed).
// pFile->mFileName is the movie name AptLoader::Update passes as pFileName (== the import to load).
// FLAG PC-platform leaf: PC has no async .apt stream, so this loads the import SYNCHRONOUSLY. This is
// a permanent platform leaf (not a stand-in to retire) -- the completion (CompleteLoad -> Resolve ->
// Fixup), the AptCharacterAnimation_Link -> FindExport link, and the AptLoader::Update state machine
// it drives are all the faithful console flow.
// =============================================================================
// =============================================================================
// AptLoadAnimation -- the engine "load a movie onto a target path" public entry
// (CgsGui::AptAux::LoadFlashAnimation @0x82849080 calls it with the "_level%d"
// path). Its X360 body has no per-address export in the dump set, so this host
// definition is the PC stand-in: parse the level index back out of the target
// path and drive the host movie-load machinery (the same load the engine body
// kicks through the loader + the pfnLoadAnimation host callback). Replace with
// the faithful engine body once it is exported + reconstructed.
// FLAG PC-platform leaf: host stand-in for the un-exported engine load entry.
// =============================================================================
// =============================================================================
// AptLoadAnimation -- the engine "load a movie onto a target path" public entry
// (CgsGui::AptAux::LoadFlashAnimation @0x82849080 calls it with the "_level%d"
// path). Its X360 body has no per-address export in the dump set, so this host
// definition is the PC stand-in: parse the level index back out of the target
// path and drive the host movie-load machinery (the same load the engine body
// kicks through the loader + the pfnLoadAnimation host callback). Replace with
// the faithful engine body once it is exported + reconstructed.
// FLAG PC-platform leaf: host stand-in for the un-exported engine load entry.
// =============================================================================
// TEMP-DIAG (delete before commit)
void AptUpdateTempDiagChain(AptCIH* pFirst)
{
    char lac[512];
    int n = std::snprintf(lac, sizeof(lac), "[AptRT] TEMPDIAG chain:");
    AptCIH* p = pFirst;
    for (int i = 0; i < 10 && p != nullptr && n < 460; ++i)
    {
        n += std::snprintf(lac + n, sizeof(lac) - n, " %p(d%d,f%08X)",
                           (void*)p,
                           p->GetCharacterInst() ? p->GetCharacterInst()->GetRenderItem()->GetDepth() : -1,
                           p->mFlagsA);
        p = p->GetDisplayListNext();
    }
    std::snprintf(lac + n, sizeof(lac) - n, p ? " ...MORE\n" : " <end>\n");
    CgsDev::Log::WriteToLog(lac);
}

int AptLoadAnimation(const char* pName, const char* pTargetPath)
{
    int liLevel = 0;
    if (pTargetPath != nullptr)
        std::sscanf(pTargetPath, "_level%d", &liLevel);
    if (BrnGui::gpActiveAptRuntimeHost != nullptr)
        BrnGui::gpActiveAptRuntimeHost->PlayMovie(pName, liLevel);
    return 1;
}

void AptLoader_StartAsyncLoad(const char* pFileName, AptFilePtr* pFile)
{
    // Re-entrancy guard: LoadImportBundle drives AptCompleteAnimationAsyncLoad which does NOT
    // re-enter Update, but a nested import's own Fixup pass-3 could register further imports; those
    // are picked up by the driving Update's outer re-pass, so a single synchronous load here is
    // safe. The guard just prevents an accidental recursive StartAsyncLoad on the same in-flight file.
    if (s_bImportReentryGuard)
        return;
    s_bImportReentryGuard = true;
    BrnGui::LoadImportBundle(pFileName, pFile);
    s_bImportReentryGuard = false;
}
