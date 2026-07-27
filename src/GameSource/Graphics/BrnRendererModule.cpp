#include "GameSource/Graphics/BrnRendererModule.h"
#include "pc/gcm/renderengine/device.h"   // renderengine::Device frame bracket
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"  // CgsDev::DebugManager (debug HUD overlay)
#include "GameSource/Gui/BrnGuiModule.h"         // BrnGui::gpActiveGuiModule (the GUI render drive)

// Minimal constructor for the off-path job placeholder embedded in BrnRendererModule
// (Option B). The job system is reconstructed with the threading core; on the
// single-threaded boot it carries no behaviour, so this definition keeps the link
// closed without faking functionality. (BufferedDispatchFrame is the REAL type now --
// its stub ctor is gone with the world-pass mount.)
EA::Jobs::Job::Job(s32 /*liPriority*/) {}
#include "GameSource/Gui/BrnGuiMovieManager.h"   // BrnGui::gpActiveMovieManager (the PC presentation draw)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // [diag] BRN_IM2D_TRACE probes
#include "rw/rwcore_structs.h"                   // rw::LinearResourceAllocator (world dispatch bin memory)
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"  // ShaderConstantTable::BeginFrame (StartOfFrame)
#include <Windows.h>   // [diag] GetEnvironmentVariableA
#include <cstdio>      // [diag] snprintf
#include <cstring>     // memcpy (per-pass DispatchObjectContext copies)
#include <new>         // world dispatch bring-up heap

// [diag] present counter (device.cpp) - stamps the trace lines with their frame.
namespace renderengine { extern u32 guPresentCount; }

// High-res frame timer (CgsTimeUtils.cpp), forward-declared - drives the thread-monitor health.
namespace CgsSystem { u32 GetSystemTimerBaseTime(); u32 GetSystemTimerFrequency(); }

// The engine-global shader-constant table (bodied by the CgsShaderConstants TU); the X360
// StartOfFrame @0x823FC160 opens its frame on the GDL write bin.
namespace CgsGraphics { extern ShaderConstantTable mShaderConstantTable; }

namespace
{
    u32  gu32LastMonitorTick = 0;
    bool gbMonitorTickValid  = false;

    // [PC presentation leaf] the movie screen-ownership linger (see the movie block in
    // Render): tick of the last frame the MovieManager's presentation cycle was active.
    u32  gu32LastMoviePresentTick = 0;
    bool gbMoviePresentTickValid  = false;

    // Submit one solid-coloured quad (4-vertex triangle strip) through the Im2d, in 1280x720 logical px.
    void EmitColouredQuad(CgsGraphics::Im2d* lpIm2d, f32 lfX0, f32 lfY0, f32 lfX1, f32 lfY1, CgsGraphics::RGBA8 lColour)
    {
        CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
        const f32 laPos[4][2] = { {lfX0, lfY0}, {lfX1, lfY0}, {lfX0, lfY1}, {lfX1, lfY1} };   // TL,TR,BL,BR
        for (s32 liVertex = 0; liVertex < 4; ++liVertex)
        {
            laVerts[liVertex].mv2Pos    = { laPos[liVertex][0], laPos[liVertex][1] };
            laVerts[liVertex].mv2Tex0UV = { 0.0f, 0.0f };
            laVerts[liVertex].mv4Colour = lColour;
        }
        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
    }
}

