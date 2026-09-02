// ============================================================================
// GameSource/Physics/VehicleManager/BrnVehicleManager_ValidateRaceCarWorldContact.cpp
//
// BrnPhysics::Vehicle::VehicleManager::ValidateRaceCarWorldContact @0x825C6088 (988 insns)
// -- THE VALIDATION WHALE (walls leg 3, 2026-08-14). PS3 DecFIGS 0x70AB20 (1523; the mangle
// is the signature authority, DWARF h:938). Slice TU: the home BrnVehicleManager.cpp (the
// asserts' own file, :7419..:7624) is still unmounted -- RaceCarPhysics_Construct precedent.
//
// Validate ONE race-car-vs-world potential contact (side A == the car, side B == the world
// triangle -- DoRaceCarWorldContactValidation swapped it before calling). The function may
// REWRITE the contact in place (the wall-normal flatten; the wheel/bottom-plane projection)
// and returns whether it survives into the Validated queue.
//
// EVERY CONSTANT BELOW IS MEASURED, none inferred (walls leg 3, x360rd out of the unpacked
// .i64 image; addresses cited per value). The two file-scope values are namespace globals on
// the console; the function-locals are its guarded function-statics (guard dword_82FBA030).
//
// METHOD NOTE: the two VMX-dense blocks (the bottom-plane rewrite and the final AABB
// acceptance) were derived from the RAW INSTRUCTION WORDS with the leg-2-validated field
// decoders (sim_kernel field layouts), NOT from the IDA operand text -- the +32 per-operand-
// field hazard and one vperm operand-order trap live exactly there. Where the raw words
// disagree with a naive reading, the words won (see the CONSOLE QUIRK note in the body).
// ============================================================================

#include "GameSource/Physics/VehicleManager/BrnVehicleManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // gpDebugPrint / gxMessageFilterFlags (debug-draw gates)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"         // TriangleCacheInterface (GetCache / GetNumCachedTriangleBatches)
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"            // PotentialContact
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"                    // Triangle4 (+ AOSTriangle)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/RaceCarPhysics.h"             // RaceCarPhysics (GetHeightAboveRoad + the friend-granted member reads)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/B5PhysicsHandlingDebugComponent.h" // Vehicle::DebugComponent (SetLastWallTriangle)
#include "GameSource/Physics/VehicleManager/BrnVehicleManagerDebugComponent.h"           // VehicleManagerDebugComponent (mbRenderWallContacts / mbRenderGroundContacts)

#include "rw/math/vpu/vector3_operation.h"                                               // vpu::{Dot, Subtract, Normalize}

#include <cstdlib>                                                                       // getenv -- the [kerb] probe's opt-in latch

namespace BrnPhysics
{
namespace Vehicle
{
    // MEASURED namespace globals (console .data):
    //   gfGroundContactCullHeight: STATICALLY-INITIALISED .data @0x82F2A148 == 0x3ECCCCCD ==
    //     0.4f (image-read; same ordinary-initialised .data bank as the flt_82F2A2xx seed
    //     constants -- nothing to disassemble).
    //   KVF_WALL_NORMAL_DOT_THRESHOLD: DYNAMIC-INIT (zero in the image at 0x82FB7F20). Its
    //     initialiser was FOUND AND READ this wave @0x82C5BBD8..0x82C5BBF8:
    //       lis r11,0x8200 ; lfs f0,0x1DA0(r11)   <- flt_82001DA0 == 0.5f
    //       lis r11,0x82FB ; ... vspltw ; stvx128 -> 0x82FB7F20
    //     == splat(0.5f). The PS3 cross-witness: _Z41__static_initialization_and_destruction
    //     @0x6C2CCC splat-initialises the same-named global. Contacts whose |normal.y| is
    //     below this are flattened to pure horizontal ("wall") normals.
    namespace
    {
        const f32 KF_GROUND_CONTACT_CULL_HEIGHT    = 0.4f;   // gfGroundContactCullHeight @0x82F2A148
        const f32 KF_WALL_NORMAL_DOT_THRESHOLD     = 0.5f;   // KVF_WALL_NORMAL_DOT_THRESHOLD (init @0x82C5BBD8, value == flt_82001DA0)
        const f32 KF_MPS_TO_MPH                    = 2.236936330795288f; // KF_MPS_TO_MPH rodata @0x8208F820 (named X360 symbol; PS3 identical)
        const f32 KF_NORMAL_ZERO_EPSILON           = 1.1920928955078125e-07f; // stru_8208F620 lane 0 == 0x34000000 == FLT_EPSILON
        const f32 KF_UP_DOT_GROUND_NORMAL_MIN      = 0.8f;   // flt_8208F9C8 == 0x3F4CCCCD (the above-ground-plane gate)
        const f32 KF_AABB_MARGIN_XY                = 0.1f;   // flt_82004014 == 0x3DCCCCCD (x/y lanes of the margin vector; the z lane is flt_82001CC0 == 0.0f)
        const f32 KF_LATERAL_RECHECK_MARGIN        = 0.2f;   // flt_82004744 == 0x3E4CCCCD (the velocity-expanded lateral band)
        const f32 KF_FINAL_ACCEPT_MAX_SPEED_MPH    = 25.0f;  // flt_82004FD8 (f29, shared with the clearance gate)

        // Per-lane finiteness of a normal's xyz (the console's vspltw + vcmpeqfp self-compare
        // triple, used four times in this body).
        inline bool IsValidVec3Lanes(const Vector3& lrV)
        {
            return lrV.x == lrV.x && lrV.y == lrV.y && lrV.z == lrV.z;
        }
    }

