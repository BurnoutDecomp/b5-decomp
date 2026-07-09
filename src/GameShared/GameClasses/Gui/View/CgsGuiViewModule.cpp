#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"

#include <cstring>

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                    // CgsCore::SPrintf (the playing-movie record)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu (the Update phase brackets)
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModuleIO.h"            // ViewIO::InputBuffer / OutputBuffer (Update IO)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h" // Im2dTransform (RenderBlackScreen's unit-to-screen)
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h" // the clear-quad vertices

namespace CgsGui
{
    // The custom-renderer manager the module installs. On the X360,
    // SetCustomRendererManager @0x824EBBF8 dispatches three subsystem-wiring calls
    // through this object's vtable (guest slots +0x38, +0x3C, +0x40), passing the
    // module's TextRenderer (+0x3E0), its LanguageManager (+0x7C0C) and the
    // caller-supplied argument respectively. The committed CgsGui::CustomRendererManager
    // is a minimal slice whose vtable order does not line up with the guest slots, so
    // the three wiring calls are modelled as named virtual methods on a local
    // declaration-only interface reflecting the observed argument flow. FLAG: the
    // precise method names/return types of the three guest vtable slots are not
    // independently DWARF-attested; they are reconstructed from the call-site argument
    // flow (subsystem back-pointer wiring).
    struct CustomRendererManagerWiring
    {
        // +0x38: receives the module's TextRenderer (guest this+0x3E0).
        virtual void SetTextRenderer(void* lpTextRenderer) = 0;
        // +0x3C: receives this module's LanguageManager (guest this+0x7C0C).
        virtual void SetLanguageManager(void* lpLanguageManager) = 0;
        // +0x40: receives the caller-supplied argument (guest r6).
        virtual void SetExtraWiring(int liArg) = 0;
    };

    // The two per-frame CPU monitors ViewModule::Update brackets its phases with
    // (X360 dword_82F3312C the view-event dispatch / dword_82F33128 the Apt update).
    // Registered by the un-homed perf-monitor setup TU; -1 handles no-op
    // Start/StopMonitor until it lands.
    static s32 giViewEventsMonitor = -1;   // dword_82F3312C
    static s32 giViewAptMonitor    = -1;   // dword_82F33128

    namespace
    {
        // The view-state payload BODIES (the AddViewState writers queue the event body
        // -- the fields after the GuiEvent<N> 12-byte header; the X360 dispatch reads
        // the first payload field at +0). Local views of the CgsGuiStateInterface.h
        // GuiEvent bodies.
        struct PlayAptMovieBody { const char* mpacMovieName; s32 miLevelNum; };
        struct ClearScreenBody  { s32 miMode; f32 mfAlpha; };
    }

    ViewModule::ViewModule()
        : mbUpdateFlash(false),
          mfHack_LastValidTimeStep(0.0f),
          mfCurrentTime(0.0f),
          mfLastUpdateTime(0.0f),
          mfLastRenderTime(0.0f),
          mfUpdateTimeDelta(0.0f),
          mfRenderTimeDelta(0.0f),
          mImRenderers(),
          mTextRenderer(),
          mFonts(),
          mLanguageManager(),
          mOutputEventQueue(),
          mePrepareStage(E_PREPARESTAGE_START),
          meReleaseStage(E_RELEASESTAGE_DONE),
          mpcLoadingMovieName(0),
          miLoadingScreenLevel(5),
          macCurrentlyPlayingMovies(),
          mbClearScreenEnabled(true),
          mfClearScreenAlpha(1.0f),
          mpCustomRendererManager(0),
          mAptAux()
    {
    }

