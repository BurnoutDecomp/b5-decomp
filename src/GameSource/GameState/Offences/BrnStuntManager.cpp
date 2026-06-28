#include "GameSource/GameState/Offences/BrnStuntManager.h"

#include <cmath>                                            // std::fabs

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert::Begin/Fire/EndAssert (verbatim X360 strings)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<N,16>::AddEvent + CgsModule::Event
#include "GameShared/GameClasses/Containers/CgsArray.h"     // Array<u16,256> (active-trigger set; via TriggerQueryManager accessors)

#include "GameSource/Math/BrnMathUtils.h"                   // BrnMath::IsPointInsideBox (additive grow; see header)
#include "GameSource/GameState/BrnGameActions.h"            // GameStateModuleIO::OnStuntElementCompleteAction / WorldStuntAction (action shapes)
#include "GameSource/GameState/BrnGameStateModuleIO.h"      // GameStateModuleIO::OutputBuffer / GameActionQueue
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TriggerQueryManager::GetTriggerData / GetActiveTrigger* (additive grows)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"          // BrnProgression::ProgressionManager::IsStuntElementDone / OnTrophyUnlock / ... (additive grows)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface accessors

#include "SharedClasses/Trigger/BrnGenericRegion.h"         // BrnTrigger::GenericRegion / Type
#include "SharedClasses/Trigger/BrnTriggerBase.h"           // BrnTrigger::TriggerRegion::GetType / GetId / GetGroupId
#include "SharedClasses/Trigger/BrnRegion.h"                // BrnTrigger::BoxRegion::GetPosition2D / GetPosition / GetDimensions / ComputeTransform / ComputeDirection
#include "SharedClasses/Trigger/BrnTriggerData.h"           // BrnTrigger::TriggerData::GetRegion / GetGenericRegion* / GetRegionCount
#include "SharedClasses/World/BrnWorldRegion.h"             // BrnWorld::WorldRegion / ECounty / EDistrict

// ============================================================================
// BrnGameState::StuntManager  -- the Super-Jump / Super-Smash / Billboard
// collectible bookkeeper. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct                @ 0x82365990
//   Prepare                  @ 0x8239C9C0
//   Update                   @ 0x8239F8D0
//   OnPropHit                @ 0x8236EE18
//   UpdateJumps              @ 0x8239D460
//   StuntElementTriggered    @ 0x82358BE0
//   CheckForTrophyUnlocks    @ 0x82399390
//   LoadDistrictMap          @ 0x82399458
//   LatchJumpElement         (de-inlined ProcessPlayerTriggers case 7; 0x8239C1F8)
//   FindTriggersCounty       @ 0x8236B310 (kept; previously bodied)
//
// The X360 build baked this whole class's assert path with the verbatim source path
//   d:\p4\b5_main\burnout\main\code\gamesource\unity\../GameState/Offences/BrnStuntManager.cpp
// reproduced below as KAC_FILE so every Assert::FireAssert matches the binary exactly.
// ============================================================================

namespace BrnGameState
{

namespace
{
    // Verbatim X360-baked source path for this TU's asserts.
    const char* const KAC_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Offences/BrnStuntManager.cpp";

    // The action queue handed to Update/UpdateJumps/ProcessStuntElement/CompleteAllStuntType is the
    // OutputBuffer's game-action queue == CgsModule::VariableEventQueue<13312,16> (the GameStateModuleIO
    // action queue). The DWARF spells the param type GameActionQueue (an incomplete alias); the X360
    // AddEvent calls target the <13312,16> queue directly, so we reinterpret_cast through, exactly as
    // BurnoutSkillzManager does (GameActionQueueImpl precedent).
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

    // Event-type ids posted onto the action queue (X360 li r5 immediates). These are slots of the
    // GameStateModuleIO action-event enum; only the four this TU emits are named here.
    // FLAG: enumerator NAMES not recovered (the X360 passes the raw integer); ids are asm-exact.
    const s32 KI_EVENT_HUD_JUMP_FAILED = 265;   // Update          (size 1)
    const s32 KI_EVENT_ON_JUMP_START   = 56;    // UpdateJumps      (size 24)
    const s32 KI_EVENT_SHOW_JUMP_NAME  = 57;    // UpdateJumps      (size 8)
}

// ----------------------------------------------------------------------------
// Construct @ 0x82365990
//
// One-time wiring: store the five injected sibling managers (asserting each non-NULL with the
// X360-verbatim member-name strings), zero the working state (the "last latched" element set, the
// jump landing timer, the signature-takedown total, every jump-state bool), default the debug
// force-complete type to E_STUNT_ELEMENT_TYPE_COUNT (== "nothing pending"), set mbAlwaysToJumpCameras
// true, Construct the receiver queue (Clear'd twice -- the X360 emits two back-to-back Clears) and
// register the debug component.
// ----------------------------------------------------------------------------
void StuntManager::Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                             TriggerQueryManager*                lpTriggerQueryManager,
                             ModeManager*                        lpModeManager,
                             TrainingManager*                    lpTrainingManager,
                             GameStateModule*                    lpGameStateModule)
{
    mpProgressionManager = lpProgressionManager;
    muLastZoneId = static_cast<uint16_t>(-1);   // X360 sth -1 @ +1540
    muLastPropId = static_cast<uint16_t>(-1);   // X360 sth -1 @ +1542
    mpLastStuntOrSmashElement = 0;              // X360 stw 0  @ +1532
    meLastStuntElementType    = E_STUNT_ELEMENT_TYPE_COUNT; // X360 stw 3 @ +1536
    mpLastJumpElement = 0;                      // X360 stw 0  @ +1544
    mbJumpActive = false;                       // X360 stb 0  @ +1556
    mbShowJumpNameNextFrame = false;            // X360 stb 0  @ +1558 (set twice; reset below)

    if (!mpProgressionManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_FILE, 62);
        CgsDev::Assert::EndAssert();
    }

    mpTriggerQueryManager = lpTriggerQueryManager;
    if (!mpTriggerQueryManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpTriggerQueryManager", KAC_FILE, 65);
        CgsDev::Assert::EndAssert();
    }

    mpModeManager = lpModeManager;
    if (!mpModeManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpModeManager", KAC_FILE, 68);
        CgsDev::Assert::EndAssert();
    }

    // X360 stores a5 (TrainingManager) @ +1528 and a6 (GameStateModule) @ +1524, asserting in
    // declaration order mpTrainingManager (line 71) then mpGameStateModule (line 74).
    mpTrainingManager = lpTrainingManager;
    if (!mpTrainingManager)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpTrainingManager", KAC_FILE, 71);
        CgsDev::Assert::EndAssert();
    }

    mpGameStateModule = lpGameStateModule;
    if (!mpGameStateModule)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule", KAC_FILE, 74);
        CgsDev::Assert::EndAssert();
    }

    // Zero the remaining jump-state bools + working scalars.
    mbHasPlayerLeftGround = false;          // +1557 (X360 stb 0 @ +1557)
    mbShowJumpNameNextFrame = false;        // +1558 (X360 stb 0 @ +1558)
    mbIsAttemptingJumpForFirstTime = false; // +1559 (X360 stb 0 @ +1559)
    mbNeedToSendJumpFailureMessage = false; // +1560 (X360 stb 0 @ +1560)
    meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST; // X360 stw 0 @ +936 (the district-map cursor)
    mfJumpLandingTime = 0.0f;               // +1548 (X360 stfs 0.0 @ +1548)
    mbAlwaysToJumpCameras = true;           // +1561 (X360 stb 1 @ +1561)

    // Init + Clear the receiver queue (the X360 sets the embedded base/capacity/alignment fields then
    // Clears twice). Construct() binds the backing buffer + sizing and Clears once; the second Clear
    // mirrors the duplicated X360 emission.
    mReceiverQueue.Construct();
    mReceiverQueue.Clear();

    // Build + register the in-game stunt debug menu (Register is the DebugComponent base member).
    mStuntManagerDebugComponent.Construct(this);
    mStuntManagerDebugComponent.Register();

    // X360 final store: meDebugCompletedUnlockType = E_STUNT_ELEMENT_TYPE_COUNT (3) @ +864.
    meDebugCompletedUnlockType = E_STUNT_ELEMENT_TYPE_COUNT;
}

