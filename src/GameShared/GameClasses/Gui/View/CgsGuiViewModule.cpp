#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"

#include <cstring>

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsGui
{
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

    void ViewModule::Construct(const char* lpcName, int liArg2, f32 lfAspectRatio,
                               const RGBA* lpAlternateTextColours, int liNumAlternateColours)
    {
        (void)lpcName;
        (void)liArg2;

        CgsModule::ModuleSingleBuffered::Construct();
        mLanguageManager.Construct();

        std::memset(&mImRenderers, 0, sizeof(mImRenderers));
        mTextRenderer.Construct();

        mAptAux.Construct(
            reinterpret_cast<::CgsGuiModuleIO::ImRendererSet*>(&mImRenderers),
            &mTextRenderer,
            &mLanguageManager,
            &mFonts,
            lfAspectRatio,
            reinterpret_cast<const rw::RGBA*>(lpAlternateTextColours),
            liNumAlternateColours);

        mfHack_LastValidTimeStep = 0.0f;
        mfCurrentTime = 0.0f;
        mfLastUpdateTime = 0.0f;
        mfLastRenderTime = 0.0f;
        mfUpdateTimeDelta = 0.0f;
        mfRenderTimeDelta = 0.0f;
        mpcLoadingMovieName = 0;
        miLoadingScreenLevel = 5;
        mpCustomRendererManager = 0;
        mbClearScreenEnabled = true;
        mfClearScreenAlpha = 1.0f;
        mbUpdateFlash = true;
        std::memset(macCurrentlyPlayingMovies, 0, sizeof(macCurrentlyPlayingMovies));
        mOutputEventQueue.Construct();
        mePrepareStage = E_PREPARESTAGE_START;
        meReleaseStage = E_RELEASESTAGE_DONE;
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
            mePrepareStage = E_PREPARESTAGE_MANAGER;
            // fall through
        case E_PREPARESTAGE_MANAGER:
            mOutputEventQueue.Prepare();
            if (!CgsModule::ModuleSingleBuffered::Prepare())
                return false;
            mePrepareStage = E_PREPARESTAGE_LANGUAGE;
            // fall through
        case E_PREPARESTAGE_LANGUAGE:
            if (!mLanguageManager.Prepare(lpLanguageHeap))
                return false;
            mePrepareStage = E_PREPARESTAGE_MOVIE;
            // fall through
        case E_PREPARESTAGE_MOVIE:
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

    void ViewModule::Destruct()
    {
        mLanguageManager.Destruct();
        mOutputEventQueue.Release();
        AptAuxPointer::mpAptAuxInst = 0;
        mpCustomRendererManager = 0;
        CgsModule::ModuleSingleBuffered::Destruct();
    }

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

    int ViewModule::GetMovieNameByLevel(int liLevel) const
    {
        CGS_ASSERT(liLevel >= 0 && liLevel < KI_NUM_MOVIE_LEVELS,
                   "liLevel out of range in GetMovieNameByLevel");
        return 32 * (liLevel + 1783) + static_cast<int>(reinterpret_cast<intptr_t>(this));
    }

    void ViewModule::SetClearScreenAlpha(f32 lfAlpha)
    {
        CGS_ASSERT(lfAlpha >= 0.0f && lfAlpha <= 1.0f,
                   "lfAlpha must be in [0,1] in SetClearScreenAlpha");
        mfClearScreenAlpha = lfAlpha;
    }

    void ViewModule::SetCustomRendererManager(CustomRendererManager* lpCustomRendererManager,
                                              int liArg3, int liArg4)
    {
        (void)liArg3;
        (void)liArg4;
        CGS_ASSERT(lpCustomRendererManager != 0, "lpCustomRendererManager");
        mpCustomRendererManager = lpCustomRendererManager;
    }

    void ViewModule::RenderBlackScreen()
    {
    }

    void ViewModule::RenderInternal(const ViewIO::InputBuffer* lpInput)
    {
        (void)lpInput;
    }

    void ViewModule::ProcessIncomingLoadNotification(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "lpEvent");
    }
}
