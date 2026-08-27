#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                  // CgsID, Vector3 (== rw::math::vpu::Vector3)
#include "rw/math/vpu/types.h"                               // rw::math::vpu::Vector3 (DriveThruTriggerData::mPosition, BoxRegion cache)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"   // CgsContainers::BitArray<46>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"  // CgsResource::ResourcePtr<T>
#include "SharedClasses/Trigger/BrnRegion.h"                 // BrnTrigger::BoxRegion (mBoxRegionCache by value)
#include "SharedClasses/Trigger/BrnGenericRegion.h"          // BrnTrigger::GenericRegion (REAL home; Type enum, meType @0x36)
// [drive-thru wave 2026-08-27] GameStateModuleIO::GameActionQueue -- the typedef this header's
// Update/ProcessDriveThru/UnlockCarChallengeForCar/SetPlayerCarDriver signatures name. It was
// only ever forward-declared here as `namespace GameStateModuleIO { struct OutputBuffer; }`, so
// every one of those four declarations was C2039/C2061 ("GameActionQueue is not a member of
// BrnGameState::GameStateModuleIO") and the whole class failed to parse -- which is the ACTUAL
// reason this TU never compiled and never mounted. The typedef's real home is
// BrnGameStateSharedIO.h:52 (`typedef CgsModule::VariableEventQueue<13312,16> GameActionQueue`);
// include it rather than re-forward-declaring (AGENTS.md "Reconstruct includes; don't fake them").
// No cycle: BrnGameStateSharedIO.h does not reach back to this header.
#include "GameSource/GameState/BrnGameStateSharedIO.h"       // BrnGameState::GameStateModuleIO::GameActionQueue

// Forward declarations of the types the manager routes through (pointers only).
namespace BrnResource    { struct VehicleList; struct VehicleListEntry; }
namespace BrnWorld       { struct GlobalColourPalette;
                           namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }
namespace BrnProgression { class  ProgressionManager; }
namespace BrnTrigger     { struct TriggerData; }
// ⚠️ [drive-thru wave 2026-08-27] `struct`, NOT `class`. CgsTimerRequestInterface.h:43 declares it
// `struct TimerRequestInterface`, and MSVC mangles the class-key into the symbol: with `class` here
// this header's Update/SetPlayerCarDriver declarations mangled to ...PEAVTimerRequestInterface...
// while the definitions (which see the real header) mangled to ...PEAUTimerRequestInterface..., so
// every call site linked against a symbol NO TU COULD DEFINE. The compile gate cannot see it -- only
// a LINK can [[shadowing-redeclarations]]. Measured: it was one of the LNK2019s on the first mount.
namespace CgsSystem      { struct TimerRequestInterface; }
namespace BrnGameState
{
    class CarSelectManager;
    class TrainingManager;
    class ModeManager;
    class GameStateModule;
    namespace GameStateModuleIO { struct OutputBuffer; }
}

