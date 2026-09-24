// =============================================================================
// GameSource/World/AI/BrnAIBuzzBy.cpp  (X360 ARTIST)
//
// BrnAI::BuzzBy -- the AI "buzz-by" director. Reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX. Bodies homed here:
//   AICarCanBuzz @0x82767020, BuzzOccured @0x82771BA8, ChooseAheadOrBehind @0x827718B8,
//   IsPlayerBuzzable @0x827719F8, IsPositionInNoBuzzZone @0x82766FC0,
//   MaintainAheadOrBehind @0x82766C40 (crash parity FX-AIBUZZ, 2026-09-24),
//   ResetActiveList @0x82771C90, StartABuzzBy @0x8278B858, Update @0x8278B8C8.
// (GetBuzzFrequency / Prepare / the remaining API land with sibling waves -- declared only.)
// =============================================================================

#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot / MagnitudeSquared / Magnitude / Normalize / operator-
#include "rw/math/fpu/scalar_operation.h"    // rw::math::fpu::KF_IS_ZERO_TOLERANCE (FLT_EPSILON, flt_820C3B70)
#include <cmath>                             // std::fabs (MaintainAheadOrBehind's console IsZero)

namespace BrnAI
{
    // --- file-scope tuning constants (DWARF-`extern`; grounded by the asm rodata uses) ------
    // ⭐⭐ THE FOUR "(runtime; value not recovered)" ZEROS ARE RECOVERED (2026-09-06, driving-path
    // 1:1 constant audit).  They were never un-homed: three are .bss slots the CRT fills at
    // static-init time, in the SAME mph->m/s shape BrnAICar_Constants.h already documents --
    //     lfs f0, flt_82F31928 (0.44703999) ; lfs f13, <mph> ; fmuls f0,f0,f13 ; stfs f0, <slot>
    // -- and the fourth is plain .rdata that a literal scan would have found.  A flagged zero is
    // only safe when 0 is the expression's identity, and here it was not: MIN_SPEED_FOR_BUZZING
    // is a `speed < K -> refuse` gate, so at 0.0 the player was buzzable while STOPPED, and the
    // two START_FAR_* feed a reset request's SPEED field, so road-rage rivals were teleported in
    // at a standstill.
    // ⚠️ NO STATIC-INIT ORDER HAZARD HERE (the trap BrnTrafficEntityModule_wT2_03.cpp documents
    // for flt_830180B0): the multiplier flt_82F31928 is a plain image constant that reads
    // 0.44703999 straight out of .rdata, not a slot another thunk has to compute first.
    // ⭐ NAMES CORRECTED 2026-09-24 (crash parity FX-AIBUZZ; VALUES UNCHANGED). The two .bss speeds
    // were filed as KF_START_FAR_AHEAD / KF_START_FAR_BEHIND and the 200.0 literal as
    // KF_ON_COMING_RESET_SPEED. Two independent readings put the DWARF names elsewhere:
    //   * the PS3 twins name them: DecFIGS ChooseAheadOrBehind 0x9CE24C and MaintainAheadOrBehind
    //     0x9CE620 load the AHEAD speed from BrnAI::KF_ON_COMING_RESET_SPEED and the BEHIND one from
    //     BrnAI::KF_FASTER_THAN_PLAYER (+ the player's speed); both distances are TOC literals
    //     (200.0 / -60.0), exactly where the X360 reads the pooled .rdata flt_820C4318 / flt_820C431C;
    //   * the CRT thunks run in declaration order (DWARF BrnAIBuzzBy.cpp:29 FASTER_THAN_PLAYER,
    //     :30 ON_COMING_RESET_SPEED, :36 MIN_SPEED_FOR_BUZZING, :57 the no-buzz centres), and this
    //     TU's are 0x82C68EC8 (25 mph) -> 0x82C68EE8 (80 mph) -> 0x82C68F08 (7.5 mph) -> 0x82C68F28
    //     (the centres); the two thunks before them (0x82C68E88 / 0x82C68EA8) feed AIAggression's
    //     GetSpeedMatchSpeed / UpdateAggressionStateClipOffBehind. START_FAR_AHEAD / START_FAR_BEHIND
    //     (DWARF :31 / :32) have no thunk and no reader in the X360 image, so they are not defined.
    const f32 KF_FASTER_THAN_PLAYER       = 11.1759996f;  // flt_8300D7F4 = thunk 0x82C68EC8, flt_820C4870 ( 25) * flt_82F31928 (0.44704)
    const f32 KF_ON_COMING_RESET_SPEED    = 35.7631989f;  // flt_8300DBEC = thunk 0x82C68EE8, flt_82004A18 ( 80) * 0.44704
    const f32 KF_MIN_SPEED_FOR_BUZZING    =  3.35279989f; // flt_8300D938 = init 0x82C68F20, flt_820C42D4 (7.5) * 0.44704
    const f32 KF_START_BEHIND_PROBABLITY  = 0.5f;    // flt_820C4168 (rodata-pinned)
    // Plain .rdata, read straight out of the image (0x820C4330 == 0x3F4CCCCD).  ChooseAheadOrBehind
    // @0x8277197C does `fcmpu cr6, roll, flt_820C4330 ; bge` -> ON_COMING, so this is a 20%/80%
    // split between the head-on and from-turnings reset types.
    const f32 KF_SIDE_TURNING_PROBABILITY = 0.800000012f;  // flt_820C4330 (plain .rdata, 0x3F4CCCCD)

