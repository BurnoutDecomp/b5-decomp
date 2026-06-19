// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring_UpdatePass.cpp
// ============================================================================
// Out-of-line method BODIES for the StuntModeScoring per-frame "update pass" group.
//
//   LANDED (compiling bodies -- reconstructed store-for-store from each method's X360 dossier):
//     Update              (X360 0x82340B40)
//     UpdateStunts        (X360 0x82338908)
//     UpdateScore         (X360 0x823212D8)   [now bodied: deps grew the +0x1B0 hit-counter
//                                              array maStuntTypeScoreCount[18] + mStuntTypeInfo[18]]
//     UpdateBufferedScore (X360 0x8232C118)   [now bodied: deps grew the +0x22BD flag
//                                              mbStuntInfoEventPending + homed TidyStuntScore +
//                                              MultiplierOutInfo; CalculateMultiplier called BY NAME]
//
//   BLOCKED (declare-only -- bodies intentionally NOT here; committed-home SIGNATURE mismatch the
//   asm proves but the additive-grow rule forbids me from retyping unilaterally; see the block
//   comment at the bottom for the precise blocker of each):
//     UpdateScores        (X360 0x82338A98)
//     PreWorldUpdate      (X360 0x823446F8)
//
// SHAPE = DecFIGS DWARF; BODY = X360 pseudocode/asm (overrides DWARF on conflict). Members are
// accessed BY NAME against the committed StuntModeScoring layout (BrnStuntModeScoring.h) and the
// committed output interface (the active-race-car interface accessors) -- no raw offset casts.
//
// OFFSET -> NAMED-MEMBER MAP (the Construct/ClearData/UpdateBufferedScore asm pin the real byte
// offsets; the committed home's declared member ORDER is authoritative for naming):
//   this+0x10 miCurrentScore   this+0x14 miTargetScore
//   this+0x18 mfPendingNonGuaranteedScore   this+0x1C mfPendingGuaranteedScore
//   this+0x20 mfComboScore     this+0x24 miComboMultiplier
//   this+0x28 mbStuntModeActive  this+0x29 mbStuntInProgress  this+0x2A mbComboInProgress
//   this+0x2B mbValidStunt       this+0x2C mbWasInAirLastFrame this+0x2D mbInitialDriftOngoing
//   this+0x2E mbTimeLimitExpired this+0x2F mbTimeUpMessageSent this+0x30 mbPlayerCarCrashing
//   this+0x40 mStuntRollInProgress  this+0x50 muStuntTypesInProgress  this+0x54 muAwesomeStuntTypesInProgress
//   this+0x58 mfTimeSinceLastStunt  this+0x5C mfSpeedMPHBeforeCrashing  this+0x60 mfTimeDelayBeforeModeEnd
//   this+0x64 mbRecentStunt   this+0x68 mRecentStunt (StuntInfo: muStuntTypes@+0,
//             muAwesomeStuntTypes@+4, miStuntScore@+8, miStuntMultiplier@+0xC, muFlatSpins@+0x10,
//             muBarrelRolls@+0x12)  => +0x68/0x6C/0x70/0x74/0x78/0x7A
//   this+0x8C mfPendingScoreTimer
//   this+0x90 mStuntTypeInfo[0] (16-byte stride; mfTimeActive +0, mfTimeSinceLast +4, mfScore +8,
//             mbActive +0xC -> +0x9C for [0])
//   this+0x1B0 maStuntTypeScoreCount[0] (4-byte stride; per-stunt-type hit counter)
//   this+0x22BD mbStuntInfoEventPending

#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"
#include "GameSource/GameState/BrnGameActions.h"   // GameStateModuleIO action types (group include)

// Completes the output-interface typedef (StuntModeScoring::ActiveRaceCarOutputInterface ==
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface): UpdateStunts dereferences
// it BY NAME (GetPlayerActiveRaceCarIndex / IsPlayerCarActive / IsPlayerCarCrashing /
// GetRaceCarState->mb...). This transitively pulls the complete RaceCarState (BrnVehicleEvents.h)
// so the per-car physics flags are named.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

