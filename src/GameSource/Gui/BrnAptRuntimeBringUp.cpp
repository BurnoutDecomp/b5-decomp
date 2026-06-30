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

    // x64 TRANSCODE STATE (USER-CONFIRMED 2026-06-30): the .apt payload kept the console
    // 4-byte serialised pointer format and the resource backing is a HIGH x64 address, so the
    // in-place u32 FixUp is impossible (it was made a no-op in CgsAptDataHeader.cpp). Instead
    // we keep the FULL 64-bit resource base and treat every serialised u32 field as an OFFSET
    // relative to it, resolving `(T*)(base64 + offset)` at every level -- never storing a 64-bit
    // address back into a u32. s_uGeomOffset is mpGeomStruct (the offset of the GuiGeometryObject).
    uintptr_t                        s_uAptResourceBase = 0;    // m_baseResources[0] (the 64-bit load base)
    u32                              s_uGeomOffset      = 0;    // mpGeomStruct (offset of the geometry object)
    u32                              s_uAptResourceSize = 0;    // the AptData resource size (REAL bound for ResolveOff)
    bool                             s_bGeomResolved    = false;// a sane geometry object was found+validated
    bool                             s_bAptBufferHasFrame = false; // a render block was opened this frame (gate the flush)
    bool                             s_bRenderProbed    = false;// emitted the one-shot fine render probes yet
    bool                             s_bFlushProbed     = false;// emitted the one-shot fine flush probe yet
    s32                              s_iLastVertsSubmitted = 0; // verts submitted last frame (for the summary log)

    // The .apt serialised POINTER SIZE (USER design: the "1:7:<n>" descriptor's third value).
    //   4 = console 32-bit pointers (offset transcode); 8 = native x64 64-bit pointers (in-place).
    // The "Apt Data:1:7:4" descriptor lives in the BUNDLE DEBUG-NAME table, which the Pool does NOT
    // retain in the Entry after load -- so it is not in the loaded resource payload. We therefore
    // DISPATCH by structural validation: try the 4-byte interpretation, and if its top-level fields
    // are implausible try the 8-byte one; the validated size wins. (A future 8-byte bundle resolves
    // through the native path unchanged.) FLAG: the explicit descriptor field is unreachable post-
    // load, so the size is inferred structurally rather than read; 4-byte is the validated default.
    s32                              s_iAptPointerSize  = 4;

    // The movie pool backing (3 mem types). The title movie is ~1.6 MB; reserve 8 MB/type
    // (the bundle's main-memory resources + the heap node overhead). Static BSS storage.
    const u32 KU_MOVIE_POOL_BYTES = 8u * 1024u * 1024u;
    u8 s_moviePoolBacking[CgsResource::E_MEMTYPE_NUMTYPES][KU_MOVIE_POOL_BYTES];
    CgsResource::Pool s_MoviePoolStorage;

    // The Apt-data resource type id (X360 0x1E == 30; CgsResource::AptDataHeaderType::GetTypeID).
    const u32 KU_APTDATA_RESOURCE_TYPE_ID = 30u;

    // The X360 interpreter stack sizes (CgsGui::AptAux::InitializeApt @0x82848E50:
    // AptUpdateInitialize's v8 block -> the AptActionInterpreter init parms). The
    // interpreter's operand-stack capacity / call-stack depth are carried in that
    // param block (v8[1]=512 etc); the AptScriptFunctionBase register window count is
    // v8[3]=256. We size the operand stack generously (the engine reads iStackSize for
    // the operand stack and iCallStackDepth for the four call-depth stacks).
    const s32 KI_APT_STACK_SIZE       = 512;   // operand-stack capacity
    const s32 KI_APT_CALLSTACK_DEPTH  = 64;    // four call-depth stacks' capacity

    // The Apt allocator sizes -- VERBATIM from AptAllocatorInitialize @0x82ADD118's
    // single caller (InitializeApt @0x82848E50 passes AptAllocatorInitialize(0x10000,
    // 0x4000, 0x10000, 0x4000)): a1/a2 = AptValueGC main/overflow, a3/a4 = DOGMA
    // main/overflow. The DOGMA fixed-size params (minSize 4, maxSize 256, the three
    // free-item bookkeeping offsets 0/0/0, bTrackOutsideAllocations 1) are transcribed
    // from the @0x82ADD118 asm (li r6,4 / li r7,0x100 / li r8,0 / li r9,0 / li r10,0 /
    // stb r11(=1)).
    const size_t KU_DOGMA_MAIN     = 0x10000;  // 64 KB main pool
    const size_t KU_DOGMA_OVERFLOW = 0x4000;   // 16 KB overflow
    const size_t KU_GC_MAIN        = 0x10000;  // 64 KB GC main pool
    const size_t KU_GC_OVERFLOW    = 0x4000;   // 16 KB GC overflow

    // The single shared DOGMA fixed-size pool (the X360 off_8324D808 all five
    // gpApt*Pool aliases point at) + the AptValueGC pool (off_8324D834). Held as raw
    // storage we placement-construct so we control lifetime + can wire the globals.
    DOGMA_PoolManager*      s_pDogmaPool = nullptr;
    AptValueGC_PoolManager* s_pGCPool    = nullptr;


    // ---- the Apt allocator + interpreter globals the engine reads (declared in the
    //      Apt SDK; defined in AptGlobals.cpp / AptTarget.cpp; null until WE wire them,
    //      because the X360 AptInit that wires them is un-reconstructed) -------------
    void WireAllocatorGlobals()
    {
        // The five operand/pseudo/render/shared/single-list pool aliases all point at
        // the one shared DOGMA pool (faithful: X360 off_8324D808).
        gpAptOperandStackPool  = s_pDogmaPool;
        gpAptPseudoDataPool    = s_pDogmaPool;
        gpAptRenderManagerPool = s_pDogmaPool;
        gpAptSharedPtrPool     = s_pDogmaPool;
        gpAptSingleListPool    = s_pDogmaPool;

        // The non-GC value pool (AptDefine.h gpNonGCPoolManager) also aliases the DOGMA
        // pool; the GC value pool pointer (gpGCPoolManager) points at the AptValueGC pool.
        gpNonGCPoolManager = s_pDogmaPool;
        gpGCPoolManager    = s_pGCPool;

        // The type-erased GC-pool view the engine stamps (off_8324D834).
        gpAptValueGCPool = s_pGCPool;
    }
}

