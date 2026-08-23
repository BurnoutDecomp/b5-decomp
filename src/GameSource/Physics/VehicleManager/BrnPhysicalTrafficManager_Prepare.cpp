// ============================================================================
// BrnPhysicalTrafficManager_Prepare.cpp
//
//   BrnPhysics::Vehicle::PhysicalTrafficManager::Prepare @0x8262CA48 (290)
//
// THE ONLY SEATER of mpaTrafficDrivers / mpaTrafficVehicles / mpaSimpleVehiclePhysics
// (Construct leaves all three NULL, BrnPhysicalTrafficManager.h's inline ctor). Until this
// body ran, the first SetBit on mUsedTrafficVehicles' owner arrays was a null deref.
//
// The address was an .ida-exports HOLE (absent from progress/identity.json); the body was
// dumped headless from a COPY of the ARTIST .i64. Its own asserts name the console file/lines BrnPhysicalTrafficManager.cpp:133/157/168/180.
// DWARF: BrnPhysicalTrafficManager.h:131 `bool Prepare(rw::LinearResourceAllocator*)`.
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/rwcore_structs.h"      // rw::ResourceDescriptor / rw::Resource / IResourceAllocator

#include <cstring>   // memset (the console's 600-byte map fill)
#include <new>       // placement new (the array constructor the allocator call runs)

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // The console allocation sizes are (size << 32 | alignment) doublewords in the asm at
    // 0x8262CB44 / 0x8262CC2C / 0x8262CCF4: drivers 0x1180 (20*224), vehicles 0x500 (20*64),
    // simple 0x720 (1*1824). The requests below are sized from HOST sizeof -- the two agree for
    // VehicleDriver (224) and diverge for the other two, which is why the host must not
    // transcribe the console number.
    const u32 KU_POOL_ALIGNMENT = 16;

    // The SIMPLE pool is ONE element: SimpleTrafficBitArray is BitArray<1u>
    // (BrnPhysicalTrafficManager.h:519) and the console's 0x720 is one whole object.
    const u32 KU_NUM_SIMPLE_TRAFFIC_PHYSICS = 1;
}

