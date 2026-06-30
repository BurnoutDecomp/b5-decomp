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
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsImRenderBufferTemplate.h" // ImRenderBuffer<V>
#include "rw/rwcore_structs.h"                                // rw::ResourceAllocatorRegistry::GetDefaultAllocator

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
    bool        s_bMovieRequested        = false;   // a PlayAptMovie request is in flight
    bool        s_bMovieLoaded           = false;   // the movie's AptFile reached "loaded"
    s32         s_iFrameCounter          = 0;       // per-frame probe throttle

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

        std::strncpy(s_acLoadedMovieName, lpacMovieName, sizeof(s_acLoadedMovieName) - 1);
        s_acLoadedMovieName[sizeof(s_acLoadedMovieName) - 1] = '\0';
        s_bMovieRequested = true;

        char lac[160];
        std::snprintf(lac, sizeof(lac), "[AptRT] PlayMovie: consume channel-41 '%s' (level %d).\n",
                      s_acLoadedMovieName, liLevelNum);
        CgsDev::Log::WriteToLog(lac);

        if (!s_bRuntimeReady)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: runtime not ready -- load deferred.\n");
            return;
        }

        // The X360 streams the .apt out of GUIAPT\<NAME>.bundle through the loader's
        // async request layer (AptLoader::Load -> the streamer -> CompleteLoad). The
        // request layer is homed but the async COMPLETION (the streamer that reads the
        // .apt bytes + AptCharacterAnimation::Fixup) is NOT, and the loader reaches the
        // current target through GetTarget() -- which is null (no AptCreateTargetInstance).
        AptTarget* lpTarget = GetTarget();
        if (lpTarget == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: GetTarget()==null -- no Apt context to load into "
                                    "(AptCreateTargetInstance un-homed). Load bails cleanly (FLAG). The "
                                    "channel-41 request WAS reached + recorded.\n");
            return;
        }

        // (Reached only once the context lands.) Register the load request through the
        // loader; the file name is GUIAPT\<NAME>.bundle's embedded .apt.
        AptLoader* lpLoader = AptTarget_GetLoader(lpTarget);
        if (lpLoader == nullptr)
        {
            CgsDev::Log::WriteToLog("[AptRT] PlayMovie: target has no loader -- bail (FLAG).\n");
            return;
        }

        char lacApt[96];
        std::snprintf(lacApt, sizeof(lacApt), "%s.apt", s_acLoadedMovieName);
        EAStringC lFileName(lacApt);
        AptFilePtr lFile = lpLoader->Load(lFileName);   // returns a "requested" handle
        CgsDev::Log::WriteToLog("[AptRT] PlayMovie: AptLoader::Load issued -- file in 'requested' state; "
                                "async completion (streamer + Fixup) is un-homed so it will not reach "
                                "'loaded' (FLAG).\n");
    }

    // -------------------------------------------------------------------------
    // AptRuntimeUpdate -- per-frame tick + render dispatch. If a movie is loaded,
    // advance its timeline + run its ActionScript, then drive the engine render
    // dispatch so geometry fills s_AptRenderBuffer. Until the loader/target land,
    // this bails after its probe (throttled).
    // -------------------------------------------------------------------------
    void AptRuntimeUpdate()
    {
        if (!s_bRuntimeReady)
            return;

        ++s_iFrameCounter;
        const bool lbProbeFrame = (s_iFrameCounter % 30) == 1;   // ~every 30 frames

        // The per-frame engine spine (AptLoader::Update -> AptLinker::Update ->
        // AptAnimationTarget::RunActions/TickIntervalTimers/TickNewInsts -> the render-
        // tree walk that calls gAptFuncs.pfnDrawRenderingUnit) is reached through the
        // current AptTarget. With no context (GetTarget()==null) there is nothing to tick
        // or render: bail after the probe so the run-log shows we got here every frame.
        AptTarget* lpTarget = GetTarget();
        if (lpTarget == nullptr)
        {
            if (lbProbeFrame)
                CgsDev::Log::WriteToLog("[AptRT] frame: no Apt context (GetTarget null) -- tick+render "
                                        "skipped (FLAG: AptUpdateInitialize un-homed).\n");
            return;
        }

        // (Reached only once the context + loader land.) Tick the loader/linker + the
        // animation director, then drive the render-tree walk. These engine entries are
        // the un-homed orchestration; each is FLAG'd. When they land, replace the bails
        // below with the real per-frame calls.
        // FLAG: AptLoader::Update / AptLinker::Update / AptAnimationTarget tick + the
        // render-tree walk are the un-reconstructed AptUpdate facade. Not invoked here
        // (calling into them with a partial context would fault); this is the tick/render
        // bail point. The render buffer flush (AptRuntimeFlush) still runs so the (empty)
        // buffer is dispatched and the path to the screen is exercised.
        if (lbProbeFrame)
            CgsDev::Log::WriteToLog("[AptRT] frame: context live but AptUpdate facade un-homed -- "
                                    "tick+render bail (FLAG).\n");
    }

    // -------------------------------------------------------------------------
    // AptRuntimeFlush -- flush the Apt render buffer to D3D9. Called from the
    // renderer hook each frame. AptRenderHandler::Render fills s_AptRenderBuffer
    // (between Begin/EndRendering) when the engine render dispatch runs; here we
    // Swap + Dispatch it to the screen. A buffer with no rendering block this frame
    // is a clean no-op (Dispatch on an empty dispatch buffer draws nothing).
    // -------------------------------------------------------------------------
    void AptRuntimeFlush()
    {
        if (!s_bRenderBufferReady)
            return;

        // The engine render dispatch (un-homed) would have bracketed the per-frame Apt
        // draws with BeginRendering/.../EndRendering on s_AptRenderBuffer.mCommandBuffer.
        // Swap the write buffer to the dispatch buffer + flush it to the GPU. If no block
        // was opened this frame the dispatch buffer is empty and Dispatch draws nothing.
        // FLAG: until the engine render-tree walk fills the buffer this is a no-op flush;
        // it is wired now so the path to the screen exists the instant geometry appears.
        s_AptRenderBuffer.mCommandBuffer.Swap();
        s_AptRenderBuffer.mCommandBuffer.Dispatch();
    }
}