namespace BrnGui
{
    // Forward declarations (definitions are `static` later in this BrnGui namespace; called
    // from PlayMovie / Update which appear before their definitions). Must be in BrnGui (not
    // the anon namespace) so the in-namespace calls bind to the static definitions.
    static void ResolveMovieGeometry(CgsResource::Entry* lpEntry);
    static s32  RenderLoadedGeometryDirect(CgsGraphics::Im2d* lpIm2d);
    static void DumpResourceBytes(const char* lpcTag, void* lpBase, u32 luOffset,
                                  u32 luResourceSize, u32 luBytes);
    static renderengine::Texture* ResolveMeshTexture(const u32* lpMesh, s32 liTexMode);

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

        // ---- STEP 1: the Apt allocators (DOGMA fixed-size + AptValueGC pools) -----
        // StaticInitialize() computes the per-VFT min/max object sizes the GC pool
        // needs from byte_82144A18; then construct both pools with the real X360 sizes
        // and wire every global the engine reads off them.
        if (!s_bAllocatorReady)
        {
            // FLAG: byte_82144A18 (the per-VFT object-size table) is un-homed (zeroed in
            // AptGlobals.cpp), so StaticInitialize derives min/max GC item size 0. The GC
            // pool will therefore not size GC items correctly -- any AptValueGC allocation
            // is the first place a real movie tick would stop. We still run it (it sets the
            // tuning statics) so the path is faithful; the zeroed table is the FLAG.
            AptValueGC_PoolManager::StaticInitialize();

            static DOGMA_PoolManager s_DogmaStorage(KU_DOGMA_MAIN, KU_DOGMA_OVERFLOW,
                                                    /*minSize*/ 4, /*maxSize*/ 256,
                                                    /*nOffsetToStoreNextInFreeItem*/ 0,
                                                    /*bStoreFreeBlockSize*/ false,
                                                    /*nOffsetToStoreSizeInFreeItem*/ 0,
                                                    /*bTrackOutsideAllocations*/ true);
            s_pDogmaPool = &s_DogmaStorage;

            static AptValueGC_PoolManager s_GCStorage(KU_GC_MAIN, KU_GC_OVERFLOW);
            s_pGCPool = &s_GCStorage;

            WireAllocatorGlobals();
            s_bAllocatorReady = true;

            char lac[160];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] step1 allocators: DOGMA(main=0x%X ovf=0x%X)=%p, GC(main=0x%X ovf=0x%X)=%p, wired.\n",
                (unsigned)KU_DOGMA_MAIN, (unsigned)KU_DOGMA_OVERFLOW, (void*)s_pDogmaPool,
                (unsigned)KU_GC_MAIN, (unsigned)KU_GC_OVERFLOW, (void*)s_pGCPool);
            CgsDev::Log::WriteToLog(lac);
        }

        // ---- STEP 2: the AptActionInterpreter (the ActionScript VM stacks) -------
        // initialize() allocates the five {count,capacity,array} stacks from the
        // operand-stack pool (now non-null). Needs gpAptOperandStackPool wired (step 1).
        if (!s_bInterpreterReady)
        {
            if (gpAptOperandStackPool == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] step2 interpreter: SKIP -- operand-stack pool null (allocator wiring failed).\n");
            }
            else
            {
                AptInitParmsT lParms;
                std::memset(&lParms, 0, sizeof(lParms));
                lParms.iStackSize          = KI_APT_STACK_SIZE;
                lParms.iCallStackDepth     = KI_APT_CALLSTACK_DEPTH;
                lParms.mbSkipTraceBytecodes = 1;   // trace bytecodes skipped on a release host

                // FLAG: AptActionInterpreter::initialize tail-calls AptScriptFunctionBase_
                // InitializeStaticData (the AS register window) -- that path reaches the
                // register/frame machinery which is only partly reconstructed. If it faults
                // it does so HERE; this is logged before + after so the run-log brackets it.
                CgsDev::Log::WriteToLog("[AptRT] step2 interpreter: calling AptActionInterpreter::initialize ...\n");
                gAptActionInterpreter.initialize(&lParms);
                s_bInterpreterReady = true;
                char lac[128];
                std::snprintf(lac, sizeof(lac),
                    "[AptRT] step2 interpreter: initialized (stack=%d callstack=%d).\n",
                    KI_APT_STACK_SIZE, KI_APT_CALLSTACK_DEPTH);
                CgsDev::Log::WriteToLog(lac);
            }
        }

        // ---- STEP 3: the Apt render buffer (the D3D9 2D buffer the engine fills) --
        // Construct + Prepare the ImRenderBuffer<V> that AptRenderHandler::Render
        // appends to (and we flush via Dispatch each frame). Prepare carves its
        // command + vertex storage from the RenderWare default resource allocator.
        if (!s_bRenderBufferReady)
        {
            s_AptRenderBuffer.mu32Head = 0;
            s_AptRenderBuffer.mCommandBuffer.Construct();

            rw::IResourceAllocator* lpAllocator = rw::ResourceAllocatorRegistry::GetDefaultAllocator();
            if (lpAllocator == nullptr)
            {
                // FLAG: no RW default allocator yet -> the buffer cannot carve its storage.
                // Defer (the renderer is up by the loading screen, so this is normally fine).
                CgsDev::Log::WriteToLog("[AptRT] step3 renderbuffer: SKIP -- rw default allocator null (deferred).\n");
            }
            else
            {
                // 256 KB command stream + 256 KB vertex stream (generous for a single
                // boot/title movie's per-frame geometry). failGracefully=true so a carve
                // failure returns false rather than asserting.
                const bool lbOk = s_AptRenderBuffer.mCommandBuffer.Prepare(
                    256u * 1024u, 256u * 1024u, lpAllocator, /*failGracefully*/ true);
                s_bRenderBufferReady = lbOk;
                CgsDev::Log::WriteToLog(lbOk
                    ? "[AptRT] step3 renderbuffer: Construct+Prepare ok (256KB cmd / 256KB vtx).\n"
                    : "[AptRT] step3 renderbuffer: Prepare FAILED (carve) -- buffer unusable (FLAG).\n");
            }
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

        // ---- STEP 5: the AptTarget context (the engine's per-process director) ----
        // Create the Apt context (AptCreateTargetInstance) + select it as current
        // (AptChangeTargetInstance) so GetTarget() returns a live context -- the X360
        // does exactly this inside InitializeApt (`*(a1+8)=AptCreateTargetInstance(v7);
        // AptChangeTargetInstance()`), and those two are now HOMED in AptTarget.cpp.
        // The create-params block is the v7[8] InitializeApt @0x82848E50 builds:
        //   v7 = {0, 1, 8, 8, 0, 512, 0, 0}
        // The AptTarget ctor reads config from p[4]/p[7]/p[2]/p[1]/p[0]/p[3] and the
        // animation director (MakeAptAnimationTarget) from words 0/1/2/3/5 (p[5]=512).
        if (!s_bTargetReady)
        {
            // Pre-existing context? (idempotent: don't create a second instance.)
            AptTarget* lpExisting = GetTarget();
            if (lpExisting != nullptr)
            {
                s_bTargetReady = true;
            }
            else if (gpAptPseudoDataPool == nullptr)
            {
                CgsDev::Log::WriteToLog("[AptRT] step5 target: SKIP -- DOGMA pool null (allocator wiring failed).\n");
            }
            else
            {
                static const u32 s_auAptCreateParams[8] = { 0u, 1u, 8u, 8u, 0u, 512u, 0u, 0u };  // InitializeApt v7
                CgsDev::Log::WriteToLog("[AptRT] step5 target: AptCreateTargetInstance + AptChangeTargetInstance ...\n");

                // FLAG: the AptTarget ctor builds an AptAnimationTarget director + an AptLoader
                // + an AptLinker (all homed) from the shared DOGMA pool. If any sub-construct
                // crosses an un-homed callee it faults HERE; bracketed by the probes so the
                // run-log pinpoints it.
                AptTarget* lpCreated = AptCreateTargetInstance(s_auAptCreateParams);
                if (lpCreated == nullptr)
                {
                    CgsDev::Log::WriteToLog("[AptRT] step5 target: AptCreateTargetInstance returned null "
                                            "(pool carve failed) -- FLAG.\n");
                }
                else
                {
                    AptChangeTargetInstance(lpCreated);   // sets gpAptTarget/TLS + the GetTarget() mirror
                    AptTarget* lpNow = GetTarget();
                    s_bTargetReady = (lpNow != nullptr);
                    char lac[200];
                    std::snprintf(lac, sizeof(lac),
                        "[AptRT] step5 target: created %p, gpAptTarget set; GetTarget()=%p; "
                        "director=%p loader=%p linker=%p.\n",
                        (void*)lpCreated, (void*)lpNow,
                        (void*)lpCreated->mpAnimationTarget, (void*)lpCreated->mpLoader,
                        (void*)lpCreated->mpLinker);
                    CgsDev::Log::WriteToLog(lac);
                }
            }
        }

        // The runtime is "ready" (for channel-41 routing) once the allocator + render
        // buffer + AptAux host are up. The target now being live (step 5) lets PlayMovie +
        // the per-frame tick reach a real GetTarget(); a failed target is a downstream bail,
        // not a bring-up failure.
        s_bRuntimeReady = s_bAllocatorReady && s_bAuxReady;
        CgsDev::Log::WriteToLog(s_bRuntimeReady
            ? (s_bTargetReady
                ? "[AptRT] bring-up: READY (alloc+interp+aux+renderbuf+TARGET up; GetTarget() live).\n"
                : "[AptRT] bring-up: READY (alloc+interp+aux+renderbuf up; target FLAG'd -- see step5).\n")
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
        if (lpacMovieName == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: null movie name -- ignored.\n");
            return;
        }

        // LOAD-ONCE GUARD: the channel-41 event re-fires EVERY frame (BootLegal keeps the
        // movie playing), so without this guard the bundle would be (re)loaded thousands of
        // times. Load each distinct movie exactly once; once loaded, ignore the re-fires.
        if (s_bMovieRequested && std::strncmp(s_acLoadedMovieName, lpacMovieName,
                                              sizeof(s_acLoadedMovieName) - 1) == 0)
            return;   // same movie already attempted -- silent (avoids per-frame log spam)

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

        // ---- SYNCHRONOUS bundle load: GUIAPT\<NAME>.bundle -----------------------------
        // The movie is a platform-4 bundle at GUIAPT\<NAME>.bundle (e.g. Title_Screen02 ->
        // GUIAPT\TITLE_SCREEN02.bundle). Load it through BundleLoader::LoadBundle (the same
        // async-FS / DeviceManager path the FSM + VIDEOLIST bundles already use); the bundle's
        // single AptData resource (type 0x1E == 30) is FixUp'd by the registered
        // CgsResource::AptDataHeaderType handler into a live CgsGui::AptDataHeader whose
        // mpGeomStruct is the relocated renderable GuiGeometryObject.
        CgsResource::RegisterAllResourceTypes();   // idempotent: ensure AptDataHeaderType (0x1E) is live

        // Build "GUIAPT/<UPPERCASE NAME>.BUNDLE" (the loader passes the path verbatim to
        // CreateFileA; Windows FS is case-insensitive, the convention is upper + '/').
        char lacBundlePath[160];
        std::snprintf(lacBundlePath, sizeof(lacBundlePath), "GUIAPT/%s.BUNDLE", s_acLoadedMovieName);
        for (char* lpc = lacBundlePath + 7; *lpc; ++lpc)   // upper-case the movie-name segment
            if (*lpc >= 'a' && *lpc <= 'z') *lpc = static_cast<char>(*lpc - 'a' + 'A');

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

        // CORRECTED HEADER LAYOUT (verified against the real TITLE_SCREEN02.bundle bytes, this
        // session). The reconstructed CgsGui::AptDataHeader puts mpGeomStruct at +12, but the REAL
        // serialised .apt header has SIX leading offset words and the GuiGeometryObject lives at
        // word[4] (+16), NOT word[3] (+12). The verified field run for Title_Screen02:
        //   word[0]=0x30   -> "Title_Screen02"      (mpacMovieName)
        //   word[1]=0x20   -> "Title_Screen02"      (mpAptData / asset-name block)
        //   word[2]=0x40   -> "Apt Data:1:7:4"+desc (mpConstData -- the AptConstFile, 1:7:4 here)
        //   word[3]=0x7100 -> "Apt constant file"+t (a SECOND const block -- NOT the geometry)
        //   word[4]=0x7200 -> {numFiles=7,numTexPages=2,fileTableOff=0x7210}  <-- GuiGeometryObject
        //   word[5]=0x7838 -> the import table (4 imports at 0x7840)
        // So read the geometry offset from word[4] directly off the resource bytes. (The struct's
        // mpGeomStruct/+12 is left to the FLAG'd no-op FixUp; we do not trust it for the geometry.)
        const u32* lpHeaderWords = reinterpret_cast<const u32*>(lpRes);
        s_uGeomOffset = lpHeaderWords[4];   // +16: the real GuiGeometryObject offset (verified)
        std::snprintf(lac, sizeof(lac),
            "[AptRT] xcode: base=0x%016llX hdr[0..5]=0x%X 0x%X 0x%X 0x%X 0x%X 0x%X (geomOff=word[4]=0x%X)\n",
            (unsigned long long)s_uAptResourceBase,
            lpHeaderWords[0], lpHeaderWords[1], lpHeaderWords[2],
            lpHeaderWords[3], lpHeaderWords[4], lpHeaderWords[5], s_uGeomOffset);
        CgsDev::Log::WriteToLog(lac);

        // HEX-DUMP PROBE (read-only, bounded): dump the first 64 bytes of the AptData resource (the
        // AptDataHeader region) so the run shows the REAL on-disk header layout. The offline bundle
        // parse showed entry layout ambiguity, so this is the ground truth.
        DumpResourceBytes("hdr@+0",   lpRes, 0,
                          lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size, 64);
        // And the region mpGeomStruct points at (if in range), to see the GuiGeometryObject bytes.
        DumpResourceBytes("geom@off", lpRes, s_uGeomOffset,
                          lpEntry->mResourceDescriptor.m_baseResourceDescriptors[0].m_size, 48);

        s_bMovieLoaded = true;

        // Resolve + sanity-validate the geometry object now (one-time), choosing the pointer-size
        // path (4-byte transcode vs 8-byte native). Sets s_bGeomResolved if sane geometry is found.
        ResolveMovieGeometry(lpEntry);

        // FLAG: the FULL movie instantiation (turn mpAptData's AptCharacterAnimation into a live
        // root AptCIH attached to GetTarget()->mpAnimationTarget's root display list, then run its
        // timeline + ActionScript per frame) is the DEEPER path and is UN-HOMED in three places:
        //   (1) the loader async-completion (AptCharacterAnimationInst needs a loaded AptFile;
        //       the streamer + AptFile state machine are un-homed),
        //   (2) AptActionInterpreter runStream is a stub (no AS execution),
        //   (3) the render-tree walk (AptRenderTreeManager::Update_SetRootItem/Update_ItemInserted
        //       are empty stubs; no top-level root->child->sibling Render walk exists).
        // Instead this slice renders the movie's STATIC geometry directly (AptRuntimeUpdate ->
        // AptRenderHandler::Render per GuiGeometryFile), which exercises the real geometry ->
        // AptIm2dRenderBuffer -> D3D9 pipeline without those un-homed layers.
    }

    // =========================================================================
    // x64 GEOMETRY TRANSCODE WALK.
    //
    // The serialised gui-geometry tree (GuiGeometryObject -> file table -> GuiGeometryFile
    // -> mesh table -> GuiGeometryMesh -> vertex table -> vertex run) is a relocatable blob:
    // every "pointer" is a file-relative OFFSET from the resource base. The homed
    // AptRenderHandler::Render + the homed GuiGeometry*::FixUp read those fields as u32 and
    // cast them to absolute addresses -- which only works when the data is in the low 4 GB
    // (it is not on x64). So we do NOT use those; we resolve every offset against the FULL
    // 64-bit base (s_uAptResourceBase) and submit the vertex runs directly to the command
    // buffer. Dual-path by pointer size (4 = console offsets, 8 = native 64-bit fields).
    //
    // 4-BYTE SERIALISED STRIDES (console .apt, little-endian post-convert):
    //   GuiGeometryObject : {u32 numFiles, u32 numTexPages, u32 fileTableOff}            (12B)
    //   file table        : u32[numFiles] of GuiGeometryFile offsets
    //   GuiGeometryFile   : {u32 id, u32 numMeshes, u32 meshTableOff}                    (12B)
    //   mesh table        : u32[numMeshes] of GuiGeometryMesh offsets
    //   GuiGeometryMesh   : {s32 type, s32 texMode, s32 texId, u32 texPtr,
    //                        u32 numVerts, u32 vertTableOff}                             (24B)
    //   vertex table      : u32[...] -- first entry is the vertex-run offset
    //   vertex run        : Basic2dColouredTexturedVertex[numVerts]                      (20B each)
    // The 8-byte path widens each offset/pointer field to 64-bit (native x64 layout) and
    // resolves it as an absolute pointer (relocated in place by the load) -- not reached by
    // this 4-byte bundle, but wired so a future 1:7:8 bundle renders unchanged.
    // =========================================================================

    // Resolve a serialised offset to a real 64-bit pointer, bounds-checked against the
    // resource size so a garbage offset logs + yields null instead of AV'ing.
    template <typename T>
    static T* ResolveOff(u32 luOffset, u32 luResourceSize)
    {
        if (luOffset == 0 || luOffset >= luResourceSize)
            return nullptr;
        return reinterpret_cast<T*>(s_uAptResourceBase + luOffset);
    }

    // Read the geometry object's three leading u32s at s_uGeomOffset and sanity-check them.
    // Returns true (and fills the out params) if the 4-byte interpretation looks plausible.
    static bool ValidateGeom4(u32 luResourceSize, u32* lpuNumFiles, u32* lpuFileTableOff)
    {
        const u32* lpObj = ResolveOff<const u32>(s_uGeomOffset, luResourceSize);
        if (lpObj == nullptr)
            return false;
        const u32 luNumFiles     = lpObj[0];
        const u32 luFileTableOff = lpObj[2];
        // Plausible: a handful of files, a file-table offset inside the resource.
        if (luNumFiles == 0 || luNumFiles > 4096u)
            return false;
        if (luFileTableOff == 0 || luFileTableOff >= luResourceSize)
            return false;
        *lpuNumFiles     = luNumFiles;
        *lpuFileTableOff = luFileTableOff;
        return true;
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

    // Resolve a textured mesh's renderengine::Texture* from the import-written pointer.
    //
    // For a texMode==1 mesh, the bundle's import-resolve (Pool::ResolveImportForEntry) wrote the
    // imported TEXTURE resource's m_baseResources[0] -- which IS a fully-realised renderengine::
    // Texture (its FixUp ran first and created mpD3DTexture via Texture::Create) -- as a 64-bit
    // pointer into the mesh's texPtr slot (mesh+12). On x64 that 8-byte write also occupies mesh+16
    // (the old numVerts slot; we recover numVerts from the vert table instead). So the texture is
    // simply `*(renderengine::Texture**)(mesh+12)` == (lpMesh[3] | lpMesh[4]<<32). The raster has no
    // x64 self-pointer issue (only mpD3DTexture, set at native width). Im2d::SetTexture binds
    // texture->mpD3DTexture directly.
    //
    // DEFENSIVE: validate the pointer looks like a live texture (non-null, mpD3DTexture non-null)
    // before returning it; otherwise fall back to null (untextured vertex-colour) so a bad/unresolved
    // import never AVs in the draw. mode-0 (texId 6969 sentinel) always returns null.
    static renderengine::Texture* ResolveMeshTexture(const u32* lpMesh, s32 liTexMode)
    {
        if (liTexMode != 1)
            return nullptr;   // mode 0 / unknown -> untextured (vertex colour)

        const u64 luLow  = static_cast<u64>(lpMesh[3]);   // mesh+12 low32
        const u64 luHigh = static_cast<u64>(lpMesh[4]);   // mesh+16 high32 (the import's 8-byte write)
        const uintptr_t luTexPtr = static_cast<uintptr_t>(luLow | (luHigh << 32));
        if (luTexPtr == 0)
            return nullptr;   // import unresolved -> untextured fallback

        renderengine::Texture* lpTexture = reinterpret_cast<renderengine::Texture*>(luTexPtr);
        // The pointer must look like a heap/resource address (reject obviously-bad low values) and
        // carry a live D3D texture, else fall back to untextured (no AV).
        if (luTexPtr < 0x10000u)
            return nullptr;
        if (lpTexture->mpD3DTexture == nullptr)
            return nullptr;   // texture not realised (FixUp didn't create it) -> untextured fallback
        return lpTexture;
    }

    // Resolve + validate the movie geometry once at load time, choosing the pointer-size path.
    static void ResolveMovieGeometry(CgsResource::Entry* lpEntry)
    {
        s_bGeomResolved = false;
        if (s_uAptResourceBase == 0 || s_uGeomOffset == 0)
        {
            CgsDev::Log::WriteToLog("[AptRT] xcode: no geometry offset (mpGeomStruct==0) -- bail (FLAG).\n");
            return;
        }
        const u32 luResourceSize =
            lpEntry->mResourceDescriptor.m_baseResourceDescriptors[CgsResource::E_MEMTYPE_MAINMEMORY].m_size;

        char lac[200];
        u32 luNumFiles = 0, luFileTableOff = 0;

        // DISPATCH: try 4-byte (this bundle's format). The explicit "1:7:<n>" descriptor lives in
        // the bundle debug-name table (discarded by the Pool), so we infer structurally: if the
        // 4-byte read is plausible, it is the console 4-byte format. (A native 1:7:8 bundle would
        // fail this 4-byte read -- its 64-bit fields make numFiles/offset implausible -- and take
        // the 8-byte branch.)
        if (ValidateGeom4(luResourceSize, &luNumFiles, &luFileTableOff))
        {
            s_iAptPointerSize = 4;
            s_bGeomResolved   = true;
            std::snprintf(lac, sizeof(lac),
                "[AptRT] xcode: aptPtrSize=4 -> transcode. geom@0x%X resSize=%u files=%u fileTableOff=0x%X\n",
                s_uGeomOffset, luResourceSize, luNumFiles, luFileTableOff);
            CgsDev::Log::WriteToLog(lac);

            // One-time probe of file 0 + its first mesh, so the run shows real geometry resolving.
            const u32* lpFileTable = ResolveOff<const u32>(luFileTableOff, luResourceSize);
            if (lpFileTable != nullptr)
            {
                const u32* lpFile0 = ResolveOff<const u32>(lpFileTable[0], luResourceSize);
                if (lpFile0 != nullptr)
                {
                    const u32 luNumMeshes0   = lpFile0[1];
                    const u32 luMeshTableOff0 = lpFile0[2];
                    u32 luVerts0 = 0, luMeshType0 = 0xFFFFFFFFu, luVertOff0 = 0;
                    const u32* lpMeshTable0 = ResolveOff<const u32>(luMeshTableOff0, luResourceSize);
                    if (lpMeshTable0 != nullptr)
                    {
                        const u32* lpMesh0 = ResolveOff<const u32>(lpMeshTable0[0], luResourceSize);
                        if (lpMesh0 != nullptr)
                        {
                            luMeshType0 = lpMesh0[0];
                            luVerts0    = lpMesh0[4];
                            luVertOff0  = lpMesh0[5];
                        }
                    }
                    std::snprintf(lac, sizeof(lac),
                        "[AptRT] xcode: file0 id=0x%X meshes=%u | mesh0 type=%u verts=%u vertTableOff=0x%X\n",
                        lpFile0[0], luNumMeshes0, luMeshType0, luVerts0, luVertOff0);
                    CgsDev::Log::WriteToLog(lac);
                }
            }
            return;
        }

        // 8-byte native path: the fields are real 64-bit pointers (relocated in place by the load).
        // FLAG: not reached by the current 4-byte bundle; wired so a converted 1:7:8 bundle resolves
        // its geometry natively. We validate the 64-bit numFiles at the geom object's first dword.
        // (The 8-byte object layout is {u32 numFiles, u32 numTexPages, u64 fileTablePtr}.)
        const u32* lpObj8 = ResolveOff<const u32>(s_uGeomOffset, luResourceSize);
        if (lpObj8 != nullptr && lpObj8[0] != 0 && lpObj8[0] <= 4096u)
        {
            s_iAptPointerSize = 8;
            s_bGeomResolved   = true;
            CgsDev::Log::WriteToLog("[AptRT] xcode: aptPtrSize=8 -> in-place (native x64). "
                                    "8-byte geometry walk wired (FLAG: untested -- no 8-byte bundle yet).\n");
            return;
        }

        CgsDev::Log::WriteToLog("[AptRT] xcode: geometry failed BOTH 4-byte and 8-byte validation "
                                "(garbage offsets / unexpected format) -- bail (FLAG). Default would be 4-byte.\n");
    }

    // -------------------------------------------------------------------------
    // RenderLoadedGeometryDirect -- the achievable render milestone. Walk the loaded movie's
    // geometry (resolving every serialised offset against the FULL 64-bit base) and draw each
    // mesh's vertex run through the PROVEN immediate-mode renderer lpIm2d (CgsGraphics::Im2d --
    // the SAME one the loading screen + debug HUD draw through). Returns the number of meshes drawn.
    //
    // RENDER PATH CHOICE: the raw ImRenderBuffer<V> double-buffer path (Clear/BeginRendering/Swap/
    // Dispatch) AV'd on first runtime use -- it was reconstructed but NEVER exercised (the working
    // 2D paths all use the Im2d IMMEDIATE wrapper). So we draw through lpIm2d->Render directly: it
    // immediately folds each run to screen-space and issues DrawPrimitiveUP -- no double-buffer, no
    // Dispatch. lpIm2d is BrnRendererModule's own mIm2dRenderer (battle-tested), passed in by the
    // render hook. Im2d::Render ALWAYS draws a triangle STRIP (it ignores the primitive type) and
    // caps at 64 verts/call; a type-0 (tri-LIST) run is drawn as separate 3-vertex strips (each ==
    // one triangle) so the list tessellation is exact. Mode-1 meshes bind their real imported texture
    // (ResolveMeshTexture); mode-0 draw untextured (vertex colour). FLAG: the per-shape AS transforms/
    // visibility (the final animated layout) are absent -- geometry is DRAWN at its authored coords,
    // textured where a texture resolves, but not animated (that needs the un-homed display-list/runStream).
    // -------------------------------------------------------------------------
    static s32 RenderLoadedGeometryDirect(CgsGraphics::Im2d* lpIm2d)
    {
        if (!s_bGeomResolved || !s_bAuxReady || lpIm2d == nullptr)
            return 0;
        if (s_iAptPointerSize != 4)
            return 0;   // FLAG: 8-byte native walk untested (no 1:7:8 bundle) -- submit nothing.

        const bool lbProbe = !s_bRenderProbed;

        // No texture (untextured -> vertex colour); the Im2d untextured path drives the stage from
        // DIFFUSE. FLAG: real per-mesh textures are the follow-on.
        if (lbProbe) CgsDev::Log::WriteToLog("[AptRT] render: enter -> SetTexture(null) ...\n");
        lpIm2d->SetTexture(static_cast<renderengine::Texture*>(nullptr));

        // REAL resource-size bound: every offset must land INSIDE the AptData resource.
        const u32 luBound = (s_uAptResourceSize != 0u) ? s_uAptResourceSize : KU_MOVIE_POOL_BYTES;

        const u32* lpObj = ResolveOff<const u32>(s_uGeomOffset, luBound);
        if (lpObj == nullptr)
            return 0;
        const u32 luNumFiles     = lpObj[0];
        const u32 luFileTableOff = lpObj[2];
        if (luNumFiles == 0 || luNumFiles > 4096u)
            return 0;
        const u32* lpFileTable = ResolveOff<const u32>(luFileTableOff, luBound);
        if (lpFileTable == nullptr)
            return 0;
        if (lbProbe) CgsDev::Log::WriteToLog("[AptRT] render: enter mesh loop ...\n");

        s32 liMeshesSubmitted = 0;
        s32 liVertsSubmitted   = 0;
        for (u32 luFile = 0; luFile < luNumFiles; ++luFile)
        {
            const u32* lpFile = ResolveOff<const u32>(lpFileTable[luFile], luBound);
            if (lpFile == nullptr)
            {
                if (lbProbe) { char lf[96]; std::snprintf(lf, sizeof(lf),
                    "[AptRT] render: file%u SKIP (file offset out of range)\n", luFile);
                    CgsDev::Log::WriteToLog(lf); }
                continue;
            }
            const u32 luNumMeshes   = lpFile[1];
            const u32 luMeshTableOff = lpFile[2];
            if (lbProbe)
            {
                char lf[128];
                std::snprintf(lf, sizeof(lf), "[AptRT] render: file%u numMeshes=%u meshTableOff=0x%X\n",
                              luFile, luNumMeshes, luMeshTableOff);
                CgsDev::Log::WriteToLog(lf);
            }
            if (luNumMeshes == 0 || luNumMeshes > 65536u)
                continue;
            const u32* lpMeshTable = ResolveOff<const u32>(luMeshTableOff, luBound);
            if (lpMeshTable == nullptr)
                continue;

            for (u32 luMesh = 0; luMesh < luNumMeshes; ++luMesh)
            {
                const u32* lpMesh = ResolveOff<const u32>(lpMeshTable[luMesh], luBound);
                if (lpMesh == nullptr)
                    continue;
                const s32 liMeshType    = static_cast<s32>(lpMesh[0]);   // +0
                const s32 liTexMode     = static_cast<s32>(lpMesh[1]);   // +4
                u32       luNumVerts    = lpMesh[4];                     // +16 (CLOBBERED for mode-1, see below)
                const u32 luVertTableOff = lpMesh[5];                    // +20

                // The vertex table: ONE u32 offset per vertex (each points at a Basic2dColoured
                // TexturedVertex; consecutive entries are 20 bytes apart). We always derive the run
                // start + the COUNT from this table, NOT from mesh+16 -- because for a textured
                // (texMode==1) mesh the bundle's import-resolve wrote an 8-byte texture pointer into
                // the texPtr slot (mesh+12), and on x64 that 8-byte write CLOBBERS mesh+16 (numVerts)
                // with the high 32 bits of the pointer (-> a huge garbage count). So count vertices by
                // walking the vert table while each entry is a sane, in-range, monotonically-advancing
                // vertex offset. This recovers the real count for both clean + import-clobbered meshes.
                const u32* lpVertTable = ResolveOff<const u32>(luVertTableOff, luBound);
                if (lpVertTable == nullptr)
                {
                    if (lbProbe) { char ls[128]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u SKIP (vertTableOff 0x%X out of range)\n",
                        luFile, luMesh, luVertTableOff); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const u32 luVertStride = static_cast<u32>(sizeof(CgsGraphics::Basic2dColouredTexturedVertex));
                const u32 luVertRunOff = lpVertTable[0];
                // Count consecutive verts (table[i] == table[0] + i*stride, all in-range), capped at 64
                // (Im2d's per-call limit) -- this is the AUTHORITATIVE count, replacing the clobbered field.
                // Bound the TABLE read itself to the resource so reading lpVertTable[lv] never over-reads.
                u32 luCountedVerts = 0;
                for (u32 lv = 0; lv < 64u; ++lv)
                {
                    // The table entry lpVertTable[lv] lives at (luVertTableOff + lv*4); stop before it
                    // would read past the resource.
                    if (luVertTableOff + (lv + 1u) * 4u > luBound)
                        break;
                    const u32 luVOff = lpVertTable[lv];
                    if (luVOff != luVertRunOff + lv * luVertStride)
                        break;
                    if (luVOff == 0 || luVOff + luVertStride > luBound)
                        break;
                    ++luCountedVerts;
                }
                luNumVerts = luCountedVerts;

                if (luNumVerts < 3u)
                {
                    if (lbProbe) { char ls[160]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u type=%d texMode=%d SKIP (recovered verts=%u < 3)\n",
                        luFile, luMesh, liMeshType, liTexMode, luNumVerts); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const u32 luRunBytes = luNumVerts * luVertStride;
                if (luVertRunOff == 0 || luVertRunOff >= luBound || luRunBytes > luBound - luVertRunOff)
                {
                    if (lbProbe) { char ls[160]; std::snprintf(ls, sizeof(ls),
                        "[AptRT] render: file%u mesh%u SKIP (vert run 0x%X+%u out of range)\n",
                        luFile, luMesh, luVertRunOff, luRunBytes); CgsDev::Log::WriteToLog(ls); }
                    continue;
                }
                const CgsGraphics::Basic2dColouredTexturedVertex* lpVerts =
                    reinterpret_cast<const CgsGraphics::Basic2dColouredTexturedVertex*>(
                        s_uAptResourceBase + luVertRunOff);

                // ---- TEXTURE: bind the real renderengine::Texture* for mode-1 meshes. The bundle's
                // import-resolve wrote the imported texture resource's m_baseResources[0] (a realised
                // renderengine::Texture, mpD3DTexture already created by its FixUp) as a 64-bit pointer
                // into the mesh's texPtr slot (mesh+12). ResolveMeshTexture reassembles + validates it;
                // Im2d::SetTexture binds texture->mpD3DTexture. mode-0 / unresolved -> null (untextured
                // vertex colour). No AV (validated before binding).
                renderengine::Texture* lpMeshTexture = ResolveMeshTexture(lpMesh, liTexMode);
                lpIm2d->SetTexture(lpMeshTexture);

                if (lbProbe)
                {
                    char lacm[224];
                    std::snprintf(lacm, sizeof(lacm),
                        "[AptRT] render: file%u mesh%u type=%d texMode=%d verts=%u(recovered) runOff=0x%X "
                        "tex=%p v0=(%.1f,%.1f) -> drawn\n",
                        luFile, luMesh, liMeshType, liTexMode, luNumVerts, luVertRunOff,
                        (void*)lpMeshTexture, lpVerts[0].mv2Pos.x, lpVerts[0].mv2Pos.y);
                    CgsDev::Log::WriteToLog(lacm);
                }

                // Draw through the PROVEN immediate path. Im2d::Render ALWAYS draws a triangle STRIP
                // (it ignores the primitive type). For a type-0 tri-LIST run we draw it as separate
                // 3-vertex strips (each == one triangle), reproducing the list exactly. Type 1/2/other
                // draw as one strip over the run.
                if (liMeshType == 0)
                {
                    for (u32 luBase = 0; luBase + 3u <= luNumVerts; luBase += 3u)
                        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), lpVerts + luBase, 3u);
                }
                else
                {
                    lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), lpVerts, luNumVerts);
                }
                ++liMeshesSubmitted;
                liVertsSubmitted += static_cast<s32>(luNumVerts);
            }
        }

        if (lbProbe)
        {
            s_bRenderProbed = true;
            char lacd[128];
            std::snprintf(lacd, sizeof(lacd),
                "[AptRT] render: DONE first pass -- %d meshes (%d verts) drawn via Im2d.\n",
                liMeshesSubmitted, liVertsSubmitted);
            CgsDev::Log::WriteToLog(lacd);
        }
        s_iLastVertsSubmitted = liVertsSubmitted;
        return liMeshesSubmitted;
    }

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

        // ---- TICK (FLAG: un-homed) -----------------------------------------------------
        // The faithful per-frame tick (AptLoader::Update -> AptLinker::Update ->
        // AptAnimationTarget RunActions/TickIntervalTimers/TickNewInsts + the display-list
        // tick + the AS frame actions) requires a live root AptCIH bound to a loaded AptFile
        // -- which needs the un-homed loader async-completion + the (stubbed) AptActionInterpreter
        // runStream. Not driven here (it would fault on the partial state). When those land,
        // tick the director + root clip here. This is the documented tick gap.
        if (lbProbeFrame)
            CgsDev::Log::WriteToLog("[AptRT] frame: tick skipped -- timeline/AS path un-homed "
                                    "(loader-completion + runStream) (FLAG).\n");

        // The RENDER moved to AptRuntimeRender (the proven immediate-mode Im2d path, driven by the
        // renderer hook with BrnRendererModule's mIm2dRenderer). AptRuntimeUpdate now only ticks --
        // it must NOT touch the never-exercised raw ImRenderBuffer<V> double-buffer path (which AV'd).
    }

    // -------------------------------------------------------------------------
    // AptRuntimeRender -- draw the loaded movie's geometry THROUGH THE PROVEN IMMEDIATE PATH.
    // Called from BrnRendererModule::Render each frame with that module's mIm2dRenderer (a live,
    // battle-tested CgsGraphics::Im2d that the loading screen + debug HUD already draw through).
    // This REPLACES the raw ImRenderBuffer<V> Clear/BeginRendering/Swap/Dispatch path that AV'd on
    // first use. Bracketed by Im2d BeginRendering/EndRendering (installs the 2D D3D state); the
    // per-mesh runs draw immediately via DrawPrimitiveUP. Hard-gated + bounded -> no AV / no-op when
    // unresolved.
    // -------------------------------------------------------------------------
    void AptRuntimeRender(CgsGraphics::Im2d* lpIm2d)
    {
        if (!s_bRuntimeReady || !s_bMovieLoaded || !s_bGeomResolved || lpIm2d == nullptr)
            return;

        // (s_iFrameCounter is advanced by AptRuntimeUpdate, which runs before this each frame.)
        const bool lbProbeFrame = (s_iFrameCounter % 30) == 1;

        // Install the 2D immediate render state (no depth/cull, alpha-blend, vertex-colour modulate),
        // draw every mesh, then close the block. Im2d::BeginRendering no-ops cleanly when gDevice is
        // null, and Im2d::Render guards null/short runs -- so this stays up even mid-bring-up.
        lpIm2d->BeginRendering();
        const s32 liUnits = RenderLoadedGeometryDirect(lpIm2d);
        lpIm2d->EndRendering();

        if (lbProbeFrame)
        {
            char lac[128];
            std::snprintf(lac, sizeof(lac),
                "[AptRT] frame %d: drew %d quads (%d verts) via Im2d immediate path.\n",
                s_iFrameCounter, liUnits, s_iLastVertsSubmitted);
            CgsDev::Log::WriteToLog(lac);
        }
    }
}
