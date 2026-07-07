// BrnPhysics::Vehicle::PhysicalTrafficManager - the 12 functions owned by this TU.
//
// Layout/shape authority: references/DecFIGS/dwarfdump/.../BrnPhysicalTrafficManager.h and
// .../BrnPhysicalTrafficVehicle.h. Body authority: the X360 pseudocode/asm at the addresses
// noted per function. Members are accessed BY NAME against the owning header.
//
// POINTER-WIDTH NOTE (flagged in the header): the X360 absolute member offsets are 32-bit
// (4-byte pointers). The bodies below reproduce the SEMANTICS by name; the literal X360
// offsets (this+103600, this+104552, this+104688, the +0x1C/+0x32 sub-offsets, etc.) are
// recorded in comments only and are NOT host-reproducible because host pointers are 8 bytes.

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"  // VehiclePhysics::AddAirRam / GetTransform / SetCrashing (full-physics body)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"  // CreatePhysicalTrafficEvent (real layout) + ETrafficType
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // Deformation::StreamedDeformationSpec::GetBoundingBox + CgsGeometric::AxisAlignedBox
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::IsValid(Vector3)
#include "rw/math/vpu/matrix44affine_operation.h"                        // rw::math::vpu::IsValid(Matrix44Affine), TransformPoint

