#pragma once

#include <cstddef>                                                       // offsetof (layout tripwire)

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h" // RoadRageModeScoring (UpdateMaxActiveCars' by-pointer param; its owning header)

namespace BrnGameState
{
// RoadRageMode is a concrete offline game mode ("RoadRage"). Bases (OfflineGameMode -> GameMode)
// are #included from their owning headers, never forked.
//
// ===================================================================================================
// CONSOLE OVERRIDE SET -- vtable 0x820D05E8 (identified by decoding its slot-6 GetName leaf
// 0x827E24D8, which returns "RoadRage"). Checked 2026-08-26 against the image and the DWARF
// (BrnRoadRageMode.h:45-145), which agree exactly.
//   slot  2  PreWorldUpdate                 0x823448C0   declared + bodied (2026-09-02)
//   slot  5  Start                          0x82330678   declared + bodied (2026-09-02)
//   slot  6  GetName                        0x827E24D8   declared + bodied
//   slot 10  OnPlayerInShortCut             0x823160A0   declared + bodied (2026-09-02)
//   slot 12  SendEvent                      0x82330A38   declared + bodied (2026-09-02)
//   slot 13  ShouldExit                     0x827E2F38   declared + bodied (`li r3,0; blr`)
//   slot 14  ShouldFinish                   0x82315D60   declared + bodied (2026-09-02)
//   slot 15  FillInGameModeSpecificResults  0x82315D40   declared + bodied (2026-09-02)
//   slot 22  HandleGameEvents               0x82315FF8   declared + bodied (2026-09-02)
//   slot 23  RequiresStreaming              0x827E2F38   declared + bodied (`li r3,0; blr`)
// Non-virtual helpers (DWARF BrnRoadRageMode.h:139-143): UpdateMaxActiveCars 0x82315E58,
// BroadcastEventsToRivals 0x82344798, UpdateHiddenRivals 0x82315DC0 -- all bodied in the .cpp.
//
// [x] FRONTIER CLOSED 2026-09-02: the seven overrides this banner used to list as "not declared
// because no body exists" now all have bodies in BrnRoadRageMode.cpp, so the vtable emitted from
// this header has no unresolved slot.
//
// KF_ROADRAGE_TRAFFIC_DENSITY (DWARF BrnRoadRageMode.h:109 / .cpp:26) is deliberately NOT declared:
// the X360 Start @0x82330678 never loads it -- the traffic density it publishes is
// `lfs 0x318(startParams) * lfs 0xC(rankData)` (start density x rank GetTrafficDensityRoadRage),
// so the constant is dead in ARTIST and its value has no load to dump from. A declaration with no
// attested definition would be a latent link hole, so it stays out until a use is found.
// ===================================================================================================
class RoadRageMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E24D8

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- road rage never idle-exits.
    // DWARF BrnRoadRageMode.h:79 declares this override. ADDED 2026-08-26 with the 26-slot base,
    // because GameMode::ShouldExit is now wired to the real ScoringSystem idle timers and without
    // this the mode would start exiting itself after 4 s of no input.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). DWARF BrnRoadRageMode.h:109 declares this override. SetupGameMode @0x8234B158
    // gates the streaming wait on it.
    virtual bool RequiresStreaming() const;

    // ===============================================================================================
    // THE SEVEN OVERRIDES LANDED 2026-09-02. Each is written in the EXACT base signature from
    // BrnGameMode.h -- a drifted parameter list would MINT A NEW SLOT instead of overriding, silently,
    // with nothing a compile-only gate can see. The member-pointer static_asserts at the foot of
    // this header are the tripwire.
    // ===============================================================================================

