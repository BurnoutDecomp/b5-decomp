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

    // OnRoadLimit posts onto the output buffer (X360 a5 arg); pointer only.
    namespace GameStateModuleIO { struct OutputBuffer; }

    // The road-rules debug component (embedded as this class's first member) reaches directly into
    // the private timing/score state below (X360 RoadRulesDebugComponent callbacks + RenderHUD read
    // mfTime/mfStuntTime/miCrashScore/maiChallengeRoadIndex/miLastRoadIndex/mpStreetManager), exactly
    // as the binary does -- so it is granted friendship here.
    class RoadRulesDebugComponent;

    class RoadRulesManager
    {
        friend class RoadRulesDebugComponent;

    public:
        // DWARF: extern const CgsID K_INVALID_ID; (class-scope static const).
        // GetCurrentRoadID returns this when there is no current road
        // (X360 returns literal 0 -> K_INVALID_ID == 0).
        static const CgsID K_INVALID_ID;

        // The single X360-attested out-of-line function (body in the .cpp).
        // DWARF: CgsID GetCurrentRoadID() const;  (BrnRoadRulesManager.h:131)
        CgsID GetCurrentRoadID() const;

        // X360 0x82335268 -- validate a road-limit region against the built RoadRules.
        // Args: regionId (== region trigger id), limitId (region group id, else trigger id).
        // Declared-only here; body lands with the RoadRulesManager TU.
        bool IsRoadLimitRegionValid(CgsID lRegionId, CgsID lLimitId) const;

        // X360 0x82352A20 -- the player crossed a road-limit region this frame. Args:
        // road-limit region id, entry-direction flag (dot(velocity, region forward) > 0),
        // output buffer, and the per-car road-limit byte. Declared-only; body in the RoadRulesManager TU.
        void OnRoadLimit(u32 luRoadLimitRegionId, bool lbEntryDirection,
                         GameStateModuleIO::OutputBuffer* lpOutput, s32 liRoadLimit);

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

        // ---- trailing members (DecFIGS DWARF BrnRoadRulesManager.h:244..266) ----
        // Materialised through miCrashScore because the road-rules debug component reads
        // maiChallengeRoadIndex[0] (+36), mfTime (+68), mfStuntTime (+88) and miCrashScore (+96).
        // The two enum members (meActiveRoadRule / mePreviousActiveRoadRule) and mbRoadRulesNotAllowed
        // would drag in the uncommitted BrnGameState::EActiveRoadRule enum, so they are modelled as
        // offset-preserving raw storage (the X360-pinned 4/4/1-byte slots) rather than the real enum;
        // this keeps the byte offsets of the materialised members exact. The natural C++ alignment of
        // the CgsID (8-byte) and the f32 run reproduces the X360 layout:
        //   maiChallengeRoadIndex[2] +36 | (pad +44..+47) | mLastLimitId +48 |
        //   meActiveRoadRule +56 | mePreviousActiveRoadRule +60 | mbRoadRulesNotAllowed +64 |
        //   (pad +65..+67) | mfTime +68 | mfTimeScoreTimeout +72 | mfTimeTarget +76 |
        //   mfNextWarningTime +80 | miNumWarningsDone +84 | mfStuntTime +88 |
        //   mfStuntRuleComboTimeout +92 | miCrashScore +96
        BrnStreetData::RoadIndex maiChallengeRoadIndex[2];  // +36  (read: [0] != -1 gates the TIME line)
        CgsID                    mLastLimitId;              // +48
        s32                      meActiveRoadRule;          // +56  (EActiveRoadRule storage; not read here)
        s32                      mePreviousActiveRoadRule;  // +60  (EActiveRoadRule storage; not read here)
        bool                     mbRoadRulesNotAllowed;     // +64
        f32                      mfTime;                    // +68  <-- DecreaseCurrentTime / RenderHUD
        f32                      mfTimeScoreTimeout;        // +72
        f32                      mfTimeTarget;              // +76
        f32                      mfNextWarningTime;         // +80
        s32                      miNumWarningsDone;         // +84
        f32                      mfStuntTime;               // +88  <-- DecreaseCurrentStuntTime
        f32                      mfStuntRuleComboTimeout;   // +92
        s32                      miCrashScore;              // +96  <-- AddCrashScore

        // ---- further trailing members: declaration-only, NOT read by this slice ----
        // Per DWARF, after miCrashScore: f32 mfInRoadTimeout; bool mbAllowExitRoadRulesAfterTimeout;
        // f32 mfExitRoadRulesTime; bool mbSwitchingActive; bool mbIsOnlineMode. Omitted (not touched
        // by any recovered TU); add when building the full class.
    };
}

#endif // BRN_ROAD_RULES_MANAGER_H
