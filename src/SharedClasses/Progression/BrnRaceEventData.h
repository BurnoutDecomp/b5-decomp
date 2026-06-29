#ifndef BRN_RACE_EVENT_DATA_H
#define BRN_RACE_EVENT_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (EventJunction / RaceEventData id accessors)

// =============================================================================
// BrnRaceEventData.h  (OWNING HEADER for the BrnProgression race-event leaf types)
//
// DWARF home: SharedClasses/Progression/BrnRaceEventData.h. The canonical file is the
// home of several sibling types (EventRacerPersonality, EventStartGridSlot, CheckpointData,
// RaceEventData, EventJunction). This is a MINIMAL OWNING SLICE: only the type whose
// body lands in this batch -- EventRacerPersonality -- is defined here with its real
// layout + the accessors the X360 attests. The other sibling types are reconstructed in
// their own TUs; when one is bodied it grows this single-owner header (do not fork).
//
// LAYOUT is X360-faithful and taken from the DecFIGS DWARF for this exact path. The
// EventRacerPersonality record is four contiguous f32 tuning values (proven by
// Construct, which stores 0.0f into the four float slots at byte offsets 0/4/8/0xC).
// =============================================================================

namespace BrnProgression
{

// Forward decl for the event-junction event pointers (complete RaceEventData is defined below;
// the junction only stores it by pointer).
struct RaceEventData;

// Per-junction record that links an offline event to an online event. DWARF BrnRaceEventData.h:
// EventJunction. 16 bytes. ProgressionData's relocation walks the junction array and rebases the
// two event pointers, so they are named here.
struct EventJunction
{
    u32 GetID() const { return muID; }
    const RaceEventData* GetOfflineEvent() const { return mpOfflineEvent; }
    const RaceEventData* GetOnlineEvent() const { return mpOnlineEvent; }
    s32 GetShotGroup() const { return miShotGroup; }

    // ---- Remaining attested API (bodies in their own TUs; declaration-only) ----
    void Construct(u32 luID, const RaceEventData* lpOfflineEvent, const RaceEventData* lpOnlineEvent, s32 liShotGroup);

    // ADDITIVE GROW (declare-only; bodies in the EventJunction/ProgressionData TU).
    // DriveThruManager::UnlockCarChallengeForCar reads the junction's event id (to find the matching
    // ProfileEvent) and the junction's own id (for the SendJunctionPlayerIsAt payload). The X360
    // junction-id read is a CgsID-width word; declared returning CgsID. FLAG: these widen the existing
    // u32 GetID() read; bodies resolve the exact source word in their own TU.
    CgsID GetEventId() const;
    CgsID GetId() const;

    u32                  muID;            // 0x00 (DWARF :80)
    const RaceEventData* mpOfflineEvent;  // 0x04 (DWARF :82)
    const RaceEventData* mpOnlineEvent;   // 0x08 (DWARF :83)
    s32                  miShotGroup;     // 0x0C (DWARF :85)
};

// Per-racer AI tuning record carried by the start-grid setup. DWARF BrnRaceEventData.h:99.
// Four f32 fields: an aggression range, a skill scalar and a speed scalar.
struct EventRacerPersonality
{
    // X360 0x826767B8. Zeroes all four tuning values.
    void Construct();

    // ---- Remaining X360-attested API (bodies in their own TUs; declaration-only here) ----
    void FixDown();
    void FixUp();
    f32  GetMinAggression() const;
    f32  GetMaxAggression() const;
    void SetAggression(f32 lfMin, f32 lfMax);
    f32  GetSkill() const;
    void SetSkill(f32 lfSkill);
    f32  GetSpeed() const;
    void SetSpeed(f32 lfSpeed);

private:
    f32 mfMinAggression;   // 0x00 (DWARF BrnRaceEventData.h:137)
    f32 mfMaxAggression;   // 0x04 (DWARF :138)
    f32 mfSkill;           // 0x08 (DWARF :139)
    f32 mfSpeed;           // 0x0C (DWARF :140)
};

// ----------------------------------------------------------------------------
// BrnProgression::RaceEventData -- one serialised offline/online race event record. DWARF home
// SharedClasses/Progression/BrnRaceEventData.h. The X360 proves a 248-byte (0xF8) record (see
// ProgressionData::FixDown @0x8267F220, which strides the event table by 248 and rebases the
// checkpoint pointer at +0x18). This is a MINIMAL OWNING SLICE: only the members the accessors
// bodied in this batch touch are named at their X360-proven offsets; the remaining bytes are
// explicit named padding so the touched members keep their exact offsets WITHOUT fabricating
// member names for the not-yet-recovered fields. Grow in place (replace a pad with the real
// DWARF member) when a future caller needs it -- do not fork a second definition.
// ----------------------------------------------------------------------------
struct RaceEventData
{
    // Per-checkpoint record carried by the event (40-byte / 0x28 stride; PROVEN by
    // GetCheckpointData's `return 40 * index + mpaCheckpoints`). Distinct from
    // BrnGameState::CheckpointData (44-byte). GetCheckpointData only returns a pointer, so the
    // element's internal layout is not needed here -- declared-only/incomplete.
    struct CheckpointData;

