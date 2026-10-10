#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h"

#include <string.h>                                                // _stricmp / strlen
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"            // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"       // CgsSceneManager::K_INVALID_ENTITY_ID
#include "GameSource/Director/Camera/Camera.h"                     // Camera (Update writes its transform and state)
#include "GameSource/Director/Camera/BrnCameraState.h"             // CameraState::E_FLAG_*
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"     // Utils::Tweaker::Construct
#include "GameSource/Director/Camera/Utils/CameraUtils.h"          // Utils::CreateLookAt
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"        // Utils::ConsoleVpu (the fused VMX forms)
#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"      // SceneQueryInterface::LineTestNearest

// BrnDirector::Camera::BehaviourRenderMetrics -- the render-metrics probe camera: its Behaviour
// overrides, the parameter pair, the GameTalk receiver and the private steps they share
// (SetToWait / SetToGetMetrics are inlined into their callers on the console).
//
// The float streaming in the receiver goes through CgsDev::StrStreamBase::operator<<(f32) and the
// virtual char* sink for the separator strings, exactly the committed CgsLog surface.

// The title network-address query. Its PC body lives in the platform layer
// (GameShared/GameClasses/System/PC/CgsXboxLivePC.cpp); it fills a 36-byte address record whose
// leading four bytes are the title's IP address.
extern "C" u32 XNetGetTitleXnAddr(void* lpXnAddr);

namespace
{
    using rw::math::vpu::Vector3;

    const f32 KF_CAMERA_HEIGHT_ABOVE_GROUND = 2.0f;     // the camera sits this far above the hit
    const f32 KF_LINE_TEST_LENGTH           = 100.0f;   // the ground probe reaches this far down
    const f32 KF_Y_OFFSET_STEP              = 2.5f;     // each missed probe restarts this much higher
    const f32 KF_MAX_Y_OFFSET               = 500.0f;   // give up and use the requested position

    const u32 KU_GROUND_ENTITY_TYPE_FLAGS   = 2u;       // the entity types the ground probe hits
    const u8  KU_ALL_VOLUME_TYPES           = 0xFFu;

    const u32 KU_XNADDR_SIZE                = 36u;

    // rw::math::vpu::Cross as the console expands it: the (y, z, x) permutes, one vmulfp128 and a
    // fused vnmsubfp, then the (y, z, x) permute of the difference -- lLhs x lRhs, rounded the
    // console's way.
    Vector3 CrossConsole(const Vector3& lLhs, const Vector3& lRhs)
    {
        using BrnDirector::Camera::Utils::ConsoleVpu::NegativeMultiplySubtract;

        const Vector3 lProduct = { lLhs.x * lRhs.y, lLhs.y * lRhs.z, lLhs.z * lRhs.x, lLhs.w * lRhs.w };
        const Vector3 lDifference = {
            NegativeMultiplySubtract(lLhs.y, lRhs.x, lProduct.x),
            NegativeMultiplySubtract(lLhs.z, lRhs.y, lProduct.y),
            NegativeMultiplySubtract(lLhs.x, lRhs.z, lProduct.z),
            NegativeMultiplySubtract(lLhs.w, lRhs.w, lProduct.w) };
        return Vector3{ lDifference.y, lDifference.z, lDifference.x, lDifference.w };
    }
}

namespace BrnDirector
{
namespace Camera
{

const f32 BehaviourRenderMetrics::KF_ROTATION_WAIT = 0.25f;
const f32 BehaviourRenderMetrics::KF_MOVE_WAIT     = 1.0f;

Vector3 BehaviourRenderMetrics::saAngles[BehaviourRenderMetrics::KU_NUM_ANGLES] =
{
    {  0.0f, 0.0f,  1.0f, 0.0f },
    {  0.7f, 0.0f,  0.7f, 0.0f },
    {  1.0f, 0.0f,  0.0f, 0.0f },
    {  0.7f, 0.0f, -0.7f, 0.0f },
    {  0.0f, 0.0f, -1.0f, 0.0f },
    { -0.7f, 0.0f, -0.7f, 0.0f },
    { -1.0f, 0.0f,  0.0f, 0.0f },
    { -0.7f, 0.0f,  0.7f, 0.0f },
};

// ----------------------------------------------------------------------------
// Construct -- the base head, an empty line-test box and no parameters.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::Construct()
{
    Behaviour::Construct();
    mLineTest.Construct();
    mpParameters = 0;
}

// ----------------------------------------------------------------------------
// Prepare -- reset the run state and register for the tool's "Camera" messages with this
// behaviour as the context. Always true.
// ----------------------------------------------------------------------------
bool BehaviourRenderMetrics::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mfYOffset = 0.0f;
    SetPrepared();
    meCameraState = E_WAITING_TO_START;
    mbTargetReady = false;
    mbIdleCamOn   = false;