// ----------------------------------------------------------------------------
// Prepare @ 0x8239C9C0
//
// Per-track setup: stream the district map, bind mWorldMap2D over it, then walk the track's generic
// regions to tally the total number of each stunt-element type (and per county).
//
// Returns false while LoadDistrictMap is still streaming (so the caller re-calls next frame); once the
// map is bound it does the one-shot tally and returns true.
//
//   * The compressed-group-id list (laCompressedGroupIds[200]) de-dupes signature-takedown groups
//     (region types 5/7 share a stunt group) -- a stunt may span several trigger regions but counts
//     once. miSignatureTakedownCount tallies the deduped count.
//   * For each generic region: classify its world position to a county (FindTriggersCounty, inlined
//     here as the WorldMap2D::GetValue -> DistrictToCounty sample), then bump the matching counter:
//       region type 5  -> miSignatureTakedownCount (handled by the dedup pass, case 0 below)
//       region type 7  (E_TYPE_JUMP)            -> maiTotalStuntElementCounts[JUMP]      (+ per county)
//       region type 8  (E_TYPE_SMASH)           -> maiTotalStuntElementCounts[SMASH]     (+ per county)
//       region type 12 (E_TYPE_OVERDRIVE_BOOST) -> maiTotalStuntElementCounts[BILLBOARD] (+ per county)
//     FLAG: region type 12 is the billboard-smash kind (its enum spelling is E_TYPE_OVERDRIVE_BOOST,
//     but the Prepare/OnPropHit mapping makes it the BILLBOARD stunt element -- confirmed by the
//     counter index 2 == E_STUNT_ELEMENT_TYPE_BILLBOARD).
// ----------------------------------------------------------------------------
bool StuntManager::Prepare(GameStateModuleIO::OutputBuffer* lpOutput)
{
    static const int32_t KI_MAX_GROUPS = 200;   // X360 lnCompressedGroupIdCount < KI_MAX_GROUPS

    if (!LoadDistrictMap(lpOutput))
    {
        return false;   // still streaming the district map
    }

    if (!mDistrictMapResourceHandle.mpResourceMemory)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mDistrictMapResourceHandle.GetResource() != NULL", KAC_FILE, 124);
        CgsDev::Assert::EndAssert();
    }
    if (!*reinterpret_cast<void* const*>(mDistrictMapResourceHandle.mpResourceMemory))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mDistrictMapResourceHandle.GetResource()->GetMemoryResource() != NULL", KAC_FILE, 125);
        CgsDev::Assert::EndAssert();
    }

    // Bind the 2D map over the streamed blob. The X360 loads the world-origin / world-size SIMD
    // constants from rodata (unk_82FAE140 / unk_82FADED0) and passes the packed grid pointer
    // (resource main-memory + the resource's own +4 header offset).
    // FLAG: the world-origin / world-size constants (KV_DISTRICT_MAP_ORIGIN / _SIZE) live in X360
    // rodata that is not in the exports; passed as flagged zero vectors here. The map blob pointer is
    // the resource's main-memory base.
    const void* lpMapBlob = *reinterpret_cast<void* const*>(mDistrictMapResourceHandle.mpResourceMemory);
    const Vector2 lWorldOrigin = Vector2{ 0.0f, 0.0f, 0.0f, 0.0f }; // FLAG: rodata unk_82FAE140 not in exports
    const Vector2 lWorldSize   = Vector2{ 0.0f, 0.0f, 0.0f, 0.0f }; // FLAG: rodata unk_82FADED0 not in exports
    mWorldMap2D.Construct(lpMapBlob, lWorldOrigin, lWorldSize);

    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerQueryManager->GetTriggerData();

    // Zero both totals tables (the X360 nested loops bound-assert leEnumIndex against the county /
    // element-type counts as it clears).
    for (int32_t liElementType = 0; liElementType < 3; ++liElementType)
    {
        maiTotalStuntElementCounts[liElementType] = 0;
        for (int32_t liCounty = 0; liCounty < 5; ++liCounty)
        {
            maaiTotalStuntElementCountsPerCounty[liElementType][liCounty] = 0;
        }
    }

    miSignatureTakedownCount = 0;

    int32_t laCompressedGroupIds[KI_MAX_GROUPS];
    int32_t lnCompressedGroupIdCount = 0;

    const int32_t liGenericRegionCount = lpTriggerData->GetGenericRegionCount();
    for (int32_t liGenericRegionIndex = 0; liGenericRegionIndex < liGenericRegionCount; ++liGenericRegionIndex)
    {
        const BrnTrigger::GenericRegion* lpGenericRegion = lpTriggerData->GetGenericRegion(liGenericRegionIndex);
        const BrnTrigger::GenericRegion::Type leType = lpGenericRegion->GetType();

        // Signature-takedown groups (region types 5 / 7) de-dupe by group id so a multi-region stunt
        // counts once. FLAG: the X360 builds this compressed-group-id list but the recovered pseudocode
        // never consumes lnCompressedGroupIdCount (miSignatureTakedownCount is incremented per-region in
        // the switch below, NOT from the deduped count). Kept byte-faithful (the list IS built); its
        // consumer was either inlined away or dropped by the decompiler.
        if (leType == BrnTrigger::GenericRegion::E_TYPE_SIGNATURE_TAKEDOWN ||
            leType == BrnTrigger::GenericRegion::E_TYPE_JUMP)
        {
            CgsID lnGroupId = lpGenericRegion->GetGroupId();
            int32_t lnRawGroupId = static_cast<int32_t>(lnGroupId);
            if (lnRawGroupId == 0)
                lnRawGroupId = static_cast<int32_t>(lpGenericRegion->GetId());

            if (lnRawGroupId != static_cast<int32_t>(lpGenericRegion->GetId()))
            {
                bool lbAlreadySeen = false;
                for (int32_t liExisting = 0; liExisting < lnCompressedGroupIdCount; ++liExisting)
                {
                    if (laCompressedGroupIds[liExisting] == lnRawGroupId)
                    {
                        lbAlreadySeen = true;
                        break;
                    }
                }
                if (!lbAlreadySeen)
                {
                    if (lnCompressedGroupIdCount >= KI_MAX_GROUPS)
                    {
                        CgsDev::Assert::BeginAssert();
                        CgsDev::Assert::FireAssert("lnCompressedGroupIdCount < KI_MAX_GROUPS", KAC_FILE, 183);
                        CgsDev::Assert::EndAssert();
                    }
                    laCompressedGroupIds[lnCompressedGroupIdCount] = lnRawGroupId;
                    ++lnCompressedGroupIdCount;
                }
            }
        }

        // County-classify this region's box centre (the inlined FindTriggersCounty).
        const Vector2 lPosition2D = lpGenericRegion->GetBoxRegion()->GetPosition2D();
        const uint8_t luDistrictValue = mWorldMap2D.GetValue(lPosition2D);

        BrnWorld::ECounty leCounty;
        if (luDistrictValue == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)   // 255 -> off-map
        {
            leCounty = BrnWorld::E_COUNTY_INVALID;   // == 5
        }
        else
        {
            if (luDistrictValue >= BrnWorld::E_DISTRICT_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leDistrict < E_DISTRICT_COUNT",
                                           "..\\..\\..\\SharedClasses\\World/BrnWorldRegion.h", 155);
                CgsDev::Assert::EndAssert();
            }
            leCounty = BrnWorld::WorldRegion::DistrictToCounty(static_cast<BrnWorld::EDistrict>(luDistrictValue));
        }

        switch (leType)
        {
            case BrnTrigger::GenericRegion::E_TYPE_SIGNATURE_TAKEDOWN:   // 5
                ++miSignatureTakedownCount;
                break;

            case BrnTrigger::GenericRegion::E_TYPE_JUMP:                 // 7 -> JUMP
                if (leCounty != BrnWorld::E_COUNTY_INVALID)
                {
                    ++maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_JUMP];
                    ++maaiTotalStuntElementCountsPerCounty[E_STUNT_ELEMENT_TYPE_JUMP][leCounty];
                }
                break;

            case BrnTrigger::GenericRegion::E_TYPE_SMASH:               // 8 -> SMASH
                if (leCounty != BrnWorld::E_COUNTY_INVALID)
                {
                    ++maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_SMASH];
                    ++maaiTotalStuntElementCountsPerCounty[E_STUNT_ELEMENT_TYPE_SMASH][leCounty];
                }
                break;

            case BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST:     // 12 -> BILLBOARD (see FLAG above)
                if (leCounty != BrnWorld::E_COUNTY_INVALID)
                {
                    ++maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_BILLBOARD];
                    ++maaiTotalStuntElementCountsPerCounty[E_STUNT_ELEMENT_TYPE_BILLBOARD][leCounty];
                }
                break;

            default:
                break;
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// Update @ 0x8239F8D0
//
// Per-frame spine. Drives the "first attempt" / jump-failure HUD message, then -- when the player car
// is active and NOT crashing -- runs the latched-element dispatch:
//   * if a stunt/smash element was latched this frame (StuntElementTriggered), ProcessStuntElement it;
//   * if a jump is in progress (mpLastJumpElement set), advance UpdateJumps;
//   * if the debug menu force-completed a type (meDebugCompletedUnlockType != COUNT), flush it.
// When the player car is inactive or crashing, the latched element + active jump are cleared.
// ----------------------------------------------------------------------------
void StuntManager::Update(GameStateModuleIO::GameActionQueue* lpActionQueue,
                          const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                          f32 lfTimeStep, bool lbIsAGameModeActive)
{
    if (!lpActionQueue)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpActionQueue != NULL", KAC_FILE, 273);
        CgsDev::Assert::EndAssert();
    }
    if (!lpActiveRaceCarInterface)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpActiveRaceCarInterface != NULL", KAC_FILE, 274);
        CgsDev::Assert::EndAssert();
    }

    GameActionQueueImpl* lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    // First-attempt -> failed transition: the X360 tests the player car's "crashing" flag
    // (*(1120*idx + base + 1914), the RaceCarState crash bit). When crashing while a first-time jump
    // attempt was armed, latch the "need to send failure" flag; when no longer crashing, fire the
    // jump-failed HUD message once.
    const bool lbPlayerCrashing = lpActiveRaceCarInterface->IsPlayerCarCrashing();
    if (lbPlayerCrashing)
    {
        if (mbIsAttemptingJumpForFirstTime)
        {
            mbIsAttemptingJumpForFirstTime = false;
            mbNeedToSendJumpFailureMessage = true;
        }
    }
    else if (mbNeedToSendJumpFailureMessage)
    {
        // type 265, size 1: HUDMessageJumpFailedAction (a 1-byte payload).
        // FLAG: payload contents not recovered (the X360 passes an uninitialised 1-byte stack slot);
        // emitted as a single zero byte.
        u8 lJumpFailedAction = 0;
        lpActionQueueImpl->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lJumpFailedAction), KI_EVENT_HUD_JUMP_FAILED, 1);
        mbNeedToSendJumpFailureMessage = false;
    }

    const bool lbPlayerActive = lpActiveRaceCarInterface->IsPlayerCarActive();
    const bool lbPlayerCrashingNow = lpActiveRaceCarInterface->IsPlayerCarCrashing();

    if (!lbPlayerActive || lbPlayerCrashingNow)
    {
        // Player inactive / crashing -> drop the latched element + any active jump.
        mpLastStuntOrSmashElement = 0;   // v8[383]
        mpLastJumpElement = 0;           // v8[386]
        mbJumpActive = false;            // +1556
        return;
    }

    // Player active + driving: run the dispatch.
    if (StuntElementTriggered())
    {
        ProcessStuntElement(lpActionQueue, /*lbIsJump*/false, lbIsAGameModeActive);
        mpLastStuntOrSmashElement = 0;   // consume the latched element
    }

    if (mpLastJumpElement)
    {
        UpdateJumps(lpActiveRaceCarInterface, lpActionQueue, lfTimeStep, lbIsAGameModeActive);
    }

    if (meDebugCompletedUnlockType != E_STUNT_ELEMENT_TYPE_COUNT)
    {
        CompleteAllStuntType(meDebugCompletedUnlockType, lpActionQueue);
        meDebugCompletedUnlockType = E_STUNT_ELEMENT_TYPE_COUNT;
    }
}

