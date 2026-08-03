#pragma once

// BrnPhysics::Vehicle::PhysicalTrafficManager - the manager that owns the pool of physical
// traffic vehicles (up to ku8TotalMaxNumPhysicalTraffic == 20) and bridges traffic events
// to/from the physics simulation. This is a minimal OWNING reconstruction of the manager
// class: the member SEQUENCE is taken verbatim from the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/.../BrnPhysicalTrafficManager.h), and the 12 functions
// owned by this TU (the accessors, the buffer allocate/bridge, the entity-id helpers and
// ResetAboveGroundTestResults) are defined against these members BY NAME in
// BrnPhysicalTrafficManager.cpp.
//
// POINTER-WIDTH DIVERGENCE (flagged): the X360 build is 32-bit, so its absolute member
// offsets (e.g. mpaTrafficVehicles @ this+103604, mu8GlobalToPhysicalEntityIndexMap @
// this+104688, mUsedTrafficVehicles word base @ this+104552) assume 4-byte pointers. On the
// 64-bit host the void*/pointer members are 8 bytes, so these absolute offsets DO NOT
// reproduce. Per project rule we pin members BY NAME + SEQUENCE and do NOT static_assert
// absolute manager offsets across pointer members. Only pointer-free sub-struct sizes are
// asserted.
//
// The many sibling types this manager embeds/points at (TrafficPhysics, VehicleDriver,
// SimpleVehiclePhysics, the debug component, the IO interfaces) are their own future TUs;
// they are forward-declared or minimally sliced here only as far as this TU's 12 functions
// require, and flagged where a future TU must complete them.
//
// ============================================================================================
// ⚠️⚠️ OPEN FINDINGS, 2026-08-03 (the Construct wave). Read before growing this header.
//
// (1) FIXED HERE: the `ArticulatedJointPool` slice below was **720 bytes too small** (112 vs the
//     real 832). PhysicalTrafficManager::Construct @0x82636CA8 proves 832 two ways:
//       ArticulatedJointPool::Construct(this + 131072 - 0x6B40) == this + 103616, and the next
//       member it writes is `stfsx f0, r31, 0x19800` == this + 104448.  104448 - 103616 == 832.
//     And the REAL class (declared inside BrnArticulatedJointPool.cpp) is exactly 832:
//       10 joints * 80 + BitArray<10> 8 + BitArray<10> 8 + 4 floats == 800 + 16 + 16 == 832.
//     With the old 112 the embedded pool overran into mfJointSwingBreakVelocity the moment
//     anything constructed it, and every member from mfJointSwingBreakVelocity onwards was
//     seated 720 bytes early -- including mUsedTrafficVehicles, whose own comment here claims
//     "X360 word base this+104552", an offset the declarations did not produce.
//
// (2) PARTLY CLOSED -- TWO ODR FORKS LEFT, was three. This header defines, at namespace scope in
//     BrnPhysics::Vehicle, its own private copies of names that already have real definitions
//     elsewhere in the SAME namespace:
//        TrafficPhysics        -- `struct { u8 [5168]; }` here vs
//                                 `class TrafficPhysics : public VehiclePhysics` in
//                                 VehiclePhysics/TrafficPhysics.h
//        ArticulatedJointPool  -- the slice below vs the real class inside
//                                 VehiclePhysics/BrnArticulatedJointPool.cpp
//     Both also disagree on the CLASS-KEY (`struct` vs `class`), which MSVC folds into the mangled
//     name of anything templated on them. Nothing has detonated only because
//     BrnPhysicalTrafficManager.cpp is not mounted; including this header together with either of
//     those two is a hard redefinition error today. De-forking THEM means pulling VehiclePhysics.h
//     in here (both really do derive from VehiclePhysics), which is its own wave.
//     (PhysicalTrafficManagerDebugComponent was the fourth fork, DE-FORKED 2026-08-03.)
//
//     ⭐ VehicleDriver is DE-FORKED as of 2026-08-03 (the Construct-blocker wave), and the old
//     note's reasoning was wrong about it: BrnVehicleDriver.h does NOT pull VehiclePhysics.h in
//     (it needs only types.hpp + BrnCommonTypes.h + SharedIO/BrnVehicleDriverControls.h), so that
//     fork was separable from the other two and cost one include. It was also the one fork that
//     BLOCKED WORK: BrnVehicleManager.h includes the real BrnVehicleDriver.h, so this header and
//     that one could not meet, and VehicleManager::Construct @0x8263B7C8 must call both
//     VehicleDriver::Construct and PhysicalTrafficManager::Construct.
//     The gate is real, not a comment: BrnVehicleManager_layout_check.cpp (mounted) now includes
//     THIS header and BrnVehicleManager.h together, so a re-fork fails the build.
//
// (3) STILL OPEN -- the host layout does not reproduce the X360 offsets even where no pointer is
//     involved. Measured host sizes vs the X360 values the asm implies:
//        CgsResource::ResourceHandle       16   vs   8   (VehicleManager's maRaceCar*Handles
//                                                         arrays independently prove 8)
//        Array<EntityId,20>                84   vs  88   (104464 .. 104552)
//        EventQueue<s8,50>                 72   vs  64   (104624 .. 104688)
//     Per project rule this TU is written BY NAME, so the bodies are correct regardless; but a
//     PhysicalTrafficManager _AssertLayout() cannot be written until those three are settled.
// ============================================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // EntityId, Vector3, CgsID, ResourceHandle
#include "GameShared/GameClasses/Containers/CgsBitArray.h"             // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/Containers/CgsArray.h"               // Array<T,N>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"              // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"          // CgsModule::IOBufferStack
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle
#include "GameSource/BurnoutConstants.h"                              // EActiveRaceCarIndex
// ⭐ DE-FORKED 2026-08-03: this header used to declare its own opaque
// `struct PhysicalTrafficManagerDebugComponent { void* mpVTable; u8 mOpaque[60]; };` at namespace
// scope, a second definition of a name that already had a real one. The real class is included.
#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManagerDebugComponent.h"
// ⭐ DE-FORKED 2026-08-03 (second one): `struct VehicleDriver { u8 mOpaque[224]; }` used to be
// declared at namespace scope below. The real struct lives here and is 224 bytes, so the stride
// this header depends on is unchanged -- see the note at the old fork's seat.
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnVehicleDriver.h"

