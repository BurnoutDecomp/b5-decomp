#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleOutputInterface.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"

#include <cstddef>   // offsetof (layout asserts)
#include <stdlib.h>  // getenv ([UI-gate] arming-timeline diag; same env guard as the wQ_04 rung)
#include <cstring>   // std::memset (the 24-byte player-trigger action record)

#include "rw/math/vpu/vector3_operation.h"  // rw::math::vpu operator-/Dot/MagnitudeSquared (refresh-gate + entry-direction maths)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu (Add/Start/StopMonitor)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<13312,16> (game-action queue)

#include "SharedClasses/Trigger/BrnTriggerData.h"        // BrnTrigger::TriggerData (GetKillzone/GetRegion/GetGenericRegion/counts)
#include "SharedClasses/Trigger/BrnTriggerBase.h"        // BrnTrigger::TriggerRegion (GetType/GetRegionIndex/GetId/GetBoxRegion)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion (Type, GetGroupId/GetId, meType @0x36)
#include "SharedClasses/Trigger/BrnKillzone.h"           // BrnTrigger::Killzone (trigger/region-id tables)
#include "SharedClasses/Trigger/BrnRegion.h"             // BrnTrigger::BoxRegion (GetDimensions/GetPosition2D/ComputeDirection)

#include <cmath>                                             // submitted look-ahead normalization

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
static s32 gsiPreWorldUpdatePM  = -1;
static s32 gsiPostWorldUpdatePM = -1;
static s32 gsiUpdateTriggersPM  = -1;
static s32 gsiSpikeTrigger1     = -1;
static s32 gsiSpikeTrigger2     = -1;

// ARTIST 0x82364BF0 constructor's monitor registrations. AddMonitor reads r3/r4/r5/f1/r7;
// r6 is the ABI's FP argument hole, not a parent handle.
static void RegisterTriggerQueryMonitors()
{
    gsiPreWorldUpdatePM  = CgsDev::PerfMonCpu::AddMonitor("TriggerQueryManager PreWorld",  CgsDev::E_PMP_5, false, 1.0f, true);
    gsiPostWorldUpdatePM = CgsDev::PerfMonCpu::AddMonitor("TriggerQueryManager PostWorld", CgsDev::E_PMP_5, false, 1.0f, true);
    gsiUpdateTriggersPM  = CgsDev::PerfMonCpu::AddMonitor("Update Triggers",               CgsDev::E_PMP_5, false, 1.0f, true);
    gsiSpikeTrigger1     = CgsDev::PerfMonCpu::AddMonitor("Spike Trigger 1",               CgsDev::E_PMP_5, false, 1.0f, true);
    gsiSpikeTrigger2     = CgsDev::PerfMonCpu::AddMonitor("Spike Trigger 2",               CgsDev::E_PMP_5, false, 1.0f, true);
}


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

// KillzoneAction game-action: event type 110 (0x6E), record size 264 (0x108).
static const s32 KI_GAME_ACTION_KILLZONE       = 110;
static const s32 KI_KILLZONE_ACTION_EVENT_SIZE = 264;

