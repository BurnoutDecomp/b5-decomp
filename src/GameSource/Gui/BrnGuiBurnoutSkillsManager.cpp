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
// NOTE: KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL (the per-skill minimum score gating the
// "you beat a record" HUD flash) is read only by SetSkillsData @0x825118F0, which the
// ledger assigns to a separate TU; its 14 values live there, so it is intentionally
// left declared-only here (no odr-use in this TU under the cl /c gate).

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

} // namespace BrnGui