// ----------------------------------------------------------------------------
// OnPropHit @ 0x8236EE18
//
// A breakable prop (luZoneId/luPropId) was destroyed at lPosition. Scan the currently-armed trigger
// regions; if the prop sits inside a smashable generic region (type 8 == Super Smash, type 12 ==
// Billboard), latch that region as the pending stunt/smash element for this frame.
//
//   For each active trigger index -> resolve its TriggerRegion. Skip non-generic regions
//   (TriggerRegion type != E_TYPE_GENERIC_REGION == 2). For a generic region of type 8 or 12, build a
//   broadphase test: only if the prop is within (half the region's smaller XZ extent) of the region
//   centre do we run the exact BrnMath::IsPointInsideBox test against the region's box transform.
//   On a hit: store muLastZoneId/muLastPropId and latch mpLastStuntOrSmashElement + meLastStuntElementType
//   (SMASH==1 for type 8, BILLBOARD==2 for type 12), then stop.
// ----------------------------------------------------------------------------
void StuntManager::OnPropHit(uint16_t luZoneId, uint16_t luPropId, Vector3 lPosition)
{
    // X360 flt_82001DA0 == 0.5 (the half-extent broadphase scale).
    const f32 KF_BROADPHASE_HALF_SCALE = 0.5f;

    for (u32 i = 0; ; ++i)
    {
        // GetLength() reproduces the X360 "Array used before Construct/Clear was called" assert
        // (CgsArray.h) on the active-trigger count word.
        if (i >= mpTriggerQueryManager->GetActiveTriggerCount())
            break;

        const u16 luTriggerIndex = mpTriggerQueryManager->GetActiveTrigger(i);
        const BrnTrigger::TriggerData* lpTriggerData = mpTriggerQueryManager->GetTriggerData();

        if (luTriggerIndex >= lpTriggerData->GetRegionCount())
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liRegionIndex < miRegionCount",
                                       "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h", 624);
            CgsDev::Assert::EndAssert();
        }

        const BrnTrigger::TriggerRegion* lpTriggerRegion = lpTriggerData->GetRegion(luTriggerIndex);

        // Only generic regions are smashable.
        if (lpTriggerRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
            continue;

        const BrnTrigger::GenericRegion* lpGenericRegion =
            static_cast<const BrnTrigger::GenericRegion*>(lpTriggerRegion);

        StuntElementType leStuntElementType = E_STUNT_ELEMENT_TYPE_JUMP; // v5; overwritten below
        bool lbIsSmashable;
        const BrnTrigger::GenericRegion::Type leType = lpGenericRegion->GetType();
        if (leType == BrnTrigger::GenericRegion::E_TYPE_SMASH)            // 8 -> SMASH
        {
            leStuntElementType = E_STUNT_ELEMENT_TYPE_SMASH;
            lbIsSmashable = true;
        }
        else if (leType == BrnTrigger::GenericRegion::E_TYPE_OVERDRIVE_BOOST)  // 12 -> BILLBOARD
        {
            leStuntElementType = E_STUNT_ELEMENT_TYPE_BILLBOARD;
            lbIsSmashable = true;
        }
        else
        {
            lbIsSmashable = false;
        }

        if (!lbIsSmashable)
            continue;

        const BrnTrigger::BoxRegion* lpBoxRegion = lpGenericRegion->GetBoxRegion();

        // Broadphase: 3D squared distance from the prop to the region box CENTRE vs a squared radius of
        // (0.5 * the LARGEST box dimension). The X360 builds the centre (x,y,z), subtracts the prop pos,
        // sums the squares over all three lanes (vmsum3fp128), and compares against (0.5*maxDim)^2.
        const Vector3 lCentre = lpBoxRegion->GetPosition();
        const f32 lfDeltaX = lCentre.x - lPosition.x;
        const f32 lfDeltaY = lCentre.y - lPosition.y;
        const f32 lfDeltaZ = lCentre.z - lPosition.z;
        const f32 lfDistSq = lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;

        // X360 fsel chain == max(dimX, max(dimY, dimZ)).
        const f32 lfDimX = lpBoxRegion->GetDimensionX();
        const f32 lfDimY = lpBoxRegion->GetDimensionY();
        const f32 lfDimZ = lpBoxRegion->GetDimensionZ();
        f32 lfLargestExtent = (lfDimY >= lfDimZ) ? lfDimY : lfDimZ;
        lfLargestExtent     = (lfDimX >= lfLargestExtent) ? lfDimX : lfLargestExtent;
        const f32 lfRadius = lfLargestExtent * KF_BROADPHASE_HALF_SCALE;
        const f32 lfRadiusSq = lfRadius * lfRadius;

        if (!(lfRadiusSq > lfDistSq))   // X360 vcmpgtfp (radius^2 > dist^2)
            continue;

        // Narrowphase: exact point-in-box test against the region's full box transform.
        // FLAG: BrnMath::IsPointInsideBox + BoxRegion::ComputeTransform are foreign TUs; the X360
        // passes the prop position + the region half-extents (dimensions) as the two SIMD args.
        const Matrix44Affine lBoxTransform = lpBoxRegion->ComputeTransform();
        const Vector3 lHalfExtents = lpBoxRegion->GetDimensions();
        if (BrnMath::IsPointInsideBox(lBoxTransform, lPosition, lHalfExtents))
        {
            muLastZoneId = luZoneId;   // +1540
            muLastPropId = luPropId;   // +1542

            // leStuntElementType is always SMASH/BILLBOARD here (the jump branch is unreachable for a
            // smashable region), so OnPropHit only ever latches the stunt/smash element.
            mpLastStuntOrSmashElement = lpGenericRegion;       // +1532
            meLastStuntElementType    = leStuntElementType;    // +1536
            break;
        }
    }
}

