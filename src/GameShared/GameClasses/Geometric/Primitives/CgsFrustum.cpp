#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/CgsCamera.h"   // CgsGraphics::CameraRwFrustum (SetFromRwFrustum source)

// ============================================================================
// CgsGeometric::Frustum -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Reconstructed here:
//   CgsGeometric::Frustum::VectorToPlane          @ 0x82840DB0
//   CgsGeometric::Frustum::GetPlaneByIndex        @ 0x8274EFE8
//   CgsGeometric::Frustum::SetPlaneByIndex        @ 0x827BAA48
//   CgsGeometric::Frustum::IsSphereInFrustum      @ 0x828AF020
//   CgsGeometric::Frustum::CalcVertices           @ 0x82840DF8  (2026-08-12)
//
// The stored planes live in a struct-of-arrays layout across the 8 x 16-byte
// maSwizzledPlanes lanes: two batches of four planes, each batch four lanes
//   +0x00 Nx  +0x10 Ny  +0x20 Nz  +0x30 offset   (planes 0..3, one per float)
//   +0x40 Nx  +0x50 Ny  +0x60 Nz  +0x70 offset   (planes 4..7, one per float)
// -- proven by the fixed load offsets in IsSphereInFrustum. The X360 gathers /
// scatters a single plane with lvsl-built vperm control vectors (rodata permute
// masks); those masks are the SIMD encoding of "select float lane (index & 3)",
// so they lower to the per-lane component moves below (the same VMX-de-swizzling
// precedent as CgsLineTests.cpp / CgsTriangleBox.cpp). Get/Set share one lane
// mapping, so the round-trip is exact regardless of the intra-batch labelling.
//
// ⭐ THE LANE MAPPING IS NOW PROVEN, NOT INFERRED (2026-08-12). CalcVertices
// @0x82840DF8 de-swizzles the batch back to AoS in the open, and its rodata
// permute masks WERE recovered -- from the .i64, not the JSON export, which
// carries no data section (`ida_bytes.get_bytes`, big-endian):
//     unk_82CDA3C0 = 00010203 00010203 00010203 14151617  -> (A.w0,A.w0,A.w0,B.w1)
//     unk_82CDA400 = 08090A0B 1C1D1E1F 00010203 00010203  -> (A.w2,B.w3,A.w0,A.w0)
//     unk_82CDB450 = 0C0D0E0F 1C1D1E1F 00010203 00010203  -> (A.w3,B.w3,A.w0,A.w0)
// Combined with the vsldoi shift -- SHB=8, decoded from the RAW instruction word
// (0x82840EA4 = 0x11AA6A2C, bits 22..25 = 1000); IDA misrenders the immediate as
// "v8", and taking that at face value silently yields a wrong transpose -- each
// de-swizzle round emits exactly
//     (lane[b+0].w<k>, lane[b+1].w<k>, lane[b+2].w<k>, lane[b+3].w<k>)
// i.e. AoS plane k of batch b = (Nx, Ny, Nz, D), then a whole-vector vxor
// against the 0x80000000 splat. That is byte-for-byte the SoA layout above plus
// VectorToPlane's negate, independently confirming Get/SetPlaneByIndex.
//
// Sibling ledger functions in their own passes (their bodies need collaborators
// this file cannot ground faithfully):
//   CgsGeometric::Frustum::DebugRender            @ 0x82845EC0  (BLOCKED)
//   CgsGeometric::Frustum::DebugRenderCustomPlanes@ 0x82845BB8  (BLOCKED)
//
// Both draw the frustum through the 3D debug renderer (CgsDev::DebugRender::
// DrawQuad / DrawArrow / DrawLine), whose declarations/arg shapes are not homed,
// and they call the still-unwritten sibling IntersectionOf3Planes @0x828415E8 and
// the un-pinned rw::collision::Plane accessors (GetNormal / GetDistance).
//
// ⚠ RETRACTED 2026-08-12 -- the banner here used to list SetFromRwFrustum
// @0x82839FA8 as BLOCKED on "its rodata vperm masks are NOT in the dossier ...
// the exact AoS->SoA lane mapping cannot be reproduced without them", and the
// body below was written to an ASSUMED identity mapping on that basis. Both
// halves of that claim were false. ALL EIGHT masks read straight out of the
// `.i64` (see SetFromRwFrustum's own banner), and they encode a real
// PERMUTATION, not identity -- which silently corrupted every frustum in the
// game until CalcVertices became the first permutation-sensitive reader and
// turned it into visible NaNs. Lesson for the next agent: "the export has no
// data section" is a statement about the EXPORT, never about the database.
// ============================================================================

