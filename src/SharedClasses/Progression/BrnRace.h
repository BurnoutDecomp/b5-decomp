#ifndef BRN_RACE_H
#define BRN_RACE_H

#include "types.hpp"
#include "SharedClasses/Progression/BrnBaseRace.h"   // BrnProgression::BaseRace (base class)
#include "GameSource/GameState/BrnGameStateTypes.h"  // BrnGameState::LandmarkIndex (sizeof 2)

// =============================================================================
// BrnRace.h  (OWNING HEADER for BrnProgression::Race)
//
// DWARF home: SharedClasses/Progression/BrnRace.h. Race extends BaseRace with the ordered
// list of landmark indices that define the route plus the per-landmark AI section index.
// This is a MINIMAL OWNING SLICE: the full DWARF data members (so the byte layout the X360
// Construct writes lines up by name) plus only the methods this batch bodies. The rest of
// the attested API (Get/Set/Add/RemoveLandmark, ...) is declared-only; bodies land with
// later TUs. Single owner -- grow here, do not fork.
//
// LAYOUT is X360-faithful (DWARF BrnRace.h:44). BaseRace occupies 0x00..0x2F; the derived
// members start at 0x30: maLandmarkIndices[16] (0x30, LandmarkIndex stride 2),
// mauAiSectionIndices[16] (0x50) and muNumLandmarks (0x70).
// =============================================================================

namespace BrnProgression
{

// LandmarkIndex's owning home is GameSource/GameState/BrnGameStateTypes.h (BrnGameState scope).
typedef BrnGameState::LandmarkIndex LandmarkIndex;

struct Race : public BaseRace
{
    // X360 0x826767D8. Brings the race up to its default state: empty name, default id,
    // cleared flags/rank/laps (the inlined BaseRace bring-up) and every landmark slot set
    // to the invalid-landmark sentinel with the landmark count zeroed.
    void Construct();

    // ---- Remaining X360-attested API (bodies in their own TUs; declaration-only here) ----
    u8                   GetNumLandmarks() const;
    LandmarkIndex        GetLandmarkIndex(u8 luIndex) const;
    const LandmarkIndex* GetLandmarkIndexArray() const;
    void                 SetLandmarkIndex(u8 luIndex, LandmarkIndex lIndex);
    LandmarkIndex        GetStartLandmarkIndex() const;
    LandmarkIndex        GetFinishLandmarkIndex() const;
    void                 ClearLandmarks();
    void                 AddLandmark(LandmarkIndex lIndex, u16 luAiSectionIndex);
    void                 RemoveLastLandmark();
    const u16*           GetAiSectionIndexArray() const;

    // Number of landmark slots (matches the DWARF array extents).
    static const s32 KI_MAX_LANDMARKS = 16;

private:
    LandmarkIndex maLandmarkIndices[KI_MAX_LANDMARKS];   // 0x30 (DWARF BrnRace.h:100)
    u16           mauAiSectionIndices[KI_MAX_LANDMARKS]; // 0x50 (DWARF :103)
    u8            muNumLandmarks;                         // 0x70 (DWARF :105)
};

}

#endif // BRN_RACE_H
