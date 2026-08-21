#pragma once

// =============================================================================
// BrnTrafficEntityModule.h -- KEYSTONE owning header for BrnTraffic::TrafficEntityModule
// and the small per-frame record types the module keeps in fixed-capacity containers.
//
// -----------------------------------------------------------------------------
// 2026-08-21 (wave T1, cluster C1): THE OPAQUE BLOB IS GONE.
//
// This class used to be modelled as `u8 mOpaque[0x73000]` with every method reaching it
// through raw X360 BYTE offsets. That was wrong twice over:
//   * the blob was TOO SMALL -- Prepare @0x8274A578 stage 3 walks `this + 0x76390`
//     (484,240) and stage 4 seeds per-race-car scratch at `this + 0x79388` (496,520),
//     both past 0x73000 == 471,040, so running either stage was an out-of-bounds write
//     (which is exactly why BrnTrafficEntityModule_wQ7_02.cpp had to gate them); and
//   * it sized a HOST object from X360 byte offsets, the tree's #1 recurring defect
//     (host pointers are 8 bytes, so the real host object is larger still).
//
// It is replaced by the REAL ordered member list. Two independent sources agree on that
// order and it is confirmed against the X360 assembly:
//   * DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/World/EntityModules/
//     TrafficEntityModule/BrnTrafficEntityModule.h) carries the full ordered member list
//     with the source line of every member (cited below as `:NNN`);
//   * the Feb-2007 leak carries the same list plus the inline accessors that name what
//     each pool is for.
// X360 ATTESTATION that the DWARF order IS the ship order (three independent anchors):
//   * head:  mePrepareStage @+0x2F0 / meReleaseStage @+0x2F4 / meResourceStage @+0x2F8 /
//     meEmptyTrafficPoolState @+0x2FC / meState @+0x300 / meStartingUpState @+0x304 /
//     meRunningState @+0x308 / meRunningStateToUseAfterStartup @+0x30C /
//     meTearingDownState @+0x310 / mReceiverQueue @+0x314 -- ten consecutive DWARF
//     members (:602-:613) landing on ten consecutive attested offsets.
//   * middle: the eleven consecutive bools :716..:726 land on the eleven consecutive
//     bytes 0x717DD..0x717E7 (mbPlayingShowtimeMode is the wQ7_01 SetPlayingShowtime
//     read @0x717DD; mbAllowDivergentBehaviour is the @0x717E7 gate that
//     ShouldBeHollywoodAction @0x827075C8 and NeedToTakeActionAgainstJunctionFUP
//     @0x82707FD0 both test). No ship-added member hides in that run.
//   * tail:   maVehicleTypeRuntime[96] (:940) is at +0x76380 -- Prepare stage 3 walks
//     `this + 0x76390` == &maVehicleTypeRuntime[0].mBBoxHalfSize (element base + 16) with
//     a 128-byte stride; 0x76380 + 96*128 == 0x79380, leaving exactly
//     miResourceRequestCount (:941) @0x79380 + mpVehicleList (:943) @0x79384 before
//     maStoredAITrafficData (:944) @0x79388, which is where Prepare stage 4 seeds eight
//     136-byte records. Three members, three offsets, zero slack.
//
// HOST-NATIVE LAYOUT, NOT X360 BYTES. Nothing here strides or offsets by an X360 byte
// value, and the _AssertLayout pins are RELATIVE ORDER + asm-attested ARRAY COUNTS only.
// Sub-aggregate sizes are allowed to differ from the console (they do: pointers widen, and
// several sub-aggregates are still only partially recovered) because every access is by
// member NAME. That is the whole point of the change.
//
// SHIP vs DecFIGS constant deltas that shape this layout (see BrnTrafficConstants.h for the
// full attestation): KU_MAX_STATIC_TRAFFIC is 199 on ship (DWARF says 200) and
// KU_MAX_TOTAL_TRAFFIC is 600 (DWARF says 601) -- both proven by the pool accessors'
// element-index arithmetic in the X360 asm.
//
// MEMBER HOLES. Four members are NOT modelled here because their type's owning header is
// not owned by this cluster and does not exist yet. Each is marked `[MEMBER HOLE]` at its
// exact ordered position with the header that must land first. Inserting them later is a
// pure insertion -- no other member moves in any way that matters, because nothing offsets.
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
// ⭐ 2026-08-21 (wave T1, cluster C4): closes [MEMBER HOLE 3]. C3 promoted HullRuntime out of
// BrnTrafficHullRuntime.cpp into this owning header and handed the insertion to C4 in its
// report ("HullRuntime -> TrafficEntityModule: HEADER READY").
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficHullRuntime.h"      // HullRuntime
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficLightManager.h"     // TrafficLightManager
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficCarStreamer.h"      // TrafficCarStreamer
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficTweakConstants.h"   // TweakValues
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficFuzzyLogicBehaviours.h" // FuzzyBehaviourLogic
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficToRaceCarInterface.h" // NearMiss collections
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficAIInterfaces.h"       // BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR

// ⭐ 2026-08-21 (wave T1 round 2, cluster R2B): the render trio's parameter types.
// BrnBlobbyShadowManager::BrnBlobbyShadowBuffer is a NESTED class, so it cannot be
// forward-declared -- its real header comes in, exactly as the sibling
// BrnTrafficEntityModuleIO.h:62 already does. It is a CHEAP include (its own only
// includes are <cstddef>, types.hpp and BrnCommonTypes.h), and it includes nothing
// that reaches back here, so it adds no cycle and no measurable graph weight.
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"  // BrnBlobbyShadowManager::BrnBlobbyShadowBuffer

