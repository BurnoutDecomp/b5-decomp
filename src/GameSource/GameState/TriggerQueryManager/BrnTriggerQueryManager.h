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
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"                // CgsModule::EventReceiverQueue<3072,16> (Prepare's reply queue)
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"                  // CgsResource::ResourcePtr<T>
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::InRemoveTriggerEvent

// Foreign types this dispatcher routes hits into. The three target functions only call named
// methods on these / store pointers to them, so forward declarations + the declared-only methods
// in their committed homes (grown additively) suffice; their full layouts are not needed here.
namespace BrnTrigger      { struct TriggerData; struct TriggerRegion; struct GenericRegion; }
namespace BrnTraffic      { struct TrafficData; }
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

    // DWARF BrnTriggerQueryManager.h:184. The trigger/traffic resource-load state machine
    // Prepare walks (the X360 stage word is this+0x61C == +1564). Enumerator names + values
    // are DWARF-authoritative; the X360 switch has exactly these seven cases.
    enum ETriggerLoadStage
    {
        E_TRIGGER_LOAD_NOT_STARTED    = 0,
        E_TRIGGER_LOAD_REQUESTED      = 1,
        E_TRIGGER_ACQUIRE_NOT_STARTED = 2,
        E_TRIGGER_ACQUIRE_REQUESTED   = 3,
        E_TRIGGER_LOAD_TRAFFIC        = 4,
        E_TRIGGER_WFLOAD_TRAFFIC      = 5,
        E_TRIGGER_LOAD_DONE           = 6,
    };

    // ---- functions this TU owns (bodies in BrnTriggerQueryManager.cpp) ----

    // ⭐ X360 0x82398218 (body: BrnTriggerQueryManager_Prepare.cpp). THE TRIGGERS.DAT LOADER.
    // Resumable: LoadBundle("Triggers.dat") into pool 5 -> acquire "TriggerData" -> bind
    // mpTriggerData -> LoadTrafficLanes -> bind mpTrafficData -> done. Returns true only at
    // E_TRIGGER_LOAD_DONE, so the caller (GameStateModule::Prepare stage 3) pumps it.
    // DWARF signature (BrnTriggerQueryManager.h:161):
    //   bool Prepare(GameStateModuleIO::OutputBuffer*, CgsModule::EventReceiverQueue<3072,16>*)
    bool Prepare(GameStateModuleIO::OutputBuffer* lpOutput,
                 CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue);

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

    // ---- ADDITIVE GROW (declare-only; bodies in the TriggerQueryManager TU) ----
    // Read accessors the StuntManager TU calls on mpTriggerQueryManager to read the track TriggerData
    // and walk the per-frame active-trigger set. X360-attested (the BrnStuntManager.cpp DWARF hints
    // name TriggerQueryManager::GetTriggerData / GetActiveTriggerCount / GetActiveTrigger).
    //   GetTriggerData()       -> mpTriggerData's main-memory pointer (X360 ResourcePtr<TriggerData>::
    //                             GetMemoryResource on this+1568).
    //   GetActiveTriggerCount()-> maActiveTriggers.GetLength() (X360 reads the live-count word
    //                             this+1424, firing the CgsArray 'Array used before Construct/Clear
    //                             was called' assert via the Array<u16,256> accessor).
    //   GetActiveTrigger(i)    -> maActiveTriggers.GetItem(i) (X360 short_256::GetItem on this+912).
    // DEFINED INLINE: the X360 emits no symbol -- every caller renders as the ResourcePtr's own
    // main-memory-pointer load off this+1568. (CarSelectManager::SetupSpawnLocations reaches the
    // track's spawn-location table through it.)
    const BrnTrigger::TriggerData* GetTriggerData() const { return mpTriggerData.operator->(); }
    u32                            GetActiveTriggerCount() const;
    u16                            GetActiveTrigger(u32 liIndex) const;

