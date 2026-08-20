#pragma once

// Vehicle-physics constants/enums. Recovered from the DecFIGS DWARF.
#include "types.hpp"
#include "BrnCommonTypes.h"  // VecFloat

namespace BrnPhysics
{
namespace Vehicle
{
    // Surface-property bank (DecFIGS BrnVehicleConstants.h/.cpp; Breaker globals
    // byte_82FB7DF0, dword_82F2A10C, unk_82FB8DE0/8890/8BD0 and byte_82FB7DF4).
    // ReadSurfaceProperties replaces the three vector tables and water flags from the
    // live AttribSys surface list. Breaker statically seeds all vector lanes to zero and
    // the used-surface count to 20.
    const s32 KI_MAX_NUM_SURFACES = 32;
    extern bool     gbReadSurfaceProperties;
    extern s32      KI_NUM_USED_SURFACES;
    extern VecFloat KAVF_SURFACE_ROUGHNESS[KI_MAX_NUM_SURFACES];
    extern VecFloat KAVF_SURFACE_GRIP[KI_MAX_NUM_SURFACES];
    extern VecFloat KAVF_SURFACE_LINEAR_DRAG[KI_MAX_NUM_SURFACES];
    extern bool     KAB_SURFACE_IS_WATER[KI_MAX_NUM_SURFACES];

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

    // ⭐ DE-DUPLICATED HERE 2026-08-03 (task #113). The EntityId owner-type byte (bits 24..31) that
    // the traffic/articulation code asserts for physics-traffic ids ("...GetOwner() ==
    // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE", value 2). It was defined TWICE at BrnPhysics::Vehicle
    // namespace scope -- BrnPhysicalTrafficManager.h:272 and BrnArticulatedJoint.h:42 -- each with a
    // comment saying it mirrored the other. Harmless while the two headers could never meet; a hard
    // C2374/C2086 the moment the ArticulatedJointPool de-fork made them meet. One owner now, and
    // this is the header the DWARF already homes the vehicle constants in.
    // FLAG (unchanged from both old comments): the full BrnWorld::EEntityType enum is owned by the
    // World module; only the one value these TUs use is reproduced.
    const u32 KU_ENTITYTYPE_TRAFFIC_VEHICLE = 2;

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

    // ⭐ ADDED 2026-08-10 (producer wave). The bounding-sphere radius every vehicle -- race car
    // AND traffic -- claims its triangle-cache slot with. ONE datum, TWO readers: both
    // VehicleManager::PrepareTriangleCache @0x82615BE4 and PhysicalTrafficManager::
    // PrepareTriangleCache @0x825EE5E8 load the SAME .rdata slot `flt_8200426C`, which is why it
    // is homed here (the shared vehicle-constants header) rather than duplicated at either site.
    //
    // ⭐ READ FROM THE IMAGE, not from Hex-Rays: x360rd.py at 0x8200426C returns
    // `40a00000` == 5.0f, on a read whose 10-point calibration passed. (The neighbouring word
    // is 0x40400000 == 3.0f, so the slot is not ambiguous.)
    //
    // WHAT IT MEANS: TriangleCacheManager::ProcessAddToCacheEvents stamps it into the slot's
    // mLastCachedSphere.w, and StartUpdateTriangleCaches then copies that sphere verbatim as the
    // PolygonSoupListSpatialMap query -- so this is the radius of the world-triangle neighbourhood
    // kept live around every car.
    const f32 KF_TRIANGLE_CACHE_SPHERE_RADIUS = 5.0f;   // X360 flt_8200426C

    // ⭐ ADDED 2026-08-13 (wheel-transform wave). The two crash-visual constants of
    // SimpleVehiclePhysics::GetWheelsWorldTransfrom @0x825D8878. Both are BrnPhysics::Vehicle
    // namespace-scope (the PS3 DecFIGS pseudocode names them through TOC symbols:
    // `KVF_MAX_BUCKLE_ANGLE_CRASHING` / `KAVF_WHEEL_TWIST_DIRECTIONS`), and BOTH are the
    // KF_GRAVITY trap again: their .data slots (0x82FB9070 / 0x82FB91E0) read ALL ZEROS in the
    // X360 image and are filled by unexported static initialisers, read out of the image this
    // wave (bank: wheeltransform_bank.md §5.2/§5.3):
    //
    //   @0x82C5D1E0: lfs f0, 0x4744(r11=0x82000000) ; vspltw ; stvx128 -> 0x82FB9070
    //                *(f32*)0x82004744 == 0x3E4CCCCD == 0.2f
    //   @0x82C5D230: vspltisw128 v0, 1 / v13, -1 ; vcsxwfp128 (int->float) ;
    //                stvx128 v0 @+0x00, v13 @+0x10, v0 @+0x20, v13 @+0x30 -> 0x82FB91E0
    //
    // KVF_MAX_BUCKLE_ANGLE_CRASHING clamps the crash "buckle" rotation (about the local Z
    // axis): angle = min((2*|posZ - streamedZ|)^2, this). ⚠️ The SQUARE is the angle -- both
    // platforms agree instruction-for-instruction; it LOOKS like a porter-bait bug, do not
    // "fix" it. 0.2 rad ~= 11.46 degrees.
    // Console storage is a splatted VecFloat; the datum is the scalar.
    const f32 KVF_MAX_BUCKLE_ANGLE_CRASHING = 0.2f;    // X360 flt_82004744 -> 0x82FB9070 (splat)

    // Per-wheel sign of the crash "twist" rotation (about the local Y axis), indexed by
    // EVehicleDrivenWheel: FL=+1, FR=-1, RL=+1, RR=-1 -- twist mirrors by side, as the name
    // promises. GetWheelsWorldTransfrom loads element [16*leWheel] (each element is a splatted
    // VecFloat on console; the datum per element is the scalar).
    const f32 KAVF_WHEEL_TWIST_DIRECTIONS[4] = { 1.0f, -1.0f, 1.0f, -1.0f };  // 0x82FB91E0
}
}
