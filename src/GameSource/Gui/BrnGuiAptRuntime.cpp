#include "GameSource/Gui/BrnGuiAptRuntime.h"

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
    // AptAux::Prepare drive (step 5).
    unsigned char                s_aAptDataHeap[256 * 1024];
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

    // ====================================================================================
    // STEP 3 -- IMPORT CONTENT-LOAD (the cross-bundle imports the title's visible content lives in).
    //
    // TITLE_SCREEN02's 7 placed chars are type-5 SPRITE CONTAINERS whose content is 100% in 5
    // IMPORT bundles (B5MenuItem / B5HelperComponents / B5ControllerButtons / B5HelpItem). The
    // parent movie's Fixup pass-3 REGISTERS each import with the loader (AptLoader::Load -> a
    // "requested" AptFile, mnState==1) but does NOT stream its data -- on the console that is the
    // async .apt stream kicked off by AptLoader::Update state 1->2 (AptLoaderStartAsyncLoad), whose
    // completion (AptCompleteAnimationAsyncLoad -> CompleteLoad -> Resolve -> Fixup) fills the import
    // in. PC has no async stream, so AptLoaderStartAsyncLoad is HOMED below to load the import bundle
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
    // (The old x64 byte-pattern root locator is RETIRED (2026-07-09):
    // AptLoader::CompleteLoad's faithful native-8 root location -- the const chunk's
    // movieOffset @const+0x18, the XB1-attested formula -- covers every bundle, so the
    // locator and its gAptLoadAnimRootOverride poke are gone. The DumpResourceBytes hex
    // probes went with them.)
    // STEP 3 (import content-load): synchronously load the import bundle named `lpacMovieName`
    // (GuiApt\<NAME>.bundle) into a resident pool, register its AptDataHeader with the data handler,
    // and drive AptCompleteAnimationAsyncLoad on `lpFile` so the import's AptFile is fully
    // loaded+resolved (mpData = root, mnState = 3). Returns true on success (idempotent: a second
    // call for the same name is a no-op that still completes the handle). Called by the homed
    // AptLoaderStartAsyncLoad (the platform stream hook).
    static bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile);
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
    // registered-data/bundle-IO completion below (LoadImportBundle) lives on as the
    // body of the real AptLoaderStartAsyncLoad platform hook.)

    // =========================================================================
    // LoadImportBundle -- content-load ONE import movie by name and drive its AptFile
    // through the faithful completion, the PC substitute for the console's async .apt
    // stream (the stream AptLoaderStartAsyncLoad kicks off).
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
        for (u32 lu = 0; lu < s_uImportBundleCount; ++lu)
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

            ++s_uImportBundleCount;
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
    static void DispatchRenderBuffer()
    {
        if (!s_bRuntimeReady || !s_bAuxReady || !s_bRenderBufferReady)
            return;

        // The flush body is re-homed to the PC backend TU (slice 5 step A): the
        // Swap -> Clear -> Dispatch consumption + its one-shot probe live in
        // GameShared/GameClasses/Gui/PC/CgsAptRenderBackendPC.cpp.
        CgsGui::DispatchAptIm2dRenderBufferPC(&s_AptRenderBuffer);
        ++s_iRenderFrame;
    }

    // Non-creating probe for the engine node mounted at a display level: walk the
    // director's root display list (the same search AptGetAnimationAtLevel opens
    // with) WITHOUT the lazy-create tail -- a liveness QUERY must not mint nodes.
    static AptCIH* FindMountedLevelNode(s32 liLevel)
    {
        if (gpAptTarget == nullptr || gpAptTarget->mpAnimationTarget == nullptr)
            return nullptr;
        AptDisplayList* lpRoot = gpAptTarget->mpAnimationTarget->GetRootDisplayList();
        AptDisplayListState* lpState = (lpRoot != nullptr) ? lpRoot->AsState() : nullptr;
        if (lpState == nullptr)
            return nullptr;
        for (AptCIH* lpNode = lpState->mpFirst; lpNode != nullptr;
             lpNode = lpNode->mpDisplayListNext)
        {
            if (lpNode->mpCharacterInst == nullptr ||
                lpNode->mpCharacterInst->mpRenderItem == nullptr)
                continue;
            if (lpNode->mpCharacterInst->mpRenderItem->GetDepth() == liLevel)
                return lpNode;
        }
        return nullptr;
    }

    static bool IsRuntimeMovieLive()
    {
        // FLOW semantics: BootLegal drives this query off its channel-41 movie at
        // display level 1. Engine-native since the AptLoadAnimation retirement: the
        // linker mounts the flow movie's anim inst onto the level-1 node -- live ==
        // that node exists with a bound character inst (SetCharacterInst ran).
        AptCIH* lpNode = FindMountedLevelNode(1);
        return lpNode != nullptr && lpNode->GetCharacterInst() != nullptr;
    }

    // True once the movie has COMPOSED: the root clip's first paced tick has run its
    // frame-0 place commands, so the child display list is populated (the PLACE-named
    // clips the view-state bridge targets exist). The console's equivalent gate is the
    // GuiCache apt-component handshake (AreAllAptComponentsInitialised): a component
    // reports initialised only once its clip is placed.
    static bool IsRuntimeMovieComposed()
    {
        // FLOW semantics (BootLegal's compose gate): composed == the mounted level-1
        // movie's first paced tick ran its frame-0 place commands (child display list
        // non-empty). Engine-native read of the same node IsRuntimeMovieLive probes.
        AptCIH* lpRoot = FindMountedLevelNode(1);
        if (lpRoot == nullptr)
            return false;
        AptCharacterInst* lpCI = lpRoot->GetCharacterInst();
        if (lpCI == nullptr || (lpCI->GetTypeTag() != 5 && lpCI->GetTypeTag() != 9))
            return false;
        AptDisplayListState* lpState =
            static_cast<AptCharacterSpriteInstBase*>(lpCI)->mDisplayList.AsState();
        return lpState != nullptr && lpState->mpFirst != nullptr;
    }


    bool AptRuntimeHost::Prepare(CgsGui::ViewModule* lpViewModule,
                                 CgsMemory::LinearMalloc* lpFlaptLinear)
    {
        mpViewModule = lpViewModule;
        s_pViewModule = lpViewModule;
        s_pFlaptLinear = lpFlaptLinear;
        return PrepareRuntime();
    }

    bool AptRuntimeHost::Prepare()
    {
        s_pViewModule = mpViewModule;
        return PrepareRuntime();
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


}

// (AptLoadAnimation's PC stand-in RETIRED 2026-07-16: the faithful engine body
// @0x82B07AC8 -- .swf strip + bg-latch reset + gpAptTarget->mpLinker->Load -- is
// homed at its real SDK home, SDKs/EATech/Apt/Apt.cpp. The play path is now
// engine-native: channel-41 event 18 -> ViewModule -> AptAux::LoadFlashAnimation
// -> AptLoadAnimation -> AptLinker::Load; AptUpdate's per-frame mpLinker->Update
// mounts the completed file at its "_level%d" target.)

// =============================================================================
// AptLoaderStartAsyncLoad (dword_8324E838) -- the platform "kick off the .apt stream" hook the
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
void AptLoaderStartAsyncLoad(const char* pFileName, AptFilePtr* pFile)
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
