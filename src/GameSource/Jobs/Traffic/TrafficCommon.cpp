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
    CGS_ASSERT(lpapHulls,             "lpapHulls");
    CGS_ASSERT(lpaParams,             "lpaParams");
    CGS_ASSERT(lpaParamTransforms,    "lpaParamTransforms");
    CGS_ASSERT(lpaVehicles,           "lpaVehicles");
    CGS_ASSERT(lpaVehicleTransforms,  "lpaVehicleTransforms");
    CGS_ASSERT(lpaVehicleAxles,       "lpaVehicleAxles");
    CGS_ASSERT(lpaVehicleRuntimeData, "lpaVehicleRuntimeData");
    CGS_ASSERT(lpRaceCarState,        "lpRaceCarState");
    CGS_ASSERT(lpEffectRand,          "lpEffectRand");

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
    mpOutNewPhysicalRequests         = nullptr;

    miLocalPlayerIndex               = static_cast<s8>(leLocalPlayerIndex);
    // The X360 stores the whole 16-byte VMX register (stvx128 v127 @ +0x80); the source
    // behaviour-centre arrives in a vector register. Copy all four lanes (Vector3 and
    // Vector4 are both the 16-byte {x,y,z,w} VPU slot).
    mBehaviourCentre.x               = lvBehaviourCentre.x;
    mBehaviourCentre.y               = lvBehaviourCentre.y;
    mBehaviourCentre.z               = lvBehaviourCentre.z;
    mBehaviourCentre.w               = lvBehaviourCentre.w;
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