    EA::GameTalk::GameTalkManager::RegisterMessageHandler(EA::GameTalk::GameTalkManager::GetInstance(),
                                                          &BehaviourRenderMetrics::GameTalkMessageReceiver,
                                                          "Camera", this);
    return true;
}

// ----------------------------------------------------------------------------
// SetToWait (inlined on the console) -- arm the settle timer.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::SetToWait(f32 lfTimeToWait, ECameraState leAfterWaitState)
{
    mfTimeToWait     = lfTimeToWait;
    meCameraState    = E_WAIT;
    meAfterWaitState = leAfterWaitState;
    mfTimeWaited     = 0.0f;
}

// ----------------------------------------------------------------------------
// RequestPosition -- a nearest-hit line test from lPosition straight down KF_LINE_TEST_LENGTH,
// answered into mLineTest; E_WAIT polls it.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::RequestPosition(const BehaviourSharedInfo& lrSharedInfo, Vector3 lPosition)
{
    const Vector3 lLineEnd = lPosition - rw::math::vpu::GetVector3_YAxis() * KF_LINE_TEST_LENGTH;

    lrSharedInfo.mpSceneQueryInterface->LineTestNearest(mLineTest, KU_GROUND_ENTITY_TYPE_FLAGS,
                                                        KU_ALL_VOLUME_TYPES, lPosition, lLineEnd,
                                                        CgsSceneManager::K_INVALID_ENTITY_ID,
                                                        CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY);
    mbWaitingForLineTest = true;
}

// ----------------------------------------------------------------------------
// SendReadyMessage -- one "TargetReady" message to the tool's listener:
//   "<activation id>;<a.b.c.d>;<address as one number>;<build date>;<build time>;<platform>"
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::SendReadyMessage(const BehaviourSharedInfo& lrSharedInfo)
{
    char lacPlatform[8];
    CgsCore::SPrintf(lacPlatform, sizeof(lacPlatform), "X360");

    u8 lauTitleAddress[KU_XNADDR_SIZE];
    XNetGetTitleXnAddr(lauTitleAddress);

    char lacAddress[20];
    CgsCore::SPrintf(lacAddress, sizeof(lacAddress), "%u.%u.%u.%u",
                     lauTitleAddress[0], lauTitleAddress[1], lauTitleAddress[2], lauTitleAddress[3]);

    const u32 luAddress = (static_cast<u32>(lauTitleAddress[0]) << 24) |
                          (static_cast<u32>(lauTitleAddress[1]) << 16) |
                          (static_cast<u32>(lauTitleAddress[2]) << 8)  |
                           static_cast<u32>(lauTitleAddress[3]);

    char lacReady[64];
    CgsCore::SPrintf(lacReady, sizeof(lacReady), "%u;%s;%u;%s;%s;%s",
                     lrSharedInfo.muRenderMetricsActivationID, lacAddress, luAddress,
                     __DATE__, __TIME__, lacPlatform);

    EA::GameTalk::GameTalkMessage lMessage("Camera");
    lMessage.AddKeyContent("TargetReady", 0, lacReady, static_cast<s32>(strlen(lacReady)));
    EA::GameTalk::GameTalkManager::GetInstance()->SendMessage("Tool.RenderMetricsListener", lMessage);

    mbTargetReady = true;
}

// ----------------------------------------------------------------------------
// Update -- the probe state machine. Every frame the camera is valid unless the behaviour has
// failed, and the two metrics flags start clear.
//   E_WAITING_TO_START  announce readiness once; show the idle camera when the tool set one.
//   E_START             probe the ground under the requested position, then settle.
//   E_WAIT              while the probe is out: on a hit above y = 0 park the camera above it
//                       and take the ground normal; on a miss probe again KF_Y_OFFSET_STEP
//                       higher, up to KF_MAX_Y_OFFSET, then give up and use the requested
//                       position on flat ground. Once parked, look along the ground normal
//                       crossed with this step's direction and, while the world is streamed,
//                       count the settle time down.
//   E_RECORD_METRICS    flag this direction for recording; turn to the next, or after the last
//                       flag the metrics for sending and go back to waiting.
// Always true.
// ----------------------------------------------------------------------------
bool BehaviourRenderMetrics::Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    const f32 lfTimestep = lrSharedInfo.GetTimestep(BrnDirector::Timestep::E_WORLD_NO_SLOMO);