    // DWARF marks BuzzBy::mRandom `extern` -- it is a TU-local file-scope static (mirroring
    // BrnRouteRequestManager's mRandom), NOT a per-instance member.
    static CgsNumeric::Random mRandom;

    // No-buzz sphere geometry -- seven zones, 16-byte centres at unk_8300DB30 (the 0x70 loop span
    // IsPositionInNoBuzzZone walks, 0x82766FC8..0x8276700C) and 4-byte radii at unk_820C4334.
    // Crash parity FX-AINAN2: both tables were zero placeholders ("not recovered"), so every
    // `distSq < radius^2` test was `< 0` and no zone ever held a buzz-by back.
    //   * The RADII are plain .rdata, read straight out of the image (x360rd 820C4334, 7 x f32).
    //   * The CENTRES slot is .data that reads zero by definition: the CRT thunk @0x82C68F28
    //     (tools/re/findinit.py 8300DB30 -> its one writer, 0x82C69058) builds the seven
    //     Vector3s on the stack from .rdata floats and stvx128's them to 0x8300DB30..0x8300DB90
    //     (0x82C69064..0x82C690C0); every w lane is `stw 0`. Source floats per lane:
    //       0: 820C8C28 / 820C4158 / 820C8C20     1: 820C8C1C / 820C8C18 / 820C8C14
    //       2: 820C8C10 / 820C8C24 / 820C8C0C     3: 820C8C08 / 820C8C24 / 820C8C04
    //       4: 820C8C00 / 82056BE4 / 820C8BFC     5: 820C8BF8 / 820C8BF4 / 820C8BF0
    //       6: 820C8BEC / 820C8BF4 / 820C8BE8
    static const Vector3 KA_NO_BUZZ_ZONE_CENTRES[KI_NUM_NO_BUZZ_ZONES] =   // unk_8300DB30
    {
        { -2430.80005f,  60.0f,        1906.59998f, 0.0f },
        { -2604.80005f,  82.0f,        1900.59998f, 0.0f },
        { -2229.80005f,  19.5f,        625.400024f, 0.0f },
        { -2205.80005f,  19.5f,        808.400024f, 0.0f },
        { -1981.80005f,  96.5f,        698.400024f, 0.0f },
        { -1059.69995f,  105.199997f, -1509.30005f, 0.0f },
        { -1054.69995f,  105.199997f, -1270.30005f, 0.0f },
    };
    static const f32 KAF_NO_BUZZ_ZONE_RADII[KI_NUM_NO_BUZZ_ZONES] =        // unk_820C4334
    {
        230.0f, 210.0f, 260.0f, 240.0f, 35.0f, 185.0f, 175.0f
    };

