#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h"

// Out-of-line bodies for BrnPhysics::Deformation::VehicleRigidBody::ApplyLocalImpulse (@0x8260E068)
// and ::RecievePassedOnImpulse (@0x8260DFA0).
//
// Both take a deformation ImpulseParams, turn it into a world-local impulse vector, and route that
// vector into the attached vehicle's physics body through one of three VehiclePhysics contact-impulse
// handlers, selected by the car's state and the contact kind. RecievePassedOnImpulse first consults a
// per-vehicle gate (the impulse-passing query) and early-outs if it is set; otherwise it applies the
// impulse exactly as ApplyLocalImpulse does. The two functions share the identical apply kernel; the
// asm is byte-for-byte the same sequence (only the params-base register differs: r11 vs r31).
//
// THE IMPULSE VECTOR (both functions, from the asm):
//   liImpulse = mpDirectionVectorTable[meImpulseDirection] * mvfImpulseMagnitude
//   asm: r9 = 16 * meImpulseDirection                       (stride 16 == one vec4 row)
//        lvx128 v0, params, +16   -> mvfImpulseMagnitude    (broadcast scalar VecFloat)
//        lvx128 v13, table, r9    -> the per-direction unit axis vector
//        vmulfp128 v1, v13, v0    -> the local impulse vector (component-wise multiply)
//
// THE ROUTING (both functions):
//   if   ( mpAttachedVehicle->IsCrashing() )                          // *(*(this+4)+1808) , 1808==0x710
//        ApplyCrashedContactImpulse( liImpulse, mImpulsePosition )    // v2 = params+32
//   elif ( params->mbWorldContact )                                   // *(params+184)
//        ApplyWallContactImpulse  ( liImpulse, mWorldImpulseDirection ) // v2 = params+48 (the normal)
//   else ApplyCarContactImpulse   ( liImpulse, mImpulsePosition )     // v3 = params+32
//
// MODELLED-vs-ASM NOTES:
//  * mpDirectionVectorTable is the X360 rodata at &unk_82FB9680 (6 vec4 rows, one per
//    ENextSensorDirection -- the signed body-axis unit vectors the impulse direction selects). Its
//    concrete bytes are NOT in the dossier, so the table is a correctly-shaped flagged-0 PLACEHOLDER
//    ([E_NSD_NUM][4] floats, 16-byte stride == the asm's `16 * meImpulseDirection`). With zero rows
//    the impulse vector is zero (faithful-but-inert); REPLACE the rows with the real X360 rodata when
//    it is homed. NEVER fabricated.
//  * mvfImpulseMagnitude is a broadcast VecFloat (the same scalar in all four lanes, the SDK idiom);
//    the vmulfp128 scales each lane of the direction row by it. Modelled as a per-lane multiply by the
//    scalar magnitude (.x lane), matching the broadcast multiply store-for-store.
//  * The two trailing bool args (ApplyCrashedContactImpulse::lbZeroResponse,
//    ApplyWallContactImpulse::lbContactPositionNotWorldSpace) are not set in this function's asm (the
//    argument register is left at its incoming value, which Hex-Rays does not track) -> passed false,
//    the faithful default for an unset register. FLAG: this is the only value the asm does not pin.
//  * RecievePassedOnImpulse's gate is the virtual dispatch `(*(**(this+4)+16))(*(this+4))` -- a query
//    on the attached vehicle through its vptr slot +0x10. ⭐⭐ SETTLED 2026-08-09 (crash/shunt wave):
//    the slot is IsPlayerVehicleInShowtime -- both concrete vtables were read off the image
//    (traffic default `li r3,0` @0x827E2F38; RaceCarPhysics override @0x825D7B68 in vtbl
//    @0x820D1034). The gate therefore reads "never re-apply passed-on deformation impulses to the
//    player's showtime vehicle"; the old role-inferred name IsIgnoringPassedOnImpulses is retired.
//    The second VecFloat param (the chain's passed-on magnitude) is carried by the signature but,
//    like the X360, is not consumed by the gate/apply path here.