// Helper for the OnJumpStart camera-cut/type selection (X360: forwards picks CameraType1/Cut1,
// backwards CameraType2/Cut2). Mirrors the inlined X360 selection; writes the cut into *lpiCut and
// returns the camera type. FLAG: exact field choice (cut1/type1 vs cut2/type2) per entry direction.
static s32 lSelectJumpCameraTypeCut(const BrnTrigger::GenericRegion* lpRegion, bool lbForwards, s32* lpiCut)
{
    if (lbForwards)
    {
        *lpiCut = lpRegion->GetCameraCut1();
        return static_cast<s32>(lpRegion->GetCameraType1());
    }
    *lpiCut = lpRegion->GetCameraCut2();
    return static_cast<s32>(lpRegion->GetCameraType2());
}

// ----------------------------------------------------------------------------
// UpdateJumps @ 0x8239D460
//
// Advance the active-jump state machine for the pending jump element (mpLastJumpElement). The phases:
//   * Player car gone inactive -> abandon the jump (clear mpLastJumpElement + mbJumpActive).
//   * NOT yet "in the jump" (mbJumpActive == 0):
//       - Look up whether this jump was already completed (ProgressionManager::IsStuntElementDone) and
//         whether the player is fast enough / entered the right way; if too slow (|speed| < 40 mph) or
//         already-done-and-not-always-cameras, drop the jump.
//       - Otherwise post an OnJumpStart action (type 56), arm mbJumpActive, reset the landing timer.
//   * Already in the jump (mbJumpActive):
//       - If the player is back on the ground (the active-race-car index byte) -> the jump completed:
//         clear mbJumpActive, clear mpLastJumpElement.
//       - If "show jump name" is pending -> post a ShowJumpName action (type 57) once.
//   * Landing settle: once all wheels are grounded, accumulate the landing timer; after 0.5s past the
//     "left ground" mark, ProcessStuntElement the jump as completed and clear the active jump.
//
// FLAG: this body walks several FOREIGN structures by raw field (the player RaceCarState's per-wheel
// ground-contact bytes + speed, the ProgressionManager done-set, the region's group/id key, and the
// BoxRegion direction). Those callees/fields are reconstructed by name where a committed accessor
// exists and otherwise FLAGGED; the exact RaceCarState field offsets (per-wheel +0x28 stride 28,
// speed @+0x3CC) are X360-asm-proven but their named accessors are not all committed.
// ----------------------------------------------------------------------------
void StuntManager::UpdateJumps(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                               GameStateModuleIO::GameActionQueue* lpActionQueue,
                               f32 lfTimeStep, bool lbIsAGameModeActive)
{
    typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState RaceCarState;

    if (!mpLastJumpElement)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpLastJumpElement != NULL", KAC_FILE, 820);
        CgsDev::Assert::EndAssert();
    }

    if (!lpActiveRaceCarInterface->IsPlayerCarActive())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("How the hell did the player do a jump whilst inactive?", KAC_FILE, 822);
        CgsDev::Assert::EndAssert();
    }

    GameActionQueueImpl* lpActionQueueImpl = reinterpret_cast<GameActionQueueImpl*>(lpActionQueue);

    // The X360 reads a per-car "is crashing" byte (the same 1120*idx + 1914 RaceCarState bit). When the
    // player is crashing mid-jump, abandon the jump.
    if (lpActiveRaceCarInterface->IsPlayerCarCrashing())
    {
        mpLastJumpElement = 0;   // +1544
        mbJumpActive = false;    // +1556
        return;
    }

    if (!mbJumpActive)
    {
        // --- jump take-off evaluation -------------------------------------------------------------
        // The pending jump's stunt-element key: its group id (or its own id when grouped to 0).
        const CgsID lJumpGroupId = mpLastJumpElement->GetGroupId();
        CgsID lJumpKey = lJumpGroupId;
        if (lJumpGroupId == 0)
            lJumpKey = mpLastJumpElement->GetId();

        // Already-completed? ProgressionManager keeps a done-set of completed stunt-element keys.
        // FLAG: the X360 reads the set directly off mpProgressionManager (+30568) via
        // Set<__int64,512>::Find; reconstructed as the named IsStuntElementDone() query.
        const bool lbHasBeenDoneBefore = mpProgressionManager->IsStuntElementDone(lJumpKey);

        // Speed gate: |speed| < 40 mph (== KF_MIN_JUMP_SPEED_MPH) -> too slow, no jump.
        // FLAG: the X360 reads RaceCarState float @ +0x3CC (== result[243]), which the committed
        // RaceCarState layout names mfMaxSpeedMPH. Semantically a "min speed to count" gate reads more
        // like the CURRENT speed (mfSpeedMPH @ +0x3C8); kept byte-faithful to the +0x3CC read but
        // flagged in case the committed RaceCarState offset diverges from this X360 build.
        const RaceCarState* lpPlayerRaceCarState = lpActiveRaceCarInterface->GetPlayerRaceCarState();
        const f32 lfPlayerSpeed = lpPlayerRaceCarState->mfMaxSpeedMPH; // X360 result[243] (@+0x3CC)
        if (std::fabs(lfPlayerSpeed) < 40.0f || (lbHasBeenDoneBefore && !mbAlwaysToJumpCameras))
        {
            mpLastJumpElement = 0;   // drop the jump
            return;
        }

        // Direction gate: did the player enter the jump region facing forwards? dot(playerDir,
        // jumpDir) >= 0. If the region is one-way (CameraCut1 > 0) and the player entered backwards,
        // also drop the jump.
        // FLAG: BoxRegion::ComputeDirection is a foreign TU returning the region's forward vector; the
        // X360 dots it with the player's facing (vmsum3fp128). Reconstructed by name; the player facing
        // comes from the player RaceCarState transform.
        const Vector3 lTriggerDir = mpLastJumpElement->GetBoxRegion()->ComputeDirection();
        const Vector3 lPlayerDir  = lpActiveRaceCarInterface->GetPlayerDirection();
        const f32 lfDot = lPlayerDir.x * lTriggerDir.x + lPlayerDir.y * lTriggerDir.y + lPlayerDir.z * lTriggerDir.z;
        const bool lbEnteredJumpForwards = (lfDot >= 0.0f);

        if (mpLastJumpElement->GetCameraCut1() > 0 && lfDot < 0.0f)
        {
            mpLastJumpElement = 0;   // one-way jump entered backwards -> drop
            return;
        }

        if (!lbIsAGameModeActive)
        {
            // Post the jump-start action (type 56, 24 bytes). VERIFIED field ORDER against the X360
            // asm (UpdateJumps 0x8239D460, the !a6 block building &v34): the stack record is
            //   v34 @+0  (8B)        = the stunt key (group id, or own id when grouped to 0)
            //   v35 @+8  (4B, extsb) = the camera-CUT byte: lbz @+0x34 forwards / @+0x35 backwards
            //   v36 @+12 (4B, extsh) = the camera-TYPE half: lhz @+0x30 forwards / @+0x32 backwards
            //   v37 @+16 (word)      = the FIRST-TIME flag == !doneBefore
            //                          (X360 v37 = (cntlzw(doneBefore) & 0x20) != 0)
            //   pad @+20 (4B)        rounds the record to 24 bytes.
            // This corrects the prior reconstruction, which mis-ordered the three non-key fields
            // (mbDoneBefore@+8 / cut@+12 / type@+16) AND wrote the INVERTED 'doneBefore' at +8 where
            // the X360 actually writes the !doneBefore first-time flag (at +16).
            // FLAG: the OnJumpStartAction struct shape is not committed; built as this X360 raw record.
            struct OnJumpStartActionPayload
            {
                CgsID mKey;            // +0  (8)  -- v34
                s32   miCameraCut;     // +8       -- v35 (cut byte: 0x34 fwd / 0x35 bwd)
                s32   miCameraType;    // +12      -- v36 (type half: 0x30 fwd / 0x32 bwd)
                s32   miFirstTimeFlag; // +16      -- v37 (== !doneBefore)
                s32   miPad;           // +20      -- rounds to 24 bytes
            } lJumpStartAction;

            lJumpStartAction.mKey = lJumpKey;
            lJumpStartAction.miCameraType =
                lSelectJumpCameraTypeCut(mpLastJumpElement, lbEnteredJumpForwards, &lJumpStartAction.miCameraCut);
            lJumpStartAction.miFirstTimeFlag = lbHasBeenDoneBefore ? 0 : 1;   // X360 v37 = !doneBefore
            lJumpStartAction.miPad = 0;

            lpActionQueueImpl->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lJumpStartAction), KI_EVENT_ON_JUMP_START, 24);

            mbShowJumpNameNextFrame = true;                    // +1558
            mbIsAttemptingJumpForFirstTime = !lbHasBeenDoneBefore; // +1559
        }

        mfJumpLandingTime = 0.0f;       // +1548
        mbJumpActive = true;            // +1556
        mbHasPlayerLeftGround = false;  // +1557
    }
    else
    {
        // --- already in the jump: emit the deferred ShowJumpName once ------------------------------
        if (mbShowJumpNameNextFrame)
        {
            // type 57, size 8: ShowJumpName carries the jump's stunt-element key.
            const CgsID lJumpGroupId = mpLastJumpElement->GetGroupId();
            CgsID lJumpKey = lJumpGroupId;
            if (lJumpGroupId == 0)
                lJumpKey = mpLastJumpElement->GetId();

            lpActionQueueImpl->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lJumpKey), KI_EVENT_SHOW_JUMP_NAME, 8);
            mbShowJumpNameNextFrame = false;
        }
    }

    // --- landing settle: once every wheel is grounded, run the post-landing timer -----------------
    // VERIFIED against the X360 asm (the `for(i=0;i<112;i+=28)` loop): the X360 walks the player
    // RaceCarState's four per-wheel ground-contact bytes via GetPlayerRaceCarState() (sub_82310240),
    // reading HIBYTE(result[i+10]) for i in {0,28,56,84} float-strides == the byte @ +0x28 of each
    // WheelLite (stride 112B). On big-endian PPC HIBYTE is the lowest-address byte of the word @0x28,
    // i.e. WheelLite.mRoadContact.mbIsOnGround. v29 (airborne) starts 1 and is cleared to 0 if ANY of
    // the four bytes is non-zero; the airborne branch (reset timer + mbHasPlayerLeftGround=1) runs
    // only when ALL FOUR wheels report mbIsOnGround == 0 (fully airborne).
    // NOTE: this REPLACES the prior `!IsPlayerInAir()` projection, whose flagged semantics
    // ('any wheel off ground == in air') were the OPPOSITE predicate and would have shifted the
    // >0.5s landing-completion window for a 1-3-wheel-airborne car. The explicit 4-wheel scan IS
    // fully recoverable from the asm, so we reproduce it byte-faithfully.
    const RaceCarState* lpLandingState = lpActiveRaceCarInterface->GetPlayerRaceCarState();
    bool lbAnyWheelGrounded = false;
    for (int liWheel = 0; liWheel < 4; ++liWheel)
    {
        if (lpLandingState->maWheels[liWheel].mRoadContact.mbIsOnGround)
        {
            lbAnyWheelGrounded = true;   // X360: clears v29 (airborne) to 0
        }
    }
    const bool lbAllWheelsGrounded = lbAnyWheelGrounded;   // v29 == 0  ->  treat as on the ground
    if (!lbAllWheelsGrounded)                              // v29 == 1  ->  fully airborne
    {
        mfJumpLandingTime = 0.0f;       // still airborne
        mbHasPlayerLeftGround = true;   // +1557
    }
    else
    {
        const bool lbLeftGround = mbHasPlayerLeftGround;
        mfJumpLandingTime = lfTimeStep + mfJumpLandingTime;
        // flt_82001DA0 == 0.5: 0.5s settle after the player has left + returned to the ground.
        if (lbLeftGround && mfJumpLandingTime > 0.5f)
        {
            ProcessStuntElement(lpActionQueue, /*lbIsJump*/true, lbIsAGameModeActive);
            mpLastJumpElement = 0;   // +1544
            mbJumpActive = false;    // +1556
            mbIsAttemptingJumpForFirstTime = false; // +1559
        }
    }
}

