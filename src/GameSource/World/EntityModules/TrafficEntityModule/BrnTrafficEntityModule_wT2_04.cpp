// ============================================================================
// BrnTrafficEntityModule_wT2_04.cpp -- the driving-traffic vehicle/scene wire.
//
//   TrafficEntityModule::UpdateVehicles_CreateNewVehicles @0x8273A308  PARTIAL
//   TrafficEntityModule::UpdateVehicles                   @0x82744F58  PARTIAL
//   TrafficEntityModule::CacheRaceCarState                @0x827185D0  (EXPORT HOLE)
//   TrafficEntityModule::GenerateSceneUpdateEvents        @0x8273B568
//   TrafficEntityModule::UpdateLerpedParamTransforms      @0x82739CD8  (EXPORT HOLE)
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficRaceCarCache.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"
#include "SharedClasses/Traffic/BrnTrafficHull.h"
#include "SharedClasses/Traffic/BrnTrafficSection.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h"
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"
#include "GameSource/Jobs/Traffic/TrafficCommon.h"          // UpdateVehiclesJobParams
#include "GameSource/Jobs/Traffic/BrnUpdateVehiclesJob.h"   // UpdateVehiclesJob
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h"

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"

#include "rw/math/vpu/matrix44affine_operation.h"
#include "rw/math/vpu/vector3_operation.h"
#include "rw/math/vpu/vector4_operation.h"

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE + diag plumbing, file-local by convention, same shape as the sibling
    // partfiles'. [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
    inline void LogMissingLeg(bool& lrbAlreadyLogged, const char* lpcLegNameAndReason)
    {
        if (lrbAlreadyLogged)
        {
            return;
        }
        lrbAlreadyLogged = true;

        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[T2-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    bool TrafficDiagEnabled()
    {
        static const bool sbEnabled = (getenv("BRN_TRAFFIC_DIAG") != 0);
        return sbEnabled;
    }

    CgsDev::Log::DebugPrint* TrafficDiagStream()
    {
        if (!TrafficDiagEnabled() || CgsDev::Log::gpDebugPrint == 0)
        {
            return 0;
        }
        return CgsDev::Log::gpDebugPrint;
    }

    // HOST SEAT for [MEMBER HOLE 5] TrafficJobStub maJobs[4] (blocker measured in
    // BrnTrafficEntityModule.h). FLAG PC-platform leaf: single-threaded job dispatch. The
    // console's stub only snapshots the params and submits; TrafficJobStub::Execute
    // @0x82752CB0 already runs the worker inline on this host, so the split runs it here.
    // File scope, like the console's own gaTrafficJobs table -- no module member invented.
    // DELETE-WHEN BrnTrafficJob.h stops pulling eajobs/job_scheduler.h.
    }  // anonymous namespace -- the two host tables need external linkage (SendPhysicalRequests
       // in _wT2_01 drains gaHostNewPhysicalRequests).
    UpdateVehiclesJob       gaHostUpdateVehiclesJobs[KU_MAX_JOBS];
    PhysicalRequestInfoList gaHostNewPhysicalRequests[KU_MAX_JOBS];
    namespace
    {

    // Stands in for Construct's 4x TrafficJobStub::Construct @0x827407B8 (which only
    // Constructs the request list and clears mbRunningJob).
    void EnsureHostJobsConstructed()
    {
        static bool sbDone = false;
        if (sbDone)
        {
            return;
        }
        sbDone = true;
        for (u32 luJob = 0; luJob < KU_MAX_JOBS; ++luJob)
        {
            gaHostNewPhysicalRequests[luJob].Construct();
        }
    }

    // Feb-2007 KF_MAX_DIST_ACROSS_LANE_lhs, folded by the ship into `ring * 1.4f -
    // flt_820BA4D0(0.7f)` at 0x8273ABB0. Deliberately NOT the module member
    // KF_MAX_DIST_ACROSS_LANE (DWARF :802): the spawn path uses the literal.
    const f32 KF_SPAWN_DIST_ACROSS_LANE = 0.7f;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateVehicles_CreateNewVehicles  @ 0x8273A308  PARTIAL  (.cpp 13477)
//
// Every param that is alive, not a zombie, and has no live vehicle
// becomes a standard Vehicle. The candidate set is three FastBitArray terms, not a linear
// scan (0x8273A468..0x8273A52C): mAliveParams & ~mZombieParams & ~mAliveVehicles.
//
// The two RNG draws come off mEffectRand in this order and both must stay: draw 1 is the
// across-lane offset, draw 2 is mfRandomVal. Swapping them re-phases every car's lights.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateVehicles_CreateNewVehicles(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    (void)lpInput;   // only read by the GATED race-car proximity arm below

    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");   // baked .cpp 13477

    if (mbWaitingForStreaming)
    {
        return;
    }

    const bool lbRejectNearPlayers =
        mbDontCreateVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour;

    if (lbRejectNearPlayers)
    {
        // GATE: the race-car proximity reject @0x8273A3FC. BLOCKERS: the +0x220 state lane sits
        // behind an IDA-truncated getter, and Param::SetDivorced is undeclared. Online-only.
        // DELETE-WHEN online traffic lands.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateVehicles_CreateNewVehicles race-car proximity rejection "
            "(mbDontCreateVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour) -- ONLINE-ONLY "
            "and unreachable offline. Blockers: the getter behind the +0x220 race-car state "
            "lane is IDA-truncated, and Param::SetDivorced is undeclared. Radius recovered: "
            "unk_8300CF70 lane 0 == 6400.0f == 80m^2");
    }

    // mAliveParams & ~mZombieParams & ~mAliveVehicles, field for field, in the console's order.
    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lNotZombie;
    lNotZombie.SetInverse(mParamSoaData.mZombieParams);

    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lParamsToCreate;
    lParamsToCreate.SetAnd(mParamSoaData.mAliveParams, lNotZombie);

    CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> lNoVehicle;
    lNoVehicle.SetInverse(mVehicleSoaData.mAliveVehicles);
    lParamsToCreate.SetAnd(lNoVehicle, lParamsToCreate);

    u32 luCreated = 0;

    for (CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator lItParam =
             lParamsToCreate.Begin();
         lItParam != lParamsToCreate.End();
         ++lItParam)
    {
        const u32 luParam = static_cast<u32>(lItParam.GetIndex());

        const Param* lpParam = GetParam(luParam);          // carries header-2350's bound
        CGS_ASSERT(lpParam->IsAlive(), "lpParam->IsAlive()");        // .cpp 13526
        CGS_ASSERT(!lpParam->IsZombie(), "!lpParam->IsZombie()");    // .cpp 13527

        const ParamTransform* lpParamTransform = GetParamTransform(luParam);
        const Vector3 lParamPos       = lpParamTransform->GetLerpedPos();
        const Vector3 lParamDirection = lpParamTransform->GetDirection();

        if (lbRejectNearPlayers)
        {
            // Second half of the same gate: without the reject the console would still fall
            // through to the maker, so skipping the whole param is what keeps the two halves
            // consistent.
            continue;
        }

        Vehicle* lpVehicle = GetVehicle(luParam);          // carries header-2459's bound
        CGS_ASSERT(!lpVehicle->IsAlive(), "!lpVehicle->IsAlive()");   // .cpp 13559

        // The console keeps only the section's bounds assert; nothing reads the Section.
        const Hull* lpHull = GetHull(lpParam->muHullIndex);
        const Section* lpSection = lpHull->GetSection(lpParam->muSectionIndex);
        (void)lpSection;

        const u8 luVehicleType = lpParam->muVehicleType;
        const VehicleTypeData*       lpVehicleType       = &mpData->mpaVehicleTypes[luVehicleType];
        const VehicleTypeUpdateData* lpVehicleTypeUpdate = &mpData->mpaVehicleTypesUpdate[luVehicleType];
        const VehicleTypeRuntime*    lpVehicleTypeRuntime = GetVehicleTypeRuntime(luVehicleType);
        const VehicleTraits*         lpVehicleTraits =
            mpData->GetVehicleTraitsForVehicleType(luVehicleType);

        const f32 lfDistAcrossLane =
            mEffectRand.RandomFloat(-KF_SPAWN_DIST_ACROSS_LANE, KF_SPAWN_DIST_ACROSS_LANE);

        // The cab test is the console's, byte-for-byte: `mxVehicleFlags != 0` (not a mask test
        // -- Feb-2007's `&&` typo survived to ship) AND a valid trailer flow type.
        u16 luTrailerIndex = static_cast<u16>(KU_INVALID_VEHICLE);
        if (lpVehicleType->mxVehicleFlags != 0
            && lpVehicleType->muTrailerFlowTypeId != 0xFFFFu)
        {
            // GATE: the trailer half @0x8273ACA4. BLOCKERS: TryAllocateTrailerId is undeclared
            // and InitialiseAsTrailer reaches the trapped Vehicle::CalcTowBarPos.
            // DELETE-WHEN articulated traffic lands.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "UpdateVehicles_CreateNewVehicles trailer half (TryAllocateTrailerId / "
                "Vehicle::InitialiseAsTrailer / the muOtherHalfIndex cross-link) -- "
                "TryAllocateTrailerId is undeclared and InitialiseAsTrailer reaches the "
                "trapped Vehicle::CalcTowBarPos. luTrailerIndex stays KU_INVALID_VEHICLE, "
                "which is the console's own no-trailer-available path");
        }

        const f32 lfRandomVal = mEffectRand.RandomFloat();

        Matrix44Affine lOutMatrix;
        lpVehicle->InitialiseAsStandard(&maVehicleAxles[luParam],
                                        lOutMatrix,
                                        lpParam,
                                        lfRandomVal,
                                        mpData->mpapHulls,
                                        luVehicleType,
                                        lpVehicleTypeRuntime,
                                        lpVehicleTypeUpdate,
                                        lpVehicleTraits,
                                        lfDistAcrossLane,
                                        lpParam->mfSpeed,
                                        lParamPos,
                                        lParamDirection,
                                        luParam,
                                        mVehicleSoaData,
                                        luTrailerIndex);

        SetVehicleTransform(luParam, lOutMatrix);
        ++luCreated;

        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T2-make] one-shot on the first driving car. DELETE-WHEN-STABLE.
            static bool sbFirst = true;
            if (sbFirst)
            {
                sbFirst = false;
                const Vector3& lrPos = lOutMatrix.wAxis;
                *lpDiag << "[T2-make] FIRST InitialiseAsStandard param=" << static_cast<s32>(luParam)
                        << " type=" << static_cast<s32>(luVehicleType)
                        << " speed=" << lpParam->mfSpeed
                        << " pos=(" << lrPos.x << ", " << lrPos.y << ", " << lrPos.z << ")\n";
            }
        }
    }

    mbDontCreateVehiclesNearAnyPlayers = false;

    if (luCreated != 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T2-make] value-latched: only on a frame that actually made cars.
            // DELETE-WHEN-STABLE.
            *lpDiag << "[T2-make] created=" << static_cast<s32>(luCreated) << " driving vehicle(s)\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CacheRaceCarState  @ 0x827185D0   (EXPORT HOLE)
