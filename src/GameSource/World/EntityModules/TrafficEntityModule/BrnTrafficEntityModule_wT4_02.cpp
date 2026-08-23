// ============================================================================
// BrnTrafficEntityModule_wT4_02.cpp
//
//   TrafficEntityModule::BuildPotentialCollisionList  @0x8274B378   (78 insns)
//   TrafficEntityModule::HandleHalfPotentialContact   @0x82747F58   (~120 insns)
//
// THE OVERLAP-PAIR HALF OF THE PROMOTION CHAIN. Once UpdateCollidableVehicles (_wT4_01.cpp)
// gives a traffic car a scene collision volume, the broad phase starts emitting race-car-vs-
// traffic overlap pairs; the scene publishes them RAW (BridgeOverlapGenerationToOutputBuffer
// deliberately ignores mbCull) and WorldModule::BridgeSceneContactsToTrafficModule_PrePhysics
// @0x827ABC50 -- LIVE and mounted -- copies them into InputBuffer_PrePhysics. These two
// functions are what turns such a pair into a physics body.
//
// MOUNT REQUIRED (conductor-owned): add
//   echo "%SRC%\GameSource\World\EntityModules\TrafficEntityModule\BrnTrafficEntityModule_wT4_02.cpp"
// to tools/build/build_game_exe.bat beside the other TrafficEntityModule mounts, in the SAME
// change that retires the gate in _wQ7_01.cpp, or the exe link fails with LNK2019.
//
// DELIBERATELY NOT HARDENED. This route does NOT go through SafeRequestMakeVehiclePhysical:
// it does not test mbTrafficIsHidden, does not read GetPhysicalReason and does not consult the
// 25-slot maTrafficPhysicsInfoListBits budget. That is the console (0x827480AC jumps straight
// to AddVehicleToPhysics). The real budget is the physics side's 20 slots, whose recycler
// GetLeastInterestingFullyPhysicalVehicle + RecycleTrafficVehicle is already bodied.
//
// GLOBAL vs PHYSICAL, the recurring wave-3 bug: `(id >> 10) & 0x3FFF` is the scene EntityId's
// 14-bit entity index, i.e. a GLOBAL traffic index in [0,600), and AddVehicleToPhysics's
// luVehicle is global too. The 20-slot PHYSICAL index only exists on the far side of
// PhysicalTrafficManager::mu8GlobalToPhysicalEntityIndexMap. Do not cross them.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficConstants.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficVehicle.h"
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"   // ETrafficType

#include "GameShared/GameClasses/Containers/CgsBitArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventOutOverlapPair.h"

namespace BrnTraffic
{
namespace
{
    // The scene owner byte a traffic volume-instance id carries (E_ENTITYTYPE_TRAFFIC).
    // `srwi r11, r30, 24 ; cmplwi r11, 2` at 0x8274B44C / 0x8274B474.
    const u32 KU_TRAFFIC_ENTITY_OWNER = 2;

    inline u32 GetEntityWordOwner(u32 luEntityWord)
    {
        return luEntityWord >> 24;
    }