namespace CgsModule { struct IOBufferStack; }
namespace BrnDirector { namespace Camera { class Camera; } }
namespace BrnPhysics { namespace Deformation { class StreamedDeformationSpec; } }
// Pointer-only uses in the render declarations (AGENTS.md forward-declaration exception
// (b)): including CgsDispatcher.h / BrnShadowMap.h here would pull the whole renderer and
// the shadow-cascade tail into every includer of this keystone header -- and this header is
// reached from BrnWorldModule.h, i.e. from most of the world build. The spellings
// (class / struct) match the canonical definitions and BrnTrafficEntityModuleIO.h:70-71.
namespace CgsGraphics { class DispatchFrame; }
namespace BrnWorld { struct ShadowMap; }
namespace BrnResource { struct VehicleList; }   // mpVehicleList (:943) -- by pointer only

namespace BrnTraffic
{
    class TrafficData;
    // NOTE (2026-08-21, wave T1 round 3): BrnTraffic::VehicleList was a MIS-SCOPED
    // forward declaration. The DWARF spells mpVehicleList's type unqualified
    // (`const VehicleList * mpVehicleList`, :943) and this header read that as a
    // BrnTraffic type; the console proves otherwise --
    // FindVehicleTypeAttribKey_EXPENSIVE @0x8273F0B8 calls
    // BrnResource::VehicleList::GetVehicleIndex / ::GetVehicleData on it, and there is
    // no BrnTraffic::VehicleList anywhere in the image. The member is retyped below to
    // BrnResource::VehicleList (real home SharedClasses/DataLists/VehicleList.h,
    // already reconstructed); it was pointer-only and unread, so nothing is orphaned.
    class DebugComponent;
    class Logger;

namespace BrnTrafficIO { class InputBuffer_PrePhysics; class OutputBuffer_PrePhysics; class InputBuffer_PostScene; class OutputBuffer_PostScene; class InputBuffer_Dispatch; class InputBuffer_PreDispatch; class OutputBuffer_PreDispatch; }

namespace BrnTrafficIO { class OutputBuffer_Prepare; }
// ADDITIVE (world-drive wave: WorldModule::EntityModulePreSceneUpdate @0x827BD1F0
// and EntityModulePostPhysicsUpdate @0x827D3F10 name these IO buffers).
namespace BrnTrafficIO { class InputBuffer_PreScene; class OutputBuffer_PreScene;
                         class InputBuffer_PostPhysics; class OutputBuffer_PostPhysics; }

    // =====================================================================================
    // The module's small record types. DWARF home for every one of them is THIS header
    // (the `:NNN` on each is its BrnTrafficEntityModule.h source line in the DWARF).
    // =====================================================================================

    // :105 -- where a traffic generator lives (a hull + a section on that hull).
    struct GeneratorAddress
    {
        u16 muHull;    // :107
        u8  muSection; // :108
    };

    // :121 -- one pending traffic-crash record. sizeof == 16 on the console
    // (Array<TrafficCrashInfo,160> count word sits at +0xA00 == 160*16, and the per-element
    // copy moves 4 dwords / a 16-byte stride) and 16 here too (pointer-free record).
    //
    // meCrashTrafficType is BrnPhysics::Vehicle::eCrashTrafficType
    // (BrnTrafficPhysicsConstants.h:32). That enum's home is not reconstructed in this
    // slice, so the field is stored here as its underlying 32-bit value (the X360 packs
    // the enum in a 4-byte slot); the enumerator names are documented for reference:
    //   Standard=0, Checked=1, Spontaneous=2, Slammed=3, Invalid=255.
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
    // on the console (Array<FiredKillZoneInfo,8> count word sits at +0x80 == 8*16, and the
    // per-element copy moves two qwords / a 16-byte stride). mKillZoneId is
    // TrafficData::KillZoneId == uint64_t (BrnTrafficData.h:40).
    struct FiredKillZoneInfo
    {
        u64 mKillZoneId;            // :241
        s32 miFramesLeftToRemember; // :242
    };

    // :255 -- the per-race-car snapshot of "which traffic entities are near this car" that
    // the AI module reads. X360-ATTESTED: Prepare @0x8274A578 stage 4 walks eight of these
    // with a 136-byte stride from `this + 0x79388`, writing `meRaceCarIndex = i` at +0 and
    // `miNumTrafficIDs = 0` at +4 -- 4 + 4 + 32*4 == 136 exactly, which is the X360
    // attestation of BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR == 32 and of the record being
    // pointer-free.
    //
    // ⭐ CORRECTED 2026-08-21 (C1 fix round): the extent constant is NOT redefined here. Its
    // canonical home is SharedIO/BrnTrafficAIInterfaces.h (`BrnTraffic::BrnTrafficIO::
    // KI_MAX_TRAFFIC_NEAR_A_RACECAR`, an s32 as the KI_ prefix requires), which is also the
    // spelling the leaked BrnTrafficEntityModule.h uses at this very member. A struct-local
    // `static const u32` copy was a fork of a type/constant that already has a reconstructable
    // home -- the thing AGENTS.md forbids.
    struct StoredAITrafficData
    {
        EActiveRaceCarIndex meRaceCarIndex;                                 // :257
        s32                 miNumTrafficIDs;                                // :258
        EntityId            maTrafficEntityIDs[BrnTrafficIO::KI_MAX_TRAFFIC_NEAR_A_RACECAR]; // :259
    };