private:

    // ---- leading preamble (offsets 0..911), grown in place 2026-08-01 -----------------------
    // this+0 .. this+351: mDebugComponent (BrnGameState::TriggerQueryManagerDebugComponent,
    // DWARF h:211). Still reserved -- the component's own header is a separate slice and
    // Prepare's Construct/Register of it is a documented deferral (see the Prepare body).
    u8 mauReserved_DebugComponent[352];              // this+0 .. this+351
    // this+352/360/368 (DWARF h:212/213/214). The three player trigger-GROUP ids.
    // TriggerQueryManager::Prepare zeroes all three with three 8-byte stores
    // (`std r28, 0x160/0x168/0x170(r31)` @0x823983D4/D8/E4) right after it binds the trigger
    // data -- CgsID is a u64, so three consecutive CgsIDs is exactly what those stores are.
    CgsID mPlayerSigTakedownGroupID;                 // this+352 (0x160)
    CgsID mPlayerSuperJumpGroupID;                   // this+360 (0x168)
    CgsID mPlayerRoadLimitGroupID;                   // this+368 (0x170)
    // this+376 .. this+911: maSoundActions (Array<GameStateModuleIO::SoundTriggerAction,16>,
    // DWARF h:217) -- not touched by any committed body; reserved so maActiveTriggers still
    // lands at this+912.
    u8 mauReserved_SoundActions[912 - 376];          // this+376 .. this+911

    // this+912. The per-frame active-trigger set (region indexes currently armed in the world
    // TriggerEntityModule). DWARF BrnTriggerQueryManager.h:220 (Array<uint16_t,256u>). Element
    // store this+912..this+1423 (256 * 2B), live-count word this+1424 (== *(this+1424) in X360).
    Array<u16, 256> maActiveTriggers;                // this+912 (elements), count @ this+1424

    // this+1428 / this+1496 (DWARF h:221/222). The previous-frame trigger sets. Not touched by
    // any committed body yet; declared as their real types so meTriggerLoadStage below is a real
    // member. Array<u16,32> is 32*2 + a 4-byte count == 68 bytes, pointer-free, so the pair is
    // exactly the console's 136 bytes on the 64-bit gate too.
    Array<u16, 32> maLastPlayerTriggers;             // this+1428 (elements), count @ this+1492
    Array<u16, 32> maLastFrameTriggers;              // this+1496 (elements), count @ this+1560

    // this+1564 (0x61C) (DWARF h:225). ⭐ The resource-load stage word Prepare switches on --
    // the X360 reads/writes `*(this + 0x61C)` at every case. Grown from the reserved blob
    // 2026-08-01 with Prepare.
    // ⚠️ FLAG (PC bring-up): the console clears it in GameStateModule::ClearData (Prepare
    // stage 0), which is not reconstructed; the in-class initialiser below stands in for that
    // one store. GameStateModule is embedded BY VALUE in BrnGameModule and is NOT in its ctor
    // init list, so without it this word is garbage. DELETE-WHEN ClearData lands.
    ETriggerLoadStage meTriggerLoadStage = E_TRIGGER_LOAD_NOT_STARTED;   // this+1564

    // this+1568. ResourcePtr onto the loaded track TriggerData resource (DWARF h:226). operator->
    // dereferences its leading main-memory pointer (this+1568); a NULL pointer fires the X360
    // "Can not instance resource pointer - it has no main memory resource" assert.
    CgsResource::ResourcePtr<BrnTrigger::TriggerData> mpTriggerData;   // this+1568 (0x1C bytes)

    // this+1600 (0x640) (DWARF h:227). The loaded traffic-lane resource. Prepare's last stage
    // binds it (`addi r3, r31, 0x640; bl BaseResourcePtr::CreateFromHandle` @0x823984A4).
    // ⚠️ The X360 gap between mpTriggerData (+1568) and this (+1600) is 32, not the 28 bytes
    // BaseResourcePtr's six 32-bit fields add up to -- so the console's ResourcePtr is 32 bytes
    // wide, not 28. Immaterial on the 64-bit gate (both are 56 there and every absolute offset
    // below this point is already #if-guarded to the 32-bit build), but the committed
    // "0x1C bytes" annotation on mpTriggerData above is WRONG by 4 and is corrected here.
    CgsResource::ResourcePtr<BrnTraffic::TrafficData> mpTrafficData;   // this+1600

    // this+1632 .. this+1775: maActiveRaceCarPosLastFrame[8] (Vector3[8], DWARF h:229) and
    // mPlayerLookAheadPos (Vector3, h:230) — not touched by any committed body; reserved so
    // mLastPlayerPosition lands at this+1776 on the console.
    u8 mauReserved_AfterTrafficData[1776 - 1632];    // this+1632 .. this+1775

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