    if (!HasFailed())
    {
        lrCamera.GetState().SetFlag(CameraState::E_FLAG_VALID, true);
    }
    lrCamera.GetState().ClearFlag(CameraState::E_FLAG_RECORD_METRICS);
    lrCamera.GetState().ClearFlag(CameraState::E_FLAG_SEND_METRICS);

    switch (meCameraState)
    {
    case E_WAITING_TO_START:
        if (!mbTargetReady)
        {
            SendReadyMessage(lrSharedInfo);
        }
        if (mbIdleCamOn)
        {
            lrCamera.mTransform = Utils::CreateLookAt(mIdlePosition, mIdlePosition + mIdleDirection);
            lrCamera.ValidateTransformWithDebugInfo();
        }
        break;

    case E_START:
        RequestPosition(lrSharedInfo, mInitialPosition);
        muAnglesChecked = 0;
        SetToWait(KF_MOVE_WAIT, E_RECORD_METRICS);
        break;

    case E_WAIT:
        if (mbWaitingForLineTest)
        {
            if (!mLineTest.HasPackage())
            {
                break;
            }

            const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult& lrResult =
                mLineTest.GetPackage();

            if (lrResult.mbIntersection && lrResult.mPosition.y > 0.0f)
            {
                mCamPosition = Vector3{ lrResult.mPosition.x,
                                        lrResult.mPosition.y + KF_CAMERA_HEIGHT_ABOVE_GROUND,
                                        lrResult.mPosition.z, 0.0f };
                mbWaitingForLineTest = false;
                mfYOffset            = 0.0f;
                mLineTest.Clear();
                mGroundNormal        = lrResult.mNormal;
            }
            else if (mfYOffset < KF_MAX_Y_OFFSET)
            {
                mfYOffset += KF_Y_OFFSET_STEP;
                mLineTest.Clear();
                RequestPosition(lrSharedInfo,
                                Utils::ConsoleVpu::MultiplyAdd(rw::math::vpu::GetVector3_YAxis(),
                                                               mfYOffset, mInitialPosition));
            }
            else
            {
                mfYOffset = 0.0f;
                mLineTest.Clear();
                mbWaitingForLineTest = false;
                mCamPosition  = mInitialPosition;
                mGroundNormal = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
            }
        }
        else
        {
            const Vector3 lLookDirection =
                CrossConsole(mGroundNormal, saAngles[(muAnglesChecked + 2) % KU_NUM_ANGLES]);
            lrCamera.mTransform = Utils::CreateLookAt(mCamPosition, mCamPosition + lLookDirection);
            lrCamera.ValidateTransformWithDebugInfo();

            if (lrSharedInfo.mbAllStreamed)
            {
                mfTimeWaited += lfTimestep;
                if (!(mfTimeWaited < mfTimeToWait))
                {
                    meCameraState = meAfterWaitState;
                }
            }
        }
        break;

    case E_RECORD_METRICS:
        lrCamera.GetState().SetFlag(CameraState::E_FLAG_RECORD_METRICS, true);
        ++muAnglesChecked;
        if (muAnglesChecked < KU_NUM_ANGLES)
        {
            SetToWait(KF_ROTATION_WAIT, E_RECORD_METRICS);
        }
        else
        {
            lrCamera.GetState().SetFlag(CameraState::E_FLAG_SEND_METRICS, true);
            meCameraState = E_WAITING_TO_START;
        }
        break;

    default:
        break;
    }