namespace CgsGeometric
{
    namespace
    {
        // The 8 stored lanes are a struct-of-arrays: maSwizzledPlanes[batch + comp]
        // packs component `comp` of a batch's four planes across the float lanes,
        // one plane per lane. Selecting plane (index & 3)'s component is picking the
        // (index & 3)-th float -- the X360 does it with an lvsl-built vperm; these
        // are the de-swizzled per-lane forms.
        inline f32 LaneGet(const Vector4& lrLane, u32 luLane)
        {
            switch (luLane)
            {
            case 0:  return lrLane.x;
            case 1:  return lrLane.y;
            case 2:  return lrLane.z;
            default: return lrLane.w;
            }
        }

        inline void LaneSet(Vector4& lrLane, u32 luLane, f32 lfValue)
        {
            switch (luLane)
            {
            case 0:  lrLane.x = lfValue; break;
            case 1:  lrLane.y = lfValue; break;
            case 2:  lrLane.z = lfValue; break;
            default: lrLane.w = lfValue; break;
            }
        }

        // --------------------------------------------------------------------
        // The 3x3 solve CalcVertices @0x82840DF8 has INLINED eight times.
        //
        // ⚠ THIS IS *NOT* THE SIBLING MEMBER Frustum::IntersectionOf3Planes
        // @0x828415E8, and must not be re-expressed as a call to it. That one
        // guards the solve with `|det| <= eps -> return false` (vandc against the
        // sign splat, vcmpgtfp. against *unk_830393B0, an early `return 0` before
        // the store). CalcVertices has NO branch of any kind -- straight-line from
        // 0x82840DF8 to the final `stvx128 v0, r4, 112` -- so routing it through
        // IntersectionOf3Planes would ADD a degeneracy early-out the binary does
        // not have. IDA's line info attributes CalcVertices to
        // SDKs/EATech/include/rw/math/vpu/detail/matrix33_operation_platform_inline.h
        // (vs vector3_operation_inline.h for IntersectionOf3Planes): the original
        // source solved it as an rwmath Matrix33 inverse-times-vector, which is
        // why the console form runs in TRANSPOSED (per-component) registers.
        //
        // Given three planes with INWARD normals and `dot3(N, p) == D` (the form
        // VectorToPlane returns and SetFromRwFrustum stores negated), Cramer:
        //     p = ( D_a*(N_b x N_c) + D_b*(N_c x N_a) + D_c*(N_a x N_b) ) / det
        //     det = dot3(N_a, N_b x N_c)
        //
        // Evaluation shape kept as the asm has it, because it decides the
        // rounding:
        //   * det is the COLUMN-0 cofactor expansion
        //         a.x*(bxc).x + b.x*(cxa).x + c.x*(axb).x
        //     (vmsum3fp128 v27, X, cross(Y,Z) over the transposed columns X/Y/Z),
        //     not the row form dot3(a, bxc). Equal in exact arithmetic, and the
        //     horizontal vmsum3fp's own summation order is not observable anyway.
        //   * each cofactor is scaled by 1/det FIRST, then by its D and summed
        //     (vmulfp128 . vmulfp128 . vmaddfp . vmaddfp).
        //
        // 1/det: the console does `vrefp` + TWO Newton-Raphson refinements
        //     e = 1 - d*x ; x = x*e + x     (x2)
        // which converges the ~12-bit VMX estimate to full float precision, so the
        // refinement is emulating exact division rather than being relied on as an
        // approximation -- project convention is then to use exact host math.
        // ⚠ NO GUARD, deliberately: the X360 divides bare here (unlike
        // IntersectionOf3Planes), so a degenerate triple yields inf/NaN on console
        // too. Do not add a clamp. (Only pathological det == 0 differs at all:
        // host 1/0 = +inf, console vrefp(0)+Newton = NaN. Neither is a defined
        // result and no caller reaches it with a real frustum.)
        // --------------------------------------------------------------------
        inline Vector4 SolvePlaneTriple(const Vector4& lrA, const Vector4& lrB, const Vector4& lrC)
        {
            // Cofactor rows of the normal matrix (rows N_a, N_b, N_c).
            const f32 lfBCx = lrB.y * lrC.z - lrB.z * lrC.y;
            const f32 lfBCy = lrB.z * lrC.x - lrB.x * lrC.z;
            const f32 lfBCz = lrB.x * lrC.y - lrB.y * lrC.x;

            const f32 lfCAx = lrC.y * lrA.z - lrC.z * lrA.y;
            const f32 lfCAy = lrC.z * lrA.x - lrC.x * lrA.z;
            const f32 lfCAz = lrC.x * lrA.y - lrC.y * lrA.x;

            const f32 lfABx = lrA.y * lrB.z - lrA.z * lrB.y;
            const f32 lfABy = lrA.z * lrB.x - lrA.x * lrB.z;
            const f32 lfABz = lrA.x * lrB.y - lrA.y * lrB.x;

            // Cofactor expansion along column 0 -- vmsum3fp128 of the X column
            // against cross(Ycolumn, Zcolumn).
            const f32 lfDet    = lrA.x * lfBCx + lrB.x * lfCAx + lrC.x * lfABx;
            const f32 lfInvDet = 1.0f / lfDet;

            // Accumulation order is the asm's: BC*D_a, then += CA*D_b, then
            // += AB*D_c (vmulfp128 -> vmaddfp -> vmaddfp).
            Vector4 lvPoint;
            lvPoint.x = ((lfBCx * lfInvDet) * lrA.w
                       + (lfCAx * lfInvDet) * lrB.w)
                       + (lfABx * lfInvDet) * lrC.w;
            lvPoint.y = ((lfBCy * lfInvDet) * lrA.w
                       + (lfCAy * lfInvDet) * lrB.w)
                       + (lfABy * lfInvDet) * lrC.w;
            lvPoint.z = ((lfBCz * lfInvDet) * lrA.w
                       + (lfCAz * lfInvDet) * lrB.w)
                       + (lfABz * lfInvDet) * lrC.w;

            // The console works four lanes wide and lane 3 of each assembled
            // cofactor vector is the SoA duplicate of its Y component
            // (v26/v30/v29 = (CX[k], CY[k], CZ[k], CY[k]) -- vmrghw/vmrglw of a
            // 3-wide result into a 4-wide register), so the stored W comes out
            // EQUAL TO Y, not 0 or 1. `stvx128` writes all sixteen bytes, so
            // reproduce it. (Every reconstructed caller overwrites W with 1.0f
            // before transforming, so this is inert downstream -- but it is what
            // the binary leaves there.)
            lvPoint.w = lvPoint.y;
            return lvPoint;
        }
    }

