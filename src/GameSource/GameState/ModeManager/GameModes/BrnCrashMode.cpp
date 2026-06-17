#include "GameSource/GameState/ModeManager/GameModes/BrnCrashMode.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// X360: BrnGameState::CrashMode::SendEvent (0x82330A58). Maps an inbound EGameModeEvent to a
// state transition. ABORT -> Quit(5); RESTART -> Intro(1); otherwise the per-state flow
// Intro(1)->InProgress(2)->Outro(3)->Results(4)->Quit(5). Showtime has no countdown phase, so any
// other current state is a programmer error (the X360-baked assert). SetCurrentState returns void;
// the pseudocode's result/return-result are register artifacts and are dropped. State ids are
// GameStateModuleIO::EGameModeState values passed as raw s32 to the base SetCurrentState.
void CrashMode::SendEvent(EGameModeEvent leEvent)
{
    if (leEvent == E_GME_ABORT)
    {
        SetCurrentState(5);   // -> Quit
        return;
    }
    if (leEvent == E_GME_RESTART)
    {
        SetCurrentState(1);   // -> Intro
        return;
    }

    switch (meCurrentState)
    {
        case 1:   // Intro
            if (leEvent == E_GME_NEXT || leEvent == E_GME_USER_ACCEPT)
            {
                SetCurrentState(2);   // -> InProgress
            }
            break;
        case 2:   // InProgress
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(3);   // -> Outro
            }
            break;
        case 3:   // Outro
            if (leEvent == E_GME_NEXT)
            {
                SetCurrentState(4);   // -> Results
            }
            break;
        case 4:   // Results
            if (leEvent == E_GME_USER_ACCEPT || leEvent == E_GME_NEXT)
            {
                SetCurrentState(5);   // -> Quit
                return;
            }
            break;
        case 5:   // Quit -- terminal
            return;
        default:
            CGS_ASSERT(false, "Should not be in this state in Showtime mode!");
            break;
    }
}

// X360: BrnGameState::CrashMode::Start (0x82322210).
//
// Sets up the mutable GameModeParams for the offline Crash/Showtime mode. The Hex-Rays output
// operates on the OLD GameModeParams layout (`GameModeParamsOld`, raw byte pokes); this
// reconstruction de-inlines those into the committed named members (AGENTS.md no-raw-offset rule).
// The first (StartGameModeParams*) and third (ScoringSystem*) DWARF params are unused by the body.
//
// The X360 asm composes the crash flag mask in r12 -- `li r12,1; <data-base 0x82000000 in the high
// half>; oris/ori ...,0x1042,0x139E` -- and ORs the low word into muFlags. As in RaceMode::Start,
// the high-dword 0x82000000 is the .data float-constant base pointer used by the adjacent
// mfTrafficDensityScale load, NOT a flag bit; the authoritative flag OR is the low-word mask
// 0x1042139E, which decomposes EXACTLY (verified) to the 11 named KU_FLAG_* bits below.
void CrashMode::Start(const StartGameModeParams* /*lpStartGameModeParams*/,
                      GameModeParams*            lpGameModeParams,
                      ScoringSystem*             /*lpScoringSystem*/)
{
    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);

    // Crash traffic density: a fixed tuning value the X360 loads from .data @0x82005548
    // (0x82000000 + 21832). The .rdata bytes are NOT present in the available references/ exports
    // (no binary/.rdata dump ships there), so the exact float is UNRESOLVED. It is provably NOT 0.0f
    // -- mfTrafficDensityScale is a direct traffic-density multiplier (BrnTrafficEntityModule.cpp:3542
    // mfGameModeDensityScale = mfTrafficDensityScale) and offline crash mode is built around dense
    // traffic to crash into, so 0.0f would disable the mode's core content. Modelled as a named,
    // crash-density-plausible best estimate; INTEGRATOR MUST read the 4 bytes at 0x82005548 from the
    // XEX and replace the value (compile-gate unaffected -- this is a runtime-parity constant).
    static const f32 KF_CRASH_TRAFFIC_DENSITY_SCALE = 2.0f; // .data @0x82005548 (value UNRESOLVED; estimate, not 0.0f)
    lpGameModeParams->SetTrafficDensityScale(KF_CRASH_TRAFFIC_DENSITY_SCALE);

    // mfLargeVehicleProbability = 1.3 (X360 *(gap34+4) = 1.3).
    lpGameModeParams->SetLargeVehicleProbability(1.3f);

    // muFlags |= 0x1042139E (the X360 low-word crash mask; high dword 0x82000000 is the data-base artifact).
    lpGameModeParams->SetFlag(
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD               // 0x00000002
      | GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP                 // 0x00000004
      | GameModeParams::KU_FLAG_ENABLE_EASY_CRASHING                  // 0x00000008
      | GameModeParams::KU_FLAG_PLAYER_MUST_BE_CRASHING               // 0x00000010
      | GameModeParams::KU_FLAG_SET_DIRECTOR_TO_CRASH_MODE_AFTER_INTRO // 0x00000080
      | GameModeParams::KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS             // 0x00000100
      | GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR        // 0x00000200
      | GameModeParams::KU_FLAG_HARDCORE_TRAFFIC_SWERVING            // 0x00001000
      | GameModeParams::KU_FLAG_DISABLE_TRAFFIC_RESET               // 0x00020000
      | GameModeParams::KU_FLAG_DISABLE_ALL_TDS                     // 0x00400000
      | GameModeParams::KU_FLAG_EASY_SMASH_PROPS);                  // 0x10000000
}
}