    return true;
}

// ----------------------------------------------------------------------------
// The parameter pair: SetParameters asserts the block carries the render-metrics tag.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::SetParameters(const Behaviour::Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourRenderMetrics,
               "lpParameters->GetType() == eBehaviourRenderMetrics");
    mpParameters = static_cast<const Parameters*>(lpParameters);
}

const Behaviour::Parameters* BehaviourRenderMetrics::GetParameters() const
{
    return mpParameters;
}

void BehaviourRenderMetrics::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourRenderMetrics::GetName() const
{
    return "BehaviourRenderMetrics";
}

// ----------------------------------------------------------------------------
// SetToGetMetrics (inlined into the receiver on the console) -- log the request; start a run at
// lPosition when idle, else log that the probe was busy.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::SetToGetMetrics(Vector3 lPosition)
{
    using namespace CgsDev::Message;

    if (gxMessageFilterFlags & 1)
    {
        CgsDev::StrStreamBase& lrPrint = *CgsDev::Log::gpDebugPrint;
        lrPrint << "Request to get metrics at ";
        lrPrint << lPosition.x;
        lrPrint << ",";
        lrPrint << lPosition.y;
        lrPrint << ",";
        lrPrint << lPosition.z;
        lrPrint << "\n.";
    }

    if (meCameraState == E_WAITING_TO_START)
    {
        mbIdleCamOn      = false;
        meCameraState    = E_START;
        mInitialPosition = lPosition;
    }
    else if (gxMessageFilterFlags & 1)
    {
        *CgsDev::Log::gpDebugPrint
            << "Unable to start getting metrics, camera behaviour controller was not ready.\n";
    }
}

// ----------------------------------------------------------------------------
// GameTalkMessageReceiver -- see the header for the per-key command map. lpContext is the
// address of the handler entry's context slot, which holds this behaviour.
// ----------------------------------------------------------------------------
void BehaviourRenderMetrics::GameTalkMessageReceiver(EA::GameTalk::GameTalkMessage* lpMessage, void* lpContext)
{
    BehaviourRenderMetrics* lpBehaviour = *static_cast<BehaviourRenderMetrics**>(lpContext);

    for (s32 liIndex = 0; liIndex < lpMessage->GetNumKeys(); ++liIndex)
    {
        const char* lpcKey = lpMessage->GetKey(liIndex);
        if (_stricmp(lpcKey, "GetMetrics") == 0)
        {
            const f32* lpfContent = static_cast<const f32*>(lpMessage->GetContent(liIndex));
            lpBehaviour->SetToGetMetrics(Vector3{ lpfContent[0], lpfContent[1], lpfContent[2], 0.0f });
        }
        else if (_stricmp(lpcKey, "SetIdleMetricsCamera") == 0)
        {
            const f32* lpfContent = static_cast<const f32*>(lpMessage->GetContent(liIndex));
            CGS_ASSERT(lpMessage->GetSize(liIndex) >= (int)(6 * sizeof(f32)),
                       "lpMessage->GetSize(luIndex) >= (int)(6 * sizeof(float32_t))");

            lpBehaviour->mIdlePosition  = Vector3{ lpfContent[0], lpfContent[1], lpfContent[2], 0.0f };
            lpBehaviour->mbIdleCamOn    = true;
            lpBehaviour->mIdleDirection = Vector3{ lpfContent[3], lpfContent[4], lpfContent[5], 0.0f };
        }
        else if (_stricmp(lpcKey, "StopRenderMetrics") == 0)
        {
            lpBehaviour->meCameraState = E_WAITING_TO_START;
        }
    }
}

}
}
