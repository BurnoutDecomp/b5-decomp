#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics / Wheel::RoadContact
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugGraph.h"   // GripCurveDebugGraph (the two graphs inside each grip-curve window)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// BrnPhysics::Vehicle::DebugComponent - the leaf ledger functions homed by the Vehicle-physics
// group (GetPath @0x825DB0D0, SetLastWallTriangle @0x825B4D60). The rest of the component's API is
// owned by a separate dev-UI pass.
//
// Construct (the per-car two-phase init), ResetGearStats and Update, the per-frame sampling tick,
// are homed here too.
//
// SLOT ADDRESSING. VehicleManager hands each car's component out as one element of the opaque
// 1024-byte span maRaceCarDebugComponent[8], so `this` is the base of that slot and the fields
// these bodies touch are addressed at their in-slot offsets -- the same sanctioned seam
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
    const u32 KU_GEAR_STATS_ROWS     = 6;
    const u32 KU_GEAR_STATS_FLOATS   = 6;       // per row, then the closed flag
    const u32 KU_SPEED_MPH_MIRROR    = 0x310;   // f32

    // Construct's own fields, in slot order.
    const u32 KU_SCRATCH_F32_264     = 0x264;   // the f32 just before the gear-stat rows; unnamed
                                                // on the recovered surface
    const u32 KU_GRIP_CURVE_WINDOW_A = 0xD0;    // two GripCurveDebugWindows, 0xC0 bytes each
    const u32 KU_GRIP_CURVE_WINDOW_B = 0x190;
    const u32 KU_SCRATCH_F32_320     = 0x320;   // four f32 (+0x320..+0x32C), unnamed; they sit
                                                // beside the two draw toggles seeded with them
    const u32 KU_WALL_TRIANGLE       = 0x350;   // the last-wall-triangle record: four quadwords
                                                // then three more f32 at +0x390/+0x394/+0x398
    const u32 KU_RENDER_COM              = 0x3A0;
    const u32 KU_RENDER_HANDLING_BODY    = 0x3A1;
    const u32 KU_OUTPUT_GEAR_STATS       = 0x3A2;
    const u32 KU_OUTPUT_DEFORMATION      = 0x3A3;
    const u32 KU_OUTPUT_TIRE_FRICTIONS   = 0x3A4;
    const u32 KU_TOGGLE_3A5              = 0x3A5;   // unnamed on the recovered surface
    const u32 KU_TOGGLE_3A6              = 0x3A6;   // unnamed on the recovered surface
    const u32 KU_DRAW_TRACTION_LINES     = 0x3A7;
    const u32 KU_DRAW_TRACTION_NORMALS   = 0x3A8;
    const u32 KU_DRAW_ABOVE_GROUND_TEST  = 0x3A9;
    const u32 KU_DRAW_LAST_WALL_TRIANGLE = 0x3AA;
    const u32 KU_DRAW_ANGULAR_VELOCITY   = 0x3AB;
    const u32 KU_DRAW_SUSPENSION_FORCES  = 0x3AC;
    const u32 KU_TOGGLE_3AD              = 0x3AD;   // unnamed on the recovered surface
    const u32 KU_WHEEL_SCRATCH       = 0x3B0;   // one quadword per wheel, stride 0x10
    const u32 KU_WHEEL_SCRATCH_FLAG  = 0x3F0;   // one byte per wheel
    const u32 KU_DOWN_FORCE          = 0x3F4;   // the mirror VehiclePhysics::UpdateDownForce writes

    // Offsets inside one GripCurveDebugWindow.
    const u32 KU_WINDOW_GRAPH_A      = 0x30;    // two GripCurveDebugGraphs, 0x40 bytes each
    const u32 KU_WINDOW_GRAPH_B      = 0x70;
    const u32 KU_WINDOW_CURVE_SRC_A  = 0xB0;    // two console-width pointer words
    const u32 KU_WINDOW_CURVE_SRC_B  = 0xB4;
    const u32 KU_WINDOW_VISIBLE      = 0xB8;    // mbVisible

    template <typename T>
    T* SlotField(void* lpSlot, u32 luOffset)
    {
        return reinterpret_cast<T*>(static_cast<u8*>(lpSlot) + luOffset);
    }

    // The console clears each of these spans with one zero vector store; four zero words write
    // exactly the same sixteen bytes without a SIMD type the tree does not model here.
    void ZeroQuadword(void* lpSlot, u32 luOffset)
    {
        u32* lpWords = SlotField<u32>(lpSlot, luOffset);
        lpWords[0] = 0u;
        lpWords[1] = 0u;
        lpWords[2] = 0u;
        lpWords[3] = 0u;
    }

    // BrnPhysics::Vehicle::GripCurveDebugWindow::Construct. The window class is not on
    // the tree's surface yet -- it exists only as a fixed 0xC0-byte region inside the component's
    // span, and the component is the only thing that constructs one -- so it is reached the same
    // way every other field in the slot is. Its two graphs ARE a recovered type, so those stores
    // land on named members. The window's own base (its DebugUI::Window part, +0x00..+0x2F) is
    // untouched, exactly as on the console.
    void ConstructGripCurveDebugWindow(void* lpSlot, u32 luWindow)
    {
        SlotField<GripCurveDebugGraph>(lpSlot, luWindow + KU_WINDOW_GRAPH_A)->Construct();
        SlotField<GripCurveDebugGraph>(lpSlot, luWindow + KU_WINDOW_GRAPH_B)->Construct();

        // The two curve-source pointer words the window's Render reads back. Console-width (four
        // byte) slots, cleared as the two words the console clears; nothing in the tree reads them.
        *SlotField<u32>(lpSlot, luWindow + KU_WINDOW_CURVE_SRC_A) = 0u;
        *SlotField<u32>(lpSlot, luWindow + KU_WINDOW_CURVE_SRC_B) = 0u;

        *SlotField<u8>(lpSlot, luWindow + KU_WINDOW_VISIBLE) = 0;
    }
}

    // The slot fields Construct writes all start at +0xD0, above every member this tree does
    // model on the component, so constructing the slot cannot walk over one of them.
    static_assert(sizeof(DebugComponent) <= KU_GRIP_CURVE_WINDOW_A,
                  "modelled DebugComponent members must stay below the first grip-curve window");

    // BrnPhysics::Vehicle::DebugComponent::ResetGearStats
    //   Clear all six gear rows -- six floats then the row's closed flag, 28 bytes apiece. Update
    //   accumulates the timestep into the row the gear cursor at +0x260 names until
    //   UpdateAndRenderGearStats closes it; this puts every row back to open and empty.
    void DebugComponent::ResetGearStats()
    {
        for (u32 luGear = 0; luGear < KU_GEAR_STATS_ROWS; ++luGear)
        {
            const u32 luRow = KU_GEAR_STATS + KU_GEAR_STATS_STRIDE * luGear;

            f32* lpGearRowFields = SlotField<f32>(this, luRow);
            for (u32 luField = 0; luField < KU_GEAR_STATS_FLOATS; ++luField)
            {
                lpGearRowFields[luField] = 0.0f;
            }

            *SlotField<u8>(this, luRow + KU_GEAR_STATS_CLOSED) = 0;
        }
    }

    // BrnPhysics::Vehicle::DebugComponent::Construct
    //   The per-car two-phase init VehicleManager::PrepareData runs once per car. It records the
    //   car this component samples, seats the two grip-curve windows, and clears everything the
    //   component accumulates or draws. The console schedules these independent stores across the
    //   whole slot; they are grouped here by what they belong to, in console order.
    //
    //   The console issues one further call on `this` right after the assert. That call lands on a
    //   body which is a bare return, and identical bodies are folded together in the image, so the
    //   symbol it carries belongs to an unrelated class. It stores nothing, so it has no statement.
    //
    //   NOT written, on the console or here: the gear cursor at +0x260 and the speed mirror at
    //   +0x310. Both start at the span's zero and Update is the only thing that moves them.
    void DebugComponent::Construct(RaceCarPhysics* lpRaceCarPhysics)
    {
        CGS_ASSERT(lpRaceCarPhysics != nullptr, "lpRaceCarPhysics != NULL");

        // The host pointer is eight bytes wide and so covers +0x250..+0x257; +0x258..+0x25F is
        // unused in the slot and the next field is the gear cursor at +0x260, so nothing shifts.
        // Update reads this field back the same way.
        *SlotField<RaceCarPhysics*>(this, KU_RACE_CAR_PHYSICS) = lpRaceCarPhysics;

        *SlotField<f32>(this, KU_SCRATCH_F32_264) = 0.0f;

        // The last-wall-triangle record: four zero quadwords, then three more floats. The console
        // issues the quadwords out of address order (the +0x380 one first); they are independent.
        // This is the in-slot record, NOT the modelled mLastWallTriangle member -- that member sits
        // at its host offset, as the slot-addressing note at the top of this file records.
        ZeroQuadword(this, KU_WALL_TRIANGLE + 0x00);
        ZeroQuadword(this, KU_WALL_TRIANGLE + 0x10);
        ZeroQuadword(this, KU_WALL_TRIANGLE + 0x20);
        ZeroQuadword(this, KU_WALL_TRIANGLE + 0x30);
        *SlotField<f32>(this, KU_WALL_TRIANGLE + 0x40) = 0.0f;
        *SlotField<f32>(this, KU_WALL_TRIANGLE + 0x44) = 0.0f;
        *SlotField<f32>(this, KU_WALL_TRIANGLE + 0x48) = 0.0f;

        // Every draw / output toggle the component's render side reads, off.
        *SlotField<u8>(this, KU_RENDER_COM)              = 0;
        *SlotField<u8>(this, KU_RENDER_HANDLING_BODY)    = 0;
        *SlotField<u8>(this, KU_OUTPUT_GEAR_STATS)       = 0;
        *SlotField<u8>(this, KU_OUTPUT_DEFORMATION)      = 0;
        *SlotField<u8>(this, KU_OUTPUT_TIRE_FRICTIONS)   = 0;
        *SlotField<u8>(this, KU_TOGGLE_3A5)              = 0;
        *SlotField<u8>(this, KU_TOGGLE_3A6)              = 0;
        *SlotField<u8>(this, KU_DRAW_TRACTION_LINES)     = 0;
        *SlotField<u8>(this, KU_DRAW_TRACTION_NORMALS)   = 0;
        *SlotField<u8>(this, KU_DRAW_ABOVE_GROUND_TEST)  = 0;
        *SlotField<u8>(this, KU_DRAW_LAST_WALL_TRIANGLE) = 0;
        *SlotField<u8>(this, KU_TOGGLE_3AD)              = 0;

        ConstructGripCurveDebugWindow(this, KU_GRIP_CURVE_WINDOW_A);
        ConstructGripCurveDebugWindow(this, KU_GRIP_CURVE_WINDOW_B);

        ResetGearStats();   // inlined by the console at this point in the body

        // Four more floats and the two toggles the console seeds alongside them.
        *SlotField<f32>(this, KU_SCRATCH_F32_320 + 0x0) = 0.0f;
        *SlotField<u8>(this, KU_DRAW_ANGULAR_VELOCITY)  = 0;
        *SlotField<f32>(this, KU_SCRATCH_F32_320 + 0x4) = 0.0f;
        *SlotField<f32>(this, KU_SCRATCH_F32_320 + 0x8) = 0.0f;
        *SlotField<u8>(this, KU_DRAW_SUSPENSION_FORCES) = 0;
        *SlotField<f32>(this, KU_SCRATCH_F32_320 + 0xC) = 0.0f;

        // One console loop, two cursors: a zero quadword per wheel at stride 0x10, and a flag byte
        // per wheel at stride 1.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            ZeroQuadword(this, KU_WHEEL_SCRATCH + 0x10 * luWheel);
            *SlotField<u8>(this, KU_WHEEL_SCRATCH_FLAG + luWheel) = 0;
        }

        *SlotField<f32>(this, KU_DOWN_FORCE) = 0.0f;
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

        // Construct stored this when VehicleManager::PrepareData built the per-car components.
        RaceCarPhysics* lpCar = *SlotField<RaceCarPhysics*>(this, KU_RACE_CAR_PHYSICS);

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
