// ============================================================================
// BrnTrafficEntityModule_wT1_01.cpp -- the traffic spawn legs.
//
// The console reaches a parked car through one chain, from the module's state machine to a
// transform written into maVehicleTransforms:
//
//   Construct @0x82740220 -> ResetEventData @0x827088B8 -> Reset @0x8272CDA0
//                                            -> EnterStartingUpState @0x82708038
//   PostPhysicsUpdate @0x8274E6D0, state E_STATE_STARTING_UP
//     E_STARTINGUPSTATE_POPULATING
//       RecalculateActiveHulls @0x8274C870        -> the NEW hull set
//       SpawnNewTraffic        @0x82748A40        -> per new hull
//         FillNewHull          @0x82743600        -> per StaticTrafficVehicle record
//           PickVehicleToSpawn @0x827235F8        -> a vehicle TYPE from the flow type
//           StaticVehicles_Generate @0x82722680   -> pop a free StaticTrafficParam slot
//       StaticVehicles_CreateNewVehicles @0x827229F0
//         Vehicle::InitialiseAsStatic @0x827567F0
//         SetVehicleTransform  @0x827142B8        -> the car now has a place in the world
//     E_STARTINGUPSTATE_WAITING_FOR_STREAMING
//       EnterRunningState      @0x827080E8
//
// That ladder runs once, on the POPULATING frame. In steady state PostPhysicsUpdate's RUNNING
// arm dispatches to UpdateDecisionFrame @0x8274E508 or UpdateNonDecisionFrame @0x8274C1A8
// (both in _wT1_06.cpp), and UpdateDecisionFrame re-runs RecalculateActiveHulls,
// SpawnNewTraffic and the StaticVehicles_* updates bodied here.
//
// Every function on the STARTING_UP chain is bodied here. A leg that reaches code or data this
// tree cannot recover is a NAMED one-shot gate, never an invented body or a silent omission.
//
// TWO KNOBS THE LEAK GETS WRONG OR HIDES:
//   * mbDEBUGTurnTrafficOff ships FALSE, not the leak's true. Construct @0x82740220 stores
//     zero at 0x82740D2C (`stbx r30`, r30 == 0, offset 0x7287E), in a run whose neighbours pin
//     the offset with no slack. Ship traffic is on by default.
//   * mfTrafficAmountScale gates the PARKED half too: FillNewHull's first act is
//     `if (mfTrafficAmountScale == 0.0f) return;` @0x82743634. The chain is Construct's
//     mfBaseDensityScale = 1.0f, ResetEventData copying it into mfGameModeDensityScale, and
//     UpdateDensity copying that into mfTrafficAmountScale every frame, so a default-
//     constructed module runs at density 1.0. Never suppress a spawn half by zeroing the
//     density: it also gates the WAITING_FOR_PLAYER and POPULATING early-outs.
//
// Layout is host-native throughout: no member is reached by an X360 byte offset, and the
// console displacements in the comments only attest which member a line resolves to.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                // Pvs (UpdateRaceCarHulls' grid walk)
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull, StaticTrafficVehicle
#include "SharedClasses/Traffic/BrnTrafficFlowType.h"           // FlowType
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // Section (the generator lane walk)
#include "SharedClasses/Traffic/BrnTrafficSectionFlow.h"        // SectionFlow (per-section spawn rate)
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"        // VehicleTypeData / UpdateData
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"       // VehicleAsset

// RCEntityActiveRaceCarOutputInterface -- the player-car snapshot UpdateRaceCarHulls builds
// its sim box around and PostPhysicsUpdate's tail latches meLocalPlayerIndex from.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Algorithms/CgsShuffle.h"        // CgsAlgorithms::Shuffle (Reset pool shuffles)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gpDebugPrint / gxMessageFilterFlags

#include "rw/math/vpu/matrix44affine_operation.h"               // rw::math::vpu::IsValid

#include <cfloat>    // FLT_MAX (the KF_MAX_FLOAT tuning seed)
#include <cmath>     // std::floor, std::cos, std::sin
#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // NAMED LEG GATE. One line per console leg with no body in this tree, logged once per
    // process. [DIAG] NOT IN THE X360 BINARY.
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
                << "[T1-traffic-leg] TrafficEntityModule leg NOT RECONSTRUCTED, skipped: "
                << lpcLegNameAndReason << " [FLAG PC partial gate]\n";
        }
    }

    // DELETE-WHEN-STABLE bring-up probes, gated on BRN_TRAFFIC_DIAG. Every probe below is
    // one-shot or value-latched, so steady state costs one env lookup per call.
    // [DIAG] NOT IN THE X360 BINARY.
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

    // The X360 immediates this file needs that are not rodata reads. Each is an instruction
    // operand, recovered rather than guessed.

    // FillNewHull @0x82743600 / PickVehicleToSpawn @0x827235F8 both draw a 1..100 roll with
    // the same magic-division sequence (`mulhwu r,x,0x51EB851F ; srwi r,r,5 ; mulli r,r,100`
    // -- strength-reduced `% 100`), then `+ 1`. De-optimised back to the modulo it came from.
    const u32 KU_PERCENTAGE_ROLL_MODULUS = 100u;

    // PickVehicleToSpawn @0x82723880..0x8272388C builds a full 64-bit CgsID literal
    //     lis r10, 0x6A16 ; lis r9, -0x40D2 ; ori r9, r9, 0xA6A4 ; insrdi r10, r9, 32, 0
    // == 0xBF2EA6A4'6A160000, and in showtime mode accepts a picked vehicle type ONLY when
    // its asset's CgsID equals that literal (`cmpld` + `li r11,1 / mr r11,r24(0)`).
    // FLAG: the 12-character printable form of this CgsID is not decoded here -- the value is
    // the recovered instruction immediate, and naming the asset would be a guess.
    const u64 KU_SHOWTIME_ONLY_VEHICLE_ASSET_ID = 0xBF2EA6A46A160000ull;

    // StaticTrafficParam::Kill (inlined into StaticVehicles_KillParam @0x82721DC4):
    // `andi. r11, r11, 0x8C ; ori r11, r11, 2`. 0x8C keeps three bits that have no attested
    // enumerator in either the DWARF or the leak (E_FLAG_ALIVE/DYING/SHOULD_BE_REMOVED/
    // ZOMBIE/DIVORCED account for 0x01/0x02/0x10/0x20/0x40 only), so the mask is spelled as
    // the recovered literal rather than as an invented enum expression.
    const u8 KU_STATIC_PARAM_KILL_KEEP_MASK = 0x8Cu;

    // StaticVehicles_RemoveDeadParam @0x827163D0 appends `{ luParam, 5 }` to the static
    // purgatory list. 5 == KU_PURGATORY_TIME_OFFLINE == KU_PURGATORY_TIME_ONLINE
    // (BrnTrafficConstants.h, both already attested at 5), which is why the console can bake
    // one immediate for both cases.
    const u16 KU_STATIC_PURGATORY_DECISION_FRAMES = 5u;

    // FillNewHull's driving half @0x827436B0 flt_820224B0. The random fractional car the
    // section walk starts with, so a hull does not emit its whole row in lockstep.
    const f32 KF_INITIAL_SPAWN_PHASE = 0.99000001f;

    // FillNewHull @0x82743878 flt_820BA8BC. Per-car forward jitter, as a fraction of spacing.
    // unk_8300CC90 == 1600.0f == 40 m squared (dyn-init thunk 0x82C662D0 squares the 40.0f
    // splat at 0x8300CB80). FillNewHull parked-half proximity cull, 0x82743B18.
    const f32 KF_PARKED_PROXIMITY_CULL_RADIUS_SQ = 1600.0f;

    // 0x82722FE8: the per-second bulb-warmth step UpdateEffects @0x82756D48 takes.
    const f32 KF_BULB_WARMTH_RATE = 5000.0f;

    const f32 KF_SPAWN_JITTER_FRACTION = 0.30000001f;

    // FillNewHull @0x82743698 flt_82001C98, the same clamp pair CalcTimeToNextGeneration
    // @0x82721B08 uses: raise the scaled rate to this floor, never above the section's own.
    const f32 KF_MIN_VEHICLES_PER_MINUTE = 1.0f;

    // SpawnNewTraffic's generator half @0x82748?? -- the headway a generator demands behind the
    // section's first param, expressed in seconds of lane speed (`fmuls f0, mfSpeed, 2.0`).
    const f32 KF_GENERATOR_MIN_HEADWAY_SECONDS = 2.0f;

    // Construct's tuning block writes each member as one 16-byte store; these two spell the
    // `vspltw`-then-`stvx128` and the lane-wise forms. [DIAG-FREE] pure de-inlining.
    inline void SetTuningSplat(Vector4& lrOut, f32 lfValue)
    {
        lrOut.x = lfValue;
        lrOut.y = lfValue;
        lrOut.z = lfValue;
        lrOut.w = lfValue;
    }

    inline void SetTuningLanes(Vector4& lrOut, f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        lrOut.x = lfX;
        lrOut.y = lfY;
        lrOut.z = lfZ;
        lrOut.w = lfW;
    }
}

