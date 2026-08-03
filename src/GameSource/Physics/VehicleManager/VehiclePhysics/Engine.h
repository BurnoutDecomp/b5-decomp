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
// committed type from VehicleAttribs.h (included, not re-declared). Per project rule the absolute
// console offsets (which assume 32-bit pointer widths in EngineAttribs' InterpedParam3 etc.)
// are NOT cross-pointer static_asserted here; the leading-member offsets that ARE load-bearing
// for memcpy size (sizeof(EngineAttribs)==0xA0) are asserted in the embed check.
//
// ⚠️ RETIRED FORK (2026-08-03): this header used to declare its own `struct EngineAttribs` at
// NAMESPACE scope, `BrnPhysics::Vehicle::EngineAttribs`. The console's type is NESTED --
// `BrnPhysics::Vehicle::VehicleAttribs::EngineAttribs` -- which is what Engine::Construct
// @0x825F3EE8 calls (`bl VehicleAttribs::EngineAttribs::Construct` @0x825B7B90, read from the
// xrefs of that function). The two spellings mangle differently, so Engine.cpp was emitting a
// call to a symbol the console never had and no TU could ever define. That, and not
// "VehicleAttribs.cpp is unmountable", was the real reason this TU would not link.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4, VecFloat
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"   // VehicleAttribs::EngineAttribs (canonical home)

namespace BrnPhysics
{
namespace Vehicle
{
    // The canonical EngineAttribs is VehicleAttribs::EngineAttribs (VehicleAttribs.h). The old
    // namespace-scope slice that lived here is retired; this alias keeps the Engine TU's spelling
    // (`EngineAttribs`) while binding it to the console's nested type, so Engine::Construct emits
    // a call to the symbol that actually exists (@0x825B7B90).
    typedef VehicleAttribs::EngineAttribs EngineAttribs;

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
