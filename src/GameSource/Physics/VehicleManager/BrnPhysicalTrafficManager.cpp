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
// ⭐ 2026-08-10 (producer wave) -- PrepareTriangleCache's collaborators.
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO.h"             // SceneManagerIO::InputBuffer_Update::GetInSceneUpdateInterface (@0x825BD8C0, write-lock twin)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // InSceneUpdateInterface::mAddToCacheQueue (X360 +0xC4930)
#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"               // VehicleManager::KI_MAX_ACTIVE_RACE_CARS (the +8 slot bias)

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
// PhysicalTrafficManager (constructor)   @ 0x827E42E8  -- MOVED, NOT DELETED.
//
// ⭐ 2026-08-03 (the un-pin wave): the body (and its full FLAG commentary) now lives INLINE in
// BrnPhysicalTrafficManager.h, verbatim. It had to move because VehicleManager embeds this class
// by value now, VehicleManager is embedded by value in PhysicsModule, and PhysicsModule's ctor is
// MOUNTED -- so the implicit constructor chain references this symbol from mounted code while THIS
// TU is still unmounted (two live ODR forks). Defining it here as well would be a duplicate symbol
// the day this TU is mounted, so there is exactly one definition and it is the inline one.
// Nothing was dropped: same eight member initialisers, same three FLAG notes.
// ---------------------------------------------------------------------------------------