    // Construct @0x828605A0 -- store order follows the guest body.
    void ViewModule::Construct(const char* lpcName, int liArg2, f32 lfAspectRatio,
                               const RGBA* lpAlternateTextColours, int liNumAlternateColours)
    {
        (void)lpcName;
        (void)liArg2;

        CgsModule::ModuleSingleBuffered::Construct();
        mLanguageManager.Construct();

        // FLAG (deferred member): the guest also constructs an embedded
        // CgsGraphics::Camera at [c:+624] via the outlined helper sub_827F94E8
        // (Camera::Construct(flt_82F30FD4, flt_82F30FD8, 0.1f, 1000.0f) -- the display
        // dimensions + near/far clip). CgsGraphics::Camera has no reconstructed
        // Construct(f32,f32,f32,f32) body/declaration yet; add the mCamera member and
        // this call when the Camera lifecycle TU lands.

        // The guest zeroes render-set slots 0/2/3/4 (+592/+600/+604/+608) individually;
        // slot 1 (+596) is left untouched.
        mImRenderers.mpIm2dRenderBuffer = 0;
        mImRenderers.mpIm3dRenderBufferUntex = 0;
        mImRenderers.mpIm3dRenderBufferRacePosition = 0;
        mImRenderers.mpIm3dRenderBufferMenusAndHud = 0;
        mTextRenderer.Construct();

        // The inlined FontCollection::Construct (the guest seeds the three slot pairs
        // from the default-handle sentinel dword_8305F174/_8305F178).
        mFonts.Construct();

        mAptAux.Construct(
            reinterpret_cast<::CgsGuiModuleIO::ImRendererSet*>(&mImRenderers),
            &mTextRenderer,
            &mLanguageManager,
            &mFonts,
            lfAspectRatio,
            reinterpret_cast<const rw::RGBA*>(lpAlternateTextColours),
            liNumAlternateColours);

        // Clear the custom-renderer mirror the render handler holds (guest +58596 ==
        // mRenderHandler+0xB4).
        mAptAux.mRenderHandler.SetCustomRendererManager(0);

        mfHack_LastValidTimeStep = 0.0f;
        mfCurrentTime = 0.0f;
        mfLastUpdateTime = 0.0f;
        mfLastRenderTime = 0.0f;
        mfUpdateTimeDelta = 0.0f;
        mfRenderTimeDelta = 0.0f;
        mpcLoadingMovieName = 0;
        mbUpdateFlash = true;
        miLoadingScreenLevel = 5;
        mpCustomRendererManager = 0;
        mfClearScreenAlpha = 1.0f;
        mbClearScreenEnabled = true;
        std::memset(macCurrentlyPlayingMovies, 0, sizeof(macCurrentlyPlayingMovies));
        mOutputEventQueue.Construct();
        // The guest tail store `*(this+4) = 1` writes the private base
        // ModuleSingleBuffered::mePrepareStage (= E_MANAGERPREPARESTAGE_INPUT) after the
        // base Construct set START. Not reproduced (the field is private to the base and
        // the observable difference is nil: the base Prepare's START case only re-nulls
        // the already-null data-structure pointers before falling into INPUT).
        // The derived mePrepareStage/meReleaseStage seeds live in the C++ ctor, not here
        // (the guest body stores neither).
    }

    bool ViewModule::Prepare(CgsMemory::HeapMalloc* lpHeap,
                             rw::IResourceAllocator* lpResAlloc,
                             CgsMemory::HeapMalloc* lpLanguageHeap,
                             CgsMemory::LinearMalloc* lpLinear)
    {
        (void)lpResAlloc;
        (void)lpLinear;

        switch (mePrepareStage)
        {
        case E_PREPARESTAGE_START:
            // The guest runs the output-queue Prepare in the START case (@0x828584A8),
            // BEFORE storing stage MANAGER -- a resume at MANAGER does not re-run it.
            mOutputEventQueue.Prepare();
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            // fall through
        case E_PREPARESTAGE_MANAGER:
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            mePrepareStage = E_PREPARESTAGE_LANGUAGE;
            // fall through
        case E_PREPARESTAGE_LANGUAGE:
            if (!mLanguageManager.Prepare(lpLanguageHeap))
                return false;
            // The guest stores stage APT directly (its jumptable routes stage 3,
            // E_PREPARESTAGE_MOVIE, to the "Unknown prepare stage" assert -- the movie
            // stage is release-side only in this build).
            mePrepareStage = E_PREPARESTAGE_APT;
            // fall through
        case E_PREPARESTAGE_APT:
            if (!mAptAux.Prepare(lpHeap))
                return false;
            mePrepareStage = E_PREPARESTAGE_DONE;
            // fall through
        case E_PREPARESTAGE_DONE:
            meReleaseStage = E_RELEASESTAGE_START;
            return true;
        default:
            CGS_ASSERT(false, "Unknown prepare stage");
            return false;
        }
    }

