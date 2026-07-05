// ============================================================================
// BrnGui::BurnoutSkillsManager  -- HUD "burnout skillz" record tracker.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (authoritative for behaviour and
// the member-store offsets) gated against the DecFIGS DWARF for the declaration
// shape (member names/types and the showing-type enum). This TU owns the eight
// X360-attested methods of the class:
//   Construct        @0x824F3B00
//   ResetSkillsData  @0x824F3BB0
//   YouBeatSkill     @0x824F3C10   (private)
//   ResetPlayerData  @0x824F3C88
//   SelectNextSkill  @0x824F3E58   (private)
//   Update           @0x824F9E00
//   SelectNext       @0x824F9ED0
//   SetRoadRuleMode  @0x824F9F30
//
// The HUD skill-of-the-moment state machine (meCurrentShowingType):
//   AUTO_ROTATE      -- cycle through the skills on a timer (KF_ROTATION_TIME)
//   SELECT           -- user has paged to a specific skill (rotation paused)
//   NEW_SCORE        -- briefly flash a freshly-beaten record (KF_NEW_PAUSE_TIME)
//   ROAD_RULE_ACTIVE -- pinned to a road-rule skill while a contest is running
// The HUD only ticks while the active game-mode type is the skills/road-rule
// mode (GuiCache::GetCurrentGameModeType() == KI_SKILLS_ACTIVE_GAME_MODE_TYPE).
// ============================================================================

#include "GameSource/Gui/BrnGuiBurnoutSkillsManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{

// ---- static constants ------------------------------------------------------
// Both dwell timers are the same 5.0s constant the X360 baked inline (flt_8200426C):
// the auto-rotate cadence (ResetSkillsData / Update) and the new-record flash hold
// (YouBeatSkill) each push mfTimeToNextChange forward by this much from GetTime().
const f32 BurnoutSkillsManager::KF_ROTATION_TIME  = 5.0f;
const f32 BurnoutSkillsManager::KF_NEW_PAUSE_TIME = 5.0f;

// flt_8206F11C: the lowest-wins record search seed (FLT_MAX). The X360 baked the literal
// in low .rodata; mirrored here as a named constant (project idiom avoids <cfloat>).
namespace
{
    const f32 KF_FLT_MAX = 3.4028235e38f;
}
//
// ----------------------------------------------------------------------------
// KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL  (rodata flt_8206F7B0)  -- DEFINITION
//   Per-skill minimum new-record score before a beaten record fires the HUD flash.
//   SetSkillsData reads KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL[leSkill] and compares the
//   new score `>=` against it. Now ODR-used in this TU (SetSkillsData folded in), so it
//   must be DEFINED here.
//
//   *** UNRECOVERED VALUES -- CONSOLIDATOR MUST FILL ***  The 14 float constants live
//   in X360 low .rodata at flt_8206F7B0; the dossier does not carry the bytes. Placeholder
//   zeros below keep the TU self-consistent but the numbers are NOT attested and
//   behaviourally wrong (all-zero thresholds make the `>=` HUD-message gate always pass).
//   Read the 14 little-endian f32 at 0x8206F7B0 and replace them. Index space is
//   EBurnoutSkillType, count 14.
// ----------------------------------------------------------------------------
const f32 BurnoutSkillsManager::KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL
    [BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT] =
{
    // TODO(consolidator): 14 f32 from rodata flt_8206F7B0 -- values UNRECOVERED.
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

// ----------------------------------------------------------------------------
// Construct  @0x824F3B00
//   Initialise the tracker against its owning GUI cache: clear every per-ARC skill
//   tally, sentinel out the network ids and current record holders, and start in the
//   auto-rotate state on the first skill.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::Construct(BrnGui::GuiCache* lpCache)
{
    CGS_ASSERT(lpCache, "lpCache");

    mpCache              = lpCache;
    mfTimeToNextChange   = 0.0f;
    meCurrentSkill       = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_AIR_TIME; // 0
    meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE;                 // 1

    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        maSkillzData[liIndex].Clear();
        maNetworkIds[liIndex] = -1;
    }

    for (s32 liSkill = 0; liSkill < BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT; ++liSkill)
    {
        maeCurrentRecordHolder[liSkill] = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    }
}

// ----------------------------------------------------------------------------
// ResetSkillsData  @0x824F3BB0
//   Drop back to auto-rotating from the first skill (unless a road-rule contest is
//   pinning the display) and arm the next rotation deadline.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::ResetSkillsData()
{
    if (meCurrentShowingType != E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE)
    {
        meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE;
        meCurrentSkill       = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_AIR_TIME;
        mfTimeToNextChange   = mpCache->GetTime() + KF_ROTATION_TIME;
    }
}

