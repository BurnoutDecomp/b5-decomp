#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"

#include <cstring>

#include "GameShared/GameClasses/Core/CgsAssert.h"

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
        mImRenderers.mpIm2dRenderer = 0;
        mImRenderers.mpReserved08 = 0;
        mImRenderers.mpReserved0C = 0;
        mImRenderers.mp3dRenderer = 0;
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

    // X360 @0x82860708 -- NOT YET RECONSTRUCTED (evidence in the ledger dossier): the
    // real body pumps the module IO buffer stacks, advances mfCurrentTime and the
    // update/render time deltas, and (when prepared) ticks CgsGui::AptAux::Update
    // @0x82853B20. Land it with the update-ownership slice.
    void ViewModule::Update(ViewIO::IOBufferStack* lpInStack,
                            ViewIO::IOBufferStack* lpOutStack,
                            const ViewIO::InputBuffer* lpInput,
                            ViewIO::OutputBuffer* lpOutput)
    {
        (void)lpInStack;
        (void)lpOutStack;
        (void)lpInput;
        (void)lpOutput;
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

    // X360 @0x82858988 -- NOT YET RECONSTRUCTED: the real body draws the
    // alpha-modulated black clear quad (mbClearScreenEnabled / mfClearScreenAlpha)
    // through the immediate-mode renderer. Land it with the render-ownership slice.
    void ViewModule::RenderBlackScreen()
    {
    }

    // X360 @0x82858AF8 -- NOT YET RECONSTRUCTED: the real body renders the shared view
    // content for one frame (clear quad + AptAux render drive). Land it with the
    // render-ownership slice.
    void ViewModule::RenderInternal(const ViewIO::InputBuffer* lpInput)
    {
        (void)lpInput;
    }

    // X360 @0x8285BD30 -- NOT YET RECONSTRUCTED: the real body registers the loaded
    // resource with the Apt data handler (AddAptData), loads the string table, and
    // validates/collects fonts. Land it with the load-ownership slice.
    void ViewModule::ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");
    }
}
