#pragma once

// =============================================================================
// BrnTrafficEntityModule.h -- owning header for BrnTraffic::TrafficEntityModule and the
// small per-frame record types it keeps in fixed-capacity containers.
//
// Member order is the DecFIGS DWARF's (each member carries its `:NNN` source line),
// confirmed against the X360 asm at three anchors: :602-:613 land on ten consecutive
// offsets from +0x2F0; the eleven bools :716-:726 on +0x717DD..+0x717E7; and
// maVehicleTypeRuntime (:940) at +0x76380, where 96*128 ends exactly at :941/:943/:944.
//
// HOST-NATIVE LAYOUT: nothing strides or offsets by an X360 byte, every access is by member
// name, and the _AssertLayout pins cover relative order plus asm-attested array counts only.
// Ship pool sizes differ from the DWARF (KU_MAX_STATIC_TRAFFIC 199, KU_MAX_TOTAL_TRAFFIC
// 600); see BrnTrafficConstants.h. One member is still absent, marked `[MEMBER HOLE 5]` at
// its ordered position with the exact blocker (holes 1, 2, 4 and 6 closed).
// =============================================================================

#include "types.hpp"        // u8/u16/u32/s8/s32/u64/f32
#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT (UpdateVehicleStuckSideTime inline)
#include "BrnCommonTypes.h" // EntityId, Vector2/3/4, Vector3Plus, VecFloat, Matrix44Affine
#include "SharedClasses/BrnSharedConstants.h"   // BrnUpdateSet
#include "GameSource/BurnoutConstants.h"        // EActiveRaceCarIndex

#include "GameShared/GameClasses/Graphics/CgsModel.h"                  // Model::State (VehicleRenderInfo::mLOD)
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"     // base class (DWARF :400)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"   // EventReceiverQueue<4096,16>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // EventQueue<T,N>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // ResourcePtr<T>
#include "GameShared/GameClasses/Containers/CgsArray.h"                // ::Array<T,N>
#include "GameShared/GameClasses/Containers/CgsSet.h"                  // ::Set<T,N>
#include "GameShared/GameClasses/Containers/CgsStack.h"                // CgsContainers::Stack<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"             // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"         // CgsContainers::FastBitArray<N>
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                  // CgsNumeric::Random
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // PerfMonCpuMonitorData

#include "SharedClasses/Traffic/BrnTrafficSharedConstants.h"           // KU_MAX_VEHICLE_TYPES, ...
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"          // Vehicle, VehicleAxles, VehicleSoaData
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"            // Param, ParamSoaData, ParamTransform, ParamListNode, ParamNeedToSlowData
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficRaceCarCache.h"     // RaceCarStateData
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficPhysicalVehicleInfo.h" // PhysicalVehicleInfo
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"      // StaticTrafficParam
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficMiscRuntimeClasses.h" // BrnTraffic::PhysicalReason
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"   // ETrafficType, eCrashTrafficType (promotion)
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"      // HullRuntime
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"     // TrafficLightManager
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.h"      // TrafficCarStreamer
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h"   // TweakValues
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h" // FuzzyBehaviourLogic
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // NearMiss collections
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"       // BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR

// BrnBlobbyShadowManager::BrnBlobbyShadowBuffer is a nested class, so the render trio below
// cannot forward-declare it. Cheap include, and it reaches back to nothing here.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"  // BrnBlobbyShadowManager::BrnBlobbyShadowBuffer
#include "GameSource/Director/Camera/Camera.h"      // BrnDirector::Camera::Camera (mCameraLastFrame, DWARF :881)

namespace CgsModule { struct IOBufferStack; }
namespace BrnPhysics { namespace Deformation { class StreamedDeformationSpec;
                                               struct DeformationOutputInterfaceForEntityModules; } }
#include "GameSource/Physics/DeformationManager/BrnDeformationConstants.h" // KU_MAX_DETACHED_PARTS_PER_VEHICLE
// Wave-T3 promotion collaborators, pointer-only. Class keys match their single homes:
// BrnVehicleDriverControls.h:289 spells BrnTrafficDriverControls struct, and
// BrnVehicleInputInterface.h:29 spells VehicleInputInterface struct (the alignas belongs to
// the definition). MSVC mangles struct vs class, so these keys are load-bearing.
namespace BrnPhysics { namespace Vehicle { struct BrnTrafficDriverControls;
                                           struct VehicleInputInterface; } }
// Pointer-only uses in the render declarations (forward-declaration exception (b)):
// including CgsDispatcher.h / BrnShadowMap.h here would pull the renderer and the
// shadow-cascade tail into every includer, and BrnWorldModule.h includes this header.
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }
namespace BrnResource { struct VehicleList; }   // mpVehicleList (:943) -- by pointer only
// CacheRaceCarState's parameter, by pointer only. Real home
// GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h;
// including it here would pull the race-car IO graph into BrnWorldModule.h.
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }
// HandlePrepareForModeAction's action record, by pointer only (forward-declaration exception
// (b)). Real home GameSource/GameState/BrnGameActions.h; including it here would pull the whole
// game-action graph -- GameModeParams, the drive-thru manager, the progression trophy data --
// into BrnWorldModule.h through this header. Class key `struct` matches that home, and MSVC
// mangles struct vs class, so it is load-bearing.
namespace BrnGameState { namespace GameStateModuleIO { struct PrepareForModeAction; } }

namespace BrnTraffic
{
    // Pointer-only in the driving-traffic declarations below; real home
    // SharedClasses/Traffic/BrnTrafficSection.h.
    struct Section;

    class TrafficData;
    // The DWARF spells mpVehicleList's type unqualified (:943), but it is
    // BrnResource::VehicleList (home SharedClasses/DataLists/VehicleList.h), not a
    // BrnTraffic type: FindVehicleTypeAttribKey_EXPENSIVE @0x8273F0B8 calls
    // BrnResource::VehicleList::GetVehicleIndex / ::GetVehicleData on it, and no
    // BrnTraffic::VehicleList exists in the image.
    class DebugComponent;
    class Logger;

namespace BrnTrafficIO { class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_Dispatch; class InputBuffer_PreDispatch; class OutputBuffer_PreDispatch; }

namespace BrnTrafficIO { class OutputBuffer_Prepare; }

    // @0x82714848 (BrnTraffic::SetGlassFractureConstants) -- the traffic module's own copy of
    // BrnWorld::SetGlassFractureConstants @0x822BD280 (same body, not ICF-folded: the linker
    // kept both). Publishes the three glass-fracture shader constants 30/31/32. Sole caller:
    // TrafficEntityModule::RenderTrafficCar @0x82728B08. Body in
    // BrnTrafficEntityModule_ProcessDeformationData.cpp. Signature is the DWARF's for the
    // BrnWorld twin (BrnRaceCarEntityModule.cpp:4): two f32s in f1/f2, the Vector2 by const
    // reference in r3, the Vector4 by value in v1.
    void SetGlassFractureConstants(f32 lfFractureStrength,
                                   f32 lfEqualisationFactor,
                                   const Vector2& lvUVScale,
                                   Vector4 lvUVOffsets);
// ADDITIVE (world-drive wave: WorldModule::EntityModulePreSceneUpdate @0x827BD1F0
// and EntityModulePostPhysicsUpdate @0x827D3F10 name these IO buffers).
namespace BrnTrafficIO { class InputBuffer_PreScene; class OutputBuffer_PreScene;
                         class InputBuffer_PostPhysics; class OutputBuffer_PostPhysics; }

    // The module's small record types. The DWARF homes every one of them in this header;
    // the `:NNN` on each is its source line there.

    // :105 -- where a traffic generator lives (a hull + a section on that hull).
    struct GeneratorAddress
    {
        u16 muHull;    // :107
        u8  muSection; // :108
    };

    // :121 -- one pending traffic-crash record. sizeof == 16 (X360: Array<TrafficCrashInfo,160>
    // count word at +0xA00 == 160*16, 16-byte per-element stride); pointer-free, so 16 here too.
    // muCrashTrafficType is BrnPhysics::Vehicle::eCrashTrafficType
    // (BrnTrafficPhysicsConstants.h:32), stored as its underlying 32-bit value because that
    // enum's home is not reconstructed. Enumerators: Standard=0, Checked=1, Spontaneous=2,
    // Slammed=3, Invalid=255.
    struct TrafficCrashInfo
    {
        EntityId mVictimId;                   // :123
        EntityId mCauserId;                   // :124
        u32      muCrashTrafficType;          // :125  (eCrashTrafficType, 4-byte)
        bool     mbNeedsToBeSentToCrashModule;// :126
    };

    // :138 -- one detached body part the physics side wants drawn this frame.
    struct DetachedPartRenderEvent
    {
        s32            miPartIndex; // :140
        Matrix44Affine mTransform;  // :141
    };

    // :239 -- a kill-zone the module fired and must remember for a few frames. sizeof == 16
    // (X360: Array<FiredKillZoneInfo,8> count word at +0x80 == 8*16, 16-byte stride).
    // mKillZoneId is TrafficData::KillZoneId == uint64_t (BrnTrafficData.h:40).
    struct FiredKillZoneInfo
    {
        u64 mKillZoneId;            // :241
        s32 miFramesLeftToRemember; // :242
    };

    // :255 -- the per-race-car snapshot of "which traffic entities are near this car" that
    // the AI module reads. X360-attested: Prepare @0x8274A578 stage 4 walks eight of these
    // with a 136-byte stride from `this + 0x79388`, writing meRaceCarIndex at +0 and
    // miNumTrafficIDs at +4; 4 + 4 + 32*4 == 136 attests both
    // BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR == 32 and a pointer-free record.
    // The extent constant's home is SharedIO/BrnTrafficAIInterfaces.h; do not copy it here.
    struct StoredAITrafficData
    {
        EActiveRaceCarIndex meRaceCarIndex;                                 // :257
        s32                 miNumTrafficIDs;                                // :258
        EntityId            maTrafficEntityIDs[BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR]; // :259
    };

    // DWARF home BrnTrafficMiscRuntimeClasses.h:94 -- one purgatory-list record: a vehicle
    // index plus a countdown of decision frames left. sizeof == 4 (X360:
    // Array<PurgatoryInfo,1>::Append @0x8270AAC0 stores muIndex then muDecisionFramesLeft as
    // halfwords, count word @+0x4 == 1*4; Array<PurgatoryInfo,400> count word @+0x640 == 400*4).
    // Homed here with the module's other small records; move it, never redefine it, when the
    // BrnTrafficMiscRuntimeClasses slice lands.
    struct PurgatoryInfo
    {
        u16 muIndex;              // :96
        u16 muDecisionFramesLeft; // :97
    };

    // DWARF home BrnTrafficVehicle.h:159 -- one per-frame vehicle-render record (the dispatch
    // list the module hands the renderer). sizeof == 12 (X360:
    // Array<VehicleRenderInfo,64>::Append @0x8270A148 copies three dwords at a 12-byte stride,
    // count word @+0x300 == 64*12). mLOD is CgsGraphics::Model::State, a 4-byte enum.
    //
    // DO NOT REORDER: pinned by static_asserts in BrnWorldModule.cpp (mfDistanceSq @4,
    // mLOD @8) and in BrnActiveRaceCarRenderParams.cpp.
    struct VehicleRenderInfo
    {
        u32                      muEntityIndex; // :161
        f32                      mfDistanceSq;  // :162
        CgsGraphics::Model::State mLOD;         // :163
    };

    // :164 -- one predicted race-car-hull change: a pending collision-shape swap to apply to a
    // nearby race car (producers AddPredictedHullChange / UpdateRaceCarHulls, dumped by
    // DEBUGDumpHullPredictions). sizeof == 8 (X360: Array<HullChangeInfo,400>::Append
    // @0x8270ACA8 stores two dwords, count word @+0xC80 == 400*8), which is exactly the
    // 4+2+2 the three fields below occupy.
    struct HullChangeInfo
    {
        EActiveRaceCarIndex meActiveRaceCarIndex; // leak :167
        u16                 muNewActiveHull;      // leak :168
        u16                 muUpdateFrame;        // leak :169
    };