    // ---- [kerb] PC bring-up instrument -- DELETE-WHEN the kerb response is proven 1:1 -------------
    // OPT-IN (BRN_KERB_PROBE=1, flow_run.ps1 -DiagEnv BRN_KERB_PROBE=1). NOT console code. Off by
    // default: the latch reads 0 once and every print is unreachable thereafter, so a default run
    // and every golden gate stay byte-identical to a build without it.
    //
    // WHY. The owner's kerb symptom ("the car reacts way too much to the curb") is decided in THIS
    // function: every race-car-vs-world contact passes through it, and the two ground culls are
    // what turn a kerb face into a wheel-plane push instead of a wall hit. The [drift]/[cvalid]
    // probes sample 1 frame in 60 and print only counts -- a kerb crossing is ~10 frames long, so
    // they cannot see it. This prints BOTH SIDES of every gate the function takes, per contact,
    // every frame: the incoming normal, the triangle's vertex heights vs kvfMaxCurbHeight, the
    // curb/wall verdict, both cull distances vs the scaled cull height, the above-ground normal and
    // its up-dot vs 0.8, the rewritten normal/point, and the verdict. Three tags share one frame
    // counter (bumped by DoRaceCarWorldContactValidation, the per-frame caller in the sibling TU):
    //   [kerb]      one line per contact, this function          (this TU)
    //   [kerb-car]  one line per moving car per frame             (BrnVehicleManagerContactGeneration.cpp)
    //   [kerb-imp]  one line per world impulse the solver applies (BrnDeformableObject.cpp)
    // Each tag is budget-limited and SAYS SO when the budget runs out -- a silent stop reads
    // exactly like "the kerb never touched anything".
    //
    // MEASURED 2026-09-02 (kerb waves r1/r2, Waterfront pavement kerb x~3391.5 z -1620..-1660, a
    // 0.15 m step; runs scratch/flow_run/kerbw_r1, kerbw_r2, BRN_KERB_PROBE=1, every frame):
    //   * The kerb FACE triangle (triN (-0.999, 0, 0.039), vertex y 0.27/0.285/0.12) classifies
    //     CURB on every contact -- h = -0.000 / -0.000 / -0.150 against kvfMaxCurbHeight 0.25.
    //     The harvest normal arrives TILTED, not the face normal: (-0.48, 0.87, 0.02) and
    //     (-0.71, 0.71, 0.03) -- the sensor sphere rides the kerb's top edge.
    //   * BOTH culls fire on every kerb-face contact: (a) dA 0.06..0.08 < wpH 0.0 + cullH 0.4
    //     (mbMinWheelDistValid 1), (b) upDotAg 0.9992 > 0.8 and dB 0.000002..0.000004 < 0.4.
    //     The contact is re-pointed to the body Up axis and the solver then applies NO impulse
    //     ([kerb-imp] is empty through the whole crossing) -- the kerb is NOT treated as a wall.
    //   * Cull (a) alone already catches every one of them, and its inputs (CalculateNewWheelPlane,
    //     e312c005 2026-08-07; this whale 43bb9c20 2026-08-14) predate the above-ground-ray fix
    //     (c6cb403f). So "the missing ray disabled the second cull and kerbs became walls" is
    //     REFUTED by chronology: pre-fix, cull (a) was re-pointing them just the same.
    //   * The car's response through a 25-degree, 30-38 mph forward crossing (r2 f1013-1040): body
    //     +0.10 m per axle step, roll rate peak +0.48 / -0.58 rad/s, pitch |0.10| rad/s, velocity
    //     heading 25.1 -> 25.8 deg, NO speed loss (accelerating 30.2 -> 39.7 mph throughout).
    //   * The one sharp event on film is NOT in this function: reversing over the same kerb at
    //     106 mph (r1 f2578-2588) the car lost 6.2 mph in ONE frame and yawed at 0.47 rad/s at
    //     the exact frame the FRONT wheel mounted (traction hit y 0.125 -> 0.276), with zero body
    //     impulses -- that is the wheel/suspension path (ApplyWheelWeight -> UpdateSuspensionSprings
    //     -> tyre forces), not the world-contact validation. Open lead, un-audited.
    //   * CONTROLS (kerbw_r4 / r4B / r4AB, one deterministic recipe, an uncommitted switch that
    //     discards a cull's verdict AFTER it has printed): with cull (b) discarded the crossing is
    //     frame-identical to the shipped build (52/52 re-pointed, 0 impulses, same mounts, same
    //     roll peak 0.35 rad/s) -- i.e. the pre-ray-fix state was already correct here. With BOTH
    //     discarded the kerb face reaches the solver as a wall: 51 impulses of 950..1121 along
    //     (-1.00, 0.00, 0.04), the wheels never mount (y 0.444 -> 0.445), the car is steered along
    //     the kerb 23 -> 16 deg. That is the failure the culls prevent, and NOT the owner's symptom.
    //
    // ==============================================================================================
    // MEASURED 2026-09-02, WAVE 2 (runs kerbX_r1..r4). Two INDEPENDENT kerb hypotheses tested; both
    // REFUTED. Nothing here changed a constant or a gate -- the whole wave is measurement.
    //
    // (A) "A WHEEL IS A CUBE AND A CUBE CANNOT CLIMB A STEP, SO THE GAME MUST HAVE A HOP THRESHOLD
    //     THE DECOMP IS MISSING." The premise is wrong on this build. An ATTACHED wheel's
    //     world-collision proxy is a SPHERE, written every frame by DeformableObject::
    //     UpdatePostPhysics @0x825DFEB0 into maWorldSensorSpheres[nSens + wheel] (radius =
    //     WheelSpec::mScale.x / 2; see the [kerb-wsph] banner in BrnDeformableObject_Update.cpp),
    //     and GetSpheresForCar hands the world test all nSens + 4 of them. MEASURED LIVE: nSens 20,
    //     wheel spheres at indices 20..23, radius 0.128 / 0.130 / 0.150 m, while the tyre's actual
    //     rolling radius is 0.331 / 0.342 m ([wsus-attr] "rad"). So the wheel primitive is a small
    //     hub-level sphere whose lowest point rides ~0.25 m ABOVE the road: it is not what meets the
    //     kerb at all. In four runs the wheel spheres produced 319 world contacts, none of them on a
    //     kerb face -- the kerb is met by the BODY-SHELL deformation sensors (indices 4..13).
    //
    // (B) "THE THRESHOLD IS THE GROUND CONTACT CULL HEIGHT AND IT IS 0.4, NOT 0.25." Both numbers
    //     are real, they are DIFFERENT constants, and this TU already has both right:
    //       * "Ground Contact Cull Height" is flt_82F2A148. The console NAMES IT ITSELF --
    //         VehicleManagerDebugComponent::OnActivate @0x825B5D90 calls
    //           CgsDev::DebugComponent::RegisterVariable(this, &flt_82F2A148, "Crashes",
    //                                                    "Ground Contact Cull Height");
    //           SetRange(&flt_82F2A148, 0.0, 1.0);  SetStep(&flt_82F2A148, 0.01);
    //         which is exactly the debug slider a retail-build experimenter would move. Image byte
    //         value (x360rd) 0x3ECCCCCD == 0.4 -> KF_GROUND_CONTACT_CULL_HEIGHT below. Its two live
    //         consumers are this function and HandleTrafficCarWorldPotentialContact @0x8263F0F0
    //         (BrnVehicleManager_TrafficCrashArms.cpp:77, also 0.4). Nothing writes it at runtime.
    //       * kvfMaxCurbHeight is a SEPARATE literal. The load is unambiguous in the asm --
    //         0x825C6684  lfs f0, (flt_8208F834 - 0x8208F9C8)(r14)  -- and x360rd reads
    //         0x8208F834 == 0x3E800000 == 0.25. (The other seven functions that reference that word
    //         are unrelated 0.25s: GetAftertouchValues, Engine::Update, UpdateFreezing,
    //         ApplyEngineForces, SetupAttribsForAI, VehicleManager::Construct.) So "we picked the
    //         wrong pool entry" is refuted from the image, not argued.
    //     The two are not interchangeable: 0.25 is compared against TRIANGLE VERTEX HEIGHTS ABOVE
    //     THE ROAD (curb vs wall), 0.4 against a DISTANCE from the wheel/above-ground plane (cull or
    //     not). Lowering the 0.4 makes cull (a) miss and kerb faces reach the solver -- which is why
    //     moving that slider makes a retail car catch on everything, and is evidence FOR the
    //     mechanism, not against our value.
    //
    // (C) THE KERB IS HOPPED, MEASURED TWICE, AT BOTH ENDS OF THE SPEED RAMP (Waterfront kerb,
    //     x ~3391.5, face triN (-1.000, 0.000, 0.023), top y 0.24, road y 0.075..0.081 -> a
    //     0.158..0.165 m step):
    //       kerbX_r2  60 mph, ~10 deg oblique: ~200 contacts f1140-1175, every one curb 1 / wall 0,
    //                 cull (a) fires (dA 0.13..0.16 < 0.4), re-pointed, ZERO impulses, speed
    //                 58.2 -> 64.3 mph (still ACCELERATING), yaw 0.01 rad/s, roll +-0.5 rad/s.
    //       kerbX_r4  24 mph, HEAD-ON (perpendicular): kerb-face contacts f1007-1022 from sensors
    //                 4/6/8/10..13, every one curb 1 / wall 0, dA 0.038..0.095 < cullH, re-pointed,
    //                 ZERO impulses; front wheels mount at f1010 (road contact y 0.081 -> 0.239);
    //                 speed 22.4 -> 28.9 mph, again NO loss. At f1034 the car meets the building
    //                 beyond the pavement: wall 1, 3 impulses, 28.6 -> 10.6 mph. Correct, and the
    //                 contrast is the control -- this function CAN stop a car, and does, for a wall.
    //     ⭐ The speed ramp is corroborated to three decimals in the same run: cullH 0.372 at
    //     23.95 mph, 0.381 at 24.27, 0.389 at 24.59, 0.400 at >= 25 -- i.e. 0.4*(v-10)/15 exactly.
    //
    // (D) mi8NumWorldCollisions "3..15 on ~7% of flat-tarmac frames" is EXPLAINED, not a bug:
    //     [wsus] nwc over run r4 is 0 on 59322 steps and 6 on 8019 (11.8% non-zero), and the
    //     matching [kerb] lines show them to be body-shell sensor spheres touching the ROAD
    //     (triN ~ (0, 1, 0)), classified curb, culled to the wheel plane. That is what
    //     gfGroundContactCullHeight exists for; the console produces them too.
    //     ⚠️ CORRECTED 2026-09-03: this used to end "culled to the wheel plane, NO IMPULSE", and
    //     that half cannot be true. mi8NumWorldCollisions (+0x1353) has exactly three writers in
    //     the whole tree -- ApplyWallContactImpulse @0x825FEA18 (unconditional),
    //     ApplyCrashedContactImpulse @0x825D4D50 and ApplyShowtimeContactImpulse @0x825D4E00
    //     (both on their zero-response arm) -- and ALL THREE bank an impulse through
    //     AddWorldSpaceImpulse before returning; the counter is zeroed at the top of
    //     DeformableObject::UpdateContacts @0x826478B0's apply phase. So `nwc == 6` MEANS six
    //     world impulses were applied that step. They are the culled contacts re-pointed to the
    //     car's up axis, i.e. the ground pushing the car UP -- benign, and exactly what the cull
    //     is for -- but they are impulses, and [kerb-imp] prints one line for each of them.
    //     ⭐ AND THE CORRECTION HAS A CONSEQUENCE, which is why it is worth the paragraph:
    //     UpdateSuspensionPostSimulation @0x825F6BB0 gates its inanimate-world RECOVERY IMPULSE
    //     arm on `mi8NumWorldCollisions == 0`. So the two arms ALTERNATE on flat tarmac: on the
    //     11.8% of steps the world-contact arm fires, the recovery arm is suppressed; on the
    //     other 88.2% the recovery arm is the only thing applying impulses at all.
    //
    // (E) A KERB TALLER THAN 0.25 m -- TESTED 2026-09-02, AND `wall` IS THE CONSOLE'S OWN ANSWER.
    //     The previous wave left this as the leading remaining candidate for "the curbs are still
    //     weird". It is now measured, and separately settled from the asm.
    //     THE EDGE (Waterfront, found by driving, not by hunting): x ~3386.8, z ~-1724.5, face
    //     triN (-0.951, 0.000, 0.310) -- a vertical pavement edge with a fence behind it. Triangle
    //     vertex world y: road side -0.240, an intermediate ledge -0.015, pavement top +0.225, i.e.
    //     a TWO-STEP rise of 0.465 m from the road; the fence's base triangle shares it at +2.535.
    //     BOTH SIDES OF THE COMPARE, run kerbT_30, f1056, 44.7 mph, head-on (approach heading 108
    //     deg == along -triN), sensors 5/7/9:
    //       h 0.456 / 0.690 / 0.217  vs maxCurb 0.250  -> curb 0  wall 1
    //       h 0.450 / 0.217 / 0.690  vs maxCurb 0.250  -> curb 0  wall 1
    //       h 0.217 / 0.450 / -0.008 vs maxCurb 0.250  -> curb 0  wall 1   (ONE vertex over)
    //       h 0.690 / 0.456 / 2.999  vs maxCurb 0.250  -> curb 0  wall 1   (the fence)
    //     cullA 0 / cullB 0 on every one -- and dA was 0.128 / 0.141 / 0.224 / 0.250 / 0.277 on five
    //     of them, i.e. BELOW cullH 0.400, so cull (a) WOULD have fired: it is the wall flag that
    //     suppresses it. repointed 0, accept 1.
    //     THE EFFECT: 44.7 -> 37.7 -> 6.8 mph in about half a second, the car does NOT climb it, and
    //     it sits against the face at 2.4 mph on full throttle for the rest of the run. Set that
    //     against (C): the 0.16 m kerb is hopped at 24 AND 60 mph with ZERO impulses and no loss.
    //     THE SAME EDGE OBLIQUELY, run st_scoutB f1108, 33.5 mph, ~18 deg closing angle: identical
    //     h/curb/wall/cull values, accepted unmodified -- and the car did NOT lose speed (33.5 ->
    //     36.0, still accelerating). So the wall CLASSIFICATION alone does not stop a car; a
    //     near-normal closing angle into a wall-classified edge does.
    //
    //     ⭐ IS `wall` THE CONSOLE'S OWN VERDICT, OR IS OUR TRANSCRIPTION MISSING AN ARM? It is the
    //     console's, settled from the image rather than from feel, three ways:
    //       1. r27 (curb, 0x825C6750) and r28 (wall, 0x825C6794) are consumed at EXACTLY four sites
    //          in the whole 988-instruction function:
    //            0x825C68B4  r27 -> the mbRenderGroundContacts debug draw
    //            0x825C69A0  r29 (== r28) -> SetLastWallTriangle into the debug component
    //            0x825C6A9C  r29 -> suppress cull (a)
    //            0x825C6B60  r29 -> suppress cull (b)
    //          At 0x825C6B70 r29 is reloaded with the constant 0x30 and is dead as a boolean
    //          thereafter. There is NO step-up arm, no ride-over, no swept test and no speed term
    //          keyed on the classification.
    //       2. The sole caller, DoRaceCarWorldContactValidation @0x825EB6C8, adds none either: swap
    //          entity order, call this, append the survivors to queue [6].
    //       3. THE CONSOLE NAMES ONLY TWO CLASSES. VehicleManagerDebugComponent::OnActivate
    //          @0x825B5D90 registers 92 debug variables; its world-contact ones are exactly
    //          "Ground Contact Cull Height", "Draw wall contacts", "Draw ground contacts",
    //          "Head-on world crash factor", "Side-on world crash factor", "Modify traffic
    //          contacts", "Draw last RC-traffic contact", "Draw last crash contact", "Draw World
    //          Contact Spies", "Num traction tests" and the "Reset Rays..." trio. Not one of those
    //          92 -- nor of Vehicle::DebugComponent's 28 (0x82620730), DeformationDebugComponent's
    //          58 (0x82623198), CollisionDebugComponent's 4 (0x822A8B80) or
    //          TriangleCollisionDebugComponent's 17 (0x828AAA68) -- is kerb-, step- or ride-related.
    //          The retail tuning surface has TWO world-contact classes and this function produces
    //          exactly those two. (Same trick that named the 0.4: the registrations name things.)
    //     ⇒ The owner's "the curbs are still weird" is NOT an unimplemented hop threshold above
    //       0.25 m. A raised edge more than 0.25 m above the car's wheel plane is a wall on the
    //       console too, and it stops the car on the console too.
    //     ⚠️ WHAT THIS EVIDENCE CANNOT DISTINGUISH. The stopped car's contact set at f1056 holds
    //       BOTH the kerb triangles (vy -0.240 .. +0.225) and the base triangle of the fence behind
    //       the pavement (vy 2.535). The kerb triangles alone classify as wall, so the
    //       classification result stands either way -- but this run cannot apportion the 44.7 ->
    //       6.8 mph between the kerb face and the fence. A head-on crossing into a >0.25 m edge
    //       with NOTHING behind it is still not measured.
    //     ⭐ AND THE FRAMES WERE OPENED, because the last wave lost a run to exactly this trap.
    //       kerbT_30 dumped 325 frames; presents 1700-1950 (the whole approach and impact) show an
    //       empty road, the raised pavement, the fence and a "NO TOWING" sign, and NO traffic car
    //       anywhere. Eight of those frames are kept beside the run under frames/keep/.
    //
    // ==============================================================================================
    // (F) THE ONE-STEP SPEED LOSS, RE-READ FROM THE COMMITTED st_scoutB LOG (2026-09-03). No new
    //     run: every number below is in scratch/flow_run/st_scoutB/BrnGame.log, which the previous
    //     wave already captured. Two things it was believed to show are wrong.
    //
    //     ⭐ (F1) "28.7 -> 8.1 mph IN ONE STEP at f1012->f1013" IS A DISPLAY ARTEFACT. The mph
    //     field in [kerb]/[kerb-car] is mfSpeedMPH, a CACHE recomputed once per step, and it lags
    //     the live velocity by one step. The f1012 line says so on its own face:
    //         [kerb-car] f 1012 ... vel -0.228203 -0.593689 -3.206746 mph 28.650387
    //     -- a 3.26 m/s velocity (7.3 mph) printed beside a 28.65 mph label. Read the VELOCITY:
    //         f1011  vel (-0.166688, -0.595939, -12.654400)   pos (3384.4275, 0.21091, -1709.7499)
    //         f1012  vel (-0.228203, -0.593689,  -3.206746)   pos (3384.4231, 0.20468, -1709.7620)
    //     The real event is f1011 -> f1012: 9.45 m/s lost in ONE step, essentially all of it
    //     along +z, i.e. straight against travel. The mph column then catches up at f1013 (7.997)
    //     and that catch-up is what four reads mistook for the event.
    //
    //     ⛔ (F2) THE DEFORMATION-ARM LEAD IS REFUTED FOR THIS EVENT. The standing hypothesis was
    //     that a culled contact still feeds the deformation route, and the evidence offered was
    //     "[kerb-imp] recorded impulses of 590 and 256 N.s". Those lines are at f1013 --
    //     grep says there is NO [kerb-imp] line at f1011 or f1012 at all -- and their normals are
    //     (-0.494, 0.862, 0.114) / (-0.004, 1.000, -0.017) / (-0.006, 1.000, -0.021) with negative
    //     closing speeds: the car SETTLING onto the ground after it had already stopped. They are
    //     the aftermath, not the cause. f1011 carried 3 world contacts and f1012 carried 2, every
    //     one curb 1 / wall 0, both culls firing, re-pointed to the up axis.
    //     ⇒ 9.45 m/s left the car in a step with ZERO world impulses. The mechanism is not on the
    //       world-contact path, so no amount of re-reading [kerb]/[kerb-imp] can name it.
    //
    //     ⚠️ (F3) AND THE POSE MOVED TOO, by something other than the integrate. dp over that step
    //     is (-0.004394, -0.006230, -0.012085). Against v_before that implies dt = 0.0264 / 0.0104
    //     / 0.00096 s on the three axes; against v_after, 0.0193 / 0.0105 / 0.0038. No single dt
    //     fits either, so mTransform.wAxis was written by more than IntegrateTransform. The
    //     post-sim bump-stop translation (UpdateSuspensionPostSimulation @0x825F6BB0,
    //     `mTransform.wAxis += penetrationNormal * dot(up*maxPen, penetrationNormal)`) is the only
    //     other writer in the step and is the obvious suspect -- UNMEASURED, stated as a lead.
    //
    //     ⭐ (F4) THE MECHANISM INVENTORY -- every way a race car's velocity can change in one
    //     PhysicsModule::Update, so the next wave starts from a closed list instead of a hunch:
    //       * ExternalPhysicsBody::CalculateNewVelocity @(PS3 twin; X360 called from 7 sites) --
    //         THE funnel: v += (F*dt + J) / m, then the four accumulators are cleared. Everything
    //         that calls itself an "impulse" arrives here.
    //       * ReadUpdatedBodies @0x82619A10 -- v.y -= KF_GRAVITY*dt, then IntegrateTransform. The
    //         only place the pose advances.
    //       * ⭐ UpdateSuspensionPostSimulation @0x825F6BB0, the `mi8NumWorldCollisions == 0` arm:
    //         per COMPRESSED TRACTION spring, CalculateCollisionImpulseWithInanimateObject with
    //         restitution unk_8208FB10 == 0xBF333333 == -0.7 (image-read this wave) along THAT
    //         WHEEL'S ROAD-CONTACT NORMAL, then AddLocalImpulse + CalculateNewVelocity, up to four
    //         times. Wheel::SetRoadContact @0x825D6C08 only requires `normal.y > 0.5` for
    //         traction, so that normal may be ~60 deg off vertical and the impulse up to ~83%
    //         HORIZONTAL. Magnitude is 0.3 * |v.n| * m_eff: at |v.n| = 12 m/s and m = 1400 kg that
    //         is ~3.6 m/s PER WHEEL -- four wheels span the whole 9.45 m/s. This arm prints
    //         nothing unless BRN_WHEEL_SUS_PROBE is set ([wsus-imp]), which is why five waves of
    //         [kerb] logs could not see it.
    //       * ⭐ StabiliseAfterHardLanding (called from the same function, just above): a DIRECT
    //         VELOCITY REWRITE, not an impulse --
    //             mLinearVelocity -= agNormal * (dot(v, agNormal) * pow(damp, dt*60))
    //         gated on `TimeSinceHardLanding < TimeToDampAfterLanding`. No impulse probe anywhere
    //         can see this one; only a before/after sample can.
    //       * The showtime arms (SetPlayerVehicleInShowtime's 20 m/s launch overwrite,
    //         CapShowtimeVelocities, UpdateShowtimePhysics's vertical floor) -- all direct writes,
    //         all dead on the free-burn path.
    //     ⇒ THE WITNESS FOR THIS IS [dv] (BRN_DV_PROBE=<m/s>), landed 2026-09-03 in
    //       ExternalPhysicsBody.cpp: it samples the player body at eight stage boundaries of
    //       PhysicsModule::Update, records every drain in between with its banked J / F*dt / the
    //       caller's return address, and prints the ledger ONLY for a step whose |dv| crosses the
    //       threshold. A direct rewrite shows up as a mark-to-mark delta with no drain in it.
    //
    // ⛔ WHAT THIS WAVE DID NOT DO. IT DID NOT REPRODUCE EITHER EVENT. Four runs were spent and
    //     none of them drove: the harness's own DRIVE verdict said
    //     "*** THE CAR NEVER MOVED ***" on every one. The blocker is environmental, not the
    //     game code, and it is written up so the next wave does not re-spend the same four runs --
    //     see the [dv] banner in ExternalPhysicsBody.cpp. So the [dv] witness is LANDED AND
    //     COMPILE-GATED BUT NEVER SEEN TO FIRE: treat it as untested instrumentation, and the
    //     first thing to check on the next run is that a deliberate hard stop DOES produce a
    //     [dv] STEP line at all.
    // ⛔ It did not reproduce the 106 mph one-step 6.2 mph loss either (open).
    //     It did not re-decode the raw words of UpdatePostPhysics's wheel-sphere block (the geometry
    //     is read from the committed transcription, not re-derived).
    // ⚠️ AND IT LOST A RUN TO EXACTLY THE TRAP THE LAST WAVE FLAGGED. kerbX_r3 showed a textbook
    //     15.7 -> 4.0 mph one-step-ish stop "at the kerb"; the FRAMES show a stationary orange
    //     traffic car parked across the approach at (~3390, -1639). The log alone was persuasive and
    //     wrong. Open the frames.
    // ==============================================================================================
    u32 guKerbProbeFrame = 0u;

