#pragma once

// BrnPhysics::Vehicle::RaceCarPhysics -- the player/AI race-car physics body (a VehiclePhysics
// subclass that adds showtime, bounce-boost, target-assist and crash state).
//
// MINIMAL OWNING SLICE. The full RaceCarPhysics carries a large PlayerParameters block and ~100
// methods owned by separate future TUs. THIS group bodies only the three ledger funcs
// GetHeightAboveRoad @0x825B3998, IsCrashingNormally @0x827E42B8 and
// IsPlayerVehicleActuallyInShowtime @0x827E42B0; the rest of the API is NOT declared here. The
// single bool member the latter two read (mbPlayerCarInShowtime) is reconstructed BY NAME.
//
// LAYOUT NOTES (from the asm):
//   * mbPlayerCarInShowtime is read at this+0x140C by both IsPlayerVehicleActuallyInShowtime
//     (`lbz r3,0x140C(r3); blr`) and IsCrashingNormally. Per project rule the absolute console
//     offset is pinned BY NAME (the ~0x1400 bytes of base+VehiclePhysics state that precede it
//     are not reproduced as padding here).
//   * GetHeightAboveRoad iterates the four driven wheels (VehiclePhysics::maWheels, console
//     stride 0xE0) reading each wheel's road-contact result and the vehicle up axis -- accessed
//     here BY NAME through the VehiclePhysics accessors, not by absolute offset.

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector3, EntityId
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehiclePhysics.h"   // base + Wheel
#include "rw/math/vpu/vector3_operation.h"   // rw::math::vpu::{Dot, Subtract}

namespace BrnPhysics
{
namespace Vehicle
{
    class RaceCarPhysics : public VehiclePhysics
    {
    public:
        // @0x827E42B0: trivial getter -- true while the player car is actually in showtime.
        //   asm: lbz r3,0x140C(r3); blr
        bool IsPlayerVehicleActuallyInShowtime() const
        {
            return mbPlayerCarInShowtime;
        }

        // @0x827E42B8: a crash counts as "normal" (i.e. NOT a special showtime/bounce crash)
        // unless the player car is in showtime AND the global bounce-boosting flag is set.
        //   asm: if (!mbPlayerCarInShowtime) return true;
        //        if (!<global bounce flag>) return true;
        //        return false;
        // FLAG (rodata): the global byte (byte_82FB84B2, sited next to lbBounceBoosting) is an
        // un-homed module static; carried here as an honest extern-declared global rather than a
        // fabricated value. The Vehicle-physics group does NOT home it (it belongs to a showtime/
        // bounce TU) -- declared extern so this leaf resolves it BY NAME.
        bool IsCrashingNormally() const;

        // @0x825B3998: the (signed) height of the car above the road, taken as the MINIMUM over the
        // driven wheels whose road-contact line test is valid AND that are on the ground (the
        // contact normal points up: dot(normal, up) > 0.5). For each such wheel the height is the
        // projection of the wheel position relative to the contact onto the contact normal. Wheels
        // that fail the on-ground test do not lower the running minimum. The X360 returns the
        // result broadcast across a VMX register; here a flat Vector3 with the height in every lane.
        //
        // FLAG (semantic): the asm's per-wheel reference vector (the `v1` operand of the
        // `vsubfp v12,v1,v12` height step) is not separately homed -- it is the wheel's own contact
        // position relative to the contact plane, reconstructed here as
        // dot(position - contactPoint, normal). The on-ground threshold (0.5) and the seed "max"
        // value are the values the X360 decompiler resolved for the inlined constants.
        Vector3 GetHeightAboveRoad() const;

        // ----- ADDITIVE GROW (Deformation car-car-impulse group): two bounce-state methods the
        //       car-car shunt path calls. DECLARE-ONLY -- their bodies are owned by a separate
        //       showtime/bounce RaceCarPhysics TU; ApplyCarCarImpulse only needs the declarations
        //       to compile under the per-TU `cl /c` gate. Signatures are DWARF-authoritative
        //       (references/DecFIGS/dwarfdump/.../RaceCarPhysics.h:331 / :382). -----

        // @ DWARF :382. True while this race car is currently bounce-boosting (showtime bounce).
        // The car-car bounce shaping picks a different boost-scale vector depending on this.
        bool IsBounceBoosting() const;

        // @ DWARF :331. Record that this race car just bounced off another body this frame: the
        // bounce impulse direction, whether this is the first/chained bounce, whether the impact
        // exceeded the minimum bounce stress, and the other entity's id. Declare-only.
        void SetJustBounced(Vector3 lvBounceDirection, bool lbFirstBounce, bool lbOverMinStress,
                            EntityId lOtherEntityId);

    private:
        bool mbPlayerCarInShowtime;   // +0x140C (pinned BY NAME)
    };
}
}
