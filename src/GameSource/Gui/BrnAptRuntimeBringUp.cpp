#include "GameSource/Gui/BrnAptRuntimeBringUp.h"

#include <cstdio>    // std::snprintf (probe logging)
#include <cstring>   // std::strncpy

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog

// ---- the Apt host adaptor + render handler (the render bridge) --------------
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptAux.h"          // CgsGui::AptAux / AptAuxPointer
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"// CgsGui::AptImRendererSet / AptIm2dRenderBuffer
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

// ---- STEP 1 FAITHFUL INSTANTIATION (gated; the direct-geometry render stays the fallback) ----
#include "SDKs/EATech/include/Apt/AptCharacterAnimation.h"    // AptCharacterAnimation::Fixup (FixupTranscode/InPlace)
#include "SDKs/EATech/include/Apt/AptConstFile.h"             // AptConstFile (pointer-size dispatch)
#include "SDKs/EATech/include/Apt/AptFile.h"                  // AptFile (synthesised loaded-file handle for instantiation)
#include "SDKs/EATech/include/Apt/AptCharacterHelper.h"       // AptGetAnimationAtLevel (the root level-0 CIH)
#include "SDKs/EATech/include/Apt/AptCharacterAnimationInst.h"// MakeCharacterAnimationInst (loaded movie -> live inst)
#include "SDKs/EATech/include/Apt/AptCIH.h"                   // AptCIH (the root display-object node)
#include "SDKs/EATech/include/Apt/AptMovie.h"                 // AptMovie::doFrameControls / resolve (timeline driver)
#include "SDKs/EATech/include/Apt/AptCharacterSpriteInstBase.h" // mDisplayList (the root CIH's child display list)
#include "SDKs/EATech/include/Apt/AptAnimationTarget.h"       // AptAnimationTarget::GetRootDisplayList (the director's root list)
#include "SDKs/EATech/include/Apt/AptDisplayList.h"           // AptDisplayList::AsState
#include "SDKs/EATech/include/Apt/AptDisplayListState.h"      // AptDisplayListState::mpFirst (the placed-node chain)
#include "SDKs/EATech/include/Apt/AptCharacterInst.h"         // AptCharacterInst::GetRenderItem/GetDepth (probe)
#include "SDKs/EATech/include/Apt/AptRenderWalk.h"            // AptRender (the faithful render-tree flush)


// The pass-3 import-registration probe sink (declared weak in AptCharacterAnimation.cpp). Logs each
// import the movie references (movie name / class name) + the loader handle Load() returned, so a run
// names the exact cross-bundle imports (e.g. charId 30 = B5HelperComponents) that must be sync-loaded.
extern "C" void CgsApt_ImportProbe(int nIndex, const char* pcMovieName, const char* pcClassName, void* pHandle)
{
    char lac[224];
    std::snprintf(lac, sizeof(lac), "[AptRT] import[%d]: movie='%s' class='%s' handle=%p\n",
                  nIndex, pcMovieName ? pcMovieName : "?", pcClassName ? pcClassName : "?", pHandle);
    CgsDev::Log::WriteToLog(lac);
}

// The per-character FixupWalk probe sink the engine TU calls (declared in AptCharacterAnimation.h).
// Logs "[AptRT] fixup: char[i] type=T @off=.." for the first N characters so a run names the exact
// character index + type + record offset reached before any (now-guarded) skip.
void CgsApt_FixupProbe(int nCharIndex, int nType, long long nCharOffset)
{
    char lac[96];
    std::snprintf(lac, sizeof(lac), "[AptRT] fixup: char[%d] type=%d @off=0x%llX\n",
                  nCharIndex, nType, static_cast<unsigned long long>(nCharOffset));
    CgsDev::Log::WriteToLog(lac);
}

// The AptGetAnimationAtLevel step-probe sink (declared in AptCharacterHelper.cpp). Names each step +
// pointer so a single run shows the exact line reached before any AV inside the homed function. This
// strong definition overrides the weak CgsApt_GalProbeDefault no-op in that TU.
extern "C" void CgsApt_GalProbe(const char* pcStep, const void* p)
{
    char lac[128];
    std::snprintf(lac, sizeof(lac), "[AptRT] gal: %s = %p\n", pcStep ? pcStep : "?", p);
    CgsDev::Log::WriteToLog(lac);
}

// The per-tick step-probe sink (declared in AptCIH.cpp). Throttled to the first ~120 calls (a handful
// of ticks) so it pinpoints the early-deref AV without flooding the per-frame log. Strong def overrides
// the weak default in AptCIH.cpp.
extern "C" void CgsApt_TickProbe(const char* pcStep, const void* p)
{
    static int s_iTickProbeBudget = 0;
    if (s_iTickProbeBudget >= 120)
        return;
    ++s_iTickProbeBudget;
    char lac[128];
    std::snprintf(lac, sizeof(lac), "[AptRT] tick: %s = %p\n", pcStep ? pcStep : "?", p);
    CgsDev::Log::WriteToLog(lac);
}

// The render-item-creation probe sink (declared in AptCharacterInst.cpp): logs each AptCharacterInst's
// render-item creation -- the char inst, the character's type, the character, the created render item,
// and the item's stored mpCharacter. Throttled. Confirms Manager_CreateItem ran + set mpCharacter.
extern "C" void CgsApt_MkItemProbe(const void* pCharInst, int nCharType, const void* pCharacter,
                                   const void* pRenderItem, const void* pItemCharacter)
{
    static int s_iMkItemBudget = 0;
    if (s_iMkItemBudget >= 32)
        return;
    ++s_iMkItemBudget;
    char lac[200];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] mkitem: charInst=%p type=%d character=%p -> renderItem=%p (item.mpCharacter=%p)\n",
        pCharInst, nCharType, pCharacter, pRenderItem, pItemCharacter);
    CgsDev::Log::WriteToLog(lac);
}

// The native-8 timeline-relocation probe sink (declared in AptCharacterAnimation.h): logs each embedded
// movie's relocated frameCount + frameTable + the count of command records relocated. Throttled.
void CgsApt_TimelineProbe(const void* pMovie, int nFrameCount, const void* pFrameTable, int nCmdsTotal)
{
    static int s_iTimelineBudget = 0;
    if (s_iTimelineBudget >= 32)
        return;
    ++s_iTimelineBudget;
    char lac[176];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] timeline: movie=%p frameCount=%d frameTable=%p commands=%d\n",
        pMovie, nFrameCount, pFrameTable, nCmdsTotal);
    CgsDev::Log::WriteToLog(lac);
}

