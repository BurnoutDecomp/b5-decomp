// ============================================================================
// BrnWorld::RaceCarEntityModule -- THE ROAD-RAGE RIVAL RANGE LOOP
// (rival range-loop wave, lane W1, 2026-09-05)
//
//   RaceCarEntityModule::UpdateInAndOutOfRangeCars      X360 0x822FF8F8  (333 instr)
//   RaceCarEntityModule::UpdateHidingEvents             X360 0x822F5830  ( 26 instr)
//   RaceCarEntityModule::SetHiddenDelay                 X360 0x822D2988  (192 instr)
//   RaceCarEntityModule::IsRaceCarWrappable             X360 0x822E9E18
//   RaceCarEntityModule::ReadOutOfRangeRaceCarDataFromAI X360 0x822E9188 (340 instr)
//   RaceCarEntityModule::GetPlayerRaceCar               X360 0x822BA068
//
// WHY THIS SLICE EXISTS. Five Road Rage rivals drive their own routes at 25-30 m/s
// (b5-decomp dev fb2274c9) and then the player pulls a kilometre away and never sees one
// again. On the console the rivals are kept in contact range by this loop, and NONE of it
// existed on the host: PreSceneUpdate never called UpdateInAndOutOfRangeCars, so no rival was
// ever detached when it fell behind and no rival was ever re-placed behind the player; and
// PostPhysicsUpdate never called UpdateHidingEvents, so the game-state side's hide timers
// (RoadRageMode::HandleGameEvents -> UpdateHiddenRivals -> BroadcastEventsToRivals) had no
// producer at all and the action-129 "this rival may re-join" broadcast was answered by
// nobody -- RaceCarEntityModule::HandleGameActions had no arm for it.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX raw assembly. The pseudocode for four of the six is
// unusable (register-allocation failure, `__asm` blocks for every vector op, twelve invented
// integer parameters on ReadOutOfRangeRaceCarDataFromAI); every claim below is from the asm.
//
// ---- THE SHAPE OF THE LOOP -------------------------------------------------------------
// UpdateInAndOutOfRangeCars runs ONCE PER SECOND, not every frame. PreSceneUpdate's call site
// @0x8230E28C..0x8230E2C0 is
//     if ((simTimerStatus->miFrameCount % (s32)(1.0f / mfTimeStep)) == 0) UpdateInAndOutOfRangeCars(...)
// (`lwz r9, 0(r31)` on the sim TimerStatus, `divw/mullw/subf.` against the `fctiwz` of
// `1.0f / (mfBaseTimeStep * mfTimeStepMultiplier)` computed at 0x8230E20C). The two passes:
//
//   pass 1, over the EIGHT active slots (0x822FF9F4..0x822FFC90)
//     count the attached cars, and for every attached, non-crashing, RACING, fully-loaded
//     AI car that IS an in-range rival:
//       * mode wraps AI cars (KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE, 0x20) and
//         IsRaceCarWrappable says yes  ->  RequestResetOnTrack (teleport it, keep it attached)
//       * otherwise, ShouldBeOutOfRange and the player has not won the event
//                                       ->  DetachActiveRaceCar (drop it out of simulation)
//
//   pass 2, over the THIRTY-FIVE global slots (0x822FFCC4..0x822FFE14)
//     stop the moment eight cars are attached; otherwise for every in-world AI car that is an
//     out-of-range rival and (in a game mode) is in the current mode:
//       ShouldBeInRange, or the player won the event  ->  AttachActiveRaceCar(car, INVALID)
//                                                         + mbComingInRange = true
//
// ---- WHAT IS NOT REPRODUCED, AND WHY ---------------------------------------------------
// Nothing. All six bodies are complete. The four numeric parks the console reads out of the
// BurnoutConstants .data block WERE recovered (see the constant banner below) rather than
// carried as flagged zeros -- their CRT dynamic initialisers are in the 0x82C4Bxxx bank and
// every one is `<mph literal> * 0.44704`.
// ============================================================================
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // OutputBuffer_PreScene / InputBuffer_PostPhysics accessors
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"                         // AIRaceCarInterface / RaceCarAIInterface
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"                // GameModeParams::KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                   // GameStateModuleIO::EGameModeType
#include "GameSource/World/AI/BrnAISharedConstants.h"                                    // BrnAI::EResetType
#include "GameSource/BurnoutConstants.h"                                                 // EActiveRaceCarIndex / EGlobalRaceCarIndex
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                                    // CgsNumeric::Random
#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // gpDebugPrint
#include <cmath>                                                                          // std::fabs (the vandc sign-mask ABS at 0x822E9FFC)
#include "rw/math/vpu/vector3_operation.h"                                               // Dot / Cross / Normalize / Magnitude / IsValid / GetVector3_*Axis