    // ------------------------------------------------------------------------
    // VectorToPlane @ 0x82840DB0
    //
    //   vspltisw v0, -1        ; v0 = 0xFFFFFFFF per lane
    //   lvx128   v13, r0, r5   ; v13 = *lrVector  (the packed swizzled lane; r4
    //                          ;                    == this is unused)
    //   vslw     v0, v0, v0    ; v0 = 0xFFFFFFFF << 31 = 0x80000000 (sign mask)
    //   vxor     v0, v13, v0   ; flip the sign bit of all four lanes -> negate
    //   stvx128  v0, r0, r3    ; *result = negated vector, reinterpreted as Plane
    //   blr
    //
    // The stored frustum lane is the negation of the plane it represents, so
    // recovering the plane is a whole-vector negate. `this` is never touched --
    // the method is const and ignores it (the stack lfs/stfs shuffle in the asm
    // is just the compiler's 16-byte copy of the negated result into the return
    // slot). Lowered to a clean per-lane arithmetic negation.
    // ------------------------------------------------------------------------
    rw::collision::Plane Frustum::VectorToPlane(const Vector4& lrVector) const
    {
        Vector4 lNegated;
        lNegated.x = -lrVector.x;
        lNegated.y = -lrVector.y;
        lNegated.z = -lrVector.z;
        lNegated.w = -lrVector.w;
        return rw::collision::Plane(lNegated);
    }