    bool KerbProbeArmed()
    {
        static s32 siArmed = -1;
        if (siArmed < 0)
        {
            const char* lpcEnv = getenv("BRN_KERB_PROBE");
            siArmed = (lpcEnv != nullptr && lpcEnv[0] != '0') ? 1 : 0;
        }
        return siArmed == 1 && CgsDev::Log::gpDebugPrint != nullptr;
    }

    // One budget per tag; the (budget+1)th call prints the exhaustion notice, later calls are silent.
    bool KerbProbeTake(u32& lruUsed, const char* lpcTag)
    {
        const u32 KU_KERB_PROBE_BUDGET = 150000u;
        if (lruUsed < KU_KERB_PROBE_BUDGET)
        {
            ++lruUsed;
            return true;
        }
        if (lruUsed == KU_KERB_PROBE_BUDGET)
        {
            ++lruUsed;
            *CgsDev::Log::gpDebugPrint
                << lpcTag << " BUDGET EXHAUSTED (" << KU_KERB_PROBE_BUDGET << " lines) at frame "
                << guKerbProbeFrame << " -- this tag prints nothing further; a later silence is "
                   "the budget, not the physics\n";
        }
        return false;
    }
    // ---- end [kerb] globals ------------------------------------------------------------------------

    // The whale writes the wall triangle into the per-car debug component through the same
    // span cast Construct stores (BrnVehicleManager_Construct.cpp:265, the FLAGGED opaque
    // 8x1024 span). The partial host DebugComponent must FIT the console's 1024-byte slot.
    static_assert(sizeof(DebugComponent) <= 1024,
                  "Vehicle::DebugComponent (partial host reconstruction) must fit the console's "
                  "1024-byte maRaceCarDebugComponent slot");

