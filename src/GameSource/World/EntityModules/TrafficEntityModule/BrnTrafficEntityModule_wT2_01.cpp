// ============================================================================
// BrnTrafficEntityModule_wT2_01.cpp -- driving-traffic generation and the lane-param list.
//
// The console makes a moving car through one chain:
//   UpdateDecisionFrame -> RebuildGeneratorList @0x82742DD0 -> AddGenerator @0x82734B00
//   SpawnNewTraffic     -> FillNewHull @0x82743600 (driving half) / the generator tick
//                        -> GenerateNewVehicle @0x82736528
//                             -> TryAllocateParamId @0x82723370 -> Param::Initialise
//                             -> Section::CalcTransformAtParameter -> ParamTransform::Initialise
//   UpdateParams_UpdateLinkedList -> InsertParamIntoList / SwapParamsInList / RemoveParamFromList
//   KillOutOfAreaTraffic @0x82734C78 / KillAllZombies @0x82734DF8 -> KillParam @0x82721FB8
//                        -> PutParamInPurgatory @0x82716510 (the free-param recycle loop)
//   RecalculateActiveHulls @0x8274C870 tail -> RebuildGeneratorList (its only xref)
//
// Layout is host-native: every member is reached by name, and the console displacements in the
// comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData, VehicleTypeData
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // Section, LaneRung
#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"        // SectionFlow
#include "SharedClasses/Traffic/BrnTrafficVehicleTraits.h"      // VehicleTraits

#include "rw/math/vpu/vector3_operation.h"                    // Dot
#include "rw/math/vpu/vector4_operation.h"                    // Splat

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cmath>     // std::floor, std::cos, std::sin

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE, file-local. NOT IN THE X360 BINARY.
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

    // ---- the generation tuning constants, recovered per call site -------------------------
    // AddGenerator @0x82734C18 flt_820BA4D0. Each generator on a hull is phase-shifted by this
    // fraction of its own period so a freshly activated hull does not emit every car at once.
    // unk_8300CC00 == 10000.0f == 100 m squared (dyn-init thunk 0x82C662B0 squares
    // unk_8300C960 == splat(flt_820BA5C8 == 100.0f)). GenerateNewVehicle @0x827366D4.
    const f32 KF_STARTLINE_SPAWN_CULL_RADIUS_SQ = 10000.0f;

    const f32 KF_GENERATOR_PHASE_SHIFT = 0.69999999f;

    // CalcTimeToNextGeneration @0x82721B6C / FillNewHull @0x82743698 flt_82001C98. The floor
    // the section's own rate is clamped UP to, and simultaneously the ceiling that clamp may
    // not exceed (`Max(vpm * scale, Min(vpm, KF_MIN_VEHICLES_PER_MINUTE))`).
    const f32 KF_MIN_VEHICLES_PER_MINUTE = 1.0f;

    // CalcTimeToNextGeneration @0x82721BD0 flt_820BA7DC. Divide-by-zero guard on the rate.
    const f32 KF_MIN_VEHICLES_PER_MINUTE_EPSILON = 1.0e-15f;

    // CalcTimeToNextGeneration @0x82721C14 flt_820BA5B4 and the inlined RandomFloat range
    // flt_820138AC == 0.40000004 == (1.2f - 0.8f) in f32, which fixes the max exactly
    // (the same min/range pair renders as `x * 0.40000004 + 0.80000001` at 0x826F99C0).
    const f32 KF_MIN_GENERATION_FACTOR = 0.80000001f;
    const f32 KF_MAX_GENERATION_FACTOR = 1.2f;
}