namespace BrnWorld
{

// ============================================================================================
// THE CONSTANTS. Every one is read out of the decrypted ARTIST image at the `lfs` symbol the
// asm names; nothing here is inferred, and nothing is a flagged zero.
//
// FOUR OF THEM READ 0x00000000 STRAIGHT OUT OF THE IMAGE and were previously written down in
// this directory as un-homed zeros (BrnRaceCarEntityModule.cpp's KF_AI_LOAD_RESET_SPEED
// banner: "no function in the image writes it"). THAT CLAIM IS WRONG, and this wave found the
// writers -- they are CRT dynamic initialisers in the 0x82C4Bxxx bank, which the earlier scan
// missed because it looked for a `lis/@l` pair on the READERS' side only:
//
//   0x82C4BB70..0x82C4BB8C   flt_82FAD728 = flt_82F31928 * flt_82014948 = 0.44704 * 120.0
//   0x82C4BB90..0x82C4BBAC   flt_82FAD514 = flt_82F31928 * flt_82020A70 = 0.44704 * 160.0
//   0x82C4BBB0..0x82C4BBCC   flt_82FAD518 = flt_82F31928 * flt_820149CC = 0.44704 *  20.0
//   0x82C4BBD0..0x82C4BBEC   flt_82FAD51C = flt_82F31928 * flt_8201499C = 0.44704 *  30.0
//   0x82C4BBF0..0x82C4BC0C   flt_82FAD96C = flt_82F31928 * flt_82004C6C = 0.44704 *  60.0
//   0x82C4BEC0..0x82C4BEFC   flt_82FAD510 = XMVectorCos(flt_82020A88 = 0.5235988 rad = 30 deg)
//
// flt_82F31928 == 0.44704 is EXACTLY the miles-per-hour to metres-per-second factor, so all
// five speeds are round mph figures and the sixth is cos(30 degrees). Six independent
// literals landing on six round physical values is the fit.
// ============================================================================================

// flt_82001CC0 -- the zero literal every one of these bodies splats.
static const f32 KF_ZERO = 0.0f;

// The mph -> m/s factor the initialiser bank multiplies by (flt_82F31928).
static const f32 KF_MPH_TO_METRES_PER_SECOND = 0.44704f;

// ---- IsRaceCarWrappable (0x822E9E18) --------------------------------------------------
// flt_82014944 @0x822E9EE8 -- 6400.0f == 80 m squared. Inside this radius the car is never
// wrapped and the clipped-time counter is reset.
static const f32 KF_WRAP_MINIMUM_DISTANCE_SQUARED = 6400.0f;
// flt_82013FA4 @0x822E9F74 / flt_8201493C @0x822E9F80 -- the along-the-player's-heading band
// the car must stay inside to avoid an immediate hide: more than 80 m behind, or more than
// 210 m ahead, and the car is hidden this frame.
static const f32 KF_WRAP_BEHIND_PLAYER_LIMIT = -80.0f;
static const f32 KF_WRAP_AHEAD_PLAYER_LIMIT  = 210.0f;
// flt_82FAD510 @0x822EA000 -- cos(30 degrees). |cos(angle between the player's heading and the
// direction to the car)| at or below this means the car is more than 30 degrees off the
// player's road axis, i.e. "clipped".
static const f32 KF_WRAP_CLIPPED_COS_ANGLE = 0.86602540f;
// flt_82014948 @0x822EA01C -- 120 clipped frames before the car is hidden.
static const f32 KF_WRAP_CLIPPED_TIME_LIMIT = 120.0f;
// flt_82001C98 @0x822EA014 -- the per-frame increment of mfClippedTime.
static const f32 KF_WRAP_CLIPPED_TIME_STEP = 1.0f;

// ---- SetHiddenDelay (0x822D2988) ------------------------------------------------------
// flt_820147FC @0x822D2A14 -- the 0.5 of the (dot + 1) * 0.5 heading agreement remap.
static const f32 KF_HIDE_HEADING_REMAP_SCALE = 0.5f;
// flt_82001C98 -- the +1 of that remap, and the clamp ceiling of both factors.
static const f32 KF_HIDE_FACTOR_MAX = 1.0f;
// flt_820149B4 @0x822D2AE0 / @0x822D2BA8 -- 5 seconds per unit of BOTH factors, so the queued
// hide time runs 5 s (facing away, on top of the player) to 15 s (facing the same way, 200 m
// or more apart).
static const f32 KF_HIDE_TIME_PER_FACTOR = 5.0f;
// flt_8200CE04 @0x822D2B84 -- 0.005 == 1/200 m: the separation that saturates the second
// factor.
static const f32 KF_HIDE_SEPARATION_SCALE = 0.005f;
// The console's own array bound; SetHiddenDelay asserts against it before appending.
static const s32 KI_MAX_HIDING_EVENTS = 8;
// KI_EVENT_RACE_CAR_NEEDS_HIDING. [!] HEADER REQUEST -- interim TU-local mirror; the same
// value and the same reason as BrnRoadRageMode.cpp:159, which carries the CONSUMER end.
// `li r5, 0x28 ; li r6, 8` @0x822F5868.
static const s32 KI_EVENT_RACE_CAR_NEEDS_HIDING = 40;

// ---- UpdateInAndOutOfRangeCars (0x822FF8F8) -------------------------------------------
// flt_82FAD96C @0x822FFB78 -- 60 mph. Above it the wrap speed follows the player's.
static const f32 KF_WRAP_PLAYER_SPEED_THRESHOLD = 60.0f * KF_MPH_TO_METRES_PER_SECOND;
// flt_82FAD51C @0x822FFB90 -- 30 mph, subtracted from the player's speed on the fast arm.
static const f32 KF_WRAP_SPEED_BELOW_PLAYER = 30.0f * KF_MPH_TO_METRES_PER_SECOND;
// flt_82FAD728 @0x822FFB84 -- 120 mph, the flat wrap speed used when the player is slow.
static const f32 KF_WRAP_SPEED_PLAYER_SLOW = 120.0f * KF_MPH_TO_METRES_PER_SECOND;
// flt_82FAD518 @0x822FFBA0 -- 20 mph, added to the player's speed on the behind-player arm.
static const f32 KF_RESET_SPEED_ABOVE_PLAYER = 20.0f * KF_MPH_TO_METRES_PER_SECOND;
// flt_82FAD514 @0x822FFBAC -- 160 mph, the cap of that arm.
static const f32 KF_RESET_SPEED_MAX = 160.0f * KF_MPH_TO_METRES_PER_SECOND;
// flt_820148B4 @0x822FFBCC / flt_820148B8 @0x822FFBD4 -- the behind-player reset distance.
// Road Rage and Marked Man place the rival 20 m behind; every other mode 50 m.
static const f32 KF_RESET_DISTANCE_BEHIND       = -50.0f;
static const f32 KF_RESET_DISTANCE_BEHIND_CLOSE = -20.0f;
// The Marked Man arm draws 1-in-5 instead of 1-in-2 (`mulhwu r11, r10, 0xCCCCCCCD ; srwi 2`
// == the /5 magic multiply, @0x822FFB44..0x822FFB5C).
static const u32 KU_WRAP_MARKED_MAN_DRAW_MODULUS = 5u;

// ---- ReadOutOfRangeRaceCarDataFromAI (0x822E9188) -------------------------------------
// flt_82014460 @0x822E9430 -- 0x34000000 == FLT_EPSILON. The degenerate-cross test.
static const f32 KF_CROSS_PRODUCT_EPSILON = 1.1920929e-07f;

// ============================================================================================
// THE MODULE TU'S FILE-SCOPE PSEUDO-RANDOM GENERATOR.
//
// The console object lives at 0x82FAD2B0: an 8-slot float ring at +0x00, muSeed at +0x20
// (qword_82FAD2D0) and muOldestBufferIndex at +0x28 (dword_82FAD2D8) -- i.e. exactly a
// CgsNumeric::Random. Three sites in the image touch it, and only three:
//   RaceCarEntityModule::Prepare            @0x82304074  the inlined Random::Construct
//   RaceCarEntityModule::GetRandomCarColour @0x822EA0C4  two inlined RandomUInt draws
//   UpdateInAndOutOfRangeCars               @0x822FFB28  the draw reproduced below
// Every draw is the same three instructions -- `ld seed ; srdi hi,32 ; mulld K ; addi 1 ; std`
// -- which is CgsNumeric::Random::RandomUInt() verbatim (CgsRandom.cpp:52).
//
// [FLAG PC bring-up] the console SEEDS it from RaceCarEntityModule::Prepare, which is
// declaration-only on this build, so it is Construct()ed lazily on first draw here instead.
// The draw sequence is therefore the console's, but its phase is not (the console has drawn
// from it once per car-colour pick before this loop ever runs). Nothing downstream is
// order-sensitive: the draw only picks WHICH of two reset styles a wrap uses.
// DELETE-WHEN RaceCarEntityModule::Prepare's Construct leg lands and this object can move
// there as a real file-scope member.
// ============================================================================================
namespace
{
    CgsNumeric::Random& GetModuleRandom()
    {
        static CgsNumeric::Random sModuleRandom;
        static bool               sbConstructed = false;

        if (!sbConstructed)
        {
            sModuleRandom.Construct();
            sbConstructed = true;
        }

        return sModuleRandom;
    }

