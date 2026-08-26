#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"

#include "GameSource/GameState/BrnGameActions.h"      // GameStateModuleIO action types (parity with sibling bodies)
#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT

// ============================================================================
// BrnStuntModeScoring_Combo.cpp
// ============================================================================
// Out-of-line method BODIES for the StuntModeScoring "Combo" group (home header
// BrnStuntModeScoring.h):
//   BeginCombo          (X360 0x82313428)  -- start a fresh combo
//   EndCombo            (X360 0x823215D8)  -- bank + tear the combo down
//   UpdateCombo         (X360 0x82320FF0)  -- per-frame combo-timer advance / auto-end
//   BankMultiplier      (X360 0x82312D68)  -- compute + stamp a recent stunt's multiplier
//   CalculateMultiplier (X360 0x82312DE8)  -- the multiplier formula (awesome popcount + spins/rolls)
//
// SHAPE = DecFIGS DWARF; BODY = X360 pseudocode/asm (work show <addr> --full --asm),
// reconstructed store-for-store and OVERRIDING the DWARF on any conflict. Members are
// reached BY NAME against the committed StuntModeScoring layout (BrnStuntModeScoring.h);
// no raw offset casts. The header documents the asm-proven byte offsets in its member
// comments -- the per-store offset annotations below cross-reference them.
//
// IsComboInProgress (X360 0x82313510) is bodied INLINE in the home header
// (BrnStuntModeScoring.h: `return mbComboInProgress;`); emitting an out-of-line body
// here would be an ODR redefinition, so it is intentionally NOT re-emitted.
//
// COMBO TIMERS (asm-proven member roles, see the header note at BrnStuntModeScoring.h):
//   mfTimeSinceLastStunt     (+0x58) -- "time since the last scoring stunt" timer. UpdateCombo
//                                       grows it once per combo-no-stunt frame; crossing
//                                       KF_TIME_WITHOUT_STUNT_TO_LOSE_COMBO ends the combo.
//                                       BeginCombo zeros it.
//   mfSpeedMPHBeforeCrashing (+0x5C) -- the per-frame combo-active timer UpdateCombo grows
//                                       unconditionally each combo frame; EndCombo snapshots it
//                                       into the recent-combo time (mfRecentComboTime, +0x88);
//                                       BeginCombo zeros it. (The DWARF source-level identifier
//                                       is "mfSpeedMPHBeforeCrashing"; the recovered asm proves
//                                       its runtime role is the combo timer -- a frame-delta
//                                       accumulator cannot be a "speed before crashing". We do
//                                       NOT rename/retype the committed member; we name it
//                                       verbatim and annotate the combo role here.)
//
// RESIDUAL FLAG -- EndCombo (0x823215D8) issues a virtual call on the member at this+0x22C0.
// The X360 Hex-Rays decodes it as
//   CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct(*(this + 0x22C0))
// but that is an ICF-folded mis-symbol: the PS3 DecFIGS (EndCombo 0x1D217C, demangled DWARF)
// shows the SAME call is actually
//   AchievementManager::OnStuntRunMultiplier(mpAchievementManager, miComboMultiplier).
// There is therefore NO 0x22C0 collision: the member at this+0x22C0 IS mpAchievementManager
// (Construct 0x8232C080 stores its arg there via a1[2224]), and the call is its
// OnStuntRunMultiplier(s32) (AchievementManagerBase.h:185, DWARF :148). The call is still left
// UN-EMITTED here only to avoid pulling the AchievementManager header (mpAchievementManager is
// forward-declared incomplete in the keystone) into this partial TU -- it is a clean drop-in
// (mpAchievementManager->OnStuntRunMultiplier(miComboMultiplier)) once the full StuntModeScoring
// TU includes that header. Every other EndCombo store maps to a committed, named member and IS
// emitted.
// ============================================================================

namespace BrnGameState
{
    namespace
    {
        // RwMathFPU::IsZero's +/- dead-band (X360 epsilon 1.1920929e-7, the flt_82002514 /
        // flt_82020B30 rodata pair). Matches the inline idiom the sibling
        // BrnStuntModeScoring_Lifecycle.cpp / _Queries.cpp use for the same RwMathFPU::IsZero
        // checks (RwMathFPU has no committed home; the comparison is reproduced inline).
        const f32 KF_SCORE_EPSILON = 1.1920929e-7f;