    // ------------------------------------------------------------------------
    // PlaneToVector -- the exact inverse of VectorToPlane (see above): the stored
    // frustum lane IS the negation of the plane it represents, so packing a plane
    // back into its lane form is the same whole-vector sign flip. The X360 folds
    // this into SetPlaneByIndex's prologue (one `vxor` against the 0x80000000
    // splat, same idiom as 0x82840DB0); declared out-of-line by the header, so it
    // is homed here beside its only caller. `this` is untouched.
    // ------------------------------------------------------------------------
    Vector4 Frustum::PlaneToVector(const rw::collision::Plane& lrPlane) const
    {
        const Vector4& lrPlaneVector = *reinterpret_cast<const Vector4*>(&lrPlane);

        Vector4 lNegated;
        lNegated.x = -lrPlaneVector.x;
        lNegated.y = -lrPlaneVector.y;
        lNegated.z = -lrPlaneVector.z;
        lNegated.w = -lrPlaneVector.w;
        return lNegated;
    }

    // ------------------------------------------------------------------------
    // GetPlaneByIndex @ 0x8274EFE8
    //
    // r3 = sret Plane buffer, r4 = this, r5 = luPlaneIndex. Asserts index < 8,
    // then, on the batch chosen by (index >= 4), builds a permute from
    // lvsl(4 * (index & 3)) and gathers the four SoA component lanes:
    //   index < 4 : lanes at this+0x00 / +0x10 / +0x20 / +0x30
    //   index >= 4: lanes at this+0x40 / +0x50 / +0x60 / +0x70
    // The four broadcast components are packed [Nx, Ny, Nz, offset] into an AoS
    // vector and handed to VectorToPlane. Lowered to the per-lane gather below.
    // ------------------------------------------------------------------------
    rw::collision::Plane Frustum::GetPlaneByIndex(u32 luPlaneIndex) const
    {
        CGS_ASSERT(luPlaneIndex < 8, "luPlaneIndex < 8");

        const u32 luLane  = luPlaneIndex & 3;
        const u32 luBatch = (luPlaneIndex >= 4) ? 4u : 0u;   // 4 SoA lanes per batch

        Vector4 lPlaneVector;
        lPlaneVector.x = LaneGet(maSwizzledPlanes[luBatch + 0], luLane);   // Nx
        lPlaneVector.y = LaneGet(maSwizzledPlanes[luBatch + 1], luLane);   // Ny
        lPlaneVector.z = LaneGet(maSwizzledPlanes[luBatch + 2], luLane);   // Nz
        lPlaneVector.w = LaneGet(maSwizzledPlanes[luBatch + 3], luLane);   // signed offset
        return VectorToPlane(lPlaneVector);
    }