    // [FLAG PC witness] -- NOT AN X360 FUNCTION.
    //
    // One `[range]` line per in->out / out->in transition, FIRST 32 ONLY, so the next run's
    // BrnGame.log says -- per rival, in order -- which way the loop moved it, how far it was
    // from the player when it did, and how long the game-state side was asked to hide it for.
    // Without this rung a loop that never fires and a loop that fires and is undone one frame
    // later produce the same log (nothing at all), and that ambiguity is exactly what let
    // "the rivals never touch the player" sit behind five landed subsystems.
    //
    // Pairs with: [ai-act] / [ai-attach] / [rot] / [ai-evt] (already in the tree).
    // DELETE-WHEN rivals stay in contact with the player for a whole Road Rage event.
    void WitnessRangeTransition(s32 liSlot, const char* lpcTransition,
                                f32 lfDistance, f32 lfHiddenTime)
    {
        static s32 siWitnessCount = 0;

        if (CgsDev::Log::gpDebugPrint == 0 || siWitnessCount >= 32)
        {
            return;
        }
        ++siWitnessCount;

        *CgsDev::Log::gpDebugPrint
            << "[range] slot " << liSlot
            << " " << lpcTransition
            << " dist " << lfDistance
            << " hideTime " << lfHiddenTime
            << " [FLAG PC witness]\n";
    }
}

// ============================================================================================
// GetPlayerRaceCar @ 0x822BA068
//
// Three instructions of body: the player's active slot, the console's IsAttached() assert
// (BrnActiveRaceCar.h:1089), and `lwz r3, 0x6F0(r31)` == ActiveRaceCar::mpRaceCar.
//
// [GUARD] the two null tests are NOT the console's. The console indexes maActiveRaceCars with
// the raw word, so mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID would read
// 7376 bytes in front of the array; and it reaches this function only with a player car
// present. On this build the caller chain is still being assembled, and a null here would be
// a fault rather than a skipped range test. Both callers below treat null as "no player car,
// do nothing", which is the console's behaviour with a player car that is not there.
// ============================================================================================
RaceCar* RaceCarEntityModule::GetPlayerRaceCar()
{
    if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        return 0;   // [GUARD], see banner
    }

    ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);
    if (lpPlayerActiveRaceCar == 0)
    {
        return 0;   // [GUARD], see banner
    }

    CGS_ASSERT(lpPlayerActiveRaceCar->IsAttached(), "IsAttached()");   // BrnActiveRaceCar.h:1089

    return lpPlayerActiveRaceCar->GetGlobalRaceCar();
}