// ----------------------------------------------------------------------------
// YouBeatSkill  @0x824F3C10
//   Flash the "new record" panel for the given skill, but only while the skills HUD
//   is active and we are currently auto-rotating or already flashing a record.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::YouBeatSkill(BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkill)
{
    if (mpCache->GetCurrentGameModeType() == KI_SKILLS_ACTIVE_GAME_MODE_TYPE)
    {
        if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE
            || meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_NEW_SCORE)
        {
            meCurrentSkill       = leSkill;
            meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_NEW_SCORE;
            mfTimeToNextChange   = mpCache->GetTime() + KF_NEW_PAUSE_TIME;
        }
    }
}

// ----------------------------------------------------------------------------
// ResetPlayerData  @0x824F3C88
//   A player (active-race-car slot) is leaving: for every skill they currently hold
//   the record on, re-elect the record holder from the remaining active players, then
//   wipe that slot's network id and skill tally.
//
//   Record direction is per-skill: the road-rule TIME skill (index 12) is lowest-wins
//   (FLT_MAX seed, `<` compare, ignoring a zero time); every other skill -- including
//   the road-rule CRASH skill (13) -- is highest-wins (0 seed, `>` compare).
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::ResetPlayerData(EActiveRaceCarIndex leActiveRaceCar)
{
    for (BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkillIter
             = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_START;
         leSkillIter < BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT;
         leSkillIter++)
    {
        if (maeCurrentRecordHolder[leSkillIter] != leActiveRaceCar)
        {
            continue;
        }

        const bool lbLowestWins = (leSkillIter == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12);

        EActiveRaceCarIndex leNewHolder = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        f32                 lfCurrentBest = lbLowestWins ? KF_FLT_MAX : 0.0f;

        for (EActiveRaceCarIndex leCarIter = E_ACTIVE_RACE_CAR_INDEX_0;
             leCarIter < E_ACTIVE_RACE_CAR_INDEX_COUNT;
             leCarIter++)
        {
            // Skip the departing player and any unoccupied slot.
            if (leCarIter == leActiveRaceCar || maNetworkIds[leCarIter] == -1)
            {
                continue;
            }

            if (lbLowestWins)
            {
                // Road-rule TIME: a stored time of 0 means "no run", so it is ignored;
                // otherwise the smallest time takes the record.
                if (maSkillzData[leCarIter].GetBurnoutSkill(BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12) != 0.0f
                    && maSkillzData[leCarIter].GetBurnoutSkill(leSkillIter) < lfCurrentBest)
                {
                    leNewHolder   = leCarIter;
                    lfCurrentBest = maSkillzData[leCarIter].GetBurnoutSkill(leSkillIter);
                }
            }
            else
            {
                if (maSkillzData[leCarIter].GetBurnoutSkill(leSkillIter) > lfCurrentBest)
                {
                    leNewHolder   = leCarIter;
                    lfCurrentBest = maSkillzData[leCarIter].GetBurnoutSkill(leSkillIter);
                }
            }
        }

        maeCurrentRecordHolder[leSkillIter] = leNewHolder;
    }

    maNetworkIds[leActiveRaceCar] = -1;
    maSkillzData[leActiveRaceCar].Clear();
}

// ----------------------------------------------------------------------------
// SelectNextSkill  @0x824F3E58  (private)
//   Advance meCurrentSkill to the next *displayable* skill, wrapping at the end and
//   skipping the two road-rule skills (12/13), which are only shown via the pinned
//   road-rule mode. Must not be called while pinned to road-rule mode.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::SelectNextSkill()
{
    CGS_ASSERT(E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE != meCurrentShowingType,
               "E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE != meCurrentShowingType");

    if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE)
    {
        return;
    }

    while (true)
    {
        meCurrentSkill = static_cast<BrnGameState::BurnoutSkillzData::EBurnoutSkillType>(meCurrentSkill + 1);

        if (meCurrentSkill >= BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT)
        {
            meCurrentSkill = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_AIR_TIME; // wrap to 0
            return;
        }

        // Skip the two road-rule skills during normal rotation.
        if (meCurrentSkill != BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12
            && meCurrentSkill != BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_13)
        {
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Update  @0x824F9E00
//   Per-frame tick: while the skills HUD is active, time-out a record flash back to
//   auto-rotate, and advance the auto-rotation on its dwell timer.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::Update()
{
    CGS_ASSERT(mpCache, "mpCache");

    if (mpCache->GetCurrentGameModeType() == KI_SKILLS_ACTIVE_GAME_MODE_TYPE)
    {
        // A "new record" flash has had its moment -- fall back to auto-rotate.
        if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_NEW_SCORE
            && mpCache->GetTime() >= mfTimeToNextChange)
        {
            meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE;
        }

        // Auto-rotation due -- step to the next skill and re-arm the timer.
        if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE
            && mpCache->GetTime() >= mfTimeToNextChange)
        {
            mfTimeToNextChange = mpCache->GetTime() + KF_ROTATION_TIME;
            SelectNextSkill();
        }
    }
}