// @ 0x82405A30 - BrnRendererModule::RenderThreeThreadMonitors. Three squares bottom-centre, one per
// worker thread: green when the thread is running in real time, red when it has fallen behind. The X360
// draws them via the untextured Basic2dColouredVertex renderer at normalised coords (x 0.55/0.57/0.59,
// y 0.91-0.94); reconstructed through mIm2dRenderer untextured (SetTexture(null) -> solid colour), with
// the normalised coords scaled to the 1280x720 logical space.
void BrnRendererModule::RenderThreeThreadMonitors(bool lbThread0, bool lbThread1, bool lbThread2)
{
    const f32 KF_W = 1280.0f;
    const f32 KF_H = 720.0f;
    const CgsGraphics::RGBA8 KC_GREEN = { 0, 255, 0, 255 };
    const CgsGraphics::RGBA8 KC_RED   = { 255, 0, 0, 255 };

    const f32  laLeftX[3]      = { 0.55f, 0.57f, 0.59f };   // normalised left edge; width 0.015
    const bool labThreadOk[3]  = { lbThread0, lbThread1, lbThread2 };

    mIm2dRenderer.BeginRendering();
    mIm2dRenderer.SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
    mIm2dRenderer.SetTexture(nullptr);   // untextured -> solid vertex colour
    for (s32 liThread = 0; liThread < 3; ++liThread)
    {
        const CgsGraphics::RGBA8 lColour = labThreadOk[liThread] ? KC_GREEN : KC_RED;
        EmitColouredQuad(&mIm2dRenderer,
                         laLeftX[liThread] * KF_W,           0.91f * KF_H,
                         (laLeftX[liThread] + 0.015f) * KF_W, 0.94f * KF_H, lColour);
    }
    mIm2dRenderer.EndRendering();
}

// @ 0x82406410 - BrnRendererModule::RenderLetterBoxBars. Draw the two solid-black bars that frame a
// widescreen (letterboxed) view - one across the top, one across the bottom. lfDestAspectRatio is the
// visible/kept vertical fraction of the screen; the cropped-away remainder (1 - lfDestAspectRatio) is
// split evenly between the two bars, so each bar is (1 - lfDestAspectRatio) * 0.5 of the height and
// spans the full width. The X360 draws them through the immediate-mode 2D renderer in normalised
// [0,1] screen space: BeginRendering -> SetTransform(cached screen transform) -> Render(top bar) ->
// Render(bottom bar) -> EndRendering, with each quad's four vertices coloured from a const RGBA black
// (DWARF locals lLetterboxY / lBlack / lTransform). Each quad is a 4-vertex triangle strip (prim 6).
void BrnRendererModule::RenderLetterBoxBars(CgsGraphics::Im2d& lIm2d, f32 lfDestAspectRatio)
{
    using namespace CgsGraphics;

    const RGBA8 KC_BLACK = { 0, 0, 0, 255 };
    const f32   lfLetterboxY = (1.0f - lfDestAspectRatio) * 0.5f;   // height of each bar (top + bottom)

    lIm2d.BeginRendering();

    // X360 SetTransform of the renderer's cached [0,1]->screen transform (module static @0x830112D0).
    // The exact matrix bytes are not recovered from the ARTIST rodata, so the default-constructed
    // Im2dTransform stands in for that cached screen transform here.
    Im2dTransform lTransform;
    lIm2d.SetTransform(lTransform);

    Basic2dColouredTexturedVertex laVerts[4];
    for (s32 liVertex = 0; liVertex < 4; ++liVertex)
    {
        laVerts[liVertex].mv4Colour  = KC_BLACK;
        laVerts[liVertex].mv2Tex0UV  = { 0.0f, 0.0f };
    }

    // Top bar: full width (x 0..1), y in [0, lfLetterboxY]. Triangle-strip order TL, BL, TR, BR.
    laVerts[0].mv2Pos = { 0.0f, 0.0f };
    laVerts[1].mv2Pos = { 0.0f, lfLetterboxY };
    laVerts[2].mv2Pos = { 1.0f, 0.0f };
    laVerts[3].mv2Pos = { 1.0f, lfLetterboxY };
    lIm2d.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);

    // Bottom bar: full width, y in [1 - lfLetterboxY, 1].
    laVerts[0].mv2Pos = { 0.0f, 1.0f - lfLetterboxY };
    laVerts[1].mv2Pos = { 0.0f, 1.0f };
    laVerts[2].mv2Pos = { 1.0f, 1.0f - lfLetterboxY };
    laVerts[3].mv2Pos = { 1.0f, 1.0f };
    lIm2d.Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);

    lIm2d.EndRendering();
}

