#pragma once

// BrnPhysics::Vehicle::Engine -- the per-vehicle engine model (gearbox + flywheel + clutch).
//
// MINIMAL OWNING SLICE. The full Engine class carries ~40 methods (Update/SetGear/GetRPM/
// torque & gear computation, etc.) owned by separate future TUs. THIS group bodies only the
// two ledger funcs Construct @0x825F3EE8 and Prepare @0x825F3F38; the remaining API listed in
// the DecFIGS DWARF (references/DecFIGS/dwarfdump/.../VehiclePhysics/Engine.h) is declared-only
// and honest so those TUs can define the bodies later without an ODR clash.
//
// LAYOUT (confirmed against the Construct/Prepare asm at 0x825F3EE8 / 0x825F3F38):
//   mAttribs  @ +0     -- EngineAttribs (0xA0 = 160 bytes); Prepare's `memcpy(this, src, 160)`
//                         copies the attribs block into this leading member.
//   mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay @ +0xA0 (Vector4)
//   mvClutchFactor_RPM_CurrentGearChangeTime                         @ +0xB0 (Vector4)
//                         -- Prepare's `addi r11,this,0xB0` + vrlimi128 writes the clutch-factor
//                            lane of this register.
//   mu8CurrentGear @ +0xC0 (u32) -- Prepare's `stw <1>, 0xC0(this)` sets it to ku8FirstGear (1).
//   mbAllowToChangeUpGear / mbAllowToChangeDownGear (bool)
//
// Members are pinned BY NAME + SEQUENCE per the DWARF; the EngineAttribs sub-type is the
// committed type from VehicleAttribs.cpp (reused by name). Per project rule the absolute
// console offsets (which assume 32-bit pointer widths in EngineAttribs' InterpedParam3 etc.)
// are NOT cross-pointer static_asserted here; the leading-member offsets that ARE load-bearing
// for memcpy size (sizeof(EngineAttribs)==0xA0) are asserted in the embed check.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4, VecFloat

namespace BrnPhysics
{
namespace Vehicle
{
    // FLAG (dep): the canonical EngineAttribs home is currently INSIDE VehicleAttribs.cpp
    // (as VehicleAttribs::EngineAttribs, sizeof 0xA0, with Construct()) -- there is NO shared
    // header exporting it, so the Engine TU cannot #include it. A minimal OWNING slice is
    // declared here so Engine's two funcs resolve mAttribs BY NAME (its leading member, copied
    // wholesale by Prepare's 160-byte memcpy and constructed by Construct). When VehicleAttribs
    // gets a real header, this slice should be REPLACED by an include of the committed type
    // (the 0xA0 size + Construct() signature already match). Members are intentionally an opaque
    // 0xA0 byte block here (the attribs' internal lanes are not read by Construct/Prepare).
    class VehicleAttribsEngineSliceTag;  // doc anchor only

    // A 4-point RPM->torque lookup curve (BrnPhysics::InterpedParam3). The Engine TU only needs
    // it as a 16-byte leading member of EngineAttribs (it is not sampled by ComputeGear /
    // GetMaxWheelAngularVelocity, which read the scalar gearing lanes); declared opaque here so
    // the slice's sizeof + member offsets match the committed VehicleAttribs::EngineAttribs.
    struct EngineTorqueCurve
    {
        Vector4 mvParams;   // BrnPhysics::InterpedParam3::mvParams
    };

    struct EngineAttribs
    {
        // Build the default engine attributes (gear ratios/torque curve/flywheel). Owned by
        // VehicleAttribs.cpp -- declared only here.
        void Construct();

        // ----- ADDITIVE GROW (engine/drivetrain group): the gearing lanes ComputeGear and
        //       GetMaxWheelAngularVelocity read BY NAME. The console EngineAttribs is a 0xA0 block
        //       (InterpedParam3 + 3 Vector4 + 6 Vector3); these accessors expose the lanes those two
        //       Engine methods index (Differential, MaxRPM, per-gear ratio/torqueScale/gearUpRPM).
        //       The member SEQUENCE/sizes mirror the committed VehicleAttribs::EngineAttribs exactly
        //       (same lvx128 offsets); sizeof stays 0xA0. When VehicleAttribs gets a real header this
        //       whole slice is replaced by an include of that type. -----
        f32 GetDifferential() const  { return mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM.x; }
        f32 GetMaxRPM() const        { return mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges.z; }
        f32 GetGearRatio(s32 liGear) const   { return mavGearRatios_TorqueScales_GearUpRPMs[liGear].x; }
        f32 GetGearUpRPM(s32 liGear) const   { return mavGearRatios_TorqueScales_GearUpRPMs[liGear].z; }