namespace BrnPhysics
{
// Forward decl of the streamed deformation model spec (real home
// BrnPhysics::Deformation::StreamedDeformationSpec, BrnStreamedDeformationSpec.h) --
// PhysicalTrafficVehicle::PreparePhysical takes it by pointer and reads its bounding box.
namespace Deformation { struct StreamedDeformationSpec; }
namespace Vehicle
{

// Forward decls for the PhysicalTrafficVehicle wave-8 method params (real homes elsewhere; only
// pointers/refs are taken here so incomplete types suffice):
struct CreatePhysicalTrafficEvent;   // spawn event -- SharedIO/BrnVehicleEvents.h
struct VehicleAttribs;               // per-car attribs -- VehiclePhysics.h
class  RaceCarPhysics;               // the checking race car -- RaceCarPhysics.h
class  BrnPlayerDriverControls;      // per-frame driver controls -- SharedIO/BrnPlayerDriverControls.h

// VecFloat: a single 16-byte VMX float lane (the DWARF spells the members below as VecFloat).
// Its canonical home is rw::math::vpu; here it is the 16-byte 4-lane vector value used for the
// per-vehicle world-space lowest point. mavfLowestPointWorldSpace is not touched by this TU's
// 12 functions.
typedef rw::math::vpu::Vector4 VecFloat;

// SimpleVehiclePhysics: the per-vehicle physics body, pointed at by mpVehicleBody and by
// mpaSimpleVehiclePhysics. Its full layout/methods are the BrnSimpleVehiclePhysics TU; this
// manager only takes/returns SimpleVehiclePhysics* (GetVehiclePhysics) and, in
// ResetAboveGroundTestResults, reaches into its above-ground-test sub-objects (flagged in the
// .cpp). Forward-declared here.
struct SimpleVehiclePhysics;

// ku8TotalMaxNumPhysicalTraffic: the pool capacity asserted throughout this TU
// ("liVehicle < ku8TotalMaxNumPhysicalTraffic", upper bound 20).
const u8 KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC = 20;

// KU8_INVALID_MAP (BrnPhysicalTrafficManager.h:428): the sentinel stored in
// mu8GlobalToPhysicalEntityIndexMap for "no physical traffic vehicle for this global id"
// (the X360 `cmplwi 0x7F` checks in ValidateAndFixUpTrafficTrafficContact).
const u8 KU8_INVALID_MAP = 127;

// KU_NUM_BITS_FOR_ENTITY_NUM: the entity-index field width inside an EntityId
// (CgsEntityId.h:116 assert "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)"). The X360
// packs an EntityId as (entityIndex << 10) | (ownerType << 24); the index field is 14 bits.
const u32 KU_NUM_BITS_FOR_ENTITY_NUM = 14;

// E_ENTITYTYPE_TRAFFIC_VEHICLE: the owner-type byte (EntityId bits 24..31) that this TU
// asserts for physics-traffic ids ("lPhysicsVehicleId.GetOwner() ==
// BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE", value 2). FLAG: the full BrnWorld::EEntityType
// enum is owned by the World module; only the value used here is reproduced.
const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE = 2;

// ---- Minimal slices of un-homed sibling types this TU touches BY NAME ----------------

// TrafficPhysics (BrnPhysicalTrafficManager.h:396, maFullTrafficPhysics[20]): the per-vehicle
// full-physics block. Its real layout/methods are a future TU; the X360 ctor builds it with a
// 0x1430-byte (5168) stride and three vtable slots plus several vector-constructor-iterator
// sub-objects. FLAG: this is an opaque size-correct slice (5168 bytes) so the manager's
// embedded array and the constructor's per-element walk reproduce; named sub-members will be
// filled in when the TrafficPhysics TU lands.
// ⚠️ ODR FORK -- see finding (2) in the banner. The real class is
// `BrnPhysics::Vehicle::TrafficPhysics : public VehiclePhysics` in VehiclePhysics/TrafficPhysics.h.
struct TrafficPhysics
{
    // @0x8262E980 (an .ida-exports HOLE -- no JSON for that address; DWARF TrafficPhysics.h:42
    // gives the signature `void Construct();`). Called 20 times by
    // PhysicalTrafficManager::Construct at a 0x1430 stride. DECLARE-ONLY: the body is owned by
    // the TrafficPhysics TU.
    void Construct();

