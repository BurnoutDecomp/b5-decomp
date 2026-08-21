// ============================================================================
// BrnTrafficEntityModule_wT1_01.cpp -- wave T1 (parked traffic cars), cluster C4:
// THE SPAWN LEGS.
//
// WHAT THIS FILE IS. The console's traffic module reaches a parked car through a single
// chain that starts in the module's own state machine and ends in a transform written into
// maVehicleTransforms:
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
//         Vehicle::InitialiseAsStatic @0x827567F0 (C3)
//         SetVehicleTransform  @0x827142B8        -> the car now HAS a place in the world
//     E_STARTINGUPSTATE_WAITING_FOR_STREAMING
//       EnterRunningState      @0x827080E8
//
// ⭐⭐ AND, SINCE 2026-08-21 (wave T1 ROUND 4, item 1), THE SAME CHAIN IN STEADY STATE. The
// ladder above runs ONCE, on the POPULATING frame. The console then re-runs its top half
// every decision frame for as long as the module is RUNNING, through a path this file's
// PostPhysicsUpdate now dispatches into:
//
//   PostPhysicsUpdate @0x8274E6D0, state E_STATE_RUNNING
//     if (!IsPaused() && !simPaused)
//       IsDecisionFrame() ? UpdateDecisionFrame @0x8274E508 : UpdateNonDecisionFrame @0x8274C1A8
//         (both landed in BrnTrafficEntityModule_wT1_06.cpp)
//         UpdateDecisionFrame -> RecalculateActiveHulls / SpawnNewTraffic /
//                                StaticVehicles_UpdateStaticParams /
//                                StaticVehicles_UpdateVehicles   -- all bodied HERE
//
// and the flag that gate turns on (mbDecisionFrame) is produced by UpdateTimers @0x82715858,
// called from PreSceneUpdate's RUNNING arm in _wT1_02.cpp. Before round 4 that flag had no
// writer in this tree at all, so the whole steady-state half was unreachable.
//
// Every function on the STARTING_UP chain is bodied here. Where a leg of a bodied function reaches
// code or data this cluster cannot recover, the leg is a NAMED one-shot gate -- the
// convention BrnTrafficEntityModule_wQ7_02.cpp established in this directory -- never an
// invented body and never a silent omission.
//
// ---------------------------------------------------------------------------
// THE SHIP DEFAULT OF mbDEBUGTurnTrafficOff IS **false** -- THE LEAK IS WRONG.
//
// The scout report flagged Feb-2007's `mbDEBUGTurnTrafficOff = true` as a trap. Measured
// from the SHIP Construct @0x82740220, it is FALSE:
//     0x8274024C  li    r30, 0                 ; r30 stays 0 for the whole function
//     0x82740D10  ori   r8, r8, 0x287E         ; 0x7287E == 469118 == mbDEBUGTurnTrafficOff
//     0x82740D2C  stbx  r30, r31, r8           ; = 0
// The neighbouring debug bools in the same store run pin the offset with zero slack
// (0x72877 mbDEBUGPickVehicleFromCamera = 0, 0x72878 muDEBUGPickedVehicle = -1,
//  0x7287C/0x7287D mbDEBUGPick_* = 0, 0x7287E mbDEBUGTurnTrafficOff = 0), and the same run
// shows the two debug bools the ship turns ON: 0x72868 mbDEBUGEnablePressureSystem = 1 and
// 0x72869 mbDEBUGEnableAvoidance = 1 (`stbx r27` with r27 = 1 from 0x82740988).
// SHIP TRAFFIC IS ON BY DEFAULT. Nothing in this file copies the leak's dev default.
//
// ---------------------------------------------------------------------------
// THE OTHER KNOB: mfTrafficAmountScale. FillNewHull's very first act is
// `if (mfTrafficAmountScale == 0.0f) return;` (0x82743634 `lfs f0,0(r14)` with
// r14 == this+464924, compared against flt_82001CC0 == 0.0f). So a density of zero kills
// the PARKED half too -- exactly as the scout warned. The value chain is
//   Construct   : mfBaseDensityScale = flt_82001C98 == 1.0f   (0x827413E0 stfsx -> 0x71810)
//   ResetEventData: mfGameModeDensityScale = mfBaseDensityScale; mfTrafficAmountScale = same
//   UpdateDensity (every PostPhysicsUpdate): mfTrafficAmountScale = mfGameModeDensityScale
// all three of which are bodied below, so a default-constructed module runs at density 1.0.
// The DRIVING half of FillNewHull is suppressed by an explicit NAMED gate, never by zeroing
// the density.
//
// ---------------------------------------------------------------------------
// SOURCES. X360 ARTIST pseudocode + assembly (.ida-exports/BURNOUT_X360_ARTIST.XEX) is rung
// 1 and arbitrates every offset, branch and store below; DecFIGS DWARF supplies declaration
// shape; the Feb-2007 leak is used ONLY where this file says so in-line (and never for a
// function ARTIST exports). Layout is HOST-NATIVE throughout: no member is reached by an
// X360 byte offset, and the console displacements quoted in the comments are attestation of
// WHICH member a line resolves to, nothing more.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // TrafficData
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                // Pvs (UpdateRaceCarHulls' grid walk)
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // Hull, StaticTrafficVehicle
#include "SharedClasses/Traffic/BrnTrafficFlowType.h"           // FlowType
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"        // VehicleTypeData / UpdateData
#include "SharedClasses/Traffic/BrnTrafficVehicleAsset.h"       // VehicleAsset

// RCEntityActiveRaceCarOutputInterface -- the player-car snapshot UpdateRaceCarHulls builds
// its sim box around and PostPhysicsUpdate's tail latches meLocalPlayerIndex from.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT
#include "GameShared/GameClasses/Algorithms/CgsShuffle.h"        // CgsAlgorithms::Shuffle (Reset pool shuffles)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // gpDebugPrint / gxMessageFilterFlags

#include "rw/math/vpu/matrix44affine_operation.h"               // rw::math::vpu::IsValid

#include <cstdlib>   // getenv