    // ------------------------------------------------------------------------
    // AICarCanBuzz @0x82767020
    // The car can buzz once its buzz distance-to-player reaches 200 units.
    // ------------------------------------------------------------------------
    bool BuzzBy::AICarCanBuzz(const AICar* lpCar)
    {
        CGS_ASSERT(lpCar != NULL, "lpCar != NULL");

        // 0x82767020: lfs f13,0x1508(lpCar); lfs f0,flt_820C4318(=200.0); blt -> 0 else 1.
        // NaN polarity (FX-AINAN2): `li r3,0 ; fcmpu ; blt 0x82767070 ; li r3,1` -- blt is not
        // taken on an unordered compare, so a NaN distance CAN buzz; `>= 200` said no.
        return !(lpCar->mfBuzzDistanceToPlayer < 200.0f);
    }

    // ------------------------------------------------------------------------
    // BuzzOccured @0x82771BA8
    // ------------------------------------------------------------------------
    bool BuzzBy::BuzzOccured(const AICar* lpPlayerCar, const AICar* lpAICar)
    {
        if (lpAICar == NULL)
        {
            return false;
        }

        // flt_820C3FA8 = 30.0f: too far from the player for the buzz to register.
        if (lpAICar->mfBuzzDistanceToPlayer > 30.0f)
        {
            return false;
        }

        const Vector3 lAIPosition      = lpAICar->GetPosition();
        const Vector3 lPlayerPosition  = lpPlayerCar->GetPosition();
        const Vector3 lPlayerDirection = lpPlayerCar->GetDirection();

        // dot(playerPos - aiPos, playerDir): negative once the AI car is behind the player's
        // facing. The buzz "occured" when that projection drops below flt_8200D5FC (-0.7f).
        const Vector3 lRelativePosition = rw::math::vpu::operator-(lPlayerPosition, lAIPosition);
        const f32 lfProjection = rw::math::vpu::Dot(lRelativePosition, lPlayerDirection);

        return -0.7f > lfProjection;
    }

    // vpu::IsZero(v, FLT_EPSILON) as MaintainAheadOrBehind inlines it (0x82766C44..0x82766C84):
    // `vandc` (|v|), `vrlimi128 v11, v13, 1, 1` (w <- x), `vcmpgtfp.` against splat(flt_820C3B70),
    // then the CR6 "all false" bit (`extrwi r10, r10, 1, 26`). "No lane above the tolerance" is
    // zero, and a NaN lane is never above it -- the shared vpu::IsZero calls a NaN lane non-zero,
    // so the console form is spelled here (the BrnAICar_Update.cpp IsZeroVmx precedent).
    static inline bool IsZeroVmx(const Vector3& lrVector)
    {
        return !(std::fabs(lrVector.x) > rw::math::fpu::KF_IS_ZERO_TOLERANCE) &&
               !(std::fabs(lrVector.y) > rw::math::fpu::KF_IS_ZERO_TOLERANCE) &&
               !(std::fabs(lrVector.z) > rw::math::fpu::KF_IS_ZERO_TOLERANCE);
    }