    u8 mOpaque[5168];   // 0x1430 - X360 per-element stride from the PhysicalTrafficManager ctor
};

// VehicleDriver: the AI/driver block, pointed at by mpaTrafficDrivers (stride 224 / 0xE0 from
// GetTrafficDriver).
//
// ⭐ DE-FORKED 2026-08-03. This slot used to be a second, opaque `struct VehicleDriver
// { u8 mOpaque[224]; }` at namespace scope in BrnPhysics::Vehicle -- a redefinition of the real
// struct that VehiclePhysics/BrnVehicleDriver.h already owns. The real header is included above
// and the stride assert below is what keeps the two readings honest.
//
// ⚠️ CORRECTION to finding (2) in the banner, which said de-forking "means pulling VehiclePhysics.h
// in here, which is its own wave". That is true of TrafficPhysics and ArticulatedJointPool -- both
// really do derive from VehiclePhysics -- but it was NOT true of VehicleDriver: BrnVehicleDriver.h
// stands alone (types.hpp + BrnCommonTypes.h + SharedIO/BrnVehicleDriverControls.h) and pulls no
// physics body in. The three forks were not one problem; this one was separable and is gone.
//
// WHY IT MATTERED: while the fork stood, this header and BrnVehicleManager.h (which includes the
// real BrnVehicleDriver.h for maRaceCarDrivers[8] / mPlayerAiDriver) could not be included in the
// same translation unit -- a hard C2011. VehicleManager::Construct @0x8263B7C8 has to call BOTH
// VehicleDriver::Construct and PhysicalTrafficManager::Construct, so the fork was a compile-level
// blocker on that function, not just a latent tidiness item.
static_assert(sizeof(VehicleDriver) == 224,
              "VehicleDriver is 224 bytes (0xE0) -- the X360 GetTrafficDriver stride "
              "(*(this+103600) + 224*idx) and VehicleManager::Construct's `addi r25, r25, 0xE0`");

// PhysicalTrafficVehicle: pointed at by mpaTrafficVehicles (stride 64 / 0x40 from the
// GetTrafficVehicle accessors). The member layout below is verbatim from the DecFIGS DWARF
// (BrnPhysicalTrafficVehicle.h) and is what this TU reads BY NAME:
//   IsTrafficVehicleSimple reads mu8PhysicalType (X360 +0x32 == 50)
//   GetVehiclePhysics reads mpVehicleBody         (X360 +0x1C == 28)
// FLAG: the methods of PhysicalTrafficVehicle are its own future TU; only the data layout is
// reproduced here so this manager TU can index it.
struct PhysicalTrafficVehicle
{
    enum EPhysicalTrafficType
    {
        E_PHYSICAL_TRAFFIC_TYPE_FULL  = 0,
        E_PHYSICAL_TRAFFIC_TYPE_SIMPLE = 1,
        E_PHYSICAL_TRAFFIC_TYPE_COUNT = 2,
    };
    enum EArticulatedJointState
    {
        E_ARTICULATE_JOINT_NONE     = 0,
        E_ARTICULATE_JOINT_ATTACHED = 1,
        E_ARTICULATE_JOINT_COUNT    = 2,
    };
    enum EArticulatedVehicleType
    {
        E_ARTICULATE_VEHICLE_NONE    = 0,
        E_ARTICULATE_VEHICLE_CAB     = 1,
        E_ARTICULATE_VEHICLE_TRAILER = 2,
        E_ARTICULATE_VEHICLE_COUNT   = 3,
    };

