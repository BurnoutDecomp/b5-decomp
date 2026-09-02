#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include <algorithm>   // std::sort (the exported std::_Sort<ContactTime*> -- walls leg 4)

#include "GameShared/GameClasses/Development/Log/CgsLog.h"                       // gpDebugPrint / gxMessageFilterFlags (walls leg 4 gates)
#include <cstdlib>                                                              // getenv -- the opt-in [chain] bring-up probe only
#include "GameSource/Physics/BrnPhysicsModuleIO.h"                               // PhysicsModuleIO::OutputBuffer (obj Update params, walls leg 4)
#include "GameSource/Physics/BrnPhysicsModuleIO_PotentialContactInterface.h"     // PhysicsModuleIO::PotentialContactInterface (walls leg 4)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h"   // InSceneUpdateInterface::SetEntityRadius (walls leg 4)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"  // CgsGeometric::AxisAlignedBox
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Dot, Mult, Subtract, ...}
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::TransformVector (walls leg 9: the per-direction world axis)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere (walls leg 9: the sensor radius the limit rows pad by)
#include "GameSource/Physics/VehicleManager/VehiclePhysics/VehicleAttribs.h"  // VehicleAttribs::mCollisionAttribs (the P4 car-car impulse scale, 2026-08-24)
#include <cmath>                                                              // std::sqrt (the P4 tangential magnitude)
#include "GameSource/World/BrnEntityTypes.h"                                  // BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE (the ApplySensorImpulse owner test)

// =================================================================================================
// BrnPhysics::Deformation::DeformableObject -- the per-frame UPDATE core (group "update").
//
// This TU bodies seven per-frame update methods of DeformableObject, reconstructed faithfully from
// the X360 ARTIST.XEX pseudocode/asm (group dossier scratchpad/wave4/dos_object.txt). Each is listed
// with its X360 address and a modelled-vs-asm note:
//
//   UpdateSensorDisplacements   @0x825DF898  advance every sensor's point-displacement vector by the
//                                            body transform, scaled by the supplied time-step lane.
//   UpdateSkinningOffsets       @0x825DFA90  drive the IK driven points from the Verlet scratch
//                                            (maVerletOffsets_Scratch) into each IK part's skin,
//                                            box-clamping the bonnet/boot panel types.
//   UpdateLocators              @0x8260A018  refresh the generic / light / camera locator transforms
//                                            from the graphics-vehicle transform via UpdateLocator.
//   ClearStoredContacts         @0x825BA350  reset every sensor's post-physics scratch + contact count.
//   ApplySensorImpulse          @0x826078B0  the heavy per-sensor apply -- build the per-direction
//                                            ImpulseParams, route it through the sensor body, then
//                                            bank the resolved world impulse on the vehicle body and
//                                            update the contact spy.
//   RenderSensors               @0x825E08C0  debug-draw every sensor sphere + its inter-sensor links.
//   UpdateOutputContactSpies    @0x826251E8  push each sensor's contact spy into the output buffer.
//
// Member-offset map recovered from the X360 asm (BrnDeformableObject.cpp), pinned BY NAME onto the
// frozen header members (cf. the sibling BrnDeformableObject_Contacts.cpp map):
//   this+6368  -> mpDeformationSpec      (spec+1618 == mu8NumDeformationSensors; == GetNumSensors()-4)
//   this+6476  -> mVehicleBody's attached VehiclePhysics  (mVehicleBody.GetVehiclePhysics())
//   this+6480  -> maDeformationSensors[] (stride 432 == sizeof(DeformationSensor))
//   this+4320  -> maVerletOffsets_Scratch[128]  (Vector3Plus; dword index 4*i)
//   this+19216 -> miNumDrivenPoints
//   this+25388 -> maIKParts[50]          (stride 16; mpSpec field at +8)
//   this+26180 -> maPartStates[50]
//   this+26232 -> miNumIKBodyParts
//   this+26384 -> mHandlingBodyID (8B)   / this+26392 -> mGlobalEntityId (the header's duplicate
//                 mu32GameModeState models the same +26392 seat -- see its :572 flag; the walls
//                 wave keeps the two equal in Prepare until the reconciliation lands)
//
// ============================ MODELLED-vs-ASM (read before editing) ==============================
// The X360 build is dense VMX128 inline assembly. Per the established house idiom (cf. the committed
// BrnDeformableObject.cpp::ApplyCarCarImpulse, BrnDeformableObject_Contacts.cpp and
// BrnDeformationSensor.cpp) the bodies below are the de-SIMD'd scalar / Vector3 equivalents written
// against the members BY NAME -- no __asm, no raw-offset pokes. The reconstruction is FAITHFUL to the
// OBSERVABLE behaviour: same control flow, same branch structure, same early-outs, same loop bounds +
// strides, same named-member stores, same call order; the per-lane SIMD arithmetic is modelled as
// explicit scalar lane math.
//
// FLAGGED-0 PLACEHOLDERS (rodata NOT in the per-function exports -- NEVER fabricated): the per-sensor
// direction basis, the apply's friction/limit/scale rows and the sensor-render colour ramp have no
// recoverable XEX symbol; they are carried as correctly-shaped honest zeros (the indexing shape / loop
// structure is exact, the numeric output stays inert until the rodata lands).
//
// FLAGGED-DEFERRED (out-of-tree callees with no in-tree declaration -- NOT fabricated): two emission
// callees are forward-declared-only externals and so cannot be reached BY NAME without inventing a
// declaration:
//   * CgsDev::Debug3DImmediateRender::DrawBox / DrawLine  (RenderSensors) -- the render class is only
//     forward-declared in the frozen header; the draw calls are documented but not emitted.
//   * DeformationSensor::OutputContactSpy                 (UpdateOutputContactSpies) -- not declared
//     on the homed DeformationSensor; the per-sensor spy push is documented but not emitted.
// Both reproduce the recoverable outer flow (loop bounds, counts, the game-mode-word source); the
// emission is left as a documented gap (no fabricated callee signatures / declarations).
//
// ASSERTS are non-gating tripwires (BeginAssert/FireAssert/EndAssert == one CGS_ASSERT): in the asm
// execution continues past a failed assert, so the C++ falls through identically.
// =================================================================================================

