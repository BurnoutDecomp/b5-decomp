#pragma once

// Shared enums for the physics-side stunt/offence detector (BrnPhysics::StuntOffencesManager).
// Verbatim from the DecFIGS DWARF (BrnStuntOffencesManagerShared.h:35/48/62). These are the
// per-car in-air state and the in-progress / completed stunt-action BITFIELDS the detector and
// its GameState consumer both read; the values are bit flags (OR-combined), not ordinals.

namespace BrnPhysics
{
    // Per-car in-air status flags (muCurrentRaceCarState). JUST_* are one-frame edge flags;
    // IN_THE_AIR_NOW rolls into IN_THE_AIR_LAST_FRAME at the end of each Update.
    enum ECurrentCarState
    {
        E_CURRENT_CAR_STATE_CLEAR               = 0,
        E_CURRENT_CAR_STATE_JUST_TAKEN_OFF      = 1,
        E_CURRENT_CAR_STATE_JUST_LANDED         = 2,
        E_CURRENT_CAR_STATE_IN_THE_AIR_NOW      = 4,
        E_CURRENT_CAR_STATE_IN_THE_AIR_LAST_FRAME = 8,
        E_CURRENT_CAR_STATE_LANDING             = 16,
        E_CURRENT_CAR_STATE_CAR_HAS_BEEN_RESET  = 32,
    };

    // Live (this-frame) stunt flags (muStuntActionInProgress).
    enum EStuntActionInProgress
    {
        E_STUNT_ACTION_IN_PROGRESS_CLEAR          = 0,
        E_STUNT_ACTION_IN_PROGRESS_BARREL_ROLL    = 1,
        E_STUNT_ACTION_IN_PROGRESS_AIR_SPIN       = 2,
        E_STUNT_ACTION_IN_PROGRESS_HANDBREAK_TURN = 4,
        E_STUNT_ACTION_IN_PROGRESS_DRIFT          = 8,
        E_STUNT_ACTION_IN_PROGRESS_DRIFT_DISTANCE = 16,
        E_STUNT_ACTION_IN_PROGRESS_IN_AIR         = 32,
        E_STUNT_ACTION_IN_PROGRESS_JUMP_DISTANCE  = 64,
        // FLAG: bit 0x80 (convoy in-progress) is OR'd by CheckForConvoy but is NOT listed in the
        // DWARF enum; it shares this word. Left undeclared here -- the .cpp uses the literal 0x80.
    };

    // Completed (one-shot) stunt flags (muStuntActionComplete).
    enum EStuntActionComplete
    {
        E_STUNT_ACTION_COMPLETE_CLEAR              = 0,
        E_STUNT_ACTION_COMPLETE_BARREL_ROLL        = 1,
        E_STUNT_ACTION_COMPLETE_AIR_SPIN           = 2,
        E_STUNT_ACTION_COMPLETE_HANDBREAK_TURN     = 4,
        E_STUNT_ACTION_COMPLETE_CLEANLANDING       = 8,
        E_STUNT_ACTION_COMPLETE_SUCCESSFUL_LANDING = 16,
        E_STUNT_ACTION_COMPLETE_DRIFT              = 32,
        E_STUNT_ACTION_COMPLETE_DRIFT_DISTANCE     = 64,
        E_STUNT_ACTION_COMPLETE_FAILED_DRIFT       = 128,
        E_STUNT_ACTION_COMPLETE_AIR                = 256,
        E_STUNT_ACTION_COMPLETE_JUMP_DISTANCE      = 512,
        // FLAG: bits 0x400 (convoy complete) + 0x800 (air-spin training) are OR'd by CheckForConvoy /
        // CheckForRollsAndSpins but are NOT in the DWARF enum; they share this word. The .cpp uses the
        // literals 0x400 / 0x800.
    };
}
