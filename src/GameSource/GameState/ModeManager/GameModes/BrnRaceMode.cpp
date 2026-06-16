#include "GameSource/GameState/ModeManager/GameModes/BrnRaceMode.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Debug-print plumbing for the X360 message-filter logging block (see the guarded
// CGS_MESSAGE spew in Start below). Modelled exactly as the established reconstruction in
// GameSource/Gui/BrnGuiModule.cpp: gxMessageFilterFlags is the global filter word and
// gpDebugPrint is the global line-writer whose vtable slot 1 is a printf-style writer.
namespace CgsDev
{
    namespace Message
    {
        extern u32 gxMessageFilterFlags;
    }

    namespace Log
    {
        struct DebugPrint;
        typedef int (*DebugPrintFn)(DebugPrint*, const char*);
        struct DebugPrint
        {
            DebugPrintFn* mpVTable;
        };
        extern DebugPrint* gpDebugPrint;
    }
}

namespace BrnGameState
{
// BrnRaceMode.cpp:27 (DWARF). The X360 GetOutroTimeout body returns 0.0, so the race mode's
// outro timeout constant is 0.0 seconds for this build.
const f32 RaceMode::KF_OUTRO_TIME_SECONDS = 0.0f;

// X360: BrnGameState::RaceMode::GetName (0x827E2488).
const char* RaceMode::GetName() const
{
    return "Race";
}

// X360: BrnGameState::RaceMode::GetOutroTimeout (0x827E2498). Returns the fixed constant
// (0.0); the literal 0.0 in the pseudocode is that constant.
f32 RaceMode::GetOutroTimeout() const
{
    return KF_OUTRO_TIME_SECONDS;
}

// X360: BrnGameState::RaceMode::Start (0x82330018).
//
// Builds the mutable GameModeParams the race will run with from the immutable
// StartGameModeParams (lpStartGameModeParams) plus its attached event-data and rank-data
// records, then places the cars on the starting grid and caches the rival count / nearest-
// finish distance on the mode.
//
// The Hex-Rays output accesses everything through raw byte offsets and renders the param
// classes' accessors/mutators as inlined field pokes; this reconstruction restores the named
// accessors (DWARF: BrnGameModeParams.h / BrnProgressionRankData.h / BrnRaceEventData.h) per
// the AGENTS.md no-raw-offset rule. The third DWARF parameter (ScoringSystem*) is unused by
// the body (Hex-Rays drops it as an unused trailing __fastcall arg) and is left untouched.
void RaceMode::Start(const StartGameModeParams* lpStartGameModeParams,
                     GameModeParams* lpGameModeParams,
                     ScoringSystem* /*lpScoringSystem*/)
{
    const BrnProgression::RaceEventData*       lpEventData          = lpStartGameModeParams->GetEventData();
    const BrnProgression::ProgressionRankData* lpProgressionRankData = lpStartGameModeParams->GetProgressionRankData();

    // X360 guards each input with CGS_ASSERT(... != NULL). mpProgressionRankData and
    // mpEventData are the StartGameModeParams' own stored pointers (the source asserts fire
    // from BrnGameModeParams.h on the StartGameModeParams accessors); the lpGameModeParams /
    // lpProgressionRankData / lpEventData asserts fire from BrnRaceMode.cpp.
    CGS_ASSERT(lpProgressionRankData != nullptr, "mpProgressionRankData != NULL");
    CGS_ASSERT(lpEventData != nullptr, "lpEventData != NULL");
    CGS_ASSERT(lpGameModeParams != nullptr, "lpGameModeParams != NULL");
    CGS_ASSERT(lpProgressionRankData != nullptr, "lpProgressionRankData != NULL");

    // Initialise the run-time param block for this mode type, then thread through the start
    // mechanism / traffic-light trigger straight from the immutable start params.
    lpGameModeParams->Construct(lpStartGameModeParams->GetGameModeType());

    // Debug spew of the rank/start tuning values, gated on the global message filter. Faithful
    // to the X360 block: each line is written through vtable slot 1 of the global debug-print
    // object, followed by a newline. (The pseudocode formats the float/int value into the
    // first call and emits "\n" via a second; the value-formatting helper is the debug-print
    // front-end, reconstructed in its own TU -- here the lines are emitted verbatim.)
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        CgsDev::Log::DebugPrint* lpPrint = CgsDev::Log::gpDebugPrint;
        lpPrint->mpVTable[1](lpPrint, "lpProgressionRankData->GetTrafficDensityRace() : ");
        lpPrint->mpVTable[1](lpPrint, "\n");
        lpPrint->mpVTable[1](lpPrint, "lpProgressionRankData->GetLargeVehicleProbability() : ");
        lpPrint->mpVTable[1](lpPrint, "\n");
        lpPrint->mpVTable[1](lpPrint, "lpStartGameModeParams->GetTrafficDensity() : ");
        lpPrint->mpVTable[1](lpPrint, "\n");
        lpPrint->mpVTable[1](lpPrint, "lpProgressionRankData->GetRaceRivalsNumber() : ");
        lpPrint->mpVTable[1](lpPrint, "\n");
        lpPrint->mpVTable[1](lpPrint, "lpStartGameModeParams->GetProgressionRankAsRatio() : ");
        lpPrint->mpVTable[1](lpPrint, "\n");
        lpPrint->mpVTable[1](lpPrint,
            "**********************************************************************************************\n");
    }

