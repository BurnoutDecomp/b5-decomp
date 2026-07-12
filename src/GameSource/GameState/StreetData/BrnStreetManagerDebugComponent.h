#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"  // CgsDev::DebugComponent (real base)

// ============================================================================
// b5-decomp/src/GameSource/Gamestate/StreetData/BrnStreetManagerDebugComponent.h
// ============================================================================
// BrnGameState::StreetManagerDebugComponent -- the in-game debug menu for the road-rules
// StreetManager. Derives from the real CgsDev::DebugComponent. It registers a "number of
// road rules to win" selector plus a handful of "win all / win N" road-rule debug actions,
// and a "send scores to network" toggle action.
//
// SHAPE + member order are DWARF-authoritative (DecFIGS dwarfdump
// GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h:77/91/92) and reproduce
// the X360 object layout: the CgsDev::DebugComponent base sub-object occupies +0x00..+0x0B
// (vtable@0, mbActive@4, mpDebugLinkedListNext@8); then
//   miNumberOfRoadRulesToWin        = +0x0C  (X360 OnActivate `addi r4,r31,0xC` -> the RegisterVariable target)
//   mpStreetManager                 = +0x10  (X360 `lwz r11,0x10(rXX)` throughout)
//   mbOutputRoadRulesScoresToNetwork= +0x14  (X360 Update `lbz r11,0x14(r30)` / SendRoadRules `stb r11,0x14(r30)`)
//
// SCOPE: only the eight functions this TU's X360 ledger attests are declared here
// (GetName, OnActivate, Update, PopulateUserChallengeScores, SendRoadRulesScoresToNetwork,
// WinAllRoadRules, WinSpecificNumberOfCrashRoadRules, WinSpecificNumberOfTimeRoadRules).
// Seven of them are bodied in the .cpp; only Update is still declaration-only -- it packs a
// road-rules score-summary game-action event from deep, still-un-named ProgressionManager/Profile
// members (Profile+0x1CD1C / Profile+0x1CD30 / ProgressionManager+0x1D0) with no attested
// accessor, so its body is left for when those members are committed. The three Win* actions ARE
// now bodied: their trophy/achievement tail routes through the now-committed ProgressionManager /
// AchievementManagerBase and the X360-attested StreetManager road-rule accessors (additively
// declared on BrnGameStateStreetManager.h). The other DWARF-listed methods (Construct/Destruct/
// GetPath/GetNumberOfRoadRulesToWin/WinAllTime*/WinAllCrash*) are NOT in this TU's X360 ledger and
// are intentionally not declared here.

namespace BrnStreetData
{
    struct StreetManager;  // (unused here) -- kept out; the pointer member below is BrnGameState::StreetManager
}

namespace BrnGameState
{
    struct StreetManager;                                // pointer member only; full def in BrnGameStateStreetManager.h
    namespace GameStateModuleIO { struct OutputBuffer; } // Update() param, by pointer only

    class StreetManagerDebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Debug-tweakable: how many road rules the "win specific number of ..." actions grant
        // (X360 OnActivate binds &miNumberOfRoadRulesToWin into the debug menu). Public to match
        // the DWARF (BrnStreetManagerDebugComponent.h:77).
        int32_t miNumberOfRoadRulesToWin;   // +0x0C

        // Per-frame pump (X360 0x8234A7E0, called by StreetManager::Update). When the
        // "send scores to network" toggle is armed, packs the local road-rules score summary
        // into the output game-action queue. DECLARATION ONLY: reaches deferred
        // StreetManager/ProgressionManager/Profile layouts + the game-action queue; bodied by a
        // later pass once those types are committed.
        void Update(GameStateModuleIO::OutputBuffer* lpOutput);

    protected:
        // Menu label (X360 0x823175F0). Overrides CgsDev::DebugComponent::GetName.
        const char* GetName() const override;

        // Registers this component's tweakables + actions with the debug menu (X360 0x8234A718).
        void OnActivate() override;

    private:
        // Layout (DWARF offsets above). Access by name, never by raw offset.
        StreetManager* mpStreetManager;                 // +0x10
        bool           mbOutputRoadRulesScoresToNetwork; // +0x14

        // ---- Debug action callbacks -----------------------------------------------------------
        // All registered with the debug menu as plain DebugCallbackFunction (void(*)(void*)) with
        // `this` handed back as the user-data, so they are STATIC (the void* context IS the
        // component -- the X360 asm loads the bare function address into the RegisterFunction
        // callback arg and passes the component as a separate user-data arg, which a non-static
        // member could not be). The DecFIGS DWARF lists them as plain members; the X360 calling
        // convention proves the static-callback shape (matches the Feb-2007 header's `static`).

        // Fills every road with a randomised local-player challenge score (X360 0x8233F928).
        static void PopulateUserChallengeScores(void* lpData);

        // Arms the "send road-rules scores to network" toggle (X360 0x82317600).
        static void SendRoadRulesScoresToNetwork(void* lpData);

        // "Win all road rules bar one" action (X360 0x82341CD8). Best-scores every road except the
        // last, then awards the road-rule trophies through the StreetManager road-rule accessors +
        // ProgressionManager (bodied in the .cpp).
        static void WinAllRoadRules(void* lpData);

        // "Win Specific Number Of RoadRules" (crash) action (X360 0x82341AC0). Best-crash-scores the
        // first miNumberOfRoadRulesToWin roads, then awards trophies (bodied in the .cpp).
        static void WinSpecificNumberOfCrashRoadRules(void* lpData);

        // "Win Specific Number Of Time RoadRules" action (X360 0x823418B8). Best-time-scores the
        // first miNumberOfRoadRulesToWin roads, then awards trophies (bodied in the .cpp).
        static void WinSpecificNumberOfTimeRoadRules(void* lpData);
    };
}
