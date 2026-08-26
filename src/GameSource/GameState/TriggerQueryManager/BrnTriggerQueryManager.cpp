#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"

#include <cstddef>   // offsetof (layout asserts)
#include <stdlib.h>  // getenv ([UI-gate] arming-timeline diag; same env guard as the wQ_04 rung)
#include <cstring>   // std::memset (the 24-byte player-trigger action record)

#include "GameSource/Math/BrnMathUtils.h"   // BrnMath::IsPointInsideBox (the player-trigger stand-in)

#include "rw/math/vpu/vector3_operation.h"  // rw::math::vpu operator-/Dot/MagnitudeSquared (refresh-gate + entry-direction maths)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu (Add/Start/StopMonitor)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<13312,16> (game-action queue)

#include "SharedClasses/Trigger/BrnTriggerData.h"        // BrnTrigger::TriggerData (GetKillzone/GetRegion/GetGenericRegion/counts)
#include "SharedClasses/Trigger/BrnTriggerBase.h"        // BrnTrigger::TriggerRegion (GetType/GetRegionIndex/GetId/GetBoxRegion)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion (Type, GetGroupId/GetId, meType @0x36)
#include "SharedClasses/Trigger/BrnKillzone.h"           // BrnTrigger::Killzone (trigger/region-id tables)
#include "SharedClasses/Trigger/BrnRegion.h"             // BrnTrigger::BoxRegion (GetDimensions/GetPosition2D/ComputeDirection)

// [stuntrace waveD D1] the light-region detection stand-in's data path: the loaded lane graph ->
// its hulls -> their light-trigger boxes. Section must precede Hull (Hull embeds Section* arrays
// whose element type it needs complete), the same include order BrnTrafficData.cpp uses.
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h" // BrnTraffic::TrafficData (muNumHulls, GetHull)
#include "SharedClasses/Traffic/BrnTrafficSection.h"          // Section / LaneRung (must precede BrnTrafficHull.h)
#include "SharedClasses/Traffic/BrnTrafficHull.h"             // BrnTraffic::Hull (mpaLightTriggers, muNumLightTriggers)
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"     // BrnTraffic::LightTrigger + KU_LIGHT_TRIGGER_ID_OWNER_TAG
#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"  // BrnTraffic::KU_MAX_HULLS (the :211 assert bound)
#include <cmath>                                             // std::fabs (per-lane abs of the box dimensions)

#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"          // BrnGameState::RoadRulesManager (OnRoadLimit/IsRoadLimitRegionValid)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"           // BrnGameState::DriveThruManager (HandleDriveThru)
#include "GameSource/GameState/Offences/BrnStuntManager.h"               // BrnGameState::StuntManager (LatchJumpElement)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // GameStateModuleIO::OutputBuffer / TriggerManagementInputInterface accessors
#include "GameSource/GameState/TriggerQueryManager/BrnKillzoneAction.h"  // BrnGameState::GameStateModuleIO::KillzoneAction (new dep slice)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface (complete)

namespace BrnGameState
{

using BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface;

// ============================================================================
// Perf-monitor handles registered by Construct (X360 dword_82CDB928..938). File-scope to match
// the binary (the DWARF spells them class-scope statics miPreWorldUpdatePM..miSpikeTrigger2 at
// BrnTriggerQueryManager.h:261-266, but the X360 emits them as file-scope globals).
// ============================================================================
static s32 gsiPreWorldUpdatePM  = 0;
static s32 gsiPostWorldUpdatePM = 0;
static s32 gsiUpdateTriggersPM  = 0;
static s32 gsiSpikeTrigger1     = 0;
static s32 gsiSpikeTrigger2     = 0;

// One-shot guard: the road-limit-region validation sweep runs only once for the loaded
// TriggerData (X360 byte_82FAE278). File-scope to match the binary.
static bool gsbRoadLimitRegionsValidated = false;

// ----------------------------------------------------------------------------
// X360-baked tuning constants (recovered from the immediates the bodies use; the DWARF
// declares the named class-scope KF_* statics at BrnTriggerQueryManager.h:200/202).
//   refresh gate: squared distance > 900.0  (KF_TRIGGER_REFRESH_DISTANCE == 30.0)
//   clip radius bias: + 70.0                 (KF_TRIGGER_CLIP_DISTANCE)
// ----------------------------------------------------------------------------
static const f32 KF_TRIGGER_REFRESH_DISTANCE_SQ = 900.0f;  // flt_8200D5F8 -- refresh gate (30.0^2)
static const f32 KF_TRIGGER_CLIP_DISTANCE       = 70.0f;   // flt_820051BC -- clip radius bias

// High type-bits tag OR-ed into a region index to form a region trigger id (X360 0x38000000,
// applied as `oris r11, r11, 0x3800`).
static const u32 KU_REGION_TRIGGER_ID_TYPE_BITS = 0x38000000u;

// AddTriggerRegion query-flag the X360 passes for every region this manager submits (literal 56,
// `li r4,0x38`). It is arg1 of AddTriggerRegion; the RESOLVED region pointer is arg2.
static const s32 KI_TRIGGER_REGION_QUERY_FLAGS = 56;

// ⚠️ [FLAG PC bring-up, gateui r4 boot fix] The world-side CONSUMER of AddTriggerRegion's
// events (TriggerEntityModule's registration drain behind WorldModule::BridgeInputToEntityModules)
// does not drain on this build yet: every active-set rebuild re-posts the whole set, the
// interface's fixed EventQueue fills, and from the first long drive the boot log storms
// "EventQueue::AddEvent - Reached Max length" + "Base event queue overflow" (boot-drive
// 2026-08-20 17:21, asserts 3..24) until the run dies. The posts feed only the world's trigger
// volume queries (drive-thru/jump car-overlap detection via ProcessPlayerTriggers -- itself still
// reduced on this build); maActiveTriggers, which OnPropHit walks, is written locally below and
// is unaffected. Gate the posts OFF until the drain chain is landed and proven.
// DELETE-WHEN TriggerEntityModule's trigger-registration drain consumes the interface queue.
static const bool KB_POST_TRIGGER_REGIONS_TO_WORLD = false;

// KillzoneAction game-action: event type 110 (0x6E), record size 264 (0x108).
static const s32 KI_GAME_ACTION_KILLZONE       = 110;
static const s32 KI_KILLZONE_ACTION_EVENT_SIZE = 264;

// ----------------------------------------------------------------------------------------
// [bugwave 2026-08-23] The PLAYER-TRIGGER game action PreWorldUpdate's fan-out posts:
// event type 109 (0x6D), record size 24 (0x18) -- `li r6,0x18 / li r5,0x6D` @0x8239F804.
// ----------------------------------------------------------------------------------------
static const s32 KI_GAME_ACTION_PLAYER_TRIGGER       = 109;
static const s32 KI_PLAYER_TRIGGER_ACTION_EVENT_SIZE = 24;

// maLastPlayerTriggers / maLastFrameTriggers are Array<u16,32> (BrnTriggerQueryManager.h:173).
static const u32 KU_MAX_PLAYER_TRIGGERS_PER_FRAME = 32u;

// The 24-byte record itself. FLAG: its DWARF name/home is not recovered -- the console builds it
// on the stack inside PreWorldUpdate and no consumer of action 109 is reconstructed in this tree
// yet -- so it is modelled here exactly as the five stores the asm makes, the same treatment
// StuntManager::UpdateJumps gives its own OnJumpStart record. Pointer-free, so the X360 offsets
// hold on the x64 gate. Move to BrnGameActions.h when its consumer lands and names it.
struct PlayerTriggerAction
{
    CgsID mId;             // +0x00  std   (lwz  region+0x24, extsw)
    s32   miRegionType;    // +0x08  stw   (lbz  region+0x2A -- TriggerRegion::meType)
    s32   miGenericType;   // +0x0C  stw   (lbz  region+0x36 -- GenericRegion::meType; type 2 only)
    s32   miRegionIndex;   // +0x10  stw   (lhz  maLastPlayerTriggers[i])
    u8    mbFirstFrame;    // +0x14  stb   (FindFirstInstanceOf(maLastFrameTriggers, idx) == -1)
    u8    mauPad[3];       // +0x15..+0x17 (the console never writes these; zeroed here)
};
static_assert(sizeof(PlayerTriggerAction) == 24, "PlayerTriggerAction is the console's 24 bytes");

// ============================================================================
// X360 0x82364BF0 — BrnGameState::TriggerQueryManager::Construct
// ============================================================================
void TriggerQueryManager::Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                                    TakedownManager*                    lpTakedownManager,
                                    RoadRulesManager*                   lpRoadRulesManager)
{
    CGS_ASSERT(lpProgressionManager != NULL, "lpProgressionManager != NULL");
    CGS_ASSERT(lpTakedownManager    != NULL, "lpTakedownManager != NULL");
    CGS_ASSERT(lpRoadRulesManager   != NULL, "lpRoadRulesManager != NULL");

    // Empty every embedded array (live-count word -> 0). The reserved-preamble arrays
    // (maSoundActions/maLastPlayerTriggers/maLastFrameTriggers) are Construct'd in the full build
    // at +896/+1492/+1560/+1564; the two modelled here are the ones later code touches.
    maActiveTriggers.Construct();      // X360: stw 0 @ +1424
    mLandmarkIndexArray.Construct();   // X360: stw 0 @ +1864
    // ⭐ [bugwave 2026-08-23] DEFECT FIX, not an addition. The comment above used to say these
    // two "are Construct'd in the full build" and leave them alone -- but the X360 Construct's
    // zero-store loop covers +1492 and +1560 as well, and Array<T,N>'s live-count word carries a
    // -1 UNCONSTRUCTED sentinel until Construct/Clear runs. With the player-trigger fan-out below
    // now live, GetLength() on either of these would fire the console's own "Array used before
    // Construct/Clear was called" assert (CgsArray.h:336) on the very first frame.
    maLastPlayerTriggers.Construct();  // X360: stw 0 @ +1492
    maLastFrameTriggers.Construct();   // X360: stw 0 @ +1560

    // Cached refresh position (X360: stvx128 v0 zero store @ +1776).
    mLastPlayerPosition = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

    // Dirty/region flags + invalid traffic-light id.
    mbTriggersUpdated            = false;   // X360: stb 0 @ +1808
    mbPlayerInTrafficLightRegion = false;   // X360: stb 0 @ +1809
    mPlayerCurrentTrafficLightId = static_cast<LightTriggerId>(-1); // X360: stw -1 @ +1812 (LightTriggerId::SetInvalid)

    // Injected managers.
    mpProgressionManager = lpProgressionManager;   // X360: stw a2 @ +1816
    mpTakedownManager    = lpTakedownManager;        // X360: stw a3 @ +1820
    mpRoadRulesManager   = lpRoadRulesManager;        // X360: stw a4 @ +1824

    // Look-ahead bools (X360: stb 1 @ +1868 / +1869).
    mbDoSoundLookAheadThisFrame = true;
    mbCarHasTeleported          = true;

    // Register the five CPU perf monitors (X360: dword_82CDB928..938; 6-arg AddMonitor form). The
    // 5th (parent) arg is 1632 (0x660) -- the binary loads `li r6,0x660` for the earlier zero-store
    // loop and never reloads r6 before the five AddMonitor calls, so all five pass 1632. Profiling
    // only; no gameplay effect.
    gsiPreWorldUpdatePM  = CgsDev::PerfMonCpu::AddMonitor("TriggerQueryManager PreWorld",  5, 0, 1.0, 1632, 1);
    gsiPostWorldUpdatePM = CgsDev::PerfMonCpu::AddMonitor("TriggerQueryManager PostWorld", 5, 0, 1.0, 1632, 1);
    gsiUpdateTriggersPM  = CgsDev::PerfMonCpu::AddMonitor("Update Triggers",               5, 0, 1.0, 1632, 1);
    gsiSpikeTrigger1     = CgsDev::PerfMonCpu::AddMonitor("Spike Trigger 1",               5, 0, 1.0, 1632, 1);
    gsiSpikeTrigger2     = CgsDev::PerfMonCpu::AddMonitor("Spike Trigger 2",               5, 0, 1.0, 1632, 1);
}

