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
//
// ⭐⭐⭐ 2026-09-05 (momentum wave): THAT LAST SENTENCE IS THE WHOLE ANSWER TO THE
// "45 kN.s WHILE ABSORPTION IS ZERO" CONTRADICTION, so it is spelled out here where the next
// wave will read it, with the instruction that proves it.
//   fdfda858 measured, inside ONE crash, arrivals split by absorption state:
//       set 4 INVINCIBLE   129 arrivals  sum|J| 134,063  max|J| 45,248
//       set 0 NORMAL      1371 arrivals  sum|J| 125,507  max|J|  1,202
//   and called the 45 kN.s arrival a CONTRADICTION -- "DeformationSensor::ApplyLocalImpulse passes
//   absorbed * 0.5 and absorbed == 0 in that window, so this route predicts nothing; there must be
//   an untraced caller". THERE IS NO UNTRACED CALLER. The premise is the error: it assumes the
//   arriving magnitude IS the passed-on argument. It is not, and the console says so in one
//   instruction --
//       0x8260E008   vmulfp128  v1, v13, v0
//   v1 is the incoming VecFloat argument (the `absorbed * 0.5f` the sensor computed at
//   0x825E173C) and it is OVERWRITTEN here, before any read, by the product of the direction row
//   with `lvx128 v0, r31, 0x10` == lpImpulseParams->mvfImpulseMagnitude. The identical pair sits
//   at 0x8260E098/0x8260E0A4 in ApplyLocalImpulse. So BOTH bodies take their magnitude from the
//   PARAMS BLOCK and the passed-on argument is DEAD in this callee on ARTIST as well as here.
//   The absorbed fraction reaches the rigid body only INDIRECTLY, through the sensor's write-back
//   `mvfImpulseMagnitude -= lfAbsorbed` (`vsubfp v0,v13,v0 ; stvx128 v0,r0,r11`
//   @0x825E175C/0x825E1760). With the absorption row identically 0.0, lfAbsorbed == 0, the
//   write-back is a no-op, and the FULL shaped impulse arrives at the body.
//   ⇒ The two rows above are ONE mechanism, not two routes: INVINCIBLE means nothing is subtracted
//   on the way, so the body is handed everything; NORMAL means every sensor in the chain takes its
//   bite first, so the body is handed the remainder. Absorption is the crumple zone, and the crumple
//   zone is what LETS A CAR KEEP MOVING.
// ⭐ AND THE SEARCH WAS DONE ANYWAY, statically, so "no untraced caller" is a measurement and not a
// preference: CollidableBody's vtable has exactly TWO slots (the image carries the two concrete
// tables back to back -- VehicleRigidBody at 0x82095220 = {0x8260E068, 0x8260DFA0} and
// DeformationSensor at 0x82095228 = {0x825E1320, 0x825E11F8}); slot 1 is reached only by
// ImpulsePasser::PassOnImpulse's `lwz r11,0(r3) ; lwz r11,4(r11) ; bctrl` @0x825BA488; slot 0 is
// dispatched exactly once in the deformation subsystem, at DeformableObject::ApplySensorImpulse
// +0x698..+0x6AC (`lwz r3, arg_74(r1) ; lwz r11,0(r3) ; lwz r11,0(r11) ; mtctr ; bctrl`
// @0x82607F48..0x82607F5C) -- a whole-image scan for that FOUR-instruction form (not
// vcallsites.py's two-instruction one, which matches every "load a word at +0 and call it" and
// returned 343 sites) leaves exactly six inside BrnPhysics, five of them on unrelated vtables
// (VehicleOutputInterface x2, VehicleManager::WriteOutVehicleStats, ShiftControl x2).
// The dispatch is on the BODY ARGUMENT -- and BOTH of ApplySensorImpulse's only two callers (its `xrefs_to`:
// ApplyCarWorldImpulse @0x82624898 and ApplyCarCarImpulse @0x82624C08) pass
// GetDeformationSensor(liSensorIndex), never &mVehicleBody. Neither 0x8260DFA0 nor 0x8260E068 is
// the target of a single direct `bl` anywhere in the image. So VehicleRigidBody::ApplyLocalImpulse
// is UNREACHED on this path and every arrival is a PassOnImpulse arrival -- which the [crash-response]
// line now prints per arrival (`entry local|passed`) rather than leaving to be argued.
// ⭐ THE EXACT RUNTIME FIGURE, recorded here because the commit message that landed this quoted a
// deliberately conservative floor: across the 14 crashes measured on this build (runs mwp_*, mwA_*,
// mwB_*, mwK_*) the arrival line printed 35,466 impulses, 35,466 of them `entry=passed` and
// **0** `entry=local`. If a future run ever prints one, it is a discovery.

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
    //
    // ⚠️ THE TWO TRAILING PARAMETERS ARE INSTRUMENT-ONLY and are read by nothing but the opt-in
    // probes below. They are honest to add precisely BECAUSE this helper does not exist on the
    // console: the X360 inlines the kernel into both virtuals, so the helper is already a PC-side
    // factoring and its argument list is ours, not ARTIST's. `lpcEntry` names which virtual we came
    // through and `lvfPassedMagnitude` carries the argument the console DISCARDS (see the banner) so
    // one log line can show the discarded value beside the one actually used.
    static void ApplyImpulseToVehicle(BrnPhysics::Vehicle::VehiclePhysics* lpVehicle,
                                      const ImpulseParams* lpImpulseParams,
                                      const char* lpcEntry, VecFloat lvfPassedMagnitude)
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
                // ⚠️⚠️ THE CAP WAS 1500 AND IT WAS SPENT INSIDE SHOT 0. fdfda858's two-shot run
                // recorded 129 + 1371 == 1500 arrivals in its FIRST crash, so the run carried no
                // arrival data at all for shot 1 and the pristine-vs-dented arrival comparison it
                // was taken to make could not be made. A zero in the second shot would have been
                // the PROBE, not the physics -- the campaign's own "diagnostics that lie" class.
                // 24000 is sized against the volume actually seen: ~18 arrivals per crash frame and
                // ~700 crash frames in the longest measured shot is ~12.6k, so a two-shot boot fits
                // with headroom. The counter is still capped, and the cap is still stated in the
                // log's own line numbers (`n=`), so a truncation remains visible rather than silent.
                static u32 suCrashArrivals = 0;
                if ( siCrashRespProbe == 1 && CgsDev::Log::gpDebugPrint != 0 && ++suCrashArrivals <= 24000u )
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
                    // ⭐ yaw and ROLL deposits added 2026-09-05 (momentum wave) -- the owner's
                    // "the car never really barrel rolls" is a question about the ROLL budget, and
                    // the line printed only the pitch one, so answering it needed offline maths on
                    // Jbody/armBody. THE MECHANISM, said here because it is not obvious: every
                    // impulse that reaches this function is Jbody == KA_IMPULSE_DIRECTIONS[dir] *
                    // magnitude, i.e. a PURE signed BODY AXIS (ApplySensorImpulse decomposes the
                    // world impulse onto the six axes, asm switch @0x82607BAC). A pure -Z impulse
                    // -- the head-on crush direction -- has (r x J).z identically zero, so it can
                    // deposit pitch and yaw and NEVER roll. Roll comes only from the +/-X and +/-Y
                    // direction magnitudes, through arm.x*J.y - arm.y*J.x.
                    // MEASURED, run mom_C1 (148 mph head-on wall wreck, 885 arrivals):
                    //     dir -Z  249 arrivals  sum|mag| 94134  max 14525
                    //     dir +/-X 295 arrivals sum|mag| 44200  max  2811
                    //     dir +/-Y 295 arrivals sum|mag| 28935  max  1062
                    //     body-frame roll moment  sum 6223  max 650   (vs a 9035 yaw deposit
                    //     from a SINGLE -Z arrival)
                    // and the integrated result over the whole crash was
                    //     sum dWbody = pitch -1.98, yaw +16.57, ROLL -0.47 rad/s.
                    // ⚠️ THAT IS NOT YET A DEFECT: a symmetric head-on wall hit cannot roll a car
                    // on the console either. What it gives is the BUDGET to compare -- the next
                    // barrel-roll experiment must be an OBLIQUE/asymmetric hit.
                    // ✅✅ AND THE OBLIQUE EXPERIMENT WAS RUN THE SAME DAY (mom_D2: same recipe plus
                    // -SteerScript "0:none,21.5:right", so the car meets the same wall side-on).
                    // The direction budget inverts exactly as the mechanism predicts, and the roll
                    // channel is ALIVE:
                    //     dir +X   478 arrivals  sum|mag| 81328   (vs 83 arrivals / 23854 head-on)
                    //     dir -Z   473 arrivals  sum|mag| 35703   (vs 249 / 94134 head-on)
                    //     deposits  pitch -12300   yaw +52589   ROLL -21410   maxRoll 1238
                    //     Wbody roll rate reached -4.24 .. +5.50 rad/s
                    // 5.50 rad/s is 85 % of UpdateCrashing's own +/-6.5 clamp, so neither the clamp
                    // nor a dead roll channel is the limiter.
                    // ⭐⭐ WHAT IS: the car is CAUGHT. Over that whole crash up.y never fell below
                    // 0.8049 (right.y -0.5953, i.e. ~36 degrees of roll) and the verdict was
                    // DRIVE_AWAY with upDot 0.9967. A 5.5 rad/s roll covers 36 degrees in 0.11 s and
                    // should keep going; something reverses it. THE NAMED SUSPECT, for whoever picks
                    // this up: VehiclePhysics::StabiliseAfterHardLanding @0x825D1890, which a
                    // crashing car still reaches through UpdateCrashing -> UpdateSuspension, reads
                    // mvMaxYawDampingOnLanding_MaxRollDampingOnLanding_... (+0x250) and damps ROLL
                    // for TimeToDampAfterLanding seconds whenever at least one front and one rear
                    // wheel are on the road. Measure mfTimeSinceHardLanding, its wheels-on-ground
                    // gate and the damping factor across a crash before touching anything.
                    const f32 lfYawDep    = lRxJ.x * lrT.yAxis.x + lRxJ.y * lrT.yAxis.y + lRxJ.z * lrT.yAxis.z;
                    const f32 lfRollDep   = lRxJ.x * lrT.zAxis.x + lRxJ.y * lrT.zAxis.y + lRxJ.z * lrT.zAxis.z;
                    const f32 lfRollRate  = lW.x * lrT.zAxis.x + lW.y * lrT.zAxis.y + lW.z * lrT.zAxis.z;
                    *CgsDev::Log::gpDebugPrint
                        << "[crash-response] arrive n=" << static_cast<s32>(suCrashArrivals)
                        // ⭐ `entry` names the virtual we came through and `passed` is the argument
                        // the console throws away at 0x8260E008. A line whose `passed` is 0.0 while
                        // `mag` is tens of thousands IS the invincible-window mechanism, printed.
                        << " entry=" << lpcEntry
                        << " passed=" << lvfPassedMagnitude.x
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
                        << " yawDeposit=" << lfYawDep
                        << " rollDeposit=" << lfRollDep
                        << " pitchRate=" << lfPitchRate
                        << " rollRate=" << lfRollRate
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
        // ⚠️ vtable slot 0. The static search in the banner finds NO dispatcher that reaches this
        // slot on a VehicleRigidBody (ApplySensorImpulse's only slot-0 dispatch is on a sensor), so
        // an "entry=local" line in a log is a discovery, not noise -- read it, do not dismiss it.
        // The zero VecFloat is only what the probe prints for "no passed-on argument exists here".
        const VecFloat lvfNoPassedMagnitude = { 0.0f, 0.0f, 0.0f, 0.0f };
        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams, "local", lvfNoPassedMagnitude);
    }

    // @0x8260DFA0  BrnVehicleRigidBody.cpp:131
    void VehicleRigidBody::RecievePassedOnImpulse(const ImpulseParams* lpImpulseParams,
                                                  VecFloat lvfPassedMagnitude)
    {
        // Gate: virtual dispatch through the attached vehicle's vptr slot +0x10 -- image-settled
        // as IsPlayerVehicleInShowtime (see the banner). A non-zero return means the player's
        // showtime car swallows further passed-on impulses -> do not apply.
        if ( mpAttachedVehicle->IsPlayerVehicleInShowtime() )
            return;

        // ⚠️ lvfPassedMagnitude is HANDED ON TO THE PROBE ONLY -- the console overwrites its
        // register (v1) at 0x8260E008 before reading it, and so does this body by taking the
        // magnitude from lpImpulseParams->mvfImpulseMagnitude inside the kernel. It is forwarded
        // here so a log line can show the DISCARDED value beside the used one; nothing computes
        // with it. (Named rather than commented out for exactly that reason.)
        ApplyImpulseToVehicle(mpAttachedVehicle, lpImpulseParams, "passed", lvfPassedMagnitude);
    }
}
}
