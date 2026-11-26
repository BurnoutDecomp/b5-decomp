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

#define CGS_ARTIST_UNITY_BRNAIBUZZBY_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAIBuzzBy.cpp"
#define CGS_ARTIST_BURNOUTCONSTANTS_H_FILE "..\\..\\..\\GameSource\\BurnoutConstants.h"
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameShared/GameClasses/Core/CgsCoreConstants.h"   // CgsCore::KF_MPH_TO_MPS (the mph tunables)
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::Dot / MagnitudeSquared / Magnitude / Normalize / operator-
#include <cfloat>                             // FLT_EPSILON (the __real 0x820C3B70 MaintainAheadOrBehind loads)
#include <cmath>                             // std::fabs (MaintainAheadOrBehind's console IsZero)
#include "GameShared/GameClasses/Development/CgsStrStream.h"                          // CgsDev::SimpleStrStream (DrawBuzzTimer)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"     // DebugRender::Draw2DText

namespace BrnAI
{
    // --- file-scope tuning constants: DWARF BrnAIBuzzBy.cpp:26..:46 (names, order; PS3 values in
    //     references/DecFIGS/dwarfdump/_compile/BrnWorldAIUnity.cpp, X360 values read from the image).
    // The mph ones are dynamically initialised (`mph * CgsCore::KF_MPH_TO_MPS`, CRT thunks
    // 0x82C68EC8 / 0x82C68EE8 / 0x82C68F08 in declaration order, into .bss, read from storage; PS3
    // ChooseAheadOrBehind 0x9CE24C / MaintainAheadOrBehind 0x9CE620 name the two reset speeds).
    // The rest are this member's .rdata block, in definition order 0x820C4314..0x820C4330 (with
    // the no-buzz radii after it, :68). GCC folded those on PS3, so its bodies show bare 200.0 /
    // -60.0 literals; on the console a read of a value goes to the TU's first-registered const
    // of that value: MaintainAheadOrBehind (the earliest reader) reads KF_START_FAR_AHEAD, so every
    // 200 in the unity is read from 0x820C4318, AICarCanBuzz's KF_TOO_CLOSE_TO_BUZZ included.
    // :27/:28 (the post-event / junk-yard buzz times), :34, :40, :42 and :48 are unreferenced on the
    // console (no slot) and left out.
    const f32 KF_TOO_CLOSE_TO_BUZZ        = 200.0f;                            // :26 rdata 0x820C4314
    const f32 KF_FASTER_THAN_PLAYER       = 25.0f * CgsCore::KF_MPH_TO_MPS;    // :29 bss 0x8300D7F4
    const f32 KF_ON_COMING_RESET_SPEED    = 80.0f * CgsCore::KF_MPH_TO_MPS;    // :30 bss 0x8300DBEC
    const f32 KF_START_FAR_AHEAD          = 200.0f;                            // :31 rdata 0x820C4318
    const f32 KF_START_FAR_BEHIND         = -60.0f;                            // :32 rdata 0x820C431C
    const f32 KF_MIN_SPEED_FOR_BUZZING    = 7.5f * CgsCore::KF_MPH_TO_MPS;     // :36 bss 0x8300D938
    const f32 KF_RAPID_BUZZ_TIME          = 10.0f;                             // :38 rdata 0x820C4320
    const f32 KF_SLOW_BUZZ_TIME           = 180.0f;                            // :39 rdata 0x820C4324
    const f32 KF_CLOSE_ENOUGH_TO_SEE_BUZZ = 30.0f;                             // :44 rdata 0x820C4328
    const f32 KF_START_BEHIND_PROBABLITY  = 0.5f;                              // :45 rdata 0x820C432C
    // ChooseAheadOrBehind @0x8277197C does `fcmpu cr6, roll, flt_820C4330 ; bge` -> ON_COMING, so
    // this is a 20%/80% split between the head-on and from-turnings reset types.
    const f32 KF_SIDE_TURNING_PROBABILITY = 0.8f;                              // :46 rdata 0x820C4330

    // DWARF BrnAIBuzzBy.h:115 lists BuzzBy::mRandom as a static data member (`extern` in the dump,
    // the member DIE's external declaration); its one object is .bss 0x8300D5A0.
    CgsNumeric::Random BuzzBy::mRandom;

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
        Vector3(-2430.80005f, 60.0f, 1906.59998f),
        Vector3(-2604.80005f, 82.0f, 1900.59998f),
        Vector3(-2229.80005f, 19.5f, 625.400024f),
        Vector3(-2205.80005f, 19.5f, 808.400024f),
        Vector3(-1981.80005f, 96.5f, 698.400024f),
        Vector3(-1059.69995f, 105.199997f, -1509.30005f),
        Vector3(-1054.69995f, 105.199997f, -1270.30005f),
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
        CGS_ASSERT_AT(lpCar != NULL, "lpCar != NULL", __FILE__, 467);