// ============================================================================
// X360 0x82391FD8 — BrnGameState::TriggerQueryManager::UpdateTriggers
// ============================================================================
void TriggerQueryManager::UpdateTriggers(
        GameStateModuleIO::OutputBuffer*           lpOutput,
        const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
{
    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerData.operator->();

    // ---- 1) one-shot road-limit-region validation ----
    // ⚠️ [FLAG PC bring-up, gateui r4 boot fix] mpRoadRulesManager is set ONLY by
    // TriggerQueryManager::Construct @0x82364BF0, which nothing calls yet (GameStateModule
    // models neither mTakedownManager nor mRoadRulesManager -- the round-2 P4 park). On the
    // first UpdateTriggers the null manager AV'd inside IsRoadLimitRegionValid (read of
    // null+0x18, boot-drive 2026-08-20 17:15). Skip ONLY this once-per-track validation until
    // the real Construct lands; the assert it carries is a content-build diagnostic, not a
    // gameplay leg. DELETE-WHEN TriggerQueryManager::Construct is called with a real
    // RoadRulesManager.
    if (!gsbRoadLimitRegionsValidated && mpRoadRulesManager == 0)
    {
        static bool gsbWarnedOnce = false;
        if (!gsbWarnedOnce && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] PARK: road-limit validation skipped (mpRoadRulesManager null; "
                   "TriggerQueryManager::Construct @0x82364BF0 not yet called)\n";
            gsbWarnedOnce = true;
        }
        gsbRoadLimitRegionsValidated = true;
    }
    if (!gsbRoadLimitRegionsValidated)
    {
        const int liGenericRegionCount = lpTriggerData->GetGenericRegionCount();
        for (int liGenericRegionIndex = 0; liGenericRegionIndex < liGenericRegionCount; ++liGenericRegionIndex)
        {
            CGS_ASSERT(liGenericRegionIndex < mpTriggerData->GetGenericRegionCount(),
                       "liGenericRegionIndex < miGenericRegionCount");
            const BrnTrigger::GenericRegion* lpRegion = lpTriggerData->GetGenericRegion(liGenericRegionIndex);
            if (lpRegion->GetType() == BrnTrigger::GenericRegion::E_TYPE_ROAD_LIMIT)
            {
                // Road-limit region id == group id when set, else the trigger id.
                const CgsID lGroupId  = lpRegion->GetGroupId();
                const CgsID lRegionId = lpRegion->GetId();
                const CgsID lLimitId  = (lGroupId != 0) ? lGroupId : lRegionId;
                // X360 0x82392100-0x823921B4: the message is built dynamically via the assert
                // StrStream operator<< as "Road limit region <regionId>(<limitId>) is broken\n
                // Did you build triggers and forget to build RoadRules?" (file BrnTriggerQuery-
                // Manager.cpp, line 695). Reproduced here as the exact concatenated string.
                CGS_ASSERT(
                    mpRoadRulesManager->IsRoadLimitRegionValid(lRegionId, lLimitId),
                    "Road limit region (id)(limit) is broken\nDid you build triggers and forget to build RoadRules?");
            }
        }
        gsbRoadLimitRegionsValidated = true;
    }

    CgsDev::PerfMonCpu::StartMonitor(gsiUpdateTriggersPM);

    // The world trigger-management input interface (write-locked; X360 GetTriggerManagementInput-
    // Interface returns OutputBuffer+0x9050). AddTriggerRegion is a member of the world-side
    // BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface; the remove path posts an
    // InRemoveTriggerEvent onto its embedded remove queue (interface+131088).
    GameStateModuleIO::TriggerManagementInputInterface* lpTriggerInterface =
        lpOutput->GetTriggerManagementInputInterface();

    // ---- 2) re-submit the armed landmark regions (first un-updated frame only) ----
    // This loop runs whenever mbTriggersUpdated is clear, independent of the player being active
    // (X360 branch at 0x82392218 -- BEFORE the player-active gate).
    if (!mbTriggersUpdated)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "Updating triggers\n";
        }

        const u32 luLandmarkCount = mLandmarkIndexArray.GetLength();
        for (u32 luIndex = 0; luIndex < luLandmarkCount; ++luIndex)
        {
            const s32 liRegionIndex = static_cast<s32>(mLandmarkIndexArray.GetItem(static_cast<u8>(luIndex)));
            CGS_ASSERT(liRegionIndex < lpTriggerData->GetRegionCount(), "liRegionIndex < miRegionCount");

            // X360 0x823922D4-E8: resolve the landmark's region index to its region pointer
            // (mppRegions[idx] @ +0x74 == GetRegion(idx)) and submit (flags=56, region pointer).
            const BrnTrigger::TriggerRegion* lpRegion = lpTriggerData->GetRegion(liRegionIndex);
            if (KB_POST_TRIGGER_REGIONS_TO_WORLD)   // see the bring-up FLAG at the constant
                lpTriggerInterface->AddTriggerRegion(KI_TRIGGER_REGION_QUERY_FLAGS, lpRegion);
        }
    }

    // ---- 3) active-set rebuild: ENTIRELY gated on the player car being active ----
    // X360 0x823922F8-0x82392340: the player-active-index assert fires unconditionally, then the
    // whole rebuild block (LABEL_32 @0x823923C4) is entered only when mbIsPlayerCarActive == 1
    // (0x8239233C `cmplwi cr6, r11, 1` / 0x82392340 `bne cr6, loc_8239265C` skips it otherwise --
    // ⭐ ROUND 8: the citation used to name 0x8239233C as the branch; it is the COMPARE, and the
    // branch is the next instruction. Re-read off the export this pass). When the player is inactive UpdateTriggers does
    // nothing further but set mbTriggersUpdated=true -- it does NOT rebuild even on the first
    // un-updated frame.
    CGS_ASSERT(lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    const bool lbPlayerCarActive = lpActiveRaceCarInterface->IsPlayerCarActive();
    if (lbPlayerCarActive)
    {
        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();

        // Rebuild on the first un-updated frame; otherwise only when the player has moved farther
        // than KF_TRIGGER_REFRESH_DISTANCE (squared distance > 900.0, full 3-lane MagnitudeSquared).
        bool lbRebuild = !mbTriggersUpdated;
        if (!lbRebuild)
        {
            const Vector3 lDelta  = rw::math::vpu::operator-(lPlayerPosition, mLastPlayerPosition);
            const f32     lfDistSq = rw::math::vpu::MagnitudeSquared(lDelta);
            if (lfDistSq > KF_TRIGGER_REFRESH_DISTANCE_SQ)
            {
                lbRebuild = true;
            }
        }

        if (lbRebuild)
        {
            // Drop every currently-active region (remove-trigger events onto the interface remove queue).
            const u32 luActiveCount = maActiveTriggers.GetLength();
            for (u32 luActive = 0; luActive < luActiveCount; ++luActive)
            {
                BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent lRemoveEvent;
                lRemoveEvent.mTriggerID =
                    static_cast<u32>(maActiveTriggers[luActive]) | KU_REGION_TRIGGER_ID_TYPE_BITS;
                if (KB_POST_TRIGGER_REGIONS_TO_WORLD)   // see the bring-up FLAG at the constant --
                    lpTriggerInterface->RemoveTrigger(lRemoveEvent);   // the REMOVE twin of the parked
                                                                       // AddTriggerRegion posts (boot-drive
                                                                       // 2026-08-20 18:18: 285 overflow
                                                                       // asserts from this producer's queue)
            }

            CgsDev::PerfMonCpu::StartMonitor(gsiSpikeTrigger2);

            // Empty the active set and rebuild it from the regions near the player.
            maActiveTriggers.Clear();

            const int liRegionCount = lpTriggerData->GetRegionCount();
            for (int liRegionIndex = 0; liRegionIndex < liRegionCount; ++liRegionIndex)
            {
                const BrnTrigger::TriggerRegion* lpTriggerRegion = lpTriggerData->GetRegion(liRegionIndex);
                // Only box-shaped (generic) regions take part in the per-frame clip test (base type == 2).
                if (lpTriggerRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                {
                    const BrnTrigger::BoxRegion* lpBoxRegion = lpTriggerRegion->GetBoxRegion();

                    // Clip radius = max(halfX, halfZ) + KF_TRIGGER_CLIP_DISTANCE, squared.
                    const f32 lfHalfX = lpBoxRegion->GetDimensionX() * 0.5f;
                    const f32 lfHalfZ = lpBoxRegion->GetDimensionZ() * 0.5f;
                    const f32 lfMaxHalfDimension = (lfHalfX <= lfHalfZ) ? lfHalfZ : lfHalfX;
                    const f32 lfClipDistance   = lfMaxHalfDimension + KF_TRIGGER_CLIP_DISTANCE;
                    const f32 lfClipDistanceSq = lfClipDistance * lfClipDistance;

                    // Horizontal (XZ) distance from the player to the box centre, squared. The X360
                    // does this as a masked-SIMD MagnitudeSquared over a vector with the vertical
                    // lane zeroed (Vector2{x,z}); reproduced here as plain scalar math on the X/Z
                    // lanes (no Vector2 operator- exists in the vpu SDK -- only Vector3).
                    const Vector2 lTriggerPosition2D = lpBoxRegion->GetPosition2D();   // {x = posX, y = posZ}
                    const f32 lfOffsetX = lTriggerPosition2D.x - lPlayerPosition.x;
                    const f32 lfOffsetZ = lTriggerPosition2D.y - lPlayerPosition.z;
                    const f32 lfDistSq  = (lfOffsetX * lfOffsetX) + (lfOffsetZ * lfOffsetZ);

                    if (lfDistSq < lfClipDistanceSq)
                    {
                        if (KB_POST_TRIGGER_REGIONS_TO_WORLD)   // see the bring-up FLAG at the constant
                            lpTriggerInterface->AddTriggerRegion(KI_TRIGGER_REGION_QUERY_FLAGS, lpTriggerRegion);
                        maActiveTriggers.Append(static_cast<u16>(liRegionIndex));
                    }
                }
            }

            CgsDev::PerfMonCpu::StopMonitor(gsiSpikeTrigger2);

            // Cache the player position used for this rebuild.
            mLastPlayerPosition = lPlayerPosition;

            // -----------------------------------------------------------------------------
            // [DIAG] NOT IN THE X360 BINARY -- the gateui ARMING TIMELINE.
            //
            // ⭐ ROUND-8 CORRECTION. The round-7 banner that stood here justified this rung with
            // "the round-7 brief's whole defect-A premise (an armed-set warm-up race) could be
            // neither confirmed nor killed". That is FALSE and is removed. The run-9 log KILLED
            // that premise: for the first smashed gate, StuntManager::OnPropHit was never called
            // at all (no `[UI-gate] bridged prop-hit` and no `[UI-gate] OnPropHit` line exists for
            // it -- BrnGame.log:4720-4745), so whatever maActiveTriggers held at that moment is
            // causally irrelevant to the first-gate failure. The break is upstream, in the world
            // module: PropEntityModule ProcessContacts' LEG-1 gate, which round 8 instruments
            // directly (PropEntityModule_wQ2_03.cpp, the "[prop-diag] LEG1 REJECT" rung).
            //
            // WHAT THIS RUNG IS, THEN: general arming instrumentation, not evidence for or
            // against defect A. The round-6 ladder had exactly ONE arming rung -- the
            // `[UI-gate] armed` one-shot in GameStateModule_gUI_00.cpp, which fires on the FIRST
            // non-empty active set and never again; on the run-9 drive that shot landed in the
            // junk yard (`armed smash=0 billboard=0 of=3`, BrnGame.log:863) and the log then said
            // nothing about arming for the rest of the drive. This rung reports EVERY rebuild of
            // the active set -- the only event that can change what OnPropHit walks -- with the
            // player position the rebuild was keyed on and the SMASH/BILLBOARD census of the
            // resulting set. It is what you read when a prop-hit event DOES reach OnPropHit and
            // latches `none`; correlate the positions against the `[prop-diag] contact` /
            // `[Q6-world] first part ... pos` lines.
            //
            // BUDGET (the PREAMBLE's "keep the ladder readable" rule). Three windows:
            //   * the first KI_UI_GATE_REBUILD_DIAG_FIRST_N rebuilds;
            //   * one extra line the first time a SMASH region enters the set (FIRST-SMASH-ARMED);
            //   * ⭐ ROUND 8: the KI_UI_GATE_REBUILD_DIAG_AFTER_SMASH rebuilds AFTER that, because
            //     the one-shot alone is spendable on the wrong region -- 400 of the world's 4670
            //     generic regions are SMASH ([UI-gate] prepare tally, BrnGame.log:217) and the
            //     clip radius is max(halfX,halfZ)+70, so an arbitrary early smash region burns the
            //     shot and the rebuild that arms the gate you care about prints nothing.
            // ⚠️ Do NOT read the first-N window as route coverage. A rebuild needs >30 u of travel
            // FROM THE PREVIOUS REBUILD POSITION, so N rebuilds is a lower bound of 30*N u of net
            // displacement and an unbounded amount of actual driving; nothing here establishes
            // that it reaches any particular gate. (The round-7 banner asserted "covers the whole
            // junk-yard exit and the first ~240 m"; that was unsupported and is withdrawn.)
            // The census loop itself now stops running once all three windows are spent, so a
            // long BRN_PROP_DIAG run pays nothing per rebuild after that.
            // ⚠️ PERF: what remains runs INSIDE the gsiUpdateTriggersPM monitored span
            // (StopMonitor(gsiUpdateTriggersPM) is after this block), so UpdateTriggers' perf
            // number is inflated while BRN_PROP_DIAG is set. Do not profile with it on.
            //
            // Same logger and same env guard (BRN_PROP_DIAG) as the `[prop-diag] BREAK` rung this
            // ladder hangs off (PropEntityModule_wQ_04.cpp).
            // -----------------------------------------------------------------------------
            {
                static const bool sbDiag              = (getenv("BRN_PROP_DIAG") != 0);
                static s32        siRebuildCount      = 0;
                static bool       sbFirstSmashLogged  = false;
                static s32        siPostSmashLinesLeft = 0;
                const s32         KI_UI_GATE_REBUILD_DIAG_FIRST_N     = 8;
                const s32         KI_UI_GATE_REBUILD_DIAG_AFTER_SMASH = 8;

                // Once every window is spent there is nothing left to print, so skip the census
                // walk entirely rather than paying it on every rebuild for the life of the run.
                const bool lbCensusStillWanted =
                    (siRebuildCount < KI_UI_GATE_REBUILD_DIAG_FIRST_N)
                    || !sbFirstSmashLogged
                    || (siPostSmashLinesLeft > 0);

                if (sbDiag && lbCensusStillWanted && CgsDev::Log::gpDebugPrint != 0)
                {
                    const u32 luArmedCount = maActiveTriggers.GetLength();
                    s32 liSmash     = 0;
                    s32 liBillboard = 0;
                    for (u32 luArmed = 0; luArmed < luArmedCount; ++luArmed)
                    {
                        const BrnTrigger::TriggerRegion* lpArmedRegion =
                            lpTriggerData->GetRegion(maActiveTriggers[luArmed]);
                        if (lpArmedRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                        {
                            continue;
                        }
                        const BrnTrigger::GenericRegion* lpArmedGeneric =
                            static_cast<const BrnTrigger::GenericRegion*>(lpArmedRegion);
                        if (lpArmedGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_SMASH)
                        {
                            ++liSmash;
                        }
                        else if (lpArmedGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST)
                        {
                            ++liBillboard;
                        }
                    }

                    const bool lbFirstSmashNow = (!sbFirstSmashLogged && liSmash > 0);
                    if (lbFirstSmashNow)
                    {
                        sbFirstSmashLogged   = true;
                        siPostSmashLinesLeft = KI_UI_GATE_REBUILD_DIAG_AFTER_SMASH;
                    }

                    bool lbPrintLine = (siRebuildCount < KI_UI_GATE_REBUILD_DIAG_FIRST_N)
                                       || lbFirstSmashNow;
                    if (!lbPrintLine && siPostSmashLinesLeft > 0)
                    {
                        --siPostSmashLinesLeft;
                        lbPrintLine = true;
                    }

                    if (lbPrintLine)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[UI-gate] trig rebuild #" << siRebuildCount
                            << " pos=(" << lPlayerPosition.x
                            << "," << lPlayerPosition.y
                            << "," << lPlayerPosition.z
                            << ") armed=" << static_cast<s32>(luArmedCount)
                            << " smash=" << liSmash
                            << " billboard=" << liBillboard
                            << (lbFirstSmashNow ? " FIRST-SMASH-ARMED\n" : "\n");
                    }
                    ++siRebuildCount;
                }
            }
        }
    }
    else
    {
        // [DIAG] NOT IN THE X360 BINARY. The one-shot twin of the rung above: the console skips
        // the ENTIRE rebuild while the player car is inactive (asm 0x82392340 `bne cr6,
        // loc_8239265C`; 0x8239233C is the `cmplwi cr6, r11, 1` it branches on -- ⭐ ROUND 8
        // corrected an off-by-one-instruction citation here and at the gate above),
        // so a log with no `trig rebuild` lines at all is answered here -- "the pump ran, the
        // player car was never active" -- rather than by silence. One line per process.
        static const bool sbDiag             = (getenv("BRN_PROP_DIAG") != 0);
        static bool       sbInactiveLogged   = false;
        if (sbDiag && !sbInactiveLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbInactiveLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] trig update SKIPPED: player car inactive (no rebuild)\n";
        }
    }

    mbTriggersUpdated = true;
    CgsDev::PerfMonCpu::StopMonitor(gsiUpdateTriggersPM);
}