// ----------------------------------------------------------------------------
// StuntElementTriggered @ 0x82358BE0
//
// Predicate: has a stunt/smash element been latched this frame? Returns false when none is latched
// (mpLastStuntOrSmashElement == NULL); otherwise asserts the latched type is a real stunt type (not
// COUNT, not JUMP -- jumps go through the jump state machine, not this path) and returns true.
// ----------------------------------------------------------------------------
bool StuntManager::StuntElementTriggered()
{
    if (!mpLastStuntOrSmashElement)
        return false;

    if (meLastStuntElementType == E_STUNT_ELEMENT_TYPE_COUNT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meLastStuntElementType != E_STUNT_ELEMENT_TYPE_COUNT", KAC_FILE, 591);
        CgsDev::Assert::EndAssert();
    }
    if (meLastStuntElementType == E_STUNT_ELEMENT_TYPE_JUMP)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("meLastStuntElementType != E_STUNT_ELEMENT_TYPE_JUMP", KAC_FILE, 592);
        CgsDev::Assert::EndAssert();
    }

    return true;
}

// ----------------------------------------------------------------------------
// CheckForTrophyUnlocks @ 0x82399390
//
// Called when a stunt-element-complete action is posted. When the action's current count has reached
// its total (all of that type collected), fire the matching trophy unlock + the special-car unlock
// check on the ProgressionManager. The trophy id is selected from the element type:
//   JUMP (0)      -> trophy 2
//   SMASH (1)     -> trophy 3
//   BILLBOARD (2) -> trophy 1
//   >= 3          -> assert "I dont know what this stunt type is!..." (a new type was added)
// ----------------------------------------------------------------------------
void StuntManager::CheckForTrophyUnlocks(GameStateModuleIO::OnStuntElementCompleteAction* lpOnStuntElementAction)
{
    GameStateModuleIO::OnStuntElementCompleteAction* lpAction = lpOnStuntElementAction;

    if (lpAction->miCurrentCount > lpAction->miTotalCount)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "lpOnStuntElementAction->miCurrentCount <= lpOnStuntElementAction->miTotalCount", KAC_FILE, 769);
        CgsDev::Assert::EndAssert();
    }

    if (lpAction->miCurrentCount == lpAction->miTotalCount)
    {
        // FLAG: the trophy ids (2/3/1) are the X360 li-immediates; the trophy-id enumerator names are
        // not recovered.
        s32 liTrophyId;
        const u32 luType = static_cast<u32>(lpAction->meStuntElementType);
        if (luType == 0)            // JUMP
        {
            liTrophyId = 2;
        }
        else if (luType == 1)       // SMASH
        {
            liTrophyId = 3;
        }
        else
        {
            if (luType >= 3)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(
                    "I dont know what this stunt type is! Someone has added a new one without thinking about the trophies! What scum!\n",
                    KAC_FILE, 795);
                CgsDev::Assert::EndAssert();
                // X360 falls through to CheckForSpecialCarUnlocks then returns.
                mpProgressionManager->CheckForSpecialCarUnlocks();
                return;
            }
            liTrophyId = 1;         // BILLBOARD
        }

        mpProgressionManager->OnTrophyUnlock(liTrophyId);
        mpProgressionManager->CheckForSpecialCarUnlocks();
    }
}

