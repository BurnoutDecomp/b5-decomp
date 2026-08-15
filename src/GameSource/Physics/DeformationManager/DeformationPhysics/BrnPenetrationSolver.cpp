#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnPenetrationSolver.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu::{Dot, Max, Subtract, Mult, ...}
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::TransformPoint

// BrnPhysics::Deformation::PenetrationSolver -- the post-physics interpenetration resolver.
//
// Three X360 ARTIST functions are bodied here, reconstructed from the VMX128 asm in the dossier:
//   AddObject         @ 0x825B3CB0  (register a body's transform + solver weighting)
//   AddVehicleContact @ 0x825B3D50  (append one car-on-car penetration contact)
//   Solve             @ 0x825BAA58  (the iterative weighted push-apart resolver)
//
// The X360 build is VMX128 inline assembly over 16-byte SIMD rows. Per project convention
// (cf. BrnDeformableObject.cpp, BrnAbsorptionTable.cpp) the bodies below are the DE-SIMD'd
// scalar/Vector3 equivalents written against the frozen, typed members BY NAME -- no __asm, no
// raw offset pokes. The asm's lvx128/stvx128 base+displacement splits and the `16*(idx+117)`
// weighting index are just the compiler's encoding of `maObjectTransforms[idx]` /
// `mavfBodyWeighting[idx]` element access; the net observable effect is element-indexed array
// access, so the reconstruction trusts the frozen header's array members.
//
// SOLVE shape (both loops, store-for-store from the asm):
//   For each contact, transform the two stored contact points into world space by the two bodies'
//   current transforms (TransformPoint over maObjectTransforms[idx]), take the A->B vector, project
//   it onto the contact normal, and clamp that projection to >= 0 (vmaxfp against zero) -- the
//   non-negative penetration depth along the normal. The depth-scaled normal is the correction the
//   bodies must move along to separate.
//     * VEHICLE loop: BOTH bodies move. The correction is divided by the sum of the two bodies'
//       weights (1/(wA+wB), the asm's vrefp + two Newton-refinement steps -> de-optimised here to an
//       exact reciprocal) to give a shared push, then body A's Pos() is advanced and body B's Pos()
//       is retreated, each scaled by the per-body weight (vmaddfp / vsubfp into the two Pos rows).
//     * WORLD loop: only body A moves (B is the immovable world, and mPointOnB is already a world
//       point, so it is NOT transformed). Body A's Pos() is advanced by the full normal*depth.
//   The corrected Pos() is written straight back into maObjectTransforms, read out later via
//   GetUpdatedTransform.
//
// MODELLED-vs-ASM:
//   * The reciprocal weight blend is de-optimised from the X360 rsqrt/refinement estimate to an
//     exact 1/(wA+wB); numerically a touch tighter, never a placeholder.
//   * Per-body weighting split: A advances by `lDisp * weightA`, B retreats by `lDisp * weightB`
//     (the vmaddfp into A's row, the vsubfp out of B's row). The two stores' weight assignment is
//     the one register-juggling detail folded by name; the magnitudes and the +/- direction are
//     asm-faithful (vmaddfp = +, vsubfp = -).
//   * vmsum3fp128 is the 3-lane dot (broadcast VecFloat) -> taken as the scalar Dot here and
//     re-broadcast for the per-lane scales, matching the SDK's broadcast-VecFloat idiom.
// No unrecovered rodata: Solve loads no constant pool -- the only immediates are 1.0f (vcfsx v12)
// and 0.0f (vspltisw), both visible in the asm. NOTHING is fabricated.
//
// Callers (X360 xrefs): AddObject / Solve <- DeformationManager::SolvePenetration;
// AddVehicleContact <- DeformationManager::AddArticulatedJointContacts.

namespace BrnPhysics
{
namespace Deformation
{
    namespace vpu = rw::math::vpu;