// ============================================================================
// X360 0x8239BF80 — BrnGameState::TriggerQueryManager::ProcessPlayerTriggers
// ============================================================================
void TriggerQueryManager::ProcessPlayerTriggers(
        bool                                        lbFirstFrame,
        const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
        const BrnTrigger::TriggerRegion*            lpTriggerRegion,
        GameStateModuleIO::OutputBuffer*            lpOutput,
        StuntManager*                               lpStuntManager,
        DriveThruManager*                           lpDriveThruManager,
        const BrnResource::VehicleList*             lpVehicleList)
{
    // Gate: only newly-entered (lbFirstFrame) generic-region hits route anywhere.
    if (!lbFirstFrame || lpTriggerRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
    {
        return;
    }

    const BrnTrigger::GenericRegion* lpGenericRegion =
        static_cast<const BrnTrigger::GenericRegion*>(lpTriggerRegion);

    switch (lpGenericRegion->GetType())
    {
        case BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD:
        case BrnTrigger::GenericRegion::E_TYPE_GAS_STATION:
        case BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP:
        case BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP:
        case BrnTrigger::GenericRegion::E_TYPE_CAR_PARK:
        {
            // ⛔⛔ [gateui r4] PARK (verify_r3_fix3gsm F2) -- THE DRIVE-THRU LEG IS REMOVED BEHIND
            // THIS FLAG, exactly the way the E_TYPE_ROAD_LIMIT arm below was parked in round 3.
            // It is NOT fabricated. The console arm is one call, transcribed from the asm:
            //
            //     // 0x8239BF80 BrnGameState::TriggerQueryManager::ProcessPlayerTriggers,
            //     // switch (*(a4 + 54)) == lpGenericRegion->GetType(), cases 0..4:
            //     BrnGameState::DriveThruManager::HandleDriveThru(a7, a4, a3, a8, a5);
            //     //   a7 = lpDriveThruManager (this)   a4 = lpGenericRegion
            //     //   a3 = lpActiveRaceCarInterface    a8 = lpVehicleList   a5 = lpOutput
            //
            // i.e. exactly
            //     lpDriveThruManager->HandleDriveThru(lpGenericRegion, lpActiveRaceCarInterface,
            //                                         lpVehicleList, lpOutput);
            //
            // WHY IT IS PARKED, measured rather than argued:
            // `DriveThruManager::HandleDriveThru` @0x8239B010 DOES have a body --
            // `GameSource/GameState/Offences/BrnDriveThruManager.cpp` -- but that TU DOES NOT
            // COMPILE, so it cannot be mounted and the call is an unconditional LNK2019 in the
            // mandatory `BrnTriggerQueryManager.cpp` mount. Re-measured 2026-08-20 with
            // `selfcheck.py`:
            //     BrnDriveThruManager.h(76,106,111,118): C2039/C2061 "GameActionQueue" is not a
            //         member of BrnGameState::GameStateModuleIO -- the header forward-declares
            //         only `struct OutputBuffer` and never includes BrnGameStateModuleIO.h,
            //         where the typedef lives.
            //     BrnDriveThruManager.cpp(403,404): C2440/C2664  BrnGameState::EActiveRaceCarIndex
            //         vs the global EActiveRaceCarIndex.
            //     BrnDriveThruManager.cpp(487,497): C2511/C2597 downstream of the header errors.
            // Fixing the header is small, but landing that TU is NOT a one-file job: it drags the
            // SIX bodiless training symbols this wave has already parked twice (the identical list
            // `StuntManager_gUI_00.cpp :: ProcessStuntElement` names) --
            //     TrainingManager::IsTipPending            (BrnTrainingManager.h:108)
            //     TrainingManager::IsTipAllowedInGameMode  (:123)
            //     TrainingManager::GetProfile              (:112)
            //     TrainingManager::GetTimeSinceLastTip     (:115)
            //     TrainingManager::RequestTip              (:118)
            //     Profile::HasPlayerSeenTrainingType       (BrnProfile.h:468)
            // -- reached from `BrnDriveThruManager.cpp :: TryPlayTrainingTip`, which its junk-yard /
            // gas / paint / car-park arms all call. None has a body or a link stub anywhere in
            // b5-decomp/src.
            //
            // ⓘ COST OF THE PARK: driving through a junk yard / gas station / body shop / paint
            // shop / car park stops opening that shop's flow. That is a REAL behavioural loss --
            // and it is off this wave's path (drive-thru regions post no stunt element, touch
            // neither the StuntManager latch nor game action 58). Without the park
            // `BrnTriggerQueryManager.cpp` cannot be mounted at all, which costs the wave BOTH
            // `[UI-gate] OnPropHit ... latch=` (OnPropHit walks maActiveTriggers, written only by
            // this file's UpdateTriggers) AND everything downstream of it.
            // RESTORE-WHEN BrnDriveThruManager.cpp compiles and its training-tip callees land.
            (void)lpDriveThruManager;
            (void)lpVehicleList;
            break;
        }

        case BrnTrigger::GenericRegion::E_TYPE_KILLZONE:
        {
            // For each killzone whose trigger list contains this region, emit a KillzoneAction
            // carrying that killzone's region-id list onto the game-action queue. The X360 has NO
            // break after the AddEvent post (0x8239C0C8 falls through to 0x8239C0CC, ++v17/v18+=4),
            // so scanning of the killzone's remaining triggers continues and can post again if a
            // second trigger in the same killzone also matches the hit region index.
            const BrnTrigger::TriggerData* lpTriggerData = mpTriggerData.operator->();
            const int liKillzoneCount = lpTriggerData->GetKillzoneCount();
            for (int liKillzoneIndex = 0; liKillzoneIndex < liKillzoneCount; ++liKillzoneIndex)
            {
                const BrnTrigger::Killzone* lpKillzone = lpTriggerData->GetKillzone(liKillzoneIndex);

                // Does this killzone's trigger list include the hit region (matched by region index)?
                const int liTriggerCount = lpKillzone->GetTriggerCount();
                for (int liTriggerIndex = 0; liTriggerIndex < liTriggerCount; ++liTriggerIndex)
                {
                    const BrnTrigger::GenericRegion* lpKillzoneTrigger =
                        static_cast<const BrnTrigger::GenericRegion*>(lpKillzone->GetTrigger(liTriggerIndex));
                    if (lpKillzoneTrigger->GetRegionIndex() == lpGenericRegion->GetRegionIndex())
                    {
                        // Build the KillzoneAction's region-id list, then queue it.
                        GameStateModuleIO::KillzoneAction lAction;
                        lAction.maRegionIds.Construct();
                        const int liRegionIdCount = lpKillzone->GetRegionIdCount();
                        for (int liRegionId = 0; liRegionId < liRegionIdCount; ++liRegionId)
                        {
                            lAction.maRegionIds.Append(lpKillzone->GetRegionId(liRegionId));
                        }

                        // The game-action queue is OutputBuffer's VariableEventQueue<13312,16>
                        // (X360 GetGameActionQueue returns this+4; the AddEvent at 0x8233FAE8 is the
                        // <13312,16> instantiation). The committed accessor returns the opaque
                        // forward-declared GameActionQueue*, so reinterpret to the real queue type.
                        CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue =
                            reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(
                                lpOutput->GetGameActionQueue());
                        lpGameActionQueue->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lAction),
                            KI_GAME_ACTION_KILLZONE,
                            KI_KILLZONE_ACTION_EVENT_SIZE);
                        // NO break -- the X360 keeps scanning this killzone's remaining triggers.
                    }
                }
            }
            break;
        }

        case BrnTrigger::GenericRegion::E_TYPE_JUMP:
        {
            // Latch this region as the StuntManager's pending jump element if one is not already set
            // (X360 case 7: if (!stunt->mbJumpActive @+1556) stunt->mpLastJumpElement @+1544 = region).
            lpStuntManager->LatchJumpElement(lpGenericRegion);
            break;
        }

        case BrnTrigger::GenericRegion::E_TYPE_ROAD_LIMIT:
        {
            // ⛔⛔ [gateui] ROUND-3 PARK (verify_r2_fixgsm F3a) -- THE WHOLE ROAD-LIMIT LEG IS
            // REMOVED BEHIND THIS FLAG, the way PreWorldUpdate's other legs were reduced. It is
            // NOT fabricated, and it is recorded here rather than deleted quietly. The console arm is:
            //
            //     if (!lpActiveRaceCarInterface->IsPlayerCarActive()) return;
            //     lVelocity        = lpActiveRaceCarInterface->GetPlayerLinearVelocity();
            //     lRegionDirection = lpGenericRegion->GetBoxRegion()->ComputeDirection();
            //     lbEntryDirection = Dot(lVelocity, lRegionDirection) > 0.0f;
            //     liRoadLimit      = <player RaceCarState::mbResetCarTransform, X360
            //                          *(1120*playerIndex + interface + 1914)>   (role unverified)
            //     luRoadLimitRegionId = GetGroupId() ? GetGroupId() : GetId();
            //     mpRoadRulesManager->OnRoadLimit(luRoadLimitRegionId, lbEntryDirection,
            //                                     lpOutput, liRoadLimit);
            //
            // WHY IT IS PARKED, measured off the export set rather than argued:
            // `RoadRulesManager::OnRoadLimit` @0x82352A20 has no body anywhere in b5-decomp/src and
            // no link stub stands in, and bodying it is not a one-function job -- it calls FOUR
            // further RoadRulesManager methods that are equally bodiless, ~400 instructions in all:
            //     OnEndRule    @0x823507C0  ( 79 insns)  -> RoadRulesManager::OnScoreCompleted
            //     OnStartRule  @0x82348398  (210 insns)  -> StreetManager::GetChallengeParScore /
            //                                               GetChallengeUserScore /
            //                                               GetChallengeFriendHighScore,
            //                                               ChallengeParScoresEntry::GetScore,
            //                                               ChallengeHighScoreEntry::GetScore
            //     OnLeaveRoad  @0x82348320  ( 30 insns)
            //     OnEnterRoad  @0x823481E8  ( 78 insns)  -> StreetManager::GetParRivalId,
            //                                               ChallengeParScoresEntry::Copy
            // -- none of which exists either, and all of which write the road-rules timing/score
            // state that BrnRoadRulesManager.h currently models only as offset-preserving raw
            // storage (meActiveRoadRule / mePreviousActiveRoadRule are `s32` stand-ins for an
            // EActiveRoadRule enum with no committed home). That is a whole subsystem, and landing
            // any part of it would ADD net unresolved externals -- the exact failure mode
            // verify_gsm/VERDICT.md F2 fails this wave for.
            //
            // ⓘ COST OF THE PARK: road-limit regions stop starting/ending Road Rules challenges.
            // That is a REAL behavioural loss and it is recorded as such -- but it is off the smash
            // gate / billboard HUD path this wave proves end to end (road limits post no stunt
            // element and touch neither the StuntManager latch nor game action 58), and without the
            // park `BrnTriggerQueryManager.cpp` cannot be mounted at all, which costs the wave
            // BOTH `[UI-gate] OnPropHit ... latch=` (OnPropHit walks maActiveTriggers, written only
            // by this file's UpdateTriggers) AND everything downstream of it.
            // ⓘ The SIBLING road-rules leg is NOT parked: `RoadRulesManager::IsRoadLimitRegionValid`
            // @0x82335268 is bodied console-exact this round (BrnRoadRulesManager.cpp), so
            // UpdateTriggers' once-per-track "did you build triggers and forget to build RoadRules?"
            // validation above is live.
            // RESTORE-WHEN the four RoadRulesManager rule/road bodies land.
            break;
        }

        default:
            // Other generic-region categories are not routed by this dispatcher.
            break;
    }
}