// rw::math::vpu Vector3 lane vocabulary (GetY/GetZ + the SetZero member): UpdateBufferedScore reads
// the spin (Y, VMX lane 1) and barrel-roll (Z, VMX lane 2) lanes of mStuntRollInProgress and zeroes
// the whole accumulator on bank. Matches the sibling Update* bodies (BrnStuntModeScoring_StuntTypes.cpp).
#include "rw/math/vpu/vector3_operation.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // X360-tunable score constants (file-scope; the original keeps them in .rodata as the
    // flt_82CDBxxx / flt_8200xxxx pool). The MAGNITUDES are UNRECOVERED in the IDA export
    // (the float pool is not in the dumped data segments) -- modelled as named 0.0f best-effort
    // per the established sibling-draft pattern (cf. BrnStuntModeScoring_StuntTypes.cpp's
    // KF_STUNT_ATTACK_* block). The X360 .rodata address is in each comment so the value can be
    // patched in when the float pool lands. FLAG: confirm every magnitude against the float pool.
    namespace
    {
        const f32 KF_ZERO                                  = 0.0f;  // flt_82001CC0 (the fsel/compare 0.0 sentinel)
        const f32 KF_ONE                                   = 1.0f;  // flt_82001C98 (the [0,1] clamp ceiling)

        // UpdateScore repetition-falloff scale (applied to score for stunt types in mask 0x20308).
        const f32 KF_SCORE_FALLOFF_RATE_DEFAULT            = 0.0f;  // flt_82CDB794 : falloff rate (types != 8)
        const f32 KF_SCORE_FALLOFF_RATE_TYPE8              = 0.0f;  // flt_82CDB798 : falloff rate (stunt type 8)
        const f32 KF_SCORE_FALLOFF_FLOOR                   = 0.0f;  // flt_82CDB79C : falloff scale floor (fsel min)

        // UpdateBufferedScore flat-spin / barrel-roll banking constants.
        const f32 KF_FLAT_SPIN_SCORE_PER_DEGREE            = 0.0f;  // flt_82CDB744 : per-spin-degree score rate
        const f32 KF_BARREL_ROLL_SCORE_PER_DEGREE          = 0.0f;  // flt_82CDB748 : per-roll-degree score rate
        const f32 KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER
                                                           = 0.0f;  // flt_82CDB780 : reverse-takeoff bonus mult
        const f32 KF_SPIN_COUNT_ROUNDING_BIAS              = 0.0f;  // flt_82001DA0 : floor-rounding bias (vmaddfp addend)
        const f32 KF_FLAT_SPIN_DEGREES_PER_SPIN            = 0.0055555557f; // flt_8202616C == 1/180 : spins = degrees/180
        const f32 KF_BARREL_ROLL_DEGREES_PER_ROLL          = 0.5f;          // flt_82004920 == 1/2   : rolls = (half-revs)/2

        // UpdateBufferedScore pending-score-timer constants.
        const f32 KF_PENDING_SCORE_TIMER_NOT_STARTED       = 0.0f;  // flt_82CDB784 : "no pending timer" sentinel
        const f32 KF_PENDING_SCORE_PENDING_TIME            = 0.0f;  // flt_82CDB788 : armed pending-score window
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::Update  (X360 0x82340B40)
    // ------------------------------------------------------------------------
    // The mode's per-frame entry point. Asserts the mode is active, clears every per-category
    // "scored this frame" flag (mStuntTypeInfo[i].mbActive) for the frame, runs the two sub-passes
    // (UpdateStunts re-rates the categories, UpdateScores banks/combos), then ticks the end-of-mode
    // delay timer -- force-zeroing it the instant the player car is crashing.
    //
    // X360 (0x82340B40): the flag-clear loop runs r11=18 iterations at stride 0x10 starting at
    // this+0x9C (== mStuntTypeInfo[0].mbActive), clearing mbActive for indices 0..17. The committed
    // home grew mStuntTypeInfo to [18] (per these asm bounds), so the literal 18 is faithful.
    //
    // NOTE on UpdateScores: the X360 call site is UpdateScores(this, a2=interface, a7=delta) -- it
    // passes the output interface. The committed (best-effort, ledger-only) home declares
    // UpdateScores(f32) with NO interface param, and UpdateScores' body needs the interface for its
    // player-active gate (it cannot be reconstructed against the committed sig -- see the BLOCKED
    // block below). UpdateScores is therefore declare-only; Update calls it with the committed
    // 1-arg signature (compiles; the interface gate is handled inside UpdateScores once its sig is
    // corrected). The asm-faithful interface argument is dropped here only because the committed
    // declaration cannot carry it.
    void StuntModeScoring::Update(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta)
    {
        CGS_ASSERT(mbStuntModeActive, "mbStuntModeActive");

        // Clear the per-category "scored this frame" flag for every stunt category (asm: 18 wide).
        for (s32 liType = 0; liType < 18; ++liType)
        {
            mStuntTypeInfo[liType].mbActive = false;
        }

        UpdateStunts(lfDelta, lpRaceCar);
        UpdateScores(lfDelta);

        // Tick down the end-of-mode delay; a crashing player car collapses it to zero immediately.
        if (mfTimeDelayBeforeModeEnd > KF_ZERO)
        {
            if (mbPlayerCarCrashing)
            {
                mfTimeDelayBeforeModeEnd = KF_ZERO;
            }
            else
            {
                mfTimeDelayBeforeModeEnd -= lfDelta;
            }
        }
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateStunts  (X360 0x82338908)
    // ------------------------------------------------------------------------
    // The stunt-detection sub-pass. Only runs while the player car is active. If the player car is
    // in a "disturbed" state this frame (mbPlayerCarCrashing was set last frame, or the car just
    // reset / was just slammed / is crashing), it abandons the in-progress stunt (clears mbValidStunt
    // and resets the per-category rating state); otherwise it runs the four category detectors
    // (air / drift / boost / driving) and folds their "valid" results into mbStuntInProgress.
    //
    // X360 (0x82338908):
    //   * v6 = *(a2+10328)  -> GetPlayerActiveRaceCarIndex()  (shared <8 count assert)
    //   * v8 = *(a2+10336)  -> IsPlayerCarActive()            (only read when index != -1)
    //   * v9  = *(1120*v6 + a2 + 1914) -> GetRaceCarState(v6)->mbResetCarTransform (@1098)
    //   * *(a2+10464)                  -> IsPlayerCarCrashing() (player-level bool ORed in)
    //   * v10 = *(1120*v6 + a2 + 1915) -> GetRaceCarState(v6)->mbJustBeenSlammed   (@1099)
    //   * disturbed = mbPlayerCarCrashing(last frame, *(v5+48)) || (v9 || *(a2+10464) || v10).
    //   * disturbed branch: *(v5+43)=0 (mbValidStunt=false) then the vtable-slot-+0x20 reset call.
    //   * else branch: v7 = UpdateAirStunts | UpdateDriftStunts | UpdateBoostStunts | UpdateDrivingStunts.
    //   * tail: *(v5+48) = disturbed (mbPlayerCarCrashing); *(v5+41) = mbStuntInProgress & v7.
    //
    // FLAGs (LEDGER-ONLY method -- no DWARF signature; best-effort per the committed home):
    //   (1) *(a2+10464) modelled as IsPlayerCarCrashing() from the crash/disturbance semantics; the
    //       exact interface field at +10464 was not offset-walked. Re-confirm vs the asm.
    //   (2) the vtable slot +0x20 reset is modelled as ClearStuntTypeInfo() (the per-category rating
    //       reset -- the semantic match). The StuntModeScoring vtable ORDER is not reconstructed in
    //       the committed home, so the exact slot-+0x20 method identity is provisional.
    void StuntModeScoring::UpdateStunts(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar)
    {
        const EActiveRaceCarIndex lePlayerIndex = lpRaceCar->GetPlayerActiveRaceCarIndex();
        CGS_ASSERT(lePlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        bool lbPlayerCarActive = false;
        if (lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            lbPlayerCarActive = lpRaceCar->IsPlayerCarActive();
        }

        if (!lbPlayerCarActive)
        {
            return;
        }

        // Per-car physics flags for the player's race-car state.
        bool lbResetCarTransform = false;
        bool lbJustBeenSlammed   = false;
        if (lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            const BrnPhysics::Vehicle::RaceCarState* lpState = lpRaceCar->GetRaceCarState(lePlayerIndex);
            lbResetCarTransform = lpState->mbResetCarTransform;   // @1098
            lbJustBeenSlammed   = lpState->mbJustBeenSlammed;     // @1099
        }

        // FLAG(1): provisional accessor for the player-level *(a2+10464) bool.
        const bool lbPlayerDisturbedNow =
            lbResetCarTransform || lpRaceCar->IsPlayerCarCrashing() || lbJustBeenSlammed;

        bool lbAnyStuntValid = false;

        if (mbPlayerCarCrashing || lbPlayerDisturbedNow)
        {
            // Disturbed: drop the in-progress stunt and reset the per-category rating state.
            mbValidStunt = false;
            ClearStuntTypeInfo();   // FLAG(2): vtable slot +0x20 reset, modelled by name.
        }
        else
        {
            // Run the four category detectors; OR their per-frame "valid stunt" results together.
            const bool lbAir     = UpdateAirStunts(lfDelta, lpRaceCar);
            const bool lbDrift   = UpdateDriftStunts(lfDelta, lpRaceCar);
            const bool lbBoost   = UpdateBoostStunts(lfDelta, lpRaceCar);
            const bool lbDriving = UpdateDrivingStunts(lfDelta, lpRaceCar);
            lbAnyStuntValid = lbAir || lbDrift || lbBoost || lbDriving;
        }

        // Carry the disturbance state into mbPlayerCarCrashing for next frame, and keep the stunt
        // in-progress flag only while it was already in progress AND something valid happened.
        mbPlayerCarCrashing = lbPlayerDisturbedNow;
        mbStuntInProgress   = mbStuntInProgress && lbAnyStuntValid;
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateScore  (X360 0x823212D8)
    // ------------------------------------------------------------------------
    // Banks an in-progress stunt score into one stunt category. Asserts the category index is in
    // range and that a stunt is in progress (or pending) and a combo is active. For the
    // "repetition-falloff" categories (mask 0x20308 == stunt types 3, 8, 9, 17) it first scales the
    // award down by a per-category falloff (1 - mfTimeActive*rate, floored), then folds the scaled
    // award into the category running score, bumps the per-category hit counter, sets the
    // in-progress-types bit, and adds the award into either the guaranteed or non-guaranteed pending
    // pot (clamped >= 0) depending on lbAwesome.
    //
    // X360 (0x823212D8): callee register map (the committed 3-param C++ sig drops the X360's unused
    // r4 middle arg -- the body never reads it, so this is faithful):
    //   r3=this   f1=lfScore (f29)   r5=leStuntType (r30, asserted >-1 && <0x12)   r6=lbAwesome (r26)
    //   * mStuntTypeInfo[type].mfScore  @ this+0x90+16*type+8   <- += scaled score
    //   * mStuntTypeInfo[type].mbActive @ this+0x90+16*type+0xC <- 1
    //   * maStuntTypeScoreCount[type]   @ this+0x1B0+4*type     <- ++ (deps-grown array)
    //   * muStuntTypesInProgress        @ this+0x50             |= (1<<type)
    //   * mfPendingGuaranteedScore (+0x1C) or mfPendingNonGuaranteedScore (+0x18) += score, fsel >= 0.
    void StuntModeScoring::UpdateScore(f32 lfScore, EStuntType leStuntType, bool lbAwesome)
    {
        // Range bound is the X360 literal 18 (cmpwi r30, 0x12). The committed EStuntType enum spells
        // E_STUNT_TYPE_COUNT == 15, but the original build's count (and mStuntTypeInfo[18] /
        // maStuntTypeScoreCount[18]) is 18 -- the asm indexes up to type 17. FLAG: the committed
        // E_STUNT_TYPE_COUNT (15) disagrees with the asm's 18-wide range; the literal 18 is faithful.
        CGS_ASSERT(leStuntType > E_STUNT_TYPE_INVALID && static_cast<s32>(leStuntType) < 18,
                   "leStuntType > E_STUNT_TYPE_INVALID && leStuntType < E_STUNT_TYPE_COUNT");

        if (!mbStuntInProgress)
        {
            CGS_ASSERT(HasAnyPendingScore() && mfPendingScoreTimer <= KF_ZERO,
                       "mbStuntInProgress || (HasAnyPendingScore() && mfPendingScoreTimer<=0.0f)");
        }
        CGS_ASSERT(mbComboInProgress, "mbComboInProgress");

        const s32 liType = static_cast<s32>(leStuntType);

        // Repetition-falloff: stunt types in mask 0x20308 (bits 3, 8, 9, 17) scale the award down by
        // a per-category falloff of (1 - mfTimeActive*rate), floored at KF_SCORE_FALLOFF_FLOOR.
        if (((1 << liType) & 0x20308) != 0)
        {
            const f32 lfRate = (liType == 8) ? KF_SCORE_FALLOFF_RATE_TYPE8
                                             : KF_SCORE_FALLOFF_RATE_DEFAULT;
            // f0 = 1.0 - mfTimeActive*rate  (fnmsubs f13,f0,f12); fsel floors it at the falloff floor.
            const f32 lfRaw = KF_ONE - (mStuntTypeInfo[liType].mfTimeActive * lfRate);
            const f32 lfScoreScale =
                ((lfRaw - KF_SCORE_FALLOFF_FLOOR) >= KF_ZERO) ? lfRaw : KF_SCORE_FALLOFF_FLOOR;
            CGS_ASSERT(lfScoreScale >= KF_ZERO && lfScoreScale <= KF_ONE,
                       "lfScoreScale >= 0.0f && lfScoreScale <= 1.0f");
            lfScore = lfScoreScale * lfScore;
        }

        // Fold the (scaled) award into the category running score + mark the category active.
        mStuntTypeInfo[liType].mfScore  += lfScore;
        mStuntTypeInfo[liType].mbActive  = true;

        // Bump the per-stunt-type hit counter (deps-grown maStuntTypeScoreCount[18] @ this+0x1B0).
        ++maStuntTypeScoreCount[liType];

        // Record the category in the in-progress-types bitmask.
        muStuntTypesInProgress |= (1u << liType);

        // Add the award into the guaranteed / non-guaranteed pending pot (fsel: clamp >= 0).
        if (lbAwesome)
        {
            const f32 lfSum = mfPendingGuaranteedScore + lfScore;
            mfPendingGuaranteedScore = (lfSum >= KF_ZERO) ? lfSum : KF_ZERO;
        }
        else
        {
            const f32 lfSum = mfPendingNonGuaranteedScore + lfScore;
            mfPendingNonGuaranteedScore = (lfSum >= KF_ZERO) ? lfSum : KF_ZERO;
        }
    }

    // ------------------------------------------------------------------------
    // StuntModeScoring::UpdateBufferedScore  (X360 0x8232C118)
    // ------------------------------------------------------------------------
    // The score-banking heart of the per-frame pass. When there is pending score and the bank gate
    // (ShouldBankScore) opens, it counts down the pending-score timer; once the timer elapses it
    // commits the buffered combo into a "recent stunt" record (mRecentStunt), computes the combo
    // multiplier via the virtual CalculateMultiplier, banks the multiplied score into the current
    // score, arms the stunt-info HUD event (mbStuntInfoEventPending), and resets the buffer. When
    // there is no pending score and no stunt is in progress, it just zeroes the per-frame rating
    // state.
    //
    // X360 (0x8232C118): the SPIN / BARREL-ROLL committal uses VMX lane math over the Vector3
    // mStuntRollInProgress (this+0x40):
    //   * lane 1 (.y) -> flat-spin: score = lane1 * (reverseTakeoffMult or 1.0) * KF_FLAT_SPIN_SCORE_PER_DEGREE
    //                    when muStuntTypesInProgress bit 0 set; the awesome-flat-spin case
    //                    (muAwesomeStuntTypesInProgress bit 0) also counts flat spins:
    //                    muFlatSpins = floor(lane1 * KF_FLAT_SPIN_DEGREES_PER_SPIN + bias), asserted >= 1.
    //   * lane 2 (.z) -> barrel-roll, symmetric, stunt type 1, muBarrelRolls.
    // The reverse-takeoff multiplier is gated on muStuntTypesInProgress bit 14 ((v>>14)&1).
    //
    // The float MAGNITUDES (KF_FLAT_SPIN_*, KF_BARREL_ROLL_*, the spin/roll degree divisors and the
    // floor-rounding bias) are UNRECOVERED in the IDA export and modelled best-effort (see the
    // file-scope constant block). The CalculateMultiplier call dispatches through the X360 vtable
    // slot +0x24; here it is called BY NAME (it is declared virtual in the committed home, so the
    // C++ call binds polymorphically without the vtable ORDER being reconstructed).
    // FLAG: confirm all banking constant magnitudes + the exact VMX lane->degree mapping when the
    // float pool lands.
    void StuntModeScoring::UpdateBufferedScore(f32 lfDelta)
    {
        CGS_ASSERT(!mbRecentStunt, "!mbRecentStunt");

        if (HasAnyPendingScore())
        {
            if (ShouldBankScore())
            {
                // Count down the pending-score timer; arm it from the NOT_STARTED sentinel first.
                if (mfPendingScoreTimer >= KF_PENDING_SCORE_TIMER_NOT_STARTED)
                {
                    mfPendingScoreTimer = KF_PENDING_SCORE_PENDING_TIME;
                }
                const f32 lfTimerAfter = mfPendingScoreTimer - lfDelta;
                mfPendingScoreTimer = lfTimerAfter;

                if (lfTimerAfter <= KF_ZERO)
                {
                    // The pending window has elapsed: commit the buffered combo into mRecentStunt.
                    mRecentStunt.miStuntScore     = 0;
                    mRecentStunt.miStuntMultiplier = 0;
                    mRecentStunt.muFlatSpins      = 0;
                    mRecentStunt.muBarrelRolls    = 0;
                    mRecentStunt.muStuntTypes        = muStuntTypesInProgress;
                    mRecentStunt.muAwesomeStuntTypes = muAwesomeStuntTypesInProgress;

                    CGS_ASSERT(mRecentStunt.muStuntTypes != 0, "mRecentStunt.muStuntTypes != 0");

                    // The reverse-takeoff bonus multiplier is applied to spin/roll banking when the
                    // REVERSE_TAKEOFF bit (bit 14) of muStuntTypesInProgress is set.
                    const bool lbReverseTakeoff = ((muStuntTypesInProgress >> 14) & 1u) != 0;
                    const f32  lfTakeoffMult =
                        lbReverseTakeoff ? KF_STUNT_ATTACK_REVERSE_TAKEOFF_SCORE_MULTIPLIER : KF_ONE;

                    if (mbValidStunt)
                    {
                        // ---- Flat-spin (stunt type 0) ----
                        if ((mRecentStunt.muStuntTypes & 0x01u) != 0)
                        {
                            const f32 lfSpinDegrees = rw::math::vpu::GetY(mStuntRollInProgress);  // VMX lane 1
                            const f32 lfSpinScore   =
                                KF_FLAT_SPIN_SCORE_PER_DEGREE * (lfTakeoffMult * lfSpinDegrees);
                            UpdateScore(lfSpinScore, static_cast<EStuntType>(0), false);

                            if ((mRecentStunt.muAwesomeStuntTypes & 0x01u) != 0)
                            {
                                // muFlatSpins = floor(spinDegrees * (1/180) + bias) (vmaddfp+vrfim).
                                const f32 lfSpins =
                                    lfSpinDegrees * KF_FLAT_SPIN_DEGREES_PER_SPIN + KF_SPIN_COUNT_ROUNDING_BIAS;
                                mRecentStunt.muFlatSpins =
                                    static_cast<u16>(static_cast<s32>(lfSpins));
                                CGS_ASSERT(mRecentStunt.muFlatSpins >= 1, "mRecentStunt.muFlatSpins >= 1");
                            }
                        }

                        // ---- Barrel-roll (stunt type 1) ----
                        if ((mRecentStunt.muStuntTypes & 0x02u) != 0)
                        {
                            const f32 lfRollDegrees = rw::math::vpu::GetZ(mStuntRollInProgress);  // VMX lane 2
                            const f32 lfRollScore   =
                                (lfTakeoffMult * lfRollDegrees) * KF_BARREL_ROLL_SCORE_PER_DEGREE;
                            UpdateScore(lfRollScore, static_cast<EStuntType>(1), false);

                            if ((mRecentStunt.muAwesomeStuntTypes & 0x02u) != 0)
                            {
                                const f32 lfRolls =
                                    lfRollDegrees * KF_BARREL_ROLL_DEGREES_PER_ROLL + KF_SPIN_COUNT_ROUNDING_BIAS;
                                mRecentStunt.muBarrelRolls =
                                    static_cast<u16>(static_cast<s32>(lfRolls));
                                CGS_ASSERT(mRecentStunt.muBarrelRolls >= 1, "mRecentStunt.muBarrelRolls >= 1");
                            }
                        }

                        // Bank the combo: round the non-guaranteed pot down to a multiple of 100,
                        // store it as the recent-stunt score, then call BankMultiplier (X360 vtable
                        // slot +0x24, one StuntInfo* arg) which fills mRecentStunt.miStuntMultiplier
                        // (internally via CalculateMultiplier) and returns the multiplier; arm the
                        // HUD event. (The asm dispatches BankMultiplier virtually; the committed home
                        // declares it non-virtual -- a vtable-ORDER detail, not a call-correctness one.)
                        const s32 liBankedScore = 100 * (static_cast<s32>(mfPendingNonGuaranteedScore) / 100);
                        mRecentStunt.miStuntScore = liBankedScore;
                        BankMultiplier(&mRecentStunt);

                        mbRecentStunt = (mRecentStunt.miStuntScore > 0);
                        // Arm the stunt-info HUD event when there are any awesome stunt types.
                        mbStuntInfoEventPending = (mRecentStunt.muAwesomeStuntTypes != 0);

                        CGS_ASSERT(mbComboInProgress, "mbComboInProgress");

                        if (mbRecentStunt)
                        {
                            // Accumulate the banked score + multiplier into the running combo.
                            miComboMultiplier += mRecentStunt.miStuntMultiplier;
                            mfComboScore      += static_cast<f32>(mRecentStunt.miStuntScore);
                        }
                    }
                    else if ((mRecentStunt.muStuntTypes & 0xC0000u) != 0)
                    {
                        // Non-air banking path (stunt types 18/19 bits): bank with no multiplier.
                        mRecentStunt.miStuntScore     = 0;
                        mRecentStunt.miStuntMultiplier = 0;
                        mbRecentStunt = true;
                    }

                    // Fold the guaranteed pending pot into the combo score, tidy it, and reset the
                    // buffer (clear the in-progress/awesome type masks + pending pots + timer +
                    // the Vector3 roll accumulator at this+0x40).
                    mfComboScore += mfPendingGuaranteedScore;
                    mfComboScore  = TidyStuntScore(mfComboScore);

                    muStuntTypesInProgress        = 0;
                    muAwesomeStuntTypesInProgress = 0;
                    mfPendingGuaranteedScore      = KF_ZERO;
                    mfPendingNonGuaranteedScore   = KF_ZERO;
                    mfPendingScoreTimer           = KF_PENDING_SCORE_TIMER_NOT_STARTED;
                    mStuntRollInProgress.SetZero();   // stvx128 v0(zero), this+0x40
                }
            }
        }
        else if (!mbStuntInProgress)
        {
            // No pending score and no stunt in progress: zero the per-frame rating state.
            mStuntRollInProgress.SetZero();   // stvx128 v0(zero), this+0x40
            muStuntTypesInProgress        = 0;
            muAwesomeStuntTypesInProgress = 0;
            for (s32 liType = 0; liType < 18; ++liType)
            {
                mStuntTypeInfo[liType].mfScore = KF_ZERO;   // asm: 18-wide stride-16 zero of .mfScore (+8)
            }
        }
    }

    // ============================================================================
    // BLOCKED -- declare-only (intentionally NO body in this partial). Both are blocked on a
    // committed-home SIGNATURE that the X360 asm proves WRONG, but the additive-grow rule forbids me
    // from retyping a committed declaration unilaterally. Each needs its (best-effort, ledger-only)
    // declaration CORRECTED in BrnStuntModeScoring.h before its body can land faithfully. Neither
    // has a committed C++ caller (their X360 callers are not yet reconstructed), so correcting the
    // signature later cannot break an embedder.
    //
    //   * UpdateScores       (X360 0x82338A98)
    //       Committed sig:  void UpdateScores(f32 lfDelta)
    //       Asm-proven sig: UpdateScores(this, const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta)
    //       The body's ENTIRE work is gated behind a PLAYER-ACTIVE test that dereferences the output
    //       interface: it reads *(a2+10328) (GetPlayerActiveRaceCarIndex, asserted < 8) and, when the
    //       index is set, *(a2+10336) (IsPlayerCarActive); ONLY when the player car is active does it
    //       call UpdateCombo / UpdateBufferedScore / UpdateStuntRepetition. The committed sig has NO
    //       interface parameter, so a body written against it would DROP that gate (and would have to
    //       fabricate a NULL for UpdateCombo, which the asm proves takes only (this, delta)). Per the
    //       "do NOT ship a body missing a gate" rule, UpdateScores stays declare-only. FIX: GROW the
    //       home declaration to UpdateScores(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar)
    //       (and update Update's call site to pass lpRaceCar); then the gated body lands. The round-1
    //       draft's NULL-passing, gate-less body was incorrect and has been removed.
    //
    //   * PreWorldUpdate     (X360 0x823446F8)
    //       Committed sig:  void PreWorldUpdate(const ActiveRaceCarOutputInterface* lpRaceCar, f32 lfDelta)
    //       Asm-proven sig: PreWorldUpdate(this, CgsModule::VariableEventQueue<13312,16>* lpOutputActionQueue)
    //       The body reads the deps-grown mbStuntInfoEventPending flag (+0x22BD); when set it packs an
    //       8-byte payload { u32 mRecentStunt.muAwesomeStuntTypes; s16 mRecentStunt.muFlatSpins;
    //       s16 mRecentStunt.muBarrelRolls } and calls
    //       lpOutputActionQueue->AddEvent(&payload, /*type*/21, /*size*/8) (asserting the queue
    //       non-null, "lpOutputActionQueue"), then CLEARS the flag. The committed sig's first param is
    //       an ActiveRaceCarOutputInterface*, NOT the VariableEventQueue<13312,16>* the body needs, and
    //       it carries a spurious f32 the asm has no parameter for -- the queue cannot be threaded
    //       through. FIX: GROW the home declaration to
    //       PreWorldUpdate(CgsModule::VariableEventQueue<13312,16>* lpOutputActionQueue) (the deps
    //       already added the +0x22BD flag this body consumes); then the body lands. The mRecentStunt
    //       member-offset map (+0x6C muAwesomeStuntTypes, +0x78 muFlatSpins, +0x7A muBarrelRolls) is
    //       X360-confirmed by the UpdateBufferedScore asm above.
    // ============================================================================
}
