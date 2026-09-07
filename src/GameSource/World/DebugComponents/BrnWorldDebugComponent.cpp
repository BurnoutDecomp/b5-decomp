#include "GameSource/World/DebugComponents/BrnWorldDebugComponent.h"

#include "GameSource/Director/Camera/Camera.h"
#include "GameSource/Director/Camera/Utils/BrnDebugController.h"
#include "GameSource/World/BrnWorldModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/vector3_operation.h"

namespace BrnWorld
{
namespace
{
    const f32 KF_MPH_TO_MPS = 0.44703999f;
    const f32 KF_TEXT_POSITION_SCREEN_X = 30.0f;
    const f32 KF_TEXT_POSITION_SCREEN_Y = 30.0f;
    const f32 KF_TEXT_HEIGHT = 10.0f;
    const f32 KF_TEXT_LINE_HEIGHT = 20.0f;
    const f32 KF_VELOCITY_MIN = 0.0f;
    const f32 KF_VELOCITY_MAX = 500.0f;
    const f32 KF_VELOCITY_STEP = 10.0f;
    const CgsDev::RGBA KU_WHITE = 0xFFFFFFFFu;

    const char KAC_INACTIVE_TEXT[2][255] =
    {
        "- Vehicle Cannon -",
        "Hold B or Circle to control cannnon"
    };

    char KAC_ACTIVE_TEXT[5][255] =
    {
        "Press right shoulder button to prime",
        "Press right trigger to fire",
        "Left shoulder/trigger to control velocity",
        "",
        "Press d-pad down to clear"
    };

    typedef BrnDirector::Camera::Utils::DebugController CameraDebugController;

