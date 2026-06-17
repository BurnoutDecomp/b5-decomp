#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnOnlineBurningHomeRunModeScoring.h
// ============================================================================
// Burning-Home-Run online-mode scorer. DWARF home BrnOnlineBurningHomeRunModeScoring.h:46;
// derives from BaseOnlineModeScoring (vptr at +0, all data members live in the base). This class
// adds NO new data members of its own -- it only overrides the scoring lifecycle and declares one
// file-private qsort comparator. Virtual override ORDER mirrors the base vtable.
//
// MINIMAL SLICE: of the eight virtuals + one private comparator the DWARF lists, only the three
// this TU reconstructs (UpdatePlayerPoints, WriteDataToOutput,
// _BurningHomeRunPlayerFinishTimesCompare) carry bodies in the .cpp; the remaining five lifecycle
// virtuals (Construct/Prepare/Release/Destruct/ClearData/Update) are declared-only and land with the
// rest of this class's TU. Declared here so the vtable is complete.

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h" // base class + fwd-decl `class ScoringSystem`
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // OnlineScoringOutputInterface

// This header intentionally does NOT include BrnScoringSystem.h. The keystone embeds this scorer BY
// VALUE, so it must see this full class definition; if this header pulled the keystone back in, the
// `#pragma once` guard would leave one of the two types incomplete at the embed point (mutual-include
// cycle). ScoringSystem is named only BY POINTER in the virtuals below (the base header forward-
// declares it); CarData is touched only in the .cpp, which includes the keystone itself.

namespace BrnGameState
{
    class OnlineBurningHomeRunModeScoring : public BaseOnlineModeScoring
    {
    public:
        // Lifecycle virtuals (declared-only here; bodies belong to the wider class TU).
        virtual void Construct();                                          // .cpp:62
        virtual bool Prepare();                                            // .cpp:92
        virtual bool Release();                                            // .cpp:107
        virtual void Destruct();                                           // .cpp:77
        virtual void ClearData();                                          // .cpp:119
        virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumActiveCars); // .cpp:148

        // X360 @ 0x8232F6C0 (.cpp:287). Builds the per-car finish ranking, sorts it, and writes
        // each car's finishing position back into the scoring system + the base position table.
        virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumActiveCars);

        // X360 @ 0x82315650 (.cpp:133). Copies this scorer's award/award-variable/team arrays into
        // the network output interface (overrides the base, omitting maiNumEliminations).
        virtual void WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput);

    private:
        // X360 @ 0x823156C0 (.cpp:163). qsort() comparator over ComparisonData (file-local struct,
        // 36-byte stride). static so it has the (const void*, const void*) free-function-pointer
        // shape qsort requires (the X360 build emits it as a member, but it is only ever taken as a
        // free function ptr).
        static s32 _BurningHomeRunPlayerFinishTimesCompare(const void* lpPlayer1, const void* lpPlayer2);
    };
}