// [T5-sens] DIAG state, DEFINED in BrnPhysicalTrafficManager_UpdateTrafficPhysics.cpp.
// NOT IN THE X360 BINARY. DELETE-WHEN-STABLE.
namespace BrnPhysics { namespace Vehicle { extern s32 gT5ApplyOwner; extern s32 gT5ApplyGlobal; } }

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // The 0.01-style normal/limit tolerance the apply's magnitude gate compares against (the asm's
        // vcmpgtfp against the splatted zero / the FLAGGED limit rows). Carried at the recovered shape.
        const f32 KF_APPLY_EPSILON = 0.0099999998f;

        // ApplySensorImpulse walks the six signed body axes (E_NEXT_SENSOR_DIRECTION 0..5) building a
        // per-direction unit vector (the switch at 0x82607BAC). KI_NUM_APPLY_DIRECTIONS == 6.
        const s32 KI_NUM_APPLY_DIRECTIONS = 6;

        // ---- [chain] PC bring-up instrument -- DELETE WHEN the wall test is banked -------------
        // OPT-IN (BRN_IMPULSE_PROBE=1). Counts every hop of the deformation impulse chain so ONE
        // run says which hop is dead, instead of inferring it across runs.
        struct ChainProbe
        {
            u32 muUpdateContacts;   // UpdateContacts entries
            u32 muSensorsScanned;   // sensors offered to GetImpulse
            u32 muGetImpulseHits;   // sensors that had a latched impulse contact
            u32 muWorldApplies;     // (unused: ApplyCarWorldImpulse lives in another TU; its only
                                    //  product is the ApplySensorImpulse call counted below)
            u32 muSensorImpulse;    // ApplySensorImpulse entries
            u32 muDirsTried;        // six-direction loop iterations reached
            u32 muDirsPassed;       // iterations whose projection was > 0
            u32 muDispatched;       // lpSensor->ApplyLocalImpulse dispatches
            u32 muNullSensor;       // dispatches skipped because lpSensor was null
        };
        ChainProbe gChainProbe = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        s32 giChainProbeOn = -1;
        inline bool ChainProbeOn()
        {
            if ( giChainProbeOn < 0 )
            {
                const char* lpcEnv = getenv( "BRN_IMPULSE_PROBE" );
                giChainProbeOn = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            return giChainProbeOn == 1;
        }

        // ⭐⭐ TABLE RETIRED 2026-08-14 (walls leg 4): the flagged-zero KsaApplyDirection placeholder
        // is the shared BrnPhysics::Deformation::KA_IMPULSE_DIRECTIONS (BrnCollidableBody.cpp) --
        // the PS3 exports name the global, its accessor AND the initializer that writes the six
        // signed unit body axes in ENextSensorDirection order. With the zero rows every projection
        // in the six-direction loop was 0 -> the loop never fired and the banked vehicle impulse
        // was identically zero (the silent-drop shape). The reference below aliases the real table.
        const Vector3 (&KsaApplyDirection)[KI_NUM_APPLY_DIRECTIONS] = KA_IMPULSE_DIRECTIONS;

        // FLAGGED-0 PLACEHOLDERS for the friction / limit / scale rows the resolved world impulse is
        // shaped by after GetImpulsesFromLocalImpulse (&unk_82FB95C0 / &unk_82FB8330 / &unk_82FB9D30).
        // Honest zeros (NEVER fabricated); the per-lane clamp/scale SHAPE is exact, the values inert.
        // ⭐ RECOVERED 2026-08-03 (static-init splats). The friction scale is tiny (1.5e-4) and the
        // clamp is 1000, which is why the pair reads as "scale hard down, then bound".
        const Vector3 KVF_APPLY_FRICTION_SCALE = { 0.000150000007f, 0.000150000007f, 0.000150000007f, 0.000150000007f };  // unk_82FB8330 @82C5D688 <- flt_8209D738
        const Vector3 KVF_APPLY_FRICTION_CLAMP = { 1000.0f, 1000.0f, 1000.0f, 1000.0f };  // unk_82FB95C0 @82C5D868 <- flt_82009E10
        const Vector3 KVF_APPLY_SHOWTIME_SCALE = { 5.0f, 5.0f, 5.0f, 5.0f };  // unk_82FB9D30 @82C5D890 <- flt_8200426C

        // ⭐ RECOVERED 2026-08-24 (deform-land wave, P4) -- the ApplySensorImpulse block-5 scale
        // rows, each from its static-init writer (headless idat decode):
        const Vector3 KVF_APPLY_WORLD_CRASH_SCALE   = { 5.0f, 5.0f, 5.0f, 5.0f };     // unk_82FB8060 @82C5D610 <- flt_8200426C
        const Vector3 KVF_APPLY_CARCAR_CRASH_SCALE  = { 20.0f, 20.0f, 20.0f, 20.0f }; // unk_82FB8300 @82C5D638 <- flt_8208F9D4
        const Vector3 KVF_APPLY_LOCAL_FORCE_SCALE   = { 0.899999976f, 0.899999976f, 0.899999976f, 0.899999976f }; // unk_82FB7F50 @82C5D5E8 <- flt_82005450
        const Vector3 KVF_APPLY_MIN_TANGENTIAL_SQ   = { 9.999999747e-05f, 9.999999747e-05f, 9.999999747e-05f, 9.999999747e-05f }; // unk_82FB9720 @82C5D840 <- flt_82002540
        // (unk_82FB9D40 -- the not-showtime bank row -- initialises to splat(0.0) @82C5D5C0;
        //  spelled as a zero literal at the select site.)

        // ⭐⭐ RECOVERED 2026-08-15 (walls leg 8) -- the DRIVE-TIME DEFORMATION budget row,
        // &unk_82FB9520. Dynamic-init (zero in the image); initialiser @0x82C5D818..0x82C5D83C loads
        // flt_82004744 == 0.2, splats it (vspltw v0,v0,0) and stores the row. ApplySensorImpulse block
        // (3) writes it into BOTH mvfAllowedCompressionFactor (+0x90) and mvfMaximumAllowedAbsorption
        // (+0xA0) for an ordinary, non-crash contact. See the long note at the select below: carrying
        // this as a zero was what made an ordinary world contact bank no momentum at all.
        const VecFloat KVF_DRIVE_TIME_DEFORMATION = { 0.2f, 0.2f, 0.2f, 0.2f };

        // The `kbAllowDriveTimeDeformation` byte the select tests (`lbz r10, 0x82F2A346` @0x826079E4).
        // ⭐ The shipped X360 image has it SET (byte == 0x01), so the live console path is the row
        // above; the zero arm is the console's own deformation-disabled build option.
        //
        // ⭐⭐⭐ RELEASED 2026-08-16 (walls leg 9) -- the divergence is RETIRED and the flag now carries
        // the console's own shipped value. Leg 8 held it false for ONE stated reason: with the chain
        // forward gated, absorbing would delete momentum instead of transferring it. Leg 9 found and
        // fixed what the gate was really standing on (five dropped `ImpulseParams` stores, chiefly
        // `mpImpulsePasser` -- see BrnDeformableObject.cpp and BrnDeformationSensor.cpp), so the two
        // flags were flipped TOGETHER exactly as leg 8 prescribed. This is no longer a divergence:
        // the shipped X360 image's byte at 0x82F2A346 is 0x01 and so is this.
        const bool KB_ALLOW_DRIVE_TIME_DEFORMATION = true;

        // The two part-type ids whose driven points are skinned through the BOX-CLAMPED path
        // (UpdateSkinningOffsetsWithinBox) -- the bonnet / boot panel types in the asm's
        // `v23 == 24 || v23 == 25` GetPartType test. FLAG: the EBodyParts enum only homes
        // E_BODY_PART_INVALID, so these are the raw part-type constants the asm compares against.
        const s32 KI_BODY_PART_BOX_CLAMPED_A = 24;
        const s32 KI_BODY_PART_BOX_CLAMPED_B = 25;

        // The takedown/showtime game-mode selector the apply's crash latch + budget gate test (the asm's
        // `HIBYTE(this+26384) == 2`, via GetHandlingBodyIdHighByte()). Same constant the bounce path uses.
        const u8 KU_GAMEMODE_BOUNCE_ELIGIBLE = 2;

        // FLAGGED-0 PLACEHOLDER for the UpdateSkinningOffsets clamp-box inflation row (&unk_82FB9550 in
        // the asm: the per-lane margin subtracted from the body suspension-extent min and added to the
        // max to build the bonnet/boot WithinBox clamp). No recoverable XEX symbol -- honest zeros
        // (NEVER fabricated); the box-construction SHAPE (min = ext - row, max = ext + row) is exact,
        // the inflation stays inert until the rodata lands.
        // ⭐ RECOVERED 2026-08-03. Note this one is NOT a splat: the initialiser @82C5D97C builds
        // {flt_82004014 (0.1), 0, 0, 0}, i.e. only the X lane carries the margin. A splat would have
        // inflated all three axes; the console inflates one.
        const Vector3 KVF_SKINNING_CLAMP_MARGIN = { 0.100000001f, 0.0f, 0.0f, 0.0f };  // unk_82FB9550
    }

    // =============================================================================================
    // UpdateSensorDisplacements @0x825DF898
    //
    // ⭐⭐ Advance every deformation sensor's point-displacement vector by ONE FRAME OF ITS OWN
    // MOTION. For each bare deformation sensor (GetNumSensors() - 4 ==
    // mpDeformationSpec->mu8NumDeformationSensors, the asm's `lbz r9, 0x652(spec)`):
    //   * p = bodyRotation * mpLocalSpaceSphere->centre    -- the sensor's world-space offset,
    //   * pointVelocity = mLinearVelocity + mAngularVelocity x p   (the standard rigid-body point
    //     velocity; the asm builds the cross with the vpermwi128 0x63 yzx double-permute),
    //   * displacement.xyz = pointVelocity * timeStep      (v127 == the VecFloat arg),
    //   * vrlimi128 keeps the original w lane (the biggest-impulse magnitude).
    // The mbActive tripwire (line 1027) is non-gating. The dense VMX is modelled per-lane.
    //
    // ⭐⭐⭐ WHY THIS FUNCTION IS LOAD-BEARING (2026-08-16, walls leg 10). The vector it writes is
    // the DENOMINATOR of DeformationSensor::ValidateAndAddContact's impact-time latch -- the single
    // gate that decides whether a contact ever becomes a deformation impulse. Two independent
    // defects met here: this body dropped both velocity terms, AND the manager-level
    // DeformationManager::UpdateSensorDisplacements that drives it was an inert conductor gate, so
    // no sensor displacement was ever written at all. Measured live before the fix, at 30.4 m/s
    // into a wall: `[latch] WALLFACE ... disp 0.000000 0.000000 0.000000`.
    // =============================================================================================
    void DeformableObject::UpdateSensorDisplacements(VecFloat lvfTimeStep)
    {
        CGS_ASSERT(mbActive, "mbActive");   // line 1027 (non-gating tripwire)

        const s32 liNumSensors = GetNumSensors() - 4;   // *(mpDeformationSpec + 1618)
        if ( liNumSensors <= 0 )
        {
            return;
        }

        // The body transform rows the sensor offset is rotated through (mVehicleBody body
        // sub-object +16 == the attached vehicle's world transform). Reached through GetTransform.
        Matrix44Affine lBodyTransform;
        GetTransform(lBodyTransform);
        const Vector3& lR  = lBodyTransform.Right();
        const Vector3& lU  = lBodyTransform.Up();
        const Vector3& lAt = lBodyTransform.At();

        // ⭐⭐⭐ RESTORED 2026-08-16 (walls leg 10) -- THE TWO VELOCITY TERMS. The asm's `r11` is
        // `*(this+0x194C) + 0x10`, i.e. the attached VehiclePhysics' TRANSFORM base, and it loads
        // FIVE vectors off it, not three:
        //   0x825DF934  lvx128 v9, r0,  r11        ; +0x00  transform Right
        //   0x825DF930  lvx128 v0, r11, 0x10       ; +0x10  transform Up
        //   0x825DF93C  lvx128 v8, r11, 0x20       ; +0x20  transform At
        //   0x825DF944  lvx128 v7, r11, 0x40       ; +0x40  mLinearVelocity   <- WAS DROPPED
        //   0x825DF940  lvx128 v0, r11, 0x50       ; +0x50  mAngularVelocity  <- WAS DROPPED
        // Those last two offsets are already attested by name in the tree: ExternallySimulatedBody.h
        // pins mTransform @+0x10, mLinearVelocity @+0x40 and mAngularVelocity @+0x50 RELATIVE TO
        // THE TRANSFORM BASE (object +0x50 / +0x60), which is exactly what VehiclePhysics'
        // GetLinearVelocity / GetAngularVelocity return.
        const Vehicle::VehiclePhysics* const lpBody = GetVehiclePhysics();
        const Vector3 lLinearVelocity  = lpBody->GetLinearVelocity();
        const Vector3 lAngularVelocity = lpBody->GetAngularVelocity();

        const f32 lfStep = lvfTimeStep.x;   // v127 broadcast time-step lane

        for ( s32 li = 0; li < liNumSensors; ++li )
        {
            DeformationSensor& lrSensor = maDeformationSensors[li];

            // Source vector = the sensor's LOCAL-SPACE sphere centre, read through *(sensor+412) ==
            // mpLocalSpaceSphere (asm: _R4 = _R10[99]; lvx v0,[r4] loads the sphere's packed
            // position/radius vector and the transform uses its xyz centre). The destination is the
            // displacement member, whose original w lane (the biggest-impulse magnitude) v12 keeps and
            // vrlimi128 re-installs after the transform.
            Vector3Plus& lrDisplacement = lrSensor.mPointDisplacement_BiggestImpulseThisFrame;
            // The sensor's mpLocalSpaceSphere is the forward-declared BrnPhysics::Deformation::Sphere
            // (incomplete here); its leading 16 bytes are centre.xyz + radius.w, so read the centre as
            // the leading Vector4 -- the same house idiom BrnTagPoint.cpp / BrnDeformationSensor.cpp use.
            const Vector4& lSphereCentre = *reinterpret_cast<const Vector4*>(lrSensor.GetLocalSpaceSphere());
            const Vector3 lSrc = { lSphereCentre.x, lSphereCentre.y, lSphereCentre.z, 0.0f };

            // (a) Rotate the sensor's local sphere centre into world space: p = R * c
            //     (0x825DF938/948/950, three vmaddfp against splat(c.x/y/z)).
            const Vector3 lP = { lSrc.x * lR.x + lSrc.y * lU.x + lSrc.z * lAt.x,
                                 lSrc.x * lR.y + lSrc.y * lU.y + lSrc.z * lAt.y,
                                 lSrc.x * lR.z + lSrc.y * lU.z + lSrc.z * lAt.z, 0.0f };

            // (b) ⭐⭐⭐ THE POINT VELOCITY: v + omega x p, then the time step.
            // ⛔ WHAT WAS HERE, and why it mattered. This wrote `(R * c) * dt` -- the rotated
            // sensor OFFSET scaled by the time step. That has no velocity in it at all: it is a
            // position times a time, ~1e-3 for a 60 Hz step, and it does not change when the car
            // moves. ValidateAndAddContact divides the contact's penetration depth by the
            // projection of THIS vector on the contact normal to get an impact time in [0,1], so
            // a displacement that is not a swept motion makes that quotient meaningless.
            // The cross product is the two-permute idiom the asm spells out:
            //   0x825DF94C  vpermwi128 v11, v0(omega), 0x63    ; 0x63 == the yzx word rotate
            //   0x825DF958  vpermwi128 v13, v13(p),    0x63
            //   0x825DF95C  vmulfp128  v0,  v0(omega), v13     ; omega * p_yzx
            //   0x825DF960  vnmsubfp   v0,  v11, v0, v10       ; - omega_yzx * p
            //   0x825DF964  vpermwi128 v0,  v0, 0x63           ; and rotate the result back
            // which expands lane-by-lane to exactly the cross product written below. The same
            // formula is already spelled in the tree for the wheels (VehiclePhysics.h:737,
            // "v_contact = mLinearVelocity + mAngularVelocity x (r_contact - bodyPos)").
            const Vector3 lPointVelocity = {
                lLinearVelocity.x + (lAngularVelocity.y * lP.z - lAngularVelocity.z * lP.y),
                lLinearVelocity.y + (lAngularVelocity.z * lP.x - lAngularVelocity.x * lP.z),
                lLinearVelocity.z + (lAngularVelocity.x * lP.y - lAngularVelocity.y * lP.x), 0.0f };

            // vrlimi128 v0,v12,1,0 -- write xyz, keep the original w (the biggest-impulse lane).
            lrDisplacement.x = lPointVelocity.x * lfStep;
            lrDisplacement.y = lPointVelocity.y * lfStep;
            lrDisplacement.z = lPointVelocity.z * lfStep;
        }
    }

    // =============================================================================================
    // UpdateSkinningOffsets @0x825DFA90
    //
    // Drive the IK driven points into each IK part's skin. The asm runs in three passes:
    //  (1) Build the per-frame clamp box from the attached vehicle's DEFORMED bounding box
    //      (`lwz r10, 0x194C(this)` = mVehicleBody's vehicle; `addi r9, r10, 0x6D0` = mDeformableAABB
    //      .mMin, +0x10 = .mMax) inflated by unk_82FB9550 -- used by the box-clamped parts.
    //  (2) Walk the live TAG points (`lwz r9, 0x4B10(this)` == miNumTagPoints; base 0x3B10, stride
    //      0x20); for each SKINNED one (`lbz 0x41(spec)`) gather (mPos - initial, w = scratch) into
    //      maVerletOffsets_Scratch (this+0x10E0); the running row (r29) advances per skinned tag.
    //  (3) Walk every IK part (miNumIKBodyParts): skip parts in state E_PART_STATE_DETATCHED (4);
    //      bonnet/boot parts (GetPartType == 24/25) skin through UpdateSkinningOffsetsWithinBox with the
    //      clamp box, all others through UpdateSkinningOffsets. r29 advances by each part's
    //      GetNumberOfDrivenPoints so the scratch slice handed to each part is its own window.
    //
    // ⭐⭐ 2026-09-02 (rest-rows wave) -- TWO TRANSCRIPTION DEFECTS RETIRED HERE, both measured:
    //   (a) The clamp box was built from FLAGGED-ZERO "suspension extents", so the box was
    //       [-0.1,+0.1] x [0,0] x [0,0] around the car origin and UpdateSkinningOffsetsWithinBox
    //       clamped every bonnet/boot driven point INTO it: the player's rows 40/41 read
    //       (-0.782, 0.148, 0.004) / (0.819, 0.148, 0.004) at rest == (0.1,0,0) - p0 exactly, the
    //       sedan's row 49 (-0.756, 0.326, 0) is the same arithmetic on its own spec. Those are the
    //       "phantom rest rows" that sailed the panel and tripped the fatal test at first sight.
    //       The console reads vehicle+0x6D0/+0x6E0 == SimpleVehiclePhysics::mDeformableAABB (the
    //       same pair ApplySensorImpulse's crash rows use) -- an ordinary box around the whole car.
    //       A flagged zero is only safe where 0 is the identity; here it was a clamp target.
    //   (b) The gather loop bound is miNumTagPoints (this+0x4B10 -- the word ResetDeformation
    //       @0x82639FD8 stores the tag count into), not miNumDrivenPoints.
    // =============================================================================================
    void DeformableObject::UpdateSkinningOffsets()
    {
        // --- (1) clamp box from the vehicle's deformed AABB (0x825DFAB4..0x825DFB0C) ---------------
        //   0x825DFAB4  lwz r10, 0x194C(r31)      ; mVehicleBody's attached VehiclePhysics
        //   0x825DFAB8  lvx128 v0, unk_82FB9550   ; the inflation row {0.1, 0, 0, 0}
        //   0x825DFAC4  addi r9, r10, 0x6D0       ; mDeformableAABB.mMin   (+0x10 == .mMax)
        //   0x825DFAF4  vsubfp v13, v13, v0       ; box.min = aabb.min - row
        //   0x825DFB00  vaddfp v0, v12, v0        ; box.max = aabb.max + row
        // The console reads the vehicle unconditionally (Prepare binds it before any reset).
        const BrnPhysics::Vehicle::VehiclePhysics* lpClampVehicle = mVehicleBody.GetVehiclePhysics();
        const Vector4& lBodyExtentMin = lpClampVehicle->GetDeformableAABB().mMin;   // vehicle + 0x6D0
        const Vector4& lBodyExtentMax = lpClampVehicle->GetDeformableAABB().mMax;   // vehicle + 0x6E0
        CgsGeometric::AxisAlignedBox lClampBox;
        lClampBox.mMin.x = lBodyExtentMin.x - KVF_SKINNING_CLAMP_MARGIN.x;
        lClampBox.mMin.y = lBodyExtentMin.y - KVF_SKINNING_CLAMP_MARGIN.y;
        lClampBox.mMin.z = lBodyExtentMin.z - KVF_SKINNING_CLAMP_MARGIN.z;
        lClampBox.mMin.w = 0.0f;
        lClampBox.mMax.x = lBodyExtentMax.x + KVF_SKINNING_CLAMP_MARGIN.x;
        lClampBox.mMax.y = lBodyExtentMax.y + KVF_SKINNING_CLAMP_MARGIN.y;
        lClampBox.mMax.z = lBodyExtentMax.z + KVF_SKINNING_CLAMP_MARGIN.z;
        lClampBox.mMax.w = 0.0f;

        // --- (2) gather the skinned tag-point offsets into the Verlet scratch (0x825DFB10..0x825DFB88)
        // `lwz r9, 0x4B10(r31)` is miNumTagPoints (this+0x4B10 -- the tag pool's count word, stored by
        // ResetDeformation @0x82639FD8 straight after the tag count is read; the tag base is 0x3B10 and
        // the driven pool starts at 0x4B20). Walk EVERY live tag point; for each whose spec carries the
        // skinned-point flag (`lbz r6, 0x41(spec)` == TagPointSpec::mbSkinnedPoint) store
        // (mPos - spec.mInitialPosition) with w = mfScratchAmount (`lfs f0, 0x1C(tag)`) into
        // maVerletOffsets_Scratch[row]; the row (r29 / r11 += 0x10) advances ONLY when the flag is set.
        // The part walk in (3) then consumes the rows that follow.
        // (Was `li < miNumDrivenPoints` -- the wrong count word; retired 2026-09-02.)
        s32 liScratchBase = 0;
        if ( miNumTagPoints > 0 )
        {
            for ( s32 li = 0; li < miNumTagPoints; ++li )
            {
                TagPoint& lrTagPoint = maTagPoints[li];
                const TagPointSpec* lpSpec = lrTagPoint.GetSpec();   // *v15 == mpSpec (TagPoint +0x10)
                if ( lpSpec != nullptr && lpSpec->IsSkinned() )       // *(*v15 + 65) == mbSkinnedPoint
                {
                    const Vector3 lOffset = lrTagPoint.GetOffsetFromInitialPosition();  // mPos - initialPos
                    Vector3Plus& lrScratch = maVerletOffsets_Scratch[liScratchBase];
                    lrScratch.x = lOffset.x;
                    lrScratch.y = lOffset.y;
                    lrScratch.z = lOffset.z;
                    lrScratch.w = lrTagPoint.GetScratchAmount();      // w lane = _R10[7] (mfScratchAmount)
                    ++liScratchBase;                                  // _R11 += 16B only when flag set
                }
            }
        }

        // --- (3) drive each IK part's skin --------------------------------------------------------
        // The running scratch base (v4) is NOT reset between (2) and (3): the asm carries the SAME v4
        // it left (2) with into the part walk, so the parts skin from the scratch slice that follows the
        // gathered driven-point offsets.
        if ( miNumIKBodyParts > 0 )
        {
            s32 liRunning = liScratchBase;   // continue v4 from (2) -- shared running base
            for ( s32 li = 0; li < miNumIKBodyParts; ++li )
            {
                IKBodyPart& lrPart = maIKParts[li];
                if ( maPartStates[li] != E_PART_STATE_DETATCHED )
                {
                    const s32 liType = static_cast<s32>(lrPart.GetPartType());
                    if ( liType == KI_BODY_PART_BOX_CLAMPED_A || liType == KI_BODY_PART_BOX_CLAMPED_B )
                    {
                        lrPart.UpdateSkinningOffsetsWithinBox(&maVerletOffsets_Scratch[liRunning], &lClampBox);
                    }
                    else
                    {
                        lrPart.UpdateSkinningOffsets(&maVerletOffsets_Scratch[liRunning]);
                    }
                }
                // v4 += part's GetNumberOfDrivenPoints (spec +464) -- whether or not the part skinned.
                liRunning += lrPart.GetNumberOfDrivenPoints();
            }
        }
    }

    // =============================================================================================
    // UpdateLocators @0x8260A018
    //
    // Refresh the camera / light / generic locator transforms from the current graphics-vehicle
    // transform. The asm:
    //   1) fetches the graphics-vehicle transform (SimpleVehiclePhysics::GetGraphicsVehicleTransform)
    //      and inverts it into the parent space passed to every UpdateLocator call (the vsubfp/vmrglw
    //      block builds the inverse-affine).
    //   2) walks the three locator groups in order -- GENERIC (count <= 15), LIGHT (count <= 24), CAMERA
    //      (count <= 1) -- each with a per-group max-count tripwire and a per-iteration index tripwire
    //      (luIndex < muNumLocators).
    //   3) calls UpdateLocator(transformSlot, typeSlot, &locatorSpec, parentTransform, lpPartMgr) for
    //      each locator, advancing the transform slot by 64 bytes (one Matrix44Affine) and the locator
    //      spec by 80 bytes (one LocatorPointSpec).
    // The per-group bounds, the strides + the asserts are exact.
    // FLAG (count source): the asm reads each group's live count from the SPEC's three private
    // LocatorPointSpecList members -- generic *(spec+36), light *(spec+52), camera *(spec+44) (asm
    // 7477/7513/7549). StreamedDeformationSpec exposes NO public accessor for mGenericTags/mLightTags/
    // mCameraTags and editing that shared (concurrently-touched) header here would race, so the counts
    // are read from the live mLocatorData mirror (miNumGeneric/Light/CameraLocators) instead. These are
    // the copies Prepare() seeds FROM those spec lists, so they agree at runtime; promote to the spec
    // counts when StreamedDeformationSpec grows a generic/light/camera-locator-list accessor.
    // FLAG (spec arg): the per-locator LocatorPointSpec the asm passes (*(spec+40/56/48) + 80*index)
    // comes from the same private spec lists, so the spec arg is passed as nullptr here (UpdateLocator
    // handles the no-spec case) -- promote alongside the count source above.
    // FLAG (parent transform): the asm sources the parent from the INVERTED
    // SimpleVehiclePhysics::GetGraphicsVehicleTransform() of the attached vehicle (the vsubfp/vmrglw
    // inverse-affine block @ 7428-7467; COM-adjusted), which the minimal VehiclePhysics slice does not
    // expose; the un-inverted body transform (GetTransform) is used as the parent here -- swap to the
    // inverted graphics transform when it is homed.
    // =============================================================================================
    void DeformableObject::UpdateLocators(DetachedPartManager* lpPartMgr)
    {
        // (1) parent space passed to each UpdateLocator (see FLAG above).
        Matrix44Affine lParent;
        GetTransform(lParent);

        CGS_ASSERT(mpDeformationSpec != nullptr, "mpDeformationSpec");   // line 4202 (non-gating)

        // (2a) GENERIC locators (max KI_MAX_GENERIC_LOCATORS == 15).
        const s32 liNumGeneric = mLocatorData.miNumGenericLocators;
        CGS_ASSERT(liNumGeneric <= 15, "(int32_t)luNumLocators <= KI_MAX_GENERIC_LOCATORS");  // line 4205
        for ( s32 li = 0; li < liNumGeneric; ++li )
        {
            CGS_ASSERT(static_cast<u32>(li) < static_cast<u32>(mLocatorData.miNumGenericLocators),
                       "luIndex < muNumLocators");  // BrnStreamedDeformationSpec.h:104
            UpdateLocator(mLocatorData.maGenericLocators[li], mLocatorData.maGenericLocatorTypes[li],
                          nullptr /* spec list private to StreamedDeformationSpec -- see FLAG */,
                          lParent, lpPartMgr);
        }

        // (2b) LIGHT locators (max KI_MAX_LIGHT_LOCATORS == 24).
        const s32 liNumLight = mLocatorData.miNumLightLocators;
        CGS_ASSERT(liNumLight <= 24, "(int32_t)luNumLocators <= KI_MAX_LIGHT_LOCATORS");  // line 4220
        for ( s32 li = 0; li < liNumLight; ++li )
        {
            CGS_ASSERT(static_cast<u32>(li) < static_cast<u32>(mLocatorData.miNumLightLocators),
                       "luIndex < muNumLocators");  // BrnStreamedDeformationSpec.h:104
            UpdateLocator(mLocatorData.maLightLocators[li], mLocatorData.maLightLocatorTypes[li],
                          nullptr /* spec list private to StreamedDeformationSpec -- see FLAG */,
                          lParent, lpPartMgr);
        }

        // (2c) CAMERA locators (max KI_MAX_CAMERA_LOCATORS == 1).
        const s32 liNumCamera = mLocatorData.miNumCameraLocators;
        CGS_ASSERT(liNumCamera <= 1, "(int32_t)luNumLocators <= KI_MAX_CAMERA_LOCATORS");  // line 4236
        for ( s32 li = 0; li < liNumCamera; ++li )
        {
            CGS_ASSERT(static_cast<u32>(li) < static_cast<u32>(mLocatorData.miNumCameraLocators),
                       "luIndex < muNumLocators");  // BrnStreamedDeformationSpec.h:104
            UpdateLocator(mLocatorData.maCameraLocators[li], mLocatorData.maCameraLocatorTypes[li],
                          nullptr /* spec list private to StreamedDeformationSpec -- see FLAG */,
                          lParent, lpPartMgr);
        }
    }

    // =============================================================================================
    // ClearStoredContacts @0x825BA350
    //
    // Reset every deformation sensor's post-physics scratch + stored-contact count to the canonical rest
    // state -- the same reset ClearNonWorldContacts performs at its tail, applied across all bare
    // deformation sensors:
    //   mfMaxPointDisplacement = 100.0  (sensor +280)
    //   maPostPhysicsVec0 / maPostPhysicsVec1 zeroed (sensor +288 / +304, two stvx128 0)
    //   mu32PostPhysicsReset = 0        (sensor +384)
    //   mi32NumStoredContacts = 0       (sensor +408)
    // The mpDeformationSpec tripwire (line 4336) is non-gating.
    // Caller (X360 xref): VehicleManager::ClearSnappedNetworkCarContacts / PhysicalTrafficManager.
    // =============================================================================================
    void DeformableObject::ClearStoredContacts()
    {
        CGS_ASSERT(mpDeformationSpec != nullptr, "mpDeformationSpec");   // line 4336 (non-gating)

        const s32 liNumSensors = GetNumSensors() - 4;   // *(mpDeformationSpec + 1618)
        for ( s32 li = 0; li < liNumSensors; ++li )
        {
            DeformationSensor& lrSensor = maDeformationSensors[li];
            lrSensor.mImpulseContact.mfImpactTimeInFrame = 100.0f;   // sensor +280 -- DISARM the impulse record (walls leg 4: the old overlay name mfMaxPointDisplacement was this field misnamed)
            for ( s32 lj = 0; lj < 4; ++lj )
            {
                lrSensor.maPostPhysicsVec0[lj] = 0.0f;  // sensor +288 (stvx128 0)
                lrSensor.maPostPhysicsVec1[lj] = 0.0f;  // sensor +304 (stvx128 0, r10+16)
            }
            lrSensor.mSpyContactId = 0;                 // sensor +384 -- spy reset (was mu32PostPhysicsReset)
            lrSensor.mi32NumStoredContacts = 0;         // sensor +408
        }
    }

    // [deform-bbox] NOT IN THE X360 BINARY -- host-side counters read by the opt-in [deform-bbox]
    // witness in BrnDeformableObject_BBox.cpp (BRN_DEFORM_TRACE). They count how many
    // ApplySensorImpulse calls built their six limit rows from the DRIVE-TIME pair ([0], the
    // 0x82607A50 arm) vs the CRASH pair ([1], the 0x82607A78 arm), so a run can say WHICH arm the
    // impulses of a given crash went through -- the drive-time rows bound every sensor at exactly
    // the drive-time limit, so an impulse applied through [0] can never take the deformed box past
    // it. DELETE-WHEN the wreck-vs-drive-away question is banked.
    u32 guDeformLimitRowArmApplies[2] = { 0u, 0u };

    // =============================================================================================
    // ApplySensorImpulse @0x826078B0 -- the heavy per-sensor apply.
    //
    // Build the per-direction ImpulseParams block, route it through the sensor body, then bank the
    // resolved world impulse on the vehicle body and (optionally) update the contact spy. Observable
    // flow (this == _R17, the attached vehicle physics == mVehicleBody.GetVehiclePhysics()):
    //
    //  1) Seed the ImpulseParams block (v215) from the relative-motion / impulse-dir / magnitude args
    //     and the contact id. The impulse-magnitude lane is max-folded with the relative-motion w lane.
    //  2) If the vehicle is in showtime (vehiclePhysics->IsPlayerVehicleInShowtime(), vtable +0x10 --
    //     image-settled 2026-08-09; the old role-inferred name IsIgnoringPassedOnImpulses is retired),
    //     pre-apply a showtime contact impulse (ApplyShowtimeContactImpulse) using the impulse-dir
    //     scaled by the relative motion.
    //  3) Pick the allowed-compression budget: if the handling-body-id high byte == 2
    //     (GetHandlingBodyIdHighByte(), HIBYTE(*(this+26384))) OR crashed OR showtime, use the unit
    //     budget; otherwise the FLAGGED drive-time deformation row (zeroed when drive-time deformation
    //     is disabled).
    //  4) For each of the six signed body axes: build the direction vector (the switch), project the
    //     impulse onto it; if the projection is positive, fill the per-direction ImpulseParams, validate
    //     the magnitude (the "Invalid sensor impulse magnitude" tripwire, line 1430), latch the
    //     deformation/crash flags (this +26408 / vehicle +1810 / vehicle +1809 on game-mode 2) and
    //     dispatch the impulse into the sensor body via lpSensor->ApplyLocalImpulse(&v215) (the
    //     `(**a37)(a37, v215)` CollidableBody vtable slot-0 virtual).
    //  5) After the loop, resolve the accumulated local impulse into world space
    //     (GetImpulsesFromLocalImpulse), shape it by the FLAGGED friction / showtime rows, and bank it on
    //     the vehicle body (AddWorldSpaceImpulse + AddWorldSpaceAngularImpulse). Accumulate the per-sensor
    //     scratch (sensor +420, the fsel ladder floored at 0.75).
    //  6) If lbAddToSpy, accumulate the impulse + (impulseDir x relMotion) into the spy vectors (sensor
    //     +288 / +304).
    //
    // The dense VMX projection / normalise / clamp math is modelled per-lane (the FLAGGED rows make the
    // friction shaping inert but honest); the branch structure, call order, the six-direction loop, the
    // asserts and the named flag latches are exact.
    // =============================================================================================
    void DeformableObject::ApplySensorImpulse(VecFloat lvfTimeStep, const StoredImpulseContact& lContact,
                                              const ImpulseParams& lImpulseParams, Vector3 lRelativeMotion,
                                              Vector3 lImpulseDir, VecFloat lvfImpulseMagnitude,
                                              DeformationSensor* lpSensor, bool lbAddToSpy,
                                              bool lbUseNormalScaledFriction)
    {
        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();

        // (1) seed the working ImpulseParams from the caller's block + the per-apply args. The asm
        // copies the incoming params WHOLE and then overwrites individual fields:
        //     0x826078F0  addi r3, r1, var_290    ; Dst == &lParams
        //     0x826078F8  li   r5, 0xC0           ; Size == sizeof(ImpulseParams) == 192
        //     0x82607900  mr   r4, r31            ; Src == the caller's block (arg r5)
        //     0x8260790C  bl   memcpy
        // so `var_290` IS the local params base, and every `var_XXX` cited below is (0x290 - XXX)
        // bytes into the block. Worked on a local copy so the caller's block is untouched.
        ImpulseParams lParams = lImpulseParams;
        lParams.mWorldImpulseDirection = lImpulseDir;
        lParams.mvfTimeStep            = lvfTimeStep;
        lParams.mvfImpulseMagnitude    = lvfImpulseMagnitude;   // stvx128 v120 -> params+0x10 (RAW)

        // ⭐⭐ CORRECTED 2026-08-24 (deform-land wave, P4): the vmaxfp at 0x82607944..54 is NOT a
        // fold into the params magnitude against relMotion.w (the old body's invention) -- it is
        // the SENSOR's biggest-impulse latch:
        //     0x82607944  lvx128 v0, sensor+0x10 ; vspltw v0,3      ; the latch (.w lane)
        //     0x8260794C  vmaxfp128 v0, v120(magnitude), v0
        //     0x82607950  vrlimi128 v13, v0, 1, 0 ; stvx128 -> sensor+0x10
        // i.e. mPointDisplacement_BiggestImpulseThisFrame.w = max(.w, magnitude). This is the
        // exact value CheckSensorForcesForJointDetachment @0x825C17F8 splats to decide panel/door
        // detach -- with the old local-copy fold no hit could ever detach a part.
        if ( lpSensor != nullptr )
        {
            f32& lrLatch = lpSensor->mPointDisplacement_BiggestImpulseThisFrame.w;
            if ( lvfImpulseMagnitude.x > lrLatch )
            {
                lrLatch = lvfImpulseMagnitude.x;
            }
        }

        // ⭐ 2026-08-16 (walls leg 9) -- THE +0xB4 STORE, AND THE FLAG THAT HID IT IS RETIRED.
        //     0x8260793C  lwz r10, 0x675C(r17)          ; r17 == this, 0x675C == 26460
        //     0x82607940  stw r10, var_1DC(r1)          ; var_290 - var_1DC == 0xB4
        // This TU used to call that "the v215[45] contact-id store ... an un-named member at
        // this+26460", and left it unmodelled. It is NOT un-named: BrnDeformableObject_Lifecycle.cpp's
        // ResetDeformation already names +26460 `meAbsorptionSet` (it writes E_ABSORPTIONSET_NORMAL
        // there, and E_ABSORPTIONSET_INVINCIBLE on a type-1 reset), and +0xB4 in ImpulseParams is
        // `meAbsorptionSet` on the DWARF sequence. Same offset, same name, both already in-tree.
        // ⛔ WHY IT MATTERS: ApplyCarCarImpulse never set this field, so on the car-car path the
        // absorption SET index was uninitialised stack -- an out-of-bounds read of a 5-row table by
        // GetAbsorption / GetSpeedForMaxAbsorbtion / GetProportionToSpeed and of the 5-row
        // KsaAbsorptionScale in the sensor's RecievePassedOnImpulse. (The world path set it in the
        // caller, so this is also the console's own single point of truth for both paths.)
        lParams.meAbsorptionSet = meAbsorptionSet;

        // (2) showtime pre-apply: when the vehicle's vtable+0x10 predicate is set (image-settled as
        // IsPlayerVehicleInShowtime), pre-apply a showtime contact impulse (the impulse DIRECTION
        // scaled by the impulse MAGNITUDE). Call order + gate preserved; the scaled vectors are
        // modelled per-lane.
        //
        // ⚠️ OPERAND CORRECTED 2026-08-15 (walls leg 7; surfaced by leg 6, re-verified here against
        // the asm rather than taken on trust). The vector handed to ApplyShowtimeContactImpulse is
        //     0x8260798C  vmulfp128 v1, v116, v120
        // and the prologue's four argument copies pin both registers:
        //     0x826078E4 vmr128 v117,v1 -> arg1 lvfTimeStep       0x826078EC vmr128 v119,v2 -> arg2 lRelativeMotion
        //     0x826078F4 vmr128 v116,v3 -> arg3 lImpulseDir       0x826078D4 vmr128 v120,v4 -> arg4 lvfImpulseMagnitude
        // so v116*v120 is lImpulseDir * lvfImpulseMagnitude -- a direction times a magnitude, i.e. an
        // impulse. The old body computed vpu::Mult(lImpulseDir, lRelativeMotion) (v116 * v119), which
        // is the wrong operand and dimensionally not an impulse at all; its banner said so in words
        // too. Dead on the junkyard path (showtime never fires there) but wrong wherever it does.
        //
        // ⚠️ CALL CORRECTED 2026-08-14 (walls wave -- this TU's FIRST COMPILE; the old 3-arg call
        // predates the 2026-08-02 C09 signature correction and never built). The X360 site
        // 0x82607978..0x8260799C is the INLINED VehicleRigidBody::ApplyShowtimeContactImpulse
        // (PS3 keeps it out-of-line @0x6E1160: (ImpulseParams*, Vector3, bool)) expanding to the
        // 5-arg VehiclePhysics handler, argument for argument:
        //     v1 = vmulfp128 v116,v120          -> lImpulseDir * lRelativeMotion (unchanged)
        //     r4 = li 0                          -> leImpulseSpace  = WORLD_SPACE
        //     v2 = lvx var_270 == lParams+0x20   -> lParams.mImpulsePosition (NOT lContact.mPointOnA)
        //     r5 = lwz var_240 == lParams+0x50   -> lParams.mePositionSpace (var_270-var_240 == 0x30
        //                                           == the two fields' spacing in ImpulseParams)
        //     r6 = lbz 0xB8(params)              -> mbWorldContact (+0xB8 == its console seat; the
        //                                           PS3 wrapper's arg is even NAMED lbIsWorldImpulse)
        const bool lbIgnoringPassedOn = (lpVehicle != nullptr) && lpVehicle->IsPlayerVehicleInShowtime();
        if ( lbIgnoringPassedOn )
        {
            // 0x8260798C  vmulfp128 v1, v116, v120  ==  lImpulseDir * lvfImpulseMagnitude
            const Vector3 lShowtimeImpulse = vpu::Mult(lImpulseDir, lvfImpulseMagnitude.x);
            lpVehicle->ApplyShowtimeContactImpulse(lShowtimeImpulse,
                                                   rw::physics::WORLD_SPACE,
                                                   lParams.mImpulsePosition,
                                                   lParams.mePositionSpace,
                                                   lParams.mbWorldContact);
        }

        // The vehicle's crashed byte (asm: `*(vehicle + 1808)`), read separately from the vtable+0x10
        // predicate above -- it selects both the budget branch and the suspension-limit displacement
        // source. IsCrashing() is the homed +1808 accessor.
        const bool lbCrashed = (lpVehicle != nullptr) && lpVehicle->IsCrashing();

        // (3) allowed-compression budget. The game-mode selector reads HIBYTE(*(this+26384)) == the
        // handling-body-id high byte (asm 1606), NOT the +26392 game-mode word -- use
        // GetHandlingBodyIdHighByte() (see header note).
        //
        // ⭐⭐⭐ CORRECTED 2026-08-15 (walls leg 8). This was a TWO-way select writing honest zeros in
        // the else arm. The X360 (@0x826079E0..0x82607A34) is a **THREE-way** select, and the arm the
        // shipped game actually takes was the missing one:
        //     0x826079E4  lbz   r10, kbAllowDriveTimeDeformation (0x82F2A346)
        //     0x826079F4  lvx128 v0, &unk_82FB9520          ; the drive-time deformation row
        //     0x826079FC  stvx128 v0 -> params +0x90        ; mvfAllowedCompressionFactor
        //     0x82607A04  stvx128 v0 -> params +0xA0        ; mvfMaximumAllowedAbsorption
        //     0x82607A08  bne  -> keep the row              ; flag SET  -> the row
        //     0x82607A0C  stvx128 v125 (== vspltisw128 0)   ; flag CLEAR -> zeros
        // ⚠️⚠️ SO THE OLD "HONEST ZERO" WAS THE **OTHER ARM'S** VALUE. It looked defensible precisely
        // because zero really is what the console writes when drive-time deformation is OFF -- but the
        // shipped image has that byte SET (0x82F2A346 == 0x01), so the live path is the 0.2 row, and
        // this build was running the deformation-disabled branch unconditionally.
        // ⛔ AND ZERO IS NOT THIS TERM'S IDENTITY: mvfMaximumAllowedAbsorption is a min() clamp on the
        // absorption fraction AND the base of DeformationSensor::ApplyLocalImpulse's powf, so a zero
        // made the absorbed impulse identically zero -- i.e. an ordinary world contact banked NO
        // momentum, which is exactly the "car drives through walls" symptom. mvfAllowedCompressionFactor
        // multiplies the compression limit, so a zero also removed every sensor's room to dent.
        // ⭐ RECOVERED FROM THE INITIALISER, not guessed: unk_82FB9520 is dynamic-init (it reads 0.0
        // straight out of the image, like the AbsorptionTable rows); its initialiser @0x82C5D818..
        // 0x82C5D83C loads flt_82004744 (== 0.2), splats it and stores it.
        if ( GetHandlingBodyIdHighByte() == KU_GAMEMODE_BOUNCE_ELIGIBLE || lbCrashed || lbIgnoringPassedOn )
        {
            // vcfsx(vspltisw 1, 0) == 1.0 @0x82607A20 -- crash / showtime / bounce-eligible gets the
            // whole budget.
            lParams.mvfAllowedCompressionFactor = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
            lParams.mvfMaximumAllowedAbsorption = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
        else if ( KB_ALLOW_DRIVE_TIME_DEFORMATION )
        {
            lParams.mvfAllowedCompressionFactor = KVF_DRIVE_TIME_DEFORMATION;
            lParams.mvfMaximumAllowedAbsorption = KVF_DRIVE_TIME_DEFORMATION;
        }
        else
        {
            // v125 == vspltisw128 0 (@0x82607918) -- drive-time deformation disabled.
            lParams.mvfAllowedCompressionFactor = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
            lParams.mvfMaximumAllowedAbsorption = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        // ⭐⭐ 2026-08-16 (walls leg 9) -- THE SIX PER-DIRECTION LIMIT ROWS (+0x40 mLimitVector).
        // The console builds a SIX-ROW stack table immediately before the loop and stores row[dir]
        // into the params block on every iteration. Without it mLimitVector was uninitialised stack
        // for every apply, and it is read by DeformationSensor::ApplyLocalImpulse @0x825E13E0..
        // 0x825E1410 (`dot3(mLimitVector - sphereCentre, hitDir)`) to bound how far a sensor may
        // travel -- i.e. garbage went straight into the compression room, and through it into the
        // per-contact sensor displacement.
        //   0x82607A44  lbz  r10, 0x710(vehicle)   ; IsCrashing() selects the source pair
        //   -- NOT crashing (0x82607A50..0x82607A74):
        //        lwz r11, 0x19C(lpSensor) ; lvx v0 ; vspltw v13, v0, 3   ; the sensor sphere RADIUS
        //        lvx v12, this+0x66F0                                    ; mDriveTimeBBoxLimitMin
        //        lvx v0,  this+0x6700                                    ; mDriveTimeBBoxLimitMax
        //        vsubfp v0, v0, v13   ->  Max - radius      (the ODD rows)
        //        vaddfp v13, v12, v13 ->  Min + radius      (the EVEN rows)
        //   -- crashing (0x82607A78..0x82607AA4): the same shape with the radius padded by 0.5
        //        (vcfsx(vspltisw 1, 1) == 0.5) and the pair taken from vehicle+0x6D0 / vehicle+0x6E0.
        //   0x82607AC4/AE4/AEC/AF4/AFC/B04  stvx v13,v0,v13,v0,v13,v0 -> var_1D0..var_180
        //     i.e. rows {+X,-X,+Y,-Y,+Z,-Z} alternate POSITIVE-limit / NEGATIVE-limit, which is
        //     exactly the ENextSensorDirection order KA_IMPULSE_DIRECTIONS uses.
        // ⭐⭐ CORRECTED 2026-09-02 (deformation wave): the CRASHING arm's pair IS named --
        // vehicle+0x6D0/+0x6E0 is SimpleVehiclePhysics::mDeformableAABB (min @+0, max @+0x10), the
        // very box UpdateDeformedBBox @0x825E0D20 accumulates from the sensors' CURRENT spheres and
        // stores back every frame. The old FLAG fell back to the DRIVE-TIME pair here, and that is
        // load-bearing: ApplyLocalImpulse bounds a sensor's travel by dot3(mLimitVector - centre,
        // hitDir), so with the drive-time rows a sensor stops EXACTLY at the drive-time limit (row =
        // limit -/+ radius) and the deformed box can never exceed it by more than the +0.5 pad. The
        // console's crash rows are measured from the current DEFORMED box instead -- 0.5 m inside
        // its far face -- so in a crash the box constraint effectively re-arms every apply and the
        // per-direction compression limits (spec x scale[set] x 1.0) are what bound the dent.
        //     0x82607A78  vspltisw v0, 1 ; vcfsx v0, v0, 1        ; 0.5
        //     0x82607A7C  lwz r10, 0x19C(r30) ; lvx128 v13 ; vspltw v13, v13, 3   ; radius
        //     0x82607A94  lvx128 v11, r11(vehicle), 0x6D0       ; mDeformableAABB.mMin
        //     0x82607A98  lvx128 v12, r11(vehicle), 0x6E0       ; mDeformableAABB.mMax
        //     0x82607A9C  vaddfp v13, v13, v0                   ; radius + 0.5
        //     0x82607AA0  vsubfp v0, v12, v13                   ; max - (r + 0.5)  (ODD rows)
        //     0x82607AA4  vaddfp v13, v11, v13                  ; min + (r + 0.5)  (EVEN rows)
        Vector3 laLimitRows[KI_NUM_APPLY_DIRECTIONS];
        {
            // `lwz r11, 0x19C(lpSensor) ; lvx v0, r11 ; vspltw v13, v0, 3` -- the sensor's LOCAL
            // sphere, w lane == the radius.
            const CgsGeometric::Sphere* lpLocalSphere =
                ( lpSensor != nullptr ) ? lpSensor->GetLocalSpaceSphere() : nullptr;
            const f32 lfSphereRadius = ( lpLocalSphere != nullptr )
                                     ? lpLocalSphere->mPositionRadius.w : 0.0f;
            const f32 lfPad    = lbCrashed ? 0.5f : 0.0f;          // vcfsx(vspltisw 1,1) == 0.5
            const f32 lfRadius = lfSphereRadius + lfPad;
            ++guDeformLimitRowArmApplies[lbCrashed ? 1 : 0];       // [deform-bbox] arm counter (DIAG)
            const Vector3& lrDriveMin = GetDriveTimeLimitsMin();   // this + 0x66F0
            const Vector3& lrDriveMax = GetDriveTimeLimitsMax();   // this + 0x6700
            const Vector3 lDeformedMin = ( lbCrashed && lpVehicle != nullptr )
                ? Vector3{ lpVehicle->GetDeformableAABB().mMin.x, lpVehicle->GetDeformableAABB().mMin.y,
                           lpVehicle->GetDeformableAABB().mMin.z, lpVehicle->GetDeformableAABB().mMin.w }
                : lrDriveMin;                                       // vehicle + 0x6D0
            const Vector3 lDeformedMax = ( lbCrashed && lpVehicle != nullptr )
                ? Vector3{ lpVehicle->GetDeformableAABB().mMax.x, lpVehicle->GetDeformableAABB().mMax.y,
                           lpVehicle->GetDeformableAABB().mMax.z, lpVehicle->GetDeformableAABB().mMax.w }
                : lrDriveMax;                                       // vehicle + 0x6E0
            const Vector3& lrMin = lDeformedMin;
            const Vector3& lrMax = lDeformedMax;
            const Vector3 lPositive = { lrMin.x + lfRadius, lrMin.y + lfRadius,
                                        lrMin.z + lfRadius, lrMin.w + lfRadius };   // vaddfp v13
            const Vector3 lNegative = { lrMax.x - lfRadius, lrMax.y - lfRadius,
                                        lrMax.z - lfRadius, lrMax.w - lfRadius };   // vsubfp v0
            for ( s32 liRow = 0; liRow < KI_NUM_APPLY_DIRECTIONS; ++liRow )
            {
                laLimitRows[liRow] = ( (liRow & 1) == 0 ) ? lPositive : lNegative;
            }
        }

        // ⭐ The world-space body axes the projection below is taken against: the console loads the
        // vehicle transform's three rotation rows ONCE, above the loop, and rotates the selected
        // local axis by them inside it.
        //   0x82607910  lwz r10, 0x194C(this)   ; the attached VehiclePhysics
        //   0x8260791C  addi r10, r10, 0x10     ; -> mTransform
        //   0x82607930  lvx128 v124, r0,  r10   ; row 0 (+0x10)
        //   0x82607928  lvx128 v123, r10, 0x10  ; row 1 (+0x20)
        //   0x82607938  lvx128 v122, r10, 0x20  ; row 2 (+0x30)
        Matrix44Affine lBodyTransform;
        if ( lpVehicle != nullptr ) { lBodyTransform = lpVehicle->GetTransform(); }
        else                       { lBodyTransform.SetIdentity(); }

        // ⭐ v126 == the IMPULSE VECTOR: `vmulfp128 v126, v116, v120` @0x82607AB4, where the prologue
        // pinned v116 == lImpulseDir (arg 5) and v120 == lvfImpulseMagnitude (arg 6). Same product
        // leg 7 recovered for the showtime pre-apply -- a direction times a magnitude.
        const Vector3 lImpulseVector = vpu::Mult(lImpulseDir, lvfImpulseMagnitude.x);

        ++gChainProbe.muSensorImpulse;   // [chain] probe -- ApplySensorImpulse reached block (4)

        // (4) the six-direction apply loop. Accumulates the local impulse the body banks in (5).
        Vector3 lAccumulatedLocalImpulse = { 0.0f, 0.0f, 0.0f, 0.0f };
        for ( s32 liDir = 0; liDir < KI_NUM_APPLY_DIRECTIONS; ++liDir )
        {
            // direction vector (the switch 0x82607BAC): the six signed unit BODY axes, verbatim the
            // shared KA_IMPULSE_DIRECTIONS table (case 0/2/4 load a row, case 1/3/5 load it and xor
            // the sign bit -- `vspltisw v0,-1 ; vslw v0,v0,v0 ; vxor`).
            const Vector3 lDirVec = KsaApplyDirection[liDir];

            // ⭐⭐⭐ 2026-08-16 (walls leg 9) -- THE PROJECTION IS MAGNITUDE-BEARING, AND IT IS WRITTEN
            // BACK INTO THE PARAMS BLOCK. The console:
            //   0x82607C30  vspltw v12, v0, 0 / v10 = splat(y) / v0 = splat(z)   ; the local axis lanes
            //   0x82607C4C  vmaddfp128 v9,   v124, v12, v9      ; v9  = row0 * axis.x
            //   0x82607C60  vmaddfp128 v9,   v123, v10, v9      ; v9 += row1 * axis.y
            //   0x82607C68  vmaddfp128 v127, v122, v0,  v127    ; v127 = v9 + row2 * axis.z
            //                                                   ; == the axis rotated into WORLD
            //   0x82607C6C  vmsum3fp128 v0, v126, v127          ; dot3(impulseVector, worldAxis)
            //   0x82607C70  stvx128 v0, r0, var_280             ; ** lParams.mvfImpulseMagnitude **
            //   0x82607C74  vcmpgtfp128. v0, v0, v125           ; the >0 skip test uses the SAME value
            // ⚠️⚠️ The tree used to compute `dot(lImpulseDir, localAxis)` -- a DIMENSIONLESS cosine in
            // [0,1] -- and never stored it, so the sensor was handed the caller's whole scalar
            // magnitude (leg 8's probe printed `mag 733.59` on every one of up to six directions
            // instead of that direction's component). Since VehicleRigidBody::ApplyImpulseToVehicle
            // banks `KA_IMPULSE_DIRECTIONS[dir] * mvfImpulseMagnitude`, this field IS the number the
            // wall eventually receives.
            // ⭐ INDEPENDENT CORROBORATION that the magnitude belongs here: block (5) shapes the
            // accumulated impulse by KVF_APPLY_FRICTION_SCALE == 1.5e-4 and clamps it at
            // KVF_APPLY_FRICTION_CLAMP == 1000. A clamp at 1000 can only ever bind on a quantity in
            // the hundreds -- a real impulse -- and never on a unit cosine.
            //   ⚠️ vmaddfp128 note: for the VMX128 three-register form the duplicated field is the
            //   ADDEND (vB == vD), so IDA's `vmaddfp128 vD, vA, vC, vB(=vD)` is vD = vA*vC + vD -- a
            //   destructive accumulate. That is NOT leg 8's plain-`vmaddfp` rule ("print position 2
            //   is the addend"), which still holds for the four-distinct-operand form; read the two
            //   separately. Here only the accumulate reading yields a matrix rotation at all.
            const Vector3 lWorldAxis = vpu::TransformVector(lBodyTransform, lDirVec);
            const f32 lfProjection = vpu::Dot(lImpulseVector, lWorldAxis);
            lParams.mvfImpulseMagnitude =
                VecFloat{ lfProjection, lfProjection, lfProjection, lfProjection };   // 0x82607C70

            ++gChainProbe.muDirsTried;   // [chain] probe
            if ( lfProjection <= 0.0f )   // vcmpgtfp against zero -- skip non-positive directions
            {
                continue;
            }
            ++gChainProbe.muDirsPassed;   // [chain] probe

            // per-direction params: the direction index (`stw r30, var_290` @0x82607C8C) and this
            // direction's limit row (`lvx128 v11, dir*16(var_1D0) ; stvx128 v11, var_250`
            // @0x82607C50/0x82607C58). The friendly-fire / double-bounce displacement scaling
            // (other car +26414 / vehicle crashed -> the FLAGGED double-bounce damp row) folds in
            // here; all rows are FLAGGED-0, so the shaping is inert.
            lParams.meImpulseDirection = static_cast<ENextSensorDirection>(liDir);
            lParams.mLimitVector       = laLimitRows[liDir];   // 0x82607C50 / 0x82607C58

            // magnitude validation tripwire (line 1430) -- non-gating. The asm self-compares the shaped
            // magnitude vector (vcmpeqfp.) to catch a NaN, then streams the real diagnostic whose leading
            // literal is "Invalid sensor impulse magnitude:\nlfImpulseMagnitude = " (asm 1856/1907-1911);
            // the per-value AppendFormat tail is the streamed diagnostic body, not part of the condition.
            CGS_ASSERT(lfProjection == lfProjection,
                       "Invalid sensor impulse magnitude:\nlfImpulseMagnitude = ");   // line 1430

            // latch the deformation flags (0x82607F1C..0x82607F44): this deformed this frame
            // (this +26408); the owning body has STARTED DEFORMING (`stb 1, 0x712(mpVehicle)`);
            // and, when the owner byte of mGlobalEntityId (+26384) is TRAFFIC_VEHICLE (2), the
            // body has STARTED FATALLY CRASHING (`stb 1, 0x711(mpVehicle)`) -- on the console a
            // traffic car that takes ANY sensor impulse is a wreck.
            // ⭐ 2026-09-02 (traffic-deformation wave): the two body latches were "documented,
            // not poked" here -- a silent drop. MEASURED (run tdef_r2): 526 [impulse] applies
            // into a traffic car, mbIsDeforming 0 on every PhysicalTrafficState it published,
            // so RenderTrafficCar's deforming arm (constant 22 = the live skin block) never ran
            // and no traffic dent could reach the mesh. Both live now, by name.
            mbHasDeformedThisFrame = true;   // this +26408 (deformed-this-frame latch)
            lpVehicle->SetStartedDeforming();                                   // vehicle +0x712
            if ( GetHandlingBodyIdHighByte() == BrnWorld::E_ENTITYTYPE_TRAFFIC_VEHICLE )   // HIBYTE(this+26384) == 2
            {
                lpVehicle->SetStartedFatallyCrashing();                         // vehicle +0x711
            }

            // dispatch into the sensor body: lpSensor->ApplyLocalImpulse(&v215). CollidableBody vtable
            // slot 0 (the `(**a37)(a37, v215)` indirect call).
            if ( lpSensor != nullptr )
            {
                ++gChainProbe.muDispatched;   // [chain] probe
                // [T5-sens] DIAG. NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. Tag the dispatch with
                // this object's owner byte + global index for the sensor-side [impulse] probe.
                BrnPhysics::Vehicle::gT5ApplyOwner  = static_cast<s32>(GetHandlingBodyIdHighByte());
                BrnPhysics::Vehicle::gT5ApplyGlobal = static_cast<s32>((GetGlobalEntityId().muValue >> 10) & 0x3FFFu);
                lpSensor->ApplyLocalImpulse(&lParams);
                BrnPhysics::Vehicle::gT5ApplyOwner  = -1;
                BrnPhysics::Vehicle::gT5ApplyGlobal = -1;
            }
            else
            {
                ++gChainProbe.muNullSensor;   // [chain] probe
            }

            // accumulate the applied local impulse (the body banks the sum in (5)).
            lAccumulatedLocalImpulse.x += lDirVec.x * lfProjection;
            lAccumulatedLocalImpulse.y += lDirVec.y * lfProjection;
            lAccumulatedLocalImpulse.z += lDirVec.z * lfProjection;
        }

        // (5) ⭐⭐ REWRITTEN 1:1 2026-08-24 (deform-land wave, P4) from the asm
        // 0x82607F80..0x8260835C. The previous body banked the ACCUMULATED per-direction impulse
        // shaped by the 1.5e-4 / 1000 / 5.0 rows -- a mis-placement: those rows belong to the
        // SENSOR SCRATCH ladder at the tail. What the console banks is a TANGENTIAL shove:
        //   scale-row select (the second IsPlayerVehicleInShowtime consult):
        //     not showtime                          -> impulseRow = 0.0, forceRow = 0.0
        //     showtime && crashed && worldContact   -> impulseRow = 5.0 (unk_82FB8060),  forceRow = 0.9 (unk_82FB7F50)
        //     showtime && crashed && other crashed  -> impulseRow = 20.0 (unk_82FB8300), forceRow = 0.9
        //     showtime && crashed && neither        -> impulseRow = 0.0,                 forceRow = 0.9
        //     showtime && !crashed                  -> both 0.0
        //     (all five rows recovered from their static-init writers 0x82C5D5C0..0x82C5D650;
        //      unk_82FB9D40 -- the not-showtime row -- initialises to 0.0 DELIBERATELY: ordinary
        //      driving banks its wall momentum through the RecievePassedOnImpulse ->
        //      ApplyWallContactImpulse chain instead, and this block contributes only SCRATCH.)
        //   tangential = relMotion - normal * dot3(relMotion, normal)          (vsubfp v123)
        //   gate: |tangential|^2 >= 1e-4 (unk_82FB9720, init 0x82C5D840) else skip to the spy;
        //   dir = -tangential / |tangential| (zero-guarded rsqrt)              (v13)
        //   scaledMag = magnitude * clamp(|tangential| * 1.0, 0, 1)            (v120; the 1.0s are
        //     the lazy first-call cache pair unk_82FBA240/unk_82FBA230 == vcfsx(1) and its refined
        //     reciprocal -- both exactly 1.0, folded here)
        //   bankLocal = dir * scaledMag * impulseRow                           (v118)
        //   GetImpulsesFromLocalImpulse(bankLocal * timestep, space0, mPointOnA, space0, &li, &ai)
        //     (BOTH space tags are the asm's literal `li r4,0 ; li r5,0` -- the tangential motion
        //      and the contact point are already world-space; the old BODY_SPACE guess is retired)
        //   CAR-CAR ATTRIB SCALE (previously MISSING): when !mbWorldContact both outputs are
        //     scaled by mpAttribs->mCollisionAttribs (+0x280) lane .y == CarAngularImpulseScale
        //     (0x826081D0..0x82608208);
        //   AddWorldSpaceImpulse + AddWorldSpaceAngularImpulse;
        //   ADDLOCALFORCE LEG (previously MISSING, 0x82608240..0x826082C0): when mbWorldContact,
        //     AddLocalForce( -tangentialDir * |tangential| * bodyMass(+0xE0 == body mfMass) *
        //     forceRow );
        //   SCRATCH LADDER (0x826082C4..0x82608358 -- where the three rows actually live):
        //     add = |tangential| * timestep * 1.5e-4 (unk_82FB8330) * min(scaledMag, 1000
        //           (unk_82FB95C0)) [* 5.0 (unk_82FB9D30) when crashed]
        //     scratch = min( max(0, scratch + add), max(0.75, scratch) )       (the fsel ladder)
        if ( lpVehicle != nullptr )
        {
            // scale-row select. The second vtable consult repeats the (2) predicate.
            Vector3 lImpulseRow = { 0.0f, 0.0f, 0.0f, 0.0f };   // unk_82FB9D40 == splat(0.0)
            Vector3 lForceRow   = { 0.0f, 0.0f, 0.0f, 0.0f };
            if ( lbIgnoringPassedOn && lbCrashed )
            {
                lForceRow = KVF_APPLY_LOCAL_FORCE_SCALE;   // unk_82FB7F50 = splat(0.9)
                if ( lParams.mbWorldContact )
                {
                    lImpulseRow = KVF_APPLY_WORLD_CRASH_SCALE;      // unk_82FB8060 = splat(5.0)
                }
                else if ( lContact.mpOtherVehicle != nullptr
                          && lContact.mpOtherVehicle->GetVehiclePhysics() != nullptr
                          && lContact.mpOtherVehicle->GetVehiclePhysics()->IsCrashing() )
                {
                    lImpulseRow = KVF_APPLY_CARCAR_CRASH_SCALE;     // unk_82FB8300 = splat(20.0)
                }
            }

            // tangential relative motion (vsubfp v123).
            const f32     lfAlongNormal = vpu::Dot(lRelativeMotion, lContact.mNormal);
            const Vector3 lTangential = vpu::Subtract(
                lRelativeMotion,
                Vector3{ lContact.mNormal.x * lfAlongNormal, lContact.mNormal.y * lfAlongNormal,
                         lContact.mNormal.z * lfAlongNormal, 0.0f });
            const f32 lfTangentialSq = vpu::Dot(lTangential, lTangential);

            Vector3 lBankLocal  = { 0.0f, 0.0f, 0.0f, 0.0f };   // v118 (0 when the gate skips)
            Vector3 lLocalForce = { 0.0f, 0.0f, 0.0f, 0.0f };   // v125 on the skip / non-force paths
            f32     lfScaledMag = lvfImpulseMagnitude.x;        // v120 (raw when the gate skips)

            if ( lfTangentialSq >= KVF_APPLY_MIN_TANGENTIAL_SQ.x )   // unk_82FB9720 = splat(1e-4)
            {
                const f32 lfTangentialLen = std::sqrt(lfTangentialSq);   // zero-guarded by the gate
                const Vector3 lNegDir = { -lTangential.x / lfTangentialLen,
                                          -lTangential.y / lfTangentialLen,
                                          -lTangential.z / lfTangentialLen, 0.0f };

                // scaledMag = magnitude * clamp(|tangential|, 0, 1)  (the lazy-cache 1.0 factors fold).
                f32 lfClamp = lfTangentialLen;
                if ( lfClamp > 1.0f ) { lfClamp = 1.0f; }
                if ( lfClamp < 0.0f ) { lfClamp = 0.0f; }
                lfScaledMag = lvfImpulseMagnitude.x * lfClamp;

                lBankLocal = vpu::Mult(lNegDir, lfScaledMag);
                lBankLocal = Vector3{ lBankLocal.x * lImpulseRow.x, lBankLocal.y * lImpulseRow.y,
                                      lBankLocal.z * lImpulseRow.z, 0.0f };

                const Vector3 lBankImpulse = vpu::Mult(lBankLocal, lvfTimeStep.x);   // v1 = v118 * v117

                Vector3 lWorldImpulse        = { 0.0f, 0.0f, 0.0f, 0.0f };
                Vector3 lWorldAngularImpulse = { 0.0f, 0.0f, 0.0f, 0.0f };
                lpVehicle->GetImpulsesFromLocalImpulse(lBankImpulse,
                                                       static_cast<rw::physics::InputSpace>(0),
                                                       lContact.mPointOnA,
                                                       static_cast<rw::physics::InputSpace>(0),
                                                       &lWorldImpulse, &lWorldAngularImpulse);

                // car-car attrib scale (previously missing).
                if ( !lParams.mbWorldContact && lpVehicle->GetAttribs() != nullptr )
                {
                    const f32 lfCarScale = lpVehicle->GetAttribs()->mCollisionAttribs
                        .mvCrashSpeedMPS_CarAngularImpulseScale_Spare_Spare.y;
                    lWorldImpulse        = vpu::Mult(lWorldImpulse, lfCarScale);
                    lWorldAngularImpulse = vpu::Mult(lWorldAngularImpulse, lfCarScale);
                }

                ExternalPhysicsBody& lBody = GetVehicleBody();
                lBody.AddWorldSpaceImpulse(lWorldImpulse);
                lBody.AddWorldSpaceAngularImpulse(lWorldAngularImpulse);

                // AddLocalForce leg (previously missing): world contacts shove the body along
                // -tangential, scaled by the body mass row and the 0.9 force row. The asm's call
                // carries the same `li r4,0 ; li r5,0` space pair + mPointOnA as the impulse
                // resolve above (0x82608254..0x826082C0).
                if ( lParams.mbWorldContact )
                {
                    const f32 lfMass = lBody.GetMass().x;   // vehicle+0xE0 row (body mfMass)
                    lLocalForce = Vector3{
                        lNegDir.x * lfTangentialLen * lfMass * lForceRow.x,
                        lNegDir.y * lfTangentialLen * lfMass * lForceRow.y,
                        lNegDir.z * lfTangentialLen * lfMass * lForceRow.z, 0.0f };
                    lBody.AddLocalForce(lLocalForce,
                                        static_cast<rw::physics::InputSpace>(0),
                                        lContact.mPointOnA,
                                        static_cast<rw::physics::InputSpace>(0));
                }

                // the sensor scratch ladder -- THE home of the 1.5e-4 / 1000 / 5.0 rows.
                if ( lpSensor != nullptr )
                {
                    f32 lfMagClamped = lfScaledMag;
                    if ( lfMagClamped > KVF_APPLY_FRICTION_CLAMP.x )   // unk_82FB95C0 = 1000
                    {
                        lfMagClamped = KVF_APPLY_FRICTION_CLAMP.x;
                    }
                    f32 lfAdd = lfTangentialLen * lvfTimeStep.x
                              * KVF_APPLY_FRICTION_SCALE.x            // unk_82FB8330 = 1.5e-4
                              * lfMagClamped;
                    if ( lbCrashed )
                    {
                        lfAdd *= KVF_APPLY_SHOWTIME_SCALE.x;          // unk_82FB9D30 = 5.0
                    }
                    const f32 lfScratch = lpSensor->mfScratchAmount;
                    f32 lfNew = lfScratch + lfAdd;
                    if ( lfNew < 0.0f ) { lfNew = 0.0f; }                       // fsel vs -f0
                    const f32 lfCap = (lfScratch > 0.75f) ? lfScratch : 0.75f;  // max(0.75, scratch)
                    lpSensor->mfScratchAmount = (lfNew < lfCap) ? lfNew : lfCap;
                }
            }

            // (6) spy accumulation (runs on the gate-skip path too, with zero bank/force --
            // 0x82608360 falls into this block).
            if ( lbAddToSpy && lpSensor != nullptr )
            {
                lpSensor->maPostPhysicsVec0[0] += lLocalForce.x + lBankLocal.x;   // v125 + v118
                lpSensor->maPostPhysicsVec0[1] += lLocalForce.y + lBankLocal.y;
                lpSensor->maPostPhysicsVec0[2] += lLocalForce.z + lBankLocal.z;

                // sensor +0x130 += impulseDir * (scaled) magnitude (vmaddfp v116 * v120).
                lpSensor->maPostPhysicsVec1[0] += lImpulseDir.x * lfScaledMag;
                lpSensor->maPostPhysicsVec1[1] += lImpulseDir.y * lfScaledMag;
                lpSensor->maPostPhysicsVec1[2] += lImpulseDir.z * lfScaledMag;
            }
        }

        (void)lAccumulatedLocalImpulse;    // the loop's accumulator: consumed only through the
                                           // per-direction sensor dispatch above (the old bank
                                           // of this sum was the P4 mis-placement)
        (void)lbUseNormalScaledFriction;
        (void)KF_APPLY_EPSILON;
    }

    // =============================================================================================
    // RenderSensors @0x825E08C0 -- debug-draw every sensor sphere + the inter-sensor links. const.
    //
    // For each of the bare deformation sensors the asm:
    //   * transforms the sensor's local sphere centre by the vehicle body transform (body +16 rows) into
    //     world space and pulls the sphere radius (vspltw lane 3),
    //   * if the sensor is its own primary (the v13 == v11 self-link test) draws the six inter-sensor
    //     link lines (DrawLine) to its linked sensors, then a white box (DrawBox, colour -1) sized by the
    //     radius,
    //   * otherwise colours the box by a 0..0.2 displacement ramp ((0.2 - dist) * 5 * 255 | 0xFF000000)
    //     and draws it.
    // After the sensor loop it draws the four wheel sensors (j in 0..3) as green boxes + green links
    // (DrawBox / DrawLine, colour -16711936).
    //
    // FLAGGED-DEFERRED: CgsDev::Debug3DImmediateRender is only forward-declared in the frozen header, so
    // DrawBox / DrawLine cannot be reached BY NAME without a fabricated declaration. The recoverable
    // outer flow (the per-sensor world-centre transform + the six-link / four-wheel loop bounds) is
    // reproduced; the draw emission is left as a documented gap (no fabricated callee signatures).
    // Caller (X360 xref): DeformationDebugComponent::RenderWorld.
    // =============================================================================================
    void DeformableObject::RenderSensors(CgsDev::Debug3DImmediateRender* lpRender, s32 liFlags) const
    {
        const s32 liNumSensors = const_cast<DeformableObject*>(this)->GetNumSensors() - 4;

        // body transform rows the local sensor centres are projected through (mVehicleBody body +16).
        Matrix44Affine lBodyTransform;
        GetTransform(lBodyTransform);   // const accessor

        // The asm loop counter v13 IS the sensor index, bounded by *(spec+1618) == mu8NumDeformationSensors
        // (== liNumSensors). Each iteration tests whether the sensor is its own primary link (v13 == v11):
        // the self-link branch draws the six inter-sensor link lines + a white box; the else (non-primary)
        // branch fires the index tripwire then draws a displacement-ramp-coloured box. FLAG: the v11
        // primary-link selector is an un-homed early arg-derived value with no accessor on the minimal
        // slice -- the self-link predicate is carried as a documented gap, so the else branch (and its
        // restored assert) is taken for every sensor here. The DrawBox / DrawLine emissions stay
        // FLAGGED-DEFERRED (see header).
        for ( s32 li = 0; li < liNumSensors; ++li )
        {
            // world-space sphere centre + radius (the modelled affine transform of the local centre)
            // would feed the DrawBox / DrawLine calls; those calls are FLAGGED-DEFERRED (see header).
            const DeformationSensor& lrSensor = maDeformationSensors[li];
            (void)lrSensor;

            // else-branch (sensor is not its own primary link) index tripwire -- asm 5519-5527.
            // Non-gating; li is the sensor index bounded by mu8NumDeformationSensors (== liNumSensors).
            CGS_ASSERT(li < liNumSensors,
                       "liSensorIndex < mu8NumDeformationSensors");  // BrnStreamedDeformationSpec.h:201
        }

        // four wheel sensors drawn green (DrawBox / DrawLine, colour -16711936) -- FLAGGED-DEFERRED.
        for ( s32 lj = 0; lj < 4; ++lj )
        {
            (void)lj;
        }

        (void)lpRender;
        (void)liFlags;
        (void)lBodyTransform;
    }

    // =============================================================================================
    // UpdateOutputContactSpies @0x826251E8
    //
    // Push each deformation sensor's contact spy into the output buffer. For each of the bare deformation
    // sensors the asm calls
    //   DeformationSensor::OutputContactSpy(sensor, lpOutput, lpContacts, this->mu32GameModeState)
    // -- the spy carries this car's game-mode/state word (this +26392) so the consumer can attribute the
    // contact. The sensor stride is 432 bytes (the maDeformationSensors[] stride); the loop bound is the
    // live sensor count.
    //
    // FLAGGED-DEFERRED: DeformationSensor::OutputContactSpy is an out-of-tree callee with no declaration
    // on the homed DeformationSensor, so it cannot be reached BY NAME without a fabricated declaration.
    // The recoverable outer flow (the per-sensor loop bound + the game-mode-word source) is reproduced;
    // the per-sensor spy push is left as a documented gap (no fabricated callee declaration).
    // Caller (X360 xref): DeformableObject::Update.
    // =============================================================================================
    void DeformableObject::UpdateOutputContactSpies(CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                                    BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContacts)
    {
        const s32 liNumSensors = GetNumSensors() - 4;   // *(mpDeformationSpec + 1618)
        const u32 luGameModeState = mu32GameModeState;  // this +26392 -- carried into each spy record

        for ( s32 li = 0; li < liNumSensors; ++li )
        {
            DeformationSensor& lrSensor = maDeformationSensors[li];
            // DeformationSensor::OutputContactSpy(&lrSensor, lpOutput, lpContacts, luGameModeState)
            // -- FLAGGED-DEFERRED (callee not declared on the homed sensor; see header).
            (void)lrSensor;
        }

        (void)lpOutput;
        (void)lpContacts;
        (void)luGameModeState;
    }

    // =============================================================================================
    // UpdateIK @0x82608858 (61 insns) -- ⭐ BODIED 2026-08-14 (walls wave; it was ABSENT from the
    // tree altogether, trial-link-measured). PS3 out-of-line twin @0x6D374C confirms structure +
    // the operand roles lane for lane.
    //
    // Two passes:
    //  (1) TAG-POINT RELAXATION (0x8260887C..0x82608900): for each of the miNumTagPoints live tag
    //      points (base this+15120, stride 32), pull the point toward its two-bone skinned target
    //      at a rate of lvfTime per frame:
    //          posA   = mpSensorA->mpLocalSpaceSphere->centre       (`lwz 0x19C(sensor)` -> lvx)
    //          posB   = mpSensorB->mpLocalSpaceSphere->centre
    //          target = (posA + offA)*wA + (posB + offB)*wB          (offsets/weights = spec +0/+16,
    //                                                                 weights in the .w lanes)
    //          mPos  += (target - mPos) * lvfTime                    (vmaddfp on the time broadcast)
    //      then re-blend the point's scratch from the two sensors' accumulated scratch with the
    //      SCALAR weight pair (spec +48/+52 -- a different pair from the .w lanes, exactly as the
    //      asm reads both):
    //          mfScratchAmount = sensorA.scratch*mfWeightA + sensorB.scratch*mfWeightB
    //  (2) IK-PART UPDATE (0x8260891C..0x82608940): for each of the miNumIKBodyParts parts, skip
    //      parts whose state == 4 (E_PART_STATE_DETATCHED, DWARF spelling -- detached parts are
    //      simulated by the part pool, not IK), else IKBodyPart::Update().
    // =============================================================================================
    void DeformableObject::UpdateIK(VecFloat lvfTime)
    {
        // ---- (1) the tag-point relaxation ------------------------------------------------------
        for (s32 liTag = 0; liTag < miNumTagPoints; ++liTag)
        {
            TagPoint& lrTag = maTagPoints[liTag];
            const TagPointSpec* lpSpec = lrTag.GetSpec();

            const Vector4& lrPosA =
                lrTag.GetDeformationSensorA()->GetLocalSpaceSphere()->mPositionRadius;
            const Vector4& lrPosB =
                lrTag.GetDeformationSensorB()->GetLocalSpaceSphere()->mPositionRadius;
            const Vector3Plus& lrOffA = lpSpec->GetOffsetAndWeightA();
            const Vector3Plus& lrOffB = lpSpec->GetOffsetAndWeightB();

            // target = (posA + offA)*wA + (posB + offB)*wB, per xyz lane (the .w weights splat).
            Vector3 lTarget;
            lTarget.x = (lrPosA.x + lrOffA.x) * lrOffA.w + (lrPosB.x + lrOffB.x) * lrOffB.w;
            lTarget.y = (lrPosA.y + lrOffA.y) * lrOffA.w + (lrPosB.y + lrOffB.y) * lrOffB.w;
            lTarget.z = (lrPosA.z + lrOffA.z) * lrOffA.w + (lrPosB.z + lrOffB.z) * lrOffB.w;

            // mPos += (target - mPos) * time  (vsubfp then vmaddfp on the broadcast time lane).
            const Vector3& lrPos = lrTag.GetPosition();
            Vector3 lNewPos;
            lNewPos.x = lrPos.x + (lTarget.x - lrPos.x) * lvfTime.x;
            lNewPos.y = lrPos.y + (lTarget.y - lrPos.y) * lvfTime.x;
            lNewPos.z = lrPos.z + (lTarget.z - lrPos.z) * lvfTime.x;
            lNewPos.w = lrPos.w;
            lrTag.SetPosition(lNewPos);

            // Scratch re-blend, SCALAR weight pair (spec +48/+52; sensors' +420 accumulators).
            lrTag.SetScratchAmount(
                lrTag.GetDeformationSensorA()->GetScratchAmount() * lpSpec->GetWeightA() +
                lrTag.GetDeformationSensorB()->GetScratchAmount() * lpSpec->GetWeightB());
        }

        // ---- (2) the IK-part update (skip detached/physical parts) -----------------------------
        for (s32 liPart = 0; liPart < miNumIKBodyParts; ++liPart)
        {
            if (maPartStates[liPart] != static_cast<u8>(E_PART_STATE_DETATCHED))   // `cmplwi 4`
            {
                maIKParts[liPart].Update();
            }
        }
    }

    // =============================================================================================
    // WALLS LEG 4 (2026-08-14): THE PER-FRAME UPDATE SPINE LANDS. Everything below this banner
    // was written this wave: SetTransform / GetWeightFactor (console-inline accessors the
    // penetration solver's read-back needs), the contact-order statics + UpdateContacts (the
    // impulse route), UpdateIKSuspensionOffsets, UpdateIKAndLocators, Update (the per-model
    // per-frame driver) and UpdatePostPhysics (the post-solve sensor maintenance).
    //
    // NOTE on the per-class static perf monitors: the consoles bracket several legs below with
    // per-class STATIC monitor ids (PS3 names them siSortContactsPerfMon / siUpdateSuspensionIK /
    // siUpdateLocators / ...; X360 carries them at dword_82F2A348..) registered by
    // ConstructUpdatePerformanceMonitors. Those statics are not homed on the host yet; the
    // brackets are OMITTED with this note (the manager-level MEMBER monitors that wrap every one
    // of these calls are real). Restore them with ConstructUpdatePerformanceMonitors.
    // =============================================================================================

    // ---------------------------------------------------------------------------------------------
    // SetTransform (DWARF :378) -- console-inline on both consoles (no export). The inlined stores
    // are visible in DeformationManager::SolvePenetration's phase-3 read-back (X360 @0x826223C0..
    // 0x826223E4: four stvx128 of the solved rows into vehiclePhysics +0x10..+0x40 == the
    // ExternallySimulatedBody::mTransform rows). The solver's positional correction reaches the
    // car through exactly this store.
    // ---------------------------------------------------------------------------------------------
    void DeformableObject::SetTransform(const Matrix44Affine* lpTransform)
    {
        GetVehicleBody().SetTransform(*lpTransform);
    }

    // ---------------------------------------------------------------------------------------------
    // GetWeightFactor (DWARF :455) -- console-inline on both consoles. The inlined read is visible
    // in SolvePenetration's phase-1 add loop (X360: `lvx128 v0, vehiclePhysics, 4176 ;
    // vspltw v1, v0, 3` -- the w lane of the +0x1050 packed row, whose named host member carries
    // ...SolvePenetrationWeightFactor in exactly that lane; VehiclePhysics.h seeds it 1.0, the
    // image-read ground truth unk_8208FB18 == 0x3F800000).
    // ---------------------------------------------------------------------------------------------
    VecFloat DeformableObject::GetWeightFactor()
    {
        const f32 lfW = mVehicleBody.GetVehiclePhysics()
            ->mvPropSpeedMaintainAlongZ_PropSpeedMaintainAlongVel_TimeSinceLastRaceCarContact_SolvePenetrationWeightFactor.w;
        return VecFloat{ lfW, lfW, lfW, lfW };   // vspltw lane-3 broadcast
    }

    // ---------------------------------------------------------------------------------------------
    // The contact-order scratch UpdateContacts sorts through. PS3 names the CLASS STATIC
    // (`DeformableObject::_mContactOrder.miNumContacts` / `.maContactTimes[i].mi16SensorIndex`);
    // the X360 carries the same block at file-static addresses (flt_82FB7B00 base, 12-byte stride:
    // sort key +0, impact time +4, sensor index +8; count word_82FB7C2C). Modelled file-static
    // here (internal linkage; the only consumer is UpdateContacts below, exactly as on console).
    // ---------------------------------------------------------------------------------------------
    namespace
    {
        struct ContactTime
        {
            f32 mfSortKey;        // +0 -- std::sort ascending key
            f32 mfImpactTime;     // +4 -- the record's sub-frame impact time
            s16 mi16SensorIndex;  // +8 -- which sensor owns the record

            bool operator<(const ContactTime& lrOther) const { return mfSortKey < lrOther.mfSortKey; }
        };

        struct ContactOrder
        {
            s16         miNumContacts;
            ContactTime maContactTimes[24];   // bare sensors (<=20) + 4 wheel slots headroom
        };
        ContactOrder _mContactOrder;
    }

    // =============================================================================================
    // UpdateContacts @0x826478B0 (348; PS3 0x74715C, 961 -- the PS3 names every piece) -- THE
    // IMPULSE ROUTE. Two phases:
    //
    //  (1) SORT: normalise the body's linear velocity (vmsum3fp + vrsqrtefp + 2 Newton refines);
    //      for each bare sensor, GetImpulse() (X360 inlines it: skip when the record's impact time
    //      > 1.0 -- the disarm sentinel -- else copy the 64-byte record); assert the time is 0..1
    //      ("Impact time is %f on sensor %d/%d", PS3 :1861); build a ContactTime row:
    //        * CAR-CAR records (mpOtherVehicle != 0): sort key = the impact time (earliest first);
    //        * WORLD records: sort key = dot(normalizedVelocity, record.mNormal) -- most head-on
    //          (most negative) first. [FLAG: the dot's second operand is the one 16-byte lane of
    //          the copied record the X360 pseudo obscures; the normal is the only physically
    //          consistent operand and matches the PS3 register flow.]
    //      then std::sort the rows (the exported std::_Sort<ContactTime*> over operator<).
    //  (2) APPLY: zero the vehicle's per-frame world-collision count (mi8NumWorldCollisions,
    //      vp+4947); for each sorted row re-read the sensor's record (same >1.0 skip) and route:
    //        * car-car -> ApplyCarCarImpulse(record, timeStep, iteration=0, sensorIdx, random)
    //        * world   -> ApplyCarWorldImpulse(record, timeStep, iteration=0, sensorIdx)
    //      and when an impulse was applied, CalculateNewVelocity(timeStep) on this body (and on
    //      the other car's body for car-car).
    // =============================================================================================
    void DeformableObject::UpdateContacts(VecFloat lvfTimeStep, CgsNumeric::Random& lrRandom)
    {
        // ---- [chain] PC bring-up instrument -- DELETE WHEN the wall test is banked -------------
        ++gChainProbe.muUpdateContacts;
        if ( ChainProbeOn() && CgsDev::Log::gpDebugPrint != 0
             && (gChainProbe.muUpdateContacts % 600u) == 0u )
        {
            *CgsDev::Log::gpDebugPrint
                << "[chain] upd " << static_cast<s32>(gChainProbe.muUpdateContacts)
                << " scan " << static_cast<s32>(gChainProbe.muSensorsScanned)
                << " getImp " << static_cast<s32>(gChainProbe.muGetImpulseHits)
                << " sensorImp " << static_cast<s32>(gChainProbe.muSensorImpulse)
                << " dirsTried " << static_cast<s32>(gChainProbe.muDirsTried)
                << " dirsPassed " << static_cast<s32>(gChainProbe.muDirsPassed)
                << " dispatched " << static_cast<s32>(gChainProbe.muDispatched)
                << " nullSensor " << static_cast<s32>(gChainProbe.muNullSensor)
                << "\n";
        }

        const Vector3 lvVelocityDir = vpu::Normalize(GetVehicleBody().GetLinearVelocity());

        // ---- (1) collect + sort ----------------------------------------------------------------
        _mContactOrder.miNumContacts = 0;

        const s32 liNumSensors = GetNumSensors() - 4;
        for ( s32 liSensor = 0; liSensor < liNumSensors; ++liSensor )
        {
            StoredImpulseContact lContact;
            ++gChainProbe.muSensorsScanned;   // [chain] probe
            if ( !maDeformationSensors[liSensor].GetImpulse(lContact) )
            {
                continue;
            }
            ++gChainProbe.muGetImpulseHits;   // [chain] probe

            // "Impact time is %f on sensor %d/%d" (PS3 :1861) -- tripwire, fire-and-continue.
            CGS_ASSERT(lContact.mfImpactTimeInFrame >= 0.0f && lContact.mfImpactTimeInFrame <= 1.0f,
                       "Impact time is ");

            f32 lfSortKey;
            if ( lContact.mpOtherVehicle != nullptr )
            {
                lfSortKey = lContact.mfImpactTimeInFrame;                 // car-car: earliest first
            }
            else
            {
                lfSortKey = vpu::Dot(lvVelocityDir, lContact.mNormal);    // world: most head-on first
            }

            ContactTime& lrRow = _mContactOrder.maContactTimes[_mContactOrder.miNumContacts];
            lrRow.mfSortKey       = lfSortKey;
            lrRow.mfImpactTime    = lContact.mfImpactTimeInFrame;
            lrRow.mi16SensorIndex = static_cast<s16>(liSensor);
            ++_mContactOrder.miNumContacts;
        }

        std::sort(_mContactOrder.maContactTimes,
                  _mContactOrder.maContactTimes + _mContactOrder.miNumContacts);

        // ---- (2) apply in sorted order ---------------------------------------------------------
        mVehicleBody.GetVehiclePhysics()->mi8NumWorldCollisions = 0;   // *(vp+4947) = 0

        const VecFloat lvfIterationZero = { 0.0f, 0.0f, 0.0f, 0.0f };  // vspltisw128 v126, 0

        for ( s32 li = 0; li < _mContactOrder.miNumContacts; ++li )
        {
            CGS_ASSERT(li < _mContactOrder.miNumContacts, "liIndex < miNumContacts");   // h:132

            const s32 liSensor = _mContactOrder.maContactTimes[li].mi16SensorIndex;

            StoredImpulseContact lContact;
            if ( !maDeformationSensors[liSensor].GetImpulse(lContact) )   // same >1.0 skip, re-read
            {
                continue;
            }

            bool lbApplied;
            if ( lContact.mpOtherVehicle != nullptr )
            {
                lbApplied = ApplyCarCarImpulse(lContact, lvfTimeStep, lvfIterationZero,
                                               liSensor, lrRandom);
            }
            else
            {
                lbApplied = ApplyCarWorldImpulse(lContact, lvfTimeStep, lvfIterationZero, liSensor);
            }

            if ( lbApplied )
            {
                GetVehicleBody().CalculateNewVelocity(lvfTimeStep);
                if ( lContact.mpOtherVehicle != nullptr )
                {
                    lContact.mpOtherVehicle->GetVehicleBody().CalculateNewVelocity(lvfTimeStep);
                }
            }
        }
    }

    // =============================================================================================
    // UpdateIKSuspensionOffsets @0x826083B0 (X360; PS3 0x6D7670, 113) -- keep the four WHEEL tag
    // points glued to their sensors + the live suspension height. Per wheel (0..3):
    //   * liTag = mu8WheelTagPointIndices[wheel]; 255 = no wheel tag point -> skip.
    //   * SNAP the tag point to its two-sensor skinned target (the UpdateIK relaxation at rate 1:
    //     target = (sphereA + offA)*wA + (sphereB + offB)*wB; mPos = target).
    //   * re-blend the tag scratch from the two sensors' accumulators (scalar spec weights).
    //   * if the spec is a SKINNED point: replace the offset's Y with the suspension-corrected Y
    //     ((pos.y - initial.y) + (wheel.mPosition.y - wheel.mStreamedPositionPlusTwistAmount.y) --
    //     the live suspension compression), and write {corrected xyz, keep w} into the tag's
    //     verlet scratch row (maVerletOffsets_Scratch[tag], the vperm<0,1,2,7> keep-w store).
    // =============================================================================================
    void DeformableObject::UpdateIKSuspensionOffsets()
    {
        const BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();

        for ( s32 liWheel = 0; liWheel < 4; ++liWheel )
        {
            const u8 lu8Tag = mu8WheelTagPointIndices[liWheel];
            if ( lu8Tag == KU_INVALID_WHEEL_TAG_POINT_INDEX )
            {
                continue;
            }

            TagPoint& lrTag = maTagPoints[lu8Tag];
            const TagPointSpec* lpSpec = lrTag.GetSpec();

            const Vector4& lrPosA =
                lrTag.GetDeformationSensorA()->GetLocalSpaceSphere()->mPositionRadius;
            const Vector4& lrPosB =
                lrTag.GetDeformationSensorB()->GetLocalSpaceSphere()->mPositionRadius;
            const Vector3Plus& lrOffA = lpSpec->GetOffsetAndWeightA();
            const Vector3Plus& lrOffB = lpSpec->GetOffsetAndWeightB();

            // target = (posA + offA)*wA + (posB + offB)*wB; SNAP (rate 1.0 -- vcfsx v7 is 1.0).
            Vector3 lTarget;
            lTarget.x = (lrPosA.x + lrOffA.x) * lrOffA.w + (lrPosB.x + lrOffB.x) * lrOffB.w;
            lTarget.y = (lrPosA.y + lrOffA.y) * lrOffA.w + (lrPosB.y + lrOffB.y) * lrOffB.w;
            lTarget.z = (lrPosA.z + lrOffA.z) * lrOffA.w + (lrPosB.z + lrOffB.z) * lrOffB.w;
            lTarget.w = lrTag.GetPosition().w;
            lrTag.SetPosition(lTarget);

            // Scratch re-blend (scalar weight pair spec+48/+52 over the sensors' +420 accumulators).
            lrTag.SetScratchAmount(
                lrTag.GetDeformationSensorA()->GetScratchAmount() * lpSpec->GetWeightA() +
                lrTag.GetDeformationSensorB()->GetScratchAmount() * lpSpec->GetWeightB());

            // Skinned wheel tag: fold the live suspension compression into the Y lane and hand the
            // corrected offset to the skinning scratch row.
            if ( lpSpec->IsSkinned() )   // spec +65 (+0x41)
            {
                const BrnPhysics::Vehicle::Wheel& lrWheel =
                    lpVehicle->GetWheel(static_cast<BrnPhysics::Vehicle::EVehicleDrivenWheel>(liWheel));

                const Vector3& lrInitial = lpSpec->GetInitialPosition();
                const f32 lfSuspensionY  = lrWheel.mPosition.y
                                         - lrWheel.mStreamedPositionPlusTwistAmount.y;

                Vector3Plus& lrScratch = maVerletOffsets_Scratch[lu8Tag];
                lrScratch.x = lTarget.x - lrInitial.x;
                lrScratch.y = (lTarget.y - lrInitial.y) + lfSuspensionY;   // vperm<0,5,2,3> Y swap
                lrScratch.z = lTarget.z - lrInitial.z;
                // w lane preserved (vperm<0,1,2,7>).

                // ---- [restrow-wheel] NOT X360; opt-in BRN_RESTROW_PROBE=1. DELETE-WHEN attributed.
                {
                    static s32 siWheelProbe = -1;
                    if ( siWheelProbe < 0 )
                    {
                        const char* lpcEnv = getenv( "BRN_RESTROW_PROBE" );
                        siWheelProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
                    }
                    static s32 siWheelLines = 0;
                    if ( siWheelProbe == 1 && siWheelLines < 48 && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        ++siWheelLines;
                        const s32 liA = static_cast<s32>(lrTag.GetDeformationSensorA() - &maDeformationSensors[0]);
                        const s32 liB = static_cast<s32>(lrTag.GetDeformationSensorB() - &maDeformationSensors[0]);
                        *CgsDev::Log::gpDebugPrint
                            << "[restrow-wheel] obj " << static_cast<s32>(mu16DeformableObjectIndex)
                            << " owner " << static_cast<s32>(GetHandlingBodyIdHighByte())
                            << " wheel " << liWheel << " tag " << static_cast<s32>(lu8Tag)
                            << " A " << liA << " (" << lrPosA.x << "," << lrPosA.y << "," << lrPosA.z << ")"
                            << " B " << liB << " (" << lrPosB.x << "," << lrPosB.y << "," << lrPosB.z << ")"
                            << " target (" << lTarget.x << "," << lTarget.y << "," << lTarget.z << ")"
                            << " init (" << lrInitial.x << "," << lrInitial.y << "," << lrInitial.z << ")"
                            << " wheelPos.y " << lrWheel.mPosition.y
                            << " streamed.y " << lrWheel.mStreamedPositionPlusTwistAmount.y
                            << " row (" << lrScratch.x << "," << lrScratch.y << "," << lrScratch.z << ", w " << lrScratch.w << ")\n";
                    }
                }
            }
        }
    }

    // =============================================================================================
    // UpdateIKAndLocators @0x82642230 (117; PS3 0x765220, 740) -- the IK/locator/wheel/glass pass
    // the manager budgets per frame. X360 flow, call for call:
    //   assert mbActive (:1801); assert mbIKUpdateRequired || gboEnableDeformationDebug (:1806);
    //   CheckForDetachment(simIn, physOut, partMgr, timeStep);
    //   UpdateIK(0.05); UpdateIK(0.1); UpdateIK(0.5); UpdateIK(1.0);   (the four-step relaxation)
    //   UpdateSkinningOffsets();
    //   UpdateWheels(simIn, wheelMgr, timeStep, random);   [GATED -- see below]
    //   UpdateGlass(timeStep, <the two deformation output interfaces off the module output>);
    //   UpdateDeformedBBox();
    //   mbIKUpdateRequired = false;
    //
    // TWO NAMED GATES (honest partials, censused):
    //   * UpdateWheels @0x826254C0 (1125; PS3 0x763658, 1778) is the wheel-deformation whale --
    //     NOT reconstructed this wave. Log-once gate; the traction/steering wheel path is a
    //     different system and unaffected.
    //   * UpdateGlass's two output interfaces come off the physics-module output buffer through
    //     accessors not yet homed on the host PhysicsModuleIO::OutputBuffer -- glass pane updates
    //     are dead on the junkyard path (no glass impacts); log-once gate.
    // =============================================================================================
    void DeformableObject::UpdateIKAndLocators(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                               BrnPhysics::PhysicsModuleIO::OutputBuffer* lpOutput,
                                               VecFloat lvfTimeStep, DetachedPartManager* lpPartMgr,
                                               DetachedWheelManager* lpWheelMgr,
                                               CgsNumeric::Random* lpRandom)
    {
        CGS_ASSERT(mbActive, "mbActive");                                                    // :1801
        CGS_ASSERT(mbIKUpdateRequired, "mbIKUpdateRequired || gboEnableDeformationDebug");   // :1806

        CheckForDetachment(lpInput, lpOutput, lpPartMgr, lvfTimeStep.x);

        UpdateIK(VecFloat{ 0.05f, 0.05f, 0.05f, 0.05f });   // v44[0] = 0.050000001
        UpdateIK(VecFloat{ 0.1f, 0.1f, 0.1f, 0.1f });       // v44[0] = 0.1
        UpdateIK(VecFloat{ 0.5f, 0.5f, 0.5f, 0.5f });       // vcsxwfp128(1,1) == 0.5
        UpdateIK(VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f });       // vcsxwfp128(1,0) == 1.0

        UpdateSkinningOffsets();

        {
            static bool sbLoggedWheelsGate = false;
            if ( !sbLoggedWheelsGate )
            {
                sbLoggedWheelsGate = true;
                if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: DeformableObject::UpdateWheels @0x826254C0 (1125) "
                           "reached but not reconstructed -- wheel deformation inert "
                           "[FLAG PC boot gate]\n";
            }
            (void)lpWheelMgr; (void)lpRandom;
        }

        {
            static bool sbLoggedGlassGate = false;
            if ( !sbLoggedGlassGate )
            {
                sbLoggedGlassGate = true;
                if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: DeformableObject::UpdateGlass leg of "
                           "UpdateIKAndLocators skipped (module-output deformation interfaces "
                           "not homed) [FLAG PC boot gate]\n";
            }
        }

        UpdateDeformedBBox();

        mbIKUpdateRequired = false;   // *(this+26409) = 0
    }

    // =============================================================================================
    // Update @0x82649160 (289; PS3 0x7585D8, 1069) -- THE PER-MODEL PER-FRAME DRIVER. Returns
    // mbIKUpdateRequired (the manager's IK-budget selector). X360 flow, store for store:
    //   assert mbActive (:1672);
    //   vp = mVehicleBody.GetVehiclePhysics(); if (vp->IsFrozen()) return mbIKUpdateRequired;
    //   if (owner byte == RACECAR && racecar->mbAISlowMo) timeStep *= 0.01  (the AI-crash slow-mo);
    //   SetEntityRadius(scene iface, mGlobalEntityId, GetEntitySphereSize());
    //   assert sphere size <= KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0 (:1687; the KVF == 100.0,
    //     recovered from the PS3 initializer);
    //   if (meAbsorptionSet == 4) mfNoDamageTimer -= timeStep;
    //   UpdateAbsorptionSet(gameMode); UpdateContacts(timeStep, random);
    //   UpdateOutputContactSpies(simOut, contacts);
    //   [kbAllowDeformationDebug -> mbHasDeformedThisFrame = 1 : the dev toggle, absent on host]
    //   UpdateSpinningDetachment(simIn, physOut, partMgr, timeStep, random);
    //   UpdateIKSuspensionOffsets(); UpdateLocators(partMgr);
    //   mAngularVelocitySum = body angular velocity (vehicle row +96 -> this+26304); and the
    //     entity sphere centre re-seeds from the velocity row's xyz (this+26320 keep w);
    //   vp body CalculateNewVelocity(timeStep);
    //   CheckForForcedDetachment(simIn, physOut, partMgr, random, timeStep);
    //   mbIKUpdateRequired |= mbHasDeformedThisFrame; vp->mbDeformedThisFrame = flag;
    //   [the showtime crashed-wheel random-detach block -- GATED, see below]
    //   return mbIKUpdateRequired.
    //
    // ONE NAMED GATE: the showtime wheel-detach tail (game mode 3 + racecar + crashing +
    // three dynamic-init thresholds unk_82FB9AE0/9700/9600, values not yet recovered) -- dead on
    // the junkyard drive path; log-once gate, censused.
    // =============================================================================================
    bool DeformableObject::Update(CgsPhysics::PhysicsSimulationIO::InputBuffer* lpInput,
                                  CgsPhysics::PhysicsSimulationIO::OutputBuffer* lpOutput,
                                  const BrnPhysics::PhysicsModuleIO::InputBuffer* lpModuleInput,
                                  BrnPhysics::PhysicsModuleIO::OutputBuffer* lpModuleOutput,
                                  VecFloat lvfTimeStep, DetachedPartManager* lpPartMgr,
                                  DetachedWheelManager* lpWheelMgr,
                                  BrnPhysics::PhysicsModuleIO::PotentialContactInterface* lpContacts,
                                  CgsNumeric::Random& lrRandom, s32 liGameMode)
    {
        CGS_ASSERT(mbActive, "mbActive");   // :1672

        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();
        if ( lpVehicle->IsFrozen() )        // *(vp+112) early-out
        {
            return mbIKUpdateRequired;
        }

        // AI-crash slow motion: a RACECAR owner in slow-mo scales the whole deformation step by
        // 0.01 (the 0.0099999998 literal in both asms; racecar mbAISlowMo == *(vp+5172)).
        VecFloat lvfStep = lvfTimeStep;
        if ( ((mGlobalEntityId.muValue >> 24) & 0xFFu) == 1u )   // HIBYTE(+26392) == E_ENTITYTYPE_RACECAR
        {
            BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar = AsRaceCarPhysics();
            if ( lpRaceCar != nullptr && lpRaceCar->IsInAICrashSlowMo() )
            {
                lvfStep.x *= 0.01f; lvfStep.y *= 0.01f; lvfStep.z *= 0.01f; lvfStep.w *= 0.01f;
            }
        }

        // SetEntityRadius(scene iface, entity word, sphere size) + the size tripwire (:1687).
        // (The module-output scene interface through the same reinterpret seam the mounted prop
        //  read-back in PhysicsModule::Update uses -- the storage member is opaque on the host.)
        // ⚠️ [marked deviation] HOST GUARD (walls leg 4 boot 1): on the PC bring-up NOTHING
        // prepares this embedded interface's queue storages yet (no consumer constructs them),
        // so the console's unconditional per-frame append filled a 0-capacity queue with a null
        // mpEvents and AV'd inside SetEntityRadius (+0xB5, Get-WinEvent->map resolved). Guard on
        // the queue actually having storage; the console has no such branch. Un-guard when the
        // scene-side consumer of the module-output scene interface lands.
        {
            CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene =
                reinterpret_cast<CgsSceneManager::SceneManagerIO::InSceneUpdateInterface*>(
                    lpModuleOutput->GetSceneInputInterface());
            if ( lpScene->GetSetEntityRadiusQueue().GetMaxLength() > 0 )
            {
                lpScene->SetEntityRadius(CgsSceneManager::EntityId(mGlobalEntityId.muValue),
                                         GetEntitySphereSize().x);
            }
            else
            {
                static bool sbLoggedRadiusGate = false;
                if ( !sbLoggedRadiusGate )
                {
                    sbLoggedRadiusGate = true;
                    if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                        *CgsDev::Log::gpDebugPrint
                            << "conductor gate: module-output scene interface unprepared -- "
                               "SetEntityRadius skipped [FLAG PC boot gate]\n";
                }
            }
        }
        CGS_ASSERT(GetEntitySphereSize().x <= 100.0f + 1.0f,
                   "GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f)");  // :1687

        if ( static_cast<s32>(meAbsorptionSet) == 4 )   // *(this+26460) == 4
        {
            mfNoDamageTimer -= lvfStep.x;               // *(this+26396) -= step
        }

        UpdateAbsorptionSet(liGameMode);
        UpdateContacts(lvfStep, lrRandom);
        UpdateOutputContactSpies(lpOutput, lpContacts);

        UpdateSpinningDetachment(lpInput, lpModuleOutput, lpPartMgr, lvfStep, lrRandom);
        UpdateIKSuspensionOffsets();
        UpdateLocators(lpPartMgr);

        // mAngularVelocitySum <- the body's angular velocity row; the entity sphere centre xyz
        // re-seeds from the body velocity row (keeps its w == the size lane).
        {
            const Vector3 lvAngular = GetVehicleBody().GetAngularVelocity();
            mAngularVelocitySum = VecFloat{ lvAngular.x, lvAngular.y, lvAngular.z, lvAngular.w };
            const Vector3 lvLinear = GetVehicleBody().GetLinearVelocity();
            SetLastLinearVelocity(lvLinear);   // this+26320 xyz keep w (the vperm{0,1,2,7} merge)
        }

        GetVehicleBody().CalculateNewVelocity(lvfStep);
        CheckForForcedDetachment(lpInput, lpModuleOutput, lpPartMgr, &lrRandom, lvfStep.x);

        mbIKUpdateRequired = mbIKUpdateRequired || mbHasDeformedThisFrame;   // +26409 |= +26408
        lpVehicle->mbDeformedThisFrame = mbHasDeformedThisFrame;             // *(vp+4954)

        // The showtime crashed-wheel random-detach tail (mode 3 + racecar + crashing + the three
        // unrecovered dynamic-init thresholds) -- NAMED GATE, dead on the junkyard drive path.
        if ( liGameMode == 3 )
        {
            static bool sbLoggedShowtimeDetachGate = false;
            if ( !sbLoggedShowtimeDetachGate )
            {
                sbLoggedShowtimeDetachGate = true;
                if ( CgsDev::Message::gxMessageFilterFlags & 1 )
                    *CgsDev::Log::gpDebugPrint
                        << "conductor gate: DeformableObject::Update's showtime wheel-detach tail "
                           "reached but not reconstructed (thresholds unk_82FB9AE0/9700/9600 "
                           "unrecovered) [FLAG PC boot gate]\n";
            }
        }

        (void)lpModuleInput;
        return mbIKUpdateRequired;
    }

    // =============================================================================================
    // UpdatePostPhysics @0x825DFEB0 (643; PS3 0x74BBE0, 1520) -- the post-solve sensor
    // maintenance, run per live model from DeformationManager::UpdatePostPhysics AFTER the
    // penetration solve wrote the corrected transform back. X360 flow:
    //   assert mbActive (:3110);
    //   (1) per bare sensor: world sphere = bodyTransform * local sphere (keep the radius w lane);
    //       zero the w lane of mPointDisplacement_BiggestImpulseThisFrame (the per-frame
    //       biggest-impulse magnitude reset);
    //   (2) FROZEN body: per sensor ClearNonWorldContacts (compact the car-car scratch);
    //       else: the ClearStoredContacts reset (disarm the impulse record at 100.0, zero the spy
    //       accumulators + spy id, zero the stored-contact count) -- the SAME store list the
    //       committed ClearStoredContacts walks, called here;
    //   (3) per wheel (0..3): re-seed the appended wheel sphere from the wheel's live X/Z and
    //       STREAMED Y (vrlimi mask-4 Y insert -- suspension-neutral height), lifted (0,scale/4,0)
    //       in body space, transformed by the body rows; radius = scale/2 (second vrlimi wins).
    //       The "Invalid wheel position: ... please tell Graham D." NaN screeds are tripwires;
    //   (4) mbDoSweptSphereTests: re-seed maSweptSpheres from the world spheres + the body point
    //       velocities (linVel + angVel x r, 1/60 length) -- the ResetSensors phase-3 math.
    // =============================================================================================
    void DeformableObject::UpdatePostPhysics(CgsSceneManager::SceneManagerIO::InSceneUpdateInterface* lpScene)
    {
        CGS_ASSERT(mbActive, "mbActive");   // :3110

        BrnPhysics::Vehicle::VehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();
        const Matrix44Affine& lrT = lpVehicle->GetTransform();

        const s32 liNumSensors = GetNumSensors() - 4;

        // ---- (1) world spheres from the solved transform + biggest-impulse magnitude reset -----
        for ( s32 li = 0; li < liNumSensors; ++li )
        {
            const Vector4& lrLocal = maLocalSensorSpheres[li].mPositionRadius;
            Vector4&       lrWorld = maWorldSensorSpheres[li].mPositionRadius;

            lrWorld.x = lrT.xAxis.x * lrLocal.x + lrT.yAxis.x * lrLocal.y
                      + lrT.zAxis.x * lrLocal.z + lrT.wAxis.x;
            lrWorld.y = lrT.xAxis.y * lrLocal.x + lrT.yAxis.y * lrLocal.y
                      + lrT.zAxis.y * lrLocal.z + lrT.wAxis.y;
            lrWorld.z = lrT.xAxis.z * lrLocal.x + lrT.yAxis.z * lrLocal.y
                      + lrT.zAxis.z * lrLocal.z + lrT.wAxis.z;
            // w (radius) preserved (vrlimi keep-w).

            maDeformationSensors[li].mPointDisplacement_BiggestImpulseThisFrame.w = 0.0f;   // vrlimi mask-1 zero
        }

        // ---- (2) contact-scratch reset ----------------------------------------------------------
        if ( lpVehicle->IsFrozen() )   // *(vp+112)
        {
            for ( s32 li = 0; li < liNumSensors; ++li )
            {
                maDeformationSensors[li].ClearNonWorldContacts();
            }
        }
        else
        {
            ClearStoredContacts();   // the identical per-sensor store list, inlined on console
        }

        // ---- (3) wheel spheres ------------------------------------------------------------------
        for ( s32 liWheel = 0; liWheel < 4; ++liWheel )
        {
            const BrnPhysics::Vehicle::Wheel& lrWheel =
                lpVehicle->GetWheel(static_cast<BrnPhysics::Vehicle::EVehicleDrivenWheel>(liWheel));

            // ":3151 !IsZero(...GetStreamedPosition())" + the two "Invalid wheel position" NaN
            // screeds are fire-and-continue tripwires; conditions carried, screeds omitted.
            const f32 lfScale = mpDeformationSpec->GetWheelSpec(liWheel)->mScale.x;

            // local = { pos.x, STREAMED.y (vrlimi mask-4 insert) + scale/4, pos.z }
            const Vector3 lvLocal{
                lrWheel.mPosition.x,
                lrWheel.mStreamedPositionPlusTwistAmount.y + (lfScale * 0.5f) * 0.5f,
                lrWheel.mPosition.z, 0.0f };

            Vector4& lrSphere = maWorldSensorSpheres[liNumSensors + liWheel].mPositionRadius;
            lrSphere.x = lrT.xAxis.x * lvLocal.x + lrT.yAxis.x * lvLocal.y
                       + lrT.zAxis.x * lvLocal.z + lrT.wAxis.x;
            lrSphere.y = lrT.xAxis.y * lvLocal.x + lrT.yAxis.y * lvLocal.y
                       + lrT.zAxis.y * lvLocal.z + lrT.wAxis.y;
            lrSphere.z = lrT.xAxis.z * lvLocal.x + lrT.yAxis.z * lvLocal.y
                       + lrT.zAxis.z * lvLocal.z + lrT.wAxis.z;
            lrSphere.w = lfScale * 0.5f;   // the second vrlimi w write wins
        }

        // ---- (4) swept-sphere re-seed (the ResetSensors phase-3 math, gated) --------------------
        if ( mbDoSweptSphereTests )
        {
            const f32 KF_CONSOLE_TIMESTEP = 0.016666668f;   // flt_82095EE0 splat, image-read

            const Vector3& lvCarPos     = lpVehicle->GetPosition();
            const Vector3& lvLinearVel  = lpVehicle->GetLinearVelocity();
            const Vector3& lvAngularVel = lpVehicle->GetAngularVelocity();

            for ( s32 liSphere = 0; liSphere < liNumSensors + 4; ++liSphere )
            {
                const Vector4& lrSphere = maWorldSensorSpheres[liSphere].mPositionRadius;

                const Vector3 lvR{ lrSphere.x - lvCarPos.x, lrSphere.y - lvCarPos.y,
                                   lrSphere.z - lvCarPos.z, 0.0f };
                const Vector3 lvPointVel = vpu::Add(lvLinearVel, vpu::Cross(lvAngularVel, lvR));

                const f32     lfSpeed = vpu::Magnitude(lvPointVel);
                const Vector3 lvDir   = vpu::Normalize(lvPointVel);

                maSweptSpheres[liSphere].Set(
                    Vector3Plus{ lrSphere.x, lrSphere.y, lrSphere.z, lrSphere.w },
                    Vector3Plus{ lvDir.x, lvDir.y, lvDir.z, lfSpeed * KF_CONSOLE_TIMESTEP });
            }
        }

        (void)lpScene;   // the scene interface is carried for the detached managers' twin passes
    }

}
}