// ----------------------------------------------------------------------------------------
// [bugwave 2026-08-23] The PLAYER-TRIGGER game action PreWorldUpdate's fan-out posts:
// event type 109 (0x6D), record size 24 (0x18) -- `li r6,0x18 / li r5,0x6D` @0x8239F804.
// ----------------------------------------------------------------------------------------
static const s32 KI_GAME_ACTION_PLAYER_TRIGGER       = 109;
static const s32 KI_PLAYER_TRIGGER_ACTION_EVENT_SIZE = 24;

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
    maSoundActions.Construct();      // ARTIST82364D14, count at896
    for (auto& lrPosition : maActiveRaceCarPosLastFrame) lrPosition = Vector3{};
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

    RegisterTriggerQueryMonitors();
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
                    lpTriggerInterface->RemoveTrigger(lRemoveEvent);
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
            // ⭐⭐⭐ [drive-thru wave 2026-08-27] UNPARKED. The console arm, transcribed from the asm:
            //
            //     // 0x8239BF80 BrnGameState::TriggerQueryManager::ProcessPlayerTriggers,
            //     // switch (*(a4 + 54)) == lpGenericRegion->GetType(), cases 0..4:
            //     BrnGameState::DriveThruManager::HandleDriveThru(a7, a4, a3, a8, a5);
            //     //   a7 = lpDriveThruManager (this)   a4 = lpGenericRegion
            //     //   a3 = lpActiveRaceCarInterface    a8 = lpVehicleList   a5 = lpOutput
            //
            // ⚠️⚠️ THE PARK NOTE THAT STOOD HERE WAS STALE, AND IT WAS STALE IN BOTH OF ITS TWO
            // MEASURED CLAIMS. Re-measured 2026-08-27 [[gates-are-stale-not-dead]]:
            //  (1) "BrnDriveThruManager.cpp DOES NOT COMPILE". It does now, and the fix was TWO
            //      LINES, not the multi-file job the note implied: BrnDriveThruManager.h was
            //      missing `#include BrnGameStateSharedIO.h` (the home of the
            //      GameStateModuleIO::GameActionQueue typedef its four signatures name -- the
            //      header only ever forward-declared `struct OutputBuffer`), and
            //      BrnDriveThruManager.cpp:403 needed `::EActiveRaceCarIndex` because
            //      BrnGameState declares its own enum of that name. `selfcheck.py` now returns
            //      STATUS=pass. Every other error in the note's list was a cascade of those two.
            //  (2) "it drags the SIX bodiless training symbols ... None has a body or a link stub
            //      anywhere in b5-decomp/src". FALSE since 2026-08-24, four days after the note was
            //      written: the [tut-ticker] wave landed and MOUNTED BrnTrainingManager.cpp, which
            //      bodies IsTipPending (:808), IsTipAllowedInGameMode (:685), GetTimeSinceLastTip
            //      (:818), RequestTip (:824) and the GetProfile accessor, and BrnProfile.cpp:532
            //      bodies HasPlayerSeenTrainingType. Zero of the six were still missing.
            // The lesson is the project's own: ASK WHEN THE NOTE LAST RAN. This one cost the whole
            // drive-thru chain a week for a missing `#include` and a missing `::`.
            lpDriveThruManager->HandleDriveThru(lpGenericRegion, lpActiveRaceCarInterface,
                                                lpVehicleList, lpOutput);

            // [DIAG] NOT IN THE X360 BINARY. ENTRY DETECTION, logged SEPARATELY from the effect.
            // This observes the swept-region hit reaching its gameplay handler;
            // successful repair/reset is observed separately at its actual owner.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[drivethru] ENTER type=" << static_cast<s32>(lpGenericRegion->GetType())
                    << " id=" << static_cast<u64>(lpGenericRegion->GetId()) << "\n";
            }
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
            // The player crossed a road-limit region: RoadRulesManager::OnRoadLimit starts, ends or
            // hands over the time rule on that road. Only with an active player car.
            if (!lpActiveRaceCarInterface->IsPlayerCarActive())
            {
                break;
            }

            // Entry direction: the player car's linear velocity (read off GetPlayerRaceCarState)
            // against the region box's direction, strictly positive (a NaN dot is not an entry).
            const Vector3 lVelocity        = lpActiveRaceCarInterface->GetPlayerRaceCarState()->mLinearVelocity;
            const Vector3 lRegionDirection = lpGenericRegion->GetBoxRegion()->ComputeDirection();
            const bool    lbEntryDirection = rw::math::vpu::Dot(lVelocity, lRegionDirection) > 0.0f;

            // The limit id is the region's group id, else its own id, each a sign-extended 32-bit
            // word; the last argument is the player car's crashing flag.
            const CgsID lRoadLimitId =
                (lpGenericRegion->GetGroupId() != 0) ? lpGenericRegion->GetGroupId() : lpGenericRegion->GetId();

            mpRoadRulesManager->OnRoadLimit(lRoadLimitId, lbEntryDirection, lpOutput,
                                            lpActiveRaceCarInterface->IsPlayerCarCrashing());
            break;
        }

        default:
            // Other generic-region categories are not routed by this dispatcher.
            break;
    }
}