    // ------------------------------------------------------------------------
    // SetPlaneByIndex @ 0x827BAA48
    //
    // r3 = this, r4 = luPlaneIndex, r5 = lrPlane. Asserts index < 8, packs the
    // plane into its stored (negated) AoS form via PlaneToVector, then, using a
    // per-lane insert mask keyed by ((index & 3) << 6), scatters each component
    // into float lane (index & 3) of the matching SoA vector -- the batch chosen
    // by (index >= 4), the same +0x00/+0x40 split GetPlaneByIndex reads back.
    // The register left in r3 by the asm is leftover mask-address scheduling, not
    // a source-level return value; the setter returns void.
    // ------------------------------------------------------------------------
    void Frustum::SetPlaneByIndex(u32 luPlaneIndex, const rw::collision::Plane& lrPlane)
    {
        CGS_ASSERT(luPlaneIndex < 8, "luPlaneIndex < 8");

        const Vector4 lPlaneVector = PlaneToVector(lrPlane);

        const u32 luLane  = luPlaneIndex & 3;
        const u32 luBatch = (luPlaneIndex >= 4) ? 4u : 0u;

        LaneSet(maSwizzledPlanes[luBatch + 0], luLane, lPlaneVector.x);   // Nx
        LaneSet(maSwizzledPlanes[luBatch + 1], luLane, lPlaneVector.y);   // Ny
        LaneSet(maSwizzledPlanes[luBatch + 2], luLane, lPlaneVector.z);   // Nz
        LaneSet(maSwizzledPlanes[luBatch + 3], luLane, lPlaneVector.w);   // signed offset
    }

    // ------------------------------------------------------------------------
    // SetFromRwFrustum @ 0x82839FA8
    //
    // Transpose the SIX world-space camera planes (CameraRwFrustum, each stored
    // [Nx, Ny, Nz, D] with `dot3(N, p) == D` and N pointing INTO the view volume)
    // into the EIGHT swizzled SoA lanes this class stores, negated (S = -plane,
    // the form VectorToPlane @0x82840DB0 un-negates).
    //
    // ⭐⭐ IT PERMUTES. The RW slot order is NOT the stored slot order, and this
    // body asserted identity from 2026-07-28 until 2026-08-12. Nothing caught it
    // because every reader that existed then (IsSphereInFrustum,
    // LooseOctree::FrustumTestEntities, FrustumTestVp) evaluates ALL eight lanes
    // and ORs the rejects -- permutation-invariant by construction. CalcVertices
    // @0x82840DF8 is the first permutation-SENSITIVE reader, and under identity
    // storage it paired near-with-left and far-with-top: two of its eight plane
    // triples became singular (NaN at vertex 2 and 6) and the other six solved
    // finite but geometrically WRONG. The silent six mattered as much as the NaN.
    //
    // THE MAPPING, READ OFF THE ASM (RW source index -> stored slot):
    //     slot 0 <- RW2   slot 1 <- RW4   slot 2 <- RW3   slot 3 <- RW5
    //     slot 4 <- RW1   slot 5 <- RW0   slot 6 <- RW1   slot 7 <- RW0
    //
    // HOW. The eight `stvx128`s at 0x8283A05C..0x8283A114 each combine two
    // `vperm`s with `vsldoi(A, B, 8)` = (A.w2, A.w3, B.w0, B.w1). Every mask came
    // out of the .i64 (the JSON export has no data section -- the same recovery
    // the CalcVertices banner describes):
    //     unk_82CDA3F0 = (A.w0, B.w0, A.w0, A.w0)   unk_82CDADB0 = (A.w0, A.w0, A.w0, B.w0)
    //     unk_82CDADC0 = (A.w1, B.w1, A.w0, A.w0)   unk_82CDADD0 = (A.w0, A.w0, A.w1, B.w1)
    //     unk_82CDADE0 = (A.w2, B.w2, A.w0, A.w0)   unk_82CDADF0 = (A.w0, A.w0, A.w2, B.w2)
    //     unk_82CDB430 = (A.w0, A.w0, A.w3, B.w3)   unk_82CDB450 = (A.w3, B.w3, A.w0, A.w0)
    // and all eight `vsldoi` immediates decode to SHB=8 from the raw words (IDA
    // renders the immediate as a register here too -- 0x8283A048 = 0x10E63A2C,
    // bits 22..25 = 1000). The batch split alone already disproves identity: the
    // loads are v13=RW0, v0=RW1, v12=RW2, v10=RW3, v11=RW4, v9=RW5, and BATCH 0
    // (this+0x00..0x30) is built only from {v12,v11,v10,v9} = RW{2,4,3,5} while
    // BATCH 1 (this+0x40..0x70) is built only from {v0,v13} = RW{1,0}. Resolving
    // the masks then gives each lane 0 exactly:
    //     lane0 = vsldoi(vperm(v12,v11,ADB0), vperm(v10,v9,A3F0), 8)
    //           = (-RW2.x, -RW4.x, -RW3.x, -RW5.x)
    // and the other three components repeat it with the .w1/.w2/.w3 mask pairs.
    //
    // ⭐ THREE INDEPENDENT ARTIFACTS AGREE. With the RW order both camera writers
    // emit (CgsCamera.h:52 -- near, far, left, right, top, bottom) this asm
    // permutation reads out as
    //     slot0=left  slot1=top  slot2=right  slot3=bottom  slot4=far  slot5=near
    // which is the DecFIGS `PlaneId` enum (CgsFrustum.h:50) VALUE FOR VALUE. The
    // asm was decoded without reference to the enum, so the enum, the camera
    // writers' plane order, and this permutation each corroborate the other two.
    //
    // ⚠ THE PAD LANES ARE NOT ZERO -- they DUPLICATE far and near. Slots 6/7 get
    // RW1/RW0 again, from the same `(A.w0,A.w0,A.w1,B.w1)`-style mask pairs that
    // fill 4/5 (batch 1's two vperm sources are both {v0,v13}, so its four lanes
    // can only ever be far/near/far/near). The previous body zeroed them on the
    // argument that "a zero plane can never reject, so it is forced rather than
    // guessed" -- the reasoning was sound but the premise was not, and the console
    // simply re-applies far/near, which rejects exactly when slots 4/5 already do.
    // Both are equivalent for every all-lane culling reader, but they are NOT
    // equivalent for GetPlaneByIndex(6)/(7), which on console returns a real
    // plane. Reproduce the binary.
    // ------------------------------------------------------------------------
    void Frustum::SetFromRwFrustum(const CgsGraphics::CameraRwFrustum& lrRw)
    {
        // Stored slot -> CameraRwFrustum::maPlanes index. See the banner: this is
        // the whole content of the eight vperm/vsldoi stores.
        static const u32 KAU_RW_SOURCE_FOR_SLOT[8] = { 2u, 4u, 3u, 5u, 1u, 0u, 1u, 0u };

        for (u32 luSlot = 0; luSlot < 8; ++luSlot)
        {
            const Vector4& lrRwPlane = lrRw.maPlanes[KAU_RW_SOURCE_FOR_SLOT[luSlot]];

            const u32 luBatch = (luSlot >= 4) ? 4u : 0u;   // 4 SoA lanes per batch
            const u32 luLane  = luSlot & 3u;

            LaneSet(maSwizzledPlanes[luBatch + 0], luLane, -lrRwPlane.x);
            LaneSet(maSwizzledPlanes[luBatch + 1], luLane, -lrRwPlane.y);
            LaneSet(maSwizzledPlanes[luBatch + 2], luLane, -lrRwPlane.z);
            LaneSet(maSwizzledPlanes[luBatch + 3], luLane, -lrRwPlane.w);
        }
    }