// ============================================================================
// SECTION 1 -- the state machine's leaf transitions.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::EnterStartingUpState  @ 0x82708038
//
// Store for store (0x8270804C..0x827080CC):
//   assert(meState == E_STATE_INVALID)                       ; baked line 1697
//   meState           = E_STATE_STARTING_UP                  ; stw 0, 0x300
//   meStartingUpState = E_STARTINGUPSTATE_FIRST (== 0)       ; stw 0, 0x304
//   mbAllowDivergentBehaviour = !mbIsOnlineGameMode || mbPlayingShowtimeMode
//                                                            ; lbzx 0x717DC / 0x717DD, stbx 0x717E7
//   mpLogger-><byte 0>        = mbAllowDivergentBehaviour    ; lwzx 0x727B4 ; stb r11, 0(r10)
//
// The leak computes AllowDivergentBehaviour() as an inline predicate; the ship caches it in
// the member at +0x717E7 here and ORs in the showtime case. It is the biggest behavioural
// switch on this chain: offline it is TRUE, which is what lets PostPhysicsUpdate's POPULATING
// arm create vehicles locally at all.
// ----------------------------------------------------------------------------
void TrafficEntityModule::EnterStartingUpState()
{
    CGS_ASSERT(meState == E_STATE_INVALID, "meState == E_STATE_INVALID");

    meState           = E_STATE_STARTING_UP;
    meStartingUpState = E_STARTINGUPSTATE_FIRST;

    mbAllowDivergentBehaviour = (!mbIsOnlineGameMode) || mbPlayingShowtimeMode;

    {
        // GATE: the console's last store is a byte written through mpLogger (+0x727B4).
        // BrnTrafficLogger.cpp is unmounted and does not compile, so the Logger type has no
        // usable declaration. It locally redeclares KU_MAX_PARAMS (:32), HullRuntime (:58),
        // ParamTransform (:81) and TrafficEntityModule (:90), all of which have real headers,
        // so it fails C2374/C2086/C2011 first and the C2027s are the cascade.
        // DELETE WHEN: those four local forks are replaced by the real includes and the file
        // is mounted. That also un-gates the other two mpLogger legs here.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "EnterStartingUpState mpLogger-><leading byte> = mbAllowDivergentBehaviour "
            "(X360 0x827080CC) -- BrnTrafficLogger.cpp is unmounted and does not compile. "
            "CAUSE: it LOCALLY REDECLARES KU_MAX_PARAMS, HullRuntime, "
            "ParamTransform and TrafficEntityModule, all of which have real headers, so it "
            "fails C2374/C2086/C2011 first and the C2027s are the cascade. Fix = delete the "
            "four local forks and include the real headers");
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::EnterRunningState  @ 0x827080E8
//
// 0x827080FC..0x82708150, store for store:
//   assert((meState == E_STATE_STARTING_UP) && (meStartingUpState == E_STARTINGUPSTATE_LAST))
//   r11 = meRunningStateToUseAfterStartup   (lwz 0x30C -- READ BEFORE the stores)
//   meState = E_STATE_RUNNING (1)           (stw 0x300)
//   meRunningState = r11                    (stw 0x308)
//   meRunningStateToUseAfterStartup = 0     (stw 0x30C)
//   meStartingUpState = -1                  (stw 0x304)
// ----------------------------------------------------------------------------
void TrafficEntityModule::EnterRunningState()
{
    CGS_ASSERT(meState == E_STATE_STARTING_UP && meStartingUpState == E_STARTINGUPSTATE_LAST,
               "( meState == E_STATE_STARTING_UP ) && ( meStartingUpState == E_STARTINGUPSTATE_LAST )");

    const ERunningState leRunningStateToUse = meRunningStateToUseAfterStartup;

    meState                         = E_STATE_RUNNING;
    meRunningState                  = leRunningStateToUse;
    meRunningStateToUseAfterStartup = E_RUNNINGSTATE_NORMAL;
    meStartingUpState               = E_STARTINGUPSTATE_INVALID;

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-static] the transition that makes the module live. One-shot by construction --
        // EnterRunningState runs once per start-up.  DELETE-WHEN-STABLE.
        *lpDiag << "[T1-static] EnterRunningState: meState -> E_STATE_RUNNING\n";
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::EnterTearingDownState  @ 0x82708168
//
//   assert(meState == E_STATE_RUNNING)                       ; baked line 1746
//   meState           = E_STATE_TEARING_DOWN (2)  ; v1[192]
//   meTearingDownState= E_TEARINGDOWNSTATE_WIPING ; v1[196] == +0x310
//   meRunningState    = E_RUNNINGSTATE_INVALID    ; v1[194] == +0x308
// ----------------------------------------------------------------------------
void TrafficEntityModule::EnterTearingDownState()
{
    CGS_ASSERT(meState == E_STATE_RUNNING, "meState == E_STATE_RUNNING");

    meState            = E_STATE_TEARING_DOWN;
    meTearingDownState = E_TEARINGDOWNSTATE_WIPING;
    meRunningState     = E_RUNNINGSTATE_INVALID;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::IsDecisionFrame  @ 0x827074E0
//
//   meState == E_STATE_STARTING_UP (0)  -> TRUE unconditionally
//   otherwise assert(meState == E_STATE_RUNNING)   ; header baked line 2113
//   return mbDecisionFrame                          ; lbzx +463861
//
// The starting-up short-circuit is why every leg of the POPULATING arm can assert
// IsDecisionFrame() and still run on the very first frame after Reset.
// ----------------------------------------------------------------------------
bool TrafficEntityModule::IsDecisionFrame()
{
    if (meState == E_STATE_STARTING_UP)
    {
        return true;
    }

    CGS_ASSERT(meState == E_STATE_RUNNING, "meState == E_STATE_RUNNING");
    return mbDecisionFrame;
}

// ============================================================================
// SECTION 2 -- the console-inlined lookups the spawn ladder uses.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::GetHull  @ 0x8271D8B0   (header baked line 2229)
//   assert(luIndex < mpData->muNumHulls);  return mpData->mpapHulls[luIndex];
// The console reads muNumHulls at TrafficData +0x02 and mpapHulls at +0x0C; both are named
// members of the committed TrafficData, so the host form is a plain index.
// ----------------------------------------------------------------------------
const Hull* TrafficEntityModule::GetHull(u32 luIndex) const
{
    CGS_ASSERT(luIndex < mpData->muNumHulls, "luIndex < (mpData->muNumHulls)");
    return mpData->mpapHulls[luIndex];
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::GetHullRuntime  @ 0x8271D9D0   (header baked lines 2267 / 2270)
//   assert(luHull < mpData->muNumHulls);
//   luRuntime = mauHullRuntimeDataIndices[luHull];
//   assert(luRuntime != KU_INVALID_HULL_RUNTIME);
//   return &maHullRuntimeData[luRuntime];      ; 1176 * luRuntime + this + 257216
// ----------------------------------------------------------------------------
HullRuntime* TrafficEntityModule::GetHullRuntime(u32 luHull)
{
    CGS_ASSERT(luHull < mpData->muNumHulls, "luHull < (mpData->muNumHulls)");

    const u8 luHullRuntime = mauHullRuntimeDataIndices[luHull];
    CGS_ASSERT(luHullRuntime != KU_INVALID_HULL_RUNTIME, "luHullRuntime != KU_INVALID_HULL_RUNTIME");

    return &maHullRuntimeData[luHullRuntime];
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::GetHullRuntimeSafe  @ 0x8271DA70   (header baked line 2288)
// Same lookup, but an unallocated slot yields NULL instead of firing the assert.
// ----------------------------------------------------------------------------
HullRuntime* TrafficEntityModule::GetHullRuntimeSafe(u32 luHull)
{
    CGS_ASSERT(luHull < mpData->muNumHulls, "luHull < (mpData->muNumHulls)");

    const u8 luHullRuntime = mauHullRuntimeDataIndices[luHull];
    if (luHullRuntime == KU_INVALID_HULL_RUNTIME)
    {
        return 0;
    }
    return &maHullRuntimeData[luHullRuntime];
}

// The const overloads (DWARF :2270 / :2274). Same lookup.
const HullRuntime* TrafficEntityModule::GetHullRuntime(u32 luHull) const
{
    return const_cast<TrafficEntityModule*>(this)->GetHullRuntime(luHull);
}

const HullRuntime* TrafficEntityModule::GetHullRuntimeSafe(u32 luHull) const
{
    return const_cast<TrafficEntityModule*>(this)->GetHullRuntimeSafe(luHull);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::SetVehicleTransform  @ 0x827142B8   (header baked line 2491, .cpp 2492)
//
//   assert(luIndex < KU_MAX_TOTAL_TRAFFIC);
//   assert(RwMath::IsValid(lTransform));
//   maVehicleTransforms[luIndex] = lTransform;      ; (luIndex + 0x7B2) << 6, four stvx128
//
// The console spells IsValid as a per-lane `vcmpeqfp` self-equality cascade over the x/y/z
// lanes of all four rows, ANDed together, which is what rw::math::vpu::IsValid(Matrix44Affine)
// reduces to. 0x7B2 * 64 == 126080 is the end of maVehicleAxles (87680 + 600*64), so the
// transform array immediately follows it.
// ----------------------------------------------------------------------------
void TrafficEntityModule::SetVehicleTransform(u32 luIndex, const Matrix44Affine& lTransform)
{
    CGS_ASSERT(luIndex < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
    CGS_ASSERT(rw::math::vpu::IsValid(lTransform), "RwMath::IsValid( lTransform )");

    maVehicleTransforms[luIndex] = lTransform;
}

// ============================================================================
// SECTION 3 -- density.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::UpdateDensity  @ 0x82716318
//
//   mfTrafficAmountScale = mfGameModeDensityScale;                 ; +464924 <- +464916
//   if (!mbAllowDivergentBehaviour && mfGameModeDensityScale > 0)  ; lbzx +464871
//       mfTrafficAmountScale =
//           lerp(flt_82F2FDE0, flt_82F2FDDC, (mActiveHulls.GetLength() - 9) * 0.015873017);
//
// The first line is what the parked chain depends on. The online arm is gated: its two
// endpoints are un-dumped .data floats at 0x82F2FDDC / 0x82F2FDE0 with no writer in the export
// set, so writing the arm would mean inventing the density curve. It is dead offline anyway,
// since mbAllowDivergentBehaviour is true whenever !mbIsOnlineGameMode. The 0.015873017f
// (== 1/63) is an instruction immediate, so only the two floats are missing.
// ----------------------------------------------------------------------------
void TrafficEntityModule::UpdateDensity()
{
    const f32 lfGameModeDensityScale = mfGameModeDensityScale;
    mfTrafficAmountScale = lfGameModeDensityScale;

    if (!mbAllowDivergentBehaviour && lfGameModeDensityScale > 0.0f)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "UpdateDensity ONLINE arm -- mfTrafficAmountScale = lerp(flt_82F2FDE0, "
            "flt_82F2FDDC, (mActiveHulls.GetLength() - 9) * (1/63)) : both endpoints are "
            "un-dumped X360 .data floats (0x82F2FDDC / 0x82F2FDE0); unreachable offline "
            "because mbAllowDivergentBehaviour is true whenever !mbIsOnlineGameMode");
    }
}

// ============================================================================
// SECTION 4 -- the static (parked) vehicle sub-system.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_UpdatePurgatory  @ 0x827228A8
//
// Walks mStaticParamPurgatoryList (Array<PurgatoryInfo,199> @ +255500, live count @ +256296)
// and ticks each record's countdown down; a record that reaches zero hands its slot back to
// mFreeStaticParamStack (Stack<u8,199> @ +256300) and is erased.
//   assert(!maStaticTrafficParams[lInfo.muIndex].IsAlive())   ; .cpp 8790
//   assert(lInfo.muIndex <= 0xff)                             ; .cpp 8791
// The console decrements IN PLACE (`*(result+1) = v5`), so the stored countdown really is
// mutated, not just tested -- reproduced by taking the element by reference.
// The `--v2` before `++v2` around Erase is Hex-Rays' spelling of "re-test this index after
// an unordered erase", de-optimised back into the loop's own index handling.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_UpdatePurgatory()
{
    u32 luIndex = 0;
    while (luIndex < mStaticParamPurgatoryList.GetLength())
    {
        PurgatoryInfo& lrInfo = mStaticParamPurgatoryList.GetItem(luIndex);

        --lrInfo.muDecisionFramesLeft;
        if (lrInfo.muDecisionFramesLeft != 0)
        {
            ++luIndex;
            continue;
        }

        CGS_ASSERT(!maStaticTrafficParams[lrInfo.muIndex].IsAlive(),
                   "!maStaticTrafficParams[lInfo.muIndex].IsAlive()");
        CGS_ASSERT(lrInfo.muIndex <= 0xFFu, "lInfo.muIndex <= 0xff");

        const u8 luFreedSlot = static_cast<u8>(lrInfo.muIndex);
        mFreeStaticParamStack.Push(luFreedSlot);

        mStaticParamPurgatoryList.Erase(luIndex);
        // no ++ -- Array::Erase shifts the tail down, so slot `luIndex` now holds a record
        // that has not been ticked yet. This is the console's own `Erase(v3, v2--); ++v2;`
        // (post-decrement then re-increment == "leave the cursor where it is"), de-optimised.
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_RemoveDeadParam  @ 0x827163D0   (.cpp 9739)
//
//   assert((meState == E_STATE_TEARING_DOWN) || IsDecisionFrame());
//   lpParam = GetStaticTrafficParam(luParam);
//   if (lpParam->IsAlive())  return;                       ; (*(p+3) & 1) == 0 gate
//   if (!lpParam->IsDying()) return;                       ; (*(p+3) & 2) != 0 gate
//   if (mbAllowDivergentBehaviour
//       && (vehicle->IsAlive() || vehicle->HasEntity() || vehicle->IsCollidable()))
//       return;                                             ; the three &1 / &2 / &4 reads
//   lpParam->ClearDying();
//   mStaticParamPurgatoryList.Append({ luParam, KU_PURGATORY_TIME });
//
// The three vehicle flag bits are read at Vehicle +5 (mxFlags) as E_FLAG_ALIVE / _HASENTITY
// / _COLLIDABLE -- i.e. "the param may only leave the world once its vehicle has fully let
// go of its scene entity and its collidable registration".
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_RemoveDeadParam(u32 luParam)
{
    CGS_ASSERT(meState == E_STATE_TEARING_DOWN || IsDecisionFrame(),
               "( meState == E_STATE_TEARING_DOWN ) || IsDecisionFrame()");

    StaticTrafficParam* lpParam = GetStaticTrafficParam(luParam);
    if (lpParam->IsAlive())
    {
        return;
    }
    if (!lpParam->IsDying())
    {
        return;
    }

    if (mbAllowDivergentBehaviour)
    {
        const Vehicle* lpVehicle = GetStaticVehicle(luParam);
        const u8 lxFlags = lpVehicle->GetFlags();
        if ((lxFlags & Vehicle::E_FLAG_ALIVE) != 0
            || (lxFlags & Vehicle::E_FLAG_HASENTITY) != 0
            || (lxFlags & Vehicle::E_FLAG_COLLIDABLE) != 0)
        {
            return;
        }
    }

    GetStaticTrafficParam(luParam)->ClearDying();

    PurgatoryInfo lInfo;
    lInfo.muIndex              = static_cast<u16>(luParam);
    lInfo.muDecisionFramesLeft = KU_STATIC_PURGATORY_DECISION_FRAMES;
    mStaticParamPurgatoryList.Append(lInfo);
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_UpdateStaticParams  @ 0x82722F28
//
//   StaticVehicles_UpdatePurgatory();
//   for (luParam = 0; luParam < KU_MAX_STATIC_TRAFFIC; ++luParam)
//       if (param.IsAlive() && param.ShouldBeRemoved())  StaticVehicles_KillParam(luParam);
//       else                                             StaticVehicles_RemoveDeadParam(luParam);
//
// The console walks `this + 254307` (== &maStaticTrafficParams[0].mxFlags) at a 6-byte
// stride and tests bits 0x01 / 0x10, which are E_FLAG_ALIVE / E_FLAG_SHOULD_BE_REMOVED.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_UpdateStaticParams()
{
    StaticVehicles_UpdatePurgatory();

    for (u32 luParam = 0; luParam < KU_MAX_STATIC_TRAFFIC; ++luParam)
    {
        const StaticTrafficParam& lrParam = maStaticTrafficParams[luParam];
        if (lrParam.IsAlive() && lrParam.ShouldBeRemoved())
        {
            StaticVehicles_KillParam(luParam);
        }
        else
        {
            StaticVehicles_RemoveDeadParam(luParam);
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_KillParam  @ 0x82721C50   (.cpp 7856..7887)
//
// Asserts (in the console's order), then kills the param and, when the param was neither a
// zombie nor divorced, kills the vehicle with it.
//
// FAITHFUL READ-AFTER-WRITE. The console loads mxFlags, applies the kill mask, stores it, and
// only then tests bit 0x40 for "divorced" (0x82721DB8..0x82721DD0), i.e. on the POST-KILL
// value. The kill mask clears 0x40, so that test can never be true and both divorced arms are
// unreachable in the shipped binary. Kept as-is: the zombie flag is captured before the kill
// (`extrwi r26,r11,1,26` @0x82721D5C), so only the divorced arms are dead, and making them
// live would add behaviour the binary does not have.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_KillParam(u32 luParam)
{
    CGS_ASSERT(luParam < KU_MAX_STATIC_TRAFFIC, "luParam < KU_MAX_STATIC_TRAFFIC");
    CGS_ASSERT(GetVehicleSpecies(luParam) == Vehicle::E_SPECIES_STANDARD,
               "GetVehicleSpecies( luParam ) == Vehicle::E_SPECIES_STANDARD");
    CGS_ASSERT(GetStaticVehicle(luParam)->IsAlive() || GetStaticTrafficParam(luParam)->IsZombie(),
               "GetStaticVehicle( luParam )->IsAlive() || GetStaticTrafficParam( luParam )->IsZombie()");

    StaticTrafficParam* lpParam = GetStaticTrafficParam(luParam);
    const bool lbWasZombie = lpParam->IsZombie();

    Vehicle* lpVehicle = GetStaticVehicle(luParam);

    CGS_ASSERT(lpParam->IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:170

    // StaticTrafficParam::Kill, inlined by the console.
    lpParam->mxFlags = static_cast<u8>((lpParam->mxFlags & KU_STATIC_PARAM_KILL_KEEP_MASK)
                                       | StaticTrafficParam::E_FLAG_DYING);

    const bool lbDivorced = (lpParam->mxFlags & StaticTrafficParam::E_FLAG_DIVORCED) != 0;

    if (lbWasZombie)
    {
        if (!lbDivorced)
        {
            CGS_ASSERT(!lpVehicle->IsAlive(), "Static vehicle was alive when its param was a zombie");
        }
        else
        {
            CGS_ASSERT(!lpVehicle->IsAlive() || (lpVehicle->GetFlags() & Vehicle::E_FLAG_ORPHAN) != 0,
                       "Static vehicle wasn't an orphan when the param was divorced");
        }
    }
    else if (!lbDivorced)
    {
        lpVehicle->SetDead(GetVehicleIndexFromStaticIndex(luParam), mVehicleSoaData);

        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "StaticVehicles_KillParam leg EnsureVehicleRemovedFromCrashModule("
            "GetVehicleIndexFromStaticIndex(luParam)) -- no body in tree; it drains the "
            "crash-module registration mVehiclesAddedToCrashModule tracks");
    }
    else
    {
        CGS_ASSERT(!lpVehicle->IsAlive() || (lpVehicle->GetFlags() & Vehicle::E_FLAG_ORPHAN) != 0,
                   "Static param was divorced but its vehicle wasn't orphaned");
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_Generate  @ 0x82722680   (.cpp 8748 / 8764)
//
//   if (mFreeStaticParamStack.IsEmpty())  return;         ; the `if (v5[50])` length test
//   luSlot = mFreeStaticParamStack.Peek(); Pop();
//   assert(!maStaticTrafficParams[luSlot].IsAlive())      ; "Static param N was still alive..."
//   maStaticTrafficParams[luSlot].Initialise(luVehicleType, luHull, luIndexOnHull);
//   if (!mbAllowDivergentBehaviour && GetStaticVehicle(luSlot)->IsAlive())
//   {   // ONLINE: the vehicle outlived its param -- divorce the pair instead of asserting
//       param.SetZombie(); param.SetDivorced();
//   }
//   else assert(!GetStaticVehicle(luSlot)->IsAlive())     ; "Static vehicle N was still alive..."
//
// (The console's `if (X || !alive) {assert-arm} else {divorce-arm}` is the same predicate
// written the other way round; de-Morganed here into the source form it came from.)
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_Generate(u8 luVehicleType, u16 luHull, u8 luIndexOnHull)
{
    if (mFreeStaticParamStack.GetLength() == 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T1-static] one-shot: the pool ran dry. With KillOutOfAreaTraffic gated (see
            // _wT1_06.cpp) this is the expected long-drive end state, and it is the line that
            // explains "parked cars stopped appearing". DELETE-WHEN-STABLE.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                *lpDiag << "[T1-static] mFreeStaticParamStack EXHAUSTED (199 slots all in "
                           "use) -- no more parked params until one is retired; "
                           "KillOutOfAreaTraffic is gated, so none will be\n";
            }
        }
        return;
    }

    const u32 luSlot = mFreeStaticParamStack.Peek();
    mFreeStaticParamStack.Pop();

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-static] the generate roll: the first ten slots taken, then every 25th. This is
        // where FillNewHull's mExistsAtAllChance roll became a real param, and it names the
        // slot, vehicle type, hull and index-on-hull InitialiseAsStatic will use next.
        // DELETE-WHEN-STABLE.
        static u32 suGenerated = 0;
        ++suGenerated;
        if (suGenerated <= 10u || (suGenerated % 25u) == 0u)
        {
            *lpDiag << "[T1-static] Generate #" << static_cast<s32>(suGenerated)
                    << " slot=" << static_cast<s32>(luSlot)
                    << " type=" << static_cast<s32>(luVehicleType)
                    << " hull=" << static_cast<s32>(luHull)
                    << " indexOnHull=" << static_cast<s32>(luIndexOnHull)
                    << " freeLeft=" << static_cast<s32>(mFreeStaticParamStack.GetLength())
                    << "\n";
        }
    }

    CGS_ASSERT(!maStaticTrafficParams[luSlot].IsAlive(),
               "Static param was still alive when we tried to regenerate it");

    maStaticTrafficParams[luSlot].Initialise(luVehicleType, luHull, luIndexOnHull);

    if (!mbAllowDivergentBehaviour && GetStaticVehicle(luSlot)->IsAlive())
    {
        maStaticTrafficParams[luSlot].SetZombie();
        maStaticTrafficParams[luSlot].SetDivorced();
    }
    else
    {
        CGS_ASSERT(!GetStaticVehicle(luSlot)->IsAlive(),
                   "Static vehicle was still alive when its param was reallocated");
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_CreateNewVehicles  @ 0x827229F0
// The parked-car maker. Feb-2007 calls it StaticVehicles_MakeAliveTheDeadOnesWithAliveParams
// and has no race-car proximity rejection; the ship renamed it and added that rejection, and
// the shape below is the ship's.
//
// ARGUMENT MAP FROM THE PROLOGUE, NOT HEX-RAYS (0x82722E50..0x82722ED8). Hex-Rays renders the
// 7th argument as literal 0; the asm builds `&mpaVehicleTypesUpdate[type]` (TrafficData +0x30,
// element stride 20), a real pointer to the VehicleTypeUpdateData whose mfWheelRadius
// VehicleAxles::SetFromVehicleTransform reads. Passing the zero null-dereferences on the first
// parked car. The register that looks like the 4th argument (r6) is the PPC float-arg GPR skip
// slot for f1.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_CreateNewVehicles(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    (void)lpInput;   // only read by the GATED race-car proximity arm below

    if (mbWaitingForStreaming)
    {
        return;
    }

    const bool lbRejectNearPlayers =
        mbDontCreateStaticVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour;

    if (lbRejectNearPlayers)
    {
        // GATE (1 of 2 in this function): the race-car proximity rejection. Parked by choice,
        // not by a blocker. Its guard is offline-dead, since EnterStartingUpState sets
        // mbAllowDivergentBehaviour = !mbIsOnlineGameMode || mbPlayingShowtimeMode, and its
        // collection half walks every active race car, i.e. rival slots this build never fills.
        // Both pieces it needs are already recovered for whoever writes online traffic: the
        // getter is DWARF :358 (X360 0x82711850) and the radius is unk_8300CF70 lane 1 ==
        // 900.0f == 30 m squared (lane 0 is 6400 == 80^2).
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "StaticVehicles_CreateNewVehicles race-car proximity rejection (mbDontCreate"
            "StaticVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour) -- ONLINE-ONLY and "
            "UNREACHABLE offline, so kept parked by choice, not by blocker: the getter "
            "(DWARF :358) and the radius (unk_8300CF70 lane 1 == 900.0f == 30m^2) are both "
            "available now");
    }

    u32 luCreated = 0;

    for (u32 luStatic = 0; luStatic < KU_MAX_STATIC_TRAFFIC; ++luStatic)
    {
        StaticTrafficParam& lrParam = maStaticTrafficParams[luStatic];
        Vehicle*            lpVehicle = &maVehicles[KU_STATIC_TRAFFIC_OFFSET + luStatic];

        if (!lrParam.IsAlive() || lrParam.IsZombie() || lpVehicle->IsAlive())
        {
            continue;
        }

        CGS_ASSERT(lrParam.IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:121
        const VehicleTypeRuntime* lpVehicleTypeRuntime = GetVehicleTypeRuntime(lrParam.muVehicleType);

        CGS_ASSERT(lrParam.IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:114
        const u8 luIndexInHull = lrParam.GetIndexInHull();

        CGS_ASSERT(lrParam.IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:107
        const Hull* lpHull = GetHull(lrParam.GetHull());

        const StaticTrafficVehicle* lpRecord = lpHull->GetStaticVehicle(luIndexInHull);

        // THE ONE-METRE DROP. The authored StaticTrafficVehicle records sit deliberately one
        // metre high: across the 583 shipped records with a WORLDCOL surface beneath them,
        // recordY - groundY is +1.026 m median (p25 +1.014, p75 +1.049). The car model's origin
        // is its wheel-contact plane, so the record must come down by one unit of its OWN up
        // axis before it becomes a render transform, and this line is what does it. Drop it and
        // every parked car hangs in the air.
        //
        // The console does it at 0x82722CD4..0x82722D20: `vsubfp v0, v13, v0` with v13 the
        // wAxis row and v0 still holding the yAxis row, storing the difference over the raw
        // wAxis. The yAxis is a unit up vector, which is why the correction is one metre and why
        // it follows the car's tilt on a banked road rather than being a world-Y constant.
        //
        // The gated proximity leg below differences THIS transform's position, not the raw
        // record's (the console reads back var_180 at 0x82722D90), so the drop belongs here.
        Matrix44Affine lTransform = lpRecord->mTransform;
        lTransform.wAxis = lTransform.wAxis - lTransform.yAxis;

        if (lbRejectNearPlayers)
        {
            // Second half of the same gate: the reject itself (SetZombie + SetDivorced).
            continue;
        }

        const u32 luVehicle = luStatic + KU_STATIC_TRAFFIC_OFFSET;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        CGS_ASSERT(lrParam.IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:121

        const u8  luVehicleType = lrParam.muVehicleType;

        // DO NOT ADD A `- 1.0f` HERE. The console's expansion at 0x82722E7C..0x82722ED4 (read
        // the [1,2) ring slot, refill it, step the LCG, then `fsubs f1, f0, f31` with f31 ==
        // 1.0f) IS CgsNumeric::Random::RandomFloat(), whose committed body already ends on that
        // subtraction and returns [0,1). Subtracting again hands InitialiseAsStatic a negative
        // mfRandomVal, which nothing asserts on: consumers treat it as a 0..1 phase, so a
        // [-1,0) value silently inverts wheel rotation and headlight phase on every parked car.
        const f32 lfRandomVal   = mEffectRand.RandomFloat();

        Matrix44Affine lOutMatrix;
        lpVehicle->InitialiseAsStatic(&maVehicleAxles[luVehicle],
                                      lOutMatrix,
                                      lfRandomVal,
                                      luVehicleType,
                                      lpVehicleTypeRuntime,
                                      &mpData->mpaVehicleTypesUpdate[luVehicleType],
                                      lTransform,
                                      luVehicle,
                                      mVehicleSoaData);

        SetVehicleTransform(luVehicle, lOutMatrix);
        ++luCreated;

        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T1-static] one-shot on the first InitialiseAsStatic, with the world position
            // seated (what CreateNewVehicleEntities hands AddEntity next frame). (0,0,0) means
            // the record transform never arrived; a plausible position with nothing on screen
            // means the fault is downstream of the module. DELETE-WHEN-STABLE.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                const Vector3& lrPos = lOutMatrix.wAxis;
                *lpDiag << "[T1-static] FIRST InitialiseAsStatic vehicle=" << static_cast<s32>(luVehicle)
                        << " (staticSlot=" << static_cast<s32>(luStatic) << ")"
                        << " type=" << static_cast<s32>(luVehicleType)
                        << " pos=(" << lrPos.x << ", " << lrPos.y << ", " << lrPos.z << ")\n";

                // [T1-height] both ends of the one-metre drop, so one line shows whether
                // 0x82722D14's `vsubfp wAxis, yAxis` is being applied: recordY - droppedY must
                // equal the record's up-vector length. DELETE-WHEN-STABLE.
                *lpDiag << "[T1-height] record pos=(" << lpRecord->mTransform.wAxis.x
                        << ", " << lpRecord->mTransform.wAxis.y
                        << ", " << lpRecord->mTransform.wAxis.z << ")"
                        << " up=(" << lpRecord->mTransform.yAxis.x
                        << ", " << lpRecord->mTransform.yAxis.y
                        << ", " << lpRecord->mTransform.yAxis.z << ")"
                        << " -> dropped Y=" << lrPos.y
                        << " (drop=" << (lpRecord->mTransform.wAxis.y - lrPos.y) << ")\n";
            }
        }
    }

    mbDontCreateStaticVehiclesNearAnyPlayers = false;

    if (luCreated != 0)
    {
        if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
        {
            // [T1-static] value-latched: only prints on a frame that actually made cars.
            // DELETE-WHEN-STABLE.
            u32 luAliveParams = 0;
            u32 luAliveVehicles = 0;
            for (u32 luStatic = 0; luStatic < KU_MAX_STATIC_TRAFFIC; ++luStatic)
            {
                if (maStaticTrafficParams[luStatic].IsAlive())                       ++luAliveParams;
                if (maVehicles[KU_STATIC_TRAFFIC_OFFSET + luStatic].IsAlive())       ++luAliveVehicles;
            }
            *lpDiag << "[T1-static] created=" << static_cast<s32>(luCreated)
                    << " aliveParams=" << static_cast<s32>(luAliveParams)
                    << " aliveVehicles=" << static_cast<s32>(luAliveVehicles) << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::StaticVehicles_UpdateVehicles  @ 0x82722F98
//
//   StaticVehicles_CreateNewVehicles(lpInput);
//   for (luVehicle = KU_STATIC_TRAFFIC_OFFSET; luVehicle < KU_TRAILER_TRAFFIC_OFFSET; ++luVehicle)
//       if (mVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle)
//           && mVehicleSoaData.<4th set>.IsBitSet(luVehicle))
//           vehicle->UpdateEffects(..., (s32)(mfSimTimeStep * 5000.0f), &mEffectRand, mfSimTimeStep);
//
// The loop bounds are the console's literals: it starts at 400 and runs while `< 0x257`
// (== 599 == KU_TRAILER_TRAFFIC_OFFSET), i.e. the 199 static slots exactly.
//
// SIGNATURE: it takes lpInput and forwards it, despite IDA typing @0x82722F98 as
// one-argument. That is a Hex-Rays artefact of a pass-through: the prologue saves only r3 and
// never writes r4, so whatever r4 held on entry flows into StaticVehicles_CreateNewVehicles'
// second parameter. UpdateDecisionFrame @0x8274E508 does `mr r4, r30` (r30 == lpInput) right
// before the `bl`, and the DWARF spells it out at BrnTrafficEntityModule.h:1839. Passing a
// literal 0 here is inert only while the race-car proximity arm stays gated.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_UpdateVehicles(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    StaticVehicles_CreateNewVehicles(lpInput);

    // The bulb-warmth delta is hoisted out of the loop by the console (0x82722FE8).
    const s32 liBulbWarmthDelta = static_cast<s32>(mfSimTimeStep * KF_BULB_WARMTH_RATE);

    // The second set is mVehicleSoaData + 240 == mPhysicalVehicles (mVehicleSoaData is at
    // module +164560 and each FastBitArray<601> is 80 bytes).
    for (u32 luVehicle = KU_STATIC_TRAFFIC_OFFSET;
         luVehicle < KU_TRAILER_TRAFFIC_OFFSET;
         ++luVehicle)
    {
        if (!mVehicleSoaData.mAliveVehicles.IsBitSet(luVehicle)
            || !mVehicleSoaData.mPhysicalVehicles.IsBitSet(luVehicle))
        {
            continue;
        }

        GetVehicle(luVehicle)->UpdateEffects(mfSimTimeStep, liBulbWarmthDelta, &mEffectRand);
    }
}

// ============================================================================
// SECTION 5 -- picking what to spawn.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::PickVehicleToSpawn  @ 0x827235F8   (.cpp 9152 / 9163)
//
//   assert(luFlowTypeId < mpData->muNumFlowTypes);
//   if (miDEBUGFlowtypeOverride >= 0 && (u32)miDEBUGFlowtypeOverride < mpData->muNumFlowTypes)
//       luFlowTypeId = miDEBUGFlowtypeOverride;
//   lpFlowType = mpData->mpapFlowTypes[luFlowTypeId];
//   assert(lpFlowType->muNumVehicleTypes > 0);
//   luRoll = (u8)(mRand.RandomUInt());            ; srdi r11,seed,32 ; clrlwi r29, r11, 24
//   for (i = 0; i < lpFlowType->muNumVehicleTypes; ++i)
//       if (luRoll <= lpFlowType->mpauCumulativeProb[i]) -> PICK i
//   if (nothing picked) { assert("Invalid flow type <id>"); return 0; }   ; li r3,0 -- a
//                                                                        ; LITERAL 0, not
//                                                                        ; mpauVehicleTypeIds[0]
//   luType = (u8)lpFlowType->mpauVehicleTypeIds[i];
//   lpType = &mpData->mpaVehicleTypes[luType];
//   ok = true;
//   if (lpType->muVehicleClass == E_VEHICLECLASS_BUS || == E_VEHICLECLASS_BIGRIG)
//       if ((mRand.RandomUInt() % 100 + 1) > miBigVehicleAmount)  ok = false;
//   if (mbPlayingShowtimeMode)
//       ok = (mpData->mpaVehicleAssets[lpType->muAssetId].GetVehicleId()
//             == KU_SHOWTIME_ONLY_VEHICLE_ASSET_ID);
//   return ok ? luType : (u8)lpFlowType->mpauVehicleTypeIds[0];
//
// The showtime comparand is a full 64-bit load (`ldx r11, mpaVehicleAssets, assetId*8` then
// `cmpld`), not "the dword at +4"; that reading is a big-endian artefact, since on this host
// the low half of a u64 lives at +0. Done by value through VehicleAsset::GetVehicleId(), which
// is width- and endian-correct on both.
//
// The 8-bit roll is attested: `clrlwi r29, r11, 24` truncates the LCG's high word to a byte,
// which is what makes it comparable against the 0..255 cumulative-probability table.
// ----------------------------------------------------------------------------
u8 TrafficEntityModule::PickVehicleToSpawn(u32 luFlowTypeId)
{
    CGS_ASSERT(luFlowTypeId < mpData->muNumFlowTypes, "luFlowTypeId < mpData->muNumFlowTypes");

    if (miDEBUGFlowtypeOverride >= 0
        && static_cast<u32>(miDEBUGFlowtypeOverride) < mpData->muNumFlowTypes)
    {
        luFlowTypeId = static_cast<u32>(miDEBUGFlowtypeOverride);
    }

    const FlowType* lpFlowType = mpData->mpapFlowTypes[luFlowTypeId];
    CGS_ASSERT(lpFlowType->muNumVehicleTypes > 0, "lpFlowType->muNumVehicleTypes > 0");

    const u8 luRoll = static_cast<u8>(mRand.RandomUInt());

    u32  luEntry = 0;
    bool lbFound = false;
    for (luEntry = 0; luEntry < lpFlowType->muNumVehicleTypes; ++luEntry)
    {
        if (luRoll <= lpFlowType->mpauCumulativeProb[luEntry])
        {
            lbFound = true;
            break;
        }
    }

    if (!lbFound)
    {
        // TWO DISTINCT CONSOLE FALLBACKS, do not collapse them. This one (LABEL_12, assert
        // .cpp 9209) returns a LITERAL zero at 0x82723A38 and never touches the type-id table.
        // The `mpauVehicleTypeIds[0]` fallback is the other path, the not-acceptable tail at
        // 0x827238CC. Returning it here would dereference a table the console avoids: LABEL_12
        // is reached exactly when muNumVehicleTypes == 0 and the array may be empty.
        CGS_ASSERT(false, "Invalid flow type");
        return 0;
    }

    const u8 luVehicleType = static_cast<u8>(lpFlowType->mpauVehicleTypeIds[luEntry]);
    const VehicleTypeData* lpType = &mpData->mpaVehicleTypes[luVehicleType];

    bool lbAcceptable = true;

    if (lpType->muVehicleClass == E_VEHICLECLASS_BUS
        || lpType->muVehicleClass == E_VEHICLECLASS_BIGRIG)
    {
        const u32 luBigVehicleRoll = (mRand.RandomUInt() % KU_PERCENTAGE_ROLL_MODULUS) + 1u;
        if (static_cast<s32>(luBigVehicleRoll) > miBigVehicleAmount)
        {
            lbAcceptable = false;
        }
    }

    if (mbPlayingShowtimeMode)
    {
        lbAcceptable = (mpData->mpaVehicleAssets[lpType->muAssetId].GetVehicleId()
                        == KU_SHOWTIME_ONLY_VEHICLE_ASSET_ID);
    }

    if (lbAcceptable)
    {
        return luVehicleType;
    }
    return static_cast<u8>(lpFlowType->mpauVehicleTypeIds[0]);
}

// ============================================================================
// SECTION 6 -- filling a hull.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::FillNewHull  @ 0x82743600
//
//   lpHull = GetHull(luHull);
//   if (mfTrafficAmountScale == 0.0f) return;        ; 0x82743634 vs flt_82001CC0 == 0.0f
//   ---- DRIVING HALF (REAL, 0x82743640..0x827439F8) : per section, lay cars at even
//        distance spacing and carry the leftover fractional car to the next section ----
//   ---- PARKED HALF (REAL, 0x82743A74..0x82743B60) ----
//   for (i = 0; i < lpHull->muNumStaticTraffic; ++i)
//   {
//       lpRec = &lpHull->mpaStaticTrafficVehicles[i];          ; base + 80*i
//       if ((mRand.RandomUInt() % 100 + 1) > lpRec->mExistsAtAllChance)   continue;  ; +0x42
//       if ((lpRec->muFlags & 2) != 0 && !mbPlayingShowtimeMode)          continue;  ; +0x43
//       if (!mbAllowDivergentBehaviour) { if (lpRec->muFlags & 1) continue; }
//       else { ...proximity cull vs *(this + 0x728C0)... }
//       StaticVehicles_Generate(PickVehicleToSpawn(lpRec->mFlowTypeID), luHull, i);  ; +0x40
//   }
//
// The `muFlags` byte at +0x43 is ship-only (it postdates the Feb-2007 record) and ARTIST tests
// only these two bits, so no enumerator is invented for them.
// ----------------------------------------------------------------------------
void TrafficEntityModule::FillNewHull(u16 luHull)
{
    const Hull* lpHull = GetHull(luHull);

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-fill] first visit per hull, keyed by a 400-bit seen-set so a hull entered, left
        // and re-entered prints once. It says the spawn chain reached a real hull; a
        // muNumStaticTraffic of 0 means that hull has no parked slots in the data.
        // DELETE-WHEN-STABLE. [DIAG] NOT IN THE X360 BINARY.
        static CgsContainers::BitArray<KU_MAX_HULLS> sSeenHulls;
        if (luHull < KU_MAX_HULLS && !sSeenHulls.IsBitSet(luHull))
        {
            sSeenHulls.SetBit(luHull);
            *lpDiag << "[T1-fill] FillNewHull first visit hull=" << static_cast<s32>(luHull)
                    << " staticRecords=" << static_cast<s32>(lpHull->muNumStaticTraffic)
                    << " density=" << mfTrafficAmountScale
                    << " freeStaticParams=" << static_cast<s32>(mFreeStaticParamStack.GetLength())
                    << "\n";
        }
    }

    if (mfTrafficAmountScale == 0.0f)
    {
        return;
    }

    // ---- DRIVING HALF, 0x82743640..0x827439F8 --------------------------------------------
    // The section loop lays cars along each lane at even DISTANCE spacing (the leak walks in
    // param units and Modulos; the ship walks in metres through
    // Section::CalcParamFromStartParamAndDistanceAlongSection) and carries the leftover
    // fractional car from one section to the next. Two ship-only additions over Feb-2007:
    // the initial phase is random, and each car gets a jitter of up to 0.3 spacings.
    f32 lfVehiclesToSpawn = mRand.RandomFloat(0.0f, KF_INITIAL_SPAWN_PHASE);

    for (u32 luSection = 0; luSection < lpHull->muNumSections; ++luSection)
    {
        const Section*     lpSection = lpHull->GetSection(luSection);
        const SectionFlow* lpFlow    = &lpHull->mpaSectionFlows[luSection];  // Hull::GetFlowData, inlined

        if (lpFlow->muVehiclesPerMinute == 0)
        {
            continue;
        }

        CGS_ASSERT(lpSection->muNumRungs > 0, "muNumRungs > 0");   // 0x82743784, GetNumSegments inlined

        const f32 lfTimeToDrive = lpSection->mfLength / lpSection->mfSpeed;

        // 0x827437E8 / 0x827437F0, the same two fsels CalcTimeToNextGeneration @0x82721B08
        // uses: raise the scaled rate to the floor, never above the section's own rate.
        f32 lfVehiclesPerMinute = 0.0f;
        if (lpFlow->muVehiclesPerMinute != 0 && mfTrafficAmountScale > 0.0f)
        {
            const f32 lfSectionRate = static_cast<f32>(lpFlow->muVehiclesPerMinute);
            const f32 lfFloor       = (lfSectionRate >= KF_MIN_VEHICLES_PER_MINUTE)
                                          ? KF_MIN_VEHICLES_PER_MINUTE
                                          : lfSectionRate;
            const f32 lfScaled      = mfTrafficAmountScale * lfSectionRate;
            lfVehiclesPerMinute     = (lfScaled >= lfFloor) ? lfScaled : lfFloor;
        }
        CGS_ASSERT(lfVehiclesPerMinute > 0.0f, "lfVehiclesPerMinute > 0.0f");

        const f32 lfSecondsPerVehicle = KF_SECONDS_PER_MINUTE / lfVehiclesPerMinute;

        lfVehiclesToSpawn += lfTimeToDrive / lfSecondsPerVehicle;

        const f32 lfWholeVehicles     = std::floor(lfVehiclesToSpawn);   // 0x82743868 frsp f31
        const f32 lfVehiclesLeftOver  = lfVehiclesToSpawn - lfWholeVehicles;

        const f32 lfDistPerVehicle = lpSection->mfLength / lfWholeVehicles;
        const f32 lfJitterRange    = lfDistPerVehicle * KF_SPAWN_JITTER_FRACTION;

        const f32* lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);

        f32 lfParamAlong = lpSection->CalcParamFromStartParamAndDistanceAlongSection(
                               0.0f, lfDistPerVehicle * lfVehiclesLeftOver, lpafRungLengths);

        for (u32 luVehicle = static_cast<u32>(lfWholeVehicles); luVehicle != 0; --luVehicle)
        {
            const f32 lfJitter = mRand.RandomFloat(0.0f, lfJitterRange);
            const f32 lfSpawnParam = lpSection->CalcParamFromStartParamAndDistanceAlongSection(
                                         lfParamAlong, lfJitter, lpafRungLengths);

            GenerateNewVehicle(PickVehicleToSpawn(lpFlow->muFlowTypeId),
                               luHull,
                               luSection,
                               lfSpawnParam);

            lfParamAlong = lpSection->CalcParamFromStartParamAndDistanceAlongSection(
                               lfParamAlong, lfDistPerVehicle, lpafRungLengths);
        }

        lfVehiclesToSpawn = lfVehiclesLeftOver;   // 0x827439F0 fmr f30, f27
    }

    for (u32 luStatic = 0; luStatic < lpHull->muNumStaticTraffic; ++luStatic)
    {
        const StaticTrafficVehicle* lpRecord = lpHull->GetStaticVehicle(luStatic);

        const u32 luExistsRoll = (mRand.RandomUInt() % KU_PERCENTAGE_ROLL_MODULUS) + 1u;
        if (luExistsRoll > lpRecord->mExistsAtAllChance)
        {
            continue;
        }

        if ((lpRecord->muFlags & 0x02u) != 0 && !mbPlayingShowtimeMode)
        {
            continue;
        }

        if (!mbAllowDivergentBehaviour)
        {
            if ((lpRecord->muFlags & 0x01u) != 0)
            {
                continue;
            }
            // The console re-reads mbAllowDivergentBehaviour here and, finding it false,
            // jumps straight to the spawn -- i.e. the proximity cull is the
            // divergent-behaviour arm only. Fall through to the spawn.
        }
        else
        {
            // 0x82743AFC..0x82743B28. Reference position = the +0x728C0 lane == the
            // mCameraLastFrame transform's Pos row. Radius unk_8300CC90 == 1600.0f == 40 m
            // squared (dyn-init thunk 0x82C662D0 squares the 40.0f splat at 0x8300CB80).
            const Vector3 lToRecord = lpRecord->mTransform.Pos() - mCameraLastFrame.GetPosition();
            if (rw::math::vpu::Dot(lToRecord, lToRecord) < KF_PARKED_PROXIMITY_CULL_RADIUS_SQ)
            {
                continue;
            }
        }

        StaticVehicles_Generate(PickVehicleToSpawn(lpRecord->mFlowTypeID),
                                luHull,
                                static_cast<u8>(luStatic));
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::SpawnNewTraffic  @ 0x82748A40   (.cpp 8378)
//
//   assert(IsDecisionFrame());
//   if (NeedToTakeActionAgainstJunctionFUP()) return;   ; the predicate is INLINED at
//                                                       ; 0x82748A90..0x82748AF8, byte for
//                                                       ; byte the committed
//                                                       ; NeedToTakeActionAgainstJunctionFUP
//   for (i = 0; i < lrNewActiveHulls.GetLength(); ++i)  FillNewHull(lrNewActiveHulls[i]);
//   ---- GENERATOR HALF (REAL, 0x82748BB0..) : tick mafTimesTillNextGeneration, emit ----
// ----------------------------------------------------------------------------
void TrafficEntityModule::SpawnNewTraffic(const ActiveHullSet& lrNewActiveHulls)
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");

    if (NeedToTakeActionAgainstJunctionFUP())
    {
        return;
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-spawn] first visit only.  DELETE-WHEN-STABLE.
        static bool sbFirstVisit = true;
        if (sbFirstVisit)
        {
            sbFirstVisit = false;
            *lpDiag << "[T1-spawn] SpawnNewTraffic first visit, newHulls="
                    << static_cast<s32>(lrNewActiveHulls.GetLength()) << "\n";
        }
    }

    for (u32 luIndex = 0; luIndex < lrNewActiveHulls.GetLength(); ++luIndex)
    {
        FillNewHull(lrNewActiveHulls[luIndex]);
    }

    // ---- GENERATOR HALF, 0x82748BB0.. -----------------------------------------------------
    // Each generator counts down by the decision-frame interval. On expiry the overshoot is
    // turned back into a distance along the lane (speed * overshoot), the car is placed there,
    // and the emission is skipped when the first param already on that section is closer than
    // two seconds of lane speed behind it.
    for (u32 luGenerator = 0; luGenerator < muNumGenerators; ++luGenerator)
    {
        mafTimesTillNextGeneration[luGenerator] -= mfSimTimeSinceLastDecision;

        if (mafTimesTillNextGeneration[luGenerator] > 0.0f)
        {
            continue;
        }

        const u32 luHull    = maGenerators[luGenerator].muHull;
        const u32 luSection = maGenerators[luGenerator].muSection;

        const Hull*        lpHull    = GetHull(luHull);
        const SectionFlow* lpFlow    = &lpHull->mpaSectionFlows[luSection];  // Hull::GetFlowData, inlined
        const Section*     lpSection = lpHull->GetSection(luSection);

        const u8  luVehicleType  = PickVehicleToSpawn(lpFlow->muFlowTypeId);
        const f32 lfDistanceIn   = -(mafTimesTillNextGeneration[luGenerator] * lpSection->mfSpeed);
        const f32* lpafRungLengths = lpHull->GetRungLengthsForSection(lpSection);

        const f32 lfParamAlong = lpSection->CalcParamFromStartParamAndDistanceAlongSection(
                                     0.0f, lfDistanceIn, lpafRungLengths);

        bool lbGenerate = true;

        const u16 luFirstParam = GetHullRuntime(luHull)->GetFirstParamInSection(luSection);
        if (luFirstParam != static_cast<u16>(KU_INVALID_PARAM))
        {
            const Param* lpFirstParam = GetParam(luFirstParam);

            const f32 lfFrontDist = lpSection->CalcDistanceAlongSection(lpFirstParam->mfParamAlong,
                                                                        lpFirstParam->muCurrentSegment,
                                                                        lpafRungLengths)
                                    - lpFirstParam->mfBackDist;

            if ((lfFrontDist - lfDistanceIn) < (lpSection->mfSpeed * KF_GENERATOR_MIN_HEADWAY_SECONDS))
            {
                lbGenerate = false;
            }
        }

        if (lbGenerate)
        {
            GenerateNewVehicle(luVehicleType, luHull, luSection, lfParamAlong);
        }

        mafTimesTillNextGeneration[luGenerator] += CalcTimeToNextGeneration(luHull, luSection);
    }
}

// ============================================================================
// SECTION 7 -- the active-hull set.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::RecalculateActiveHulls  @ 0x8274C870   PARTIAL   (.cpp 7275..7367)
//
// Real here: the five entry asserts, the previous-set snapshot, the rebuild of mActiveHulls
// from maaRaceCarHulls, the two SetDifferences that produce the caller's new/old sets, the
// miDEBUGOverBudgetness reset, the same rebuild for mActiveHullsForLocalPlayer, and
// UpdateRaceCarHulls @0x82721460's offline arm expanded at its single call site below.
//
// Gated, each with its own reason:
//   * PredictHullChanges @0x827348E8 -- an export hole (no per-function JSON), and online-only
//     (`!mbAllowDivergentBehaviour && meState == E_STATE_RUNNING`).
//   * the baked debug hull-override list (the bool at X360 +0x729F0 selecting the 15-entry
//     table at unk_820BA81C) -- that byte sits in the un-emitted DWARF :892..:895 window
//     between mfDEBUGAvoidance_PassScore and miPerfMon_PreSceneUpdate, so it has no name.
//   * the std::_Sort of mActiveHulls -- ::Set<T,N> has no Sort. Order-only: SetDifference is
//     order-independent, so only the order FillNewHull visits hulls in changes.
//   * mHullsToAddTriggersFor / mHullsToRemoveTriggersFor -- they need ::Array<T,N>::AppendSet,
//     which CgsArray.h does not declare (it has AppendArray only).
//   * the light-manager events either side of the HullRuntime loops, and the stopline walk
//     ending in HullRuntime::SetStoplineRed @0x8274D82C -- TrafficLightManager has no body.
//
// The per-old-hull HullRuntime::Release + free, the per-new-hull allocate + HullRuntime::Prepare
// and the tail call to RebuildGeneratorList @0x8274D8F4 are LIVE (wave T2 round 1).
// ----------------------------------------------------------------------------
void TrafficEntityModule::RecalculateActiveHulls(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput,
    ActiveHullSet* lpOutNewHulls,
    ActiveHullSet* lpOutOldHulls)
{
    CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");
    CGS_ASSERT(lpInput != 0, "lpInput != NULL");
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");
    CGS_ASSERT(lpOutNewHulls != 0, "lpOutNewHulls != NULL");
    CGS_ASSERT(lpOutOldHulls != 0, "lpOutOldHulls != NULL");

    if (!mbAllowDivergentBehaviour && meState == E_STATE_RUNNING)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls leg PredictHullChanges @0x827348E8 -- EXPORT HOLE (no "
            "per-function JSON in .ida-exports); ONLINE-only arm, dead offline");
    }

    // ====================================================================================
    // THE BODY OF TrafficEntityModule::UpdateRaceCarHulls @0x82721460, expanded at its single
    // call site. It is the only producer of maaRaceCarHulls, which mActiveHulls is rebuilt
    // from, so without it the new-hull set is always empty and FillNewHull never runs.
    //
    // FLAG: OUTLINE-ME. The console has this as a member function with exactly one caller, but
    // it is not declared in BrnTrafficEntityModule.h and a member cannot be defined without a
    // declaration. A free function taking `TrafficEntityModule&` would be the shim
    // anti-pattern the faithfulness gate catches, so it is expanded here instead. To outline
    // it, add this line to BrnTrafficEntityModule.h beside RecalculateActiveHulls (private,
    // like its caller) and move the block below into a partfile verbatim; it reads only
    // lpInput and members, so nothing else changes:
    //
    //        void UpdateRaceCarHulls( const BrnTrafficIO::InputBuffer_PostPhysics* lpInput );
    //
    // WHAT THE CONSOLE DOES (0x82721484..0x827217FC):
    //   assert(IsDecisionFrame());                                     ; baked .cpp 7575
    //   lpActive = lpInput->GetActiveRaceCarOutputInterface();         ; sub_82711850
    //   if (mbAllowDivergentBehaviour)              ; lbzx +0x717E7
    //   {
    //       if (lpActive->IsPlayerCarActive() == 1)
    //       {
    //           lePlay = lpActive->GetPlayerActiveRaceCarIndex();      ; 0x82277BF8
    //           for (i = 0; i < 8; ++i) maaRaceCarHulls[i].Clear();    ; stw 0, +0x55820 stride 0x18
    //           lCentre = <player car position | two DEBUG overrides>
    //           lHalf   = Vector3(mfTrafficSimRadius)                  ; lvx +0x713B0, vperm/vrlimi
    //           minCell = Pvs::GetHullIndexForPoint(lCentre - lHalf, &minX, &minZ);
    //           maxCell = Pvs::GetHullIndexForPoint(lCentre + lHalf, &maxX, &maxZ);
    //           for (x = minX; x <= maxX; ++x)
    //               for (z = minZ; z <= maxZ; ++z)
    //                   maaRaceCarHulls[lePlay].Append(Pvs::GetHullIndexForIndices(x, z));
    //           if (!mbInOfflineCarSelect)                             ; lbzx +0x713C8
    //               assert(maaRaceCarHulls[lePlay].GetLength() <= 4);  ; "We ended up with too many hulls turned on: "
    //       }
    //   }
    //   else { ...the ONLINE predicted-hull-change replay, GATED below... }
    //
    // The return value of the two corner calls is discarded: the console keeps only the four
    // grid coordinates and overwrites the linear cell index. The calls still happen, because
    // their bounds assert is a real side effect.
    // ====================================================================================
    {
        CGS_ASSERT(IsDecisionFrame(), "IsDecisionFrame()");   // baked .cpp 7575

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars =
            lpInput->GetActiveRaceCarOutputInterface();

        if (mbAllowDivergentBehaviour)
        {
            if (lpActiveRaceCars->IsPlayerCarActive())
            {
                const EActiveRaceCarIndex lePlayerCar = lpActiveRaceCars->GetPlayerActiveRaceCarIndex();

                // 0x82721508..0x82721550: eight Clear()s, one per active-race-car slot, with
                // the enum-walk assert the console bakes from BurnoutConstants.h:39. The
                // stride is 0x18 == sizeof(Array<u16,9>) and the base is the count word.
                for (s32 liRaceCar = 0; liRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCar)
                {
                    maaRaceCarHulls[liRaceCar].Clear();
                    CGS_ASSERT(liRaceCar + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
                }

                // The sim-box centre, console default (0x82721590): the player car's world
                // position, `GetRaceCarState(lePlay)->mTransform.wAxis` (asm `addi r11, state,
                // 0x1F0 ; lvx128 v126, r11, 0x30`, where 0x1F0 is RaceCarState::mTransform and
                // +0x30 its translation row). IDA names the call GetRaceCarStateMutable
                // @0x8227D690 because the const twin was ICF-folded onto it; a const interface
                // pointer needs the const form.
                //
                // ASSERT DELTA, deliberate: 0x8227D690 carries three asserts (index >= 0,
                // index < COUNT, IsRaceCarActive(index)); the committed const :220 body carries
                // only the two bounds ones. The third is unreachable here, since
                // IsPlayerCarActive() already returned true. Same applies at
                // PostPhysicsUpdate's tail, which reaches the same accessor.
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState*
                    lpPlayerState = lpActiveRaceCars->GetRaceCarState(lePlayerCar);
                const Vector3 lSimCentre = lpPlayerState->mTransform.wAxis;

                {
                    // GATE: the two DEBUG sim-centre overrides @0x82721554 / 0x827215A8, both
                    // substituting mCameraLastFrame.GetPosition() for lSimCentre.
                    // BLOCKER: the selector word at X360 +0x729D4 is mCameraLastFrame+0x144,
                    // an unnamed Camera field, and DebugComponent::+0x34 has no name either.
                    // DELETE-WHEN both are named. DEBUG-ONLY; the live default is taken.
                    static bool sbLogged = false;
                    LogMissingLeg(sbLogged,
                        "UpdateRaceCarHulls DEBUG sim-centre overrides @0x82721554 / "
                        "0x827215A8 -- selector words mCameraLastFrame+0x144 and "
                        "DebugComponent+0x34 are unnamed. DEBUG-ONLY, no live effect");
                }

                // The box half-extent, 0x827215CC..0x82721604: mfTrafficSimRadius through the
                // SDK's VecFloat -> Vector3 lane shuffle (w zeroed, y restored). Construct
                // seeds the member as a splat of 195.0f, so every lane the shuffle can select
                // is 195.0f, and only lanes 0 and 2 reach the Pvs. Written as the Vector3 it
                // produces rather than transcribed as VMX.
                Vector3 lHalfExtent;
                lHalfExtent.x = mfTrafficSimRadius.x;
                lHalfExtent.y = mfTrafficSimRadius.y;
                lHalfExtent.z = mfTrafficSimRadius.z;
                lHalfExtent.w = 0.0f;

                Vector3 lBoxMin;                        // vsubfp128 v125, v126, v127
                lBoxMin.x = lSimCentre.x - lHalfExtent.x;
                lBoxMin.y = lSimCentre.y - lHalfExtent.y;
                lBoxMin.z = lSimCentre.z - lHalfExtent.z;
                lBoxMin.w = 0.0f;

                Vector3 lBoxMax;                        // vaddfp128 v127, v126, v127
                lBoxMax.x = lSimCentre.x + lHalfExtent.x;
                lBoxMax.y = lSimCentre.y + lHalfExtent.y;
                lBoxMax.z = lSimCentre.z + lHalfExtent.z;
                lBoxMax.w = 0.0f;

                // ---- corner -> grid coordinates -------------------------------------------
                // Each corner goes through TrafficData::operator-> then `lwz r3, 8(r3)`, i.e.
                // mpData->mpPvs (TrafficData +0x08, static_asserted in
                // BrnTrafficDataResourceType.h).
                const Pvs* lpPvs = mpData->mpPvs;

                s32 liMinX = 0;
                s32 liMinZ = 0;
                lpPvs->GetHullIndexForPoint(lBoxMin, liMinX, liMinZ);

                s32 liMaxX = 0;
                s32 liMaxZ = 0;
                lpPvs->GetHullIndexForPoint(lBoxMax, liMaxX, liMaxZ);

                // ---- walk the rectangle ---------------------------------------------------
                // Outer loop X (r29, 0x827216F4), inner loop Z (r31, 0x827216E8) -- that
                // order is the asm's, and it decides the order FillNewHull later visits hulls
                // in. Both bounds are INCLUSIVE (`ble`), and both loops are entered only when
                // min <= max (`bgt` skips), which is why an empty box produces no hulls
                // rather than wrapping.
                for (s32 liCellX = liMinX; liCellX <= liMaxX; ++liCellX)
                {
                    for (s32 liCellZ = liMinZ; liCellZ <= liMaxZ; ++liCellZ)
                    {
                        const u16 luHull =
                            static_cast<u16>(lpPvs->GetHullIndexForIndices(liCellX, liCellZ));
                        maaRaceCarHulls[lePlayerCar].Append(luHull);
                    }
                }

                if (!mbInOfflineCarSelect)
                {
                    // 0x82721700..0x827217FC. The console builds the message with the count
                    // appended ("We ended up with too many hulls turned on: %u"); the budget
                    // literal is `cmplwi r11, 4 ; ble ->`, i.e. the assert fires above FOUR
                    // even though the array holds KU_MAX_ACTIVE_HULLS_PER_RACECAR (9).
                    CGS_ASSERT(maaRaceCarHulls[lePlayerCar].GetLength() <= 4u,
                               "We ended up with too many hulls turned on");
                }
            }
        }
        else
        {
            // GATE: the online arm from 0x82721870, which replays maPredictedHullChanges
            // instead of computing the box locally so every client turns the same hulls on in
            // the same frame, and on a miss latches mbHullSyncDivergence. Its producer
            // PredictHullChanges @0x827348E8 is an ARTIST export hole, so the array is never
            // filled here and replaying it would only assert. Dead offline anyway.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "UpdateRaceCarHulls ONLINE arm (!mbAllowDivergentBehaviour) -- replays "
                "maPredictedHullChanges, whose producer PredictHullChanges @0x827348E8 is an "
                "ARTIST EXPORT HOLE. Dead offline");
        }
    }

    // Snapshot the previous set, then rebuild it. The console memcpy's 148 bytes (Set<u16,72>:
    // 144 element bytes plus the 4-byte length) into a stack temp before clearing.
    const ActiveHullSet lPreviousActiveHulls = mActiveHulls;
    mActiveHulls.Clear();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls DEBUG hull-override arm (the bool at X360 +0x729F0 "
            "selects the baked 15-entry hull table at unk_820BA81C) -- that byte lies in the "
            "DecFIGS un-emitted :892..:895 window, so it has no attested member name");
    }

    for (EActiveRaceCarIndex leRaceCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leRaceCar = static_cast<EActiveRaceCarIndex>(static_cast<s32>(leRaceCar) + 1))
    {
        const ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>& lrRaceCarHulls =
            maaRaceCarHulls[leRaceCar];

        for (u32 luIndex = 0; luIndex < lrRaceCarHulls.GetLength(); ++luIndex)
        {
            const u16 luRaceCarHull = lrRaceCarHulls.GetItem(luIndex);
            CGS_ASSERT(luRaceCarHull < KU_MAX_HULLS, "luRaceCarHull < KU_MAX_HULLS");
            mActiveHulls.Insert(luRaceCarHull);
        }
    }

    if (mActiveHulls.GetLength() != 0)
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls std::_Sort(mActiveHulls) -- ::Set<T,N> (CgsSet.h, not "
            "this cluster's file) has no Sort. ORDER ONLY: SetDifference is order-independent "
            "so the new/old sets are identical; only FillNewHull's visit order changes");
    }

    lpOutNewHulls->SetDifference(mActiveHulls, lPreviousActiveHulls);
    lpOutOldHulls->SetDifference(lPreviousActiveHulls, mActiveHulls);

    if (lpOutNewHulls->GetLength() != 0 || lpOutOldHulls->GetLength() != 0)
    {
        // LABEL_43: the debug over-budget counter resets whenever the active set moved.
        // +468932 == miDEBUGOverBudgetness (:845), pinned by the debug block's run: mpLogger
        // @+468916, mbDEBUGStopTrafficMoving @+468920, meDEBUGAirRamToFire @+468924,
        // miDEBUGOverrideVehicleToSpawn @+468928.
        miDEBUGOverBudgetness = 0;
    }

    // ---- the same computation for the LOCAL player's hulls -------------------------------
    const ActiveHullSet lPreviousLocalHulls = mActiveHullsForLocalPlayer;
    mActiveHullsForLocalPlayer.Clear();

    if (meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        const ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>& lrLocalHulls =
            maaRaceCarHulls[meLocalPlayerIndex];

        for (u32 luIndex = 0; luIndex < lrLocalHulls.GetLength(); ++luIndex)
        {
            const u16 luRaceCarHull = lrLocalHulls.GetItem(luIndex);
            CGS_ASSERT(luRaceCarHull < KU_MAX_HULLS, "luRaceCarHull < KU_MAX_HULLS");
            mActiveHullsForLocalPlayer.Insert(luRaceCarHull);
        }
    }

    {
        // GATE: the two Array<u16,72>::AppendSet calls @0x8274?? that feed
        // mHullsToAddTriggersFor / mHullsToRemoveTriggersFor. BLOCKER: ::Array<T,N>::AppendSet
        // is absent from CgsArray.h, which declares AppendArray only.
        // DELETE-WHEN CgsArray.h grows AppendSet. Triggers are not on the round-1 driving path.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls trigger legs -- the two Array<u16,72>::AppendSet calls "
            "feeding mHullsToAddTriggersFor / mHullsToRemoveTriggersFor need "
            "::Array<T,N>::AppendSet, absent from CgsArray.h (it declares AppendArray only)");
    }

    // ---- HullRuntime free, one per hull that left the set ---------------------------------
    // 0x8274?? .. `HullRuntime::Release(1176 * idx + this + 257216)` then the bit-array free
    // and the index reset. mauHullRuntimeDataIndices / maHullRuntimeData are reached by name.
    for (u32 luOld = 0; luOld < lpOutOldHulls->GetLength(); ++luOld)
    {
        const u16 luHull        = lpOutOldHulls->GetItem(luOld);
        const u8  luHullRuntime = mauHullRuntimeDataIndices[luHull];

        if (luHullRuntime == KU_INVALID_HULL_RUNTIME)
        {
            continue;
        }

        CGS_ASSERT(luHullRuntime < KU_MAX_ACTIVE_HULLS, "luIndex < NUMBITS");
        CGS_ASSERT(mUsedHullRuntimeData.IsBitSet(luHullRuntime),
                   "mUsedHullRuntimeData.IsBitSet( luHullRuntime )");

        maHullRuntimeData[luHullRuntime].Release();
        mUsedHullRuntimeData.UnSetBit(luHullRuntime);
        mauHullRuntimeDataIndices[luHull] = KU_INVALID_HULL_RUNTIME;
    }

    // ---- HullRuntime allocate, one per hull that joined ------------------------------------
    // The console picks the slot with a cntlzd scan for the first CLEAR bit of
    // mUsedHullRuntimeData; CgsBitArray.h exposes no such primitive, so the scan is written
    // as the linear loop it was strength-reduced from (same result: the lowest free slot).
    for (u32 luNew = 0; luNew < lpOutNewHulls->GetLength(); ++luNew)
    {
        const u16 luHull = lpOutNewHulls->GetItem(luNew);

        CGS_ASSERT(mauHullRuntimeDataIndices[luHull] == KU_INVALID_HULL_RUNTIME,
                   "mauHullRuntimeDataIndices[luHull] == KU_INVALID_HULL_RUNTIME");

        s32 liHullRuntime = -1;
        for (u32 luSlot = 0; luSlot < KU_MAX_ACTIVE_HULLS; ++luSlot)
        {
            if (!mUsedHullRuntimeData.IsBitSet(luSlot))
            {
                liHullRuntime = static_cast<s32>(luSlot);
                break;
            }
        }

        CGS_ASSERT(liHullRuntime >= 0, "liHullRuntime >= 0");
        if (liHullRuntime < 0)
        {
            continue;
        }

        maHullRuntimeData[liHullRuntime].Prepare(GetHull(luHull), luHull);
        mUsedHullRuntimeData.SetBit(static_cast<u32>(liHullRuntime));
        mauHullRuntimeDataIndices[luHull] = static_cast<u8>(liHullRuntime);
    }

    {
        // GATE: the light-manager events either side of the two loops above, and the trailing
        // per-hull stopline walk that ends in HullRuntime::SetStoplineRed.
        // BLOCKER: TrafficLightManager has no Construct/Update body in this tree (see the
        // Reset and PostPhysicsUpdate gates). DELETE-WHEN the light manager lands.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls light-manager legs -- the per-hull add/remove events and "
            "the stopline walk ending in HullRuntime::SetStoplineRed. TrafficLightManager has "
            "no Construct/Update body in this tree; lights stay in their default phase");
    }

    // @0x8274D890..0x8274D8F4. The generator list is rebuilt only when the active-hull set
    // actually moved; RecalculateActiveHulls is its ONLY xref, so muNumGenerators has no other
    // producer. Both GetLength reads carry the Set's own "length != -1" assert (CgsSet.h 227).
    if (lpOutNewHulls->GetLength() != 0 || lpOutOldHulls->GetLength() != 0)
    {
        RebuildGeneratorList();
    }

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-hull] value-latched: prints only when the active-hull count changes.
        // DELETE-WHEN-STABLE.
        static s32 siLastActiveHullCount = -1;
        const s32 liActiveHullCount = static_cast<s32>(mActiveHulls.GetLength());
        if (liActiveHullCount != siLastActiveHullCount)
        {
            siLastActiveHullCount = liActiveHullCount;
            *lpDiag << "[T1-hull] activeHulls=" << liActiveHullCount
                    << " new=" << static_cast<s32>(lpOutNewHulls->GetLength())
                    << " old=" << static_cast<s32>(lpOutOldHulls->GetLength()) << "\n";
        }
    }
}

// ============================================================================
// SECTION 8 -- the per-frame driver.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::PostPhysicsUpdate  @ 0x8274E6D0   PARTIAL
//
// Real here: the buffer lock bracket, the streaming-complete latch, UpdateDensity, the
// mbDEBUGTurnTrafficOff tear-down trigger, the whole E_STATE_STARTING_UP arm (POPULATING is
// where parked cars are created), and the tail's local-player refresh (meLocalPlayerIndex,
// mLocalPlayerPosition, mLocalPlayerDirection).
//
// Gated: the pre-state head (UpdateDEBUG, HandleExternalRequests), most of the
// E_STATE_RUNNING arm, the E_STATE_TEARING_DOWN arm, and the rest of the tail (perfmon,
// UpdateEventStarts, the network, traffic-type and replay legs). None is bodied in this tree
// and none is on the parked-car path.
//
// DEPENDS ON PreSceneUpdate BEING MOUNTED. The console advances
// E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING in PreSceneUpdate @0x8274A968, not here,
// and its body lives in BrnTrafficEntityModule_wT1_02.cpp. That transition cannot be moved
// here: it reads `lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive()` on the
// PRE-SCENE input buffer, which this function does not have, and emitting it without that test
// would advance to POPULATING before the player car exists, reaching RUNNING with an empty
// world. So the POPULATING arm below runs only once _wT1_02.cpp is in
// tools/build/build_game_exe.bat. Until then the exe fails to link with LNK2019 on
// PreSceneUpdate; the per-TU `cl /c` gate cannot see the mount either way.
// ----------------------------------------------------------------------------
void TrafficEntityModule::PostPhysicsUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                            CgsModule::IOBufferStack* lpOutputBufferStack,
                                            BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                            BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput,
                                            BrnUpdateSet lUpdateSet)
{
    (void)lpInputBufferStack;
    (void)lpOutputBufferStack;

    lpOutput->LockForWrite();
    lpInput->LockForRead();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate head legs UpdateDEBUG @0x8271DC78 / HandleExternalRequests -- "
            "neither is bodied in this tree. Consequence: the module never consumes game-mode "
            "requests, so mfGameModeDensityScale keeps the value ResetEventData seeded "
            "(mfBaseDensityScale == 1.0f), which is the density parked cars need");
    }

    // The streamer pump, head call (`bl UpdateStreaming` @0x8274E740, before UpdateDensity's
    // at 0x8274E7A0). This is what makes the game ASK for a VEH_T*_GR bundle: SetAssetList
    // publishes the catalogue, and nothing is requested until TrafficCarStreamer::Update runs,
    // whose only pump is UpdateStreaming (body in BrnTrafficEntityModule_wT1_04.cpp, which
    // must be mounted or this is an LNK2019).
    UpdateStreaming(lpOutput);

    if (mbWaitingForStreaming && mStreamer.AreAllAssetsLoaded())
    {
        mbWaitingForStreaming = false;

        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate StreamingCompleteEvent(E_MODULE_TRAFFIC_ENTITY) emit -- TWO "
            "blockers, both one step: (a) the tree's EModule enum in BrnGameEvents.h carries "
            "only an invented-name E_MODULE_WORLD_GRAPHICS=2, while the DecFIGS DWARF gives the "
            "whole set (TRAFFIC_ENTITY=0, RACE_CAR_ENTITY=1, WORLD_ENTITY=2, GUI_SCREEN=3, "
            "COUNT=4) and the X360 emit stores literal 0, so it needs an additive completion in "
            "a header this wave does not own; (b) the console posts `AddEvent(&record, 9, 16)` "
            "where 16 is the CONSOLE record size, so the host size must come from the host "
            "type. The FLAG ITSELF is cleared, so the module does not wait forever");
    }

    UpdateDensity();

    if (mbDEBUGTurnTrafficOff && meState == E_STATE_RUNNING && !IsPaused())
    {
        EnterTearingDownState();
    }

    switch (meState)
    {
    case E_STATE_STARTING_UP:
        switch (meStartingUpState)
        {
        case E_STARTINGUPSTATE_WAITING_FOR_PLAYER:
            // The console's arm is empty here; the transition lives in PreSceneUpdate
            // @0x8274A968 (_wT1_02.cpp). PreScene runs before PostPhysics, so the state can
            // flip and the next arm run on the same frame, which is why meLocalPlayerIndex,
            // refreshed in this function's tail, is one frame old when POPULATING reads it.
            // That is the console's ordering too.
            break;

        case E_STARTINGUPSTATE_POPULATING:
        {
            ActiveHullSet lNewActiveHulls;
            ActiveHullSet lOldActiveHulls;
            lNewActiveHulls.Construct();
            lOldActiveHulls.Construct();

            RecalculateActiveHulls(lpInput, lpOutput, &lNewActiveHulls, &lOldActiveHulls);

            CGS_ASSERT(lOldActiveHulls.GetLength() == 0, "lOldActiveHulls.GetLength() == 0");

            // 0x8274ED00..0x8274ED1C: `meLocalPlayerIndex != -1 || mbAllowDivergentBehaviour`.
            // Offline the right-hand side is always true, so populating starts on the first
            // decision frame; online it waits until the local player is seated, which is what
            // keeps every client's population deterministic.
            if (meLocalPlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID || mbAllowDivergentBehaviour)
            {
                SpawnNewTraffic(lNewActiveHulls);

                if (mbAllowDivergentBehaviour)
                {
                    // @0x8273A308.
                    UpdateVehicles_CreateNewVehicles(lpInput);

                    StaticVehicles_CreateNewVehicles(lpInput);
                }

                // The streamer pump, POPULATING call (`bl UpdateStreaming` @0x8274ED58, right
                // after SpawnNewTraffic @0x8274ED28). The console places it OUTSIDE the
                // mbAllowDivergentBehaviour block above, so an online client that created no
                // vehicles locally still streams the assets its hull declares. It runs on the
                // one frame the module populates, with the player's hull list freshly built by
                // RecalculateActiveHulls, so AddVehiclesToTargetList has a hull to read.
                UpdateStreaming(lpOutput);

                // The POPULATING pass slices the WHOLE pool in one call ([0, 400), not the
                // non-decision frame's 100), so muLastParamCalculated is 400 before the first
                // decision frame. Body in _wT2_05.cpp.
                UpdateParams_DoTimeSlicedLogic(0,
                                               KU_MAX_PARAMS,
                                               lpInput->GetActiveRaceCarOutputInterface());

                meStartingUpState = E_STARTINGUPSTATE_WAITING_FOR_STREAMING;
            }
        }
        break;

        case E_STARTINGUPSTATE_WAITING_FOR_STREAMING:
            // 0x8274EC38..0x8274EC70. The `!mbAllowDivergentBehaviour` short-circuit is the
            // ship's addition over Feb-2007: an online client created no vehicles locally, so
            // it has nothing to wait for.
            if (!mbAllowDivergentBehaviour || mbDEBUGTurnTrafficOff || mStreamer.AreAllAssetsLoaded())
            {
                EnterRunningState();

                if (mbWaitingForStreaming && mStreamer.AreAllAssetsLoaded())
                {
                    mbWaitingForStreaming = false;
                    // Same StreamingCompleteEvent emit as above; already gated there.
                }
            }
            break;

        default:
            CGS_ASSERT(false, "Invalid starting up state");   // baked .cpp line 2934
            break;
        }
        break;

    // ====================================================================================
    // THE STEADY-STATE ARM. It calls neither RecalculateActiveHulls, SpawnNewTraffic nor the
    // StaticVehicles_* updates; it dispatches to UpdateDecisionFrame / UpdateNonDecisionFrame
    // (_wT1_06.cpp), which call them. Writing them here would be a second, invented copy of
    // the decision frame.
    //
    // Console order, 0x8274E7F0..0x8274E8FC, reproduced exactly:
    //   StartMonitor(+0x72A0C)
    //   HandleRecycledTraffic / HandleExternalResponses / HandleResetRaceCarEvents /
    //   HandleContactPoints / ProcessDeformationData                       [all GATED]
    //   StopMonitor(+0x72A0C)
    //   if (IsPaused() || lbSimPaused)  { StartMonitor(+0x72A28); }
    //   else { IsDecisionFrame() ? UpdateDecisionFrame : UpdateNonDecisionFrame ;
    //          StartMonitor(+0x72A28);
    //          GenerateSceneUpdateEvents ; TrafficLightManager::Update(mfSimTimeStep) }
    //   ProcessNearbyTrafficSceneQueryResults                              [GATED]
    //   GenerateRemovedVehicleEvents / GenerateSlamRecoveryEvents /
    //   GenerateVehicleCrashedEvents                                       [GATED]
    //   three 80-byte soa->output copies                                   [GATED]
    //   StopMonitor(+0x72A28)
    // ====================================================================================
    case E_STATE_RUNNING:
    {
        // ⭐ UN-GATED wave T3 (physical traffic): HandleExternalResponses @0x82732C68 is the
        // second of the five head legs and is BODIED (_wT3_04.cpp). It is what turns the physics
        // side's PhysicalTrafficState queue back into world vehicle transforms, so a car the
        // player hits actually moves. The other four legs keep their gate below.
        HandleExternalResponses(lpInput);

        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PostPhysicsUpdate E_STATE_RUNNING head legs -- HandleRecycledTraffic "
                "@0x82741AF8 / HandleResetRaceCarEvents / HandleContactPoints / "
                "ProcessDeformationData. None is bodied in this tree; all four consume crash / "
                "deformation input rings (later T3 rounds). HandleExternalResponses is LIVE "
                "as of wave T3 round 1");
        }

        // 0x8274E710 `clrlwi r27, r30, 31` -- bit 0 of the update set is the sim-paused bit,
        // fed straight into the `IsPaused() || ...` test below. FLAG (no enumerator):
        // BrnUpdateSet is a bare `typedef u16` with no named bits, so the bit is written as a
        // literal. Its name is unrecovered; the leak calls it E_HLA_UPDATE_PAUSED.
        const bool lbSimPaused = ((lUpdateSet & 1u) != 0);

        if (IsPaused() || lbSimPaused)
        {
            // The console's paused arm is genuinely empty apart from the perfmon bracket:
            // a paused traffic sim advances nothing and posts nothing.
        }
        else
        {
            if (IsDecisionFrame())
            {
                UpdateDecisionFrame(lpInput, lpOutput);
            }
            else
            {
                UpdateNonDecisionFrame(lpInput, lpOutput);
            }

            // The per-frame scene MOVER; body @0x8273B568 belongs to cluster C3.
            GenerateSceneUpdateEvents(lpOutput);

            {
                // GATE: TrafficLightManager::Update, declared at BrnTrafficLightManager.h:93
                // and bodied nowhere. Traffic-light phase state, not parked cars.
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "PostPhysicsUpdate RUNNING leg TrafficLightManager::Update(mfSimTimeStep) "
                    "-- DECLARED at BrnTrafficLightManager.h:93 and bodied nowhere in this "
                    "tree (ledger-done-but-bodyless). Light phases stay frozen; no parked-car "
                    "consumer");
            }
        }

        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PostPhysicsUpdate E_STATE_RUNNING tail legs -- "
                "ProcessNearbyTrafficSceneQueryResults, GenerateRemovedVehicleEvents, "
                "GenerateSlamRecoveryEvents, GenerateVehicleCrashedEvents, and the three "
                "80-byte mVehicleSoaData -> OutputBuffer_PostPhysics copies (soa members "
                "mPhysicalVehicles / mVehiclesRenderedLastFrame / mPhysicalVehiclesFarFrom"
                "Player into the crash-traffic input interface at console +3240/+3320/+3400). "
                "None is bodied; all are crash-module surface (wave 3)");
        }
    }
    break;

    case E_STATE_TEARING_DOWN:
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate E_STATE_TEARING_DOWN arm -- the WIPING pass (KillParam over "
            "400 params, RemoveVehicle over 600 vehicles, StaticVehicles_KillParam over 199 "
            "static params), FLUSHING and the WAITING_TO_RESET countdown into Reset(). "
            "KillParam @? and RemoveVehicle @? have no bodies, so wiping cannot be emitted "
            "without dropping half of it");
    }
    break;

    default:
        CGS_ASSERT(false, "Invalid state in traffic system");   // baked .cpp line 3041
        break;
    }

    // ====================================================================================
    // The local-player refresh, X360 0x8274ED90..0x8274EE88. meLocalPlayerIndex is what this
    // function's POPULATING arm and RecalculateActiveHulls' mActiveHullsForLocalPlayer rebuild
    // both key off; mLocalPlayerPosition / mLocalPlayerDirection are the module's cached copy
    // of where the player is, read by the driving and streaming legs of later waves.
    //
    // The console calls the getter three times (0x8274ED94, 0x8274EE30, 0x8274EE5C) and re-reads
    // the index from the member between them. Same buffer and interface each time, i.e. the
    // compiler rematerialising an inlined accessor, so it is de-inlined to one local; the
    // getter's read-lock assert is idempotent.
    //
    // +0x1F0 == RaceCarState::mTransform, +0x30 == wAxis (translation) and +0x20 == zAxis
    // (forward). Two displacements off one base: position and DIRECTION, not one vector twice.
    // ====================================================================================
    {
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars =
            lpInput->GetActiveRaceCarOutputInterface();

        if (lpActiveRaceCars->IsPlayerCarActive())
        {
            meLocalPlayerIndex = lpActiveRaceCars->GetPlayerActiveRaceCarIndex();

            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState*
                lpPlayerState = lpActiveRaceCars->GetRaceCarState(meLocalPlayerIndex);

            mLocalPlayerPosition  = lpPlayerState->mTransform.wAxis;
            mLocalPlayerDirection = lpPlayerState->mTransform.zAxis;
        }
        else
        {
            meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
    }

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate remaining tail legs -- the perfmon bracket, UpdateEventStarts "
            "@0x82743B80, GenerateNetworkUpdateEvents, ProcessTrafficTypeRequests "
            "@0x8272B880 (EXPORT HOLE) and the replay-serialiser registration/write");
    }

    lpInput->UnlockForRead();
    lpOutput->UnlockForWrite();
}