// @ 0x8240A778 - BrnRendererModule::Construct. Reconstructed from the X360 ARTIST build.
//
// Option B (layout-faithful incremental): the loading-screen render path is reconstructed
// for real here - the double-buffered shader-constant frames and the loading-screen
// renderer, which is what actually draws during boot. The remaining subsystems the full
// Construct builds (effects arbitrator, dispatch frames, the Im2d/Im3d family, render-
// target memory, corona/postfx/occlusion/shadow/sun managers) are held as opaque storage
// and their construction is reconstructed incrementally; none of them draws during the
// loading screen, so the screen boots through the real module without them.
namespace
{
    // [PC bring-up] The dispatch bins' backing memory. The X360 carves them from
    // the renderer's graphics allocator (BrnRendererMemory::Construct ->
    // mpGraphicsAllocator, this+14668) whose reconstruction is still open; until
    // it lands, a renderer-owned rw::LinearResourceAllocator over one heap block
    // supplies DoAllocate with identical semantics.
    rw::LinearResourceAllocator sWorldDispatchAllocator;
    bool                        sbWorldDispatchAllocatorReady = false;

    // The X360 render-frame bin size is the rodata global dword_82F24238, which
    // the function-only exports leave UNVALUED; this PC sizing is a documented
    // choice (world-city frames: object commands + expanded mesh commands +
    // constant scratch + sort arrays all live in the frame bin).
    const u32 KU_PC_DISPATCH_BIN_BYTES     = 12u * 1024u * 1024u;
    const u32 KU_PC_GDL_DISPATCH_BIN_BYTES = 8u * 1024u * 1024u;
    const u32 KU_NUM_DISPATCH_LISTS        = 25u;   // X360 Construct: GetList ids 0..24

    bool EnsureWorldDispatchAllocator()
    {
        if (sbWorldDispatchAllocatorReady)
            return true;

        const u32 luHeapBytes = KU_PC_DISPATCH_BIN_BYTES
                              + 2u * KU_PC_GDL_DISPATCH_BIN_BYTES
                              + (3u * 4096u); // per-bin align128(size)+128 slop + headroom
        void* lpHeap = ::operator new(luHeapBytes, std::nothrow);
        if (lpHeap == 0)
            return false;

        rw::Resource lHeapResource;
        rw::ResourceDescriptor lHeapCapacity;
        for (u32 luLane = 0; luLane < 4; ++luLane)
        {
            lHeapResource.m_baseResources[luLane] = (luLane == 0) ? lpHeap : 0;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_size      = (luLane == 0) ? luHeapBytes : 0u;
            lHeapCapacity.m_baseResourceDescriptors[luLane].m_alignment = (luLane == 0) ? 128u : 1u;
        }
        sWorldDispatchAllocator.Initialize(lHeapResource, lHeapCapacity);
        sbWorldDispatchAllocatorReady = true;
        return true;
    }
}

void BrnRendererModule::Construct()
{
    // Double-buffered per-frame shader constants (maShaderConstantsFrames[2]).
    maShaderConstantsFrames[0].Construct();
    maShaderConstantsFrames[1].Construct();

    // ---- The render-dispatch machinery (X360 Construct mid-section) ----------
    // DispatchFrame::Construct(&this+768, 25, dword_82F24238, mpGraphicsAllocator)
    // + SetupBuiltinInterpreters(&maInterpretFunctions) + the interpreter object.
    // The GDL side (mDoubleBufferedDispatchFrame, this+680) is built through the
    // real BufferedDispatchFrame with 2 slots so the game thread can fill the
    // write frame while Render walks the read frame.
    if (EnsureWorldDispatchAllocator())
    {
        mSingleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);

        mDoubleBufferedDispatchFrame.SetNumDispatchFrames(2);
        mDoubleBufferedDispatchFrame.Construct(KU_NUM_DISPATCH_LISTS,
                                               KU_PC_GDL_DISPATCH_BIN_BYTES,
                                               &sWorldDispatchAllocator);

        CgsGraphics::SetupBuiltinInterpreters(maInterpretFunctions);
        mpInterpreter = new CgsGraphics::DispatchPacketInterpreter(maInterpretFunctions, 4);
        mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
        mpInterpreter->SetTime(0.0f);
    }

    // The loading-screen renderer (creates its textures + scratch buffer, picks language).
    mLoadingScreenRenderer.Construct();
}

