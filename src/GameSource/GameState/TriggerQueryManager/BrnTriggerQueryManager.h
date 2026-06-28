// ===== Owning header: GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h =====
// BrnGameState::TriggerQueryManager — the GameState-side per-frame trigger dispatcher owned by
// GameStateModule (DWARF BrnGameStateModule.h:772 -> `TriggerQueryManager mTriggerQueryManager`).
// Plain value type (no base, no vtable: none of the reconstructed X360 bodies touch a vptr on `this`).
//
// MINIMAL SLICE (BrnDriveThruManager.h / BrnGameStateModuleIO.h precedent): only the members the
// reconstructed functions touch are declared, placed at their X360 byte offsets by leading
// reserved blobs. The full class preamble is NOT modelled — grow these reserved blobs into real
// members from the full DWARF layout (BrnTriggerQueryManager.h) when the whole class lands.
//
// X360-attested member offsets (from Construct @0x82364BF0, ProcessPlayerTriggers @0x8239BF80,
// UpdateTriggers @0x82391FD8):
//   maActiveTriggers          : elements this+912 (0x390), live-count word this+1424 (0x590)
//   mpTriggerData             : this+1568 (0x620)  (ResourcePtr<TriggerData>; +0 == main-memory ptr)
//   mLastPlayerPosition       : this+1776 (0x6F0)  (Vector3, 16B SIMD; cached for refresh test)
//   mbTriggersUpdated         : this+1808 (0x710)  (byte dirty flag)
//   mbPlayerInTrafficLightRegion : this+1809 (0x711)
//   mPlayerCurrentTrafficLightId : this+1812 (0x714)
//   mpProgressionManager      : this+1816 (0x718)
//   mpTakedownManager         : this+1820 (0x71C)
//   mpRoadRulesManager        : this+1824 (0x720)
//   mLandmarkIndexArray       : elements this+1832 (0x728), live-count word this+1864 (0x748)
//   mbDoSoundLookAheadThisFrame : this+1868 (0x74C)
//   mbCarHasTeleported        : this+1869 (0x74D)
#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                          // Vector3 (rw::math::vpu::Vector3)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                  // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsArray.h"                             // Array<T,N> (generic, committed)
#include "GameSource/GameState/BrnGameStateTypes.h"                                 // BrnGameState::LandmarkIndex
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"           // BrnGameState::LightTriggerId (committed typedef home)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"                  // CgsResource::ResourcePtr<T>
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent

// Foreign types this dispatcher routes hits into. The three target functions only call named
// methods on these / store pointers to them, so forward declarations + the declared-only methods
// in their committed homes (grown additively) suffice; their full layouts are not needed here.
namespace BrnTrigger      { struct TriggerData; struct TriggerRegion; struct GenericRegion; }
namespace BrnProgression  { struct ProgressionManager; }
namespace BrnGameState    { class TakedownManager; class RoadRulesManager; class StuntManager; class DriveThruManager; }
namespace BrnResource     { struct VehicleList; }
namespace BrnGameState { namespace GameStateModuleIO { struct OutputBuffer; struct PostWorldInputBuffer; } }
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

namespace BrnGameState
{
// X360-attested fixed array of the landmark/region indexes currently armed for the active game
// mode. The element home (BrnGameState::LandmarkIndex) and the generic Array<T,N> body are both
// committed; the leaf explicit instantiation lives in Array_LandmarkIndex_16.cpp.
typedef Array<LandmarkIndex, 16> LandmarkIndexArray;

class TriggerQueryManager
{
public:

    // ---- functions this TU owns (bodies in BrnTriggerQueryManager.cpp) ----

    // X360 0x82364BF0. One-time init: asserts the three injected manager pointers are non-NULL,
    // zeroes the cached trigger state (all the Array<> live-count words, the cached vectors, the
    // dirty/region flags), stores the manager pointers, and registers the five CPU perf monitors.
    void Construct(BrnProgression::ProgressionManager* lpProgressionManager,
                   TakedownManager*                    lpTakedownManager,
                   RoadRulesManager*                   lpRoadRulesManager);

    // X360 0x82391FD8. Refresh the world TriggerEntityModule's set of armed trigger regions for
    // this frame: validate every road-limit region once, (re)submit the armed landmark regions,
    // then — when the player has moved far enough — drop the previously-active region set and
    // re-arm every region whose box is within the clip radius of the player. Marks mbTriggersUpdated.
    void UpdateTriggers(GameStateModuleIO::OutputBuffer*                                       lpOutput,
                        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface);

    // X360 0x8239BF80. Route one player-trigger hit (a generic-region trigger the player is inside)
    // to the right gameplay manager based on the generic-region category: drive-thru categories
    // (junk yard / gas station / body shop / paint shop / car park) -> DriveThruManager; killzone
    // (6) -> a KillzoneAction onto the game-action queue; jump (7) -> StuntManager's pending jump;
    // road-limit (11) -> RoadRulesManager::OnRoadLimit.
    void ProcessPlayerTriggers(bool                                                              lbFirstFrame,
                               const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                               const BrnTrigger::TriggerRegion*                                  lpTriggerRegion,
                               GameStateModuleIO::OutputBuffer*                                  lpOutput,
                               StuntManager*                                                     lpStuntManager,
                               DriveThruManager*                                                 lpDriveThruManager,
                               const BrnResource::VehicleList*                                   lpVehicleList);

    // ---- previously-committed functions of this class (kept) ----