    // -----------------------------------------------------------------------------------------
    // Construct -- X360-attested by the CreateIOBuffer<PenetrationSolver> instantiation
    // @0x825BC9E8, which inlines this whole body. After `Alloc(this, 260912, name)` the only
    // work it does is two zero stores:
    //     lis r10,3 ; ori r10,r10,0xFB20 ; stwx r28,r11,r10   -> +0x3FB20 (260896) = 0
    //     lis  r9,3 ; ori  r9, r9,0xFB24 ; stwx r28,r11, r9   -> +0x3FB24 (260900) = 0
    // which are miNumVehicleContacts and miNumWorldContacts (offsets confirmed against this
    // header's layout: 16 base + 29*64 + 29*16 + 2*(2016*64) + 2*256 == 260896).
    // NOTE, deliberately: there is NO `stb 1,0(p)` in the instantiation, so the console does
    // NOT raise the IOBuffer status byte for this buffer and neither do we -- nothing
    // Lock*s a PenetrationSolver (DeformationManager::SolvePenetration drives it directly).
    // The contact arrays themselves are left uninitialised, as on the console: the two counts
    // are the only liveness state and AddVehicleContact/AddWorldContact fully overwrite each
    // slot they append.
    // -----------------------------------------------------------------------------------------
    void PenetrationSolver::Construct()
    {
        miNumVehicleContacts = 0;
        miNumWorldContacts   = 0;
    }

    // -----------------------------------------------------------------------------------------
    // Destruct (DWARF :69) -- BODIED 2026-08-15 (IO-buffer zero-fill removal audit).
    // CgsIOBufferStack.h's DestroyIOBuffer<T> is the console's mirror now and calls T::Destruct,
    // so this could no longer stay declaration-only (DeformationManager's post-physics pass tears
    // the solver down at BrnDeformationManager_Contacts.cpp:201).
    // The console has no out-of-line symbol for it -- DestroyIOBuffer<PenetrationSolver>
    // @0x825BCAD8 INLINES it, and the whole body is two stores before the Free(…, 260912):
    //     *(solver + 260896) = 0    == miNumVehicleContacts
    //     *(solver + 260900) = 0    == miNumWorldContacts
    // i.e. exactly Construct's pair, and NOTHING else -- note in particular that it does NOT
    // chain to CgsModule::IOBuffer::Destruct, so the status byte is deliberately left standing.
    // Reproduced as-is rather than "tidied" into a base call.
    void PenetrationSolver::Destruct()
    {
        miNumVehicleContacts = 0;
        miNumWorldContacts   = 0;
    }

    // -----------------------------------------------------------------------------------------
    // AddObject @ 0x825B3CB0
    //   Register body `liIndex`: store its world transform into maObjectTransforms[liIndex] (the
    //   four 16-byte rows the asm copies with lvx128/stvx128) and its solver weighting into
    //   mavfBodyWeighting[liIndex] (the `16*(liIndex+117)` stvx128). The bounds assert is a
    //   non-gating tripwire -- in the asm execution continues past a failed assert and the stores
    //   run regardless, so the C++ falls through the same way.
    // -----------------------------------------------------------------------------------------
    void PenetrationSolver::AddObject(s32 liIndex, Matrix44Affine lTransform, VecFloat lvfWeighting)
    {
        CGS_ASSERT(liIndex < KI_MAX_PENETRATION_BODIES, "liIndex < KI_MAX_PENETRATION_BODIES");

        maObjectTransforms[liIndex] = lTransform;
        mavfBodyWeighting[liIndex]  = lvfWeighting;
    }

