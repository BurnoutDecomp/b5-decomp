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
// 600); see BrnTrafficConstants.h. Four members are absent, each marked `[MEMBER HOLE]` at
// its ordered position with the header that must land first.
// =============================================================================

#include "types.hpp"        // u8/u16/u32/s8/s32/u64/f32
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
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficParam.h"            // Param, ParamSoaData, ParamTransform
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficStaticParam.h"      // StaticTrafficParam
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicleTypeRuntime.h"
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

namespace CgsModule { struct IOBufferStack; }
namespace BrnDirector { namespace Camera { class Camera; } }
namespace BrnPhysics { namespace Deformation { class StreamedDeformationSpec; } }
// Pointer-only uses in the render declarations (forward-declaration exception (b)):
// including CgsDispatcher.h / BrnShadowMap.h here would pull the renderer and the
// shadow-cascade tail into every includer, and BrnWorldModule.h includes this header.
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }
namespace BrnResource { struct VehicleList; }   // mpVehicleList (:943) -- by pointer only

namespace BrnTraffic
{
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
    // mDetachedPartQueue is an EventQueue, so it needs Construct() called on it.
    // TrafficPhysicsInfo::Construct(s32) (:214) is where the console does that.
    struct TrafficPhysicsInfo
    {
        // :153 -- the queue instantiation, spelled as the typedef the DWARF names.
        typedef CgsModule::EventQueue<DetachedPartRenderEvent, 20> DetachedPartRenderQueue;

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
        const VehicleTypeRuntime* GetVehicleTypeRuntime(u32 luVehicleType) const; // leak :1655

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
        void* GetParamNeedToSlowData(u32 luParam);                   // @ 0x827077D0 (FLAG: [MEMBER HOLE 1])
        void* Avoidance_CalculateDistancePosVelToOrig(void* lpResult);// @ 0x82708DD0 (FLAG: VMX)
        void  Avoidance_CalculatePassingScore();                     // @ 0x827199B8 (FLAG: VMX)
        void  CalculateAndSetSteeringUsingAvoidance();               // @ 0x8273D258 (FLAG: VMX)
        void  CalculateDriverGasBrake();                             // @ 0x82718CD8 (FLAG)
        void  DEBUG_AddFuzzyLogicData();                             // @ 0x82716040 (FLAG: debug)
        void  DEBUG_RenderContactPoint();                            // @ 0x827082B8 (FLAG: debug)
        u64   GetCarAssetAttribKey(u32 luVehicle);                   // @ 0x8273EFC8 (FLAG: Vehicle interior)
        void  GetDeterministicParamPos(u32 luParam);                 // @ 0x82714258 (FLAG: ParamTransform)
        void  GetSympCrashingTargetPos(u32 luParam, void* lpOut);    // @ 0x82708C10 (FLAG)
        void  GetTrafficPhysicsInfoForVehicl();                      // @ 0x82714500 (FLAG)
        void  HideAllTraffic();                                      // @ 0x8273F418 (FLAG)
        void  UnhideAllTraffic();                                    // @ 0x8274A500 (FLAG)
        // Body in BrnTrafficEntityModule_wT1_05.cpp, beside its caller CreateNewVehicleEntities.
        // FLAG: DWARF :1323 spells it `bool IsVehiclesParamAZombie(uint32_t) const`. The
        // trailing const is dropped because the three accessors it calls (GetParam / GetVehicle
        // / GetStaticTrafficParamFro) are non-const here; add it once those gain const
        // overloads. The body only reads, so it is const-correct in substance.
        bool  IsVehiclesParamAZombie(u32 luVehicle);                 // @ 0x82715D70 (DWARF :1323)
        void  JunctionFUP_StopOffscreenTraffic(void* lpData, bool lbFlag); // @ 0x82719868 (FLAG)
        void  JunctionFUP_TryClearupNonMovingPhysical();             // @ 0x8273F2E8 (FLAG)
        void  KillDyingVehicleEntities();                            // @ 0x82741E40 (FLAG)
        void  PutParamInPurgatory(u32 luParam);                      // @ 0x82716510 (FLAG: Array interior)
        void  RebuildGeneratorList();                                // @ 0x82742DD0 (FLAG)
        void  UpdateNormalPhysical(u32 luIndex, void* lpDriverControls); // @ 0x8273EF08 (FLAG)
        void  UpdateParams_CalcDesiredSpeed();                       // @ 0x82717928 (FLAG: VMX)
        void  UpdateParams_TryAvoidCrashing();                       // @ 0x82716948 (FLAG: VMX)
        void  UpdateParams_TryToReinsertParam();                     // @ 0x827247F0 (FLAG)
        void  UpdateSerialiser();                                    // @ 0x8272DA80 (FLAG)
        // FLAG (unrecoverable rodata): the stuck-side timer floor constant flt_82001CC0 (the
        // fsel else-value at 0x82707FB0) is not attested in the dossier pseudocode, so the
        // timer clamp cannot be written without fabricating it. Declaration-only until it is.
        void  UpdateVehicleStuckSideTime(s32 liFlags, s32 liMask, f32 lfReset,
                                         f32 lfThreshold, f32* lpfTimer);   // @ 0x82707F00 (FLAG)
        void  UpdateVehicleStuckTimers(void* lpPhysicsInfo, f32 lfReset, f32 lfThreshold); // @ 0x82708D48 (FLAG)

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