    const CameraDebugController* AsCameraDebugController(
        const BrnWorldIO::DebugController* lpController)
    {
        static_assert(sizeof(BrnWorldIO::DebugController) == sizeof(CameraDebugController),
                      "the world IO controller is the director debug-controller snapshot");
        return reinterpret_cast<const CameraDebugController*>(lpController);
    }
}

// @0x827B1828.
void WorldDebugComponent::Construct(WorldModule* lpWorldModule)
{
    CgsDev::DebugComponent::Construct();
    CGS_ASSERT(lpWorldModule != nullptr, "lpWorldModule != NULL");

    mpWorldModule = lpWorldModule;
    mfVehicleVelocity = 100.0f;
    mbShowTarget = false;
    mbHaveDebugController = false;
    mbVehicleGunIsPrimed = false;
    mVehiclePosition.SetZero();
    mVehicleDirection.SetZero();
    mbAIDrivesPlayer = false;
}

// @0x827DD1F0.
const char* WorldDebugComponent::GetName() const
{
    return "World Module";
}

bool WorldDebugComponent::IsSimple() const
{
    return false;
}

// @0x827BF4A8.
void WorldDebugComponent::OnActivate()
{
    CgsDev::DebugComponent::OnActivate();

    RegisterVariable(&mbAIDrivesPlayer, "AI Drives Player");
    SetChangeCallback(&mbAIDrivesPlayer, &WorldDebugComponent::AIDrivesPlayerChanged, this);

    RegisterVariable(&mbShowTarget, "Vehicle Cannon", "Activate Cannon");
    RegisterVariable(&mfVehicleVelocity, "Vehicle Cannon/Controls", "Velocity (mph)");
    SetRange(&mfVehicleVelocity, KF_VELOCITY_MIN, KF_VELOCITY_MAX);
    SetStep(&mfVehicleVelocity, KF_VELOCITY_STEP);
    RegisterFunction(&WorldDebugComponent::PrimeGunCallback, this, "Vehicle Cannon/Controls", "Prime");
    RegisterFunction(&WorldDebugComponent::FireGunCallback, this, "Vehicle Cannon/Controls", "Fire");
    RegisterFunction(&WorldDebugComponent::UnPrimeGunCallback, this, "Vehicle Cannon/Controls", "Clear");

    RegisterVariable(&mpWorldModule->mbStoreKBEachFrame, "", "Store data each frame");
    RegisterVariable(&mpWorldModule->mnDEBUGKBToStoreEachFrame, "", "KB to dump each frame");
    RegisterFunction(&WorldDebugComponent::ClearStoredFile, this, "Dumped file to hd", "Delete");

    RegisterFunction(&WorldDebugComponent::TriggerCollWorldInvalidate, this, "Collision", "Invalidate");
    RegisterFunction(&WorldDebugComponent::TriggerCollWorldValidate, this, "Collision", "Validate");

    RegisterVariable(&mpWorldModule->mb30hzEnvironmentMap, "Graphics", "Render environment map at 30hz");
    RegisterVariable(&mpWorldModule->mbForceOnlyBackdrops, "Graphics", "Force only backdrops");
    RegisterVariable(&mpWorldModule->mbRenderBackdrops, "Graphics", "Enable backdrop rendering");

    RegisterVariable(&mpWorldModule->mfCarKeyLightMultiplier, "Graphics", "Vehicle key light multiplier");
    SetRange(&mpWorldModule->mfCarKeyLightMultiplier, 0.0f, 10.0f);
    SetStep(&mpWorldModule->mfCarKeyLightMultiplier, 0.025f);
    RegisterVariable(&mpWorldModule->mfCarAmbientLightMultiplier, "Graphics", "Vehicle ambient light multiplier");
    SetRange(&mpWorldModule->mfCarAmbientLightMultiplier, 0.0f, 10.0f);
    SetStep(&mpWorldModule->mfCarAmbientLightMultiplier, 0.025f);

    RegisterVariable(&mpWorldModule->mShaderLodInfo.mbUseShaderLod, "Graphics/ShaderLOD", "Use shader LOD");
    RegisterVariable(&mpWorldModule->mShaderLodInfo.mfShaderLod1NearDistance,
                     "Graphics/ShaderLOD", "LOD1 distance thresh");
    RegisterVariable(&mpWorldModule->mShaderLodInfo.miEnvMapTechnique,
                     "Graphics/ShaderLOD", "Env map technique");
    RegisterVariable(&mpWorldModule->mShaderLodInfo.miOverrideTechnique,
                     "Graphics/ShaderLOD", "Override technique");
}

void WorldDebugComponent::OnRegister()
{
    CgsDev::DebugComponent::OnRegister();
}

// @0x827BF818.
void WorldDebugComponent::Update(const BrnWorldIO::DebugController* lpDebugController)
{
    CGS_ASSERT(lpDebugController != nullptr, "lpDebugControls != NULL");
    if (lpDebugController == nullptr)
    {
        mbHaveDebugController = false;
        return;
    }

    if (mbAIDrivesPlayer)
    {
        const EActiveRaceCarIndex lePlayerARCIndex = mpWorldModule->meLocalPlayerActiveRaceCarIndex;
        CGS_ASSERT(lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        if (lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
            lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            mpWorldModule->maeCarControls[lePlayerARCIndex] = 2;
        }
    }

    const CameraDebugController* lpControls = AsCameraDebugController(lpDebugController);
    mbHaveDebugController = lpControls->GetIsPressed(CameraDebugController::E_CONTROL_RIGHT_BUTTON);
    if (!mbHaveDebugController)
        return;

    if (lpControls->GetJustPressed(CameraDebugController::E_CONTROL_LOWER_TRIGGER_LEFT))
    {
        mfVehicleVelocity += KF_VELOCITY_STEP;
        if (mfVehicleVelocity > KF_VELOCITY_MAX)
            mfVehicleVelocity = KF_VELOCITY_MAX;
    }
    if (lpControls->GetJustPressed(CameraDebugController::E_CONTROL_UPPER_TRIGGER_LEFT))
    {
        mfVehicleVelocity -= KF_VELOCITY_STEP;
        if (mfVehicleVelocity < KF_VELOCITY_MIN)
            mfVehicleVelocity = KF_VELOCITY_MIN;
    }
    if (lpControls->GetJustPressed(CameraDebugController::E_CONTROL_UPPER_TRIGGER_RIGHT))
        PrimeGun();
    if (lpControls->GetJustPressed(CameraDebugController::E_CONTROL_LOWER_TRIGGER_RIGHT))
        FireGun();
    if (lpControls->GetJustPressed(CameraDebugController::E_CONTROL_DOWN_DPAD))
        UnPrimeGun();
}

void WorldDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    if (mbVehicleGunIsPrimed)
        DrawVehicleGun(lpDisplay);
    if (mbShowTarget)
        DrawTarget(lpDisplay);
}

// @0x827BFAA8.
void WorldDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (mbShowTarget == true)
        DrawInactiveMessage(lpDisplay);
    if (mbHaveDebugController == true)
        DrawActiveMessage(lpDisplay);
}

// @0x827B18C0.
void WorldDebugComponent::DrawVehicleGun(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const Vector3 lStep = mVehicleDirection * (mfVehicleVelocity / 100.0f);
    for (u32 luIndex = 0; luIndex <= 10; ++luIndex)
    {
        const Vector3 lCentre = mVehiclePosition + lStep * static_cast<f32>(luIndex);
        const f32 lfRadius = 0.1f - static_cast<f32>(luIndex) * 0.0080000004f;
        lpDisplay->DrawHollowSphere(lCentre, lfRadius, rw::RGBA(255, 255, 255, 255));
    }

    const Vector3 lEnd = mVehiclePosition + mVehicleDirection * (mfVehicleVelocity / 10.0f);
    lpDisplay->DrawLine(mVehiclePosition, lEnd, rw::RGBA(255, 255, 255, 255));
}

void WorldDebugComponent::DrawTarget(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const BrnDirector::Camera::Camera* lpCamera = mpWorldModule->GetLastCameraInput();
    lpDisplay->DrawHollowSphere(lpCamera->GetPosition() + lpCamera->GetDirection(),
                                0.01f, rw::RGBA(255, 255, 255, 255));
}