    // -----------------------------------------------------------------------------------------
    // AddVehicleContact @ 0x825B3D50
    //   Append one car-on-car penetration contact into maVehicleContacts[miNumVehicleContacts]:
    //   the two contact points, the shared normal, and the two body indices, then bump the live
    //   count. The asm dcbz128 cache-line-zeroes the slot first (the rationale for the
    //   mau8PaddingForCacheLineZeroing over-run guard members); zeroing the slot is subsumed by the
    //   field-by-field assignment here. The bounds assert is a non-gating tripwire (the append runs
    //   regardless on overflow, matching the asm).
    // -----------------------------------------------------------------------------------------
    void PenetrationSolver::AddVehicleContact(Vector3 lPointOnA, Vector3 lPointOnB, Vector3 lNormal,
                                              s32 liIndexA, s32 liIndexB)
    {
        CGS_ASSERT(miNumVehicleContacts < KI_MAX_PENETRATION_CONTACTS,
                   "miNumVehicleContacts < KI_MAX_PENETRATION_CONTACTS");

        PenetrationContact& lrContact = maVehicleContacts[miNumVehicleContacts];
        lrContact.mPointOnA = lPointOnA;
        lrContact.mPointOnB = lPointOnB;
        lrContact.mNormal   = lNormal;
        lrContact.miIndexA  = liIndexA;
        lrContact.miIndexB  = liIndexB;

        ++miNumVehicleContacts;
    }

    // -----------------------------------------------------------------------------------------
    // AddWorldContact -- ⭐ ADDED 2026-08-14 (deformation-mount wave). NO out-of-line export on
    // EITHER console (console-inline at its call sites -- DeformationSensor::
    // AddContactsToPenetrationSolver's world arm); the exact sibling of AddVehicleContact above
    // over maWorldContacts/miNumWorldContacts (the header declares both, DWARF :91).
    // -----------------------------------------------------------------------------------------
    void PenetrationSolver::AddWorldContact(Vector3 lPointOnA, Vector3 lPointOnB, Vector3 lNormal,
                                            s32 liIndexA, s32 liIndexB)
    {
        CGS_ASSERT(miNumWorldContacts < KI_MAX_PENETRATION_CONTACTS,
                   "miNumWorldContacts < KI_MAX_PENETRATION_CONTACTS");

        PenetrationContact& lrContact = maWorldContacts[miNumWorldContacts];
        lrContact.mPointOnA = lPointOnA;
        lrContact.mPointOnB = lPointOnB;
        lrContact.mNormal   = lNormal;
        lrContact.miIndexA  = liIndexA;
        lrContact.miIndexB  = liIndexB;

        ++miNumWorldContacts;
    }

    // -----------------------------------------------------------------------------------------
    // GetNumWorldContacts -- ⭐ ADDED 2026-08-14 (deformation-mount wave). No export on either
    // console (console-inline); the trivial live-count read (DWARF :106).
    // -----------------------------------------------------------------------------------------
    s32 PenetrationSolver::GetNumWorldContacts() const
    {
        return miNumWorldContacts;
    }

    // (walls leg 4: the vehicle-count twin, same console-inline shape.)
    s32 PenetrationSolver::GetNumVehicleContacts() const
    {
        return miNumVehicleContacts;
    }

    // -----------------------------------------------------------------------------------------
    // GetUpdatedTransform (DWARF :124) -- ⭐ 2026-08-14 (walls leg 4). Console-inline on BOTH
    // consoles (no export on either); the inlined access is visible in SolvePenetration's
    // phase-3 read-back (X360 @0x826221D0: `slwi r11, idx, 6` + base -> the 64-byte-stride
    // maObjectTransforms element whose four rows are then NaN-tripwired and stored back into
    // the model). The element-address return is the whole body.
    // -----------------------------------------------------------------------------------------
    const Matrix44Affine* PenetrationSolver::GetUpdatedTransform(s32 liIndex) const
    {
        CGS_ASSERT(liIndex < KI_MAX_PENETRATION_BODIES, "liIndex < KI_MAX_PENETRATION_BODIES");
        return &maObjectTransforms[liIndex];
    }