// @ 0x82405E28 (BrnRendererModule::Update, the SetDispatchFrame expression)
// The GDL frame the game side fills this update frame:
//   v26 = (*(*(this + 680) + 28))(this + 680);   // vtable slot 7
//   RendererIO::OutputBuffer::SetDispatchFrame(lpOutput, v26);
// The full Update (camera copy, the fourteen other OutputBuffer publications,
// the render-switch/effects-frame plumbing) lands with the renderer IO buffers;
// this accessor is the one lane the world dispatch feed needs, named so the
// renderer -> world bridge binds to a real seam instead of poking the member.
CgsGraphics::DispatchFrame* BrnRendererModule::GetDispatchFrameForWrite()
{
    return &mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite();
}

// @ 0x823FC160 - BrnRendererModule::StartOfFrame.
// X360 order: Reset the GDL write frame, rewind the seven immediate-mode render
// buffers, ShaderConstantTable::BeginFrame on the GDL write bin, clear the 7x7
// texture-scope scratch (unk_83011A8C), rewind the corona submission interface.
// Reconstructed here: the two GDL halves (the parts whose subsystems exist).
// FLAG [PC gate]: the im-buffer rewinds / texture-scope clear / corona rewind
// land with CgsTextureScopeTable and the corona manager.
void BrnRendererModule::StartOfFrame()
{
    if (mpInterpreter == 0)
        return;   // Construct's allocator gate did not open -- no GDL ring.

    mDoubleBufferedDispatchFrame.GetDispatchFrameForWrite().Reset();
    CgsGraphics::mShaderConstantTable.BeginFrame(
        &mDoubleBufferedDispatchFrame.GetDispatchBinForWrite());
}

// @ 0x823FC678 - BrnRendererModule::SwapBuffers (called by EndOfFrame @0x823FFE28).
// X360 order: the GDL ring Swap (vtable slot 4), two ShaderConstantTable
// Destruct calls, EffectsArbitrator::EndOfFrame, the shader-constants frame
// flip (+2768 <- +2769, +2769 <- 1 - old, BrnShaderConstantsFrame::Construct on
// the new write slot and the two +1964 flags), the seven im-buffer Swaps and the
// blobby-shadow / corona index flips.
// Reconstructed here: the GDL Swap + the shader-constants frame flip.
// FLAG [PC gate]: the rest lands with those subsystems.
void BrnRendererModule::SwapBuffers()
{
    if (mpInterpreter == 0)
        return;

    mDoubleBufferedDispatchFrame.Swap();

    mu8ShaderConstantsFrameInternal = mu8ShaderConstantsFrameExternal;
    mu8ShaderConstantsFrameExternal =
        static_cast<u8>(1u - mu8ShaderConstantsFrameInternal);
    maShaderConstantsFrames[mu8ShaderConstantsFrameExternal].Construct();
}

// @ 0x823FFE28 - BrnRendererModule::EndOfFrame, called from
// BrnGame::BrnGameModule::OnEndOfUpdateFrame @0x823DBBA0.
//
// The X360 body takes a `freeze rendering` bool and runs a 3-state latch over
// this+50548 / this+50552 that suppresses SwapBuffers while the freeze is held
// (0 = running -> swap; 1 = entering, swap once the 2-frame counter expires;
// 2 = frozen-but-still-swapping-once). It then consumes the this+50276 ->
// this+50277 camera-cut edge Update sets. FLAG [PC gate]: neither the freeze
// latch pair nor the camera-cut pair is in the PC member layout yet, and no PC
// caller passes the bool -- this is the freeze=false path, which is the only one
// the game runs outside the debug freeze-frame feature.
void BrnRendererModule::EndOfFrame()
{
    SwapBuffers();
}

