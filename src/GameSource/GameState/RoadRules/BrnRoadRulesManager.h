// ---------------------------------------------------------------------------
// GameSource/GameState/RoadRules/BrnRoadRulesManager.h
//   (canonical home for BrnGameState::RoadRulesManager)
//
// MINIMAL-COHERENT SLICE for the one X360-attested out-of-line function:
//   RoadRulesManager::GetCurrentRoadID  @ 0x82327438
//
// Member layout pinned to the X360 binary AND the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/RoadRules/BrnRoadRulesManager.h).
// The two offsets this function reads:
//   *(this+20) -> mpStreetManager   (asserted "mpStreetManager")
//   *(this+32) -> miLastRoadIndex   (the "current road"; -1 == none)
// These pin the DWARF member order:
//   mRoadRulesDebugComponent  +0   (20 bytes; DebugComponent vtbl + mpRoadRulesManager + 2 bools, padded)
//   mpStreetManager           +20  (0x14)
//   mpModeManager             +24  (0x18)
//   mpTrainingManager         +28  (0x1C)
//   miLastRoadIndex           +32  (0x20)   <-- read by GetCurrentRoadID
//
// The leading mRoadRulesDebugComponent is represented as explicit 20-byte
// storage because its real type (BrnGameState::RoadRulesDebugComponent :
// CgsDev::DebugComponent) is not committed yet and is not touched by this
// function. Trailing members (after miLastRoadIndex) are declaration-only
// padding -- not read by this TU -- and are left for the full class build.
//
// DWARF return type for GetCurrentRoadID is CgsID (u64); the X360 Hex-Rays
// "int" + 4-byte "*(Road+16)" read is the decompiler truncating the 8-byte
// CgsID load (it also printed "local variable allocation has failed").
// ---------------------------------------------------------------------------
#ifndef BRN_ROAD_RULES_MANAGER_H
#define BRN_ROAD_RULES_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                          // CgsID (u64)
#include "SharedClasses/StreetData/BrnStreetData.h"  // BrnStreetData::RoadIndex (SpanBase::RoadIndex in DWARF)

namespace BrnGameState
{
    // Forward declarations -- this TU only stores pointers to these / does not
    // touch their layout. Real homes:
    //   StreetManager   -> GameSource/GameState/StreetData/BrnGameStateStreetManager.h (NOT committed)
    //   ModeManager     -> GameSource/GameState/ModeManager/...                        (NOT committed)
    //   TrainingManager -> GameSource/GameState/...                                    (NOT committed)
    // DWARF declares StreetManager as `struct`; use `struct` to pre-agree with the
    // real home and avoid a future C4099 struct/class tag mismatch.
    struct StreetManager;
    class ModeManager;
    class TrainingManager;

    class RoadRulesManager
    {
    public:
        // DWARF: extern const CgsID K_INVALID_ID; (class-scope static const).
        // GetCurrentRoadID returns this when there is no current road
        // (X360 returns literal 0 -> K_INVALID_ID == 0).
        static const CgsID K_INVALID_ID;

        // The single X360-attested out-of-line function (body in the .cpp).
        // DWARF: CgsID GetCurrentRoadID() const;  (BrnRoadRulesManager.h:131)
        CgsID GetCurrentRoadID() const;

        // ---- rest of the public interface: declaration-only (own TUs) ----
        // void Construct( StreetManager*, ModeManager*, TrainingManager* );
        // void Destruct();
        // bool IsRoadRulesActive() const;
        // ... (omitted from this slice; see DWARF for the full list)

    private:
        // +0 : BrnGameState::RoadRulesDebugComponent mRoadRulesDebugComponent;
        //      Modeled as raw 20-byte storage so mpStreetManager lands at +20.
        //      (DebugComponent vtable ptr + mpRoadRulesManager + mbRenderInfo +
        //       mbRenderTimes, padded to 20.)
        u8                    maRoadRulesDebugComponentStorage[20];  // +0

        StreetManager*        mpStreetManager;     // +20 (0x14)
        ModeManager*          mpModeManager;       // +24 (0x18)
        TrainingManager*      mpTrainingManager;   // +28 (0x1C)

        // DWARF type: SpanBase::RoadIndex (== BrnStreetData::RoadIndex == int32_t).
        // -1 sentinel == "no current road".
        BrnStreetData::RoadIndex miLastRoadIndex;  // +32 (0x20)  <-- read here

        // ---- trailing members: declaration-only, NOT read by this TU -------
        // Per DecFIGS DWARF (BrnRoadRulesManager.h:244..266), in order:
        //   BrnStreetData::RoadIndex maiChallengeRoadIndex[2];
        //   CgsID                    mLastLimitId;
        //   EActiveRoadRule          meActiveRoadRule;
        //   EActiveRoadRule          mePreviousActiveRoadRule;
        //   bool                     mbRoadRulesNotAllowed;
        //   f32                      mfTime, mfTimeScoreTimeout, mfTimeTarget,
        //                            mfNextWarningTime;
        //   int32_t                  miNumWarningsDone;
        //   f32                      mfStuntTime, mfStuntRuleComboTimeout;
        //   int32_t                  miCrashScore;
        //   f32                      mfInRoadTimeout;
        //   bool                     mbAllowExitRoadRulesAfterTimeout;
        //   f32                      mfExitRoadRulesTime;
        //   bool                     mbSwitchingActive;
        //   bool                     mbIsOnlineMode;
        // Left out of this minimal slice (they do not affect GetCurrentRoadID
        // and would drag in EActiveRoadRule); add when building the full class.
    };
}

#endif // BRN_ROAD_RULES_MANAGER_H