    private:
        // @+0x00 mTorqueCurve (InterpedParam3, 16B)
        EngineTorqueCurve mTorqueCurve;
        // @+0x10 (.x=Differential .y=TransmissionEfficiency .z=EngineResistance .w=GearDownRPM)
        Vector4 mvDifferential_TransmissionEfficiency_EngineResistance_GearDownRPM;
        // @+0x20 (.x=MaxTorque .y=TorqueFallOffRPM .z=MaxRPM .w=LSDMSpeedToAllowGearChanges)
        Vector4 mvMaxTorque_TorqueFallOffRPM_MaxRPM_LSDMSpeedToAllowGearChanges;
        // @+0x30 (.x=FlyWheelInertia .y=FlyWheelFriction .z=GearChangeTime)
        Vector4 mvFlyWheelInertia_FlyWheelFriction_GearChangeTime;
        // @+0x40 per-gear (.x=gearRatio .y=torqueScale .z=gearUpRPM), 6 gears * 16B = 0x60 -> ends @0xA0
        Vector3 mavGearRatios_TorqueScales_GearUpRPMs[6];
    };
    // Gear-index constants (DWARF Engine.h:55-57). ku8ReverseGear is the sentinel below first
    // gear (0); first gear is 1; highest is 5. Declared here as the shared engine vocabulary.
    static const u8 KU8_REVERSE_GEAR = 0;
    static const u8 KU8_FIRST_GEAR   = 1;
    static const u8 KU8_HIGHEST_GEAR = 5;

    class Engine
    {
    public:
        // --- This group's two ledger functions (bodied in Engine.cpp) ---

        // @0x825F3EE8: construct the engine -- build the default attribs block then Reset() the
        // running state to a stopped engine at zero wheel angular velocity.
        void Construct();

        // @0x825F3F38: (re)prepare from a supplied attribs block -- copy the attribs in, seed the
        // clutch factor + RPM lane, select first gear, and Reset() the running state. Returns true.
        bool Prepare(const EngineAttribs* lpAttribs);

        // --- ADDITIVE GROW (engine/drivetrain group): two clean gearbox leaves (bodied in Engine.cpp) ---

        // @0x825CF010: the automatic-gearbox selector. Given the current engine drive (lfEngineDrive,
        // passed in v1 = mvEngineDrive lane0 by the caller), returns the highest gear 1..5 whose
        // gear-up RPM threshold is exceeded by `engineDrive * Differential * gearRatio[g] *
        // KF_RPM_GEAR_METRIC`, or 0 (reverse/neutral) when engineDrive < -0.0099999998.
        s32 ComputeGear(f32 lfEngineDrive) const;

        // @0x825BFDA0: the rev limiter mapped through the current gearing --
        //   maxWheelOmega = MaxRPM / (Differential * gearRatio[mu8CurrentGear])
        // with a div-by-zero guard on the denominator. Returns the magnitude broadcast across a
        // Vector4 (the X360 stores it via stvx128 into the caller's result buffer).
        Vector4 GetMaxWheelAngularVelocity() const;

        // @0x825CB288: integrate the flywheel + recompute RPM + ComputeGear. Ships on X360 as a
        // debug Opt-vs-Unopt assert harness (degenerate pseudocode) -- owned/BLOCKED by this group's
        // ledger, declared only here so ApplyEngineForces can call it without an ODR clash.
        void Update(/* dt + control/contact args; see ApplyEngineForces call site */);

        // --- Remaining Engine API: owned by separate future TUs -- declared only (no body). ---
        // Reset seeds the running-state registers from a wheel angular velocity; it is called by
        // both Construct and Prepare but lives in its own TU.
        void Reset(VecFloat lvfWheelAngularVelocity);

    private:
        EngineAttribs mAttribs;                                                                  // +0x00
        Vector4 mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay;                 // +0xA0
        Vector4 mvClutchFactor_RPM_CurrentGearChangeTime;                                         // +0xB0
        u32     mu8CurrentGear;                                                                   // +0xC0
        bool    mbAllowToChangeUpGear;
        bool    mbAllowToChangeDownGear;
    };
}
}