// ============================================================================================
// SetHiddenDelay @ 0x822D2988
//
// Queue ONE "this rival needs hiding" record for lpRaceCar. Its only caller is
// IsRaceCarWrappable, on the arms that decide the car may be teleported: the game-state side
// then counts the queued time down (RoadRageMode::UpdateHiddenRivals) and refuses to let the
// rival re-join until it reaches zero (BroadcastEventsToRivals' action 129).
//
// The hide time is the sum of two 0..1 factors, each worth 5 seconds:
//   heading agreement  (dot(playerHeading, carHeading) + 1) * 0.5, clamped to <= 1
//   separation         |carPos - playerPos| * 0.005          , clamped to 0..1
// so a rival driving straight at the player from 200 m gets ~5 s and one driving away from
// far off gets ~15 s. The record's slot field is the car's OWN active-slot index
// (`lbz r10, 0xAC(r26) ; extsb` @0x822D2ACC), i.e. the slot it is about to lose.
//
// ASM WALK:
//   0x822D29BC  GetActiveRaceCar(mePlayerActiveRaceCarIndex)
//   0x822D29C8  ActiveRaceCar::GetDirection(player)              -> v127
//   0x822D29DC  RaceCar::GetDirection(lpRaceCar)                 -> v0
//   0x822D29EC  vmsum3fp128 v0, v0, v127                          the heading dot
//   0x822D2A10  fadds/fmuls/fsubs/fsel                            (dot + 1) * 0.5, min 1.0
//   0x822D29FC  cmpwi r11, 8 ; blt -- the "Hiding events flooded with N stored !" assert
//   0x822D2AE8  fmadds f30, f30, 5.0, 5.0                         record.mfHiddenTime
//   0x822D2AF0  stfs -> +4  /  0x822D2AF4  stw -> +0              the record
//   0x822D2B08  ActiveRaceCar::GetPosition(player)               -> v127
//   0x822D2B1C  RaceCar::GetPosition(lpRaceCar)                  -> v13
//   0x822D2B44  the rsqrt+2NR Magnitude of the difference        -> f29
//   0x822D2B8C  fmuls/fneg/fsel/fsubs/fsel                        clamp(f29 * 0.005, 0, 1)
//   0x822D2BB0  fadds -> record.mfHiddenTime
//   0x822D2BC8  the gxMessageFilterFlags & 1 debug print
//   0x822D2C64  ++miHidingEvents
//
// [GUARD] the console fires its flooded-array assert and then WRITES ANYWAY, off the end of
// the eight-entry array. That is a stack/member smash on this build, so the append is
// skipped after the assert. Said out loud rather than silently: this is the one place this
// file does not keep the console's unconditional behaviour after an assert.
// ============================================================================================
void RaceCarEntityModule::SetHiddenDelay(RaceCar* lpRaceCar)
{
    CGS_ASSERT(lpRaceCar != 0, "lpRaceCar");
    if (lpRaceCar == 0)
    {
        return;
    }

    ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);
    if (lpPlayerActiveRaceCar == 0)
    {
        return;   // [GUARD] -- see GetPlayerRaceCar's banner
    }

    // ---- factor 1: how closely the two cars agree on where "forward" is ----------------
    const f32 lfHeadingDot = rw::math::vpu::Dot(lpRaceCar->GetDirection(),
                                                lpPlayerActiveRaceCar->GetDirection());

    f32 lfHeadingFactor = (lfHeadingDot + KF_HIDE_FACTOR_MAX) * KF_HIDE_HEADING_REMAP_SCALE;
    if (lfHeadingFactor >= KF_HIDE_FACTOR_MAX)
    {
        lfHeadingFactor = KF_HIDE_FACTOR_MAX;                      // fsel f30, f13, f31, f0
    }

    CGS_ASSERT(miHidingEvents < KI_MAX_HIDING_EVENTS,
               "Hiding events flooded");                            // X360 :8932
    if (miHidingEvents >= KI_MAX_HIDING_EVENTS)
    {
        return;   // [GUARD] -- see banner; the console writes past the array here
    }

    RaceCarNeedsHidingEventRecord& lrRecord = mHidingEvents[miHidingEvents];

    lrRecord.miActiveRaceCarIndex = static_cast<s32>(lpRaceCar->GetActiveRaceCarIndex());
    lrRecord.mfHiddenTime         = lfHeadingFactor * KF_HIDE_TIME_PER_FACTOR
                                                    + KF_HIDE_TIME_PER_FACTOR;

    // ---- factor 2: how far apart the two cars are --------------------------------------
    const Vector3 lSeparation =
        lpRaceCar->GetPosition() - lpPlayerActiveRaceCar->GetPosition();
    const f32 lfSeparation = rw::math::vpu::Magnitude(lSeparation);

    f32 lfSeparationFactor = lfSeparation * KF_HIDE_SEPARATION_SCALE;
    if (lfSeparationFactor <= KF_ZERO)
    {
        lfSeparationFactor = KF_ZERO;                               // fneg/fsel pair
    }
    if (lfSeparationFactor >= KF_HIDE_FACTOR_MAX)
    {
        lfSeparationFactor = KF_HIDE_FACTOR_MAX;                    // fsubs/fsel pair
    }

    lrRecord.mfHiddenTime += lfSeparationFactor * KF_HIDE_TIME_PER_FACTOR;

    // 0x822D2BC8 -- the console's own gxMessageFilterFlags-gated trace, kept as a debug print.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "Extra time from separation = " << (lfSeparationFactor * KF_HIDE_TIME_PER_FACTOR)
            << " at " << lfSeparation
            << "M, direction time = "
            << (lfHeadingFactor * KF_HIDE_TIME_PER_FACTOR + KF_HIDE_TIME_PER_FACTOR)
            << "\n";
    }

    ++miHidingEvents;
}