    // ------------------------------------------------------------------------
    // MaintainAheadOrBehind @0x82766C40  (static -- see the header; DWARF BrnAIBuzzBy.cpp:124)
    //
    // Free-roam placement of a rival that has just streamed in within 250 m of the player: its
    // only caller is RaceCarEntityModule::PlaceRaceCarOnLoad's ARM A (0x822CE780..0x822CE7FC),
    // which hands the result straight to RaceCar::RequestResetOnTrack. The car is kept on the side
    // of the player it loaded on: AHEAD -> reset 200 m ahead at the on-coming speed, type 5 when it
    // faces the player's way and type 4 when it faces him; otherwise (behind, level, or NaN) ->
    // a road-rage reset 60 m back at the player's speed + 25 mph.
    //   0x82766C40  vsubfp v0, v1(lPosition), v3(lPlayerPosition)          lRelativePosition
    //   0x82766C44..0x82766C84  IsZero(lRelativePosition) (above) -> bne skips the normalise
    //   0x82766C88..0x82766CC4  vmsum3fp128 + vrsqrtefp + two Newton steps + vmulfp128 -- no zero
    //                           guard; the IsZero screen is the guard (|lane| > FLT_EPSILON, so
    //                           the squared length is > 0), which is why the SDK Normalize's own
    //                           zero arm can never run here
    //   0x82766CCC..0x82766D0C  vmsum3fp128 (rel, v5 lPlayerDirection) ; vcmpgtfp. > splat(flt_82001CC0
    //                           0.0) ; CR6 "all true" (extrwi 1,24) ; beq -> BEHIND (NaN -> BEHIND)
    //   AHEAD  0x82766D10..0x82766D5C  vmsum3fp128 (v2 lDirection, v5) ; vcmpgtfp. > 0.0 -> r10 ;
    //                           `ori r9, r10, 4` -> +0xC (5 same way, 4 facing / NaN) ;
    //                           +8 = flt_820C4318 (200.0) ; +0 = 0 ; +4 = flt_8300DBEC
    //   BEHIND 0x82766D64..0x82766DF4  +8 = flt_820C431C (-60.0) ; +0 = 0 ; +0xC = 3 ;
    //                           +4 = |lPlayerVelocity| (vmsum3fp128 + rsqrt, `vcmpeqfp ; vsel` -> 0
    //                           for a zero velocity) `vaddfp` splat(flt_8300D7F4)
    // ResetOnTrackRequest::Construct is inlined with the race car index 0 (`li r11, 0 ; stw r11,
    // 0(r3)` in both arms; the PS3 twin 0x9CE738 / 0x9CE7F4 stores the same 0) -- the caller reads
    // only +4 / +8 / +0xC.
    // [FX-AIBUZZ 2026-09-24: was declaration-only, and PlaceRaceCarOnLoad parked on it]
    // ------------------------------------------------------------------------
    void BuzzBy::MaintainAheadOrBehind(AIModuleIO::ResetOnTrackRequest* lpRequest,
                                       Vector3 lPosition, Vector3 lDirection, Vector3 lPlayerPosition,
                                       Vector3 lPlayerVelocity, Vector3 lPlayerDirection)
    {
        const Vector3 lRelativePosition = rw::math::vpu::operator-(lPosition, lPlayerPosition);

        Vector3 lRelativeDirection = lRelativePosition;
        if (!IsZeroVmx(lRelativePosition))
        {
            lRelativeDirection = rw::math::vpu::Normalize(lRelativePosition);
        }

        EResetType luResetFlags;
        f32        lfResetSpeed;
        f32        lfResetDistance;

        if (rw::math::vpu::Dot(lRelativeDirection, lPlayerDirection) > 0.0f)
        {
            // In front of the player: keep it there, facing whichever way it already faces.
            luResetFlags    = (rw::math::vpu::Dot(lDirection, lPlayerDirection) > 0.0f)
                                  ? E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE        // 4 | 1
                                  : E_RESET_TYPE_AHEAD_PLAYER_ON_COMING;        // 4 | 0
            lfResetSpeed    = KF_ON_COMING_RESET_SPEED;                         // flt_8300DBEC
            lfResetDistance = 200.0f;                                           // flt_820C4318
        }
        else
        {
            // Behind the player: bring it up from behind, faster than the player.
            luResetFlags    = E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE;             // li r9, 3
            lfResetSpeed    = rw::math::vpu::Magnitude(lPlayerVelocity) + KF_FASTER_THAN_PLAYER;
            lfResetDistance = -60.0f;                                           // flt_820C431C
        }

        lpRequest->Construct(E_GLOBAL_RACE_CAR_INDEX_0, lfResetSpeed, lfResetDistance, luResetFlags);
    }