    // -----------------------------------------------------------------------------------------
    // Solve @ 0x825BAA58
    //   Resolve every accumulated penetration, writing the corrected per-body transforms back into
    //   maObjectTransforms. Two passes: the car-on-car pass (both bodies movable, weighted) then the
    //   car-on-world pass (only the car moves). Each pass mirrors the asm's do/while over the live
    //   contact count (`miNumVehicleContacts` / `miNumWorldContacts`).
    // -----------------------------------------------------------------------------------------
    void PenetrationSolver::Solve()
    {
        // ---- car-on-car contacts: both bodies move, weighted by mavfBodyWeighting ----
        // asm: `if ( result[65224] > 0 ) do { ... } while ( v2 < result[65224] )`.
        if (miNumVehicleContacts > 0)
        {
            for (s32 liContact = 0; liContact < miNumVehicleContacts; ++liContact)
            {
                const PenetrationContact& lContact = maVehicleContacts[liContact];

                Matrix44Affine& lrTransformA = maObjectTransforms[lContact.miIndexA];
                Matrix44Affine& lrTransformB = maObjectTransforms[lContact.miIndexB];

                // World-space positions of the two contact points (TransformPoint over each body's
                // current transform) and the A->B separation vector (operator-).
                const Vector3 lvWorldA = vpu::TransformPoint(lrTransformA, lContact.mPointOnA);
                const Vector3 lvWorldB = vpu::TransformPoint(lrTransformB, lContact.mPointOnB);
                const Vector3 lAToB    = vpu::Subtract(lvWorldB, lvWorldA);

                // Penetration depth along the normal, clamped non-negative (vmsum3fp128 dot, vmaxfp
                // against zero). vfAToBDotN is the broadcast VecFloat in the asm; the dot is
                // de-modelled to a scalar, so the vmaxfp-against-zero clamp is a scalar max here.
                const f32 lfDot       = vpu::Dot(lAToB, lContact.mNormal);
                const f32 lvfAToBDotN = (lfDot > 0.0f) ? lfDot : 0.0f;

                // Shared correction split by the two bodies' weight sum (vrefp + Newton refinement
                // -> exact reciprocal here). lDisp is the depth-scaled normal divided by (wA+wB).
                const f32 lfWeightA        = mavfBodyWeighting[lContact.miIndexA].x;
                const f32 lfWeightB        = mavfBodyWeighting[lContact.miIndexB].x;
                const f32 lfWeightSumRecip = 1.0f / (lfWeightA + lfWeightB);

                const Vector3 lDisp = vpu::Mult(lContact.mNormal, lvfAToBDotN * lfWeightSumRecip);

                // Push the two bodies apart: A advances (vmaddfp into A's Pos row), B retreats
                // (vsubfp out of B's Pos row), each by its own weighted share.
                lrTransformA.Pos() = vpu::Add(lrTransformA.Pos(), vpu::Mult(lDisp, lfWeightA));
                lrTransformB.Pos() = vpu::Subtract(lrTransformB.Pos(), vpu::Mult(lDisp, lfWeightB));
            }
        }

        // ---- car-on-world contacts: only the car (body A) moves ----
        // asm: `if ( result[65225] > 0 ) do { ... } while ( v20 < result[65225] )`.
        if (miNumWorldContacts > 0)
        {
            for (s32 liContact = 0; liContact < miNumWorldContacts; ++liContact)
            {
                const PenetrationContact& lContact = maWorldContacts[liContact];

                Matrix44Affine& lrTransformA = maObjectTransforms[lContact.miIndexA];

                // Only body A's point is transformed; mPointOnB is already a world point (the world
                // is immovable, so it has no transform to apply).
                const Vector3 lvWorldA = vpu::TransformPoint(lrTransformA, lContact.mPointOnA);
                const Vector3 lAToB    = vpu::Subtract(lContact.mPointOnB, lvWorldA);

                const f32 lfDot       = vpu::Dot(lAToB, lContact.mNormal);
                const f32 lvfAToBDotN = (lfDot > 0.0f) ? lfDot : 0.0f;

                // The car takes the full normal*depth correction (no weight split, no opposite body).
                lrTransformA.Pos() = vpu::Add(lrTransformA.Pos(), vpu::Mult(lContact.mNormal, lvfAToBDotN));
            }
        }
    }
}
}