// ============================================================================================
// IsRaceCarWrappable @ 0x822E9E18
//
// "May this rival be teleported relative to the player this frame?" Only reached from
// UpdateInAndOutOfRangeCars, and only for a mode that carries
// KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE (0x20). Every arm that returns TRUE first queues the
// hiding record through SetHiddenDelay.
//
// ASM WALK (0x822E9E38..0x822EA084):
//   0x822E9E38  !lpRaceCar->HasActiveRaceCar()                      -> false
//   0x822E9E50  !lpRaceCar->GetActiveRaceCar()->IsActive()          -> false
//   0x822E9E60  !lpRaceCar->IsAllowedInRoadRage()   [+0xAE]         -> false
//   0x822E9E80  player active car IsCrashing()                      -> false
//   0x822E9E9C  if (!mbIsInGameMode)  ->  return !ShouldBeInRange(GetPlayerRaceCar())
//               (`cntlzw ; extrwi 1,26` @0x822EA070 == the boolean NOT)
//   ---- in a game mode: -----------------------------------------------------------------
//   0x822E9F10  6400.0f > |carPos - playerPos|^2   -> mfClippedTime = 0; return false
//   0x822E9F60  d = dot(carPos - playerPos, playerHeading)
//   0x822E9F7C  d < -80  ||  d > 210               -> SetHiddenDelay; return true
//   0x822E9FEC  |dot(Normalize(carPos - playerPos), playerHeading)| > cos(30 deg)
//                                                  -> mfClippedTime = 0; return false
//   0x822EA018  mfClippedTime += 1; > 120          -> SetHiddenDelay; return true
//                                        else      -> return false
//
// [!] THE NORMALISE IS VMX128, AND vmx128.py WAS RUN ON IT. 0x822E9F8C..0x822E9FE0 is a
// vrsqrtefp128 + two Newton refinements whose IDA-rendered vA fields (v123..v127) carry the
// swapped-high-bit encoding; the raw fields say the pipeline is the standard
// rsqrt(lenSq) broadcast, multiplied into the separation vector at 0x822E9FE8 and dotted with
// the player heading at 0x822E9FEC. That is Normalize() and nothing else.
// ============================================================================================
bool RaceCarEntityModule::IsRaceCarWrappable(RaceCar* lpRaceCar)
{
    CGS_ASSERT(lpRaceCar != 0, "lpRaceCar");
    if (lpRaceCar == 0)
    {
        return false;
    }

    if (!lpRaceCar->HasActiveRaceCar())
    {
        return false;
    }
    if (!lpRaceCar->GetActiveRaceCar()->IsActive())
    {
        return false;
    }
    if (!lpRaceCar->IsAllowedInRoadRage())
    {
        return false;
    }

    ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);
    if (lpPlayerActiveRaceCar == 0)
    {
        return false;   // [GUARD] -- see GetPlayerRaceCar's banner
    }
    if (lpPlayerActiveRaceCar->IsCrashing())
    {
        return false;
    }

    // ---- free burn: wrap exactly when the car is not close enough to keep -------------
    if (!mbIsInGameMode)
    {
        const RaceCar* lpPlayerRaceCar = GetPlayerRaceCar();
        if (lpPlayerRaceCar == 0)
        {
            return false;   // [GUARD]
        }

        return !lpRaceCar->ShouldBeInRange(lpPlayerRaceCar);
    }

    // ---- in a game mode ----------------------------------------------------------------
    const Vector3 lSeparation =
        lpRaceCar->GetPosition() - lpPlayerActiveRaceCar->GetPosition();
    const f32 lfDistanceSquared = rw::math::vpu::Dot(lSeparation, lSeparation);

    if (KF_WRAP_MINIMUM_DISTANCE_SQUARED > lfDistanceSquared)
    {
        mfClippedTime = KF_ZERO;
        return false;
    }

    const Vector3 lPlayerDirection = lpPlayerActiveRaceCar->GetDirection();
    const f32 lfDistanceAlongPlayerHeading = rw::math::vpu::Dot(lSeparation, lPlayerDirection);

    if (lfDistanceAlongPlayerHeading < KF_WRAP_BEHIND_PLAYER_LIMIT ||
        lfDistanceAlongPlayerHeading > KF_WRAP_AHEAD_PLAYER_LIMIT)
    {
        SetHiddenDelay(lpRaceCar);
        return true;
    }

    const f32 lfCosAngleToCar =
        rw::math::vpu::Dot(rw::math::vpu::Normalize(lSeparation), lPlayerDirection);

    if (std::fabs(lfCosAngleToCar) > KF_WRAP_CLIPPED_COS_ANGLE)   // fabs f13, f0 @0x822E9FFC
    {
        mfClippedTime = KF_ZERO;
        return false;
    }

    mfClippedTime += KF_WRAP_CLIPPED_TIME_STEP;
    if (mfClippedTime > KF_WRAP_CLIPPED_TIME_LIMIT)
    {
        SetHiddenDelay(lpRaceCar);
        return true;
    }

    return false;
}

// ============================================================================================
// UpdateHidingEvents @ 0x822F5830
//
// Post every pending record as game event 40 and empty the array. Called from
// PostPhysicsUpdate @0x823076B0 with lpOutput->GetGameEventQueue() -- the SAME queue and the
// SAME accessor (sub_822B67D0) UpdateCurrentWorldRegion is handed one call earlier, which is
// the transport RoadRageMode::HandleGameEvents drains.
//
// The console re-reads miHidingEvents from the member every iteration (`lwz r11, 0(r29)` at
// the loop foot) and only then zeroes it; AddEvent cannot change it, so a hoisted bound is
// the same program.
// ============================================================================================
void RaceCarEntityModule::UpdateHidingEvents(
        RaceCarEntityModuleIO::GameEventQueue* lpGameEventQueue)
{
    CGS_ASSERT(lpGameEventQueue != 0, "lpGameEventQueue");
    if (lpGameEventQueue == 0)
    {
        // [GUARD] the console has no null test; on this build the output buffer's accessor
        // chain is still being assembled. The count is still cleared, exactly as below, so a
        // null queue drops the frame's records rather than replaying them for ever.
        miHidingEvents = 0;
        return;
    }

    for (s32 liEvent = 0; liEvent < miHidingEvents; ++liEvent)
    {
        lpGameEventQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&mHidingEvents[liEvent]),
            KI_EVENT_RACE_CAR_NEEDS_HIDING,
            static_cast<s32>(sizeof(RaceCarNeedsHidingEventRecord)));   // li r5, 0x28 ; li r6, 8
    }

    miHidingEvents = 0;
}

