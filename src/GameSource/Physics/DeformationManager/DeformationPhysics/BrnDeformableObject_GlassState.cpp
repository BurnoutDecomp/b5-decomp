#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject.h"

#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"  // GlassPaneSpec / WheelSpec / GetGlassPaneSpec
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"          // EGlassState, GlassSmashOrCrackEvent, DeformationOutputInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationState.h"                    // CarState
#include "GameSource/Physics/VehicleManager/VehiclePhysics/BrnSimpleVehiclePhysics.h"              // SimpleVehiclePhysics::GetGraphicsVehicleTransform / GetWheelsWorldTransfrom
#include "GameShared/GameClasses/Physics/Deformation/BrnWheelPhysicalStates.h"                     // WheelPhysicalStates
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedPartManager.h"       // DetachedPartManager -> PhysicalBodyPart (UpdateAndOutputJointStates)
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnDetachedWheelManager.h"      // DetachedWheelManager::GetWheel -> PhysicalWheel (OutputWheelData's detached arm)
#include "GameShared/GameClasses/Core/CgsAssert.h"                                                 // CGS_ASSERT

#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::{Subtract, MagnitudeSquared, ...}
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::{TransformPoint, TransformVector}

#include <cstring>   // std::memset (the FLAGGED zero-seed of the un-homed wheel-state scratch)
#include <cmath>     // std::sqrt  (the crack-amount remap's vrsqrtefp + Newton refine converges to this)

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
//  * ⭐ THE GLASS BAND IS FULLY RECOVERED -- no flagged zeros remain in this file's glass path.
//    The two thresholds UpdateGlassSmashedState compares against (&unk_82FB9BC0 = SMASH,
//    &unk_82FB9AD0 = CRACK) are .bss dyn-init slots, so they read 0 out of the image BY
//    DEFINITION; the values come from their store thunks at 0x82C5DA38 / 0x82C5DA80, each of
//    which is literally `lvx128 the constant; vmulfp128 v0,v0,v0; stvx128 the threshold` -- the
//    thresholds are the SELF-SQUARES of the two SendGlassUpdateEvents remap constants
//    (0.05^2 = 0.0025 and 0.001^2 = 1e-06, both read from .rdata at 0x820047C8 / 0x82013F90).
//    The pane is scaled by a vcfsx(2)^2 == 4.0 factor before the compare (vspltisw v0,2; vcfsx;
//    vmulfp128 v127,v0,v0 -> 4.0) -- that 4.0 IS visible in the asm and is reproduced. Since the
//    state machine SUMS four corners' squared displacement, that 4.0 makes the compare a MEAN:
//    the pane cracks at 1 mm of average control-point displacement and smashes at 5 cm.
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
        // RECOVERED rodata (history kept because the zeros here were NOT inert -- see the ⚠️ below):
        // the two per-pane displacement thresholds the smash-state
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

        // ⭐ THE REMAP IS TRANSCRIBED (2026-09-06). It was carried as an honest zero for two waves
        // because the CODE, not the numbers, was missing. Decoded operand by operand out of
        // SendGlassUpdateEvents' `if (state == CRACKED)` arm (the vrsqrtefp/vrefp block), reading
        // classic vmaddfp/vnmsubfp/vsel in RAW FIELD ORDER D,A,B,C:
        //     vrsqrtefp v13,v12                ; r0   = rsqrt_est(maxDispSq)
        //     vcmpeqfp  v4, v8, v12            ; mask = (maxDispSq == 0)          <- the zero guard
        //     vcfsx     v5, v10, 1  / v0,v10,0 ; 0.5  /  1.0   (from vspltisw v10,1)
        //     vmulfp128 v2, v13, v13           ; r0^2
        //     vnmsubfp  v7, v12, v0, v2        ; 1 - maxDispSq*r0^2       (D = B - A*C)
        //     vmulfp128 v5, v13, v5            ; 0.5*r0
        //     vmaddfp   v13, v5, v13, v7       ; r1 = r0 + 0.5*r0*(1 - x*r0^2)    (D = A*C + B)
        //     vmulfp128 v13, v12, v13          ; x*r1 == sqrt(maxDispSq)
        //     vsel      v7, v13, v8, v4        ; (x == 0) ? 0 : sqrt(x)   (D = C ? B : A)
        //     vsubfp    v11, v0, v9            ; SPAN     = 0.05 - 0.001
        //     vrefp/vnmsubfp/vmaddfp x2        ; 1/SPAN, two Newton refines
        //     vsubfp    v12, v7, v9            ; sqrt(maxDispSq) - 0.001
        //     vmulfp128 v13, v13, v12          ; t = (sqrt(maxDispSq) - 0.001) / SPAN
        //     vmaxfp/vminfp v8 / v0            ; clamp01(t)
        //     vsubfp/vmulfp128/vsubfp          ; 1 - (1 - clamp01(t))^2
        // so the whole thing is a LINEAR RAMP in displacement DISTANCE across the band, eased out.
        //
        // ⭐⭐ IT CORROBORATES ITSELF, and that is why it can be landed without inventing anything:
        // the two remap endpoints are EXACTLY the two state-machine band edges. UpdateGlassSmashedState
        // compares against unk_82FB9AD0 == unk_82FB9DF0^2 == 0.001^2 (crack) and unk_82FB9BC0 ==
        // unk_82FB9650^2 == 0.05^2 (smash) -- the dyn-init thunks at 0x82C5DA38 / 0x82C5DA80 are
        // literally `lvx128 the constant; vmulfp128 v0,v0,v0; stvx128 the threshold`. A pane is
        // CRACKED exactly while its displacement sits between 1 mm and 5 cm, and the crack amount
        // ramps 0 -> 1 across exactly that interval. Two independently recovered pairs of numbers
        // meeting at the same two edges is not a coincidence a fabricated formula could produce.
        // (Aggregate differs by design: the state machine sums the four corners' squared displacement
        // and scales the threshold by 4 -- i.e. compares the MEAN; the remap takes the MAX corner.)
        static const f32 KF_GLASS_CRACK_REMAP_LO = 0.001000000047497451f;  // unk_82FB9DF0 <- flt_82013F90 @82C5DA58
        static const f32 KF_GLASS_CRACK_REMAP_HI = 0.05000000074505806f;   // unk_82FB9650 <- flt_820047C8 @82C5DA10

        // -------------------------------------------------------------------------------------
        // Console byte offsets the X360 image indexes through raw pointer arithmetic. They hold on
        // the 32-bit console; on the x64 host the absolute offsets do NOT all reproduce, but the
        // STRIDES + relative offsets the serialisers walk are faithful. Used only where the target
        // struct interior is not (yet) homed -- the homed members are reached BY NAME.
        // -------------------------------------------------------------------------------------
        // DeformableObject member offsets (console).
        // ⛔⛔ X360 VALUES -- REFERENCE ONLY, NEVER IN HOST POINTER ARITHMETIC (2026-09-06, glass
        // wave). maTagPoints and maDrivenPoints are homed BY-VALUE members, and EVERY OTHER reader
        // in the tree already spells them `maTagPoints[i]` / `maDrivenPoints[i]`. This file was the
        // last holdout: SIX reads used `this + 15120 + 32*i` / `this + 19232 + 48*i` -- the tag and
        // driven pair inside each of UpdateGlassSmashedState and SendGlassUpdateEvents, plus one
        // wheel-tag read in each of OutputState and GetWheelTagPoints -- and BOTH the
        // seat and the stride are wrong on x64. ⭐ MEASURED, run glassfix_A, printed by the
        // [glassspec] probe below out of the live object:
        //       host tagSeat 16832 stride 48   drivenSeat 22992 stride 64
        //     (console          15120       32              19232       48)
        // The seats are out by 1,712 and 3,760 bytes -- mLocatorData, mImpulsePasser, mVehicleBody
        // and maDeformationSensors[20] all sit between `this` and maTagPoints and all contain
        // pointers -- and the strides are out by 50% and 33% (TagPoint is {Vector3, 3 ptrs, f32}:
        // 32 bytes on a 32-bit console, 48 here; IKDrivenPoint 48 -> 64). For the highest live tag
        // index the old expression missed by 3.3 KB, i.e. it read a DIFFERENT MEMBER; the glass
        // pair then took *(element+16) as a TagPointSpec* and DEREFERENCED it. Kept below purely as
        // the figures for reading the asm, exactly like KU_SENSOR_ARRAY_OFFSET /
        // KU_VEHICLE_PHYSICS_PTR.
        static const u32 KU_TAG_POINT_ARRAY_OFFSET   = 15120;  // reference only -- &maTagPoints[0]  (console stride 32)
        static const u32 KU_TAG_POINT_STRIDE         = 32;     // reference only -- console sizeof(TagPoint)
        static const u32 KU_DRIVEN_POINT_ARRAY_OFFSET = 19232; // reference only -- &maDrivenPoints[0] (console stride 48)
        static const u32 KU_DRIVEN_POINT_STRIDE      = 48;     // reference only -- console sizeof(IKDrivenPoint)
        static const u32 KU_TAG_POINT_SPEC_PTR       = 16;     // reference only -- TagPoint::mpSpec
        static const u32 KU_TAG_SPEC_REST_POS_OFFSET = 32;     // reference only -- TagPointSpec::mInitialPositionAndDetachThreshold
        static const u32 KU_DRIVEN_REST_POS_OFFSET   = 32;     // reference only -- IKDrivenPoint::mpSpec (-> mInitialPos @ +0)

        // GlassPaneSpec member offsets (console; sizeof 112).
        static const u32 KU_GLASS_POINT_INDEX_0      = 80;     // maiPointIndex[0]  (s16; [1]=82,[2]=84,[3]=86)
        static const u32 KU_GLASS_SKIN_FLAG_0        = 88;     // mabSkinToControlPoint[0] (bool; [1]=89,[2]=90,[3]=91)

        // CarState write offsets OutputState fills (console; CarState interior not fully homed).
        static const u32 KU_CARSTATE_SENSOR_ARRAY    = 32;     // &maSensors[0] write base (80-byte stride)
        static const u32 KU_CARSTATE_SENSOR_STRIDE   = 80;     // per-sensor record stride
        static const u32 KU_CARSTATE_WHEEL_BASE      = 1600;   // the 4 wheel handling-transform rows
        static const u32 KU_CARSTATE_WHEEL_STRIDE    = 16;     // one row per wheel slot
        // ⭐ These two are now REFERENCE ONLY (2026-09-06, widening-sweep wave): both members are
        // homed and named in BrnDeformationState.h, so OutputState reaches them as
        // mfSummedDisplacementSquared / mu8NumSensors. Kept as the figures for reading the asm
        // (`stfs f0, 0x6A0(r4)` @0x825C1F7C and `stb r11, 0x6A4(r4)` @0x825C1EDC -- note the
        // console stores a BYTE at 0x6A4, which is what retired the old 4-byte write there).
        static const u32 KU_CARSTATE_SCRATCH_SUM     = 1696;   // reference only -- mfSummedDisplacementSquared
        static const u32 KU_CARSTATE_SENSOR_COUNT    = 1700;   // reference only -- mu8NumSensors (a BYTE)
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

        // ⭐ RECOVERED 2026-08-24 (deform-land wave): unk_82FB8070 = splat(flt_820049E0 = 100.0),
        // static-init writer @0x82C5D660 (headless idat xref decode). Consumed by OutputWheelData's
        // sphere-size tripwire, UpdateAndOutputJointStates' detached-part radius clamp
        // (@0x82609CE8/0x82609D28) and DeformableObject_Update's SetEntityRadius tripwire
        // (@0x82649290).
        static const f32 KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE = 100.0f;
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
            // ⭐⭐ BY NAME as of 2026-09-06 (glass wave) -- SEE THE X64-WIDENING BANNER AT
            // KU_TAG_POINT_ARRAY_OFFSET. This was `this + 15120 + 32*index` / `this + 19232 +
            // 48*index`: CONSOLE seats and CONSOLE strides applied to a WIDENED host object, so
            // neither the base nor the stride landed on a point. The tag-point read then took
            // *(element+16) -- a field of whatever member actually sits at +15120 -- as a
            // TagPointSpec pointer and dereferenced it. Same defect class, same file, as the
            // `*(this + 6476)` vehicle read fixed in the deform-land wave 12 lines below.
            Vector3 lLivePosition;
            Vector3 lRestPosition;
            if (lu8SkinFlag)
            {
                // Skinned: the TagPoint table. element = maTagPoints[index]; live = element.mPos
                // (element +0); rest = *(element.mpSpec + 32) == TagPointSpec::GetInitialPosition.
                const TagPoint& lrTagPoint = maTagPoints[li16PointIndex];
                lLivePosition = lrTagPoint.GetPosition();
                lRestPosition = lrTagPoint.GetInitialPosition();
            }
            else
            {
                // Driven: the IKDrivenPoint table. element = maDrivenPoints[index]; live = element +0;
                // rest = *(*(element + 32)) -- console +0x20 is mpSpec and its leading vector is
                // IKDrivenPointSpec::mInitialPos (asm: _R10 = *(_R11+32); lvx128 v12, r0, r10).
                const IKDrivenPoint& lrDrivenPoint = maDrivenPoints[li16PointIndex];
                lLivePosition = lrDrivenPoint.GetPosition();
                lRestPosition = lrDrivenPoint.GetOriginalPosition();
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
    // shaping reproduces the asm's outer "1 - (1 - clamp)^2" form gated on the CRACKED (==1) state,
    // and ⭐ as of 2026-09-06 the INNER REMAP too: the vrsqrtefp/vrefp block is transcribed operand
    // by operand into a linear ramp of |maxDisplacement| across [unk_82FB9DF0, unk_82FB9650] ==
    // [0.001, 0.05], both read out of the image and both equal to the state machine's own two band
    // edges. The vsel (maxDispSq == 0) NaN guard is the console's and is reproduced. The queue
    // appends call the homed
    // BaseEventQueue::AddEventSafe BY NAME on the two glass queues (render-side @ +6896, entity-
    // module-side @ +15920).
    // ===========================================================================================
    void DeformableObject::SendGlassUpdateEvents(s32 liPaneIndex, DeformationOutputInterface* lpOut,
                                                 DeformationOutputInterfaceForEntityModules* lpOutEM)
    {
        const GlassPaneSpec* lpPaneSpec = mpDeformationSpec->GetGlassPaneSpec(liPaneIndex);

        const char* lpcPane    = reinterpret_cast<const char*>(lpPaneSpec);

        // The attached vehicle physics (the asm's *(this+6476)). Two transforms are used: the
        // graphics transform (GetGraphicsVehicleTransf result v71) rotates the pane normal into
        // world; the vehicle's physics transform at +16 (the asm's *(this+6476)+16) transforms the
        // live corner control-points into world.
        const char* lpcVehicle = reinterpret_cast<const char*>(mVehicleBody.GetVehiclePhysics());
        // ^ BY NAME (fixed 2026-08-24, deform-land wave): the old `*(this + 6476)` read used the
        // CONSOLE offset on the HOST object -- every pointer above the seat widens on x64, so it
        // dereferenced garbage. BOOT-MEASURED: first OutputData frame AV'd at OutputWheelData+0x67
        // (fault 0x123587, event log -> map). KU_VEHICLE_PHYSICS_PTR stays as asm provenance only.
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

            // ⭐⭐ BY NAME as of 2026-09-06 (glass wave), same fix and same reason as
            // UpdateGlassSmashedState above: console seats + console strides on a widened host
            // object reached neither table.
            Vector3 lLivePosition;
            Vector3 lRestPosition;
            if (lu8SkinFlag)
            {
                const TagPoint& lrTagPoint = maTagPoints[li16PointIndex];
                lLivePosition = lrTagPoint.GetPosition();
                lRestPosition = lrTagPoint.GetInitialPosition();
            }
            else
            {
                const IKDrivenPoint& lrDrivenPoint = maDrivenPoints[li16PointIndex];
                lLivePosition = lrDrivenPoint.GetPosition();
                lRestPosition = lrDrivenPoint.GetOriginalPosition();
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

        // Crack amount: gated on the pane being CRACKED (the asm's `if (v11 == 1)`). The console
        // shapes 1 - (1 - clamp01(t))^2 over t = (|maxDisplacement| - 0.001) / (0.05 - 0.001), i.e.
        // a linear ramp in displacement DISTANCE across exactly the intact->cracked->smashed band,
        // eased out by the outer square. See the KF_GLASS_CRACK_REMAP_LO/HI banner above for the
        // operand-by-operand decode and for why the two endpoints are self-corroborating.
        //
        // MODELLED-vs-asm: the console reaches the square root as `vrsqrtefp` + ONE Newton refine
        // (r1 = r0 + 0.5*r0*(1 - x*r0^2)) then `x * r1`, and the 1/(0.05-0.001) divide as `vrefp` +
        // TWO Newton refines. Both converge to within a float ulp, so they are written here as the
        // exact sqrt/divide -- the same de-optimisation every committed sibling in this subsystem
        // uses for the identical idiom (BrnDeformationSensor.cpp, BrnPenetrationSolver.cpp,
        // BrnDeformationManager_VehicleContactFixUp.cpp).
        //
        // ⚠️ THE ZERO GUARD IS THE CONSOLE'S, NOT DEFENSIVE CODE WE ADDED. `vcmpeqfp v4, v8, v12`
        // builds a (maxDispSq == 0) mask and the `vsel v7, v13, v8, v4` substitutes 0 for the
        // sqrt result on that lane -- because vrsqrtefp(0) is +inf and 0 * inf is a NaN, which
        // would otherwise propagate all the way into the event's crack amount. Reproduced exactly.
        f32 lfCrackAmount = 0.0f;
        if (leNewState == E_GLASS_STATE_CRACKED)
        {
            // vrsqrtefp + Newton + the vsel zero-guard == a guarded sqrt.
            const f32 lfMaxDisplacement = (lfMaxDisplacementSq == 0.0f)
                                              ? 0.0f
                                              : std::sqrt(lfMaxDisplacementSq);

            // t = (|maxDisplacement| - LO) / (HI - LO), then clamp01 (vmaxfp 0 / vminfp 1.0).
            const f32 lfSpan     = KF_GLASS_CRACK_REMAP_HI - KF_GLASS_CRACK_REMAP_LO;  // vsubfp v11, v0, v9
            const f32 lfRemapped = (lfMaxDisplacement - KF_GLASS_CRACK_REMAP_LO) / lfSpan;

            f32 lfClamped = lfRemapped;
            if (lfClamped < 0.0f) lfClamped = 0.0f;
            if (lfClamped > 1.0f) lfClamped = 1.0f;
            const f32 lfOneMinus = 1.0f - lfClamped;
            lfCrackAmount = 1.0f - lfOneMinus * lfOneMinus;
        }
        lEvent.mfCrackAmount = lfCrackAmount;

        // ---------------------------------------------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. BRN_GLASS_PROBE=1 only. Read-only; deleted when the glass
        // question is banked.
        //
        // WHAT IT HAS TO DISTINGUISH, and why it is a HISTOGRAM and not a max: the failure this
        // replaces was a hard 0.0, so "did the remap light up" is answered by whether any bucket
        // ABOVE b0 is populated -- a max alone could not tell one saturated pane from a whole car
        // of them, and a mean could not tell "all zero" from "half zero". Buckets are on the crack
        // amount; the three counters above them bound the population the buckets came from. The
        // summary prints on EVERY smash/crack event once armed (state changes are rare -- a pane
        // changes state at most twice per wreck) and re-prints its zero row, so a silent probe is
        // distinguishable from a probe that armed and saw nothing.
        {
            static s32 siGlassProbe = -1;
            if ( siGlassProbe < 0 )
            {
                const char* lpcEnv = getenv("BRN_GLASS_PROBE");
                siGlassProbe = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
            }
            if ( siGlassProbe == 1 && CgsDev::Log::gpDebugPrint != 0 )
            {
                static u32 suEvents = 0, suCracked = 0, suSmashed = 0, suIntact = 0;
                static u32 suBucket[6] = { 0, 0, 0, 0, 0, 0 };
                static f32 sfMinDisp = 1e30f, sfMaxDisp = -1e30f;

                ++suEvents;
                if      ( leNewState == E_GLASS_STATE_CRACKED ) ++suCracked;
                else if ( leNewState == E_GLASS_STATE_SMASHED ) ++suSmashed;
                else                                            ++suIntact;

                if ( leNewState == E_GLASS_STATE_CRACKED )
                {
                    const f32 lfDisp = (lfMaxDisplacementSq > 0.0f) ? std::sqrt(lfMaxDisplacementSq) : 0.0f;
                    if ( lfDisp < sfMinDisp ) sfMinDisp = lfDisp;
                    if ( lfDisp > sfMaxDisp ) sfMaxDisp = lfDisp;

                    const f32 lfA = lfCrackAmount;
                    if      ( lfA <= 0.0f  ) ++suBucket[0];
                    else if ( lfA <  0.25f ) ++suBucket[1];
                    else if ( lfA <  0.5f  ) ++suBucket[2];
                    else if ( lfA <  0.75f ) ++suBucket[3];
                    else if ( lfA <  1.0f  ) ++suBucket[4];
                    else                     ++suBucket[5];
                }

                *CgsDev::Log::gpDebugPrint
                    << "[glass] pane " << liPaneIndex << " state " << static_cast<s32>(leNewState)
                    << " crack " << lfCrackAmount
                    << " maxDisp " << ((lfMaxDisplacementSq > 0.0f) ? std::sqrt(lfMaxDisplacementSq) : 0.0f)
                    << " | events " << suEvents
                    << " intact " << suIntact << " cracked " << suCracked << " smashed " << suSmashed
                    << " | crackAmt b0 " << suBucket[0] << " b1 " << suBucket[1] << " b2 " << suBucket[2]
                    << " b3 " << suBucket[3] << " b4 " << suBucket[4] << " b1.0 " << suBucket[5]
                    << " | dispRange " << ((sfMinDisp < 1e29f) ? sfMinDisp : 0.0f)
                    << " .. " << ((sfMaxDisp > -1e29f) ? sfMaxDisp : 0.0f)
                    << "\n";
            }
        }

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

        // Queue onto BOTH glass queues (render-side @ this-relative +6896, entity-module @ console
        // +15920). ⭐ BY NAME on both sides as of 2026-08-24 (deform-land wave): the old raw
        // `lpOutEM + 15920` cast was a CONSOLE offset -- on the host, maSkinData's widened
        // pointers push the queue past +15920, so the cast pointed into maLocatorData and every
        // AddEventSafe would have corrupted the locator table (latent while the EM interface had
        // no consumer; the readback landed this wave). Same defect class as the EM operator= fix.
        lpOut->mGlassSmashOrCrackQueue.AddEventSafe(lEvent);
        lpOutEM->GetGlassSmashOrCrackQueue().AddEventSafe(lEvent);
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
        // miNumGlassPanes lives at spec +32; the asm reads *(*(this+6368)+32). BY NAME as of
        // 2026-09-06 -- the record's members are public (it IS the on-disc image) and the header
        // static_asserts miNumGlassPanes @ +32, so the two spellings are the same read.
        const s32 liNumPanes = mpDeformationSpec->miNumGlassPanes;

        // ---------------------------------------------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY. BRN_GLASS_PROBE=1 only. Read-only; deleted when the glass
        // question is banked.
        //
        // WHY THIS EXISTS AND WHAT IT HAS TO SETTLE. 59702646 concluded from an AV's CODE OFFSET
        // (UpdateGlassSmashedState+0x3E) that maGlassPaneData was absent behind a non-zero pane
        // count. No faulting address was captured and the registers quoted were call-clobbered, so
        // that was an inference, not a measurement -- and there is a second, certain pointer bug in
        // the same 62 bytes (the console-seat tag/driven reads, fixed above). This prints the SPEC'S
        // OWN FOUR TABLE SLOTS side by side, so the three explanations separate on sight:
        //   * all four slots small and similar  -> FixUp never ran; the slots are still on-disc
        //     base-relative offsets (the record is 1712 bytes, so a real offset is >= 1712).
        //   * glass slot 0 while the other three are large  -> the pane array really is absent and
        //     the producer hunt is on (the 27 rig-less retail cars carry a GARBAGE glass slot with
        //     count 0 -- count 0 with a junk slot is NORMAL and is not this).
        //   * all four large and the pane loop runs  -> the AV was the tag/driven ghost.
        // It also prints the HOST seats of maTagPoints/maDrivenPoints so the console figures
        // 15120/19232 and strides 32/48 can be compared against what this build actually has.
        // Capped at 8 lines for the whole process: a per-object-per-frame probe would starve the
        // harness [[watch-the-window-asserts-pause]].
        {
            static s32 siSpecProbe = -1;
            if ( siSpecProbe < 0 )
            {
                const char* lpcEnv = getenv("BRN_GLASS_PROBE");
                siSpecProbe = (lpcEnv != 0 && atoi(lpcEnv) > 0) ? 1 : 0;
            }
            if ( siSpecProbe == 1 && CgsDev::Log::gpDebugPrint != 0 )
            {
                static u32 suSpecLines = 0;
                if ( suSpecLines < 8u )
                {
                    ++suSpecLines;
                    const StreamedDeformationSpec* lpSpec = mpDeformationSpec;
                    const char* lpcObject = reinterpret_cast<const char*>(this);
                    *CgsDev::Log::gpDebugPrint
                        << "[glassspec] obj " << static_cast<s32>(mu16DeformableObjectIndex)
                        << " spec " << static_cast<void*>(const_cast<StreamedDeformationSpec*>(lpSpec))
                        << " sizeof " << static_cast<u64>(sizeof(StreamedDeformationSpec))
                        << " | tag slot "    << lpSpec->maTagPointData.muSlot
                        << " n " << lpSpec->miNumberOfTagPoints
                        << " | driven slot " << lpSpec->maDrivenPointData.muSlot
                        << " n " << lpSpec->miNumberOfDrivenPoints
                        << " | ik slot "     << lpSpec->maIKPartData.muSlot
                        << " n " << lpSpec->miNumberOfIKParts
                        << " | GLASS slot "  << lpSpec->maGlassPaneData.muSlot
                        << " n " << lpSpec->miNumGlassPanes
                        << " ptr " << static_cast<void*>(lpSpec->maGlassPaneData.Get())
                        << " | live tags " << miNumTagPoints << " driven " << miNumDrivenPoints
                        << " | host tagSeat "
                        << static_cast<u64>(reinterpret_cast<const char*>(&maTagPoints[0]) - lpcObject)
                        << " stride " << static_cast<u64>(sizeof(TagPoint))
                        << " drivenSeat "
                        << static_cast<u64>(reinterpret_cast<const char*>(&maDrivenPoints[0]) - lpcObject)
                        << " stride " << static_cast<u64>(sizeof(IKDrivenPoint))
                        << " (console 15120/32, 19232/48)\n";
                }
            }
        }

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
        const char* lpcSpec     = reinterpret_cast<const char*>(mpDeformationSpec);

        // (1) sensor count: spec +1618 (mu8NumDeformationSensors) -> CarState::mu8NumSensors.
        // ⭐ BY NAME 2026-09-06 (widening-sweep wave). This was
        //     *reinterpret_cast<s32*>(lpcCarState + 1700) = liNumSensors;
        // and it wrote FOUR bytes over a ONE-byte member: Deformation::CarState declares
        // `u8 mu8NumSensors` at +0x6A4 (1700) followed immediately by `u8 maTailPad[11]`
        // (BrnDeformationState.h:83-85), so three bytes of the tail pad were clobbered on every
        // call. It read back correctly only because x64 is little-endian and the count is < 256,
        // so the low byte happened to land on the member -- the console's own reader
        // (CarState::GetSensor @0x825B3678) `lbz`s exactly this one byte.
        // ⚠️ NOT a widening ghost: CarState is an explicitly-padded POD with NO pointers and
        // `static_assert(sizeof(CarState) == 1712)` in BrnDeformationState_DeformationState.cpp,
        // so 1700 IS the host offsetof. The offset was right; the WIDTH was not.
        const s32 liNumSensors = *reinterpret_cast<const u8*>(lpcSpec + 1618);
        lpCarState->mu8NumSensors = static_cast<u8>(liNumSensors);

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

        // The accumulated scratch displacement sum. BY NAME 2026-09-06 (widening-sweep wave);
        // was `*(f32*)(lpcCarState + 1696)`. CarState::mfSummedDisplacementSquared is at exactly
        // +0x6A0 == 1696 (BrnDeformationState.h:82) and the struct is pointer-free, so this was
        // the right byte -- it is spelled by name so it STAYS the right byte.
        lpCarState->mfSummedDisplacementSquared = lfScratchSumSq;

        // (3) copy the four wheel handling rows from the attached vehicle (vehicle +1744) into the
        // CarState wheel block (+1600..). The asm copies four 16-byte rows.
        const char* lpcVehicle = reinterpret_cast<const char*>(mVehicleBody.GetVehiclePhysics());
        // ^ BY NAME (fixed 2026-08-24, deform-land wave): the old `*(this + 6476)` read used the
        // CONSOLE offset on the HOST object -- every pointer above the seat widens on x64, so it
        // dereferenced garbage. BOOT-MEASURED: first OutputData frame AV'd at OutputWheelData+0x67
        // (fault 0x123587, event log -> map). KU_VEHICLE_PHYSICS_PTR stays as asm provenance only.
        const char* lpcHandling = lpcVehicle + KU_VEHICLE_HANDLING_ROWS;
        // ⚠️ OPEN, RAISED 2026-09-06 (widening-sweep wave) -- NOT RESOLVED HERE, AND NOT TOUCHED.
        // Two banners in this tree disagree about how wide this copy is, and raw offsets are why
        // nobody has had to notice:
        //   * BrnDeformationState.h:71-73 calls +0x640 a "deformed-bbox pair (32 bytes)" and homes
        //     it as mDeformedBBoxMin/mDeformedBBoxMax, with maWheelTagPoints[4] starting at +0x660.
        //   * this loop writes FOUR 16-byte rows from +0x640, i.e. +0x640..+0x680 -- so rows 2 and
        //     3 land on maWheelTagPoints[0] and [1], which step (4) below then writes again.
        // The console sets up BOTH cursors together (@0x825C1F78 `addi r10,r4,0x640` and
        // @0x825C1F94 `addi r9,r4,0x660`, source @0x825C1F88 `addi r9,r11,0x6D0`), so the overlap
        // may be real and order-dependent, or this loop may simply be one iteration count too
        // long. Settling it needs the +0x640 cursor's own loop read out of OutputState @0x825C1EA8;
        // that is an OutputState PARITY question, not an x64-widening one, so this wave leaves it
        // stated rather than guessed. ⭐ Note WHAT SURFACED IT: spelling the neighbouring writes by
        // name made two writes to the same bytes visible; as raw offsets they were invisible.
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
            // ⭐⭐ BY NAME as of 2026-09-06 (glass wave): this read used the console seat/stride on
            // the widened host object, so every wheel tag point written into the CarState was a
            // slice of whatever member actually sits at host +15120. Silent -- unlike the glass
            // path it never dereferenced what it read, so it produced wrong numbers, not a fault.
            Vector3 lTagPoint = maTagPoints[lpWheelSpec->liTagPointIndex].GetPosition();
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

        const char* lpcVehicle = reinterpret_cast<const char*>(mVehicleBody.GetVehiclePhysics());
        // ^ BY NAME (fixed 2026-08-24, deform-land wave): the old `*(this + 6476)` read used the
        // CONSOLE offset on the HOST object -- every pointer above the seat widens on x64, so it
        // dereferenced garbage. BOOT-MEASURED: first OutputData frame AV'd at OutputWheelData+0x67
        // (fault 0x123587, event log -> map). KU_VEHICLE_PHYSICS_PTR stays as asm provenance only.
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
                // ⭐⭐ BY NAME as of 2026-09-06 (glass wave) -- see OutputState above; the console
                // seat/stride never reached maTagPoints on the widened host object.
                const Vector3 lTagPointLocal = maTagPoints[liTagPointIndex].GetPosition();

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
        // BY NAME (fixed 2026-08-24, deform-land wave): the old `*(this + 6476)` read used the
        // CONSOLE offset on the HOST object -- every pointer above the seat widens on x64, so it
        // dereferenced garbage. BOOT-MEASURED: first OutputData frame AV'd at OutputWheelData+0x67
        // (fault 0x123587, event log -> map). KU_VEHICLE_PHYSICS_PTR stays as asm provenance only.
        // (2026-08-25: the raw lpcThis/lpcVehicle views are gone with the by-name wheel walk.)
        Vehicle::SimpleVehiclePhysics* lpSimple =
            reinterpret_cast<Vehicle::SimpleVehiclePhysics*>(mVehicleBody.GetVehiclePhysics());

        // The wheel detach flag the asm reads once before the loop (*(spec+112) == -1).
        const char* lpcSpec  = reinterpret_cast<const char*>(mpDeformationSpec);
        const bool  lbApplySteer = (*reinterpret_cast<const s32*>(lpcSpec + 112) == -1);

        // ⭐⭐ CORRECTED 2026-08-24 (deform-land wave; BOOT-MEASURED -- the old model fired the
        // sphere-size tripwire 799 times in one run). The running bound v127 is NOT a scalar
        // seeded from zero and folded with |wheel WORLD position| (a coordinate magnitude, ~3000 m
        // in Paradise City): the asm SEEDS it from this car's OWN entity radius
        // (this+0x66D0 == mLastLinearVelocityPlusEntityRadius, w lane), folds per-wheel
        // DISTANCE-FROM-VEHICLE + radius exactly like UpdateAndOutputJointStates' detached-part
        // fold (the same vaddfp/vcmpgtfp/vmaxfp/vsel cascade @0x8260914C..0x8260915C), and at the
        // tail min-clamps + stores it BACK INTO THE MEMBER (vrlimi mask 1 @0x8260919C ->
        // stvx this+0x66D0) before asserting THE MEMBER.
        f32 lfEntitySphereSize = mLastLinearVelocityPlusEntityRadius.w;

        // The assembled per-wheel scratch state block (the local v61 the asm copies out). FLAG:
        // 0x188 bytes; its interior is the un-homed entity-module per-wheel physical state. Zeroed
        // (honest seed) -- NEVER fabricated. PROMOTE when that layout is homed.
        WheelPhysicalStates lWheelStates;
        std::memset(&lWheelStates, 0, sizeof(lWheelStates));

        // Per-wheel transform assembly (the do/while over the four wheels; the asm walks
        // `vehicle + 304 + 224*wheel` and tests the detached discriminator at +215 from the
        // per-wheel base -- i.e. maWheels[wheel].mu8State: SimpleVehiclePhysics' console
        // wheel array is at +0x130 stride 0xE0 and the state byte is Wheel+0xD7, all three
        // pinned in BrnSimpleVehiclePhysics.h's X360Layout table).
        //
        // ⭐⭐ BY NAME (wheel-blank regression fix, 2026-08-25). The old transcription kept
        // the RAW CONSOLE walk on the HOST object -- the same defect class this function's
        // own vehicle-pointer read was fixed for on 08-24, one paragraph up: the host Wheel
        // carries a widened pointer at +0xD0, so BOTH the stride (224 -> host sizeof(Wheel))
        // AND the state byte's offset moved, and `vehicle+304+224*w+215` read GARBAGE. A
        // garbage byte that happens to read 2 classifies the wheel DETACHED, skips the live
        // arm, and publishes an all-zero row -- which the L3 readback then copies over the
        // real wheel pose: the car renders WHEEL-LESS. This is exactly how the user-visible
        // "no tyres while driving" regression escaped the 08-24 wave's own fix -- that wave
        // fixed the zero-SEEDED block but left the discriminator read raw, so whether a
        // wheel published depended on uninitialised host bytes that flip once the car is
        // driving. (The boot [carrender] snapshot passes because the stand-in pose owns the
        // wheels before the deformation entries exist.)
        for (s32 liWheel = 0; liWheel < 4; ++liWheel)
        {
            const u8 lu8WheelPhysState =
                lpSimple->GetWheel(static_cast<Vehicle::EVehicleDrivenWheel>(liWheel))->mu8State;

            if (lu8WheelPhysState == 2)
            {
                // ---- DETACHED arm, LANDED 2026-09-02 (deform close-out wave) ----
                // 0x82608F74  bl sub_825E8308 == DetachedWheelManager::GetWheel(EntityId, liWheel)
                //   (r4 = the handling id's entity word, r5 = the wheel index; the callee matches
                //   owner byte, entity index AND the packed part index == wheel index).
                // 0x82608FDC..0x82608FFC  the record's four render-transform rows -> the output row
                //   (+0x00..+0x30), rec+0x60 mLinearVelocity -> +0x40, a zero vector -> +0x50.
                // 0x82609000..0x826090D0  wheels 0 and 2 (the LEFT pair) are pre-multiplied by
                //   lInverseX (DWARF :3445) == diag(-1, 1, -1) with a zero translation row, i.e.
                //   (-xAxis, yAxis, -zAxis, wAxis): the wheel mesh is authored for the right side.
                // 0x82609114 / 0x82609118  stb 1,-4 ; stb 0,0  ->  exists = 1, attached = 0.
                // 0x826090D4..0x82609154  |rec pos - car pos| (vrsqrtefp + two Newton steps,
                //   zero-guarded) ; radius = rec+0x7C ; if (dist <= KVF_MAX) size = max(size,
                //   dist + radius) -- the SAME fold as the live arm, with the radius term.
                // r4 = the high dword of the handling id (RigidBodyId::GetEntityId, DWARF :3442).
                EntityId lVehicleEntityId;
                lVehicleEntityId.muValue = static_cast<u32>(static_cast<u64>(mHandlingBodyID) >> 32);
                const PhysicalWheel* lpPhysWheel = lpWheelMgr->GetWheel(lVehicleEntityId, liWheel);   // :3442
                if (lpPhysWheel != 0)
                {
                    Matrix44Affine lWheelTransform = *lpPhysWheel->GetRenderTransform();
                    if (liWheel == 0 || liWheel == 2)
                    {
                        // lInverseX * M, row-vector convention: rows x and z negated, y and the
                        // translation row untouched (the four vmaddfp chains @0x82609090..0x826090C0).
                        lWheelTransform.xAxis.x = -lWheelTransform.xAxis.x;
                        lWheelTransform.xAxis.y = -lWheelTransform.xAxis.y;
                        lWheelTransform.xAxis.z = -lWheelTransform.xAxis.z;
                        lWheelTransform.zAxis.x = -lWheelTransform.zAxis.x;
                        lWheelTransform.zAxis.y = -lWheelTransform.zAxis.y;
                        lWheelTransform.zAxis.z = -lWheelTransform.zAxis.z;
                    }
                    lWheelStates.maStates[liWheel].mWorldSpaceTransform       = lWheelTransform;
                    lWheelStates.maStates[liWheel].mWorldSpaceVelocity        = lpPhysWheel->GetLinearVelocity();
                    lWheelStates.maStates[liWheel].mWorldSpaceAngularVelocity = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
                    lWheelStates.mabWheelExists[liWheel]   = true;    // stb 1, -4
                    lWheelStates.mabWheelAttached[liWheel] = false;   // stb 0,  0

                    const Vector3 lToWheel = vpu::Subtract(lpPhysWheel->GetRenderTransform()->Pos(),
                                                           lpSimple->GetTransform().Pos());
                    const f32 lfDist = vpu::Magnitude(lToWheel);
                    if (lfDist <= KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE)
                    {
                        const f32 lfReach = lfDist + lpPhysWheel->GetRadius();
                        if (lfReach > lfEntitySphereSize)
                        {
                            lfEntitySphereSize = lfReach;
                        }
                    }
                }
                // No record (the slot was reclaimed): the row stays zero-seeded, exists = 0 --
                // the console leaves the block's seed too (0x82608F7C beq -> loc_82609160).
            }
            else
            {
                // Live: the attached vehicle's world wheel transform (homed call).
                const Matrix44Affine lWheelTransform =
                    lpSimple->GetWheelsWorldTransfrom(static_cast<Vehicle::EVehicleDrivenWheel>(liWheel), lbApplySteer);

                // ⭐ THE LIVE WHEEL ROW IS WRITTEN NOW (2026-08-24, deform-land wave;
                // BOOT-MEASURED: with the block left zero-seeded, the newly-live readback L3
                // published zero transforms + exists=0 over UpdatePhysicsState's good wheel
                // poses and the car rendered WHEEL-LESS). The consumer contract
                // (ActiveRaceCar::UpdateWheelPhysicsState's snapshot view) is {64-byte
                // transform @ 96*wheel; on-ground byte @ 0x180+wheel}; those two fields are
                // filled per live wheel. FLAG: the remaining per-wheel scalars (velocities /
                // forces inside the 96-byte entry) stay zero-seeded until the interior is homed.
                // 2026-09-02 (traffic-deformation wave): BY NAME, now that WheelPhysicalStates
                // is homed -- and the SECOND byte the console's live arm writes is restored:
                //     0x82609114  stb r24, -4(r11)   mabWheelExists[w]   = 1
                //     0x82609118  stb r29,  0(r11)   mabWheelAttached[w] = 1
                // (r11 = block + 0x184 + w). The old raw-offset transcription wrote only the
                // exists byte; TrafficEntityModule::ProcessDeformationData reads
                // "exists && !attached" as a torn-off wheel and marks the car FATALLY CRASHING,
                // so with the attached byte left zero every physical traffic car would have been
                // flagged fatal on its first deformation frame. The detached-wheel arm above
                // (`stb 1,-4 ; stb 0,0`: exists, not attached) stays un-homed with its record.
                lWheelStates.maStates[liWheel].mWorldSpaceTransform = lWheelTransform;
                lWheelStates.mabWheelExists[liWheel]   = true;
                lWheelStates.mabWheelAttached[liWheel] = true;

                // Fold the wheel's DISTANCE FROM THE VEHICLE into the running entity radius (the
                // dist <= kMax gate + max(size, dist + r) cascade). FLAG: the per-wheel radius
                // term (v7) is not homed; folded as 0 -- exact for an attached wheel, an
                // under-estimate only for the not-yet-modelled detached arm.
                const Vector3 lToWheel = vpu::Subtract(lWheelTransform.Pos(),
                                                       lpSimple->GetTransform().Pos());
                const f32 lfDist = vpu::Magnitude(lToWheel);
                if (lfDist <= KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE
                    && lfDist > lfEntitySphereSize)
                {
                    lfEntitySphereSize = lfDist;
                }
            }
        }

        // -------------------------------------------------------------------------------------
        // [wheelreset] NOT IN THE X360 BINARY -- opt-in witness, BRN_WHEELRESET_PROBE=1 (0/unset
        // == inert). This is the ONE site in the program where BOTH SIDES of the "does the
        // respawned car have four wheels" question exist on the SAME FRAME for the SAME wheel:
        //   * the PHYSICS side -- Wheel::mu8State (0 attached / 1 twisting / 2 detached), read
        //     from maWheels[w] two lines above;
        //   * the PUBLISHED side -- mabWheelExists[w] / mabWheelAttached[w], which is what
        //     ActiveRaceCar::UpdateWheelPhysicsState and TrafficEntityModule::
        //     ProcessDeformationData actually consume ("exists && !attached" == torn off);
        //   * the MANAGER side -- whether DetachedWheelManager still holds a record for this
        //     vehicle+wheel, which is the state ResetDeformation's RemoveVehicleWheels clears.
        // A log line that showed only one of the three could say "on the road" while the screen
        // showed a three-wheeled car, and both would be true of different numbers.
        // Emits only when the 4-wheel signature CHANGES, so a whole run costs a handful of lines
        // and every transition (detach, reset, re-attach) is on the record.
        // DELETE-WHEN the respawn-after-wheel-loss question is banked on film.
        // -------------------------------------------------------------------------------------
        {
            static s32 siWheelResetProbe = -1;
            if ( siWheelResetProbe < 0 )
            {
                const char* lpcEnv = getenv("BRN_WHEELRESET_PROBE");
                siWheelResetProbe = ( lpcEnv != 0 && lpcEnv[0] != '0' ) ? 1 : 0;
            }
            if ( siWheelResetProbe == 1 && CgsDev::Log::gpDebugPrint != 0 )
            {
                EntityId lProbeVehicleId;
                lProbeVehicleId.muValue = static_cast<u32>(static_cast<u64>(mHandlingBodyID) >> 32);

                u32 luSignature = 0u;
                for ( s32 liW = 0; liW < 4; ++liW )
                {
                    const u32 luState = lpSimple->GetWheel(static_cast<Vehicle::EVehicleDrivenWheel>(liW))->mu8State;
                    const u32 luRec   = ( lpWheelMgr->GetWheel(lProbeVehicleId, liW) != 0 ) ? 1u : 0u;
                    const u32 luPub   = ( lWheelStates.mabWheelExists[liW] ? 2u : 0u )
                                      | ( lWheelStates.mabWheelAttached[liW] ? 1u : 0u );
                    luSignature = (luSignature << 8) | ((luState & 3u) << 4) | (luRec << 2) | (luPub & 3u);
                }

                static const void* sapProbeObj[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                static u32         sauProbeSig[8] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                                                      0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
                static u32         suProbeNext = 0u;

                s32 liProbeSlot = -1;
                for ( u32 luS = 0; luS < 8u; ++luS )
                {
                    if ( sapProbeObj[luS] == static_cast<const void*>(this) ) { liProbeSlot = static_cast<s32>(luS); break; }
                }
                if ( liProbeSlot < 0 )
                {
                    liProbeSlot = static_cast<s32>(suProbeNext % 8u);
                    ++suProbeNext;
                    sapProbeObj[liProbeSlot] = this;
                    sauProbeSig[liProbeSlot] = 0xFFFFFFFFu;
                }

                if ( sauProbeSig[liProbeSlot] != luSignature )
                {
                    sauProbeSig[liProbeSlot] = luSignature;
                    *CgsDev::Log::gpDebugPrint
                        << "[wheelreset] obj " << static_cast<s32>(mu16DeformableObjectIndex)
                        << " entity " << static_cast<s32>(lProbeVehicleId.muValue)
                        << " broken " << static_cast<s32>(miNumBrokenWheels)
                        << " force " << (mbForceWheelsToDetach ? 1 : 0);
                    for ( s32 liW = 0; liW < 4; ++liW )
                    {
                        *CgsDev::Log::gpDebugPrint
                            << " | w" << liW
                            << " state " << static_cast<s32>(lpSimple->GetWheel(static_cast<Vehicle::EVehicleDrivenWheel>(liW))->mu8State)
                            << " exists " << (lWheelStates.mabWheelExists[liW] ? 1 : 0)
                            << " attached " << (lWheelStates.mabWheelAttached[liW] ? 1 : 0)
                            << " rec " << ((lpWheelMgr->GetWheel(lProbeVehicleId, liW) != 0) ? 1 : 0);
                    }
                    *CgsDev::Log::gpDebugPrint << "\n";
                }
            }
        }

        // Tail: min-clamp + store the member's w lane (vminfp @0x8260917C; vrlimi mask 1 +
        // stvx this+0x66D0 @0x8260919C..0x826091A8), then the bounds tripwire ON THE MEMBER.
        // ⭐ KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE (unk_82FB8070) = splat(100.0), static-init
        // writer @0x82C5D660 -- the old "un-recovered rodata" flag is retired.
        if (lfEntitySphereSize > KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE)
        {
            lfEntitySphereSize = KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE;
        }
        mLastLinearVelocityPlusEntityRadius.w = lfEntitySphereSize;
        CGS_ASSERT(lfEntitySphereSize <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f),
                   "GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f)");

        // Write into the entity-module output interface entry BY NAME (2026-08-24: the interface
        // interior is homed now; the console offsets the asm indexes with are recorded in the
        // KU_EM_* constants above as provenance).
        const u32 luEntry = lpOutEM->GetNumEntries();

        // muNumEntries < KU_MAX_DEFORMATION_MODELS (non-gating tripwire).
        CGS_ASSERT(luEntry < KU_EM_MAX_ENTRIES, "muNumEntries < KU_MAX_DEFORMATION_MODELS");

        // Volume id: maBaseIDs[n] = the handling-body VolumeInstanceId with the low 32 bits
        // cleared -- asm 0x826091EC `ld r11, 0x6710(this)` (mHandlingBodyID) then 0x826091F4
        // `clrrdi r31, r11, 32` (keep only the entity word in the high dword) then the stdx.
        // ⭐ CORRECTED 2026-08-24 (deform-land wave): the old FLAG placeholder stored
        // mu16DeformableObjectIndex here, whose entity-owner byte is 0 -- the readback's L3 leg
        // (owner byte must be E_ENTITYTYPE_RACECAR == 1) would have rejected every entry.
        lpOutEM->SetBaseId(luEntry,
                           GetHandlingBodyVolumeInstanceId().muId
                               & CgsSceneManager::VolumeInstanceId::KU_ENTITY_ID_MASK);

        // Wheel state: a3[100*n+60] = the assembled WheelPhysicalStates block (homed operator=).
        *reinterpret_cast<WheelPhysicalStates*>(lpOutEM->GetWheelStateSlot(luEntry)) = lWheelStates;

        // ++muNumEntries.
        lpOutEM->SetNumEntries(luEntry + 1u);
    }

    // ===========================================================================================
    // RefreshEntitySphereSizeFromVehicleExtent  (console-INLINE slice of DeformationManager::
    // OutputData @0x826225D8, pass-2 preamble 0x82622A64..0x82622AC8; landed 2026-08-24,
    // deform-land wave -- this store was previously a SILENT DROP behind the "folded into
    // OutputWheelData" comment, which OutputWheelData's body never actually carried.)
    //
    // The asm: load the attached vehicle's half-extent vector (vehicle + 1696 ==
    // SimpleVehiclePhysics::mHalfExtent), compute its magnitude with the zero-guarded
    // rsqrt-Newton idiom (vmsum3fp / vrsqrtefp / two refines / vsel 0), and merge ONLY the w
    // lane into mLastLinearVelocityPlusEntityRadius (vrlimi128 v8, v0, 1, 0) -- i.e. the
    // entity sphere-size seed is |halfExtent| and the xyz (last linear velocity) lanes are
    // untouched.
    // ===========================================================================================
    void DeformableObject::RefreshEntitySphereSizeFromVehicleExtent()
    {
        const Vehicle::SimpleVehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();
        const f32 lfRadius = vpu::Magnitude(lpVehicle->GetHalfExtent());   // zero-guard folded (|0| == 0)
        mLastLinearVelocityPlusEntityRadius.w = lfRadius;
    }

    // ===========================================================================================
    // UpdateAndOutputJointStates  @ 0x82609AE8  (183 insns; landed 2026-08-24, deform-land wave)
    //
    // For each of this car's live PHYSICAL parts (mau8PhysicalBodyPartPoolIndex[0..
    // mi16NumPhysicalParts), pool slots resolved through the detached-part manager):
    //   - still JOINTED (part->IsJoinedToVehicle()): emit a JointedPartStateEvent
    //     { vehicle entity id (the handling-body volume id's entity word, `ld 0x6710` hi-dword),
    //       part type (ikPart->spec GetPartType, spec+476),
    //       joint rotation proportion (GetJointRotationProportion),
    //       hinge velocity (mLocalGraphicsPositionPlusJointVelocity.w, part+0x170 lane w) }
    //     via AddEventSafe onto lpOut->mJointedPartStateQueue (lpOut + 0x74).
    //   - DETACHED: grow the running entity sphere size to cover the part --
    //     dist = |part->GetPosition() - vehiclePos| (vehicle transform row 3, vehicle+0x40);
    //     if (dist <= KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE)
    //         size = max(size, dist + part->GetSphereRadius());
    // Tail: mLastLinearVelocityPlusEntityRadius.w = min(size, KVF_MAX) and the non-gating
    // tripwire "GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f)"
    // (BrnDeformableObject.cpp:3959). Per-part asserts as baked: "lpPart != NULL" (:3905),
    // "lpIKPart != NULL" (:3913), "IsJoinedToVehicle()" (BrnPhysicalBodyPart.h:276).
    // ===========================================================================================
    void DeformableObject::UpdateAndOutputJointStates(DeformationOutputInterface* lpOut,
                                                      DetachedPartManager* lpPartMgr)
    {
        f32 lfSphereSize = mLastLinearVelocityPlusEntityRadius.w;   // v127 = splat(+0x66D0 lane w)

        const Vehicle::SimpleVehiclePhysics* lpVehicle = mVehicleBody.GetVehiclePhysics();
        const Vector3 lVehiclePos = lpVehicle->GetTransform().Pos();  // v125 = vehicle+0x40

        const s32 liNumParts = static_cast<s32>(mi16NumPhysicalParts);
        for (s32 liPart = 0; liPart < liNumParts; ++liPart)
        {
            PhysicalBodyPart* lpPart =
                lpPartMgr->GetPartFromIndex(static_cast<u16>(mau8PhysicalBodyPartPoolIndex[liPart]));
            CGS_ASSERT(lpPart != 0, "lpPart != NULL");

            if (lpPart->IsJoinedToVehicle())
            {
                const IKBodyPart* lpIKPart = lpPart->GetIKPart();
                CGS_ASSERT(lpIKPart != 0, "lpIKPart != NULL");

                JointedPartStateEvent lEvent;
                lEvent.mVehicleId = EntityId{ static_cast<u32>(
                    GetHandlingBodyVolumeInstanceId().muId >> 32) };            // `ld 0x6710; srdi 32`
                lEvent.meType = lpIKPart->GetPartType();                        // *(ikPart+8) -> spec+0x1DC
                lEvent.mfCurrentOrientation = lpPart->GetJointRotationProportion().x;

                // The asm re-tests the jointed flag between the two reads (IsJoinedToVehicle(),
                // BrnPhysicalBodyPart.h:276) -- a non-gating tripwire.
                CGS_ASSERT(lpPart->IsJoinedToVehicle(), "IsJoinedToVehicle()");
                lEvent.mfHingeVelocity = lpPart->GetJointVelocity().x;          // part+0x170 lane w

                lpOut->mJointedPartStateQueue.AddEventSafe(lEvent);
            }
            else
            {
                // Detached: cover the part with the entity sphere (zero-guarded magnitude).
                const f32 lfDist =
                    vpu::Magnitude(vpu::Subtract(lpPart->GetPosition(), lVehiclePos));
                if (lfDist <= KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE)            // vcmpgtfp/vsel arm
                {
                    const f32 lfCover = lfDist + lpPart->GetSphereRadius();
                    if (lfCover > lfSphereSize)
                    {
                        lfSphereSize = lfCover;                                 // vmaxfp
                    }
                }
            }
        }

        // Tail: clamp + store the w lane only (vrlimi mask 1), then the sphere-size tripwire.
        if (lfSphereSize > KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE)               // vminfp
        {
            lfSphereSize = KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE;
        }
        mLastLinearVelocityPlusEntityRadius.w = lfSphereSize;

        CGS_ASSERT(lfSphereSize <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f),
                   "GetEntitySphereSize() <= (KVF_MAX_DEFORMABLE_OBJECT_SPHERE_SIZE + 1.0f)");
    }
}
}