// ARTIST8239F5C8, DecFIGS h:107. Consume the preceding world query results.
void TriggerQueryManager::PreWorldUpdate(
    const GameStateModuleIO::PreWorldInputBuffer* /*lpInput*/,
    GameStateModuleIO::OutputBuffer* lpOutput,
    StuntManager* lpStuntManager,
    DriveThruManager* lpDriveThruManager,
    const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    const BrnResource::VehicleList* lpVehicleList)
{
    CgsDev::PerfMonCpu::StartMonitor(gsiPreWorldUpdatePM);
    UpdateTriggers(lpOutput, lpActiveRaceCarInterface);
    SubmitTriggerQueries(lpOutput, lpActiveRaceCarInterface);
    CacheSoundQueryPositions(lpActiveRaceCarInterface);
    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerData.operator->();

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
    PostSoundActions(lpOutput); // ARTIST8239F840..8239F8AC
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

namespace BrnGameState
{
// ARTIST 82392680. The line query follows the cached race-car motion. Sound's
// look-ahead is submitted on alternate eligible records, not alternate frames.
void TriggerQueryManager::SubmitTriggerQueries(
    GameStateModuleIO::OutputBuffer* lpOutput,
    const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
{
    CGS_ASSERT(lpOutput != NULL, "lpOutput != NULL");
    CGS_ASSERT(lpActiveRaceCarInterface != NULL, "lpActiveRaceCarInterface != NULL");
    auto* lpTriggerInterface = lpOutput->GetTriggerQueryInputInterface();
    CGS_ASSERT(lpTriggerInterface != NULL, "lpTriggerInterface != NULL");
    for (s32 liCar = static_cast<s32>(lpActiveRaceCarInterface->maCarsInTheRace.GetLength()); liCar > 0; )
    {
        const auto& lrCar = lpActiveRaceCarInterface->maCarsInTheRace[--liCar];
        mbCarHasTeleported = true;
        const Vector3 lDelta = lrCar.mPreviousPosition - lrCar.mPosition;
        // 823927E4 vmsum3fp128: one rounding of the three products' sum.
        const f32 lfDistanceSquared = static_cast<f32>(
            static_cast<f64>(lDelta.x) * lDelta.x + static_cast<f64>(lDelta.y) * lDelta.y
            + static_cast<f64>(lDelta.z) * lDelta.z);
        // vcmpgtfp(100, distanceSquared): unordered also skips the entire arm.
        if (!(lfDistanceSquared < 100.0f)) continue;

        PackedIndex lPackedCarIndexes;
        lPackedCarIndexes.SetGlobalRaceCarIndex(lrCar.meGlobalRaceCarIndex);
        lPackedCarIndexes.SetActiveRaceCarIndex(lrCar.meActiveRaceCarIndex);
        BrnWorld::TriggerEntityModuleIO::InLineTestEvent lEvent;
        lEvent.mQueryID.Set(56, lPackedCarIndexes.GetPackedRaceCarIndex());
        lEvent.mTriggerTypeFlags = 4;
        lEvent.mLineStart = lrCar.mPreviousPosition;
        lEvent.mLineEnd = lrCar.mPosition;
        lpTriggerInterface->AddEvent(&lEvent, 3);

        if (lrCar.mbIsPlayer)
        {
            Vector3 lLookAheadDist{lrCar.mDirection.x * 0.25f, lrCar.mDirection.y * 0.25f,
                                  lrCar.mDirection.z * 0.25f, lrCar.mDirection.w * 0.25f};
            const f32 lfLengthSquared = static_cast<f32>(
                static_cast<f64>(lLookAheadDist.x) * lLookAheadDist.x
                + static_cast<f64>(lLookAheadDist.y) * lLookAheadDist.y
                + static_cast<f64>(lLookAheadDist.z) * lLookAheadDist.z);
            // 823928D8..823928F8: vrsqrtefp and TWO Newton steps, with no
            // zero-vector guard. FLAG (model): correctly rounded initial estimate,
            // matching the existing console VMX model; denormals are not flushed.
            f32 lfReciprocalLength = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfLengthSquared)));
            for (s32 liStep = 0; liStep < 2; ++liStep)
            {
                const f32 lfSquared = lfReciprocalLength * lfReciprocalLength;
                const f32 lfHalf = lfReciprocalLength * 0.5f;
                const f32 lfDifference = std::fma(lfLengthSquared, lfSquared, -1.0f);
                const f32 lfResidual = lfDifference != lfDifference ? lfDifference : -lfDifference;
                lfReciprocalLength = std::fma(lfHalf, lfResidual, lfReciprocalLength);
            }
            const Vector3 lForward{lLookAheadDist.x * lfReciprocalLength,
                                   lLookAheadDist.y * lfReciprocalLength,
                                   lLookAheadDist.z * lfReciprocalLength,
                                   lLookAheadDist.w * lfReciprocalLength};
            if (lfLengthSquared > 225.0f)
                lLookAheadDist = Vector3{lForward.x * 15.0f, lForward.y * 15.0f,
                                         lForward.z * 15.0f, lForward.w * 15.0f};
            mPlayerLookAheadPos = Vector3{lrCar.mPosition.x + lLookAheadDist.x,
                                         lrCar.mPosition.y + lLookAheadDist.y,
                                         lrCar.mPosition.z + lLookAheadDist.z,
                                         lrCar.mPosition.w + lLookAheadDist.w};
            if (mbDoSoundLookAheadThisFrame)
            {
                lEvent.mQueryID.SetIndex(999);
                lEvent.mLineStart = mPlayerLookAheadPos;
                // 82392990 vmaddfp D,A,B,C = A*C+B.
                lEvent.mLineEnd = Vector3{std::fma(lForward.x, 4.0f, mPlayerLookAheadPos.x),
                                          std::fma(lForward.y, 4.0f, mPlayerLookAheadPos.y),
                                          std::fma(lForward.z, 4.0f, mPlayerLookAheadPos.z),
                                          std::fma(lForward.w, 4.0f, mPlayerLookAheadPos.w)};
                lpTriggerInterface->AddEvent(&lEvent, 3);
            }
        }
        mbDoSoundLookAheadThisFrame = !mbDoSoundLookAheadThisFrame;
        mbCarHasTeleported = false;
    }
}

