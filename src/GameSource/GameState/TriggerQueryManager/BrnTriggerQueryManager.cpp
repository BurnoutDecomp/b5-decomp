#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"

#include <cstddef>   // offsetof (layout asserts)

#include "rw/math/vpu/vector3_operation.h"  // rw::math::vpu operator-/Dot/MagnitudeSquared (refresh-gate + entry-direction maths)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu (Add/Start/StopMonitor)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<13312,16> (game-action queue)

#include "SharedClasses/Trigger/BrnTriggerData.h"        // BrnTrigger::TriggerData (GetKillzone/GetRegion/GetGenericRegion/counts)
#include "SharedClasses/Trigger/BrnTriggerBase.h"        // BrnTrigger::TriggerRegion (GetType/GetRegionIndex/GetId/GetBoxRegion)
#include "SharedClasses/Trigger/BrnGenericRegion.h"      // BrnTrigger::GenericRegion (Type, GetGroupId/GetId, meType @0x36)
#include "SharedClasses/Trigger/BrnKillzone.h"           // BrnTrigger::Killzone (trigger/region-id tables)
#include "SharedClasses/Trigger/BrnRegion.h"             // BrnTrigger::BoxRegion (GetDimensions/GetPosition2D/ComputeDirection)

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

// KillzoneAction game-action: event type 110 (0x6E), record size 264 (0x108).
static const s32 KI_GAME_ACTION_KILLZONE       = 110;
static const s32 KI_KILLZONE_ACTION_EVENT_SIZE = 264;

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
    // X360 0x823922F8-0x8239233C: the player-active-index assert fires unconditionally, then the
    // whole rebuild block (LABEL_32 @0x823923C4) is entered only when mbIsPlayerCarActive == 1
    // (bne loc_8239265C skips it otherwise). When the player is inactive UpdateTriggers does
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
            // Drive-thru categories -> DriveThruManager (X360 HandleDriveThru(region, activeCar, vehicleList, output)).
            lpDriveThruManager->HandleDriveThru(lpGenericRegion, lpActiveRaceCarInterface, lpVehicleList, lpOutput);
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
            // Only the active player car matters for road limits.
            if (!lpActiveRaceCarInterface->IsPlayerCarActive())
            {
                return;
            }

            // Entry direction: dot(player velocity, region forward direction) > 0.
            const Vector3 lVelocity        = lpActiveRaceCarInterface->GetPlayerLinearVelocity();
            const Vector3 lRegionDirection = lpGenericRegion->GetBoxRegion()->ComputeDirection();
            const bool    lbEntryDirection = rw::math::vpu::Dot(lVelocity, lRegionDirection) > 0.0f;

            // Per-car road-limit byte read off the player's RaceCarState (X360
            // *(1120*playerIndex + interface + 1914), == RaceCarState::mbResetCarTransform @1098).
            // FLAG: the semantic role of this byte as the OnRoadLimit `a5` arg is unverified -- the
            // X360 reads exactly this field; reproduced faithfully.
            s32 liRoadLimit = 0;
            if (lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() != static_cast<EActiveRaceCarIndex>(-1))
            {
                const RCEntityActiveRaceCarOutputInterface::RaceCarState* lpState =
                    lpActiveRaceCarInterface->GetPlayerRaceCarState();
                liRoadLimit = lpState->mbResetCarTransform ? 1 : 0;
            }

            // Road-limit region id == group id when set, else the trigger id.
            const CgsID lGroupId  = lpGenericRegion->GetGroupId();
            const CgsID lRegionId = lpGenericRegion->GetId();
            const u32   luRoadLimitRegionId = static_cast<u32>((lGroupId != 0) ? lGroupId : lRegionId);

            mpRoadRulesManager->OnRoadLimit(luRoadLimitRegionId, lbEntryDirection, lpOutput, liRoadLimit);
            break;
        }

        default:
            // Other generic-region categories are not routed by this dispatcher.
            break;
    }
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