    // ------------------------------------------------------------------------
    // IsSphereInFrustum @ 0x828AF020
    //
    // r3 = this, r4 = sphere ([cx, cy, cz, radius] in one lane). Splats the
    // centre (x/y/z) and radius, then runs the two SoA batches in parallel:
    //   distance = Nx*cx + Ny*cy + Nz*cz - offset            (per plane)
    //   outside  = distance > radius                          (vcmpgtfp)
    // The per-lane b0/b1 outside masks are OR'd, mapped to 1.0/0.0 (vsel), and
    // vcmpeqfp. against 0.0 sets CR6[all-true] iff every plane keeps the sphere
    // inside -- the bit the function returns. Re-rolled over the 4 SoA lanes; a
    // NaN distance compares not-greater (matching vcmpgtfp), so it never rejects.
    // ------------------------------------------------------------------------
    bool Frustum::IsSphereInFrustum(const Sphere& lrSphere) const
    {
        const f32 lfCx     = lrSphere.mPositionRadius.x;
        const f32 lfCy     = lrSphere.mPositionRadius.y;
        const f32 lfCz     = lrSphere.mPositionRadius.z;
        const f32 lfRadius = lrSphere.mPositionRadius.w;

        for (u32 luLane = 0; luLane < 4; ++luLane)   // 4 lanes x 2 batches = 8 planes
        {
            const f32 lfDistance0 =
                  LaneGet(maSwizzledPlanes[0], luLane) * lfCx
                + LaneGet(maSwizzledPlanes[1], luLane) * lfCy
                + LaneGet(maSwizzledPlanes[2], luLane) * lfCz
                - LaneGet(maSwizzledPlanes[3], luLane);

            const f32 lfDistance1 =
                  LaneGet(maSwizzledPlanes[4], luLane) * lfCx
                + LaneGet(maSwizzledPlanes[5], luLane) * lfCy
                + LaneGet(maSwizzledPlanes[6], luLane) * lfCz
                - LaneGet(maSwizzledPlanes[7], luLane);

            if (lfDistance0 > lfRadius || lfDistance1 > lfRadius)
                return false;
        }
        return true;
    }

