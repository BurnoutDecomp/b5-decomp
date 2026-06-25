#include "GameSource/Director/BrnDirectorDevTools.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnMainDirector.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"

namespace BrnDirector
{
    DirectorDevTools* DirectorDevTools::mpInstance = nullptr;

    void DirectorDevTools::Construct(MainDirector* lpDirectorModule,
                                     DirectorResourceManager* lpDirectorResourceManager)
    {
        CGS_ASSERT(!mpInstance, "mpInstance==NULL");
        CGS_ASSERT(lpDirectorModule, "lpCameraModule != NULL");

        mpDirectorResourceManager = lpDirectorResourceManager;
        mpDirectorModule = lpDirectorModule;
        mpInstance = this;
    }

    void DirectorDevTools::Update(const DirectorIO::InputBuffer* lpInput,
                                  const Camera::Camera& lrCamera)
    {
        LiveCamUpdate(lrCamera);

        CgsDev::DebugInterface lDebugInterface;
        CgsDev::DebugManager& lrDebugManager = lDebugInterface.GetDebugManager();
        CgsDev::DebugUI::DebugUI& lrDebugUI = lDebugInterface.GetUI();
        ICEWrapper& lrICEWrapper = mpDirectorModule->GetICEWrapper();

        lrICEWrapper.SetAcceptInput(!lrDebugUI.IsVisible());

        const bool lbEditorActive = lrICEWrapper.IsEditorActive();
        if (!lrICEWrapper.WasEditorActive() && lbEditorActive)
            lrICEWrapper.ReconstructCameraMover(
                mpDirectorResourceManager->GetIceResourceManager());
        lrICEWrapper.SetWasEditorActive(lbEditorActive);

        if (!lrDebugUI.IsVisible() && lbEditorActive)
            lrICEWrapper.UpdateAction(lpInput->GetControll()->mDebugController);

        CgsDev::DebugManager::ThreadSafeRelease(&lrDebugManager);
    }
}