    // ------------------------------------------------------------------------
    // ChooseAheadOrBehind @0x827718B8
    // ------------------------------------------------------------------------
    void BuzzBy::ChooseAheadOrBehind(AIModuleIO::ResetOnTrackRequest* lpRequest,
                                     f32 lfPlayerSpeed,
                                     EGlobalRaceCarIndex leGlobalRaceCarToTeleport)
    {
        // ⭐⭐ THE `- 1.0f` THAT USED TO BE ON BOTH COMPARISONS WAS A DOUBLE SUBTRACTION, and it
        // made the whole far-AHEAD half of this function DEAD CODE (2026-09-06 driving-path audit).
        // The console's ring-buffer draw yields a float in [1,2) and the caller subtracts 1.0 --
        // `lis r7,0x3F80 ; inslwi r7,r8,23,9` then `fsubs f12, f13, flt_82001C98(1.0)` at
        // 0x8277192C..0x82771934 -- but OUR CgsNumeric::Random::RandomFloat() already performs
        // that subtraction internally (CgsRandom.cpp: `lfRandomFraction = lfRandomFractionPlusOne
        // - 1.0f; return lfRandomFraction;`).  Subtracting again put the roll in [-1, 0), so
        // `roll <= 0.5` was ALWAYS true and every buzz-by reset took the BEHIND arm; the console
        // takes it about half the time.  Nothing can see this but the asm: it compiles, links,
        // runs, and simply never chooses the other branch.
        //   asm 0x82771940  fcmpu cr6, roll, flt_820C4168 (0.5) ; ble -> the BEHIND arm
        //   asm 0x8277197C  fcmpu cr6, roll2, flt_820C4330 (0.8) ; bge -> ON_COMING else TURNINGS
        const f32 lfAheadLikelyHood = mRandom.RandomFloat();   // already the uniform [0,1) roll

        if (lfAheadLikelyHood <= KF_START_BEHIND_PROBABLITY)
        {
            // Start the reset far BEHIND the player, road-rage style. asm 0x827719DC..0x827719EC:
            // f1 = flt_8300D7F4 (KF_FASTER_THAN_PLAYER) + speed (`fadds f1, f0, f1`), f2 =
            // flt_820C431C (-60.0), type 3. Construct stores f1->resetSpeed(+4), f2->resetDistance(+8).
            lpRequest->Construct(leGlobalRaceCarToTeleport,
                                 KF_FASTER_THAN_PLAYER + lfPlayerSpeed,
                                 -60.0f,
                                 E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE);
        }
        else
        {
            // Far AHEAD: a second roll picks head-on (type 4) vs from-turnings (type 5).
            const f32 lfSideTurningsLikelyHood = mRandom.RandomFloat();

            EResetType leResetType;
            if (lfSideTurningsLikelyHood >= KF_SIDE_TURNING_PROBABILITY)
            {
                leResetType = E_RESET_TYPE_AHEAD_PLAYER_ON_COMING;
            }
            else
            {
                leResetType = E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE;
            }

            // asm 0x827719AC..0x827719B4: f1 = flt_8300DBEC (KF_ON_COMING_RESET_SPEED), f2 =
            // flt_820C4318 (200.0, the reset distance).
            lpRequest->Construct(leGlobalRaceCarToTeleport,
                                 KF_ON_COMING_RESET_SPEED,
                                 200.0f,
                                 leResetType);
        }
    }