    // DWARF home BrnTrafficMiscRuntimeClasses.h:94 -- one purgatory-list record: a vehicle
    // index plus a countdown of decision frames left. sizeof == 4 (X360-attested: the
    // Array<PurgatoryInfo,N> instantiations put their live-count word at byte N*4 and copy
    // each element as two halfwords -- Array<PurgatoryInfo,1>::Append @ 0x8270AAC0 stores
    // `sth muIndex@+0` then `sth muDecisionFramesLeft@+2`, count word @ +0x4 == 1*4;
    // Array<PurgatoryInfo,400> count word @ +0x640 == 400*4 (Erase @ 0x8270A770);
    // Array<PurgatoryInfo,1>::GetItem @ 0x8270CA28 returns 4*index + base). Homed here with
    // the module's other small record types; grow this header (never redefine) when the
    // BrnTrafficMiscRuntimeClasses slice lands.
    struct PurgatoryInfo
    {
        u16 muIndex;              // :96
        u16 muDecisionFramesLeft; // :97
    };

    // DWARF home BrnTrafficVehicle.h:159 -- one per-frame vehicle-render record (the dispatch
    // list the module hands the renderer). sizeof == 12 (X360-attested:
    // Array<VehicleRenderInfo,64>::Append @ 0x8270A148 copies three dwords (`stw` x3) at a
    // 12-byte stride (index*12 == `slwi r,1; add; slwi r,2`), count word @ +0x300 == 64*12;
    // ::Get @ 0x827BA2A0 returns 12*index + base). mLOD is CgsGraphics::Model::State (a
    // 4-byte enum, committed in CgsModel.h).
    //
    // DO NOT REORDER: pinned by static_asserts in BrnWorldModule.cpp (mfDistanceSq @4,
    // mLOD @8) and in BrnActiveRaceCarRenderParams.cpp.
    struct VehicleRenderInfo
    {
        u32                      muEntityIndex; // :161
        f32                      mfDistanceSq;  // :162
        CgsGraphics::Model::State mLOD;         // :163
    };

    // :164 (Feb-2007 leak, same shape) -- one predicted race-car-hull change record: the
    // module keeps a list of pending hull (collision-shape) swaps to apply to nearby race
    // cars (producers: AddPredictedHullChange / UpdateRaceCarHulls; consumer/debug dump
    // DEBUGDumpHullPredictions). sizeof == 8 (X360-attested): the Array<HullChangeInfo,400>
    // instantiations put their live-count word at byte N*8 and copy each element as two
    // dwords -- Append @ 0x8270ACA8 stores `stw r10,0(slot)` then `stw r10,4(slot)`, count
    // word @ +0xC80 == 400*8; EraseFast @ 0x8270ADD0 overwrites slot[index] with the last
    // live element via a two-dword copy at an 8-byte stride; GetItem @ 0x8270CE00 returns
    // 8*index + base.
    //
    // ⭐ 2026-08-21: the interior is NO LONGER opaque. The Feb-2007 leak spells it
    // (BrnTrafficEntityModule.h:164) as the three fields below, which is exactly the
    // 4+2+2 == 8-byte footprint the Array bodies move as two dwords. The previous
    // `muWord0/muWord1` FLAG is retired.
    struct HullChangeInfo
    {
        EActiveRaceCarIndex meActiveRaceCarIndex; // leak :167
        u16                 muNewActiveHull;      // leak :168
        u16                 muUpdateFrame;        // leak :169
    };

    // :272 -- one "crashing thing" the module tracks while building the per-frame list of
    // things nearby traffic should react to (producers/consumers:
    // UpdateParams_BuildListOfCrashingThings / _TryStartSympatheticCrashing /
    // _TryAvoidCrashing, all operating on Array<CrashingThingData,168>). sizeof == 32
    // (X360-attested: Array<CrashingThingData,168>::operator[] @ 0x8270BE68 returns
    // 32*index + base -- `slwi r,index,5` -- and reads the live-count word at byte +0x1500 ==
    // 168*32). The 32-byte footprint is the 16-byte/16-aligned Vector3 (mPosition) followed by
    // the 4-byte EntityId and the bool, rounded up to the Vector3's 16-byte alignment.
    struct CrashingThingData
    {
        Vector3  mPosition;            // :275
        EntityId mEntityId;            // :276
        bool     mbShowtimeCrashMagnet;// :278
    };

    // NB: BrnTraffic::PhysicalVehicleInfo (the Array<PhysicalVehicleInfo,33> element) is NOT
    // homed here -- it has its own committed home BrnTrafficPhysicalVehicleInfo.h; do not
    // redefine it (ODR).

    // :293 -- a SIMD "structure-of-arrays" packet holding four collidable vehicles at once
    // (the trailing "4" in the type name = four vehicles per record), cached in
    // mCachedCollidableList (Array<CollidableVehicleInfo4,16> -> up to 64 vehicles ==
    // KU_MAX_COLLIDABLE_CACHED_TRAFFIC). Each Vector4 lane holds the same field for all four
    // vehicles. sizeof == 128 (X360-attested: Array<CollidableVehicleInfo4,16>::operator[]
    // @ 0x8270D260 returns (index<<7) + base and reads the live-count word at byte +0x800 ==
    // 16*128). The 128-byte footprint is exactly eight 16-byte/16-aligned Vector4 registers.
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

