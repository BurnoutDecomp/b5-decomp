#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // GlassPaneSpec / WheelSpec / GetGlassPaneSpec
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"          // EGlassState, GlassSmashOrCrackEvent, DeformationOutputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"                    // CarState
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"              // SimpleVehiclePhysics::GetGraphicsVehicleTransform / GetWheelsWorldTransfrom
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"                     // WheelPhysicalStates
#include "GameShared/GameClasses/Core/CgsAssert.h"                                                 // CGS_ASSERT

#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{Subtract, MagnitudeSquared, ...}
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{TransformPoint, TransformVector}

#include <cstring>   // std::memset (the FLAGGED zero-seed of the un-homed wheel-state scratch)

// ============================================================================
// GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_GlassState.cpp
//
// BrnPhysics::Deformation::DeformableObject -- the GLASS + STATE-OUTPUT group. Reconstructed
// store-for-store from BURNOUT_X360_ARTIST.XEX. This file owns the windscreen/window smash
// driver and the deformation-state serialisers:
//
//   UpdateGlass             @ 0x82626658  -- per-frame glass spine: for each streamed pane, run
//                                            the smash-state test and, if it changed, emit the
//                                            crack/smash output event.
//   UpdateGlassSmashedState @ 0x825B9DB8  -- (private) advance ONE pane's EGlassState by comparing
//                                            the summed displacement of its four control points to
//                                            two rodata thresholds (crack band, smash band).
//   SendGlassUpdateEvents   @ 0x82608950  -- (private) build a GlassSmashOrCrackEvent for ONE pane
//                                            (corners transformed to world, normal, linear velocity,
//                                            transform, crack amount) and AddEventSafe it onto BOTH
//                                            the render-side and entity-module-side glass queues.
//   OutputState             @ 0x825C1EA8  -- serialise the per-sensor deformation displacement +
//                                            wheel handling state into a CarState record.
//   OutputWheelData         @ 0x82608E28  -- serialise the four wheels' world transforms + physical
//                                            state into a DeformationOutputInterfaceForEntityModules
//                                            entry (PARTIAL -- see FLAG below).
//   GetWheelTagPoints       @ 0x825DFC20  -- write the four wheels' world-space tag-point positions
//                                            into the caller's Vector3[4] (skips wheels with no tag).
//
// MODELLED-vs-asm conventions (same as the committed sibling slices
// BrnStreamedDeformationSpec.cpp / BrnAbsorptionTable.cpp / BrnDeformationSensor.cpp /
// BrnPhysicalBodyPart.cpp):
//
//  * VMX128 vector math is modelled lane-by-lane in scalar f32 / via the rwmath vpu vocabulary.
//    A `vmsum3fp128 v,v` is the xyz dot of a vector with itself (squared magnitude); a
//    `vsubfp`/`vaddfp` is per-lane sub/add; a `vmaddfp` cascade over four matrix rows is the
//    affine TransformPoint; a `vspltw v,v,N` broadcast reads lane N.
//
//  * Asserts are NON-GATING tripwires: a failed CGS_ASSERT runs Begin/Fire/End and execution
//    CONTINUES past it, exactly as the X360 BeginAssert/FireAssert/EndAssert triple (no early-out).
//    The assert strings are the asm's FireAssert literals (source paths/line numbers stripped per
//    project rule). The OutputState / GetWheelTagPoints "Invalid wheel ... please tell Graham D"
//    asserts build a formatted message in the asm; that is a pure tripwire whose body has no
//    observable side effect, so it is modelled as the simple bounds CGS_ASSERT it gates.
//
//  * FLAGGED-0 / placeholder rodata: the two glass-displacement thresholds UpdateGlassSmashedState
//    compares against (X360 rodata &unk_82FB9BC0 = the SMASH band, &unk_82FB9AD0 = the CRACK band)
//    are NOT in the per-function exports. Per the project no-fabrication rule they are carried as
//    correctly-shaped, clearly-labelled file-static placeholders (honest zeros) -- NEVER invented.
//    The pane is scaled by a vcfsx(2)^2 == 4.0 factor before the compare (vspltisw v0,2; vcfsx;
//    vmulfp128 v127,v0,v0 -> 4.0) -- that 4.0 IS visible in the asm and is reproduced.
//
//  * Console member byte offsets the asm indexes (this+6368 mpDeformationSpec, this+6476 the
//    attached vehicle physics, this+15120 maTagPoints[stride 32], this+19232 maDrivenPoints
//    [stride 48], this+26415 the per-frame "glass smash effect suppressed" latch) map onto the
//    frozen-header members BY NAME below. The per-frame +26415 latch has NO separately-named DWARF
//    member (like the bounce latches at +26413/+26414); the frozen header's only glass-effect
//    suppression bool is mbDontPlayGlassPaneEffects, so the latch is modelled onto it (FLAGGED at
//    each use). OutputState / OutputWheelData write into the CarState / entity-module output records
//    whose interiors are NOT fully homed -- those writes use the asm-authoritative byte offsets via
//    raw pointer arithmetic exactly as BrnStreamedDeformationSpec.cpp does for its un-homed sibling
//    spec types; the homed pieces (the vehicle transforms, GetGlassPaneSpec/GetWheelSpec, the event
//    queue AddEventSafe, WheelPhysicalStates::operator=) are called BY NAME.
// ============================================================================

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    namespace
    {
        // -------------------------------------------------------------------------------------
        // FLAGGED-0 PLACEHOLDER rodata: the two per-pane displacement thresholds the smash-state
        // machine compares the summed squared control-point displacement against. The asm loads a
        // full vec4 from each, multiplies it by the visible 4.0 scale (lane 0 is the live compare),
        // and runs a vcmpgtfp. Shape (a 16-byte vec4 threshold) is authoritative; the numeric bytes
        // are NOT in the exports and are carried as honest zeros -- NEVER fabricated. NOTE: with a zero
        // threshold the strict-greater (vcmpgtfp) compare OVER-triggers -- ANY positive displacement
        // crosses the band -- so this is NOT inert; it is a placeholder-rodata side effect that resolves
        // only when the real threshold bytes are recovered.
        //   &unk_82FB9BC0 -> the SMASH band (intact/cracked -> smashed transition)
        //   &unk_82FB9AD0 -> the CRACK  band (intact -> cracked transition)
        // ⭐ RECOVERED 2026-08-03, and the pair corroborates its own reading: both initialisers are
        // SELF-SQUARES. unk_82FB9BC0 @82C5DA38 splats unk_82FB9650 * itself (0.05^2 = 0.0025) and
        // unk_82FB9AD0 @82C5DA80 splats unk_82FB9DF0 * itself (0.001^2 = 1e-06). That two independent
        // thresholds are both squares of the two SendGlassUpdateEvents remap constants confirms the
        // compare is against a SQUARED displacement, exactly as the block comment claims.
        // ⚠️ The zeros were the opposite of inert: a strict-greater compare against 0 fired on ANY
        // positive displacement, so glass went straight to cracked/smashed on the first contact.
        static const Vector4 KVF_GLASS_SMASH_THRESHOLD = { 0.00250000018f, 0.00250000018f, 0.00250000018f, 0.00250000018f }; // unk_82FB9BC0 = unk_82FB9650^2
        static const Vector4 KVF_GLASS_CRACK_THRESHOLD = { 1.00000011e-06f, 1.00000011e-06f, 1.00000011e-06f, 1.00000011e-06f }; // unk_82FB9AD0 = unk_82FB9DF0^2

        // The asm-visible displacement pre-scale: vspltisw v0,2 ; vcfsx v0,v0,0 -> 2.0 ; the
        // threshold vec4 is then vmulfp128'd by (2.0 * 2.0) == 4.0 before the compare.
        static const f32 KF_GLASS_DISPLACEMENT_SCALE = 4.0f;   // asm-visible (= 2.0 * 2.0)

        // ⚠️ STILL A PLACEHOLDER, DELIBERATELY. The two constants ARE now recovered --
        //   unk_82FB9DF0 @82C5DA58 <- flt_82013F90 = 0.001   (splat)
        //   unk_82FB9650 @82C5DA10 <- flt_820047C8 = 0.05    (splat)
        // -- but what is missing here is not the numbers, it is the CODE: SendGlassUpdateEvents runs
        // them through a vrsqrtefp / vrefp Newton refine that maps the max corner displacement-squared
        // into the [0,1] remap feeding the 1-(1-clamp)^2 outer shape, and that remap has never been
        // transcribed. Substituting a plausible sqrt-clamp formula around the real constants would be
        // exactly the kind of confident invention this repo keeps paying for, so the result stays an
        // honest zero and the two values are recorded here for whoever writes the remap.
        static const f32 KF_GLASS_CRACK_REMAP_PLACEHOLDER = 0.0f; // FLAG: the REMAP is unmodelled (values known: 0.001 / 0.05)

        // -------------------------------------------------------------------------------------
        // Console byte offsets the X360 image indexes through raw pointer arithmetic. They hold on
        // the 32-bit console; on the x64 host the absolute offsets do NOT all reproduce, but the
        // STRIDES + relative offsets the serialisers walk are faithful. Used only where the target
        // struct interior is not (yet) homed -- the homed members are reached BY NAME.
        // -------------------------------------------------------------------------------------
        // DeformableObject member offsets (console).
        static const u32 KU_TAG_POINT_ARRAY_OFFSET   = 15120;  // &maTagPoints[0]      (TagPoint stride 32)
        static const u32 KU_TAG_POINT_STRIDE         = 32;     // sizeof(TagPoint)
        static const u32 KU_DRIVEN_POINT_ARRAY_OFFSET = 19232; // &maDrivenPoints[0]   (IKDrivenPoint stride 48)
        static const u32 KU_DRIVEN_POINT_STRIDE      = 48;     // sizeof(IKDrivenPoint)
        static const u32 KU_TAG_POINT_SPEC_PTR       = 16;     // TagPoint::mpSpec (the +16 control-point spec ptr)
        static const u32 KU_TAG_SPEC_REST_POS_OFFSET = 32;     // TagPointSpec rest-position vector (+32)
        static const u32 KU_DRIVEN_REST_POS_OFFSET   = 32;     // IKDrivenPoint control-vector POINTER (+32 -> ptr -> vec)

        // GlassPaneSpec member offsets (console; sizeof 112).
        static const u32 KU_GLASS_POINT_INDEX_0      = 80;     // maiPointIndex[0]  (s16; [1]=82,[2]=84,[3]=86)
        static const u32 KU_GLASS_SKIN_FLAG_0        = 88;     // mabSkinToControlPoint[0] (bool; [1]=89,[2]=90,[3]=91)

        // CarState write offsets OutputState fills (console; CarState interior not fully homed).
        static const u32 KU_CARSTATE_SENSOR_ARRAY    = 32;     // &maSensors[0] write base (80-byte stride)
        static const u32 KU_CARSTATE_SENSOR_STRIDE   = 80;     // per-sensor record stride
        static const u32 KU_CARSTATE_WHEEL_BASE      = 1600;   // the 4 wheel handling-transform rows
        static const u32 KU_CARSTATE_WHEEL_STRIDE    = 16;     // one row per wheel slot
        static const u32 KU_CARSTATE_SCRATCH_SUM     = 1696;   // f32 accumulated scratch displacement
        static const u32 KU_CARSTATE_SENSOR_COUNT    = 1700;   // sensor count (mirror of spec count)
        static const u32 KU_CARSTATE_WHEEL_TAG_BASE  = 1632;   // the 4 wheel tag-point world positions

        // DeformationSensor stride / fields OutputState walks (console).
        // ⛔ X360 VALUES -- DO NOT USE EITHER IN HOST POINTER ARITHMETIC. `this+6484` is
        // &maDeformationSensors[0].mpSpec and 432 is the CONSOLE sizeof(DeformationSensor); on x64
        // every pointer in the sensor widens, so neither lands on a sensor. OutputState walks
        // maDeformationSensors[] BY NAME. Kept only as the figures for reading the asm
        // (`v8 = result + 1621`, `v8[102]`, `v8[103]`).
        static const u32 KU_SENSOR_ARRAY_OFFSET      = 6484;   // reference only -- see above
        static const u32 KU_SENSOR_STRIDE            = 432;    // reference only -- see above

        // The attached vehicle physics pointer + its transform sub-blocks (console).
        static const u32 KU_VEHICLE_PHYSICS_PTR      = 6476;   // mVehicleBody.GetVehiclePhysics() (dword 1619)
        static const u32 KU_VEHICLE_HANDLING_ROWS    = 1744;   // the four wheel handling rows OutputState copies
        static const u32 KU_VEHICLE_TRANSFORM_OFFSET = 16;     // the graphics/handling transform (4 rows from +16)
        static const u32 KU_VEHICLE_WHEEL_STRIDE     = 224;    // per-wheel vehicle block stride (v18 += 224)
        static const u32 KU_VEHICLE_WHEEL_BLOCK_OFFSET = 432;  // wheel-block source value (+304+128; the OutputState tag w-lane source)

        // GlassSmashOrCrackEvent queue offset for the entity-module output interface (its interior is
        // opaque, so its glass queue is reached at the asm offset; the render-side queue is reached BY
        // NAME via DeformationOutputInterface::mGlassSmashOrCrackQueue). The render-side console offset
        // is +6896 and the per-frame "glass smash effect suppressed" latch is +26415 -- both modelled
        // by name (the queue member / mbDontPlayGlassPaneEffects) rather than by raw offset.
        static const u32 KU_OUTPUT_EM_GLASS_QUEUE    = 15920;  // entity-module interface glass queue

        // OutputWheelData entity-module write offsets (console).
        static const u32 KU_EM_NUM_ENTRIES           = 0;      // muNumEntries (the *a3 word)
        static const u32 KU_EM_VOLUME_ID_BASE        = 8;      // &maBaseIDs[0] (dword index 2; stride 8)
        static const u32 KU_EM_VOLUME_ID_STRIDE      = 8;      // a3[2*n+2] -> 8 bytes per id
        static const u32 KU_EM_WHEEL_STATE_BASE      = 240;    // &maWheelStates[0] (dword index 60; stride 400)
        static const u32 KU_EM_WHEEL_STATE_STRIDE    = 400;    // a3[100*n+60] -> 400 bytes per state
        static const u32 KU_EM_MAX_ENTRIES           = 28;     // KU_MAX_DEFORMATION_MODELS
    }

    // ===========================================================================================
    // UpdateGlassSmashedState  @ 0x825B9DB8   (private; called only by UpdateGlass)
    //
    // Advance ONE glass pane's EGlassState. The pane's four control points each have an index
    // (GlassPaneSpec::maiPointIndex[0..3]) and a "skin to control point" flag
    // (GlassPaneSpec::mabSkinToControlPoint[0..3]); the flag selects whether the point's live
    // world position comes from the TagPoint table (skinned) or the IKDrivenPoint table. For each
    // of the four points the asm forms (controlPointRest - livePosition), squares it (vmsum3fp128)
    // and accumulates the four squared displacements into one scalar (v0). It then walks the state
    // machine on the current pane state v25 (== maGlassPaneStates[paneIndex]):
    //   - INTACT (0):  if accumulated >= CRACK threshold*4 -> CRACKED, return true.
    //   - CRACKED (1): if accumulated >= SMASH threshold*4 -> SMASHED, return true; else return false.
    //   - SMASHED (2): nothing to do (it is already at the terminal state) -> falls to the result.
    // Returns true iff the pane's state changed this call (so UpdateGlass emits the event).
    //
    // ASM CONTROL FLOW (faithful): the asm first tests the SMASH band (if state==2 it skips straight
    // to the cracked-band test via LABEL_23). The structure below preserves that exact branch order.
    // ===========================================================================================
    bool DeformableObject::UpdateGlassSmashedState(s32 liPaneIndex, VecFloat /*lvfTimeStep*/)
    {
        // v5 = GetGlassPaneSpec(paneIndex) (the empty-named RECOVERED StreamedDeformationSpec callee).
        const GlassPaneSpec* lpPaneSpec = mpDeformationSpec->GetGlassPaneSpec(liPaneIndex);

        // The threshold vec4s are pre-scaled by the asm-visible 4.0 (vspltisw 2 -> vcfsx -> 2.0;
        // vmulfp128 v127,v0,v0 -> 4.0). Lane 0 is the live compare lane.
        const f32 lfSmashThreshold = KVF_GLASS_SMASH_THRESHOLD.x * KF_GLASS_DISPLACEMENT_SCALE;
        const f32 lfCrackThreshold = KVF_GLASS_CRACK_THRESHOLD.x * KF_GLASS_DISPLACEMENT_SCALE;

        const char* lpcThis        = reinterpret_cast<const char*>(this);
        const char* lpcPaneSpec    = reinterpret_cast<const char*>(lpPaneSpec);

        // Accumulate the four control points' squared rest-to-live displacement (vaddfp of the four
        // vmsum3fp128 dots).
        f32 lfDisplacementSumSq = 0.0f;
        for (s32 liPoint = 0; liPoint < 4; ++liPoint)
        {
            // maiPointIndex[liPoint] (s16) + mabSkinToControlPoint[liPoint] (bool).
            const s16 li16PointIndex = *reinterpret_cast<const s16*>(
                lpcPaneSpec + KU_GLASS_POINT_INDEX_0 + 2 * liPoint);
            const u8 lu8SkinFlag = *reinterpret_cast<const u8*>(
                lpcPaneSpec + KU_GLASS_SKIN_FLAG_0 + liPoint);

            // Live world position + its rest/control position, selected by the skin flag.
            Vector3 lLivePosition;
            Vector3 lRestPosition;
            if (lu8SkinFlag)
            {
                // Skinned: the TagPoint table. element = maTagPoints[index]; live = element.mPos
                // (element +0); rest = *(element.mpSpec + 32).
                const char* lpcElement = lpcThis + KU_TAG_POINT_ARRAY_OFFSET + KU_TAG_POINT_STRIDE * li16PointIndex;
                lLivePosition = *reinterpret_cast<const Vector3*>(lpcElement);
                const char* lpcSpec = *reinterpret_cast<const char* const*>(lpcElement + KU_TAG_POINT_SPEC_PTR);
                lRestPosition = *reinterpret_cast<const Vector3*>(lpcSpec + KU_TAG_SPEC_REST_POS_OFFSET);
            }
            else
            {
                // Driven: the IKDrivenPoint table. element = maDrivenPoints[index]; live = element +0;
                // rest = *(*(element + 32)) -- element+32 holds a POINTER to the control vector
                // (asm: _R10 = *(_R11+32); lvx128 v12, r0, r10).
                const char* lpcElement = lpcThis + KU_DRIVEN_POINT_ARRAY_OFFSET + KU_DRIVEN_POINT_STRIDE * li16PointIndex;
                lLivePosition = *reinterpret_cast<const Vector3*>(lpcElement);
                const char* lpcControl = *reinterpret_cast<const char* const*>(lpcElement + KU_DRIVEN_REST_POS_OFFSET);
                lRestPosition = *reinterpret_cast<const Vector3*>(lpcControl);
            }

            // (live - rest), squared magnitude (vsubfp then vmsum3fp128), summed.
            const Vector3 lDisplacement = vpu::Subtract(lLivePosition, lRestPosition);
            lfDisplacementSumSq += vpu::MagnitudeSquared(lDisplacement);
        }

        // v25 = current pane state.
        EGlassState& lreState = maGlassPaneStates[liPaneIndex];
        bool lbChanged;

        // The asm tests the SMASH band first ONLY when not already smashed (state != 2). When
        // state == 2 it jumps past the smash test (LABEL_23) into the crack test.
        if (lreState != E_GLASS_STATE_SMASHED &&
            lfDisplacementSumSq > lfSmashThreshold)   // vcmpgtfp. v0 > threshold (smash band)
        {
            lreState  = E_GLASS_STATE_SMASHED;
            lbChanged = true;
        }
        else
        {
            // LABEL_23: the crack-band test. If the pane is intact (state == 0) and over the crack
            // band -> CRACKED. Otherwise the result is "did the pane reach the CRACKED state" (the
            // asm's `result = (v25 == 1)` at LABEL_19) -- i.e. a pane already CRACKED reports no
            // change here (it changed on a previous frame), a SMASHED/INTACT pane reports false.
            if (lreState == E_GLASS_STATE_INTACT &&
                lfDisplacementSumSq > lfCrackThreshold)   // vcmpgtfp. v0 > threshold (crack band)
            {
                lreState  = E_GLASS_STATE_CRACKED;
                lbChanged = true;
            }
            else
            {
                lbChanged = (lreState == E_GLASS_STATE_CRACKED);
            }
        }

        return lbChanged;
    }

    // ===========================================================================================
    // SendGlassUpdateEvents  @ 0x82608950   (private; called only by UpdateGlass when a pane changed)
    //
    // Build a GlassSmashOrCrackEvent for ONE pane and queue it onto BOTH output interfaces. The
    // event carries: the four world-space corner positions, the world normal, the pane's linear
    // velocity, the world transform, this car's entity id, the pane's body-part type, the new glass
    // state, the crack amount, and the per-frame "don't play smash effect" suppression flag.
    //
    // The world transform used is the attached vehicle's graphics transform
    // (SimpleVehiclePhysics::GetGraphicsVehicleTransform). Each corner is transformed to world by the
    // vehicle physics transform (vehicle +16) applied to (live control-point position +
    // GlassPaneSpec::maCornerPositionOffsets[i], spec +16/+32/+48/+64); the asm runs a vmaddfp
    // transform cascade per corner. The pane's linear velocity is copied from the car velocity vector
    // at vehicle +80. The entity id is the word at *(a1+26392) (mu32GameModeState), copied verbatim.
    //
    // MODELLED-vs-asm: the event payload writes hit the GlassSmashOrCrackEvent fields whose interior
    // layout is homed (BrnDeformationOutputInterface.h). The per-corner world transform reproduces
    // the asm's vmaddfp cascade through the homed TransformPoint/Add vocabulary. The crack-amount
    // shaping reproduces the asm's outer "1 - (1 - clamp)^2" form gated on the CRACKED (==1) state;
    // the inner remap pulls two UNRECOVERED rodata vec4s (unk_82FB9DF0 / unk_82FB9650) and is carried
    // as a FLAGGED-0 placeholder (NEVER a fabricated sqrt-clamp). The two queue appends call the homed
    // BaseEventQueue::AddEventSafe BY NAME on the two glass queues (render-side @ +6896, entity-
    // module-side @ +15920).
    // ===========================================================================================
    void DeformableObject::SendGlassUpdateEvents(s32 liPaneIndex, DeformationOutputInterface* lpOut,
                                                 DeformationOutputInterfaceForEntityModules* lpOutEM)
    {
        const GlassPaneSpec* lpPaneSpec = mpDeformationSpec->GetGlassPaneSpec(liPaneIndex);

        const char* lpcPane    = reinterpret_cast<const char*>(lpPaneSpec);
        const char* lpcThis    = reinterpret_cast<const char*>(this);

        // The attached vehicle physics (the asm's *(this+6476)). Two transforms are used: the
        // graphics transform (GetGraphicsVehicleTransf result v71) rotates the pane normal into
        // world; the vehicle's physics transform at +16 (the asm's *(this+6476)+16) transforms the
        // live corner control-points into world.
        const char* lpcVehicle = *reinterpret_cast<const char* const*>(lpcThis + KU_VEHICLE_PHYSICS_PTR);
        const Vehicle::SimpleVehiclePhysics* lpSimple =
            reinterpret_cast<const Vehicle::SimpleVehiclePhysics*>(lpcVehicle);
        const Matrix44Affine lGraphicsTransform = lpSimple->GetGraphicsVehicleTransform();
        const Matrix44Affine& lrVehicleTransform =
            *reinterpret_cast<const Matrix44Affine*>(lpcVehicle + KU_VEHICLE_TRANSFORM_OFFSET);

        // The pane's new state (v11 = maGlassPaneStates[paneIndex]).
        const EGlassState leNewState = maGlassPaneStates[liPaneIndex];

        // Build the event.
        GlassSmashOrCrackEvent lEvent;

        // World normal: the pane normal (GlassPaneSpec::mNormal, spec +0) rotated into world by the
        // graphics transform's 3x3 (vspltw of the normal lanes + vmaddfp over the transform rows).
        const Vector3 lPaneNormal = lpPaneSpec->mNormal;
        lEvent.mNormal = vpu::TransformVector(lGraphicsTransform, lPaneNormal);

        // The four world-space corners: each = TransformPoint(vehicleTransform, livePos +
        // maCornerPositionOffsets[i]). The live position for corner i is the maTagPoints /
        // maDrivenPoints element selected by the pane's skin flag (same selection as
        // UpdateGlassSmashedState); the asm then ADDS the per-corner GlassPaneSpec offset
        // (lvx128 v13,r31,{16,32,48,64}; vaddfp v0,v0,v13) BEFORE the vehicle-transform vmaddfp
        // cascade. The largest squared (live - control) displacement (vmaxfp v10) is tracked for
        // the crack-amount shaping.
        f32 lfMaxDisplacementSq = 0.0f;
        for (s32 liCorner = 0; liCorner < 4; ++liCorner)
        {
            const s16 li16PointIndex = *reinterpret_cast<const s16*>(
                lpcPane + KU_GLASS_POINT_INDEX_0 + 2 * liCorner);
            const u8 lu8SkinFlag = *reinterpret_cast<const u8*>(
                lpcPane + KU_GLASS_SKIN_FLAG_0 + liCorner);

            Vector3 lLivePosition;
            Vector3 lRestPosition;
            if (lu8SkinFlag)
            {
                const char* lpcElement = lpcThis + KU_TAG_POINT_ARRAY_OFFSET + KU_TAG_POINT_STRIDE * li16PointIndex;
                lLivePosition = *reinterpret_cast<const Vector3*>(lpcElement);
                const char* lpcTagSpec = *reinterpret_cast<const char* const*>(lpcElement + KU_TAG_POINT_SPEC_PTR);
                lRestPosition = *reinterpret_cast<const Vector3*>(lpcTagSpec + KU_TAG_SPEC_REST_POS_OFFSET);
            }
            else
            {
                const char* lpcElement = lpcThis + KU_DRIVEN_POINT_ARRAY_OFFSET + KU_DRIVEN_POINT_STRIDE * li16PointIndex;
                lLivePosition = *reinterpret_cast<const Vector3*>(lpcElement);
                const char* lpcControl = *reinterpret_cast<const char* const*>(lpcElement + KU_DRIVEN_REST_POS_OFFSET);
                lRestPosition = *reinterpret_cast<const Vector3*>(lpcControl);
            }

            // World corner = (live control point + per-corner offset) transformed by the vehicle
            // physics transform. The +maCornerPositionOffsets[i] addend is the asm's vaddfp v0,v0,v13
            // (spec rows at +16/+32/+48/+64) feeding the vmaddfp cascade.
            const Vector3 lCornerLocal = vpu::Add(lLivePosition, lpPaneSpec->maCornerPositionOffsets[liCorner]);
            lEvent.maCorners[liCorner] = vpu::TransformPoint(lrVehicleTransform, lCornerLocal);

            // Track the largest squared (live - control) displacement (vmaxfp v10 over the corners).
            const Vector3 lDisplacement = vpu::Subtract(lLivePosition, lRestPosition);
            const f32 lfDispSq = vpu::MagnitudeSquared(lDisplacement);
            if (lfDispSq > lfMaxDisplacementSq) lfMaxDisplacementSq = lfDispSq;
        }

        // Linear velocity: the asm copies the car velocity vector at vehicle[+80] (lvx128 v0,r4,80
        // -> stvx &v61 -> the event's mLinearVelocity field). It is NOT zeroed -- the vspltisw v8,0
        // seed feeds the crack-amount/max-disp lanes (v69/v10), not the velocity.
        lEvent.mLinearVelocity = *reinterpret_cast<const Vector3*>(lpcVehicle + 80);

        // Crack amount: gated on the pane being CRACKED (the asm's `if (v11 == 1)`). The asm shapes
        // 1 - (1 - clamp01(remap(maxDispSq)))^2, where the inner remap pulls TWO unrecovered rodata
        // vec4s (unk_82FB9DF0, unk_82FB9650) through a vrsqrtefp/vrefp Newton refine. Those two
        // constants are NOT in the exports; per the project no-fabrication rule the inner remap is
        // carried as a FLAGGED-0 placeholder (the remap result degenerates to 0 -> crack amount 0)
        // rather than substituting a fabricated sqrt-clamp formula. The 1-(1-clamp)^2 outer shape
        // and the CRACKED(==1) gate are reproduced. When not cracked the amount stays at the 0.0 seed.
        f32 lfCrackAmount = 0.0f;
        if (leNewState == E_GLASS_STATE_CRACKED)
        {
            // FLAG: unk_82FB9DF0 / unk_82FB9650 unrecovered -> inner remap placeholder == 0.0.
            const f32 lfRemapped = KF_GLASS_CRACK_REMAP_PLACEHOLDER;   // FLAG: rodata pair unrecovered
            f32 lfClamped = lfRemapped;
            if (lfClamped < 0.0f) lfClamped = 0.0f;
            if (lfClamped > 1.0f) lfClamped = 1.0f;
            const f32 lfOneMinus = 1.0f - lfClamped;
            lfCrackAmount = 1.0f - lfOneMinus * lfOneMinus;
            (void)lfMaxDisplacementSq;   // the asm feeds maxDispSq into the (placeholder) remap.
        }
        lEvent.mfCrackAmount = lfCrackAmount;

        // The remaining scalar payload: transform, entity id, body-part type, new state, suppress flag.
        // The asm writes three consecutive int fields v66/v67/v68 == the event's id / part / state.
        // v66 reads the word at *(a1+26392) -- the frozen header's reconstructed mu32GameModeState;
        // it is copied BYTE-FAITHFULLY into the event's id field (not the semantic mGlobalEntityId).
        // v67 == the spec crack/part param (+100 == mePartType), v68 == the new pane state.
        lEvent.mTransform        = lGraphicsTransform;
        lEvent.mVehicleEntityId  = EntityId{ mu32GameModeState };
        lEvent.meGlassPart       = lpPaneSpec->mePartType;
        lEvent.meNewState        = leNewState;

        // The per-frame "glass smash effect suppressed" latch (asm v70 = *(a1+26415)). FLAG: this
        // byte has no separately-named DWARF member; modelled onto the frozen header's only glass-
        // effect-suppression bool (mbDontPlayGlassPaneEffects). UpdateGlass clears the latch at end.
        lEvent.mbDontPlaySmashEffect = mbDontPlayGlassPaneEffects;

        // Queue onto BOTH glass queues (render-side @ this-relative +6896, entity-module @ +15920).
        // The homed DeformationOutputInterface exposes the render-side queue BY NAME; the entity-
        // module interface interior is opaque, so its queue is reached at the asm offset.
        lpOut->mGlassSmashOrCrackQueue.AddEventSafe(lEvent);

        CgsModule::BaseEventQueue<GlassSmashOrCrackEvent>* lpEMQueue =
            reinterpret_cast<CgsModule::BaseEventQueue<GlassSmashOrCrackEvent>*>(
                reinterpret_cast<char*>(lpOutEM) + KU_OUTPUT_EM_GLASS_QUEUE);
        lpEMQueue->AddEventSafe(lEvent);
    }

    // ===========================================================================================
    // UpdateGlass  @ 0x82626658
    //
    // Per-frame glass spine. For each of the streamed panes (miNumGlassPanes, read via the spec at
    // *(*(this+6368)+32)) run UpdateGlassSmashedState; if the pane's state changed, emit its
    // crack/smash output event via SendGlassUpdateEvents. After the loop, clear the per-frame
    // "glass smash effect suppressed" latch (asm *(this+26415) = 0). The asm seeds a 4.0 SIMD
    // constant (v127 = vcfsx(2)^2) that it passes to UpdateGlassSmashedState as the displacement
    // pre-scale -- that scale is folded into UpdateGlassSmashedState's threshold compare here.
    // ===========================================================================================
    void DeformableObject::UpdateGlass(f32 lfTimeStep, DeformationOutputInterface* lpOut,
                                       DeformationOutputInterfaceForEntityModules* lpOutEM)
    {
        // miNumGlassPanes lives at spec +32; the asm reads *(*(this+6368)+32). The spec keeps it
        // private, so reach it at the asm-attested offset (the spec interior is homed but the count
        // is not exposed by a public accessor).
        const char* lpcSpec      = reinterpret_cast<const char*>(mpDeformationSpec);
        const s32   liNumPanes   = *reinterpret_cast<const s32*>(lpcSpec + 32);

        for (s32 liPane = 0; liPane < liNumPanes; ++liPane)
        {
            // The 4.0 pre-scale (asm v127) is passed as the time-step-position vfloat arg.
            if (UpdateGlassSmashedState(liPane, VecFloat{ KF_GLASS_DISPLACEMENT_SCALE,
                                                          KF_GLASS_DISPLACEMENT_SCALE,
                                                          KF_GLASS_DISPLACEMENT_SCALE,
                                                          KF_GLASS_DISPLACEMENT_SCALE }))
            {
                SendGlassUpdateEvents(liPane, lpOut, lpOutEM);
            }
        }

        // *(this+26415) = 0 -- clear the per-frame glass-smash-effect-suppressed latch. FLAG: modelled
        // onto mbDontPlayGlassPaneEffects (no separately-named DWARF member for this byte).
        mbDontPlayGlassPaneEffects = false;

        (void)lfTimeStep;   // the asm consumes lfTimeStep only as the SIMD-scale seed, folded above.
    }

    // ===========================================================================================
    // OutputState  @ 0x825C1EA8
    //
    // Serialise this car's per-sensor deformation displacement + wheel handling state into a CarState
    // record (lpCarState). The asm:
    //   1. writes the deformation-sensor count (spec +1618 == mu8NumDeformationSensors) into the
    //      CarState count field (+1700);
    //   2. for each sensor, copies the sensor's displacement vectors into the CarState sensor record
    //      (80-byte stride from +32) and accumulates the summed squared displacement into a scalar
    //      (written to CarState +1696);
    //   3. copies the four wheel handling rows from the attached vehicle (vehicle +1744) into the
    //      CarState wheel block (+1600..);
    //   4. for each of the four wheels, asserts (asm order: lpWheel, liWheel < eNumWheels, lpWheelSpec,
    //      liTagPointIndex != -1) and that the vehicle wheel-block value is finite (non-gating
    //      "Invalid wheel position: , please tell Graham D." tripwire), then writes maTagPoints[idx]
    //      (LOCAL, w lane merged from the wheel-block value -- NOT transformed) into the CarState
    //      wheel-tag block (+1632, 16-byte stride).
    //
    // MODELLED-vs-asm: the CarState interior is NOT fully homed (BrnDeformationState.h models only
    // the sensor-count + the opaque per-sensor records), and the DeformationSensor displacement
    // sub-vectors the asm copies are reached through raw offsets within the 432-byte sensor stride.
    // The writes therefore use the asm-authoritative byte offsets via raw pointer arithmetic exactly
    // as BrnStreamedDeformationSpec.cpp does for its un-homed sibling spec types. The bounds asserts
    // are the asm's FireAssert literals; the formatted "Invalid wheel position" message is a pure
    // tripwire modelled as the bounds CGS_ASSERT it gates.
    // ===========================================================================================
    void DeformableObject::OutputState(CarState* lpCarState)
    {
        char*       lpcCarState = reinterpret_cast<char*>(lpCarState);
        const char* lpcThis     = reinterpret_cast<const char*>(this);
        const char* lpcSpec     = reinterpret_cast<const char*>(mpDeformationSpec);

        // (1) sensor count: spec +1618 (mu8NumDeformationSensors) -> CarState +1700.
        const s32 liNumSensors = *reinterpret_cast<const u8*>(lpcSpec + 1618);
        *reinterpret_cast<s32*>(lpcCarState + KU_CARSTATE_SENSOR_COUNT) = liNumSensors;

        // (2) per-sensor displacement copy + summed-squared-displacement accumulate.
        f32 lfScratchSumSq = 0.0f;   // v61 (the running vaddfp accumulator)
        if (liNumSensors)
        {
            char* lpcDst = lpcCarState + KU_CARSTATE_SENSOR_ARRAY;                          // a2+32 (80-byte stride)
            s32   liSensor = 0;
            s32 liRemaining = liNumSensors;
            do
            {
                // The sensor walk is BY NAME (never this+6484 / stride 432 -- X360 values). The
                // three pointers the asm loads out of the sensor record (*v8, v8[102], v8[103]) are
                // mpSpec, mpLocalSpaceSphere and mpWorldSpaceSphere; their INTERIORS (SensorSpec's
                // leading rest vector + its +40 scalar, the spheres' leading Vector4) stay
                // raw-offset because neither type's layout is homed in this TU.
                const DeformationSensor& lrSensor = maDeformationSensors[liSensor];

                const char* lpA = reinterpret_cast<const char*>(lrSensor.mpSpec);              // *v8
                const char* lpB = reinterpret_cast<const char*>(lrSensor.mpLocalSpaceSphere);  // v8[102]

                const Vector3 lDst   = *reinterpret_cast<const Vector3*>(lpcDst);           // current dst row
                const Vector3 lvA    = *reinterpret_cast<const Vector3*>(lpA);
                const Vector3 lvB    = *reinterpret_cast<const Vector3*>(lpB);

                // *(dst+32) = scalar at *(*v8 + 40).
                *reinterpret_cast<f32*>(lpcDst + 32) = *reinterpret_cast<const f32*>(lpA + 40);

                // dst+16 = (B - A) - dst   (vsubfp v12=B-A; vsubfp v0=v12-dst).
                const Vector3 lDelta = vpu::Subtract(vpu::Subtract(lvB, lvA), lDst);
                *reinterpret_cast<Vector3*>(lpcDst + 16) = lDelta;

                // dst-32 = A   (stvx128 v0, r10, r7 with r7 == -32).
                *reinterpret_cast<Vector3*>(lpcDst - 32) = lvA;

                // dst+0 = (B - A); accumulate its squared magnitude into the scratch sum.
                const Vector3 lBminusA = vpu::Subtract(lvB, lvA);
                *reinterpret_cast<Vector3*>(lpcDst) = lBminusA;
                lfScratchSumSq += vpu::MagnitudeSquared(lBminusA);

                // dst-16 = the sensor scalar vector at v8[103]. The asm stores it via
                // stvx128 v12,r8,r7 with r8 = dst+16 and r7 = -32 => (dst+16) + (-32) = dst-16.
                const char* lpC = reinterpret_cast<const char*>(lrSensor.mpWorldSpaceSphere);  // v8[103]
                *reinterpret_cast<Vector3*>(lpcDst - 16) = *reinterpret_cast<const Vector3*>(lpC);

                lpcDst += KU_CARSTATE_SENSOR_STRIDE;      // += 80
                ++liSensor;                               // console: v8 += 108 dwords (432 bytes)
                --liRemaining;
            }
            while (liRemaining);
        }

        // CarState +1696 = the accumulated scratch displacement sum.
        *reinterpret_cast<f32*>(lpcCarState + KU_CARSTATE_SCRATCH_SUM) = lfScratchSumSq;

        // (3) copy the four wheel handling rows from the attached vehicle (vehicle +1744) into the
        // CarState wheel block (+1600..). The asm copies four 16-byte rows.
        const char* lpcVehicle = *reinterpret_cast<const char* const*>(lpcThis + KU_VEHICLE_PHYSICS_PTR);
        const char* lpcHandling = lpcVehicle + KU_VEHICLE_HANDLING_ROWS;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            *reinterpret_cast<Vector3*>(lpcCarState + KU_CARSTATE_WHEEL_BASE + KU_CARSTATE_WHEEL_STRIDE * liRow) =
                *reinterpret_cast<const Vector3*>(lpcHandling + 16 * liRow);
        }

        // (4) per-wheel CarState tag-point write. UNLIKE GetWheelTagPoints, the asm does NOT transform
        // the tag point here: it writes maTagPoints[tagIndex] (this+15120+32*idx, LOCAL) with its w
        // lane replaced by the w lane of the vehicle wheel-block value at vehicle + 224*wheel + 432
        // (vrlimi128 v127,v0,4,0). The per-wheel asserts run in the asm order lpWheel, liWheel <
        // eNumWheels, lpWheelSpec, liTagPointIndex != -1; the finite-position tripwire is on the
        // VEHICLE-block value (v0 @ +432), not the local tag point.
        char* lpcWheelTagDst = lpcCarState + KU_CARSTATE_WHEEL_TAG_BASE;
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            // lpWheel: the per-wheel vehicle base (vehicle + 224*wheel); asm `if (v19 == -304)` is the
            // null-vehicle sentinel check on that pointer (vehicle ptr + 304 == 0 -> base == -304).
            const char* lpcWheelBase = lpcVehicle + KU_VEHICLE_WHEEL_STRIDE * liWheel;
            CGS_ASSERT(reinterpret_cast<std::intptr_t>(lpcWheelBase) != -304, "lpWheel");

            CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");

            const WheelSpec* lpWheelSpec = mpDeformationSpec->GetWheelSpec(liWheel);
            CGS_ASSERT(lpWheelSpec != nullptr, "lpWheelSpec");
            CGS_ASSERT(lpWheelSpec->liTagPointIndex != -1, "lpWheelSpec->liTagPointIndex != -1");

            // The vehicle wheel-block value the asm loads (lvx128 v0, r0, r29 with r29 = vehicle +
            // 224*wheel + 432); its finiteness is the tripwire and its w lane is merged into the row.
            const char* lpcWheelBlock = lpcWheelBase + KU_VEHICLE_WHEEL_BLOCK_OFFSET;
            const Vector3 lWheelBlockValue = *reinterpret_cast<const Vector3*>(lpcWheelBlock);

            // Validity tripwire on the vehicle-block value ("Invalid wheel position ... please tell
            // Graham D." -- a pure formatted-message tripwire with no observable side effect).
            CGS_ASSERT(vpu::IsValid(lWheelBlockValue), "Invalid wheel position: , please tell Graham D.");

            // maTagPoints[tagIndex] (LOCAL, NOT transformed), w lane replaced by the wheel-block w lane.
            const char* lpcElement = lpcThis + KU_TAG_POINT_ARRAY_OFFSET +
                                     KU_TAG_POINT_STRIDE * lpWheelSpec->liTagPointIndex;
            Vector3 lTagPoint = *reinterpret_cast<const Vector3*>(lpcElement);
            lTagPoint.w = lWheelBlockValue.w;   // vrlimi128 v127, v0, 4, 0 (w-lane merge)
            *reinterpret_cast<Vector3*>(lpcWheelTagDst) = lTagPoint;

            lpcWheelTagDst += KU_CARSTATE_WHEEL_STRIDE;   // += 16
        }
    }

    // ===========================================================================================
    // GetWheelTagPoints  @ 0x825DFC20   (const)
    //
    // Write the four wheels' world-space tag-point positions into lpaOut[0..3]. For each wheel:
    //   - resolve the wheel's deformation tag-point index (GetWheelSpec(wheel)->liTagPointIndex);
    //   - if the index is -1 the wheel has no tag point and the output slot is left untouched (the
    //     asm's `if (v11 != -1)` guard -- the slot keeps whatever the caller seeded);
    //   - otherwise transform the tag-point's local position (maTagPoints[index].mPos) by the
    //     attached vehicle's transform (vehicle +16) and store the world position.
    // The asm asserts mpDeformationSpec is non-null and each visited wheel/spec/tag-point index is
    // valid + the tag-point position is finite ("Invalid wheel tag point position ... Please tell
    // Graham D"); all are non-gating tripwires.
    // ===========================================================================================
    void DeformableObject::GetWheelTagPoints(Vector3* lpaOut) const
    {
        CGS_ASSERT(mpDeformationSpec != nullptr, "mpDeformationSpec");

        const char* lpcThis    = reinterpret_cast<const char*>(this);
        const char* lpcVehicle = *reinterpret_cast<const char* const*>(lpcThis + KU_VEHICLE_PHYSICS_PTR);
        const Matrix44Affine& lrVehicleTransform =
            *reinterpret_cast<const Matrix44Affine*>(lpcVehicle + KU_VEHICLE_TRANSFORM_OFFSET);

        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            CGS_ASSERT(liWheel < 4, "liWheel < eNumWheels");

            const WheelSpec* lpWheelSpec = mpDeformationSpec->GetWheelSpec(liWheel);
            CGS_ASSERT(lpWheelSpec != nullptr, "lpWheelSpec");

            const s32 liTagPointIndex = lpWheelSpec->liTagPointIndex;
            if (liTagPointIndex != -1)
            {
                const char* lpcElement = lpcThis + KU_TAG_POINT_ARRAY_OFFSET +
                                         KU_TAG_POINT_STRIDE * liTagPointIndex;
                const Vector3 lTagPointLocal = *reinterpret_cast<const Vector3*>(lpcElement);

                // Validity tripwire (the asm's formatted FireAssert literal; pure tripwire, no side
                // effect). asm string == "Invalid wheel tag point position: <pos> . Please tell Graham D".
                CGS_ASSERT(vpu::IsValid(lTagPointLocal),
                           "Invalid wheel tag point position: . Please tell Graham D");

                // World position = TransformPoint(vehicleTransform, tagPointLocal).
                lpaOut[liWheel] = vpu::TransformPoint(lrVehicleTransform, lTagPointLocal);
            }
            // else: index == -1 -> leave lpaOut[liWheel] untouched (asm skips the store).
        }
    }

    // ===========================================================================================
    // OutputWheelData  @ 0x82608E28   (PARTIAL FIDELITY -- see FLAG)
    //
    // Serialise the four wheels' world transforms + physical state into one entry of a
    // DeformationOutputInterfaceForEntityModules (lpOutEM). For each of the four wheels:
    //   - if the wheel is in the "detached" physical state (sensor/part state byte == 2) and a
    //     detached-wheel record exists for it (the asm's sub_825E8308 lookup against the wheel
    //     manager), build the wheel's world transform from that detached record's position;
    //   - otherwise build it from the attached vehicle's live wheel transform
    //     (SimpleVehiclePhysics::GetWheelsWorldTransfrom(wheel, applySteer));
    //   - fold the per-wheel result into a running entity-sphere-size bound and stamp the per-wheel
    //     scratch state block.
    // Finally it bounds-checks the entity sphere size + the output entry count (non-gating
    // tripwires), writes the per-wheel volume id, copies the assembled WheelPhysicalStates block
    // into the output entry (WheelPhysicalStates::operator=), and bumps the entry count.
    //
    // FLAG (PARTIAL): the X360 pseudocode for this function carries the "local variable allocation
    // has failed, the output may be wrong" marker; its per-wheel transform assembly threads through
    // a NOT-yet-homed detached-wheel lookup (sub_825E8308) and an un-homed 0x188-byte per-wheel
    // scratch-state block (the local v60/v61 buffers), and writes into the entity-module output
    // interface whose interior is opaque. The CONTROL FLOW (the per-wheel detached/live branch, the
    // four-wheel loop bound, the count/sphere tripwires, the volume-id + state writes, the count
    // bump) and the homed calls (GetWheelsWorldTransfrom, WheelPhysicalStates::operator=) are
    // reproduced faithfully; the un-homed detached-record transform math + the per-wheel scratch
    // block are carried as honestly-FLAGGED zero-seeded scratch (NEVER fabricated) and the output
    // entry is written at the asm-authoritative offsets. PROMOTE the detached-record path + the
    // scratch-state interior when the DetachedWheelManager record + the entity-module wheel-state
    // layout are homed.
    // ===========================================================================================
    void DeformableObject::OutputWheelData(s32 /*liWheelIndex*/,
                                           DeformationOutputInterfaceForEntityModules* lpOutEM,
                                           DetachedWheelManager* lpWheelMgr)
    {
        const char* lpcThis    = reinterpret_cast<const char*>(this);
        const char* lpcVehicle = *reinterpret_cast<const char* const*>(lpcThis + KU_VEHICLE_PHYSICS_PTR);
        Vehicle::SimpleVehiclePhysics* lpSimple =
            reinterpret_cast<Vehicle::SimpleVehiclePhysics*>(mVehicleBody.GetVehiclePhysics());

        // The wheel detach flag the asm reads once before the loop (*(spec+112) == -1).
        const char* lpcSpec  = reinterpret_cast<const char*>(mpDeformationSpec);
        const bool  lbApplySteer = (*reinterpret_cast<const s32*>(lpcSpec + 112) == -1);

        // The running entity-sphere-size bound (v127) -- seeded from the vehicle wheel base (+6580)
        // w lane in the asm; carried as a scalar here.
        f32 lfEntitySphereSize = 0.0f;

        // The assembled per-wheel scratch state block (the local v61 the asm copies out). FLAG:
        // 0x188 bytes; its interior is the un-homed entity-module per-wheel physical state. Zeroed
        // (honest seed) -- NEVER fabricated. PROMOTE when that layout is homed.
        WheelPhysicalStates lWheelStates;
        std::memset(&lWheelStates, 0, sizeof(lWheelStates));

        // Per-wheel transform assembly (the do/while over the four wheels; v15 += 224, ++v14). The
        // per-wheel state base is vehicle + 304 + 224*wheel (asm: _R28 = a1[1619] + v15 + 304); the
        // detached discriminator is at +519 from the vehicle (== +215 from the per-wheel base).
        const char* lpcWheelBase = lpcVehicle + 304;
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            // The per-wheel "detached" discriminator byte the asm tests (*(vehicle + v15 + 519) == 2).
            const char* lpcWheelState = lpcWheelBase + 224 * liWheel;
            const u8 lu8WheelPhysState = *reinterpret_cast<const u8*>(lpcWheelState + 215);

            if (lu8WheelPhysState == 2)
            {
                // Detached: look up the detached-wheel record for this wheel against the wheel
                // manager (sub_825E8308). FLAG: the lookup + the record's transform math are not
                // homed; the live-vehicle branch below is the faithful path. The detached record's
                // world transform is carried as identity (honest placeholder) until homed.
                (void)lpWheelMgr;
                // (no observable output produced for the un-homed detached path; state stays zero)
            }
            else
            {
                // Live: the attached vehicle's world wheel transform (homed call).
                const Matrix44Affine lWheelTransform =
                    lpSimple->GetWheelsWorldTransfrom(static_cast<Vehicle::EVehicleDrivenWheel>(liWheel), lbApplySteer);

                // Fold the wheel position into the running sphere-size bound (vmaxfp/vminfp cascade);
                // modelled as the max of the wheel translation magnitude.
                const f32 lfWheelExtent = vpu::Magnitude(lWheelTransform.Pos());
                if (lfWheelExtent > lfEntitySphereSize) lfEntitySphereSize = lfWheelExtent;
            }
        }

        // Bounds tripwire: GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f).
        // FLAG: KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE is un-recovered rodata; the compare is a
        // non-gating tripwire, so the bound is left to the (placeholder-zero) sphere size + 1.0f.
        CGS_ASSERT(lfEntitySphereSize <= (0.0f + 1.0f),
                   "GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f)");

        // Write into the entity-module output interface entry. The interface interior is opaque, so
        // the count / id / state writes use the asm-authoritative offsets.
        char* lpcOutEM = reinterpret_cast<char*>(lpOutEM);
        u32&  lruNumEntries = *reinterpret_cast<u32*>(lpcOutEM + KU_EM_NUM_ENTRIES);

        // muNumEntries < KU_MAX_DEFORMATION_MODELS (non-gating tripwire).
        CGS_ASSERT(lruNumEntries < KU_EM_MAX_ENTRIES, "muNumEntries < KU_MAX_DEFORMATION_MODELS");

        const u32 luEntry = lruNumEntries;

        // Volume id: a3[2*n+2] = the per-car volume id (the asm copies a 64-bit id from &v14-1; the
        // id source is this car's deformable-object index region). FLAG: the exact id word is the
        // entry's base volume id; carried from the deformable-object index.
        *reinterpret_cast<u64*>(lpcOutEM + KU_EM_VOLUME_ID_BASE + KU_EM_VOLUME_ID_STRIDE * luEntry) =
            static_cast<u64>(mu16DeformableObjectIndex);

        // Wheel state: a3[100*n+60] = the assembled WheelPhysicalStates block (homed operator=).
        WheelPhysicalStates* lpDst = reinterpret_cast<WheelPhysicalStates*>(
            lpcOutEM + KU_EM_WHEEL_STATE_BASE + KU_EM_WHEEL_STATE_STRIDE * luEntry);
        *lpDst = lWheelStates;

        // ++muNumEntries.
        ++lruNumEntries;
    }
}
}