// ----------------------------------------------------------------------------
// LoadDistrictMap @ 0x82399458
//
// District-map streaming state machine. Drives meDistrictMapLoadStage through:
//   E_DISTRICT_MAP_LOAD_REQUEST(0)     -> queue a LoadBundle("Districts.dat") request, advance to (1)
//   E_DISTRICT_MAP_LOAD_RESPONSE(1)    -> wait until the receiver queue has the response, advance to (2)
//   E_DISTRICT_MAP_ACQUIRE_REQUEST(2)  -> queue an AcquireResource(hash("Districts")) request, -> (3)
//   E_DISTRICT_MAP_ACQUIRE_RESPONSE(3) -> on response, bind mDistrictMapResourceHandle from the event
//                                         payload, advance to (4)
//   E_DISTRICT_MAP_DONE(4)             -> return true (map ready)
// Returns false until E_DISTRICT_MAP_DONE so Prepare re-polls each frame.
// ----------------------------------------------------------------------------
bool StuntManager::LoadDistrictMap(GameStateModuleIO::OutputBuffer* lpOutput)
{
    if (!lpOutput)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutput", KAC_FILE, 960);
        CgsDev::Assert::EndAssert();
    }

    switch (meDistrictMapLoadStage)
    {
        case E_DISTRICT_MAP_LOAD_REQUEST:
        {
            mReceiverQueue.Clear();
            // Request the Districts.dat bundle through the output buffer's resource-request interface.
            // FLAG: the foreign call is
            //   lpOutput->GetResourceRequestInterface()->LoadBundle(&mReceiverQueue, 1, 5, "Districts.dat", 0)
            // (BrnResource::GameDataIO::RequestInterface<3072>::LoadBundle, a foreign TU). The interface
            // getter is committed; LoadBundle's full signature is not modelled here, so the request is
            // DEFERRED (not fabricated) -- the X360 args are recorded above.
            const GameStateModuleIO::ResourceRequestInterface* lpRequestInterface =
                lpOutput->GetResourceRequestInterface();
            (void)lpRequestInterface;
            meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_RESPONSE;
            return false;
        }

        case E_DISTRICT_MAP_LOAD_RESPONSE:
        {
            if (mReceiverQueue.GetCount() <= 0)
                return false;   // still waiting for the bundle-load response
            meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_REQUEST;
            return false;
        }

        case E_DISTRICT_MAP_ACQUIRE_REQUEST:
        {
            mReceiverQueue.Clear();
            // Acquire the "Districts" resource: the X360 builds a 24-byte AcquireResource event
            // { &mReceiverQueue, 1, pool 5, id == HashString("Districts") | 0x500000000 } and AddEvents
            // it (type 4) onto the request interface's <3072,16> queue.
            // FLAG: the foreign call is
            //   lpOutput->GetResourceRequestInterface()->AcquireResource(&mReceiverQueue, 1, 5,
            //       CgsResource::ID::HashString("Districts") | 0x500000000LL)
            // (BrnResource::GameDataIO::RequestInterface<3072>::AcquireResource, a foreign TU). The
            // interface getter is committed; the acquire request is DEFERRED (not fabricated).
            const GameStateModuleIO::ResourceRequestInterface* lpRequestInterface =
                lpOutput->GetResourceRequestInterface();
            (void)lpRequestInterface;
            meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
            return false;
        }

        case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
        {
            if (mReceiverQueue.GetCount() <= 0)
                return false;   // still waiting for the acquire response
            meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;

            // Bind the resource handle from the acquire-response event payload. The X360 reads the two
            // handle words from (response event payload + 24) and stores them into the handle's two
            // pointer slots (mpResourceMemory / mpSourceEntry).
            // FLAG: the response-event walk (GetFirstEvent -> AcquireResourceResponse payload + 24) is a
            // foreign event shape; the handle is left null here and FLAGGED -- Prepare's
            // GetResource()!=NULL asserts will fire if the foreign acquire path is not wired. The store
            // target (mDistrictMapResourceHandle's two pointers) is X360-exact.
            mDistrictMapResourceHandle.mpResourceMemory = 0; // FLAG: from AcquireResourceResponse payload+24 (event shape foreign)
            mDistrictMapResourceHandle.mpSourceEntry    = 0; // FLAG: from AcquireResourceResponse payload+28
            return false;
        }

        case E_DISTRICT_MAP_DONE:
            return true;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unknown meDistrictMapLoadStage", KAC_FILE, 1030);
            CgsDev::Assert::EndAssert();
            return false;
    }
}