// Sound-related legs of ARTIST PreWorldUpdate8239F618..8239F6EC and
// 8239F840..8239F8AC, split around the existing player-trigger fan-out.
void TriggerQueryManager::CacheSoundQueryPositions(const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
{
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        const auto leCar = static_cast<EActiveRaceCarIndex>(liCar);
        CGS_ASSERT(leCar >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leCar < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        if (lpActiveRaceCarInterface->IsRaceCarActive(leCar))
            maActiveRaceCarPosLastFrame[liCar] = lpActiveRaceCarInterface->GetRaceCarState(leCar)->mTransform.Pos();
        CGS_ASSERT(liCar + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }
}

void TriggerQueryManager::PostSoundActions(GameStateModuleIO::OutputBuffer* lpOutput)
{
    for (u32 luAction = 0; luAction < maSoundActions.GetLength(); ++luAction)
        lpOutput->GetGameActionQueue()->AddEvent(&maSoundActions[luAction], 218);
    maSoundActions.Clear();
}

// ARTIST8236E858. Matching is on both the entity word and query kind.
bool TriggerQueryManager::IsSoundActionPresent(EntityId lEntityId,
    GameStateModuleIO::SoundTriggerAction::eType leResultType) const
{
    for (u32 luAction = 0; luAction < maSoundActions.GetLength(); ++luAction)
    {
        const auto& lrAction = maSoundActions[luAction];
        if (lrAction.mEntityId.muValue == lEntityId.muValue && lrAction.meResultType == leResultType)
            return true;
    }
    return false;
}

// ARTIST82379710. No-overlap results must explicitly clear the prior frame's bits.
void TriggerQueryManager::CheckSoundActions(const RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface)
{
    using GameStateModuleIO::SoundTriggerAction;
    for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
    {
        const auto leCar = static_cast<EActiveRaceCarIndex>(liCar);
        if (lpActiveRaceCarInterface->IsRaceCarActive(leCar))
        {
            const EntityId lEntity = lpActiveRaceCarInterface->GetRaceCarState(leCar)->mEntityId;
            if (!IsSoundActionPresent(lEntity, SoundTriggerAction::E_TYPE_AT_ENTITY))
            {
                SoundTriggerAction lAction;
                lAction.mQueryPos = maActiveRaceCarPosLastFrame[liCar];
                lAction.mEntityId = lEntity;
                lAction.meResultType = SoundTriggerAction::E_TYPE_AT_ENTITY;
                lAction.muActiveTriggers = 0;
                maSoundActions.Append(lAction);
            }
        }
    }
    if (lpActiveRaceCarInterface->IsPlayerCarActive())
    {
        const EntityId lEntity = lpActiveRaceCarInterface->GetPlayerRaceCarState()->mEntityId;
        if (!IsSoundActionPresent(lEntity, SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY))
        {
            SoundTriggerAction lAction;
            lAction.mQueryPos = mPlayerLookAheadPos;
            lAction.mEntityId = lEntity;
            lAction.meResultType = SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY;
            lAction.muActiveTriggers = 0;
            maSoundActions.Append(lAction);
        }
    }
}

// ARTIST82386BD8. Gameplay and sound consume the same swept-volume results.
// The query's global slot is bits0..7, active slot bits8..15; its owner is56.
// A player hit is selected by active slot, not by the query-owner byte.
void TriggerQueryManager::PostWorldUpdate(
    const GameStateModuleIO::PostWorldInputBuffer* lpInput,
    ModeManager* lpModeManager, EActiveRaceCarIndex lePlayerCar)
{
    using GameStateModuleIO::SoundTriggerAction;
    using BrnWorld::TriggerEntityModuleIO::OutLineTestResultEvent;
    CgsDev::PerfMonCpu::StartMonitor(gsiPostWorldUpdatePM);
    CGS_ASSERT(lpInput != NULL, "lpInput != NULL");
    const auto* lpTriggerResults = lpInput->GetTriggerEntityOutputInterface();
    CGS_ASSERT(lpTriggerResults != NULL, "lpTriggerResults != NULL");
    const auto* lpTriggerResultQueue = lpTriggerResults;
    CGS_ASSERT(lpTriggerResultQueue != NULL, "lpTriggerResultQueue != NULL");
    const auto* lpActiveRaceCarInterface = lpInput->GetActiveRaceCarOutputInterface();
    CGS_ASSERT(lpActiveRaceCarInterface != NULL, "lpActiveRaceCarInterface != NULL");
    // 82386CD8..CEC: these are results for this frame, including an empty one.
    lpModeManager->ClearModeStartRegion();
    mbPlayerInTrafficLightRegion = false;
    mPlayerCurrentTrafficLightId = static_cast<LightTriggerId>(-1);
    mPlayerSigTakedownGroupID = 0;
    mPlayerSuperJumpGroupID = 0;
    mPlayerRoadLimitGroupID = 0;
    const CgsModule::Event* lpEvent = NULL;
    s32 liSize = 0;
    s32 liType = lpTriggerResultQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent)
    {
        CGS_ASSERT(liType == 1, "Unexpected trigger result");
        if (liType == 1)
        {
            const auto* lpResult = static_cast<const OutLineTestResultEvent*>(lpEvent);
            if (lpResult->mQueryID.GetOwner() == 56)
            {
                const u32 luQueryIndex = lpResult->mQueryID.GetIndex();
                const bool lbLookAhead = luQueryIndex == 999;
                const auto leCar = lbLookAhead ? lePlayerCar
                    : static_cast<EActiveRaceCarIndex>((luQueryIndex >> 8) & 0xff);
                const bool lbPlayer = leCar == lePlayerCar;
                const bool lbActive = lpActiveRaceCarInterface->IsRaceCarActive(leCar);
                // Only the sound record needs a live entity. The assembly still
                // handles ordinary gameplay results if that entity became inactive.
                if (!lbLookAhead || lbActive)
                {
                    SoundTriggerAction lAction;
                    if (lbActive)
                    {
                        lAction.mQueryPos = lbLookAhead ? mPlayerLookAheadPos : maActiveRaceCarPosLastFrame[leCar];
                        lAction.mEntityId = lpActiveRaceCarInterface->GetRaceCarState(leCar)->mEntityId;
                        lAction.meResultType = lbLookAhead ? SoundTriggerAction::E_TYPE_AHEAD_OF_ENTITY : SoundTriggerAction::E_TYPE_AT_ENTITY;
                    }
                    lAction.muActiveTriggers = 0;
                    const auto* lpTriggers = lpResult->GetTriggerIds();
                    for (s32 liTrigger = 0; liTrigger < lpResult->miNumTriggers; ++liTrigger)
                    {
                        const u32 luTrigger = lpTriggers[liTrigger];
                        if ((luTrigger >> 24) == 56)
                        {
                            const u32 luRegionIndex = luTrigger & 0x00ffffff;
                            const auto* lpRegion = mpTriggerData->GetRegion(luRegionIndex);
                            if (!lbLookAhead && lbPlayer)
                                maLastPlayerTriggers.Append(static_cast<u16>(luRegionIndex));
                            if (lpRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                            {
                                const auto* lpGeneric = static_cast<const BrnTrigger::GenericRegion*>(lpRegion);
                                const u32 luBit = static_cast<u32>(lpGeneric->GetType()) - 19;
                                if (luBit <= 12) lAction.muActiveTriggers |= 1u << luBit;
                                if (!lbLookAhead && lbPlayer)
                                {
                                    const CgsID lGroup = lpGeneric->GetGroupId() != 0
                                        ? lpGeneric->GetGroupId() : lpGeneric->GetId();
                                    if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_JUMP)
                                        mPlayerSuperJumpGroupID = lGroup;
                                    else if (lpGeneric->GetType() == BrnTrigger::GenericRegion::E_TYPE_ROAD_LIMIT)
                                        mPlayerRoadLimitGroupID = lGroup;
                                }
                            }
                            else if (!lbLookAhead && lpRegion->GetType() == BrnTrigger::TriggerRegion::E_TYPE_LANDMARK
                                     && (!lpModeManager->IsOnlineGameMode() || lbPlayer))
                            {
                                lpModeManager->RaceCarTriggersLandmark(lpActiveRaceCarInterface,
                                    static_cast<EGlobalRaceCarIndex>(luQueryIndex & 0xff), leCar,
                                    static_cast<LandmarkIndex>(luRegionIndex), lbPlayer);
                            }
                        }
                        else if (!lbLookAhead)
                        {
                            CGS_ASSERT((luTrigger >> 24) == 57, "Unknown trigger owner in line test result");
                            if ((luTrigger >> 24) == 57 && lbPlayer)
                            {
                                mbPlayerInTrafficLightRegion = true;
                                mPlayerCurrentTrafficLightId = static_cast<LightTriggerId>(luTrigger);
                            }
                        }
                    }
                    if (lbLookAhead) mCachedLookAheadSoundAction = lAction;
                    if (lbActive) maSoundActions.Append(lAction);
                }
            }
        }
        liType = lpTriggerResultQueue->GetNextEvent(lpEvent, &lpEvent, &liSize);
    }
    if (!mbCarHasTeleported && mbDoSoundLookAheadThisFrame && !mCachedLookAheadSoundAction.IsEmpty())
    {
        mCachedLookAheadSoundAction.mQueryPos = mPlayerLookAheadPos;
        maSoundActions.Append(mCachedLookAheadSoundAction);
        mCachedLookAheadSoundAction.mQueryPos = Vector3{};
        mCachedLookAheadSoundAction.mEntityId.muValue = 0;
        mCachedLookAheadSoundAction.meResultType = SoundTriggerAction::E_TYPE_INVALID;
        mCachedLookAheadSoundAction.muActiveTriggers = 0;
    }
    CheckSoundActions(lpActiveRaceCarInterface);
    CgsDev::PerfMonCpu::StopMonitor(gsiPostWorldUpdatePM);
}

}