    // Slot 2 (vtbl+8), X360 0x823448C0. Base first (all six forwarded unchanged: r4..r9 are
    // untouched between the prologue and the `bl GameMode::PreWorldUpdate`), then -- only while
    // meCurrentState == E_GMS_IN_PROGRESS -- UpdateMaxActiveCars / UpdateHiddenRivals on the
    // frame delta, then BroadcastEventsToRivals on every frame.
    virtual void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                bool lbPaused,
                                const ScoringSystem* lpScoringSystem);

    // Slot 5 (vtbl+20), X360 0x82330678. The console body takes only THREE registers (r3 this,
    // r4 lpStartGameModeParams, r5 lpGameModeParams) and never touches r6 -- the ScoringSystem*
    // the base slot passes is unused here, exactly as in the sibling offline Start bodies.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);

    // Slot 10 (vtbl+40), X360 0x823160A0. Arms every rival's hidden timer to
    // KF_HIDE_TIME_IF_PLAYER_IN_SHORTCUT and requests a rival broadcast.
    virtual void OnPlayerInShortCut();

    // Slot 12 (vtbl+48), X360 0x82330A38. E_GME_NEXT while IN_PROGRESS jumps straight to
    // E_GMS_RESULTS (a road rage has no outro); everything else is the base.
    virtual void SendEvent(EGameModeEvent leEvent);

    // Slot 14 (vtbl+56), X360 0x82315D60. NON-const, MUTABLE ScoringSystem* -- the base's declared
    // shape. Reads the two idle timers; on "finish" it also re-arms the mode's broadcast latches.
    virtual bool ShouldFinish(ScoringSystem* lpScoringSystem);

    // Slot 15 (vtbl+60), X360 0x82315D40. miFinishPosition = 1 when the takedown target was met,
    // else 2 -- read off the embedded road-rage scorer.
    virtual void FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                               GameStateModuleIO::FinishedModeAction* lpAction);

    // Slot 22 (vtbl+88), X360 0x82315FF8. Consumes the "race car needs hiding" game event (X360
    // id 40): stores the requested hidden time for that rival and requests a rival broadcast.
    virtual void HandleGameEvents(const CgsModule::Event* lpEvent, s32 liEventType);