    // X360 0x825B3358: range-checked read of meArticulatedVehicleType (+36). One of
    // PhysicalTrafficVehicle's own methods (FLAG above): bodied against the named +36 member.
    EArticulatedVehicleType GetArticulatedVehicleType() const;

    // ---- wave-7 methods (bodied in BrnPhysicalTrafficManager.cpp) ----------------------------
    // @0x825C0148: assert fully-physical and return mpVehicleBody as the concrete full-physics body.
    TrafficPhysics* GetFullTrafficPhysics();
    // @0x826152E0: forward an air-ram impulse to the full-physics body (only when FULLY physical).
    void            AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay,
                              Vector3 lvCustomImpulse, Vector3 lvCustomPosition, f32 lfTimerTillFire);
    // @0x825B33B8: true iff this is a SIMPLE (non-full-physics) traffic vehicle.
    bool            IsSimple() const;
    // @0x825B3418: true iff this vehicle has an ATTACHED (non-broken) articulation joint.
    bool            HasNonBrokenJoint() const;
    // @0x825C01B8: latch the active-race-car that "checked" this vehicle (asserts not already checked).
    void            SetCheckOwner(EActiveRaceCarIndex leCheckOwner);
    // has miCheckOwner been set to a valid race-car (!= 0xFF sentinel)?
    bool            HasBeenChecked() const { return miCheckOwner != -1; }
    // @0x825C0220 / @0x825C0538: the articulation point in world / local space.
    Vector3         GetArticulationPointWorldSpace() const;
    Vector3         GetArticulationPointLocalSpace() const;

    // ---- wave-8 methods (bodied in BrnPhysicalTrafficManager.cpp) --------------------------------
    // @0x82641058: seed this vehicle's id/state fields from the spawn event; when fully physical,
    // delegate the full-physics body prepare to the TrafficPhysics TU.
    bool PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent, VehicleAttribs* lpAttribs,
                         const Deformation::StreamedDeformationSpec* lpModelData,
                         const Vector3* lpWheelPositions, const f32* lpafWheelRadii);
    // @0x8261E360: latch the checking race car + reset the check-notify timer; when the checker is
    // hard enough and this car is fully physical, arm the full body's crashing state.
    void OnChecked(EActiveRaceCarIndex leOwner, const RaceCarPhysics* lpRaceCarPhysics,
                   Vector3 lContactPointOnTraffic);
    // @0x826411C0: per-frame update -- accumulate the check-notify timer; forward the full-physics
    // per-frame tick to the TrafficPhysics TU.
    void Update(f32 lfSimTimerTimeStep, f32 lfGameTimerTimeStep, const Matrix44Affine* lpCameraMatrix,
                const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis);
    // @0x825F3B68: mark this vehicle as an articulated cab/trailer + (delegated) compute its
    // articulation point from the deformation model's hitch locator.
    void SetArticulated(const CreatePhysicalTrafficEvent& lrCreateTrafficEvent,
                        EArticulatedVehicleType leVehicleType);