namespace BrnPhysics
{
namespace Deformation
{
    // ⭐ TABLE RETIRED 2026-08-14 (walls leg 4): the flagged-zero KA_IMPULSE_DIRECTION_VECTORS
    // placeholder (X360 rodata &unk_82FB9680, dynamic-init so zero in the image) is now the REAL
    // shared BrnPhysics::Deformation::KA_IMPULSE_DIRECTIONS in BrnCollidableBody.cpp -- the PS3
    // exports name the global AND its rows' initializer (see that TU's banner). With the zero rows
    // every impulse this TU applied multiplied to zero (the silent-drop shape); now real.

    // The shared apply kernel of both functions: build the impulse vector and route it into the
    // attached vehicle. Factored out only for the bodies below to share -- the X360 inlines it into
    // both functions identically. (The PS3 keeps both out-of-line, @0x6E0A60/@0x6E0B8C, and both
    // resolve the row through CollidableBody::GetDirectionVector -- the same table access.)
    static void ApplyImpulseToVehicle(BrnPhysics::Vehicle::VehiclePhysics* lpVehicle,
                                      const ImpulseParams* lpImpulseParams)
    {
        // liImpulse = direction-row * magnitude   (vmulfp128 v1, v13, v0)
        const Vector3& lrDirection = KA_IMPULSE_DIRECTIONS[lpImpulseParams->meImpulseDirection];
        const f32 lfMagnitude = lpImpulseParams->mvfImpulseMagnitude.x;   // broadcast VecFloat lane
        const Vector3 liImpulse =
        {
            lrDirection.x * lfMagnitude,
            lrDirection.y * lfMagnitude,
            lrDirection.z * lfMagnitude,
            lrDirection.w * lfMagnitude,
        };

        // Route by car state then contact kind.
        if ( lpVehicle->IsCrashing() )                       // *(*(this+4)+1808)
        {
            const Vector3& lrContactPosition = lpImpulseParams->mImpulsePosition;   // params+32 (v2)
            // 0x8260E088 `li r4,1` (BODY_SPACE impulse) and 0x8260E090 `lwz r5,0x50(r11)`
            // (== ImpulseParams::mePositionSpace) are set ONCE, above the three-way branch, so
            // all three Apply*ContactImpulse variants receive the same pair. r6 is the bool.
            lpVehicle->ApplyCrashedContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                                  lrContactPosition, lpImpulseParams->mePositionSpace,
                                                  false);
        }
        else if ( lpImpulseParams->mbWorldContact )          // *(params+184)
        {
            const Vector3& lrContactNormal = lpImpulseParams->mWorldImpulseDirection;   // params+48 (v2)
            lpVehicle->ApplyWallContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                               lrContactNormal, lpImpulseParams->mePositionSpace);
        }
        else
        {
            const Vector3& lrContactPosition = lpImpulseParams->mImpulsePosition;   // params+32 (v3)
            lpVehicle->ApplyCarContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                              lrContactPosition, lpImpulseParams->mePositionSpace);
        }
    }

    // @0x8260E068  BrnVehicleRigidBody.cpp:186
    void VehicleRigidBody::ApplyLocalImpulse(ImpulseParams* lpImpulseParams)
    {
        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);
    }

    // @0x8260DFA0  BrnVehicleRigidBody.cpp:131
    void VehicleRigidBody::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams,
                                                  VecFloat /*lvfPassedMagnitude*/)
    {
        // Gate: virtual dispatch through the attached vehicle's vptr slot +0x10 -- image-settled
        // as IsPlayerVehicleInShowtime (see the banner). A non-zero return means the player's
        // showtime car swallows further passed-on impulses -> do not apply.
        if ( mpAttachedVehicle->IsPlayerVehicleInShowtime() )
            return;

        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams);
    }
}
}