namespace BrnTraffic
{
namespace
{
    // ------------------------------------------------------------------------------------
    // NAMED LEG GATE. One line per console leg that has no body in this tree, logged once
    // per process. Same shape (and the same "[FLAG PC partial gate]" tail) as
    // BrnTrafficEntityModule_wQ7_02.cpp's LogMissingLeg, so a boot log reads as one stream.
    // [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------------------
    // DELETE-WHEN-STABLE bring-up probes. Gated on the BRN_TRAFFIC_DIAG environment knob
    // (this wave's own knob -- the wQ7 files borrowed BRN_PROP_DIAG, which is what cluster
    // C8 exists to retire). Every probe below is either one-shot or value-latched, so a
    // steady state costs one env lookup per call and nothing else.
    // [DIAG] NOT IN THE X360 BINARY.
    // ------------------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------------------
    // The X360 immediates this file needs that are NOT rodata reads -- each one is an
    // instruction operand, i.e. recovered, not guessed.
    // ------------------------------------------------------------------------------------

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
// THE SHIP CACHES WHAT THE LEAK COMPUTED. Feb-2007 has an inline predicate
// `AllowDivergentBehaviour() { return !IsPlayingOnlineGameMode(); }`; ship hoists it into
// the member at +0x717E7 here and ORs in the showtime case. That member is the single
// biggest behavioural switch on this whole chain -- OFFLINE IT IS TRUE, which is what makes
// PostPhysicsUpdate's POPULATING arm create vehicles locally at all (see the arm below).
// ----------------------------------------------------------------------------
void TrafficEntityModule::EnterStartingUpState()
{
    CGS_ASSERT(meState == E_STATE_INVALID, "meState == E_STATE_INVALID");

    meState           = E_STATE_STARTING_UP;
    meStartingUpState = E_STARTINGUPSTATE_FIRST;

    mbAllowDivergentBehaviour = (!mbIsOnlineGameMode) || mbPlayingShowtimeMode;

    {
        // The console's last store is a byte written THROUGH mpLogger (+0x727B4, the pointer
        // Construct fills from the debug allocator). BrnTrafficLogger.cpp does not compile in
        // this tree, so the Logger type has no usable declaration here.
        //
        // ⭐ REASON RE-MEASURED 2026-08-21 (wave T1 round 4 sweep, item 4). Two earlier
        // spellings of this banner were both wrong about WHY, and the difference decides who
        // can fix it. It is NOT "C2027" (round 1's) and NOT "a memcpy called with two
        // arguments at line 296" (round 3's). Running the gate today, the FIRST errors are
        // REDEFINITIONS: BrnTrafficLogger.cpp re-declares `KU_MAX_PARAMS` (:32, against
        // BrnTrafficConstants.h:68), `class HullRuntime` (:58, against BrnTrafficHullRuntime.h:46),
        // `class ParamTransform` (:81, against BrnTrafficParam.h:196) and `class
        // TrafficEntityModule` (:90, against BrnTrafficEntityModule.h:414). The C2027s that
        // follow are the CASCADE from that last one -- the local fork is incomplete, so every
        // member use fails. In other words this file is a textbook instance of the AGENTS.md
        // "don't locally redefine a type that has a real home" rule, and the fix is to delete
        // its four local declarations and include the four real headers -- NOT to chase a
        // memcpy. That is a one-file job for whoever owns the logger, plus a mount decision;
        // it would un-gate the three mpLogger legs in this file.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "EnterStartingUpState mpLogger-><leading byte> = mbAllowDivergentBehaviour "
            "(X360 0x827080CC) -- BrnTrafficLogger.cpp is unmounted and does not compile. "
            "MEASURED CAUSE (round 4): it LOCALLY REDECLARES KU_MAX_PARAMS, HullRuntime, "
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

// ----------------------------------------------------------------------------
// TrafficEntityModule::SetVehicleTransform  @ 0x827142B8   (header baked line 2491, .cpp 2492)
//
//   assert(luIndex < KU_MAX_TOTAL_TRAFFIC);
//   assert(RwMath::IsValid(lTransform));
//   maVehicleTransforms[luIndex] = lTransform;      ; (luIndex + 0x7B2) << 6, four stvx128
//
// The console spells IsValid as a per-row / per-lane `vcmpeqfp` self-equality cascade over
// the x/y/z lanes of all four rows, ANDed together -- which is exactly what the committed
// rw::math::vpu::IsValid(Matrix44Affine) reduces to. 0x7B2 * 64 == 126080 == the end of
// maVehicleAxles (87680 + 600*64), i.e. the transform array immediately follows it; on the
// host that is `&maVehicleTransforms[luIndex]` with no byte arithmetic at all.
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
// The FIRST line is the one the parked chain depends on and it is exact. The online arm is
// gated: its two endpoints live in .data at 0x82F2FDDC / 0x82F2FDE0 (initialised data, no
// writer anywhere in the export set -- the same class of unrecoverable constant C3 parked
// KF_VEHICLE_UPDATE_MATRIX_OLD_UP_FACTOR for), so writing it would mean inventing the
// density curve. It is unreachable offline anyway: mbAllowDivergentBehaviour is TRUE
// whenever !mbIsOnlineGameMode (EnterStartingUpState above), so the branch is dead in every
// single-player boot. 0.015873017f == 1/63 is an instruction immediate, not a guess, and is
// recorded here so the next agent only has to dump two floats.
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
// ⚠️ FAITHFUL READ-AFTER-WRITE. The console loads mxFlags, applies the kill mask, STORES it,
// and only then tests bit 0x40 for "divorced" (0x82721DB8..0x82721DD0):
//     lbz   r11, 3(r30)
//     andi. r11, r11, 0x8C
//     ori   r11, r11, 2
//     stb   r11, 3(r30)
//     rlwinm r11, r11, 0,25,25      <- bit 0x40 OF THE POST-KILL VALUE
// The kill mask clears 0x40, so that test can never be true and both "divorced" arms are
// unreachable in the shipped binary. This is reproduced exactly rather than "fixed": the
// zombie flag IS captured before the kill (the console does that too, `extrwi r26,r11,1,26`
// at 0x82721D5C), so only the divorced arms are dead, and turning them live would ADD
// behaviour the binary does not have.
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
            // _wT1_06.cpp) this is the EXPECTED long-drive end state, not a defect -- but it
            // is the line that explains "parked cars stopped appearing", so it is worth one
            // print. DELETE-WHEN-STABLE.
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
        // [T1-static] the GENERATE roll: the first ten slots taken, then every 25th. This is
        // the "StaticVehicles_Generate rolls" line the round-4 brief asked for -- it is where
        // FillNewHull's mExistsAtAllChance roll turned into a real param, and it names the
        // slot / vehicle type / hull / index-on-hull that InitialiseAsStatic will use next.
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
// TrafficEntityModule::StaticVehicles_CreateNewVehicles  @ 0x827229F0   *** THE PARKED-CAR MAKER ***
//
// SHIP RENAME: Feb-2007 calls this StaticVehicles_MakeAliveTheDeadOnesWithAliveParams and it
// has no race-car proximity rejection. Ship renamed it AND added that rejection; the shape
// below is the SHIP's.
//
// Whole body:
//   if (mbWaitingForStreaming) return;                          ; lbz +464910
//   Array<Vector3,8> laPlayerPositions;
//   if (mbDontCreateStaticVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour)  [GATED]
//       ... collect every active race car's position ...
//   for (luStatic = 0; luStatic < KU_MAX_STATIC_TRAFFIC; ++luStatic)
//   {
//       if (!param.IsAlive() || param.IsZombie() || vehicle->IsAlive()) continue;
//       lpRuntime  = GetVehicleTypeRuntime(param.muVehicleType);
//       lpHull     = GetHull(param.GetHull());
//       lpRecord   = lpHull->GetStaticVehicle(param.GetIndexInHull());
//       lTransform = lpRecord->mTransform;                       ; four lvx128 into a stack copy
//       lTransform.SetW( lTransform.GetW() - lTransform.GetY() );  ; THE ONE-METRE DROP
//       if (mbDontCreateStaticVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour) [GATED]
//           ... reject + zombie/divorce when within lane 1 of unk_8300CF70 of any player ...
//       luVehicle = luStatic + KU_STATIC_TRAFFIC_OFFSET;
//       assert(luVehicle < KU_MAX_TOTAL_TRAFFIC);                ; header baked 2475
//       vehicle->InitialiseAsStatic(&maVehicleAxles[luVehicle], lOutMatrix,
//                                   mEffectRand.RandomFloat(),
//                                   param.muVehicleType, lpRuntime,
//                                   &mpData->mpaVehicleTypesUpdate[param.muVehicleType],
//                                   lTransform, luVehicle, mVehicleSoaData);
//       SetVehicleTransform(luVehicle, lOutMatrix);
//   }
//   mbDontCreateStaticVehiclesNearAnyPlayers = false;
//
// THE ARGUMENT MAP IS READ OFF THE PROLOGUE, NOT HEX-RAYS (0x82722E50..0x82722ED8). Hex-Rays
// renders the 7th argument as literal `0`; the asm builds it as
//     lbz r31, 1(r24)                       ; param.muVehicleType
//     slwi r9, r31, 2 ; add r11, r31, r9 ; slwi r11, r11, 2      ; * 20
//     lwz  r10, 0x30(TrafficData)           ; mpaVehicleTypesUpdate  (X360 +0x30)
//     add  r27, r10, r11                    ; -> &mpaVehicleTypesUpdate[type]
// i.e. it is a REAL pointer to the 20-byte VehicleTypeUpdateData whose mfWheelRadius
// VehicleAxles::SetFromVehicleTransform then reads (`lfs 0(r6)` -- C3's finding). Passing
// Hex-Rays' zero would have null-dereferenced on the very first parked car.
// The register that LOOKS like the 4th argument (r6) is the PPC float-arg GPR skip slot for
// f1 -- the same trap C3 documented on InitialiseAsStatic itself.
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
        // GATE 1 of 2 for this function. ⭐ RE-DERIVED 2026-08-21 (wave T1 round 2, cluster
        // R2A): BOTH of round 1's stated blockers are now closed --
        //   (a) InputBuffer_PostPhysics::GetActiveRaceCarOutputInterface() (X360 0x82711850)
        //       EXISTS, declared at BrnTrafficEntityModuleIO.h DWARF :358 and bodied in
        //       BrnTrafficEntityModuleIO.cpp this round;
        //   (b) the rejection radius is RECOVERED: unk_8300CF70 = { 6400.0f, 900.0f, 0, 0 }
        //       from the unnamed dyn-init thunk at 0x82C66E98, so `vspltw lane 1` == 900.0f
        //       == 30 m squared (lane 0 is 6400 == 80^2, read elsewhere).
        // -- AND YET THE LEG STAYS GATED, DELIBERATELY. It is OFFLINE-DEAD by construction:
        // its guard is `mbDontCreateStaticVehiclesNearAnyPlayers && !mbAllowDivergentBehaviour`
        // and EnterStartingUpState sets mbAllowDivergentBehaviour = !mbIsOnlineGameMode ||
        // mbPlayingShowtimeMode, so offline the guard can never be true and the block below
        // is unreachable. Landing it would add an untestable online-only path (its collection
        // half also walks every ACTIVE race car, i.e. the rival slots this build never
        // populates) to a wave whose goal is parked cars offline. Round 2's spec keeps it
        // parked for exactly that reason; the two facts above are recorded here so the wave
        // that DOES own online traffic can write it without another recovery pass.
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

        // ⭐⭐ THE ONE-METRE DROP -- `lTransform.SetW( lTransform.GetW() - lTransform.GetY() )`.
        // RESTORED 2026-08-21 (wave T1 round 5). Round 1..4 copied the record transform
        // verbatim and every parked car hung ~1.03 m in the air.
        //
        // THE AUTHORED RECORD IS DELIBERATELY ONE METRE HIGH. Measured, not argued: for all
        // 583 of the 875 shipped StaticTrafficVehicle records that have a WORLDCOL surface
        // directly beneath them, recordY - groundY is +1.026 m median (p25 +1.014, p75 +1.049;
        // 505 of them inside [+0.9,+1.1]). The car MODEL's origin is the wheel-contact plane
        // -- from the type's own StreamedDeformationSpec, wheel centre y -0.4337 in handling-
        // body space, wheel radius 0.5*mScale.y == 0.325, and mCarModelSpaceToHandlingBody-
        // SpaceTransform's translation y -0.7557, so contact sits at car-model y -0.003. So the
        // record has to come DOWN by one unit of its own up axis before it becomes a render
        // transform, and it is this line, not the axle seeding, that does it.
        //
        // SHIP ASM, 0x82722CD4..0x82722D20, store for store (r3 == the record):
        //     lvx128  v0,  r0, r3        ; row 0  xAxis                 -> stack var_1B0
        //     lvx128  v0,  r3, 0x10      ; row 1  yAxis (the UP row)    -> stack var_1A0
        //     lvx128  v13, r3, 0x20      ; row 2  zAxis                 -> stack var_190
        //     lvx128  v13, r3, 0x30      ; row 3  wAxis (the position)
        //     vsubfp  v0,  v13, v0       ; wAxis - yAxis    <-- v0 STILL HOLDS ROW 1
        //     stvx128 v13, r0, var_180   ; the raw wAxis is stored...
        //     stvx128 v0,  r0, var_180   ; ...and immediately overwritten by the difference
        // The Feb-2007 original spells the identical statement inline in
        // StaticVehicles_MakeAliveTheDeadOnesWithAliveParams (leak BrnTrafficEntityModule.cpp
        // :4722), so leak and ship agree and the ship did NOT drop the line when it renamed
        // the function. The record's yAxis is a UNIT up vector (hull 122 record 3 reads
        // (0.0031, 1.0000, 0.0000)), which is why the correction is one metre and why it
        // follows the car's own tilt on a banked road instead of being a world-Y constant.
        //
        // The GATED player-proximity leg below differences THIS transform's position, not the
        // raw record's (the console reads back var_180 at 0x82722D90), so the drop belongs
        // before it, exactly where it sits.
        Matrix44Affine lTransform = lpRecord->mTransform;
        lTransform.wAxis = lTransform.wAxis - lTransform.yAxis;

        if (lbRejectNearPlayers)
        {
            // Second half of GATE 1: the reject itself (SetZombie + SetDivorced). Not
            // emitted, for the reasons above -- and emitting it with a guessed radius would
            // silently delete parked cars.
            continue;
        }

        const u32 luVehicle = luStatic + KU_STATIC_TRAFFIC_OFFSET;
        CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");
        CGS_ASSERT(lrParam.IsAlive(), "IsAlive()");   // BrnTrafficStaticParam.h:121

        const u8  luVehicleType = lrParam.muVehicleType;

        // ⚠️ DO NOT ADD A `- 1.0f` HERE. The console's expansion at
        // 0x82722E7C..0x82722ED4 is: read the ring slot (a [1,2) float), refill that slot
        // from the OLD seed's high word, step the LCG, advance the cursor, then
        // `fsubs f1, f0, f31` with f31 == flt_82001C98 == 1.0f. That IS
        // CgsNumeric::Random::RandomFloat() in this tree -- its committed body
        // (CgsRandom.cpp) already ends on `lfRandomFractionPlusOne - 1.0f` and returns a
        // [0,1) value. Subtracting again would hand InitialiseAsStatic a NEGATIVE
        // mfRandomVal, which nothing would assert on: every consumer treats it as a 0..1
        // phase/jitter, so a [-1,0) value would silently invert wheel-rot and headlight
        // phase on every parked car. Caught by reading the committed callee, not the asm.
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
            // [T1-static] FIRST InitialiseAsStatic, one-shot, with slot + type + the world
            // POSITION the console seated (lOutMatrix's translation row, i.e. what
            // CreateNewVehicleEntities will hand AddEntity next frame). If this position is
            // (0,0,0) the record's transform never arrived; if it is plausible but nothing
            // renders, the fault is downstream of the module. DELETE-WHEN-STABLE.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                const Vector3& lrPos = lOutMatrix.wAxis;
                *lpDiag << "[T1-static] FIRST InitialiseAsStatic vehicle=" << static_cast<s32>(luVehicle)
                        << " (staticSlot=" << static_cast<s32>(luStatic) << ")"
                        << " type=" << static_cast<s32>(luVehicleType)
                        << " pos=(" << lrPos.x << ", " << lrPos.y << ", " << lrPos.z << ")\n";

                // [T1-height] the one-metre drop, both ends of it, so a future reader can see
                // in one line whether 0x82722D14's `vsubfp wAxis, yAxis` is being applied.
                // recordY - droppedY must equal the record's up-vector length (1.0 m on flat
                // road); the authored records sit +1.026 m median above the WORLDCOL surface.
                // DELETE-WHEN-STABLE.
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
// ⭐ SIGNATURE CORRECTED 2026-08-21 (wave T1 round 4, item 1) -- IT TAKES lpInput AND FORWARDS
// IT, and the fix removes a latent null deref. IDA types @0x82722F98 as one-argument, which is
// a Hex-Rays artefact of a PASS-THROUGH: the prologue saves only r3 and never writes r4, so
// whatever r4 held on entry flows straight into StaticVehicles_CreateNewVehicles' second
// parameter. The caller proves what that is -- UpdateDecisionFrame @0x8274E508 does
// `0x8274E61C mr r4, r30` (r30 == lpInput) immediately before the `bl` -- and the DWARF spells
// it out at BrnTrafficEntityModule.h:1839. The old body passed a LITERAL 0, which is inert
// only while StaticVehicles_CreateNewVehicles' race-car proximity arm stays gated; the moment
// the online wave un-gates it, that arm dereferences lpInput.
// ----------------------------------------------------------------------------
void TrafficEntityModule::StaticVehicles_UpdateVehicles(
    const BrnTrafficIO::InputBuffer_PostPhysics* lpInput)
{
    StaticVehicles_CreateNewVehicles(lpInput);

    static bool sbLogged = false;
    LogMissingLeg(sbLogged,
        "StaticVehicles_UpdateVehicles per-vehicle Vehicle::UpdateEffects leg -- "
        "BrnTraffic::Vehicle::UpdateEffects has no declaration or body anywhere in the tree "
        "(C3 landed 64 Vehicle functions; this is not one of them). Headlight/indicator "
        "effect state on parked cars is therefore static, which is correct-looking for a "
        "parked car and wrong only for its blinking-hazard variants");
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
// ⚠️ RECURRING-BUG CLASS (b) CAUGHT HERE. The console reads the showtime comparand with
// `ldx r11, mpaVehicleAssets, assetId*8` -- a full 64-bit load -- and compares `cmpld`
// against the 64-bit literal. An earlier reading of the same site as "the dword at +4"
// would be an X360-big-endian artefact: on this little-endian host the low half of a u64
// lives at +0, not +4. It is done BY VALUE through VehicleAsset::GetVehicleId(), which is
// width- and endian-correct on both.
//
// The 8-bit roll is deliberate and attested: `clrlwi r29, r11, 24` truncates the LCG's high
// word to a byte before the comparison, which is what makes comparing it against the u8
// cumulative-probability table meaningful (the table is 0..255, not 0..100).
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
        // ⚠️ TWO DISTINCT CONSOLE FALLBACKS -- do not collapse them.
        // This one (the ".cpp 9209" assert, LABEL_12) returns a LITERAL ZERO and never
        // touches the type-id table:
        //     0x82723A30  bl   CgsDev__Assert__FireAssert     ; ..., 9209
        //     0x82723A34  bl   CgsDev__Assert__EndAssert
        //     0x82723A38  li   r3, 0                          <-- the return value
        //     0x82723A3C  addi r1, r1, 0xB0 ; b __restgprlr_23
        // The `mpauVehicleTypeIds[0]` fallback is the OTHER path -- the not-acceptable
        // tail at 0x827238CC (`lwz r11,0(r26)` / `lhz r11,0(r11)` / `clrlwi r3,r11,24`),
        // reached only when the big-vehicle/showtime predicate in r29 is 0. Returning it
        // here would also DEREFERENCE a table the console deliberately avoids: LABEL_12 is
        // reached from `if (!*(v19+8))`, i.e. exactly when muNumVehicleTypes == 0 and the
        // array may be empty.
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
//   ---- DRIVING HALF (GATED) : per section, per lane, seed moving vehicles ----
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
// The two `muFlags` bits are exactly the SHIP-ONLY mode gates the scout predicted (the byte
// at +0x43 postdates the Feb-2007 record); cluster C2 landed the field and recorded that
// ARTIST tests only these two bits, so no enumerator is invented for them here either.
// ----------------------------------------------------------------------------
void TrafficEntityModule::FillNewHull(u16 luHull)
{
    const Hull* lpHull = GetHull(luHull);

    if (CgsDev::Log::DebugPrint* lpDiag = TrafficDiagStream())
    {
        // [T1-fill] FIRST VISIT PER HULL, and only per hull -- a 400-bit seen-set keyed by
        // hull index, so a hull that is entered, left and re-entered prints once. This is the
        // line that says the spawn chain reached a real hull with real records in it; a
        // muNumStaticTraffic of 0 here means the hull genuinely has no parked slots, which is
        // a DATA answer, not a code one. DELETE-WHEN-STABLE. [DIAG] NOT IN THE X360 BINARY.
        // (function-scope static of a POD aggregate -> zero-initialised before first use, so
        // no explicit clear is needed or wanted.)
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

    {
        // ---- THE DRIVING HALF, GATED BY NAME (never by density -- see the file banner) ----
        // The console's first loop walks the hull's sections, converts each section's
        // SectionFlow::muVehiclesPerMinute into a spacing along the lane
        // (Section::CalcParamFromStartParamAndDistanceAlongSection) and calls
        // GenerateNewVehicle for each slot. NONE of that chain exists in this tree:
        // Section::CalcParamFromStartParamAndDistanceAlongSection and
        // TrafficEntityModule::GenerateNewVehicle @0x82736528 are both bodiless, and the
        // half depends on the lane-param pool ([MEMBER HOLE 1] ParamNeedToSlowData and
        // [MEMBER HOLE 2] ParamListNode are still not modelled), which is wave 2's scope.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "FillNewHull DRIVING half (per-section generator seeding -> "
            "Section::CalcParamFromStartParamAndDistanceAlongSection + GenerateNewVehicle "
            "@0x82736528) -- WAVE 2. Explicitly gated, NOT suppressed by setting "
            "mfTrafficAmountScale to zero (that would also kill the parked half)");
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
            // ---- THE PROXIMITY CULL, GATED BY NAME ----
            // 0x82743AFC..0x82743B28:
            //     lvx128 v0, r20, 0x728C0        ; the reference position
            //     lvx128 v12, rec, 0x30          ; the record's transform translation row
            //     vsubfp / vmsum3fp128 / vcmpgtfp.  unk_8300CC90 vs distSq -> skip when nearer
            //
            // ⭐ RE-DERIVED 2026-08-21 (wave T1 round 2, cluster R2A). Round 1 recorded TWO
            //   blockers here. ONE IS NOW CLOSED and one still stands, so the gate REMAINS --
            //   with the reason narrowed to a single item.
            //
            //   (b) RETIRED. The squared radius unk_8300CC90 is 1600.0f. It is seeded by an
            //       unnamed MSVC dynamic-initialiser thunk at 0x82C662D0 that computes it as
            //       0x8300CB80 (splat 40.0f, thunk 0x82C66110, source flt_820BA590 == 40.0)
            //       multiplied by itself -- `vmulfp128 v0, v0, v0`, this codebase's `_SQ`
            //       idiom. So the cull radius is 40 m, and 40^2 == 1600. (Recovered by
            //       walking XrefsTo in the .i64: a dyn-init thunk is not a function in the
            //       database, so NO per-function export grep can ever prove one absent --
            //       which is why round 1 read the slot as "un-dumped rodata". Evidence:
            //       scratchpad traffic_wave/recovered_constants.md + thunk_dump*.txt.)
            //
            //   (a) STANDS, and is now the ONLY blocker. The reference position at X360
            //       +0x728C0 falls inside the un-modelled [MEMBER HOLE 6]
            //       `Camera mCameraLastFrame` window of the keystone header, which this
            //       cluster may not edit this round. The window is bounded on both sides:
            //       mfSpeedMultiplier (:879) ends at +0x72884, and mbDEBUGWorstCase (:883)
            //       resumes at +0x729D8 (pinned backwards from miPerfMon_PrePhysicsUpdate
            //       @+0x729FC through :886..:891), so +0x728C0 is 0x3C bytes into a member
            //       with no reconstructed interior.
            //       ⭐ NEW CORROBORATION, from UpdateRaceCarHulls @0x82721460 landed above:
            //       that function reads the IDENTICAL +0x728C0 lane as an OVERRIDE for the
            //       player-car sim-box centre, selected by a flag bit at +0x729D4 (also
            //       inside the window) and re-forced by mpDebugComponent->+0x34. A datum
            //       that lives in a `Camera` member, is switched by a flag in the same
            //       member, and is forced by the debug component, is a debug camera position.
            //       That is a much sharper reading than round 1's "suggestive", but it is
            //       still an inference about a member with no DWARF name, so it is NOT
            //       written.
            //
            // EFFECT OF THE GATE: parked cars near the reference position are NOT culled, so
            // this build spawns a SUPERSET of the console's parked set. That fails loud in
            // the log and visibly (a car may sit where the player starts) rather than
            // silently deleting cars, which is the safer direction for wave 1.
            //
            // UNBLOCKED BY exactly one thing now: modelling `Camera mCameraLastFrame` at
            // DWARF :881 in BrnTrafficEntityModule.h (or naming the lane some other attested
            // way). The radius is ready: `const f32 KF_PARKED_TRAFFIC_CULL_RADIUS_SQ = 1600.0f;`
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "FillNewHull parked-half proximity cull -- ONE blocker left: the reference "
                "position X360 +0x728C0 lies in the un-modelled [MEMBER HOLE 6] "
                "mCameraLastFrame window (no attested name). The squared radius is RECOVERED "
                "(unk_8300CC90 == 1600.0f == 40m^2, dyn-init thunk 0x82C662D0). Not culling "
                "spawns a SUPERSET of the console's parked set");
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
//   ---- GENERATOR HALF (GATED) : tick mafTimesTillNextGeneration, emit driving cars ----
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

    if (muNumGenerators != 0)
    {
        // ---- THE GENERATOR HALF, GATED BY NAME (driving traffic; wave 2) ----
        // 0x82748BB0..: each of muNumGenerators entries counts mafTimesTillNextGeneration
        // down by mfSimTimeStep, and on expiry runs PickVehicleToSpawn ->
        // Section::CalcParamFromStartParamAndDistanceAlongSection ->
        // HullRuntime::GetFirstParamInSection -> Section::CalcDistanceAlongSection ->
        // GenerateNewVehicle -> CalcTimeToNextGeneration. Of that chain only
        // HullRuntime::GetFirstParamInSection exists (C3); the four Section/module functions
        // are bodiless and the whole leg is lane-param work, i.e. wave 2.
        // It is UNREACHABLE today anyway: muNumGenerators is only ever raised by
        // RebuildGeneratorList @0x82742DD0, which has no body either.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "SpawnNewTraffic GENERATOR half (mafTimesTillNextGeneration tick -> "
            "GenerateNewVehicle @0x82736528 / CalcTimeToNextGeneration) -- WAVE 2; "
            "unreachable today because RebuildGeneratorList @0x82742DD0 has no body so "
            "muNumGenerators stays 0");
    }
}

// ============================================================================
// SECTION 7 -- the active-hull set.
// ============================================================================

// ----------------------------------------------------------------------------
// TrafficEntityModule::RecalculateActiveHulls  @ 0x8274C870   *** PARTIAL ***  (.cpp 7275..7367)
//
// REAL here: the five entry asserts, the previous-set snapshot, the rebuild of mActiveHulls
// from maaRaceCarHulls, the two SetDifferences that produce the caller's new/old sets, the
// miDEBUGOverBudgetness reset, and the same rebuild for mActiveHullsForLocalPlayer.
//
// ⭐ ALSO REAL SINCE 2026-08-21 (wave T1 round 2, cluster R2A): UpdateRaceCarHulls
// @0x82721460's offline arm, expanded at its single call site below -- the function that
// FILLS maaRaceCarHulls, i.e. the reason this function has anything to rebuild from. Only
// its two DEBUG sim-centre overrides and its online (predicted-hull-change) arm are gated.
//
// GATED here, each with its own reason:
//   * PredictHullChanges @0x827348E8 -- an EXPORT HOLE (no per-function JSON at all), and
//     online-only (`!mbAllowDivergentBehaviour && meState == E_STATE_RUNNING`).
//   * the baked debug hull-override list (the bool at X360 +0x729F0 selects a 15-entry table
//     at unk_820BA81C) -- that byte sits in the DWARF's un-emitted :892..:895 window between
//     mfDEBUGAvoidance_PassScore and miPerfMon_PreSceneUpdate, so it has no name; C1 parked
//     the identically-un-nameable byte at +0x72521 for the same reason.
//   * the std::_Sort of mActiveHulls -- ::Set<T,N> has no Sort and CgsSet.h is not this
//     cluster's file. Order-only: SetDifference is order-independent, so the new/old sets
//     are identical either way; only the order FillNewHull visits hulls in changes.
//   * mHullsToAddTriggersFor / mHullsToRemoveTriggersFor -- these need
//     ::Array<T,N>::AppendSet, which CgsArray.h does not declare (it has AppendArray only).
//   * the per-old-hull HullRuntime::Release + mUsedHullRuntimeData free and the per-new-hull
//     allocate + HullRuntime::Prepare, plus the trigger/light-manager events they drive.
//     Parked cars do not read HullRuntime (only the driving generator does), so this gate
//     does not block wave 1; SpawnNewTraffic's generator half is gated for the same wave.
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
    // ⭐⭐ GATE RETIRED 2026-08-21 (wave T1 round 2, cluster R2A) -- THE BODY OF
    //     TrafficEntityModule::UpdateRaceCarHulls @0x82721460, ROUND 1's BLOCKER B1.
    //
    // This is the only producer of maaRaceCarHulls, and mActiveHulls is rebuilt from nothing
    // else, so until it ran the new-hull set was always empty and FillNewHull never executed.
    // All three of round 1's blockers are closed: the Pvs overload is declared and bodied
    // (BrnTrafficPvs.h/.cpp, DWARF :64 / :70), mfTrafficSimRadius is seeded from the
    // recovered 0x8300CF10 lane 0 (Construct, below), and
    // InputBuffer_PostPhysics::GetActiveRaceCarOutputInterface() exists (DWARF :358).
    //
    // ⚠️ WHY THE BODY IS EXPANDED HERE INSTEAD OF LIVING AT `TrafficEntityModule::
    //    UpdateRaceCarHulls` IN A PARTFILE. The console has it as a separate member function
    //    with EXACTLY ONE caller (its `xrefs_to` names only RecalculateActiveHulls
    //    @0x8274C870), and it is NOT declared in BrnTrafficEntityModule.h. That header is
    //    owned by a different cluster this round, so the declaration line cannot be added
    //    from here, and a member function cannot be defined without one. Turning it into a
    //    free function taking `TrafficEntityModule&` would be the `Apt*_<verb>` shim
    //    anti-pattern the faithfulness gate exists to catch. Expanding it at its single call
    //    site is therefore the only faithful option available -- and it is REVERSIBLE IN ONE
    //    EDIT. FLAG: OUTLINE-ME. THE EXACT DECLARATION TEXT, verbatim, is this one line:
    //
    //        void UpdateRaceCarHulls( const BrnTrafficIO::InputBuffer_PostPhysics* lpInput );
    //
    //    Add it to BrnTrafficEntityModule.h beside RecalculateActiveHulls (same access
    //    section; the console's only caller is RecalculateActiveHulls @0x8274C870, so it is
    //    private like its caller), then move the block below into a partfile verbatim -- the
    //    body already reads only `lpInput` and members, so no other edit is needed.
    //    It is spelled out HERE, in the tree, ON PURPOSE: the first pass cited an external
    //    wave document for it, and a hand-off that lives outside the tree is a hand-off that
    //    can go missing (the C5 banner in this directory says exactly that -- and it did).
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
    // ⚠️ THE RETURN VALUE OF THE TWO CORNER CALLS IS DISCARDED. The console keeps only the
    // four grid coordinates; the linear cell index it also computes is dead in both cases
    // (its `result` register is simply overwritten). Reproduced -- the calls are still made
    // because their bounds ASSERT is a real side effect.
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

                // ---- the centre of the traffic sim box ------------------------------------
                // Console default (0x82721590..0x827215A4): the PLAYER CAR's world position,
                // read as `GetRaceCarState(lePlay)->mTransform.wAxis` -- the asm builds
                // `addi r11, <state>, 0x1F0 ; lvx128 v126, r11, 0x30`, and 0x1F0 == 496 ==
                // RaceCarState::mTransform with +0x30 == Matrix44Affine::wAxis, the
                // translation row. (The X360 symbol at that call is IDA's
                // `RCEntityActiveRaceCarO...` == GetRaceCarStateMutable @0x8227D690; the
                // const twin GetRaceCarState was ICF-folded onto it, which is exactly what
                // BrnRCEntityActiveRaceCarOutputInterface.cpp's own banner for :220 records.
                // A const interface pointer must use the const form.)
                //
                // ⚠️ ONE ASSERT DIFFERS, deliberately, and it is recorded rather than
                // "fixed": 0x8227D690 carries THREE asserts (index >= 0, index < COUNT, and
                // `IsRaceCarActive(index)` at BrnRaceCarEntityModuleOutputInterface.h:792);
                // the committed const :220 body carries only the two bounds asserts. The
                // third is unreachable here anyway -- IsPlayerCarActive() has already
                // returned true, and the player slot cannot be simultaneously the player's
                // and inactive -- so adding it would be new surface on someone else's file
                // for no behavioural gain. Same note applies to PostPhysicsUpdate's tail,
                // which reaches the same accessor.
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState*
                    lpPlayerState = lpActiveRaceCars->GetRaceCarState(lePlayerCar);
                const Vector3 lSimCentre = lpPlayerState->mTransform.wAxis;

                {
                    // ---- THE TWO DEBUG CENTRE OVERRIDES, GATED BY NAME ----
                    // Both replace lSimCentre with the SAME 16-byte lane at X360 +0x728C0:
                    //   (a) 0x82721554..0x8272158C: `ldx r11, this, 0x729D0` then
                    //       `rlwinm r11,r11,0,17,17` -- i.e. the word at +0x729D4 tested
                    //       against 0x4000 -- selects the lane instead of the car;
                    //   (b) 0x827215A8..0x827215C8: `lwzx r11, this, 0x727B0` (mpDebugComponent,
                    //       pinned by its neighbour mpLogger at +0x727B4 -- see
                    //       EnterStartingUpState) and, if non-null, its byte at +0x34,
                    //       overrides it again.
                    // BOTH the datum (+0x728C0) and the flag word (+0x729D4) fall inside the
                    // un-modelled [MEMBER HOLE 6] `Camera mCameraLastFrame` window of the
                    // keystone header (mfSpeedMultiplier :879 ends at +0x72884;
                    // mbDEBUGWorstCase :883 resumes at +0x729D8, pinned backwards from
                    // miPerfMon_PrePhysicsUpdate @+0x729FC), so neither has an attested member
                    // name and neither can be reached from this cluster's files.
                    //
                    // ⭐ THIS IS ALSO NEW EVIDENCE FOR FillNewHull's parked-half cull, which
                    // reads the identical +0x728C0 lane: a datum that (i) lives inside a
                    // `Camera` member, (ii) is selected by a flag bit that lives inside the
                    // same member, and (iii) is overridden by the DEBUG COMPONENT, is a debug
                    // camera position -- consistent with mbDEBUGPickVehicleFromCamera being
                    // the only consumer [MEMBER HOLE 6]'s banner names. Still not attestation
                    // of a NAME, so still not written.
                    //
                    // EFFECT OF THE GATE: NONE on a normal boot. Both arms are debug-only
                    // overrides of a default this code takes; the live path is the console's
                    // live path.
                    static bool sbLogged = false;
                    LogMissingLeg(sbLogged,
                        "UpdateRaceCarHulls DEBUG sim-centre overrides (the +0x729D4 & 0x4000 "
                        "flag arm and the mpDebugComponent->+0x34 arm, both substituting the "
                        "+0x728C0 lane) -- both the lane and the flag lie inside the "
                        "un-modelled [MEMBER HOLE 6] mCameraLastFrame window, so neither has "
                        "an attested name. DEBUG-ONLY: the live default (the player car's "
                        "position) is taken");
                }

                // ---- the box half-extent --------------------------------------------------
                // 0x827215CC..0x82721604: `lvx128 v0, this, 0x713B0` (mfTrafficSimRadius),
                // then `vperm128 v127, v0, <zero>, <table at unk_82CDA350>` +
                // `vrlimi128 v127, v0, 2, 0` -- the SDK's standard VecFloat -> Vector3 lane
                // shuffle (w zeroed, y restored from the source). Since Construct seeds the
                // member as a SPLAT of 195.0f, every lane the shuffle can select is 195.0f;
                // and only the X and Z lanes reach the Pvs at all (both GetHullIndexForPoint
                // overloads read lanes 0 and 2 only), so the shuffle is expressed as the
                // Vector3 it produces rather than transcribed as VMX.
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
            // ---- THE ONLINE ARM, GATED BY NAME ----
            // 0x82721870..: replays maPredictedHullChanges (the Array<HullChangeInfo,400>
            // PredictHullChanges @0x827348E8 fills) instead of computing the box locally, so
            // every client turns the same hulls on in the same frame; on a miss it prints
            // "HULL SYNC DIVERGENCE: hit historical prediction", calls
            // DEBUGDumpHullPredictions @0x827211B0 and latches mbHullSyncDivergence.
            // GATED because its producer PredictHullChanges is an ARTIST EXPORT HOLE (no
            // per-function JSON exists at all), so maPredictedHullChanges is never filled in
            // this tree and replaying it would only assert. UNREACHABLE OFFLINE:
            // mbAllowDivergentBehaviour is true whenever !mbIsOnlineGameMode.
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "UpdateRaceCarHulls ONLINE arm (!mbAllowDivergentBehaviour) -- replays "
                "maPredictedHullChanges, whose producer PredictHullChanges @0x827348E8 is an "
                "ARTIST EXPORT HOLE. Dead offline");
        }
    }