    // ------------------------------------------------------------------------
    // IsPlayerBuzzable @0x827719F8
    // ------------------------------------------------------------------------
    bool BuzzBy::IsPlayerBuzzable(AICar* lpPlayerCar)
    {
        // The player must be actively driving, up to speed, not crashing or in a shortcut, and
        // we must not already have a full backlog of cars awaiting collection.
        if (lpPlayerCar->mbIsInShortcut)      // 0x154C
        {
            return false;
        }
        if (lpPlayerCar->mbIsCrashing)        // 0x1542
        {
            return false;
        }
        if (lpPlayerCar->GetSpeed() < KF_MIN_SPEED_FOR_BUZZING)
        {
            return false;
        }
        if (!lpPlayerCar->mbIsDrivenByPlayer) // 0x154A
        {
            return false;
        }
        if (miCarsAwaitingCollection >= KI_MAX_CARS_AWAITING_COLLECTION)
        {
            return false;
        }

        const Vector3 lPlayerPosition = lpPlayerCar->GetPosition();
        if (IsPositionInNoBuzzZone(lPlayerPosition))
        {
            return false;
        }

        // Count idle/active (IN_RANGE / OUT_OF_RANGE) non-player buzz cars within
        // flt_8201C220 (40000 == 200^2) of the player.
        s32 liNearbyBuzzCars = 0;
        for (s32 liActive = 0; liActive < miNumActiveCars; ++liActive)
        {
            AICar* lpCar = &mpGlobalRaceCars[maeActiveList[liActive]];

            const EAICarState leState = lpCar->GetState();
            const bool lbEligible = (leState == E_AI_CAR_STATE_IN_RANGE) ||
                                    (leState == E_AI_CAR_STATE_OUT_OF_RANGE);

            if (lbEligible && !lpCar->mbIsPlayer)
            {
                const Vector3 lCarPosition = lpCar->GetPosition();
                const Vector3 lRelativePosition =
                    rw::math::vpu::operator-(lPlayerPosition, lCarPosition);
                const f32 lfDistanceSq = rw::math::vpu::MagnitudeSquared(lRelativePosition);

                if (40000.0f > lfDistanceSq)
                {
                    ++liNearbyBuzzCars;
                }
            }
        }

        return liNearbyBuzzCars < 1;
    }

    // ------------------------------------------------------------------------
    // IsPositionInNoBuzzZone @0x82766FC0
    // Inside any no-buzz sphere?  distanceSq(lPosition, centre) < radiusSq.
    // ------------------------------------------------------------------------
    bool BuzzBy::IsPositionInNoBuzzZone(Vector3 lPosition) const
    {
        for (s32 liZoneIndex = 0; liZoneIndex < KI_NUM_NO_BUZZ_ZONES; ++liZoneIndex)
        {
            const f32 lfDistanceSq =
                rw::math::vpu::MagnitudeSquared(lPosition - KA_NO_BUZZ_ZONE_CENTRES[liZoneIndex]);
            const f32 lfRadius = KAF_NO_BUZZ_ZONE_RADII[liZoneIndex];
            if (lfDistanceSq < lfRadius * lfRadius)
                return true;
        }
        return false;
    }