namespace BrnGameState
{
// ============================================================================
// X360 DriveThruManager constants (recovered from the immediates the bodies use; the
// DWARF declares the named class-scope KF_*/KI_* statics in BrnDriveThruManager.cpp).
// ============================================================================
//   KI_MAX_DRIVE_THRUS_IN_THE_WORLD == 46 (0x2E): the maDriveThruTriggerData / BitArray<46>
//   capacity Prepare asserts against, and the per-frame Update sweep count.
const u32 KI_MAX_DRIVE_THRUS_IN_THE_WORLD = 46;

// ----------------------------------------------------------------------------
// DriveThruTriggerData (DWARF BrnDriveThruManager.h:46). One entry per drive-thru region
// loaded for the level. Stride 0x30 (48): the X360 Construct loop advances `r11 += 0x30`
// and writes mfTimeToActiveation (+0x20) = -1.0 and mpGenericRegion (+0x00) = 0 each step.
// ----------------------------------------------------------------------------
struct DriveThruTriggerData
{
    const BrnTrigger::GenericRegion* mpGenericRegion;   // +0x00
    u8                               maPad04[0x10 - 0x04];
    Vector3                          mPosition;          // +0x10 (16B, 16-aligned)
    f32                              mfTimeToActiveation;// +0x20  (-1.0 == inactive)
    u8                               maPad24[0x30 - 0x24];
};

// ============================================================================
// BrnGameState::DriveThruManager (DWARF BrnDriveThruManager.h:62). Owns the world's
// drive-thru regions (junk yard / gas station / body shop / paint shop / car park),
// drives the per-frame discovery + activation flow, and posts game actions onto the
// GameStateModuleIO action queue. A plain struct, no base.
//
// Member byte offsets below are pinned from the X360 Construct/Prepare/Update/HandleDriveThru
// asm (`*(this + N)` stores/reads). The semantic-parity rule (AGENTS.md) makes named members
// authoritative; _AssertLayout() pins the offsets the reconstructed bodies depend on so the
// MSVC gate enforces fidelity.
// ============================================================================
struct DriveThruManager
{
public:
    void Construct(CarSelectManager* lpCarSelectManager,
                   TrainingManager*  lpTrainingManager,
                   ModeManager*      lpModeManager,
                   GameStateModule*  lpGameStateModule,
                   BrnProgression::ProgressionManager* lpProgressionManager);  // X360 0x8236E2E8

    bool Prepare(const BrnTrigger::TriggerData* lpTriggerData,
                 CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> lpPlayerCarColours); // X360 0x8236E3C0

    // X360 0x8239EEF0. Per-frame tick: ages the active drive-thru timers, fires the
    // activation/discovery events and posts the resulting game actions.
    void Update(GameStateModuleIO::GameActionQueue*  lpActionQueue,
                CgsSystem::TimerRequestInterface* lpTimerRequestInterface,
                f32  lfTimeStep,
                BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRcOutputInterface,
                bool lbIsOnline,
                bool lbIsFreeburn,
                bool lbIsShowtiming,
                bool lbIsAnythingPaused,
                bool lbIsInJunkyard,
                bool lbInviteInProgress,
                const BrnResource::VehicleList* lpVehicleList);

    // X360 0x8239B010. Routes a player-entered drive-thru region into the shop flow:
    // checks the player car against the vehicle list, tallies discovered drive-thrus, and
    // posts the appropriate game actions onto the output buffer. Bodied by THIS TU.
    void HandleDriveThru(const BrnTrigger::GenericRegion*                                             lpRegion,
                         const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                         const BrnResource::VehicleList*                                              lpVehicleList,
                         GameStateModuleIO::OutputBuffer*                                             lpOutput);

    void DriveThroughsCloseOnceActivatedUntilFurtherNotice();   // own TU; declared-only
    void DriveThroughsCanNowOpenAgain();                        // own TU; declared-only
    bool Release();                                             // own TU; declared-only
    void Destruct();                                           // own TU; declared-only

    s32  GetTotalDriveThrusOfType(BrnTrigger::GenericRegion::Type leTriggerType);  // X360 0x82356468

private:
    // X360 0x82386840. When a body-shop repair unlocks a car that gates an event junction,
    // flag that event found, bump the count, and post the unlock + autosave actions.
    void UnlockCarChallengeForCar(CgsID lRepairedCarID, GameStateModuleIO::GameActionQueue* lpActionQueue);

    // X360 0x8239B6E8. Dispatches one drive-thru type to its shop behaviour (junk yard /
    // gas station / body shop / paint shop / car park). Bodied by THIS TU.
    void ProcessDriveThru(BrnTrigger::GenericRegion::Type leTriggerType,
                          GameStateModuleIO::GameActionQueue*   lpActionQueue,
                          CgsSystem::TimerRequestInterface* lpTimerRequestInterface,
                          BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRcOutputInterface,
                          bool  lbIsOnline,
                          bool* lpbIsInJunkyard);