// ============================================================================
// [FLAG PC bring-up] NOT IN THE X360 BINARY -- the light-region box scan.
//
// [stuntrace waveD, agent D1] THIS STANDS IN FOR TriggerQueryManager::PostWorldUpdate
// @0x82386BD8's OWNER-57 ARM, and for nothing else. What the console does there, verbatim:
//
//   0x82386CDC  stb  r18(0), 0x711(r27)   ; mbPlayerInTrafficLightRegion = false   \ EVERY frame,
//   0x82386CE0  stw  r10(-1), 0x714(r27)  ; mPlayerCurrentTrafficLightId = -1      / before the walk
//   ... then, for each 32-bit result word in the world's trigger LINE-TEST result queue
//       (CgsModule::VariableEventQueue<1024,16>, filled by the TriggerEntityModule):
//   0x82386F54  cmplwi r28, 0x39          ; owner byte == 57 -> a TRAFFIC-LIGHT trigger
//                                         ;   (56 == a TriggerData region -> the OTHER arm)
//   0x82386F5C  clrlwi r11, r25, 24 / beq ; ONLY when the result belongs to the PLAYER's car
//   0x82386F68  stb  r23(1), 0x711(r27)   ; mbPlayerInTrafficLightRegion = true
//   0x82386F74  insrwi r11, 0x39, 8,0     ; \ the TriggerId base-class pair, SetOwner then SetId:
//   0x82386F80  insrwi r11, r29,  24,8    ; / mPlayerCurrentTrafficLightId = 0x39000000 | (id & 0xFFFFFF)
//
// So the console DERIVES BOTH MEMBERS FROM SCRATCH EVERY FRAME -- there is no latch to clear and
// no "leave" event; "not in a region this frame" is just the top-of-function reset surviving.
// This stand-in reproduces that shape exactly, and the only thing it replaces is WHERE the hit
// comes from: instead of a line-test result queue it tests the player's position against the
// loaded lane graph's light-trigger boxes directly.
//
// WHY A STAND-IN AT ALL: every stage of the console's producer chain is inert on this build (the
// five "[FLAG PC boot gate] ... inert" lines the sibling banner below lists verbatim), and
// TriggerQueryManager::SubmitTriggerQueries @0x82392680 -- the request half -- has no body here
// either. Nothing on PC has ever written mbPlayerInTrafficLightRegion, so
// GetPlayerCurrentTrafficLightId's own CGS_ASSERT(IsPlayerInTrafficLightRegion()) fires the
// moment anything asks, and the whole offline event-start chain behind it is unreachable.
//
// THE BOXES. BrnTraffic::TrafficEntityModule::ManageTriggers @0x82747518 is the console's sole
// producer of owner-57 triggers: it walks every streamed-in hull, then every entry of that hull's
// mpaLightTriggers (Hull +0x34, stride 32, count at Hull +0x0E), and registers each one with the
// world as a box trigger whose id is `(hull << 8) | 0x39000000 | lightTriggerIndex`
// (0x827477EC/F8/FC). This scan walks the SAME arrays and packs the SAME id, so the value it
// latches is bit-identical to the one the console's queue would have carried.
//
// ⚠️ FULL EXTENTS, HALVED HERE. ManageTriggers passes abs(mDimensions) through unchanged and the
// WORLD halves them -- TriggerEntityModule::ProcessAddTriggerEvents @0x822D9520..0x822D9554:
// `vspltisw128 v126,1 ; vcsxwfp128 v127,v126,1` (== 0.5), `vmulfp128 v1,v0,v127`, then
// rw::collision::BoxVolume::Initialize. BrnMath::IsPointInsideBox takes HALF extents, so the
// `* 0.5f` below is what makes this box the same size as the console's. Dropping it would make
// every junction twice as wide in each axis and fire the banner from the next street over.
//
// ⚠️ POINT TEST vs SWEPT LINE. The console line-tests the car's travel this frame; this is a point
// test at the car's origin. A box thinner than one frame of travel could be tunnelled through --
// but the shipped light triggers are 6..56 m by 12 m by 48..104 m (measured over all 443 records
// in B5TRAFFIC.BNDL), so nothing here is close to that. Stated rather than hidden, exactly as the
// sibling region stand-in below states it.
//
// ⚠️ FIRST HIT WINS. Overlapping approach boxes of the SAME junction all resolve through
// Hull::mpaLightTriggerJunctionLookup to the SAME JunctionLogicBox, so which one is latched does
// not change the junction that comes out; the console's own answer is "whichever result the queue
// happened to hold last", which is no more principled. Documented so nobody reads the `break` as
// a considered priority rule.
//
// COST: 443 boxes on this track, each one Y-rotation-only transform + a squared-distance reject,
// with BrnMath::IsPointInsideBox reached only by survivors. Measured need is 443; there is no cap
// and no cache, deliberately -- a cache would need invalidating on every track load and this is
// code that exists to be deleted.
//
// DELETE-WHEN the real trigger line-test pipeline lands (TriggerEntityModule::PreSceneUpdate /
// PostSceneUpdate / PrePhysicsUpdate + TriggerQueryManager::SubmitTriggerQueries) and
// TriggerQueryManager::PostWorldUpdate @0x82386BD8 is mounted as the real producer: then delete
// this function AND its call site in stage (0b) below, and the owner-57 arm is the console's
// verbatim.
// ============================================================================
static bool FindLightTriggerContainingPoint(const BrnTraffic::TrafficData* lpTrafficData,
                                            const Vector3&                 lrPoint,
                                            u32&                           lruOutTriggerId)
{
    for (u32 luHull = 0; luHull < lpTrafficData->muNumHulls; ++luHull)
    {
        const BrnTraffic::Hull* lpHull = lpTrafficData->GetHull(luHull);
        if (lpHull == 0 || lpHull->mpaLightTriggers == 0)
        {
            continue;
        }

        // ManageTriggers' own loop bound (`lbz r11, 0xE(r26)` @0x8274787C).
        const u32 luTriggerCount = lpHull->muNumLightTriggers;
        for (u32 luTrigger = 0; luTrigger < luTriggerCount; ++luTrigger)
        {
            const BrnTraffic::LightTrigger& lrTrigger = lpHull->mpaLightTriggers[luTrigger];

            // `vandc128 v0, v127, <0x80000000 splat>` @0x82747834 -- per-lane fabs, then the
            // world's own 0.5 (see the banner) to reach the half extents IsPointInsideBox wants.
            const Vector3 lDimensions = lrTrigger.GetDimensions();
            const Vector3 lHalfExtents = { std::fabs(lDimensions.x) * 0.5f,
                                           std::fabs(lDimensions.y) * 0.5f,
                                           std::fabs(lDimensions.z) * 0.5f,
                                           0.0f };

            // ExpandPosPlusYRotToTransform(mPosPlusYRot) -- the console's own
            // `bl BrnTraffic::ExpandPosPlusYRotToTransform` @0x82747800. Its translation row IS
            // the packed lane, so Pos() is the box centre with no extra accessor.
            const Matrix44Affine lTransform = lrTrigger.GetTransform();

            // Conservative broadphase (NOT the console's -- it has none, the scene manager's
            // spatial partition does this job). The sum of the three HALF extents is >= the box's
            // bounding-sphere radius, so this can only over-accept; it exists to keep the
            // seven-assert IsPointInsideBox off the other 442 boxes.
            const Vector3 lCentre  = lTransform.Pos();
            const f32 lfDeltaX = lCentre.x - lrPoint.x;
            const f32 lfDeltaY = lCentre.y - lrPoint.y;
            const f32 lfDeltaZ = lCentre.z - lrPoint.z;
            const f32 lfDistSq = lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
            const f32 lfRadius = lHalfExtents.x + lHalfExtents.y + lHalfExtents.z;
            if (lfDistSq > (lfRadius * lfRadius))
            {
                continue;
            }

            if (BrnMath::IsPointInsideBox(lTransform, lrPoint, lHalfExtents))
            {
                // BrnTraffic::LightTriggerId::Set(luHull, luTrigger), inlined by the console at
                // 0x827477EC..0x827477FC. Its two bounds asserts are baked at
                // BrnTrafficLightTrigger.h:211/212 and are reproduced here because this code is
                // the one that PACKS the handle -- the shipped data satisfies both (315 hulls,
                // <= 16 triggers per hull), so a fire means the lane graph changed shape.
                CGS_ASSERT(luHull < BrnTraffic::KU_MAX_HULLS, "luHull < KU_MAX_HULLS");
                CGS_ASSERT(luTrigger < 256u, "luLightTriggerIndex < 256");

                lruOutTriggerId = (luHull << 8) | BrnTraffic::KU_LIGHT_TRIGGER_ID_OWNER_TAG | luTrigger;
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// X360 0x8239F5C8 - BrnGameState::TriggerQueryManager::PreWorldUpdate, ITS PLAYER-TRIGGER
// FAN-OUT LEG.  [bugwave 2026-08-23 -- THE SUPER-JUMP ROOT CAUSE]
//
// ROOT CAUSE THIS CLOSES, measured rather than argued. Before this landed,
// `grep -rn ProcessPlayerTriggers b5-decomp/src` found the DEFINITION and nothing else: the
// function had no caller anywhere in the tree. It is the ONLY thing in the image that calls
// StuntManager::LatchJumpElement (console case 7 @0x8239C1F8), which is the ONLY writer of
// StuntManager::mpLastJumpElement (+1544). StuntManager::Update runs UpdateJumps only under
// `if (mpLastJumpElement)`, so on this build the ENTIRE jump state machine -- the take-off
// gate, game action 56 (OnJumpStart, the camera request), game action 57 (ShowJumpName), the
// 0.5 s landing settle and its ProcessStuntElement(lbIsJump=true) that increments the
// super-jump tally -- never executed once. Both halves of the user report ("super jumps do not
// get counted at all. camera is also not firing") hang off this one missing call.
// The SMASH/BILLBOARD half of the same ladder survived because it enters through a completely
// different door: RecordPropHitEvent -> ProcessGameEvents case 111 -> StuntManager::OnPropHit,
// which walks maActiveTriggers directly and never touches ProcessPlayerTriggers.
//
// THE CONSOLE'S BODY, leg by leg (r29 == this):
//   0x8239F630  UpdateTriggers(this, lpOutput, lpActiveRaceCarInterface)     [already mounted,
//               in GameStateModule_gUI_00.cpp :: PreWorldUpdateStuntBringUp]
//   0x8239F63C  SubmitTriggerQueries(this, lpOutput, lpActiveRaceCarInterface)  [PARKED, see (P1)]
//   0x8239F650  the 8-slot loop caching each active car position into
//               maActiveRaceCarPosLastFrame[] (this+1632)                       [PARKED, see (P2)]
//   0x8239F714  THIS LEG: for i in [0, maLastPlayerTriggers.GetLength())
//                   liRegionIndex = maLastPlayerTriggers.GetItem(i)
//                   assert liRegionIndex < GetRegionCount()   (BrnTriggerData.h:624)
//                   lpRegion = mpTriggerData->GetRegion(liRegionIndex)
//                   record.mId          = (s64)lpRegion->GetId()         std  @+0x00 (lwz +0x24)
//                   record.miRegionType = lpRegion->GetType()            stw  @+0x08 (lbz +0x2A)
//                   if (miRegionType == 2)
//                       record.miGenericType = generic->GetType()        stw  @+0x0C (lbz +0x36)
//                   record.miRegionIndex = liRegionIndex                 stw  @+0x10 (lhz)
//                   record.mbFirstFrame  =
//                       (maLastFrameTriggers.FindFirstInstanceOf(liRegionIndex) == -1)
//                                                                         stb  @+0x14
//                   lpOutput->GetGameActionQueue()->AddEvent(&record, 109, 24)
//                   ProcessPlayerTriggers(record.mbFirstFrame, lpActiveRaceCarInterface,
//                                         lpRegion, lpOutput, lpStuntManager,
//                                         lpDriveThruManager, lpVehicleList)
//   0x8239F848  the maSoundActions drain -> game action 218 (32 bytes)          [PARKED, see (P3)]
//   0x8239F8AC  maSoundActions count = 0;  maLastFrameTriggers count = 0;
//   0x8239F8B8  maLastFrameTriggers.AppendArray(maLastPlayerTriggers);
//   0x8239F8BC  maLastPlayerTriggers count = 0;
//
// THE ONE PC BRING-UP STAND-IN, NAMED. maLastPlayerTriggers (+1428) is written in exactly ONE
// place in the whole X360 image: TriggerQueryManager::PostWorldUpdate @0x82386BD8, at
// `short_32_::Append(this + 1428, &regionIndex)` inside its walk of the PostWorldInputBuffer's
// TRIGGER LINE-TEST RESULT QUEUE (a VariableEventQueue<1024,16> of owner-56 line-test results
// produced by the world's TriggerEntityModule). EVERY stage of that producer chain is inert on
// this build -- the baseline boot log prints all five, verbatim:
//     "TriggerEntityModule::PreSceneUpdate: inert [FLAG PC boot gate]"
//     "BrnWorld::TriggerEntityModule::PostSceneUpdate: inert (body not reconstructed)"
//     "WorldModule::BridgeTriggerModuleToSceneModule_PostScene: inert (body not reconstructed)"
//     "WorldModule::BridgeSceneQueryResultsToTriggerModule_PrePhysics: inert (body not reconstructed)"
//     "BrnWorld::TriggerEntityModule::PrePhysicsUpdate: inert (body not reconstructed)"
// -- and TriggerQueryManager::SubmitTriggerQueries, the request half, has no body here either.
// So the fan-out below would walk an array that can never be non-empty.
// The stand-in fills maLastPlayerTriggers by testing the PLAYER'S WORLD POSITION against the
// armed regions in maActiveTriggers, using the SAME two-stage broadphase + IsPointInsideBox
// idiom StuntManager::OnPropHit @0x8236EE18 already uses against the same array. It is a POINT
// test where the console runs a swept LINE test from the car, so a region thinner than one
// frame of travel could be missed; jump regions are tens of metres deep, so this is sound for
// the jump ladder and is stated rather than hidden.
// DELETE-WHEN the TriggerEntityModule line-test chain lands and TriggerQueryManager::
// PostWorldUpdate @0x82386BD8 becomes the real producer: then delete stage (0) below, mount
// PostWorldUpdate, and this function is the console's leg verbatim.
//
// (P1) SubmitTriggerQueries @0x82392680 -- no body in this tree, and its only consumer is the
//      same inert TriggerEntityModule. Its absence is exactly what stage (0) stands in for.
// (P2) maActiveRaceCarPosLastFrame[8] lives inside mauReserved_AfterTrafficData (+1632..+1775),
//      which this slice does not model as members. Nothing in the mounted set reads it.
// (P3) the maSoundActions -> action-218 drain: maSoundActions (+376) is likewise reserved
//      storage here and has no producer on this build (CheckSoundActions @0x82379710 is not
//      mounted), so the loop would be over an array that is always empty. The two count-zeroing
//      stores the console makes at 0x8239F8AC for it are therefore also omitted.
// ============================================================================
void TriggerQueryManager::PreWorldUpdatePlayerTriggersBringUp(
        GameStateModuleIO::OutputBuffer*            lpOutput,
        const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
        StuntManager*                               lpStuntManager,
        DriveThruManager*                           lpDriveThruManager,
        const BrnResource::VehicleList*             lpVehicleList)
{
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");
    CGS_ASSERT(lpActiveRaceCarInterface != 0, "lpActiveRaceCarInterface != NULL");
    CGS_ASSERT(lpStuntManager != 0, "lpStuntManager != NULL");
    if (lpOutput == 0 || lpActiveRaceCarInterface == 0 || lpStuntManager == 0)
    {
        return;
    }

    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerData.operator->();
    if (lpTriggerData == 0)
    {
        return;   // Triggers.dat not bound yet (Prepare has not reached E_TRIGGER_LOAD_DONE)
    }

    CgsDev::PerfMonCpu::StartMonitor(gsiPreWorldUpdatePM);

    // ------------------------------------------------------------------------------------
    // (0) [FLAG PC bring-up] THE PRODUCER STAND-IN -- see the banner. NOT IN THE X360 BINARY.
    //     Console equivalent: TriggerQueryManager::PostWorldUpdate @0x82386BD8's
    //     `short_32_::Append(this + 1428, ...)` over the world trigger line-test results.
    // ------------------------------------------------------------------------------------
    if (lpActiveRaceCarInterface->IsPlayerCarActive())
    {
        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const u32     luArmedCount    = maActiveTriggers.GetLength();

        for (u32 luArmed = 0; luArmed < luArmedCount; ++luArmed)
        {
            // maLastPlayerTriggers is Array<u16,32>; Append past capacity fires the container's
            // own assert. The console's producer has the same bound, so stop at it.
            if (maLastPlayerTriggers.GetLength() >= KU_MAX_PLAYER_TRIGGERS_PER_FRAME)
            {
                break;
            }

            const u16 luRegionIndex = maActiveTriggers[luArmed];
            const BrnTrigger::TriggerRegion* lpArmedRegion = lpTriggerData->GetRegion(luRegionIndex);
            if (lpArmedRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
            {
                continue;
            }

            const BrnTrigger::BoxRegion* lpBoxRegion = lpArmedRegion->GetBoxRegion();

            // Conservative broadphase (NOT the console's -- OnPropHit's `0.5 * largestExtent`
            // radius is only sound for a roughly-cubic box, and a false NEGATIVE here would be
            // exactly the silent-no-jump bug this function exists to fix). The sum of the three
            // dimensions is >= the box's bounding-sphere radius under either reading of
            // GetDimensions (full extent or half extent), so it can only over-accept.
            const Vector3 lCentre  = lpBoxRegion->GetPosition();
            const f32 lfDeltaX = lCentre.x - lPlayerPosition.x;
            const f32 lfDeltaY = lCentre.y - lPlayerPosition.y;
            const f32 lfDeltaZ = lCentre.z - lPlayerPosition.z;
            const f32 lfDistSq = lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
            const f32 lfRadius = lpBoxRegion->GetDimensionX()
                               + lpBoxRegion->GetDimensionY()
                               + lpBoxRegion->GetDimensionZ();
            if (lfDistSq > (lfRadius * lfRadius))
            {
                continue;
            }

            const Vector3 lHalfExtents = lpBoxRegion->GetDimensions();

            // [FLAG PC bring-up] DEGENERATE AUTHORED BOXES. The RETAIL TriggerData carries
            // exactly three generic regions whose box dimensions are ALL NEGATIVE -- verified
            // bit-for-bit in the X360 original (D:\bp-staging\x360-retail TRIGGERS.DAT), so
            // this is authored data, not a transcode/reader defect:
            //     region 2793  id 425555  sub 11 ROAD_LIMIT        (3078.6, 4.6, -1922.9)
            //     region 3879  id 609215  sub 18 PICTURE_PARADISE  ( 475.1, 33.2,  1087.2)
            //     region 3888  id 616364  sub 18 PICTURE_PARADISE  ( 680.1, 36.6,  1049.8)
            // The console never hands these to BrnMath::IsPointInsideBox -- its only callers
            // are StuntManager::OnPropHit (sub-types 8/12 only) and the ChallengeManager --
            // so its `lBoxDimensions >= 0` asserts never see them. This stand-in is the one
            // caller that walks EVERY armed generic region (the first is 69 u off the
            // junkyard-exit route, hence the intermittent per-frame assert triplets once it
            // armed). A negative extent can satisfy no slab test ("never inside"), which is
            // also these regions' effective behaviour on the console, so skip them BEFORE
            // the assert contract instead of feeding it authored-degenerate data.
            if (lHalfExtents.x < 0.0f || lHalfExtents.y < 0.0f || lHalfExtents.z < 0.0f)
            {
                static bool sbDegenerateBoxLogged = false;
                if (!sbDegenerateBoxLogged && CgsDev::Log::gpDebugPrint != 0)
                {
                    sbDegenerateBoxLogged = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[TriggerQueryManager] armed region " << luRegionIndex
                        << " id=" << static_cast<s32>(lpArmedRegion->GetId())
                        << " has negative box dimensions (authored; see the degenerate-box"
                           " banner) -- skipped\n";
                }
                continue;
            }

            const Matrix44Affine lBoxTransform = lpBoxRegion->ComputeTransform();
            // ⛔ CONDUCTOR / FLAG, found by the waveD D1 light-region pass and NOT acted on here
            // because it changes boot-verified jump behaviour: `lHalfExtents` above is
            // BoxRegion::GetDimensions() UNSCALED, and the console's box dimensions are FULL
            // extents. TriggerEntityModule::ProcessAddTriggerEvents @0x822D9520..0x822D9554
            // multiplies InAddBoxTriggerEvent::mDimensions by 0.5 (`vspltisw128 v126,1 ;
            // vcsxwfp128 v127,v126,1 ; vmulfp128 v1,v0,v127`) before
            // rw::collision::BoxVolume::Initialize, and BrnMath::IsPointInsideBox takes HALF
            // extents -- so this test currently arms every generic region at TWICE its authored
            // size in each axis. ChallengeManager::IsPointInTriggerRegion's reconstruction
            // (BrnChallengeManager_wC_04.cpp:148) already passes `GetDimensions() * 0.5f`, i.e.
            // the two in-tree callers of the same predicate disagree. The new light-region stage
            // (0b) below uses the halved form. Decide and unify in one pass, with a jump-ladder
            // re-verify -- do not "fix" it as a drive-by.
            if (BrnMath::IsPointInsideBox(lBoxTransform, lPlayerPosition, lHalfExtents))
            {
                maLastPlayerTriggers.Append(luRegionIndex);
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // (0b) [FLAG PC bring-up] THE LIGHT-REGION STAND-IN -- NOT IN THE X360 BINARY.
    //      Replaces TriggerQueryManager::PostWorldUpdate @0x82386BD8's OWNER-57 ARM (and only
    //      that arm; the owner-56 arm's job is stage (0) above). Read the
    //      FindLightTriggerContainingPoint banner before touching anything here.
    //
    //      Both members are RE-DERIVED FROM SCRATCH, exactly as the console re-derives them:
    //      the reset pair is PostWorldUpdate's own `stb 0, 0x711` / `stw -1, 0x714`
    //      (0x82386CDC/CE0), and "the player left the region" is nothing more than that reset
    //      surviving the scan. There is no leave event to miss and no latch to age out.
    //
    //      PLAYER CAR ONLY. The console writes the pair only inside `if (v20)`, v20 being
    //      "this line-test result's owner car index == the player's active race car index"
    //      (@0x82386F5C) -- AI and traffic cars generate the same owner-57 results and must not
    //      move the player's junction. IsPlayerCarActive() + GetPlayerPosition() is that gate.
    //
    //      DELETE-WHEN PostWorldUpdate @0x82386BD8 is mounted (see the helper's DELETE-WHEN).
    // ------------------------------------------------------------------------------------
    mbPlayerInTrafficLightRegion = false;                             // stb 0, 0x711(r27)
    mPlayerCurrentTrafficLightId = static_cast<LightTriggerId>(-1);   // stw -1, 0x714(r27)

    // The traffic-lane resource is bound by Prepare's LAST stage (BrnTriggerQueryManager_Prepare
    // .cpp:253, the "[TriggerQueryManager] LOADED -- trigger=1 traffic=1" line) -- but the stage
    // word is NOT a boundness proxy: Prepare assigns mpTrafficData and sets E_TRIGGER_LOAD_DONE
    // UNCONDITIONALLY (its own diagnostic prints traffic= as HasMemoryResource() ? 1 : 0), and
    // ResourcePtr::GetMemoryResource ASSERTS then RETURNS NULL on an unbound handle
    // (CgsResourcePtr.h:246-251) -- the assert-is-not-a-guard class. So the pointer is
    // null-tested too; a failed B5TRAFFIC resolve leaves the top-of-frame reset pair standing,
    // which IS the console's "not in a region" state. (2026-08-26 verify catch.)
    if (meTriggerLoadStage == E_TRIGGER_LOAD_DONE && lpActiveRaceCarInterface->IsPlayerCarActive())
    {
        const BrnTraffic::TrafficData* lpTrafficData = GetTrafficData();
        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();

        u32 luTriggerId = 0;
        if (lpTrafficData != 0 &&
            FindLightTriggerContainingPoint(lpTrafficData, lPlayerPosition, luTriggerId))
        {
            mbPlayerInTrafficLightRegion = true;                      // stb 1, 0x711(r27)
            // `insrwi r11, 0x39, 8,0` then `insrwi r11, r29, 24,8` -- SetOwner(57) then SetId(id24).
            // The helper already returns the handle in exactly that shape; the mask/or pair is kept
            // so the console's two stores stay legible at the site that performs them.
            mPlayerCurrentTrafficLightId = static_cast<LightTriggerId>(
                (luTriggerId & 0x00FFFFFFu) | BrnTraffic::KU_LIGHT_TRIGGER_ID_OWNER_TAG);
        }
    }

    // [DIAG] NOT IN THE X360 BINARY. Edge-triggered enter/leave for the light region, with the
    // packed handle. This is THE rung that separates "the car is not standing in a junction box"
    // from "it is, and the chain above CheckIfPlayerIsAtJunctionWithAnEvent dropped it": the id
    // printed here is what TrafficData::GetJunctionLogicBoxForTrafficLight decodes as
    // hull = (id >> 8) & 0xFFFF, trigger = id & 0xFF -- cross-check it against
    // scratch/stuntrace_scout/eventdata/dump_lighttriggers.py's `lightTriggerId=` column (the
    // stunt-run test target is 0x7a08 == hull 122, trigger 8, junction 480897 / event 558269).
    // Capped so a car parked on a box boundary cannot spam the log.
    {
        static const bool sbJunctionDiag = (getenv("BRN_JUNCTION_DIAG") != 0);
        if (sbJunctionDiag)
        {
            static bool sbWasInRegion   = false;
            static u32  suLastTriggerId = static_cast<u32>(-1);
            static s32  siJunctionLines = 0;
            const s32   KI_JUNCTION_DIAG_MAX_LINES = 64;

            const u32 luCurrentId = static_cast<u32>(mPlayerCurrentTrafficLightId);
            if ((mbPlayerInTrafficLightRegion != sbWasInRegion || luCurrentId != suLastTriggerId)
                && siJunctionLines < KI_JUNCTION_DIAG_MAX_LINES
                && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siJunctionLines;
                // The log stream has no hex manipulator, so the handle is printed as its two
                // decoded halves (which is what dump_lighttriggers.py's hex column means) plus
                // the raw packed word in decimal. hull 122 / trigger 8 == 0x7a08 == the target.
                if (mbPlayerInTrafficLightRegion)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] [junction] ENTER light region: hull="
                        << static_cast<s32>((luCurrentId >> 8) & 0xFFFFu)
                        << " trigger=" << static_cast<s32>(luCurrentId & 0xFFu)
                        << " packedId(dec)=" << static_cast<s32>(luCurrentId) << "\n";
                }
                else
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] [junction] LEAVE light region (was hull="
                        << static_cast<s32>((suLastTriggerId >> 8) & 0xFFFFu)
                        << " trigger=" << static_cast<s32>(suLastTriggerId & 0xFFu) << ")\n";
                }
            }
            sbWasInRegion   = mbPlayerInTrafficLightRegion;
            suLastTriggerId = luCurrentId;
        }
    }

    // ------------------------------------------------------------------------------------
    // (1) THE CONSOLE'S FAN-OUT (0x8239F714..0x8239F83C), verbatim.
    // ------------------------------------------------------------------------------------
    CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue =
        reinterpret_cast<CgsModule::VariableEventQueue<13312, 16>*>(lpOutput->GetGameActionQueue());

    const u32 luPlayerTriggerCount = maLastPlayerTriggers.GetLength();
    for (u32 luTrigger = 0; luTrigger < luPlayerTriggerCount; ++luTrigger)
    {
        const u16 luRegionIndex = maLastPlayerTriggers.GetItem(luTrigger);

        CGS_ASSERT(static_cast<s32>(luRegionIndex) < lpTriggerData->GetRegionCount(),
                   "liRegionIndex < miRegionCount");   // BrnTriggerData.h:624

        const BrnTrigger::TriggerRegion* lpRegion = lpTriggerData->GetRegion(luRegionIndex);

        // The 24-byte PLAYER-TRIGGER game action (id 109). Field offsets are the console's own
        // stack record (base == r1 + var_B0): std @+0x00, stw @+0x08, stw @+0x0C, stw @+0x10,
        // stb @+0x14. The console leaves +0x0C stale when the region is not generic and never
        // writes +0x15..+0x17; zeroed here so the record is deterministic.
        PlayerTriggerAction lAction;
        std::memset(&lAction, 0, sizeof(lAction));
        lAction.mId          = lpRegion->GetId();                                  // lwz +0x24, extsw
        lAction.miRegionType = static_cast<s32>(lpRegion->GetType());              // lbz +0x2A
        if (lpRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
        {
            lAction.miGenericType = static_cast<s32>(
                static_cast<const BrnTrigger::GenericRegion*>(lpRegion)->GetType());   // lbz +0x36
        }
        lAction.miRegionIndex = static_cast<s32>(luRegionIndex);
        // `subf r11, r11, r27(-1) ; cntlzw ; extrwi 1,26` == (FindFirstInstanceOf(..) == -1).
        lAction.mbFirstFrame  = (maLastFrameTriggers.FindFirstInstanceOf(luRegionIndex) == -1);

        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                    KI_GAME_ACTION_PLAYER_TRIGGER,
                                    KI_PLAYER_TRIGGER_ACTION_EVENT_SIZE);

        ProcessPlayerTriggers(lAction.mbFirstFrame, lpActiveRaceCarInterface, lpRegion,
                              lpOutput, lpStuntManager, lpDriveThruManager, lpVehicleList);
    }

    // [DIAG] NOT IN THE X360 BINARY. First-N: what the fan-out routed. This is the rung that
    // separates "the player never entered a jump region" from "the region was entered and the
    // StuntManager dropped it" -- pair it with the `[jump-ladder]` rungs in BrnStuntManager.cpp.
    if (luPlayerTriggerCount != 0)
    {
        static s32 siFanOutLines = 0;
        const s32  KI_FANOUT_DIAG_FIRST_N = 12;
        if (siFanOutLines < KI_FANOUT_DIAG_FIRST_N && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siFanOutLines;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] player-trigger fan-out: hits="
                << static_cast<s32>(luPlayerTriggerCount) << " genericTypes=";
            for (u32 luDiag = 0; luDiag < luPlayerTriggerCount; ++luDiag)
            {
                const BrnTrigger::TriggerRegion* lpDiagRegion =
                    lpTriggerData->GetRegion(maLastPlayerTriggers.GetItem(luDiag));
                s32 liGenericType = -1;
                if (lpDiagRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                {
                    liGenericType = static_cast<s32>(
                        static_cast<const BrnTrigger::GenericRegion*>(lpDiagRegion)->GetType());
                }
                *CgsDev::Log::gpDebugPrint << " " << liGenericType;
            }
            *CgsDev::Log::gpDebugPrint << " (7 == E_TYPE_JUMP)\n";
        }
    }

    // ------------------------------------------------------------------------------------
    // (2) THE CONSOLE'S TAIL (0x8239F8AC..0x8239F8BC): this frame's set becomes last frame's.
    // ------------------------------------------------------------------------------------
    maLastFrameTriggers.Clear();                          // stw 0, 0x618(r29)
    maLastFrameTriggers.AppendArray(maLastPlayerTriggers);
    maLastPlayerTriggers.Clear();                         // stw 0, 0x5D4(r29)

    CgsDev::PerfMonCpu::StopMonitor(gsiPreWorldUpdatePM);
}

// ============================================================================
// Previously-committed functions of this class (kept; mLandmarkIndexes renamed to the DWARF
// member spelling mLandmarkIndexArray -- BrnTriggerQueryManager.h:245).
// ============================================================================

// X360 0x82326538.
void TriggerQueryManager::ClearLandmarkIndexesForGameMode(
    CgsModule::EventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent, 256>& lrRemoveTriggerQueue)
{
    for (u32 luIndex = 0; luIndex < mLandmarkIndexArray.GetLength(); ++luIndex)
    {
        BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent lRemoveEvent;
        lRemoveEvent.mTriggerID =
            static_cast<u32>(static_cast<s32>(mLandmarkIndexArray.GetItem(luIndex)))
            | KU_REGION_TRIGGER_ID_TYPE_BITS;
        lrRemoveTriggerQueue.AddEvent(lRemoveEvent);
    }

    mLandmarkIndexArray.Clear();
}

// X360 0x823265E8.
bool TriggerQueryManager::AddLandmarkIndexForGameMode(LandmarkIndex lLandmarkIndex)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "luLandmarkIndex: " << static_cast<s32>(lLandmarkIndex) << "\n";
    }

    if (mLandmarkIndexArray.Contains(lLandmarkIndex))
    {
        return true;
    }

    mbTriggersUpdated = false;                  // X360: byte store 0 at this+1808
    mLandmarkIndexArray.Append(lLandmarkIndex);
    return true;
}

// X360 0x82355D78.
LightTriggerId TriggerQueryManager::GetPlayerCurrentTrafficLightId() const
{
    CGS_ASSERT(IsPlayerInTrafficLightRegion(), "IsPlayerInTrafficLightRegion()");
    return mPlayerCurrentTrafficLightId;
}

// [gateui] BODIED 2026-08-20. The X360 emits no symbol -- it inlines the single byte read at
// this+1809 at every call site, including inside GetPlayerCurrentTrafficLightId's own assert just
// above (which is why it showed up as an UNDEF external the moment this TU was measured for the
// gateui mount: the assert names it, the header declared it "body elsewhere in the full TU", and
// nowhere in the tree was that body).
bool TriggerQueryManager::IsPlayerInTrafficLightRegion() const
{
    return mbPlayerInTrafficLightRegion;
}

// ----------------------------------------------------------------------------
// [gateui] The two active-trigger-set read accessors -- BODIED 2026-08-20.
//
// The X360 emits NO symbol for either: every call site (StuntManager::OnPropHit @0x8236EE18 is
// the one this wave needs) renders as an inlined read of the Array<u16,256> at this+912 -- the
// live-count word at this+1424 for the count, and `*(this + 912 + 2*i)` for the item, each behind
// the CgsArray "Array used before Construct/Clear was called" sentinel check. De-inlined to these
// two named accessors so no reconstructed body has to poke a byte offset (they were declared for
// exactly that in the StuntManager grow, and left bodiless -- the round-1 verify pass measured
// them as UNDEF externals blocking the whole GameState mount).
//
// maActiveTriggers is written ONLY by UpdateTriggers above (Clear + Append), so a caller that
// reads a count of 0 is reading "the trigger pump has not run this frame", which is exactly what
// the `[UI-gate] armed` / `[UI-gate] OnPropHit ... armed=` rungs report.
// ----------------------------------------------------------------------------
u32 TriggerQueryManager::GetActiveTriggerCount() const
{
    return maActiveTriggers.GetLength();
}

u16 TriggerQueryManager::GetActiveTrigger(u32 liIndex) const
{
    return maActiveTriggers.GetItem(liIndex);
}

// ----------------------------------------------------------------------------
// Compile-time offset guards (integer/pointer members only; Vector3 omitted to keep the class
// standard-layout-agnostic). Never called.
// ----------------------------------------------------------------------------
void TriggerQueryManager::_AssertLayout()
{
    // These two members precede the embedded ResourcePtr<TriggerData> (mpTriggerData), so their
    // X360 offsets are pointer-width-independent and hold on the 64-bit gate.
    static_assert(offsetof(TriggerQueryManager, maActiveTriggers)              == 912,  "maActiveTriggers @ +912");
    static_assert(offsetof(TriggerQueryManager, mpTriggerData)                == 1568, "mpTriggerData @ +1568");

    // Every member BELOW sits AFTER the by-value CgsResource::ResourcePtr<TriggerData>
    // (mpTriggerData). That type holds 5 raw pointers + a ResourceHandle: 0x1C (28B) on the X360
    // 32-bit ABI, but 0x38 (56B) under the 64-bit MSVC gate. The X360 absolute offsets below are
    // therefore physically unreachable on the gate (they assume 4-byte pointers). Per the committed
    // codebase convention for ResourcePtr-embedding structs (see BrnWorldGraphicsStreamer.h:
    // "Absolute offsets/size are NOT static_asserted"), guard these X360-faithful offset guards to
    // the 32-bit/X360-width build so they document the binary layout without breaking the 64-bit
    // gate. The member ORDER, names, and the reserved-padding intent are unchanged from the verified
    // reconstruction; only the unsatisfiable-on-x64 compile-time offset guards are made conditional.
#if defined(_M_IX86) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4)
    static_assert(offsetof(TriggerQueryManager, mbTriggersUpdated)            == 1808, "mbTriggersUpdated @ +1808");
    static_assert(offsetof(TriggerQueryManager, mbPlayerInTrafficLightRegion) == 1809, "mbPlayerInTrafficLightRegion @ +1809");
    static_assert(offsetof(TriggerQueryManager, mPlayerCurrentTrafficLightId) == 1812, "mPlayerCurrentTrafficLightId @ +1812");
    static_assert(offsetof(TriggerQueryManager, mpProgressionManager)         == 1816, "mpProgressionManager @ +1816");
    static_assert(offsetof(TriggerQueryManager, mpTakedownManager)            == 1820, "mpTakedownManager @ +1820");
    static_assert(offsetof(TriggerQueryManager, mpRoadRulesManager)           == 1824, "mpRoadRulesManager @ +1824");
    static_assert(offsetof(TriggerQueryManager, mLandmarkIndexArray)          == 1832, "mLandmarkIndexArray @ +1832");
    static_assert(offsetof(TriggerQueryManager, mbDoSoundLookAheadThisFrame)  == 1868, "mbDoSoundLookAheadThisFrame @ +1868");
    static_assert(offsetof(TriggerQueryManager, mbCarHasTeleported)           == 1869, "mbCarHasTeleported @ +1869");
#endif
}

}

// ============================================================================
// PackedIndex -- GLOBAL scope per DWARF (bare struct, bare method definitions), NOT inside
// namespace BrnGameState. Declared in BrnTriggerQueryManager.h.
// ============================================================================

// ============================================================================
// X360 0x82355DE8 - PackedIndex::SetGlobalRaceCarIndex
// ============================================================================
// Store the global-race-car slot into the packed index's +0 word. Asserts the value is a valid
// in-range global slot (0..34, not INVALID) and that it fits in one byte, then stores the low byte
// (X360 clrlwi r31,r28,24 -> stw r31,0(r26)). The bounds assert's message is built dynamically on
// the X360 (StrStream: "Bad Global Race Car Index Set : " << value); collapsed here to the base
// rodata string per the assert-collapse rule.
void PackedIndex::SetGlobalRaceCarIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT((leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT) &&
               (leGlobalRaceCarIndex != E_GLOBAL_RACE_CAR_INDEX_INVALID),
               "Bad Global Race Car Index Set : ");
    CGS_ASSERT((static_cast<s32>(leGlobalRaceCarIndex) & 0xff) == static_cast<s32>(leGlobalRaceCarIndex),
               "(leGlobalRaceCarIndex & 0xff) == leGlobalRaceCarIndex");

    // X360: stw (a2 & 0xff) @ this+0 (meGlobalRaceCarIndex).
    meGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(static_cast<s32>(leGlobalRaceCarIndex) & 0xff);
}

// ============================================================================
// X360 0x82355EC8 - PackedIndex::SetActiveRaceCarIndex
// ============================================================================
// Store the active-race-car slot into the packed index's +4 word. Asserts the value is a valid
// in-range active slot (0..7, not INVALID) and that it fits in one byte, then stores the low byte
// (X360 clrlwi r31,r28,24 -> stw r31,4(r26)). The bounds assert's message is built dynamically on
// the X360 (StrStream: "Bad Active Race Car Index Set : " << value); collapsed here to the base
// rodata string per the assert-collapse rule.
void PackedIndex::SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT) &&
               (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID),
               "Bad Active Race Car Index Set : ");
    CGS_ASSERT((static_cast<s32>(leActiveRaceCarIndex) & 0xff) == static_cast<s32>(leActiveRaceCarIndex),
               "(leActiveRaceCarIndex & 0xff) == leActiveRaceCarIndex");

    // X360: stw (a2 & 0xff) @ this+4 (meActiveRaceCarIndex).
    meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(static_cast<s32>(leActiveRaceCarIndex) & 0xff);
}