// ============================================================================
// SECTION 9 -- construction / reset.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::ResetEventData  @ 0x827088B8
//
// The per-event default block, and the only thing that seeds the density the parked chain
// gates on. LAYOUT ATTESTATION, every console offset resolved to its member:
//   mTrafficLightTriggerId = -1              (+464852)   meGameMode = -1            (+464856)
//   mbIsOnlineGameMode = false               (+464860)   mbPlayingShowtimeMode = false (+464861)
//   mbGameModeAllowsSwerving = true          (+464862)   mbHardcoreSwerveForMode = false (+464863)
//   mbGameModeAllowsKillzones = true         (+464864)   mbAtStartLineSoProtect... = false (+464865)
//   mbEnsureTrafficLightDelay = false        (+464866)   mbGameModeClearsTraffic = false (+464867)
//   mbNeedToSetUpLightsForEventStart = false (+464868)   mbAllowDivergentBehaviour = true (+464871)
//   mfGameModeDensityScale = mfBaseDensityScale (+464916 <- +464912)
//   mfTrafficAmountScale   = mfBaseDensityScale (+464924)
//   miBigVehicleAmount = 100                 (+464928)   mfTimeSinceLastShowtimeSpawn = 0 (+464952)
//   mfSpeedMultiplier = 1.0f                 (+469120)   mbNeedToBroadcastHullChange = false (+350430)
//   mfTrafficLightChangeBackDelay = 0.0f     (+463880)
//   maEventGridStartPositions[0..7] = 0      (+464720 .. +464832, eight stvx128 of a zero splat)
//   muNumberOfParticipantsInCurrentEvent = 0 (+464848)
//   mfCrashSliderCrashScore = 0 / Decay = 0.5f / Factor = 0.8f / FinalValue = 0
//                                            (+467824 / +467828 / +467832 / +467836)
//   mfShowtimeTimer = 0 / TimeNextCrashSpike = 0 / MisBounceTimer = 0
//                                            (+468144 / +468148 / +468156)
//   mfPlayerIdleTime = 0.0f                  (+468260)
//
// mfPlayerIdleTime at +468260 is forced, not guessed: 468144..468156 is :772..:775, the replay
// serialiser occupies the un-emitted :776/:777 window from +468160 (the console registers it as
// `RegisterSerialiser(..., this + 468160)`), and the next member is the 8-aligned
// mVehiclesToUpdateCollidables at +468264, leaving exactly one 4-byte slot for :778.
// ----------------------------------------------------------------------------
void TrafficEntityModule::ResetEventData()
{
    mTrafficLightTriggerId = 0xFFFFFFFFu;
    meGameMode             = -1;

    mbIsOnlineGameMode                       = false;
    mbPlayingShowtimeMode                    = false;
    mbGameModeAllowsSwerving                 = true;
    mbHardcoreSwerveForMode                  = false;
    mbGameModeAllowsKillzones                = true;
    mbAtStartLineSoProtectRaceCarsFromTraffic= false;
    mbEnsureTrafficLightDelay                = false;
    mbGameModeClearsTraffic                  = false;
    mbNeedToSetUpLightsForEventStart         = false;
    mbAllowDivergentBehaviour                = true;

    mbNeedToBroadcastHullChange = false;

    {
        // GATE: `*mpLogger = 1`, the same unnamed leading Logger byte EnterStartingUpState
        // writes, blocked the same way.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "ResetEventData mpLogger-><leading byte> = 1 -- Logger has no usable declaration "
            "(BrnTrafficLogger.cpp is unmounted and does not compile; see the breakdown at "
            "EnterStartingUpState)");
    }

    const f32 lfBaseDensityScale = mfBaseDensityScale;
    mfGameModeDensityScale = lfBaseDensityScale;
    mfTrafficAmountScale   = lfBaseDensityScale;

    miBigVehicleAmount           = 100;
    mfTimeSinceLastShowtimeSpawn = 0.0f;
    mfSpeedMultiplier            = 1.0f;
    mfTrafficLightChangeBackDelay= 0.0f;

    for (u32 luIndex = 0; luIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luIndex)
    {
        maEventGridStartPositions[luIndex].SetZero();
    }
    muNumberOfParticipantsInCurrentEvent = 0;

    mfCrashSliderCrashScore      = 0.0f;
    mfCrashSliderCrashScoreDecay = 0.5f;
    mfCrashSliderCrashScoreFactor= 0.80000001f;
    mfCrashSliderFinalValue      = 0.0f;

    mfShowtimeTimer              = 0.0f;
    mfShowtimeTimeNextCrashSpike = 0.0f;
    mfShowtimeMisBounceTimer     = 0.0f;

    mfPlayerIdleTime = 0.0f;
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::Reset  @ 0x8272CDA0   PARTIAL
//
// The pool constructor: what makes 199 static params and 600 vehicles exist in a usable state.
// The order below is the console's; gated legs carry their reason at the site.
//
// The offset-to-member resolution this body rests on closes with no slack at three independent
// anchors: maVehicles @+10880 and maVehicleAxles @+87680 (the Construct loop's two bases, with
// their 128/64 strides); the six 160-array live-count words at
// +357100/+359664/+359988/+360312/+360636/+360960 (deltas 2564, then 324 four times, which is
// TrafficCrashInfo(16)*160+4 then u16(2)*160+4); and maHullRuntimeData @+257216 + 72*1176 ==
// mUsedHullRuntimeData @+341888.
// ----------------------------------------------------------------------------
void TrafficEntityModule::Reset()
{
    // FLAG -- a known divergence, not an oversight. The console does not call the canonical
    // Random::Construct here. At 0x8272CDB8..0x8272CE88 it seeds both generators with the
    // traffic-specific literal 0x8FE06DC2, then primes all eight ring slots writing the CURRENT
    // slot before advancing. Random::Construct() instead installs KU_RANDOM_DEFAULT_SEED,
    // forces slot 0 to 1.0f, and primes slots 1..7 by advancing first. Both leave the ring
    // valid, so what differs is WHICH pseudo-random stream the traffic module runs on, and
    // therefore which parked record wins its mExistsAtAllChance roll and which vehicle type
    // PickVehicleToSpawn draws. Construct() is called anyway, because an unprimed ring makes
    // RandomFloat() return uninitialised storage.
    // FIX: add `void ConstructWithSeed(u64)` to CgsRandom.h doing the block above, and call it
    // here with 0x8FE06DC2ull.
    mRand.Construct();
    mEffectRand.Construct();

    muFramesSinceDecision = 100;
    mbDecisionFrame       = false;
    mfSimTimeStep         = 0.0f;

    meState            = E_STATE_INVALID;
    meStartingUpState  = E_STARTINGUPSTATE_INVALID;
    meRunningState     = E_RUNNINGSTATE_INVALID;
    meTearingDownState = E_TEARINGDOWNSTATE_INVALID;

    muUpdateCount = 0;

    EnterStartingUpState();

    mfTrafficAmountScale               = mfGameModeDensityScale;
    mfTimeSincePlayerHullChange        = 0.0f;
    mfTimeSincePlayerWasDrivingQuickly = 0.0f;
    muNumFramesBeforeStateChange       = 0xFFu;
    mbAllVehiclesDead                  = true;
    muPreviousPlayerHull               = 0xFFFFu;
    mbNeedToKillAllZombies             = false;
    miDEBUGOverBudgetness              = 0;

    {
        // GATE: the pseudocode's `BaseCollisionGenerator::Destruct(mpLogger)` is an ICF fold;
        // the real callee is a Logger reset, and the Logger type is unusable here.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset leg <ICF-folded>(mpLogger) @0x8272CE9C -- IDA attributes the callee to "
            "CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct, which is an "
            "identical-code-folding artefact; the real callee is a Logger reset and "
            "BrnTrafficLogger.cpp does not compile (see EnterStartingUpState)");
    }

    // ---- container resets ---------------------------------------------------------------
    mFreeParams.Clear();
    maPurgatoryList.Clear();
    mParamsToReinsert.Clear();

    mFreeStaticParamStack.Clear();
    mStaticParamPurgatoryList.Clear();

    mTrailerPurgatoryList.Clear();
    mFreeTrailerStack.Clear();

    maNewCrashedVehicles.Clear();
    maEmergencyCrashingVehicles.Clear();
    maNewCrashedNetworkVehicles.Clear();
    maRecentlyRemovedVehicles.Clear();
    maNewRemovedVehicles.Clear();
    maRecentlyRecoveredSlammedTraffic.Clear();

    muNumGenerators = 0;

    mVehiclesAddedToCrashModule.UnSetAll();
    maTrafficPhysicsInfoListBits.Prepare();

    // The 25 physical-traffic scratch records; body in BrnTrafficEntityModule_wT1_03.cpp.
    // The argument is the OWNING VEHICLE INDEX and Reset passes the "no owner" sentinel: the
    // console literal is 0xFFFF here and a sign-extended -1 in Construct @0x82740220, the same
    // 16 bits into the u16 member.
    //
    // This loop does not bind mDetachedPartQueue and never did: the console's Construct writes
    // one zero byte at record +0x00 and nothing else in the queue's span. That park lives in
    // wT1_03.cpp and at the Construct-tail gate below.
    for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES; luSlot++ )
    {
        maTrafficPhysicsInfoList[luSlot].Construct(
            static_cast< s32 >( TrafficPhysicsInfo::KU16_NO_OWNING_VEHICLE ) );
    }

    // The param membership sets: three consecutive 10-qword zeroing blocks at this+251648 /
    // +251728 / +251808. mParamSoaData is at +251648 and BrnTrafficParam.h pins mAliveParams
    // @0x00, mDyingParams @0x50, mZombieParams @0xA0; FastBitArray<601> is exactly 10 u64
    // fields (0x50 bytes), so the three offsets tile with no slack. The console inlines the
    // clears rather than calling ParamSoaData::Construct, and UnSetAll() is that same loop.
    // Without these, Param::IsAlive()/IsDying()/IsZombie() read uninitialised storage.
    mParamSoaData.mAliveParams.UnSetAll();
    mParamSoaData.mDyingParams.UnSetAll();
    mParamSoaData.mZombieParams.UnSetAll();

    mVehicleSoaData.Construct();

    // ---- the lane-param pool. 0x8272D0FC..0x8272D148, one loop over 400 slots: the three
    // Constructs at their own strides (maParams 0x80, maParamTransforms 0x40,
    // maParamNeedToSlowData 0x10 -- that third one is ParamNeedToSlowData::Construct INLINED,
    // storing FLT_MAX to +4/+8/+12 and the two sentinels to +0/+2), then the mFreeParams push.
    // The maParamListNodes Constructs are a separate 400-iteration loop at 0x8272D158.
    for (u32 luParam = 0; luParam < KU_MAX_PARAMS; ++luParam)
    {
        maParams[luParam].Construct();
        maParamTransforms[luParam].Construct();
        maParamNeedToSlowData[luParam].Construct();
        mFreeParams.Push(static_cast<u16>(luParam));
    }

    for (u32 luNode = 0; luNode < KU_MAX_PARAMS; ++luNode)
    {
        maParamListNodes[luNode].Construct();
    }

    // ---- the static (parked) param pool -- THE ONE THIS WAVE NEEDS ----------------------
    for (u32 luStatic = 0; luStatic < KU_MAX_STATIC_TRAFFIC; ++luStatic)
    {
        maStaticTrafficParams[luStatic].Construct();
        mFreeStaticParamStack.Push(static_cast<u8>(luStatic));
    }

    // The single trailer slot, pushed by FULL vehicle index (599 == KU_TRAILER_TRAFFIC_OFFSET).
    mFreeTrailerStack.Push(static_cast<u16>(KU_TRAILER_TRAFFIC_OFFSET));

    // The pool shuffles. The console's three instantiations are Shuffle<u16, Stack<u16,400>>
    // @0x8271B110 (mFreeParams), Shuffle<u8, Stack<u8,199>> @0x8271B298
    // (mFreeStaticParamStack) and Shuffle<u16, Stack<u16,1>> @0x8271B420 (mFreeTrailerStack).
    //
    // ARGUMENTS, from the asm (0x8272D204..0x8272D284), identical in all three calls:
    // r3 = the stack, r4 = 0, r5 = the stack's own GetLength(), r6 = this + 0x1330 == mRand
    // (:615, the first member after mReceiverQueue :613). The window is the whole live stack.
    //
    // The static one is on the parked-car path: unshuffled, StaticVehicles_CreateNewVehicles
    // pops slots in reverse push order (198..0) every boot, so which record lands in which slot
    // is deterministic instead of randomised. Nothing breaks, which is why it is easy to miss.
    CgsAlgorithms::Shuffle<u16>( mFreeParams, 0, mFreeParams.GetLength(), mRand );
    CgsAlgorithms::Shuffle<u8>( mFreeStaticParamStack, 0, mFreeStaticParamStack.GetLength(), mRand );
    CgsAlgorithms::Shuffle<u16>( mFreeTrailerStack, 0, mFreeTrailerStack.GetLength(), mRand );

    // ---- the vehicle pool ---------------------------------------------------------------
    for (u32 luVehicle = 0; luVehicle < KU_MAX_TOTAL_TRAFFIC; ++luVehicle)
    {
        Matrix44Affine lTransform;
        maVehicles[luVehicle].Construct(&maVehicleAxles[luVehicle], lTransform);
        SetVehicleTransform(luVehicle, lTransform);
    }

    // ---- hulls ---------------------------------------------------------------------------
    mActiveHulls.Clear();
    mActiveHullsForLocalPlayer.Clear();
    mHullsToAddTriggersFor.Clear();
    mHullsToRemoveTriggersFor.Clear();
    maPredictedHullChanges.Clear();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset leg mHullsToRemoveTriggersFor.AppendSet(mActiveHullsForLocalPlayer) -- "
            "::Array<T,N>::AppendSet is absent from CgsArray.h (AppendArray only). The set "
            "is empty at this point on every boot path, so the call is a no-op today");
    }

    for (u32 luHull = 0; luHull < KU_MAX_HULLS; ++luHull)
    {
        mauHullRuntimeDataIndices[luHull] = KU_INVALID_HULL_RUNTIME;
    }
    for (u32 luRuntime = 0; luRuntime < KU_MAX_ACTIVE_HULLS; ++luRuntime)
    {
        maHullRuntimeData[luRuntime].Construct();
    }
    mUsedHullRuntimeData.Prepare();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset leg TrafficLightManager::Construct(mTrafficLightManager) @0x8272D0F4 -- "
            "BrnTrafficLightManager.h declares no Construct (the mounted light-manager slice "
            "landed its Update/knock-down surface only)");
    }

    // ---- per-race-car scratch -------------------------------------------------------------
    for (u32 luRaceCar = 0; luRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++luRaceCar)
    {
        maStoredAITrafficData[luRaceCar].meRaceCarIndex  = static_cast<EActiveRaceCarIndex>(luRaceCar);
        maStoredAITrafficData[luRaceCar].miNumTrafficIDs = 0;
        maaRaceCarHulls[luRaceCar].Clear();
    }
    muCurrentlyPredictedHull    = 0xFFFFu;
    mbNeedToBroadcastHullChange = false;
    mbHullSyncDivergence        = false;
    mbNetworkHasDetectedDivergence = false;

    {
        // The `mau16HullsToActivateAfterReset` replay of the online hull set (guarded by
        // mbActivateOnlineHullsAfterReset) walks mpData->mpPvs cell sets. It is online-only
        // and is exactly the code path UpdateRaceCarHulls' online arm mirrors.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset mbActivateOnlineHullsAfterReset replay block (maaRaceCarHulls seeded from "
            "mau16HullsToActivateAfterReset + Pvs::GetHullPvs) -- ONLINE only; the flag is "
            "false on every offline boot, so the block does not execute");
    }

    // ---- collidable cache / avoidance -----------------------------------------------------
    mCachedCollidableList.SetFullCount();
    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset CollidableVehicleInfo4 lane pre-fill (16x vperm of KF_MAX_FLOAT through "
            "the permute-control constant unk_8327F140) + PrecalculateAvoidanceFeelerData "
            "@0x8272D9C4 -- the permute control is un-dumped rodata and "
            "PrecalculateAvoidanceFeelerData has no body. Avoidance is driving-traffic "
            "surface (wave 2)");
    }

    // The four crash-slider stores. The two zeros are `stfsx f31` with f31 == 0.0f; the two
    // seeds come from flt_820BA62C (Decay) and flt_820BA5B4 (Factor), which IDA resolves as 0.5
    // and 0.80000001 and which Construct's tail and ResetEventData load through the same two
    // symbols.
    //
    // WHY THEY MATTER: PostPhysicsUpdate's TEARING_DOWN arm calls Reset() with no Construct,
    // and UpdateCrashSlider @0x82715A18 overwrites both members at runtime, so dropping these
    // stores leaves the tear-down path running on last frame's slider tuning.
    mfCrashSliderCrashScore       = 0.0f;         // 0x72370  stfsx f31
    mfCrashSliderCrashScoreDecay  = 0.5f;         // 0x72374  flt_820BA62C
    mfCrashSliderCrashScoreFactor = 0.80000001f;  // 0x72378  flt_820BA5B4
    mfCrashSliderFinalValue       = 0.0f;         // 0x7237C  stfsx f31

    mfShowtimeTimer              = 0.0f;   // 0x724B0
    mfShowtimeTimeNextCrashSpike = 0.0f;   // 0x724B4
    mfShowtimeMisBounceTimer     = 0.0f;   // 0x724BC
    mfPlayerIdleTime             = 0.0f;   // 0x72524

    mShowtimePlayerGroundPos.SetZero();
    mAveragePhysicalCentre.SetZero();

    mfJunctionFUP                         = 0.0f;
    mfJunctionFUP_TimeTillNextPhysicalKill= 1.0f;
    mbInPictureParadise                   = false;

    mVehiclesToUpdateCollidables.UnSetAll();
    mVehiclesAvoidableLastFrame.UnSetAll();
    // 0x8272D8xx sets the FIRST 300 bits of mVehiclesToUpdateCollidables, not all 600. No
    // constant in BrnTrafficConstants.h spells 300, so the literal bound stands as it is.
    for (u32 luBit = 0; luBit < 300u; ++luBit)
    {
        mVehiclesToUpdateCollidables.SetBit(luBit);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::Construct  @ 0x82740220   PARTIAL
//
// Real here: the base construct, the receiver queue, the streamer, the two density seeds, the
// debug-flag defaults (including mbDEBUGTurnTrafficOff = false), the render caps, the 25
// TrafficPhysicsInfo constructs, ResetEventData + Reset, and the 96 VehicleTypeRuntime
// constructs.
//
// Gated: the ~25 vectorised tuning members (:799..:821), the four TrafficJobStub constructs
// ([MEMBER HOLE 5]), the replay serialiser, the 102,800-byte
// maTrafficPhysicsInfoList memset, the debug component and logger allocations, the twenty
// perfmon monitors, and the debug-render stream reader. Each is a named one-shot below.
// ----------------------------------------------------------------------------
void TrafficEntityModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    // ---- the vectorised tuning members, DWARF :799..:821 ---------------------------------
    // 0x8274028C..0x8274078C, one 16-byte store per member at this + 0x725F0 + 0x10*n. Every
    // source float was recovered from .rdata (0x820BA23C+); the two runtime-computed ones are
    // marked at their line. :816 mfVehicleRollFilterTime and :817 mTweakValues are NOT part of
    // this run: the console leaves the first to UpdateTimers and hands the second to
    // FuzzyBehaviourLogic::Construct (LANDED below -- it seeds all 21 mega-tweek constants).
    SetTuningSplat(KF_TWO_PI,                            6.2831855f);      // 0x725F0 flt_820BA250
    SetTuningSplat(KF_MAX_FLOAT,                         FLT_MAX);         // 0x72600 flt_820BA23C
    SetTuningSplat(KF_APPROX_LANE_WIDTH,                 4.5f);            // 0x72610 flt_820BA580
    SetTuningSplat(KF_MAX_DIST_ACROSS_LANE,              0.69999999f);     // 0x72620 flt_820BA4D0
    SetTuningSplat(KF_VEHICLE_STOPLINE_SIDE_SPACE,       0.89999998f);     // 0x72630 flt_820BA540
    SetTuningSplat(KF_VEHICLE_STOPLINE_SIDE_VARIATION,   0.25f);           // 0x72640 flt_820BA544
    SetTuningSplat(KF_VEHICLE_MAX_DIST_FROM_LANE_CENTRE, 1.29999995f);     // 0x72650 flt_820BA554

    // 0x72660, the one lane-wise Vector4 of the run (var_1E0 assembled at 0x82740370..0x827403B4).
    SetTuningLanes(kfVehicle_OptimalDistFromTarget_SpeedBalanceFactor_DirectionDampingFactor_MinDistToMove,
                   2.0f, 2.0f, 2.5f, 0.40000001f);

    SetTuningSplat(KF_VEHICLE_MAX_STEERING_DELTA,        0.025f);          // 0x72670 flt_820BA524

    // 0x72680: XMVectorSin(splat(flt_820BA528 == 25.0f) * splat(flt_820BA244 == KF_DEG_TO_RAD)).
    SetTuningSplat(KF_VEHICLE_SIN_MAX_STEERING_ANGLE,
                   std::sin(25.0f * 0.01745329238474369f));

    // 0x72690: flt_8300CB58, seeded by the dyn-init thunk at 0x82C66280 as
    // flt_82001C98(1.0f) / (flt_82F31928(0.44704f, mph->m/s) * flt_820BA5E4(10.0f)), i.e. the
    // reciprocal of 10 mph in m/s.
    SetTuningSplat(KF_VEHICLE_RECIP_ROLL_SPEED_MIN,      1.0f / (0.44704f * 10.0f));

    SetTuningSplat(KF_VEHICLE_ROLL_FACTOR,               -0.1f);           // 0x726A0 flt_8200D530
    SetTuningSplat(KF_VEHICLE_PITCH_RECIP_MAX_DECEL,     0.2f);            // 0x726B0 flt_82004744
    SetTuningSplat(KF_VEHICLE_PITCH_DAMPING_FACTOR,      0.94999999f);     // 0x726C0 flt_820BA57C
    SetTuningSplat(KF_VEHICLE_PITCH_SCALE,               0.050000001f);    // 0x726D0 flt_820047C8

    // The four cones, {cos(half-angle), length, recip-Y-scale, w}. The console builds each with
    // a read-modify-write of the member's own lanes, so the lanes it never stores keep whatever
    // the record held; they are written as 0.0f here. Angles: dbl_8200D500 == 10 degrees,
    // dbl_820BFBF0 == 20 degrees, both in radians.
    SetTuningLanes(kfParamSympatheticCone_CosAngle_Length_RecipYScale_W,
                   static_cast<f32>(std::cos(0.1745329238474369)), 30.0f, 0.25f, 0.0f);   // 0x726E0
    SetTuningLanes(kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W,
                   static_cast<f32>(std::cos(0.3490658476948738)), 50.0f, 0.0f, 0.0f);    // 0x726F0
    SetTuningLanes(kfVehicle_AvoidancePassingFactor_Constants,
                   4.0f, 10.0f, 10.0f, 3.0f);                                             // 0x72770
    SetTuningLanes(kfVehicle_AvoidanceCone_CosAngle_Length_RecipYScale_W,
                   static_cast<f32>(std::cos(0.1745329238474369)), 15.0f, 0.25f, 0.0f);   // 0x72780
    SetTuningLanes(kfVehicle_Avoidance_Constants,
                   10.0f, 50.0f, 0.0f, 0.0f);                                             // 0x72790
    SetTuningLanes(kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W,
                   static_cast<f32>(std::cos(0.1745329238474369)), 30.0f, 0.25f, 0.0f);   // 0x727A0

    // 0x827407B8..0x827407CC is `for (4) maJobs[i].Construct()`; 0x827407D8/0x827407F8 is
    // `li r8, 4; stw r8, 0x2A00(r31)`. The stub Constructs live in the host job table in
    // _wT2_04.cpp while [MEMBER HOLE 5] is open (blocker measured in the header).
    muNumUpdateVehiclesJobs = KU_MAX_JOBS;

    // The console inlines EventReceiverQueue<4096,16>::Construct at 0x827407E0..0x82740844
    // (mpBuffer = &maBuffer, miCapacity = 0x1000, miAlignment = 0x10, miCount = 0, then the
    // alignment fix-up Clear() repeats). This is the de-inlined form, and it is what binds the
    // queue at construction time rather than in _wQ7_02.cpp's Prepare stage-0 safety net.
    mReceiverQueue.Construct();

    mStreamer.Construct();

    // ⭐⭐ 0x8274087C..0x8274088C -- FuzzyBehaviourLogic::Construct(this+0x71860, this+0x72710),
    // i.e. mFuzzyBehaviours.Construct(&mTweakValues). LANDED wave 3 round 3; it was inside the
    // gate below and the gate's "no body in this tree" claim was STALE -- the body has been in
    // BrnTrafficFuzzyLogicBehaviours.cpp (mounted, build_game_exe.bat:2176) all along.
    // WHAT IT COST: with this call missing, every fuzzy envelope AND all 21 mega-tweek constants
    // stayed at their zero-init values, so ProcessParamRules returned six zero scores. The
    // action pick (UpdateParams_PrecalcBehaviourParams @0x827180FC) seeds best = 0.0 / index = 0
    // and only replaces on `>`, so a six-zero score vector elects action 0 == DRIVE_AROUND_
    // OBSTRUCTION -- which is exactly the boot's `[T3-behaviour] histogram [2]=5462`, with the
    // case-0 signature stopDist 2.0 and targetSpeed (rand+1)*scale == 4.586. The NORMAL arm
    // (action 5) can only win once mNormalScore lane x is its real 0.2.
    mFuzzyBehaviours.Construct(&mTweakValues);

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Construct sub-object legs TrafficEntitySerialiser::Construct, "
            "CgsResource::BaseResourcePtr::CreateFromHandle(mpData-adjacent slot), "
            "the 102800-byte maTrafficPhysicsInfoList memset "
            "(RECURRING-BUG CLASS (a) LIVES HERE, not in the 25 Constructs: this memset -- or "
            "Construct's own tail -- is the only remaining candidate for whoever binds "
            "mDetachedPartQueue.mpEvents, and neither has been read yet), the 32-slot showtime "
            "list seed, the DebugComponent and Logger allocations, the twenty "
            "CgsDev::PerfMonCpu::AddMonitor registrations and DebugRenderStreamReader::"
            "Construct -- none of those callees has a body or a usable declaration in this tree");
    }

    // The 25 physical-traffic scratch records; body in BrnTrafficEntityModule_wT1_03.cpp. The
    // console's literal is a sign-extended -1, the same 16 bits the u16 member takes.
    //
    // ORDER WARNING: the console memsets the 102,800-byte array and THEN runs these Constructs,
    // and that memset is gated above. So every field Construct does not touch is zero on the
    // console and is whatever the host storage holds here. Every reader of those fields is a
    // gated physical-traffic leg, so the loop is safe to run without the memset, but the record
    // is not initialised.
    for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES; luSlot++ )
    {
        maTrafficPhysicsInfoList[luSlot].Construct(
            static_cast< s32 >( TrafficPhysicsInfo::KU16_NO_OWNING_VEHICLE ) );
    }

    // The sim box and render caps. The source vector at .data 0x8300CF10 is
    // { 195.0f, 395.0f, 62500.0f, 160000.0f }, seeded by an unnamed MSVC dynamic-initialiser
    // thunk at 0x82C66F18 (not a function in the IDA database, so invisible to per-function
    // export scans). Lanes 2 and 3 corroborate the naming: 62500 == 250^2, 160000 == 400^2.
    //
    // Construct decides which lane goes where, no inference:
    //     0x8274079C  vspltw v0, v0, 0          ; SPLAT LANE 0 -> {195,195,195,195}
    //     0x827407A0  stvx128 v0, r31, r10      ; 0x713B0 == mfTrafficSimRadius
    //     0x827407A4  lfs f0, +8(r11)           ; LANE 2 == 62500
    //     0x827407AC  stfsx f0, r31, r9         ; 0x713C4 == mfRenderCullDistanceSq
    //     0x827407B0  stbx r30, r31, r7         ; 0x713C8 == mbInOfflineCarSelect (r30 == 0)
    //     0x827407B4  stwx r11, r31, r8         ; 0x713C0 == muMaxVehiclesToRender = 32
    // So mfTrafficSimRadius is a splat of lane 0, not the raw vector; lanes 1 and 3 are read by
    // other functions and never reach this member. A zero here would collapse the sim box to
    // one Pvs cell without ever producing a non-finite value.
    // 195 m is the half-extent of the box UpdateRaceCarHulls builds around the player, which at
    // the shipped Pvs cell size gives the handful of hulls its "> 4" assert polices.
    mfTrafficSimRadius.x = 195.0f;
    mfTrafficSimRadius.y = 195.0f;
    mfTrafficSimRadius.z = 195.0f;
    mfTrafficSimRadius.w = 195.0f;

    muMaxVehiclesToRender  = 32;
    mfRenderCullDistanceSq = 62500.0f;   // == 250.0f * 250.0f
    mbInOfflineCarSelect   = false;

    // ---- the debug flag defaults, measured (0x82740C58..0x82740D2C) ---------------------
    mbDEBUGEnablePressureSystem = true;    // 0x72868 stbx r27 (r27 == 1)
    mbDEBUGEnableAvoidance      = true;    // 0x72869 stbx r27
    mbDEBUGTestSympCrash        = false;   // 0x7286A stbx r30 (r30 == 0 for the whole body)
    mbDEBUGRenderContacts       = false;   // 0x7286B
    mpaDEBUGVehicleFuzzyLogic   = 0;       // 0x7286C stwx r30
    muDEBUGVehicleFuzzyLogicCount = 0;     // 0x72870 stwx r30
    mbDEBUGShowtimeStuff        = false;   // 0x72874
    mbDEBUGOverrideJunctionFUP  = false;   // 0x72875
    mbDEBUGFakeShowtime         = false;   // 0x72876
    mbDEBUGPickVehicleFromCamera= false;   // 0x72877
    muDEBUGPickedVehicle        = 0xFFFFFFFFu; // 0x72878 stwx r10 (r10 == -1)
    mbDEBUGPick_StopVehicle     = false;   // 0x7287C
    mbDEBUGPick_DontStopForPickedVehicle = false; // 0x7287D
    mbDEBUGTurnTrafficOff       = false;   // 0x7287E  <-- THE SHIP DEFAULT. NOT the leak's `true`.

    mbDEBUGStopTrafficMoving          = false;  // 0x727B8
    miDEBUGOverrideVehicleToSpawn     = -1;     // 0x727C0 stwx r29 (r29 == -1)
    mbDEBUGDontRenderMeshes           = false;  // 0x727C8
    mbDEBUGAllowAnarchy               = false;  // 0x727C9
    mfDEBUGTrafficLightTimeMultiplier = 1.0f;   // 0x727D0 stfsx flt_82001C98 == 1.0f
    mbDEBUGEnableKillzones            = true;   // 0x727D4 stbx r27
    // LOAD-BEARING: PickVehicleToSpawn gates on `miDEBUGFlowtypeOverride >= 0`, so dropping
    // this store leaves the zero-initialised member reading as flow type 0 and forces every
    // spawn in the world onto that flow type's vehicle mix. Nothing asserts; the city just
    // looks like it owns one kind of car.
    miDEBUGFlowtypeOverride           = -1;     // 0x727D8 stwx r29
    mDEBUGRecentlyFiredKillZones.Clear();       // 0x72860 stwx r30 -- the live-count word

    // ---- the remaining plain stores of the console's body, in offset order ---------------
    mbAtStartLineSoProtectRaceCarsFromTraffic = false; // 0x717E1
    mbPlayerIsPowerParking                    = false; // 0x717E5
    mbShowtimePlayerOnGround                  = false; // 0x717E6
    mbWaitingForStreaming                     = false; // 0x7180E
    mbNeedToKillAllZombies                    = false; // 0x7180F
    mfShowtimeTrafficDensityScale             = 1.0f;  // 0x71824 stfsx flt_82001C98
    muLastParamCalculated                     = 0;     // 0x71830

    muShowtimeVehicleInfoCount = 0;                    // 0x72480
    mShowtimePlayerLandingPos2D.SetZero();             // 0x72490
    mShowtimePlayerGroundPos.SetZero();                // 0x724A0

    mbNetworkHasDetectedDivergence = false;            // 0x72B54
    mbHullSyncDivergence           = false;            // 0x725EC

    // ---- the density seed (0x827413E0: stfsx flt_82001C98 -> this + 0x71810) -------------
    mfBaseDensityScale = 1.0f;

    ResetEventData();
    Reset();

    // ---- 96 per-vehicle-type runtimes (0x8274142C..0x82741440, `addi r26,r26,0x80`) -------
    for (u32 luVehicleType = 0; luVehicleType < KU_MAX_VEHICLE_TYPES; ++luVehicleType)
    {
        maVehicleTypeRuntime[luVehicleType].Construct();
    }

    // 0x82741448 `stw r30, 0x2FC(r31)`.
    meEmptyTrafficPoolState = E_EMPTYTRAFFICPOOLSTATE_IDLE;

    // The console's tail stores (0x827414D4..0x8274175C): everything Construct re-seeds after
    // ResetEventData and Reset have run. The crash-slider values and showtime timers are the
    // same numbers ResetEventData writes; mfJunctionFUP's partner is flt_82001C98 == 1.0f, the
    // datum mfBaseDensityScale is also seeded from.
    mfCrashSliderCrashScore       = 0.0f;   // 0x72370
    mfCrashSliderCrashScoreDecay  = 0.5f;   // 0x72374
    mfCrashSliderCrashScoreFactor = 0.80000001f; // 0x72378
    mfCrashSliderFinalValue       = 0.0f;   // 0x7237C

    mfShowtimeTimer               = 0.0f;   // 0x724B0
    mfShowtimeTimeNextCrashSpike  = 0.0f;   // 0x724B4
    mfShowtimeMisBounceTimer      = 0.0f;   // 0x724BC
    mfPlayerIdleTime              = 0.0f;   // 0x72524

    mAveragePhysicalCentre.SetZero();       // 0x725D0
    mfJunctionFUP                          = 0.0f; // 0x725E0
    mfJunctionFUP_TimeTillNextPhysicalKill = 1.0f; // 0x725E4
    mbTrafficIsHidden                        = false; // 0x725E8
    mbDontCreateVehiclesNearAnyPlayers       = false; // 0x725E9
    mbDontCreateStaticVehiclesNearAnyPlayers = false; // 0x725EA
    mbInPictureParadise                      = false; // 0x725EB

    {
        // GATE: two stores inside the un-emitted DWARF :776/:777 window, i.e. the
        // BrnReplays::TrafficEntitySerialiser member the console registers at `this + 468160`.
        // Construct writes zero to +0x72520 and ONE to +0x72521, while EnterReplay @0x827081D8
        // and LeaveReplay @0x82708248 both write zero to +0x72521. A flag that constructs to 1
        // and clears on entering AND leaving replay is not mbInReplay, so it stays unnamed.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Construct stores +0x72520 = 0 and +0x72521 = 1 -- both inside the replay "
            "serialiser's DWARF un-emitted :776/:777 window; no attested member name "
            "(the same byte EnterReplay/LeaveReplay clear)");
    }

    // 0x82741450..0x82741460. mLocalPlayerDirection @+0x713E0 is NOT written here; the console
    // leaves it to PostPhysicsUpdate's tail.
    mLocalPlayerPosition.SetZero();
    meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    // The console's very last store, immediately before the epilogue: `li r11, 1 ; stb r11,
    // 4(r31)` @0x82741758. Byte +4 is CgsModule::Module::mbIsNewModule, which the base
    // Construct at the top of this function set to zero and the traffic module flips back.
    //
    // LOAD-BEARING for Prepare stage 1: ModuleSingleBuffered::Prepare @0x8286E7A0 tests that
    // byte at every stage. Non-zero skips the old-style DataStructure ladder and returns 1;
    // zero calls CreateInputDataStructure through the vtable, which this class does not
    // override, so the base placeholder returns null and Prepare returns FALSE every frame,
    // forever. That is a boot hang at WorldModule::Prepare's traffic stage.
    mbIsNewModule = true;

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-static] one-shot construction tell.  DELETE-WHEN-STABLE.
        *lpDiag << "[T1-static] TrafficEntityModule::Construct done, mbDEBUGTurnTrafficOff="
                << (mbDEBUGTurnTrafficOff ? 1 : 0)
                << " mfTrafficAmountScale*1000=" << static_cast<s32>(mfTrafficAmountScale * 1000.0f)
                << " freeStaticParams=" << static_cast<s32>(mFreeStaticParamStack.GetLength())
                << "\n";
    }
}

}
