#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics / Wheel::RoadContact
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Vehicle::DebugComponent - the leaf ledger functions homed by the Vehicle-physics
// group (GetPath @0x825DB0D0, SetLastWallTriangle @0x825B4D60). The rest of the component's API is
// owned by a separate dev-UI pass.
//
// Update, the per-frame sampling tick, is homed here too.
//
// SLOT ADDRESSING. VehicleManager hands each car's component out as one element of the opaque
// 1024-byte span maRaceCarDebugComponent[8], so `this` is the base of that slot and the fields
// Update touches are addressed at their in-slot offsets -- the same sanctioned seam
// VehiclePhysics::UpdateDownForce already uses for the down-force mirror at +0x3F4. NOTE that
// mLastWallTriangle above is a modelled member and therefore does NOT sit at its own in-slot
// offset (+0x350); it overlaps the road-contact mirror below. Both are write-only today (nothing
// in the tree reads either), and the overlap goes away when this component gets a real layout.

namespace BrnPhysics
{
namespace Vehicle
{
namespace
{
    // In-slot offsets, all confirmed against Construct/ResetGearStats/UpdateAndRenderGearStats.
    const u32 KU_ROAD_CONTACT_MIRROR = 0x10;    // Wheel::RoadContact[4], stride 0x30
    const u32 KU_ROAD_CONTACT_STRIDE = 0x30;
    const u32 KU_RACE_CAR_PHYSICS    = 0x250;   // the car this component samples
    const u32 KU_GEAR_STATS_GEAR     = 0x260;   // u32: which gear row is being filled
    const u32 KU_GEAR_STATS          = 0x268;   // six rows, stride 28
    const u32 KU_GEAR_STATS_STRIDE   = 28;
    const u32 KU_GEAR_STATS_CLOSED   = 0x18;    // row-relative: the row is finished
    const u32 KU_SPEED_MPH_MIRROR    = 0x310;   // f32

    template <typename T>
    T* SlotField(void* lpSlot, u32 luOffset)
    {
        return reinterpret_cast<T*>(static_cast<u8*>(lpSlot) + luOffset);
    }
}

    // @0x825DB0D0  BrnPhysics::Vehicle::DebugComponent::GetPath
    //   lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
    const char* DebugComponent::GetPath() const
    {
        return "Physics";
    }

    // @0x825B4D60  BrnPhysics::Vehicle::DebugComponent::SetLastWallTriangle
    //   Record the collision triangle of the last wall the car scraped (for the
    //   DrawLastWallTriangle debug overlay). The asm asserts lpTriangle != NULL then copies the
    //   whole 80-byte AOSTriangle (10 qwords, ld/std loop) into mLastWallTriangle @ this+0x350.
    //   sizeof(CgsGeometric::Triangle4::AOSTriangle) == 0x50 == 80, so the 10-qword copy is
    //   exactly the member assign.
    void DebugComponent::SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle)
    {
        CGS_ASSERT(lpTriangle != nullptr, "lpTriangle != NULL");

        mLastWallTriangle = *lpTriangle;   // 10-qword (80-byte) AOSTriangle copy into this+0x350
    }

    // BrnPhysics::Vehicle::DebugComponent::Update
    //   The per-car debug tick VehicleManager::UpdateVehiclePhysics calls once per live car with
    //   the sim timestep. It samples, it never drives anything: it accumulates time into the
    //   current gear's stats row, mirrors the four wheels' road-contact results and the car's
    //   speed into the slot, and returns. Nothing here is conditional on a debug-menu toggle --
    //   the toggles are read by the render/HUD side, which is the dev-UI pass's work.
    void DebugComponent::Update(f32 lfTimeStep)
    {
        // (1) Gear stats. The row for the gear currently being sampled keeps adding the timestep
        //     until UpdateAndRenderGearStats closes it; a closed row is left alone. Six rows of
        //     28 bytes, six floats then the closed flag.
        const u32 luGear = *SlotField<const u32>(this, KU_GEAR_STATS_GEAR);
        u8* lpGearRow = SlotField<u8>(this, KU_GEAR_STATS + KU_GEAR_STATS_STRIDE * luGear);
        if (*(lpGearRow + KU_GEAR_STATS_CLOSED) == 0)
        {
            *reinterpret_cast<f32*>(lpGearRow) = lfTimeStep + *reinterpret_cast<f32*>(lpGearRow);
        }

        RaceCarPhysics* lpCar = *SlotField<RaceCarPhysics*>(this, KU_RACE_CAR_PHYSICS);

        // [FLAG PC bring-up gate] The console always has a car here -- DebugComponent::Construct
        // stores it when VehicleManager::PrepareData builds the per-car components. That
        // construction is still gated in this tree (see the LogDebugComponentGate note in
        // BrnVehicleManager_PrepareData.cpp), so the slot is zero storage and this pointer reads
        // NULL on every car, every frame. Sampling through it would fault. The gear-stats
        // accumulate above is unaffected and runs exactly as the console runs it.
        // DELETE-WHEN the per-car DebugComponent::Construct gate in PrepareData is closed.
        if (lpCar == 0)
        {
            return;
        }

        // (2) Mirror each wheel's road-contact result into the slot. The console copies the
        //     leading 0x2C bytes of the wheel -- exactly Wheel::RoadContact -- and leaves the
        //     four pad bytes of each 0x30-byte destination slot untouched.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            const Wheel::RoadContact& lrSource =
                lpCar->GetWheel(static_cast<EVehicleDrivenWheel>(luWheel)).GetRoadContact();
            Wheel::RoadContact* lpMirror = SlotField<Wheel::RoadContact>(
                this, KU_ROAD_CONTACT_MIRROR + KU_ROAD_CONTACT_STRIDE * luWheel);

            lpMirror->mPosition               = lrSource.mPosition;
            lpMirror->mNormal                 = lrSource.mNormal;
            lpMirror->mfLineDistanceToRoad    = lrSource.mfLineDistanceToRoad;
            lpMirror->mCollisionTag           = lrSource.mCollisionTag;
            lpMirror->mbIsOnGround            = lrSource.mbIsOnGround;
            lpMirror->mbWasOnGroundLastUpdate = lrSource.mbWasOnGroundLastUpdate;
            lpMirror->mbIsCloseToGround       = lrSource.mbIsCloseToGround;
            lpMirror->mbLineTestIsValid       = lrSource.mbLineTestIsValid;
        }

        // (3) Mirror the car's speed. The console loads the whole mfSpeedMPH vector and keeps
        //     the x lane.
        *SlotField<f32>(this, KU_SPEED_MPH_MIRROR) = lpCar->GetSpeedMPH().x;
    }
}
}
