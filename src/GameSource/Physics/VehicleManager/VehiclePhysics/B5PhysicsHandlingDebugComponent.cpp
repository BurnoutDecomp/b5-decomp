#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h"
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"   // RaceCarPhysics / Wheel::RoadContact
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstddef>   // offsetof

// BrnPhysics::Vehicle::DebugComponent -- the component's data side: the per-car two-phase
// Construct, the gear-stat reset, the two debug-menu name getters, the last-wall-triangle setter
// and the per-frame sampling tick. The render/HUD side is owned by its own dev-UI pass.
//
// Every body here reaches its state through NAMED MEMBERS. The 1024-byte opaque slot the
// component used to be addressed through -- and the SlotField<T> offset seam that went with it --
// are gone: VehicleManager embeds `DebugComponent maRaceCarDebugComponent[8]` by value, so the
// elements are constructed, carry a live vptr, and index at the host stride.

namespace BrnPhysics
{
namespace Vehicle
{
    // ---- layout gate ---------------------------------------------------------------------------
    //
    // The recovered class accounts for all 1024 bytes of the console object with nothing left over.
    // Its host image is bigger, because six pointer-shaped things widen 4 -> 8 (the base's vptr and
    // list link, this class's car pointer, each window's vptr and its two wheel pointers) and two
    // of those widenings force a 16-byte re-alignment behind them. The chain below names each step
    // where it happens and adds it to a running term, so every member's console offset is pinned by
    // a host offsetof the compiler checks. Get any member's type, size or order wrong and one of
    // these fires.
    void DebugComponent::_AssertLayout()
    {
        typedef DebugComponent D;

        // Step 1: the base. Console 12 bytes (vptr + the active flag + the list link) padded to
        // 0x10 by the road-contact array's 16-byte alignment; host 24, padded to 0x20.
        const std::ptrdiff_t KD_AFTER_BASE = 0x10;
        static_assert(offsetof(D, maRoadContacts) == 0x010 + KD_AFTER_BASE, "maRoadContacts");

        // Step 2: nothing widens across the road-contact mirror -- RoadContact is 0x30 on both
        // sides (two 16-byte vectors, a float, a collision tag and four flag bytes).
        static_assert(sizeof(Wheel::RoadContact) == 0x30, "RoadContact is width-identical");
        static_assert(offsetof(D, mFrontWheelsGripCurveWindow) == 0x0D0 + KD_AFTER_BASE,
                      "mFrontWheelsGripCurveWindow");

        // Step 3: one grip-curve window apiece. Each is 0xC0 on the console and 0xD0 on the host:
        // its two wheel pointers widen (+8) and the 16-byte alignment its graphs impose rounds the
        // tail up (+8). The window's own base is width-identical, so the graphs stay at +0x30/+0x70
        // inside it.
        const std::ptrdiff_t KD_WINDOW_STEP = 0x10;
        static_assert(sizeof(GripCurveDebugWindow) == 0x0C0 + KD_WINDOW_STEP,
                      "GripCurveDebugWindow host size -- the number KD_WINDOW_STEP carries");
        const std::ptrdiff_t KD_AFTER_WINDOWS = KD_AFTER_BASE + 2 * KD_WINDOW_STEP;
        static_assert(offsetof(D, mRearWheelsGripCurveWindow) == 0x190 + KD_AFTER_BASE + KD_WINDOW_STEP,
                      "mRearWheelsGripCurveWindow");
        static_assert(offsetof(D, mpRaceCarPhysics) == 0x250 + KD_AFTER_WINDOWS, "mpRaceCarPhysics");

        // Step 4: the car pointer itself widens 4 -> 8. Everything from the inertia triple to the
        // clutch factor is float- or int-shaped and moves as one block behind it.
        const std::ptrdiff_t KD_AFTER_CAR_POINTER = KD_AFTER_WINDOWS + 4;
        static_assert(offsetof(D, mafInertia)     == 0x254 + KD_AFTER_CAR_POINTER, "mafInertia");
        static_assert(offsetof(D, miCurrentGear)  == 0x260 + KD_AFTER_CAR_POINTER, "miCurrentGear");
        static_assert(offsetof(D, mfLastFrameRpm) == 0x264 + KD_AFTER_CAR_POINTER, "mfLastFrameRpm");

        static_assert(sizeof(GearStats) == 28, "GearStats is width-identical (six floats + a flag)");
        static_assert(offsetof(D, mGearStats) == 0x268 + KD_AFTER_CAR_POINTER, "mGearStats");
        static_assert(offsetof(D, mfSpeedMPH) == 0x310 + KD_AFTER_CAR_POINTER,
                      "mfSpeedMPH -- and therefore that the six gear rows close on it");

        static_assert(offsetof(D, mafHackComOffset)  == 0x314 + KD_AFTER_CAR_POINTER, "mafHackComOffset");
        static_assert(offsetof(D, mfHackCrashExtraPitchVelocity)  == 0x320 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraPitchVelocity");
        static_assert(offsetof(D, mfHackCrashExtraYawVelocity)    == 0x324 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraYawVelocity");
        static_assert(offsetof(D, mfHackCrashExtraRollVelocity)   == 0x328 + KD_AFTER_CAR_POINTER, "mfHackCrashExtraRollVelocity");
        static_assert(offsetof(D, mfHackCrashExtraLinearVelocity) == 0x32C + KD_AFTER_CAR_POINTER, "mfHackCrashExtraLinearVelocity");
        static_assert(offsetof(D, mfRPM)                      == 0x330 + KD_AFTER_CAR_POINTER, "mfRPM");
        static_assert(offsetof(D, mfEngineDrive)              == 0x334 + KD_AFTER_CAR_POINTER, "mfEngineDrive");
        static_assert(offsetof(D, mfReactionTorque)           == 0x338 + KD_AFTER_CAR_POINTER, "mfReactionTorque");
        static_assert(offsetof(D, mfFlyWheelAngularVelocity)  == 0x33C + KD_AFTER_CAR_POINTER, "mfFlyWheelAngularVelocity");
        static_assert(offsetof(D, mfClutchDelay)              == 0x340 + KD_AFTER_CAR_POINTER, "mfClutchDelay");
        static_assert(offsetof(D, mfClutchFactor)             == 0x344 + KD_AFTER_CAR_POINTER, "mfClutchFactor");

        // Step 5: the wall triangle is 16-aligned, and the block in front of it ends four bytes
        // short of a 16-byte boundary on the console. So the +4 the car pointer cost is ABSORBED by
        // padding the console already had, and the term drops back to what it was before the
        // pointer -- the one place in this class where a widening costs nothing. From here to the
        // end of the object it stays constant.
        const std::ptrdiff_t KD_AFTER_TRIANGLE_ALIGN = KD_AFTER_WINDOWS;
        static_assert(sizeof(CgsGeometric::Triangle4::AOSTriangle) == 0x50,
                      "AOSTriangle is width-identical (four vectors + three floats, 16-aligned)");
        static_assert(offsetof(D, mLastWallTriangle) == 0x350 + KD_AFTER_TRIANGLE_ALIGN, "mLastWallTriangle");

        // The fourteen toggles are one contiguous run; asserting both ends pins all of them,
        // because a bool array has no padding to hide a missing or extra member in.
        static_assert(offsetof(D, mbRenderCOMPosition) == 0x3A0 + KD_AFTER_TRIANGLE_ALIGN, "mbRenderCOMPosition");
        static_assert(offsetof(D, mbStoredPhysics)     == 0x3AD + KD_AFTER_TRIANGLE_ALIGN, "mbStoredPhysics");

        static_assert(offsetof(D, maSuspensionForces)        == 0x3B0 + KD_AFTER_TRIANGLE_ALIGN, "maSuspensionForces");
        static_assert(offsetof(D, mabSuspensionForceApplied) == 0x3F0 + KD_AFTER_TRIANGLE_ALIGN, "mabSuspensionForceApplied");
        static_assert(offsetof(D, mfDownForceY)              == 0x3F4 + KD_AFTER_TRIANGLE_ALIGN, "mfDownForceY");

        // And the closure: the console object ends at 1024 with eight bytes of tail pad behind the
        // down-force mirror, so the host object is 1024 + the accumulated term, rounded to 16.
        static_assert(sizeof(D) == 1024 + KD_AFTER_TRIANGLE_ALIGN,
                      "the recovered class closes on the console's 1024-byte object -- this is the "
                      "number VehicleManager's drift term is derived from");
    }

    // BrnPhysics::Vehicle::GearStats::Reset
    //   One gear row back to open and empty: six floats and the row's closed flag.
    void GearStats::Reset()
    {
        mfTimeInGear                 = 0.0f;
        mfSpeedOutOfGearGoingUp      = 0.0f;
        mfSpeedOutOfGearGoingDown    = 0.0f;
        mfRpmWhenChangedOutGoingUp   = 0.0f;
        mfRpmWhenChangedOutGoingDown = 0.0f;
        mfRpmWhenChangedInGoingUp    = 0.0f;
        mbDoneUpdating               = false;
    }

    // BrnPhysics::Vehicle::DebugComponent::ResetGearStats
    //   Clear all six gear rows. The debug menu registers this as an action callback with the
    //   component as its user data, which is why it is static and takes a void*: Update accumulates
    //   the timestep into the row miCurrentGear names until the gear-stat report closes it, and
    //   this puts every row back to open and empty.
    void DebugComponent::ResetGearStats(void* lpComponent)
    {
        DebugComponent* lpThis = static_cast<DebugComponent*>(lpComponent);

        for (u32 luGear = 0; luGear < 6u; ++luGear)
        {
            lpThis->mGearStats[luGear].Reset();
        }
    }

    // BrnPhysics::Vehicle::DebugComponent::Construct
    //   The per-car two-phase init VehicleManager::PrepareData runs once per car. It records the
    //   car this component samples, seats the two grip-curve windows, and clears everything the
    //   component accumulates or draws. The console schedules these independent stores across the
    //   whole object; they are grouped here by what they belong to, in console order.
    //
    //   The console issues one further call on `this` right after the assert. That call lands on a
    //   body which is a bare return, and identical bodies are folded together, so the symbol it
    //   carries belongs to an unrelated class. It stores nothing, so it has no statement -- in
    //   particular it does NOT set a vptr; the vptr comes from the constructor, as it does here.
    //
    //   NOT written, on the console or here: the gear cursor, the speed mirror, the inertia
    //   triple, the COM-offset triple and the engine-report block. All of them start zeroed (the
    //   manager lives in static storage on both sides) and their writers are elsewhere.
    void DebugComponent::Construct(RaceCarPhysics* lpRaceCarPhysics)
    {
        CGS_ASSERT(lpRaceCarPhysics != nullptr, "lpRaceCarPhysics != NULL");

        mpRaceCarPhysics = lpRaceCarPhysics;
        mfLastFrameRpm   = 0.0f;

        // The last-wall-triangle record: four zero vectors, then the three edge cosines. The
        // console issues the vectors out of address order (the normal first); they are independent.
        mLastWallTriangle.mVertex0      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mVertex1      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mVertex2      = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mNormal       = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        mLastWallTriangle.mfEdgeCosine0 = 0.0f;
        mLastWallTriangle.mfEdgeCosine1 = 0.0f;
        mLastWallTriangle.mfEdgeCosine2 = 0.0f;

        // Every draw / report toggle the component's render side reads, off. (The two the console
        // seeds later, interleaved with the crash-velocity scratch, are below where it puts them.)
        mbRenderCOMPosition           = false;
        mbRenderHandlingBody          = false;
        mbOutputGearStats             = false;
        mbOutputDeformationStats      = false;
        mbShowTireFrictions           = false;
        mbRenderFrontWheelsGripCurves = false;
        mbRenderRearWheelsGripCurves  = false;
        mbDrawTractionLines           = false;
        mbDrawTractionLineNormals     = false;
        mbDrawAboveGroundLineTest     = false;
        mbDrawLastWallTriangle        = false;
        mbStoredPhysics               = false;

        mFrontWheelsGripCurveWindow.Construct();
        mRearWheelsGripCurveWindow.Construct();

        ResetGearStats(this);   // inlined by the console at this point in the body

        mfHackCrashExtraPitchVelocity  = 0.0f;
        mbDrawAngularVelocity          = false;
        mfHackCrashExtraYawVelocity    = 0.0f;
        mfHackCrashExtraRollVelocity   = 0.0f;
        mbDrawSuspensionForces         = false;
        mfHackCrashExtraLinearVelocity = 0.0f;

        // One console loop, two cursors: the per-wheel suspension force and its applied flag.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            maSuspensionForces[luWheel]        = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
            mabSuspensionForceApplied[luWheel] = false;
        }

        mfDownForceY = 0.0f;
    }

    // BrnPhysics::Vehicle::DebugComponent::GetName
    //   lis r11,aHandling@ha ; addi r3,r11,aHandling@l "Handling" ; blr
    const char* DebugComponent::GetName() const
    {
        return "Handling";
    }

    // BrnPhysics::Vehicle::DebugComponent::GetPath
    //   lis r11,aPhysics@ha ; addi r3,r11,aPhysics@l "Physics" ; blr
    const char* DebugComponent::GetPath() const
    {
        return "Physics";
    }

    // BrnPhysics::Vehicle::DebugComponent::SetLastWallTriangle
    //   Record the collision triangle of the last wall the car scraped (for the
    //   DrawLastWallTriangle debug overlay). The console asserts lpTriangle != NULL then copies the
    //   whole 80-byte AOSTriangle, which is exactly the member assign.
    void DebugComponent::SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle)
    {
        CGS_ASSERT(lpTriangle != nullptr, "lpTriangle != NULL");

        mLastWallTriangle = *lpTriangle;
    }

