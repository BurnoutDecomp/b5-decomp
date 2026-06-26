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
// The X360 asm composes the full 64-bit crash flag mask in r12 -- `li r12,1; sldi r12,r12,32;
// oris r12,r12,0x1042; ori r12,r12,0x139E` (== 0x1_1042139E) -- and ORs it into the u64 muFlags
// (`ld r10,0x860; or r11,r10,r12; std r11,0x860`). The high bit 0x1_00000000 is a GENUINE flag
// (KU_FLAG_DISABLE_PROP_PROGRESSION), built by `li 1; sldi 32`, NOT the .data base pointer -- the
// 0x82000000 the earlier reconstruction mistook for the high half is only the `lis r11,flt_...@ha`
// float-load base for mfTrafficDensityScale. PS3 DecFIGS 0x1D6B70 confirms the literal: `|= 0x11042139ELL`.
// The mask decomposes EXACTLY (verified) to the 12 named KU_FLAG_* bits below.
void CrashMode::Start(const StartGameModeParams* /*lpStartGameModeParams*/,
                      GameModeParams*            lpGameModeParams,
                      ScoringSystem*             /*lpScoringSystem*/)
{
    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME);

    // Crash traffic density: a fixed tuning value the X360 loads from .rdata flt_82005548 (the
    // float bits are not in the available X360 exports). RESOLVED from the PS3 DecFIGS build
    // (CrashMode::Start 0x1D6B70), where the same store reads the literal 2.5
    // (`*(lpGameModeParams + 48) = 2.5`). Same source -> the X360 value is 2.5.
    static const f32 KF_CRASH_TRAFFIC_DENSITY_SCALE = 2.5f; // PS3 DecFIGS 0x1D6B70 (mfTrafficDensityScale)
    lpGameModeParams->SetTrafficDensityScale(KF_CRASH_TRAFFIC_DENSITY_SCALE);

    // mfLargeVehicleProbability = 1.3 (X360 *(gap34+4) = 1.3).
    lpGameModeParams->SetLargeVehicleProbability(1.3f);

    // muFlags |= 0x1_1042139E (full 64-bit X360 crash mask; the 0x1_00000000 bit is a real flag).
    lpGameModeParams->SetFlag(
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD               // 0x000000002
      | GameModeParams::KU_FLAG_DISABLE_CRASH_CLEAN_UP                 // 0x000000004
      | GameModeParams::KU_FLAG_ENABLE_EASY_CRASHING                  // 0x000000008
      | GameModeParams::KU_FLAG_PLAYER_MUST_BE_CRASHING               // 0x000000010
      | GameModeParams::KU_FLAG_SET_DIRECTOR_TO_CRASH_MODE_AFTER_INTRO // 0x000000080
      | GameModeParams::KU_FLAG_ALLOW_CRASH_PLAY_CONTROLS             // 0x000000100
      | GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR        // 0x000000200
      | GameModeParams::KU_FLAG_HARDCORE_TRAFFIC_SWERVING            // 0x000001000
      | GameModeParams::KU_FLAG_DISABLE_TRAFFIC_RESET               // 0x000020000
      | GameModeParams::KU_FLAG_DISABLE_ALL_TDS                     // 0x000400000
      | GameModeParams::KU_FLAG_EASY_SMASH_PROPS                    // 0x010000000
      | GameModeParams::KU_FLAG_DISABLE_PROP_PROGRESSION);          // 0x100000000
}
}