    bool ViewModule::Release()
    {
        switch (meReleaseStage)
        {
        case E_RELEASESTAGE_START:
            meReleaseStage = E_RELEASESTAGE_APT;
            mOutputEventQueue.Release();
            // fall through
        case E_RELEASESTAGE_APT:
            if (!mAptAux.Release())
                return false;
            meReleaseStage = E_RELEASESTAGE_MOVIE;
            // fall through
        case E_RELEASESTAGE_MOVIE:
            meReleaseStage = E_RELEASESTAGE_LANGUAGE;
            // fall through
        case E_RELEASESTAGE_LANGUAGE:
            if (!mLanguageManager.Release())
                return false;
            meReleaseStage = E_RELEASESTAGE_MANAGER;
            // fall through
        case E_RELEASESTAGE_MANAGER:
            if (!CgsModule::ModuleSingleBuffered::Release())
                return false;
            meReleaseStage = E_RELEASESTAGE_DONE;
            // fall through
        case E_RELEASESTAGE_DONE:
            mePrepareStage = E_PREPARESTAGE_START;
            return true;
        default:
            CGS_ASSERT(false, "Unknown release stage");
            return false;
        }
    }

    // Destruct @0x82858688.
    void ViewModule::Destruct()
    {
        mLanguageManager.Destruct();
        mOutputEventQueue.Release();
        AptAuxPointer::mpAptAuxInst = 0;
        // FLAG (deferred store): the guest zeroes AptAux guest +0x410 (ViewModule
        // +58400) here -- an apt-engine bookkeeping slot in the un-modelled gap between
        // mAptDataHandler and mRenderHandler (its producer is un-homed). Reproduce it
        // when that AptAux member gains a named home. (A previous recon mismapped this
        // store to mpCustomRendererManager, which the guest does NOT touch here.)
        CgsModule::ModuleSingleBuffered::Destruct();
    }

    // Update @0x82860708 -- the per-frame view drive: clear the output event queue,
    // dispatch the incoming view-state events under the input read lock, advance the
    // update-time bookkeeping, then (when the flash update is enabled) tick the Apt
    // host by the elapsed milliseconds. The IO buffer stacks ride the module
    // scheduler signature; the X360 body never touches them.
    void ViewModule::Update(ViewIO::IOBufferStack* lpInStack,
                            ViewIO::IOBufferStack* lpOutStack,
                            const ViewIO::InputBuffer* lpInput,
                            ViewIO::OutputBuffer* lpOutput)
    {
        (void)lpInStack;
        (void)lpOutStack;

        mOutputEventQueue.CgsModule::VariableEventQueue<256, 16>::Clear();

        lpInput->LockForRead();
        lpOutput->LockForWrite();

        CgsDev::PerfMonCpu::StartMonitor(giViewEventsMonitor);
        ProcessIncomingViewEvents(&lpInput->GetEvents(), lpOutput);
        CgsDev::PerfMonCpu::StopMonitor(giViewEventsMonitor);

        lpInput->UnlockForRead();

        mfCurrentTime += mfHack_LastValidTimeStep;
        const f32 lfPrevUpdateTime = mfLastUpdateTime;
        mfLastUpdateTime = mfCurrentTime;
        mfUpdateTimeDelta = mfCurrentTime - lfPrevUpdateTime;
        const s32 liDeltaMs = static_cast<s32>(mfUpdateTimeDelta * 1000.0f);

        CgsDev::PerfMonCpu::StartMonitor(giViewAptMonitor);
        if (mbUpdateFlash)
            mAptAux.Update(liDeltaMs);
        CgsDev::PerfMonCpu::StopMonitor(giViewAptMonitor);

        lpOutput->UnlockForWrite();
    }