// ============================================================================================
// UpdateInAndOutOfRangeCars @ 0x822FF8F8
//
// See the file banner for the two passes. Notes that only the asm settles:
//
// [!] THE HEAD GATE IS TWO TESTS, NOT ONE (0x822FF924..0x822FF958):
//        if (mbIsInOnlineGameMode)                  return;
//        if (!mbModeStartedPlaying && mbIsInGameMode) return;
//     -- so the loop runs in free burn always, and in an offline game mode only once that
//     mode has actually started playing. mbModeStartedPlaying is DWARF
//     BrnRaceCarEntityModule.h:386 at +0x18354; see the header member's banner for the
//     writer set and the bring-up flag on this build.
//
// [!] THE WRAP ARM AND THE DETACH ARM ARE MUTUALLY EXCLUSIVE (0x822FFAA0). A mode that wraps
//     AI cars NEVER detaches them from this function -- it teleports them and keeps the slot.
//     Road Rage is such a mode (KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE is 0x20).
//
// [!] THE WRAP SPEED/STYLE IS A COIN FLIP, AND IT IS THE MODULE TU's OWN Random.
//     0x822FFB28..0x822FFB68 draws one RandomUInt and tests `draw & 1` -- except in Marked Man
//     (meGameModeType == 8), where it tests `draw % 5 == 0`, i.e. the from-turnings style is
//     five times rarer there.
//       draw hit  -> E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE, distance 0, speed
//                    (playerSpeed >= 60 mph) ? playerSpeed - 30 mph : 120 mph
//       draw miss -> E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE, speed min(playerSpeed + 20 mph,
//                    160 mph), distance -20 m in Road Rage / Marked Man, -50 m otherwise
//     The player SPEED is |ActiveRaceCar::GetVelocity(playerSlot)| (the rsqrt+2NR pipeline at
//     0x822FFAE0..0x822FFB24), not a stored member.
//
// [!] `mbWonLastEvent` (+0x78C on the PLAYER's active car) is an OVERRIDE IN BOTH PASSES, in
//     opposite directions: while it is set, pass 1 never detaches anybody and pass 2 attaches
//     every out-of-range rival regardless of distance. That is the end-of-event "bring the
//     whole field back for the celebration" behaviour. The member is the one this tree already
//     pinned at that offset (BrnActiveRaceCar.h:1191).
//
// [!] PASS 2's EIGHT-CAR CEILING IS CHECKED AFTER GetGlobalRaceCar, NOT BEFORE
//     (0x822FFCCC..0x822FFCDC), and it BREAKS out of the loop rather than skipping the slot.
// ============================================================================================
void RaceCarEntityModule::UpdateInAndOutOfRangeCars(
        RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput)
{
    s32 liAttachedCars = 0;

    if (mbIsInOnlineGameMode)                                       // +0x18345
    {
        return;
    }
    if (!mbModeStartedPlaying && mbIsInGameMode)                    // +0x18354 / +0x18344
    {
        return;
    }
    if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
    {
        return;                                                     // cmpwi cr6, r4, -1 @0x822FF96C
    }

    ActiveRaceCar* lpPlayerActiveRaceCar = GetActiveRaceCar(mePlayerActiveRaceCarIndex);
    if (lpPlayerActiveRaceCar == 0 || !lpPlayerActiveRaceCar->IsActive())
    {
        return;
    }

    // The console re-reads this flag per car (`li r4, 0x20 ; bl GetGameModeFlag` @0x822FFA90,
    // inside pass 1); GetGameModeFlag is a pure read of mxGameModeFlags and nothing in the loop
    // writes that member, so hoisting it is the same program.
    const bool lbWrapAICars = GetGameModeFlag(
        BrnGameState::GameModeParams::KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE);   // li r4, 0x20

    // ========================================================================================
    // PASS 1 (0x822FF9F4..0x822FFC90) -- push in-range rivals out, or wrap them.
    // ========================================================================================
    for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
    {
        ActiveRaceCar* lpActiveRaceCar =
            GetActiveRaceCar(static_cast<EActiveRaceCarIndex>(liSlot));

        if (lpActiveRaceCar != 0 && lpActiveRaceCar->IsAttached())
        {
            ++liAttachedCars;

            RaceCar* lpRaceCar = 0;
            if (!lpActiveRaceCar->IsCrashing() &&
                lpActiveRaceCar->IsOnRaceStartState(ActiveRaceCar::E_RACE_START_STATE_RACING) &&
                !lpActiveRaceCar->IsWaitingForLoad())        // lbz 0x740 ; cmplwi 1 ; beq -> skip
            {
                lpRaceCar = lpActiveRaceCar->GetGlobalRaceCar();
            }

            if (lpRaceCar != 0 && lpRaceCar->IsAIDriven() && lpRaceCar->IsInRangeRival())
            {
                if (lbWrapAICars)
                {
                    // ---- the WRAP arm (0x822FFAA4..0x822FFBF0) ----------------------------
                    if (IsRaceCarWrappable(lpRaceCar))
                    {
                        const f32 lfPlayerSpeed =
                            rw::math::vpu::Magnitude(lpPlayerActiveRaceCar->GetVelocity());

                        BrnAI::EResetType leResetType;
                        f32               lfResetSpeed;
                        f32               lfResetDistance = KF_ZERO;   // fmr f30, f29

                        const u32  luDraw = GetModuleRandom().RandomUInt();
                        const bool lbFromTurnings =
                            (meGameModeType ==
                             BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN)
                                ? ((luDraw % KU_WRAP_MARKED_MAN_DRAW_MODULUS) == 0u)
                                : ((luDraw & 1u) != 0u);

                        if (lbFromTurnings)
                        {
                            leResetType  = BrnAI::E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE;   // li r31, 5
                            lfResetSpeed = (lfPlayerSpeed >= KF_WRAP_PLAYER_SPEED_THRESHOLD)
                                ? (lfPlayerSpeed - KF_WRAP_SPEED_BELOW_PLAYER)
                                : KF_WRAP_SPEED_PLAYER_SLOW;
                        }
                        else
                        {
                            leResetType  = BrnAI::E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE;   // li r31, 3
                            lfResetSpeed = lfPlayerSpeed + KF_RESET_SPEED_ABOVE_PLAYER;
                            if (lfResetSpeed >= KF_RESET_SPEED_MAX)
                            {
                                lfResetSpeed = KF_RESET_SPEED_MAX;             // fsel f31, f12, f13, f0
                            }

                            lfResetDistance =
                                (meGameModeType ==
                                     BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE ||
                                 meGameModeType ==
                                     BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN)
                                    ? KF_RESET_DISTANCE_BEHIND_CLOSE
                                    : KF_RESET_DISTANCE_BEHIND;
                        }

                        lpActiveRaceCar->GetGlobalRaceCar()->RequestResetOnTrack(
                            lfResetSpeed, leResetType, lfResetDistance);

                        WitnessRangeTransition(liSlot, "wrap",
                                               rw::math::vpu::Magnitude(
                                                   lpRaceCar->GetPosition() -
                                                   lpPlayerActiveRaceCar->GetPosition()),
                                               lfResetSpeed);
                    }
                }
                else
                {
                    // ---- the DETACH arm (0x822FFBF4..0x822FFC64) --------------------------
                    RaceCar* lpPlayerRaceCar = GetPlayerRaceCar();

                    if (lpPlayerRaceCar != 0 &&
                        lpRaceCar->ShouldBeOutOfRange(lpPlayerRaceCar) &&
                        !lpPlayerActiveRaceCar->mbWonLastEvent)               // lbz 0x78C
                    {
                        // The console evaluates the three output accessors in THIS order
                        // (0x822FFC30 / 0x822FFC3C / 0x822FFC48); each carries its own
                        // "Not locked for writing" tripwire, so the order is observable.
                        CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneInputInterface =
                            lpOutput->GetSceneInputInterface();
                        BrnAI::AIModuleIO::RaceCarAIInterface* lpRaceCarAIInterface =
                            lpOutput->GetRaceCarAIInterface();
                        BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInputInterface =
                            lpOutput->GetVehicleInputInterface();

                        WitnessRangeTransition(liSlot, "in->out",
                                               rw::math::vpu::Magnitude(
                                                   lpRaceCar->GetPosition() -
                                                   lpPlayerRaceCar->GetPosition()),
                                               KF_ZERO);

                        DetachActiveRaceCar(lpRaceCar, lpVehicleInputInterface,
                                            lpRaceCarAIInterface, lpSceneInputInterface);
                    }
                }
            }
        }

        CGS_ASSERT(liSlot + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");   // BurnoutConstants.h:39
    }

    // ========================================================================================
    // PASS 2 (0x822FFCC4..0x822FFE14) -- pull out-of-range rivals back in.
    // ========================================================================================
    for (s32 liGlobalSlot = 0; liGlobalSlot < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobalSlot)
    {
        RaceCar* lpRaceCar = GetGlobalRaceCar(static_cast<EGlobalRaceCarIndex>(liGlobalSlot));

        if (liAttachedCars >= E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            break;                                              // cmpwi r22, 8 ; bge -> exit
        }

        CGS_ASSERT(lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT");            // BrnRaceCar.h:547

        if (!lpRaceCar->IsInWorld() || !lpRaceCar->IsAIDriven())
        {
            CGS_ASSERT(liGlobalSlot + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");   // BurnoutConstants.h:84
            continue;
        }
        if (mbIsInGameMode && !lpRaceCar->IsInCurrentGameMode())
        {
            CGS_ASSERT(liGlobalSlot + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
            continue;
        }
        if (!lpRaceCar->IsOutOfRangeRival())
        {
            CGS_ASSERT(liGlobalSlot + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
            continue;
        }

        RaceCar* lpPlayerRaceCar = GetPlayerRaceCar();

        const bool lbBringIn =
            (lpPlayerRaceCar != 0 && lpRaceCar->ShouldBeInRange(lpPlayerRaceCar)) ||
            lpPlayerActiveRaceCar->mbWonLastEvent;                       // lbz 0x78C

        if (lbBringIn)
        {
            WitnessRangeTransition(
                liGlobalSlot, "out->in",
                (lpPlayerRaceCar != 0)
                    ? rw::math::vpu::Magnitude(lpRaceCar->GetPosition() -
                                               lpPlayerRaceCar->GetPosition())
                    : KF_ZERO,
                KF_ZERO);

            AttachActiveRaceCar(lpRaceCar, E_ACTIVE_RACE_CAR_INDEX_INVALID);   // li r5, -1

            // 0x822FFDC0 `stb r24(1), 0x775(r11)` -- ActiveRaceCar::mbComingInRange.
            ActiveRaceCar* lpAttachedCar = lpRaceCar->GetActiveRaceCar();
            if (lpAttachedCar != 0)
            {
                lpAttachedCar->mbComingInRange = true;
            }
            ++liAttachedCars;

            CGS_ASSERT(lpRaceCar->GetActiveRaceCar() != 0,
                       "lpRaceCar->GetActiveRaceCar() != NULL");    // X360 :5504
        }

        CGS_ASSERT(liGlobalSlot + 1 <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");   // BurnoutConstants.h:84
    }
}

// ============================================================================================
// ReadOutOfRangeRaceCarDataFromAI @ 0x822E9188
//
// THE OUT-OF-RANGE POSE RETURN PATH. For every global slot the AI is simulating WITHOUT an
// active car, rebuild a world transform from the AI's {At, Position} pair and store it into
// the RaceCar. Without it a rival that goes out of range freezes where it was dropped, so
// pass 2 of UpdateInAndOutOfRangeCars would re-attach it at a stale position kilometres away.
// Called from PostPhysicsUpdate @0x82307604, between ProcessCreateVehicleEvents and
// ReadUpdatedActiveRaceCarDataFromPhysics, with the post-physics INPUT buffer.
//
// ASM WALK (per global slot, r27 == &maRaceCars[i].muType, stride 0xB0):
//   0x822E923C  SetCanPassThroughTraffic(ai.CanPassThroughTraffic(i))   (`stb r3, 9(r27)` == +0xAD)
//   0x822E9268  run the rest only if the slot is NOT in the world, has no active car, or its
//               active car is still E_STATE_ATTACHED (`lbz 0x740 ; cmplwi 1`)
//   0x822E92A4  ai.WasInactiveRaceCarUpdated(i)   -- else skip
//   0x822E9300  lUp = unk_82181510 == {0,1,0,0}   (the world Y axis; == GetVector3_YAxis())
//   0x822E9314  lAt = ai.GetInactiveRaceCarAt(i)
//   0x822E9424  lRight = Cross(lUp, lAt)          (the vpermwi 0x63 == yzx permute pair)
//   0x822E94B4  |lRight| > flt_82014460 == FLT_EPSILON ?
//   0x822E94CC  no  -> lUp = rw::math::vpu::detail::gIVector == {1,0,0,0}; recompute lRight
//   0x822E952C  lRight = Normalize(lRight)
//   0x822E957C  lUp    = Normalize(Cross(lAt, lRight))
//   0x822E9584  lPos   = ai.GetInactiveRaceCarPosition(i)
//   0x822E96A0  UpdatePositioningData(lTransform, &mWorldMap2D)   (`this + 0x18300`)
//
// [!] THE TRANSFORM IS THE FOUR CONTIGUOUS STACK SLOTS var_120/var_110/var_100/var_F0, i.e.
//     xAxis = Right, yAxis = Up, zAxis = At, wAxis = Pos -- var_120 is what
//     UpdatePositioningData receives. The four IsValid asserts fire in the order
//     At (:5931) / Up (:5932) / Right (:5945) / Pos (:5946), which is what names each row.
// ============================================================================================
void RaceCarEntityModule::ReadOutOfRangeRaceCarDataFromAI(
        RaceCarEntityModuleIO::InputBuffer_PostPhysics* lpInput)
{
    CGS_ASSERT(lpInput != 0, "lpInputBuffer");
    if (lpInput == 0)
    {
        return;
    }

    const BrnAI::AIModuleIO::AIRaceCarInterface* lpAIRaceCarInterface =
        lpInput->GetAIRaceCarInterface();
    if (lpAIRaceCarInterface == 0)
    {
        return;   // [GUARD] -- the console's accessor cannot return null (embedded sub-object)
    }

    for (s32 liGlobalSlot = 0; liGlobalSlot < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liGlobalSlot)
    {
        RaceCar* lpRaceCar = GetGlobalRaceCar(static_cast<EGlobalRaceCarIndex>(liGlobalSlot));

        const s8 li8Slot = static_cast<s8>(liGlobalSlot);   // `extsb r30, r11` @0x822E9234

        lpRaceCar->SetCanPassThroughTraffic(
            lpAIRaceCarInterface->CanPassThroughTraffic(li8Slot));

        CGS_ASSERT(lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT");        // BrnRaceCar.h:547

        // 0x822E9268: the slot is handled here only while the AI owns it -- i.e. it is out of
        // the world, or it has no active car, or its active car has not finished loading.
        const ActiveRaceCar* lpActiveRaceCar = lpRaceCar->GetActiveRaceCar();
        const bool lbSimulatedByAI =
            !lpRaceCar->IsInWorld() ||
            lpActiveRaceCar == 0 ||
            !lpActiveRaceCar->IsWaitingForLoad();

        if (!lbSimulatedByAI)
        {
            continue;
        }
        if (!lpAIRaceCarInterface->WasInactiveRaceCarUpdated(li8Slot))
        {
            continue;
        }

        CGS_ASSERT(lpRaceCar->GetType() < E_RACE_CAR_TYPE_COUNT,
                   "muType < E_RACE_CAR_TYPE_COUNT");        // BrnRaceCar.h:547
        CGS_ASSERT(lpRaceCar->IsInWorld(), "lpRaceCar->IsInWorld()");   // X360 :5926

        Vector3 lUp = rw::math::vpu::GetVector3_YAxis();      // unk_82181510 == {0,1,0,0}
        const Vector3 lAt = lpAIRaceCarInterface->GetInactiveRaceCarAt(li8Slot);

        CGS_ASSERT(rw::math::vpu::IsValid(lAt), "RwMath::IsValid(lTransform.At())");   // :5931
        CGS_ASSERT(rw::math::vpu::IsValid(lUp), "RwMath::IsValid(lTransform.Up())");   // :5932

        Vector3 lRight = rw::math::vpu::Cross(lUp, lAt);
        if (!(rw::math::vpu::Magnitude(lRight) > KF_CROSS_PRODUCT_EPSILON))
        {
            // 0x822E94CC -- lAt is (near enough) the world Y axis, so pick a different up.
            lUp    = rw::math::vpu::GetVector3_XAxis();       // detail::gIVector == {1,0,0,0}
            lRight = rw::math::vpu::Cross(lUp, lAt);
        }

        lRight = rw::math::vpu::Normalize(lRight);
        lUp    = rw::math::vpu::Normalize(rw::math::vpu::Cross(lAt, lRight));

        const Vector3 lPosition = lpAIRaceCarInterface->GetInactiveRaceCarPosition(li8Slot);

        CGS_ASSERT(rw::math::vpu::IsValid(lRight),
                   "RwMath::IsValid(lTransform.Right())");                              // :5945
        CGS_ASSERT(rw::math::vpu::IsValid(lPosition),
                   "RwMath::IsValid(lTransform.Pos())");                                // :5946

        Matrix44Affine lTransform;
        lTransform.Right() = lRight;
        lTransform.Up()    = lUp;
        lTransform.At()    = lAt;
        lTransform.Pos()   = lPosition;

        lpRaceCar->UpdatePositioningData(lTransform, &mWorldMap2D);   // `this + 0x18300`
    }
}

}
