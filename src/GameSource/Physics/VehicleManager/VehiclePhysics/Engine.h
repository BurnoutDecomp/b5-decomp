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

#include <cstddef>

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

        // @0x825CF010: the automatic-gearbox selector. Returns the highest gear 1..5 whose gear-up
        // RPM threshold is exceeded by `|drive * gearRatio[g] * Differential * 60/(2*pi)|`, or 0
        // (reverse/neutral) when the drive is below -0.01.
        // ⚠️ The parameter is a VecFloat, not an f32: the asm's compare and multiplies use the
        // vector register v1 (`vcmpgefp. v0,v1,v0`, `vmulfp128 v12,v1,v12`), and a PPC `f32`
        // argument would arrive in f1. It was declared `f32` here until 2026-08-03.
        s32 ComputeGear(VecFloat lvfEngineDrive) const;

        // @0x825BFDA0: the rev limiter mapped through the current gearing --
        //   maxWheelOmega = MaxRPM / (Differential * gearRatio[mu8CurrentGear])
        // with a div-by-zero guard on the denominator. Returns the magnitude broadcast across a
        // Vector4 (the X360 stores it via stvx128 into the caller's result buffer).
        Vector4 GetMaxWheelAngularVelocity() const;

        // @0x825CB288: integrate the flywheel + recompute RPM + ComputeGear -- THE POWERTRAIN
        // TORQUE CORE (throttle -> clutch -> flywheel -> gearbox -> drive force). ⛔ STILL A LOUD
        // TRAP (VehiclePhysicsLinkStubs.cpp), deferred to its own wave: this function ships on
        // BOTH readable consoles as a debug Opt-vs-Unopt assert harness -- X360 @0x825CB288 is
        // 3937 asm lines whose only callees are CgsDev::Assert / StrStream / GetVaultArray /
        // BasePriorityQueue::Clear, and the PS3 DecFIGS copy @0x712834 is 10324 lines with 1173
        // assert references. Reconstructing the real torque math out of that harness is a whole
        // wave; ApplyEngineForces (bodied) calls it and OntoWheels (bodied) reads the mvEngineDrive
        // lane it produces, so the *application* layer is real while the torque model stays deferred
        // behind the trap. The 9-arg signature is the PS3 mangled name
        // (_ZN...6Engine6UpdateEN2rw4math3vpu8VecFloatES5_S5_bS5_S5_bS5_S5_) laid against the X360
        // ApplyEngineForces @0x8261FC10 register map (v1..v7 + r4/r5), so ApplyEngineForces emits
        // the call the console had and no future body will ODR-clash.
        void Update(VecFloat lvfWheelAngularVelocity, VecFloat lvfGas, VecFloat lvfBrake,
                    bool lbHandBrake, VecFloat lvfSteering, VecFloat lvfRearWheelRadius,
                    bool lbAllowReverseDrive, VecFloat lvfForwardSpeed, VecFloat lvfTimeStep);

        // [PC-leaf accessor] ApplyEngineForcesOntoWheels @0x825FB000 reads the engine's drive-force
        // lane directly off the embedded engine (`lvx128 v0, this+0xFA0 ; vspltw v0,v0,0` == mEngine
        // (+0xF00) + 0xA0 lane0). Engine::Update (the trapped powertrain core) writes it. Exposed as
        // a named getter so the host reads the named member instead of an offset cast.
        f32 GetEngineDrive() const
        {
            return mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay.x;
        }

        // @0x825CF130..0x825CF274 (82 items): seed the running-state registers from a wheel
        // angular velocity -- zero the drive/torque/clutch lanes, park the flywheel at idle
        // (1000 RPM in rad/s), pick a gear, derive RPM from the gearing, and re-arm both
        // allow-change flags. Bodied in Engine.cpp.
        // ⚠️ EXPORT-SET HOLE (the fourth): no JSON in .ida-exports; ComputeGear @0x825CF010 is 72
        // instrs so it ends exactly at 0x825CF130, and the next indexed symbol is 0x825CF278.
        void Reset(VecFloat lvfWheelAngularVelocity);

        // [PC-leaf accessor] The console's UpdateDriving @0x82638248 pokes the two allow-change
        // bytes of the embedded engine directly (`stb rX, 0xFC4(r31) ; stb rX, 0xFC5(r31)` --
        // always the SAME value, 1 on the ground / 0 in the air). Exposed as one named setter
        // so the host write stays on the named members instead of an offset cast.
        void SetAllowGearChanges(bool lbAllow)
        {
            mbAllowToChangeUpGear   = lbAllow;   // +0xC4
            mbAllowToChangeDownGear = lbAllow;   // +0xC5
        }

        // [PC-leaf accessor] The console's wheel cluster reads the embedded engine's gear word
        // directly off the owner (`lwz rX, 0xFC0(r31)` in UpdateWheels @0x8261E7B4 /
        // UpdateWheelInertia @0x825F67xx / UpdateBrakesAndGetBrakingFactor @0x825D02xx --
        // VehiclePhysics base +0xF00 == mEngine, +0xC0 == mu8CurrentGear). Gear 0 is
        // reverse/neutral (ComputeGear's `< -0.01` leg); Prepare seeds ku8FirstGear == 1.
        // Exposed as one named getter so those hosts read the named member, not an offset cast.
        u32 GetCurrentGear() const { return mu8CurrentGear; }

        // --- Remaining Engine API: owned by separate future TUs -- declared only (no body). ---

    private:
        // ---- THE ASSERT SET ------------------------------------------------------------------
        // Engine's running state is pure POD (no pointers), so the console's absolute offsets
        // survive the x64 widening unchanged and CAN be asserted. Defined in Engine.cpp -- it has
        // to be a member so that `offsetof` may name the private members below, and it has to be
        // out-of-line so that `Engine` is complete at the point the asserts are evaluated. Never
        // called, never emitted.
        //
        // ⚠️ A `sizeof` assert would be PERMUTATION-BLIND here: swapping the two Vector4s, or the
        // two bools, keeps sizeof(Engine) at 0xD0. The per-member offsetof block in that function
        // is what catches those, and it is the part to extend. Tamper-tested 2026-08-03.
        static void BpAssertConsoleLayout();

        EngineAttribs mAttribs;                                                                  // +0x00
        Vector4 mvEngineDrive_ReactionTorque_FlyWheelAngularVelocity_ClutchDelay;                 // +0xA0
        Vector4 mvClutchFactor_RPM_CurrentGearChangeTime;                                         // +0xB0
        u32     mu8CurrentGear;                                                                   // +0xC0
        bool    mbAllowToChangeUpGear;                                                            // +0xC4
        bool    mbAllowToChangeDownGear;                                                          // +0xC5
    };
}
}