// ----------------------------------------------------------------------------
// SelectNext  @0x824F9ED0
//   User paged to the next skill: from any of the auto-rotate / select / new-score
//   states, switch into the manual SELECT state, advance the skill, and pause the
//   rotation timer.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::SelectNext()
{
    if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE
        || meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_NEW_SCORE
        || meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_SELECT)
    {
        meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_SELECT;
        SelectNextSkill();
        mfTimeToNextChange = 0.0f;
    }
}

// ----------------------------------------------------------------------------
// SetRoadRuleMode  @0x824F9F30
//   React to the active road-rule changing: pin the display to the matching road-rule
//   skill while a TIME/CRASH contest is live, or release back to auto-rotate when it
//   ends.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::SetRoadRuleMode(BrnGameState::EActiveRoadRule leRoadRule)
{
    switch (leRoadRule)
    {
        case BrnGameState::E_ACTIVE_ROAD_RULE_NONE:
            // Contest over -- if we were pinned, drop back to auto-rotate.
            if (meCurrentShowingType == E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE)
            {
                meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE;
                SelectNextSkill();
            }
            break;

        case BrnGameState::E_ACTIVE_ROAD_RULE_OFFLINE_TIME:
        case BrnGameState::E_ACTIVE_ROAD_RULE_ONLINE_TIME:
            meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE;
            meCurrentSkill       = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12; // road-rule TIME (12)
            break;

        case BrnGameState::E_ACTIVE_ROAD_RULE_OFFLINE_CRASH:
        case BrnGameState::E_ACTIVE_ROAD_RULE_ONLINE_CRASH:
            meCurrentShowingType = E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE;
            meCurrentSkill       = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_13; // road-rule CRASH (13)
            break;

        default:
            CGS_ASSERT(false, "Invalid road rule mode\n");
            break;
    }
}