// @0x827B1AA8.
void WorldDebugComponent::DrawInactiveMessage(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    f32 lfY = KF_TEXT_POSITION_SCREEN_Y;
    for (u32 luLine = 0; luLine < 2; ++luLine)
    {
        lpDisplay->DrawText(KAC_INACTIVE_TEXT[luLine], KF_TEXT_POSITION_SCREEN_X,
                            lfY, KF_TEXT_HEIGHT, KU_WHITE);
        lfY += KF_TEXT_LINE_HEIGHT;
    }
}

// @0x827B1B30.
void WorldDebugComponent::DrawActiveMessage(CgsDev::Debug2DImmediateRender* lpDisplay) const
{
    CgsCore::SPrintf(KAC_ACTIVE_TEXT[3], 255, "Velocity: %.1f", mfVehicleVelocity);
    f32 lfY = 70.0f;
    for (u32 luLine = 0; luLine < 5; ++luLine)
    {
        lpDisplay->DrawText(KAC_ACTIVE_TEXT[luLine], KF_TEXT_POSITION_SCREEN_X,
                            lfY, KF_TEXT_HEIGHT, KU_WHITE);
        lfY += KF_TEXT_LINE_HEIGHT;
    }
}

void WorldDebugComponent::PrimeGun()
{
    mbVehicleGunIsPrimed = true;
    const BrnDirector::Camera::Camera* lpCamera = mpWorldModule->GetLastCameraInput();
    mVehiclePosition = lpCamera->GetPosition();
    mVehicleDirection = lpCamera->GetDirection();
}

void WorldDebugComponent::UnPrimeGun()
{
    mbVehicleGunIsPrimed = false;
    mpWorldModule->mRaceCarEntityModule.DEBUG_mbCleanDebugRaceCarThisFrame = true;
}

// @0x827B1BD0. ARTIST publishes the named race-car-module request but has no
// reader for it in the final image; preserve that shipped producer exactly.
void WorldDebugComponent::FireGun()
{
    if (!mbVehicleGunIsPrimed)
        return;

    RaceCarEntityModule& lrRaceCars = mpWorldModule->mRaceCarEntityModule;
    lrRaceCars.DEBUG_mbSpawnDebugRaceCarThisFrame = true;
    lrRaceCars.DEBUG_mDebugRaceCarPosition = mVehiclePosition;
    lrRaceCars.DEBUG_mDebugRaceCarDirection = mVehicleDirection;
    lrRaceCars.DEBUG_mfDebugRaceCarVelocity = mfVehicleVelocity * KF_MPH_TO_MPS;
}

void WorldDebugComponent::ClearStoredFile(void*)
{
    // Empty in ARTIST (folded with the shared empty-function body).
}

void WorldDebugComponent::UnPrimeGunCallback(void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis != nullptr)
        static_cast<WorldDebugComponent*>(lpThis)->UnPrimeGun();
}

void WorldDebugComponent::PrimeGunCallback(void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis != nullptr)
        static_cast<WorldDebugComponent*>(lpThis)->PrimeGun();
}

void WorldDebugComponent::FireGunCallback(void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis != nullptr)
        static_cast<WorldDebugComponent*>(lpThis)->FireGun();
}

void WorldDebugComponent::TriggerCollWorldValidate(void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis != nullptr)
    {
        WorldDebugComponent* lpComponent = static_cast<WorldDebugComponent*>(lpThis);
        lpComponent->mpWorldModule->mWorldEntityModule.mbDebugTriggerValidate = true;
    }
}

void WorldDebugComponent::TriggerCollWorldInvalidate(void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis != nullptr)
    {
        WorldDebugComponent* lpComponent = static_cast<WorldDebugComponent*>(lpThis);
        lpComponent->mpWorldModule->mWorldEntityModule.mbDebugTriggerInvalidate = true;
    }
}

// @0x827B1FC0.
void WorldDebugComponent::AIDrivesPlayerChanged(void*, void* lpThis)
{
    CGS_ASSERT(lpThis != nullptr, "Invalid data in the callback");
    if (lpThis == nullptr)
        return;

    WorldDebugComponent* lpComponent = static_cast<WorldDebugComponent*>(lpThis);
    WorldModule* lpWorldModule = lpComponent->mpWorldModule;
    const EActiveRaceCarIndex lePlayerARCIndex = lpWorldModule->meLocalPlayerActiveRaceCarIndex;
    CGS_ASSERT(lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    if (lePlayerARCIndex < E_ACTIVE_RACE_CAR_INDEX_0 ||
        lePlayerARCIndex >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
        return;

    if (lpComponent->mbAIDrivesPlayer)
    {
        lpWorldModule->maeCarControls[lePlayerARCIndex] = 2;
        lpWorldModule->mAIModule.mbAIPlayerInvulnerable = false;
    }
    else
    {
        lpWorldModule->maeCarControls[lePlayerARCIndex] = 1;
        lpWorldModule->mAIModule.mbAIPlayerInvulnerable = true;
    }
}
}