    // :272 -- one "crashing thing" nearby traffic should react to this frame (producers and
    // consumers: UpdateParams_BuildListOfCrashingThings / _TryStartSympatheticCrashing /
    // _TryAvoidCrashing, all on Array<CrashingThingData,168>). sizeof == 32 (X360:
    // Array<CrashingThingData,168>::operator[] @0x8270BE68 returns 32*index + base, count word
    // @+0x1500 == 168*32) -- the 16-aligned Vector3, the EntityId and the bool, rounded up.
    struct CrashingThingData
    {
        Vector3  mPosition;            // :275
        EntityId mEntityId;            // :276
        bool     mbShowtimeCrashMagnet;// :278
    };

    // NB: BrnTraffic::PhysicalVehicleInfo (the Array<PhysicalVehicleInfo,33> element) is NOT
    // homed here -- it has its own committed home BrnTrafficPhysicalVehicleInfo.h; do not
    // redefine it (ODR).

    // :293 -- a structure-of-arrays packet holding four collidable vehicles at once (the "4" in
    // the name), cached in mCachedCollidableList (Array<CollidableVehicleInfo4,16>, so up to 64
    // vehicles == KU_MAX_COLLIDABLE_CACHED_TRAFFIC). Each Vector4 lane holds one field for all
    // four vehicles. sizeof == 128 (X360: Array<CollidableVehicleInfo4,16>::operator[]
    // @0x8270D260 returns (index<<7) + base, count word @+0x800 == 16*128) == eight Vector4s.
    struct CollidableVehicleInfo4
    {
        Vector4 mPosition_X;       // :295
        Vector4 mPosition_Y;       // :296
        Vector4 mPosition_Z;       // :297
        Vector4 mLinearVelocity_X; // :298
        Vector4 mLinearVelocity_Y; // :299
        Vector4 mLinearVelocity_Z; // :300
        Vector4 mHalfLengths;      // :302
        Vector4 mHalfWidths;       // :303
    };

    // :311 -- the per-vehicle avoidance debug snapshot (allocated on demand, pointed at by
    // mpaDEBUGVehicleAvoidance). Debug-only; never read by shipped logic.
    struct DEBUG_VehicleAvoidance
    {
        Vector3 mPosition;         // :313
        Vector3 mCurrentDirection; // :314
        Vector3 mAvoidDirection;   // :315
        Vector3 mTarget;           // :316
        Vector3 maFeelers[KI_TRAFFIC_AVOIDANCE_FEELERS];    // :318
        f32     maFeelerScore[KI_TRAFFIC_AVOIDANCE_FEELERS];// :319
        s32     miBestFeeler;      // :320
        f32     mfOverallRisk;     // :321
        f32     maPassScore[KI_TRAFFIC_AVOIDANCE_FEELERS];  // :323
    };

    // :350 -- the per-vehicle fuzzy-logic debug snapshot. sizeof == 64 (X360: Prepare
    // @0x8274A578 stage 4 allocates 2560 bytes for KU_DEBUG_MAX_FUZZY_LOGIC == 40 of them),
    // which is what u32 + 16-aligned Vector3 + f32[6] comes to.
    struct DEBUG_VehicleFuzzyLogic
    {
        u32     muVehicle;              // :352
        Vector3 mRenderPosition;        // :353
        f32     mafParamOutputScores[6];// :354
    };

    // :367 -- one showtime-eligible traffic vehicle (the showtime "crash magnet" list).
    struct ShowtimeVehicleInfo
    {
        u32 muVehicleIndex; // :378
        u8  muFlags;        // :379
    };

    // :156 -- the per-PHYSICAL-traffic-vehicle scratch record: everything the deformation /
    // render side needs for a traffic car that has been promoted to a real physics body.
    // One of these per KU_MAX_PHYSICAL_TRAFFIC_VEHICLES slot (maTrafficPhysicsInfoList).
    //
    // :153/:169 mDetachedPartQueue -- ARTIST DIVERGES FROM THE DWARF HERE (2026-09-02,
    // traffic-deformation wave). DecFIGS spells it `EventQueue<DetachedPartRenderEvent,20>`
    // (an 80-byte-stride event array + length), but every ARTIST access to the span
    // +0x000..+0x520 is the compact per-vehicle record below, and three independent X360
    // sites agree on its layout:
    //   * the writer, ProcessDeformationData @0x8271EA94..0x8271EB10: `lbz r29, 0(info)` as
    //     the slot (asserted < KU_MAX_DETACHED_PARTS_PER_VEHICLE == 20), `stbx partIndex` at
    //     info+1+slot, the event's four transform rows at info+32+64*slot, `stb ++count, 0(info)`;
    //     and its per-frame reset `stb 0, 0(info)` over all 25 records @0x8271E984;
    //   * TrafficPhysicsInfo::Construct @0x82751EB0 `stb 0, 0(r3)` -- the count, not the top
    //     byte of a 32-bit mpEvents as an earlier wave read it;
    //   * the reader, RenderTrafficCar pseudocode :1319-:1345: `if (*info)` count,
    //     `info[1 + n]` part index, `info + 32 + 64*n` the transform.
    // The DWARF type would be 1612 bytes; the span the ARTIST stride leaves it is 1312, which
    // is exactly 32 + 20 * 64. mvRoadTestNormal_HeightAboveRoad follows at +0x520 on both.
    // The record's ARTIST name is not recovered (no symbol touches it); the DWARF typedef
    // name is kept for the member's sake.
    struct TrafficPhysicsInfo
    {
        struct DetachedPartRenderQueue
        {
            u8             muNumParts;                                                            // +0x00
            u8             mau8PartIndex[BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE]; // +0x01
            // 11 bytes of alignment to the first 16-byte row (Matrix44Affine is 16-aligned).
            Matrix44Affine maTransforms[BrnPhysics::Deformation::KU_MAX_DETACHED_PARTS_PER_VEHICLE]; // +0x20
        };

        // :173 -- KU_MAX_VERLET_POINTS worth of skinning scratch (the 128-point Verlet skin).
        static const u32 KU_NUM_SKINNING_OFFSETS = 128;
        static const u32 KU_NUM_WHEELS           = 4;
        static const u32 KU_MAX_LIGHT_LOCATORS   = 24;
        static const u32 KU_NUM_GLASS_PANES      = 8;

        DetachedPartRenderQueue mDetachedPartQueue;                       // :169
        Vector3Plus mvRoadTestNormal_HeightAboveRoad;                     // :171
        Vector3Plus maSkinningOffsets_Scratch[KU_NUM_SKINNING_OFFSETS];   // :173
        Matrix44Affine maWheelTransforms[KU_NUM_WHEELS];                  // :175
        Vector3 maLightLocatorPositions[KU_MAX_LIGHT_LOCATORS];           // :178
        // :179 -- BrnPhysics::Deformation::ETagPointType[24], stored as the enum's underlying
        // 32-bit value so this header stays out of the deformation include graph.
        // FLAG: retype to the real enum when a consumer needs it; the width does not change.
        s32 maLightTagPointTypes[KU_MAX_LIGHT_LOCATORS];                  // :179
        bool mabWheelExists[KU_NUM_WHEELS];                               // :181
        f32 mfStuckTimeFront;                                             // :183
        f32 mfStuckTimeBack;                                              // :184
        f32 mfStuckTimerDebounce;                                         // :185
        f32 mfTimeNotDriving;                                             // :187
        f32 mfSteeringDirection;                                          // :190
        f32 mfDrivingDirection;                                           // :191
        s8  miNumLightLocators;                                           // :193
        bool mbIsDeforming;                                               // :194
        bool mbIsFatallyCrashing;                                         // :195
        u8  mu8RenderDamageFlags;                                         // :198
        f32 mafGlassPaneFractureAmounts[KU_NUM_GLASS_PANES];              // :202
        u8  muContactSideFlags;                                           // :204

        // SHIP-ONLY TAIL. The DWARF's TrafficPhysicsInfo ends at muContactSideFlags :204;
        // these two are a merge-window addition ARTIST has and DecFIGS does not, so they carry
        // an X360 attestation instead of a DWARF line: Construct @0x82751E88 closes with
        // `sth r4, 0x100A(r3)`, storing its s32 liIndex argument as a halfword, and Destruct
        // @0x82751EE8 is `li r11,-1 ; sth r11, 0x100A(r3)` and nothing else, which makes 0xFFFF
        // the sentinel and u16 the width. The reader is HandleExternalResponses @0x82732C68.
        // muPad205 is the console's alignment byte, modelled explicitly so the source pins the
        // order of the two ship members rather than a host padding rule.
        u8  muPad205;                 // X360 +0x1009 (alignment; no DWARF)
        u16 muOwningVehicleIndex;     // X360 +0x100A -- SHIP-ONLY, no DWARF

        // The "no owner" sentinel Destruct writes (`li r11,-1` truncated to the halfword).
        static const u16 KU16_NO_OWNING_VEHICLE = 0xFFFFu;

        void Construct(s32 liIndex); // :214
        void Destruct();             // :218
        bool IsStuckFront() const;   // :221
        bool IsStuckBack() const;    // :224
    };

    // BrnTraffic::TrafficEntityModule (DWARF :400). The base class is DWARF-attested and
    // asm-attested: Prepare @0x8274A578 stage 1 calls
    // CgsModule::ModuleSingleBuffered::Prepare(a1) with `this` unadjusted, so the base
    // sub-object sits at offset 0.
    class TrafficEntityModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // --- DWARF-attested nested enums (:486-:559) ------------------------------------

        // :486
        enum EPrepareStage
        {
            E_PREPARESTAGE_START       = 0,
            E_PREPARESTAGE_MANAGER     = 1,
            E_PREPARESTAGE_LOADINGWORLD= 2,
            E_PREPARESTAGE_VOLUMES     = 3,
            E_PREPARESTAGE_DEBUG       = 4,
            E_PREPARESTAGE_DONE        = 5,
        };

        // :496
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // :503
        enum EResourceAcquireStage
        {
            E_RESOURCE_LOAD_BASEDATA_NOT_STARTED = 0,
            E_RESOURCE_LOAD_BASEDATA_REQUESTED   = 1,
            E_RESOURCE_LOAD_VEHICLELISTAQUIRE    = 2,
            E_RESOURCE_LOAD_WFVEHICLELISTAQUIRE  = 3,
            E_RESOURCE_LOAD_VEHICLES             = 4,
            E_RESOURCE_WFLOAD_VEHICLES           = 5,
            E_RESOURCE_LOAD_PHYSICS              = 6,
            E_RESOURCE_WFLOAD_PHYSICS            = 7,
            E_RESOURCE_LOAD_ATTRIBS              = 8,
            E_RESOURCE_WFLOAD_ATTRIBS            = 9,
            E_RESOURCE_LOAD_WHEELS               = 10,
            E_RESOURCE_WFLOAD_WHEELS             = 11,
            E_RESOURCE_ACQUIRE_COUNT             = 12,
        };

        // :521
        enum EState
        {
            E_STATE_INVALID      = -1,
            E_STATE_STARTING_UP  = 0,
            E_STATE_RUNNING      = 1,
            E_STATE_TEARING_DOWN = 2,
        };

        // :530
        enum EStartingUpState
        {
            E_STARTINGUPSTATE_INVALID              = -1,
            E_STARTINGUPSTATE_WAITING_FOR_PLAYER   = 0,
            E_STARTINGUPSTATE_POPULATING           = 1,
            E_STARTINGUPSTATE_WAITING_FOR_STREAMING= 2,
            E_STARTINGUPSTATE_FIRST                = 0,
            E_STARTINGUPSTATE_LAST                 = 2,
        };

        // :542
        enum ERunningState
        {
            E_RUNNINGSTATE_INVALID = -1,
            E_RUNNINGSTATE_NORMAL  = 0,
            E_RUNNINGSTATE_PAUSED  = 1,
        };

        // :550
        enum ETearingDownState
        {
            E_TEARINGDOWNSTATE_INVALID         = -1,
            E_TEARINGDOWNSTATE_WIPING          = 0,
            E_TEARINGDOWNSTATE_FLUSHING        = 1,
            E_TEARINGDOWNSTATE_WAITING_TO_RESET= 2,
        };

        // :559
        enum EEmptyTrafficPoolState
        {
            E_EMPTYTRAFFICPOOLSTATE_IDLE     = 0,
            E_EMPTYTRAFFICPOOLSTATE_EMPTYING = 1,
            E_EMPTYTRAFFICPOOLSTATE_EMPTY    = 2,
            E_EMPTYTRAFFICPOOLSTATE_FILLING  = 3,
        };