    // ProcessIncomingViewEvents @0x8285FCE8 -- route each queued view-state event by
    // id. Unhandled ids fall through silently (the X360 default case is empty).
    void ViewModule::ProcessIncomingViewEvents(const GuiEventQueueBase<65536, 16>* lpEvents,
                                               ViewIO::OutputBuffer* lpOutput)
    {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liSize = 0;
        s32 liEventId =
            lpEvents->CgsModule::VariableEventQueue<65536, 16>::GetFirstEvent(&lpEvent, &liSize);
        while (lpEvent != nullptr)
        {
            switch (liEventId)
            {
            case 10:
            case 11:
            case 12:
                ProcessIncomingLanguageEvent(lpEvent, liSize, liEventId, lpOutput);
                break;

            case 14:
                // The load notification routes through the virtual (guest vtbl slot
                // +0x50), so the BrnGui::ViewModule override handles a FLAPT load.
                ProcessIncomingLoadNotification(lpEvent);
                break;

            case 15:
                ProcessIncomingUnloadRequestNotification(lpEvent);
                break;

            case 17:
            case 18:
            case 19:
            case 20:
                ProcessIncomingAptEvent(lpEvent, liEventId);
                break;

            case 25:
            {
                const ClearScreenBody* lpBody =
                    reinterpret_cast<const ClearScreenBody*>(lpEvent);
                if (lpBody->miMode == 0)
                {
                    mbClearScreenEnabled = true;
                    SetClearScreenAlpha(lpBody->mfAlpha);
                }
                else if (lpBody->miMode == 1)
                {
                    mbClearScreenEnabled = false;
                }
                else
                {
                    CGS_ASSERT(false, "Unhandled enum");
                }
                break;
            }

            case 26:
            {
                const f32 lfTimeStep = *reinterpret_cast<const f32*>(lpEvent);
                if (lfTimeStep > 0.0f)
                    mfHack_LastValidTimeStep = lfTimeStep;
                break;
            }

            case 32:
                // FLAG (deferred store): the guest raises the global byte_8305A6DC
                // here; its consumer is un-recovered, so the flag has no named home
                // yet. Reproduce the store when its owner TU lands.
                break;

            case 61:
            case 62:
                // FLAG (deferred stores): the guest cycles a 0..4 counter (61, module
                // +166996) and stores the asserted spacing value (62, +167000) -- both
                // land in the un-modelled tail of mAptAux (past the render handler).
                // Reproduce them when those AptAux members gain named homes.
                break;

            default:
                break;
            }

            // The custom-renderer manager per-event hook (guest vtbl slot +0x10 on
            // mpCustomRendererManager, called for every event when a manager is
            // installed). FLAG (deferred dispatch): no manager is installed on the
            // boot path and the manager real vtable order is un-recovered; wire the
            // hook when the CustomRendererManager type lands.

            liEventId = lpEvents->CgsModule::VariableEventQueue<65536, 16>::GetNextEvent(
                lpEvent, &lpEvent, &liSize);
        }
    }

    // ProcessIncomingAptEvent @0x8285EAE8 -- the apt view events.
    void ViewModule::ProcessIncomingAptEvent(const void* lpEvent, s32 liEventId)
    {
        switch (liEventId)
        {
        case 17:
            CGS_ASSERT(false,
                "This should never happen - we have a message for Apt View, seriously that's mental!");
            break;

        case 18:
        {
            const PlayAptMovieBody* lpBody =
                reinterpret_cast<const PlayAptMovieBody*>(lpEvent);

            CGS_ASSERT(lpBody->mpacMovieName != 0,
                       "Invalid movie to play in ViewModule::ProcessIncomingAptEvent");
            CGS_ASSERT(static_cast<u32>(lpBody->miLevelNum) <= 8u,
                       "lpPlayMovieEvent->miLevelNum >= 0 && lpPlayMovieEvent->miLevelNum < KI_NUM_MOVIE_LEVELS");
            CGS_ASSERT(std::strlen(lpBody->mpacMovieName) < 31,
                       "strlen( lpPlayMovieEvent->mpacMovieName ) < KI_MOVIE_NAME_LEN-1");

            // Record the movie now playing on the level (GetMovieNameByLevel reads it).
            if (std::strlen(lpBody->mpacMovieName) > 1)
            {
                CgsCore::SPrintf(macCurrentlyPlayingMovies[lpBody->miLevelNum], 32,
                                 "%s", lpBody->mpacMovieName);
            }

            mAptAux.LoadFlashAnimation(lpBody->mpacMovieName, lpBody->miLevelNum);
            break;
        }

        case 19:
        case 20:
        {
            // Post the bool show/hide apt state (type 33) onto the output queue.
            u8 lbShow = (liEventId == 19) ? 1u : 0u;
            mOutputEventQueue.CgsModule::VariableEventQueue<256, 16>::AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lbShow), 33, 1);
            break;
        }