    // :350 -- the per-vehicle fuzzy-logic debug snapshot. X360-ATTESTED SIZE: Prepare
    // @0x8274A578 stage 4 allocates 2560 bytes for KU_DEBUG_MAX_FUZZY_LOGIC == 40 of these,
    // i.e. exactly 64 bytes each -- which is what u32 + (16-aligned) Vector3 + f32[6] is.
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
    // ⚠️ RECURRING-BUG WATCH (class (a), never-Constructed queues): mDetachedPartQueue is an
    // EventQueue and MUST have its Construct() called by whoever constructs this record --
    // TrafficPhysicsInfo::Construct(s32) is the DWARF-attested place (:214). The module's
    // Construct @0x82740220 is the owner of that call. Nothing in this cluster constructs it;
    // C4 must emit it.
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
        // :179 -- BrnPhysics::Deformation::ETagPointType[24]. Stored as the enum's
        // underlying 32-bit value: the enum's owning header
        // (GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h)
        // is not included here to keep this keystone header out of the deformation include
        // graph, and the array is never read by any code this cluster lands. FLAG: retype to
        // the real enum when a consumer needs it -- the width does not change.
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

        // ⭐⭐ SHIP-ONLY TAIL, LANDED 2026-08-21 (wave T1 round 3, closure item 4). The
        // DecFIGS DWARF's TrafficPhysicsInfo ENDS at muContactSideFlags :204 -- these two are
        // a merge-window addition ARTIST has and DecFIGS does not (exactly the delta
        // AGENTS.md's BUILD LINEAGE section predicts), so they carry an X360 attestation
        // instead of a DWARF line number:
        //   * TrafficPhysicsInfo::Construct @0x82751E88 closes with `sth r4, 0x100A(r3)` --
        //     it stores its own `s32 liIndex` argument there as a HALFWORD;
        //   * TrafficPhysicsInfo::Destruct @0x82751EE8 is `li r11,-1 ; sth r11, 0x100A(r3)`
        //     and NOTHING ELSE -- the whole teardown of the record is "mark the slot unowned",
        //     which makes 0xFFFF the sentinel and a u16 the width;
        //   * the caller asserts the argument `< KU_MAX_TOTAL_TRAFFIC` (600) before passing
        //     it, which fits a u16 with 0xFFFF free as the sentinel;
        //   * TrafficEntityModule::HandleExternalResponses @0x82732C68 is the reader.
        // muPad205 is the console's own alignment byte at +0x1009 (muContactSideFlags is at
        // +0x1008 and the halfword store is 2-aligned at +0x100A). It is modelled explicitly
        // rather than left to the compiler so the ORDER of the two ship members is pinned by
        // the source, not by a host padding rule.
        //
        // HOST-NATIVE: nothing here offsets by an X360 byte -- the console offsets above are
        // provenance for WHICH member exists and in what order, and every access is by name.
        u8  muPad205;                 // X360 +0x1009 (alignment; no DWARF)
        u16 muOwningVehicleIndex;     // X360 +0x100A -- SHIP-ONLY, no DWARF

        // The "no owner" sentinel Destruct writes (`li r11,-1` truncated to the halfword).
        static const u16 KU16_NO_OWNING_VEHICLE = 0xFFFFu;