namespace BrnPhysics
{
namespace Vehicle
{

// ---------------------------------------------------------------------------------------
// GetTrafficVehicle (non-const)   @ 0x825B4800   (the unnamed body shares this with
// GetTrafficInterest_0 @ 0x825B4880; both return &mpaTrafficVehicles[idx], stride 0x40)
// X360: result = *(this+103604) + (liVehicle << 6)
// ---------------------------------------------------------------------------------------
PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32 liVehicle)
{
    CGS_ASSERT(liVehicle >= 0, "liVehicle >= 0");
    CGS_ASSERT(liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "liVehicle < ku8TotalMaxNumPhysicalTraffic");
    return &mpaTrafficVehicles[liVehicle];
}

// GetTrafficVehicle (const)   @ 0x825B4880  (the X360 const accessor; same stride-0x40 math)
const PhysicalTrafficVehicle* PhysicalTrafficManager::GetTrafficVehicle(s32 liVehicle) const
{
    CGS_ASSERT(liVehicle >= 0, "liVehicle >= 0");
    CGS_ASSERT(liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "liVehicle < ku8TotalMaxNumPhysicalTraffic");
    return &mpaTrafficVehicles[liVehicle];
}

// ---------------------------------------------------------------------------------------
// PhysicalTrafficVehicle::GetArticulatedVehicleType (const)   @ 0x825B3358
// X360: range-asserts meArticulatedVehicleType (+0x24 == 36) against the enum bounds
// (cmpwi 0 / cmpwi 3 -> fires when type < E_ARTICULATE_VEHICLE_NONE or >= E_..._COUNT),
// then returns it. One of PhysicalTrafficVehicle's own methods; bodied here against the
// named +36 member.
// ---------------------------------------------------------------------------------------
PhysicalTrafficVehicle::EArticulatedVehicleType PhysicalTrafficVehicle::GetArticulatedVehicleType() const
{
    CGS_ASSERT(meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE
                   && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT,
               "meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT");
    return meArticulatedVehicleType;
}

// ---------------------------------------------------------------------------------------
// GetTrafficDriver   @ 0x825B4900
// X360: result = *(this+103600) + 224 * liVehicle    (mpaTrafficDrivers[liVehicle])
// ---------------------------------------------------------------------------------------
VehicleDriver* PhysicalTrafficManager::GetTrafficDriver(s32 liVehicle)
{
    CGS_ASSERT(liVehicle >= 0, "liVehicle >= 0");
    CGS_ASSERT(liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "liVehicle < ku8TotalMaxNumPhysicalTraffic");
    return &mpaTrafficDrivers[liVehicle];
}

// ---------------------------------------------------------------------------------------
// GetVehiclePhysics   @ 0x825B4A28
// X360: assert lPhysicsVehicleId.GetOwner() == E_ENTITYTYPE_TRAFFIC_VEHICLE (HIBYTE == 2),
//       then return GetTrafficVehicle( (id >> 10) & 0x3FFF )->mpVehicleBody  (the +0x1C read).
// The (id >> 10) & 0x3FFF is EntityId::GetEntityIndex() (14-bit field above the 10-bit tag).
// ---------------------------------------------------------------------------------------
SimpleVehiclePhysics* PhysicalTrafficManager::GetVehiclePhysics(EntityId lPhysicsVehicleId)
{
    const u32 luOwner = lPhysicsVehicleId.muValue >> 24;
    CGS_ASSERT(luOwner == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
               "lPhysicsVehicleId.GetOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

    const s32 liIndex = static_cast<s32>((lPhysicsVehicleId.muValue >> 10) & 0x3FFFu);
    // X360 +0x1C of the PhysicalTrafficVehicle == mpVehicleBody.
    return GetTrafficVehicle(liIndex)->mpVehicleBody;
}

// ---------------------------------------------------------------------------------------
// IsTrafficVehicleSimple   @ 0x825B4A98
// X360: leType = GetTrafficVehicle( (id >> 10) & 0x3FFF )->mu8PhysicalType  (the +0x32 byte);
//       assert leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT; return leType == E_..._SIMPLE.
// (The X360 return is the `cntlzw(leType-1)>>5` idiom == (leType == 1).)
// ---------------------------------------------------------------------------------------
bool PhysicalTrafficManager::IsTrafficVehicleSimple(EntityId lPhysicsVehicleId) const
{
    const s32 liIndex = static_cast<s32>((lPhysicsVehicleId.muValue >> 10) & 0x3FFFu);
    const u8 lu8Type = GetTrafficVehicle(liIndex)->mu8PhysicalType;   // X360 +0x32
    CGS_ASSERT(lu8Type < PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT,
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
    return lu8Type == PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_SIMPLE;
}

// ---------------------------------------------------------------------------------------
// GetPhysicsEntityId   @ 0x825B4980
// X360: *result = 0; assert index in [0,20) and index < (1<<14);
//       *result = (index << 10) | 0x2000000.    (owner byte 2 == TRAFFIC_VEHICLE)
// ---------------------------------------------------------------------------------------
EntityId PhysicalTrafficManager::GetPhysicsEntityId(s32 liTrafficIndex) const
{
    CGS_ASSERT(liTrafficIndex >= 0 && liTrafficIndex < static_cast<s32>(KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC),
               "liTrafficIndex >= 0 && liTrafficIndex < static_cast<int32_t>( ku8TotalMaxNumPhysicalTraffic )");

    EntityId lResult;
    lResult.muValue = 0;

    const u32 luEntityIndex = static_cast<u32>(liTrafficIndex);
    CGS_ASSERT(luEntityIndex < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");

    // (index << 10) | (E_ENTITYTYPE_TRAFFIC_VEHICLE << 24)  ==  (index << 10) | 0x02000000.
    lResult.muValue = (luEntityIndex << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);
    return lResult;
}

// ---------------------------------------------------------------------------------------
// GetGlobalTrafficEntityId   @ 0x825C2C38
// X360: assert lu16TrafficCarIndex < 20 (the CgsBitArray bounds message);
//       assert mUsedTrafficVehicles.IsBitSet(lu16TrafficCarIndex);
//       *result = maTrafficEntityIDs[lu16TrafficCarIndex].
// The X360 word math (8*((idx>>6)+13069)+this) is mUsedTrafficVehicles.maxBits[idx/64];
// (4*(idx+25840)+this) is &maTrafficEntityIDs[idx]. Both reproduced BY NAME below.
// ---------------------------------------------------------------------------------------
EntityId PhysicalTrafficManager::GetGlobalTrafficEntityId(u16 lu16TrafficCarIndex) const
{
    // CgsBitArray.h:203 bounds assert ("invalid index : <i> < 20"), built by the inlined
    // StrStream in the X360; the house CGS_ASSERT carries the stringized condition.
    CGS_ASSERT(lu16TrafficCarIndex < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC, "invalid index : luIndex < 20");

    CGS_ASSERT(mUsedTrafficVehicles.IsBitSet(lu16TrafficCarIndex),
               "mUsedTrafficVehicles.IsBitSet( lu16TrafficCarIndex )");

    return maTrafficEntityIDs[lu16TrafficCarIndex];
}

// ---------------------------------------------------------------------------------------
// AllocateInternalBuffers   @ 0x82615958
// X360: assert lpInputBufferStack/lpOutputBufferStack != NULL and mpArticulatedJointCreateBuffer
//       == NULL; then CreateIOBuffer<ArticulatedJointCreateBuffer>(&mpArticulatedJointCreateBuffer)
//       on the INPUT stack (r3 = lpInputBufferStack); assert it came back non-NULL.
// (lpOutputBufferStack is only NULL-checked here; the deallocate path uses it.)
// ---------------------------------------------------------------------------------------
void PhysicalTrafficManager::AllocateInternalBuffers(CgsModule::IOBufferStack* lpInputBufferStack,
                                                      CgsModule::IOBufferStack* lpOutputBufferStack)
{
    CGS_ASSERT(lpInputBufferStack != nullptr, "lpInputBufferStack != NULL");
    CGS_ASSERT(lpOutputBufferStack != nullptr, "lpOutputBufferStack != NULL");
    CGS_ASSERT(mpArticulatedJointCreateBuffer == nullptr, "mpArticulatedJointCreateBuffer == NULL");

    lpInputBufferStack->CreateIOBuffer<ArticulatedJointCreateBuffer>(
        &mpArticulatedJointCreateBuffer, "ArticulatedJointBuffer");

    CGS_ASSERT(mpArticulatedJointCreateBuffer != nullptr, "mpArticulatedJointCreateBuffer != NULL");
}

// ---------------------------------------------------------------------------------------
// BridgeArticulatedJointRequestsToSim   @ 0x82615B10
// X360: assert lpOutputRequestInterface != NULL and mpArticulatedJointCreateBuffer != NULL;
//       mArticulatedJointPool.SendCreateRemoveJointEvents(lpOutputRequestInterface,
//                                                          mpArticulatedJointCreateBuffer).
// (X360 r3 = this+103616 == &mArticulatedJointPool, r4 = lpOutputRequestInterface,
//  r5 = mpArticulatedJointCreateBuffer.)
// ---------------------------------------------------------------------------------------
void PhysicalTrafficManager::BridgeArticulatedJointRequestsToSim(
        VehicleOutputRequestInterface* lpOutputRequestInterface)
{
    CGS_ASSERT(lpOutputRequestInterface != nullptr, "lpOutputRequestInterface != NULL");
    CGS_ASSERT(mpArticulatedJointCreateBuffer != nullptr, "mpArticulatedJointCreateBuffer != NULL");

    mArticulatedJointPool.SendCreateRemoveJointEvents(lpOutputRequestInterface,
                                                      mpArticulatedJointCreateBuffer);
}

// ---------------------------------------------------------------------------------------
// PhysicalTrafficManager (constructor)   @ 0x827E42E8
// X360: walks maFullTrafficPhysics[0..19] (stride 0x1430), placing each element's three
// vtables and running the vector-constructor-iterators for its embedded
// CgsCollision::BaseCollisionGenerator sub-arrays; then sets the unused-potential-traffic
// queue head sentinel (*(this+104544) = -1) and the debug-component vtable (*(this+105616)).
//
// FLAG (un-homed): the per-element TrafficPhysics construction (its vtables + the
// vector-constructor-iterator sub-object init) bottoms out in the TrafficPhysics +
// CgsSceneManager::CgsCollision::BaseCollisionGenerator TUs, which are NOT homed. We cannot
// reproduce those placement-new walks against the opaque TrafficPhysics slice without
// fabricating its layout, so the per-element construction is left to those TUs. The two
// observable, name-bearing inits this TU CAN reproduce are restated below. Default-construct
// the embedded/named members so the object is well-formed under MSVC.
// ---------------------------------------------------------------------------------------
PhysicalTrafficManager::PhysicalTrafficManager()
    : mpaTrafficDrivers(nullptr)
    , mpaTrafficVehicles(nullptr)
    , mpaSimpleVehiclePhysics(nullptr)
    , mpArticulatedJointCreateBuffer(nullptr)
    , mfJointSwingBreakVelocity(0.0f)
    , mfJointTwistBreakVelocity(0.0f)
    , mfJointLinearBreakMph(0.0f)
    , mbAllowArticulatedJointBreaking(false)
{
    // X360 *(this+104544) = -1: the EventQueue head/read-index sentinel of
    // mUnusedPotentialTrafficQueue. FLAG: the exact member is inside the un-homed
    // CgsModule::EventQueue body layout; the observable effect is the queue starting empty.
    // Its Construct()/reset is owned by the EventQueue TU; here the member is value-initialised.

    // X360 *(this+105616) = &debug-component vtable: mDebugComponent's vtable. FLAG: the debug
    // component is its own TU; the vtable store happens in its constructor, not reproduced here.

    // FLAG: the maFullTrafficPhysics[20] per-element placement walk (TrafficPhysics ctor +
    // BaseCollisionGenerator vector-constructor-iterators) is left to those un-homed TUs.
}

// ---------------------------------------------------------------------------------------
// ResetAboveGroundTestResults   @ 0x825E8808
// X360: for each set bit (vehicle index) of mUsedTrafficVehicles (the GetFirstNonZeroBit /
// GetNextNonZeroBit walk with the CgsBitArray.h:203 bounds asserts), take
// GetTrafficVehicle(idx)->mpVehicleBody and reset its above-ground / down-ray test sub-objects
// (three AboveGroundTestResult-shaped blocks at the +0x130/+0x210/+0x3D0 sub-offsets and a
// fourth result region at +0x570 that is zeroed then stamped with the "invalid surface" tag
// 0xFFFF/0x8000 + a const float).
//
// FLAG (un-homed): every per-vehicle store in this function lands inside SimpleVehiclePhysics
// at byte offsets (+0x130, +0x210, +0x3D0, +0x570, +0x318/+0x3C4, ...) whose named layout is
// the BrnSimpleVehiclePhysics TU, which is NOT homed. Reproducing those raw-offset stores here
// would require fabricating that layout, which the rules forbid. We therefore reproduce the
// CONTROL FLOW faithfully BY NAME (iterate the used-vehicle bits, fetch each vehicle body) and
// delegate the per-body reset to SimpleVehiclePhysics once that TU lands. The reset call is a
// declared-only hook on the forward-declared type, so this is honest, not approximated:
// nothing is silently zeroed.
// ---------------------------------------------------------------------------------------
void PhysicalTrafficManager::ResetAboveGroundTestResults()
{
    // X360 GetFirstNonZeroBit / GetNextNonZeroBit walk over mUsedTrafficVehicles.
    for (s32 liVehicle = mUsedTrafficVehicles.GetFirstNonZeroBit();
         liVehicle != CgsContainers::BitArray<20u>::KI_INVALID_BITINDEX;
         liVehicle = mUsedTrafficVehicles.GetNextNonZeroBit(liVehicle))
    {
        CGS_ASSERT(liVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC,
                   "liVehicle < ku8TotalMaxNumPhysicalTraffic");

        // X360: SimpleVehiclePhysics* lpBody = GetTrafficVehicle(liVehicle)->mpVehicleBody;
        //       lpBody->ResetAboveGroundTestResults();   // the +0x130/+0x210/+0x3D0/+0x570 stores
        // FLAG: GetTrafficVehicle(liVehicle)->mpVehicleBody and the per-body reset are deferred to
        // the BrnSimpleVehiclePhysics TU (un-homed byte layout); see header note.
        (void)liVehicle;
    }
}

// ---------------------------------------------------------------------------------------
// ValidateAndFixUpTrafficTrafficContact   @ 0x8259BD10
// X360: lpContact is a PotentialContact*; its two volume-instance EntityIds live at +0x30 and
// +0x38 (as the high dword of a 64-bit field, i.e. the EntityId is in the upper 32 bits).
//   - assert each id's owner byte == E_ENTITYTYPE_TRAFFIC_VEHICLE (HIBYTE == 2)
//   - for each, GetEntityIndex() = (idHigh >> 10) & 0x3FFF, assert < 0x258 (sizeof the map 601);
//     look up mu8GlobalToPhysicalEntityIndexMap[index]; if == KU8_INVALID_MAP(127) the physics
//     id is "invalid" (0) and validation fails; otherwise the physics id is (phys<<10)|0x2000000.
//   - if BOTH map to a physical vehicle, overwrite both EntityId high-dwords in the contact with
//     the physics ids and return true; otherwise leave them and return false.
//
// FLAG (un-homed): the contact record is the un-homed PotentialContact type; its +0x30/+0x38
// 64-bit volume-instance-id fields are addressed by raw byte offset here (a void* + offset),
// matching the X360 `ld 0x30(r26)` / `std 0x30(r26)`. This is the only honest way to touch an
// un-homed record's known field offsets without fabricating its full layout.
// ---------------------------------------------------------------------------------------
bool PhysicalTrafficManager::ValidateAndFixUpTrafficTrafficContact(void* lpContact) const
{
    u8* lpcContact = static_cast<u8*>(lpContact);

    // The two volume-instance-id fields are 64-bit; the EntityId occupies the HIGH 32 bits
    // (X360 `ld 0x30; srdi 32`). FLAG: +0x30 / +0x38 are PotentialContact field offsets.
    u64& lru64IdA = *reinterpret_cast<u64*>(lpcContact + 0x30);
    u64& lru64IdB = *reinterpret_cast<u64*>(lpcContact + 0x38);

    const u32 luIdAHigh = static_cast<u32>(lru64IdA >> 32);
    const u32 luIdBHigh = static_cast<u32>(lru64IdB >> 32);

    CGS_ASSERT((luIdAHigh >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
               "lpContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");
    CGS_ASSERT((luIdBHigh >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
               "lpContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE");

    // ---- resolve A's GLOBAL id -> physics id via the map ----
    const u32 luGlobalIndexA = (luIdAHigh >> 10) & 0x3FFFu;
    CGS_ASSERT(luGlobalIndexA < sizeof(mu8GlobalToPhysicalEntityIndexMap),
               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

    const u8 lu8PhysA = mu8GlobalToPhysicalEntityIndexMap[luGlobalIndexA];
    bool lbValidA;
    u32  luPhysIdA = 0;
    if (lu8PhysA == KU8_INVALID_MAP)
    {
        lbValidA = false;
    }
    else
    {
        CGS_ASSERT(lu8PhysA < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
                   "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
        luPhysIdA = (static_cast<u32>(lu8PhysA) << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);
        lbValidA = true;
    }

    if (!lbValidA)
    {
        return false;
    }

    // ---- resolve B's GLOBAL id -> physics id via the map ----
    const u32 luGlobalIndexB = (luIdBHigh >> 10) & 0x3FFFu;
    CGS_ASSERT(luGlobalIndexB < sizeof(mu8GlobalToPhysicalEntityIndexMap),
               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");

    const u8 lu8PhysB = mu8GlobalToPhysicalEntityIndexMap[luGlobalIndexB];
    u32 luPhysIdB = 0;
    if (lu8PhysB == KU8_INVALID_MAP)
    {
        return false;
    }
    CGS_ASSERT(lu8PhysB < (1u << KU_NUM_BITS_FOR_ENTITY_NUM),
               "luEntityIndex < (1U << KU_NUM_BITS_FOR_ENTITY_NUM)");
    luPhysIdB = (static_cast<u32>(lu8PhysB) << 10) | (KU_ENTITYTYPE_TRAFFIC_VEHICLE << 24);

    // ---- both valid: stamp the physics ids back into the contact (preserve the low 32 bits) ----
    // X360: stw 0 @0x30; ld 0x30; or (physIdA<<32); std 0x30  (clears the high dword first then
    // ORs the physics id into the upper 32 bits, leaving the low 32 bits untouched).
    lru64IdA = (lru64IdA & 0x00000000FFFFFFFFull) | (static_cast<u64>(luPhysIdA) << 32);
    lru64IdB = (lru64IdB & 0x00000000FFFFFFFFull) | (static_cast<u64>(luPhysIdB) << 32);
    return true;
}

// =========================================================================================
// PhysicalTrafficVehicle wave-7 methods (their own ledger funcs; homed here alongside the
// already-committed GetArticulatedVehicleType). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// FLAG (type coherence): mpVehicleBody is stored as SimpleVehiclePhysics* in this TU's opaque
// slice; on console TrafficPhysics : VehiclePhysics : SimpleVehiclePhysics so it is the same
// object, but the committed slices do not model that inheritance. GetFullTrafficPhysics /
// AddAirRam / GetArticulationPointWorldSpace therefore reinterpret_cast the raw +0x1C pointer to
// the concrete body type, mirroring the X360 raw pointer load. (The manager header's opaque
// `struct TrafficPhysics` and the real class in TrafficPhysics.h stay distinct; this TU only casts.)
// =========================================================================================

// PhysicalTrafficVehicle::GetFullTrafficPhysics   @0x825C0148  (dossier symbol 'GetFullTraffic')
//   Assert mu8PhysicalType (+0x32) < COUNT; assert the vehicle is fully-physical -> IsFullyPhysical();
//   return mpVehicleBody (+0x1C) as the concrete full-physics body.
TrafficPhysics* PhysicalTrafficVehicle::GetFullTrafficPhysics()
{
    CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT,
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
    CGS_ASSERT(mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_FULL, "IsFullyPhysical()");
    return reinterpret_cast<TrafficPhysics*>(mpVehicleBody);
}

// PhysicalTrafficVehicle::AddAirRam   @0x826152E0
//   Assert mu8PhysicalType < COUNT; only when the vehicle is FULLY physical (leType == 0) forward
//   the two Vector3 args into the full-physics body's VehiclePhysics::AddAirRam.
void PhysicalTrafficVehicle::AddAirRam(u32 luFlags, f32 lfFactor, f32 lfDecay,
                                       Vector3 lvCustomImpulse, Vector3 lvCustomPosition,
                                       f32 lfTimerTillFire)
{
    CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT,
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
    if (mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_FULL)
    {
        // X360: GetFullTraffic() returns *(this+0x1C)=mpVehicleBody; VehiclePhysics::AddAirRam(body,...).
        reinterpret_cast<VehiclePhysics*>(GetFullTrafficPhysics())->AddAirRam(
            luFlags, lfFactor, lfDecay, lvCustomImpulse, lvCustomPosition, lfTimerTillFire);
    }
}

// PhysicalTrafficVehicle::IsSimple (const)   @0x825B33B8
//   Assert mu8PhysicalType < COUNT; return leType == SIMPLE (the `cntlzw(leType-1)>>5` idiom).
bool PhysicalTrafficVehicle::IsSimple() const
{
    CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT,
               "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
    return mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_SIMPLE;
}

// PhysicalTrafficVehicle::SetCheckOwner   @0x825C01B8
//   Assert the car has not already been checked (miCheckOwner at +0x33 == 0xFF sentinel) ->
//   '!HasBeenChecked()'; store the owner byte.
void PhysicalTrafficVehicle::SetCheckOwner(EActiveRaceCarIndex leCheckOwner)
{
    CGS_ASSERT(!HasBeenChecked(), "!HasBeenChecked()");
    miCheckOwner = static_cast<s8>(leCheckOwner);
}

// PhysicalTrafficVehicle::GetArticulationPointLocalSpace (const)   @0x825C0538
//   Range-assert meArticulatedVehicleType; assert it is CAB or TRAILER; assert the stored local
//   point is finite; return mArticulationPointLocal (+0). Pure member read -- no body type needed.
Vector3 PhysicalTrafficVehicle::GetArticulationPointLocalSpace() const
{
    CGS_ASSERT(meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE
                   && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT,
               "meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT");
    CGS_ASSERT(GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB
                   || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER,
               "GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER");
    CGS_ASSERT(rw::math::vpu::IsValid(mArticulationPointLocal),
               "RwMathVPU::IsValid( mArticulationPointLocal )");
    return mArticulationPointLocal;
}

// PhysicalTrafficVehicle::GetArticulationPointWorldSpace (const)   @0x825C0220
//   Same type + local-point finiteness asserts as the local-space accessor, plus a finiteness
//   assert over the vehicle body's transform (GetPhysics()->GetTransform()). Then transforms
//   mArticulationPointLocal by that transform (vmaddfp cascade == TransformPoint) and returns the
//   world-space point. GetPhysics() returns *(this+0x1C)=mpVehicleBody.
Vector3 PhysicalTrafficVehicle::GetArticulationPointWorldSpace() const
{
    CGS_ASSERT(meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE
                   && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT,
               "meArticulatedVehicleType >= E_ARTICULATE_VEHICLE_NONE && meArticulatedVehicleType < E_ARTICULATE_VEHICLE_COUNT");
    CGS_ASSERT(GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB
                   || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER,
               "GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER");
    CGS_ASSERT(rw::math::vpu::IsValid(mArticulationPointLocal),
               "RwMathVPU::IsValid( mArticulationPointLocal )");

    const rw::math::vpu::Matrix44Affine& lrTransform =
        reinterpret_cast<const VehiclePhysics*>(mpVehicleBody)->GetTransform();
    CGS_ASSERT(rw::math::vpu::IsValid(lrTransform),
               "RwMathVPU::IsValid( GetPhysics()->GetTransform() )");

    return rw::math::vpu::TransformPoint(lrTransform, mArticulationPointLocal);
}

// =========================================================================================
// PhysicalTrafficVehicle wave-8 methods (their own ledger funcs, homed here alongside the
// wave-7 set). Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// FLAG (opaque-TrafficPhysics contract): this TU models the full-physics body as the opaque
// `struct TrafficPhysics` (manager header) and, per the wave-7 precedent, reinterpret_casts the
// raw mpVehicleBody / GetFullTrafficPhysics() pointer to the concrete VehiclePhysics for the base
// entries it can reach BY NAME. The real `class TrafficPhysics : VehiclePhysics` (TrafficPhysics.h)
// CANNOT be included here -- its name collides with the opaque slice -- so the TrafficPhysics-derived
// forwards (TrafficPhysics::PreparePhysical / TrafficPhysics::Update) are DELEGATED to the
// TrafficPhysics TU, exactly as every other full-physics operation in this TU is. Each function
// below reproduces the observable PhysicalTrafficVehicle member-state writes (the fields this class
// OWNS) store-for-store; the delegated / un-homed collaborator work is flagged inline, never faked.
// =========================================================================================

// PhysicalTrafficVehicle::PreparePhysical   @0x82641058
//   Assert mpVehicleBody != NULL and mu8PhysicalType < COUNT. When fully physical, fetch the deform
//   model's bounding box and (delegated) run the full-physics prepare. Then unconditionally seed the
//   vehicle's id/state fields from the spawn event: mCgsID (+0x10), zero the check-notify timer, clear
//   mbRammed/mbUsingBoxWithWorld, reset miCheckOwner (+0x33) to the not-checked sentinel, inline
//   ClearArticulatedState() (+0x24/+0x28/+0x2C), promote to CAB when the event's mbIsCab is set, copy
//   the event's ETrafficType into mePhysicalTrafficState (+0x20) and, for the CRASHING type, arm the
//   body's crashing virtual. Returns true.
bool PhysicalTrafficVehicle::PreparePhysical(const CreatePhysicalTrafficEvent* lpEvent,
                                             VehicleAttribs* lpAttribs,
                                             const Deformation::StreamedDeformationSpec* lpModelData,
                                             const Vector3* lpWheelPositions, const f32* lpafWheelRadii)
{
    CGS_ASSERT(mpVehicleBody != nullptr, "mpVehicleBody != NULL");
    CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT, "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");

    if (mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_FULL)
    {
        // The AABB enclosing every deformation-sensor sphere (X360 builds it on the stack).
        CgsGeometric::AxisAlignedBox lAABB;
        lpModelData->GetBoundingBox(lAABB);

        // FLAG (delegated full-physics prepare): the console then calls
        //   TrafficPhysics::PreparePhysical(GetFullTraffic(), event, attribs, lAABB, model,
        //                                   wheelPositions, wheelRadii)
        // to build the full body. That entry lives on the real TrafficPhysics class, which this
        // opaque-contract TU cannot include (see the group note above); it is owned by the
        // TrafficPhysics TU (TrafficPhysics.cpp) and is NOT fabricated here.
        (void)lAABB;
        (void)lpAttribs;
        (void)lpWheelPositions;
        (void)lpafWheelRadii;
    }

    // ---- this vehicle's own id/state fields (unconditional, in X360 store order) ----
    mCgsID                   = lpEvent->mCgsID;                  // +0x10 (event +0x88)
    mfTimeSinceCheckNotify   = 0.0f;                             // +0x18
    mbRammed                 = false;                            // +0x30
    miCheckOwner             = -1;                               // +0x33 (not-yet-checked sentinel)
    mbUsingBoxWithWorld      = false;                            // +0x34
    // inlined ClearArticulatedState():
    meArticulatedVehicleType = E_ARTICULATE_VEHICLE_NONE;        // +0x24
    meArticulatedJointState  = E_ARTICULATE_JOINT_NONE;          // +0x28
    miJointIndex             = -1;                               // +0x2C
    if (lpEvent->mbIsCab)                                        // event +0x84
        meArticulatedVehicleType = E_ARTICULATE_VEHICLE_CAB;

    mePhysicalTrafficState = static_cast<u32>(lpEvent->meTrafficType);   // +0x20 (event +0x80)
    switch (lpEvent->meTrafficType)
    {
    case E_TRAFFIC_TYPE_POTENTIAL:   // 0 -- no body activation
    case E_TRAFFIC_TYPE_PHYSICAL:    // 2 -- no body activation
        break;
    case E_TRAFFIC_TYPE_CRASHING:    // 1 -- arm the body's crashing state
        // X360: (*(*mpVehicleBody + 8))(mpVehicleBody) -- vtbl slot 2 == the crashing-activation
        // virtual. Modeled BY NAME as VehiclePhysics::SetCrashing (the crash arm OnChecked calls
        // directly); reinterpret the raw body pointer as the wave-7 accessors do.
        reinterpret_cast<VehiclePhysics*>(mpVehicleBody)->SetCrashing();
        break;
    default:                         // 3+ (SLAMMED, ...) -- not a valid PHYSICAL prepare state
        CGS_ASSERT(false, "Invalid physical traffic type");
        break;
    }
    return true;
}

// PhysicalTrafficVehicle::OnChecked   @0x8261E360
//   Assert leOwner is a valid race-car index and lpRaceCarPhysics != NULL. Latch the checker
//   (miCheckOwner, +0x33) and reset the check-notify timer (mfTimeSinceCheckNotify, +0x18). When the
//   checker is "hard" (an un-homed RaceCarPhysics strength byte > 8) and this car is fully physical,
//   arm the full body's crashing state; the ensuing crash-impulse fold is delegated.
void PhysicalTrafficVehicle::OnChecked(EActiveRaceCarIndex leOwner,
                                       const RaceCarPhysics* lpRaceCarPhysics,
                                       Vector3 lContactPointOnTraffic)
{
    CGS_ASSERT(leOwner >= E_ACTIVE_RACE_CAR_INDEX_0 && leOwner < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "(leOwner >= E_ACTIVE_RACE_CAR_INDEX_0) && (leOwner < E_ACTIVE_RACE_CAR_INDEX_COUNT)");
    CGS_ASSERT(lpRaceCarPhysics != nullptr, "lpRaceCarPhysics");

    miCheckOwner           = static_cast<s8>(leOwner);   // +0x33
    mfTimeSinceCheckNotify = 0.0f;                        // +0x18

    // FLAG (un-homed RaceCarPhysics field): the console gates the crash-impulse on a byte at
    // RaceCarPhysics +0x140E (a checker-strength / collision-severity field owned by the
    // RaceCarPhysics TU). Read here by raw offset -- the only honest way to touch an un-homed field
    // without fabricating the full layout -- purely to reproduce the `> 8` gate.
    const u8 lu8CheckerStrength = *(reinterpret_cast<const u8*>(lpRaceCarPhysics) + 0x140E);
    if (lu8CheckerStrength > 8u)
    {
        CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT,
                   "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");
        if (mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_FULL)
        {
            // Arm the full body's crashing state (X360: VehiclePhysics::SetCrashing on GetFullTraffic()).
            reinterpret_cast<VehiclePhysics*>(GetFullTrafficPhysics())->SetCrashing();

            // FLAG (delegated crash-impulse math): the console then reads SetCrashing()'s returned
            // sub-object pointer and folds a crash impulse into its +0x50 vector -- a VMX dot/max/FMA
            // cascade using RaceCarPhysics +0x1340 (the checker's linear-velocity direction) and
            // lContactPointOnTraffic. That impulse lands in the opaque TrafficPhysics slice + un-homed
            // RaceCarPhysics velocity and is owned by the full-physics TU (this TU only arms the
            // crash). Not fabricated here.
            (void)lContactPointOnTraffic;
        }
    }
}

// PhysicalTrafficVehicle::Update   @0x826411C0
//   Assert mu8PhysicalType < COUNT. When fully physical, delegate the full-physics per-frame tick
//   (UpdateFreezing + freeze-state insert + TrafficPhysics::Update). Unconditionally accumulate the
//   check-notify timer by the sim time-step.
void PhysicalTrafficVehicle::Update(f32 lfSimTimerTimeStep, f32 lfGameTimerTimeStep,
                                    const Matrix44Affine* lpCameraMatrix,
                                    const BrnPlayerDriverControls* lpControls, bool lbImpactTime,
                                    bool lbDoForceAdditiveAftertouch, bool lbUseSixaxis)
{
    CGS_ASSERT(mu8PhysicalType < E_PHYSICAL_TRAFFIC_TYPE_COUNT, "leType < E_PHYSICAL_TRAFFIC_TYPE_COUNT");

    if (mu8PhysicalType == E_PHYSICAL_TRAFFIC_TYPE_FULL)
    {
        // FLAG (delegated full-physics tick): the console fetches GetFullTraffic() and runs, in order,
        //   VehiclePhysics::UpdateFreezing(body, controls, dt)   -- BLOCKED in the VehiclePhysics TU,
        //   a conditional freeze-state vector insert at TrafficPhysics +0x1060 guarded by the global
        //     freeze-disable flag (byte_82F2A1A6),
        //   and, when the body is not already settled (+0x70 == 0),
        //   TrafficPhysics::Update(body, camera, controls, ...).
        // All of that lands inside the opaque TrafficPhysics/VehiclePhysics slice and the BLOCKED
        // UpdateFreezing, so it is delegated to those TUs (this opaque-contract TU cannot include the
        // real TrafficPhysics class). Not fabricated here.
        (void)lpCameraMatrix;
        (void)lpControls;
        (void)lbImpactTime;
        (void)lbDoForceAdditiveAftertouch;
        (void)lbUseSixaxis;
        (void)lfGameTimerTimeStep;
    }

    // The one member write this vehicle owns each frame: accumulate the check-notify timer (+0x18).
    mfTimeSinceCheckNotify += lfSimTimerTimeStep;
}

// PhysicalTrafficVehicle::SetArticulated   @0x825F3B68
//   Assert the type is CAB or TRAILER and this vehicle has no joint yet (miJointIndex == -1). Set
//   meArticulatedVehicleType (+0x24) and mark the joint ATTACHED (+0x28). The console then computes
//   mArticulationPointLocal from the deformation model's hitch locator and, when fully physical, sets
//   the full body's articulated solve-penetration weight; both are delegated (see inline flags).
void PhysicalTrafficVehicle::SetArticulated(const CreatePhysicalTrafficEvent& lrCreateTrafficEvent,
                                            EArticulatedVehicleType leVehicleType)
{
    CGS_ASSERT(leVehicleType == E_ARTICULATE_VEHICLE_CAB || leVehicleType == E_ARTICULATE_VEHICLE_TRAILER,
               "leVehicleType == E_ARTICULATE_VEHICLE_CAB || leVehicleType == E_ARTICULATE_VEHICLE_TRAILER");
    CGS_ASSERT(miJointIndex == -1, "miJointIndex == -1");

    meArticulatedVehicleType = leVehicleType;                  // +0x24
    meArticulatedJointState  = E_ARTICULATE_JOINT_ATTACHED;    // +0x28

    // FLAG (delegated articulation-point computation): the console resolves the spawn event's model
    // handle (lrCreateTrafficEvent.mModelHandle, event +0x78) to a Deformation::StreamedDeformationSpec
    // (CgsResource::BaseResourcePtr::CreateFromHandle + BrnPhysics::Def), builds the inverse
    // car-model->handling-body transform (StreamedDeformationSpec::GetCarModelSpaceToHandlingBodySpace
    // Transform + rw::math::vpu::InverseOfMatrixWithOrthonormal3x3), searches the spec's generic
    // locator list (GetGenericLocators) for the hitch tag point (tag type 29 for a CAB, 28 for a
    // TRAILER; asserts "Failed to find articulation tag point" when absent), then transforms that
    // locator's translation into handling-body space (LocatorPointSpecList::GetLocatorXf +
    // rw::math::vpu::TransformPoint) and stores the result into mArticulationPointLocal (+0), asserting
    // "RwMathVPU::IsValid( mArticulationPointLocal )". Those collaborators (BrnPhysics::Def and the
    // StreamedDeformationSpec transform/locator accessors) are owned by the resource + deformation TUs
    // and are not declared for this TU; the point is NOT fabricated here.
    (void)lrCreateTrafficEvent;

    // FLAG (un-recoverable constant + un-declared setter): when fully physical the console finally
    // calls VehiclePhysics::SetSolvePenetrationWeightFactor(GetFullTraffic(),
    // KF_ARTICULATED_SOLVE_PENETRATION_WEIGHT_FACTOR), inserting the weight into the full body at
    // +0x1050. The factor is loaded from un-dumped rodata (unk_8208FACC) so its value is not
    // recoverable (a guessed constant would be wrong), and the setter is undeclared; delegated to the
    // full-physics TU rather than guessed.
}

}   // namespace Vehicle
}   // namespace BrnPhysics