    // X360 0x82326538. Drain every armed landmark index back out as a "remove trigger" event onto
    // the trigger-entity module's remove queue, then empty the landmark array.
    void ClearLandmarkIndexesForGameMode(
        CgsModule::EventQueue<BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent, 256>& lrRemoveTriggerQueue);

    // X360 0x823265E8. Arm one landmark index for the active mode if it is not already present.
    bool AddLandmarkIndexForGameMode(LandmarkIndex lLandmarkIndex);

    // X360 0x82355D78. Return the trigger id of the traffic-light region the player is currently in.
    LightTriggerId GetPlayerCurrentTrafficLightId() const;

    // Declared-only here (body elsewhere in the full TU). Reads mbPlayerInTrafficLightRegion.
    bool IsPlayerInTrafficLightRegion() const;

private:

    // ---- leading preamble (offsets 0..911) not yet reconstructed; reserved to place the first
    //      touched member (maActiveTriggers @912) at its X360 byte offset. The real members here
    //      per DWARF are: mDebugComponent, 3 * CgsID group ids, maSoundActions (Array<SoundTrigger-
    //      Action,16>). Grow into real members when the full class lands.
    u8 mauReserved_Head[912];                        // this+0 .. this+911

    // this+912. The per-frame active-trigger set (region indexes currently armed in the world
    // TriggerEntityModule). DWARF BrnTriggerQueryManager.h:220 (Array<uint16_t,256u>). Element
    // store this+912..this+1423 (256 * 2B), live-count word this+1424 (== *(this+1424) in X360).
    Array<u16, 256> maActiveTriggers;                // this+912 (elements), count @ this+1424

    // this+1428 .. this+1567: maLastPlayerTriggers (Array<u16,32>), maLastFrameTriggers
    // (Array<u16,32>), meTriggerLoadStage (enum) — not touched by these three funcs; reserved so
    // mpTriggerData lands at this+1568.
    u8 mauReserved_AfterActive[1568 - 1428];         // this+1428 .. this+1567

    // this+1568. ResourcePtr onto the loaded track TriggerData resource (DWARF h:226). operator->
    // dereferences its leading main-memory pointer (this+1568); a NULL pointer fires the X360
    // "Can not instance resource pointer - it has no main memory resource" assert.
    CgsResource::ResourcePtr<BrnTrigger::TriggerData> mpTriggerData;   // this+1568 (0x1C bytes)

    // this+1596 .. this+1775: mpTrafficData (ResourcePtr), maActiveRaceCarPosLastFrame[8]
    // (Vector3[8]), mPlayerLookAheadPos (Vector3) — not touched by these three funcs; reserved so
    // mLastPlayerPosition lands at this+1776.
    u8 mauReserved_AfterTriggerData[1776 - (1568 + 28)]; // this+1596 .. this+1775

    // this+1776. Cached player world position from the last trigger refresh (Vector3, 16B SIMD).
    // UpdateTriggers compares the current player position against this (squared distance >
    // KF_TRIGGER_REFRESH_DISTANCE^2 == 900.0) to decide whether to rebuild the active-trigger set,
    // and overwrites it after a rebuild. (DWARF h:231 mLastPlayerPosition.)
    Vector3 mLastPlayerPosition;                      // this+1776 (16B)

    // this+1792 .. this+1807: mLastPlayerPosition2D (Vector2) — not touched; reserved.
    u8 mauReserved_AfterLastPos[1808 - (1776 + 16)];  // this+1792 .. this+1807

    // this+1808 (byte). "Triggers updated" dirty flag (DWARF h:234). UpdateTriggers gates the
    // active-set rebuild on it (only rebuilds the first frame it is clear) and sets it true at the
    // end; AddLandmarkIndexForGameMode clears it when a new landmark index is armed.
    bool mbTriggersUpdated;                          // this+1808 (DWARF h:234)

    // this+1809 (byte). The flag IsPlayerInTrafficLightRegion() returns.
    bool mbPlayerInTrafficLightRegion;               // this+1809 (DWARF h:236)

    u8   mau8Pad_1810[2];                            // this+1810 .. this+1811 (align to u32)

    // this+1812. The traffic-light trigger handle handed back by GetPlayerCurrentTrafficLightId.
    // Construct stores -1 (LightTriggerId::SetInvalid). (DWARF h:237.)
    LightTriggerId mPlayerCurrentTrafficLightId;     // this+1812

    // this+1816/1820/1824. The three injected gameplay managers (DWARF h:240/241/242). Construct
    // stores its three ctor args here; UpdateTriggers/ProcessPlayerTriggers read mpRoadRulesManager.
    BrnProgression::ProgressionManager* mpProgressionManager;   // this+1816
    TakedownManager*                    mpTakedownManager;      // this+1820
    RoadRulesManager*                   mpRoadRulesManager;     // this+1824

    u8 mauReserved_AfterManagers[1832 - (1824 + 4)]; // this+1828 .. this+1831 (mpModeManager region; reserved)

    // this+1832. The landmark/region indexes armed for the active game mode (DWARF h:245). Element
    // store this+1832..this+1863 (16 * 2B), live-count word this+1864. DWARF spells it
    // mLandmarkIndexArray (single leading m).
    LandmarkIndexArray mLandmarkIndexArray;          // this+1832 (elements), count @ this+1864

    // this+1868/1869 (bytes). Construct sets both to 1 (DWARF h:246/247).
    bool mbDoSoundLookAheadThisFrame;                // this+1868
    bool mbCarHasTeleported;                          // this+1869

    // Compile-time offset guards (private members -> assert from a member-fn context).
    static void _AssertLayout();
};
}