    // ==========================================================================================
    // ValidateRaceCarWorldContact @0x825C6088   (DWARF h:938; asserts BrnVehicleManager.cpp
    // :7419..:7624 -- every line number below is the console's own)
    // ==========================================================================================
    bool VehicleManager::ValidateRaceCarWorldContact(
        CgsSceneManager::SceneManagerIO::PotentialContact* lpInOutContact,
        const CgsSceneManager::SceneManagerIO::TriangleCacheInterface* lpTriCacheInterface,
        f32 lfTimeStep)
    {
        namespace vpu = rw::math::vpu;
        typedef CgsGeometric::Triangle4 Triangle4;

        // ---- the guarded function-statics (guard dword_82FBA030; every value image-read) ------
        static const f32 KVF_GROUND_CLEARANCE_MAX_SPEED_MPH = 25.0f;  // bit0, flt_82004FD8
        static const f32 KVF_GROUND_CLEARANCE_MIN_SPEED_MPH = 10.0f;  // bit1, flt_82004A20
        static const f32 kvfMaxCurbHeight                   = 0.25f;  // bit2, flt_8208F834
        static const f32 kvfWallNormalMaxHeight             = 0.3f;   // bit3, flt_82004740 (f30)
        static const f32 KVF_CAR_BOTTOM_PLANE_MODIFIER      = 0.0f;   // bit4, flt_82001CC0

        CGS_ASSERT(lpInOutContact != nullptr, "lpInOutContact != NULL");           // :7419
        CGS_ASSERT(lpTriCacheInterface != nullptr, "lpTriCacheInterface != NULL"); // :7420

        CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                   "Normal invalid on entry to validate\n");                       // :7421
        CGS_ASSERT(fabsf(lpInOutContact->mNormal.x) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.y) > KF_NORMAL_ZERO_EPSILON
                       || fabsf(lpInOutContact->mNormal.z) > KF_NORMAL_ZERO_EPSILON,
                   "Normal zero on entry to validate\n");                          // :7422

