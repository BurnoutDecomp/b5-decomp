#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"   // CgsDev::DebugComponent (real base)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"   // CgsGeometric::Triangle4::AOSTriangle
#include "GameSource/Physics/VehicleManager/VehiclePhysics/Wheel.h"     // Wheel::RoadContact, Vector3
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnGripCurveDebugWindow.h"   // GripCurveDebugWindow

// BrnPhysics::Vehicle::DebugComponent - the in-game handling/grip-curve debug menu for the
// vehicle physics, and BrnPhysics::Vehicle::GearStats, the per-gear sample row it accumulates.
//
// LAYOUT. The member set and order below are the recovered class's, complete: laid onto the
// console's 1024-byte object they account for every byte of it, with no gap left over and no
// member left unseated. The per-member console offsets are in the trailing comments; the closure
// is asserted, seat by seat, by _AssertLayout() in this class's .cpp.
//
// This is a HOST layout, not a console-pinned one: the base's vptr and list link, the component's
// own car pointer and each window's two wheel pointers all widen 4 -> 8, so the members sit at
// host offsets and the class is larger than 1024. That divergence is CARRIED, exactly the way
// VehicleManager already carries four others - as a named, tripwired drift term
// (KU_HOST_DRIFT_AFTER_RACECAR_DEBUG_COMPONENTS), derived from this class's own sizeof so it
// cannot go stale. VehicleManager embeds `DebugComponent maRaceCarDebugComponent[8]` by value and
// the array subscript does the striding, so nothing in the tree spells a slot stride by hand.
//
// The render/HUD side (RenderWorld, RenderHUD and the ten draw/report leaves they dispatch to,
// PrimitiveVehicleDebugRender2D, and the OnActivate menu registration) is owned by its own dev-UI
// pass. The toggles those read are declared here because they are part of the layout and Construct
// seeds every one of them.

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics;

    // One gear's worth of sampled shift data, 28 bytes. The component keeps six rows and
    // accumulates into the row miCurrentGear names until the gear-stat report closes it.
    class GearStats
    {
    public:
        // Back to open and empty. The component's ResetGearStats runs this over all six rows.
        void Reset();

        f32  mfTimeInGear;                 // +0x00
        f32  mfSpeedOutOfGearGoingUp;      // +0x04
        f32  mfSpeedOutOfGearGoingDown;    // +0x08
        f32  mfRpmWhenChangedOutGoingUp;   // +0x0C
        f32  mfRpmWhenChangedOutGoingDown; // +0x10
        f32  mfRpmWhenChangedInGoingUp;    // +0x14
        bool mbDoneUpdating;               // +0x18 -- row closed, stop accumulating
    };

    class DebugComponent : public CgsDev::DebugComponent
    {
    public:
        // Two-phase init, one per car, run from VehicleManager::PrepareData. Records the car this
        // component samples, seats the two grip-curve windows, clears the gear stats and every
        // draw toggle, and zeroes the per-wheel and last-wall-triangle scratch. Bodied in this TU.
        void Construct(RaceCarPhysics* lpRaceCarPhysics);

        // Clear all six gear-stat rows. STATIC, and it takes the component as a void*, because it
        // is registered with the debug menu as a plain "Reset gear stats" action callback -- the
        // console passes the component as the callback's user data and the body reads it straight
        // out of the first argument. Construct inlines it; the gear-stat report is what closes a
        // row again.
        static void ResetGearStats(void* lpComponent);

        // Stash the collision triangle of the last wall the car scraped, for the
        // DrawLastWallTriangle debug overlay. Copies the whole 80-byte AOSTriangle into
        // mLastWallTriangle.
        void SetLastWallTriangle(const CgsGeometric::Triangle4::AOSTriangle* lpTriangle);

        // The per-frame tick VehicleManager::UpdateVehiclePhysics calls once per live car with
        // the sim timestep. Samples only: gear-stats time accumulate, the four wheels'
        // road-contact mirror, and the speed mirror. Bodied in this TU.
        void Update(f32 lfTimeStep);

        // Mirror the magnitude of the down force VehiclePhysics::UpdateDownForce just applied, for
        // the suspension page of the debug menu. Inlined by the console at its one call site.
        void RecordDownForce(Vector3 lvDownForce);

    protected:
        // The debug-menu leaf name and the group it is filed under: "Physics/Handling".
        // DebugComponent::Register reads both through the vtable when it threads this component
        // onto the debug manager's list.
        const char* GetName() const override;
        const char* GetPath() const override;

    private:
        // Compile-time seat gate. Every member below is pinned to its console offset through the
        // cumulative host-widening chain; defined in this class's .cpp so it compiles with the
        // class. Never called.
        static void _AssertLayout();

        // ---- the sampled mirror of the car, refreshed every tick by Update ---------------------
        Wheel::RoadContact   maRoadContacts[4];                  // +0x010 (console stride 0x30)

        // ---- the two grip-curve windows, one per axle ------------------------------------------
        GripCurveDebugWindow mFrontWheelsGripCurveWindow;        // +0x0D0 (console 0xC0 apiece)
        GripCurveDebugWindow mRearWheelsGripCurveWindow;         // +0x190

        // ---- the car this component samples, and the engine/gear sample state -------------------
        RaceCarPhysics*      mpRaceCarPhysics;                   // +0x250
        f32                  mafInertia[3];                      // +0x254
        s32                  miCurrentGear;                      // +0x260 -- which row Update fills
        f32                  mfLastFrameRpm;                     // +0x264
        GearStats            mGearStats[6];                      // +0x268 (stride 28)
        f32                  mfSpeedMPH;                         // +0x310

        // ---- the handling-tweak scratch the debug menu exposes as editable variables ------------
        f32                  mafHackComOffset[3];                // +0x314
        f32                  mfHackCrashExtraPitchVelocity;      // +0x320
        f32                  mfHackCrashExtraYawVelocity;        // +0x324
        f32                  mfHackCrashExtraRollVelocity;       // +0x328
        f32                  mfHackCrashExtraLinearVelocity;     // +0x32C
        f32                  mfRPM;                              // +0x330
        f32                  mfEngineDrive;                      // +0x334
        f32                  mfReactionTorque;                   // +0x338
        f32                  mfFlyWheelAngularVelocity;          // +0x33C
        f32                  mfClutchDelay;                      // +0x340
        f32                  mfClutchFactor;                     // +0x344

        // ---- the last wall-collision triangle recorded for the debug overlay --------------------
        CgsGeometric::Triangle4::AOSTriangle mLastWallTriangle;   // +0x350 (80 bytes)

        // ---- the draw / report toggles, one contiguous run of fourteen bytes --------------------
        bool                 mbRenderCOMPosition;                // +0x3A0
        bool                 mbRenderHandlingBody;               // +0x3A1
        bool                 mbOutputGearStats;                  // +0x3A2
        bool                 mbOutputDeformationStats;           // +0x3A3
        bool                 mbShowTireFrictions;                // +0x3A4
        bool                 mbRenderFrontWheelsGripCurves;      // +0x3A5
        bool                 mbRenderRearWheelsGripCurves;       // +0x3A6
        bool                 mbDrawTractionLines;                // +0x3A7
        bool                 mbDrawTractionLineNormals;          // +0x3A8
        bool                 mbDrawAboveGroundLineTest;          // +0x3A9
        bool                 mbDrawLastWallTriangle;             // +0x3AA
        bool                 mbDrawAngularVelocity;              // +0x3AB
        bool                 mbDrawSuspensionForces;             // +0x3AC
        bool                 mbStoredPhysics;                    // +0x3AD

        // ---- the per-wheel suspension-force record the overlay draws ----------------------------
        Vector3              maSuspensionForces[4];              // +0x3B0 (stride 0x10)
        bool                 mabSuspensionForceApplied[4];       // +0x3F0
        f32                  mfDownForceY;                       // +0x3F4 -- VehiclePhysics'
                                                                 //           down-force mirror
    };
}
}
