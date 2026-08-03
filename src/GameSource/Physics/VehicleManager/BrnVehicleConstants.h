#pragma once

// Vehicle-physics constants/enums. Recovered from the DecFIGS DWARF.
#include "types.hpp"

namespace BrnPhysics
{
namespace Vehicle
{
    // ⭐⭐ GRAVITY -- what actually accelerates a Burnout race car downward. [V] 2026-08-03.
    //
    // A race car is a BrnPhysics::ExternalPhysicsBody: the game integrates it ITSELF and only
    // publishes the pose to rw::physics. So the rw::physics SimulationParams::mGravity that
    // PhysicsModule::Prepare stage 3 builds -- (0, -9.81, 0) -- is NOT what moves it, and
    // ExternalPhysicsBody::CalculateNewVelocity @0x825A1B10 (which applies the force/impulse
    // accumulators) loads no constants at all. Gravity never enters the force accumulator.
    //
    // It is applied DIRECTLY to the body's linear velocity, once per car per frame, by
    // VehicleManager::ReadUpdatedBodies @0x82619A10, immediately before IntegrateTransform:
    //     lvx128    v0, r0, r15          ; r15 = &<this constant, splatted>
    //     vmulfp128 v13, v0, v127        ; g * dt
    //     lvx128    v0, r0, r10          ; r10 = &mBody.mLinearVelocity   (vehicle+0x50)
    //     vspltw    v0, v0, 1            ; .y
    //     vsubfp    v0, v0, v13          ; y - g*dt
    //     vrlimi128 v12, v0, 4, 0
    //     stvx128   v0, r0, r10
    //     bl        ExternalPhysicsBody::IntegrateTransform
    // i.e. **mLinearVelocity.y -= KF_GRAVITY * dt**, skipped entirely when the vehicle's frozen
    // byte (VehiclePhysics +0x70) is set. PhysicalTrafficManager::ReadUpdatedBodies @0x825EF608
    // does the identical thing for traffic. The same constant is also the suspension-stiffness
    // term: UpdateSuspensionSprings computes k = massOnSpring * g / restDisplacement, and the
    // DecFIGS DWARF names that local **lvfGravity** (VehiclePhysics.cpp:3202) -- which is what
    // identifies this slot by NAME and not merely by value.
    //
    // ⚠️ WHY FIVE WAVES OF LITERAL SCANS MISSED IT. On the X360 the value lives in a .data slot
    // (unk_82FB9160) that reads **all zeros in the image**; it is filled at static-init time by a
    // tiny unexported, IDA-unmarked initialiser at 0x82C5B128 that splats the .rdata scalar
    // flt_8208F83C. Read out of the IDB with headless IDA: flt_8208F83C == 0x411CF5C3 ==
    // 9.81000042f. No use site contains the literal, so "a scan of the export set for 9.81 finds
    // nothing in the vehicle chain" was a TRUE statement about the export set and a FALSE
    // conclusion about the game. (Console storage is a splatted VecFloat; the datum is the scalar.)
    const f32 KF_GRAVITY = 9.81000042f;   // X360 flt_8208F83C -> unk_82FB9160 (splat)

    // ⭐ How long the handbrake has to have been RELEASED before a car may enter a drift.
    // [V] 2026-08-03. Read by VehiclePhysics::CheckForEnteringDrift @0x825FA4E4 against the
    // TimeSinceLastHandBrake lane (+0x1080 .w):
    //     addi   r11, r3, 0x1080
    //     lvx128 v13, r0, r11 ; vspltw v13,v13,3      ; TimeSinceLastHandBrake
    //     lvx128 v0,  r0, <unk_82FB9170>
    //     vcmpgtfp. v0, v13, v0                       ; require t > this
    //
    // NAMED, not guessed: the PS3 DecFIGS build materialises the same slot through a TOC entry whose
    // symbol is `_ZN10BrnPhysics7Vehicle37KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFTE`
    // (PS3 CheckForEnteringDrift @0x6C89D4), i.e. BrnPhysics::Vehicle:: namespace scope -- which is
    // why it is homed here and not as a function-scope static. The lane the X360 tests it against
    // was independently named TimeSinceLastHandBrake by this tree before the constant was found.
    //
    // ⚠️ SAME TRAP AS KF_GRAVITY: unk_82FB9170 reads ALL ZEROS in the X360 image. It is filled at
    // static-init by an unexported, IDA-unmarked initialiser (disassembled at 0x82C5C9D8) that
    // splats the .rdata scalar flt_82001D9C == 2.0f. Console storage is a splatted VecFloat; the
    // datum is the scalar. Verified by reading that initialiser, NOT by trusting the splat-pattern
    // table -- the table mis-pairs multi-store blocks (it shows one slot receiving 2, 100 and 80).
    const f32 KVF_HANDBRAKE_OFF_TIME_TO_ALLOW_DRIFT = 2.0f;   // X360 flt_82001D9C -> unk_82FB9170

    // Severity/kind of a vehicle-vs-vehicle impact.
    enum EImpactType : s32
    {
        E_IMPACT_NONE         = 0,
        E_IMPACT_TRADING_PAINT = 1,
        E_IMPACT_NUDGE        = 2,
        E_IMPACT_SLAM         = 3,
        E_IMPACT_SHUNT        = 4,
        E_IMPACT_BOOST_SLAM   = 5,
        E_IMPACT_BOOST_SHUNT  = 6,
        E_IMPACT_GRINDING     = 7,
        E_IMPACT_RUBBING      = 8,
        E_IMPACT_COUNT        = 9
    };

    // Who-hit-whom classification of a race-car-vs-race-car impact (DWARF BrnVehicleConstants.h).
    // Used by the takedown classifier (VehicleManager::RaceCarResponseInfo::meImpactSitutation).
    enum EImpactSituation
    {
        E_IMPACT_SITUATION_INVALID     = -1,
        E_IMPACT_SITUATION_PLAYER_ON_AI = 0,
        E_IMPACT_SITUATION_AI_ON_PLAYER = 1,
        E_IMPACT_SITUATION_AI_ON_AI    = 2,
        E_IMPACT_SITUATION_NETWORK     = 3,
        E_IMPACT_SITUATION_COUNT       = 4,
    };

    // Lifecycle state of a traffic vehicle.
    enum ETrafficType : s32
    {
        E_TRAFFIC_TYPE_POTENTIAL = 0,
        E_TRAFFIC_TYPE_CRASHING  = 1,
        E_TRAFFIC_TYPE_PHYSICAL  = 2,
        E_TRAFFIC_TYPE_SLAMMED   = 3,
        E_TRAFFIC_TYPE_COUNT     = 4
    };

    // How a traffic vehicle was made to crash. DWARF home BrnTrafficPhysicsConstants.h:32;
    // homed here additively alongside its sibling ETrafficType (the project already keeps
    // the vehicle traffic enums in BrnVehicleConstants.h rather than forking a new header).
    // ADDITIVE GROW (flagged by Vehicle-events group): new enum, no change to existing types.
    // Plain (un-fixed) enum per the DWARF; the X360 copies it as a 4-byte word. Top enumerator
    // 255 forces an int-width underlying type, matching the 4-byte field stride in
    // TrafficSlammedEvent.
    enum eCrashTrafficType
    {
        eCrashTrafficType_Standard    = 0,
        eCrashTrafficType_Checked     = 1,
        eCrashTrafficType_Spontaneous = 2,
        eCrashTrafficType_Slammed     = 3,
        eCrashTrafficType_Invalid     = 255
    };
}
}