// ---------------------------------------------------------------------------------------
// PhysicalTrafficManager::Construct   @ 0x82636CA8   (99 instructions)
// DWARF BrnPhysicalTrafficManager.h:475 -- `void Construct()`. Its ONLY caller in the image is
// VehicleManager::Construct @0x8263B7C8.
//
// EVERY offset in the X360 body was decoded and every one lands on a name-matching DWARF member
// (the base for each `addis rN, r31, 2` group is 131072 minus the displacement):
//     20 x TrafficPhysics::Construct(this + i*0x1430)              maFullTrafficPhysics[20]
//     20 x stwx -1 at 4*(i + 0x64F0) == 4*i + 103360               maTrafficEntityIDs[20]
//      4 x stwx 0  at 0x194B0/B4/B8/BC == 103600/604/608/612       the four pool pointers
//          ArticulatedJointPool::Construct(this + 103616)          mArticulatedJointPool
//          stfsx 2.0f @104448, 2.0f @104452, 10.0f @104456         the three joint-break limits
//          stbx  0    @104460                                      mbAllowArticulatedJointBreaking
//      8 x std 0 at 104552/560/568/576/584/600/608/616             8 of the 9 traffic bitsets
//          EventQueue<char,50>::Construct(this + 104624)           mUnusedPotentialTrafficQueue
//          mDebugComponent.Construct(this)  (inlined, this+105616)
//
// ⭐ The chain closes to the byte, which is what re-seats mArticulatedJointPool to 832 (see the
// header banner, finding (1)): 104624 + 64 == 104688 == mu8GlobalToPhysicalEntityIndexMap, +600
// == 105288, 16-aligned -> 105296, + 20*16 == 105616 == mDebugComponent.
//
// ⚠️ `mTestedTrafficVehicles` IS NOT CLEARED. Nine BitArrays sit at 104552..104624 at stride 8;
// the body writes eight of them and skips exactly the one at 104592. That is transcribed here as
// an omission, not "fixed" -- do not add it without new evidence.
//
// The three joint-break limits are plain scalar-pool constants read out of the image:
// flt_82001D9C == 2.0 (used twice) and flt_82004A20 == 10.0. Neither is a static-init slot.
//
// FLAG (un-homed callees): TrafficPhysics::Construct @0x8262E980 is an .ida-exports HOLE and
// ArticulatedJointPool::Construct @0x82600938 lives with the real pool class in
// BrnArticulatedJointPool.cpp; both are called BY NAME through declare-only declarations on the
// sliced types in this header (see the ODR-fork finding (2) there).
// ---------------------------------------------------------------------------------------
void PhysicalTrafficManager::Construct()
{
    for (s32 liVehicle = 0; liVehicle < static_cast<s32>(KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC); ++liVehicle)
    {
        maFullTrafficPhysics[liVehicle].Construct();   // bl TrafficPhysics::Construct, stride 0x1430
    }

    for (s32 liVehicle = 0; liVehicle < static_cast<s32>(KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC); ++liVehicle)
    {
        // stwx r10(-1): the invalid-EntityId sentinel (CgsEntityId.h KU_INVALID_ENTITY_ID).
        maTrafficEntityIDs[liVehicle].muValue = 0xFFFFFFFFu;
    }

    mpaTrafficDrivers              = 0;   // stwx 0, this + 103600
    mpaTrafficVehicles             = 0;   // stwx 0, this + 103604
    mpaSimpleVehiclePhysics        = 0;   // stwx 0, this + 103608
    mpArticulatedJointCreateBuffer = 0;   // stwx 0, this + 103612

    mArticulatedJointPool.Construct();    // bl ArticulatedJointPool::Construct(this + 103616)

    mfJointSwingBreakVelocity       = 2.0f;   // stfsx flt_82001D9C, this + 104448
    mfJointTwistBreakVelocity       = 2.0f;   // stfsx flt_82001D9C, this + 104452
    mfJointLinearBreakMph           = 10.0f;  // stfsx flt_82004A20, this + 104456
    mbAllowArticulatedJointBreaking = false;  // stbx  0,            this + 104460

    mUsedTrafficVehicles.UnSetAll();            // std 0, this + 104552
    mUsedFullTrafficPhysics.UnSetAll();         // std 0, this + 104560
    mUsedSimpleVehiclePhysics.UnSetAll();       // std 0, this + 104568
    mPotentialTrafficVehicles.UnSetAll();       // std 0, this + 104576
    mTrafficDeformationModelsActive.UnSetAll(); // std 0, this + 104584
    // ⚠️ mTestedTrafficVehicles (this + 104592) is deliberately NOT written -- see the banner.
    mAddedTrafficVehicles.UnSetAll();           // std 0, this + 104600
    mRemovedTrafficVehicles.UnSetAll();         // std 0, this + 104608
    mMadeSimpleTrafficVehicles.UnSetAll();      // std 0, this + 104616

    mUnusedPotentialTrafficQueue.Construct();   // bl EventQueue<char,50>::Construct(this + 104624)

    mDebugComponent.Construct(this);            // inlined @0x82636DF8..0x82636E28
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

// PhysicalTrafficManager::ProcessAddAirRamEvent   @0x8261DC08   (121 instructions)
//   VehicleManager::UpdateVehicleEffects @0x82629E18 forwards every owner==TRAFFIC air-ram
//   event here. The X360, branch for branch:
//     * assert the event volume id's owner byte is TRAFFIC_VEHICLE (2) -- the streamed
//       "Tried to use physical traffic manager to add air ram to non-traffic thing: <id>"
//       (BrnPhysicalTrafficManager.cpp:3529) lowered to CGS_ASSERT per the standing rule;
//       fire-and-continue.
//     * inlined global->physical map lookup (the same shape GetTrafficPhysicsEntityID* uses):
//       bounds tripwire index < 600 ("lGlobalEntityId.GetEntityIndex() <
//       sizeof(mu8GlobalToPhysicalEntityIndexMap)", BrnPhysicalTrafficManager.h:944), then
//       mu8GlobalToPhysicalEntityIndexMap[idx]; KU8_INVALID_MAP (127) == no physical vehicle
//       -> the event is dropped. A hit repacks (physIdx << 10) | (TRAFFIC << 24) -- the seed
//       value before the repack is the K_INVALID_ENTITY_ID global (dword_82F2A3A4), never
//       read on the drop path, so it is not reproduced.
//     * GetTrafficVehicle(physIdx) [the asm's GetTrafficInterest_0 alias -- both bodies are
//       &mpaTrafficVehicles[idx]] -> PhysicalTrafficVehicle::AddAirRam with the same
//       field->argument map UpdateVehicleEffects uses for race cars: (muEffectFlags,
//       mDirectionAndMagnitude.w, mfDecay, mDirectionAndMagnitude, mPosition, mfStartTime)
//       == asm r4 / f1(+0x1C) / f2(+0x0C) / v1(+0x10) / v2(+0x20) / f3(+0x30).
//   ⚠️ HOST DIVERGENCE, flagged: for a map index >= 600 the console runs the map read anyway
//   (an OOB read feeding the sentinel test); the host guards it -- the same accepted
//   divergence as BrnVehicleManager_ValidateSimulationContacts.cpp's bounds tripwire.
void PhysicalTrafficManager::ProcessAddAirRamEvent(const CreateAirRamEvent* lpEvent)
{
    const u32 luEntityWord = static_cast<u32>(lpEvent->mVolumeId.muId >> 32);

    CGS_ASSERT((luEntityWord >> 24) == KU_ENTITYTYPE_TRAFFIC_VEHICLE,
               "Tried to use physical traffic manager to add air ram to non-traffic thing: ");   // :3529

    const u32 luGlobalIndex = (luEntityWord >> 10) & 0x3FFFu;
    CGS_ASSERT(luGlobalIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap),
               "lGlobalEntityId.GetEntityIndex() < sizeof(mu8GlobalToPhysicalEntityIndexMap)");  // h:944

    if (luGlobalIndex < sizeof(mu8GlobalToPhysicalEntityIndexMap))   // host bounds guard (see banner)
    {
        const u8 lu8PhysicalIndex = mu8GlobalToPhysicalEntityIndexMap[luGlobalIndex];
        if (lu8PhysicalIndex != KU8_INVALID_MAP)
        {
            // The console passes the raw 16-byte mDirectionAndMagnitude register as v1 (its w
            // lane rides along); reproduced lane-for-lane rather than through GetVector3(),
            // which would zero the w lane the console keeps.
            const Vector3 lvDirection{ lpEvent->mDirectionAndMagnitude.x,
                                       lpEvent->mDirectionAndMagnitude.y,
                                       lpEvent->mDirectionAndMagnitude.z,
                                       lpEvent->mDirectionAndMagnitude.w };
            GetTrafficVehicle(static_cast<s32>(lu8PhysicalIndex))->AddAirRam(
                lpEvent->muEffectFlags,
                lpEvent->mDirectionAndMagnitude.w,   // lfFactor == the packed magnitude lane (asm f1 <- +0x1C)
                lpEvent->mfDecay,
                lvDirection,
                lpEvent->mPosition,
                lpEvent->mfStartTime);
        }
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

// PhysicalTrafficVehicle::HasNonBrokenJoint (const)   @0x825B3418
//   Range-assert meArticulatedJointState (+0x28); then validate the joint invariants per state:
//   NONE -> miJointIndex (+0x2C) must be the -1 sentinel; ATTACHED -> miJointIndex >= 0 and the
//   articulated vehicle type is CAB or TRAILER; any other state fires the "Invalid joint state"
//   assert. Returns true only for the ATTACHED state. Pure member reads -- no body type needed.
bool PhysicalTrafficVehicle::HasNonBrokenJoint() const
{
    CGS_ASSERT(meArticulatedJointState >= E_ARTICULATE_JOINT_NONE
                   && meArticulatedJointState < E_ARTICULATE_JOINT_COUNT,
               "meArticulatedJointState >= E_ARTICULATE_JOINT_NONE && meArticulatedJointState < E_ARTICULATE_JOINT_COUNT");

    if (meArticulatedJointState == E_ARTICULATE_JOINT_NONE)
    {
        CGS_ASSERT(miJointIndex == -1, "miJointIndex == -1");
    }
    else if (meArticulatedJointState == E_ARTICULATE_JOINT_ATTACHED)
    {
        CGS_ASSERT(miJointIndex >= 0, "miJointIndex >= 0");
        CGS_ASSERT(GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB
                       || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER,
                   "GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_CAB || GetArticulatedVehicleType() == E_ARTICULATE_VEHICLE_TRAILER");
    }
    else
    {
        // X360: an inlined StrStream builds "Invalid joint state: <meArticulatedJointState>" and
        // fires the assert at BrnPhysicalTrafficVehicle.h:628. The streamed value is approximated by
        // the static prefix (same convention as the other StrStream asserts in this TU); the branch
        // is only reachable for an out-of-range state, so the fire is unconditional here.
        CGS_ASSERT(false, "Invalid joint state: ");
    }

    return meArticulatedJointState == E_ARTICULATE_JOINT_ATTACHED;
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

// =================================================================================================
// PhysicalTrafficManager::ValidateTrafficContact  @0x825CACB8  (PS3 DecFIGS 0x6E5DF8)
//
// ⚠⚠ TRAP STUB (closure enforcement, 2026-08-06 big-five #2 wave) -- the REAL body (169 X360 asm
// lines, 6 callees: validate one traffic-vs-world potential contact against the vehicle-input
// triangle cache) is NOT reconstructed yet. Dead code today: the only caller chain is
// VehicleManager::ValidateTrafficContact <- PhysicsModule::BridgeContactsToSimulation <-
// Update @0x825B0640, still a link stub, so /OPT:REF strips this. RECONSTRUCT-NEXT.
// =================================================================================================
bool PhysicalTrafficManager::ValidateTrafficContact(
    CgsSceneManager::SceneManagerIO::PotentialContact* /*lpContact*/,
    const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* /*lpTriCacheInterface*/,
    f32 /*lfTimeStep*/)
{
    CGS_ASSERT(false,
               "TRAP: PhysicalTrafficManager::ValidateTrafficContact @0x825CACB8 "
               "not reconstructed (big-five #2 closure stub)\n");
    return false;
}

// =================================================================================================
// PhysicalTrafficManager::PrepareTriangleCache  @0x825EE5A0  (39 insns)
//
// ⭐ ADDED 2026-08-10 (producer wave). Claims this manager's 20 triangle-cache slots. Decoded off
// the ASM (the Hex-Rays view is faithful here, but the two stack slots are what carry the record):
//
//   0x825EE5B8  cmplwi r31, 0  -> assert "lpSceneInputBuffer_Update != NULL"
//                                 BrnPhysicalTrafficManager.cpp:244  (li r5, 0xF4)
//   0x825EE5E4  bl  CgsSceneManager::SceneManagerIO::InputBuffer_Update::
//                   GetInSceneUpdateInterface      <- @0x825BD8C0, the NON-CONST (write-lock,
//                                                     bit 3) twin. Correct: the whole Prepare
//                                                     stage runs under lpSceneInput->LockForWrite().
//   0x825EE5EC  addis r30, r3, 0xC ; addi r30, r30, 0x4930   -> +0xC4930 == mAddToCacheQueue
//   0x825EE5F8  lfs   f0, flt_8200426C ; stfs f0, var_1C(r1) -> the radius, hoisted OUT of the loop
//   0x825EE600  addi  r11, r31, 8      ; stw r11, var_20(r1) -> miCacheSlot = i + 8
//   0x825EE610  bl    BaseEventQueue<InEventAddToCache>::AddEvent
//   0x825EE618  cmpwi r31, 0x14        -> 20 iterations == KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC
//   return true
//
// ⭐ THE RADIUS IS READ FROM THE IMAGE, NOT FROM HEX-RAYS: `flt_8200426C` == 0x40A00000 == 5.0f
// (x360rd.py, whose 10-point calibration passed on the same read).
// ⭐ THE +8 BIAS IS THE RACE-CAR BLOCK: VehicleManager::PrepareTriangleCache claims 0..7 first and
// then calls this, so traffic owns 8..27. Spelled as KI_MAX_ACTIVE_RACE_CARS rather than the
// literal 8 -- that is what the bias IS, and the two counts are the same two array bounds.
//
// ⚠️ AS-SHIPPED: `this` is never read (r3 is dead after the prologue) and the return value is the
// constant 1. Both are reproduced rather than "cleaned up": the caller tests the return.
// =================================================================================================
bool PhysicalTrafficManager::PrepareTriangleCache(
    CgsSceneManager::SceneManagerIO::InputBuffer_Update* lpSceneInputBuffer_Update)
{
    CGS_ASSERT(lpSceneInputBuffer_Update != NULL, "lpSceneInputBuffer_Update != NULL");   // :244

    CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpSceneUpdate =
        lpSceneInputBuffer_Update->GetInSceneUpdateInterface();

    for (s32 liTrafficVehicle = 0;
         liTrafficVehicle < static_cast<s32>(KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC);
         ++liTrafficVehicle)
    {
        CgsSceneManager::TriangleCacheManagerIO::InEventAddToCache lEvent;
        lEvent.miCacheSlot         = liTrafficVehicle + VehicleManager::KI_MAX_ACTIVE_RACE_CARS;
        lEvent.mfCacheSphereRadius = KF_TRIANGLE_CACHE_SPHERE_RADIUS;
        lpSceneUpdate->mAddToCacheQueue.AddEvent(lEvent);
    }

    return true;
}

}   // namespace Vehicle
}   // namespace BrnPhysics