        // :596 -- the Array<PhysicalVehicleInfo,33> capacity (that Array instantiation TU is
        // already committed next to this header).
        static const u32 KU_MAX_PHYSICAL_VEHICLES_TO_CACHE = 33;

        // The event-receiver queue instantiation the DWARF names at :613.
        typedef CgsModule::EventReceiverQueue<4096, 16> TrafficReceiverQueue;

        // ---- lifecycle (attested by WorldModule::Construct @0x827CF540 /
        //      ::DestructWorld @0x827BD0F0 / ::ReleaseWorld @0x827BCE58) ----
        void Construct();
        void Destruct();
        bool Release();

        // ---- (WorldModule::EntityModulePrePhysicsUpdate @0x827BD5B8) ----
        void PrePhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                               CgsModule::IOBufferStack* lpOutputBufferStack,
                               BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
                               BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                               BrnUpdateSet lUpdateSet );

        // ---- (WorldModule::EntityModulePostSceneUpdate @0x827C3C58) ----
        void PostSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                              CgsModule::IOBufferStack* lpOutputBufferStack,
                              BrnTrafficIO::InputBuffer_PostScene* lpInput,
                              BrnTrafficIO::OutputBuffer_PostScene* lpOutput,
                              BrnUpdateSet lUpdateSet );

        // ---- (WorldModule::EntityModulePreSceneUpdate @0x827BD1F0) ----
        void PreSceneUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                             CgsModule::IOBufferStack* lpOutputBufferStack,
                             BrnTrafficIO::InputBuffer_PreScene* lpInput,
                             BrnTrafficIO::OutputBuffer_PreScene* lpOutput,
                             BrnUpdateSet lUpdateSet );

        // ---- (WorldModule::EntityModulePostPhysicsUpdate @0x827D3F10 +
        //      WorldModule::UpdateForBootUpVideo @0x827CFDE0) ----
        void PostPhysicsUpdate( CgsModule::IOBufferStack* lpInputBufferStack,
                                CgsModule::IOBufferStack* lpOutputBufferStack,
                                BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput,
                                BrnUpdateSet lUpdateSet );

        // ---- the render trio (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
        // Bodies in BrnTrafficEntityModule_Render.cpp. Each signature is the DecFIGS DWARF's
        // parameter list (:19315 / :23926 / :23103), cross-checked against the X360 call site
        // and callee prologue.

        // @0x8274D900 (export hole -- no .ida-exports/.../0x8274D900.json). DWARF :19315.
        // Visible-id walk -> IsAlive skip -> distance cull at mfRenderCullDistanceSq ->
        // near-far sort -> cap at muMaxVehiclesToRender. Takes the BUFFER (X360
        // @0x827D249C `mr r5, r19`), unlike GenerateDispatchLists below.
        void PreDispatchUpdate( const BrnTrafficIO::InputBuffer_PreDispatch* lpInput,
                                BrnTrafficIO::OutputBuffer_PreDispatch* lpOutput );

        // @0x8273B280, DWARF :23926.
        // Arg 2 is the ARRAY, not the buffer: X360 `addi r22, r19, 4` @0x827D24B0 (r19 == the
        // OutputBuffer_PreDispatch) feeds the call @0x827D2824, and the callee's first use is
        // `bl Array<VehicleRenderInfo,64>::GetLength` @0x8273B328. On the host the two
        // addresses genuinely differ (IOBuffer leads with a 1-byte FlagSet8), so a body written
        // against the buffer would read miCount out of the status byte.
        // The four vector arguments come from v1..v4 at the console call site (@0x827D27F8..
        // @0x827D2810); the callee forwards (position, direction) to both corona producers and
        // (position, scattering, colour) to RenderTrafficCar. Camera is a reference, per DWARF.
        void GenerateDispatchLists( const BrnTrafficIO::InputBuffer_Dispatch* lpInput,
                                    const ::Array<VehicleRenderInfo, 64u>& laTrafficRenderInfos,
                                    Vector4 lFogScattering,
                                    Vector4 lFogColourPlusWhiteLevel,
                                    Vector3 lCameraPosition,
                                    Vector3 lCameraDirection,
                                    s32 liModelOnlyDisplayList,
                                    s32 liOpaqueList,
                                    s32 liTransparentList,
                                    const BrnDirector::Camera::Camera& lBrnCamera );

        // @0x82728B08, DWARF :23103 (parameter names and order verbatim). lFrontLights /
        // lRearLights are the out-pair SubmitCoronasForVehicle (DWARF :22445) writes and this
        // function consumes, not a corona-only side channel.
        // lpiUpdatedNumDamagedVehiclesRendered is the shared per-frame budget
        // GenerateDispatchLists seeds to 0 once for the whole loop (`*a52 < 5`, pseudocode :1254).
        void RenderTrafficCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                               u32 luEntityIdx,
                               Vector3 lCameraPosition,
                               Vector4 lFogScattering,
                               Vector4 lFogColourPlusWhiteLevel,
                               BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowRenderer,
                               s32 liModelOnlyDisplayList,
                               s32 liOpaqueList,
                               s32 liTransparentList,
                               BrnWorld::ShadowMap* lpShadowMap,
                               CgsGraphics::Model::State lLOD,
                               Vector4 lFrontLights,
                               Vector4 lRearLights,
                               s32* lpiUpdatedNumDamagedVehiclesRendered );

        // No member sits at X360 +0x71870: GenerateDispatchLists' `stvx128 v127, r29, r11`
        // @0x8273B2DC is the inlined `mFuzzyBehaviours.DEBUGSetLastCameraPos(lCameraPosition)`.
        // mpData is at +0x71840 and a console ResourcePtr is 32 bytes, so mFuzzyBehaviours
        // (:753) starts at 0x71860 and its mDEBUGLastCameraPos is at +0x10 == 0x71870.
        // PARKED: the sibling unnamed word at +0x72A38 is a ship-only PerfMonCpu id, past the
        // DWARF's perfmon block. Leave it unnamed until something attests it.

        // @0x8274A578. PARTIAL body in BrnTrafficEntityModule_wQ7_02.cpp: the six-stage ladder
        // is real, stages 1/3/4 are named one-shot gates.
        // FLAG (module vtable wave): DWARF :1079 declares it `virtual`; it is non-virtual here
        // because the base's `virtual bool Prepare()` takes no argument, so this overload hides
        // rather than overrides it and a virtual would add a slot the console lacks.
        bool Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer );

        // @0x82746A88 (465 insns), DWARF :1266 `bool LoadData(OutputBuffer_Prepare*)`.
        // The module's resource-acquire ladder, driven by Prepare stage 2. PARTIAL body in
        // BrnTrafficEntityModule_wQ7_02.cpp.
        bool LoadData( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer );

        // @0x82720A90, DWARF :1443. Drains the two prop->traffic rings (traffic-light
        // knock-downs and restores) that
        // WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70 copied into the
        // pre-physics input buffer, applying each to mTrafficLightManager.
        // Body in BrnTrafficEntityModule_wQ7_01.cpp.
        void HandlePropModuleRequests( const BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
                                       BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput );

        // ---- pool accessors. Each indexes a named pool; the address on each line is the
        //      console accessor whose element-index arithmetic attests the pool bounds. ----
        Vehicle* GetVehicle(u32 luIndex);                            // leak :1590
        // DWARF :1230 -- the const twin the console ICF-folds onto :1227. Body in
        // _wT3_00.cpp; added so the const accessors reach the vehicle pool.
        const Vehicle* GetVehicle(u32 luIndex) const;                // DWARF :1230
        Vehicle* GetStandardVehicle(u32 luIndex);                    // @ 0x82707A38
        Vehicle* GetStaticVehicle(u32 luIndex);                      // @ 0x827079D0
        Vehicle* GetTrailerVehicle(s32 liIndex);                     // @ 0x82707AA0
        VehicleAxles* GetVehicleAxles(u32 luIndex);                  // @ 0x82707C28
        StaticTrafficParam* GetStaticTrafficParam(u32 luIndex);      // @ 0x82707858
        StaticTrafficParam* GetStaticTrafficParamFro(u32 luIndex);   // @ 0x82707950
        StaticTrafficParam* GetStaticTrafficParamFromFullV(u32 luIndex); // @ 0x827078D0
        u32   GetVehicleIndexFromStaticIndex(u32 luStaticVehicle);   // @ 0x82707D18
        Param* GetParam(u32 luParam);                                // leak :1457
        ParamPlan* GetParamPlan(u32 luParam, u32 luPlan);            // @ 0x82707D70
        // const overload -- DoesParamNeedToStopForStopline @0x827249F8 is a const method and
        // calls it (sub_82707E38 in its listing).
        const ParamPlan* GetParamPlan(u32 luParam, u32 luPlan) const;
        const VehicleTypeRuntime* GetVehicleTypeRuntime(u32 luVehicleType) const; // leak :1655

        // @ 0x827077D0. Bounds-asserts luParam < KU_MAX_PARAMS AND
        // muLastParamCalculated >= KU_MAX_PARAMS, then returns &maParamNeedToSlowData[luParam].
        // FLAG: DWARF :2469 spells only the const overload; the ship needs the mutable one
        // (UpdateParams_PrecalcBehaviourParams @0x82717C48 writes through it).
        ParamNeedToSlowData* GetParamNeedToSlowData(u32 luParam);

        // @ 0x82707700 (EXPORT HOLE -- no per-function JSON; DEBUG_AddFuzzyLogicData
        // @0x82716040 calls it by name). DWARF :2455. Shape is GetParam's: bounds assert then
        // &maParamTransforms[luParam].
        ParamTransform* GetParamTransform(u32 luParam);

        // @ 0x82707C90 (EXPORT HOLE -- GetSympCrashingTargetPos @0x82708C10 calls it by name).
        // DWARF :2558 returns Matrix44Affine BY VALUE; SetVehicleTransform @0x827142B8 is the
        // writer.
        Matrix44Affine GetVehicleTransform(u32 luIndex) const;

        // DWARF :2698 (const overload) / BrnTrafficUnity.cpp:15022. No standalone ARTIST
        // symbol: every X360 caller inlines the bounds assert plus the array index. Declared
        // mutable because UpdateParams_UpdateLinkedList @0x82739660 relinks through it.
        ParamListNode* GetParamListNode(u32 luParam);

        // DWARF :1602 (BrnTrafficUnity.cpp:18527, .cpp 9146). Reads the param's list node
        // and returns muPrevParam. No standalone ARTIST symbol: every caller inlines it.
        u32 GetParamBehind(u32 luParam) const;

        // X360 @0x8273F0B8. Body in BrnTrafficEntityModule_wQ7_02.cpp beside its one caller,
        // LoadData stage 7. It is a TrafficEntityModule member: its pseudocode reaches mpData
        // and mpVehicleList, and its three baked asserts cite BrnTrafficEntityModule.cpp.
        // Return width is 64 bits, not Attribute::Key (u32 tree-wide): it tail-returns
        // CgsAttribSys::AttribSysCollectionKey::GetHashKey, which this tree models as u64, and
        // VehicleTypeRuntime::Prepare stores the whole register with `std`. Spelled through
        // VehicleTypeRuntime's AttribKey typedef so producer and consumer cannot drift.
        // _EXPENSIVE is the console's own name: it does a linear VehicleList::GetVehicleIndex
        // scan per vehicle type, so LoadData calls it once per type at load, never per frame.
        VehicleTypeRuntime::AttribKey FindVehicleTypeAttribKey_EXPENSIVE(u32 luVehicleType) const;

        bool  IsPaused();                                            // @ 0x82707560
        bool  ShouldBeHollywoodAction();                             // @ 0x827075C8
        bool  NeedToTakeActionAgainstJunctionFUP();                  // @ 0x82707FD0
        void  EnterReplay();                                         // @ 0x827081D8
        void  LeaveReplay();                                         // @ 0x82708248
        void  RestartTraffic();                                      // @ 0x82708F98

        // ---- declaration-only and FLAGged. These reach sub-aggregate interiors that are
        //      only partially recovered (Vehicle / Param tails, the VMX avoidance pipeline),
        //      so they land with the waves that own those types. ----
        void* Avoidance_CalculateDistancePosVelToOrig(void* lpResult);// @ 0x82708DD0 (FLAG: VMX)
        void  Avoidance_CalculatePassingScore();                     // @ 0x827199B8 (FLAG: VMX)
        void  CalculateAndSetSteeringUsingAvoidance();               // @ 0x8273D258 (FLAG: VMX)
        // @ 0x82718CD8. r3 is the sret VecFloat, r4 this, r5 luVehicle, v1 the forward
        // distance to the target, v2 the param's linear velocity. Bodied in _wT3_02.cpp.
        VecFloat CalculateDriverGasBrake(u32 luVehicle, VecFloat lfDistToTarget,
                                         Vector3 lParamLinearVelocity);
        void  DEBUG_AddFuzzyLogicData();                             // @ 0x82716040 (FLAG: debug)
        void  DEBUG_RenderContactPoint();                            // @ 0x827082B8 (FLAG: debug)
        // @0x8273EFC8 (59). BODIED in _wT3_00.cpp. DWARF :1812 spells it
        // `const Attribute::Key GetCarAssetAttribKey(uint32_t) const` -- the trailing const
        // is restored here (the body only reads; it reaches Vehicle through the const
        // GetVehicle overload below). Return width is the console's 8-byte key.
        VehicleTypeRuntime::AttribKey GetCarAssetAttribKey(u32 luVehicle) const;
        void  GetDeterministicParamPos(u32 luParam);                 // @ 0x82714258 (FLAG: ParamTransform)
        // @0x82708C10. SIGNATURE CORRECTED 2026-08-28 from the asm, which was never a
        // `void f(u32, void*)`: r4 is the sympathetic-crash target's EntityId (the body splits
        // its owner byte and 14-bit index), r5 is a Vector3 out-slot, and r3 returns 0/1
        // ("found a live target position"). The FLAG that stood here said it was an ARTIST
        // export hole with no body -- STALE, the per-function export exists (77 asm lines) and
        // the body is in _wT2_06.cpp.
        bool  GetSympCrashingTargetPos(EntityId lTargetEntityId, Vector3* lpOutPos) const;
        // @0x82714500 (153). BODIED in _wT3_00.cpp. DWARF :1242/:1245
        // (`GetTrafficPhysicsInfoForVehicle`; the X360/ledger symbol truncates the name).
        // Returns &maTrafficPhysicsInfoList[ GetVehicle(luVehicle)->GetPhysicalPartsIndex() ].
        TrafficPhysicsInfo*       GetTrafficPhysicsInfoForVehicl(u32 luVehicle);
        const TrafficPhysicsInfo* GetTrafficPhysicsInfoForVehicl(u32 luVehicle) const;
        void  HideAllTraffic();                                      // @ 0x8273F418 (FLAG)
        void  UnhideAllTraffic();                                    // @ 0x8274A500 (FLAG)
        // Body in BrnTrafficEntityModule_wT1_05.cpp, beside its caller CreateNewVehicleEntities.
        // FLAG: DWARF :1323 spells it `bool IsVehiclesParamAZombie(uint32_t) const`. The
        // trailing const is dropped because the three accessors it calls (GetParam / GetVehicle
        // / GetStaticTrafficParamFro) are non-const here; add it once those gain const
        // overloads. The body only reads, so it is const-correct in substance.
        bool  IsVehiclesParamAZombie(u32 luVehicle);                 // @ 0x82715D70 (DWARF :1323)
        // @0x82719868 / @0x8273F2E8. SIGNATURES CORRECTED 2026-08-28 from the asm; BODIES in
        // _wT5_01.cpp. The FLAG that stood here carried TWO wrong declarations:
        // `void JunctionFUP_StopOffscreenTraffic(void*, bool)` and, worse,
        // `void JunctionFUP_TryClearupNonMovingPhysical()` -- no parameters and a void return
        // for a function the console calls with two arguments and whose r3 returns 0/1
        // (0x8273F308 `li r3,0` / 0x8273F408 `li r3,1`). The first parameter is the LIVE
        // FastBitArray iterator, not an opaque blob: both bodies do `lwz r11, 0(r4)` == its
        // miIndex, and both DWARF dumps spell it `const FastBitArray<601>::Iterator&`
        // (BrnTrafficEntityModule.h:1884/:1887). Spelled through VehicleSoaData's own
        // KU_MAX_VEHICLES so the iterator type matches the sets UpdateJunctionFUP walks.
        void  JunctionFUP_StopOffscreenTraffic(
                  const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
                  bool lbRenderedLastFrame);
        bool  JunctionFUP_TryClearupNonMovingPhysical(
                  const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
                  bool lbRenderedLastFrame);
        // @ 0x82741E40 / @ 0x8272EB40 -- the REMOVE half of the scene registration (bodies in
        // BrnTrafficEntityModule_KillDyingVehicleEntities.cpp). The output buffer is the same
        // PreScene one CreateNewVehicleEntities takes (assert .cpp 4410/4463); the callee takes
        // the LIVE iterator because Vehicle::SetCollidable consumes its cached word mask.
        void  KillDyingVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
        void  KillDyingVehicleEntity(
                  u32 luVehicle,
                  const CgsContainers::FastBitArray<VehicleSoaData::KU_MAX_VEHICLES>::Iterator& lrItVehicle,
                  BrnTrafficIO::OutputBuffer_PreScene* lpOutput);
        void  PutParamInPurgatory(u32 luParam);                      // @ 0x82716510 (FLAG: Array interior)
        void  RebuildGeneratorList();                                // @ 0x82742DD0 (FLAG)
        void  UpdateSerialiser();                                    // @ 0x8272DA80 (FLAG)
        // @ 0x82707F00. Console defines it here (its assert names BrnTrafficEntityModule.h:2637).
        // The fsel else-value flt_82001CC0 is 0.0f: Param::Construct @0x82751B60 loads it as the
        // SetParamAlong argument `0.0`, ParamListNode::Construct @0x82751C38 as `mfParamAlong = 0.0`.
        void  UpdateVehicleStuckSideTime(s32 liFlags, s32 liMask, f32 lfReset,
                                         f32 lfThreshold, f32* lpfTimer)
        {
            CGS_ASSERT(lpfTimer != 0, "lpfTimer");   // 0x82707F2C..0x82707F50, .h:2637

            if ((liFlags & liMask) != 0)
            {
                if (*lpfTimer < lfThreshold)    // 0x82707F64 fcmpu / bge
                {
                    *lpfTimer = lfReset;
                }
                *lpfTimer = mfSimTimeStep + *lpfTimer;   // 0x82707F80 fadds
            }
            else
            {
                *lpfTimer = *lpfTimer - mfSimTimeStep;   // 0x82707FA8 fsubs
                if (*lpfTimer < 0.0f)                    // 0x82707FB4 fsel f0, f0, f0, 0.0f
                {
                    *lpfTimer = 0.0f;
                }
            }
        }
        void  UpdateVehicleStuckTimers(void* lpPhysicsInfo, f32 lfReset, f32 lfThreshold); // @ 0x82708D48 (FLAG)

        // =====================================================================================
        // WAVE T3 ROUND 1 -- PHYSICAL TRAFFIC. The world-side promotion chain plus the driver
        // producer, declared together so every cluster compiles against one header.
        // Signatures are the DecFIGS DWARF's (BrnTrafficEntityModule.h :1170..:1578).
        // =====================================================================================

        // The per-vehicle "made physical this frame" set the promotion chain threads through.
        // DWARF spells it BitArray<601u>; the ship pool is 600 (asserted as 0x258 everywhere),
        // the same off-by-one the DWARF carries on the index map.
        typedef CgsContainers::BitArray<KU_MAX_TOTAL_TRAFFIC> TotalTrafficBitArray;

        // @0x8274C510 (96). DWARF :1569. PrePhysicsUpdate's RUNNING arm: walk each
        // TrafficJobStub's PhysicalRequestInfoList, promote, then clear the list.
        void SendPhysicalRequests(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                  TotalTrafficBitArray* lpMadePhysical);

        // @0x8274AFD0 (234). DWARF :1572. Vehicle::GetPhysicalReason is SIGN-extended
        // (0x82705540 lbz + extsb) and 0x8274B184 compares cmpwi r3, -1: == -1 is correct here.
        void SafeRequestMakeVehiclePhysical(u32 luVehicle, PhysicalReason leReason,
                                            EntityId lTargetEntityId,
                                            BrnPhysics::Vehicle::ETrafficType leTrafficType,
                                            BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
                                            BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                            TotalTrafficBitArray* lpMadePhysical);

        // @0x82747200 (162). DWARF :1575.
        void MakeVehiclePhysical(u32 luVehicle,
                                 BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                 TotalTrafficBitArray* lpMadePhysical,
                                 EntityId lTargetEntityId,
                                 BrnPhysics::Vehicle::ETrafficType leTrafficType,
                                 BrnPhysics::Vehicle::eCrashTrafficType leCrashType);

        // @0x827425B0 (462). DWARF :1405. Posts the spawn event onto the physics input
        // interface (CreatePhysicalTraffic, or CreateArticulatedTraffic on the trailer arm).
        void AddVehicleToPhysics(u32 luVehicle, EntityId lTargetEntityId,
                                 BrnPhysics::Vehicle::VehicleInputInterface* lpVehicleInput,
                                 BrnPhysics::Vehicle::ETrafficType leTrafficType,
                                 TotalTrafficBitArray* lpMadePhysical);

        // ---- THE DEMOTION HALF. Bodies in _wT3_02.cpp beside their caller.
        // The chain, end to end: DriveTowardsTarget -> ReturnPhysicalVehicleToTraffic ->
        // StopVehicleBeingPhysical (frees the module's TrafficPhysicsInfo slot AND queues the
        // vehicle index in maNewRemovedVehicles) -> CleanUpCrashedVehiclePhysics (drains that
        // array into the physics RemoveTrafficEvent queue) -> PhysicalTrafficManager::
        // ProcessRemoveEvents frees the 20-slot physical pool. Without it the pool only ever
        // fills.

        // @0x8273DCD0 (188). Hand a physical car back to the param sim: re-seat its axles on
        // its current transform, stop it being physical, un-stick a DRIVE_AROUND param, then
        // recurse into the articulated other half.
        void ReturnPhysicalVehicleToTraffic(u32 luVehicle);

        // @0x8271FED0 (88). The third console argument is a byte; the append to
        // maNewRemovedVehicles (and therefore the physics RemoveTrafficEvent) is skipped when
        // it is non-zero, so it names "physics already knows". FLAG: name inferred from that
        // single use, not from a symbol.
        void StopVehicleBeingPhysical(u32 luVehicle, bool lbSuppressPhysicsRemoval);

        // @0x82720960 (76). Drain maNewRemovedVehicles into the physics RemoveTrafficEvent
        // queue and clear it. Called from PrePhysicsUpdate's three arms.
        void CleanUpCrashedVehiclePhysics(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput);

        // @0x8271DD30 (96). BODIED in _wT3_00.cpp. DWARF :1578; the Feb-2007 leak
        // spells the body verbatim at BrnTrafficEntityModule.cpp:1410.
        void CalculateInitialPhysicalState(const Vehicle* lpInVehicle,
                                           Matrix44Affine lVehicleTransform,
                                           Vector3& lOutInitialVelocity,
                                           Vector3& lOutAngularVelocity,
                                           u8* lpuOutAttribsId,
                                           Matrix44Affine& lOutTransform) const;

        // @0x82720EC0 (188). DWARF :1170. Claims the maTrafficPhysicsInfoList slot, Constructs
        // the record, and flips Vehicle::SetPhysical / OnPhysical.
        void RecordTrafficVehicleIsPhysical(u32 luVehicle, EntityId lEntityId,
                                            EntityId lTargetEntityId,
                                            BrnPhysics::Vehicle::eCrashTrafficType leCrashType,
                                            f32 lfArg4, f32 lfArg5);

        // @0x82748E78 (1439). DWARF :1356. The PRODUCER of BrnTrafficDriverControls -- the one
        // missing piece for "the promoted car keeps driving".
        void GenerateDriverInputs(BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput);

        // The manoeuvre arms GenerateDriverInputs dispatches to (DWARF :1371..:1398).
        // UpdateExtremeSwerving is @0x8273E8D0 (164 insns) -- it has no per-function export
        // JSON, but IDA names it and the body was dumped headless.
        // lpOutput is the console's r5 (GenerateDriverInputs' arg_1C); the arm never reads it.
        void UpdateExtremeSwerving(u32 luVehicle,
                                   BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                   BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        void UpdateRecoveringFromSlam(u32 luVehicle,
                                      BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        void Update3PointTurnManoeuvre(u32 luVehicle,
                                       BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        void UpdateGiveUpManoeuvre(u32 luVehicle,
                                   BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        void UpdateStuckReverseManoeuvre(u32 luVehicle,
                                         BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        void UpdateNormalPhysical(u32 luVehicle,
                                  BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        // @0x8273DFC0 (DWARF :1400). The shared "drive at Vehicle::GetTargetPos" body every
        // non-swerving physical traffic car runs. lbAllowReturnToTraffic is the console's r5
        // (UpdateNormalPhysical passes 1).
        void DriveTowardsTarget(u32 luVehicle, bool lbAllowReturnToTraffic,
                                BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls);
        // @0x8272C010 -- declaration only this round (see the gate in _wT3_02.cpp).
        bool CheckIfPhysicalVehicleIsStuck(u32 luVehicle);
        // @0x8273D378 (594). DWARF :1394. BODIED 2026-08-29 in
        // BrnTrafficEntityModule_SympatheticCrash.cpp -- the four-state chain-crash driver
        // (HEADON / ACCELERATE / HANDBRAKE / LOCKUP) that aims a nearby traffic car at a wreck
        // and then crashes it.
        void UpdateSympatheticCrashing(u32 luVehicle, EntityId lEntityId,
                                       BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                       BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls,
                                       f32 lfTimeStep);

        // @0x8272BA08 (95). DWARF :1587. The commit every arm of UpdateSympatheticCrashing ends
        // on: reason = CRASHED, crash type = Standard, and the SetTrafficCrashingEvent that
        // PhysicalTrafficManager::ProcessSetTrafficCrashingEvents drains. Same TU.
        void CrashVehicleForSympatheticCrashState(u32 luVehicle,
                                                  BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput);

        // FLAG: OUTLINED, NOT A CONSOLE SYMBOL. The X360 emits this 45-instruction block TWICE
        // inside UpdateSympatheticCrashing (0x8273D754..0x8273D860 and 0x8273DAD8..0x8273DBDC),
        // instruction-identical apart from the stack slots -- i.e. the compiler inlined one
        // helper at two call sites. Restored as that helper per AGENTS.md "Inlining reversal".
        // It adds no behaviour: it is the camera-facing test, CalculateDriverGasBrake, and the
        // [0,1] gas/brake split, in the console's own order.
        void ApplyDriverGasBrake(u32 luVehicle, f32 lfForwardDistance, Vector3 lvVehiclePos,
                                 BrnPhysics::Vehicle::BrnTrafficDriverControls* lpOutControls);

        // @0x82718E48 (153). DWARF :1365.
        void CalculateAndSetSteering(u32 luVehicle, Vector3 lTargetDirection,
                                     BrnPhysics::Vehicle::BrnTrafficDriverControls* lpControls,
                                     VecFloat lvfScale);

        // @0x82732C68 (1302). DWARF :1432 (leak :3652). PostPhysicsUpdate's RUNNING head leg:
        // drains mCrashedTrafficEventQueue and the PhysicalTrafficState queue back onto the
        // world vehicles. SIGN: the read-back applies Negate(mBBoxOffset) where
        // CalculateInitialPhysicalState added it.
        void HandleExternalResponses(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput);

        // @0x8271DEB0 (883). DWARF: `void ProcessDeformationData(const BrnPhysics::Deformation::
        // DeformationOutputInterfaceForEntityModules*)`. PostPhysicsUpdate's RUNNING head leg
        // right after HandleExternalResponses (0x8274E8A4): pulls the physics side's
        // per-frame deformation output for every PHYSICAL traffic vehicle into its
        // TrafficPhysicsInfo -- the 128-point skinning offsets (and the fatal-deformation
        // test), the four wheel transforms + mabWheelExists, the light-locator table, the
        // detached-part records and the glass smash/crack state. Body in
        // BrnTrafficEntityModule_ProcessDeformationData.cpp.
        void ProcessDeformationData(
            const BrnPhysics::Deformation::DeformationOutputInterfaceForEntityModules* lpDefInterface);

        // ---- the spawn legs. Bodies in BrnTrafficEntityModule_wT1_01.cpp; the address on
        //      each line is its ARTIST entry point. ----

        // The active-hull set the spawn ladder passes around. X360-attested as Set<u16,72>:
        // SpawnNewTraffic @0x82748A40 reads the length word at +0x90 == 144 == 72 * sizeof(u16),
        // and RecalculateActiveHulls @0x8274C870 memcpy's 148 bytes of it.
        typedef ::Set<u16, KU_MAX_ACTIVE_HULLS> ActiveHullSet;

        // ---- module lifecycle / state machine ----
        void Reset();                    // @ 0x8272CDA0
        void ResetEventData();           // @ 0x827088B8
        void EnterStartingUpState();     // @ 0x82708038
        void EnterTearingDownState();    // @ 0x82708168
        void EnterRunningState();        // @ 0x827080E8
        bool IsDecisionFrame();          // @ 0x827074E0
        void UpdateDensity();            // @ 0x82716318

        // DWARF :1140 gives the shape; the X360 has no standalone export because every call
        // site inlines it to the flag load (SpawnShowtimeTraffic @0x82743088:
        // `lis r11,7 / ori r11,r11,0x17DD / lbzx r11,r30,r11 / cmplwi r11,0`). Inline here for
        // the same reason.
        bool IsPlayingShowtimeGameMode() const { return mbPlayingShowtimeMode; }

        // ---- the streamer pump. Bodies in BrnTrafficEntityModule_wT1_04.cpp. ----
        //
        // DWARF :1554 returns void even though the X360 body leaves the tail Append's bool in
        // r3; that is a tail-position value, not a return.
        //
        // UpdateStreaming is what makes the game ask for a VEH_T* bundle. LoadData's stage-1
        // SetAssetList publishes the catalogue, but nothing requests a bundle until
        // TrafficCarStreamer::Update runs, and UpdateStreaming is its only pump (both call
        // sites are arms of PostPhysicsUpdate @0x8274E6D0).
        void UpdateStreaming(BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);   // @ 0x82748848

        // @0x82722470 -- the MODULE-level half (TrafficCarStreamer has a same-named member
        // @0x8274F6A0 that takes the list; this one FINDS the list). Console signature is
        // `this` only. Called from UpdateStreaming's meEmptyTrafficPoolState arms 0 and 3.
        void AddVehiclesToTargetList();                                          // @ 0x82722470

        // ---- hull / vehicle-type lookups (console-inlined accessors, out-of-line here) ----
        const Hull*   GetHull(u32 luIndex) const;             // @ 0x8271D8B0
        HullRuntime*  GetHullRuntime(u32 luHull);             // @ 0x8271D9D0
        HullRuntime*  GetHullRuntimeSafe(u32 luHull);         // @ 0x8271DA70 (returns 0 when unallocated)
        // DWARF :2270 / :2274 spell both accessors const; the ship needs the mutable pair too.
        const HullRuntime* GetHullRuntime(u32 luHull) const;
        const HullRuntime* GetHullRuntimeSafe(u32 luHull) const;

        // @ 0x827142B8 -- validity-asserts the affine and stores it into maVehicleTransforms.
        void SetVehicleTransform(u32 luIndex, const Matrix44Affine& lTransform);

        // ---- the spawn ladder ----
        void RecalculateActiveHulls(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput,
                                    ActiveHullSet* lpOutNewHulls,
                                    ActiveHullSet* lpOutOldHulls);        // @ 0x8274C870
        void SpawnNewTraffic(const ActiveHullSet& lrNewActiveHulls);      // @ 0x82748A40
        void FillNewHull(u16 luHull);                                     // @ 0x82743600
        u8   PickVehicleToSpawn(u32 luFlowTypeId);                        // @ 0x827235F8

        // The SHOWTIME half of the ladder (bodies in _wT1_07.cpp). DWARF :1566 / :1548 /
        // :1611; shapes are the DWARF's. SpawnShowtimeTraffic is UpdateDecisionFrame's
        // showtime-only second spawn source, and the two helpers are the spacing tests it
        // and the authored generator both need.
        void SpawnShowtimeTraffic();                                       // @ 0x82743038
        bool IsParamTooClose(u32 luHull, u32 luSection, f32 lfParamAlong);  // @ 0x82726470
        u32  CountParamsOnSection(u32 luHull, u32 luSection) const;        // @ 0x82723D10

        // ---- the static (parked) vehicle sub-system ----
        void StaticVehicles_Generate(u8 luVehicleType, u16 luHull, u8 luIndexOnHull); // @ 0x82722680
        void StaticVehicles_CreateNewVehicles(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput); // @ 0x827229F0
        // Takes the post-physics INPUT buffer and forwards it (DWARF :1839). IDA's prototype
        // for @0x82722F98 shows one argument, a Hex-Rays artefact of a pass-through: the body
        // never touches r4, and its only caller UpdateDecisionFrame @0x8274E508 loads it
        // (`0x8274E61C mr r4, r30`, r30 == lpInput) right before the `bl`, so r4 flows straight
        // into StaticVehicles_CreateNewVehicles. Dropping the parameter would pass a literal
        // null there, a null deref once that function's race-car proximity arm is un-gated.
        void StaticVehicles_UpdateVehicles(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput); // @ 0x82722F98
        void StaticVehicles_UpdateStaticParams();                         // @ 0x82722F28
        void StaticVehicles_UpdatePurgatory();                            // @ 0x827228A8
        void StaticVehicles_KillParam(u32 luParam);                       // @ 0x82721C50
        void StaticVehicles_RemoveDeadParam(u32 luParam);                 // @ 0x827163D0

        // ---- the steady-state loop and scene presence ----

        // @0x82715858, DWARF :1287. Body in BrnTrafficEntityModule_wT1_06.cpp.
        // It is the image's only writer of mbDecisionFrame (the +0x713F5 store). Without it
        // IsDecisionFrame() returns Reset's `false` for ever once meState leaves
        // E_STATE_STARTING_UP, so PostPhysicsUpdate's RUNNING arm can only take the
        // non-decision branch and RecalculateActiveHulls / SpawnNewTraffic / the
        // StaticVehicles_* updates never run. Its one caller is PreSceneUpdate's
        // E_STATE_RUNNING arm (@0x8274A968).
        void UpdateTimers(const BrnTrafficIO::InputBuffer_PreScene* lpInput);

        // @0x82715A18, DWARF :1290. Body in BrnTrafficEntityModule_wT5_01.cpp.
        // THE ONLY WRITER of mfCrashSliderFinalValue, i.e. the only producer of the input
        // ShouldBeHollywoodAction() reads. Called from PreSceneUpdate's E_STATE_RUNNING arm
        // immediately after UpdateTimers (`bl 0x82715A18` @0x8274ABF0), inside the same
        // `!IsPaused() && !lbSimPaused` guard.
        void UpdateCrashSlider();

        // @0x8271FBE8 (185 insns). Body in BrnTrafficEntityModule_wT5_01.cpp.
        // Takes a vehicle back out of the physics/crash module's books: clears its
        // mVehiclesAddedToCrashModule bit and queues it on maRecentlyRemovedVehicles, then --
        // if it was mid-slam-recovery -- queues it on maRecentlyRecoveredSlammedTraffic and
        // resets its crash type to eCrashTrafficType_Invalid. Callers: StopVehicleBeingPhysical,
        // StaticVehicles_KillParam, KillParam and RemoveVehicle -- the last of which is now
        // bodied beside it (below), so this one is live on the shipped path.
        void EnsureVehicleRemovedFromCrashModule(u32 luVehicle);

        // @0x8272E370 (499 insns), DWARF BrnTrafficEntityModule.h:1797. Body in
        // BrnTrafficEntityModule_wT5_01.cpp.
        // THE JUNCTION-FUP RELIEF VALVE, and the module's single kill entry point: eleven
        // callers (UpdateJunctionFUP, JunctionFUP_TryClearupNonMovingPhysical,
        // ReturnPhysicalVehicleToTraffic, TryClearupOffscreenTraffic, ClearupCrashedTraffic,
        // CleanUpCrashedVehicles, HandleRecycledTraffic, KillAllTrafficInCylinder,
        // FireKillZone, HideAllTraffic, PostPhysicsUpdate). It retires the vehicle's SoA
        // liveness and its crash-module registration and breaks any cab/trailer articulation;
        // it marks the PARAM (zombie or should-be-removed depending on
        // mbAllowDivergentBehaviour) rather than freeing it -- the actual pool recycle is
        // KillDyingVehicleEntities' job, and the scene/collision teardown stays there too.
        void RemoveVehicle(u32 luVehicle);

        // @0x82745218, DWARF :1875. Body in BrnTrafficEntityModule_wT5_01.cpp.
        // THE ONLY WRITER of mfJunctionFUP, i.e. the only producer of the input
        // NeedToTakeActionAgainstJunctionFUP() reads -- which gates UpdateParams_TryAvoidCrashing
        // AND SpawnNewTraffic @0x82748A40. Called from PrePhysicsUpdate between
        // BuildPotentialCollisionList and GenerateDriverInputs (`bl 0x82745218` @0x8274C7B0,
        // r3 == this only).
        void UpdateJunctionFUP();

        // @0x8274E508, DWARF :1476. The per-decision-frame call set the console runs in
        // E_STATE_RUNNING. Body in _wT1_06.cpp, PARTIAL: the driving-traffic legs are named
        // gates, the parked ladder is real.
        void UpdateDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                 BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8274C1A8, DWARF :1479. The cheap frames between decision frames. Body in
        // _wT1_06.cpp, PARTIAL for the same reason.
        void UpdateNonDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8274B660 (.cpp 5833). The post-physics game-action dispatch: walks the input
        // buffer's game-action queue and fans out to sixteen per-action arms. Body in
        // _wT6_03.cpp, PARTIAL -- only action 23 (PREPARE_FOR_MODE) is reconstructed; the
        // other fifteen are named gates blocked on callees with no body here.
        void HandleExternalRequests(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x827480D8. The per-event arming handler, dispatched from HandleExternalRequests
        // @0x8274B660 on game action 23 (E_ACTION_PREPARE_FOR_MODE). ⭐ Sole non-debug writer
        // of mbPlayingShowtimeMode, and the writer of meGameMode, the swerve/killzone/
        // clear-traffic flags, mfGameModeDensityScale, miBigVehicleAmount, mfSpeedMultiplier
        // and the crash-slider block. THREE arguments (this / lpInput / lpPFMAction) -- the
        // prologue reads r3/r4/r5 only; Hex-Rays' a4/a5 are phantoms. Body in _wT6_02.cpp.
        void HandlePrepareForModeAction(
            const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
            const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPFMAction);

        // @0x827353E8, DWARF :1551. The jam relief valve UpdateNonDecisionFrame runs when
        // mbNeedToRunTrafficJamNuker is latched: collect each maximal run of consecutive
        // behaviour-5 params under 5 m/s and, when a run is longer than four, mark every third
        // of them SetShouldBeRemoved. Independent of the junction FUP score, so a junction
        // pinned below the 65 threshold cannot starve it. Body in _wT6_01.cpp.
        void NukeTrafficJams();

        // ---- the driving-traffic set. DECLARATIONS ONLY: every body belongs to a later
        //      cluster of this round. Each signature is the DecFIGS DWARF's, checked argument
        //      for argument against the X360 call site where one exists; the address on each
        //      line is its ARTIST entry point. `EXPORT HOLE` means the image has the function
        //      (a named call site proves it) but no per-function JSON.

        // generation
        void AddGenerator(u32 luHull, u32 luSection, f32* lpfTimeTillNextGeneration); // @0x82734B00
        f32  CalcTimeToNextGeneration(u32 luHull, u32 luSection);    // @0x82721B08
        void GenerateNewVehicle(u32 luHull, u32 luSection, u32 luVehicleType, f32 lfParamAlong); // @0x82736528
        u32  TryAllocateParamId();                                   // @0x82723370
        void InsertParamIntoList(u32 luParam, u32 luHull, u32 luSection, f32 lfParamAlong); // @0x82725CB8
        void RemoveParamFromList(u32 luParam);                       // @0x82726340
        // @0x827261E8 (EXPORT HOLE -- UpdateParams_UpdateLinkedList @0x82739660 calls it
        // by name). DWARF BrnTrafficEntityModule.cpp:12824.
        void SwapParamsInList(u32 luParamA, u32 luParamB);

        // per-decision-frame param simulation
        // @0x82744A80 (DWARF :1626). lpInput is forwarded to UpdateParams_BuildListOfCrashingThings:
        // `mr r29, r4` @0x82744A94, `mr r5, r29` @0x82744C20, `bl ..._BuildListOfCrashingThings`
        // @0x82744C6C -- and that callee asserts r5 non-null against the string "lpInput".
        void UpdateParams(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput);
        void UpdateParams_UpdateDead();                              // @0x827369A8
        void UpdateParams_UpdatePurgatoryList();                     // @0x827244E0
        void UpdateParams_UpdatePlan(u32 luParam, u32 luSectionIndex); // @0x82737CE8
        void UpdateParams_UpdateBehaviour(u32 luParam);              // @0x82716C90
        void UpdateParams_PrecalcBehaviourParams(u32 luParam,
                                                 const Section* lpSection,
                                                 const Hull* lpHull,
                                                 Vector4 lConeA,
                                                 Vector4 lConeB,
                                                 Vector4 lConeC);    // @0x82717C48
        void UpdateParams_CalcDesiredSpeed(u32 luParam,
                                           const Section* lpSection,
                                           const Hull* lpHull,
                                           const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAvoidSet); // @0x82717928
        f32  UpdateParams_CalcAcceleration(u32 luParam,
                                           const Param* lpParam,
                                           const Section* lpSection,
                                           const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAvoidSet) const; // @0x827172B8
        void UpdateParams_IncrementParam(u32 luParam,
                                         const Hull** lpapHull,
                                         const Section** lpapSection); // @0x82738C80
        void UpdateParams_HandleLaneChanges(u32 luParam,
                                            const Hull* lpHull,
                                            const Section** lpapSection); // @0x82725880
        void UpdateParams_UpdateLinkedList();                        // @0x82739660
        // @0x82743FE8 (ARTIST EXPORT HOLE; DWARF :1635, BrnTrafficUnity.cpp:18355, .cpp 9770).
        // The non-decision-frame param time-slicer: it is what advances muLastParamCalculated.
        void UpdateParams_DoTimeSlicedLogic(
            u32 luBeginParam,
            u32 luEndParam,
            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                lpActiveRaceCarInterface);
        void UpdateParams_UpdateNeighbours(Param* lpParam,
                                           const Section* lpSection,
                                           const Hull* lpHull);      // @0x82708AC8
        void UpdateParams_TryToReinsertParam(u32 luParam);           // @0x827247F0
        void UpdateParam_CheckIfNeedToSlow(u32 luParam,
                                           const Hull* lpHull,
                                           u32 luSectionIndex,
                                           const Section* lpSection,
                                           const ::Array<PhysicalVehicleInfo,
                                                         KU_MAX_PHYSICAL_VEHICLES_TO_CACHE>* lpaPhysicalVehicles); // @0x82738468
        void UpdateParam_CheckIfInsideParamInFront(u32 luParam);     // @0x82717A70
        bool DoesParamNeedToStopForStopline(u32 luParam,
                                            u32 luSectionIndex,
                                            const Section* lpSection,
                                            const Hull* lpHull,
                                            f32* lpfOutStopDist) const; // @0x827249F8
        void EatParamsNextPlan(u32 luParam);                         // @0x827087D0
        u32  FindNextParam(u32 luParam, u32 luSectionIndex, f32 lfParamAlong) const; // @0x82723A48
        u32  FindNextParamRelative(u32 luParam, f32 lfParamAlong) const; // @0x82708400
        u32  FindNearestParamInFront(u32 luParam, f32 lfMaxDist, f32* lpfOutDist) const; // @0x82725060
        u32  FindFirstParamAfterPos(u32 luHull, u32 luSectionIndex, f32 lfParamAlong,
                                    f32* lpfOutParamAlong) const;    // @0x82723B80
        void UpdatePressure_Reset();                                 // @0x8272BB88
        void Pressure_PickSplitToTake(const Section* lpSection,
                                      u8* lpuOutSplit,
                                      u16* lpuOutHull,
                                      u8* lpuOutSection) const;      // @0x8272BC68
        void KillParam(u32 luParam);                                 // @0x82721FB8
        void KillAllZombies();                                       // @0x82734DF8
        void KillOutOfAreaTraffic(ActiveHullSet* lpActiveHulls);     // @0x82734C78

        // crash surface -- GATED, declared so the two consumers above link.
        void UpdateParams_BuildListOfCrashingThings(
                ::Array<CrashingThingData, 168u>* lpaOut,
                const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                const CgsContainers::FastBitArray<KU_PARAM_MAX_PARAMS>& lrAvoidSet); // @0x82737270
        void UpdateParams_TryAvoidCrashing(u32 luParam,
                                           const ::Array<CrashingThingData, 168u>* lpaCrashingThings); // @0x82716948
        void UpdateParams_TryStartSympatheticCrashing(u32 luParam,
                                                      const ::Array<CrashingThingData, 168u>* lpaCrashingThings); // @0x827165D8

        // vehicle creation + the kinematic move
        void UpdateVehicles_CreateNewVehicles(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput); // @0x8273A308
        void UpdateVehicles(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                            BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput); // @0x82744F58
        // @0x827185D0 (EXPORT HOLE -- UpdateVehicles calls it by name). DWARF :1716 spells the
        // parameter InputBuffer_PreScene::ActiveRaceCarOutputInterface, which is a typedef for
        // the type below; spelled through the real type because the IO buffers are only
        // forward-declared here.
        void CacheRaceCarState(
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                    lpRaceCars);

        // scene presence
        void GenerateSceneUpdateEvents(BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput); // @0x8273B568
        // @0x82739CD8 (EXPORT HOLE -- UpdateNonDecisionFrame @0x8274C1A8 calls it by name).
        void UpdateLerpedParamTransforms();

        // PARKED, deliberately NOT declared: FindFirstParamOnSection (DWARF :1596, leak
        // BrnTrafficEntityModule.cpp:4959). It has no progress/status.json row and no named
        // ARTIST call site, so nothing attests it exists in the ship image.

        // @0x8272FA30, DWARF :1317. Per-vehicle scene registration: it turns an alive traffic
        // vehicle into an entity the scene manager can hand to the renderer. Body in
        // BrnTrafficEntityModule_wT1_05.cpp; its only caller is PreSceneUpdate's
        // E_STATE_RUNNING arm.
        void CreateNewVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput);

        // =====================================================================================
        // WAVE 4 -- THE COLLISION half of the scene registration, and the overlap-pair half of
        // the promotion chain. Bodies in BrnTrafficEntityModule_wT4_01.cpp / _wT4_02.cpp.
        // =====================================================================================

        // @0x827302C8 (~1030 insns). PreSceneUpdate's E_STATE_RUNNING leg, immediately after
        // CreateNewVehicleEntities. Prologue r3 this, r4 lpInput, r5 lpOutput; asserts
        // "lpInput != NULL" / "lpOutput != NULL" at .cpp 4807/4808. It is the ONLY producer of
        // mVehicleSoaData.mCollidableVehicles and the only caller of AddVolumeInstance /
        // AddForCollision for a traffic vehicle -- without it traffic has no broad-phase
        // presence at all.
        void UpdateCollidableVehicles(const BrnTrafficIO::InputBuffer_PreScene* lpInput,
                                      BrnTrafficIO::OutputBuffer_PreScene* lpOutput);

        // @0x8274B378 (78 insns). PrePhysicsUpdate's E_STATE_RUNNING else-arm, first leg.
        // Asserts .cpp 5550/5551/5552. Walks the scene's RAW overlap-pair list and promotes
        // every traffic half that is not already physical.
        void BuildPotentialCollisionList(const BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
                                         BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                         TotalTrafficBitArray* lpCreatedBodies);

        // @0x82747F58 (~120 insns). BuildPotentialCollisionList's per-half worker.
        // lu64HalfVolumeInstanceId is the console's r5 -- the WHOLE 64-bit OutOverlapPair id
        // whose high dword luHalfEntityWord already is. The body never reads it (r5 is not even
        // saved in the prologue); it is kept so the argument list matches the console's and a
        // later reader is not left wondering which of the six arguments went missing.
        void HandleHalfPotentialContact(u32 luHalfEntityWord,
                                        u64 lu64HalfVolumeInstanceId,
                                        u32 luOtherHalfEntityWord,
                                        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
                                        TotalTrafficBitArray* lpCreatedBodies);

        static void _AssertLayout();

    private:
        // MEMBERS in DWARF/ship order. Every `:NNN` is the DWARF's source line.
        // No member is placed by an X360 byte offset; the offsets noted are attestation only.

        BrnTrafficIO::TrafficToRaceCarInterface_PreScene::NearMissTrafficCollection
                              mNearMissTrafficCollection;        // :599
        BrnTrafficIO::TrafficToRaceCarInterface_PreScene::NearMissRaceCarCollection
                              mNearMissRaceCarCollection;        // :600

        EPrepareStage         mePrepareStage;                    // :602  (X360 +0x2F0)
        EReleaseStage         meReleaseStage;                    // :603  (+0x2F4)
        EResourceAcquireStage meResourceStage;                   // :604  (+0x2F8)
        EEmptyTrafficPoolState meEmptyTrafficPoolState;          // :605  (+0x2FC)

        EState                meState;                           // :607  (+0x300)
        EStartingUpState      meStartingUpState;                 // :608  (+0x304)
        ERunningState         meRunningState;                    // :609  (+0x308)
        ERunningState         meRunningStateToUseAfterStartup;   // :610  (+0x30C)
        ETearingDownState     meTearingDownState;                // :611  (+0x310)

        TrafficReceiverQueue  mReceiverQueue;                    // :613  (+0x314)

        CgsNumeric::Random    mRand;                             // :615
        CgsNumeric::Random    mEffectRand;                       // :616

        // [MEMBER HOLE 5 -- DWARF :619] TrafficJobStub maJobs[4]
        //   BLOCKER (measured): including BrnTrafficJob.h here fails every mounted
        //   traffic TU with C2011 on EA::Thread::{Semaphore,Mutex,Condition,RWMutex}Parameters
        //   -- SDKs/EATech/eathread/BrnEAThreadX360.h redefines the vendor/EAThread snapshot's
        //   types, and it arrives ONLY through BrnTrafficJob.h -> eajobs/job_scheduler.h.
        //   BrnTrafficJob.h's other includes are clean against this header (probed).
        //   FIX (one line, owner GameSource/Jobs/Traffic): move job_scheduler.h out of
        //   BrnTrafficJob.h into BrnTrafficJob.cpp; `extern JobScheduler gJobManager;` needs
        //   only an incomplete type. Insert HERE then, and drop the host split in _wT2_04.cpp.
        u32                   muNumUpdateVehiclesJobs;           // :620

        // ---- the vehicle pools: three pools in one array (X360-attested). Standard is
        // elements [0, KU_MAX_STANDARD_TRAFFIC), static (parked) is
        // [KU_STATIC_TRAFFIC_OFFSET, +KU_MAX_STATIC_TRAFFIC), and the single trailer slot is
        // KU_TRAILER_TRAFFIC_OFFSET. The console accessors differ only by base element index
        // (85 / 485 / 684 at a 128-byte stride), which is 400 / 199 / 1 apart exactly.
        Vehicle               maVehicles[KU_MAX_TOTAL_TRAFFIC];  // :631
        VehicleAxles          maVehicleAxles[KU_MAX_TOTAL_TRAFFIC]; // :632
        Matrix44Affine        maVehicleTransforms[KU_MAX_TOTAL_TRAFFIC]; // :633
        CgsContainers::FastBitArray<KU_MAX_TOTAL_TRAFFIC>
                              mVehiclesAddedToCrashModule;       // :634
        VehicleSoaData        mVehicleSoaData;                   // :635

        Param                 maParams[KU_MAX_PARAMS];           // :637
        // Console bases close with zero slack across all four pool arrays: maParams +165248
        // (GetParam @0x82707630), maParamNeedToSlowData +216448 == 165248 + 400*128
        // (GetParamNeedToSlowData @0x827077D0), maParamListNodes +222848 == 216448 + 400*16
        // (Reset @0x8272CDA0 Constructs 400 of them from there at an 8-byte stride), and
        // maParamTransforms +226048 == 222848 + 400*8 (the base UpdateVehicles @0x82744F58
        // passes as mpaParamTransforms).
        ParamNeedToSlowData   maParamNeedToSlowData[KU_MAX_PARAMS]; // :638
        ParamListNode         maParamListNodes[KU_MAX_PARAMS];   // :639
        ParamTransform        maParamTransforms[KU_MAX_PARAMS];  // :640
        ParamSoaData          mParamSoaData;                     // :641
        ::Array<u16, 1u>      mParamsToReinsert;                 // :642

        ::Array<PurgatoryInfo, KU_MAX_PARAMS>  maPurgatoryList;  // :644
        CgsContainers::Stack<u16, KU_MAX_PARAMS> mFreeParams;    // :645

        StaticTrafficParam    maStaticTrafficParams[KU_MAX_STATIC_TRAFFIC];     // :648
        ::Array<PurgatoryInfo, KU_MAX_STATIC_TRAFFIC> mStaticParamPurgatoryList;// :649
        CgsContainers::Stack<u8, KU_MAX_STATIC_TRAFFIC> mFreeStaticParamStack;  // :650

        ::Array<PurgatoryInfo, KU_MAX_TRAILER_TRAFFIC> mTrailerPurgatoryList;   // :653
        CgsContainers::Stack<u16, KU_MAX_TRAILER_TRAFFIC> mFreeTrailerStack;    // :654

        ::Set<u16, KU_MAX_ACTIVE_HULLS> mActiveHulls;                // :656
        ::Set<u16, KU_MAX_ACTIVE_HULLS> mActiveHullsForLocalPlayer;  // :657
        u8                    mauHullRuntimeDataIndices[KU_MAX_HULLS]; // :658
        // X360 attestation for this run: GetHullRuntimeSafe @0x8271DA70 and GetHullRuntime
        // @0x8271D9D0 both read mauHullRuntimeDataIndices[luHull] at +256816 and index the
        // record array at +257216 with a 1176-byte stride (sizeof(HullRuntime) == 0x498);
        // Reset @0x8272CDA0 Constructs 72 of them from +257216 and clears mUsedHullRuntimeData's
        // two 64-bit fields at +341888/+341896. 257216 + 72*1176 == 341888, so nothing hides
        // between the array and the bit array.
        HullRuntime           maHullRuntimeData[KU_MAX_ACTIVE_HULLS];      // :659
        CgsContainers::BitArray<KU_MAX_ACTIVE_HULLS> mUsedHullRuntimeData; // :660
        TrafficLightManager   mTrafficLightManager;                 // :661  (X360 +0x53790)

        ::Array<u16, KU_MAX_ACTIVE_HULLS> mHullsToAddTriggersFor;    // :663
        ::Array<u16, KU_MAX_ACTIVE_HULLS> mHullsToRemoveTriggersFor; // :664

        ::Array<HullChangeInfo, KU_MAX_HULL_CHANGES> maPredictedHullChanges; // :666
        ::Array<u16, KU_MAX_ACTIVE_HULLS_PER_RACECAR>
                              maaRaceCarHulls[E_ACTIVE_RACE_CAR_INDEX_COUNT];      // :667
        u16                   mau16HullsToActivateAfterReset[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // :668
        u16                   muCurrentlyPredictedHull;             // :669
        bool                  mbNeedToBroadcastHullChange;          // :670
        bool                  mbActivateOnlineHullsAfterReset;      // :671
        HullChangeInfo        mHullChangeToBroadcast;               // :672

        GeneratorAddress      maGenerators[KU_MAX_GENERATORS];              // :674
        f32                   mafTimesTillNextGeneration[KU_MAX_GENERATORS];// :675
        u32                   muNumGenerators;                              // :676

        // Extent 160 on all six: the literal the DWARF prints (Array<...,160u>) and the
        // capacity the committed Array_TrafficCrashInfo_160.cpp instantiation carries. It stays
        // a literal, not KU_MAX_NEW_CRASHED_VEHICLES / KU_MAX_NEW_REMOVED_VEHICLES (both 25),
        // which are the per-frame budgets the producers check, not the array extents.
        // PARKED: nothing names the constant that spells 160. Find it, do not invent one.
        ::Array<TrafficCrashInfo, 160u> maNewCrashedVehicles;        // :678
        ::Array<TrafficCrashInfo, 160u> maEmergencyCrashingVehicles; // :679
        ::Array<u16, 160u> maNewCrashedNetworkVehicles;              // :680
        ::Array<u16, 160u> maRecentlyRemovedVehicles;                // :681
        ::Array<u16, 160u> maNewRemovedVehicles;                     // :682
        ::Array<u16, 160u> maRecentlyRecoveredSlammedTraffic;        // :683

        TrafficPhysicsInfo    maTrafficPhysicsInfoList[KU_MAX_PHYSICAL_TRAFFIC_VEHICLES]; // :685
        CgsContainers::BitArray<KU_MAX_PHYSICAL_TRAFFIC_VEHICLES>
                              maTrafficPhysicsInfoListBits;                 // :686

        VecFloat              mfTrafficSimRadius;                   // :688
        u32                   muMaxVehiclesToRender;                // :689
        f32                   mfRenderCullDistanceSq;               // :690
        bool                  mbInOfflineCarSelect;                 // :691

        Vector3               mLocalPlayerPosition;                 // :693
        Vector3               mLocalPlayerDirection;                // :694
        EActiveRaceCarIndex   meLocalPlayerIndex;                   // :695

        u8                    muFramesSinceDecision;                // :697
        bool                  mbDecisionFrame;                      // :698
        f32                   mfSimTimeSinceLastDecision;           // :699
        f32                   mfSimTimeStep;                        // :700
        f32                   mfSimTimeStepMultiplier;              // :701

        bool                  mbNeedToRunTrafficJamNuker;           // :703

        f32                   mfTrafficLightChangeBackDelay;        // :705
        VecFloat              mfSimTimeStepVec;                     // :706

        // Filled once per UpdateVehicles @0x82744F58 by CacheRaceCarState @0x827185D0, then
        // handed to the vehicle job by const pointer (UpdateVehiclesJobParams).
        RaceCarStateData      mRaceCarState;                     // :708

        Vector3               maEventGridStartPositions[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // :710
        u8                    muNumberOfParticipantsInCurrentEvent;              // :711

        // :713 -- DWARF type BrnTraffic::LightTriggerId, an un-homed SharedClasses handle struct
        // wrapping a 32-bit id (BrnGameModeParams.h models it as `typedef u32 LightTriggerId`).
        // Stored as the raw handle; nothing here dereferences it.
        // FLAG: retype when SharedClasses/Traffic homes the struct.
        u32                   mTrafficLightTriggerId;               // :713
        // :714 -- DWARF type BrnGameState::GameStateModuleIO::EGameModeType, stored as its
        // underlying 32-bit value so this header stays out of the GameState IO include graph.
        // FLAG: same-width retype.
        s32                   meGameMode;                           // :714
        bool                  mbIsOnlineGameMode;                   // :715
        bool                  mbPlayingShowtimeMode;                // :716 (X360 +0x717DD)
        bool                  mbGameModeAllowsSwerving;             // :717
        bool                  mbHardcoreSwerveForMode;              // :718
        bool                  mbGameModeAllowsKillzones;            // :719
        bool                  mbAtStartLineSoProtectRaceCarsFromTraffic; // :720
        bool                  mbEnsureTrafficLightDelay;            // :721
        bool                  mbGameModeClearsTraffic;              // :722
        bool                  mbNeedToSetUpLightsForEventStart;     // :723
        bool                  mbPlayerIsPowerParking;               // :724
        bool                  mbShowtimePlayerOnGround;             // :725
        bool                  mbAllowDivergentBehaviour;            // :726 (X360 +0x717E7)

        // :728/:729 -- DWARF type RoadRulesRecvData::NetworkPlayerID, which has no home in the
        // tree and no attested width, so both are stored as the 64-bit EA online id the rest of
        // the network layer uses. FLAG: width is NOT attested. Retype and re-check the array
        // extent when RoadRulesRecvData lands. Nothing here serialises them.
        u64                   maGameModeNetworkPlayerID[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // :728
        u64                   mNetworkLocalPlayerID;                // :729

        bool                  mbAllVehiclesDead;                    // :731
        u8                    muNumFramesBeforeStateChange;         // :732
        bool                  mbWaitingForStreaming;                // :733
        bool                  mbNeedToKillAllZombies;               // :734

        f32                   mfBaseDensityScale;                   // :736

        f32                   mfGameModeDensityScale;               // :738
        f32                   mfGameModeLargeVehicleProbability;    // :739
        f32                   mfTrafficAmountScale;                 // :740
        s32                   miBigVehicleAmount;                   // :741
        f32                   mfShowtimeTrafficDensityScale;        // :742

        f32                   mfTimeSincePlayerHullChange;          // :744
        f32                   mfTimeSincePlayerWasDrivingQuickly;   // :745
        u32                   muLastParamCalculated;                // :746
        u16                   muPreviousPlayerHull;                 // :747
        f32                   mfTimeSinceLastShowtimeSpawn;         // :748

        u16                   muNumTrafficInsertionsThisFrame;      // :750

        CgsResource::ResourcePtr<TrafficData> mpData;               // :752 (X360 +0x71840)
        Fuzzy::FuzzyBehaviourLogic mFuzzyBehaviours;                     // :753

        u16                   muUpdateCount;                        // :755

        bool                  mbInReplay;                           // :757
        bool                  mbAllowTrafficDeformationSkinning;    // :758

        ::Array<CollidableVehicleInfo4, KU_MAX_COLLIDABLE_CACHED_TRAFFIC_ARRAY>
                              mCachedCollidableList;                // :760
        Vector2               maFeelerCosSin[KI_TRAFFIC_AVOIDANCE_FEELERS_CALC_COUNT]; // :761

        f32                   mfCrashSliderCrashScore;              // :763
        f32                   mfCrashSliderCrashScoreDecay;         // :764
        f32                   mfCrashSliderCrashScoreFactor;        // :765
        f32                   mfCrashSliderFinalValue;              // :766

        ShowtimeVehicleInfo   maShowtimeVehicleInfoList[KU_MAX_SHOWTIME_TRAFFIC_VEHICLES]; // :768
        u32                   muShowtimeVehicleInfoCount;           // :769
        Vector3               mShowtimePlayerLandingPos2D;          // :770
        Vector3               mShowtimePlayerGroundPos;             // :771
        f32                   mfShowtimeTimer;                      // :772
        f32                   mfShowtimeTimeNextCrashSpike;         // :773
        f32                   mfShowtimeTimeLastCrashSpike;         // :774
        f32                   mfShowtimeMisBounceTimer;             // :775

        f32                   mfPlayerIdleTime;                     // :778

        CgsContainers::FastBitArray<KU_MAX_TOTAL_TRAFFIC> mVehiclesToUpdateCollidables;  // :780
        CgsContainers::FastBitArray<KU_MAX_TOTAL_TRAFFIC> mVehiclesAvoidableLastFrame;   // :781

        Vector3               mAveragePhysicalCentre;               // :784
        f32                   mfJunctionFUP;                        // :785 (X360 +0x725E0)
        f32                   mfJunctionFUP_TimeTillNextPhysicalKill; // :786

        bool                  mbTrafficIsHidden;                    // :789

        bool                  mbDontCreateVehiclesNearAnyPlayers;       // :791
        bool                  mbDontCreateStaticVehiclesNearAnyPlayers; // :792

        bool                  mbInPictureParadise;                  // :794

        bool                  mbHullSyncDivergence;                 // :796

        // :799-:821 -- per-object copies of the vectorised tuning constants. Both builds make
        // them members, not file-scope constants, so the debug tools can retune them live.
        // Construct @0x82740220 seeds the values.
        VecFloat              KF_TWO_PI;                                  // :799
        VecFloat              KF_MAX_FLOAT;                               // :800
        VecFloat              KF_APPROX_LANE_WIDTH;                       // :801
        VecFloat              KF_MAX_DIST_ACROSS_LANE;                    // :802
        VecFloat              KF_VEHICLE_STOPLINE_SIDE_SPACE;             // :803
        VecFloat              KF_VEHICLE_STOPLINE_SIDE_VARIATION;         // :804
        VecFloat              KF_VEHICLE_MAX_DIST_FROM_LANE_CENTRE;       // :805
        Vector4               kfVehicle_OptimalDistFromTarget_SpeedBalanceFactor_DirectionDampingFactor_MinDistToMove; // :806
        VecFloat              KF_VEHICLE_MAX_STEERING_DELTA;              // :807
        VecFloat              KF_VEHICLE_SIN_MAX_STEERING_ANGLE;          // :808
        VecFloat              KF_VEHICLE_RECIP_ROLL_SPEED_MIN;            // :809
        VecFloat              KF_VEHICLE_ROLL_FACTOR;                     // :810
        VecFloat              KF_VEHICLE_PITCH_RECIP_MAX_DECEL;           // :811
        VecFloat              KF_VEHICLE_PITCH_DAMPING_FACTOR;            // :812
        VecFloat              KF_VEHICLE_PITCH_SCALE;                     // :813
        Vector4               kfParamSympatheticCone_CosAngle_Length_RecipYScale_W;         // :814
        Vector4               kfParamSympatheticConeShowTime_CosAngle_Length_RecipYScale_W; // :815
        VecFloat              mfVehicleRollFilterTime;                    // :816
        TweakValues           mTweakValues;                               // :817
        Vector4               kfVehicle_AvoidancePassingFactor_Constants;                   // :818
        Vector4               kfVehicle_AvoidanceCone_CosAngle_Length_RecipYScale_W;        // :819
        Vector4               kfVehicle_Avoidance_Constants;                                // :820
        Vector4               kfParamAvoidCrashCone_CosAngle_Length_RecipYScale_W;          // :821

        DebugComponent*       mpDebugComponent;                     // :834
        Logger*               mpLogger;                             // :836

        bool                  mbDEBUGStopTrafficMoving;             // :842
        // :843 -- DWARF type BrnTraffic::AirRams::AirRamType. Its home
        // (BrnTrafficDebugComponent.h) includes this header, so including it back is a cycle.
        // Stored as the enum's underlying 32-bit value.
        // FLAG: same-width retype if the enum ever moves to a leaf header.
        s32                   meDEBUGAirRamToFire;                  // :843
        s32                   miDEBUGOverrideVehicleToSpawn;        // :844
        s32                   miDEBUGOverBudgetness;                // :845
        bool                  mbDEBUGDontRenderMeshes;              // :846
        bool                  mbDEBUGAllowAnarchy;                  // :847
        f32                   mfDEBUGDistanceForAnarchy;            // :848
        f32                   mfDEBUGTrafficLightTimeMultiplier;    // :849
        bool                  mbDEBUGEnableKillzones;               // :850
        s32                   miDEBUGFlowtypeOverride;              // :851

        ::Array<FiredKillZoneInfo, 8u> mDEBUGRecentlyFiredKillZones; // :853

        // This four-bool run is pinned by the fuzzy-logic allocation either side of it: Prepare
        // @0x8274A578 stores the 2560-byte block to +469100 and 0 to +469104, so the
        // pointer/count pair is 469100/469104 and :865/:866 follow at 469108/469109 (0x72875 is
        // the byte NeedToTakeActionAgainstJunctionFUP @0x82707FD0 and UpdateJunctionFUP
        // @0x82745218 read). The four bools fill 469096..469099 below the pointer, the middle
        // one anchored by CalculateAndSetSteeringUsingAvoidance @0x8273D258 reading 0x72869.
        bool                  mbDEBUGEnablePressureSystem;          // :855 (X360 +0x72868)
        bool                  mbDEBUGEnableAvoidance;               // :856 (X360 +0x72869)

        bool                  mbDEBUGTestSympCrash;                 // :858 (X360 +0x7286A)

        bool                  mbDEBUGRenderContacts;                // :860 (X360 +0x7286B)

        DEBUG_VehicleFuzzyLogic* mpaDEBUGVehicleFuzzyLogic;         // :862 (X360 +0x7286C; Prepare stage 4 allocs 40*64)
        u32                   muDEBUGVehicleFuzzyLogicCount;        // :863 (X360 +0x72870)

        bool                  mbDEBUGShowtimeStuff;                 // :865 (X360 +0x72874)
        bool                  mbDEBUGOverrideJunctionFUP;           // :866 (X360 +0x72875)

        bool                  mbDEBUGFakeShowtime;                  // :868 (X360 +0x72876)

        bool                  mbDEBUGPickVehicleFromCamera;         // :870
        u32                   muDEBUGPickedVehicle;                 // :871

        bool                  mbDEBUGPick_StopVehicle;              // :873
        bool                  mbDEBUGPick_DontStopForPickedVehicle; // :874

        bool                  mbDEBUGTurnTrafficOff;                // :877

        f32                   mfSpeedMultiplier;                    // :879

        // :881. X360 +0x72890; the position lane the proximity culls and the job split read
        // at +0x728C0 is this camera's transform Pos row (+0x30). Written by
        // GenerateDispatchLists @0x8273B280 (Camera::operator= @0x8273B2C8).
        BrnDirector::Camera::Camera mCameraLastFrame;                // :881

        bool                  mbDEBUGWorstCase;                     // :883

        DEBUG_VehicleAvoidance* mpaDEBUGVehicleAvoidance;           // :886
        u32                   muDEBUGVehicleAvoidanceCount;         // :887

        f32                   mfDEBUGAvoidance_LineTestResultR;     // :889
        f32                   mfDEBUGAvoidance_LineTestScore;       // :890
        f32                   mfDEBUGAvoidance_PassScore;           // :891

        s32                   miPerfMon_PreSceneUpdate;             // :896
        s32                   miPerfMon_PostSceneUpdate;            // :897
        s32                   miPerfMon_PrePhysicsUpdate;           // :898 (X360 +0x729FC)
        s32                   miPerfMon_Driving;                    // :899
        s32                   miPerfMon_PostPhysicsUpdate;          // :900
        s32                   miPerfMon_PostPhysicsUpdate_Pre0;     // :901
        s32                   miPerfMon_PostPhysicsUpdate_Pre1;     // :902
        s32                   miPerfMon_ProcessDeformation;         // :903
        s32                   miPerfMon_UpdateParam;                // :904
        s32                   miPerfMon_UpdateParam_IncParam;       // :905
        s32                   miPerfMon_UpdateParam_CalcSpeed;      // :906
        s32                   miPerfMon_UpdateParamNonDecision;     // :907
        s32                   miPerfMon_UpdateVehicle;              // :908
        s32                   miPerfMon_PostPhysicsUpdate_Post0;    // :909
        s32                   miPerfMon_PostPhysicsUpdate_Post1;    // :910
        s32                   miPerfMon_RenderCoronas_ActiveHulls;  // :911
        s32                   miPerfMon_RenderCoronas_InactiveHulls;// :912
        s32                   miPerfMon_RenderCoronas_Vehicles;     // :913
        s32                   miPerfMon_UpdateCollidableVehicles;   // :914
        s32                   miPerfMon_UpdateDecision_Part0;       // :915
        s32                   miPerfMon_UpdateDecision_Part1;       // :916

        bool                  mbDEBUGRenderSpecialPerfmons;         // :923

        CgsDev::PerfMonCpuMonitorData mPostPhysPerfmonData_Decision;    // :925
        CgsDev::PerfMonCpuMonitorData mParamPerfmonData_Decision;       // :926
        CgsDev::PerfMonCpuMonitorData mVehiclePerfmonData_Decision;     // :927

        CgsDev::PerfMonCpuMonitorData mPostPhysPerfmonData_NonDecision; // :929
        CgsDev::PerfMonCpuMonitorData mParamPerfmonData_NonDecision;    // :930
        CgsDev::PerfMonCpuMonitorData mVehiclePerfmonData_NonDecision;  // :931

        bool                  mbNetworkHasDetectedDivergence;       // :935

        TrafficCarStreamer    mStreamer;                            // :938
        CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>
                              maTrafficVehiclePhysicsSpecs[KU_MAX_VEHICLE_ASSETS];  // :939
        // X360-ATTESTED BASE: Prepare stage 3 walks &maVehicleTypeRuntime[i].mBBoxHalfSize
        // from this+0x76390 with a 128-byte stride, bounded by mpData->muNumVehicleTypes.
        VehicleTypeRuntime    maVehicleTypeRuntime[KU_MAX_VEHICLE_TYPES];           // :940
        s32                   miResourceRequestCount;               // :941

        const BrnResource::VehicleList* mpVehicleList;              // :943 (see the note
                                                                    //  by the forward decls)
        // X360-ATTESTED: Prepare stage 4 seeds eight 136-byte records from this+0x79388.
        StoredAITrafficData   maStoredAITrafficData[E_ACTIVE_RACE_CAR_INDEX_COUNT];       // :944
    };
}