        void Construct(s32 liIndex); // :214
        void Destruct();             // :218
        bool IsStuckFront() const;   // :221
        bool IsStuckBack() const;    // :224
    };

    // =========================================================================
    // BrnTraffic::TrafficEntityModule (DWARF :400) -- the TRAFFIC entity module.
    //
    // Base class is DWARF-attested (`: public CgsModule::ModuleSingleBuffered`) AND
    // asm-attested: Prepare @0x8274A578 stage 1 calls
    // `CgsModule::ModuleSingleBuffered::Prepare(a1)` with `this` UNADJUSTED, so the base
    // sub-object sits at offset 0. Sibling BrnWorld::PropEntityModule already models the
    // same base the same way.
    // =========================================================================
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

        // =====================================================================================
        // ---- THE RENDER TRIO (WorldModule::GenerateDispatchLists @0x827D1CE8) ----
        // ⭐ 2026-08-21 (wave T1 round 2, cluster R2B). Bodies land in
        // BrnTrafficEntityModule_Render.cpp. Every signature below is the DecFIGS DWARF's
        // verbatim parameter list (dwarfdump/_compile/BrnTrafficUnity.cpp :19315 / :23926 /
        // :23103), each cross-checked against the X360 call site and callee prologue.
        // =====================================================================================

        // @0x8274D900 (EXPORT HOLE -- no .ida-exports/.../0x8274D900.json). DWARF :19315.
        // Visible-id walk -> IsAlive skip -> distance cull at mfRenderCullDistanceSq ->
        // near-far sort -> cap at muMaxVehiclesToRender. Takes the BUFFER (X360
        // @0x827D249C `mr r5, r19`), unlike GenerateDispatchLists below.
        void PreDispatchUpdate( const BrnTrafficIO::InputBuffer_PreDispatch* lpInput,
                                BrnTrafficIO::OutputBuffer_PreDispatch* lpOutput );

        // @0x8273B280. DWARF :23926, verbatim:
        //   GenerateDispatchLists( const InputBuffer_Dispatch*,
        //                          const Array<BrnTraffic::VehicleRenderInfo,64u>& laTrafficRenderInfos,
        //                          Vector4 lFogScattering, Vector4 lFogColourPlusWhiteLevel,
        //                          Vector3 lCameraPosition, Vector3 lCameraDirection,
        //                          int32_t liModelOnlyDisplayList, int32_t liOpaqueList,
        //                          int32_t liTransparentList, const Camera& lBrnCamera )
        //
        // ⭐ THE COMMITTED 6-ARG DECLARATION WAS WRONG IN TWO WAYS; both are fixed here in the
        // same change that lands the body and retires the WorldLinkStubs gate (they cannot move
        // apart -- the gate defines the symbol out-of-line, so a header-only edit is C2511):
        //   (1) ARG 2 IS THE ARRAY, NOT THE BUFFER. X360 `addi r22, r19, 4` @0x827D24B0 with
        //       r19 == the OutputBuffer_PreDispatch, `mr r5, r22` @0x827D2814 into the call
        //       @0x827D2824; the callee's FIRST use proves the type -- `mr r26, r5` @0x8273B2B8,
        //       `mr r3, r26` @0x8273B324, `bl Array<VehicleRenderInfo,64>::GetLength`
        //       @0x8273B328. On the host the two addresses genuinely differ (IOBuffer leads with
        //       a 1-byte FlagSet8), so a body written against the buffer would read miCount out
        //       of the status byte.
        //   (2) FOUR VECTOR ARGUMENTS WERE MISSING. The console call site loads v1..v4
        //       (`vmr128 v4,v114` @0x827D27F8, `v3,v127` @0x827D2800, `v2,v115` @0x827D2808,
        //       `v1,v116` @0x827D2810), identified by comparing the same registers at the
        //       race-car site @0x827D27A0 and the world site @0x827D28BC. The callee re-homes
        //       them at entry as v124=lFogScattering, v123=lFogColourPlusWhiteLevel,
        //       v127=lCameraPosition, v126=lCameraDirection and forwards (position, direction)
        //       to both corona producers and (position, scattering, colour) to RenderTrafficCar.
        // The Camera is a REFERENCE per the DWARF (the old declaration said pointer).
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

        // @0x82728B08 (3,252 pseudocode lines -- the module's single biggest function).
        // DWARF :23103, verbatim parameter names and order. The per-vehicle vector pair the
        // DWARF calls lFrontLights / lRearLights is the OUT-pair SubmitCoronasForVehicle
        // (DWARF :22445, wave 2) writes and this function CONSUMES -- they are not a
        // corona-only side channel. lpiUpdatedNumDamagedVehiclesRendered is the shared
        // per-frame budget GenerateDispatchLists seeds to 0 once for the whole loop
        // (`*a52 < 5` at pseudocode :1254).
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

        // ⭐ PARK RETIRED 2026-08-21 (R2B): "module member at +0x71870 (465008) is absent from
        // C1's layout". THERE IS NO MISSING MEMBER. GenerateDispatchLists' unconditional
        // `stvx128 v127, r29, r11` (r11 == 0x71870) @0x8273B2DC is the INLINED body of
        // `mFuzzyBehaviours.DEBUGSetLastCameraPos( lCameraPosition )`:
        //   mpData      is at X360 +0x71840 and a console CgsResource::ResourcePtr is 32 bytes
        //               (this header's own maTrafficVehiclePhysicsSpecs note: "32-byte X360
        //               stride"), so mpData spans 0x71840..0x71860;
        //   mFuzzyBehaviours (:753) therefore starts at 0x71860, and
        //   FuzzyBehaviourLogic::mDEBUGLastCameraPos is at its +0x10
        //   (BrnTrafficFuzzyLogicBehaviours.h, offset-annotated there) == 0x71870 exactly,
        //   16-aligned and 16 bytes wide, which is what the store is.
        // No member is added here; the body names the call instead. (The same arithmetic makes
        // the sibling unnamed word at +0x72A38 a genuine ship-only PerfMonCpu id -- it is far
        // past the DWARF's perfmon block -- and it stays parked, not invented.)

        // @0x8274A578 (252 insns). PARTIAL body in BrnTrafficEntityModule_wQ7_02.cpp
        // (wave Q7): the six-stage ladder is real; stages 1/3/4 are NAMED one-shot gates.
        // DWARF :1079 declares it `virtual`; kept non-virtual here because the base's
        // `virtual bool Prepare()` takes no argument -- this overload HIDES rather than
        // overrides it, and making it virtual would add a slot the console does not have
        // at this position. FLAG for the wave that reconstructs the module vtable.
        bool Prepare( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer );

        // @0x82746A88 (465 insns), DWARF :1266 `bool LoadData(OutputBuffer_Prepare*)`.
        // The module's resource-acquire ladder, driven by Prepare stage 2. PARTIAL body in
        // BrnTrafficEntityModule_wQ7_02.cpp.
        bool LoadData( BrnTrafficIO::OutputBuffer_Prepare* lpOutputBuffer );

        // @0x82720A90 (118 insns), DWARF :1443. Drains the two prop->traffic rings
        // (traffic-light knock-downs / restores) that
        // WorldModule::BridgePropModuleToTrafficModule_PrePhysics @0x827AEA70 copied into the
        // pre-physics input buffer, and applies each to mTrafficLightManager. REAL body in
        // BrnTrafficEntityModule_wQ7_01.cpp.
        void HandlePropModuleRequests( const BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
                                       BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput );

        // ---------------------------------------------------------------------
        // Pool accessors. ⭐ 2026-08-21: these used to return `void*` computed from X360 byte
        // offsets; they now index the real named pools and return the DWARF's return types.
        // The X360 element-index arithmetic is preserved as the attestation comment on each.
        // ---------------------------------------------------------------------
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

        // *** DECLARED 2026-08-21 (wave T1 round 3, closure item 3). X360 @0x8273F0B8, body in
        // BrnTrafficEntityModule_wQ7_02.cpp beside its ONE caller, LoadData stage 7.
        // Round 2 recorded it under "recurring bug class (d)" as "not declared at all -- its
        // ledger row's primary_file is the CgsStrStream.h catch-all"; that misattribution is
        // what kept it off this class for two rounds. It IS a TrafficEntityModule member: its
        // pseudocode reaches `this + 464960` (mpData) and `this + 496516` (mpVehicleList), and
        // its three baked assert strings all cite BrnTrafficEntityModule.cpp (:17180 / :17181 /
        // :17185).
        //
        // RETURN WIDTH: 64 bits, NOT Attribute::Key (which is u32 tree-wide). The function
        // tail-returns CgsAttribSys::AttribSysCollectionKey::GetHashKey, which this tree
        // already models as u64 (widened 2026-08-01 -- "a 32-bit key can never match"), and
        // VehicleTypeRuntime::Prepare stores the whole register with `std`. Spelled through
        // VehicleTypeRuntime's own AttribKey typedef so producer and consumer cannot drift.
        //
        // _EXPENSIVE is the console's own name: it does a linear VehicleList::GetVehicleIndex
        // scan per vehicle type, which is why LoadData calls it exactly once per type at load
        // time and never per frame.
        VehicleTypeRuntime::AttribKey FindVehicleTypeAttribKey_EXPENSIVE(u32 luVehicleType) const;

        bool  IsPaused();                                            // @ 0x82707560
        bool  ShouldBeHollywoodAction();                             // @ 0x827075C8
        bool  NeedToTakeActionAgainstJunctionFUP();                  // @ 0x82707FD0
        void  EnterReplay();                                         // @ 0x827081D8
        void  LeaveReplay();                                         // @ 0x82708248
        void  RestartTraffic();                                      // @ 0x82708F98

        // ---------------------------------------------------------------------
        // DECLARATION-ONLY + FLAGGED. These reach sub-aggregate interiors that are still
        // only partially recovered (Vehicle / Param tails, the VMX avoidance pipeline, the
        // un-homed HullRuntime), so they keep their declarations and land with the clusters
        // that own those types.
        // ---------------------------------------------------------------------
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
        // ⭐ BODIED 2026-08-21 (wave T1 round 4, item 2) in BrnTrafficEntityModule_wT1_05.cpp,
        // beside its one wave-relevant caller CreateNewVehicleEntities. The old "(FLAG:
        // Vehicle interior)" tag is retired: all three species arms resolve to committed
        // accessors -- GetParam / GetStaticTrafficParamFro / GetVehicle -- plus
        // Vehicle::GetCabIndex, which is declared and bodied as of this round.
        // ⚠️ DWARF :1323 spells it `bool IsVehiclesParamAZombie(uint32_t) const`. The trailing
        // const is NOT adopted here, deliberately and with the reason recorded: the three
        // accessors it calls are non-const in this tree, so adding it would require const
        // overloads of GetParam / GetVehicle / GetStaticTrafficParamFro -- new surface on the
        // pool-accessor block for no behavioural difference. One-step follow-up when someone
        // adds those overloads; the body below reads and never writes, so it is const-correct
        // in substance already.
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
        // FLAG (un-recoverable rodata): the stuck-side timer floor constant flt_82001CC0 (the
        // fsel else-value at 0x82707FB0) is NOT attested in the dossier pseudocode, so the
        // timer clamp cannot be reconstructed without fabricating it -- declaration-only.
        void  UpdateVehicleStuckSideTime(s32 liFlags, s32 liMask, f32 lfReset,
                                         f32 lfThreshold, f32* lpfTimer);   // @ 0x82707F00 (FLAG)
        void  UpdateVehicleStuckTimers(void* lpPhysicsInfo, f32 lfReset, f32 lfThreshold); // @ 0x82708D48 (FLAG)

        // =====================================================================================
        // ⭐ ADDED 2026-08-21 -- wave T1, cluster C4 (THE SPAWN LEGS).
        // Bodies land in BrnTrafficEntityModule_wT1_01.cpp. Every declaration below is
        // X360-ledger attested; the address on each line is its ARTIST entry point.
        // =====================================================================================

        // The two active-hull set instantiations the spawn ladder passes around. X360-attested
        // as Set<u16,72>: SpawnNewTraffic @0x82748A40 reads the length word at `+0x90` == 144
        // == 72 * sizeof(u16), and RecalculateActiveHulls @0x8274C870 memcpy's 148 bytes of it.
        typedef ::Set<u16, KU_MAX_ACTIVE_HULLS> ActiveHullSet;

        // ---- module lifecycle / state machine ----
        void Reset();                    // @ 0x8272CDA0
        void ResetEventData();           // @ 0x827088B8
        void EnterStartingUpState();     // @ 0x82708038
        void EnterTearingDownState();    // @ 0x82708168
        void EnterRunningState();        // @ 0x827080E8
        bool IsDecisionFrame();          // @ 0x827074E0
        void UpdateDensity();            // @ 0x82716318

        // ---- the STREAMER PUMP (wave T1 round 3, closure item 1) --------------------------
        // *** DECLARED 2026-08-21. Both were the "four blocked declarations" round 2 parked
        // UpdateStreaming on -- "landing it needs FOUR declarations in files this cluster does
        // not own". This round owns every traffic-module file, so they land. Bodies in
        // BrnTrafficEntityModule_wT1_04.cpp.
        //
        // DWARF :1554 `void UpdateStreaming(OutputBuffer_PostPhysics *)` -- the DWARF's own
        // signature, and it returns VOID even though the X360 body leaves the tail Append's
        // bool in r3 (a tail-position value, not a return).
        //
        // ⭐ THIS IS THE FUNCTION THAT MAKES THE GAME ASK FOR A VEH_T* BUNDLE. LoadData's
        // stage-1 SetAssetList publishes the CATALOGUE; nothing requests a bundle until
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
        // ⚠️ SIGNATURE CORRECTED 2026-08-21 (wave T1 round 4, item 1): it takes the
        // post-physics INPUT buffer and forwards it. IDA's prototype for @0x82722F98 shows one
        // argument, which is a Hex-Rays artefact of a pass-through: the body never touches r4,
        // and its ONLY caller UpdateDecisionFrame @0x8274E508 loads it
        // (`0x8274E61C mr r4, r30` where r30 == lpInput) immediately before the `bl`, so r4
        // flows straight into StaticVehicles_CreateNewVehicles. The DWARF spells it out --
        // BrnTrafficEntityModule.h:1839 `void StaticVehicles_UpdateVehicles(const
        // InputBuffer_PostPhysics *)`. This is not cosmetic: the old spelling made the body
        // pass a LITERAL NULL to StaticVehicles_CreateNewVehicles, which is a null deref the
        // moment that function's race-car proximity arm is un-gated for the online wave.
        void StaticVehicles_UpdateVehicles(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput); // @ 0x82722F98
        void StaticVehicles_UpdateStaticParams();                         // @ 0x82722F28
        void StaticVehicles_UpdatePurgatory();                            // @ 0x827228A8
        void StaticVehicles_KillParam(u32 luParam);                       // @ 0x82721C50
        void StaticVehicles_RemoveDeadParam(u32 luParam);                 // @ 0x827163D0

        // =====================================================================================
        // ⭐ ADDED 2026-08-21 -- wave T1 ROUND 4 (THE STEADY-STATE LOOP + SCENE PRESENCE).
        // Every one is DWARF-declared and X360-ledger attested; the DWARF line is on each.
        // =====================================================================================

        // @0x82715858, DWARF :1287 `void UpdateTimers(const InputBuffer_PreScene *)`.
        // Body in BrnTrafficEntityModule_wT1_06.cpp.
        //
        // ⭐⭐ THIS IS THE ONLY WRITER OF mbDecisionFrame IN THE IMAGE (grep-verified over the
        // whole export set for the +0x713F5 store). Until it ran, IsDecisionFrame() returned
        // Reset's `false` for ever once meState left E_STATE_STARTING_UP, so
        // PostPhysicsUpdate's RUNNING arm could only ever take the NON-decision branch and
        // RecalculateActiveHulls / SpawnNewTraffic / the StaticVehicles_* updates were
        // unreachable in steady state. Its single caller is PreSceneUpdate's E_STATE_RUNNING
        // arm (@0x8274A968), which is why the two land together.
        void UpdateTimers(const BrnTrafficIO::InputBuffer_PreScene* lpInput);

        // @0x8274E508, DWARF :1476. THE STEADY-STATE DECISION FRAME: the per-decision-frame
        // call set the console runs in E_STATE_RUNNING. Body in _wT1_06.cpp, PARTIAL (the
        // driving-traffic legs are named gates; the parked ladder is real).
        void UpdateDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                 BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8274C1A8, DWARF :1479. The cheap frames between decision frames. Body in
        // _wT1_06.cpp, PARTIAL for the same reason.
        void UpdateNonDecisionFrame(const BrnTrafficIO::InputBuffer_PostPhysics* lpInput,
                                    BrnTrafficIO::OutputBuffer_PostPhysics* lpOutput);

        // @0x8272FA30, DWARF :1317 `void CreateNewVehicleEntities(OutputBuffer_PreScene *)`.
        // THE PER-VEHICLE SCENE REGISTRATION -- it is what turns an alive traffic vehicle into
        // an entity the scene manager can hand to the renderer. Body in
        // BrnTrafficEntityModule_wT1_05.cpp. Its only caller in the image is PreSceneUpdate's
        // E_STATE_RUNNING arm.
        void CreateNewVehicleEntities(BrnTrafficIO::OutputBuffer_PreScene* lpOutput);

        static void _AssertLayout();

    private:
        // =================================================================================
        // MEMBERS -- DWARF/ship order. Every `:NNN` is the DWARF's BrnTrafficEntityModule.h
        // source line. HOST-NATIVE: no member is placed by an X360 byte offset.
        // =================================================================================

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
        //   embeds EA::Jobs::Job by value and drags the whole EATech eajobs SDK into this
        //   keystone header's include graph -- and this header is pulled in by
        //   BrnWorldModule.h, i.e. by most of the world build. Not modelled here as a
        //   deliberate include-graph decision, NOT because the type is unknown (it is fully
        //   declared in BrnTrafficJob.h). Insert HERE when the job wave needs it.
        u32                   muNumUpdateVehiclesJobs;           // :620

        // ---- the vehicle pools. THREE POOLS, ONE ARRAY (X360-attested): the standard pool
        // is elements [0, KU_MAX_STANDARD_TRAFFIC), the static (parked) pool is
        // [KU_STATIC_TRAFFIC_OFFSET, +KU_MAX_STATIC_TRAFFIC) and the single trailer slot is
        // KU_TRAILER_TRAFFIC_OFFSET. The console accessors differ only by their base element
        // index (85 / 485 / 684 at a 128-byte stride), which is 400/199/1 apart exactly.
        Vehicle               maVehicles[KU_MAX_TOTAL_TRAFFIC];  // :631
        VehicleAxles          maVehicleAxles[KU_MAX_TOTAL_TRAFFIC]; // :632
        Matrix44Affine        maVehicleTransforms[KU_MAX_TOTAL_TRAFFIC]; // :633
        CgsContainers::FastBitArray<KU_MAX_TOTAL_TRAFFIC>
                              mVehiclesAddedToCrashModule;       // :634
        VehicleSoaData        mVehicleSoaData;                   // :635

        Param                 maParams[KU_MAX_PARAMS];           // :637
        // [MEMBER HOLE 1 -- DWARF :638] ParamNeedToSlowData maParamNeedToSlowData[400]
        //   BLOCKER: BrnTraffic::ParamNeedToSlowData's DWARF home is BrnTrafficParam.h, which
        //   this cluster does not own and which does not declare it. X360-attested shape:
        //   GetParamNeedToSlowData @0x827077D0 returns `16 * (luParam + 13528) + this`, so the
        //   record is 16 bytes and the array is KU_MAX_PARAMS long. ADD the struct to
        //   BrnTrafficParam.h, then insert `ParamNeedToSlowData maParamNeedToSlowData[KU_MAX_PARAMS];`
        //   HERE and body GetParamNeedToSlowData.
        // [MEMBER HOLE 2 -- DWARF :639] ParamListNode maParamListNodes[400]
        //   BLOCKER: same -- BrnTraffic::ParamListNode's DWARF home is BrnTrafficParam.h and it
        //   is not declared there. It is the doubly-linked "params in this section, in order"
        //   node the UpdateParams_UpdateLinkedList pipeline walks (wave 2). Insert HERE.
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
        // ⭐ [MEMBER HOLE 3] CLOSED 2026-08-21 (wave T1, cluster C4). C3 landed the real
        // BrnTrafficHullRuntime.h; the member is inserted at its DWARF position here.
        // X360 ATTESTATION of the run: GetHullRuntimeSafe @0x8271DA70 and GetHullRuntime
        // @0x8271D9D0 both read `mauHullRuntimeDataIndices[luHull]` at +256816 and index the
        // record array at +257216 with a 1176-byte stride (== sizeof(HullRuntime) == 0x498);
        // Reset @0x8272CDA0 Constructs 72 of them from +257216 and clears the two 64-bit
        // fields of mUsedHullRuntimeData at +341888/+341896 -- and 257216 + 72*1176 == 341888
        // exactly, so nothing hides between the array and the bit array.
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

        // Extent 160 on all six: that is the literal the DWARF prints for every one of them
        // (Array<...,160u>) and the capacity the committed Array_TrafficCrashInfo_160.cpp
        // instantiation carries. It is deliberately spelled as a literal, NOT as
        // KU_MAX_NEW_CRASHED_VEHICLES (25) / KU_MAX_NEW_REMOVED_VEHICLES (25): those are the
        // per-frame BUDGETS the producers check against, not the array extents, and the
        // constant that spells 160 is not named in either the DWARF or the leak. Do not
        // invent a name for it -- find it.
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
        //   BLOCKER: BrnTraffic::RaceCarStateData's DWARF home is
        //   GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficRaceCarCache.h, which
        //   does not exist in the tree at all. It is the per-frame cache of the active race
        //   cars the traffic sim reacts to (>= 928 bytes on the console -- Prepare stage 0
        //   zeroes an 8-byte field at this+0x713A0, which lands inside it). Reconstruct that
        //   header, then insert `RaceCarStateData mRaceCarState;` HERE.

        Vector3               maEventGridStartPositions[E_ACTIVE_RACE_CAR_INDEX_COUNT]; // :710
        u8                    muNumberOfParticipantsInCurrentEvent;              // :711

        // :713 -- DWARF type BrnTraffic::LightTriggerId (an un-homed SharedClasses handle
        // struct wrapping a 32-bit id; BrnGameModeParams.h models the same handle as
        // `typedef u32 LightTriggerId`). Stored as the raw handle: this cluster never
        // dereferences it, and widening/narrowing is not at issue. FLAG: retype when
        // SharedClasses/Traffic homes the struct.
        u32                   mTrafficLightTriggerId;               // :713
        // :714 -- DWARF type BrnGameState::GameStateModuleIO::EGameModeType. Stored as its
        // underlying 32-bit value so this keystone header does not pull the whole GameState
        // IO header into BrnWorldModule.h's include graph. FLAG: same-width retype.
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

        // :728/:729 -- DWARF type RoadRulesRecvData::NetworkPlayerID. That type has no home in
        // the tree and no attested width, so the two members are stored as the 64-bit EA
        // online id the rest of the network layer uses. FLAG: NOT width-attested; retype (and
        // re-check the array extent) when RoadRulesRecvData lands. Never serialised by this
        // cluster.
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

        // :799-:821 -- the per-object copies of the vectorised tuning constants. They are
        // MEMBERS, not file-scope constants, on both builds (the console keeps them in the
        // object so the debug tools can retune them live); their VALUES are seeded by
        // Construct @0x82740220 and are therefore C4's, not this cluster's.
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
        // (BrnTrafficDebugComponent.h) INCLUDES this header, so including it back would be a
        // cycle; stored as the enum's underlying 32-bit value. FLAG: same-width retype if the
        // enum is ever moved to a leaf header.
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

        // ⭐ CORRECTED 2026-08-21 (C1 fix round). This four-bool run is FULLY pinned and the
        // earlier annotation that put mbDEBUGShowtimeStuff at +0x7286A was wrong (0x7286A and
        // 0x72875 are 11 bytes apart, so they could never be adjacent bools).
        // Prepare @0x8274A578 stores the 2560-byte fuzzy-logic allocation to +469100 and 0 to
        // +469104, so the pointer/count pair sits at 469100/469104 and :865/:866 follow at
        // 469108/469109 -- 469109 (0x72875) being exactly the byte
        // NeedToTakeActionAgainstJunctionFUP @0x82707FD0 and UpdateJunctionFUP @0x82745218
        // read. The four bools therefore fill 469096..469099 below the pointer, with the middle
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
        //   BLOCKER: DWARF type `Camera` == BrnDirector::Camera::Camera, whose owning header is
        //   heavy and would pull the director camera graph into BrnWorldModule.h's includes.
        //   Only consumer is the mbDEBUGPickVehicleFromCamera debug tool. Insert HERE when
        //   that tool is reconstructed.

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