    // ------------------------------------------------------------------------
    // ResetActiveList @0x82771C90
    // Rebuild the buzz-by active list from the global race-car array.
    // ------------------------------------------------------------------------
    void BuzzBy::ResetActiveList()
    {
        miNumActiveCars = 0;

        EGlobalRaceCarIndex leGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        for (s32 liCar = 0; liCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            const AICar* lpAICar = reinterpret_cast<const AICar*>(
                reinterpret_cast<const u8*>(mpGlobalRaceCars) + liCar * 0x1560);

            // Active == IN_RANGE (0) or OUT_OF_RANGE (1); and not the player car.
            const EAICarState leState = lpAICar->GetState();
            const bool lbActive = (leState == E_AI_CAR_STATE_IN_RANGE ||
                                   leState == E_AI_CAR_STATE_OUT_OF_RANGE);
            if (lbActive && !lpAICar->IsPlayerCar())
            {
                CGS_ASSERT(miNumActiveCars < 35, "Too many AI cars in buzz-by active list\n");

                maeActiveList[miNumActiveCars] = leGlobalRaceCarIndex;
                mafBuzzTimes[miNumActiveCars]  = GetBuzzFrequency(lpAICar);
                ++miNumActiveCars;
            }

            leGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(leGlobalRaceCarIndex + 1);
            CGS_ASSERT(leGlobalRaceCarIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT");
        }
    }

    // ------------------------------------------------------------------------
    // StartABuzzBy @0x8278B858
    // Teleport (buzz) leGlobalRaceCarToTeleport past the player.
    // ------------------------------------------------------------------------
    void BuzzBy::StartABuzzBy(const AICar* lpPlayerCar, EGlobalRaceCarIndex leGlobalRaceCarToTeleport)
    {
        CGS_ASSERT(leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID,
                   "leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID");

        const f32 lfPlayerSpeed = lpPlayerCar->GetSpeed();

        AIModuleIO::ResetOnTrackRequest lRequest;
        ChooseAheadOrBehind(&lRequest, lfPlayerSpeed, leGlobalRaceCarToTeleport);

        // X360 `lwz r3, 0x128(this)` (mpResetOnTrackManager) then
        // ResetOnTrackManager::PushResetOnTrackRequest(mgr, &request) @0x82769E88.
        mpResetOnTrackManager->PushResetOnTrackRequest(&lRequest);
    }

    // ------------------------------------------------------------------------
    // Update @0x8278B8C8
    // Per-frame buzz-by tick.
    // ------------------------------------------------------------------------
    void BuzzBy::Update(f32 lfTimeStep, AICar* lpPlayerCar, AICar* lpBuzzCar, bool* lpbBuzzOccured)
    {
        *lpbBuzzOccured = false;

        // While in-game / in a junkyard, freeze free-roam buzzing: reset the timer and re-arm.
        if (mbIsInGameMode || mbIsInJunkyard)
        {
            mfTimeInFreeRoam  = 0.0f;
            mbResetBuzzTimers = true;
            return;
        }

        if (mbResetBuzzTimers)
        {
            mfTimeInFreeRoam = 0.0f;
            ResetActiveList();
            mbResetBuzzTimers = false;
        }

        *lpbBuzzOccured = BuzzOccured(lpPlayerCar, lpBuzzCar);

        if (!IsPlayerBuzzable(lpPlayerCar))
            return;

        mfTimeInFreeRoam += lfTimeStep;

        EGlobalRaceCarIndex leCarToTeleport = E_GLOBAL_RACE_CAR_INDEX_COUNT;   // 35 sentinel
        const AICar*        lpChosenCar     = nullptr;
        s32                 liChosenSlot    = -1;
        bool                lbFound         = false;

        for (s32 liEntry = 0; liEntry < miNumActiveCars; ++liEntry)
        {
            // Timer elapsed (buzz overdue) for this entry?
            if (mfTimeInFreeRoam > mafBuzzTimes[liEntry] &&
                (mfTimeInFreeRoam - mafBuzzTimes[liEntry]) > 0.0f)
            {
                const EGlobalRaceCarIndex leIndex = maeActiveList[liEntry];
                const AICar* lpAICar = reinterpret_cast<const AICar*>(
                    reinterpret_cast<const u8*>(mpGlobalRaceCars) + leIndex * 0x1560);

                const EAICarState leState = lpAICar->GetState();
                const bool lbActive = (leState == E_AI_CAR_STATE_IN_RANGE ||
                                       leState == E_AI_CAR_STATE_OUT_OF_RANGE);
                if (lbActive && AICarCanBuzz(lpAICar))
                {
                    leCarToTeleport = leIndex;
                    lpChosenCar     = lpAICar;
                    liChosenSlot    = liEntry;
                    lbFound         = true;
                }
            }
        }

        if (lbFound)
        {
            StartABuzzBy(lpPlayerCar, leCarToTeleport);

            // Re-seed the chosen entry's buzz timer from the buzzed car's frequency.
            mafBuzzTimes[liChosenSlot] = mfTimeInFreeRoam + GetBuzzFrequency(lpChosenCar);
        }
    }
    // GetBuzzFrequency (DWARF BrnAIBuzzBy.h:88) -- no export; inlined in ResetActiveList @0x82771C90
    // (0x82771D30..: `lvlx unk_820C4324 / lvlx unk_820C4320 / vspltw x2 / lfs 0x1510(car) / vsubfp / vmaddfp`)
    // == Lerp(10.0f, 180.0f, mfBuzzFrequencyRatio); both floats read from image.bin. Conductor, 2026-09-03.
    f32 BuzzBy::GetBuzzFrequency(const AICar* lpAICar) const
    {
        const f32 KF_BUZZ_FREQUENCY_MIN = 10.0f;    // flt_820C4320
        const f32 KF_BUZZ_FREQUENCY_MAX = 180.0f;   // flt_820C4324
        return KF_BUZZ_FREQUENCY_MIN + (KF_BUZZ_FREQUENCY_MAX - KF_BUZZ_FREQUENCY_MIN) * lpAICar->GetBuzzFrequencyRatio();
    }


    // Prepare (DWARF BrnAIBuzzBy.h:60) -- no X360 export; AIModule::Prepare @0x82798070 stage 4 inlines
    // the WHOLE body off r10 = module + 0x50000 - 0x1424 == +0x4EBDC (mBuzzBy), and the PS3 DecFIGS
    // keeps it standalone (0x9BAD3C, same field set, ending in ClearCarsAwaitingCollection):
    //   0x827982EC  stfs 0.0 -> +0x000   mfTimeInFreeRoam
    //   0x827982F0  stb  0   -> +0x004   mbIsInGameMode
    //   0x827982F4  stb  0   -> +0x005   mbIsInJunkyard
    //   0x827982FC  stw  0   -> +0x124   miNumActiveCars
    //   0x82798304..0x82798370           mRandom.Construct() on the file-static at 0x8300D5A0,
    //                                    constant-folded: ring 3F800000 3FE43E6C 3F98B09C 3FDA23E0
    //                                    3FE21EDC 3FDDEB96 3F9C9A72 3F923D76, seed 0xB5E330D0_2EC654DA
    //                                    (insrdi 0x82798358), index 0
    //   0x82798374  stw  maAICars -> +0x008    0x82798378  stw ROTM -> +0x128
    //   0x8279837C  stb  1   -> +0x006   mbResetBuzzTimers
    //   0x82798380  stw  0   -> +0x12C   miCarsAwaitingCollection
    // findinit 0x8300D5A0 finds exactly two sites -- this writer (0x827982D8) and the reader
    // ChooseAheadOrBehind @0x827718D0 -- so no CRT thunk seeds the Random.
    // ⛔ CORRECTED 2026-09-22 (crash parity G05-D1): only the two pointer stores were here. The
    // file-static Random was never Constructed, so its first 8 draws were 0.0 - 1.0 and every early
    // buzz-by took the BEHIND arm; mbResetBuzzTimers was left 0.
    void BuzzBy::Prepare(AICar* lpGlobalRaceCars, ResetOnTrackManager* lpResetOnTrackManager)
    {
        mfTimeInFreeRoam      = 0.0f;
        mbIsInGameMode        = false;
        mbIsInJunkyard        = false;
        miNumActiveCars       = 0;
        mRandom.Construct();
        mpGlobalRaceCars      = lpGlobalRaceCars;
        mpResetOnTrackManager = lpResetOnTrackManager;
        mbResetBuzzTimers     = true;
        ClearCarsAwaitingCollection();
    }

}