// Per-frame (frame 0) timeline probe sink (declared in AptCharacterAnimation.h): the frame's command
// count + mpCommands pointer before/after relocation (offset -> in-resource pointer). Throttled.
void CgsApt_TimelineFrameProbe(int nFrame, int nCmdCount, unsigned long long luCmdsBefore,
                              unsigned long long luCmdsAfter)
{
    static int s_iTLFrameBudget = 0;
    if (s_iTLFrameBudget >= 16)
        return;
    ++s_iTLFrameBudget;
    char lac[176];
    std::snprintf(lac, sizeof(lac),
        "[AptRT] timeline: frame=%d cmdCount=%d mpCommands(before)=0x%llX (after)=0x%llX\n",
        nFrame, nCmdCount, luCmdsBefore, luCmdsAfter);
    CgsDev::Log::WriteToLog(lac);
}

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

    // ---- the host AptAux singleton ---------------------------------------------
    // The real AptAux is large (mRenderHandler alone is ~108 KB); allocate it as one
    // static object (the X360 holds a single static instance the callbacks resolve
    // through AptAuxPointer::mpAptAuxInst). Constructed in AptRuntimeBringUp.
    CgsGui::AptAux s_AptAux;

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

    // ---- the ImRendererSet handed to AptAux::Construct -------------------------
    // AptAux::Construct reinterpret_cast<AptImRendererSet*>(this set) and reads:
    //   +0x00 mpIm2dRenderer  -> our s_AptRenderBuffer  (the 2D render buffer)
    //   +0x10 mp3dRenderer    -> a non-null sentinel    (Render asserts it != 0)
    // CgsGuiModuleIO::ImRendererSet's leading bytes are an opaque 5-dword blob
    // (maRendererPtrs[20]); the AptImRendererSet view aliases the same first/4th
    // pointer slots. We build the AptImRendererSet directly and hand its address
    // (re-typed) to Construct so the aliasing is exact + obvious.
    CgsGui::AptRenderHandler::AptImRendererSet s_AptImRendererSet;

    // A non-null 3D-renderer sentinel so AptRenderHandler::Render's
    // `mpImRenderers->mp3dRenderer != 0` assert passes. The Apt boot/title movies are
    // 2D-only, so the 3D renderer is never dereferenced on this path; a sentinel keeps
    // the assert quiet without dragging in the (out-of-scope) 3D render set.
    // FLAG: sentinel only -- a real Im3dRenderBuffer is out of this slice's scope (the
    // title movie draws 2D). If a 3D Apt path is ever exercised this must become real.
    int s_i3dRendererSentinel = 0;

    // ---- the loaded movie handle (channel-41 result) ---------------------------
    char        s_acLoadedMovieName[64] = { 0 };
    bool        s_bMovieRequested        = false;   // a PlayAptMovie request is in flight (load attempted)
    bool        s_bMovieLoaded           = false;   // the movie bundle loaded + the AptDataHeader resolved
    s32         s_iFrameCounter          = 0;       // per-frame probe throttle
    s32         s_iRenderFrame           = 0;       // render-walk per-frame trace counter

    // ---- the loaded movie's resource pool + its relocated Apt geometry ---------
    // The movie bundle (GUIAPT\<NAME>.bundle) is loaded SYNCHRONOUSLY into this pool via
    // BundleLoader::LoadBundle (the same async-FS/DeviceManager path the FSM + VIDEOLIST
    // bundles use). Its single AptData resource (type 0x1E == 30) is FixUp'd by the
    // registered CgsResource::AptDataHeaderType handler into a live CgsGui::AptDataHeader,
    // whose mpGeomStruct is a relocated CgsResource::GuiGeometryObject (the renderable shape
    // geometry). We hold that object + render its files each frame through the homed
    // AptRenderHandler::Render -- the achievable render milestone (geometry -> the buffer
    // that is already Dispatched to D3D9), bypassing the un-homed engine render-tree walk.
    CgsResource::Pool*               s_pMoviePool  = nullptr;   // holds the loaded movie bundle
    CgsGui::AptDataHeader*           s_pAptHeader  = nullptr;   // the movie header (fields are RAW offsets on x64)

    // The loaded AptData resource span (the 64-bit load base + size). The faithful engine walks the
    // native-8 records against this base (AptCharacterAnimation::Fixup / AptMovie::resolve64 / the
    // Step-2 AptResolveShapeGeometry), stashed into CgsGui::gAptResourceSpanBase/Size for those.
    uintptr_t                        s_uAptResourceBase = 0;    // m_baseResources[0] (the 64-bit load base)
    u32                              s_uAptResourceSize = 0;    // the AptData resource size (relocation bound)
    bool                             s_bFlushProbed     = false;// emitted the one-shot render-flush probe yet

    // The movie pool backing (3 mem types). The title movie is ~1.6 MB; reserve 8 MB/type
    // (the bundle's main-memory resources + the heap node overhead). Static BSS storage.
    const u32 KU_MOVIE_POOL_BYTES = 8u * 1024u * 1024u;
    u8 s_moviePoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_MOVIE_POOL_BYTES];
    CgsResource::Pool s_MoviePoolStorage;

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
    // STEP 8 -- FAITHFUL LOAD-PATH STATE (gated; direct-geometry render stays the fallback).
    //
    // DriveFaithfulLoad drives the REAL Apt load path (retiring the invented signature-scan +
    // hand-rolled AptFile synthesis):
    //   AptDataHandler::AddAptData(header) -> AptLoader::Load(name) ->
    //   AptCallbackFile::LoadAnimation(name,&h) -> AptCompleteAnimationAsyncLoad ->
    //   AptLoader::CompleteLoad -> AptCharacterAnimation::Resolve -> Fixup;  then
    //   AptGetAnimationAtLevel(0) -> MakeCharacterAnimationInst(pFile) -> root->SetCharacterInst.
    //
    // Our GUIAPT bundle is the native 8-byte ("Apt Data:1:7:8") format: the Fixup relocates the
    // movie root IN PLACE at the real (high) resource address -- no low-4GB copy, no transcode
    // (8-byte slots hold full x64 addresses). The ONE FLAG'd x64 piece that remains is the
    // movie-root location: the converted bundle's AptConstFile::mnDataRootOffset does not locate
    // the type-9 root, so the host scans for the 0x09876543 signature (LocateMovieRoot8) and hands
    // CompleteLoad the root header; everything else in the chain is faithful.
    //
    // DEFENSIVE: the whole path is one-shot + heavily guarded; ANY failure leaves
    // s_bFaithfulInstantiated false and the direct-geometry render (s_bGeomResolved) intact, so the
    // game stays up + still shows the title art. The faithful per-frame TICK + RENDER are the next
    // passes (NOT this one) -- this only stands up the AptCharacterAnimation + root AptCIH tree.
    // ====================================================================================
    const bool KB_FAITHFUL_PATH_ENABLED = true;   // master gate for the faithful load path.

    AptCharacterAnimation* s_pFaithfulCharAnim = nullptr;   // the movie-root def base (post-Fixup)
    AptFile*               s_pFaithfulAptFile  = nullptr;   // the loaded AptFile handle (mpData = root)
    void*                  s_pFaithfulRootCIH  = nullptr;   // the root AptCIH placed on the director
    bool   s_bFaithfulAttempted    = false;
    bool   s_bFaithfulInstantiated = false;
    s32    s_iTickFrame            = 0;     // STEP-2 per-frame tick counter (for the placement probes)

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
    const u32 KU_MAX_IMPORT_BUNDLES  = 8u;                        // the title graph pulls ~6-8 distinct
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
    const bool KB_NESTED_DIRTY_PROPAGATION = true;

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
    // AptChangeTargetInstance (SDKs/EATech/Apt/AptInit.cpp). AptRuntimeBringUp() below drives
    // it after the (host-adaptor) render buffer + AptAux::Construct are up.
}

