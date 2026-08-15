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
        // ⛔⛔ HELD FALSE 2026-08-15 (walls leg 8) -- A DELIBERATE, DOCUMENTED DIVERGENCE FROM THE
        // SHIPPED IMAGE, and it is stated plainly rather than buried. Setting it true is CORRECT and
        // was measured working end to end: DeformationSensor::ApplyLocalImpulse then reports
        // `maxAllow 0.200000 base 0.200000 expo 1.000000 factor 0.200000 absorbed 146.7` and the
        // sensor spheres start to dent (`move 0.0047`). It is held for ONE reason:
        // ⛔ the chain forward out of ApplyLocalImpulse is currently gated (unconstructed pool objects
        // -> a wild virtual call in ImpulsePasser::PassOnImpulse; see that gate's banner). With the
        // forward gated, turning this on makes each contact ABSORB ~100-150 units of impulse and
        // hand it to nobody -- momentum deleted, not transferred. That is the silent-drop shape this
        // subsystem keeps being bitten by, and it is strictly worse than being inert.
        // ⭐ FLIP THIS AND THE CHAIN GATE TOGETHER, in the same measured step, once the 20-sensor
        // Construct loop lands. Neither is useful alone.
        const bool KB_ALLOW_DRIVE_TIME_DEFORMATION = false;

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
    // Advance every deformation sensor's stored point-displacement vector by one step. For each of the
    // bare deformation sensors (GetNumSensors() - 4 == mpDeformationSpec->mu8NumDeformationSensors) the
    // asm:
    //   * loads the sensor's displacement source vector,
    //   * transforms it through the vehicle body's world transform (mVehicleBody body +16, the four
    //     16-byte rows), with the cross-product-style lane permute (vpermwi128 0x63) that re-derives the
    //     orthogonal completion,
    //   * scales the result by the supplied time-step lane (v127 == the VecFloat arg), and
    //   * writes it back into the sensor's leading displacement vector (vrlimi128 keeps the w lane).
    // The mbActive tripwire (line 1027) is non-gating. The dense VMX transform is modelled per-lane over
    // the body transform rows.
    // =============================================================================================
    void DeformableObject::UpdateSensorDisplacements(VecFloat lvfTimeStep)
    {
        CGS_ASSERT(mbActive, "mbActive");   // line 1027 (non-gating tripwire)

        const s32 liNumSensors = GetNumSensors() - 4;   // *(mpDeformationSpec + 1618)
        if ( liNumSensors <= 0 )
        {
            return;
        }

        // The body transform rows the sensor displacement is re-projected through (mVehicleBody body
        // sub-object +16 == the attached vehicle's world transform). Reached through GetTransform.
        Matrix44Affine lBodyTransform;
        GetTransform(lBodyTransform);
        const Vector3& lR  = lBodyTransform.Right();
        const Vector3& lU  = lBodyTransform.Up();
        const Vector3& lAt = lBodyTransform.At();

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

            // Re-project through the body rows, then the orthogonal-completion permute (vpermwi128 0x63
            // == the yzx rotate the vnmsubfp cross step uses) and the time-step scale. Modelled per-lane;
            // the permute term reduces to the body-frame projection of the source vector.
            Vector3 lProjected;
            lProjected.x = (lSrc.x * lR.x + lSrc.y * lU.x + lSrc.z * lAt.x) * lfStep;
            lProjected.y = (lSrc.x * lR.y + lSrc.y * lU.y + lSrc.z * lAt.y) * lfStep;
            lProjected.z = (lSrc.x * lR.z + lSrc.y * lU.z + lSrc.z * lAt.z) * lfStep;

            // vrlimi128 v0,v12,1,0 -- write xyz, keep the original w (the biggest-impulse lane).
            lrDisplacement.x = lProjected.x;
            lrDisplacement.y = lProjected.y;
            lrDisplacement.z = lProjected.z;
        }
    }

    // =============================================================================================
    // UpdateSkinningOffsets @0x825DFA90
    //
    // Drive the IK driven points into each IK part's skin. The asm runs in three passes:
    //  (1) Build the per-frame clamp box from the vehicle body's suspension extents (body +1744 /
    //      +1760 / +1768) -- used by the box-clamped parts.
    //  (2) If miNumDrivenPoints > 0, walk the live driven points; the running base into
    //      maVerletOffsets_Scratch (v4) advances per live driven point.
    //  (3) Walk every IK part (miNumIKBodyParts): skip parts in state E_PART_STATE_DETATCHED (4);
    //      bonnet/boot parts (GetPartType == 24/25) skin through UpdateSkinningOffsetsWithinBox with the
    //      clamp box, all others through UpdateSkinningOffsets. v4 advances by each part's
    //      GetNumberOfDrivenPoints so the scratch slice handed to each part is its own window.
    // The clamp-box construction is the dense VMX (modelled); the part walk + the running driven-point
    // index + the per-part-type dispatch are exact.
    // =============================================================================================
    void DeformableObject::UpdateSkinningOffsets()
    {
        // --- (1) clamp box from the body suspension extents (modelled VMX) ------------------------
        // asm 7783-7810: load the FLAGGED inflation row (unk_82FB9550), then the body suspension-extent
        // rows -- v27 = body +1744 (the extent-min row), v28 packs body +1760 / +1768 (the extent-max
        // row). The clamp box the WithinBox parts consume is
        //     box.min = bodyExtentMin - margin ; box.max = bodyExtentMax + margin
        // (vsubfp v13 = v13 - v0 ; vaddfp v0 = v12 + v0). The margin row is the FLAGGED-0
        // KVF_SKINNING_CLAMP_MARGIN. FLAG: the body suspension-extent rows (body +1744/+1760/+1768) are
        // NOT pinned on the minimal VehiclePhysics/ExternalPhysicsBody slice -- they are carried as zero
        // extents here, so the box is (-margin, +margin) and stays inert; the subtract/add SHAPE is exact.
        // Promote bodyExtentMin/Max to the real suspension-extent reads when that slice grows.
        const Vector3 lBodyExtentMin = { 0.0f, 0.0f, 0.0f, 0.0f };  // FLAG: body +1744 (suspension extent min)
        const Vector3 lBodyExtentMax = { 0.0f, 0.0f, 0.0f, 0.0f };  // FLAG: body +1760/+1768 (suspension extent max)
        CgsGeometric::AxisAlignedBox lClampBox;
        lClampBox.mMin.x = lBodyExtentMin.x - KVF_SKINNING_CLAMP_MARGIN.x;
        lClampBox.mMin.y = lBodyExtentMin.y - KVF_SKINNING_CLAMP_MARGIN.y;
        lClampBox.mMin.z = lBodyExtentMin.z - KVF_SKINNING_CLAMP_MARGIN.z;
        lClampBox.mMin.w = 0.0f;
        lClampBox.mMax.x = lBodyExtentMax.x + KVF_SKINNING_CLAMP_MARGIN.x;
        lClampBox.mMax.y = lBodyExtentMax.y + KVF_SKINNING_CLAMP_MARGIN.y;
        lClampBox.mMax.z = lBodyExtentMax.z + KVF_SKINNING_CLAMP_MARGIN.z;
        lClampBox.mMax.w = 0.0f;

        // --- (2) gather the driven tag-point offsets into the Verlet scratch (v4) ------------------
        // asm 7811-7851: when miNumDrivenPoints > 0, walk the live tag points (maTagPoints, byte +15120,
        // 32-byte stride; the loop bound is result[4804] == miNumDrivenPoints). For each tag point whose
        // spec carries the skinned-point flag (*(*v15 + 65) == TagPointSpec::mbSkinnedPoint), compute the
        // offset-from-rest ((mPos - spec.mInitialPosition), vsubfp v0 = v0 - v12) and store it into
        // maVerletOffsets_Scratch[scratchBase] with the w lane carrying the tag point's scratch amount
        // (_R10[7] == mfScratchAmount); the scratch base (v4) advances by one Vector3Plus ONLY when the
        // flag is set. The part walk in (3) then consumes this gathered slice through &maVerletOffsets_
        // Scratch[liRunning].
        s32 liScratchBase = 0;
        if ( miNumDrivenPoints > 0 )
        {
            for ( s32 li = 0; li < miNumDrivenPoints; ++li )
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

        // (1) seed the working ImpulseParams from the caller's block + the per-apply args. The asm copies
        // the incoming params (memcpy v215) then overwrites the magnitude lane (max-folded with the
        // relative-motion w lane) and the contact-id slot (v215[45] = *(this+26460)). Worked on a local
        // copy so the caller's block is untouched. FLAG: the v215[45] contact-id store reads an un-named
        // member at this+26460 with no homed accessor and the ImpulseParams slot-45 has no named field on
        // the minimal slice -- the store is documented but not modelled (no fabricated member).
        ImpulseParams lParams = lImpulseParams;
        lParams.mWorldImpulseDirection = lImpulseDir;
        lParams.mvfTimeStep            = lvfTimeStep;
        {
            const f32 lfRelW = lRelativeMotion.w;   // vspltw v0,3 of the relative-motion vector
            const f32 lfMag  = lvfImpulseMagnitude.x;
            lParams.mvfImpulseMagnitude.x = (lfMag > lfRelW) ? lfMag : lfRelW;   // vmaxfp
        }

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

        ++gChainProbe.muSensorImpulse;   // [chain] probe -- ApplySensorImpulse reached block (4)

        // (4) the six-direction apply loop. Accumulates the local impulse the body banks in (5).
        Vector3 lAccumulatedLocalImpulse = { 0.0f, 0.0f, 0.0f, 0.0f };
        for ( s32 liDir = 0; liDir < KI_NUM_APPLY_DIRECTIONS; ++liDir )
        {
            // direction vector (the switch 0x82607BAC). Direction 0 is the zero/identity seed; the others
            // select +/- a body axis. Carried through the FLAGGED-0 basis table.
            const Vector3 lDirVec = KsaApplyDirection[liDir];

            // project the impulse onto this direction (vmsum3fp128 of the impulse-dir vs the basis).
            const f32 lfProjection = vpu::Dot(lImpulseDir, lDirVec);
            ++gChainProbe.muDirsTried;   // [chain] probe
            if ( lfProjection <= 0.0f )   // vcmpgtfp against zero -- skip non-positive directions
            {
                continue;
            }
            ++gChainProbe.muDirsPassed;   // [chain] probe

            // per-direction params: the direction index. The friendly-fire / double-bounce displacement
            // scaling (other car +26414 / vehicle crashed -> the FLAGGED double-bounce damp row) folds in
            // here; all rows are FLAGGED-0, so the shaping is inert.
            lParams.meImpulseDirection = static_cast<ENextSensorDirection>(liDir);

            // magnitude validation tripwire (line 1430) -- non-gating. The asm self-compares the shaped
            // magnitude vector (vcmpeqfp.) to catch a NaN, then streams the real diagnostic whose leading
            // literal is "Invalid sensor impulse magnitude:\nlfImpulseMagnitude = " (asm 1856/1907-1911);
            // the per-value AppendFormat tail is the streamed diagnostic body, not part of the condition.
            CGS_ASSERT(lfProjection == lfProjection,
                       "Invalid sensor impulse magnitude:\nlfImpulseMagnitude = ");   // line 1430

            // latch the deformation flag: this deformed this frame (this +26408). The vehicle's
            // received-impulse (+1810) and the GetHandlingBodyIdHighByte()==2 crashed-contact (+1809,
            // gated on HIBYTE(*(this+26384)) per asm 1919) latches live on the physics body (un-named on
            // the minimal slice -- documented, not poked).
            mbHasDeformedThisFrame = true;   // this +26408 (deformed-this-frame latch)

            // dispatch into the sensor body: lpSensor->ApplyLocalImpulse(&v215). CollidableBody vtable
            // slot 0 (the `(**a37)(a37, v215)` indirect call).
            if ( lpSensor != nullptr )
            {
                ++gChainProbe.muDispatched;   // [chain] probe
                lpSensor->ApplyLocalImpulse(&lParams);
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

        // (5) resolve + bank the world impulse. The asm removes the residual relative motion along the
        // normal, clamps it into the FLAGGED friction band, scales the impulse, then asks the body for
        // the world linear + angular impulse (GetImpulsesFromLocalImpulse) and banks them
        // (AddWorldSpaceImpulse + AddWorldSpaceAngularImpulse). The crashed path additionally applies a
        // local force (AddLocalForce). The friction rows are FLAGGED-0, so the residual term is inert.
        if ( lpVehicle != nullptr )
        {
            // residual motion along the normal removed (vsubfp v123 = relMotion - n*(relMotion.n)).
            const f32     lfAlongNormal = vpu::Dot(lRelativeMotion, lContact.mNormal);
            const Vector3 lResidual = vpu::Subtract(
                lRelativeMotion,
                Vector3{ lContact.mNormal.x * lfAlongNormal, lContact.mNormal.y * lfAlongNormal,
                         lContact.mNormal.z * lfAlongNormal, 0.0f });
            (void)lResidual;   // shape preserved; the friction scale rows below are FLAGGED-0

            // shaped local impulse = accumulated impulse * FLAGGED friction scale, clamped into the
            // FLAGGED friction band; showtime scales by the FLAGGED showtime row. Inert but honest.
            Vector3 lShaped = vpu::Mult(lAccumulatedLocalImpulse, KVF_APPLY_FRICTION_SCALE);
            lShaped.x = (lShaped.x < KVF_APPLY_FRICTION_CLAMP.x) ? lShaped.x : KVF_APPLY_FRICTION_CLAMP.x;
            lShaped.y = (lShaped.y < KVF_APPLY_FRICTION_CLAMP.y) ? lShaped.y : KVF_APPLY_FRICTION_CLAMP.y;
            lShaped.z = (lShaped.z < KVF_APPLY_FRICTION_CLAMP.z) ? lShaped.z : KVF_APPLY_FRICTION_CLAMP.z;
            if ( lbCrashed )
            {
                lShaped = vpu::Mult(lShaped, KVF_APPLY_SHOWTIME_SCALE);
            }

            // resolve into world linear + angular impulse and bank on the vehicle body.
            Vector3 lWorldImpulse        = { 0.0f, 0.0f, 0.0f, 0.0f };
            Vector3 lWorldAngularImpulse = { 0.0f, 0.0f, 0.0f, 0.0f };
            // The base kernel is the SIX-argument ExternalPhysicsBody form (@0x825A1A80): the two
            // rw::physics::InputSpace tags gate the two rotations (r4 == 1 -> rotate the impulse
            // into world, r5 == 0 -> the position is already world and only needs the COM
            // subtraction). The 4-argument VehiclePhysics-scoped overload this call used to bind to
            // never existed -- it was a shadowing redeclaration retired in physics wave 3.
            // FLAG: this call site's own two tags are NOT recovered from asm here (this TU is
            // unmounted and its X360 body was not re-read this wave); BODY_SPACE / WORLD_SPACE is
            // what the shape of the surrounding code implies -- a local (body) impulse at a world
            // contact point (lContact.mPointOnA is a world contact point) -- and matches the only
            // asm-proven sibling (VehicleRigidBody::ApplyImpulseToVehicle @0x8260E088 `li r4,1`).
            lpVehicle->GetImpulsesFromLocalImpulse(lShaped, rw::physics::BODY_SPACE,
                                                   lContact.mPointOnA, rw::physics::WORLD_SPACE,
                                                   &lWorldImpulse, &lWorldAngularImpulse);

            ExternalPhysicsBody& lBody = GetVehicleBody();
            lBody.AddWorldSpaceImpulse(lWorldImpulse);
            lBody.AddWorldSpaceAngularImpulse(lWorldAngularImpulse);

            // accumulate the per-sensor scratch (sensor +420): the fsel ladder, floored at the 0.75 band.
            if ( lpSensor != nullptr )
            {
                const f32 lfNew   = -(lShaped.x + lpSensor->mfScratchAmount);
                const f32 lfFloor = 0.75f - lpSensor->mfScratchAmount;
                const f32 lfPick  = (lfNew >= 0.0f) ? lfNew : lfFloor;
                lpSensor->mfScratchAmount = (lfPick >= 0.0f) ? lfPick : lpSensor->mfScratchAmount;
            }
        }

        // (6) spy accumulation. When lbAddToSpy, the asm accumulates the impulse into the sensor +288
        // vector and the (impulseDir x relMotion) term into +304. Modelled over the post-physics scratch
        // vectors on the sensor.
        if ( lbAddToSpy && lpSensor != nullptr )
        {
            lpSensor->maPostPhysicsVec0[0] += lAccumulatedLocalImpulse.x;
            lpSensor->maPostPhysicsVec0[1] += lAccumulatedLocalImpulse.y;
            lpSensor->maPostPhysicsVec0[2] += lAccumulatedLocalImpulse.z;

            const Vector3 lTorque = vpu::Mult(lImpulseDir, lRelativeMotion);
            lpSensor->maPostPhysicsVec1[0] += lTorque.x;
            lpSensor->maPostPhysicsVec1[1] += lTorque.y;
            lpSensor->maPostPhysicsVec1[2] += lTorque.z;
        }

        (void)lbUseNormalScaledFriction;   // selects the FLAGGED friction row (inert here)
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
            const BrnPhysics::Vehicle::RaceCarPhysics* lpRaceCar = AsRaceCarPhysics();
            if ( lpRaceCar != nullptr && lpRaceCar->IsAISlowMo() )
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