// @0x8262CA48. DWARF :131. Returns the constant 1 -- there is no failure path; each of the
// three allocations only fires an assert and carries the null pointer forward.
bool PhysicalTrafficManager::Prepare(rw::IResourceAllocator* lpPhysicsAllocator)
{
    CGS_ASSERT(lpPhysicsAllocator != 0, "lpPhysicsAllocator != NULL");   // .cpp:133

    // The nine 8-byte zero stores at 0x8262CAAC..0x8262CB08, in asm order. EIGHT of the nine
    // BitArrays; mTestedTrafficVehicles (X360 +104592) IS NOT ONE OF THEM. That omission is the
    // console's -- Construct skips it too -- and it stays an omission.
    // (0x8262CAE0 and 0x8262CAE8 both store to +104552; the duplicate is idempotent.)
    mPotentialTrafficVehicles.Prepare();        // +104576
    mTrafficDeformationModelsActive.Prepare();  // +104584
    mUnusedPotentialTrafficQueue.Clear();       // +104632 == the queue's miLength (stwx)
    mUsedSimpleVehiclePhysics.Prepare();        // +104568
    mUsedFullTrafficPhysics.Prepare();          // +104560
    mUsedTrafficVehicles.Prepare();             // +104552
    mAddedTrafficVehicles.Prepare();            // +104600
    mRemovedTrafficVehicles.Prepare();          // +104608
    mMadeSimpleTrafficVehicles.Prepare();       // +104616

    // 0x8262CB0C `memset(this + 104688, 0x7F, 0x258)` -- 600 bytes of KU8_INVALID_MAP.
    memset(mu8GlobalToPhysicalEntityIndexMap, KU8_INVALID_MAP,
           sizeof(mu8GlobalToPhysicalEntityIndexMap));

    // ---- the three pool allocations -----------------------------------------------------
    // rw::ResourceDescriptor's default constructor is the console's own {size 0, alignment 1}
    // seed loop (0x8262CB10..0x8262CB30, five entries on the X360's <5> descriptor); only
    // entry 0 is then filled, and DoAllocate is the vtable slot at +0x10.

    rw::ResourceDescriptor lDriversDescriptor;
    lDriversDescriptor.m_baseResourceDescriptors[0].m_size =
        static_cast<u32>(sizeof(VehicleDriver) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC);
    lDriversDescriptor.m_baseResourceDescriptors[0].m_alignment = KU_POOL_ALIGNMENT;
    rw::Resource lDriversResource =
        lpPhysicsAllocator->DoAllocate(lDriversDescriptor, "Physics traffic drivers");
    mpaTrafficDrivers = static_cast<VehicleDriver*>(lDriversResource.m_baseResources[0]);
    CGS_ASSERT(mpaTrafficDrivers != 0,
               "Failed to allocate array of traffic drivers from physics allocator");   // .cpp:157

    rw::ResourceDescriptor lVehiclesDescriptor;
    lVehiclesDescriptor.m_baseResourceDescriptors[0].m_size =
        static_cast<u32>(sizeof(PhysicalTrafficVehicle) * KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC);
    lVehiclesDescriptor.m_baseResourceDescriptors[0].m_alignment = KU_POOL_ALIGNMENT;
    rw::Resource lVehiclesResource =
        lpPhysicsAllocator->DoAllocate(lVehiclesDescriptor, "Physical traffic vehicles");
    mpaTrafficVehicles =
        static_cast<PhysicalTrafficVehicle*>(lVehiclesResource.m_baseResources[0]);
    CGS_ASSERT(mpaTrafficVehicles != 0,
               "Failed to allocate array of traffic vehicles from physics allocator");  // .cpp:168

    rw::ResourceDescriptor lSimpleDescriptor;
    lSimpleDescriptor.m_baseResourceDescriptors[0].m_size =
        static_cast<u32>(sizeof(SimpleVehiclePhysics) * KU_NUM_SIMPLE_TRAFFIC_PHYSICS);
    lSimpleDescriptor.m_baseResourceDescriptors[0].m_alignment = KU_POOL_ALIGNMENT;
    rw::Resource lSimpleResource =
        lpPhysicsAllocator->DoAllocate(lSimpleDescriptor, "Simple traffic physics");
    mpaSimpleVehiclePhysics =
        static_cast<SimpleVehiclePhysics*>(lSimpleResource.m_baseResources[0]);
    if (mpaSimpleVehiclePhysics != 0)
    {
        // 0x8262CD28..0x8262CD78: the C++ ARRAY CONSTRUCTOR the allocator call runs over the
        // raw block -- the vtable seat plus three BaseCollisionGenerator vector-constructor-
        // iterators over the embedded sub-arrays (+0x130 x4x224, +0x4B0 x4x32, +0x530 x4x16).
        // Those sub-objects are not modelled on the host, so the default construction below is
        // the whole of what the host has to run. Element-wise (not `new T[n]`) because MSVC
        // puts an array cookie in front of a placement array-new of a type with a destructor.
        for (u32 luSimple = 0; luSimple < KU_NUM_SIMPLE_TRAFFIC_PHYSICS; ++luSimple)
        {
            new (&mpaSimpleVehiclePhysics[luSimple]) SimpleVehiclePhysics();
        }
    }
    CGS_ASSERT(mpaSimpleVehiclePhysics != 0,
               "Failed to allocate array of simple traffic physics from physics allocator"); // .cpp:180

    // ---- the three seat loops ------------------------------------------------------------
    // 0x8262CDF4..0x8262CE24.
    for (u32 luDriver = 0; luDriver < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC; ++luDriver)
    {
        mpaTrafficDrivers[luDriver].Construct();
        mpaTrafficDrivers[luDriver].Prepare();
    }

    // CONSOLE DEFECT, GATED NOT COPIED. Both loops below close with
    // `VehicleDriver::Prepare(mpaTrafficDrivers + 4480)` -- element 20 of a 20-element pool
    // (r28 is pinned to 224*20 at 0x8262CE30 and never re-derived). VehicleDriver::Prepare
    // @0x825B8680 WRITES 214 bytes, so on the host that is a heap overrun into the very
    // PhysicalTrafficVehicle pool the third loop is seeding. The index arithmetic is
    // reproduced; the out-of-range call is skipped and reported once.
    // DELETE-WHEN a wave re-derives whether the console meant 20 FULL + 1 SIMPLE drivers.
    const u32 luConsoleTrailingDriver = KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC;

    // 0x8262CE34..0x8262CE5C.
    for (u32 luSimple = 0; luSimple < KU_NUM_SIMPLE_TRAFFIC_PHYSICS; ++luSimple)
    {
        mpaSimpleVehiclePhysics[luSimple].Construct();
        if (luConsoleTrailingDriver < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC)
        {
            mpaTrafficDrivers[luConsoleTrailingDriver].Prepare();
        }
    }

    // 0x8262CE6C..0x8262CEB4 -- the per-slot field seeds, in asm order.
    for (u32 luVehicle = 0; luVehicle < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC; ++luVehicle)
    {
        PhysicalTrafficVehicle& lrVehicle = mpaTrafficVehicles[luVehicle];
        lrVehicle.mpVehicleBody            = 0;                              // +28  stw 0
        lrVehicle.mu8PhysicsPoolIndex      = 0;                              // +49  stb 0
        // +50 stb 2 -- neither FULL (0) nor SIMPLE (1); the "no physical type yet" seat.
        lrVehicle.mu8PhysicalType          = PhysicalTrafficVehicle::E_PHYSICAL_TRAFFIC_TYPE_COUNT;
        lrVehicle.mePhysicalTrafficState   = E_TRAFFIC_TYPE_PHYSICAL;        // +32  stw 2
        lrVehicle.meArticulatedVehicleType =
            PhysicalTrafficVehicle::E_ARTICULATE_VEHICLE_NONE;               // +36  stw 0
        lrVehicle.meArticulatedJointState  =
            PhysicalTrafficVehicle::E_ARTICULATE_JOINT_NONE;                 // +40  stw 0
        lrVehicle.miJointIndex             = -1;                             // +44  stw -1
        lrVehicle.mCgsID                   = 0;                              // +16  std 0
        lrVehicle.mbUsingBoxWithWorld      = false;                          // +52  stb 0
        // NOT seeded by the console: mbRammed (+48) and miCheckOwner (+51).

        if (luConsoleTrailingDriver < KU8_TOTAL_MAX_NUM_PHYSICAL_TRAFFIC)
        {
            mpaTrafficDrivers[luConsoleTrailingDriver].Prepare();
        }
    }

    mDebugComponent.Register();   // 0x8262CEC0 (this + 105616)

    return true;   // 0x8262CEC4 `li r3, 1`
}

}
}
