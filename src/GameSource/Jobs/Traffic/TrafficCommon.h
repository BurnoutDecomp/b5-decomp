#pragma once

// BrnTraffic::UpdateVehiclesJobParams (+ base BaseTrafficJobParams, JobProcess enum,
// JobParams union) -- the per-job parameter block handed to the traffic vehicle-update
// job (built by BrnTraffic::TrafficEntityModule::UpdateVehicles, consumed by the job).
//
// Layout + member NAMES are verbatim from the DecFIGS DWARF (TrafficCommon.h:52..182).
// Byte offsets are pinned by the X360 asm of UpdateVehiclesJobParams::Construct
// @ 0x82706E28 (store base = r31, displacement == true byte offset). The struct is
// non-polymorphic (first store is meProcess at +0; no vptr). mEffectRand is a
// by-value CgsNumeric::Random (sizeof 0x30, copied whole by the 6x ld/std loop).
// mBehaviourCentre is a 16-byte VMX register (stvx128 at +0x80) == one Vector4.
//
// Only Construct is bodied in TrafficCommon.cpp (this batch). SetOutputs and the
// BaseTrafficJobParams::Construct are declared-only, owned by a future TU.

#include "types.hpp"          // u32/u16/s8/f32/bool
#include "BrnCommonTypes.h"   // Vector4, Vector3, Matrix44Affine
#include "GameSource/BurnoutConstants.h"  // EActiveRaceCarIndex
#include "GameShared/GameClasses/Numeric/CgsRandom.h"  // CgsNumeric::Random

// -- opaque peripheral types (used only as pointers here) ------------------------
struct ParamTransform;
struct Vehicle;
struct VehicleAxles;
struct VehicleTypeRuntime;
struct RaceCarStateData;
struct DebugRenderStreamReader;

namespace BrnTraffic {

using CgsNumeric::Random;

struct Hull;
struct Param;
// The output physical-request list the traffic job fills; real home is
// BrnTrafficMiscRuntimeClasses.h (Array<PhysicalRequestInfo,25>). Forward-declared here
// (only used by-pointer).
struct PhysicalRequestInfoList;

// DWARF TrafficCommon.h:52
enum JobProcess
{
    E_JOBPROCESS_UPDATE_VEHICLES = 0,
    E_JOBPROCESS_COUNT           = 1,
};

// DWARF TrafficCommon.h:69 -- sizeof 8, no vptr.
struct BaseTrafficJobParams
{
    void Construct(JobProcess leProcess, DebugRenderStreamReader* lpDebugStream);

    JobProcess               meProcess;      // +0x00
    DebugRenderStreamReader*  mpDebugStream;  // +0x04
};

// DWARF TrafficCommon.h:95 -- offsets pinned by Construct @ 0x82706E28.
struct alignas(16) UpdateVehiclesJobParams : public BaseTrafficJobParams
{
    void Construct(
        u32                       luBeginVehicle,
        u32                       luEndVehicle,
        Hull**                    lpapHulls,
        u16                       luNumHulls,
        const Param*              lpaParams,
        const ParamTransform*     lpaParamTransforms,
        Vehicle*                  lpaVehicles,
        Matrix44Affine*           lpaVehicleTransforms,
        VehicleAxles*             lpaVehicleAxles,
        const VehicleTypeRuntime* lpaVehicleRuntimeData,
        const RaceCarStateData*   lpRaceCarState,
        f32                       lfSimTimeStep,
        f32                       lfSimTimeSinceLastDecision,
        const Random*             lpEffectRand,
        EActiveRaceCarIndex       leLocalPlayerIndex,
        bool                      lbGameModeAllowsHardcoreSwerving,
        bool                      lbGameModeAllowsSwerving,
        bool                      lbDEBUGStopTrafficMoving,
        Vector3                   lvBehaviourCentre,
        f32                       lfCrashSliderFinalValue,
        DebugRenderStreamReader*  lpDebugStream);

    void SetOutputs(PhysicalRequestInfoList* lpOutNewPhysicalRequests);

    u16                       muBeginVehicle;                   // +0x08
    u16                       muEndVehicle;                     // +0x0A
    Hull**                    mpapHulls;                        // +0x0C
    u16                       muNumHulls;                       // +0x10
    const Param*              mpaParams;                        // +0x14
    const ParamTransform*     mpaParamTransforms;               // +0x18
    Vehicle*                  mpaVehicles;                      // +0x1C
    Matrix44Affine*           mpaVehicleTransforms;             // +0x20
    VehicleAxles*             mpaVehicleAxles;                  // +0x24
    const VehicleTypeRuntime* mpaVehicleRuntimeData;            // +0x28
    const RaceCarStateData*   mpRaceCarState;                   // +0x2C
    f32                       mfSimTimeStep;                    // +0x30
    f32                       mfSimTimeSinceLastDecision;       // +0x34
    // (+0x38..+0x3F padding to 16-align mEffectRand)
    Random                    mEffectRand;                      // +0x40 (sizeof 0x30)
    f32                       mfCrashSliderFinalValue;          // +0x70
    s8                        miLocalPlayerIndex;               // +0x74
    bool                      mbGameModeAllowsHardcoreSwerving; // +0x75
    bool                      mbGameModeAllowsSwerving;         // +0x76
    bool                      mbDEBUGStopTrafficMoving;         // +0x77
    // (+0x78..+0x7F padding to 16-align mBehaviourCentre)
    Vector4                   mBehaviourCentre;                 // +0x80 (16 bytes)
    PhysicalRequestInfoList*  mpOutNewPhysicalRequests;         // +0x90
};

// DWARF TrafficCommon.h:179
union JobParams
{
    JobProcess               meProcess;        // +0x00
    UpdateVehiclesJobParams  mUpdateVehicles;
};

} // namespace BrnTraffic