private:
    // ---- non-virtual helpers (DWARF BrnRoadRageMode.h:139/:141/:143) --------------------------
    // X360 0x82315E58. Advances the madness ratio on lfDeltaTime, derives the madness level from
    // the rank ratio (+ post-target takedowns), and re-derives how many rivals may be active.
    void UpdateMaxActiveCars(f32 lfDeltaTime, const RoadRageModeScoring* lpRoadRageScoring);
    // X360 0x82344798. Posts the per-rival "allowed in road rage" / "madness level" game actions
    // when the matching latch is set, then clears the latch.
    void BroadcastEventsToRivals(GameStateModuleIO::OutputBuffer* lpOutput);
    // X360 0x82315DC0. Ticks every armed hidden timer down by lfDeltaTime (capped to the madness-
    // scaled maximum) and requests a rival broadcast when one expires.
    void UpdateHiddenRivals(f32 lfDeltaTime);

    // ---- DWARF member layout (BrnRoadRageMode.h:111-124), in DWARF order --------------------------
    // CONSOLE OFFSETS, every one re-derived from the asm this pass (this == r3/r30/r31 as noted):
    //   +188 (0xBC) miNumberOfTransmittedRivals     Start `stw r29(0), 0xBC(r30)`; UpdateMaxActiveCars `lwz 0xBC / stw 0xBC`
    //   +192 (0xC0) miNumberOfAllowedRoadRageRivals Start `stw r28(7), 0xC0(r30)`; BroadcastEventsToRivals `lwz 0xC0`
    //   +196 (0xC4) miNumberOfRivals                Start `stw r11, 0xC4(r30)` (three times, the clamp chain)
    //   +200 (0xC8) mAddCarPreDelay                 never touched by any of the ten X360 bodies (DWARF-only slot)
    //   +204 (0xCC) mfRoadRageMadness               UpdateMaxActiveCars `stfs f0, 0xCC(r3)`; UpdateHiddenRivals `lvlx v12, r3, r7(0xCC)`
    //   +208 (0xD0) mfRoadRageMadnessRatio          UpdateMaxActiveCars `lfs f0, 0xD0(r3)` / `stfs f12, 0xD0(r3)`
    //   +212 (0xD4) mfHiddenTime[8]                 HandleGameEvents `stfsx f0, (idx+0x35)*4, r3`; OnPlayerInShortCut `addi r11, r3, 0xD4` x8
    //   +244 (0xF4) mbUpdateRivals                  `stb r9(1), 0xF4(r3)` everywhere; BroadcastEventsToRivals `lbz 0xF4`
    //   +245 (0xF5) mbUpdateMadnessLevel            `stb r9(1), 0xF5(r3)`; BroadcastEventsToRivals `lbz 0xF5`
    //   +248 (0xF8) mfMadnessBroadcastLevel         UpdateMaxActiveCars `lfs f13, 0xF8(r3)` / `stfs f0, 0xF8(r3)`
    //   +252 (0xFC) mfProgressionRankAsRatio        Start `stfs f0, 0xFC(r30)`; UpdateMaxActiveCars `addi r10, r3, 0xFC` (lvlx)
    // The console OfflineGameMode ends at +188 (its last member is miDebugDesignIndexOfLandmark-
    // ToAlwaysRaceTo @+184, BrnOfflineGameMode.h), so +188 is exactly where the DWARF's first
    // RoadRageMode member lands -- the base and the derived agree, no padding is needed. Host
    // offsets differ (x64 widening of the base); parity is by NAME, pinned order-only below.
    s32  miNumberOfTransmittedRivals;
    s32  miNumberOfAllowedRoadRageRivals;
    s32  miNumberOfRivals;
    f32  mAddCarPreDelay;
    f32  mfRoadRageMadness;
    f32  mfRoadRageMadnessRatio;
    f32  mfHiddenTime[8];
    bool mbUpdateRivals;
    bool mbUpdateMadnessLevel;
    f32  mfMadnessBroadcastLevel;
    f32  mfProgressionRankAsRatio;

    // ---- LAYOUT TRIPWIRE (order-only, the BrnModeManager_AssertLayout.cpp recipe) ---------------
    // Never an absolute offset (the x64 base is wider than the console's 188 bytes); every line is a
    // `<` so it survives the widening while still catching a re-sort of the DWARF run above, plus
    // the two facts the asm relies on: mfHiddenTime is eight consecutive f32 (the `(idx+53)*4`
    // index and the `mtctr 8` fill loops) and the two latch bytes are adjacent (0xF4/0xF5).
    // Private members are only reachable from inside the class, hence the member function.
    static void AssertLayout()
    {
        static_assert(offsetof(RoadRageMode, miNumberOfTransmittedRivals) >= sizeof(OfflineGameMode),
                      "RoadRageMode's members must follow the complete OfflineGameMode base (console +188)");
        static_assert(offsetof(RoadRageMode, miNumberOfTransmittedRivals)     < offsetof(RoadRageMode, miNumberOfAllowedRoadRageRivals), "order (console +188 < +192)");
        static_assert(offsetof(RoadRageMode, miNumberOfAllowedRoadRageRivals) < offsetof(RoadRageMode, miNumberOfRivals),                "order (console +192 < +196)");
        static_assert(offsetof(RoadRageMode, miNumberOfRivals)                < offsetof(RoadRageMode, mAddCarPreDelay),                 "order (console +196 < +200)");
        static_assert(offsetof(RoadRageMode, mAddCarPreDelay)                 < offsetof(RoadRageMode, mfRoadRageMadness),               "order (console +200 < +204)");
        static_assert(offsetof(RoadRageMode, mfRoadRageMadness)               < offsetof(RoadRageMode, mfRoadRageMadnessRatio),          "order (console +204 < +208)");
        static_assert(offsetof(RoadRageMode, mfRoadRageMadnessRatio)          < offsetof(RoadRageMode, mfHiddenTime),                    "order (console +208 < +212)");
        static_assert(offsetof(RoadRageMode, mfHiddenTime)                    < offsetof(RoadRageMode, mbUpdateRivals),                  "order (console +212 < +244)");
        static_assert(offsetof(RoadRageMode, mbUpdateRivals)                  < offsetof(RoadRageMode, mbUpdateMadnessLevel),            "order (console +244 < +245)");
        static_assert(offsetof(RoadRageMode, mbUpdateMadnessLevel)            < offsetof(RoadRageMode, mfMadnessBroadcastLevel),         "order (console +245 < +248)");
        static_assert(offsetof(RoadRageMode, mfMadnessBroadcastLevel)         < offsetof(RoadRageMode, mfProgressionRankAsRatio),        "order (console +248 < +252)");
        static_assert(sizeof(mfHiddenTime) == 8 * sizeof(f32), "mfHiddenTime is f32[8] (console +212..+243)");
        static_assert(offsetof(RoadRageMode, mbUpdateMadnessLevel) == offsetof(RoadRageMode, mbUpdateRivals) + 1,
                      "mbUpdateRivals / mbUpdateMadnessLevel are adjacent bytes (console 0xF4 / 0xF5)");
    }
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (RoadRageMode::*)() const>(&RoadRageMode::GetName)) != 0,
              "RoadRageMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<bool (RoadRageMode::*)(const ScoringSystem*) const>(&RoadRageMode::ShouldExit)) != 0,
              "RoadRageMode::ShouldExit must bind GameMode vtable slot 13");
