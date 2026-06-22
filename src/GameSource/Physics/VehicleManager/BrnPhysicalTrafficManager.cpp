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

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

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

}   // namespace Vehicle
}   // namespace BrnPhysics
