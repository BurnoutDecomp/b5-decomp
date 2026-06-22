#ifndef BRN_RACE_EVENT_DATA_H
#define BRN_RACE_EVENT_DATA_H

#include "types.hpp"

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

// Forward decl for the event-junction event pointers (complete RaceEventData lives in its own
// owning slice; the junction only stores it by pointer).
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

}

#endif // BRN_RACE_EVENT_DATA_H