// ============================================================================
// SECTION 1 -- the generator list.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::RebuildGeneratorList  @ 0x82742DD0   (leak BrnTrafficEntityModule.cpp:4183)
//
// One generator per lane section that traffic can enter the active-hull set through: either the
// section behind it is outside the set, or there is no section behind it at all.
// ----------------------------------------------------------------------------
void TrafficEntityModule::RebuildGeneratorList()
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");

    f32 lfPhase = 0.0f;                 // 0x82742E30 flt_82001CC0 == 0.0f

    muNumGenerators = 0;                // 0x82742E2C

    for (u32 luActiveHull = 0; luActiveHull < mActiveHulls.GetLength(); ++luActiveHull)
    {
        const u32   luHull = mActiveHulls[luActiveHull];
        const Hull* lpHull = GetHull(luHull);

        for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
        {
            // Hull::GetFlowData (BrnTrafficHull.h:264), console-inlined here as
            // `lwz r11, 0x28(hull)` + the 4-byte stride; that method is declared in
            // BrnTrafficHull.h, which this cluster does not own.
            const SectionFlow* lpFlow = &lpHull->mpaSectionFlows[luSection];

            if (lpFlow->muVehiclesPerMinute == 0)
            {
                continue;
            }

            const Section* lpSection = lpHull->GetSection(luSection);

            // 0x82742FDC / 0x82742FE8. The console fuses the leak's two arms into one test.
            if (mActiveHulls.Find(lpSection->mauBackwardHulls[E_DIR_STRAIGHT_ON]) == ActiveHullSet::KU_INVALID
                || lpSection->mauBackwardSections[E_DIR_STRAIGHT_ON] == KU_INVALID_SECTION)
            {
                AddGenerator(luHull, luSection, &lfPhase);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::AddGenerator  @ 0x82734B00   (leak :4237)
//
// Appends one generator and advances the shared emission phase by KF_GENERATOR_PHASE_SHIFT,
// wrapped into [0, 1) with the console's inlined double-precision Floor.
// ----------------------------------------------------------------------------
void TrafficEntityModule::AddGenerator(u32 luHull, u32 luSection, f32* lpfTimeTillNextGeneration)
{
    CGS_ASSERT(muNumGenerators < KU_MAX_GENERATORS - 1, "muNumGenerators < KU_MAX_GENERATORS - 1");
    CGS_ASSERT(*lpfTimeTillNextGeneration >= -0.001f, "*lpUpdatedPhase >= -0.001f");
    CGS_ASSERT(*lpfTimeTillNextGeneration <= 1.001f, "*lpUpdatedPhase <= 1.001f");

    maGenerators[muNumGenerators].muHull    = static_cast<u16>(luHull);
    maGenerators[muNumGenerators].muSection = static_cast<u8>(luSection);

    mafTimesTillNextGeneration[muNumGenerators] =
        CalcTimeToNextGeneration(luHull, luSection) + *lpfTimeTillNextGeneration;

    ++muNumGenerators;

    const f32 lfNewPhase = *lpfTimeTillNextGeneration + KF_GENERATOR_PHASE_SHIFT;
    *lpfTimeTillNextGeneration = lfNewPhase - std::floor(lfNewPhase);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::CalcTimeToNextGeneration  @ 0x82721B08   (leak :4266)
//
// Seconds until this section emits its next car. The leak multiplies the section rate by a
// time-of-day GlobalTrafficDensity variable; the ship does not -- 0x82721B80 reads
// mfTrafficAmountScale and nothing else.
// ----------------------------------------------------------------------------
f32 TrafficEntityModule::CalcTimeToNextGeneration(u32 luHull, u32 luSection)
{
    const Hull* lpHull = GetHull(luHull);
    CGS_ASSERT(luSection < lpHull->muNumSections, "luIndex < muNumSections");

    const SectionFlow* lpFlowData = &lpHull->mpaSectionFlows[luSection];

    f32 lfVehiclesPerMinute = 0.0f;

    if (lpFlowData->muVehiclesPerMinute != 0 && mfTrafficAmountScale > 0.0f)
    {
        const f32 lfSectionRate = static_cast<f32>(lpFlowData->muVehiclesPerMinute);

        // 0x82721BA8 / 0x82721BB0, two fsels: raise the scaled rate to the floor, but never
        // above the section's own unscaled rate.
        const f32 lfFloor = (lfSectionRate >= KF_MIN_VEHICLES_PER_MINUTE)
                                ? KF_MIN_VEHICLES_PER_MINUTE
                                : lfSectionRate;
        const f32 lfScaled = mfTrafficAmountScale * lfSectionRate;

        lfVehiclesPerMinute = (lfScaled >= lfFloor) ? lfScaled : lfFloor;
    }

    if (lfVehiclesPerMinute < KF_MIN_VEHICLES_PER_MINUTE_EPSILON)   // 0x82721BFC fsel
    {
        lfVehiclesPerMinute = KF_MIN_VEHICLES_PER_MINUTE_EPSILON;
    }

    const f32 lfBaseTime   = KF_SECONDS_PER_MINUTE / lfVehiclesPerMinute;
    const f32 lfModulation = mRand.RandomFloat(KF_MIN_GENERATION_FACTOR, KF_MAX_GENERATION_FACTOR);

    return lfModulation * lfBaseTime;
}

// ============================================================================
// SECTION 2 -- birth of one driving vehicle.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::TryAllocateParamId  @ 0x82723370   (leak :4878)
//
// Ledger files this under CgsStack.h; it is a real TrafficEntityModule member.
// ----------------------------------------------------------------------------
u32 TrafficEntityModule::TryAllocateParamId()
{
    if (!mFreeParams.IsEmpty())
    {
        const u32 luFreeParam = mFreeParams.Peek();

        CGS_ASSERT(!GetParam(luFreeParam)->IsAlive(), "Param was alive when it was allocated");

        mFreeParams.Pop();
        return luFreeParam;
    }

    ++miDEBUGOverBudgetness;            // 0x827234A4, this + 0x727C4
    return KU_INVALID_PARAM;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::GenerateNewVehicle  @ 0x82736528   (leak :4785)
//
// PARAMETER ORDER is the console's prologue, which is the leak's and NOT the keystone header's
// parameter NAMES: 0x82736548 r4 = luVehicleTypeId, 0x82736550 r5 = luHullIndex, 0x82736558
// r6 = luSectionIndex, f1 = lfParamAlong. All four declared types are identical, so the
// declaration binds; only the names in BrnTrafficEntityModule.h (not this cluster's file) read
// in the wrong order.
// ----------------------------------------------------------------------------
void TrafficEntityModule::GenerateNewVehicle(u32 luVehicleTypeId,
                                             u32 luHullIndex,
                                             u32 luSectionIndex,
                                             f32 lfParamAlong)
{
    CGS_ASSERT(luVehicleTypeId < mpData->muNumVehicleTypes,
               "luVehicleTypeId < mpData->muNumVehicleTypes");
    CGS_ASSERT(lfParamAlong >= 0.0f, "lfParamAlong >= 0.0f");
    CGS_ASSERT(lfParamAlong < static_cast<f32>(GetHull(luHullIndex)->GetSection(luSectionIndex)->muNumRungs),
               "lfParamAlong < (float32_t)GetHull( luHullIndex )->GetSection( luSectionIndex )->muNumRungs");

    const Hull*    lpHull    = GetHull(luHullIndex);
    const Section* lpSection = lpHull->GetSection(luSectionIndex);

    // 0x8273662C..0x8273663C. The debug forced type, seeded to -1 by Construct.
    if (miDEBUGOverrideVehicleToSpawn >= 0)
    {
        luVehicleTypeId = static_cast<u32>(miDEBUGOverrideVehicleToSpawn);
    }

    if (mbAtStartLineSoProtectRaceCarsFromTraffic && mbAllowDivergentBehaviour)
    {
        // 0x82736670..0x827366E4. Reference lane +0x728C0 == mCameraLastFrame's Pos row;
        // radius unk_8300CC00 == 10000.0f == 100 m squared (dyn-init thunk 0x82C662B0 squares
        // unk_8300C960 == splat(flt_820BA5C8 == 100.0f)).
        Vector3 lSpawnPos;
        lpSection->CalcPositionAtParameter(lpHull->mpaRungs,
                                           rw::math::vpu::Splat(lfParamAlong),
                                           static_cast<u32>(lfParamAlong),
                                           lSpawnPos);

        const Vector3 lToSpawn = lSpawnPos - mCameraLastFrame.GetPosition();
        if (rw::math::vpu::Dot(lToSpawn, lToSpawn) < KF_STARTLINE_SPAWN_CULL_RADIUS_SQ)
        {
            return;
        }
    }

    const u32 luFreeSlot = TryAllocateParamId();
    if (luFreeSlot == KU_INVALID_PARAM)
    {
        return;
    }

    Param* lpNewParam = GetParam(luFreeSlot);
    CGS_ASSERT(!lpNewParam->IsAlive(), "Recycled param which was still alive");

    // 0x827367A4..0x82736868. RandomFloat(0,1) inlined; its (v - 1.0f) is the raw ring float
    // brought back into [0,1).
    const f32 lfRandomVal = mRand.RandomFloat(0.0f, 1.0f);

    const VehicleTypeData*    lpVehicleType        = &mpData->mpaVehicleTypes[luVehicleTypeId];
    const VehicleTypeRuntime* lpVehicleTypeRuntime = GetVehicleTypeRuntime(luVehicleTypeId);
    const VehicleTraits*      lpVehicleTraits      = mpData->GetVehicleTraitsForVehicleType(luVehicleTypeId);

    lpNewParam->Initialise(luHullIndex,
                           luSectionIndex,
                           lfParamAlong,
                           lfRandomVal,
                           luVehicleTypeId,
                           lpHull,
                           lpVehicleType,
                           lpVehicleTypeRuntime,
                           lpVehicleTraits,
                           luFreeSlot,
                           mParamSoaData);

    // 0x8273688C..0x827368DC. The third output of CalcTransformAtParameter is the RIGHT axis
    // here (leak :4864 stores it into ParamTransform::mRight); the declaration in
    // BrnTrafficSection.h names that parameter for its WorldMap consumer instead.
    // 0x827368A0 vspltw v1, v0, 0 -- the parameter reaches Section as a broadcast lane.
    const VecFloat lParamLane = { lfParamAlong, lfParamAlong, lfParamAlong, lfParamAlong };

    Vector3 lPos;
    Vector3 lDir;
    Vector3 lRight;
    lpSection->CalcTransformAtParameter(lpHull->mpaRungs,
                                        lParamLane,
                                        lpNewParam->muCurrentSegment,
                                        lPos,
                                        lDir,
                                        lRight);

    // 0x82736... GetParamTransform @0x82707700 (export hole) resolves to this same slot;
    // the console indexes maParamTransforms inline here.
    ParamTransform* lpTransform = &maParamTransforms[luFreeSlot];

    VecFloat lfSpeed;
    lfSpeed.x = lpNewParam->mfSpeed;
    lfSpeed.y = lpNewParam->mfSpeed;
    lfSpeed.z = lpNewParam->mfSpeed;
    lfSpeed.w = lpNewParam->mfSpeed;
    lpTransform->Initialise(lPos, lDir, lRight, lfSpeed);

    // 0x827368E0. Offline (mbAllowDivergentBehaviour false) a still-alive recycled vehicle is
    // divorced from its param rather than asserted about.
    if (!mbAllowDivergentBehaviour)
    {
        Vehicle* lpVehicle = GetVehicle(luFreeSlot);
        if (lpVehicle->IsAlive())
        {
            lpNewParam->SetZombie(luFreeSlot, mParamSoaData);
            lpNewParam->SetDivorced();   // 0x8273691C
        }
    }
    else
    {
        CGS_ASSERT(!GetVehicle(luFreeSlot)->IsAlive(),
                   "Vehicle was still alive when its param was reallocated");
    }
}

// ============================================================================
// SECTION 3 -- the per-section ordered param list.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::InsertParamIntoList  @ 0x82725CB8   (leak :6766)
// ----------------------------------------------------------------------------
void TrafficEntityModule::InsertParamIntoList(u32 luParam, u32 luHull, u32 luSection, f32 lfParamAlong)
{
    CGS_ASSERT(luParam < KU_MAX_STANDARD_TRAFFIC, "luParam < KU_MAX_STANDARD_TRAFFIC");

    const u32 luNextParamId = FindNextParam(luHull, luSection, lfParamAlong);
    CGS_ASSERT(luNextParamId != luParam, "luNextParamId != luParam");

    ParamListNode* lpParamNode = &maParamListNodes[luParam];
    CGS_ASSERT(lpParamNode->mfParamAlong == lfParamAlong, "lpParamNode->mfParamAlong == lfParamAlong");

    if (luNextParamId == KU_INVALID_PARAM)
    {
        lpParamNode->muNextParam = static_cast<u16>(KU_INVALID_PARAM);
        lpParamNode->muPrevParam = static_cast<u16>(KU_INVALID_PARAM);

        GetHullRuntime(luHull)->SetFirstParamInSection(luSection,
                                                       static_cast<u16>(luParam),
                                                       static_cast<u16>(KU_INVALID_PARAM));
        return;
    }

    CGS_ASSERT(luNextParamId < KU_MAX_PARAMS, "Out of range param in list");

    ParamListNode* lpNextParamNode = &maParamListNodes[luNextParamId];
    CGS_ASSERT(GetParam(luParam)->muSectionIndex == GetParam(luNextParamId)->muSectionIndex,
               "Param in front is in a different section");

    if (lfParamAlong < lpNextParamNode->mfParamAlong
        || (lfParamAlong == lpNextParamNode->mfParamAlong && luParam < luNextParamId))
    {
        lpParamNode->muNextParam     = static_cast<u16>(luNextParamId);
        lpParamNode->muPrevParam     = lpNextParamNode->muPrevParam;
        lpNextParamNode->muPrevParam = static_cast<u16>(luParam);

        if (lpParamNode->muPrevParam != static_cast<u16>(KU_INVALID_PARAM))
        {
            ParamListNode* lpPrevParamNode = &maParamListNodes[lpParamNode->muPrevParam];
            CGS_ASSERT(lpPrevParamNode->muNextParam == luNextParamId,
                       "lpPrevParamNode->muNextParam == luNextParamId");
            CGS_ASSERT(lpPrevParamNode->mfParamAlong <= lpNextParamNode->mfParamAlong,
                       "lpPrevParamNode->mfParamAlong <= lpNextParamNode->mfParamAlong");
            CGS_ASSERT(lpPrevParamNode->mfParamAlong <= lfParamAlong,
                       "lpPrevParamNode->mfParamAlong <= lfParamAlong");
            CGS_ASSERT(GetParam(lpParamNode->muPrevParam)->muSectionIndex == GetParam(luParam)->muSectionIndex,
                       "GetParam( lpParamNode->muPrevParam )->muSectionIndex == GetParam( luParam )->muSectionIndex");

            lpPrevParamNode->muNextParam = static_cast<u16>(luParam);

            CGS_ASSERT(lpPrevParamNode->muNextParam == lpNextParamNode->muPrevParam,
                       "lpPrevParamNode->muNextParam == lpNextParamNode->muPrevParam");
        }
        else
        {
            GetHullRuntime(luHull)->SetFirstParamInSection(luSection,
                                                           static_cast<u16>(luParam),
                                                           lpParamNode->muNextParam);
        }
    }
    else
    {
        ParamListNode* lpPrevParamNode = lpNextParamNode;
        const u32      luPrevParamId   = luNextParamId;

        lpParamNode->muNextParam     = lpPrevParamNode->muNextParam;
        lpParamNode->muPrevParam     = static_cast<u16>(luPrevParamId);
        lpPrevParamNode->muNextParam = static_cast<u16>(luParam);

        if (lpParamNode->muNextParam != static_cast<u16>(KU_INVALID_PARAM))
        {
            lpNextParamNode = &maParamListNodes[lpParamNode->muNextParam];
            CGS_ASSERT(lpNextParamNode->muPrevParam == luPrevParamId,
                       "lpNextParamNode->muPrevParam == luPrevParamId");
            CGS_ASSERT(lpPrevParamNode->mfParamAlong <= lpNextParamNode->mfParamAlong,
                       "lpPrevParamNode->mfParamAlong <= lpNextParamNode->mfParamAlong");
            CGS_ASSERT(lpNextParamNode->mfParamAlong >= lfParamAlong,
                       "lpNextParamNode->mfParamAlong >= lfParamAlong");
            CGS_ASSERT(GetParam(luParam)->muSectionIndex == GetParam(luNextParamId)->muSectionIndex,
                       "GetParam( luParam )->muSectionIndex == GetParam( luNextParamId )->muSectionIndex");

            lpNextParamNode->muPrevParam = static_cast<u16>(luParam);

            CGS_ASSERT(lpPrevParamNode->muNextParam == lpNextParamNode->muPrevParam,
                       "lpPrevParamNode->muNextParam == lpNextParamNode->muPrevParam");
        }
    }

    CGS_ASSERT(lpParamNode->muNextParam != lpParamNode->muPrevParam,
               "lpParamNode->muNextParam != lpParamNode->muPrevParam");
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::SwapParamsInList  @ 0x827261E8   (EXPORT HOLE; leak :6864)
//
// Signature taken from its two call sites inside UpdateParams_UpdateLinkedList @0x82739660
// (0x82739A5C passes r4 = the current param and r5 = the one in front; 0x82739A98 passes
// r4 = the one behind and r5 = the current param), so the pair is always (earlier, later).
// ----------------------------------------------------------------------------
void TrafficEntityModule::SwapParamsInList(u32 luParam, u32 luNextParam)
{
    CGS_ASSERT(luParam < KU_MAX_STANDARD_TRAFFIC, "luParam < KU_MAX_STANDARD_TRAFFIC");
    CGS_ASSERT(luNextParam < KU_MAX_STANDARD_TRAFFIC, "luNextParam < KU_MAX_STANDARD_TRAFFIC");

    ParamListNode* lpNode     = &maParamListNodes[luParam];
    ParamListNode* lpNextNode = &maParamListNodes[luNextParam];

    CGS_ASSERT(lpNode->muNextParam == luNextParam, "lpNode->muNextParam == luNextParam");
    CGS_ASSERT(luParam == lpNextNode->muPrevParam, "luParam == lpNextNode->muPrevParam");

    const u32 luPrevParam = lpNode->muPrevParam;
    lpNextNode->muPrevParam = static_cast<u16>(luPrevParam);

    if (luPrevParam != KU_INVALID_PARAM)
    {
        maParamListNodes[luPrevParam].muNextParam = static_cast<u16>(luNextParam);
    }
    else
    {
        const Param* lpParam = GetParam(luParam);
        GetHullRuntime(lpParam->muStartHullIndex)
            ->SetFirstParamInSection(lpParam->muSectionIndex,
                                     static_cast<u16>(luNextParam),
                                     static_cast<u16>(luParam));
    }

    const u32 luNextNextParam = lpNextNode->muNextParam;
    lpNode->muNextParam = static_cast<u16>(luNextNextParam);

    if (luNextNextParam != KU_INVALID_PARAM)
    {
        maParamListNodes[luNextNextParam].muPrevParam = static_cast<u16>(luParam);
    }

    lpNextNode->muNextParam = static_cast<u16>(luParam);
    lpNode->muPrevParam     = static_cast<u16>(luNextParam);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::RemoveParamFromList  @ 0x82726340   (leak :6915)
// ----------------------------------------------------------------------------
void TrafficEntityModule::RemoveParamFromList(u32 luParam)
{
    CGS_ASSERT(luParam < KU_MAX_STANDARD_TRAFFIC, "luParam < KU_MAX_STANDARD_TRAFFIC");

    ParamListNode* lpNode = &maParamListNodes[luParam];

    if (lpNode->muNextParam != static_cast<u16>(KU_INVALID_PARAM))
    {
        ParamListNode* lpNextNode = &maParamListNodes[lpNode->muNextParam];
        CGS_ASSERT(lpNextNode->muPrevParam == luParam, "lpNextNode->muPrevParam == luParam");

        lpNextNode->muPrevParam = lpNode->muPrevParam;
    }

    if (lpNode->muPrevParam != static_cast<u16>(KU_INVALID_PARAM))
    {
        ParamListNode* lpPrevNode = &maParamListNodes[lpNode->muPrevParam];
        CGS_ASSERT(lpPrevNode->muNextParam == luParam, "lpPrevNode->muNextParam == luParam");

        lpPrevNode->muNextParam = lpNode->muNextParam;
    }
    else
    {
        // 0x8272642C / 0x82726450: the console reads muStartHullIndex (+0x66) and
        // muStartSectionIndex (+0x65), i.e. where the param was LINKED IN, not where it is now.
        const Param* lpParam = GetParam(luParam);
        if (lpParam->muStartHullIndex != KU_INVALID_HULL)
        {
            HullRuntime* lpHullRuntime = GetHullRuntimeSafe(lpParam->muStartHullIndex);
            if (lpHullRuntime != 0)
            {
                lpHullRuntime->SetFirstParamInSection(lpParam->muStartSectionIndex,
                                                      lpNode->muNextParam,
                                                      static_cast<u16>(luParam));
            }
        }
    }

    lpNode->muNextParam = static_cast<u16>(KU_INVALID_PARAM);
    lpNode->muPrevParam = static_cast<u16>(KU_INVALID_PARAM);
}

// ============================================================================
// SECTION 4 -- death.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::PutParamInPurgatory  @ 0x82716510   (.cpp 9821, .h 2350)
//
// Parks a dead param id for five decision frames before UpdateParams_UpdatePurgatoryList
// @0x827244E0 returns it to mFreeParams. Two xrefs: KillParam and UpdateParams_UpdateDead.
// Without it no param id is ever recycled and generation stops after 400 kills.
// ----------------------------------------------------------------------------
void TrafficEntityModule::PutParamInPurgatory(u32 luParam)
{
    CGS_ASSERT(meState == E_STATE_TEARING_DOWN || IsDecisionFrame(),
               "( meState == E_STATE_TEARING_DOWN ) || IsDecisionFrame()");
    CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");

    Param* lpParam = GetParam(luParam);

    // 0x82716594 lbz 0x40 / clrrwi 7: mxFlags bit 0x80 == E_FLAG_IN_PURGATORY.
    if (lpParam->IsInPurgatory())
    {
        return;
    }

    // 0x827165A4 `li 5`, one emission for both callers: KU_PURGATORY_TIME_ONLINE and
    // KU_PURGATORY_TIME_OFFLINE are both 5 in BrnTrafficConstants.h, so the pair folded and
    // the asm cannot say which name the source used.
    PurgatoryInfo lInfo;
    lInfo.muIndex              = static_cast<u16>(luParam);
    lInfo.muDecisionFramesLeft = 5;

    maPurgatoryList.Append(lInfo);
    lpParam->SetInPurgatory(true);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillParam  @ 0x82721FB8   PARTIAL   (the ship rewrote the leak's :4383)
//
// Marks the param dying, then either orphans or kills its vehicle, then purgatories the id.
// One gate remains, the crash-module / trailer arm; it names its blocker at the site.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillParam(u32 luParam)
{
    CGS_ASSERT(meState == E_STATE_TEARING_DOWN || IsDecisionFrame(),
               "( meState == E_STATE_TEARING_DOWN ) || IsDecisionFrame()");
    CGS_ASSERT(luParam < KU_MAX_STANDARD_TRAFFIC, "luParam < KU_MAX_STANDARD_TRAFFIC");

    Param*   lpParam   = GetParam(luParam);
    Vehicle* lpVehicle = GetVehicle(luParam);

    CGS_ASSERT(lpVehicle->IsAlive() || lpParam->IsZombie() || mbWaitingForStreaming
                   || meState != E_STATE_RUNNING,
               "Tried to kill a param that isn't a zombie but has a dead vehicle");

    // 0x82721F?? mxFlags read once, before SetDyingState rewrites it.
    const u32  lxParamFlags = lpParam->mxFlags;
    const bool lbDivorced   = (lxParamFlags & Param::E_FLAG_DIVORCED) != 0;

    bool lbKillVehicle = false;
    if ((lxParamFlags & Param::E_FLAG_SHOULD_BE_REMOVED) == 0
        || (lbDivorced && (!lpVehicle->IsAlive() || (lpVehicle->GetFlags() & Vehicle::E_FLAG_ORPHAN) == 0)))
    {
        lbKillVehicle = true;
    }

    lpParam->SetDyingState(luParam, mParamSoaData);

    if (lpVehicle->IsAlive() && (lpVehicle->GetFlags() & Vehicle::E_FLAG_ORPHAN) == 0)
    {
        if (lbKillVehicle && (lpVehicle->GetFlags() & Vehicle::E_FLAG_PHYSICAL) != 0)
        {
            CGS_ASSERT(!lbDivorced, "Param is divorced, but it's vehicle isn't an orphan");
            if (!lbDivorced)
            {
                lpVehicle->SetOrphan();
            }
        }
        else
        {
            lpVehicle->SetDead(luParam, mVehicleSoaData);

            {
                // GATE: EnsureVehicleRemovedFromCrashModule @0x82721?? and, for a STANDARD
                // vehicle with a trailer, the Vehicle::GetTrailerIndex / DetachArticulation
                // pair. Blocker: none of the three is declared in this tree
                // (BrnTrafficVehicle.h models muOtherHalfIndex but no accessor pair, and the
                // crash-module helper has no declaration at all).
                // DELETE-WHEN those declarations land (crash surface, wave 3 / trailers).
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "KillParam leg EnsureVehicleRemovedFromCrashModule + the trailer detach "
                    "(Vehicle::GetTrailerIndex / Vehicle::DetachArticulation) -- none of the "
                    "three is declared in this tree. The vehicle is still marked dead, so the "
                    "param slot recycles; a towed trailer stays alive one extra kill");
            }
        }
    }

    if (!mbAllowDivergentBehaviour)
    {
        CGS_ASSERT(lpParam->IsDying(), "Param wasn't dying when we expected it to be");

        lpParam->ClearDying(luParam, mParamSoaData);
        PutParamInPurgatory(luParam);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillAllZombies  @ 0x82734DF8
//
// Walks mParamSoaData.mZombieParams with the FastBitArray iterator and kills every set bit.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillAllZombies()
{
    const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrZombies = mParamSoaData.mZombieParams;

    for (CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>::Iterator lIt = lrZombies.Begin();
         lIt.GetIndex() != lrZombies.End();
         ++lIt)
    {
        const u32 luParam = static_cast<u32>(lIt.GetIndex());

        CGS_ASSERT(luParam < KU_MAX_PARAMS, "luParam < KU_MAX_PARAMS");
        CGS_ASSERT(GetParam(luParam)->IsZombie(), "GetParam( luParam )->IsZombie()");

        KillParam(luParam);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::KillOutOfAreaTraffic  @ 0x82734C78   (leak :4290)
//
// Ledger files this under CgsSet.h; it is a real TrafficEntityModule member. The argument is
// the OLD hull set RecalculateActiveHulls just produced.
// ----------------------------------------------------------------------------
void TrafficEntityModule::KillOutOfAreaTraffic(ActiveHullSet* lpOldHulls)
{
    CGS_ASSERT(lpOldHulls != 0, "lpOldHulls != NULL");

    if (lpOldHulls->GetLength() == 0)
    {
        return;
    }

    for (u32 luParam = 0; luParam < KU_MAX_STANDARD_TRAFFIC; ++luParam)
    {
        const Param* lpParam = GetParam(luParam);

        if (lpParam->IsAlive() && lpOldHulls->Contains(lpParam->muHullIndex))
        {
            KillParam(luParam);
        }
    }

    for (u32 luStaticParam = 0; luStaticParam < KU_MAX_STATIC_TRAFFIC; ++luStaticParam)
    {
        const StaticTrafficParam* lpStaticParam = &maStaticTrafficParams[luStaticParam];

        if (lpStaticParam->IsAlive()
            && lpOldHulls->Find(lpStaticParam->GetHull()) != ActiveHullSet::KU_INVALID)
        {
            StaticVehicles_KillParam(luStaticParam);
        }
    }
}

}
