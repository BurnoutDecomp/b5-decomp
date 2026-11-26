#define CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\jobs\\traffic\\TrafficCommon.h"
#include "GameSource/Jobs/Traffic/TrafficCommon.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// BrnTraffic::UpdateVehiclesJobParams::Construct @ 0x82706E28. Fill the per-job
// parameter block for the traffic vehicle-update job (built by
// TrafficEntityModule::UpdateVehicles). Store-for-store from BURNOUT_X360_ARTIST.XEX:
// nine null-check asserts (in asm order) then the 24 field stores, with the union arm
// tagged E_JOBPROCESS_UPDATE_VEHICLES and the output-list pointer seeded null.

void BrnTraffic::UpdateVehiclesJobParams::Construct(
    u32                        luBeginVehicle,
    u32                        luEndVehicle,
    BrnTraffic::Hull**         lpapHulls,
    u16                        luNumHulls,
    const BrnTraffic::Param*   lpaParams,
    const BrnTraffic::ParamTransform* lpaParamTransforms,
    BrnTraffic::Vehicle*       lpaVehicles,
    Matrix44Affine*            lpaVehicleTransforms,
    BrnTraffic::VehicleAxles*  lpaVehicleAxles,
    const BrnTraffic::VehicleTypeRuntime* lpaVehicleRuntimeData,
    const BrnTraffic::RaceCarStateData*   lpRaceCarState,
    f32                        lfSimTimeStep,
    f32                        lfSimTimeSinceLastDecision,
    const BrnTraffic::Random*  lpEffectRand,
    EActiveRaceCarIndex        leLocalPlayerIndex,
    bool                       lbGameModeAllowsHardcoreSwerving,
    bool                       lbGameModeAllowsSwerving,
    bool                       lbDEBUGStopTrafficMoving,
    Vector3                    lvBehaviourCentre,
    f32                        lfCrashSliderFinalValue,
    DebugRenderStreamReader*   lpDebugStream)
{
    CGS_ASSERT_AT(lpapHulls,             "lpapHulls", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 237);
    CGS_ASSERT_AT(lpaParams,             "lpaParams", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 238);
    CGS_ASSERT_AT(lpaParamTransforms,    "lpaParamTransforms", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 239);
    CGS_ASSERT_AT(lpaVehicles,           "lpaVehicles", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 240);
    CGS_ASSERT_AT(lpaVehicleTransforms,  "lpaVehicleTransforms", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 241);
    CGS_ASSERT_AT(lpaVehicleAxles,       "lpaVehicleAxles", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 242);
    CGS_ASSERT_AT(lpaVehicleRuntimeData, "lpaVehicleRuntimeData", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 243);
    CGS_ASSERT_AT(lpRaceCarState,        "lpRaceCarState", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 244);
    CGS_ASSERT_AT(lpEffectRand,          "lpEffectRand", CGS_ARTIST_ABS_TRAFFICCOMMON_H_FILE, 245);

    meProcess                        = E_JOBPROCESS_UPDATE_VEHICLES;
    mpDebugStream                    = lpDebugStream;

    muBeginVehicle                   = static_cast<u16>(luBeginVehicle);
    muEndVehicle                     = static_cast<u16>(luEndVehicle);
    mpapHulls                        = lpapHulls;
    muNumHulls                       = luNumHulls;

    mpaParams                        = lpaParams;
    mpaParamTransforms               = lpaParamTransforms;
    mpaVehicles                      = lpaVehicles;
    mpaVehicleTransforms             = lpaVehicleTransforms;
    mpaVehicleAxles                  = lpaVehicleAxles;
    mpaVehicleRuntimeData            = lpaVehicleRuntimeData;
    mpRaceCarState                   = lpRaceCarState;

    mfSimTimeStep                    = lfSimTimeStep;
    mfSimTimeSinceLastDecision       = lfSimTimeSinceLastDecision;

    mEffectRand                      = *lpEffectRand;

    mfCrashSliderFinalValue          = lfCrashSliderFinalValue;
    mpOutNewPhysicalRequests         = 0;

    miLocalPlayerIndex               = static_cast<s8>(leLocalPlayerIndex);
    // The X360 stores the whole 16-byte VMX register (stvx128 v127 @ +0x80); the source
    // behaviour-centre arrives in a vector register. Copy all four lanes (Vector3 and
    // Vector4 are both the 16-byte {x,y,z,w} VPU slot).
    mBehaviourCentre                 = lvBehaviourCentre.mV;
    mbGameModeAllowsHardcoreSwerving = lbGameModeAllowsHardcoreSwerving;
    mbGameModeAllowsSwerving         = lbGameModeAllowsSwerving;
    mbDEBUGStopTrafficMoving         = lbDEBUGStopTrafficMoving;
}

// X360: inlined into TrafficJobStub::Execute @0x82752CB0 (it asserts the incoming list pointer
// then stores it at the params' +0x90). DWARF body _compile/BrnTrafficUnity2.cpp:229.
void BrnTraffic::UpdateVehiclesJobParams::SetOutputs(
    BrnTraffic::PhysicalRequestInfoList* lpOutNewPhysicalRequests)
{
    CGS_ASSERT(lpOutNewPhysicalRequests, "lpOutNewPhysicalRequests");
    mpOutNewPhysicalRequests = lpOutNewPhysicalRequests;
}