        // @0x8274E508, DWARF :1476. The per-decision-frame call set the console runs in
        // E_STATE_RUNNING. Body in _wT1_06.cpp, PARTIAL: the driving-traffic legs are named
        // gates, the parked ladder is real.
        void UpdateDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                 BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8274C1A8, DWARF :1479. The cheap frames between decision frames. Body in
        // _wT1_06.cpp, PARTIAL for the same reason.
        void UpdateNonDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8272FA30, DWARF :1317. Per-vehicle scene registration: it turns an alive traffic
        // vehicle into an entity the scene manager can hand to the renderer. Body in
        // BrnTrafficEntityModule_wT1_05.cpp; its only caller is PreSceneUpdate's
        // E_STATE_RUNNING arm.
        void CreateNewVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput);

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
        //   BLOCKER: BrnTraffic::TrafficJobStub (home GameSource/Jobs/Traffic/BrnTrafficJob.h)
        //   embeds EA::Jobs::Job by value, which would drag the whole EATech eajobs SDK into
        //   this header's include graph, and BrnWorldModule.h includes this header. The type
        //   is fully declared; this is an include-graph choice. Insert HERE when a job wave
        //   needs it.
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
        // [MEMBER HOLE 1 -- DWARF :638] ParamNeedToSlowData maParamNeedToSlowData[400]
        //   BLOCKER: BrnTraffic::ParamNeedToSlowData's DWARF home is BrnTrafficParam.h, which
        //   does not declare it. X360-attested shape: GetParamNeedToSlowData @0x827077D0
        //   returns `16 * (luParam + 13528) + this`, so the record is 16 bytes and the array is
        //   KU_MAX_PARAMS long. Add the struct to BrnTrafficParam.h, insert the member HERE,
        //   then body GetParamNeedToSlowData.
        // [MEMBER HOLE 2 -- DWARF :639] ParamListNode maParamListNodes[400]
        //   BLOCKER: same, BrnTraffic::ParamListNode is undeclared in BrnTrafficParam.h. It is
        //   the doubly-linked "params in this section, in order" node the
        //   UpdateParams_UpdateLinkedList pipeline walks. Insert HERE.
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

        // [MEMBER HOLE 4 -- DWARF :708] RaceCarStateData mRaceCarState
        //   BLOCKER: BrnTraffic::RaceCarStateData's DWARF home,
        //   TrafficEntityModule/BrnTrafficRaceCarCache.h, does not exist in the tree. It is the
        //   per-frame cache of the active race cars the traffic sim reacts to (>= 928 bytes on
        //   the console: Prepare stage 0 zeroes an 8-byte field at this+0x713A0, inside it).
        //   Reconstruct that header, then insert the member HERE.

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

        // [MEMBER HOLE 6 -- DWARF :881] Camera mCameraLastFrame
        //   BLOCKER: DWARF type `Camera` is BrnDirector::Camera::Camera, whose header would
        //   pull the director camera graph into BrnWorldModule.h's includes. Its only consumer
        //   is the mbDEBUGPickVehicleFromCamera tool. Insert HERE when that tool lands.

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