    // `extrwi r29, r4, 14, 8` == the EntityId's 14-bit entity index at bit 10.
    inline u32 GetEntityWordIndex(u32 luEntityWord)
    {
        return (luEntityWord >> 10) & 0x3FFFu;
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::BuildPotentialCollisionList  @ 0x8274B378   (.cpp 5550)
//
// Complete, 0x8274B408..0x8274B4AC. Each 24-byte OutOverlapPair carries two 64-bit
// VolumeInstanceIds; the scene EntityId lives in each one's HIGH dword (`ld` then `srdi 32`),
// and the owner byte is that word's top byte. Both halves are tested, so a traffic-vs-traffic
// pair promotes BOTH cars.
//
// The console re-fetches the queue inside the loop (0x8274B420 calls the getter every
// iteration); that is the compiler rematerialising an inlined accessor whose only side effect
// is an idempotent read-lock assert, so it is hoisted to one local here.
// ----------------------------------------------------------------------------
void TrafficEntityModule::BuildPotentialCollisionList(
        const BrnTrafficIO::InputBuffer_PrePhysics* lpInput,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        TotalTrafficBitArray* lpCreatedBodies)
{
    CGS_ASSERT(lpInput != 0,         "lpInput != NULL");           // baked .cpp 5550
    CGS_ASSERT(lpOutput != 0,        "lpOutput != NULL");          // baked .cpp 5551
    CGS_ASSERT(lpCreatedBodies != 0, "lpCreatedBodies != NULL");   // baked .cpp 5552

    if (lpInput == 0 || lpOutput == 0 || lpCreatedBodies == 0)   // PC-safety guard
    {
        return;
    }

    typedef CgsSceneManager::SceneManagerIO::OutOverlapPair OutOverlapPair;

    const BrnTrafficIO::InputBuffer_PrePhysics::OverlapPairsQueue* lpOverlapPairs =
        lpInput->GetOverlapPairsQueue();

    const s32 liLength = static_cast<s32>(lpOverlapPairs->GetLength());

    for (s32 liPair = 0; liPair < liLength; ++liPair)
    {
        const OutOverlapPair& lrPair = lpOverlapPairs->GetEvent(liPair);

        const u64 lu64IdA = lrPair.muVolumeInstanceIdA.muId;
        const u64 lu64IdB = lrPair.muVolumeInstanceIdB.muId;

        const u32 luWordA = static_cast<u32>(lu64IdA >> 32);
        const u32 luWordB = static_cast<u32>(lu64IdB >> 32);

        if (GetEntityWordOwner(luWordA) == KU_TRAFFIC_ENTITY_OWNER)
        {
            HandleHalfPotentialContact(luWordA, lu64IdA, luWordB, lpOutput, lpCreatedBodies);
        }
        if (GetEntityWordOwner(luWordB) == KU_TRAFFIC_ENTITY_OWNER)
        {
            HandleHalfPotentialContact(luWordB, lu64IdB, luWordA, lpOutput, lpCreatedBodies);
        }
    }
}

// ----------------------------------------------------------------------------
// TrafficEntityModule::HandleHalfPotentialContact  @ 0x82747F58
//
// Complete, 0x82747F64..0x827480CC. The two flag tests are the console's byte read of mxFlags
// (`lbz r11,5(vehicle)` at this + (idx+0x55)*128 + 5, i.e. maVehicles base +10880 stride 128)
// masked with 1 and 8 -- IsAlive() and !IsPhysical(), reached BY NAME here.
//
// The FOURTH argument the console passes -- the OTHER half's entity word -- becomes
// AddVehicleToPhysics's lTargetEntityId, which is what the physics side records as the
// vehicle that provoked the promotion.
// ----------------------------------------------------------------------------
void TrafficEntityModule::HandleHalfPotentialContact(
        u32 luHalfEntityWord,
        u64 lu64HalfVolumeInstanceId,
        u32 luOtherHalfEntityWord,
        BrnTrafficIO::OutputBuffer_PrePhysics* lpOutput,
        TotalTrafficBitArray* lpCreatedBodies)
{
    // r5 is not even saved in the console prologue: the whole-qword id rides the call so the
    // argument list matches the pair walker's, and nothing reads it.
    (void)lu64HalfVolumeInstanceId;

    const u32 luVehicle = GetEntityWordIndex(luHalfEntityWord);
    CGS_ASSERT(luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC");  // .h:2459

    if (luVehicle >= KU_MAX_TOTAL_TRAFFIC)   // PC-safety guard: a bad id must not index the pool
    {
        return;
    }

    const Vehicle* lpVehicle = GetVehicle(luVehicle);

    // 0x82747FB0 / 0x82747FBC. A dead car has nothing to promote; a car that is ALREADY
    // physical has a slot, and re-posting a create event for it would spend one of the 25
    // CreatePhysicalTrafficEvent ring slots for nothing.
    if (!lpVehicle->IsAlive() || lpVehicle->IsPhysical())
    {
        return;
    }

    // 0x8274807C: the inlined BitArray<600>::IsBitSet, with its own "invalid index : " message
    // (CgsBitArray.h:203). lpCreatedBodies is PrePhysicsUpdate's per-frame set, so one car is
    // promoted at most once per frame however many pairs name it.
    if (lpCreatedBodies->IsBitSet(luVehicle))
    {
        return;
    }

    // 0x827480AC..0x827480CC. NOTE the type: E_TRAFFIC_TYPE_POTENTIAL is NOT a lightweight
    // proxy -- PhysicalTrafficManager::GetFreeTrafficVehicleWithPhysics hands out a FULL
    // 20-slot body for it and PreparePhysical simply does not arm the body's crashing state.
    EntityId lTargetEntityId;
    lTargetEntityId.muValue = luOtherHalfEntityWord;

    AddVehicleToPhysics(luVehicle,
                        lTargetEntityId,
                        lpOutput->GetVehicleInputInterface(),
                        BrnPhysics::Vehicle::E_TRAFFIC_TYPE_POTENTIAL,
                        lpCreatedBodies);
}

}
