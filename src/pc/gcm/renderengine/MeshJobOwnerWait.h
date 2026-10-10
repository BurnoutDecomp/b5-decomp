#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "SDKs/EATech/eajobs/job_types.h"

// FLAG PC-platform leaf: update-side mesh jobs are joined by the window/frame
// owner. An asserting worker waits for that owner, and concurrent D3D work can
// send synchronous window messages. Keep both dependencies moving while joining.
namespace renderengine
{
    struct MeshJobOwnerWaitPC
    {
        bool mbQuit = false;
        int miQuitCode = 0;

        static EA::Jobs::WaitOnControl Poll(void* lpContext)
        {
            auto& lrWait = *static_cast<MeshJobOwnerWaitPC*>(lpContext);
            CgsDev::Assert::ServiceWorkerAssertsWhileWaitingPC();
            MSG lMessage;
            while (PeekMessageW(&lMessage, nullptr, 0, 0, PM_REMOVE))
            {
                if (lMessage.message == WM_QUIT)
                {
                    lrWait.mbQuit = true;
                    lrWait.miQuitCode = static_cast<int>(lMessage.wParam);
                }
                else
                {
                    TranslateMessage(&lMessage);
                    DispatchMessageW(&lMessage);
                }
            }
            return EA::Jobs::WAIT_ON_YIELD_THEN_CONTINUE;
        }

        ~MeshJobOwnerWaitPC()
        {
            if (mbQuit) PostQuitMessage(miQuitCode);
        }
    };
}