// @ 0x823F5898 - BrnRendererModule::ConvertObjectsToMeshes. The X360 runs 16
// object-to-mesh jobs when the MT switch (byte_82F2423C) is on, else the
// single-threaded fallback: per GDL object list j in 0..12, reset the constant
// table's dispatch shadow, take a fresh copy of the 240-byte object context and
// expand the read-side frame's list into the render frame's mesh lists.
// The PC bring-up runs that ST fallback (the job scheduler is not up).
void BrnRendererModule::ConvertObjectsToMeshes(CgsGraphics::BufferedDispatchFrame* lpGdlFrames,
                                               CgsGraphics::DispatchFrame* /*lpMeshFrame*/,
                                               CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
                                               const CgsGraphics::DispatchObjectContext* lpContext)
{
    for (u32 luListId = 0; luListId < 13u; ++luListId)
    {
        // X360 per-pass prologue: mShaderConstantTable.ResetShadowingForDispatch()
        // + the 7x7 texture-scope scratch clear (unk_83011A90). Both shadow the
        // PRODUCER-side dirty tracking; the expansion context below is a fresh
        // copy each pass, so the bring-up defers them with the texture-scope
        // reconstruction. FLAG [deferred with CgsTextureScopeTable].

        CgsGraphics::DispatchObjectContext lContextCopy;
        std::memcpy(&lContextCopy, lpContext, sizeof(lContextCopy));

        CgsGraphics::DispatchFrame& lrGdlFrame = lpGdlFrames->GetDispatchFrameForRead();
        lrGdlFrame.GetList(luListId)->DispatchAllObjectToMesh(
            lpInterpreter, lpInterpreter->GetSingleBufferedDispatchFrame(),
            &lContextCopy, 0, -1);
    }
}

// @ 0x823F5F70 - BrnRendererModule::SortDispatchLists. The X360 preps 16
// RadixSort jobs over lists {0,2,1,3,4, 5..10, 21, 11, 19, 15, 20}; the PC
// bring-up sorts the same lists synchronously.
void BrnRendererModule::SortDispatchLists(CgsGraphics::DispatchFrame* lpMeshFrame)
{
    static const u32 KAU_SORTED_LISTS[16] =
        { 0u, 2u, 1u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 21u, 11u, 19u, 15u, 20u };
    for (u32 luIndex = 0; luIndex < 16u; ++luIndex)
    {
        lpMeshFrame->GetList(KAU_SORTED_LISTS[luIndex])->SortForDispatch();
    }
}