#include "GameSource/BurnoutConstants.h"  // global ::EGlobalRaceCarIndex / ::EActiveRaceCarIndex
#include <cstddef>                        // offsetof (PackedIndex layout pins)

// PackedIndex - the small value type that packs a global-race-car slot (+0) and an active-race-car
// slot (+4) into two 32-bit enum words. DWARF: BrnTriggerQueryManager.h:395 (struct PackedIndex),
// members meGlobalRaceCarIndex @+0 (h:418), meActiveRaceCarIndex @+4 (h:419). DWARF declares this
// struct + its methods at GLOBAL scope (bare 'struct PackedIndex', bare 'void PackedIndex::Set...'
// in BrnGameStateUnity.cpp:10350/10359) -- NOT inside namespace BrnGameState. It is found by
// unqualified lookup from within BrnGameState::TriggerQueryManager::SubmitTriggerQueries. Only the
// two Set* accessors of this batch have reconstructed X360 bodies (0x82355DE8 / 0x82355EC8); the
// remaining DWARF-declared members (SetPackedData / GetPackedRaceCarIndex / GetGlobalRaceCarIndex /
// GetActiveRaceCarIndex) are declare-only here and grow additively when their bodies land.
struct PackedIndex
{
public:
    // X360 0x82355DE8. Bounds-check + byte-fit assert, then store (value & 0xff) to meGlobalRaceCarIndex.
    void SetGlobalRaceCarIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex);

    // X360 0x82355EC8. Bounds-check + byte-fit assert, then store (value & 0xff) to meActiveRaceCarIndex.
    void SetActiveRaceCarIndex(EActiveRaceCarIndex leActiveRaceCarIndex);

    // ---- declare-only (DWARF BrnTriggerQueryManager.h:399/408/411/414; bodies land later) ----
    void                SetPackedData(s32 liPackedData);
    s32                 GetPackedRaceCarIndex() const;
    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const;
    EActiveRaceCarIndex GetActiveRaceCarIndex() const;

private:
    // this+0 (DWARF h:418). Global race-car slot (0..34 / INVALID); SetGlobalRaceCarIndex stores here.
    EGlobalRaceCarIndex meGlobalRaceCarIndex;   // this+0
    // this+4 (DWARF h:419). Active race-car slot (0..7 / INVALID); SetActiveRaceCarIndex stores here.
    EActiveRaceCarIndex meActiveRaceCarIndex;   // this+4

    // Never-called layout pin so any drift in a member offset is a compile error.
    friend void _PackedIndex_AssertLayout();
};

// Pointer-free sub-struct: X360 32-bit offsets hold on the 64-bit gate (both members are 4-byte enums).
inline void _PackedIndex_AssertLayout()
{
    static_assert(offsetof(PackedIndex, meGlobalRaceCarIndex) == 0, "meGlobalRaceCarIndex @ +0");
    static_assert(offsetof(PackedIndex, meActiveRaceCarIndex) == 4, "meActiveRaceCarIndex @ +4");
    static_assert(sizeof(PackedIndex) == 8, "PackedIndex is two 4-byte enum words");
}