// ----------------------------------------------------------------------------
// LatchJumpElement  (de-inlined TriggerQueryManager::ProcessPlayerTriggers case 7 @ 0x8239C1F8)
//
// The player entered a jump trigger region. Latch it as the pending jump element -- but only if no jump
// is already active (so a second jump region entered mid-jump does not clobber the in-progress one).
// ----------------------------------------------------------------------------
void StuntManager::LatchJumpElement(const BrnTrigger::GenericRegion* lpRegion)
{
    if (!mbJumpActive)
        mpLastJumpElement = lpRegion;
}

// ----------------------------------------------------------------------------
// FindTriggersCounty @ 0x8236B310  (kept; previously bodied)
//
// Map a trigger's box centre to its owning county via the 2D district map: sample mWorldMap2D at the
// region's XZ position, convert the resulting district id to its county. An off-map sample (255) yields
// E_COUNTY_INVALID (== 5).
// ----------------------------------------------------------------------------
BrnWorld::ECounty StuntManager::FindTriggersCounty(const BrnTrigger::GenericRegion* lpRegion)
{
    const Vector2 lPosition2D = lpRegion->GetBoxRegion()->GetPosition2D();

    const uint8_t luDistrictValue = mWorldMap2D.GetValue(lPosition2D);
    if (luDistrictValue == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)   // 255
    {
        return BrnWorld::E_COUNTY_INVALID;   // == E_COUNTY_VALID_COUNT == 5
    }

    BrnWorld::WorldRegion lRegion;
    lRegion.Construct(static_cast<BrnWorld::EDistrict>(luDistrictValue));
    return lRegion.GetCounty();
}

// f32 KF_MIN_JUMP_SPEED_MPH (DWARF :156). The X360 take-off speed gate is |speed| < 40 mph (see
// UpdateJumps), so the published constant is 40.0 mph.
// FLAG: value 40.0f inferred from the UpdateJumps comparison immediate (flt_82004D0C); the rodata
// symbol's exact stored value is not in the exports.
const f32 StuntManager::KF_MIN_JUMP_SPEED_MPH = 40.0f;

}
