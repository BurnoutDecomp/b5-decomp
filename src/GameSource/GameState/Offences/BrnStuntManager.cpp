#include "GameSource/GameState/Offences/BrnStuntManager.h"

#include <cmath>                                            // std::fabs
#include <cstring>                                          // std::memset (the OnJumpStart record)
#include <stdlib.h>                                         // getenv  ([UI-gate] diag ladder)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CgsDev::Assert::Begin/Fire/EndAssert (verbatim X360 strings)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // CgsDev::Log::gpDebugPrint ([UI-gate] diag ladder)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"    // EventReceiverQueue<512,16>::Clear/GetCount/GetFirstEvent
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // RequestInterface<3072>::LoadBundle / AcquireResource
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // CgsResource::Events::AcquireResourceResponse (the handle bind)
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

    // The console's own Districts.dat literals, off LoadDistrictMap @0x82399458:
    //   case 0  LoadBundle(&mReceiverQueue, 1, 5, "Districts.dat", 0)
    //   case 2  AcquireResource(..., HashString("Districts"))
    // Identical spelling to StreetManager::LoadDistrictMap @0x8234FB98 and
    // BrnWorldModule::LoadDistrictMap @0x827D11D8 for the same file/resource.
    const char* const KPC_DISTRICT_MAP_BUNDLE_NAME   = "Districts.dat";
    const char* const KPC_DISTRICT_MAP_RESOURCE_NAME = "Districts";
    const s32         KI_DISTRICT_MAP_EVENT_ID       = 1;
    const s32         KI_DISTRICT_MAP_POOL_ID        = 5;

    // ⭐⭐ [gateui] THE DISTRICT-MAP WORLD RECT -- RECOVERED 2026-08-20, and it was a
    // PLACEHOLDER-ZERO of the KF_DEFAULT_ASPECTRATIO class (shadow campaign): with both vectors
    // zero, WorldMap2D::GetValue samples off-map for EVERY position, every region classifies
    // E_COUNTY_INVALID, the whole Prepare tally stays zero, and both ProcessStuntElement's
    // "county complete" / "all complete" legs and the HUD popup's <total> are garbage.
    //
    // The X360 reads them as two rodata-looking .data vectors that are ZERO IN THE STATIC IMAGE --
    // which is why they are absent from the JSON export set and why the earlier pass FLAGged them.
    // They are MSVC dynamic-initialiser thunks (the standard trap): recovered with headless IDA on
    // a private .i64 copy. The thunk at 0x82C4CDC8 writes unk_82FAE140 and the one at 0x82C4CD88
    // writes unk_82FADED0, each building the vector on the stack from two float constants and a
    // zeroed high half:
    //     0x82C4CD88:  -0x10 = flt_8200D4EC (-4208.0)   -0x0C = flt_8200D4E8 (-3846.0)
    //                  -0x08 = 0 (std r9)               -> stvx to unk_82FADED0
    //     0x82C4CDC8:  -0x10 = flt_8200D4F4 ( 8270.0)   -0x0C = flt_8200D4F0 ( 6101.0)
    //                  -0x08 = 0 (std r9)               -> stvx to unk_82FAE140
    //
    // ⛔ AND THE SCOUT MAP'S ASSIGNMENT OF THE TWO IS BACKWARDS -- the ARGUMENT REGISTERS settle
    // it. WorldMap2D::Construct(const void* lpData, Vector2 lWorldOrigin, Vector2 lWorldSize) takes
    // its FIRST vector parameter in v1 and its second in v2 (PPC VMX arg order). Both call sites
    // agree, byte for byte:
    //     Prepare                        @0x8239CA70  lvx128 v1 <- unk_82FADED0   (== ORIGIN)
    //                                    @0x8239CA64  lvx128 v2 <- unk_82FAE140   (== SIZE)
    //     SendSetUpAllEventStartsMessage @0x82375A78  lvx128 v1 <- unk_82FADED0
    //                                    @0x82375A6C  lvx128 v2 <- unk_82FAE140
    // So unk_82FADED0 is the ORIGIN and unk_82FAE140 is the SIZE, giving Paradise City the world
    // rect x in [-4208, +4062], z in [-3846, +2255] -- an 8270 x 6101 m map, which is the right
    // order of magnitude for the track and is what makes the district grid sample on-map at all.
    // (Named per CXX_NAMING_CONVENTIONS; the console emits them as unnamed .data vectors.)
    const Vector2 KV_DISTRICT_MAP_WORLD_ORIGIN = { -4208.0f, -3846.0f, 0.0f, 0.0f }; // unk_82FADED0
    const Vector2 KV_DISTRICT_MAP_WORLD_SIZE   = {  8270.0f,  6101.0f, 0.0f, 0.0f }; // unk_82FAE140

    // ⚠️ [FLAG PC bring-up] NOT IN THE X360 BINARY -- the district-map ACQUIRE RETRY BUDGET.
    // The console's LoadDistrictMap has no retry and its Prepare dereferences the handle
    // unconditionally, because on the console the Districts.dat bundle is resident long before
    // the acquire is issued. On PC the pool answers an acquire for a non-resident bundle with a
    // BOTH-NULL handle (the defect that printed `[StreetManager] district map: handle=0` until
    // 2026-08-11), so the console shape is a null dereference on the boot-critical path.
    // The chosen host behaviour is WAIT-then-GIVE-UP, never crash and never stall:
    //   * a null-handle response re-arms the acquire and returns false (Prepare re-pumps stage 4),
    //   * after KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES pumps the map is declared unavailable, the
    //     load machine latches DONE and Prepare SKIPS the map bind + the census (leaving the
    //     tally zero) and returns true, so the boot completes with a loud one-shot log instead
    //     of hanging on stage 4.
    // Held as file statics rather than StuntManager members so the console's class layout is
    // untouched (there is exactly one GameStateModule, hence one StuntManager).
    // DELETE-WHEN the Districts.dat bundle is proven resident at stage 4 on PC.
    const s32 KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES = 8;
    s32       giDistrictMapAcquireRetries         = 0;
    bool      gbDistrictMapUnavailable            = false;

    // ⛔ [gateui] ROUND-3 FIX (verify_r2_fixgsm F2) -- THE GIVE-UP LATCH IS DRIVEN OFF THE RETRY
    // COUNTER FOR *BOTH* NOT-READY SHAPES, and the readiness test lives in ONE place so the two
    // sites cannot drift apart again.
    // The round-2 body tested readiness as TWO clauses in Prepare
    //     (mpResourceMemory != 0) && (*mpResourceMemory != 0)
    // but latched gbDistrictMapUnavailable in LoadDistrictMap under the FIRST clause only. The
    // second shape -- a non-null handle whose SmallResource main-memory pointer is still null --
    // therefore had no give-up path at all: Prepare returned false every pump, LoadDistrictMap sat
    // in E_DISTRICT_MAP_DONE (whose arm never re-issues the acquire), and stage 4 never advanced.
    // That is an UNBOUNDED BOOT STALL before Car Select. The second shape is not hypothetical --
    // it is the console's own assert line 125
    // (`mDistrictMapResourceHandle.GetResource()->GetMemoryResource() != NULL`), which exists
    // precisely because it is reachable.
    // Both clauses are now answered by this one predicate, which LoadDistrictMap's
    // E_DISTRICT_MAP_ACQUIRE_RESPONSE arm evaluates before it decides retry-vs-give-up, so every
    // not-ready shape is bounded by the SAME budget and Prepare's wait always terminates.
    // NOTE the read is a serialised-blob walk of the resource header (AGENTS.md's documented
    // exception): mpResourceMemory points at the pool's SmallResource record whose first word is
    // the main-memory base -- the same two-load the console's Prepare does at
    // 0x8239CA74 (`lwz r11, 0(r10)`).
    bool DistrictMapHandleReady(const CgsResource::ResourceHandle& lHandle)
    {
        return (lHandle.mpResourceMemory != 0) &&
               (*reinterpret_cast<void* const*>(lHandle.mpResourceMemory) != 0);
    }

    // ⛔ [gateui] ROUND-3 FIX (verify_r2_fixgsm F4) -- NOT IN THE X360 BINARY. An empty
    // WorldMap2D grid header for the give-up path: `{ u16 width = 0; u16 height = 0; }` with no
    // value bytes. Constructing mWorldMap2D over this leaves muWidth == muHeight == 0, so
    // CgsWorldMap2D.cpp :: GetValue's `liX >= muWidth` bound check rejects EVERY sample (0 >= 0)
    // and answers KU_INVALID_WORLD_MAP_VALUE -> FindTriggersCounty answers E_COUNTY_INVALID.
    // Without this, the give-up arm returned true having never Constructed mWorldMap2D at all
    // (StuntManager::Construct does not initialise it, and neither does the console's
    // 0x82365990), so FindTriggersCounty would divide by whatever mWorldSize.x happened to hold
    // -- zero if the embedding value-initialises, giving inf and a UB float->int cast.
    // The world rect passed alongside is the REAL one, so the divisor is never zero either way.
    const u16 gauDistrictMapEmptyGrid[2] = { 0, 0 };

    // [DIAG] NOT IN THE X360 BINARY. The wave-gateui `[UI-gate]` ladder, same idiom + same env
    // guard as PropEntityModule_wQ_04.cpp's "[prop-diag] BREAK" rung (the producer this chain
    // hangs off). Evaluated ONCE per process, never per call.
    bool UIGateDiagOn()
    {
        static const bool sbDiag = (getenv("BRN_PROP_DIAG") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
    }

    // [DIAG] NOT IN THE X360 BINARY. First-N latch for the per-event `[UI-gate]` rungs, copied
    // from the `[prop-diag] BREAK` rung's own first-N guard in PropEntityModule_wQ_04.cpp: a
    // multi-prop smash bursts, and the PREAMBLE requires the ladder stay readable.
    const s32 KI_UI_GATE_DIAG_FIRST_N = 16;

    bool UIGateDiagFirstN(s32* lpiCounter)
    {
        if (!UIGateDiagOn())
            return false;
        if (*lpiCounter >= KI_UI_GATE_DIAG_FIRST_N)
            return false;
        ++(*lpiCounter);
        return true;
    }
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

    // ⛔ [gateui] THE TWO CONSOLE ASSERTS ARE NOW *WAIT* GATES, NOT FALL-THROUGHS. The console
    // fires each assert and then dereferences the handle regardless -- it can afford to, because
    // on the console the Districts.dat bundle was made resident 19 Prepare stages before this
    // point and the acquire always answered. On PC the pool answers the acquire whether or not
    // the bundle is resident, so a NULL handle here is reachable, and the console's shape would
    // be TWO null dereferences on the boot-critical Prepare path (the asserts in this tree LOG
    // AND CONTINUE). Each not-ready path now returns false, which breaks stage 4 out of
    // GameStateModule::Prepare's machine and re-pumps it next frame -- the same wait shape as the
    // Triggers.dat leg (BrnTriggerQueryManager_Prepare.cpp :: Prepare, E_TRIGGER_ACQUIRE_REQUESTED
    // `if (lpReceiverQueue->GetCount() == 0) return false;`).
    //
    // ⛔ [gateui] ROUND-3 BANNER CORRECTION (verify_r2_fixgsm F2). The round-2 banner here claimed
    // "LoadDistrictMap re-issues the acquire on each such retry, and gives the census up ... once
    // its retry budget is spent". That was TRUE only for the null-`mpResourceMemory` shape; the
    // `*mpResourceMemory == 0` shape had no give-up path and stalled stage 4 for ever. Both shapes
    // now go through the single `DistrictMapHandleReady` predicate at the top of this file, which
    // LoadDistrictMap's ACQUIRE_RESPONSE arm uses to decide retry-vs-give-up -- so the statement
    // is now true as written, for BOTH shapes, and this wait is bounded by
    // KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES pumps.
    // ⓘ RE-PREPARE (verify_r2_fixgsm F5): the give-up is PROCESS-FINAL. See the note on the
    // give-up arm below and the corrected banner in BrnGameStateModule.h.
    //
    // The assert strings/lines are still the console's, and each fires ONCE (a per-pump assert
    // storm is what the 440-assert perf incident was).
    if (!DistrictMapHandleReady(mDistrictMapResourceHandle))
    {
        // Fire the console's own two asserts (verbatim strings/lines), ONCE each -- a per-pump
        // assert storm is what the 440-assert perf incident was.
        static bool sbHandleAssertFired = false;
        if (!sbHandleAssertFired)
        {
            sbHandleAssertFired = true;
            CgsDev::Assert::BeginAssert();
            if (!mDistrictMapResourceHandle.mpResourceMemory)
            {
                CgsDev::Assert::FireAssert("mDistrictMapResourceHandle.GetResource() != NULL", KAC_FILE, 124);
            }
            else
            {
                CgsDev::Assert::FireAssert(
                    "mDistrictMapResourceHandle.GetResource()->GetMemoryResource() != NULL", KAC_FILE, 125);
            }
            CgsDev::Assert::EndAssert();
        }

        if (!gbDistrictMapUnavailable)
        {
            // WAIT: LoadDistrictMap re-arms the acquire and re-pumps stage 4 next frame. BOUNDED --
            // the ACQUIRE_RESPONSE arm sets gbDistrictMapUnavailable once the budget is spent, for
            // EITHER not-ready shape, so this arm cannot be taken for ever.
            return false;
        }

        // Retry budget spent. SKIP the census rather than dereference a null handle (the console's
        // shape) or stall the boot on stage 4 for ever. The tally stays zero, which the ladder's
        // rung-0 line below reports, and every downstream <total> is then meaningless --
        // deliberately loud.
        //
        // ⛔ [gateui] ROUND-3 FIX (verify_r2_fixgsm F4): bind mWorldMap2D over an EMPTY grid before
        // returning. The round-2 body returned true having never Constructed it, leaving
        // muWidth/muHeight/mpValues and the world rect at whatever the embedding left there --
        // and FindTriggersCounty / ProcessStuntElement / CompleteAllStuntType all keep calling
        // GetValue afterwards, dividing by mWorldSize.x. With a zeroed embedding that is
        // `x/0 -> inf` and `static_cast<int32_t>(inf * muWidth)` is UB. The empty grid makes every
        // sample answer KU_INVALID_WORLD_MAP_VALUE by the bound check instead, i.e. every region
        // classifies E_COUNTY_INVALID -- which the off-map guard in
        // StuntManager_gUI_00.cpp :: ProcessStuntElement already handles explicitly.
        mWorldMap2D.Construct(gauDistrictMapEmptyGrid,
                              KV_DISTRICT_MAP_WORLD_ORIGIN, KV_DISTRICT_MAP_WORLD_SIZE);

        // ⛔ [gateui r4] ROUND-4 FIX (verify_r3_fix3gsm S1): REPEAT THE CENSUS CLEAR HERE.
        // The census's own clear loop (below, after the readiness gate) was the ONLY writer that
        // zeroes these three tallies, and this arm skips it -- so the log line that follows was
        // false in the helpful direction. StuntManager::Construct (console 0x82365990 and this
        // tree's body alike) initialises mpLastStuntOrSmashElement / mpLastJumpElement /
        // mfJumpLandingTime and friends but NEVER touches the tally tables, and GameStateModule is
        // embedded by value in BrnGameModule -- so without this the bytes are whatever the
        // embedding left there. That is not cosmetic: maiTotalStuntElementCounts[type] IS the
        // action-58 record's miTotalCount (the "/45" the HUD popup prints), and the
        // `miCurrentCount <= miTotalCount` assert below would storm on a negative garbage total.
        // Same clear as the census's, deliberately duplicated rather than hoisted: hoisting it
        // above the readiness gate would re-run it on every pump of a multi-frame wait.
        for (int32_t liElementType = 0; liElementType < 3; ++liElementType)
        {
            maiTotalStuntElementCounts[liElementType] = 0;
            for (int32_t liCounty = 0; liCounty < 5; ++liCounty)
            {
                maaiTotalStuntElementCountsPerCounty[liElementType][liCounty] = 0;
            }
        }
        miSignatureTakedownCount = 0;

        static bool sbUnavailableLogged = false;
        if (!sbUnavailableLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbUnavailableLogged = true;
            *CgsDev::Log::gpDebugPrint
                << "[UI-gate] prepare district-map UNAVAILABLE after "
                << KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES
                << " acquire retries -- census SKIPPED, all stunt totals ZEROED (they read 0)\n";
        }

        // ⛔ [gateui] ROUND-3 NOTE (verify_r2_fixgsm F5) -- A GIVE-UP IS PROCESS-FINAL, ON PURPOSE.
        // giDistrictMapAcquireRetries / gbDistrictMapUnavailable / the two one-shot log latches are
        // file statics that live for the process, and the load machine's own terminal state is
        // equally sticky: meDistrictMapLoadStage is E_DISTRICT_MAP_DONE by the time this arm can be
        // reached, and that case returns true without re-issuing anything (the console's shape --
        // nothing in the image, and nothing in this tree, ever writes meDistrictMapLoadStage back
        // to E_DISTRICT_MAP_LOAD_REQUEST outside Construct). So a LATER Prepare pass takes this arm
        // immediately and never re-runs the census, even if Districts.dat became resident in the
        // meantime. That is deliberate, not an oversight: re-arming the acquire from here would
        // re-enter the load machine from inside Prepare's own wait with no bound on how many times
        // the budget could be refilled, which is the stall this round exists to remove.
        // The latches ARE resettable in the one place a fresh attempt is meaningful --
        // LoadDistrictMap's E_DISTRICT_MAP_LOAD_REQUEST arm zeroes them -- so if a future owner
        // ever re-arms meDistrictMapLoadStage (a real Clear()/Destruct(), or a per-track re-prepare
        // that resets the sub-object), the budget starts fresh with no further change here.
        // BrnGameStateModule.h's stage-4 banner is corrected to say this.
        return true;
    }

    // Bind the 2D map over the streamed blob. Console @0x8239CA50..0x8239CA80:
    //     lvx128 v2 <- unk_82FAE140            ; lWorldSize
    //     lvx128 v1 <- unk_82FADED0            ; lWorldOrigin
    //     lwz  r10, 0x3A0(this)                ; mDistrictMapResourceHandle.mpResourceMemory
    //     lwz  r11, 0(r10)                     ; -> the SmallResource's main-memory base   (M)
    //     lwz  r10, 4(r11)                     ; -> the blob's own header offset           (*(M+4))
    //     add  r4, r10, r11                    ; lpData == M + *(M+4)
    //     WorldMap2D::Construct(this+0x370, r4, v1, v2)
    //
    // ⛔ [gateui] TWO FIXES HERE, both live defects in the committed body:
    //  (1) THE `+ *(M+4)` HEADER SKIP WAS MISSING -- the old line handed WorldMap2D the resource's
    //      main-memory BASE, so muWidth/muHeight/mpValues were read off the blob's header words
    //      instead of the packed grid. (SendSetUpAllEventStartsMessage @0x82375A80..0x82375A88
    //      does the identical two-load-and-add on the same resource, so the +4 offset is attested
    //      twice.) The read IS a serialised-blob walk (AGENTS.md's documented exception): the
    //      bytes are a Districts.dat file image, not a C++ object.
    //  (2) THE WORLD RECT WAS TWO FLAGGED ZERO VECTORS -- see KV_DISTRICT_MAP_WORLD_ORIGIN /
    //      _SIZE above for the dyn-init recovery and for why the two are the OPPOSITE way round
    //      from the way the wave scout mapped them.
    const u8* lpResourceMemoryBase =
        *reinterpret_cast<u8* const*>(mDistrictMapResourceHandle.mpResourceMemory);
    const void* lpMapBlob =
        lpResourceMemoryBase + *reinterpret_cast<const u32*>(lpResourceMemoryBase + 4);  // serialised Districts.dat blob header
    mWorldMap2D.Construct(lpMapBlob, KV_DISTRICT_MAP_WORLD_ORIGIN, KV_DISTRICT_MAP_WORLD_SIZE);

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

        // Signature-takedown / jump groups (region types 5 / 7) de-dupe by group id so a multi-region
        // stunt counts ONCE.
        //
        // ⛔⛔ BANNER CORRECTION (measured 2026-08-29, from the ASM -- the previous banner here said
        // "the recovered pseudocode never consumes lnCompressedGroupIdCount ... its consumer was
        // either inlined away or dropped by the decompiler". THAT WAS WRONG, and it cost the pause
        // screen's stat panel a wrong denominator: it printed JUMPS 0/68 against Paradise City's 50.)
        // The consumer is a BRANCH, not a statement, which is why reading the pseudocode alone missed
        // it. X360 @0x8239CBE8:
        //     0x8239CBE8  lwz   r9, 0(r10)        ; laCompressedGroupIds[i]
        //     0x8239CBEC  cmpw  cr6, r9, r31
        //     0x8239CBF0  beq   cr6, loc_8239CC08 ;   found
        //     ...
        //     0x8239CC04  b     loc_8239CC10      ;   not found -> fall through and ADD
        //     0x8239CC08  cmpw  cr6, r11, r16
        //     0x8239CC0C  blt   cr6, def_8239CCDC ; ⭐ FOUND -> jump to the tally switch's DEFAULT
        //                                         ;   arm, i.e. count NOTHING for this region and
        //                                         ;   skip the county classify with it.
        // So an already-seen group id `continue`s the loop. Only types 5 and 7 pass through the
        // filter, which is exactly why SMASHES (type 8) already totalled the retail 400 while JUMPS
        // (type 7) over-counted: 68 type-7 regions, 50 distinct jump groups.
        if (leType == BrnTrigger::GenericRegion::E_TYPE_SIGNATURE_TAKEDOWN ||
            leType == BrnTrigger::GenericRegion::E_TYPE_JUMP)
        {
            CgsID lnGroupId = lpGenericRegion->GetGroupId();
            int32_t lnRawGroupId = static_cast<int32_t>(lnGroupId);
            if (lnRawGroupId == 0)
                lnRawGroupId = static_cast<int32_t>(lpGenericRegion->GetId());

            // A region with no group of its own (group id 0, or a group id equal to its own id)
            // never enters the list and is always counted -- the `cmpld`/`beq` at 0x8239CBCC.
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
                if (lbAlreadySeen)
                {
                    continue;   // ⭐ `blt cr6, def_8239CCDC` -- this region's group is already tallied
                }

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

    // [DIAG] NOT IN THE X360 BINARY. The `[UI-gate]` ladder's rung 0: the census this Prepare just
    // built. It is the ONLY direct read-out of whether the district map bound to a real grid --
    // an all-zero tally means either the map blob is wrong or the world rect is (see
    // KV_DISTRICT_MAP_WORLD_ORIGIN above), and every downstream count/total is then garbage.
    if (UIGateDiagOn())
    {
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] prepare tally jump=" << maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_JUMP]
            << " smash="     << maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_SMASH]
            << " billboard=" << maiTotalStuntElementCounts[E_STUNT_ELEMENT_TYPE_BILLBOARD]
            << " sigTD="     << miSignatureTakedownCount
            << " regions="   << liGenericRegionCount << "\n";
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
        // X360 0x8239FA3C `mr r6, r27` / 0x8239FA40 `li r5, 0` -- four arguments.
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
    // [gateui] VALUE CONFIRMED 2026-08-20 by a direct read of the .i64: the 4 bytes at 0x82001DA0
    // are 3F 00 00 00 == 0.5f big-endian. (flt_82004D0C, the KF_MIN_JUMP_SPEED_MPH immediate at the
    // bottom of this file, reads 42 20 00 00 == 40.0f -- that FLAG can be retired too.)
    const f32 KF_BROADPHASE_HALF_SCALE = 0.5f;

    // [DIAG] NOT IN THE X360 BINARY. Rung 1 of the `[UI-gate]` ladder: what the latch decided for
    // this prop. Emitted at the single exit so it reports the OUTCOME, not the attempt, and it also
    // reports the armed-set size -- because an empty armed set is the difference between
    // "the prop was outside every smash region" and "TriggerQueryManager never ran" (the wave's
    // known blocker D). FIRST-N guarded (KI_UI_GATE_DIAG_FIRST_N), because a single multi-prop
    // smash fires this once PER PROP CONTACT -- the same burst the "[prop-diag] BREAK" rung this
    // pairs with guards against.
    const BrnTrigger::GenericRegion* lpLatchedBefore = mpLastStuntOrSmashElement;
    const u32 luArmedCount = mpTriggerQueryManager->GetActiveTriggerCount();

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

    // [DIAG] see the note at the top of this body.
    static s32 siOnPropHitDiagCount = 0;
    if (UIGateDiagFirstN(&siOnPropHitDiagCount))
    {
        const bool lbLatchedNow = (mpLastStuntOrSmashElement != 0 &&
                                   mpLastStuntOrSmashElement != lpLatchedBefore);
        const char* lpcLatch = "none";
        if (lbLatchedNow)
        {
            lpcLatch = (meLastStuntElementType == E_STUNT_ELEMENT_TYPE_SMASH) ? "SMASH" : "BILLBOARD";
        }
        *CgsDev::Log::gpDebugPrint
            << "[UI-gate] OnPropHit zone=" << luZoneId
            << " prop=" << luPropId
            << " latch=" << lpcLatch
            << " armed=" << luArmedCount << "\n";
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

        // Already-completed? ProgressionManager keeps a PER-TYPE done-set of completed
        // stunt-element keys. X360 @0x8239D460 line 99: `Find(v18 + 30568, &key)` -- 30568 is the
        // array BASE, i.e. index `4104 * type` with type == E_STUNT_ELEMENT_TYPE_JUMP (0), which
        // is why the type does not appear in the pseudocode here at all.
        // [gateui] RE-POINTED at the TYPED query (DWARF BrnProgressionManager.h:447
        // `bool IsStuntElementDone(BrnGameState::StuntElementType, CgsID) const`). The type-less
        // form this used to call had no body anywhere and no way to pick a set.
        const bool lbHasBeenDoneBefore =
            mpProgressionManager->IsStuntElementDone(E_STUNT_ELEMENT_TYPE_JUMP, lJumpKey);

        // Speed gate: |speed| < 40 mph (== KF_MIN_JUMP_SPEED_MPH) -> too slow, no jump.
        //
        // ⭐⭐ [bugwave 2026-08-23] FIELD-NAME DEFECT FIXED, and the old FLAG RETIRED. This read
        // used to be `mfMaxSpeedMPH`, justified by a FLAG that said "the X360 reads +0x3CC, which
        // the committed RaceCarState layout names mfMaxSpeedMPH ... semantically it reads more
        // like mfSpeedMPH @ +0x3C8". BOTH halves of that FLAG were wrong by exactly one field.
        // The producer settles it -- BrnVehicleOutputInterface_UpdateRaceCarState.cpp:50-51 (and
        // the identical map in BrnVehicleEvents.h:72) transcribe the only writer of this struct,
        // VehicleOutputInterface::UpdateRaceCarState @0x825EC808:
        //     +0x3CC  972  mfSpeedMPH      <- physics+0x6C0 lane .x   (the CURRENT speed)
        //     +0x3D0  976  mfMaxSpeedMPH   <- spec+0x70   lane .z     (the car's SPEC top speed)
        // and three independent consumers agree that +0x3CC is mfSpeedMPH (GameBridgeWorldToX.cpp
        // :103, BrnGameModule.cpp:1967/:2000, BrnBehaviourGameplayExternal.cpp:1604).
        // So the console's take-off gate tests the CURRENT speed, and reading mfMaxSpeedMPH here
        // compared a car's ~150 mph spec top speed against the 40 mph gate -- i.e. the gate could
        // never reject, and a jump region would arm a take-off at a standstill.
        // (This is the recurring "X360 number used on the host" class in its offset-arithmetic
        // form: the offset was right, the field it was mapped to was one slot off.)
        const RaceCarState* lpPlayerRaceCarState = lpActiveRaceCarInterface->GetPlayerRaceCarState();
        const f32 lfPlayerSpeed = lpPlayerRaceCarState->mfSpeedMPH;   // X360 +0x3CC == 972
        if (std::fabs(lfPlayerSpeed) < 40.0f || (lbHasBeenDoneBefore && !mbAlwaysToJumpCameras))
        {
            // [DIAG] NOT IN THE X360 BINARY. Rung 2a of the `[jump-ladder]`: the region WAS
            // entered and the state machine DID run -- it declined the jump, and says why. This
            // is the line that separates "no jump trigger reached the StuntManager" (no
            // `[jump-ladder]` line at all) from "the take-off gate rejected it".
            static s32 siDropLines = 0;
            const s32  KI_JUMP_DROP_DIAG_FIRST_N = 8;
            if (siDropLines < KI_JUMP_DROP_DIAG_FIRST_N && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siDropLines;
                *CgsDev::Log::gpDebugPrint
                    << "[FLAG PC bring-up] [jump-ladder] take-off DECLINED speed="
                    << lfPlayerSpeed
                    << " (gate 40 mph) doneBefore=" << (lbHasBeenDoneBefore ? 1 : 0)
                    << " alwaysToJumpCameras=" << (mbAlwaysToJumpCameras ? 1 : 0) << "\n";
            }
            mpLastJumpElement = 0;   // drop the jump
            return;
        }

        // Direction gate: did the player enter the jump region facing forwards? dot(playerDir,
        // jumpDir) >= 0. If the region is one-way (miIsOneWay @+0x37) and the player entered backwards,
        // also drop the jump.
        // FLAG: BoxRegion::ComputeDirection is a foreign TU returning the region's forward vector; the
        // X360 dots it with the player's facing (vmsum3fp128). Reconstructed by name; the player facing
        // comes from the player RaceCarState transform.
        const Vector3 lTriggerDir = mpLastJumpElement->GetBoxRegion()->ComputeDirection();
        const Vector3 lPlayerDir  = lpActiveRaceCarInterface->GetPlayerDirection();
        const f32 lfDot = lPlayerDir.x * lTriggerDir.x + lPlayerDir.y * lTriggerDir.y + lPlayerDir.z * lTriggerDir.z;
        const bool lbEnteredJumpForwards = (lfDot >= 0.0f);

        // ⭐ [bugwave 2026-08-23] THE ONE-WAY GATE READ THE WRONG FIELD. The console loads a
        // BYTE at region +0x37 and tests it signed-greater-than-zero
        // (UpdateJumps @0x8239D650 `lbz r11, 0x37(r10)` / `extsb` / `cmpwi r11, 0` / `bgt`),
        // and +0x37 is `miIsOneWay` in the DWARF-gated layout (BrnGenericRegion.h:175).
        // This tested `GetCameraCut1()`, the int16 at +0x30 -- a different member entirely,
        // and one that is non-zero on every region that has an authored camera. The effect was
        // to treat ordinary two-way jumps as one-way and silently drop them whenever the player
        // crossed the region facing 'backwards' by the ComputeDirection convention.
        if (mpLastJumpElement->IsOneWay() && lfDot < 0.0f)
        {
            mpLastJumpElement = 0;   // one-way jump entered backwards -> drop
            return;
        }

        if (!lbIsAGameModeActive)
        {
            // Post the jump-start action (type 56, 24 bytes). VERIFIED field ORDER against the X360
            // asm (UpdateJumps 0x8239D460, the !a6 block building &v34): the stack record is
            //   v34 @+0  (8B)        = the stunt key (group id, or own id when grouped to 0)
            //   v35 @+8  (4B, extsb) = the camera-TYPE byte: lbz @+0x34 forwards / @+0x35 backwards
            //   v36 @+12 (4B, extsh) = the camera-CUT  half: lhz @+0x30 forwards / @+0x32 backwards
            //   v37 @+16 (word)      = the FIRST-TIME flag == !doneBefore
            //                          (X360 v37 = (cntlzw(doneBefore) & 0x20) != 0)
            //   pad @+20 (4B)        rounds the record to 24 bytes.
            // This corrects the prior reconstruction, which mis-ordered the three non-key fields
            // (mbDoneBefore@+8 / cut@+12 / type@+16) AND wrote the INVERTED 'doneBefore' at +8 where
            // the X360 actually writes the !doneBefore first-time flag (at +16).
            // FLAG: the OnJumpStartAction struct shape is not committed; built as this X360 raw record.
            //
            // ⭐⭐ [bugwave 2026-08-23] CORRECTION: THE FIRST-TIME FIELD AT +0x10 IS A BYTE.
            // The previous shape declared it `s32 miFirstTimeFlag`. The asm says otherwise at
            // BOTH ends, and on the console the difference is total:
            //   producer @0x8239D6B4  `stb  r30, 0xE0+var_80(r1)`   -- a BYTE store at +0x10
            //   consumer @0x822381CC  `lbz  r11, 0x10(r30)`         -- a BYTE load  at +0x10
            //     (MainDirector::ProcessInputQueue case 56, bodied this same wave)
            // Had the producer really stored a word, the consumer's `lbz` would have read the
            // MOST-SIGNIFICANT byte of a big-endian 0/1 -- i.e. always 0 -- and the authored
            // super-jump camera could never have taken its first-time arm. This is the
            // X360-numbers-on-the-x64-host bug class in its endianness form: on this
            // little-endian host an s32 1 at +0x10 happens to read back as byte 1, so the
            // mismatch would have been invisible here and wrong on the console. Modelled as the
            // byte it is, with the record explicitly zeroed (the console leaves +0x11..+0x17 as
            // stale stack; nothing reads them).
            struct OnJumpStartActionPayload
            {
                CgsID mKey;            // +0x00 (8)  -- std  (the stunt-element key)
                // ⭐⭐ [bugwave 2026-08-23] THESE TWO WERE IN EACH OTHER'S SLOTS. The offsets
                // the previous banner quoted are right; the LABELS on them were guesses, and
                // the wiring below followed the labels instead of the offsets. Settled by the
                // DWARF-gated layout in BrnGenericRegion.h:169-175 --
                //     0x30/0x32  int16_t miCameraCut1/2     (the authored camera INDEX)
                //     0x34/0x35  int8_t  miCameraType1/2    (a StuntCameraType: 0/1/2)
                // and the console reads the BYTE into +0x08 and the HALF into +0x0C
                // (UpdateJumps @0x8239D6BC-0x8239D6EC: `lbz 0x34` -> stw var_88 == +0x08,
                //  `lhz 0x30` -> stw var_84 == +0x0C). So +0x08 is the TYPE and +0x0C the CUT.
                // The consumer proves it independently: MainDirector::ProcessInputQueue case 56
                // @0x822381B4 tests `+0x08 == 1` -- i.e. E_STUNT_CAMERA_TYPE_CUSTOM -- and then
                // publishes `+0x0C` as miActionRequestedCamera, the camera id. A cut INDEX can
                // never equal the enum, which is exactly what the boot-drive measured:
                // `[jump-ladder] action=56 OnJumpStart posted cut=42 type=1` fell through every
                // arm and left miThisFramesActionFlags at 0, so no jump camera was ever
                // requested. Swapping the two declarations puts each value in its own slot.
                s32   miCameraType;    // +0x08      -- stw  (TYPE byte: 0x34 fwd / 0x35 bwd, extsb)
                s32   miCameraCut;     // +0x0C      -- stw  (CUT  half: 0x30 fwd / 0x32 bwd, extsh)
                u8    mbFirstTime;     // +0x10      -- stb  (== !doneBefore)
                u8    mauPad[7];       // +0x11..+0x17 (console: stale stack; zeroed here)
            } lJumpStartAction;
            std::memset(&lJumpStartAction, 0, sizeof(lJumpStartAction));

            lJumpStartAction.mKey = lJumpKey;
            lJumpStartAction.miCameraType =
                lSelectJumpCameraTypeCut(mpLastJumpElement, lbEnteredJumpForwards, &lJumpStartAction.miCameraCut);
            lJumpStartAction.mbFirstTime = lbHasBeenDoneBefore ? 0u : 1u;   // X360: extrwi(cntlzw(doneBefore),1,26)

            lpActionQueueImpl->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lJumpStartAction), KI_EVENT_ON_JUMP_START, 24);

            // [DIAG] NOT IN THE X360 BINARY. Rung 3 of the `[jump-ladder]`: the take-off action
            // that carries the camera request actually went onto the game-action queue.
            {
                static s32 siJumpStartLines = 0;
                const s32  KI_JUMP_START_DIAG_FIRST_N = 8;
                if (siJumpStartLines < KI_JUMP_START_DIAG_FIRST_N && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siJumpStartLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] [jump-ladder] action=56 OnJumpStart posted cut="
                        << lJumpStartAction.miCameraCut
                        << " type=" << lJumpStartAction.miCameraType
                        << " firstTime=" << static_cast<s32>(lJumpStartAction.mbFirstTime) << "\n";
                }
            }

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
            // [DIAG] NOT IN THE X360 BINARY. Rung 4 of the `[jump-ladder]` -- THE COUNT. This
            // is the call that reaches Profile::AddStuntElement(JUMP, key, county) and posts
            // game action 58, i.e. the one that makes a super jump count. If the fan-out and
            // take-off rungs print and this one does not, the break is in the landing settle.
            {
                static s32 siLandLines = 0;
                const s32  KI_JUMP_LAND_DIAG_FIRST_N = 8;
                if (siLandLines < KI_JUMP_LAND_DIAG_FIRST_N && CgsDev::Log::gpDebugPrint != 0)
                {
                    ++siLandLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[FLAG PC bring-up] [jump-ladder] JUMP COMPLETED -- ProcessStuntElement"
                           "(isJump=true), landingTime=" << mfJumpLandingTime << "\n";
                }
            }

            // X360 0x8239D7C0 `mr r6, r24` / 0x8239D7C4 `li r5, 1` -- four arguments.
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
                // [gateui] PARKED CALL -- see the block below (same symbol, same reason).
                return;
            }
            liTrophyId = 1;         // BILLBOARD
        }

        // ⚠️ [gateui] PARKED CALLS, NOT FABRICATED -- the ModeManager::HandleWorldStunt treatment
        // (StuntManager_gUI_00.cpp :: ProcessStuntElement, step 6), applied for the same measured
        // reason. The console runs, in this order:
        //     mpProgressionManager->OnTrophyUnlock(liTrophyId);            // X360 0x82389740
        //     mpProgressionManager->CheckForSpecialCarUnlocks();           // X360 0x82396058
        // Owner `deps` PARKED both this wave with a measured blocker list, written into the
        // declarations at BrnProgressionManager.h (`⛔ [gateui] PARKED 2026-08-20, NOT bodied`):
        //     OnTrophyUnlock            needs ProgressionManager::UnlockCarFromTrophy @0x8237B0E8
        //                               and an owning header for BrnProgression::ProgressionData's
        //                               trophy table (+64 base / +68 count, 16-byte records)
        //     CheckForSpecialCarUnlocks needs ProgressionManager::ComputeCompletionPercentage
        //                               @0x8238A198 (320 insns) and
        //                               ProgressionManager::UnlockSpecialCars @0x8237AF38 (106 insns)
        // NEITHER symbol has a body anywhere in b5-decomp/src and no link stub stands in, so
        // calling them is a hard LNK2019 that blocks the ENTIRE gsm mount -- and with it every
        // `[UI-gate]` line this wave exists to print. Both are trophy/car-unlock side effects that
        // run AFTER the action-58 record is fully built (CheckForTrophyUnlocks is handed a
        // finished OnStuntElementCompleteAction and writes none of its five fields), so parking
        // them costs the HUD popup nothing. Land the calls the moment either body does.
        (void)liTrophyId;
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
            // ⭐ [gateui] REAL (2026-08-20; was an inert deferral). Console order @0x82399458 case 0,
            // store for store: Clear the receiver queue FIRST, then LoadBundle. The four literals are
            // the console's own (`aDistrictsDat`, event id 1, pool 5 == the GameData pool, hdCache 0);
            // the request goes through the SAME committed builder GameStateModule::Prepare stage 4 has
            // been driving since 2026-08-11 (BrnGameStateModule.cpp, E_PREPARESTAGE_STUNT_MANAGER),
            // which is what this machine now REPLACES -- see the stage body there.
            //
            // ⛔ [gateui] ROUND-3 (verify_r2_fixgsm F5) -- NOT IN THE X360 BINARY. This is the one
            // place a FRESH district-map attempt begins, so it is the one place the PC bring-up's
            // give-up latches are reset. Today nothing re-arms meDistrictMapLoadStage after
            // Construct, so this runs exactly once per process and a give-up is process-final (see
            // the note on Prepare's give-up arm); the reset exists so that whenever a real
            // Clear()/Destruct() or a per-track re-prepare DOES re-arm the stage, the retry budget
            // starts fresh instead of taking the spent give-up arm for ever.
            giDistrictMapAcquireRetries = 0;
            gbDistrictMapUnavailable    = false;

            mReceiverQueue.Clear();
            lpOutput->GetResourceRequestInterface()->LoadBundle(
                &mReceiverQueue, KI_DISTRICT_MAP_EVENT_ID, KI_DISTRICT_MAP_POOL_ID,
                KPC_DISTRICT_MAP_BUNDLE_NAME, /*lbUseHDCache*/ false);
            meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_RESPONSE;
            return false;   // the console returns 0 here too
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
            // ⭐ [gateui] REAL (2026-08-20). The X360 INLINES the acquire builder here -- it writes the
            // 24-byte record { mpUser = &mReceiverQueue, miEventId = 1, miPoolId = 5,
            // mResourceId = HashString("Districts") } and AddEvents it as type 4 onto the request
            // interface's <3072,16> queue. Issued here through the de-inlined committed builder
            // (RequestInterface<3072>::AcquireResource), which is the identical request.
            //
            // ⚠️ THE `| 0x500000000LL` IN THE PSEUDOCODE IS A HEX-RAYS STORE-FUSION ARTIFACT, not a
            // tag: the decompiler folded the separate `li r10,5 / stw miPoolId` store into the `std`
            // of the hash. HashString @0x828D84A8 ends `clrldi r3,r3,32`, so the id's high dword is
            // ZERO; a tagged id matches nothing in Pool::FindResource and the pool replies with a
            // both-null handle. This is the SAME defect that printed
            // `[StreetManager] district map: handle=0` until 2026-08-11 -- see the long note at
            // StreetManager::LoadDistrictMap (BrnGameStateStreetManager_wB_01.cpp), which is the
            // working template this leg is copied from. Do not re-introduce the tag.
            mReceiverQueue.Clear();
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                &mReceiverQueue, KI_DISTRICT_MAP_EVENT_ID, KI_DISTRICT_MAP_POOL_ID,
                KPC_DISTRICT_MAP_RESOURCE_NAME);
            meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
            return false;
        }

        case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
        {
            if (mReceiverQueue.GetCount() <= 0)
                return false;   // still waiting for the acquire response

            // ⭐ [gateui] REAL (2026-08-20). The console inlines GetFirstEvent -- its
            // `v7 = (a1[237] <= 0) ? 0 : a1[238] + a1[235] + 8` is exactly the receiver queue's
            // buffer base + first-event offset + the 8-byte event header, i.e. the payload pointer --
            // and then reads the pair at payload +24:
            //     a1[232] = *(v7+24);   a1[233] = *(v7+28);
            // 232*4 == 0x3A0 == mDistrictMapResourceHandle.mpResourceMemory, 233*4 == 0x3A4 ==
            // .mpSourceEntry. That record at payload +0x18 IS the AcquireResourceResponse's
            // {mpResourceMemory, mpSourceEntry} pair (PoolModule::DoAcquireResourceRequest
            // @0x828FCD48 builds it), so it is read BY MEMBER and NEVER at the console's literal
            // +0x18/+0x1C -- the host handle is 16 bytes where the console's is 8, and every literal
            // past it shifts. Same idiom as StreetManager::LoadDistrictMap and
            // TriggerQueryManager::Prepare's acquire leg.
            const CgsModule::Event* lpEvent = 0;
            s32                     liSize  = 0;
            mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

            if (lpEvent != 0)
            {
                // reinterpret_cast, not static_cast: CgsResource::Events::Event and CgsModule::Event
                // are unrelated roots and the receiver queue hands out the module one.
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);

                mDistrictMapResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                mDistrictMapResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
            }

            // ⚠️ [FLAG PC bring-up] NOT IN THE X360 BINARY -- the not-ready retry. The console
            // advances straight to DONE here. See KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES at the top
            // of this file for why the host cannot: the pool answers an acquire for a NON-RESIDENT
            // bundle with a both-null handle, and Prepare would then dereference it.
            //
            // ⛔ [gateui] ROUND-3 FIX (verify_r2_fixgsm F2) -- THE TEST IS THE FULL READINESS
            // PREDICATE, NOT JUST `mpResourceMemory == 0`. Round 2 tested only the outer pointer
            // here while Prepare tested BOTH clauses, so a handle that came back non-null but whose
            // SmallResource main-memory word was still null (the console's own assert line 125)
            // latched DONE with gbDistrictMapUnavailable FALSE -- and Prepare then waited on a
            // machine that never re-issues, for ever. Both sites now ask
            // DistrictMapHandleReady(), so the retry budget bounds EVERY not-ready shape and
            // Prepare's wait is guaranteed to terminate in either the ready or the give-up arm.
            if (!DistrictMapHandleReady(mDistrictMapResourceHandle) &&
                giDistrictMapAcquireRetries < KI_MAX_DISTRICT_MAP_ACQUIRE_RETRIES)
            {
                ++giDistrictMapAcquireRetries;
                meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_REQUEST;   // re-issue next pump
                return false;
            }
            if (!DistrictMapHandleReady(mDistrictMapResourceHandle))
            {
                gbDistrictMapUnavailable = true;   // Prepare skips the census, loudly
            }

            meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;
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
    {
        mpLastJumpElement = lpRegion;

        // [DIAG] NOT IN THE X360 BINARY. Rung 1 of the `[jump-ladder]`: the jump region reached
        // the StuntManager at all. Until the player-trigger fan-out landed
        // (BrnTriggerQueryManager.cpp :: PreWorldUpdatePlayerTriggersBringUp) this function had
        // no caller in the tree, so this line is the direct witness that the root-cause fix is
        // live. First-N, because a jump region is re-entered every frame until take-off arms.
        static s32 siLatchLines = 0;
        const s32  KI_JUMP_LATCH_DIAG_FIRST_N = 8;
        if (siLatchLines < KI_JUMP_LATCH_DIAG_FIRST_N && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siLatchLines;
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] [jump-ladder] LatchJumpElement: jump region latched (id="
                << static_cast<s32>(lpRegion->GetRegionIndex()) << ")\n";
        }
    }
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
