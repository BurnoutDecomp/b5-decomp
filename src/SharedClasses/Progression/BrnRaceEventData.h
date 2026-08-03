#ifndef BRN_RACE_EVENT_DATA_H
#define BRN_RACE_EVENT_DATA_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // CgsID (EventJunction / RaceEventData id accessors)

// =============================================================================
// BrnRaceEventData.h  (OWNING HEADER for the BrnProgression race-event leaf types)
//
// DWARF home: SharedClasses/Progression/BrnRaceEventData.h. The canonical file is the
// home of several sibling types (EventRacerPersonality, EventStartGridSlot, CheckpointData,
// RaceEventData, EventJunction). This is a MINIMAL OWNING SLICE: a sibling type is carved
// here only once a caller needs it, with its real layout + the accessors the X360 attests.
// Complete so far: EventJunction, EventRacerPersonality, CheckpointData (full DWARF layout),
// RaceEventData (partial -- named members + explicit padding). EventStartGridSlot is still
// absent. When another type is needed it grows this single-owner header (do not fork).
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
// BrnProgression::CheckpointData -- one entry of a race event's checkpoint table.
//
// SCOPE NOTE: the DWARF gives this as a NAMESPACE-SCOPE sibling of RaceEventData
// (`struct BrnProgression::CheckpointData`, DWARF BrnRaceEventData.h:246), exactly like
// EventJunction / EventRacerPersonality / EventStartGridSlot -- and exactly as this file's
// own banner already describes it. An earlier slice of this header declared it nested inside
// RaceEventData (`RaceEventData::CheckpointData`); that nesting was not DWARF-backed and is
// corrected here. RaceEventData's mpaCheckpoints / GetCheckpointData / SetCheckpointData all
// name it unqualified in the DWARF, which resolves to this namespace-scope type.
//
// Do not confuse with BrnGameState::CheckpointData (GameSource/GameState/BrnCheckpointData.h),
// which is an unrelated 44-byte record.
//
// LAYOUT: 40 bytes (0x28), all scalar, no pointers -- so the host layout matches the console
// byte-for-byte. The 0x28 stride is independently PROVEN by the X360:
// RaceEventData::GetCheckpointData @0x8230F858 computes `((i + i*4) << 3) + *(this+0x18)`
// == i*40 + base.
// ----------------------------------------------------------------------------
struct CheckpointData
{
    // DWARF BrnRaceEventData.h:248 (emitted by the dumper both as this in-class constant and
    // as its namespace-scope out-of-line definition). Sizes mauBlockSectionIds.
    static const s32 KI_MAX_BLOCK_SECTION_COUNT = 8;

    // INLINE ON PURPOSE (not a stub): no standalone X360 symbol exists for this accessor --
    // callers in *other* TUs inline the load directly, which is only possible if the original
    // header defined it inline. Proof: BrnGui::CrashNavMap::CalculateEventZoomFactor
    // @0x824BF5E4 does `lwzx r4, r30, r11` (r11 == mpaCheckpoints, r30 stepping 0x28 per pass)
    // and feeds the loaded word straight to GuiCache::GetLandmarkInfoFromID -- no call. Same
    // shape in BrnGui::PreRaceFlyByState::CalculateZoomFactor @0x824BE8F0: `lwz r11,0x18(r29)`
    // then `lwzx r4, r30, r11` @0x824BEA30 with `addi r30, r30, 0x28` @0x824BEA44 as the stride.
    // DWARF BrnRaceEventData.h:265.
    u32 GetLandmarkId() const { return muLandmarkId; }

    // DWARF BrnRaceEventData.h:268. Inline for the same reason (no standalone X360 symbol).
    s32 GetBlockSectionCount() const { return miBlockSectionCount; }

    // ---- Remaining DWARF-attested API (declaration-only) ----
    // FLAG: none of these has a standalone X360 symbol either (the whole record is
    // header-inline on the console) and none has a caller in the tree yet, so they are left
    // DECLARED ONLY rather than given fabricated bodies. Whichever TU first needs one bodies
    // it -- in this header if the asm proves it inline, in BrnRaceEventData.cpp otherwise.
    void Construct(u32 luLandmarkId);              // DWARF :252
    void FixDown();                                // DWARF :255
    void FixUp();                                  // DWARF :258
    void AddBlockSection(u32 luBlockSectionId);    // DWARF :262
    u32  GetBlockSectionId(s32 liIndex) const;     // DWARF :272

private:
    u32 muLandmarkId;                                     // 0x00 (DWARF :275)
    s32 miBlockSectionCount;                              // 0x04 (DWARF :276)
    u32 mauBlockSectionIds[KI_MAX_BLOCK_SECTION_COUNT];   // 0x08..0x27 (DWARF :277)
};

// The 0x28 element stride RaceEventData::GetCheckpointData hard-codes. Pointer-free scalar
// run, so this is a host-safe check of the console layout rather than a baked console offset.
static_assert(sizeof(CheckpointData) == 40, "BrnProgression::CheckpointData is a 40-byte (0x28) record");

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
    // The per-checkpoint record this event's table is made of is BrnProgression::CheckpointData,
    // defined above at namespace scope (DWARF :246) -- it is NOT a nested type. It is now a
    // COMPLETE type, so GetCheckpointData's result can be dereferenced.

    // Number of rank thresholds (the rank-score / rank-time tables are sized to this; the X360
    // bounds-checks the rank arg against 6 in GetRankScore @0x823543D0 / GetRankTime @0x8230F748).
    static const u32 KU_NUM_RANKS = 6;

    // Progression race-event mode classification. DWARF-attested
    // (references/DecFIGS/dwarfdump/SharedClasses/Progression/BrnRaceEventData.h).
    // BrnGui::EventPanel::ConvertLocalEventDefToProgressionEventDef maps its local EEventType
    // onto this; E_MODE_PURSUIT=5 is retained even though panel-side mapping collapses
    // E_EVENT_TYPE_ALL to E_MODE_COUNT.
    enum EModeType
    {
        E_MODE_INVALID       = -1,
        E_MODE_RACE          = 0,
        E_MODE_ROAD_RAGE     = 1,
        E_MODE_STUNT_ATTACK  = 2,
        E_MODE_SURVIVOR      = 3,
        E_MODE_BURNING_ROUTE = 4,
        E_MODE_PURSUIT       = 5,
        E_MODE_COUNT         = 6,
    };

    // X360 0x8230F808. Returns &mpaCheckpoints[liCheckpointIndex] (asserts 0 <= index <
    // miCheckpointCount, BrnRaceEventData.h:953). 40-byte stride.
    const CheckpointData* GetCheckpointData(s32 liCheckpointIndex) const;

    // The live checkpoint count -- the bound GetCheckpointData asserts against. DWARF
    // BrnRaceEventData.h:368 (`int32_t GetCheckpointCount() const;`).
    // INLINE ON PURPOSE (not a stub): there is no standalone X360 symbol for it, yet callers in
    // OTHER TUs load the word directly -- CrashNavMap::CalculateEventZoomFactor @0x824BF590
    // (`lwz r11, 0x1C(r29)`) and PreRaceFlyByState::CalculateZoomFactor @0x824BE9DC/0x824BEA78 both
    // re-read +0x1C every loop pass with no call. That is only possible if the original header
    // defined the accessor inline, so it is reconstructed inline here.
    s32 GetCheckpointCount() const { return miCheckpointCount; }

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