    // BrnPhysics::Vehicle::DebugComponent::Update
    //   The per-car debug tick VehicleManager::UpdateVehiclePhysics calls once per live car with
    //   the sim timestep. It samples, it never drives anything: it accumulates time into the
    //   current gear's stats row, mirrors the four wheels' road-contact results and the car's
    //   speed, and returns. Nothing here is conditional on a debug-menu toggle -- the toggles are
    //   read by the render/HUD side, which is the dev-UI pass's work.
    void DebugComponent::Update(f32 lfTimeStep)
    {
        // (1) Gear stats. The row for the gear currently being sampled keeps adding the timestep
        //     until the gear-stat report closes it; a closed row is left alone.
        GearStats& lrGearRow = mGearStats[miCurrentGear];
        if (!lrGearRow.mbDoneUpdating)
        {
            lrGearRow.mfTimeInGear = lfTimeStep + lrGearRow.mfTimeInGear;
        }

        // Construct stored this when VehicleManager::PrepareData built the per-car components.
        RaceCarPhysics* lpCar = mpRaceCarPhysics;

        // (2) Mirror each wheel's road-contact result. The console copies the leading 0x2C bytes of
        //     the wheel -- exactly Wheel::RoadContact -- and leaves the four pad bytes of each
        //     0x30-byte destination untouched.
        for (u32 luWheel = 0; luWheel < eNumDrivenWheels; ++luWheel)
        {
            const Wheel::RoadContact& lrSource =
                lpCar->GetWheel(static_cast<EVehicleDrivenWheel>(luWheel)).GetRoadContact();
            Wheel::RoadContact& lrMirror = maRoadContacts[luWheel];

            lrMirror.mPosition               = lrSource.mPosition;
            lrMirror.mNormal                 = lrSource.mNormal;
            lrMirror.mfLineDistanceToRoad    = lrSource.mfLineDistanceToRoad;
            lrMirror.mCollisionTag           = lrSource.mCollisionTag;
            lrMirror.mbIsOnGround            = lrSource.mbIsOnGround;
            lrMirror.mbWasOnGroundLastUpdate = lrSource.mbWasOnGroundLastUpdate;
            lrMirror.mbIsCloseToGround       = lrSource.mbIsCloseToGround;
            lrMirror.mbLineTestIsValid       = lrSource.mbLineTestIsValid;
        }

        // (3) Mirror the car's speed. The console loads the whole mfSpeedMPH vector and keeps
        //     the x lane.
        mfSpeedMPH = lpCar->GetSpeedMPH().x;
    }

    // BrnPhysics::Vehicle::DebugComponent::RecordDownForce
    //   The tail of VehiclePhysics::UpdateDownForce: mirror the magnitude of the down-force it just
    //   applied, for the suspension page of the debug menu. The console inlines this, storing the
    //   force vector's y lane (the only lane the force has).
    void DebugComponent::RecordDownForce(Vector3 lvDownForce)
    {
        mfDownForceY = lvDownForce.y;
    }
}
}
