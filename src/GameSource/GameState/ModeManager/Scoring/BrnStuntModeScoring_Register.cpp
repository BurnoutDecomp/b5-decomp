#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"

#include "GameSource/GameState/BrnGameActions.h"        // WorldStuntAction / PowerParkResultAction (the action types these bodies deref)
#include "GameSource/GameState/BrnGameStateTypes.h"      // EStuntType, StuntElementType
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

// ============================================================================
// BrnStuntModeScoring_Register.cpp
// ============================================================================
// Bodies for the stunt-scorer "register / score-a-stunt" cluster of
// BrnGameState::StuntModeScoring (home header BrnStuntModeScoring.h). Each landed body is
// reconstructed STORE-FOR-STORE from its X360 pseudocode/asm (addresses in the per-method
// comments) and touches StuntModeScoring members BY NAME. Gate is `cl /c` (no link): every
// callee bodied in the not-yet-reconstructed sibling TUs (RegisterStunt's HasAnyPendingScore,
// UpdateScore, BeginCombo, EndCombo, UpdateStuntRating) is DECLARE-ONLY in the home header,
// which is all a compile gate needs.
//
// OFFSET -> NAMED MEMBER map (anchored from the X360 asm of Activate @0x82312FA0,
// ClearData @0x82321108, EndCombo @0x823215D8 -- all in this type's TU):
//   +0x10 miCurrentScore   +0x14 miTargetScore   +0x18 mfPendingNonGuaranteedScore
//   +0x1C mfPendingGuaranteedScore   +0x20 mfComboScore   +0x24 miComboMultiplier
//   +0x28 mbStuntModeActive   +0x29 mbStuntInProgress   +0x2A mbComboInProgress
//   +0x2B mbValidStunt   +0x2C mbWasInAirLastFrame   +0x2D mbInitialDriftOngoing
//   +0x2E mbTimeLimitExpired   +0x2F mbTimeUpMessageSent   +0x30 mbPlayerCarCrashing
//   +0x40 mStuntRollInProgress(Vector3)   +0x50 muStuntTypesInProgress
//   +0x54 muAwesomeStuntTypesInProgress   +0x58 mfTimeSinceLastStunt
//   +0x5C mfSpeedMPHBeforeCrashing   +0x60 mfTimeDelayBeforeModeEnd
//   +0x8C mfPendingScoreTimer   +0x90 mStuntTypeInfo[0] (16B stride: mfTimeActive@0,
//   mfTimeSinceLast@4, mfScore@8, mbActive@12)   +0x1220 mRecentStuntElementSet.
//
// VTABLE NOTE: RegisterStunt's "start the combo" tail dispatches through the embedded scorer's
// vtable slot +0x1C, and (in DealWithPowerPark) the post-score slot +0x20. The home header models
// only HasStuntModeEnded (slot +0x14) + CalculateMultiplier (slot +0x28) as virtual and explicitly
// DEFERS reconstructing the full vtable order. Slot +0x1C resolves to BeginCombo, slot +0x20 to
// EndCombo (the Combo draft anchors both: UpdateCombo dispatches EndCombo via +0x20). We call
// BeginCombo()/EndCombo() by NAME here. FLAG: these call the BASE directly; they do NOT reproduce
// the StuntModeScoringOnline overrides the X360 reaches via the vtable. Faithful virtual dispatch
// lands when this type's TU reconstructs the full vtable order.
//
// LANDED -- DealWithInProgressStunt (0x82321710): signature was RECONCILED earlier from the call-site
//   ModeManager::ProcessEvent (0x82340AB8) -- the real args are (const OnStuntElementCompleteAction*
//   lpAction [r4], f32 lfDelta [f1], s32 liPlayerActiveRaceCarIndex [r6 == GameStateModule::
//   GetPlayerActiveRaceCarIndex]); IDA's a4(r5) is uninitialised garbage at the call site and dropped.
//   The OLD committed sig (const WorldStuntAction*) was wrong on BOTH the action type AND the missing
//   delta+index args. The BODY now lands below: OnStuntElementCompleteAction (BrnGameActions.h) has
//   been GROWN ADDITIVELY with the X360-only convoy block (maConvoyLegDistances[8]@+0x24,
//   maConvoyMemberIds[8]@+0x44, miConvoyMemberCount@+0x64), so the convoy-shaped record the asm walks
//   is now expressible by NAME. See the DealWithInProgressStunt body comment for the store-for-store
//   decode.
// ============================================================================

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// RegisterStunt -- X360 0x82313370.
// Gate-keeps the start of a scored stunt. If a stunt is already in progress this is a no-op that
// reports success. Otherwise it refuses (returns false) while the mode is winding down
// (mfTimeDelayBeforeModeEnd > 0) or the player car is crashing. On a fresh start it either arms
// the pending-score timer (when score is already pending) or flags the stunt valid, marks a stunt
// in progress, and -- if no combo is running yet -- opens one (vtable slot +0x1C == BeginCombo).
// ----------------------------------------------------------------------------
bool StuntModeScoring::RegisterStunt()
{
    if (!mbStuntInProgress)
    {
        // Cannot start a new stunt while the mode-end delay timer is counting down, or while the
        // player car is in a crash. (0x60 > 0.0  ||  0x30 != 0)  -> return false.
        if (mfTimeDelayBeforeModeEnd > 0.0f || mbPlayerCarCrashing)
        {
            return false;
        }

        if (HasAnyPendingScore())
        {
            // FLAG: flt_82CDB788 magnitude unrecovered -- the "pending time" the timer is armed
            // with (best-effort KF_PENDING_SCORE_PENDING_TIME); stored into mfPendingScoreTimer.
            const f32 KF_PENDING_SCORE_PENDING_TIME = 0.0f;   // FLAG: flt_82CDB788 value unrecovered
            mfPendingScoreTimer = KF_PENDING_SCORE_PENDING_TIME;
        }
        else
        {
            mbValidStunt = true;
        }

        const bool lbComboAlreadyInProgress = mbComboInProgress;
        mbStuntInProgress = true;

        if (!lbComboAlreadyInProgress)
        {
            // X360: (*(vtable + 0x1C))(this)  -- the virtual that opens a combo. See VTABLE NOTE.
            BeginCombo();
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// DealWithStunt -- X360 0x8232CEB0.
// A world stunt element (super-jump / super-smash / billboard-smash) was performed. When the stunt
// mode is active, the 64-bit stunt-element key (lpAction->mId) is de-duped against the
// already-counted set (mRecentStuntElementSet, this+0x1220):
//   * FIRST time this element is seen (Find == not-present): branch on the element discriminant
//     (lpAction->meStuntElementType) --
//       JUMP(0)      : RegisterStunt; on success UpdateScore(flt_82CDB718, SUPER_JUMP=4) then
//                      UpdateStuntRating(4,...); element is then queued for Insert.
//       SMASH(1)     : RegisterStunt; on success UpdateScore(flt_82CDB71C, SUPER_SMASH=5).
//                      NOTE the asm gotos LABEL_14 directly -- SMASH gets NO UpdateStuntRating and
//                      is NOT inserted into the set (v13 stays false).
//       BILLBOARD(2) : RegisterStunt; on success UpdateScore(flt_82CDB720, BILLBOARD=6) then
//                      UpdateStuntRating(6,...); element is then queued for Insert.
//       >= COUNT(3)  : CGS_ASSERT "Unknown world stunt type." (the X360 asserts at .cpp:1247).
//     If the element was scored+rated (the JUMP/BILLBOARD paths, v13==true) it is Insert'd into the
//     set (asserting room first, .cpp:1254).
//   * ALREADY in the set (repeat): RegisterStunt; on success mark the repetition bit in
//     muStuntTypesInProgress (|= 0x40000) and clear mbValidStunt (this element no longer qualifies).
//
// The X360 ABI passes the small WorldStuntAction by value (discriminant in r4, 64-bit mId in r5);
// the home header models the param as a pointer, so we deref lpAction and access members BY NAME.
// ----------------------------------------------------------------------------
void StuntModeScoring::DealWithStunt(const GameStateModuleIO::WorldStuntAction* lpAction)
{
    if (!mbStuntModeActive)
    {
        return;
    }

    // The de-dupe key is the 64-bit stunt-element id (Set<CgsID,512>). X360: Find(this+0x1220, &id).
    const CgsID lElementId = lpAction->mId;

    if (mRecentStuntElementSet.Find(lElementId) == StuntElementSet::KU_INVALID)
    {
        // FLAG: flt_82CDB718 / flt_82CDB71C / flt_82CDB720 magnitudes unrecovered -- the base score
        // each world-stunt element awards (best-effort 0.0f). Re-confirm against the asm when this
        // TU's full BrnStuntModeScoring.cpp lands.
        const f32 KF_SUPER_JUMP_STUNT_SCORE = 0.0f;   // FLAG: flt_82CDB718 value unrecovered
        const f32 KF_SUPER_SMASH_STUNT_SCORE = 0.0f;  // FLAG: flt_82CDB71C value unrecovered
        const f32 KF_BILLBOARD_STUNT_SCORE = 0.0f;    // FLAG: flt_82CDB720 value unrecovered

        bool       lbScored = false;
        EStuntType leStuntType = E_STUNT_TYPE_INVALID;

        switch (lpAction->meStuntElementType)
        {
            case E_STUNT_ELEMENT_TYPE_JUMP:   // 0
                if (RegisterStunt())
                {
                    UpdateScore(KF_SUPER_JUMP_STUNT_SCORE, E_STUNT_TYPE_SUPER_JUMP, false);
                    leStuntType = E_STUNT_TYPE_SUPER_JUMP;
                    UpdateStuntRating(leStuntType, 0.0f, 0.0f, 0.0f);
                    lbScored = true;
                }
                break;

            case E_STUNT_ELEMENT_TYPE_SMASH:  // 1
                // SMASH does NOT rate or insert (the asm gotos LABEL_14 with v13 still false).
                if (RegisterStunt())
                {
                    UpdateScore(KF_SUPER_SMASH_STUNT_SCORE, E_STUNT_TYPE_SUPER_SMASH, false);
                }
                break;

            case E_STUNT_ELEMENT_TYPE_BILLBOARD:  // 2
                if (RegisterStunt())
                {
                    UpdateScore(KF_BILLBOARD_STUNT_SCORE, E_STUNT_TYPE_BILLBOARD, false);
                    leStuntType = E_STUNT_TYPE_BILLBOARD;
                    UpdateStuntRating(leStuntType, 0.0f, 0.0f, 0.0f);
                    lbScored = true;
                }
                break;

            default:  // >= E_STUNT_ELEMENT_TYPE_COUNT (3)
                CGS_ASSERT(false, "Unknown world stunt type.");
                break;
        }

        if (lbScored)
        {
            CGS_ASSERT(mRecentStuntElementSet.GetLength() < mRecentStuntElementSet.GetCapacity(),
                       "mRecentStuntElementSet.GetLength() < mRecentStuntElementSet.GetCapacity()");
            mRecentStuntElementSet.Insert(lElementId);
        }
    }
    else
    {
        // Already counted this element this run: it is a repeat. RegisterStunt, then flag the
        // repetition (muStuntTypesInProgress |= 0x40000) and drop the valid-stunt flag.
        if (RegisterStunt())
        {
            muStuntTypesInProgress |= 0x40000u;   // X360 0x8232D04C: oris r11,r11,4 (== |0x40000)
            mbValidStunt = false;                 // X360 0x8232D050: stb 0, 0x2B(r31)
        }
    }
}

// ----------------------------------------------------------------------------
// DealWithHitProp -- X360 0x823214C8.
// A prop was hit. When the stunt mode is active and the hit carries the "scoring" flag bit
// (luFlags & 1), register a stunt and, on success, award the prop-hit score. luPropId is not
// read on this path (it is recorded elsewhere); the X360 passes a zero score with stunt type 5.
// ----------------------------------------------------------------------------
void StuntModeScoring::DealWithHitProp(u16 luPropId, u8 luFlags)
{
    (void)luPropId;   // unused on this path (X360 does not read it here)

    if (mbStuntModeActive && (luFlags & 1) != 0)
    {
        if (RegisterStunt())
        {
            // X360: UpdateScore(this, f1=flt_82001CC0=0.0f, r5=5, r6=0).
            UpdateScore(0.0f, E_STUNT_TYPE_SUPER_SMASH, false);
        }
    }
}

// ----------------------------------------------------------------------------
// DealWithPowerPark -- X360 0x82321530.
// A power-park manoeuvre resolved. When the stunt mode is active and the park SUCCEEDED
// (lpAction->meOutcome == E_PPO_SUCCESS), register a stunt and, on success:
//   * award a score proportional to the park's overall rating
//     ((f32)lpAction->miOverallRating * flt_82CDB728), stunt type POWER_PARK(11), awesome=true;
//   * dispatch the post-score vtable slot +0x20 == EndCombo (best-effort base call; see VTABLE NOTE).
// ----------------------------------------------------------------------------
void StuntModeScoring::DealWithPowerPark(const GameStateModuleIO::PowerParkResultAction* lpAction)
{
    if (mbStuntModeActive && lpAction->meOutcome == BrnWorld::E_PPO_SUCCESS)
    {
        if (RegisterStunt())
        {
            // FLAG: flt_82CDB728 magnitude unrecovered -- the per-rating-point score the power-park
            // award scales by (best-effort 0.0f). X360: score = (f32)miOverallRating * flt_82CDB728.
            const f32 KF_POWER_PARK_SCORE_PER_RATING = 0.0f;   // FLAG: flt_82CDB728 value unrecovered
            const f32 lfScore = static_cast<f32>(lpAction->miOverallRating) * KF_POWER_PARK_SCORE_PER_RATING;

            UpdateScore(lfScore, E_STUNT_TYPE_POWER_PARK, true);

            // X360: (*(vtable + 0x20))(this) == EndCombo. See VTABLE NOTE (base call, not the
            // StuntModeScoringOnline override).
            EndCombo();
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateStuntRepetition -- X360 0x82312F38.
// Walks all per-category rating slots (mStuntTypeInfo) once per frame to age the repetition
// timers. The X360 iterates 18 slots (up to E_STUNT_TYPE_RATING_AWESOME); the home header now
// sizes the array [18] to match.
//
// The base pointer the asm walks is this+0x94 (== &mStuntTypeInfo[0].mfTimeSinceLast), stepping
// 0x10 per slot; it reads/writes *(p)   == mfTimeSinceLast (@+0x94),
//                                *(p-4) == mfTimeActive    (@+0x90),
//                                *(p+8) == mbActive        (@+0x9C).
//   ACTIVE  : mfTimeActive += lfDelta; mfTimeSinceLast = 0.0f.
//   INACTIVE: mfTimeSinceLast += lfDelta; once it crosses the repetition timeout the run has
//             lapsed -> mfTimeActive = 0.0f.
//
// (Round-1 draft had the two timers INVERTED; corrected here per the asm -- see the offset map
// above: the *(p-4)/mfTimeActive store is the one the delta accumulates in the ACTIVE branch.)
// ----------------------------------------------------------------------------
void StuntModeScoring::UpdateStuntRepetition(f32 lfDelta)
{
    // FLAG: flt_82CDB7A0 magnitude unrecovered -- the repetition-decay timeout the "since last"
    // timer must cross (best-effort KF_STUNT_TYPE_REPETITION_TIMEOUT).
    const f32 KF_STUNT_TYPE_REPETITION_TIMEOUT = 0.0f;   // FLAG: flt_82CDB7A0 value unrecovered

    for (s32 liStuntTypeIndex = 0; liStuntTypeIndex < 18; ++liStuntTypeIndex)
    {
        StuntTypeInfo& lInfo = mStuntTypeInfo[liStuntTypeIndex];

        if (lInfo.mbActive)
        {
            // Active: bank the elapsed step into the "active" run, clear "time since last".
            lInfo.mfTimeActive = lInfo.mfTimeActive + lfDelta;
            lInfo.mfTimeSinceLast = 0.0f;
        }
        else
        {
            // Inactive: grow "time since last"; once it crosses the timeout, the repetition window
            // has lapsed -- reset the "active" run.
            lInfo.mfTimeSinceLast = lInfo.mfTimeSinceLast + lfDelta;
            if (lInfo.mfTimeSinceLast >= KF_STUNT_TYPE_REPETITION_TIMEOUT)
            {
                lInfo.mfTimeActive = 0.0f;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// UpdateStuntRating -- X360 0x823130C8.
// Promotes the in-progress stunt to "good" and/or "awesome" based on a measured value (lfA)
// against its good/awesome thresholds (lfB/lfC). Asserts a stunt + combo are both in progress.
// "Good" sets the 0x100000 bit in muStuntTypesInProgress; "awesome" sets the 0x200000 bit there
// and ORs (1 << leStuntType) into muAwesomeStuntTypesInProgress.
//
// Param spelling follows the home header (EStuntType, f32, f32, f32). In the X360 the value bit
// is `1 << leStuntType` with leStuntType reaching E_STUNT_TYPE_RATING_GOOD(17), so the shift is
// done on a 32-bit operand into the 32-bit awesome-types mask.
// ----------------------------------------------------------------------------
void StuntModeScoring::UpdateStuntRating(EStuntType leStuntType, f32 lfA, f32 lfB, f32 lfC)
{
    CGS_ASSERT(mbStuntInProgress && mbComboInProgress, "mbStuntInProgress && mbComboInProgress");

    if (lfA >= lfB)
    {
        // 0x82313140: oris 0x10 into the high half of the 32-bit word == OR 0x100000.
        muStuntTypesInProgress |= 0x100000u;
    }

    if (lfA >= lfC)
    {
        // 0x8231315C: oris 0x20 == OR 0x200000, plus (1 << leStuntType) into the awesome mask.
        muStuntTypesInProgress |= 0x200000u;
        muAwesomeStuntTypesInProgress = (1u << static_cast<u32>(leStuntType)) | muAwesomeStuntTypesInProgress;
    }
}

// ----------------------------------------------------------------------------
// DealWithInProgressStunt -- X360 0x82321710.
// A convoy in-progress stunt element resolved for the local player. Gates on the stunt mode being
// active, then on the action being a real convoy (member count > 1). When both hold it RegisterStunt's
// and -- if a stunt could be started -- finds the convoy "leg" that qualifies (the first per-leg
// distance >= flt_82CDB778), locates this player's slot in the convoy member-id table, and awards a
// score scaled by that slot index (so later joiners score more): UpdateScore((slot * flt_82CDB73C) *
// lfDelta, E_STUNT_TYPE_RATING_GOOD(17), awesome=true).
//
// STORE-FOR-STORE map (asm @0x82321710; r30=this, r31=lpAction, f31=lfDelta, r29=liPlayerActiveRaceCarIndex):
//   0x82321730 lbz 0x28(this)        -> gate mbStuntModeActive (byte; offset map +0x28).
//   0x8232173C lwz 0x64(lpAction)>1  -> gate lpAction->miConvoyMemberCount > 1.
//   0x82321748 bl RegisterStunt; clrlwi r3,24 / beq -> proceed only if it returned true.
//   0x82321764 r10 = lpAction+0x24   -> &maConvoyLegDistances[0]; walk [0..7] until *p >= flt_82CDB778
//                                       (lfs/fcmpu/bge), break with that index in r11; if r11==8 none.
//   0x82321794 r11 = lpAction+0x44   -> &maConvoyMemberIds[0]; linear-search [0..7] for r29 (player
//                                       index); full miss (r10>=8) -> assert "Player not in this convoy!"
//                                       (BrnStuntModeScoring.cpp:1666).
//   0x82321838+ found slot r10: extsw -> (s64) slot, fcfid/frsp -> (f32); f0 = slot * flt_82CDB73C;
//              f1 = f0 * f31 (lfDelta); li r5,0x11 (E_STUNT_TYPE_RATING_GOOD=17); li r6,1 (awesome);
//              UpdateScore(this, <r4 garbage dropped>, f1=score, r5=17, r6=1).
// The committed UpdateScore sig is void(f32 lfScore, EStuntType, bool); confirmed against this call
// (r5=type, r6=awesome -- the UpdateScore body @0x823212D8 reads its stunt-type from r5/a3 and its
// awesome flag from r6/a4, the IDA pseudocode's `v9`/r4 is the dropped uninitialised slot). No
// divergence -- the committed sig matches the asm.
//
// FLAG: flt_82CDB778 (per-leg qualifying-distance threshold) and flt_82CDB73C (per-convoy-slot score
// multiplier) magnitudes are UNRECOVERED -- not present in any .ida-exports data dump (same gap as the
// sibling flt_82CDB718/71C/720/728 scores in this file). Named constants are defined at their X360
// addresses with a best-effort value (0.0f); re-confirm against the asm when this TU's full
// BrnStuntModeScoring.cpp lands.
// ----------------------------------------------------------------------------
void StuntModeScoring::DealWithInProgressStunt(const GameStateModuleIO::OnStuntElementCompleteAction* lpAction,
                                               f32 lfDelta, s32 liPlayerActiveRaceCarIndex)
{
    // FLAG: rodata magnitudes unrecovered (see header note).
    const f32 KF_CONVOY_LEG_DISTANCE_THRESHOLD = 0.0f;   // FLAG: flt_82CDB778 value unrecovered
    const f32 KF_CONVOY_SLOT_SCORE_MULTIPLIER  = 0.0f;   // FLAG: flt_82CDB73C value unrecovered

    // 0x28(this) gate: only score while the stunt mode is active.
    if (!mbStuntModeActive)
    {
        return;
    }

    // A real convoy needs more than one member (the loop gate `count > 1`).
    if (lpAction->miConvoyMemberCount <= 1)
    {
        return;
    }

    // Try to start/continue a scored stunt; bail if it could not be registered.
    if (!RegisterStunt())
    {
        return;
    }

    // Walk the per-leg distances until one qualifies (>= threshold); break with that leg index.
    s32 liLegIndex = 0;
    while (liLegIndex < 8)
    {
        if (lpAction->maConvoyLegDistances[liLegIndex] >= KF_CONVOY_LEG_DISTANCE_THRESHOLD)
        {
            break;
        }
        ++liLegIndex;
    }

    // No qualifying leg -> nothing to award.
    if (liLegIndex == 8)
    {
        return;
    }

    // Locate this player's slot in the convoy member-id table.
    s32 liMemberIndex = 0;
    while (lpAction->maConvoyMemberIds[liMemberIndex] != liPlayerActiveRaceCarIndex)
    {
        ++liMemberIndex;
        if (liMemberIndex >= 8)
        {
            // X360 assert at BrnStuntModeScoring.cpp:1666 (the player must be in this convoy).
            CGS_ASSERT(false, "Player not in this convoy!");
            return;
        }
    }

    // Award a score scaled by the player's convoy slot index and the frame delta. Stunt type 17 ==
    // E_STUNT_TYPE_RATING_GOOD (li r5,0x11), awesome == true (li r6,1).
    const f32 lfScore = (static_cast<f32>(liMemberIndex) * KF_CONVOY_SLOT_SCORE_MULTIPLIER) * lfDelta;
    UpdateScore(lfScore, E_STUNT_TYPE_RATING_GOOD, true);
}

}   // namespace BrnGameState