    // ---- snapshot the previous set, then rebuild it -------------------------------------
    // The console memcpy's 148 bytes (Set<u16,72>: 144 element bytes + the 4-byte length) of
    // mActiveHulls into a stack temp before clearing it.
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
        // 0x8274CA00-ish LABEL_43: the debug over-budget counter is reset whenever the active
        // set actually moved. +468932 == miDEBUGOverBudgetness (:845) -- pinned by the debug
        // block's own run: mpLogger @+468916, mbDEBUGStopTrafficMoving @+468920,
        // meDEBUGAirRamToFire @+468924, miDEBUGOverrideVehicleToSpawn @+468928 (Construct
        // stores -1 there), miDEBUGOverBudgetness @+468932.
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
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "RecalculateActiveHulls trigger/hull-runtime legs -- (a) the two "
            "Array<u16,72>::AppendSet calls that feed mHullsToAddTriggersFor / "
            "mHullsToRemoveTriggersFor need ::Array<T,N>::AppendSet, absent from CgsArray.h "
            "(it declares AppendArray only); (b) the per-old-hull HullRuntime::Release + "
            "mUsedHullRuntimeData free and the per-new-hull allocate + HullRuntime::Prepare "
            "and their light-manager events. Parked cars never read HullRuntime -- only the "
            "driving generator does, which is gated for wave 2");
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
// TrafficEntityModule::PostPhysicsUpdate  @ 0x8274E6D0   *** PARTIAL ***
//
// REAL here: the buffer lock bracket, the streaming-complete latch, UpdateDensity, the
// mbDEBUGTurnTrafficOff tear-down trigger, and the ENTIRE E_STATE_STARTING_UP arm (which is
// the arm this cluster exists for -- POPULATING is where parked cars are created).
//
// ⭐ ALSO REAL SINCE 2026-08-21 (wave T1 round 2, cluster R2A): the tail's LOCAL PLAYER
// refresh (meLocalPlayerIndex / mLocalPlayerPosition / mLocalPlayerDirection), un-gated now
// that InputBuffer_PostPhysics::GetActiveRaceCarOutputInterface() exists.
//
// GATED here: the pre-state head (UpdateDEBUG / HandleExternalRequests / UpdateStreaming),
// the E_STATE_RUNNING arm, the E_STATE_TEARING_DOWN arm, and the REST of the tail (perfmon,
// UpdateEventStarts, the network + traffic-type + replay legs). None of them has a body in
// this tree and none of them is on the parked-car path.
//
// ⚠️ ONE-WAY DEPENDENCE ON PreSceneUpdate -- SATISFIED IN SOURCE 2026-08-21 (wave T1 round 2,
// cluster R2A), CONDITIONAL ON THE MOUNT: PreSceneUpdate is REAL in the sibling partfile
// BrnTrafficEntityModule_wT1_02.cpp and its WorldLinkStubs.cpp gate is RETIRED in the same
// change -- so meStartingUpState reaches POPULATING and the arm below is entered AS SOON AS
//     echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT1_02.cpp"
// is added to tools/build/build_game_exe.bat beside this file's own mount. That bat line is
// CONDUCTOR-OWNED (agents may not edit the build script), so until it lands the partfile is
// not compiled into the exe and the exe does not link at all (LNK2019 on PreSceneUpdate --
// loud, not silent, which is the point of retiring the gate in the same change).
// ⚠️ DO NOT re-promote this to an unconditional "the arm below is entered" claim without
// grepping the bat first: the per-TU `cl /c` gate is blind to the mount, so a green selfcheck
// proves nothing about it. That is exactly the stale-banner class this wave was told to hunt.
// The paragraph that follows is kept because it records WHY the dependence exists and why the
// transition could not simply be moved here.
//
// The console advances
// E_STARTINGUPSTATE_WAITING_FOR_PLAYER -> _POPULATING in **PreSceneUpdate** @0x8274A968, not
// here (Feb-2007 spells that arm at BrnTrafficEntityModule.cpp:1196; the X360 export for
// PreSceneUpdate is a HOLE, so the leak is the only rung available for it). BEFORE this round
// PreSceneUpdate was an inert gate in WorldLinkStubs.cpp, so meStartingUpState never
// left WAITING_FOR_PLAYER and the POPULATING arm below was never entered. That was the
// SECOND of the two blockers named in the C4 report -- and it could not be bodied here
// either, because the leak's arm reads
// `lpInput->GetActiveRaceCarOutputInterface()->IsPlayerCarActive()` on the PRE-SCENE input
// buffer, which this function does not have. (That getter itself was the round-1 gap: it is
// now REAL -- InputBuffer_PreScene::GetActiveRaceCarOutputInterface, DWARF :153 / sub_82710BD8
// -- in BrnTrafficEntityModuleIO.cpp, landed by this same cluster.) Emitting the
// transition WITHOUT that test would advance to POPULATING before the player car exists,
// which is strictly worse than not advancing at all (no hulls, no cars, and the module then
// walks on to RUNNING with an empty world).
// ----------------------------------------------------------------------------
void TrafficEntityModule::PostPhysicsUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                                            CgsModule::IOBufferStack* lpOutputBufferStack,
                                            BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                            BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput,
                                            BrnUpdateSet lUpdateSet)
{
    (void)lpInputBufferStack;
    (void)lpOutputBufferStack;
    // NOTE: lUpdateSet is READ as of wave T1 round 4 -- see the E_STATE_RUNNING arm's
    // lbSimPaused (X360 0x8274E710 `clrlwi r27, r30, 31`). Its `(void)` cast is gone.

    lpOutput->LockForWrite();
    lpInput->LockForRead();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate head legs UpdateDEBUG @0x8271DC78 / HandleExternalRequests -- "
            "neither is bodied in this tree. Consequence for wave 1: the module never consumes "
            "game-mode requests, so mfGameModeDensityScale keeps the value ResetEventData "
            "seeded (mfBaseDensityScale == 1.0f), which is the density parked cars need. "
            "(The third leg this gate used to cover, UpdateStreaming @0x82748848, is LIVE as "
            "of wave T1 round 3 -- see the call immediately below)");
    }

    // ---- THE STREAMER PUMP, head call (0x8274E740 `bl UpdateStreaming`) -------------------
    // (address corrected 2026-08-21 round-3 FIX pass: the old comment said 0x8274E718, which
    // is inside the preceding block; the `bl` is at 0x8274E740, and UpdateDensity's is at
    // 0x8274E7A0 -- so "before UpdateDensity" is confirmed, only the address was off.)
    // ⭐⭐ UN-GATED 2026-08-21 (wave T1 round 3, closure item 1). The body now exists in the
    // sibling partfile BrnTrafficEntityModule_wT1_04.cpp. This is the call that makes the
    // game ASK for a VEH_T*_GR bundle: LoadData's SetAssetList publishes the catalogue, and
    // nothing requests anything until TrafficCarStreamer::Update runs -- whose only pump is
    // UpdateStreaming, whose only two call sites are this one and the POPULATING one below.
    //
    // ⚠️ MOUNT DEPENDENCE, stated so nobody reads a green selfcheck as proof: the per-TU
    // `cl /c` gate cannot see whether _wT1_04.cpp is on tools/build/build_game_exe.bat. If it
    // is not mounted this call is an LNK2019 at exe link -- loud, which is the point. The bat
    // line is conductor-owned and is in this round's report.
    UpdateStreaming(lpOutput);

    if (mbWaitingForStreaming && mStreamer.AreAllAssetsLoaded())
    {
        mbWaitingForStreaming = false;

        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "PostPhysicsUpdate StreamingCompleteEvent(E_MODULE_TRAFFIC_ENTITY) emit -- "
            "BANNER CORRECTED 2026-08-21 (wave T1 round 3 sweep): the old text said 'the GsmIO "
            "event type ... is not reconstructed', and that HALF IS FALSE -- "
            "BrnGameState::GameStateModuleIO::StreamingCompleteEvent is real "
            "(GameSource/GameState/BrnGameEvents.h:99) and the queue getter is real "
            "(GetGameEventQueue non-const, BrnTrafficEntityModuleIO.h). TWO REAL BLOCKERS "
            "REMAIN, both one-step: (a) that struct's EModule enum in the tree carries only an "
            "INVENTED-name E_MODULE_WORLD_GRAPHICS=2; the DecFIGS DWARF gives the whole set "
            "(E_MODULE_TRAFFIC_ENTITY=0, RACE_CAR_ENTITY=1, WORLD_ENTITY=2, GUI_SCREEN=3, "
            "COUNT=4) and the X360 emit here stores literal 0, so the traffic enumerator is "
            "doubly attested and needs an ADDITIVE completion in BrnGameEvents.h (not this "
            "wave's file); (b) the console posts `AddEvent(&record, 9, 16)` writing ONLY word 0 "
            "-- 16 is the CONSOLE record size and the host record is not 16 bytes, so the size "
            "argument has to be derived from the host type, not copied. The FLAG ITSELF is "
            "cleared, faithfully, so the module does not wait forever");
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
            // The console's arm is empty HERE -- the transition lives in PreSceneUpdate
            // @0x8274A968, which is REAL as of 2026-08-21 in the sibling partfile
            // BrnTrafficEntityModule_wT1_02.cpp (see the banner). PreScene runs before
            // PostPhysics in the frame, so the state can flip and this arm's successor run on
            // the SAME frame -- which is why meLocalPlayerIndex, refreshed in this function's
            // tail, is one frame old when POPULATING reads it. That is the console's ordering
            // too, and offline the POPULATING guard falls through on mbAllowDivergentBehaviour
            // regardless.
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
                    {
                        static bool sbLogged = false;
                        LogMissingLeg(sbLogged,
                            "PostPhysicsUpdate POPULATING leg UpdateVehicles_CreateNewVehicles "
                            "@0x8273A308 -- the DRIVING-traffic maker (wave 2); its lane-param "
                            "pool needs [MEMBER HOLE 1] ParamNeedToSlowData and [MEMBER HOLE 2] "
                            "ParamListNode, neither modelled");
                    }

                    StaticVehicles_CreateNewVehicles(lpInput);
                }

                // ---- THE STREAMER PUMP, POPULATING call (0x8274ED58) ------------------
                // (address corrected 2026-08-21 round-3 FIX pass: 0x8274ED60 is the FOLLOWING
                // `bl sub_82711850`; the `bl UpdateStreaming` is at 0x8274ED58, right after
                // SpawnNewTraffic @0x8274ED28. The placement claim is unchanged and re-derived.)
                // ⭐⭐ UN-GATED 2026-08-21 (wave T1 round 3, closure item 1). Note the console
                // places it OUTSIDE the mbAllowDivergentBehaviour block above -- an online
                // client that created no vehicles locally still streams the assets its hull
                // declares. Order reproduced exactly: after the spawn legs, before
                // UpdateParams_DoTimeSlicedLogic, before the state advance.
                //
                // This is the call that matters for the parked-car frontier: it runs on the
                // ONE frame the module populates, with the player's hull list freshly built
                // by RecalculateActiveHulls, so AddVehiclesToTargetList has a hull to read.
                UpdateStreaming(lpOutput);

                {
                    static bool sbLogged = false;
                    LogMissingLeg(sbLogged,
                        "PostPhysicsUpdate POPULATING leg UpdateParams_DoTimeSlicedLogic "
                        "@0x82743FE8 -- an EXPORT HOLE with no body in this tree. It is the "
                        "DRIVING-param time-slicer (wave 2); parked cars have no param plan "
                        "to slice. (Its companion on this gate, UpdateStreaming @0x82748848, "
                        "is LIVE as of wave T1 round 3 -- see the call immediately above)");
                }

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
    // ⭐⭐ THE STEADY-STATE ARM, UN-GATED 2026-08-21 (wave T1 round 4, item 1).
    //
    // Round 3's banner here listed eleven legs and said "none is bodied". THREE OF THE
    // ELEVEN NOW ARE -- UpdateDecisionFrame @0x8274E508 and UpdateNonDecisionFrame
    // @0x8274C1A8 (both landed this round in BrnTrafficEntityModule_wT1_06.cpp) and the
    // dispatch between them -- and those three are the whole reason the arm exists on the
    // parked path. Everything else in the old list stays gated below, each on its own line
    // with its own reason.
    //
    // ⚠️ THE ROUND-4 BRIEF NAMED THE WRONG HOME FOR THESE LEGS, AND THE ASM SETTLES IT.
    // The brief asked to "un-gate the PostPhysicsUpdate E_STATE_RUNNING-arm legs ...
    // RecalculateActiveHulls, SpawnNewTraffic, the StaticVehicles_* updates". This arm calls
    // NONE of those three. It dispatches to UpdateDecisionFrame, which calls all of them.
    // Un-gating "them" here would have meant writing a second, invented copy of the decision
    // frame inside PostPhysicsUpdate. See _wT1_06.cpp's banner for the full derivation, and
    // for the gate the brief did not name at all (IsDecisionFrame's producer, UpdateTimers).
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
        {
            static bool sbLogged = false;
            LogMissingLeg(sbLogged,
                "PostPhysicsUpdate E_STATE_RUNNING head legs -- HandleRecycledTraffic "
                "@0x82741AF8 / HandleExternalResponses / HandleResetRaceCarEvents / "
                "HandleContactPoints / ProcessDeformationData. None is bodied in this tree; "
                "all five consume DRIVING/crash input rings (waves 2 and 3) and none of them "
                "produces or consumes a parked car");
        }

        // 0x8274E710 `clrlwi r27, r30, 31` -- bit 0 of the update set is the SIM-PAUSED bit.
        // ⚠️ FLAG (no enumerator): BrnUpdateSet is a bare `typedef u16` with no named bits in
        // this tree (SharedClasses/BrnSharedConstants.h), and PreSceneUpdate gates its own
        // update-set decode for exactly that reason -- but there the ARTIST body is an export
        // hole, so nothing attests the mask. HERE IT IS ATTESTED: this function's own asm
        // masks bit 0 and feeds it straight into the `IsPaused() || ...` test below. The bit's
        // NAME is still unrecovered (the Feb-2007 leak calls it E_HLA_UPDATE_PAUSED); the bit
        // itself is not a guess, so the leg is written rather than gated.
        const bool lbSimPaused = ((lUpdateSet & 1u) != 0);

        if (IsPaused() || lbSimPaused)
        {
            // The console's paused arm is genuinely empty apart from the perfmon bracket:
            // a paused traffic sim advances nothing and posts nothing.
        }
        else
        {
            // ⭐⭐ THE DISPATCH. This is the whole of item 1.
            if (IsDecisionFrame())
            {
                UpdateDecisionFrame(lpInput, lpOutput);
            }
            else
            {
                UpdateNonDecisionFrame(lpInput, lpOutput);
            }

            {
                // GenerateSceneUpdateEvents: the per-frame MOVER (one scene update event per
                // vehicle whose transform changed). No body in this tree, and it is the
                // driving half by construction -- a parked car's transform never changes
                // after StaticVehicles_CreateNewVehicles seats it, and its scene entity was
                // already created AT its world position by CreateNewVehicleEntities' AddEntity
                // (which carries the bounding-sphere centre). So parked cars are correctly
                // placed without it; only moving traffic would be stale. Recorded here
                // because "does the parked path also need a per-frame position leg?" was an
                // explicit round-4 question: the answer is NO, and this is where the seam is.
                static bool sbLogged = false;
                LogMissingLeg(sbLogged,
                    "PostPhysicsUpdate RUNNING leg GenerateSceneUpdateEvents -- no body; it is "
                    "the per-frame scene MOVER for traffic that moved. Parked cars do not need "
                    "it: AddEntity already carries the world-space bounding-sphere centre and a "
                    "parked transform never changes. Needed by wave 2 (driving traffic)");
            }

            {
                // TrafficLightManager::Update(mfSimTimeStep) -- DECLARED
                // (BrnTrafficLightManager.h:93) and bodied NOWHERE
                // (recurring bug class (d)). Traffic-light phase state, not parked cars.
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
    // ⭐⭐ TAIL LEG UN-GATED 2026-08-21 (wave T1 round 2, cluster R2A): the LOCAL PLAYER
    //     position / direction / index refresh, X360 0x8274ED90..0x8274EE88.
    //
    // Round 1 gated this because it reads the player car through
    // InputBuffer_PostPhysics::GetActiveRaceCarOutputInterface(), which had no declaration.
    // It does now (DWARF :358). This leg matters twice over on the parked path:
    //   * meLocalPlayerIndex is what PostPhysicsUpdate's POPULATING arm and
    //     RecalculateActiveHulls' mActiveHullsForLocalPlayer rebuild both key off; while it
    //     stayed E_ACTIVE_RACE_CAR_INDEX_INVALID the local-player hull set was always empty;
    //   * mLocalPlayerPosition / mLocalPlayerDirection are the module's own cached copy of
    //     where the player is, read by the driving/streaming legs of later waves.
    //
    // Console, statement for statement:
    //     lpActive = lpInput->GetActiveRaceCarOutputInterface();      ; 0x8274ED94 sub_82711850
    //     if (lpActive->IsPlayerCarActive())                          ; INLINED 0x8274ED9C..
    //         ; (the inline is the committed body: assert mePlayerActiveRaceCarIndex < COUNT
    //         ;  at BrnRaceCarEntityModuleOutputInterface.h:967, then `index != -1 && +0x2860`)
    //     {
    //         meLocalPlayerIndex = lpActive->GetPlayerActiveRaceCarIndex();  ; 0x82277BF8, inlined
    //                                                                        ; stw -> +0x713F0
    //         mLocalPlayerPosition  = state->mTransform.wAxis;        ; addi +0x1F0 ; lvx +0x30
    //                                                                 ; stvx -> +0x713D0
    //         mLocalPlayerDirection = state->mTransform.zAxis;        ; addi +0x1F0 ; lvx +0x20
    //                                                                 ; stvx -> +0x713E0
    //     }
    //     else  { meLocalPlayerIndex = -1; }                          ; 0x8274EE84 li r11,-1 ; stwx
    //
    // ⚠️ THE CONSOLE CALLS THE GETTER THREE TIMES (0x8274ED94 / 0x8274EE30 / 0x8274EE5C) and
    // re-reads the index out of the member between them (`lwz r30, 0(r29)` at 0x8274EE44).
    // It is the same buffer and the same interface every time -- the repetition is the
    // compiler re-materialising an inlined accessor, not three different objects -- so it is
    // de-inlined here to one local. The read-lock assert inside the getter is idempotent.
    //
    // ⚠️ +0x1F0 == 496 == RaceCarState::mTransform; +0x30 == Matrix44Affine::wAxis (the
    // translation row) and +0x20 == zAxis (the forward row). Two different displacements off
    // one base -- position and DIRECTION, not the same vector twice.
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
            "@0x8272B880 (EXPORT HOLE) and the replay-serialiser registration/write. The "
            "local-player position/direction/index refresh that used to be named here is NOW "
            "REAL (see the block above)");
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
// The per-event ("we are entering a new game mode/event") default block. Landed because it,
// and nothing else, seeds the density the whole parked chain gates on. Store for store, with
// every offset resolved to its member:
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
// The +468260 naming is not a guess: the run 468144/468148/468152/468156 is :772..:775, the
// replay serialiser occupies the DWARF's un-emitted :776/:777 window from +468160 (the
// console registers it as `RegisterSerialiser(..., this + 468160)`), and Reset's next member
// is the 8-aligned FastBitArray mVehiclesToUpdateCollidables at +468264 -- which leaves
// exactly one 4-byte slot at +468260 for :778 mfPlayerIdleTime.
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
        // `v1 = mpLogger; *v1 = 1;` -- same un-named leading Logger byte EnterStartingUpState
        // writes; same blocker (BrnTrafficLogger.cpp does not compile).
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "ResetEventData mpLogger-><leading byte> = 1 -- Logger has no usable declaration "
            "(BrnTrafficLogger.cpp is unmounted and does not compile; it locally REDECLARES "
            "KU_MAX_PARAMS / HullRuntime / ParamTransform / TrafficEntityModule -- see the "
            "measured breakdown at EnterStartingUpState)");
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
// TrafficEntityModule::Reset  @ 0x8272CDA0   *** PARTIAL ***
//
// The pool constructor: it is what makes 199 static params and 600 vehicles exist in a
// usable state. REAL here, in the console's order:
//   mRand.Construct(); mEffectRand.Construct();
//   muFramesSinceDecision = 100; mbDecisionFrame = false; mfSimTimeStep = 0;
//   meState = meStartingUpState = meRunningState = meTearingDownState = -1;
//   muUpdateCount = 0;
//   EnterStartingUpState();
//   mfTrafficAmountScale = mfGameModeDensityScale;
//   mfTimeSincePlayerHullChange = 0; mfTimeSincePlayerWasDrivingQuickly = 0;
//   muNumFramesBeforeStateChange = 0xFF; mbAllVehiclesDead = true;
//   muPreviousPlayerHull = 0xFFFF; mbNeedToKillAllZombies = false;
//   the container clears (mFreeParams / maPurgatoryList / mParamsToReinsert /
//     mFreeStaticParamStack / mStaticParamPurgatoryList / mTrailerPurgatoryList /
//     mFreeTrailerStack / the six 160-arrays / muNumGenerators / mActiveHulls /
//     mActiveHullsForLocalPlayer / mHullsToAddTriggersFor / mHullsToRemoveTriggersFor /
//     maPredictedHullChanges / mCachedCollidableList-full);
//   mVehiclesAddedToCrashModule + maTrafficPhysicsInfoListBits cleared;
//   mParamSoaData's mAliveParams / mDyingParams / mZombieParams cleared (three inlined
//     10-qword zeroing blocks at +251648 / +251728 / +251808);
//   mVehicleSoaData.Construct();
//   199x StaticTrafficParam::Construct + push the slot onto mFreeStaticParamStack;
//   the single trailer slot pushed (599);
//   600x Vehicle::Construct + SetVehicleTransform;
//   mauHullRuntimeDataIndices[0..399] = KU_INVALID_HULL_RUNTIME;
//   72x HullRuntime::Construct; mUsedHullRuntimeData cleared;
//   maStoredAITrafficData[i] = { i, 0 };
//   maaRaceCarHulls[i].Clear(); muCurrentlyPredictedHull = 0xFFFF;
//   mbNeedToBroadcastHullChange = false; mbHullSyncDivergence = false;
//   mbNetworkHasDetectedDivergence = false; miDEBUGOverBudgetness = 0;
//   the four crash-slider stores (CrashScore = 0, Decay = 0.5f, Factor = 0.80000001f,
//     FinalValue = 0) -- the two seeds are flt_820BA62C / flt_820BA5B4, the same two
//     symbols Construct's tail and ResetEventData load;
//   mShowtimePlayerGroundPos = 0; mAveragePhysicalCentre = 0;
//   mfJunctionFUP = 0; mfJunctionFUP_TimeTillNextPhysicalKill = 1.0f; mbInPictureParadise = false;
//   mVehiclesToUpdateCollidables / mVehiclesAvoidableLastFrame cleared, then the first 300
//     bits of mVehiclesToUpdateCollidables set.
//
// The offset->member resolution above is not ordering guesswork: it closes with zero slack
// at three independent anchors -- maVehicles @+10880 / maVehicleAxles @+87680 (the Construct
// loop's two bases and their 128/64 strides), the six 160-array live-count words at
// +357100/+359664/+359988/+360312/+360636/+360960 (deltas 2564,324,324,324,324, which is
// exactly TrafficCrashInfo(16)*160+4 then u16(2)*160+4), and
// maHullRuntimeData @+257216 + 72*1176 == mUsedHullRuntimeData @+341888.
//
// GATED here, each with its reason at the site.
// ----------------------------------------------------------------------------
void TrafficEntityModule::Reset()
{
    // ⚠️ FLAG -- A KNOWN, NAMED DIVERGENCE, NOT AN OVERSIGHT.
    // The console does NOT call the canonical CgsNumeric::Random::Construct here. Read at
    // 0x8272CDB8..0x8272CE88 it is, for BOTH generators (mRand @+4912, mEffectRand @+4960):
    //     li  r11, 0x6DC2 ; oris r7, r11, 0x8FE0   -> r7 = 0x000000008FE06DC2
    //     std r7, 0x20(rand)                       -> muSeed = that literal
    //     stw r24(0), 0x28(rand)                   -> muOldestBufferIndex = 0
    //     8x { bits = Convert(hi32(muSeed)) ; muSeed = muSeed*K + 1 ;
    //          ring[muOldestBufferIndex] = bits ; muOldestBufferIndex = (idx+1) & 7 }
    // i.e. a TRAFFIC-SPECIFIC seed plus a full 8-slot prime that writes the CURRENT slot and
    // then advances. Random::Construct() (CgsRandom.h) instead installs
    // KU_RANDOM_DEFAULT_SEED, forces slot 0 to exactly 1.0f, and primes slots 1..7 by
    // advancing FIRST. Both end with muOldestBufferIndex == 0 and all eight slots live, so
    // the ring is correctly primed either way -- what differs is WHICH pseudo-random stream
    // the traffic module runs on, and therefore which parked record wins its
    // mExistsAtAllChance roll and which vehicle type PickVehicleToSpawn draws.
    // Construct() is called because leaving the ring unprimed would make RandomFloat()
    // return uninitialised storage, which is strictly worse and silent.
    // ONE-METHOD FIX (CgsRandom.h is not this cluster's file -- see the C4 report's parks):
    // add `void ConstructWithSeed(u64 lu64Seed)` doing exactly the block above, and call it
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
        // `BaseCollisionGenerator::Destruct(mpLogger)` in the pseudocode is an ICF fold --
        // the callee shares a body with that unrelated symbol. Whatever it is, it takes
        // mpLogger (+468916, the pointer Construct fills) and the Logger type is unusable
        // here (see EnterStartingUpState).
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset leg <ICF-folded>(mpLogger) @0x8272CE9C -- IDA attributes the callee to "
            "CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct, which is an "
            "identical-code-folding artefact; the real callee is a Logger reset and "
            "BrnTrafficLogger.cpp does not compile (local redeclaration fork -- see the "
            "measured breakdown at EnterStartingUpState)");
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

    // ---- the 25 physical-traffic scratch records ---------------------------------------
    // ⭐⭐ UN-GATED 2026-08-21 (wave T1 round 3, closure item 4). BOTH halves of the round-1
    // gate text above were refuted by R2C and are now retired:
    //   * "bodied nowhere" is FALSE -- TrafficPhysicsInfo::Construct is a real 14-instruction
    //     X360 leaf at 0x82751E88 and is bodied in the sibling partfile
    //     BrnTrafficEntityModule_wT1_03.cpp;
    //   * "RECURRING-BUG CLASS (a): mDetachedPartQueue would be left un-Constructed" is a
    //     MISATTRIBUTION of the bug, not of the fact. The console's Construct DOES NOT CALL
    //     EventQueue::Construct -- it writes one zero byte at record +0x00 and nothing else in
    //     the queue's span. Running this loop was never going to bind that queue and NOT
    //     running it never prevented anything. Class (a) is still open for mDetachedPartQueue
    //     and is re-flagged where it actually lives (wT1_03's park list + the Construct-tail
    //     gate below), NOT here.
    //
    // The argument is the OWNING VEHICLE INDEX; Reset passes the "no owner" sentinel. The
    // console literal is 0xFFFF here and a sign-extended -1 in Construct @0x82740220 -- the
    // same 16 bits into the u16 member, spelled through the named constant.
    for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES; luSlot++ )
    {
        maTrafficPhysicsInfoList[luSlot].Construct(
            static_cast< s32 >( TrafficPhysicsInfo::KU16_NO_OWNING_VEHICLE ) );
    }

    // ---- the param membership sets ---------------------------------------------------------
    // Console (0x8272D7xx region, pseudocode `v21 = _R30 + 251648`): three consecutive
    // 10-qword zeroing blocks at this+251648 / +251728 / +251808 --
    //     do   *(8 * v20++ + v21) = 0;        while (v20 < 0xA);   -> mAliveParams
    //     for (i = 0; i < 0xA; ++i) *(8 * (i + 10) + v21) = 0;     -> mDyingParams
    //     for (j = 0; j < 0xA; ++j) *(8 * (j + 20) + v21) = 0;     -> mZombieParams
    // mParamSoaData is at +251648 (member map) and BrnTrafficParam.h pins mAliveParams @0x00,
    // mDyingParams @0x50, mZombieParams @0xA0; FastBitArray<601> is exactly 10 u64 fields
    // (0x50 bytes), so 251648+0x50 == 251728 and +0xA0 == 251808 with ZERO slack. The console
    // inlines the clears rather than calling ParamSoaData::Construct, and so do we -- the
    // three UnSetAll() bodies are the same field-zeroing loop.
    // RECURRING-BUG CLASS (a): without these, Param::IsAlive()/IsDying()/IsZombie() read
    // uninitialised storage on a MODELLED member. This is not a gated leg.
    mParamSoaData.mAliveParams.UnSetAll();
    mParamSoaData.mDyingParams.UnSetAll();
    mParamSoaData.mZombieParams.UnSetAll();

    mVehicleSoaData.Construct();

    {
        // 400x Param::Construct + ParamTransform::Construct + ParamListNode::Construct, the
        // per-param MAX_FLOAT/-1 seeds, the mFreeParams push and the two CgsAlgorithms::
        // Shuffle calls over mFreeParams / mFreeStaticParamStack.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Reset lane-param pool construction (400x Param::Construct + "
            "ParamTransform::Construct + ParamListNode::Construct + the mFreeParams seed) -- "
            "Param::Construct and ParamTransform::Construct are declared-only in "
            "BrnTrafficParam.h and ParamListNode has no type at all ([MEMBER HOLE 2]). "
            "WAVE 2; parked cars never touch a Param");
        static bool sbLoggedShuffle = false;
        LogMissingLeg(sbLoggedShuffle,
            "Reset CgsAlgorithms::Shuffle over mFreeParams -- the SHUFFLE is real (see the "
            "static/trailer ones below); it is the STACK that is never filled here, because "
            "the 400x Param::Construct seed above it is gated. Shuffling an empty stack is a "
            "no-op, so the call is skipped with the pool it operates on rather than pretended");
    }

    // ---- the static (parked) param pool -- THE ONE THIS WAVE NEEDS ----------------------
    for (u32 luStatic = 0; luStatic < KU_MAX_STATIC_TRAFFIC; ++luStatic)
    {
        maStaticTrafficParams[luStatic].Construct();
        mFreeStaticParamStack.Push(static_cast<u8>(luStatic));
    }

    // The single trailer slot, pushed by FULL vehicle index (599 == KU_TRAILER_TRAFFIC_OFFSET).
    mFreeTrailerStack.Push(static_cast<u16>(KU_TRAILER_TRAFFIC_OFFSET));

    // ---- the two pool shuffles ------------------------------------------------------------
    // ⭐⭐ STALE-BANNER UN-GATE 2026-08-21 (wave T1 round 3 sweep). The gate above used to say
    // "CgsAlgorithms::Shuffle has no reconstruction in the tree". THAT IS FALSE and one grep
    // kills it: GameShared/GameClasses/Algorithms/CgsShuffle.h exists, and its own banner
    // names THESE THREE INSTANTIATIONS as its provenance --
    //     Shuffle<u16, Stack<u16,400>> @0x8271B110   (mFreeParams)
    //     Shuffle<u8,  Stack<u8,199>>  @0x8271B298   (mFreeStaticParamStack)
    //     Shuffle<u16, Stack<u16,1>>   @0x8271B420   (mFreeTrailerStack)
    // -- "all driven by BrnTraffic::TrafficEntityModule::Reset", i.e. by this exact function.
    // The container types match the members here exactly and Stack<T,N>::operator[] /
    // GetLength are both present. This is the false-helpful banner class round 1 warned about:
    // it named a real console leg, gave a plausible reason, and the reason had already been
    // paid off in another cluster's file.
    //
    // ARGUMENTS, from the asm (0x8272D204..0x8272D284), all three calls identical in shape:
    //     r3 = the stack, r4 = 0, r5 = the stack's OWN GetLength(), r6 = this + 0x1330
    // and this + 0x1330 is mRand (:615) -- it lands immediately after mReceiverQueue (:613,
    // an EventReceiverQueue<4096,16> based at +0x314), and mRand is the first member after it.
    // The window is [0, GetLength()), i.e. the whole live stack.
    //
    // ⚠️ THIS ONE IS ON THE PARKED-CAR PATH. mFreeStaticParamStack is what
    // StaticVehicles_CreateNewVehicles pops a parked slot from; unshuffled, slots come out in
    // reverse push order (198..0) every single boot, so which record lands in which slot is
    // deterministic instead of randomised. It never breaks anything -- every record still gets
    // a slot -- which is exactly why it could sit gated for two rounds without anyone noticing.
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

    // ---- the four crash-slider stores -------------------------------------------------------
    // The two ZEROS are `stfsx f31` with f31 == flt_82001CC0 == 0.0f (0x8272D838 / 0x8272D848).
    // The two SEEDS are `stfsx f0/f13` fed by
    //     0x8272D7F4  lfs f0,  0x3F0(r21)   ; r21 == &flt_820BA23C  -> 0x820BA62C
    //     0x8272D7FC  lfs f13, 0x378(r21)                           -> 0x820BA5B4
    //     0x8272D814  stfsx f0,  r30, r10   ; r10 = 0x72374 (Decay)
    //     0x8272D840  stfsx f13, r30, r9    ; r9  = 0x72378 (Factor)
    // Those are the SAME TWO SYMBOLS Construct's tail loads (0x827414AC flt_820BA62C ->
    // 0x72374, 0x827414B4 flt_820BA5B4 -> 0x72378) and the same two ResetEventData
    // @0x827088B8 loads (0x82708AA8 / 0x82708AAC -> r7 = 0x72374, r6 = 0x72378), where IDA
    // resolves them outright as 0.5 and 0.80000001. The values are therefore ATTESTED, not
    // guessed -- an earlier gate here rested on a premise the exports refute.
    // WHY IT MATTERS: PostPhysicsUpdate's TEARING_DOWN arm calls Reset() with NO Construct
    // (`if (*(_R31+464909)) --*(...); else Reset(_R31);`), and UpdateCrashSlider @0x82715A18
    // overwrites both members at runtime (0.0f/10.0f, then 0.1f/1.5f) -- so dropping these
    // two stores would leave the tear-down -> Reset path running on the LAST FRAME's slider
    // tuning instead of the re-seeded defaults.
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
    // 0x8272D8xx: the console then sets the FIRST 300 bits (`while (v79 < 0x12C)`) of
    // mVehiclesToUpdateCollidables -- not all 600. Transcribed as the literal bound it is;
    // no constant in BrnTrafficConstants.h spells 300 and inventing one would be a guess.
    for (u32 luBit = 0; luBit < 300u; ++luBit)
    {
        mVehiclesToUpdateCollidables.SetBit(luBit);
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::Construct  @ 0x82740220   *** PARTIAL ***
//
// REAL here: the base construct, the receiver queue, the streamer, the two density seeds,
// the debug-flag defaults (INCLUDING the mbDEBUGTurnTrafficOff = false this cluster was
// asked to settle -- see the file banner), the render caps, ResetEventData + Reset, and the
// 96 VehicleTypeRuntime constructs.
//
// GATED here: the ~25 vectorised tuning members (:799..:821), the four TrafficJobStub
// constructs ([MEMBER HOLE 5]), the replay serialiser, the fuzzy-behaviour logic, the
// 102,800-byte maTrafficPhysicsInfoList memset, the debug component + logger allocations,
// the twenty perfmon monitors, and the debug-render stream reader. Each is a named one-shot
// below. (The 25 TrafficPhysicsInfo::Construct calls were on this list until 2026-08-21 wave
// T1 round 3; they are now REAL -- body in BrnTrafficEntityModule_wT1_03.cpp.)
// ----------------------------------------------------------------------------
void TrafficEntityModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Construct vectorised tuning-member seeds (:799..:821 -- KF_TWO_PI, KF_MAX_FLOAT, "
            "the lane/steering/pitch/roll blocks, the four sympathetic/avoidance cones and "
            "mTweakValues) -- every one of them is a splat of an un-dumped X360 rodata float "
            "or an XMVectorSin/cos of one. They are DRIVING-traffic tuning: nothing on the "
            "parked path reads any of them");
        static bool sbLoggedJobs = false;
        LogMissingLeg(sbLoggedJobs,
            "Construct 4x TrafficJobStub::Construct -- [MEMBER HOLE 5]: BrnTraffic::"
            "TrafficJobStub embeds EA::Jobs::Job by value and would drag the EATech eajobs "
            "SDK into this keystone header's include graph (and therefore into "
            "BrnWorldModule.h's). Deliberate include-graph decision, recorded by C1");
    }

    // The console's EventReceiverQueue<4096,16>::Construct is INLINED here at
    // 0x827407E0..0x82740844 (mpBuffer = &maBuffer, miCapacity = 0x1000, miAlignment = 0x10,
    // miCount = 0, then the alignment fix-up Clear() repeats). Calling the real Construct is
    // the de-inlined form.
    //
    // ⭐ THIS RETIRES THE wQ7_02 RELOCATION. BrnTrafficEntityModule_wQ7_02.cpp's Prepare
    // stage 0 carried a "[FLAG PC bring-up] Construct relocated here" block precisely because
    // this Construct was an inert gate; with a real body the queue is bound at construction
    // time, where the console binds it.
    mReceiverQueue.Construct();

    mStreamer.Construct();

    {
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Construct sub-object legs TrafficEntitySerialiser::Construct, "
            "CgsResource::BaseResourcePtr::CreateFromHandle(mpData-adjacent slot), "
            "FuzzyBehaviourLogic::Construct, the 102800-byte maTrafficPhysicsInfoList memset "
            "(RECURRING-BUG CLASS (a) LIVES HERE, not in the 25 Constructs: this memset -- or "
            "Construct's own tail -- is the only remaining candidate for whoever binds "
            "mDetachedPartQueue.mpEvents, and neither has been read yet), the 32-slot showtime "
            "list seed, the DebugComponent and Logger allocations, the twenty "
            "CgsDev::PerfMonCpu::AddMonitor registrations and DebugRenderStreamReader::"
            "Construct -- none of those callees has a body or a usable declaration in this tree");
    }

    // ---- the 25 physical-traffic scratch records (0x82740220's own loop) -----------------
    // ⭐⭐ UN-GATED 2026-08-21 (wave T1 round 3, closure item 4) -- same retirement as Reset's
    // copy above; the body is real in BrnTrafficEntityModule_wT1_03.cpp. The console's literal
    // here is a sign-extended -1 (`li r11,-1`), which is the same 16 bits the u16 member takes.
    //
    // ⚠️ THE MEMSET IS STILL GATED (immediately above) AND THAT ORDER MATTERS ON THE CONSOLE:
    // the console memsets the 102,800-byte array and THEN runs these 25 Constructs, so every
    // field this Construct does not touch is zero on the console and is whatever the host
    // module's storage holds here. Nothing on the parked-car path reads any of them (every
    // reader is a gated wave-3 physical-traffic leg), which is why the Construct loop is safe
    // to run without the memset -- but do not promote that to "the record is initialised".
    for ( u32 luSlot = 0; luSlot < KU_MAX_PHYSICAL_TRAFFIC_VEHICLES; luSlot++ )
    {
        maTrafficPhysicsInfoList[luSlot].Construct(
            static_cast< s32 >( TrafficPhysicsInfo::KU16_NO_OWNING_VEHICLE ) );
    }

    // ---- the sim box + render caps (X360 +463792 / +463808 / +463812 / +463816) ----------
    //
    // ⭐⭐ GATE RETIRED 2026-08-21 (wave T1 round 2, cluster R2A). Round 1 left these two
    // UNWRITTEN because the source vector at .data 0x8300CF10 read as zero in every
    // per-function export -- and deliberately did NOT write a placeholder zero, which was the
    // right call: a zero mfTrafficSimRadius collapses the traffic sim box to one Pvs cell
    // without ever producing a non-finite value (the shadow-system wave's exact failure
    // mode). The values are now RECOVERED: the whole 16-byte vector is seeded by an UNNAMED
    // MSVC dynamic-initialiser thunk at 0x82C66F18, which is not a function in the IDA
    // database and is therefore invisible to any per-function export scan; the conductor
    // found it by walking XrefsTo in the .i64 (scratchpad traffic_wave/recovered_constants.md,
    // raw evidence thunk_dump*.txt). It builds
    //     0x8300CF10 = { 195.0f, 395.0f, 62500.0f, 160000.0f }
    // and lanes 2 and 3 corroborate each other and the naming: 62500 == 250^2 and
    // 160000 == 400^2, i.e. squared distances, which is what `...DistanceSq` must hold.
    //
    // Construct's own instructions decide WHICH lane goes where -- no inference:
    //     0x82740790  lis    r11, unk_8300CF10@ha
    //     0x82740794  addi   r11, r11, unk_8300CF10@l
    //     0x82740798  lvx128 v0, r0, r11              ; the whole 16-byte vector
    //     0x8274079C  vspltw v0, v0, 0                ; SPLAT LANE 0  -> {195,195,195,195}
    //     0x827407A0  stvx128 v0, r31, r10            ; r10 == 0x713B0 == mfTrafficSimRadius
    //     0x827407A4  lfs    f0, (flt_8300CF18 - 0x8300CF10)(r11)   ; +8 == LANE 2 == 62500
    //     0x827407A8  li     r11, 0x20
    //     0x827407AC  stfsx  f0, r31, r9              ; r9  == 0x713C4 == mfRenderCullDistanceSq
    //     0x827407B0  stbx   r30, r31, r7             ; r7  == 0x713C8 == mbInOfflineCarSelect (r30 == 0)
    //     0x827407B4  stwx   r11, r31, r8             ; r8  == 0x713C0 == muMaxVehiclesToRender = 32
    // So mfTrafficSimRadius is a SPLAT of lane 0 (all four lanes 195.0f), NOT the raw vector:
    // lanes 1 and 3 of 0x8300CF10 are read by other functions (HandleExternalRequests) and
    // never reach this member. 195 m is the half-extent of the box UpdateRaceCarHulls builds
    // around the player, which at the shipped Pvs cell size is the handful of hulls the
    // "too many hulls turned on: > 4" assert further down that function polices.
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
    // ⚠️ LOAD-BEARING, AND EASY TO MISS: 0x727D8 `stwx r29` == miDEBUGFlowtypeOverride = -1.
    // PickVehicleToSpawn gates on `miDEBUGFlowtypeOverride >= 0`, so if this store were
    // dropped the ZERO-INITIALISED member would read as flow type 0 and EVERY spawn in the
    // world -- parked and driving -- would be forced onto flow type 0's vehicle mix. Nothing
    // would assert; it would just look like the city only owns one kind of car.
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

    // ---- the console's TAIL stores (0x827414D4..0x8274175C), i.e. everything Construct
    // re-seeds AFTER ResetEventData + Reset have run. All four crash-slider values and the
    // three showtime timers are the same numbers ResetEventData writes; mfJunctionFUP's
    // partner is flt_82001C98 == 1.0f (the pseudocode resolves the load as
    // `*(0x82000000 + 0x1C98)`, the same datum mfBaseDensityScale is seeded from).
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
        // Two stores in the DecFIGS un-emitted :776/:777 window that C1 already parked from
        // the other side. NEW EVIDENCE from this body, worth recording: Construct writes
        // ZERO to +0x72520 and **ONE** to +0x72521, while EnterReplay @0x827081D8 and
        // LeaveReplay @0x82708248 BOTH write ZERO to +0x72521. A flag that constructs to 1
        // and is cleared on entering AND on leaving replay is a "not currently inside the
        // replay serialiser's own window" latch, not mbInReplay -- which corroborates C1's
        // refusal to name it. The window is the BrnReplays::TrafficEntitySerialiser member
        // the console registers at `this + 468160`.
        static bool sbLogged = false;
        LogMissingLeg(sbLogged,
            "Construct stores +0x72520 = 0 and +0x72521 = 1 -- both inside the replay "
            "serialiser's DWARF un-emitted :776/:777 window; no attested member name "
            "(C1 parked the same byte from EnterReplay/LeaveReplay)");
    }

    // 0x82741450..0x82741460: `stvx128 v127, r31, 0x713D0` (mLocalPlayerPosition = 0) and
    // `stwx r29, r31, 0x713F0` with r29 == -1 (meLocalPlayerIndex = INVALID).
    // NOTE: mLocalPlayerDirection @+0x713E0 is NOT written here -- the console leaves it to
    // PostPhysicsUpdate's tail. Not adding a store the binary does not have.
    mLocalPlayerPosition.SetZero();
    meLocalPlayerIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;

    // ⭐⭐ ADDED 2026-08-21 -- wave T1 ROUND 2, cluster R2C.
    // ⚠️ CROSS-CLUSTER EDIT, ONE LINE, AND IT IS LOAD-BEARING FOR Prepare STAGE 1.
    //
    // THE CONSOLE'S VERY LAST STORE IN THIS FUNCTION, immediately before the epilogue:
    //     0x82741758  li   r11, 1
    //     0x82741760  stb  r11, 4(r31)         ; r31 == this, byte +4
    // `this + 4` is CgsModule::Module::mbIsNewModule (the vptr occupies +0, and
    // ModuleSingleBuffered::Construct @0x8286E768's own pseudocode names the same three slots
    // -- `field_4 = 0` (this flag), `field_8 = 0` (mePrepareStage), `field_C = 6`
    // (meReleaseStage)). The base Construct called at the top of this function sets it to
    // ZERO; the traffic module flips it back to ONE here.
    //
    // WHY IT MATTERS, and why its absence was invisible: CgsModule::ModuleSingleBuffered::
    // Prepare @0x8286E7A0 tests `*(a1 + 4)` at EVERY stage. Non-zero => it skips the whole
    // old-style DataStructure ladder and falls straight to LABEL_20 (`a1[3] = 0; a1[2] = 6;
    // return 1`). ZERO => it calls vtable slot 56 (CreateInputDataStructure) and
    // `if (!v4) return 0;`. TrafficEntityModule overrides NEITHER CreateInputDataStructure nor
    // CreateOutputDataStructure (the DecFIGS TrafficEntityModule declares neither), so the
    // base placeholder runs and returns nullptr -- i.e. with this store missing, the base
    // Prepare returns FALSE FOR EVERY FRAME, FOREVER.
    //
    // That is a BOOT HANG, not a cosmetic gap: TrafficEntityModule::Prepare stage 1 (now
    // un-gated in BrnTrafficEntityModule_wQ7_02.cpp) forwards the base's false as its own,
    // and WorldModule::Prepare's traffic stage would never advance. It was dormant only
    // because stage 1 was still a log-once gate.
    //
    // Purely additive: one store the console makes, at the position the console makes it, on
    // a member this class inherits as `protected` (CgsModule::Module::mbIsNewModule,
    // CgsModule.h:66). Owner of this file: please keep it.
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
