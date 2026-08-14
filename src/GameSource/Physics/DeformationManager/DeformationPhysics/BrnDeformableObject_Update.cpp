#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

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

        // FLAGGED-0 PLACEHOLDER for the per-direction signed body-axis basis the apply switch loads
        // (&w::math::vpu::detail::gIVector and the &unk_82181510 / &unk_82181520 +/-axis rows). One
        // 16-byte vector per direction. Honest zeros (NEVER fabricated); the indexing shape is exact.
        // Direction 0 is the identity/zero seed (LABEL_21 vmr v0,v13).
        const Vector3 KsaApplyDirection[KI_NUM_APPLY_DIRECTIONS] = {};

        // FLAGGED-0 PLACEHOLDERS for the friction / limit / scale rows the resolved world impulse is
        // shaped by after GetImpulsesFromLocalImpulse (&unk_82FB95C0 / &unk_82FB8330 / &unk_82FB9D30).
        // Honest zeros (NEVER fabricated); the per-lane clamp/scale SHAPE is exact, the values inert.
        // ⭐ RECOVERED 2026-08-03 (static-init splats). The friction scale is tiny (1.5e-4) and the
        // clamp is 1000, which is why the pair reads as "scale hard down, then bound".
        const Vector3 KVF_APPLY_FRICTION_SCALE = { 0.000150000007f, 0.000150000007f, 0.000150000007f, 0.000150000007f };  // unk_82FB8330 @82C5D688 <- flt_8209D738
        const Vector3 KVF_APPLY_FRICTION_CLAMP = { 1000.0f, 1000.0f, 1000.0f, 1000.0f };  // unk_82FB95C0 @82C5D868 <- flt_82009E10
        const Vector3 KVF_APPLY_SHOWTIME_SCALE = { 5.0f, 5.0f, 5.0f, 5.0f };  // unk_82FB9D30 @82C5D890 <- flt_8200426C

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
            lrSensor.mfMaxPointDisplacement = 100.0f;   // sensor +280
            for ( s32 lj = 0; lj < 4; ++lj )
            {
                lrSensor.maPostPhysicsVec0[lj] = 0.0f;  // sensor +288 (stvx128 0)
                lrSensor.maPostPhysicsVec1[lj] = 0.0f;  // sensor +304 (stvx128 0, r10+16)
            }
            lrSensor.mu32PostPhysicsReset = 0;          // sensor +384
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
        // IsPlayerVehicleInShowtime), pre-apply a showtime contact impulse (impulse-dir scaled by the
        // relative motion). Call order + gate preserved; the scaled vectors are modelled per-lane.
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
            const Vector3 lShowtimeImpulse = vpu::Mult(lImpulseDir, lRelativeMotion);
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

        // (3) allowed-compression budget. Game-mode 2 OR crashed OR the vtable+0x10 predicate -> the unit
        // budget; else the FLAGGED drive-time deformation row (zeroed when drive-time deformation is off,
        // kbAllowDriveTimeDeformation). Modelled as the budget lanes on the params (asm line 1606). The
        // game-mode selector reads HIBYTE(*(this+26384)) == the handling-body-id high byte (asm 1606),
        // NOT the +26392 game-mode word -- use GetHandlingBodyIdHighByte() (see header note).
        if ( GetHandlingBodyIdHighByte() == KU_GAMEMODE_BOUNCE_ELIGIBLE || lbCrashed || lbIgnoringPassedOn )
        {
            lParams.mvfAllowedCompressionFactor = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
            lParams.mvfMaximumAllowedAbsorption = VecFloat{ 1.0f, 1.0f, 1.0f, 1.0f };
        }
        else
        {
            // FLAGGED-0 drive-time deformation row (&unk_82FB9520) -- honest zeros.
            lParams.mvfAllowedCompressionFactor = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
            lParams.mvfMaximumAllowedAbsorption = VecFloat{ 0.0f, 0.0f, 0.0f, 0.0f };
        }

        // (4) the six-direction apply loop. Accumulates the local impulse the body banks in (5).
        Vector3 lAccumulatedLocalImpulse = { 0.0f, 0.0f, 0.0f, 0.0f };
        for ( s32 liDir = 0; liDir < KI_NUM_APPLY_DIRECTIONS; ++liDir )
        {
            // direction vector (the switch 0x82607BAC). Direction 0 is the zero/identity seed; the others
            // select +/- a body axis. Carried through the FLAGGED-0 basis table.
            const Vector3 lDirVec = KsaApplyDirection[liDir];

            // project the impulse onto this direction (vmsum3fp128 of the impulse-dir vs the basis).
            const f32 lfProjection = vpu::Dot(lImpulseDir, lDirVec);
            if ( lfProjection <= 0.0f )   // vcmpgtfp against zero -- skip non-positive directions
            {
                continue;
            }

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
                lpSensor->ApplyLocalImpulse(&lParams);
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
                                                    PotentialContactInterface* lpContacts)
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
}
}
