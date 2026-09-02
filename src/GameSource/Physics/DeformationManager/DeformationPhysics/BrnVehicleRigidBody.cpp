#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnVehicleRigidBody.h"

#include <cstdlib>                                          // getenv -- the opt-in [chainarrive] probe only
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint -- the opt-in [chainarrive] probe only

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

        // ---- [chainarrive] PC bring-up instrument -- DELETE WHEN the wall test is banked ---------
        // OPT-IN (BRN_IMPULSE_PROBE=1) so a default run and every golden gate stay byte-identical.
        //
        // ⭐⭐ WHY HERE AND NOWHERE ELSE. The brief's observable is "AddWorldSpaceImpulse is REACHED
        // with a non-zero impulse", but a probe AT AddWorldSpaceImpulse cannot answer it: block (5)
        // of DeformableObject::ApplySensorImpulse ALREADY banks there on every contact, chain or no
        // chain, and so do the wheels. This function is the impulse-passing chain's ARRIVAL and is
        // reached from nowhere else -- VehicleRigidBody's two virtuals are only ever dispatched
        // through ImpulsePasser::mapCollidableBodies. A line here IS a chain delivery, by
        // construction, and it prints the impulse BEFORE the handler so the number is the input the
        // wall response is built from.
        {
            static s32 siArriveProbe = -1;
            if ( siArriveProbe < 0 )
            {
                const char* lpcEnv = getenv( "BRN_IMPULSE_PROBE" );
                siArriveProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            // ⚠️ TWO WINDOWS, because one flat cap cannot see both things. The chain delivers ~60
            // arrivals per frame while the car simply RESTS on the ground (+Y floor support), so a
            // flat cap of 3000 was consumed 50 m before the wall. The first window proves the chain
            // came alive at all; the second keeps only arrivals whose impulse is HORIZONTAL-dominant
            // -- the same normal-direction discriminator the [wall] probe uses to tell a wall face
            // from the junkyard floor.
            static u32 suArrivals   = 0;
            static u32 suHorizontal = 0;
            ++suArrivals;
            const f32 lfHorizontal = liImpulse.x * liImpulse.x + liImpulse.z * liImpulse.z;
            const bool lbInteresting = ( suArrivals <= 30u )
                                     || ( lfHorizontal > 1.0f && ++suHorizontal <= 600u );
            if ( siArriveProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && lbInteresting )
            {
                const char* lpcRoute = lpVehicle->IsCrashing()          ? "CRASH"
                                     : lpImpulseParams->mbWorldContact  ? "WALL"
                                                                        : "CARCAR";
                // ⭐ The vehicle POSITION is printed with every arrival so a line can be attributed
                // to a place in the world without correlating two probes across a 275 s log -- the
                // wall face this campaign drives at is a known z, so `route WALL` at that z with a
                // horizontal impulse IS the wall taking momentum, in ONE line.
                const Vector3 lPos = lpVehicle->GetPosition();
                *CgsDev::Log::gpDebugPrint
                    << "[chainarrive] n " << static_cast<s32>(suArrivals)
                    << " route " << lpcRoute
                    << " dir " << static_cast<s32>(lpImpulseParams->meImpulseDirection)
                    << " mag " << lfMagnitude
                    << " impulse " << liImpulse.x << " " << liImpulse.y << " " << liImpulse.z
                    << " pos " << lPos.x << " " << lPos.y << " " << lPos.z
                    << "\n";
            }
        }

        // Route by car state then contact kind.
        if ( lpVehicle->IsCrashing() )                       // *(*(this+4)+1808)
        {
            const Vector3& lrContactPosition = lpImpulseParams->mImpulsePosition;   // params+32 (v2)
            // 0x8260E088 `li r4,1` (BODY_SPACE impulse) and 0x8260E090 `lwz r5,0x50(r11)`
            // (== ImpulseParams::mePositionSpace) are set ONCE, above the three-way branch, so
            // all three Apply*ContactImpulse variants receive the same pair.
            //
            // ⭐⭐ CORRECTED 2026-09-02 (crash-response wave): r6 is NOT a literal. Both console
            // callers load it from the params block:
            //     ApplyLocalImpulse       0x8260E0B4  lbz r6, 0xB8(r11)     ; params +0xB8
            //     RecievePassedOnImpulse  0x8260E018  lbz r6, 0xB8(r31)     ; params +0xB8
            // and +0xB8 is ImpulseParams::mbWorldContact -- the SAME byte the sibling branch below
            // tests (`lbz r10, 0xB8` @0x8260E0D0). The tree passed `false` here, which in
            // ApplyCrashedContactImpulse @0x825D4D50 selects the `beq` arm at 0x825D4DB4: the angular
            // impulse is multiplied by mCollisionAttribs +0x280 lane .y (CarAngularImpulseScale) and
            // neither mi8NumWorldCollisions (+0x1353) nor the SecondsSinceLastWallContact lane
            // (+0x1070.w) is touched. On the console a WORLD contact against a crashing car takes the
            // other arm (0x825D4D90..0x825D4DB0): counter bumped, wall-contact lane zeroed (so
            // mbContactingWall reads true on the next UpdateCrashing), angular impulse applied RAW.
            lpVehicle->ApplyCrashedContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                                  lrContactPosition, lpImpulseParams->mePositionSpace,
                                                  lpImpulseParams->mbWorldContact);

            // ---- [crash-response] PC bring-up instrument -- DELETE WHEN the crash response is 1:1 -
            // OPT-IN (BRN_CRASH_RESPONSE_DIAG=1). Every impulse the deformation system hands a
            // CRASHING car, with the lever arm it will be applied through: the contact point in the
            // car's own frame (so "bumper height" is a number, not a guess), the world impulse, the
            // angular impulse r x J the kernel will bank, and the pitch rate the car already carries.
            // A roof landing is a pitch budget; this prints every deposit into it.
            {
                static s32 siCrashRespProbe = -1;
                if ( siCrashRespProbe < 0 )
                {
                    const char* lpcEnv = getenv( "BRN_CRASH_RESPONSE_DIAG" );
                    siCrashRespProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
                }
                static u32 suCrashArrivals = 0;
                if ( siCrashRespProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && ++suCrashArrivals <= 1500u )
                {
                    const Matrix44Affine& lrT = lpVehicle->GetTransform();
                    const Vector3 lPos = lrT.Pos();
                    // the arm exactly as GetImpulsesFromLocalImpulse forms it for this tag pair
                    Vector3 lArmW;
                    if ( lpImpulseParams->mePositionSpace == rw::physics::WORLD_SPACE )
                    {
                        lArmW = Vector3{ lrContactPosition.x - lPos.x, lrContactPosition.y - lPos.y,
                                         lrContactPosition.z - lPos.z, 0.0f };
                    }
                    else
                    {
                        lArmW = Vector3{
                            lrT.xAxis.x * lrContactPosition.x + lrT.yAxis.x * lrContactPosition.y + lrT.zAxis.x * lrContactPosition.z,
                            lrT.xAxis.y * lrContactPosition.x + lrT.yAxis.y * lrContactPosition.y + lrT.zAxis.y * lrContactPosition.z,
                            lrT.xAxis.z * lrContactPosition.x + lrT.yAxis.z * lrContactPosition.y + lrT.zAxis.z * lrContactPosition.z,
                            0.0f };
                    }
                    const Vector3 lJW{
                        lrT.xAxis.x * liImpulse.x + lrT.yAxis.x * liImpulse.y + lrT.zAxis.x * liImpulse.z,
                        lrT.xAxis.y * liImpulse.x + lrT.yAxis.y * liImpulse.y + lrT.zAxis.y * liImpulse.z,
                        lrT.xAxis.z * liImpulse.x + lrT.yAxis.z * liImpulse.y + lrT.zAxis.z * liImpulse.z,
                        0.0f };
                    const Vector3 lRxJ{ lArmW.y * lJW.z - lArmW.z * lJW.y,
                                        lArmW.z * lJW.x - lArmW.x * lJW.z,
                                        lArmW.x * lJW.y - lArmW.y * lJW.x, 0.0f };
                    const Vector3 lW = lpVehicle->GetAngularVelocity();
                    const f32 lfPitchRate = lW.x * lrT.xAxis.x + lW.y * lrT.xAxis.y + lW.z * lrT.xAxis.z;
                    const f32 lfPitchDep  = lRxJ.x * lrT.xAxis.x + lRxJ.y * lrT.xAxis.y + lRxJ.z * lrT.xAxis.z;
                    *CgsDev::Log::gpDebugPrint
                        << "[crash-response] arrive n=" << static_cast<s32>(suCrashArrivals)
                        << " world=" << ( lpImpulseParams->mbWorldContact ? 1 : 0 )
                        << " dir=" << static_cast<s32>(lpImpulseParams->meImpulseDirection)
                        << " mag=" << lfMagnitude
                        << " Jbody=(" << liImpulse.x << "," << liImpulse.y << "," << liImpulse.z << ")"
                        << " posSpace=" << static_cast<s32>(lpImpulseParams->mePositionSpace)
                        << " armBody=(" << ( lArmW.x * lrT.xAxis.x + lArmW.y * lrT.xAxis.y + lArmW.z * lrT.xAxis.z )
                        << "," << ( lArmW.x * lrT.yAxis.x + lArmW.y * lrT.yAxis.y + lArmW.z * lrT.yAxis.z )
                        << "," << ( lArmW.x * lrT.zAxis.x + lArmW.y * lrT.zAxis.y + lArmW.z * lrT.zAxis.z ) << ")"
                        << " rxJ=(" << lRxJ.x << "," << lRxJ.y << "," << lRxJ.z << ")"
                        << " pitchDeposit=" << lfPitchDep
                        << " pitchRate=" << lfPitchRate
                        << " up.y=" << lrT.yAxis.y << " fwd.y=" << lrT.zAxis.y
                        << " closing=" << lpImpulseParams->mvfVelocityAlongNormal.x
                        << "\n";
                }
            }
        }
        else if ( lpImpulseParams->mbWorldContact )          // *(params+184)
        {
            lpVehicle->ApplyWallContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                               lpImpulseParams->mWorldImpulseDirection,
                                               lpImpulseParams->mImpulsePosition,
                                               lpImpulseParams->mePositionSpace);
        }
        else
        {
            lpVehicle->ApplyCarContactImpulse(liImpulse, rw::physics::BODY_SPACE,
                                              lpImpulseParams->mWorldImpulseDirection,
                                              lpImpulseParams->mImpulsePosition,
                                              lpImpulseParams->mePositionSpace);
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