    // ------------------------------------------------------------------------
    // CalcVertices @ 0x82840DF8      (r3 = this, r4 = lapVerts; void, no branches)
    //
    // Write the eight frustum corners, each the intersection of a plane triple.
    // Called by BrnWorld::ShadowMap::ComputeTSMMatrix @0x827BFF58,
    // ComputeBoundingBoxMatrix @0x827D91B0 and CalculateShadowMapCameras
    // @0x827DA820 -- the shadow-cascade fitter's only source of frustum corners.
    //
    // STAGE 1 -- de-swizzle. The X360 transposes the SoA batches back into six
    // AoS planes and negates each (VectorToPlane's whole-vector vxor). Which
    // register ends up holding which plane is pinned by the splat word each
    // transpose round selects (lvsl(0/4/8/12) -> vspltw word 0 -> a "select word
    // k" vperm control), and word k of a batch is plane k:
    //     v13 = -lane[0..3].w0 = plane 0 = PlaneLeft
    //     v10 = -lane[0..3].w1 = plane 1 = PlaneTop
    //     v12 = -lane[0..3].w2 = plane 2 = PlaneRight
    //     v11 = -lane[0..3].w3 = plane 3 = PlaneBottom
    //     v7  = -lane[4..7].w0 = plane 4 = PlaneFar
    //     v9  = -lane[4..7].w1 = plane 5 = PlaneNear
    // (planes 6/7, the pad lanes SetFromRwFrustum zeroes, are never read here --
    // a zero plane has no intersection, which is why the corner solve only ever
    // touches the six real ones.)
    //
    // STAGE 2 -- eight solves. Each block's triple is read off UNAMBIGUOUSLY from
    // its distance gather, `vperm vD, <A>, <B>, unk_82CDB450` (= (A.w, B.w, ..))
    // followed by `vrlimi128 vD, <C>, 2, 1` (rotate C left one word, insert into
    // element 2 -> C.w), giving (D_a, D_b, D_c) for exactly the triple whose
    // normals feed that block's column transpose:
    //     r4+0x00 (v13,v11,v9) L,B,N     r4+0x40 (v13,v11,v7) L,B,F
    //     r4+0x10 (v12,v11,v9) R,B,N     r4+0x50 (v12,v11,v7) R,B,F
    //     r4+0x20 (v13,v10,v9) L,T,N     r4+0x60 (v13,v10,v7) L,T,F
    //     r4+0x30 (v12,v10,v9) R,T,N     r4+0x70 (v12,v10,v7) R,T,F
    // -- a clean 2x2x2 product {Left,Right} x {Bottom,Top} x {Near,Far}, i.e. the
    // near quad (BL, BR, TL, TR) then the far quad in the same order. The eight
    // unrolled blocks are re-rolled into the loop below over exactly that table;
    // the emitted store offsets (0x00,0x10,...,0x70) fix the output order.
    // ------------------------------------------------------------------------
    void Frustum::CalcVertices(Vector4* lapVerts) const
    {
        // Stage 1: the six real planes, AoS, un-negated -- (Nx, Ny, Nz, D) with
        // N pointing into the view volume and dot3(N, p) == D.
        Vector4 laPlanes[6];
        for (u32 luPlane = 0; luPlane < 6; ++luPlane)
        {
            const u32 luBatch = (luPlane >= 4) ? 4u : 0u;   // 4 SoA lanes per batch
            const u32 luLane  = luPlane & 3u;

            laPlanes[luPlane].x = -LaneGet(maSwizzledPlanes[luBatch + 0], luLane);
            laPlanes[luPlane].y = -LaneGet(maSwizzledPlanes[luBatch + 1], luLane);
            laPlanes[luPlane].z = -LaneGet(maSwizzledPlanes[luBatch + 2], luLane);
            laPlanes[luPlane].w = -LaneGet(maSwizzledPlanes[luBatch + 3], luLane);
        }

        // Stage 2: bit 0 picks the horizontal plane, bit 1 the vertical, bit 2
        // the depth -- exactly the store order the eight inlined blocks emit.
        for (u32 luVert = 0; luVert < 8; ++luVert)
        {
            const PlaneId lHorizontal = ((luVert & 1u) != 0) ? PlaneRight : PlaneLeft;
            const PlaneId lVertical   = ((luVert & 2u) != 0) ? PlaneTop   : PlaneBottom;
            const PlaneId lDepth      = ((luVert & 4u) != 0) ? PlaneFar   : PlaneNear;

            lapVerts[luVert] = SolvePlaneTriple(laPlanes[lHorizontal],
                                                laPlanes[lVertical],
                                                laPlanes[lDepth]);
        }
    }