//
// One snapshot per UpdateVehicles of the race cars the traffic sim reacts to. Feb-2007 built
// the four Append lists inline (BrnTrafficEntityModule.cpp:6976..:7025); the ship hoisted
// them into mRaceCarState and added the two per-index tables.
//
// FLAG: the four lists are the leak verbatim. The fill of mabRaceCarActive /
// maActiveRaceCarPositions is REASONED from their DWARF names, since the function has no
// per-function export. DELETE-WHEN the export hole is filled.
// ----------------------------------------------------------------------------
void TrafficEntityModule::CacheRaceCarState(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCars)
{
    CGS_ASSERT(lpRaceCars != 0, "lpActiveRaceCarOutputInterface");

    mRaceCarState.mRaceCarPositions.Construct();
    mRaceCarState.mRaceCarLinearVelocities.Construct();
    mRaceCarState.mRaceCarSpeeds.Construct();
    mRaceCarState.mRaceCarXZVelocityDirs.Construct();

    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(liIndex);

        const bool lbActive = lpRaceCars->IsRaceCarActive(leIndex);
        mRaceCarState.mabRaceCarActive[liIndex] = lbActive;

        if (!lbActive)
        {
            continue;
        }

        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpRaceCars->GetRaceCarState(leIndex);
        mRaceCarState.maActiveRaceCarPositions[liIndex] = lpRaceCarState->mTransform.Pos();

        if (lpRaceCars->IsRaceCarRival(leIndex))
        {
            continue;
        }

        mRaceCarState.mRaceCarPositions.Append(lpRaceCarState->mTransform.Pos());
        mRaceCarState.mRaceCarLinearVelocities.Append(lpRaceCarState->mLinearVelocity);

        Vector3 lXZVelocity = lpRaceCarState->mLinearVelocity;
        lXZVelocity.y = 0.0f;

        if (!rw::math::vpu::IsZero(lXZVelocity))
        {
            Vector3 lVelocityDir;
            const f32 lfSpeed =
                rw::math::vpu::NormalizeReturnMagnitude(lXZVelocity, lVelocityDir);
            mRaceCarState.mRaceCarXZVelocityDirs.Append(lVelocityDir);
            mRaceCarState.mRaceCarSpeeds.Append(rw::math::vpu::Splat(lfSpeed));
        }
        else
        {
            // The stalled-car fallback keeps the direction list index-parallel: fall back on
            // the car's own At axis, and on world Z if that is flat too.
            Vector3 lFacing = lpRaceCarState->mTransform.At();
            lFacing.y = 0.0f;

            if (!rw::math::vpu::IsZero(lFacing))
            {
                mRaceCarState.mRaceCarXZVelocityDirs.Append(lpRaceCarState->mTransform.At());
            }
            else
            {
                mRaceCarState.mRaceCarXZVelocityDirs.Append(rw::math::vpu::GetVector3_ZAxis());
            }

            mRaceCarState.mRaceCarSpeeds.Append(rw::math::vpu::Splat(0.0f));
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateVehicles  @ 0x82744F58   PARTIAL   (.cpp 7194)
//
// The job splitter. It creates this decision frame's vehicles, snapshots the race cars, then
// hands [0, KU_MAX_PARAMS) to muNumUpdateVehiclesJobs workers in equal slices (the last slice
// always ends at 400). The stubs are the host table above while [MEMBER HOLE 5] is open.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateVehicles(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput)
{
    (void)lpOutput;   // the console reaches the physical-request list through the job stubs

    {
        // GATE: the PerfMonCpu bracket (miPerfMon_UpdateVehicle) and the
        // DebugRenderStreamReader bracket over unk_8300CD00. BLOCKER: neither has a home in
        // this tree, same disposition as every sibling partfile's. DELETE-WHEN either lands.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateVehicles PerfMonCpu Start/StopMonitor bracket and the "
            "DebugRenderStreamReader Begin/End bracket over unk_8300CD00 -- the monitor "
            "handles are never issued and the debug render stream has no home in this tree");
    }

    if (IsDecisionFrame())
    {
        UpdateVehicles_CreateNewVehicles(lpInput);
    }

    CacheRaceCarState(lpInput->GetActiveRaceCarOutputInterface());

    // The job split, 0x82745000..0x827451E4. Every argument below is the console's, read off
    // the Construct call site: r8/r9/r10 are maParams/maParamTransforms/maVehicles; the six
    // stack slots are maVehicleTransforms, maVehicleAxles, maVehicleTypeRuntime,
    // &mRaceCarState, &mEffectRand and meLocalPlayerIndex; the three bytes are
    // mbHardcoreSwerveForMode (+0x717DF), mbGameModeAllowsSwerving (+0x717DE) and
    // mbDEBUGStopTrafficMoving (+0x727B8); f1/f2/f3 are mfSimTimeStep, mfSimTimeSinceLastDecision
    // and mfCrashSliderFinalValue; and v1 is the +0x728C0 lane == mCameraLastFrame's Pos row.
    EnsureHostJobsConstructed();

    // Console `twllei r11, 0` -- the divide traps on a zero job count.
    CGS_ASSERT(muNumUpdateVehiclesJobs != 0, "muNumUpdateVehiclesJobs != 0");

    const u32 luParamsPerJob = (muNumUpdateVehiclesJobs != 0)
                                   ? (KU_MAX_PARAMS / muNumUpdateVehiclesJobs)
                                   : KU_MAX_PARAMS;
    u32 luBeginParam = 0;

    for (u32 luJob = 0; luJob < muNumUpdateVehiclesJobs; ++luJob)
    {
        // The last slice always ends at KU_MAX_PARAMS, whatever the division left over.
        const u32 luEndParam = (luJob == muNumUpdateVehiclesJobs - 1)
                                   ? KU_MAX_PARAMS
                                   : (luBeginParam + luParamsPerJob);

        UpdateVehiclesJobParams lJobParams;
        lJobParams.Construct(
            luBeginParam,
            luEndParam,
            mpData->mpapHulls,
            mpData->muNumHulls,
            maParams,
            maParamTransforms,
            maVehicles,
            maVehicleTransforms,
            maVehicleAxles,
            maVehicleTypeRuntime,
            &mRaceCarState,
            mfSimTimeStep,
            mfSimTimeSinceLastDecision,
            &mEffectRand,
            meLocalPlayerIndex,
            mbHardcoreSwerveForMode,
            mbGameModeAllowsSwerving,
            mbDEBUGStopTrafficMoving,
            mCameraLastFrame.GetPosition(),
            mfCrashSliderFinalValue,
            0);   // lpDebugStream: the console passes &unk_8300CD00, which has no home here

        // SetOutputs, inlined exactly as TrafficJobStub::Execute @0x82752CB0 does it.
        lJobParams.mpOutNewPhysicalRequests = &gaHostNewPhysicalRequests[luJob];

        gaHostUpdateVehiclesJobs[luJob].Execute(&lJobParams);

        // 0x82745178..0x827451A4: one bare LCG step on mEffectRand per job (ld/mulld/addi 1/std
        // at +0x20, no ring touch) -- CgsNumeric::Random::RandomBool's step, result unused.
        (void)mEffectRand.RandomBool();

        luBeginParam = luEndParam;
    }

    // The console's second loop is 4x TrafficJobStub::WaitOn @0x827451CC..0x827451E4. Under the
    // synchronous dispatch above every slice has already completed, so the join is a no-op.

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T2-job] one-shot. DELETE-WHEN-STABLE.
        static bool sbFirstSplit = true;
        if (sbFirstSplit)
        {
            sbFirstSplit = false;
            *lpDiag << "[T2-job] FIRST UpdateVehicles split jobs="
                    << static_cast<s32>(muNumUpdateVehiclesJobs)
                    << " paramsPerJob=" << static_cast<s32>(luParamsPerJob)
                    << " dispatch=sync\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateLerpedParamTransforms  @ 0x82739CD8   (EXPORT HOLE)
//
// The between-decision-frames interpolator: one ParamTransform::UpdateLerpedPosition
// @0x82712968 per live param, stepped by mfSimTimeStepVec.
//
// FLAG: the loop SHAPE is reasoned, not attested -- the function has no per-function JSON and
// no Feb-2007 counterpart, and its only caller (UpdateNonDecisionFrame @0x8274C21C) passes
// nothing but `this`. mAliveParams is the module's own liveness set and the only one a
// per-param sweep could use. DELETE-WHEN the export hole is filled.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateLerpedParamTransforms()
{
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        if (!mParamSoaData.mAliveParams.IsBitSet(luParam))
        {
            continue;
        }
        GetParamTransform(luParam)->UpdateLerpedPosition(mfSimTimeStepVec);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::GenerateSceneUpdateEvents  @ 0x8273B568   (.cpp 14131)
//
// THE PER-FRAME SCENE MOVER. Without it a traffic car's scene entity stays wherever
// CreateNewVehicleEntities' AddEntity put it and the frustum filter culls it against the
// wrong bounds.
//
// The console splits Feb-2007's single loop into two bit-set walks over
// (mVehiclesWithEntities & mAliveVehicles), and the split is behavioural, not cosmetic:
// the NON-collidable arm publishes the RAW transform position, while the collidable arm
// publishes the bbox-offset position and, when not frozen, the whole bbox-offset transform.
// ----------------------------------------------------------------------------
void TrafficEntityModule::GenerateSceneUpdateEvents(BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // baked .cpp 14131

    typedef BrnTrafficIO::OutputBuffer_PostPhysics::SceneInputInterface SceneInputInterface;
    SceneInputInterface* lpSceneInputInterface = lpOutput->GetSceneInputInterface();
    CGS_ASSERT(lpSceneInputInterface != 0, "lpSceneInputInterface");   // baked .cpp 14139

    typedef CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES> TrafficBitArray;

    TrafficBitArray lVehiclesWithEntities;
    lVehiclesWithEntities.SetAnd(mVehicleSoaData.mVehiclesWithEntities,
                                 mVehicleSoaData.mAliveVehicles);

    TrafficBitArray lCollidableWithEntities;
    lCollidableWithEntities.SetAnd(mVehicleSoaData.mCollidableVehicles, lVehiclesWithEntities);

    TrafficBitArray lNotCollidable;
    lNotCollidable.SetInverse(mVehicleSoaData.mCollidableVehicles);
    lVehiclesWithEntities.SetAnd(lNotCollidable, lVehiclesWithEntities);

    u32 luMoved = 0;

    for (TrafficBitArray::Iterator lIt = lVehiclesWithEntities.Begin();
         lIt != lVehiclesWithEntities.End();
         ++lIt)
    {
        const u32 luVehicle = static_cast<u32>(lIt.GetIndex());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");

        lpSceneInputInterface->SetEntityPosition(
            CgsSceneManager::EntityId(MakeTrafficEntityId(luVehicle).muValue),
            GetVehicleTransform(luVehicle).Pos());
        ++luMoved;
    }

    for (TrafficBitArray::Iterator lIt = lCollidableWithEntities.Begin();
         lIt != lCollidableWithEntities.End();
         ++lIt)
    {
        const u32 luVehicle = static_cast<u32>(lIt.GetIndex());

        const Vehicle* lpVehicle = GetVehicle(luVehicle);
        CGS_ASSERT(lpVehicle->HasEntity(), "lpVehicle->HasEntity()");   // baked .cpp 14180
        CGS_ASSERT(lpVehicle->IsAlive(), "IsAlive()");                  // BrnTrafficVehicle.h:786

        const VehicleTypeRuntime* lpVehicleTypeRuntime =
            GetVehicleTypeRuntime(lpVehicle->GetVehicleType());
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");

        // Translate(mBBoxOffset) * transform. Only the position row changes: the translate
        // matrix's rotation is identity, so the product's rows are the transform's own and its
        // position is TransformPoint(transform, mBBoxOffset) -- the three fmas at
        // 0x8273C0D8..0x8273C118, in the same order.
        Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
        lTransform.wAxis = rw::math::vpu::TransformPoint(
            lTransform, lpVehicleTypeRuntime->GetBBoxOffset());

        const EntityId lTrafficEntityId = MakeTrafficEntityId(luVehicle);
        lpSceneInputInterface->SetEntityPosition(
            CgsSceneManager::EntityId(lTrafficEntityId.muValue), lTransform.Pos());

        if (!lpVehicle->IsFrozen())
        {
            // The volume-instance id is the entity id in the high doubleword (`sldi r4, r11, 32`
            // at 0x8273C1BC), which is what SetEntityIDOwner/SetEntityIDEntityIndex build.
            CgsSceneManager::VolumeInstanceId lVolumeInstanceId;
            lVolumeInstanceId.muId = static_cast<u64>(lTrafficEntityId.muValue) << 32;
            lpSceneInputInterface->SetVolumeInstanceTransform(lVolumeInstanceId, lTransform);
        }
        ++luMoved;
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // ------------------------------------------------------------------------------
        // [T4-clip] THE DECISIVE NEGATIVE WITNESS on the world side. [DIAG] NOT IN THE X360
        // BINARY. Each frame, test the local player's position against every alive vehicle's
        // OBB (the type runtime's bbox, offset and expanded 0.5 m) and shout once per vehicle.
        //
        // The two strings MUST stay different:
        //   "NON-physical" -- the player is INSIDE a traffic car that never got a physics
        //                     slot. That is the wave-4 break: gate 1 or gate 2 did not fire.
        //   "PHYSICAL"     -- promotion worked and the CONTACTS failed. A completely different
        //                     bug, on the physics side, and chasing the module for it wastes a
        //                     round.
        //
        // Seated here because this function already walks exactly this bit set with the
        // transform and the type runtime in hand. DELETE-WHEN-STABLE.
        // ------------------------------------------------------------------------------
        if (meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
            mRaceCarState.mabRaceCarActive[meLocalPlayerIndex])
        {
            const Vector3 lPlayerPos = mRaceCarState.maActiveRaceCarPositions[meLocalPlayerIndex];

            static bool sabReported[KU_MAX_TOTAL_TRAFFIC] = { false };

            TrafficBitArray lAlivePresent;
            lAlivePresent.SetAnd(mVehicleSoaData.mVehiclesWithEntities,
                                 mVehicleSoaData.mAliveVehicles);

            for (TrafficBitArray::Iterator lIt = lAlivePresent.Begin();
                 lIt != lAlivePresent.End();
                 ++lIt)
            {
                const u32 luVehicle = static_cast<u32>(lIt.GetIndex());
                if (luVehicle >= KU_MAX_TOTAL_TRAFFIC || sabReported[luVehicle])
                {
                    continue;
                }

                const Vehicle* lpVehicle = GetVehicle(luVehicle);
                const VehicleTypeRuntime* lpVehicleTypeRuntime =
                    GetVehicleTypeRuntime(lpVehicle->GetVehicleType());

                const Matrix44Affine lTransform = GetVehicleTransform(luVehicle);
                const Vector3 lCentre = rw::math::vpu::TransformPoint(
                    lTransform, lpVehicleTypeRuntime->GetBBoxOffset());
                const Vector3 lHalf = lpVehicleTypeRuntime->GetBBoxHalfSize();

                const Vector3 lToPlayer = lPlayerPos - lCentre;
                const f32 lfLocalX = rw::math::vpu::Dot(lToPlayer, lTransform.xAxis);
                const f32 lfLocalY = rw::math::vpu::Dot(lToPlayer, lTransform.yAxis);
                const f32 lfLocalZ = rw::math::vpu::Dot(lToPlayer, lTransform.zAxis);

                const f32 KF_T4_CLIP_EXPAND = 0.5f;
                const bool lbInside =
                    (lfLocalX > -(lHalf.x + KF_T4_CLIP_EXPAND) && lfLocalX < (lHalf.x + KF_T4_CLIP_EXPAND)) &&
                    (lfLocalY > -(lHalf.y + KF_T4_CLIP_EXPAND) && lfLocalY < (lHalf.y + KF_T4_CLIP_EXPAND)) &&
                    (lfLocalZ > -(lHalf.z + KF_T4_CLIP_EXPAND) && lfLocalZ < (lHalf.z + KF_T4_CLIP_EXPAND));

                if (!lbInside)
                {
                    continue;
                }

                sabReported[luVehicle] = true;

                if (lpVehicle->IsPhysical())
                {
                    *lpDiag << "[T4-clip] player inside PHYSICAL traffic vehicle "
                            << static_cast<s32>(luVehicle)
                            << " at " << lPlayerPos.x << "," << lPlayerPos.y << "," << lPlayerPos.z
                            << " -- promotion WORKED, the CONTACTS failed (physics side)"
                               " [DELETE-WHEN-STABLE]\n";
                }
                else
                {
                    *lpDiag << "[T4-clip] player inside NON-physical traffic vehicle "
                            << static_cast<s32>(luVehicle)
                            << " at " << lPlayerPos.x << "," << lPlayerPos.y << "," << lPlayerPos.z
                            << " (collidable=" << (lpVehicle->IsCollidable() ? 1 : 0)
                            << ") -- no physics slot [DELETE-WHEN-STABLE]\n";
                }
            }
        }
    }

    if (luMoved != 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T2-scene] one-shot, then value-latched on the published count so a reader can
            // see the population change rather than a single frame. DELETE-WHEN-STABLE.
            static s32 siLastPublished = -1;
            if (siLastPublished != static_cast<s32>(luMoved))
            {
                const bool lbFirst = (siLastPublished < 0);
                siLastPublished = static_cast<s32>(luMoved);
                *lpDiag << (lbFirst ? "[T2-scene] FIRST GenerateSceneUpdateEvents: moved "
                                    : "[T2-scene] published count changed: moved ")
                        << siLastPublished << " traffic entities\n";
            }
        }
    }
}

}