    Vector3                 mArticulationPointLocal;     // +0
    CgsID                   mCgsID;                      // +16 (u64; +12..15 padding)
    f32                     mfTimeSinceCheckNotify;      // +24
    SimpleVehiclePhysics*   mpVehicleBody;               // +28 (X360 4B ptr) -> GetVehiclePhysics
    u32                     mePhysicalTrafficState;      // +32 (BrnPhysics::Vehicle::ETrafficType)
    EArticulatedVehicleType meArticulatedVehicleType;    // +36
    EArticulatedJointState  meArticulatedJointState;     // +40
    s32                     miJointIndex;                // +44
    bool                    mbRammed;                    // +48
    u8                      mu8PhysicsPoolIndex;         // +49
    u8                      mu8PhysicalType;             // +50 -> IsTrafficVehicleSimple
    s8                      miCheckOwner;                // +51
    bool                    mbUsingBoxWithWorld;         // +52
    // pad to the X360 0x40 stride (mOpaque keeps element stride == 64 on the host too, but the
    // pointer member above is 8B on host so the trailing pad just keeps the type non-degenerate)
};

// ArticulatedJointCreateBuffer: the IO buffer pushed onto the input stack by
// AllocateInternalBuffers (CreateIOBuffer<ArticulatedJointCreateBuffer>). Real layout/use is a
// future TU; opaque minimal slice so the pointer member + CreateIOBuffer<T> instantiate.
struct ArticulatedJointCreateBuffer
{
    u8 mOpaque[16];
};

// ArticulatedJointPool: embedded by value (BrnPhysicalTrafficManager.h:406). A self-contained
// reconstruction already exists in BrnArticulatedJointPool.cpp; this TU only needs to take the
// address of the pool member and pass it to ArticulatedJointPool::SendCreateRemoveJointEvents
// (declared below, owned by the ArticulatedJointPool TU). FLAG: SendCreateRemoveJointEvents is
// declared-only here (no body) so this TU compiles without redefining the pool.
struct ArticulatedJointPool
{
    // @0x82600938. DECLARE-ONLY -- the body lives with the real class in
    // VehiclePhysics/BrnArticulatedJointPool.cpp, which returns int; the mangled name encodes the
    // return type on MSVC, so this declaration must agree with it.
    int  Construct();
    void SendCreateRemoveJointEvents(const void* lpOutputRequestInterface,
                                     ArticulatedJointCreateBuffer* lpCreateBuffer);

    // ⚠️⚠️ CORRECTED 2026-08-03: this span was **112**, which is 720 bytes short. See finding (1)
    // in the banner -- PhysicalTrafficManager::Construct pins the pool at this+103616 and the
    // next member it writes at this+104448, and the real class in BrnArticulatedJointPool.cpp is
    // exactly 832 bytes (10*80 joints + 2*8 bit-masks + 4*4 limit floats). With 112 the embedded
    // pool overran its successor and every member after it was seated 720 bytes early.
    u8 mOpaque[832];
};

// Forward-declared IO/interface dependencies (their own TUs).
struct VehicleOutputRequestInterface;

// =====================================================================================
// PhysicalTrafficManager
// =====================================================================================
class PhysicalTrafficManager
{
public:
    typedef Array<EntityId, 20u>            RecycledTrafficIdArray;
    typedef CgsContainers::BitArray<20u>    TotalPhysicalTrafficBitArray;
    typedef CgsContainers::BitArray<20u>    FullyPhysicalTrafficBitArray;
    typedef CgsContainers::BitArray<1u>     SimpleTrafficBitArray;

    // ---- the 12 functions owned by THIS TU ----------------------------------------

    // Accessors into the per-vehicle arrays (X360 0x825B4880 / 0x825B4800 share a body):
    // both the const and non-const GetTrafficVehicle return &mpaTrafficVehicles[idx].
    PhysicalTrafficVehicle*       GetTrafficVehicle(s32 liVehicle);
    const PhysicalTrafficVehicle* GetTrafficVehicle(s32 liVehicle) const;

    // X360 0x825B4900: &mpaTrafficDrivers[idx] (stride 224).
    VehicleDriver* GetTrafficDriver(s32 liVehicle);

    // X360 0x825B4A28: mpaTrafficVehicles[ lPhysicsVehicleId.GetEntityIndex() ].mpVehicleBody
    SimpleVehiclePhysics* GetVehiclePhysics(EntityId lPhysicsVehicleId);

    // X360 0x825B4A98: true iff mpaTrafficVehicles[idx].mu8PhysicalType == E_..._SIMPLE
    bool IsTrafficVehicleSimple(EntityId lPhysicsVehicleId) const;

    // X360 0x825B4980: pack a physics-traffic EntityId from a traffic index.
    EntityId GetPhysicsEntityId(s32 liTrafficIndex) const;

