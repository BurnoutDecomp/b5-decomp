#include "GameSource/Director/BrnDirectorDevTools.h"

#include <string.h>                                          // _stricmp (the GameTalk key dispatch)
#include <stdlib.h>                                          // atof (the CameraPos float fields)
#include "rw/core/stdc/stdc.h"                               // rw::core::stdc::ConvertAToI
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitrator.h"   // Arbitrator (render-metrics pokes + the debug fly camera)
#include "GameSource/Director/BrnDirectorICEWrapper.h"
#include "GameSource/Director/BrnDirectorResourceManager.h"
#include "GameSource/Director/BrnMainDirector.h"
#include "GameSource/Director/Camera/Camera.h"               // Camera::Camera (LiveCamUpdate's transform reads)
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
#include "SDKs/Packages/ICE/ICEAuthor.hpp"                   // ICE::ICEAuthor (take/assembly ops)

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

    // -----------------------------------------------------------------------
    // @ 0x822095A0 -- the GameTalk message callback (this TU; the asserts name
    // BrnDirectorDevTools.cpp:130/:219/:244). Single-key messages only; the key
    // dispatch order below is the asm's stricmp chain:
    //   "SendCamera"         raise the send-camera latch (LiveCamUpdate answers).
    //   "CameraPos"          parse 6 comma-TERMINATED floats -> the fly pose +
    //                        the fly latch (exactly 6 fields or nothing).
    //   "StartRenderMetrics" poke the arbitrator's render-metrics request pair
    //                        (the do-flag + the content's first byte).
    //   "StopRenderMetrics"  clear the arbitrator's do-flag.
    //   "ICEEdit"            GUID -> take (resolve or create "New Take") ->
    //                        editor ON for it.
    //   "ICEClose"           editor OFF.
    //   "ICEAssemblyEdit"    GUID -> take (resolve or create "New Assembly") ->
    //                        author EditAssembly.
    //   "ICETakeBrowse"      GUID -> when the take exists and the editor is up,
    //                        author SetAssemblySourceTake.
    //   "ICESetPostFXHook"   value -> when the editor is up on channel 9 (the
    //                        gameplay space), author interval element 44.
    // -----------------------------------------------------------------------
    void DirectorDevTools::GameTalkMsgHandler(EA::GameTalk::GameTalkMessage* lpMessage)
    {
        CGS_ASSERT(mpInstance != nullptr, "mpInstance!=NULL");   // :130 (non-fatal on the X360; the dispatch runs regardless)

        if (lpMessage->GetNumKeys() != 1)
            return;

        const char* lpcKey = lpMessage->GetKey(0);
        if (_stricmp(lpcKey, "SendCamera") == 0)
        {
            mpInstance->muLiveCamRequest = E_LIVECAMREQUEST_SEND_CAMERA;
        }
        else if (_stricmp(lpcKey, "CameraPos") == 0)
        {
            const char* lpcContent =
                static_cast<const char*>(lpMessage->GetKeyContent("CameraPos"));

            // The X360's own scanner: accumulate characters into a field buffer;
            // each ',' closes a field through atof. Fields are counted on commas
            // ONLY (a NUL without a trailing comma drops the final field), and the
            // scan stops after 6 fields or the NUL, whichever first.
            char lacField[16];   // X360 sp+0x50 field accumulator
            f32  lafValues[6];
            s32  liNumValues = 0;
            s32  liFieldLength = 0;
            do
            {
                const char lcChar = *lpcContent;
                if (lcChar == ',')
                {
                    lacField[liFieldLength] = 0;
                    lafValues[liNumValues] = static_cast<f32>(atof(lacField));
                    ++liNumValues;
                    liFieldLength = 0;
                }
                else
                {
                    lacField[liFieldLength++] = lcChar;
                }
            }
            while (*lpcContent++ && liNumValues < 6);

            if (liNumValues == 6)
            {
                mpInstance->mFlyCamPosition =
                    Vector3{ lafValues[0], lafValues[1], lafValues[2], 0.0f };
                mpInstance->mFlyCamTarget =
                    Vector3{ lafValues[3], lafValues[4], lafValues[5], 0.0f };
                mpInstance->muLiveCamRequest = E_LIVECAMREQUEST_FLY_CAMERA;
            }
        }
        else if (_stricmp(lpcKey, "StartRenderMetrics") == 0)
        {
            const char* lpcContent =
                static_cast<const char*>(lpMessage->GetKeyContent("StartRenderMetrics"));
            Arbitrator& lrArbitrator = mpInstance->mpDirectorModule->GetArbitrator();
            lrArbitrator.SetDoRenderMetrics(true);
            lrArbitrator.SetRenderMetricsArg(static_cast<u8>(*lpcContent));
        }
        else if (_stricmp(lpcKey, "StopRenderMetrics") == 0)
        {
            mpInstance->mpDirectorModule->GetArbitrator().SetDoRenderMetrics(false);
        }
        else if (_stricmp(lpcKey, "ICEEdit") == 0)
        {
            mpInstance->miCurrentTakeGuid = rw::core::stdc::ConvertAToI(
                static_cast<const char*>(lpMessage->GetKeyContent("ICEEdit")));

            ICE::ICETakeData* lpTakeData = mpInstance->mpDirectorResourceManager
                                               ->GetKeyAnimFromGuid(mpInstance->miCurrentTakeGuid);
            if (lpTakeData == nullptr)
            {
                lpTakeData = mpInstance->mpDirectorModule->GetICEWrapper().GetAuthor()
                                 .CreateNewTake("New Take", mpInstance->miCurrentTakeGuid);
                CGS_ASSERT(lpTakeData, "lpTakeData");   // :219 (non-fatal; EditorOn runs regardless)
            }
            mpInstance->mpDirectorModule->GetICEWrapper().EditorOn(lpTakeData);
        }
        else if (_stricmp(lpcKey, "ICEClose") == 0)
        {
            mpInstance->mpDirectorModule->GetICEWrapper().EditorOff();
        }
        else if (_stricmp(lpcKey, "ICEAssemblyEdit") == 0)
        {
            mpInstance->miCurrentTakeGuid = rw::core::stdc::ConvertAToI(
                static_cast<const char*>(lpMessage->GetKeyContent("ICEAssemblyEdit")));

            ICE::ICETakeData* lpTakeData = mpInstance->mpDirectorResourceManager
                                               ->GetKeyAnimFromGuid(mpInstance->miCurrentTakeGuid);
            if (lpTakeData == nullptr)
            {
                lpTakeData = mpInstance->mpDirectorModule->GetICEWrapper().GetAuthor()
                                 .CreateNewTake("New Assembly", mpInstance->miCurrentTakeGuid);
                CGS_ASSERT(lpTakeData, "lpTakeData");   // :244 (non-fatal; EditAssembly runs regardless)
            }
            mpInstance->mpDirectorModule->GetICEWrapper().GetAuthor().EditAssembly(lpTakeData);
        }
        else if (_stricmp(lpcKey, "ICETakeBrowse") == 0)
        {
            const s32 liGuid = rw::core::stdc::ConvertAToI(
                static_cast<const char*>(lpMessage->GetKeyContent("ICETakeBrowse")));

            if (mpInstance->mpDirectorResourceManager->GetKeyAnimFromGuid(liGuid) != nullptr)
            {
                ICEWrapper& lrICEWrapper = mpInstance->mpDirectorModule->GetICEWrapper();
                if (lrICEWrapper.GetEditor().miState > 0)   // editor up (AreMenusActive)
                    lrICEWrapper.GetAuthor().SetAssemblySourceTake(liGuid);
            }
        }
        else if (_stricmp(lpcKey, "ICESetPostFXHook") == 0)
        {
            const s32 liValue = rw::core::stdc::ConvertAToI(
                static_cast<const char*>(lpMessage->GetKeyContent("ICESetPostFXHook")));

            ICE::ICEController& lrEditor =
                mpInstance->mpDirectorModule->GetICEWrapper().GetEditor();
            // Editor up AND the current channel is 9 (the X360 literal; channel 9
            // == the gameplay space in the ICE space table).
            if (lrEditor.miState > 0 && lrEditor.miCurrentChannel == 9)
                lrEditor.SetCurrentIntervalValue(44, liValue);   // element 44: the post-FX hook slot
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82239C80 -- consume the live-camera request latch (this TU).
    //   SEND_CAMERA: format the camera's position (transform wAxis) then its
    //     at-axis (zAxis) as "%4.4f,..." and ship it to "Tool.CanEdit" as the
    //     "CameraPos" key of a "Camera"-channel GameTalk message. (Note the
    //     outbound text has NO trailing comma -- the inbound scanner above needs
    //     one; the tool echoes back its own terminated form.)
    //   FLY_CAMERA (any latch value 2): point the arbitrator's debug fly-world
    //     camera at the parsed pose.
    // Both paths clear the latch; other values fall through untouched.
    // -----------------------------------------------------------------------
    void DirectorDevTools::LiveCamUpdate(const Camera::Camera& lrCamera)
    {
        if (muLiveCamRequest == E_LIVECAMREQUEST_NONE)
            return;

        if (muLiveCamRequest == E_LIVECAMREQUEST_SEND_CAMERA)
        {
            const rw::math::vpu::Matrix44Affine& lrTransform = lrCamera.GetTransform();

            char lacCameraPos[256];   // X360 sp+0x90 message text
            CgsCore::SPrintf(lacCameraPos, 256, "%4.4f,%4.4f,%4.4f,%4.4f,%4.4f,%4.4f",
                             lrTransform.wAxis.x, lrTransform.wAxis.y, lrTransform.wAxis.z,
                             lrTransform.zAxis.x, lrTransform.zAxis.y, lrTransform.zAxis.z);

            EA::GameTalk::GameTalkMessage lMessage("Camera");

            // The X360 rolls its own NUL scan for the content length.
            const char* lpcEnd = lacCameraPos;
            while (*lpcEnd)
                ++lpcEnd;
            lMessage.AddKeyContent("CameraPos", 0, lacCameraPos,
                                   static_cast<s32>(lpcEnd - lacCameraPos));

            // GetInstance() is evaluated for its side effect; SendMessage is the
            // static endpoint form (the ICEFile FileClose idiom, GameTalk.h).
            EA::GameTalk::GameTalkManager::GetInstance();
            EA::GameTalk::GameTalkManager::SendMessage("Tool.CanEdit", lMessage);

            muLiveCamRequest = E_LIVECAMREQUEST_NONE;
        }
        else if (muLiveCamRequest < 3u)
        {
            mpDirectorModule->GetArbitrator().DebugCameraFlyWorldLookAt(mFlyCamPosition,
                                                                        mFlyCamTarget);
            muLiveCamRequest = E_LIVECAMREQUEST_NONE;
        }
    }
}
