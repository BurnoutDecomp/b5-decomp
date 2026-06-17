#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnOnlineRaceModeScoring.h
// ============================================================================
// Online RACE-mode scorer. Derives from BaseOnlineModeScoring and overrides the scoring
// virtuals (Update / UpdatePlayerPoints / ClearData / ...). DWARF home BrnOnlineRaceModeScoring.h:44.
// The class adds NO data members of its own (the DWARF struct is methods + the nested
// IndexToFinishTuple only), so its object layout is identical to the base; ClearData below
// writes BASE members.
//
// MINIMAL SLICE: only the four reconstructed functions get full bodies in the .cpp; the rest of the
// virtual lifecycle (Construct/Prepare/Release/Destruct/WriteDataToOutput) is declared-only so the
// vtable slots exist in this TU's relative order. Override order matches the base vtable.

#include "types.hpp"
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h" // base + Compare*/UpdatePlayerTeams/SetPlayerPosition + fwd-decl `class ScoringSystem`
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // OnlineScoringOutputInterface
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                    // BrnNetwork::NetworkPlayerID (== s32)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                       // CgsSystem::Time

// This header intentionally does NOT include BrnScoringSystem.h. The keystone embeds this scorer BY
// VALUE, so it must see this full class definition; if this header pulled the keystone back in, the
// `#pragma once` guard would leave one of the two types incomplete at the embed point (mutual-include
// cycle). ScoringSystem is named only BY POINTER in the virtuals below (the base header forward-
// declares it); CarData is touched only in the .cpp, which includes the keystone itself.

namespace BrnGameState
{
class OnlineRaceModeScoring : public BaseOnlineModeScoring
{
public:
    // ---- virtual lifecycle (declared-only; bodies belong to the rest of this TU) ----
    virtual void Construct();                                       // BrnOnlineRaceModeScoring.cpp:42
    virtual bool Prepare();                                         //                          .cpp:72
    virtual bool Release();                                         //                          .cpp:87
    virtual void Destruct();                                        //                          .cpp:57

    // X360 0x8232F948 (.cpp:101). Forwards verbatim to BaseOnlineModeScoring::UpdatePlayerTeams.
    virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 0x8232EB70 (.cpp:239). Sorts the active race cars by finishing order and assigns
    // points + finish positions. NOTE: DWARF types the ScoringSystem param non-const here
    // (Update's is const); honored.
    virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    virtual void WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput); // declared-only (.cpp:322)

    // X360 0x82315610 (.cpp:335). Resets the per-slot online award + team arrays (base members).
    virtual void ClearData();

private:
    // DWARF BrnOnlineRaceModeScoring.h:91. One sortable row per active race car. 24-byte stride
    // (qsort element size 24 in UpdatePlayerPoints; X360 asm confirms mFinishTime @ +8 via the
    // 0x18-spaced init writes). Field offsets are DWARF-authoritative.
    struct IndexToFinishTuple
    {
        EActiveRaceCarIndex         meRaceCarIndex;     // +0
        BrnNetwork::NetworkPlayerID mNetworkPlayerID;   // +4  (-1 == empty/invalid slot)
        CgsSystem::Time             mFinishTime;        // +8  (8 bytes: miSeconds @+8, mfFraction @+12)
        f32                         mfDistanceToFinish; // +16
        bool                        mbTimedOut;         // +20
        bool                        mbDisconnected;     // +21
        bool                        mbFinished;         // +22
        // +23 tail pad to 24
    };

    // X360 0x82314B58 (.cpp:115). qsort comparator over IndexToFinishTuple. The X360 build emits
    // it as a non-static private member (no `this` use); reconstructed `static` so it is a valid
    // C function pointer for qsort. Returns -1 / 0 / +1.
    static int _PlayerFinishTimesCompare(const void* lpPlayer1, const void* lpPlayer2);
};
}