    // X360 0x825C2C38: look up the GLOBAL entity id stored for a traffic-car slot.
    EntityId GetGlobalTrafficEntityId(u16 lu16TrafficCarIndex) const;

    // X360 0x82615958: push the ArticulatedJointCreateBuffer onto the input IO stack.
    void AllocateInternalBuffers(CgsModule::IOBufferStack* lpInputBufferStack,
                                 CgsModule::IOBufferStack* lpOutputBufferStack);

    // X360 0x82615B10: drain the create-buffer into the output request interface.
    void BridgeArticulatedJointRequestsToSim(VehicleOutputRequestInterface* lpOutputRequestInterface);

    // X360 0x827E42E8: the constructor (per-element walk + sentinel init).
    PhysicalTrafficManager();

    // X360 0x82636CA8 (99 instructions; DWARF BrnPhysicalTrafficManager.h:475 `void Construct()`).
    // One-shot construction: build all 20 full-physics bodies and the joint pool, invalidate the
    // 20 traffic entity ids, null the four pool pointers, seed the joint-break limits, clear the
    // traffic bitsets, construct the unused-potential queue and the debug component.
    // Its ONLY caller in the image is VehicleManager::Construct @0x8263B7C8.
    void Construct();

    // X360 0x825E8808: reset the above-ground (down-ray) test results for every used vehicle.
    void ResetAboveGroundTestResults();

    // X360 0x8259BD10: validate (and fix up) a traffic-vs-traffic potential contact.
    bool ValidateAndFixUpTrafficTrafficContact(void* lpContact) const;

private:
    // ---- member SEQUENCE verbatim from the DecFIGS DWARF (offsets are X360/32-bit) --------
    TrafficPhysics                maFullTrafficPhysics[20];                 // :396
    EntityId                      maTrafficEntityIDs[20];                   // :397
    CgsResource::ResourceHandle   maTrafficCarModelHandles[20];             // :398
    VehicleDriver*                mpaTrafficDrivers;                        // :400 (X360 this+103600)
    PhysicalTrafficVehicle*       mpaTrafficVehicles;                       // :401 (X360 this+103604)
    SimpleVehiclePhysics*         mpaSimpleVehiclePhysics;                  // :402 (X360 this+103608)
    ArticulatedJointCreateBuffer* mpArticulatedJointCreateBuffer;          // :405 (X360 this+103612)
    ArticulatedJointPool          mArticulatedJointPool;                    // :406 (X360 this+103616)
    f32                           mfJointSwingBreakVelocity;                // :407
    f32                           mfJointTwistBreakVelocity;                // :408
    f32                           mfJointLinearBreakMph;                    // :409
    bool                          mbAllowArticulatedJointBreaking;          // :410
    RecycledTrafficIdArray        maRecycledTrafficThisFrame;               // :413
    TotalPhysicalTrafficBitArray  mUsedTrafficVehicles;                     // :415 (X360 word base this+104552)
    FullyPhysicalTrafficBitArray  mUsedFullTrafficPhysics;                  // :416
    SimpleTrafficBitArray         mUsedSimpleVehiclePhysics;                // :417
    TotalPhysicalTrafficBitArray  mPotentialTrafficVehicles;                // :419
    TotalPhysicalTrafficBitArray  mTrafficDeformationModelsActive;          // :420
    TotalPhysicalTrafficBitArray  mTestedTrafficVehicles;                   // :421
    TotalPhysicalTrafficBitArray  mAddedTrafficVehicles;                    // :422
    TotalPhysicalTrafficBitArray  mRemovedTrafficVehicles;                  // :423
    TotalPhysicalTrafficBitArray  mMadeSimpleTrafficVehicles;               // :424
    CgsModule::EventQueue<s8, 50> mUnusedPotentialTrafficQueue;             // :425
    // :429 (X360 this+104688). The X360 bound check in ValidateAndFixUpTrafficTrafficContact is
    // `GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)` and the build computes that
    // sizeof as exactly 600 (asm `cmplwi 0x258`). The DecFIGS DWARF spells the element count as
    // 601, but the asm is authoritative, so the map is sized 600 here to keep the `< sizeof(map)`
    // assert evaluating to the same 600 the X360 used.
    u8                            mu8GlobalToPhysicalEntityIndexMap[600];
    VecFloat                      mavfLowestPointWorldSpace[20];            // :432
    PhysicalTrafficManagerDebugComponent mDebugComponent;                   // :435
};

}   // namespace Vehicle
}   // namespace BrnPhysics