        inline bool RwMathFPU_IsZero(f32 lfValue)
        {
            return (lfValue <= KF_SCORE_EPSILON) && (lfValue >= -KF_SCORE_EPSILON);
        }

        // --- StuntModeScoring combo tuning constants (BrnStuntModeScoring.cpp anon-ns block) -----
        // Names are role-attested from the per-method semantics; the magnitudes are RECOVERED as
        // direct big-endian f32 reads of the named flt_ addresses out of the decrypted X360 ARTIST
        // basefile (image VA - 0x82000000 == file offset). Each address was cross-checked against
        // the referencing method's own asm export: UpdateCombo (0x82320FF0) references flt_82CDB790
        // and EndCombo (0x823215D8) references flt_82CDB7B0, and neither references the other's.
        // These stay file-local: when the full BrnStuntModeScoring.cpp is reconstructed it owns the
        // canonical anonymous-namespace block; delete these and let the shared constants resolve.

        // X360 flt_82CDB790 (VA 0x82CDB790) -- UpdateCombo's "seconds without a scoring stunt
        // before the combo is lost" threshold (mfTimeSinceLastStunt crossing it auto-ends the combo).
        const f32 KF_TIME_WITHOUT_STUNT_TO_LOSE_COMBO = 5.0f;

        // X360 flt_82CDB7B0 (VA 0x82CDB7B0) -- the delay EndCombo parks mfTimeDelayBeforeModeEnd at
        // when the combo ends with the time limit already expired (and the run is not endless).
        const f32 KF_TIME_DELAY_BEFORE_MODE_END = 3.0f;
    }

// ----------------------------------------------------------------------------
// BeginCombo  --  X360 0x82313428
// ----------------------------------------------------------------------------
// Start a fresh combo: zero the combo score and both combo timers, latch the
// combo-in-progress flag, reset the multiplier to its neutral 1, and assert that no
// pending (guaranteed / non-guaranteed) score is still hanging around at combo start.
// Reconstructed store-for-store from the X360 asm (store order preserved).
void StuntModeScoring::BeginCombo()
{
    mfComboScore             = 0.0f;   // X360 stfs flt_82001CC0(=0), 0x20
    mfTimeSinceLastStunt     = 0.0f;   // X360 stfs 0, 0x58 (combo timer: time-since-last-stunt)
    mfSpeedMPHBeforeCrashing = 0.0f;   // X360 stfs 0, 0x5C (combo timer: combo-active)
    mbComboInProgress        = true;   // X360 stb 1, 0x2A
    miComboMultiplier        = 1;      // X360 stw 1, 0x24

    // X360 lfs 0x1C -> RwMathFPU::IsZero assert (BrnStuntModeScoring.cpp:1460).
    CGS_ASSERT(RwMathFPU_IsZero(mfPendingGuaranteedScore),
               "RwMathFPU::IsZero(mfPendingGuaranteedScore)");
    // X360 lfs 0x18 -> RwMathFPU::IsZero assert (BrnStuntModeScoring.cpp:1461).
    CGS_ASSERT(RwMathFPU_IsZero(mfPendingNonGuaranteedScore),
               "RwMathFPU::IsZero(mfPendingNonGuaranteedScore)");
}

// ----------------------------------------------------------------------------
// EndCombo  --  X360 0x823215D8
// ----------------------------------------------------------------------------
// Bank the finished combo and tear it down:
//   * If the combo scored ((s32)mfComboScore > 0): record the recent-combo snapshot
//     (mbRecentCombo, miRecentComboScore = miComboMultiplier * (s32)mfComboScore,
//     mfRecentComboTime = the combo-active timer).
//   * If the combo ended with the time limit already expired (and the run is not endless),
//     park mfTimeDelayBeforeModeEnd at the mode-end delay tunable.
//   * [FLAG -- one call isolated, see file header] BaseCollisionGenerator::Destruct(member@0x22C0).
//   * Accumulate the banked combo into miCurrentScore (+= miComboMultiplier * (s32)mfComboScore).
//   * Clear the three recent containers, the per-category rating records, and reset the
//     combo/stunt scoring state to its post-combo baseline.
// Reconstructed store-for-store from the X360 asm (store order preserved).
void StuntModeScoring::EndCombo()
{
    // X360: liComboScore = (s32)mfComboScore (lfs 0x20; fctiwz).
    const s32 liComboScore = static_cast<s32>(mfComboScore);

    if (liComboScore > 0)
    {
        mfRecentComboTime  = mfSpeedMPHBeforeCrashing;        // X360 lfs 0x5C -> stfs 0x88
        mbRecentCombo      = true;                            // X360 stb 1, 0x80
        miRecentComboScore = miComboMultiplier * liComboScore; // X360 mullw 0x24 * (s32)0x20 -> stw 0x84
    }

    if (mbComboInProgress && mbTimeLimitExpired && !mbEndlessStuntRun)  // X360 0x2A && 0x2E && !0x22BC
    {
        mfTimeDelayBeforeModeEnd = KF_TIME_DELAY_BEFORE_MODE_END;       // X360 stfs flt_82CDB7B0, 0x60
    }

    // FLAG (sole isolated call -- see file header): the X360 virtual call on *(this + 0x22C0),
    // which Hex-Rays mis-symbols as BaseCollisionGenerator::Destruct, is really (per the PS3
    // DecFIGS EndCombo 0x1D217C) mpAchievementManager->OnStuntRunMultiplier(miComboMultiplier).
    // Left UN-EMITTED only to keep this partial TU from including the AchievementManager header
    // (mpAchievementManager is forward-declared incomplete in the keystone).
    // mpAchievementManager->OnStuntRunMultiplier(miComboMultiplier);

    // X360 0x10 += (s32)0x20 * 0x24  (mfComboScore re-read + fctiwz; mullw; add to miCurrentScore).
    miCurrentScore += miComboMultiplier * static_cast<s32>(mfComboScore);

    // X360 container clears (same three Clear()s as ClearData): jump ring @0x208/0x20C/0x210,
    // stunt-element set length @0x2220, prop ring @0x2230/0x2234/0x2238.
    mRecentJumpSet.Clear();          // X360 stw 0, 0x208/0x20C/0x210
    mRecentStuntElementSet.Clear();  // X360 stw 0, 0x2220
    mRecentPropSet.Clear();          // X360 stw 0, 0x2230/0x2234/0x2238

    // X360 do/while (mtctr 18; base this+0x90; stride 16): clear every per-category rating record.
    for (s32 liIndex = 0; liIndex < 18; ++liIndex)
    {
        mStuntTypeInfo[liIndex].mfScore         = 0.0f;  // X360 stfs 0, 0x00
        mStuntTypeInfo[liIndex].mfTimeActive    = 0.0f;  // X360 stfs 0, 0x08 (+8 in the loop body)
        mStuntTypeInfo[liIndex].mfTimeSinceLast = 0.0f;  // X360 stfs 0, 0x04
        mStuntTypeInfo[liIndex].mbActive        = false; // X360 stb 0, 0x0C
    }

    mfPendingGuaranteedScore      = 0.0f;   // X360 stfs 0, 0x1C
    miComboMultiplier             = 1;      // X360 stw 1, 0x24
    mfPendingNonGuaranteedScore   = 0.0f;   // X360 stfs 0, 0x18
    mbComboInProgress             = false;  // X360 stb 0, 0x2A
    mfComboScore                  = 0.0f;   // X360 stfs 0, 0x20
    mbStuntInProgress             = false;  // X360 stb 0, 0x29
    muStuntTypesInProgress        = 0;      // X360 stw 0, 0x50
}

// ----------------------------------------------------------------------------
// UpdateCombo  --  X360 0x82320FF0
// ----------------------------------------------------------------------------
// Per-frame combo timer advance + auto-end. While a combo is in progress:
//   * the combo-active timer (mfSpeedMPHBeforeCrashing) advances every frame;
//   * if a stunt is in progress OR any score is pending, the time-since-last-stunt timer is
//     held at zero (the combo is "alive");
//   * otherwise the time-since-last-stunt timer advances, and the combo auto-ends when it
//     crosses KF_TIME_WITHOUT_STUNT_TO_LOSE_COMBO, or when the time limit has expired and the
//     combo score has drained to zero with nothing pending.
// The lpRaceCar param is unread by this body (the X360 asm touches only lfDelta); it is part
// of the committed signature. EndCombo() is reached via a vtable +0x20 dispatch in the asm;
// for semantic parity the base body calls EndCombo() directly (cf. the Construct->ClearData
// vtable +0x10 dispatch handled the same way in BrnStuntModeScoring_Lifecycle.cpp).
// Reconstructed store-for-store from the X360 asm.
void StuntModeScoring::UpdateCombo(f32 lfDelta, const ActiveRaceCarOutputInterface* lpRaceCar)
{
    (void)lpRaceCar;  // X360: param present in the signature but unread in this body.

    if (mbComboInProgress)  // X360 lbz 0x2A
    {
        mfSpeedMPHBeforeCrashing += lfDelta;  // X360 lfs 0x5C; fadds; stfs 0x5C (combo-active timer)

        // X360: r11 = mbStuntInProgress (0x29); if (r11 || HasAnyPendingScore()) -> reset 0x58.
        if (mbStuntInProgress || HasAnyPendingScore())
        {
            mfTimeSinceLastStunt = 0.0f;  // X360 stfs flt_82001CC0(=0), 0x58
        }
        else
        {
            mfTimeSinceLastStunt += lfDelta;  // X360 lfs 0x58; fadds; stfs 0x58 (time-since-last-stunt)

            if (mfTimeSinceLastStunt > KF_TIME_WITHOUT_STUNT_TO_LOSE_COMBO)  // X360 fcmpu flt_82CDB790
            {
                EndCombo();  // X360 vtable +0x20 dispatch on `this`
            }

            if (mbTimeLimitExpired)  // X360 lbz 0x2E
            {
                // X360: f0 = mfComboScore (0x20); RwMathFPU::IsZero check.
                if (RwMathFPU_IsZero(mfComboScore) && !HasAnyPendingScore())
                {
                    EndCombo();  // X360 vtable +0x20 dispatch on `this`
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// BankMultiplier  --  X360 0x82312D68
// ----------------------------------------------------------------------------
// Compute the multiplier for a just-completed stunt and stamp it into the stunt record.
// Asserts the stunt pointer, runs CalculateMultiplier into a stack MultiplierOutInfo
// (X360 vtable +0x28 dispatch -> the virtual CalculateMultiplier), writes the result into
// lpRecentStunt->miStuntMultiplier (StuntInfo +0x0C), and returns it.
// Reconstructed store-for-store from the X360 asm.
s32 StuntModeScoring::BankMultiplier(StuntInfo* lpRecentStunt)
{
    CGS_ASSERT(lpRecentStunt, "lpRecentStunt");  // X360 cmplwi r31,0 -> FireAssert ...:503

    MultiplierOutInfo loMultiplierOutInfo;       // X360 24-byte stack record [sp+...] BYREF
    // X360: result = (*(*this + 0x28))(this, lpRecentStunt, &loMultiplierOutInfo)  (virtual CalculateMultiplier).
    const s32 liMultiplier = CalculateMultiplier(lpRecentStunt, &loMultiplierOutInfo);

    lpRecentStunt->miStuntMultiplier = liMultiplier;  // X360 stw r3, 0xC(r31) -> StuntInfo +0x0C
    return liMultiplier;
}

// ----------------------------------------------------------------------------
// CalculateMultiplier  --  X360 0x82312DE8
// ----------------------------------------------------------------------------
// The multiplier formula. The base multiplier is:
//     popcount(muAwesomeStuntTypes)                              (X360 SWAR byte-sum popcount)
//   + (muBarrelRolls >= 1 ? muBarrelRolls - 1 : 0)               (X360 (((1-x)>>31) & (x-1)) idiom)
//   + (muFlatSpins   >= 1 ? muFlatSpins   - 1 : 0)
//   + muBarrelRolls
// It also fills the MultiplierOutInfo output record from the awesome-type bit-mask:
//     bit 0x04 -> mbAwesomeFlatSpin    (out +6)
//     bit 0x10 -> mbAwesomeBarrelRoll  (out +7)
//     bit 0x40 -> mbAwesomeStunt       (out +8)
//     bit 0x01 -> muFlatSpins   = lpStuntInfo->muFlatSpins         (out +0, u16)
//     bit 0x02 -> muBarrelRolls = ROL(lpStuntInfo->muBarrelRolls,1)(out +2, u16)
// Reconstructed store-for-store from the X360 asm. The popcount is the X360 SWAR idiom
// (x - ((x>>1)&0x55..); nibble-sum; *0x01010101 >> 24); we reproduce it as a clean bit count
// over muAwesomeStuntTypes (the same 32-bit value), which is numerically identical.
s32 StuntModeScoring::CalculateMultiplier(const StuntInfo* lpStuntInfo,
                                          MultiplierOutInfo* lpMultiplierOutInfo)
{
    // X360 SWAR popcount of muAwesomeStuntTypes (a2+4). Reproduced as the equivalent bit count.
    const u32 luAwesome = lpStuntInfo->muAwesomeStuntTypes;
    u32 luPopCount = 0;
    for (u32 luBits = luAwesome; luBits != 0; luBits >>= 1)
    {
        luPopCount += (luBits & 1u);
    }

    const u32 luBarrelRolls = lpStuntInfo->muBarrelRolls;  // X360 lhz 0x12 (a2+18)
    const u32 luFlatSpins   = lpStuntInfo->muFlatSpins;    // X360 lhz 0x10 (a2+16)

    // X360 (((1 - x) >> 31) & (x - 1)) == (x >= 1 ? x - 1 : 0) for the unsigned counts.
    const s32 liBarrelRollBonus = (luBarrelRolls >= 1u) ? static_cast<s32>(luBarrelRolls - 1u) : 0;
    const s32 liFlatSpinBonus   = (luFlatSpins   >= 1u) ? static_cast<s32>(luFlatSpins   - 1u) : 0;

    const s32 liMultiplier = static_cast<s32>(luPopCount)
                           + liBarrelRollBonus
                           + liFlatSpinBonus
                           + static_cast<s32>(luBarrelRolls);

    CGS_ASSERT(lpMultiplierOutInfo, "lpMultiplierOutInfo");  // X360 cmplwi r30,0 -> FireAssert ...:537

    if ((luAwesome & 0x04u) == 0x04u)   // X360 rlwinm 0,29,29; cmplwi 4
    {
        lpMultiplierOutInfo->mbAwesomeFlatSpin = true;    // X360 stb 1, 6(r30)
    }
    if ((luAwesome & 0x10u) == 0x10u)   // X360 rlwinm 0,27,27; cmplwi 0x10
    {
        lpMultiplierOutInfo->mbAwesomeBarrelRoll = true;  // X360 stb 1, 7(r30)
    }
    if ((luAwesome & 0x40u) == 0x40u)   // X360 rlwinm 0,25,25; cmplwi 0x40
    {
        lpMultiplierOutInfo->mbAwesomeStunt = true;       // X360 stb 1, 8(r30)
    }
    if ((luAwesome & 0x01u) == 0x01u)   // X360 clrlwi 0,31; cmplwi 1
    {
        lpMultiplierOutInfo->muFlatSpins = static_cast<u16>(luFlatSpins);  // X360 lhz 0x10; sth 0(r30)
    }
    if ((luAwesome & 0x02u) == 0x02u)   // X360 rlwinm 0,30,30; cmplwi 2
    {
        // X360 __ROL4__(muBarrelRolls, 1) stored as a halfword (rotlwi r11,r11,1; sth 2(r30)).
        const u32 luRol = (luBarrelRolls << 1) | (luBarrelRolls >> 31);
        lpMultiplierOutInfo->muBarrelRolls = static_cast<u16>(luRol);  // X360 sth 2(r30)
    }

    return liMultiplier;
}

} // namespace BrnGameState