    // Number of rank thresholds (the rank-score / rank-time tables are sized to this; the X360
    // bounds-checks the rank arg against 6 in GetRankScore @0x823543D0 / GetRankTime @0x8230F748).
    static const u32 KU_NUM_RANKS = 6;

    // X360 0x8230F808. Returns &mpaCheckpoints[liCheckpointIndex] (asserts 0 <= index <
    // miCheckpointCount, BrnRaceEventData.h:953). 40-byte stride.
    const CheckpointData* GetCheckpointData(s32 liCheckpointIndex) const;

    // X360 0x823543D0. Returns the target score for rank luRank (asserts luRank < 6,
    // BrnRaceEventData.h:883). maiRankScores lives at +0x28.
    s32 GetRankScore(u32 luRank) const;

    // X360 0x8230F748. Returns the target time (seconds, f32) for rank luRank (asserts luRank < 6,
    // BrnRaceEventData.h:898). mafRankTimes lives at +0x40.
    f32 GetRankTime(u32 luRank) const;

    // ---- Remaining X360-attested API (bodies + the backing members land in their own TUs) ----
    // The number of rivals on the start grid / added during the event. RaceMode::Start reads both.
    // Declared-only here (their backing fields are not in this minimal slice -- a future TU grows
    // this single owner with the proven offsets). Previously a competing 2-method stub
    // BrnProgression::RaceEventData lived in BrnGameModeParams.h; that stub is retired and that
    // header now defers to this single owner (ODR).
    u8 GetStartRivalCount() const;
    u8 GetAddRivalCount() const;

    // ADDITIVE GROW (declare-only; body in the RaceEventData TU).
    // The car id this event unlocks (X360 word +0x14 -- the value DriveThruManager::
    // UnlockCarChallengeForCar matches each junction against the repaired car id). Declare-only.
    CgsID GetUnlockCarId() const;

    // ADDITIVE GROW (declare-only; bodies in the RaceEventData TU) -- BrnSatNavRenderer TU.
    // The sat-nav icon renderer reads three record fields when caching an on-map event icon:
    //   * GetEventTypeByte  -- X360 byte +0xEC; the renderer indexes a sat-nav-icon lookup
    //     table with it to pick the icon's UV row (E_SATNAVICON_EVENT_*).
    //   * GetIconFrameBase  -- X360 byte +0xED; the icon's animation base frame (renderer
    //     adds 6 to it for the mini-icon variant).
    //   * GetEventInstanceId -- X360 doubleword +0x10; the event-instance id the renderer
    //     compares against the cache's "current" event to special-case the active event.
    // Declare-only: the backing fields live past this minimal slice (a future TU grows the
    // single owner with the proven offsets). Returned by accessor so the renderer stays off
    // raw +0xEC/+0xED/+0x10 casts.
    u8    GetEventTypeByte() const;
    u8    GetIconFrameBase() const;
    u64   GetEventInstanceId() const;

private:
    u8                    maPad_00[0x18];                 // 0x00..0x17 (id / car id / leading scalars -- not in this slice)
    const CheckpointData* mpaCheckpoints;                 // 0x18  checkpoint table base (X360 GetCheckpointData base)
    s32                   miCheckpointCount;              // 0x1C  live checkpoint count
    u8                    maPad_20[0x08];                 // 0x20..0x27 (not in this slice)
    s32                   maiRankScores[KU_NUM_RANKS];    // 0x28  rank target scores (6 * 4 == 0x18 -> 0x40)
    f32                   mafRankTimes[KU_NUM_RANKS];     // 0x40  rank target times  (6 * 4 == 0x18 -> 0x58)
    u8                    maPad_58[0xA0];                 // 0x58..0xF7 (remaining record -- not in this slice; sizeof == 0xF8)
};

}

#endif // BRN_RACE_EVENT_DATA_H