        // 0x82767020: lfs f13,0x1508(lpCar); lfs f0,flt_820C4318(=200.0, the pooled 200); blt -> 0 else 1.
        // NaN polarity (FX-AINAN2): `li r3,0 ; fcmpu ; blt 0x82767070 ; li r3,1` -- blt is not
        // taken on an unordered compare, so a NaN distance CAN buzz; `>= 200` said no.
        if (lpCar->mfBuzzDistanceToPlayer < KF_TOO_CLOSE_TO_BUZZ)
        {
            return false;
        }
        return true;
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

        // flt_820C3FA8 = 30.0f (the TU's pooled 30): too far from the player for the buzz to register.
        if (lpAICar->mfBuzzDistanceToPlayer > KF_CLOSE_ENOUGH_TO_SEE_BUZZ)
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
        return !(std::fabs(static_cast<float>(lrVector.GetX())) > FLT_EPSILON) &&
               !(std::fabs(static_cast<float>(lrVector.GetY())) > FLT_EPSILON) &&
               !(std::fabs(static_cast<float>(lrVector.GetZ())) > FLT_EPSILON);
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
            lfResetDistance = KF_START_FAR_AHEAD;                               // flt_820C4318
        }
        else
        {
            // Behind the player: bring it up from behind, faster than the player.
            luResetFlags    = E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE;             // li r9, 3
            lfResetSpeed    = rw::math::vpu::Magnitude(lPlayerVelocity) + KF_FASTER_THAN_PLAYER;
            lfResetDistance = KF_START_FAR_BEHIND;                              // flt_820C431C
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
                                 KF_START_FAR_BEHIND,
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
                                 KF_START_FAR_AHEAD,
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
                CGS_ASSERT_AT(miNumActiveCars < 35, "Too many AI cars in buzz-by active list\n", CGS_ARTIST_UNITY_BRNAIBUZZBY_CPP_FILE, 631);

                maeActiveList[miNumActiveCars] = leGlobalRaceCarIndex;
                mafBuzzTimes[miNumActiveCars]  = GetBuzzFrequency(lpAICar);
                ++miNumActiveCars;
            }

            leGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(leGlobalRaceCarIndex + 1);
            CGS_ASSERT_AT(leGlobalRaceCarIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT", CGS_ARTIST_BURNOUTCONSTANTS_H_FILE, 84);
        }
    }

    // ------------------------------------------------------------------------
    // StartABuzzBy @0x8278B858
    // Teleport (buzz) leGlobalRaceCarToTeleport past the player.
    // ------------------------------------------------------------------------
    void BuzzBy::StartABuzzBy(const AICar* lpPlayerCar, EGlobalRaceCarIndex leGlobalRaceCarToTeleport)
    {
        // DWARF: the only local is lRequest (cpp:279); the speed goes straight into the call, and
        // the assert sits on cpp:281 (li r5 at 0x8278B880).
        AIModuleIO::ResetOnTrackRequest lRequest;

        CGS_ASSERT_AT(leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID,
                   "leGlobalRaceCarToTeleport != E_GLOBAL_RACE_CAR_INDEX_INVALID", __FILE__, 281);

        ChooseAheadOrBehind(&lRequest, lpPlayerCar->GetSpeed(), leGlobalRaceCarToTeleport);

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
        const AICar*        lpChosenCar     = 0;
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
        // PS3 0x9B6DF8 names both ends; the console loads them as VecFloats (lvlx 0x820C4320/4324).
        return KF_RAPID_BUZZ_TIME + (KF_SLOW_BUZZ_TIME - KF_RAPID_BUZZ_TIME) * lpAICar->GetBuzzFrequencyRatio();
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

    // ------------------------------------------------------------------------
    // DrawBuzzTimer
    // While free roaming: the cars-awaiting-collection count with the whole seconds of free-roam
    // time on one line and the active buzz-list size under it; in a game mode or the junkyard just
    // a note saying so.
    // ------------------------------------------------------------------------
    void BuzzBy::DrawBuzzTimer()
    {
        static const f32  KF_X          = 80.0f;
        static const f32  KF_TEXT_SCALE = 16.0f;
        static const u32  KU_COLOUR     = 0xFF8080C8u;
        static const u32  KU_LIST_COLOUR = 0xFFC88080u;

        CgsDev::SimpleStrStream lStream;
        CgsDev::DebugInterface  lDebugInterface;
        lStream.Reset();

        if (mbIsInGameMode || mbIsInJunkyard)
        {
            lStream << "In game mode or JYard";
            lDebugInterface.GetRender().Draw2DText(lStream.GetBuffer(), KF_X, 80.0f, KF_TEXT_SCALE, KU_COLOUR);
        }
        else
        {
            lStream << miCarsAwaitingCollection << " for collection, ";
            lStream << "Free burn time " << static_cast<s32>(mfTimeInFreeRoam);
            lDebugInterface.GetRender().Draw2DText(lStream.GetBuffer(), KF_X, 80.0f, KF_TEXT_SCALE, KU_COLOUR);

            lStream.Reset();
            lStream << "Num cars in buzz list " << miNumActiveCars;
            lDebugInterface.GetRender().Draw2DText(lStream.GetBuffer(), KF_X, 100.0f, KF_TEXT_SCALE, KU_LIST_COLOUR);
        }
    }

}
