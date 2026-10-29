#include "GameSource/Director/BrnReplayDirector.h"
#include "GameSource/Replays/Serialisers/BrnReplayDirectorSerialiser.h"
#include "GameSource/Director/DirectorModule/BrnDirectorInputOutput.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIO.h"
#include "GameSource/Director/DirectorModule/BrnDirectorModuleIOOutputBuffer.hpp"
#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"
#include "GameSource/AttribSys/Generated/classes/camerabumperbehaviour.h"
#include "GameSource/AttribSys/Generated/classes/cameraexternalbehaviour.h"

namespace BrnDirector
{
// ARTIST 827E2020: behaviour/policy vtables, ICETake constructors, the
// AllVehicleData/DebugLog Array sentinel constructors, and VehicleTracker's
// CarScoreData constructor are supplied by these real typed members.
ReplayDirector::ReplayDirector() = default;

// ARTIST 82250558. The alleged VMX random pipeline was a decompiler overlap:
// the assembly uses scalar mulld/std/stwx and exactly matches Random::Construct.
// VMX blocks are named matrix, controller and timestep value copies.
void ReplayDirector::FillInSharedInfo(Camera::BehaviourSharedInfo* lpInfo,
                                      Camera::CollisionPolicySharedInfo* lpPolicy,
                                      BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                                      const DirectorInputOutput* lpIO)
{
    const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
    const EActiveRaceCarIndex lePlayer = static_cast<EActiveRaceCarIndex>(lpInput->GetPlayerCarIndex());
    mRotationController.Construct();
    mSphericalRotationController.Construct();
    mSceneSpace = lpFrame->mCamera.mTransform;
    mTrafficLightSpace = lpFrame->mCamera.mTransform;
    mAllVehicleData.Construct();
    mAllVehicleData.Update(*lpInput->GetUsedRaceCars(), lpInput->GetRaceCarInfo(), lePlayer,
        &lpInput->GetTrafficOutputInterface()->GetTrafficDirectorEntityArray(), lpInput->GetVehicleInfoArray());
    mGameState.Clear();
    mVehicleTracker.Update(&mGameState, lpInput, lePlayer, false);
    mRandom.Construct();
    mDebugLog.Construct();
    mDebugPrinter.Construct();

    const CgsSystem::TimerStatusInterface* lpTimers = lpInput->GetTimerStatusInterface();
    CGS_ASSERT(lpTimers->GetGameTimerStatus()->IsRunning(),
               "lDirectorSharedInfo.mpInput->GetTimerStatusInterface()->GetGameTimerStatus()->IsRunning()");
    const f32 lfGame = lpTimers->GetGameTimerStatus()->GetCurrentTimeStep();
    const f32 lfWorld = lpInput->IsSimPaused() ? 0.0f : lpTimers->GetSimTimerStatus()->GetCurrentTimeStep();
    const f32 lfWorldNoSlomo = lpInput->IsSimPaused() ? 0.0f : lpTimers->GetSimTimerStatus()->GetBaseTimeStep();
    Timestep lTimestep;
    lTimestep.Set(VecFloat(lfGame), VecFloat(lfWorld), VecFloat(lfWorldNoSlomo),
                  lfGame, lfWorld, lfWorldNoSlomo);

    // ARTIST performs both rank-one queries. Preserve their order and the
    // nearest-car assertion path even though the first returned reference is unused.
    const EActiveRaceCarIndex leNearest = mAllVehicleData.GetNearestRaceCarIndexToPlayer(1);
    mAllVehicleData.GetRaceCar(leNearest);
    const Matrix44Affine lLooseHeading = mAllVehicleData.GetPlayerLooseHeadingSpace();
    const Matrix44Affine lHeading = mAllVehicleData.GetPlayerHeadingSpace();
    const Matrix44Affine lImpact = mAllVehicleData.GetPlayerImpactSpace();
    const Camera::VehicleInfo& lrNearest = mAllVehicleData.GetRaceCar(
        mAllVehicleData.GetNearestRaceCarIndexToPlayer(1));
    mCameraSpaceHandler.Construct(mAllVehicleData.GetPlayer().mRaceCarState.mTransform,
        lrNearest.mRaceCarState.mTransform, mTrafficLightSpace, mSceneSpace,
        lImpact, lHeading, lLooseHeading, lrNearest.mRaceCarState.mTransform, 0);

    const ControllerInfo& lrControl = *lpInput->GetControll();
    lpInfo->mpEffectInterface = 0;
    lpInfo->mpDebugLog = &mDebugLog;
    lpInfo->mfCrashTimeRemaining = 3.0f;
    lpInfo->mpDirectorResourceManager = lpIO->mpResourceManager;
    lpInfo->mpDebugPrinter = &mDebugPrinter;
    lpInfo->mUsedRaceCars = *lpInput->GetUsedRaceCars();
    lpInfo->mePlayerCarIndex = lePlayer;
    lpInfo->mpAllVehicleData = &mAllVehicleData;
    lpInfo->mpRaceCars = lpInput->GetRaceCarInfo();
    lpInfo->mPlayerInfo = lpInfo->mpRaceCars[lePlayer];
    lpInfo->mTimestep = lTimestep;
    lpInfo->mpWorldMap = lpIO->mpWorldMap;
    lpInfo->mpRandom = &mRandom;
    lpInfo->mpPlayerTracker = &mVehicleTracker;
    lpInfo->mpCameraSpaceHandler = &mCameraSpaceHandler;
    lpInfo->mpSceneQueryInterface = lpIO->mpSceneQueryInterface;
    lpInfo->mRotationController = mRotationController;
    lpInfo->mSphericalRotationController = mSphericalRotationController;
    lpInfo->mbIceDataBeingEdited = false;
    lpInfo->mbAllStreamed = lpInput->IsAllStreamed();
    lpInfo->mCameraModifier = lrControl.mCameraModifier;
    lpInfo->mbUseControlPauseBehaviour = true;
    lpInfo->mbLookback = lrControl.mbLookback;
    lpInfo->mpBehaviourManager = 0;
    // VMX vperm mask82CDA350: {-x,y,-x,-x}.
    lpInfo->mCarModifier = {-lrControl.mCarModifier.x, lrControl.mCarModifier.y,
                           -lrControl.mCarModifier.x, -lrControl.mCarModifier.x};
    lpInfo->mRotationController = mRotationController;
    lpInfo->mSphericalRotationController = mSphericalRotationController;
    lpInfo->mfTempFOVBoostAmount = lpFrame->mTempCameraBoostResponder.GetFOVBoostAmount();
    lpInfo->mfSpeedRatio = lpFrame->mSpeedResponder.GetSpeedRatio();
    lpInfo->muRenderMetricsActivationID = 0;
    lpInfo->mfLastHandbrakeTime = lpFrame->mfLastHandbrakeTime;

    lpPolicy->mpRequestInterface = lpIO->mpSceneQueryInterface;
    lpPolicy->mUsedRaceCars = *lpInput->GetUsedRaceCars();
    lpPolicy->mpAllVehicleData = &mAllVehicleData;
    lpPolicy->mePlayerCarIndex = lePlayer;
    lpPolicy->mpRaceCars = lpInput->GetRaceCarInfo();
    lpPolicy->mpPlayerCar = &lpPolicy->mpRaceCars[lePlayer];
    lpPolicy->mpPlayerCarTransform = &lpPolicy->mpPlayerCar->mRaceCarState.mTransform;
    lpPolicy->mpRandom = &mRandom;
    lpPolicy->mpDebugPrinter = &mDebugPrinter;
    lpPolicy->mTimestep = lTimestep;
}

// ARTIST 8225BD28. A first replay frame rebuilds both gameplay cameras from
// the recorded car's camera attributes before issuing either camera's queries.
void ReplayDirector::PreSceneQueryUpdate(BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                                         const DirectorInputOutput* lpIO)
{
    const DirectorIO::InputBuffer* lpInput = lpIO->mpInputBuffer;
    const s32 liPlayer = lpInput->GetPlayerCarIndex();
    if (liPlayer == -1)
        return;
    if (!lpInput->GetUsedRaceCars()->IsBitSet(static_cast<u32>(liPlayer)))
        return;

    if (mbFirstFrame)
    {
        CGS_ASSERT(lpFrame->mCarBaseAssetAttribsKey != 0,
                   "lpDirectorSerialiserFrame->mCarBaseAssetAttribsKey!=0");
        Attrib::Gen::burnoutcarasset lCar(lpFrame->mCarBaseAssetAttribsKey, 0);
        CGS_ASSERT(lCar.IsValid(), "Invalid car asset, key:");
        Attrib::Gen::camerabumperbehaviour lBumper(
            const_cast<Attrib::Collection*>(lCar.GetBumperCamRefSpec()->GetCollection()), 0);
        Attrib::Gen::cameraexternalbehaviour lExternal(
            const_cast<Attrib::Collection*>(lCar.GetExternalCamRefSpec()->GetCollection()), 0);
        (void)lCar.GetAssetName();
        CGS_ASSERT(lBumper.IsValid(), "Invalid bumpercam asset, key:");
        CGS_ASSERT(lBumper.GetBoostFOV() > 0.0f, "Invalid bumpercam Boost FOV, key:");
        CGS_ASSERT(lExternal.IsValid(), "Invalid externalcam asset, key:");
        CGS_ASSERT(lExternal.GetBoostFOV() > 0.0f, "Invalid externalcam Boost FOV, key:");
        Camera::BehaviourGameplayExternal::Parameters::Source lExternalSource;
        lExternalSource.mpfValues = static_cast<const f32*>(lExternal.GetLayoutPointer());
        mExternalParameters.Set(&lExternalSource);
        Camera::BehaviourGameplayBumper::Parameters::Source lBumperSource;
        lBumperSource.mpfValues = static_cast<const f32*>(lBumper.GetLayoutPointer());
        mBumperParameters.Set(&lBumperSource);
        Camera::BehaviourSharedPrepareReleaseInfo lPrepare;
        lPrepare.mpInterpolateLockInterface = 0;
        lPrepare.mpDirectorResourceManager = lpIO->mpResourceManager;
        mExternalCamera.Construct();
        mExternalBehaviour.Construct();
        mExternalBehaviour.SetParameters(&mExternalParameters);
        mExternalBehaviour.Prepare(lPrepare);
        mExternalBehaviour.SetReplayCamera(true);
        mBumperCamera.Construct();
        mBumperBehaviour.Construct();
        mBumperBehaviour.SetParameters(&mBumperParameters);
        mBumperBehaviour.Prepare(lPrepare);
        mGameState.Clear();
        mVehicleTracker.Construct();
        mVehicleTracker.SetVehicleIndex(liPlayer);
        mVehicleTracker.Update(&mGameState, lpInput, static_cast<EActiveRaceCarIndex>(liPlayer), false);
    }
    Camera::BehaviourSharedInfo lInfo;
    Camera::CollisionPolicySharedInfo lPolicyInfo;
    FillInSharedInfo(&lInfo, &lPolicyInfo, lpFrame, lpIO);
    mExternalBehaviour.Update(mExternalCamera, lInfo);
    Camera::CollisionPolicy* lpPolicy = mExternalBehaviour.GetCollisionPolicy();
    if (lpPolicy)
        lpPolicy->GenerateSceneQueries(lPolicyInfo, mExternalCamera);
    mBumperBehaviour.Update(mBumperCamera, lInfo);
    lpPolicy = mBumperBehaviour.GetCollisionPolicy();
    if (lpPolicy)
        lpPolicy->GenerateSceneQueries(lPolicyInfo, mBumperCamera);
}

// ARTIST 8225C298. Collision processing precedes each behaviour's post-query
// pass. Camera cycling takes effect on the following frame, after selection.
void ReplayDirector::Update(BrnReplays::DirectorSerialiserStaticLayout* lpFrame,
                            const DirectorInputOutput* lpIO)
{
    const s32 liPlayer = lpIO->mpInputBuffer->GetPlayerCarIndex();
    if (liPlayer == -1)
        return;
    if (!lpIO->mpInputBuffer->GetUsedRaceCars()->IsBitSet(static_cast<u32>(liPlayer)))
        return;
    Camera::BehaviourSharedInfo lInfo;
    Camera::CollisionPolicySharedInfo lPolicyInfo;
    FillInSharedInfo(&lInfo, &lPolicyInfo, lpFrame, lpIO);
    Camera::CollisionPolicy* lpPolicy = mExternalBehaviour.GetCollisionPolicy();
    if (lpPolicy)
        lpPolicy->ProcessSceneQueryResults(lPolicyInfo, mExternalCamera);
    mExternalBehaviour.PostCollisionUpdate(mExternalCamera, lInfo);
    lpPolicy = mBumperBehaviour.GetCollisionPolicy();
    if (lpPolicy)
        lpPolicy->ProcessSceneQueryResults(lPolicyInfo, mBumperCamera);
    mBumperBehaviour.PostCollisionUpdate(mBumperCamera, lInfo);

    Camera::Camera lSelected;
    lSelected.Construct();
    if (!lpFrame->mCamera.GetState().IsFlagSet(3) || lpFrame->mCamera.GetState().IsFlagSet(10))
        lSelected = lpFrame->mCamera;
    else
    {
        lSelected = miCameraSelection ? mBumperCamera : mExternalCamera;
        if (lpIO->mpInputBuffer->GetControll()->mbCycleCameras)
            miCameraSelection = (miCameraSelection + 1) % 2;
    }
    if (mbFirstFrame)
    {
        lSelected.mEffects.mbHasStartHookNameString = false;
        lSelected.mEffects.mbHasStopHookNameString = false;
        lSelected.mEffects.muRequestedPostFxId = 0x7BEC6;
    }
    lSelected.mState.CopyFlagsToPrevious(mFinalCamera.mState);
    mFinalCamera = lSelected;
    lSelected.CopyToCgsCamera(&mCgsCamera);
    lpIO->mpOutputBuffer->SetCameraOutput(lSelected);
    lpIO->mpOutputBuffer->SetCgsCamera(mCgsCamera);
    mbFirstFrame = false;
}
}