// The world/car/sky pass block of Render (@0x8240BFA8 mid-section). Pass order
// and list ids are the X360's (see the renderer wave log for the full map):
//   shadow cascades (lists 0,2,1,3,4)  -> gated OFF on PC (Z-only interpreter
//     + shadow-map render manager still under reconstruction)
//   env-map faces (lists 5..10)        -> gated OFF on PC (env-map targets)
//   pre-Z (list 21)                    -> gated OFF on PC (Z-only interpreter)
//   CARS OPAQUE  (list 19)  -> DispatchAllMeshes
//   WORLD OPAQUE (list 11)  -> DispatchAllMeshes
//   sky                     -> gated OFF on PC (SkyDomeManager not constructed)
//   WORLD TRANSPARENT (15)  -> DispatchAllMeshes
//   CARS TRANSPARENT  (20)  -> DispatchAllMeshes (blobby shadows gated off)
// Occlusion-query interleaving is the mbOcclusionCull* path (default false).
void BrnRendererModule::RenderWorldPasses(const BrnGame::DispatchThreadInputBuffer* /*lpDispatchThreadInputBuffer*/)
{
    using namespace CgsGraphics;

    if (mpInterpreter == 0)
        return;

    // Start-of-frame: reset the render frame + point the interpreter at it
    // (X360: DispatchFrame::Reset(this+768); interp+12 = frame; interp+8 = 0).
    mSingleBufferedDispatchFrame.Reset();
    mpInterpreter->SetSingleBufferedDispatchFrame(&mSingleBufferedDispatchFrame);
    mpInterpreter->SetTime(0.0f);

    // The 240-byte object context (X360 builds it on the Render stack):
    // constant shadow cleared, list base 0, the pre-Z config from the module.
    DispatchObjectContext lContext;
    std::memset(&lContext, 0, sizeof(lContext));
    lContext.ResetShadowing();
    lContext.miListIdBase       = 0;
    lContext.mbPreZEnabled      = false;   // FLAG [PC gate]: real value = mbRenderPreZ once the
                                           // Z-only interpreter is reconstructed
    lContext.mbPreZAlphaEnabled = mbRenderPreZAlpha;
    const f32 lfPreZDistance = mbPreZNearOnly ? mfPreZDistanceThreshold : 100000.0f;
    for (u32 luLane = 0; luLane < 4; ++luLane)
        lContext.mvPreZDistanceThreshold[luLane] = lfPreZDistance * lfPreZDistance;

    // Object -> mesh expansion + the pass sorts.
    ConvertObjectsToMeshes(&mDoubleBufferedDispatchFrame, &mSingleBufferedDispatchFrame,
                           mpInterpreter, &lContext);
    SortDispatchLists(&mSingleBufferedDispatchFrame);

    // Pass stats (X360 60-frame averages; the raw totals feed the debug HUD).
    const u32 luCarOpaque        = mSingleBufferedDispatchFrame.GetList(19)->GetCount();
    const u32 luWorldOpaque      = mSingleBufferedDispatchFrame.GetList(11)->GetCount();
    const u32 luWorldTransparent = mSingleBufferedDispatchFrame.GetList(15)->GetCount();
    const u32 luCarTransparent   = mSingleBufferedDispatchFrame.GetList(20)->GetCount();
    mu32NumWorldOpaqueObjectTotals      += luWorldOpaque;
    mu32NumCarOpaqueObjectTotals        += luCarOpaque;
    mu32NumWorldTransparentObjectTotals += luWorldTransparent;
    mu32NumCarTransparentObjectTotals   += luCarTransparent;

    const bool lbOpaqueWork = (mbRenderCarsOpaque && luCarOpaque != 0)
                           || (mbRenderWorldOpaque && luWorldOpaque != 0);
    const bool lbTransparentWork = (mbRenderWorldTransparent && luWorldTransparent != 0)
                                || (mbRenderCarsTransparent && luCarTransparent != 0);

    // [PC bring-up states] The per-pass render states normally come from the
    // technique state groups the walk binds on each technique change
    // (MaterialState -- its porter + the x64 state-object seam are still open).
    // Opaque passes: Z test+write on, no blending. FLAG: replace with the real
    // state-group binds when the MaterialState path lands.
    //
    // Applied ONLY when a pass will actually walk records: with no world data
    // (boot, menus, every frame before the streamer delivers geometry) the whole
    // block leaves the device state untouched, so the 2D/GUI tail below sees
    // exactly the state it saw before this pass existed.
    if (lbOpaqueWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);

        if (mbRenderCarsOpaque)
        {
            mSingleBufferedDispatchFrame.GetList(19)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
        if (mbRenderWorldOpaque)
        {
            mSingleBufferedDispatchFrame.GetList(11)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
    }

    // (sky here when BrnSkyDomeManager comes online)

    // Transparent passes: Z test on / write off, alpha blend on. Same FLAG.
    if (lbTransparentWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(true);

        if (mbRenderWorldTransparent)
        {
            mSingleBufferedDispatchFrame.GetList(15)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
        if (mbRenderCarsTransparent)
        {
            mSingleBufferedDispatchFrame.GetList(20)->DispatchAllMeshes(mpInterpreter, &lContext, 0, -1);
        }
    }

    // Back to the opaque default before the 2D overlay tail (the Im2d path re-sets
    // its own states, so this only matters for the frames a world pass ran).
    if (lbOpaqueWork || lbTransparentWork)
    {
        renderengine::Device::SetWorldPassDefaultStates(false);
    }
}

// @ 0x8240BFA8 - BrnRendererModule::Render. Reconstructed from the X360 ARTIST build.
//
// The full Render walks the whole frame (shadow maps, env map, world/car opaque +
// transparent, sky, coronas, particles, post-fx, MSAA resolve) and finishes with the
// loading-screen overlay and the present. During boot none of the world systems have
// data, so those passes are data-gated off; Option B reconstructs the part that actually
// runs - frame begin, the loading-screen foreground overlay, and the present. The gameplay
// passes are reconstructed incrementally as their subsystems come online.
void BrnRendererModule::Render(const BrnGame::DispatchThreadInputBuffer* lpDispatchThreadInputBuffer)
{
    if (!renderengine::Device::FrameBegin())
    {
        return;
    }

    // Forward the dispatch buffer's loading-screen command into the renderer - the X360
    // Render does exactly this each frame (@0x8240BFA8: AddCommand(*(lpDispatchIn+9828)),
    // the `lwzx r4, r26, 0x9990` at 0x8240C17C). The command is one-shot: the manager's
    // end-of-frame Swap re-Constructs each new write buffer, so the slot reads
    // E_LSC_NONE (AddCommand's no-op default) on the frames between events.
    //
    // (The old PC video gate died with the movie pass re-home: fullscreen movies now
    // present inside the GUI pass exactly like the console, and the flow states manage
    // the loading screen around them through the real 19/20 protocol -- BootLoading::
    // OnLeave and PostTitleScreenLoad post StopAptLoadingMovie before playing a video.)
    if (lpDispatchThreadInputBuffer != 0)
        mLoadingScreenRenderer.AddCommand(
            lpDispatchThreadInputBuffer->GetLoadingScreenCommand());

    // The gameplay render walk (object->mesh conversion, pass sorts, the world/
    // car passes). On the X360 this whole block precedes the 2D overlay tail;
    // with no world GDL data the lists are empty and every pass no-ops.
    RenderWorldPasses(lpDispatchThreadInputBuffer);

    // Save/load background layer: in E_LSC_SHOWSAVELOADBG mode the loading screen renders
    // BENEATH the GUI, so the SaveLoadComponent prompt draws over the dimmed loading art.
    // Layer order is the console Render tail @0x8240BFA8: RenderBackground -> the GUI
    // dispatch flush -> RenderForeground.
    mLoadingScreenRenderer.RenderBackground(&mIm2dRenderer);

    // GUI render drive (the Apt/view frame): the X360 render pass runs the GUI module's
    // Render (BrnGui::GuiModule::Render @0x825146B8 -> CgsGui::GuiModule::Render
    // @0x8285AF38 -> ViewModule::Render @0x82858810 -> RenderInternal @0x82858AF8 ->
    // AptAux::Render -> the engine render walk), which fills the published Apt command
    // buffer, then the PC dispatch leaf flushes it to D3D9 and the movie pass presents
    // the active fullscreen video over it (UpdateAndRenderMovieManager, inside the GUI
    // pass, exactly the console order). Clean no-op until the GUI module is prepared.
    // This is how BootLegal's Title_Screen02 movie reaches the screen. [GUI render path]
    if (BrnGui::gpActiveGuiModule != 0)
        BrnGui::gpActiveGuiModule->Render(&mIm2dRenderer);

    // (gameplay-render passes here when reconstructed; gated off during the loading screen)

    mLoadingScreenRenderer.RenderForeground(&mIm2dRenderer);

    // Full-screen movie presentation. FLAG PC-platform: on the X360 the movie frame is
    // drawn inside the GUI pass (UpdateAndRenderMovieManager) and the XMV presentation
    // then owns the screen ABOVE the whole 2D frame -- the boot logos play over the
    // still-latched loading screen (BootVideos @0x82478778 posts no hide; the first 20
    // is BootLegal::OnEnter's). The PC FFmpeg substitute has no overlay plane, so its
    // presentation quad draws here, after the loading-screen foreground, to reproduce
    // that layering. The manager's Update stays in its real GUI-pass home.
    if (BrnGui::gpActiveMovieManager != 0)
    {
        // The XMV presentation owns the screen for the WHOLE video cycle, not just the
        // frames a picture is up: the console shows BLACK between the boot logos (player
        // teardown + the 10+10-frame memory-return delays before the next video is
        // queued) and across each crossfade tail -- never the latched loading screen.
        // The PC stand-in reproduces that ownership with an opaque black underlay while
        // the manager's presentation cycle is active (IsMoviePresentationActive), held
        // for a short linger past the cycle's end to cover the event-queue hops between
        // one video's finish-report and the next play command (logo -> logo) or the
        // title state's hide/589-overlay takeover (last logo -> BF_LEGAL).
        const bool lbPresenting = BrnGui::gpActiveMovieManager->IsMoviePresentationActive();
        const u32  lu32PresentNow  = CgsSystem::GetSystemTimerBaseTime();
        const u32  lu32PresentFreq = CgsSystem::GetSystemTimerFrequency();
        if (lbPresenting)
        {
            gu32LastMoviePresentTick = lu32PresentNow;
            gbMoviePresentTickValid  = true;
        }
        const bool lbOwnsScreen = lbPresenting ||
            (gbMoviePresentTickValid && lu32PresentFreq != 0u &&
             (lu32PresentNow - gu32LastMoviePresentTick) < lu32PresentFreq / 4u);
        // [diag] BRN_IM2D_TRACE: surface the underlay latch state on the same cadence as
        // the Im2d draw trace (queued id + manager state + owns-screen).
        {
            static int siTrace = -1;
            if (siTrace < 0)
            {
                char lacBuf[8];
                siTrace = (GetEnvironmentVariableA("BRN_IM2D_TRACE", lacBuf, sizeof(lacBuf)) > 0) ? 1 : 0;
            }
            if (siTrace == 1 && (renderengine::guPresentCount % 60u) == 0u)
            {
                char lacMsg[160];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[MovieOwn] f=%u presenting=%d owns=%d queued=%d state=%d\n",
                              renderengine::guPresentCount, lbPresenting ? 1 : 0, lbOwnsScreen ? 1 : 0,
                              BrnGui::gpActiveMovieManager->IsMovieQueued() ? 1 : 0,
                              static_cast<s32>(BrnGui::gpActiveMovieManager->GetState()));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
        if (lbOwnsScreen)
        {
            const CgsGraphics::RGBA8 KC_MOVIE_BLACK = { 0, 0, 0, 255 };
            mIm2dRenderer.BeginRendering();
            mIm2dRenderer.SetState(static_cast<const CgsGraphics::BlendState*>(nullptr));
            mIm2dRenderer.SetTexture(nullptr);   // untextured -> solid vertex colour
            EmitColouredQuad(&mIm2dRenderer, 0.0f, 0.0f, 1280.0f, 720.0f, KC_MOVIE_BLACK);
            mIm2dRenderer.EndRendering();
        }
        BrnGui::gpActiveMovieManager->Render(&mIm2dRenderer);
    }

    // Debug HUD overlay (the on-screen perf squares) - drawn on top of the loading screen, before the
    // present. The debug manager is the BrnGameModule-owned singleton (constructed at boot); the X360
    // Render path issues this each frame between the foreground overlay and ShowPixelBuffer. RenderWorld
    // (3D) is deferred, so the view/camera args are unused; the 2D buffer is the real Im2d the loading
    // screen renders through (mIm2dRenderer).
    if (CgsDev::DebugManager* lpDebugManager = CgsDev::DebugManager::ThreadSafeAquire())
    {
        Matrix44 lViewProjection;
        lViewProjection.SetIdentity();
        Vector3 lCameraPosition;
        lCameraPosition.SetZero();
        lpDebugManager->Render(lViewProjection, lCameraPosition, nullptr, &mIm2dRenderer);
        CgsDev::DebugManager::ThreadSafeRelease(lpDebugManager);
    }

    // The three per-thread monitor squares (X360 RenderThreeThreadMonitors). The real per-thread
    // "running in real time" flags need the threading system (deferred), so they are derived here from
    // the present-to-present frame time - matching the observed behaviour (green at framerate, reddening
    // as the game/CPU slows). The X360 gates this on a debug-display flag.
    {
        const u32 lu32Now  = CgsSystem::GetSystemTimerBaseTime();
        const u32 lu32Freq = CgsSystem::GetSystemTimerFrequency();
        f32 lfFrameMs = 0.0f;
        if (gbMonitorTickValid && lu32Freq != 0u)
            lfFrameMs = static_cast<f32>(static_cast<double>(lu32Now - gu32LastMonitorTick) * 1000.0 / static_cast<double>(lu32Freq));
        gu32LastMonitorTick = lu32Now;
        gbMonitorTickValid  = true;

        const f32 lfBudgetMs = 1000.0f / 60.0f;
        s32 liBehind = 0;
        if (lfFrameMs > lfBudgetMs * 1.10f) liBehind = 1;
        if (lfFrameMs > lfBudgetMs * 1.50f) liBehind = 2;
        if (lfFrameMs > lfBudgetMs * 2.00f) liBehind = 3;
        RenderThreeThreadMonitors(liBehind < 3, liBehind < 2, liBehind < 1);
    }

    renderengine::Device::ShowPixelBuffer();
}

// Renders the on-screen assert overlay (forwarded from BrnGameModule::RenderAssert). The real
// body draws the assert text via the immediate-mode renderer; minimal until the assert overlay
// path is reconstructed (asserts are inert on the boot/loading path).
void BrnRendererModule::RenderAssert(const AssertData* /*lpAssertData*/)
{
}
