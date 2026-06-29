#pragma once

// ============================================================================
// BrnGui::BurnoutSkillsManager
//
// The HUD "burnout skillz" record tracker. It owns, per active race-car, the
// best-so-far score for each of the 14 burnout skills (air time, barrel roll,
// boost chain, ... plus the two road-rule TIME/CRASH records), tracks which
// race-car currently holds each record, and drives the HUD skill-of-the-moment
// rotation: it auto-rotates through the skills, flashes a "new record" panel
// when a record changes hands, and switches to a fixed road-rule display while
// a road-rule contest is active.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (authoritative for behaviour AND
// layout offsets) gated against the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/Gui/BrnGuiBurnoutSkillsManager.h,
// which supplies the declaration shape: member names/types and the showing-type
// enum). All nine X360-attested methods of the class are reconstructed in the
// sibling .cpp.
//
// LAYOUT (X360 Construct @0x824F3B00 / ResetPlayerData @0x824F3C88 store offsets
// are authoritative). sizeof == 0x228 (552 bytes):
//   +0x000  BurnoutSkillzData maSkillzData[8]      (8 * 0x38 == 0x1C0; per-ARC scores)
//   +0x1C0  s32               maNetworkIds[8]      (8 * 4 == 0x20; -1 sentinels)
//   +0x1E0  EBurnoutSkillType meCurrentSkill       (the skill the HUD is showing)
//   +0x1E4  EBurnoutSkillsShowingType meCurrentShowingType
//   +0x1E8  GuiCache*         mpCache
//   +0x1EC  f32               mfTimeToNextChange   (auto-rotate / pause deadline)
//   +0x1F0  EActiveRaceCarIndex maeCurrentRecordHolder[14] (-1 sentinels; per-skill)
// ============================================================================

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"          // EActiveRaceCarIndex
#include "GameSource/GameState/BrnGameStateTypes.h"// BrnGameState::EActiveRoadRule
#include "GameSource/GameState/ModeManager/Scoring/BrnBurnoutSkillzData.h" // BurnoutSkillzData (by value) + EBurnoutSkillType
#include "GameSource/Gui/BrnGuiCache.h"            // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"    // GuiNewBurnoutSkillzEvent, GuiNewBurnoutHudMessageEvent
#include "GameShared/GameClasses/Gui/CgsGuiModuleIO.h" // CgsGui::CgsGuiModuleIO::OutputBuffer

namespace BrnGui
{

struct BurnoutSkillsManager
{
    // BrnGuiBurnoutSkillsManager.h:106 (DWARF) -- which HUD state the skill display is in.
    enum EBurnoutSkillsShowingType
    {
        E_BURNOUT_SKILLS_SHOWING_TYPE_NONE             = 0,
        E_BURNOUT_SKILLS_SHOWING_TYPE_AUTO_ROTATE      = 1,
        E_BURNOUT_SKILLS_SHOWING_TYPE_SELECT           = 2,
        E_BURNOUT_SKILLS_SHOWING_TYPE_NEW_SCORE        = 3,
        E_BURNOUT_SKILLS_SHOWING_TYPE_ROAD_RULE_ACTIVE = 4,
        E_BURNOUT_SKILLS_SHOWING_TYPE_COUNT            = 5,
    };

    // ---- public interface (DWARF :62-101; all X360-attested) -----------------------
    void Construct(BrnGui::GuiCache* lpCache);                       // @0x824F3B00
    void Update();                                                   // @0x824F9E00
    void ResetSkillsData();                                          // @0x824F3BB0
    void SelectNext();                                              // @0x824F9ED0
    void SetSkillsData(EActiveRaceCarIndex leActiveRaceCar,
                       const GuiNewBurnoutSkillzEvent* lpEvent,
                       CgsGui::CgsGuiModuleIO::OutputBuffer* lpOutput); // @0x825118F0

    BrnGameState::BurnoutSkillzData::EBurnoutSkillType GetCurrentSkill() const; // DWARF :85

    // @0x8240EBB0 -- the stored best score for (leSkill, leActiveRaceCar). Tail-calls
    // BurnoutSkillzData::GetBurnoutSkill on maSkillzData[leActiveRaceCar]; returns its f32.
    f32 GetBurnoutSkillForARC(BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkill,
                              EActiveRaceCarIndex leActiveRaceCar) const;

    void ResetPlayerData(EActiveRaceCarIndex leActiveRaceCar);      // @0x824F3C88
    void SetRoadRuleMode(BrnGameState::EActiveRoadRule leRoadRule); // @0x824F9F30

private:
    void YouBeatSkill(BrnGameState::BurnoutSkillzData::EBurnoutSkillType leSkill); // @0x824F3C10
    void SelectNextSkill();                                                        // @0x824F3E58

    // ---- constants (DWARF :118-121) ------------------------------------------------
    // Per-skill minimum score before a freshly-beaten record fires the HUD flash (X360
    // SetSkillsData: lfsx f0, leSkill*4, &table; compared `>=` against the new score).
    // Indexed by EBurnoutSkillType, count == 14.
    static const f32 KAF_MINIMUM_HUD_MESSAGE_SKILLS_LEVEL[BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT];
    // Auto-rotate dwell (ResetSkillsData / Update set mfTimeToNextChange = GetTime() + this).
    static const f32 KF_ROTATION_TIME;
    // New-record flash dwell (YouBeatSkill sets mfTimeToNextChange = GetTime() + this).
    static const f32 KF_NEW_PAUSE_TIME;

    // The current-game-mode-type value (GuiCache::GetCurrentGameModeType) that gates the
    // skills HUD: Update / YouBeatSkill / SetSkillsData only act when the active mode type
    // is this. X360 baked literal 15 (== the road-rule / open-world skills mode).
    static const s32 KI_SKILLS_ACTIVE_GAME_MODE_TYPE = 15;

    // ---- data (X360 store-offset authoritative) ------------------------------------
    BrnGameState::BurnoutSkillzData maSkillzData[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x000 per-ARC best scores
    s32                             maNetworkIds[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // +0x1C0 (RoadRulesRecvData::NetworkPlayerID; -1 sentinel)
    BrnGameState::BurnoutSkillzData::EBurnoutSkillType meCurrentSkill;           // +0x1E0
    EBurnoutSkillsShowingType       meCurrentShowingType;                        // +0x1E4
    BrnGui::GuiCache*               mpCache;                                     // +0x1E8
    f32                             mfTimeToNextChange;                          // +0x1EC
    // GROWN vs DWARF (FLAG): DWARF declares maeCurrentRecordHolder[12], but the X360
    // Construct fill loop runs 14 iterations (li r10,0xE) and ResetPlayerData/SetSkillsData
    // index it for skill in [0,14) (e.g. maeCurrentRecordHolder[12] @0x220 == the road-rule
    // TIME record). The asm overrides the DWARF, so the array is [14] to match the 14-skill
    // count. -1 == no current record holder.
    EActiveRaceCarIndex             maeCurrentRecordHolder[BrnGameState::BurnoutSkillzData::E_BURNOUT_SKILL_COUNT]; // +0x1F0
};

} // namespace BrnGui