    // Traffic density for the race = the start params' base density scaled by the rank's
    // race-specific density factor.
    lpGameModeParams->SetTrafficDensityScale(
        lpStartGameModeParams->GetTrafficDensity() * lpProgressionRankData->GetTrafficDensityRace());
    lpGameModeParams->SetLargeVehicleProbability(lpProgressionRankData->GetLargeVehicleProbability());

    // Number of rivals = the event's start-rival count, clamped down to the rank's allowed
    // race-rival maximum.
    s32 liNumRivals = lpEventData->GetStartRivalCount();
    if (liNumRivals >= static_cast<s32>(lpProgressionRankData->GetRaceRivalsNumber()))
    {
        liNumRivals = static_cast<s32>(lpProgressionRankData->GetRaceRivalsNumber());
    }
    lpGameModeParams->SetNumRivals(liNumRivals);

    lpGameModeParams->SetProgressionRankAsRatio(lpStartGameModeParams->GetProgressionRankAsRatio());

    // The X360 body sets the route-finding / AI-speed-selection style fields to their
    // race defaults (raw enum values 8, 8, 8, 2 in the pseudocode) and ORs in the race
    // run-mode flag set. These large-offset writes are the inlined GameModeParams mutators;
    // they are surfaced as the named flag set below. The two route-finding-style fields, the
    // AI speed-selection method and the A* distance-function field are GameModeParams members
    // set to fixed race defaults -- left to the full GameModeParams TU to model with their
    // BrnAI enums (not surfaced here to avoid forking those enums for a partial stub).

    // muFlags |= 0x0000_0008_2080_E803 (low dword 0x2080E803, high dword 8 = bit35 set). The
    // X360 asm composes the mask in r12: `li r12,8; sldi r12,r12,32; oris r12,r12,0x2080;
    // ori r12,r12,0xE803`, i.e. high dword = 8 -> 0x0000000800000000 (bit35). The Hex-Rays
    // `HIDWORD(muFlags)=2` is a decompiler artifact that conflates the SEPARATE adjacent field
    // store `stw r9,0x84C(r31)` (`*(a3+0x84C)=2`); the assembly OR is authoritative. Bit35 is
    // the 4th unnumbered DWARF flag after AI_RESET_ON_TRACK_BEHIND (bit31) =
    // KU_FLAG_ALLOW_REVENGE_TAKEDOWNS. The named low-word flags below match the DecFIGS
    // KU_FLAG_* table.
    lpGameModeParams->SetFlag(
        GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID         // 0x00000001
      | GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD       // 0x00000002
      | GameModeParams::KU_FLAG_HAS_ROUTE                      // 0x00002000
      | GameModeParams::KU_FLAG_AI_DRIVE_BY_START              // 0x00004000
      | GameModeParams::KU_FLAG_USE_RACE_BALANCING             // 0x00008000
      | GameModeParams::KU_FLAG_USES_NAVIGATION                // 0x20000000
      | GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC           // 0x00000800
      | GameModeParams::KU_FLAG_DISABLE_CRASH_EXTENSIONS       // 0x00800000
      | GameModeParams::KU_FLAG_ALLOW_REVENGE_TAKEDOWNS);      // 0x800000000 (bit35)

    lpGameModeParams->SetStartMechanism(lpStartGameModeParams->GetStartMechanism());
    lpGameModeParams->SetTrafficLightTriggerId(lpStartGameModeParams->GetTrafficLightTriggerId());

    // Place the cars: one slot per rival plus the player.
    mpModeManager->SetStartingGrid(lpGameModeParams, liNumRivals + 1, false);

    // X360 also reads the first start position (GetStartPosition(0)); the result is discarded
    // (the call has the side effect of the in-bounds array assert). Cache the mode's rival
    // total and the nearest-player-to-finish seed distance.
    miNumRivalsInRace = lpEventData->GetStartRivalCount() + lpEventData->GetAddRivalCount();

    // flt_82CDB7D0 -- a large "very far" sentinel that seeds the nearest-player-to-finish
    // tracker so the first real measurement always wins. The exact float bytes at 0x82CDB7D0
    // are not present in the available IDA JSON exports; the value is known only to be a large
    // sentinel (a sibling user, BrnGameState 0x8231F590, compares `dist > 200000.0 && dist !=
    // flt_82CDB7D0`, i.e. it is larger than 200000). 1.0e7 is used as a defensible stand-in.
    // INTEGRATOR: confirm the literal against the .i64 (read the 4 bytes at 0x82CDB7D0) and
    // promote it to a named KF_ constant if it recurs.
    mfNearestPlayerDistToFinish = 10000000.0f; // flt_82CDB7D0 (value unconfirmed -- see note)
}
}