        // [kerb] probe captures (no behaviour): the harvest normal as it arrived (already
        // sign-flipped by the caller's SwapEntityOrder, i.e. pointing OUT of the world into the car),
        // and this TU's line budget.
        const Vector3 lKerbNormalIn = lpInOutContact->mNormal;
        static u32    suKerbLines   = 0u;

        // ---- the wall-normal flatten: near-horizontal normals become PURE horizontal ----------
        if (KF_WALL_NORMAL_DOT_THRESHOLD > fabsf(lpInOutContact->mNormal.y))
        {
            Vector3 lFlat = lpInOutContact->mNormal;
            lFlat.y = 0.0f;                              // vrlimi zero-insert on lane 1
            lpInOutContact->mNormal = vpu::Normalize(lFlat);   // vmsum3fp + vrsqrtefp 2-Newton
            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after clearing Y component\n");             // :7432
        }

        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdA.muId >> 56) == 1u,
                   "lpInOutContact->muVolumeInstanceIdA.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_RACECAR"); // :7436
        CGS_ASSERT((lpInOutContact->muVolumeInstanceIdB.muId >> 56) == 0u,
                   "lpInOutContact->muVolumeInstanceIdB.GetEntityIDOwner() == BrnWorld::E_ENTITYTYPE_WORLD");   // :7437

        const u32 luRaceCarIndex =
            static_cast<u32>(lpInOutContact->muVolumeInstanceIdA.muId >> 42) & 0x3FFFu;
        RaceCarPhysics& lrCar = maRaceCarVehicles[luRaceCarIndex];

        const Vector3 lPointOnCar   = lpInOutContact->mPointOnA;   // v117
        const Vector3 lPointOnWorld = lpInOutContact->mPointOnB;   // v119
        const Vector3 lUpAxis       = lrCar.GetUpAxis();           // v122 == car+0x20 (yAxis)

        // speed in MPH off the cached velocity magnitude (mNormLinearVelocityMag.w, car+0x1340
        // lane 3, m/s) -- distinct from the mfSpeedMPH read the final gate makes.
        const f32 lfSpeedMph = lrCar.mNormLinearVelocityMag.w * KF_MPS_TO_MPH;

        // ---- the contact's triangle, re-fetched from the car's cache window --------------------
        const u16 lu16TriangleIndex = lpInOutContact->mu16PrimitiveIndexB;   // +0x4A (the WORLD side)
        const s32 liLane  = static_cast<s32>(lu16TriangleIndex) % 4;         // srawi/addze signed split
        const s32 liBatch = static_cast<s32>(lu16TriangleIndex) / 4;

        const s32 liNumBatches =
            lpTriCacheInterface->GetNumCachedTriangleBatches(static_cast<s32>(luRaceCarIndex));
        // Streamed value tail ("Invalid contact with triangle N within batch M, when there are
        // only K batches in total") lowered to the static prefix per the standing project rule.
        CGS_ASSERT(liBatch < liNumBatches, "Invalid contact with triangle ");      // :7486

        // GetCache carries the "mpTriangleCacheManager != NULL" tripwire the console fires here.
        const Triangle4* lpCache =
            lpTriCacheInterface->GetCache(static_cast<s32>(luRaceCarIndex));
        Triangle4::AOSTriangle lTriangle;
        lpCache[liBatch].GetAOSTriangle(liLane, lTriangle);   // {V0,V1,V2, unit normal, edge cosines}

        // ---- curb / wall classification --------------------------------------------------------
        // Heights of the triangle's three vertices above the road plane(s) under the car's
        // on-ground wheels (GetHeightAboveRoad -- the real query-point overload, fixed this wave).
        const f32 lfHeight0 = lrCar.GetHeightAboveRoad(lTriangle.mVertex0).x;
        const f32 lfHeight1 = lrCar.GetHeightAboveRoad(lTriangle.mVertex1).x;
        const f32 lfHeight2 = lrCar.GetHeightAboveRoad(lTriangle.mVertex2).x;

        const bool lbIsCurb = kvfMaxCurbHeight > lfHeight0
                           && kvfMaxCurbHeight > lfHeight1
                           && kvfMaxCurbHeight > lfHeight2;
        const bool lbIsWall = !lbIsCurb
                           && (kvfWallNormalMaxHeight > fabsf(lTriangle.mNormal.y));

        // ---- the two debug-draw overlays: GATED, NOT RECONSTRUCTED -----------------------------
        // The console draws the classified triangle + three vertex spheres (DebugRender::
        // DrawTriangle @0x8282C218 / DrawSolidSphere @0x8282BF28, offset flt_820047C8 == 0.05,
        // red 0xFF0000FF for walls / green 0xFF00FF00 for curbs) behind the dev-menu toggles
        // mbRenderWallContacts / mbRenderGroundContacts (mDebugComponent +597/+598). Both
        // toggles are FALSE on this build (nothing sets them); the 3D CInEventDrawTriangle
        // record + dispatcher are not reconstructed. Loud named gate, dead until toggled.
        if ((mDebugComponent.RenderWallContacts() && lbIsWall)
            || (mDebugComponent.RenderGroundContacts() && lbIsCurb))
        {
            static bool sbLoggedDrawGate = false;
            if (!sbLoggedDrawGate)
            {
                sbLoggedDrawGate = true;
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: ValidateRaceCarWorldContact's wall/ground contact "
                           "debug draw (DebugRender::DrawTriangle 3D family) not reconstructed "
                           "[FLAG PC boot gate]. Reported once, not per frame\n";
            }
        }

        // ---- record the wall triangle on the car's debug component (REAL console behaviour,
        //      unconditional on the wall classification -- NOT a draw) --------------------------
        if (lbIsWall)
        {
            // The console indexes the component array directly (`this + 0x27DC0 + (car << 10)`).
            // Same span cast as Construct's (BrnVehicleManager_Construct.cpp:265, FLAGGED there);
            // the fit static_assert at the top of this TU bounds the write.
            reinterpret_cast<DebugComponent*>(&maRaceCarDebugComponent[luRaceCarIndex][0])
                ->SetLastWallTriangle(&lTriangle);
        }

        // ---- the speed-scaled ground-clearance gate --------------------------------------------
        f32 lfCullHeight = KF_GROUND_CONTACT_CULL_HEIGHT;                     // v121 = splat(0.4)
        if (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH > lfSpeedMph)
        {
            if (KVF_GROUND_CLEARANCE_MIN_SPEED_MPH > lfSpeedMph)
            {
                // [kerb] the fast path is a verdict too -- say so rather than vanish. Filtered to a
                // MOVING car (>= 3 mph): a parked car's body shell posts ~24 ground contacts a frame
                // (measured, junkyard, kerb run r1), which ate the whole budget before the drive.
                if (lfSpeedMph >= 3.0f && KerbProbeArmed() && KerbProbeTake(suKerbLines, "[kerb]"))
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[kerb] f " << guKerbProbeFrame << " car " << luRaceCarIndex
                        << " SUB10 mph " << lfSpeedMph << " min " << KVF_GROUND_CLEARANCE_MIN_SPEED_MPH
                        << " nIn " << lKerbNormalIn.x << " " << lKerbNormalIn.y << " " << lKerbNormalIn.z
                        << " pB " << lPointOnWorld.x << " " << lPointOnWorld.y << " " << lPointOnWorld.z
                        << " tri " << static_cast<s32>(lu16TriangleIndex)
                        << " h " << lfHeight0 << " " << lfHeight1 << " " << lfHeight2
                        << " curb " << (lbIsCurb ? 1 : 0) << " wall " << (lbIsWall ? 1 : 0)
                        << " accept 1 (every contact accepted below 10 mph, unrewritten)\n";
                }
                return true;   // below 10 mph EVERY contact is accepted (li r3,1 fast path)
            }
            // 10..25 mph: scale the cull height by (speed-10)/15 (vrefp + 2 Newton reciprocal).
            lfCullHeight = lfCullHeight
                * (lfSpeedMph - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH)
                / (KVF_GROUND_CLEARANCE_MAX_SPEED_MPH - KVF_GROUND_CLEARANCE_MIN_SPEED_MPH);
        }

        // ---- the two ground-cull candidates (both suppressed for walls) ------------------------
        bool lbCullToPlane = false;   // r30

        // [kerb] both-sides captures of the two culls (probe only; no behaviour).
        f32  lfKerbDistA = 0.0f;
        bool lbKerbCullA = false;
        f32  lfKerbDistB = 0.0f;
        bool lbKerbCullB = false;
        f32  lfKerbHeightAbovePlane = 0.0f;

        // (a) the wheel-plane test, gated on mbMinWheelDistValid (car+0x714): is the world-side
        //     point within the (scaled) cull height of the car's wheel plane?
        if (lrCar.mbMinWheelDistValid)
        {
            const Vector3 lDelta = vpu::Subtract(lPointOnWorld,
                                                 lrCar.mWheelPlanePosAndHeight.GetVector3());
            const f32 lfDist = vpu::Dot(lDelta, lUpAxis);
            lfKerbDistA = lfDist;                                    // [kerb]
            if (lrCar.mWheelPlanePosAndHeight.w + lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
                lbKerbCullA   = true;                                // [kerb]
            }
        }

        // (b) the above-ground-test plane (car+0x570), when its normal is up-ish (dot > 0.8).
        const AboveGroundTestResult& lrAboveGround = *lrCar.GetAboveGroundTestResult();
        const f32 lfKerbUpDotAg = vpu::Dot(lUpAxis, lrAboveGround.mIntersectionNormal);   // [kerb] the gate's raw operand
        if (vpu::Dot(lUpAxis, lrAboveGround.mIntersectionNormal) > KF_UP_DOT_GROUND_NORMAL_MIN)
        {
            // The console re-reads mPointOnB here and asserts it against the register-cached
            // copy ("lPointOnOther == lpInOutContact->mPointOnB", :7598). Nothing between the
            // entry load and this point writes mPointOnB, so the check is an identity on the
            // host -- reproduced as this note, not as a self-comparison.
            const f32 lfDist = vpu::Dot(
                vpu::Subtract(lPointOnWorld, lrAboveGround.mIntersectionPosition),
                lrAboveGround.mIntersectionNormal);
            lfKerbDistB = lfDist;                                    // [kerb]
            if (lfCullHeight > lfDist && !lbIsWall)
            {
                lbCullToPlane = true;
                lbKerbCullB   = true;                                // [kerb]
            }
        }

        // ---- the plane projection rewrite ------------------------------------------------------
        if (lbCullToPlane)
        {
            // World-space plane anchor: the FRONT-LEFT wheel's streamed (local) position pushed
            // through the car transform (raw-word verified: vmaddfp chain vD = vA*vC + vB over
            // the xAxis/yAxis/zAxis rows + wAxis, local point = car+0x1C0 ==
            // maWheels[0].mStreamedPositionPlusTwistAmount.xyz).
            const Matrix44Affine& lrTransform = lrCar.GetTransform();
            const Vector3Plus& lrLocal =
                lrCar.GetWheel(eFrontLeftWheel).mStreamedPositionPlusTwistAmount;
            Vector3 lPlanePoint;
            lPlanePoint.x = lrTransform.wAxis.x + lrTransform.xAxis.x * lrLocal.x
                          + lrTransform.yAxis.x * lrLocal.y + lrTransform.zAxis.x * lrLocal.z;
            lPlanePoint.y = lrTransform.wAxis.y + lrTransform.xAxis.y * lrLocal.x
                          + lrTransform.yAxis.y * lrLocal.y + lrTransform.zAxis.y * lrLocal.z;
            lPlanePoint.z = lrTransform.wAxis.z + lrTransform.xAxis.z * lrLocal.x
                          + lrTransform.yAxis.z * lrLocal.y + lrTransform.zAxis.z * lrLocal.z;
            lPlanePoint.w = 0.0f;

            // mNormal := the car's up axis; mPointOnA := its projection onto the wheel plane
            // (shifted by the zero KVF_CAR_BOTTOM_PLANE_MODIFIER, kept for fidelity).
            lpInOutContact->mNormal = lUpAxis;
            const f32 lfHeightAbovePlane =
                vpu::Dot(vpu::Subtract(lPointOnCar, lPlanePoint), lUpAxis)
                - KVF_CAR_BOTTOM_PLANE_MODIFIER;
            lfKerbHeightAbovePlane = lfHeightAbovePlane;             // [kerb]
            lpInOutContact->mPointOnA.x = lPointOnCar.x - lUpAxis.x * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.y = lPointOnCar.y - lUpAxis.y * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.z = lPointOnCar.z - lUpAxis.z * lfHeightAbovePlane;
            lpInOutContact->mPointOnA.w = lPointOnCar.w - lUpAxis.w * lfHeightAbovePlane;

            CGS_ASSERT(IsValidVec3Lanes(lpInOutContact->mNormal),
                       "Normal invalid after setting to wheel plane normal\n");    // :7615
        }

        // ---- the final acceptance: is the world point inside the car's (deformable) AABB, ------
        //      expanded by the frame's displacement?
        const Matrix44Affine& lrTransform = lrCar.GetTransform();
        const Vector3 lDiff = vpu::Subtract(lPointOnWorld, lrTransform.wAxis);
        const f32 lfLocalX = vpu::Dot(lDiff, lrTransform.xAxis);
        const f32 lfLocalY = vpu::Dot(lDiff, lrTransform.yAxis);
        const f32 lfLocalZ = vpu::Dot(lDiff, lrTransform.zAxis);

        const Vector3 lDisplacement{ lrCar.mLinearVelocity.x * lfTimeStep,
                                     lrCar.mLinearVelocity.y * lfTimeStep,
                                     lrCar.mLinearVelocity.z * lfTimeStep,
                                     lrCar.mLinearVelocity.w * lfTimeStep };
        const f32 lfDispX = vpu::Dot(lDisplacement, lrTransform.xAxis);   // v10 (kept SIGNED for the recheck)
        const f32 lfDispY = vpu::Dot(lDisplacement, lrTransform.yAxis);
        const f32 lfDispZ = vpu::Dot(lDisplacement, lrTransform.zAxis);

        // upper = max + (0.1, 0.1, 0.0) + |disp| ; lower = min - (0.1, 0.1, 0.0) - |disp|
        // (margin lanes raw-word verified: x/y == flt_82004014, z == flt_82001CC0 == 0).
        const f32 lfUpperX = lrCar.mDeformableAABB.mMax.x + KF_AABB_MARGIN_XY + fabsf(lfDispX);
        const f32 lfUpperY = lrCar.mDeformableAABB.mMax.y + KF_AABB_MARGIN_XY + fabsf(lfDispY);
        const f32 lfUpperZ = lrCar.mDeformableAABB.mMax.z + 0.0f            + fabsf(lfDispZ);
        const f32 lfLowerX = lrCar.mDeformableAABB.mMin.x - KF_AABB_MARGIN_XY - fabsf(lfDispX);
        const f32 lfLowerY = lrCar.mDeformableAABB.mMin.y - KF_AABB_MARGIN_XY - fabsf(lfDispY);
        const f32 lfLowerZ = lrCar.mDeformableAABB.mMin.z - 0.0f            - fabsf(lfDispZ);

        // CONSOLE QUIRK REPRODUCED, raw-word verified: the three LOWER-bound compares all
        // test the LOCAL-Y coordinate (0x825C6DAC == 0x825C6DE4 == word 112C4EC6 -> vcmpgtfp.
        // v9, v12(localY), v9(lower splat); 0x825C6E20 -> 100C06C6 likewise), where the upper-
        // bound compares use x/y/z correctly. The PS3 compiles the SAME pairing (its cmp2/4/6
        // all read the one splat register) -- a source-level copy/paste in the shipping game,
        // reproduced faithfully rather than "fixed".
        const bool lbInsideAabb = (lfUpperX > lfLocalX) && (lfLocalY > lfLowerX)
                               && (lfUpperY > lfLocalY) && (lfLocalY > lfLowerY)
                               && (lfUpperZ > lfLocalZ) && (lfLocalY > lfLowerZ);

        bool lbAccept;
        if (lbInsideAabb)
        {
            lbAccept = true;
        }
        else if (lrCar.mbCrashing)                    // car+0x710: crashing cars keep contacts
        {
            lbAccept = true;
        }
        else if (lrCar.mbHasAir)                      // car+0x1350: airborne cars keep contacts
        {
            lbAccept = true;
        }
        else
        {
            // slow-ish cars keep out-of-AABB contacts too (mfSpeedMPH is a lane splat).
            lbAccept = KF_FINAL_ACCEPT_MAX_SPEED_MPH > lrCar.GetSpeedMPH().x;
        }

        // ---- the velocity-expanded LATERAL recheck (always runs; can only REJECT) --------------
        // Band on the local-X axis only, margin 0.2, expanded toward the travel direction.
        // dx == 0.0 exactly collapses the band to zero (the console's vmr-zero path) -- only
        // reachable at speeds >= 10 mph (the sub-10 fast path returned above), where an exactly
        // zero lateral displacement dot is a measure-zero event, exactly as shipped.
        f32 lfBandUpperX;
        f32 lfBandLowerX;
        if (lfDispX > 0.0f)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.x + KF_LATERAL_RECHECK_MARGIN + lfDispX;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.x - KF_LATERAL_RECHECK_MARGIN;
        }
        else if (0.0f > lfDispX)
        {
            lfBandUpperX = lrCar.mDeformableAABB.mMax.x + KF_LATERAL_RECHECK_MARGIN;
            lfBandLowerX = lrCar.mDeformableAABB.mMin.x - KF_LATERAL_RECHECK_MARGIN + lfDispX;
        }
        else
        {
            lfBandUpperX = 0.0f;
            lfBandLowerX = 0.0f;
        }
        if (lfLocalX > lfBandUpperX || lfBandLowerX > lfLocalX)
        {
            lbAccept = false;
        }

        // ---- [kerb] the per-contact witness: every gate above, both sides (TU-scope banner) ------
        if (KerbProbeArmed() && KerbProbeTake(suKerbLines, "[kerb]"))
        {
            *CgsDev::Log::gpDebugPrint
                << "[kerb] f " << guKerbProbeFrame << " car " << luRaceCarIndex
                << " mph " << lfSpeedMph
                << " nIn " << lKerbNormalIn.x << " " << lKerbNormalIn.y << " " << lKerbNormalIn.z
                << " pA " << lPointOnCar.x << " " << lPointOnCar.y << " " << lPointOnCar.z
                << " pB " << lPointOnWorld.x << " " << lPointOnWorld.y << " " << lPointOnWorld.z
                // sphA == mu16PrimitiveIndexA, the CAR-side primitive the narrow phase hit
                // (ContactGeneratorJob::ExecuteSphereListWithTriangleList writes muPrimitive1Index
                // = the sphere index; SwapEntityOrder moved it to the A slot). The car's world
                // sphere list is GetWorldSpaceSpheres() == GetNumSensors() == the deformation
                // spec's sensor count PLUS FOUR APPENDED WHEEL SPHERES, so sphA >= nSens means
                // "a WHEEL sphere generated this contact". Pair with [kerb-wsph], which prints
                // nSens and the four wheel spheres' geometry for the same frame.
                << " sphA " << static_cast<s32>(lpInOutContact->mu16PrimitiveIndexA)
                << " tri " << static_cast<s32>(lu16TriangleIndex)
                << " triN " << lTriangle.mNormal.x << " " << lTriangle.mNormal.y << " " << lTriangle.mNormal.z
                << " vy " << lTriangle.mVertex0.y << " " << lTriangle.mVertex1.y << " " << lTriangle.mVertex2.y
                << " h " << lfHeight0 << " " << lfHeight1 << " " << lfHeight2 << " maxCurb " << kvfMaxCurbHeight
                << " curb " << (lbIsCurb ? 1 : 0) << " wall " << (lbIsWall ? 1 : 0)
                << " cullH " << lfCullHeight
                << " mwdv " << (lrCar.mbMinWheelDistValid ? 1 : 0)
                << " wpH " << lrCar.mWheelPlanePosAndHeight.w
                << " dA " << lfKerbDistA << " cullA " << (lbKerbCullA ? 1 : 0)
                << " agValid " << (lrAboveGround.mbValid ? 1 : 0)
                << " agN " << lrAboveGround.mIntersectionNormal.x << " "
                << lrAboveGround.mIntersectionNormal.y << " " << lrAboveGround.mIntersectionNormal.z
                << " agPy " << lrAboveGround.mIntersectionPosition.y
                << " upDotAg " << lfKerbUpDotAg << " min " << KF_UP_DOT_GROUND_NORMAL_MIN
                << " dB " << lfKerbDistB << " cullB " << (lbKerbCullB ? 1 : 0)
                << " repointed " << (lbCullToPlane ? 1 : 0)
                << " nOut " << lpInOutContact->mNormal.x << " " << lpInOutContact->mNormal.y << " "
                << lpInOutContact->mNormal.z
                << " pAout " << lpInOutContact->mPointOnA.x << " " << lpInOutContact->mPointOnA.y << " "
                << lpInOutContact->mPointOnA.z
                << " hAbovePlane " << lfKerbHeightAbovePlane
                << " local " << lfLocalX << " " << lfLocalY << " " << lfLocalZ
                << " aabb " << (lbInsideAabb ? 1 : 0)
                << " hasAir " << (lrCar.mbHasAir ? 1 : 0)
                << " accept " << (lbAccept ? 1 : 0) << "\n";
        }
        // ---- end [kerb] ------------------------------------------------------------------------

        return lbAccept;
    }

    // ==========================================================================================
    // Vehicle::DebugComponent::SetLastWallTriangle @0x825B4D60: the temporary copy that lived
    // here is DELETED 2026-08-24 (physics mount wave B3) -- its declared home TU
    // (B5PhysicsHandlingDebugComponent.cpp) is mounted now and owns the identical body. The
    // old banner's vtable fear (dragging the CgsDev::DebugComponent base's unreconstructed
    // virtual surface) was RE-MEASURED this wave: ZERO LNK2019 -- that base surface has landed
    // since; the link flagged only the predicted LNK2005, exactly as the banner said it would.
    // ==========================================================================================
}
}