static_assert(sizeof(static_cast<bool (RoadRageMode::*)() const>(&RoadRageMode::RequiresStreaming)) != 0,
              "RoadRageMode::RequiresStreaming must bind GameMode vtable slot 23");
static_assert(sizeof(static_cast<void (RoadRageMode::*)(GameStateModuleIO::OutputBuffer*,
                                                        const GameStateModuleIO::PreWorldInputBuffer*,
                                                        const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*,
                                                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,
                                                        bool,
                                                        const ScoringSystem*)>(&RoadRageMode::PreWorldUpdate)) != 0,
              "RoadRageMode::PreWorldUpdate must bind GameMode vtable slot 2 (X360 0x823448C0)");
static_assert(sizeof(static_cast<void (RoadRageMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&RoadRageMode::Start)) != 0,
              "RoadRageMode::Start must bind GameMode vtable slot 5 (X360 0x82330678)");
static_assert(sizeof(static_cast<void (RoadRageMode::*)()>(&RoadRageMode::OnPlayerInShortCut)) != 0,
              "RoadRageMode::OnPlayerInShortCut must bind GameMode vtable slot 10 (X360 0x823160A0)");
static_assert(sizeof(static_cast<void (RoadRageMode::*)(EGameModeEvent)>(&RoadRageMode::SendEvent)) != 0,
              "RoadRageMode::SendEvent must bind GameMode vtable slot 12 (X360 0x82330A38)");
static_assert(sizeof(static_cast<bool (RoadRageMode::*)(ScoringSystem*)>(&RoadRageMode::ShouldFinish)) != 0,
              "RoadRageMode::ShouldFinish must bind GameMode vtable slot 14 (X360 0x82315D60)");
static_assert(sizeof(static_cast<void (RoadRageMode::*)(const ScoringSystem*, GameStateModuleIO::FinishedModeAction*)>(&RoadRageMode::FillInGameModeSpecificResults)) != 0,
              "RoadRageMode::FillInGameModeSpecificResults must bind GameMode vtable slot 15 (X360 0x82315D40)");
static_assert(sizeof(static_cast<void (RoadRageMode::*)(const CgsModule::Event*, s32)>(&RoadRageMode::HandleGameEvents)) != 0,
              "RoadRageMode::HandleGameEvents must bind GameMode vtable slot 22 (X360 0x82315FF8)");
}