// ----------------------------------------------------------------------------
// GetBurnoutSkillForARC  @0x8240EBB0
//   Return the stored best score for (leSkill, leActiveRaceCar): index the per-ARC
//   BurnoutSkillzData tally (X360 stride 56 == sizeof BurnoutSkillzData) and forward
//   to its GetBurnoutSkill. Two leading range guards on the ARC index precede the
//   mulli/add that forms &maSkillzData[leActiveRaceCar].
// ----------------------------------------------------------------------------
f32 BurnoutSkillsManager::GetBurnoutSkillForARC(
    BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkill,
    EActiveRaceCarIndex leActiveRaceCar) const
{
    CGS_ASSERT(leActiveRaceCar >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCar < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return maSkillzData[leActiveRaceCar].GetBurnoutSkill(leSkill);
}

// ----------------------------------------------------------------------------
// SetSkillsData  @0x825118F0
//   Fold one incoming per-frame "new burnout skillz" event (one active-race-car's
//   fresh scores) into the record table. For each of the 14 skills whose new score
//   beats the stored record, re-elect that skill's record holder to this player,
//   fire the "you beat it" HUD flash when the local player set the record, and -- for
//   the displayable skills, when the event asks for it and the score clears the
//   per-skill HUD threshold in a >1-player race -- publish a GuiNewBurnoutHudMessage
//   flash naming the previous and new record holders. Finally latch the player's
//   network id and copy the whole score block into their per-ARC slot.
//
//   Record direction matches ResetPlayerData: the road-rule TIME skill (index 12) is
//   lowest-wins (a stored time of 0 means "no run"); every other skill is highest-wins.
//   Skill index 11 is not processed here (the asm branches straight past it), and the
//   two road-rule skills (12/13) never emit a HUD-message flash.
// ----------------------------------------------------------------------------
void BurnoutSkillsManager::SetSkillsData(EActiveRaceCarIndex leActiveRaceCar,
                                         const GuiNewBurnoutSkillzEvent* lpEvent,
                                         CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput)
{
    if (!lpEvent)
    {
        // No scores this frame == the player left: clear their slot / re-elect records.
        ResetPlayerData(leActiveRaceCar);
        return;
    }

    // The local player's active-race-car index (captured once, as the X360 does).
    const EActiveRaceCarIndex leLocalPlayer =
        static_cast<EActiveRaceCarIndex>(mpCache->GetPlayerActiveRaceCarIndex());

    for (BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkill
             = BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_START;
         ;
         )
    {
        CGS_ASSERT(leSkill >= BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_START,
                   "leSkillType >= E_BURNOUT_SKILL_START");
        CGS_ASSERT(leSkill < BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
                   "leSkillType < E_BURNOUT_SKILL_COUNT");

        // Read the event's fresh score for this skill (public accessor; the X360 reads
        // the mafBurnoutSkilz[] field directly, semantically identical to GetBurnoutSkill).
        const f32 lfNewScore = lpEvent->mSkillzData.GetBurnoutSkill(leSkill);

        // Skill index 11 is never folded in (the asm jumps straight to the increment).
        if (leSkill != BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_ROAD_RULE_CRASH)
        {
            EActiveRaceCarIndex* lpRecordHolder = nullptr;
            bool                 lbBeaten       = false;

            if (leSkill == BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12)
            {
                // Road-rule TIME: lowest-wins, and a stored time of 0 is "no run".
                if (lfNewScore > 0.0f)
                {
                    lpRecordHolder = &maeCurrentRecordHolder[
                        BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12];
                    const EActiveRaceCarIndex leHolder = *lpRecordHolder;

                    lbBeaten =
                        (leHolder == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                        || (maSkillzData[leHolder].GetBurnoutSkill(
                                BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12) == 0.0f)
                        || (maSkillzData[leHolder].GetBurnoutSkill(
                                BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12) > lfNewScore);
                }
            }
            else if (lfNewScore > 0.0f)
            {
                // Every other skill: highest-wins.
                lpRecordHolder = &maeCurrentRecordHolder[leSkill];
                const EActiveRaceCarIndex leHolder = *lpRecordHolder;

                lbBeaten =
                    (leHolder == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    || (maSkillzData[leHolder].GetBurnoutSkill(leSkill) < lfNewScore);
            }

            if (lbBeaten)
            {
                // The local player just set this record -> flash the "you beat it" panel.
                if (leActiveRaceCar == leLocalPlayer)
                {
                    YouBeatSkill(leSkill);
                }

                // The two road-rule skills never emit a HUD-message flash.
                if (leSkill != BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_12
                    && leSkill != BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_EXTRA_13)
                {
                    CGS_ASSERT(mpCache, "mpCache");

                    if (lpEvent->mbUpdateHUDMessage
                        && mpCache->GetNumActivePlayers() > 1
                        && lfNewScore >= KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL[leSkill])
                    {
                        const EActiveRaceCarIndex lePreviousOwner = *lpRecordHolder;

                        GuiNewBurnoutHudMessageEvent lMessage;
                        lMessage.mRoadID  = 0;
                        lMessage.meSkill  = leSkill;

                        if (leActiveRaceCar == leLocalPlayer)
                        {
                            // The local player took the record for themselves.
                            lMessage.meMessageType =
                                GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_YOU_GOT;
                            lMessage.meNewOwner      = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                            lMessage.mePreviousOwner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                        }
                        else if (leActiveRaceCar == lePreviousOwner
                                 || lePreviousOwner == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                        {
                            // A rival extended (or first-claimed) their own record.
                            lMessage.meMessageType =
                                GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_GOT;
                            lMessage.meNewOwner      = leActiveRaceCar;
                            lMessage.mePreviousOwner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                        }
                        else if (lePreviousOwner == leLocalPlayer)
                        {
                            // A rival beat the local player's record.
                            lMessage.meMessageType =
                                GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YOUR;
                            lMessage.meNewOwner      = leActiveRaceCar;
                            lMessage.mePreviousOwner = E_ACTIVE_RACE_CAR_INDEX_INVALID;
                        }
                        else
                        {
                            // A rival beat another rival's record.
                            lMessage.meMessageType =
                                GuiNewBurnoutHudMessageEvent::E_BURNOUT_SKILLZ_MESSAGE_TYPE_X_BEAT_YS;
                            lMessage.meNewOwner      = leActiveRaceCar;
                            lMessage.mePreviousOwner = lePreviousOwner;
                        }

                        lpOutput->AddGuiOutEvent(lMessage);
                    }
                }

                // This player now holds the record for this skill.
                *lpRecordHolder = leActiveRaceCar;
            }
        }

        leSkill++;
        CGS_ASSERT(static_cast<int>(leSkill)
                       <= BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT,
                   "leEnumIndex <= BurnoutSkillzData::E_BURNOUT_SKILL_COUNT");
        if (leSkill >= BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT)
        {
            break;
        }
    }

    // Latch the origin player's network id and copy their whole score block into slot.
    maNetworkIds[leActiveRaceCar] = lpEvent->mNetworkPlayerID;
    maSkillzData[leActiveRaceCar] = lpEvent->mSkillzData;
}

} // namespace BrnGui