        default:
            CGS_ASSERT(false, "Unexpected event sent to ViewModule::ProcessIncomingAptEvent");
            break;
        }
    }

    // X360 @0x8285ED00 -- NOT YET RECONSTRUCTED (the ledger dossier holds the
    // reviewed body): the language view events (10-12). Land with the
    // language-ownership slice.
    void ViewModule::ProcessIncomingLanguageEvent(const void* lpEvent, s32 liSize,
                                                  s32 liEventId,
                                                  ViewIO::OutputBuffer* lpOutput)
    {
        (void)lpEvent;
        (void)liSize;
        (void)liEventId;
        (void)lpOutput;
    }

    // X360 @0x828586E8 -- NOT YET RECONSTRUCTED (the ledger dossier holds the
    // reviewed body): the unload-request notification (15). Land with the
    // load-ownership slice.
    void ViewModule::ProcessIncomingUnloadRequestNotification(const void* lpEvent)
    {
        (void)lpEvent;
    }

    // GetMovieNameByLevel @0x824EBCA8. The guest arithmetic `32*(liLevel+1783) + this`
    // is exactly &macCurrentlyPlayingMovies[liLevel] (the array sits at [c:+57056] ==
    // 32*1783, stride 32); DWARF declares the const char* return.
    const char* ViewModule::GetMovieNameByLevel(int liLevel) const
    {
        CGS_ASSERT(liLevel >= 0 && liLevel < KI_NUM_MOVIE_LEVELS,
                   "liLevel>=0 && liLevel < KI_NUM_MOVIE_LEVELS");
        return macCurrentlyPlayingMovies[liLevel];
    }

    void ViewModule::SetClearScreenAlpha(f32 lfAlpha)
    {
        CGS_ASSERT(lfAlpha >= 0.0f && lfAlpha <= 1.0f,
                   "lfAlpha must be in [0,1] in SetClearScreenAlpha");
        mfClearScreenAlpha = lfAlpha;
    }

    // SetCustomRendererManager @0x824EBBF8. Install the manager, wire the module
    // sub-systems into it (three vtable dispatches, the manager pointer re-read from
    // the member slot before each call, matching the asm `lwz r3,0(r31)`), then mirror
    // the pointer into the render handler's slot (guest +58596 == mRenderHandler+0xB4).
    void ViewModule::SetCustomRendererManager(CustomRendererManager* lpCustomRendererManager,
                                              int liArg3, int liArg4)
    {
        (void)liArg3;   // guest r5: never read by the X360 body

        CGS_ASSERT(lpCustomRendererManager != 0, "lpCustomRendererManager");

        mpCustomRendererManager = lpCustomRendererManager;

        CustomRendererManagerWiring* lpWiring =
            reinterpret_cast<CustomRendererManagerWiring*>(mpCustomRendererManager);
        lpWiring->SetTextRenderer(&mTextRenderer);
        lpWiring->SetLanguageManager(&mLanguageManager);
        lpWiring->SetExtraWiring(liArg4);

        mAptAux.mRenderHandler.SetCustomRendererManager(mpCustomRendererManager);
    }

    // RenderBlackScreen @0x82858988 -- when the clear screen is enabled (+0xE000), draw
    // the alpha-modulated black clear quad through the Im2d command buffer: open a render
    // block, install the unit-to-screen transform (the X360 reads the constant transform
    // at flt_830112D0), bind the untextured/cull-none/standard-blend frame states from
    // the shared state library, then submit a 4-vertex triangle strip over the unit
    // square whose colour is black at mfClearScreenAlpha (+0xE004), and close the block.
    void ViewModule::RenderBlackScreen()
    {
        if (!mbClearScreenEnabled)
            return;

        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrBuffer =
            mImRenderers.mpIm2dRenderBuffer->mCommandBuffer;

        lrBuffer.BeginRendering();

        // The constant unit-to-screen transform (X360 flt_830112D0): the body draws the
        // quad over the UNIT square, so the batch transform maps it onto the full
        // 1280x720 logical screen (the fixed logical space the 2D dispatch scales from),
        // with the identity colour transform (scale 255 / shift 0 in the CXForm
        // (A,R,G,B) channel order the dispatch decodes). The X360 data global is not
        // exported; the values are pinned by that observable contract (full-screen
        // clear + unmodified vertex colour).
        CgsGraphics::Im2dTransform lUnitToScreen;
        lUnitToScreen.mOriginXYZ.SetZero();          // origin (0,0)
        lUnitToScreen.mRightUp.x = 1280.0f;          // right = (1280, 0)
        lUnitToScreen.mRightUp.y = 0.0f;
        lUnitToScreen.mRightUp.z = 0.0f;             // up    = (0, 720)
        lUnitToScreen.mRightUp.w = 720.0f;
        lUnitToScreen.mColourShift.SetZero();        // (A,R,G,B) shift 0
        lUnitToScreen.mColourScale.x = 255.0f;       // (A,R,G,B) scale identity
        lUnitToScreen.mColourScale.y = 255.0f;
        lUnitToScreen.mColourScale.z = 255.0f;
        lUnitToScreen.mColourScale.w = 255.0f;
        lrBuffer.SetTransform(lUnitToScreen);

        // Frame states: the X360 binds the shared state library's untextured texture
        // state (dword_83010F5C), cull-none rasteriser (dword_83010F3C) and standard
        // alpha-blend (dword_83010F20). The ImRendererBase::StateLibrary global is not
        // modelled in this slice (see AptRenderHandler::Construct's white-texture note);
        // the PC dispatch installs exactly those defaults in its prologue, and the
        // untextured contract is carried by the null-texture command (the dispatch's
        // SELECTARG2/diffuse-only path). FLAG: bind the three library states here once
        // the state-library global lands.
        lrBuffer.SetTexture(0);

        // The unit-square strip: (0,0) (0,1) (1,0) (1,1), UV == position, colour black
        // at the clear-screen alpha (the X360 packs (alpha*255) into the byte-swizzled
        // vertex colour word; the PC vertex carries named colour bytes).
        u8 lu8Alpha = static_cast<u8>(mfClearScreenAlpha * 255.0f);
        CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];
        const f32 lafCorners[4][2] = { { 0.0f, 0.0f }, { 0.0f, 1.0f },
                                       { 1.0f, 0.0f }, { 1.0f, 1.0f } };
        for (int liIndex = 0; liIndex < 4; ++liIndex)
        {
            laVertices[liIndex].mv2Pos.x    = lafCorners[liIndex][0];
            laVertices[liIndex].mv2Pos.y    = lafCorners[liIndex][1];
            laVertices[liIndex].mv2Tex0UV.x = lafCorners[liIndex][0];
            laVertices[liIndex].mv2Tex0UV.y = lafCorners[liIndex][1];
            laVertices[liIndex].mv4Colour.r = 0;
            laVertices[liIndex].mv4Colour.g = 0;
            laVertices[liIndex].mv4Colour.b = 0;
            laVertices[liIndex].mv4Colour.a = lu8Alpha;
        }
        lrBuffer.Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

        lrBuffer.EndRendering();
    }

    // RenderInternal @0x82858AF8 -- render the shared view content for one frame:
    // bracket the frame with the custom-renderer-manager render notifies (guest vtbl
    // slot +0x18, phase 1 open / phase 2 close -- the handoff that publishes the
    // filled renderer set to the console's render-side consumer), open the Im2d (and
    // MenusAndHud Im3d) command buffers, bind the frame-default states, run the Apt
    // render walk with the render-side elapsed milliseconds, then close the buffers.
    void ViewModule::RenderInternal(const ViewIO::InputBuffer* lpInput)
    {
        CGS_ASSERT(lpInput != 0, "lpViewInput");

        // Custom-renderer-manager phase-1 notify (mgr->vtbl[+0x18](mgr, &mImRenderers, 1)).
        // No manager is installed on the PC boot path and the manager's real vtable order
        // is un-recovered (the same deferred hook as ProcessIncomingViewEvents'); the PC
        // consumption of the filled buffers is the dispatch the render drive runs after
        // this returns. FLAG (deferred dispatch): wire both notifies when the
        // CustomRendererManager type lands.

        const s32 liDeltaMs = static_cast<s32>(mfRenderTimeDelta * 1000.0f);

        CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>& lrBuffer =
            mImRenderers.mpIm2dRenderBuffer->mCommandBuffer;

        lrBuffer.BeginRendering();
        // The MenusAndHud Im3d buffer bracket (guest +0x260: BeginRendering here, its
        // EndRendering below). The 3D immediate buffers are not wired on the PC-minimal
        // path -- the slot carries the host's non-null assert-satisfier, not a real
        // buffer (the boot/title movies are 2D-only) -- so the bracket is STRUCTURALLY
        // GATED on the Im3d render-buffer instantiations landing.

        // Frame-default states (standard blend dword_83010F20, cull-none rasteriser
        // dword_83010F3C, z-buffer-off depth dword_83010F54 from the shared state
        // library). The StateLibrary global is not modelled in this slice; the PC
        // dispatch prologue installs exactly those defaults (blend on/src-alpha,
        // cull none, z off). FLAG: bind the three library states here once the
        // state-library global lands.

        mAptAux.Render(liDeltaMs);

        lrBuffer.EndRendering();

        // Custom-renderer-manager phase-2 notify (mgr->vtbl[+0x18](mgr, &mImRenderers, 2))
        // -- deferred with phase 1 above.
    }

    // Render @0x82858810 -- the public per-frame render entry (CgsGui::GuiModule::Render
    // @0x8285AF38 drives it with the view input buffer the GUI module owns): under the
    // input read lock, assert + copy the input buffer's renderer set into mImRenderers,
    // assign the input camera, advance the render-side time bookkeeping
    // (mfRenderTimeDelta = mfCurrentTime - mfLastRenderTime -- the milliseconds feed the
    // Apt consumed-render-tick bank), dispatch the RenderInternal virtual (guest vtbl
    // +0x4C), then re-null the copied renderer slots (slot 1 excepted: the X360 leaves
    // +0x254 untouched).
    void ViewModule::Render(const ViewIO::InputBuffer* lpViewInput)
    {
        lpViewInput->LockForRead();

        const ViewIO::ImRendererSet& lrRenderers = lpViewInput->GetImRenderers();
        CGS_ASSERT(lrRenderers.mpIm2dRenderBuffer != 0,
                   "lpViewInput->GetImRenderers().mpIm2dRenderBuffer");
        // The X360 also asserts mpIm3dRenderBufferUntex / mpIm3dRenderBufferRacePosition /
        // mpIm3dRenderBufferMenusAndHud non-null. The 3D immediate buffers are not wired
        // on the PC-minimal path (RenderInternal's 3D bracket is structurally gated on
        // their instantiations landing), so those three asserts return with that slice.

        mImRenderers.mpIm2dRenderBuffer =
            static_cast<CgsGui::AptIm2dRenderBuffer*>(lrRenderers.mpIm2dRenderBuffer);
        mImRenderers.mpReserved04                   = lrRenderers.mpReserved04;
        mImRenderers.mpIm3dRenderBufferUntex        = lrRenderers.mpIm3dRenderBufferUntex;
        mImRenderers.mpIm3dRenderBufferRacePosition = lrRenderers.mpIm3dRenderBufferRacePosition;
        mImRenderers.mpIm3dRenderBufferMenusAndHud  = lrRenderers.mpIm3dRenderBufferMenusAndHud;

        // FLAG (deferred member): the guest assigns the input camera (set+0x20) into the
        // module's embedded CgsGraphics::Camera at [c:+624] (Camera::operator=). The
        // module's camera member is not modelled yet (see Construct's camera note); land
        // the copy with the Camera lifecycle TU.

        // Render-side time bookkeeping: the delta between the update-side accumulated
        // time (mfCurrentTime, advanced by Update) and the last render's view of it.
        const f32 lfLastRenderTime = mfLastRenderTime;
        mfLastRenderTime  = mfCurrentTime;
        mfRenderTimeDelta = mfCurrentTime - lfLastRenderTime;

        // The RenderInternal virtual dispatch (guest vtbl slot +0x4C -- BrnGui::ViewModule
        // overrides it with the black-screen + base + Flapt chain).
        RenderInternal(lpViewInput);

        // Re-null the copied renderer slots (0/2/3/4; slot 1 is left as copied).
        mImRenderers.mpIm2dRenderBuffer             = 0;
        mImRenderers.mpIm3dRenderBufferUntex        = 0;
        mImRenderers.mpIm3dRenderBufferRacePosition = 0;
        mImRenderers.mpIm3dRenderBufferMenusAndHud  = 0;

        lpViewInput->UnlockForRead();
    }

    // X360 @0x8285BD30 -- NOT YET RECONSTRUCTED: the real body registers the loaded
    // resource with the Apt data handler (AddAptData), loads the string table, and
    // validates/collects fonts. Land it with the load-ownership slice.
    void ViewModule::ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");
    }
}