    // The four below have their own X360 TUs; declared-only (called by the bodied funcs).
    void SetPlayerCarDriver(GameStateModuleIO::GameActionQueue* lpActionQueue,
                            CgsSystem::TimerRequestInterface* lpTimerRequestInterface,
                            const BrnTrigger::BoxRegion* lpBoxRegion,
                            bool lbPlayerDriving, f32 lfMaxSpeed);                 // X360 0x823867A0
    bool DoesDriveThruTypeClose(BrnTrigger::GenericRegion::Type leType);          // own TU
    void PlayAutoRepairTraining(const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRcOutputInterface,
                                const BrnResource::VehicleList* lpVehicleList);    // X360 0x82378848

    static void _AssertLayout();  // never called; pins the offsets the bodies rely on

    // ---- layout (offsets pinned from X360 asm; see _AssertLayout) ----------
    DriveThruTriggerData          maDriveThruTriggerData[KI_MAX_DRIVE_THRUS_IN_THE_WORLD]; // +0x000  (46 * 0x30 == 0x8A0)
    CgsContainers::BitArray<46u>  maDriveThroughClosed;     // +0x8A0 (one u64)

    s32 miTotalCarParks;     // +0x8A8 (2216)
    s32 miTotalGasStations;  // +0x8AC (2220)
    s32 miTotalBodyShops;    // +0x8B0 (2224)
    s32 miTotalPaintShops;   // +0x8B4 (2228)
    s32 miTotalJunkYards;    // +0x8B8 (2232)
    s32 miTotalDriveThrus;   // +0x8BC (2236)

    BrnTrigger::GenericRegion::Type meDiscoveredDriveThruType;     // +0x8C0 (2240)
    s32                             miNumDriveThrusOfThisTypeFound;// +0x8C4 (2244)
    s32                             miTotalDriveThrusOfThisType;   // +0x8C8 (2248)
    bool                            mbCurrentDriveThruJustFound;   // +0x8CC (2252)
    u8                              maPad8CD[0x8D0 - 0x8CD];

    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mpPlayerCarColours;  // +0x8D0 (2256)

    BrnTrigger::BoxRegion           mBoxRegionCache;  // +0x8F0 (2288)  (9 words copied by HandleDriveThru)
    BrnTrigger::GenericRegion::Type meDriveThruCache; // +0x914 (2324)
    CgsID                           mIdCache;         // +0x918 (2328)
    bool                            mbIsClosed;       // +0x920 (2336)
    u8                              maPad921[0x924 - 0x921];

    CarSelectManager*                   mpCarSelectManager;  // +0x924 (2340)
    TrainingManager*                    mpTrainingManager;   // +0x928 (2344)
    ModeManager*                        mpModeManager;       // +0x92C (2348)
    GameStateModule*                    mpGameStateModule;   // +0x930 (2352)

    // CgsID is u64 (8 bytes, BrnCommonTypes.h:32). The two CgsID members are CONTIGUOUS: the X360
    // does `std @0x938` then `std @0x940`, so mRepairedCarID(8)@0x938..0x940 and
    // mDriveThruToTurnOffId(8)@0x940..0x948 with NO pad between them. The former maPad93C/maPad944/
    // maPad94F pads assumed CgsID==4 and were spurious -- they shifted the trailing bool block and
    // pushed mpProgressionManager off 0x950, failing the layout assert. Removed; natural 8-byte
    // alignment now reproduces the X360 layout.
    CgsID mRepairedCarID;       // +0x938 (2360)  (8B; contiguous with the next CgsID)
    CgsID mDriveThruToTurnOffId;// +0x940 (2368)  (8B)

    bool  mbCurrentCarJustRepaired;        // +0x948 (2376)
    bool  mbNeedToSendDriveThruOffMessage; // +0x949 (2377)
    bool  mbPlayerCanUseDriveThrus;        // +0x94A (2378)
    bool  mbPlayerCanUseJunkyards;         // +0x94B (2379)
    bool  mbDriveThroughsCloseWhenUsed;    // +0x94C (2380)
    bool  mbNeedToSendCantPaintMessage;    // +0x94D (2381)
    bool  mbNeedToSendMustFixMessage;      // +0x94E (2382)
    // (3-byte natural pad to align the pointer) -> mpProgressionManager @0x950.

    BrnProgression::ProgressionManager* mpProgressionManager;  // +0x950 (2384)
};
}