namespace BrnGui
{
    // Forward declarations (definitions are `static` later in this BrnGui namespace; called
    // from PlayMovie / Update which appear before their definitions). Must be in BrnGui (not
    // the anon namespace) so the in-namespace calls bind to the static definitions.
    static void DumpResourceBytes(const char* lpcTag, void* lpBase, u32 luOffset,
                                  u32 luResourceSize, u32 luBytes);
    static void DriveFaithfulLoad(CgsResource::Entry* lpEntry);
    static u32  LocateMovieRoot8(uintptr_t luBase, u32 luSize);
    // STEP 3 (import content-load): synchronously load the import bundle named `lpacMovieName`
    // (GuiApt\<NAME>.bundle) into a resident pool, register its AptDataHeader with the data handler,
    // and drive AptCompleteAnimationAsyncLoad on `lpFile` so the import's AptFile is fully
    // loaded+resolved (mpData = root, mnState = 3). Returns true on success (idempotent: a second
    // call for the same name is a no-op that still completes the handle). Called by the homed
    // AptLoader_StartAsyncLoad (the platform stream hook).
    static bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile);
    // STEP 4 (nested-content dirty propagation): recursively mark every sprite/animation node in a
    // clip's child display list dirty so the NEXT tick recurses into it + runs its doFrameControls
    // (placing its own nested shapes/images). The console propagates the dirty bit through the
    // render-tree manager on placement; this stands in for that propagation. // FLAG: propagation
    // stand-in (the render-tree-manager dirty propagation on placement is the deferred piece).
    static void PropagateDirtyToChildren(AptCIH* lpNode, int nDepth);

    bool AptRuntimeIsReady() { return s_bRuntimeReady; }

    // -------------------------------------------------------------------------
    // AptRuntimeBringUp -- the once-only host bring-up (idempotent). Mirrors the
    // X360 CgsGui::AptAux::InitializeApt @0x82848E50 + AptAllocatorInitialize
    // @0x82ADD118, but only the pieces whose engine bodies exist; every step that
    // crosses an un-homed engine routine is // FLAG'd and skipped defensively.
    // -------------------------------------------------------------------------
    bool AptRuntimeBringUp()
    {
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
            s_AptImRendererSet.mpIm2dRenderer = &s_AptRenderBuffer;     // GetIm2dRendererType() target
            s_AptImRendererSet.mpReserved04   = nullptr;
            s_AptImRendererSet.mpReserved08   = nullptr;
            s_AptImRendererSet.mpReserved0C   = nullptr;
            s_AptImRendererSet.mp3dRenderer   = &s_i3dRendererSentinel; // Render asserts != 0

            // AptAux::Construct args:
            //   ImRendererSet*       -> our AptImRendererSet (re-typed; bit-aliased, see header)
            //   TextRenderer*        -> null   FLAG: the glyph batcher is out of scope (no Apt
            //                                  static-text on the title screen is required to render
            //                                  the movie's shapes; DrawString would need it)
            //   LanguageManager*     -> null   FLAG: localised-string lookup unused for shape draw
            //   FontCollection*      -> null   FLAG: text layout unused for shape draw
            //   aspectRatio          -> 16:9 (1280/720) so the stage resolution lands at 1280x720
            //   alt-colour table     -> null / 0 entries (no alt text colours needed for shapes)
            const f32 lfAspect = 1280.0f / 720.0f;
            CgsDev::Log::WriteToLog("[AptRT] step4 aux: calling AptAux::Construct ...\n");
            s_AptAux.Construct(reinterpret_cast<CgsGuiModuleIO::ImRendererSet*>(&s_AptImRendererSet),
                               /*TextRenderer*/   nullptr,
                               /*LanguageManager*/nullptr,
                               /*FontCollection*/ nullptr,
                               lfAspect,
                               /*AlternateTextColours*/ nullptr,
                               /*NumAlternateColours*/  0);
            s_bAuxReady = (CgsGui::AptAuxPointer::mpAptAuxInst == &s_AptAux);
            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step4 aux: Construct done. singleton=%p im2d=%p (== &renderbuf %p)\n",
                (void*)CgsGui::AptAuxPointer::mpAptAuxInst,
                (void*)s_AptImRendererSet.mpIm2dRenderer, (void*)&s_AptRenderBuffer);
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
                s_AptAux.InitializeApt();

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
    // AptRuntimePlayMovie -- consume a channel-41 GuiEventPlayAptMovie. Records the
    // movie name and ATTEMPTS to load it through the homed Apt loader. Defensive:
    // bails (logs) wherever the load path crosses an un-homed piece.
    // -------------------------------------------------------------------------
    void AptRuntimePlayMovie(const char* lpacMovieName, s32 liLevelNum)
    {
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
        {
            // null / empty channel-41 name: a spurious re-post (BootLegal interleaves empty events).
            // Ignore it -- do NOT clobber the loaded-movie name or trigger a (failed) reload.
            return;
        }

        // LOAD-ONCE GUARD: the channel-41 event re-fires EVERY frame (BootLegal keeps the movie
        // playing), so without this guard the bundle would be (re)loaded thousands of times, each
        // allocating CgsResource pool entries until the pool overflows. Load each distinct movie
        // exactly once; once REQUESTED or LOADED, ignore re-fires of the same name. (Guarding on
        // s_bMovieLoaded too is essential: an interleaved empty event used to reset s_bMovieRequested,
        // so the same movie reloaded every cycle and exhausted CgsResourcePool.)
        if ((s_bMovieRequested || s_bMovieLoaded) &&
            std::strncmp(s_acLoadedMovieName, lpacMovieName, sizeof(s_acLoadedMovieName) - 1) == 0)
            return;   // same movie already attempted/loaded -- silent (avoids per-frame reload+spam)

        std::strncpy(s_acLoadedMovieName, lpacMovieName, sizeof(s_acLoadedMovieName) - 1);
        s_acLoadedMovieName[sizeof(s_acLoadedMovieName) - 1] = '\0';
        s_bMovieRequested = true;

        char lac[200];
        std::snprintf(lac, sizeof(lac), "[AptRT] PlayMovie: consume channel-41 '%s' (level %d).\n",
                      s_acLoadedMovieName, liLevelNum);
        CgsDev::Log::WriteToLog(lac);

        if (!s_bRuntimeReady)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: runtime not ready -- load deferred.\n");
            s_bMovieRequested = false;   // allow a retry next time the event fires
            return;
        }

        // ---- SYNCHRONOUS bundle load: GUIAPT\<NAME>.BUNDLE (the FAITHFUL bundle path) ------------
        // FAITHFULNESS (2026-06-30, user): the GUIAPT32/GUIAPT64 split was NON-FAITHFUL test
        // scaffolding -- the original game loads apt movies from GUIAPT\. Our PC build's GUIAPT
        // bundle carries the converted "Apt Data:1:7:8" (native 8-byte) data, so the path stays
        // faithful (GUIAPT) while the data content is the x64-converted form. Load through
        // BundleLoader::LoadBundle; the AptData resource (type 0x1E == 30) is FixUp'd by the
        // registered CgsResource::AptDataHeaderType handler.
        CgsResource::RegisterAllResourceTypes();   // idempotent: ensure AptDataHeaderType (0x1E) is live

        // Build the FAITHFUL bundle path "GuiApt\<NAME>.bundle" -- the exact format string the X360
        // apt loader uses (verified @0x828504B0), name passed as-is (Windows resolves case/separator).
        // The loader returns <=0 if missing -> bails cleanly.
        char lacBundlePath[160];
        std::snprintf(lacBundlePath, sizeof(lacBundlePath), "GuiApt\\%s.bundle", s_acLoadedMovieName);

        s_pMoviePool = &s_MoviePoolStorage;
        CgsResource::Pool::InitOptions lOptions;
        lOptions.miId   = 4;
        lOptions.mpcName = "AptMovie";
        for (u32 lt = 0; lt < CgsResource::E_MEMTYPE_NUMTYPES; ++lt)
        {
            lOptions.maHeapInfo[lt].muMaxNodes       = 256u;
            lOptions.maHeapInfo[lt].muHeapMemorySize = KU_MOVIE_POOL_BYTES - 128u * 1024u;
            lOptions.maHeapInfo[lt].muHeapAlignment  = 16u;
            lOptions.mResource.m_baseResources[lt]   = s_moviePoolBacking[lt];
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_size      = KU_MOVIE_POOL_BYTES;
            lOptions.mDescriptor.m_baseResourceDescriptors[lt].m_alignment = 16u;
        }
        lOptions.muMaxResources         = 64u;
        lOptions.muMaxImports           = 64u;
        lOptions.miRefCountThreshold    = 0;
        lOptions.miNumDependencies      = 0;
        lOptions.miBankId               = 0;
        lOptions.mbAllowDefragmentation = false;
        s_pMoviePool->InitPool(&lOptions);

        CgsResource::BundleLoader lLoader;
        const s32 liLoaded = lLoader.LoadBundle(lacBundlePath, s_pMoviePool,
                                                CgsResource::ResolveResourceType);
        std::snprintf(lac, sizeof(lac), "[AptRT] load: '%s' -> %d resources.\n", lacBundlePath, liLoaded);
        CgsDev::Log::WriteToLog(lac);
        if (liLoaded <= 0)
        {
            // FLAG: bundle missing / not platform-4 / unreadable. Bail cleanly.
            CgsDev::Log::WriteToLog("[AptRT] load: bundle missing/unreadable or not platform-4 -- bail (FLAG).\n");
            return;
        }

        // Pull the single AptData (0x1E) resource; its bytes are the relocated AptDataHeader.
        s32 liIndex = -1;
        CgsResource::Entry* lpEntry =
            s_pMoviePool->FindFirstResourceOfType(KU_APTDATA_RESOURCE_TYPE_ID, &liIndex);
        if (lpEntry == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] load: no AptData (type 0x1E) resource in bundle -- bail (FLAG).\n");
            return;
        }
        // PER-MEM-TYPE BASE PROBE: log all three m_baseResources[] + their sizes. The platform-4
        // resource can span multiple memory-type sections; the geometry/const offsets may be
        // relative to one of these. This shows which base is which at runtime (ground truth).
        std::snprintf(lac, sizeof(lac),
            "[AptRT] probe: AptData entry idx=%d  mem0=%p (size=%u)  mem1=%p (size=%u)  mem2=%p (size=%u)\n",
            liIndex,
            lpEntry->mResource.m_baseResources[0],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size,
            lpEntry->mResource.m_baseResources[1],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[1].m_size,
            lpEntry->mResource.m_baseResources[2],
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[2].m_size);
        CgsDev::Log::WriteToLog(lac);

        void* lpRes = lpEntry->mResource.m_baseResources[CgsResource::E_MEMTYPE_MAINMEMORY];
        if (lpRes == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] load: AptData resource has null main-memory bytes -- bail (FLAG).\n");
            return;
        }
        s_pAptHeader = reinterpret_cast<CgsGui::AptDataHeader*>(lpRes);

        // x64 TRANSCODE (USER-CONFIRMED 2026-06-30): AptDataHeader::FixUp is a NO-OP on x64 (the
        // 4-byte .apt format cannot be in-place-relocated into a high x64 backing -- see
        // CgsAptDataHeader.cpp). So the header's pointer fields are RAW file-relative OFFSETS. We
        // keep the FULL 64-bit resource base and resolve every offset as (T*)(base64 + offset).
        s_uAptResourceBase = reinterpret_cast<uintptr_t>(lpRes);
        s_uAptResourceSize =
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

        // Log the native-8 AptDataHeader field run (diagnostic; the geometry the render walk draws is
        // resolved faithfully by the engine -- AptCharacterAnimation::Fixup case-1 sets each shape's
        // char+0x20 via AptResolveShapeGeometry off the header's field[4] GuiGeometryObject).
        const u32* lpHeaderWords = reinterpret_cast<const u32*>(lpRes);
        std::snprintf(lac, sizeof(lac),
            "[AptRT] load: base=0x%016llX hdr words[0..5]=0x%X 0x%X 0x%X 0x%X 0x%X 0x%X\n",
            (unsigned long long)s_uAptResourceBase,
            lpHeaderWords[0], lpHeaderWords[1], lpHeaderWords[2],
            lpHeaderWords[3], lpHeaderWords[4], lpHeaderWords[5]);
        CgsDev::Log::WriteToLog(lac);

        // HEX-DUMP PROBE (read-only, bounded): the first 64 bytes of the AptData resource (the
        // AptDataHeader region), for ground-truth diagnostics.
        DumpResourceBytes("hdr@+0",   lpRes, 0,
                          lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size, 64);

        s_bMovieLoaded = true;

        // STEP 8 (gated): drive the FAITHFUL Apt load path (LoadAnimation -> AptCompleteAnimation
        // AsyncLoad -> AptLoader::CompleteLoad -> Resolve -> Fixup -> MakeCharacterAnimationInst),
        // retiring the invented signature-scan + hand-rolled AptFile synthesis. One-shot + heavily
        // guarded; on any failure the direct-geometry render above stays intact so the title art
        // still shows. The faithful per-frame tick/AS render are the next passes.
        DriveFaithfulLoad(lpEntry);
    }

    // Read-only, bounded hex dump of a resource region to the log (diagnostic ground truth).
    // Logs luBytes bytes starting at lpBase+luOffset, clamped to [luOffset, luResourceSize), as
    // hex + the u32 interpretation. Never reads past the resource size (no AV).
    static void DumpResourceBytes(const char* lpcTag, void* lpBase, u32 luOffset,
                                  u32 luResourceSize, u32 luBytes)
    {
        if (lpBase == nullptr)
            return;
        if (luOffset >= luResourceSize)
        {
            char lacOOR[128];
            std::snprintf(lacOOR, sizeof(lacOOR),
                "[AptRT] dump %s: offset 0x%X out of range (resSize=0x%X) -- skipped.\n",
                lpcTag, luOffset, luResourceSize);
            CgsDev::Log::WriteToLog(lacOOR);
            return;
        }
        u32 luAvail = luResourceSize - luOffset;
        if (luBytes > luAvail) luBytes = luAvail;
        if (luBytes > 64u)     luBytes = 64u;   // cap the log line length

        const u8* lpb = reinterpret_cast<const u8*>(lpBase) + luOffset;
        char lac[320];
        int liPos = std::snprintf(lac, sizeof(lac), "[AptRT] dump %s (@+0x%X, %u bytes): ",
                                  lpcTag, luOffset, luBytes);
        for (u32 lu = 0; lu < luBytes && liPos < static_cast<int>(sizeof(lac)) - 4; ++lu)
            liPos += std::snprintf(lac + liPos, sizeof(lac) - liPos, "%02x ", lpb[lu]);
        std::snprintf(lac + liPos, sizeof(lac) - liPos, "\n");
        CgsDev::Log::WriteToLog(lac);

        // u32 view (first up to 8 words).
        const u32 luWords = (luBytes / 4u) > 8u ? 8u : (luBytes / 4u);
        if (luWords > 0)
        {
            liPos = std::snprintf(lac, sizeof(lac), "[AptRT] dump %s u32: ", lpcTag);
            for (u32 lu = 0; lu < luWords && liPos < static_cast<int>(sizeof(lac)) - 12; ++lu)
            {
                u32 luWord;
                std::memcpy(&luWord, lpb + lu * 4u, sizeof(luWord));
                liPos += std::snprintf(lac + liPos, sizeof(lac) - liPos, "0x%08X ", luWord);
            }
            std::snprintf(lac + liPos, sizeof(lac) - liPos, "\n");
            CgsDev::Log::WriteToLog(lac);
        }
    }


    // =========================================================================
    // LocateMovieRoot8 -- FLAG (x64 converted 8-byte bundle): find the type-9 MOVIE ROOT
    // character header by scanning for the 0x09876543 character signature. Returns the byte
    // offset (from luBase) of the ROOT CHARACTER HEADER (mnType==9 lives here), or 0.
    //
    // The console locates the root via pConstFile->mnDataRootOffset (def base at root+16 for
    // the 4-byte format), but our converted 8-byte .apt's dataRootOffset reads 0xB0, which is
    // NOT the movie root (the console data layout diverged in conversion). So the host scans:
    // for each signature hit, header = sig-8, def base = header+0x20 (native-8), and the movie
    // root is the UNIQUE type-9 record with sane screen dims. This is the ONE genuinely
    // un-homable-as-console piece; everything downstream (CompleteLoad/Resolve/Fixup) is faithful.
    // For TITLE_SCREEN02 the only validating hit is the char header @res+0x4930 (def base 0x4950:
    // charCount=41, w=1280, h=720, ms=33). CompleteLoad derives def base = root + 0x20.
    // =========================================================================
    static u32 LocateMovieRoot8(uintptr_t luBase, u32 luSize)
    {
        const u32 kHdrToDefBase = 0x20u;   // native-8 character header size (def base @ header+0x20)
        for (u32 luScan = 8u; luScan + 4u <= luSize; luScan += 4u)
        {
            if (*reinterpret_cast<const u32*>(luBase + luScan) != 0x09876543u)
                continue;
            const u32 luHdr  = luScan - 8u;                                     // sig @ header+8
            const u32 luType = *reinterpret_cast<const u32*>(luBase + luHdr);   // header+0 type
            const u32 luDB   = luHdr + kHdrToDefBase;
            if (luDB + 0x50u > luSize)
                continue;
            const u32 luW  = *reinterpret_cast<const u32*>(luBase + luDB + 0x28u);
            const u32 luH  = *reinterpret_cast<const u32*>(luBase + luDB + 0x2Cu);
            const u32 luMs = *reinterpret_cast<const u32*>(luBase + luDB + 0x30u);
            const u32 luCC = *reinterpret_cast<const u32*>(luBase + luDB + 0x18u);
            if (luType == 9u && luW >= 320u && luW <= 2048u && luH >= 240u && luH <= 1536u &&
                luMs >= 10u && luMs <= 100u && luCC >= 1u && luCC <= 512u)
            {
                char lac[176];
                const u32 luFC = *reinterpret_cast<const u32*>(luBase + luDB + 0x00u);
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] faithful: movie root sig@0x%X charHdr@0x%X defbase@0x%X type=9 "
                    "frameCount=%u charCount=%u w=%u h=%u ms=%u\n",
                    luScan, luHdr, luDB, luFC, luCC, luW, luH, luMs);
                CgsDev::Log::WriteToLog(lac);
                return luHdr;   // the ROOT CHARACTER HEADER offset
            }
        }
        return 0;
    }

    // =========================================================================
    // DriveFaithfulLoad -- STEP 8: drive the FAITHFUL Apt load path (retires the invented
    // TryFaithfulInstantiate signature-scan + hand-rolled AptFile synthesis). Chain:
    //
    //   AddAptData(header)  ->  AptLoader::Load(name)  ->  AptCallbackFile::LoadAnimation(name,&h)
    //       -> AptCompleteAnimationAsyncLoad -> AptLoader::CompleteLoad -> Resolve -> Fixup
    //   then AptGetAnimationAtLevel(0) -> MakeCharacterAnimationInst(pFile) -> SetCharacterInst.
    //
    // The invented pieces that are GONE: the hand-rolled AptFile (now real, via AptLoader::Load
    // + CompleteLoad's field stores), the invented Fixup invocation (now driven by CompleteLoad
    // -> Resolve), and the invented instantiation orchestration. The ONE FLAG'd x64 piece that
    // stays is the movie-root location (LocateMovieRoot8) -- the converted 8-byte bundle's
    // dataRootOffset does not locate the root, so the host scans + hands CompleteLoad the root.
    //
    // On ANY failure: bail cleanly (s_bFaithfulInstantiated stays false), leaving the
    // direct-geometry render (s_bGeomResolved) intact so the title art still shows.
    // =========================================================================
    static void DriveFaithfulLoad(CgsResource::Entry* lpEntry)
    {
        (void)lpEntry;
        if (!KB_FAITHFUL_PATH_ENABLED || s_bFaithfulAttempted)
            return;
        s_bFaithfulAttempted = true;

        char lac[224];
        CgsDev::Log::WriteToLog("[AptRT] faithful: STEP 8 load-path begin.\n");

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

        // The AptData resource IS the AptDataHeader; the header sits at the resource base.
        const u32 luSize = s_uAptResourceSize;
        const uintptr_t luBase = s_uAptResourceBase;
        if (luSize == 0 || luBase == 0 || s_pAptHeader == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: no resource bytes / header -- bail (FLAG).\n");
            return;
        }

        // Peek the pointer size from the "Apt Data:1:7:N" signature. That signature lives in the
        // APT DATA chunk -- hdr field 2 (@+0x10) of the libapt2 6-field header [name, baseName,
        // aptData, const, geom, size]. (Renamed 2026-07-01: this slot was mislabelled "constData";
        // the real const chunk is hdr field 3 @+0x18 -- see LoadAnimation's un-collapse.)
        const u32 luAptDataOff = static_cast<u32>(*reinterpret_cast<const u64*>(luBase + 0x10u));
        if (luAptDataOff == 0 || luAptDataOff >= luSize)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: aptData offset out of range -- bail (FLAG).\n");
            return;
        }
        AptConstFile* lpConstFile = reinterpret_cast<AptConstFile*>(luBase + luAptDataOff);
        const int liPtrSize = lpConstFile->GetPointerSizeBytes();   // 8 (native) or 4 (console)
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: header@0x%llX aptData@0x%X ptrSize=%d (%u bytes).\n",
            (unsigned long long)luBase, luAptDataOff, liPtrSize, luSize);
        CgsDev::Log::WriteToLog(lac);

        // --- 1. LOCATE the movie-root character header (FLAG: x64 converted bundle) ------------
        // CompleteLoad needs the root header (mpData) -- our converted bundle's dataRootOffset does
        // not locate it, so the host scans (see LocateMovieRoot8). Stash it for LoadAnimation ->
        // CompleteLoad (via gAptLoadAnimRootOverride). On the 4-byte console path leave it null so
        // CompleteLoad uses the faithful pBase + dataRootOffset formula.
        void* lpRootOverride = nullptr;
        if (liPtrSize == 8)
        {
            const u32 luRootHdrOff = LocateMovieRoot8(luBase, luSize);
            if (luRootHdrOff == 0)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: no sane type-9 movie root found "
                                        "-- bail to fallback, game stays up (FLAG).\n");
                return;
            }
            lpRootOverride = reinterpret_cast<void*>(luBase + luRootHdrOff);
        }
        CgsGui::gAptLoadAnimRootOverride = lpRootOverride;

        // Stash the AptData resource span so the native-8 AptMovie::resolve64 relocation walk
        // (driven by AptCharacterAnimation::Fixup case-5/9) can bounds-check every serialised
        // offset slot. base == the load base == the resource base (luBase) on our path.
        CgsGui::gAptResourceSpanBase = luBase;
        CgsGui::gAptResourceSpanSize = s_uAptResourceSize;

        // --- 2. REGISTER the header with the data handler (AptDataHandler::AddAptData) ----------
        // So the faithful FindAptData(name) inside LoadAnimation resolves it. Idempotent by name.
        // FLAG (x64): AddAptData takes the resolved name (the header's mpacMovieName is an
        // un-relocated offset on x64); we pass s_acLoadedMovieName.
        CgsGui::AptAux* lpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;
        if (lpAptAux == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: AptAux singleton null -- bail (FLAG).\n");
            CgsGui::gAptLoadAnimRootOverride = nullptr;
            return;
        }
        lpAptAux->mAptDataHandler.AddAptData(reinterpret_cast<CgsGui::AptDataHeader*>(s_pAptHeader),
                                             s_acLoadedMovieName);
        CgsDev::Log::WriteToLog("[AptRT] faithful: AddAptData(header) registered.\n");

        // --- 3. REGISTER / look up the AptFile handle (AptLoader::Load) --------------------------
        // Load returns a handle owning ONE counted reference. We keep it (laOwned) and hand a
        // COPY to LoadAnimation (which consumes its copy, per the console by-value slot). The owned
        // handle survives, and after CompleteLoad it points at the loaded movie (mpData = root).
        EAStringC lNameStr(s_acLoadedMovieName);
        AptFilePtr laOwned = lpTarget->mpLoader->Load(lNameStr);
        if (laOwned.pData == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: AptLoader::Load returned null handle -- bail (FLAG).\n");
            CgsGui::gAptLoadAnimRootOverride = nullptr;
            return;
        }

        AptFilePtr laForCallback;
        laForCallback.pData = laOwned.pData;
        AptSharedPtrIncRef(laForCallback.pData);   // the callback consumes this copy (keep laOwned alive)

        // --- 4. DRIVE the faithful load: LoadAnimation -> AsyncLoad -> CompleteLoad -> Resolve ->
        //        Fixup. The Fixup relocates the movie root in place (charCount/importCount set). ---
        std::snprintf(lac, sizeof(lac),
            "[AptRT] faithful: LoadAnimation('%s', handle=%p) [rootOverride=%p] ...\n",
            s_acLoadedMovieName, (void*)laForCallback.pData, lpRootOverride);
        CgsDev::Log::WriteToLog(lac);
        CgsGui::AptCallbackFile::LoadAnimation(s_acLoadedMovieName, &laForCallback);
        CgsGui::gAptLoadAnimRootOverride = nullptr;   // one-shot; done

        // The owned handle's AptFile is now loaded (CompleteLoad set mpData = root, state = 3).
        AptFile* lpFile = laOwned.pData;
        if (lpFile == nullptr || lpFile->mpData == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: post-LoadAnimation AptFile not loaded "
                                    "(mpData null) -- bail (FLAG).\n");
            return;
        }
        s_pFaithfulAptFile  = lpFile;
        s_pFaithfulCharAnim = reinterpret_cast<AptCharacterAnimation*>(
            static_cast<char*>(lpFile->mpData) + ((liPtrSize == 8) ? 0x20 : 0x10));   // def base

        // "Fixup COMPLETED" marker (read the counts the Fixup walk populated at the def base).
        {
            const u32 luOffCC = (liPtrSize == 8) ? 0x18u : 0x0Cu;   // charCount
            const u32 luOffIC = (liPtrSize == 8) ? 0x34u : 0x20u;   // importCount
            const int liCC = *reinterpret_cast<const int*>(
                reinterpret_cast<char*>(s_pFaithfulCharAnim) + luOffCC);
            const int liIC = *reinterpret_cast<const int*>(
                reinterpret_cast<char*>(s_pFaithfulCharAnim) + luOffIC);
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: Fixup COMPLETED. charCount@+0x18=%d importCount@+0x34=%d (via CompleteLoad)\n",
                liCC, liIC);
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
                reinterpret_cast<char*>(s_pFaithfulCharAnim) + 0x34);   // def+0x34 importCount
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: STEP3 content-load %d import(s) via AptLoader::Update ...\n",
                liImportCount);
            CgsDev::Log::WriteToLog(lac);
            lpTarget->mpLoader->Update();   // drives StartAsyncLoad (load) + Link for every import
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: STEP3 content-load done. import bundles resident=%u.\n",
                s_uImportBundleCount);
            CgsDev::Log::WriteToLog(lac);
        }

        // STEP 5 PROBE (blocker #1): confirm the ROOT movie's frame table is RELOCATED by
        // AptCharacterAnimation::Fixup case-9 -> AptMovie::resolve64. The root def-base IS
        // the root AptMovie (frameCount@+0x00, mpFrames@+0x08). Before the fix mpFrames held
        // the raw file offset 0x5180; after resolve64 it is a live pointer (base + 0x5180),
        // and frame[0].mnCommandCount should be 13 (verified vs TITLE_SCREEN02.bundle).
        {
            const char* lpRootMovie = reinterpret_cast<const char*>(s_pFaithfulCharAnim);
            const int liFrameCount = *reinterpret_cast<const int*>(lpRootMovie + 0x00);
            void** lpFrames = *reinterpret_cast<void* const*>(lpRootMovie + 0x08) ?
                *reinterpret_cast<void** const*>(lpRootMovie + 0x08) : nullptr;
            const unsigned long long luFramesRaw =
                *reinterpret_cast<const unsigned long long*>(lpRootMovie + 0x08);
            // The frame table is a relocated pointer iff it lands inside the resource span.
            const bool lbRelocated =
                (luFramesRaw >= s_uAptResourceBase &&
                 luFramesRaw <  s_uAptResourceBase + s_uAptResourceSize);
            int liFrame0Cmds = -1;
            if (lbRelocated && lpFrames != nullptr)
            {
                // frame[0]: {mnCommandCount@0, mpCommands@8} (native-8 stride 16).
                liFrame0Cmds = *reinterpret_cast<const int*>(lpFrames);
            }
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: STEP5 frame-table: frameCount=%d mpFrames=0x%016llX relocated=%s "
                "frame0.cmdCount=%d (want 13)\n",
                liFrameCount, luFramesRaw, lbRelocated ? "YES" : "NO(raw-offset)", liFrame0Cmds);
            CgsDev::Log::WriteToLog(lac);
        }

        // --- 5. the root level-0 CIH on the director's root display list ---------------------------
        // AptGetAnimationAtLevel(0) searches the director's root display list then (if absent) creates
        // an AptCIH via the GC pool. Self-test the GC pool first (probe the 40-byte carve).
        {
            void* lpGCTest = gpGCPoolManager->Allocate(40);
            if (lpGCTest == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] faithful: GC pool cannot carve 40 bytes -- bail (FLAG).\n");
                return;
            }
            gpGCPoolManager->Deallocate(lpGCTest, 40);
        }
        CgsDev::Log::WriteToLog("[AptRT] faithful: AptGetAnimationAtLevel(0) (create root CIH) ...\n");
        AptCIH* lpRootCIH = AptGetAnimationAtLevel(0);
        if (lpRootCIH == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] faithful: root CIH null (GC pool / display list) -- bail (FLAG).\n");
            return;
        }
        s_pFaithfulRootCIH = lpRootCIH;

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
        // AptRuntimeUpdate (Step 3) instead. DEFENSIVE: guarded; a null animInst leaves the empty root
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
            s_bFaithfulInstantiated = true;
            // Open the input-recorder/replay gate (normal-play value) so the freshly-placed imported
            // sprite CONTAINERS auto-advance + run doFrameControls, recursing to place their nested
            // shapes/images (the title's actual visible content lives inside those imported sprites).
            // FLAG: the replay-recorder subsystem that would drive this is out of scope; set once here.
            gbAptRecorderGate = 1;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] faithful: INSTANTIATED -- animInst %p bound to root CIH %p (movie root %p); "
                "IncCharacterList walked %d chars; sprite state seeded. dlParent(after bind)=%p\n",
                (void*)lpAnimInst, (void*)lpRootCIH, lpFile->mpData,
                *reinterpret_cast<const int*>(reinterpret_cast<char*>(s_pFaithfulCharAnim) + 0x18),
                (void*)lpRootCIH->GetDisplayListParent());
            CgsDev::Log::WriteToLog(lac);
        }
        else
        {
            CgsDev::Log::WriteToLog(
                "[AptRT] faithful: MakeCharacterAnimationInst returned null -- root CIH stays empty "
                "(game up; geometry fallback). (FLAG)\n");
            s_bFaithfulInstantiated = false;
        }
    }

    // =========================================================================
    // LoadImportBundle -- STEP 3: synchronously content-load ONE import movie by name and drive its
    // AptFile through the faithful completion (AptCompleteAnimationAsyncLoad -> CompleteLoad ->
    // Resolve -> Fixup), so the import's AptFile::mpData points at its resolved movie root
    // (mnState == 3). This is the PC substitute for the console's async .apt stream (the stream that
    // AptLoader_StartAsyncLoad kicks off + whose completion the stream subsystem posts). The import
    // bundle stays resident (registered in s_aImportBundles). Idempotent by name.
    //
    // The import bundle is a "1:7:8" apt movie corrected offline by fix_apt_bundle.py (so its char
    // table + movie root are clean). The load mirrors AptRuntimePlayMovie's parent load exactly:
    // BundleLoader::LoadBundle -> the AptData (0x1E) resource -> AddAptData(name) -> LocateMovieRoot8
    // -> AptCompleteAnimationAsyncLoad. // FLAG (x64 converted bundle): the movie-root location uses
    // the signature scan (LocateMovieRoot8) exactly as the parent path does.
    // =========================================================================
    static bool LoadImportBundle(const char* lpacMovieName, AptFilePtr* lpFile)
    {
        char lac[256];
        if (lpacMovieName == nullptr || lpacMovieName[0] == '\0')
            return false;

        // Dedup: already resident? (an import referenced by >1 movie loads once). If so, just
        // complete the passed handle against the resident span (its AptFile may be a fresh
        // "requested" handle from a second referencing movie's Fixup pass-3).
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

            // Register the header with the data handler so a faithful FindAptData(name) resolves it.
            CgsGui::AptAux* lpAptAux = CgsGui::AptAuxPointer::mpAptAuxInst;
            if (lpAptAux != nullptr)
                lpAptAux->mAptDataHandler.AddAptData(
                    reinterpret_cast<CgsGui::AptDataHeader*>(lpRes), lpSlot->macName);

            ++s_uImportBundleCount;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] import-load: '%s' resident base=0x%016llX size=%u (slot %u).\n",
                lpSlot->macName, (unsigned long long)lpSlot->luBase, lpSlot->luSize,
                s_uImportBundleCount - 1);
            CgsDev::Log::WriteToLog(lac);
        }

        // Drive the faithful completion for this handle against the resident span. UN-COLLAPSED
        // (2026-07-01, matches LoadAnimation): the libapt2 6-field header puts aptData@+0x10 and
        // const@+0x18; pBase = the "Apt Data:1:7:8" chunk (the reloc base), pConstFile = the
        // "Apt constant file" chunk (the _parseStream ctx + the movieOffset root locator).
        const u32 luAptDataOff = static_cast<u32>(*reinterpret_cast<const u64*>(lpSlot->luBase + 0x10u));
        const u32 luConstOff   = static_cast<u32>(*reinterpret_cast<const u64*>(lpSlot->luBase + 0x18u));
        if (luAptDataOff == 0 || luAptDataOff >= lpSlot->luSize)
        {
            CgsDev::Log::WriteToLog("[AptRT] import-load: aptData offset out of range -- bail (FLAG).\n");
            return false;
        }
        void* lpBase = reinterpret_cast<void*>(lpSlot->luBase + luAptDataOff);
        AptConstFile* lpConstFile = (luConstOff != 0 && luConstOff < lpSlot->luSize)
            ? reinterpret_cast<AptConstFile*>(lpSlot->luBase + luConstOff)
            : reinterpret_cast<AptConstFile*>(lpBase);   // degenerate: the old collapse

        const u32 luRootHdrOff = LocateMovieRoot8(lpSlot->luBase, lpSlot->luSize);
        if (luRootHdrOff == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] import-load: no type-9 movie root found -- bail (FLAG).\n");
            return false;
        }
        void* lpRootOverride = reinterpret_cast<void*>(lpSlot->luBase + luRootHdrOff);

        // Stash THIS import's span so its Fixup case-5/9 (AptMovie::resolve64) bounds-checks correctly.
        // (Each import relocates against its own base; restore the parent's span after -- Update may
        // continue driving the parent's link, which reads the parent span.)
        const uintptr_t luPrevSpanBase = CgsGui::gAptResourceSpanBase;
        const uint32_t  luPrevSpanSize = CgsGui::gAptResourceSpanSize;
        CgsGui::gAptResourceSpanBase = lpSlot->luBase;
        CgsGui::gAptResourceSpanSize = lpSlot->luSize;
        CgsGui::gAptLoadAnimRootOverride = lpRootOverride;

        std::snprintf(lac, sizeof(lac),
            "[AptRT] import-load: complete '%s' handle=%p base=0x%016llX root@0x%X ...\n",
            lpSlot->macName, (void*)(lpFile ? lpFile->pData : nullptr),
            (unsigned long long)lpSlot->luBase, luRootHdrOff);
        CgsDev::Log::WriteToLog(lac);

        // AptCompleteAnimationAsyncLoad(handle, pBase, pConstFile, pAptDataHeader, pPreResolvedRoot)
        // -> CompleteLoad -> Resolve -> Fixup: sets the import AptFile's mpData = root, mnState = 3.
        AptCompleteAnimationAsyncLoad(lpFile, lpBase, lpConstFile, lpSlot->lpHeader, lpRootOverride);

        CgsGui::gAptLoadAnimRootOverride = nullptr;
        CgsGui::gAptResourceSpanBase = luPrevSpanBase;
        CgsGui::gAptResourceSpanSize = luPrevSpanSize;
        return true;
    }

    // =========================================================================
    // PropagateDirtyToChildren -- STEP 4 dirty propagation (the missing piece for nested content).
    //
    // The root CIH is dirtied each frame (host), so its tick runs frame-0's place commands + places
    // the top-level sprite CONTAINERS. Those placed children come out with mnClipActionFlags 0xC0
    // (needs-action + fresh) but mFlagsA bit25 (dirty) CLEAR, so AptCIH::tick early-returns on them
    // (its first line gates on the dirty bit) and they never run THEIR doFrameControls -> their nested
    // shapes/images are never placed. On the console the render-tree-manager propagates the dirty bit
    // down as nodes are placed; that propagation is the deferred piece. This walks a clip's child
    // display list and SetDirtyState(true) every sprite/animation node (recursively), so the next tick
    // recurses into each + places its content -- cascading one display-list level per frame until the
    // whole tree is composed. // FLAG: stand-in for the render-tree-manager placement dirty propagation.
    // =========================================================================
    static void PropagateDirtyToChildren(AptCIH* lpNode, int nDepth)
    {
        if (lpNode == nullptr || nDepth > 12)   // depth cap (guards against a pathological cycle)
            return;
        AptCharacterInst* lpCI = lpNode->GetCharacterInst();
        if (lpCI == nullptr)
            return;
        const uint32_t luTag = lpCI->GetTypeTag();
        if (luTag != 5 && luTag != 9)           // only sprite/animation clips carry a child list
            return;
        AptCharacterSpriteInstBase* lpSprite = static_cast<AptCharacterSpriteInstBase*>(lpCI);
        AptDisplayListState* lpState = lpSprite->mDisplayList.AsState();
        if (lpState == nullptr)
            return;
        for (AptCIH* lpChild = lpState->mpFirst; lpChild != nullptr;
             lpChild = lpChild->GetDisplayListNext())
        {
            AptCharacterInst* lpChildCI = lpChild->GetCharacterInst();
            if (lpChildCI == nullptr)
                continue;
            const uint32_t luChildTag = lpChildCI->GetTypeTag();
            if (luChildTag == 5 || luChildTag == 9)
            {
                if (!lpChild->GetDirtyState())
                    lpChild->SetDirtyState(true, false);   // dirty so its next tick places its content
                PropagateDirtyToChildren(lpChild, nDepth + 1);   // recurse deeper as levels appear
            }
        }
    }


    // -------------------------------------------------------------------------
    // RETIRED (2026-07-01): the invented direct-geometry render (RenderLoadedGeometryDirect +
    // ResolveMeshTexture) is GONE. AptRuntimeRender now drives the FAITHFUL render-tree walk
    // (AptRender) which flushes through the real display-list -> render-tree -> AptRenderHandler::
    // Render -> ImRenderBuffer -> D3D9 path. The prior fallback walked the movie geometry directly
    // (offset transcode) + drew via the Im2d immediate wrapper -- invention that bypassed the engine
    // render tree; removed per the render-tree-flush milestone.
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    // AptRuntimeUpdate -- per-frame TICK only. Advances the frame counter and (when a movie is
    // loaded) would tick its timeline + ActionScript. The RENDER moved to AptRuntimeRender (the
    // proven immediate-mode Im2d path). Called from GuiModule::Update (runs before the render hook).
    // -------------------------------------------------------------------------
    void AptRuntimeUpdate()
    {
        if (!s_bRuntimeReady)
            return;

        ++s_iFrameCounter;
        const bool lbProbeFrame = (s_iFrameCounter % 30) == 1;   // ~every 30 frames

        // Nothing loaded yet -> nothing to tick/render this frame.
        if (!s_bMovieLoaded)
            return;

        // ---- STEP 2: TICK the instantiated faithful root CIH ---------------------------------------
        // The homed AptCIH::tick advances the clip's play-head; for a fresh clip it runs frame 0's
        // place/remove timeline commands (doFrameControls -> placeObject), POPULATING the display list
        // with child AptCIHs, then queues the frame's ActionScript. We drive it on the instantiated root
        // CIH each frame. The render-tree manager is the null stub, but AptRTM_CreateItem now falls back
        // to the homed Manager_CreateItem factory, so each placed inst gets a real render item (carrying
        // its character) -- which is what tick / AptCIH_GetClipMovie reach the movie through. Every deref
        // is guarded (the engine null-safety we added) so the tick completes structurally or bails.
        // UN-DEFERRED (2026-07-01): the three timeline-engine blockers that held the faithful tick
        // off are ALL homed, so the tick now runs frame 0's place commands and composes the movie:
        //   (a) FRAME TABLE RELOCATED. AptCharacterAnimation::Fixup case-5/9 now drives the native-8
        //       AptMovie::resolve64 (AptMovie.cpp) on the embedded movie (char+0x20), relocating
        //       mpFrames @movie+0x08 (the 0x5180 offset) + the per-frame command arrays/pointers into
        //       live pointers. VERIFIED at load: "STEP5 frame-table: ... relocated=YES frame0.cmdCount=13".
        //   (b) NATIVE-8 doFrameControls. AptMovie::doFrameControls (AptMovie.cpp) is re-typed to
        //       (AptDisplayList*, AptCIH*, int) and reads the movie char-anim def-base + charTable /
        //       import table through the parent CIH's named char-inst chain (char+0x20 embed,
        //       charTable/importTable named members) -- the native-8 layout, not console offsets.
        //   (c) placeObject IS HOMED. sub_82B0AE08 is decompiled faithfully as AptMovie_PlaceCommand
        //       (AptMovie.cpp): it reads the serialised place-info record and calls the fully-homed
        //       AptDisplayList::placeObjectNCXForm, which creates/inserts the placed AptCharacterInst.
        // Frame 0 of TITLE_SCREEN02 carries 12 place commands + 1 back-to-script (verified vs the
        // bundle), so the tick populates the root clip's child display list with the title's characters.
        // FLAG (re-deferred 2026-07-01, boot-stability): resolve64 + native-8 doFrameControls +
        // AptMovie_PlaceCommand are homed and frame-0 composition is PROVEN (childNodes=7, no AV).
        // But the tick ADVANCES the 103-frame timeline and silently AVs (~25s) on a LATER frame that
        // places the deferred pieces: the imported char (charId 30 -> charTable[30] null because
        // Fixup pass-3 import-load is deferred) and shape chars whose geometry is deferred (case-1
        // pfnLoadRenderingUnit). Re-enabling the full tick regresses the stable boot, so it stays
        // deferred until the import-load + shape-geometry (+ the render-tree flush for pixels) land.
        // The render-leaf BODIES stay faithful + committed; the per-frame drive is now ENABLED
        // (2026-07-01): the faithful per-frame tick composes the movie AND recurses into the nested
        // imported sprite CONTAINERS (KB_NESTED_DIRTY_PROPAGATION), so the title's actual visible
        // content (the "Paradise City" logo art, which lives one display-list level deeper inside the
        // imported clips) places + draws (PROVEN via screenshot). The prior AV blockers are closed:
        //   - resolve64 now relocates EVERY per-frame command record's pointer slots (XB1-verified),
        //     so a nested container's doFrameControls/queueFrameActions read their records without AV;
        //   - the AS-VM EXECUTION stays deferred (relocated stream POINTERS, but runStream is stubbed
        //     and never runs -- an un-run action queue is a valid state);
        //   - the LOOP-WRAP (frame == frameCount -> jumpToFrame(0)) no longer AVs: jumpToFrame's
        //     arbitrary-jump path (AptMovie::DoTemporaryFrameControls + the AptPseudoDisplayList
        //     temporary-frame skip resolver) is UN-HOMED for the native-8 layout, so AptCIH::tick's
        //     wrap now RESETS the play-head to frame 0 directly (a plain loop) instead of the
        //     un-homed replay-merge -- see the FLAG in AptCIH::tick.
        // lbTickReady is TRUE every frame (the tick + nested recursion run continuously, stable 60s).
        // FLAG (honest boundaries that remain): (a) 3 nested sprite movies have a CONVERTER-malformed
        // frame table (command-array ptr at frame+0x04 not native-8 frame+0x08) and are safely skipped
        // (doFrameControls/queueFrameActions null-mpCommands guard) -- their sub-content stays unplaced;
        // (b) without the AS VM to GATE the transitions (hold-on-label / wait-for-input), the timeline
        // auto-advances through its transin/fade/transout frames, so the title is transient rather than
        // held. Both are the deferred AS-VM / offline-bundle-fix follow-ons, not invention.
        bool lbTickReady = true;
        if (lbTickReady && s_bFaithfulInstantiated && s_pFaithfulRootCIH != nullptr)
        {
            AptCIH* lpRoot = static_cast<AptCIH*>(s_pFaithfulRootCIH);
            // Mark the root clip "dirty" so AptCIH::tick processes it (mFlagsA bit25 == GetDirtyState,
            // the tick gate; the ctor leaves it clear). SetDirtyState(true,...) sets bit25. (FLAG: the
            // X360 dirties via the render-tree manager's propagation; we set it directly each frame.)
            if (!lpRoot->GetDirtyState())
                lpRoot->SetDirtyState(true, false);

            if (s_iTickFrame < 4)   // probe the first few frames (frame 0 is where placement happens)
            {
                char lacp[128];
                std::snprintf(lacp, sizeof(lacp), "[AptRT] tick: frame=%d -> AptCIH::tick(root %p) ...\n",
                              s_iTickFrame, (void*)lpRoot);
                CgsDev::Log::WriteToLog(lacp);
            }

            const int liTickResult = lpRoot->tick();   // advance frame 0 -> place characters

            // STEP 4 (ENABLED 2026-07-01): propagate dirty into the freshly-placed sprite CONTAINERS
            // so their next tick recurses + places their nested content. This places the nested shapes
            // (doFrameControls succeeds). The prior AV boundary is CLOSED: AptMovie::resolve64 now
            // relocates every per-frame command record's pointer slots at the native-8 offsets (tag-1
            // Action stream ptr @cmd+0x08, tag-2 Label name ptr @cmd+0x08, tag-3 PlaceObject name
            // @body+0x30 + clipActions block @body+0x40 + its record-array/stream ptrs, tag-8 Morph
            // stream ptr @cmd+0x08), so a nested container's doFrameControls / queueFrameActions read
            // their records without dereferencing an un-relocated file offset. // FLAG (deferred AS-VM
            // EXECUTION): the action-stream *contents* (the AS bytecode) stay un-parsed and un-run --
            // resolve64 relocates only the record POINTERS, and the interpreter's runStream is stubbed,
            // so queued actions never execute. An un-run AS action queue is a valid state; statically
            // placed nested shapes/images compose without the VM. Dynamically script-attached content
            // (content the AS bytecode would attach at runtime) is the remaining VM last-mile.
            if (KB_NESTED_DIRTY_PROPAGATION)
                PropagateDirtyToChildren(lpRoot, 0);

            // Walk the director's root display list + count the placed nodes (the placement result).
            s32 liNodes = 0;
            AptTarget* lpTgt = GetTarget();
            if (lpTgt != nullptr && lpTgt->mpAnimationTarget != nullptr)
            {
                AptDisplayList* lpRootList = lpTgt->mpAnimationTarget->GetRootDisplayList();
                AptDisplayListState* lpState = (lpRootList != nullptr) ? lpRootList->AsState() : nullptr;
                if (lpState != nullptr)
                {
                    for (AptCIH* lpN = lpState->mpFirst; lpN != nullptr && liNodes < 4096;
                         lpN = lpN->GetDisplayListNext())
                    {
                        ++liNodes;
                        if (s_iTickFrame < 2 && liNodes <= 6)
                        {
                            AptCharacterInst* lpCI = lpN->GetCharacterInst();
                            char lacn[160];
                            std::snprintf(lacn, sizeof(lacn),
                                "[AptRT] tick:   node %p charInst=%p renderItem=%p depth=%d\n",
                                (void*)lpN, (void*)lpCI,
                                (void*)(lpCI ? lpCI->GetRenderItem() : nullptr),
                                lpCI ? lpCI->GetDepth() : -1);
                            CgsDev::Log::WriteToLog(lacn);
                        }
                    }
                }
            }

            // Also count the ROOT CIH's CHILD display list -- doFrameControls places into the animInst's
            // own mDisplayList (the sprite/animation child list), not the director root. This is where the
            // native-8 place path's nodes land.
            s32 liChildNodes = 0;
            {
                AptCharacterInst* lpRootCI = lpRoot->GetCharacterInst();
                if (lpRootCI != nullptr)
                {
                    AptCharacterSpriteInstBase* lpSprite =
                        static_cast<AptCharacterSpriteInstBase*>(lpRootCI);
                    AptDisplayListState* lpChildState = lpSprite->mDisplayList.AsState();
                    if (lpChildState != nullptr)
                    {
                        for (AptCIH* lpN = lpChildState->mpFirst; lpN != nullptr && liChildNodes < 4096;
                             lpN = lpN->GetDisplayListNext())
                        {
                            ++liChildNodes;
                            if (s_iTickFrame < 2 && liChildNodes <= 8)
                            {
                                AptCharacterInst* lpCI = lpN->GetCharacterInst();
                                AptRenderItem* lpRI = lpCI ? lpCI->GetRenderItem() : nullptr;
                                AptCharacter* lpChar = lpRI ? lpRI->mpCharacter : nullptr;
                                // char+0x20 == the shape geometry rendering unit (set by Fixup case-1).
                                const void* lpGeom = lpChar
                                    ? *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(lpChar) + 0x20)
                                    : nullptr;
                                const int liType = lpChar
                                    ? *reinterpret_cast<const int*>(lpChar) : -999;   // AptCharacter::mnType @+0
                                // DIAG: the child's dirty bit (mFlagsA bit25), its sprite play-state
                                // flags (mnClipActionFlags), and its OWN nested display-list count --
                                // to see whether the imported sprite CONTAINERS place their content.
                                const unsigned luFlagsA = lpN->mFlagsA;
                                unsigned luClipFlags = 0;
                                s32 liGrandKids = -1;
                                if (lpCI != nullptr && (lpCI->GetTypeTag() == 5 || lpCI->GetTypeTag() == 9))
                                {
                                    AptCharacterSpriteInstBase* lpChildSprite =
                                        static_cast<AptCharacterSpriteInstBase*>(lpCI);
                                    luClipFlags = lpChildSprite->mnClipActionFlags;
                                    AptDisplayListState* lpGK = lpChildSprite->mDisplayList.AsState();
                                    liGrandKids = 0;
                                    if (lpGK != nullptr)
                                        for (AptCIH* lpG = lpGK->mpFirst; lpG != nullptr && liGrandKids < 4096;
                                             lpG = lpG->GetDisplayListNext())
                                            ++liGrandKids;
                                }
                                char lacn[288];
                                std::snprintf(lacn, sizeof(lacn),
                                    "[AptRT] tick:   child node %p charInst=%p typeTag=%u char=%p charType=%d geom@+0x20=%p depth=%d dirty=%d clipFlags=0x%X grandKids=%d\n",
                                    (void*)lpN, (void*)lpCI, lpCI ? lpCI->GetTypeTag() : 0u,
                                    (void*)lpChar, liType, lpGeom,
                                    lpCI ? lpCI->GetDepth() : -1,
                                    (luFlagsA >> 25) & 1u, luClipFlags, liGrandKids);
                                CgsDev::Log::WriteToLog(lacn);
                            }
                        }
                    }
                }
            }

            if (s_iTickFrame < 3 || s_iTickFrame == 100)
            {
                char lacd[192];
                std::snprintf(lacd, sizeof(lacd),
                    "[AptRT] tick: frame=%d displayList nodes=%d childNodes=%d (tick result=%d)\n",
                    s_iTickFrame, liNodes, liChildNodes, liTickResult);
                CgsDev::Log::WriteToLog(lacd);
            }
            ++s_iTickFrame;
        }
        else if (lbProbeFrame)
        {
            CgsDev::Log::WriteToLog("[AptRT] frame: no faithful root CIH -- tick skipped (geometry "
                                    "fallback render only) (FLAG).\n");
        }

        // The RENDER moved to AptRuntimeRender (the proven immediate-mode Im2d path, driven by the
        // renderer hook with BrnRendererModule's mIm2dRenderer).
    }

    // -------------------------------------------------------------------------
    // AptRuntimeRender -- FAITHFUL render-tree flush. Called from BrnRendererModule::Render each
    // frame. Drives the homed X360 render-tree walk (AptRender @0x82AF33E8): it traverses the
    // current target's render-item tree (built by the tick's AptRenderTreeManager::Update_* calls)
    // and, per visible node, calls the render item's Render() virtual -> AptCharacter::render ->
    // AptHook_DrawShape -> gAptFuncs.pfnDrawRenderingUnit == CgsGui::AptCallbackRender::
    // DrawRenderingUnit -> CgsGui::AptRenderHandler::Render, which APPENDS the shape's mesh batches
    // to s_AptRenderBuffer.mCommandBuffer (mpImRenderers->mpIm2dRenderer, wired in step 4). We then
    // flush that buffer to D3D9: BeginRendering (open the block AptRenderHandler::Render appends
    // into) -> AptRender (walk fills it) -> EndRendering -> Swap -> Dispatch (the faithful PC D3D9
    // command re-issue -- the same ImRenderBuffer<V> path the loading screen/debug HUD dispatch).
    //
    // This RETIRES the invented direct-geometry fallback (RenderLoadedGeometryDirect + the word[4]
    // header hack): the movie now composes + renders through the real Apt display-list -> render-tree
    // -> D3D9 path. lpIm2d (BrnRendererModule's own live Im2d) is unused by the walk (the walk draws
    // through the Apt render handler's own buffer), but the D3D device it shares is already frame-open.
    //
    // Hard-gated: only runs once the runtime + AptAux render handler + the Apt render buffer are up
    // (s_bRenderBufferReady). Until the tick populates the render-root list the walk finds an empty
    // root and draws nothing -- a clean no-op, no AV.
    // -------------------------------------------------------------------------
    void AptRuntimeRender(CgsGraphics::Im2d* lpIm2d)
    {
        (void)lpIm2d;   // the walk draws through the Apt render handler's own command buffer
        if (!s_bRuntimeReady || !s_bMovieLoaded || !s_bAuxReady || !s_bRenderBufferReady)
            return;

        const bool lbFirst = !s_bFlushProbed;

        // Open the render block the Apt render handler appends into, walk the render tree (which
        // fills the buffer via AptRenderHandler::Render), close, then freeze + dispatch to D3D9.
        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrBuffer =
            s_AptRenderBuffer.mCommandBuffer;

        lrBuffer.BeginRendering();
        AptRender(/*nLayerMask*/ 0);   // 0 == all display layers
        lrBuffer.EndRendering();
        lrBuffer.Swap();               // freeze the write buffer for dispatch
        lrBuffer.Dispatch();           // re-issue every command to the D3D9 device (faithful PC path)
        ++s_iRenderFrame;

        if (lbFirst)
        {
            s_bFlushProbed = true;
            CgsDev::Log::WriteToLog(
                "[AptRT] render: render-tree walk (AptRender) -> D3D9 via ImRenderBuffer::Dispatch (OK; per-frame).\n");
        }
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
// =============================================================================
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