    // ------------------------------------------------------------------------
    // Never called -- layout pins for the swizzled batch every function above
    // indexes by hand. A member function so the body is a complete-class context
    // with private access.
    // ------------------------------------------------------------------------
    void Frustum::_AssertLayout()
    {
        // One 16-byte SIMD lane per plane slot.
        static_assert(sizeof(Vector4) == 16, "Vector4 must be one 16-byte SIMD lane");
        static_assert(alignof(Vector4) == 16, "Vector4 must be 16-byte aligned");

        // 8 lanes = 0x80. This is the size FrustumJobQueryInfo::operator=
        // block-copies per maFrustum[] element, and the +0x40 batch split every
        // accessor here relies on.
        static_assert(sizeof(Frustum::maSwizzledPlanes) == 128, "Frustum plane batch must be 128 bytes");
        static_assert(sizeof(Frustum::maSwizzledPlanes) / sizeof(Vector4) == 8, "Frustum must store exactly 8 swizzled lanes");
        static_assert(sizeof(Frustum) == 128, "sizeof(CgsGeometric::Frustum) must be 128 (0x80)");
        static_assert(alignof(Frustum) == 16, "Frustum must be 16-byte aligned (lvx128/stvx128)");

        // POINTER INVARIANT: maSwizzledPlanes is the SOLE data member and is
        // exactly as large as the whole object, so it is necessarily at offset 0.
        // That is what lets CalcVertices/IsSphereInFrustum address the lanes as
        // `this + 0x00 .. this + 0x70` the way the X360 asm does.
        static_assert(sizeof(Frustum) == sizeof(Frustum::maSwizzledPlanes), "maSwizzledPlanes must be the whole object (offset 0)");

        // The PlaneId numbering is load-bearing: CalcVertices' de-swizzle reads
        // plane k from float lane (k & 3) of batch (k >= 4), so these values ARE
        // the lane indices, not labels. Pinned by the splat-word order in the
        // @0x82840DF8 transpose.
        static_assert(PlaneLeft == 0 && PlaneTop == 1 && PlaneRight == 2, "batch 0 lane order");
        static_assert(PlaneBottom == 3 && PlaneFar == 4 && PlaneNear == 5, "batch 0/1 lane order");
    }
}
