#include "vendor/renderware/collision/CapsuleVolume.hpp"

#include "vendor/renderware/collision/GPInstance.hpp"   // GPInstance (CreateGPInstance)

#include <cmath>     // fabs
#include <cstdint>   // uintptr_t (the console 32-bit mVolumeTag pointer image)
#include <cstring>   // memcpy (GetMaximumFeature edge copy)

// ===========================================================================
// rw::collision::CapsuleVolume -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following this directory's
// precedent (CylinderVolume.cpp / TriangleVolume.cpp), these are SEMANTIC
// reconstructions lowered to portable scalar float maths, preserving the store
// order, offsets and every side effect. The vspltisw(-1)/vslw sign mask lowers
// to std::fabs; vmaddfp vD,vA,vB,vC == vA*vC + vB (the operand rule attested
// across this rw::collision family's asm, and reasserted at each site below).
//
// ONE function is NOT homed here (see the header for the reason):
//   LineSegIntersect @ 0x82BAFCF8 -- a 428-instruction register-allocated VMX
//   narrow-phase kernel. Left BLOCKED per the project's no-fabrication rule;
//   note its three .rdata scalars ARE recovered (0.0f / 1.0f / -1.0f) and are
//   not the blocker -- the register-allocated per-lane geometry is.
// ===========================================================================

namespace rw
{
namespace collision
{

// ---------------------------------------------------------------------------
// X360 unk_82F918F0 -- the per-VolumeType GP callback table (the canonical
// static GPInstance::sVolumeMethods[6], DWARF volume.h:484). CreateGPInstance
// copies one row of it into GPInstance::mMethods.
//
// The table lives in .data (0x82CD0000-0x832BE424) and its CONTENTS ARE
// RECOVERED from the image; the rows are, by VolumeType index:
//   [0] UNUSED   { 0, 0, 0, 0 }
//   [1] SPHERE   { GPSphere::GetMaximumFeature   @0x82BA8088,
//                  GPSphere::GetInterval         @0x82BA80A8,
//                  GPSphere::GetIntervals        @0x82BA80C0,  STUB @0x82AD5078 }
//   [2] CAPSULE  { GPCapsule::GetMaximumFeature  @0x82BAFA80,
//                  GPCapsule::GetInterval        @0x82BAFB68,
//                  GPCapsule::GetIntervals       @0x82BAFC10,  STUB @0x82AD5078 }
//   [3] TRIANGLE { GPTriangle::GetMaximumFeature @0x82BBA158,
//                  GPTriangle::GetInterval       @0x82BBA6A0,
//                  GPTriangle::GetIntervals      @0x82BBA6E0,  STUB @0x82AD5078 }
//   [4] BOX      { GPBox::GetMaximumFeature      @0x82BA8918,
//                  GPBox::GetInterval            @0x82BA8650,
//                  GPBox::GetIntervals           @0x82BA9238,  STUB @0x82AD5078 }
//   [5] CYLINDER { GPCylinder::GetMaximumFeature @0x82BAE430,
//                  GPCylinder::GetInterval       @0x82BAD7F8,
//                  GPCylinder::GetIntervals      @0x82BAD938,  STUB @0x82AD5078 }
// The mGetBBox slot (+0xB0) is 0x82AD5078 in every populated row -- the
// binary's shared empty function (a lone `blr`, COMDAT-folded across the whole
// XEX), i.e. the GP GetBBox callback is a NO-OP on this build.
//
// The DEFINITION is NOT written here: this table is shared by every volume
// type's CreateGPInstance (CylinderVolume @0x82BAC7F8, TriangleVolume
// @0x82BBAA00, ...) and belongs to the un-recovered GP registration TU, which
// is also the only place GPSphere::GetMaximumFeature is declared. Declared
// extern only -- the same treatment the committed GPInstance.hpp gives the
// sibling dispatch table off_82F91800 (gapFindBestSeparatingDirection).
// FLAGGED: definition pending -> link blocker until that TU lands.
extern const GPInstance::VolumeMethods
    g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];   // X360 unk_82F918F0

namespace
{
    // The transform prologue shared by CreateGPInstance @0x82BAF6F0 and
    // GetBBox @0x82BAF9C8 (byte-for-byte the same instruction sequence).
    // vmaddfp vD,vA,vB,vC == vA*vC + vB in this family's asm; all FOUR VMX
    // lanes are computed (for an affine matrix the w column is 0, but the
    // console does not special-case it, so neither does this).

    // Rotate a direction row by the transform's 3x3:
    //   vmulfp128 v8,axis.x,row0 ; vmaddfp axis.y,row1 ; vmaddfp axis.z,row2
    inline Vec4 RotateByTransform3x3(const Vec4& arDir, const Vec4* lapRows)
    {
        Vec4 lvOut;
        lvOut.x = arDir.x * lapRows[0].x;
        lvOut.y = arDir.x * lapRows[0].y;
        lvOut.z = arDir.x * lapRows[0].z;
        lvOut.w = arDir.x * lapRows[0].w;

        lvOut.x = arDir.y * lapRows[1].x + lvOut.x;
        lvOut.y = arDir.y * lapRows[1].y + lvOut.y;
        lvOut.z = arDir.y * lapRows[1].z + lvOut.z;
        lvOut.w = arDir.y * lapRows[1].w + lvOut.w;

        lvOut.x = arDir.z * lapRows[2].x + lvOut.x;
        lvOut.y = arDir.z * lapRows[2].y + lvOut.y;
        lvOut.z = arDir.z * lapRows[2].z + lvOut.z;
        lvOut.w = arDir.z * lapRows[2].w + lvOut.w;
        return lvOut;
    }

    // Transform a point by the full affine matrix. ACCUMULATION ORDER is the
    // asm's: row3 is folded in FIRST (vmaddfp v12, centre.x, row0, row3), then
    // centre.y*row1, then centre.z*row2.
    inline Vec4 TransformPointByTransform(const Vec4& arPoint, const Vec4* lapRows)
    {
        Vec4 lvOut;
        lvOut.x = arPoint.x * lapRows[0].x + lapRows[3].x;
        lvOut.y = arPoint.x * lapRows[0].y + lapRows[3].y;
        lvOut.z = arPoint.x * lapRows[0].z + lapRows[3].z;
        lvOut.w = arPoint.x * lapRows[0].w + lapRows[3].w;

        lvOut.x = arPoint.y * lapRows[1].x + lvOut.x;
        lvOut.y = arPoint.y * lapRows[1].y + lvOut.y;
        lvOut.z = arPoint.y * lapRows[1].z + lvOut.z;
        lvOut.w = arPoint.y * lapRows[1].w + lvOut.w;

        lvOut.x = arPoint.z * lapRows[2].x + lvOut.x;
        lvOut.y = arPoint.z * lapRows[2].y + lvOut.y;
        lvOut.z = arPoint.z * lapRows[2].z + lvOut.z;
        lvOut.w = arPoint.z * lapRows[2].w + lvOut.w;
        return lvOut;
    }
}

// ---------------------------------------------------------------------------
// CapsuleVolume::CreateGPInstance @ 0x82BAF6F0
//
// r3 = this, r4 = &arInst, r5 = lpTransform (may be NULL). No null guard on
// r4 -- the console dereferences it unconditionally.
//
//   cmplwi cr6, r5, 0 ; beq -> local path
//   [transform path -- the SAME prologue as GetBBox @0x82BAF9C8]
//     v0  = rotate(maFrame[2] (+0x20), transform 3x3)
//     v13 = transform(maFrame[3] (+0x30)) as a point
//   [local path]
//     v0  = maFrame[2] ; v13 = maFrame[3]
//   then, in store order:
//     stw     r11, 0x84(r4)   mVolumeTag        = (console 32-bit) this
//     stvx128 v13, r0, r4     mPos      (+0x00) = centre        (all 4 lanes)
//     stb     0,   0x8C(r4)   mNumFaceNormals   = 0
//     stw     0,   0x88(r4)   mUserTag          = 0
//     stw     2,   0x90(r4)   mVolumeType       = 2  (GPInstance::CAPSULE)
//     stb     1,   0x8D(r4)   mNumEdgeDirections= 1
//     lfs f0,0x50(r11) ; stfs f0,0x80(r4)   mFatness = mfRadius
//     stvx128 v0, r4, 0x40    mEdgeDirections[0] = axis          (all 4 lanes)
//     lvx128 +0x70 ; stfs mfHalfHeight into lane 0 ; stvx128 back to +0x70
//                             mDimensions.x = mfHalfHeight -- a read-modify-
//                             write through the stack: lanes y/z/w of
//                             mDimensions are left EXACTLY as the caller's
//                             buffer had them (NOT zeroed).
//     lwz r11,0x90(r4)        (the type is RE-READ from the instance, not
//                              reused from the immediate)
//     stw r8, 0x94(r4)        mFlags = this->muFlags (+0x5C)
//     mMethods (+0xA4) = unk_82F918F0[type]   (4 words on console)
//     li r3, 1 ; blr
//
// CHECKLIST A: the console copies the row with 4 lwz/stw pairs after
// `slwi r11, r11, 4` (its VolumeMethods is 4 x 4-byte pointers = 0x10). Those
// literals are CONSOLE-ONLY and stay in this comment: GPInstance::mMethods is
// pointer-widened on x64 (GPInstance.hpp:164), so the copy here is a
// whole-struct assignment and the index is a typed array index -- never a
// byte base + (type << 4).
// ---------------------------------------------------------------------------
RwBool CapsuleVolume::CreateGPInstance(GPInstance& arInst, const Vec4* lpTransform) const
{
    Vec4 lvAxis;     // v0  -- the (optionally rotated) capsule axis, maFrame[2]
    Vec4 lvCentre;   // v13 -- the (optionally transformed) centre,  maFrame[3]

    if (lpTransform != nullptr)
    {
        lvAxis   = RotateByTransform3x3(maFrame[2], lpTransform);
        lvCentre = TransformPointByTransform(maFrame[3], lpTransform);
    }
    else
    {
        lvAxis   = maFrame[2];
        lvCentre = maFrame[3];
    }

    // FLAG (host width): the console stores its 4-byte `this` straight into the
    // u32 mVolumeTag word, and the batch kernels copy that word verbatim into
    // PrimitivePairIntersectResult::v1/v2. The committed GPInstance keeps the
    // field as the console's 32-bit pointer IMAGE, so this truncates on x64;
    // the real fix is to promote GPInstance::mVolumeTag (and PPIR::v1/v2) to a
    // pointer-width field, which belongs to the GPInstance TU, not here.
    arInst.mVolumeTag        = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mPos              = lvCentre;
    arInst.mNumFaceNormals   = 0;
    arInst.mUserTag          = 0;
    arInst.mVolumeType       = GPInstance::CAPSULE;   // stw 2, 0x90
    arInst.mNumEdgeDirections= 1;
    arInst.mFatness          = mfRadius;
    arInst.mEdgeDirections[0]= lvAxis;

    // The +0x70 round trip: lane x only; y/z/w survive untouched.
    arInst.mDimensions.x     = mfHalfHeight;

    arInst.mFlags            = muFlags;

    // The console re-reads the type word it just stored; keep that read.
    arInst.mMethods          = g_aGPVolumeMethods[arInst.mVolumeType];

    return 1;
}

// ---------------------------------------------------------------------------
// CapsuleVolume::GetBBoxDiag @ 0x82BAF6A8
//
//   vspltisw v0,-1 ; vslw v0,v0,v0            ; per-lane 0x80000000 sign mask
//   lvlx     v12, r4+0x50 ; vspltw 0          ; mfRadius splat      (addend, vB)
//   vspltisw v13,2 ; vcfsx v13,v13,0          ; 2.0f
//   lvlx     v11, r4+0x44 ; vspltw 0          ; mfHalfHeight splat  (mult, vC)
//   lvx128   v10, r4+0x20                      ; maFrame[2] (oriented axis)
//   vandc    v0, v10, v0                       ; |maFrame[2]| (clear sign bit)
//   vmaddfp  v0, v0, v12, v11                  ; vA*vC+vB = |axis|*mfHalfHeight + mfRadius
//   vmulfp128 v0, v0, v13                       ; * 2.0
//   stvx128  v0, r0, r3                         ; -> 16-byte return slot
// The diagonal is exactly twice the GetBBox half-extent (max - min).
// ---------------------------------------------------------------------------
math::vpu::Vector3 CapsuleVolume::GetBBoxDiag() const
{
    const Vec4& lvAxis = maFrame[2];

    const f32 lfDiagX = 2.0f * (std::fabs(lvAxis.x) * mfHalfHeight + mfRadius);
    const f32 lfDiagY = 2.0f * (std::fabs(lvAxis.y) * mfHalfHeight + mfRadius);
    const f32 lfDiagZ = 2.0f * (std::fabs(lvAxis.z) * mfHalfHeight + mfRadius);

    return math::vpu::Vector3(lfDiagX, lfDiagY, lfDiagZ);
}

// ---------------------------------------------------------------------------
// CapsuleVolume::GetMaximumFeature @ 0x82BAF808
//
//   axis   = maFrame[1] (+0x10)   centre = maFrame[3] (+0x30)
//   h      = mfHalfHeight (+0x44, splat via lvlx)
//   P0     = axis*h + centre      (vmaddfp v0, v0, v13, v11: vA*vC+vB, vA=axis,
//                                  vC=v11=h, vB=v13=centre)
//   P1     = centre - axis*h      (vmulfp128 v12,v0,v12 ; vsubfp v13,v13,v12)
//   FeatureEdge(edge, P0, P1) ; memcpy(feature+0x10, edge, 0x40) ; numedges = 1
// abCcw (r4) / arDir (r5) are dead in the asm.
// ---------------------------------------------------------------------------
void CapsuleVolume::GetMaximumFeature(RwBool /*abCcw*/, const Vec4& /*arDir*/,
                                      Feature& arResult) const
{
    const Vec4& lvAxis   = maFrame[1];
    const Vec4& lvCentre = maFrame[3];
    const f32   lfScale  = mfHalfHeight;   // +0x44 scalar (splat)

    Vec4 lvP0;   // var_70: axis * mfHalfHeight + centre
    lvP0.x = lvAxis.x * lfScale + lvCentre.x;
    lvP0.y = lvAxis.y * lfScale + lvCentre.y;
    lvP0.z = lvAxis.z * lfScale + lvCentre.z;
    lvP0.w = lvAxis.w * lfScale + lvCentre.w;

    Vec4 lvP1;   // var_60: centre - axis * mfHalfHeight
    lvP1.x = lvCentre.x - lvAxis.x * lfScale;
    lvP1.y = lvCentre.y - lvAxis.y * lfScale;
    lvP1.z = lvCentre.z - lvAxis.z * lfScale;
    lvP1.w = lvCentre.w - lvAxis.w * lfScale;

    const FeatureEdge lEdge(lvP0, lvP1);
    std::memcpy(&arResult.edges[0], &lEdge, sizeof(FeatureEdge));

    arResult.numedges = 1;
}

// ---------------------------------------------------------------------------
// CapsuleVolume::GetBBox @ 0x82BAF9C8
//
// Two paths (asm-attested; there is NO tight/loose split -- r5 is dead):
//   1. lpTransform == NULL: axis = maFrame[2], centre = maFrame[3] (local).
//   2. lpTransform != NULL: the oriented axis (maFrame[2]) is rotated by the
//      transform's 3x3 (axis = m2.x*row0 + m2.y*row1 + m2.z*row2) and the
//      centre (maFrame[3]) is transformed as a point (centre = m3.x*row0 +
//      m3.y*row1 + m3.z*row2 + row3).
// Both paths: extent[i] = |axis[i]| * mfHalfHeight + mfRadius,
//             min = centre - extent, max = centre + extent; returns 1.
// ---------------------------------------------------------------------------
RwBool CapsuleVolume::GetBBox(const Vec4* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    f32 lfAxisX;
    f32 lfAxisY;
    f32 lfAxisZ;
    f32 lfCentreX;
    f32 lfCentreY;
    f32 lfCentreZ;

    if (lpTransform != nullptr)
    {
        const Vec4& lvM2   = maFrame[2];   // oriented axis
        const Vec4& lvM3   = maFrame[3];   // centre
        const Vec4& lvRow0 = lpTransform[0];
        const Vec4& lvRow1 = lpTransform[1];
        const Vec4& lvRow2 = lpTransform[2];
        const Vec4& lvRow3 = lpTransform[3];

        // Rotate the oriented axis by the transform's 3x3.
        lfAxisX = lvM2.x * lvRow0.x + lvM2.y * lvRow1.x + lvM2.z * lvRow2.x;
        lfAxisY = lvM2.x * lvRow0.y + lvM2.y * lvRow1.y + lvM2.z * lvRow2.y;
        lfAxisZ = lvM2.x * lvRow0.z + lvM2.y * lvRow1.z + lvM2.z * lvRow2.z;

        // Transform the centre as a point (row3 = translation).
        lfCentreX = lvM3.x * lvRow0.x + lvM3.y * lvRow1.x + lvM3.z * lvRow2.x + lvRow3.x;
        lfCentreY = lvM3.x * lvRow0.y + lvM3.y * lvRow1.y + lvM3.z * lvRow2.y + lvRow3.y;
        lfCentreZ = lvM3.x * lvRow0.z + lvM3.y * lvRow1.z + lvM3.z * lvRow2.z + lvRow3.z;
    }
    else
    {
        lfAxisX = maFrame[2].x;
        lfAxisY = maFrame[2].y;
        lfAxisZ = maFrame[2].z;
        lfCentreX = maFrame[3].x;
        lfCentreY = maFrame[3].y;
        lfCentreZ = maFrame[3].z;
    }

    // Half extent per axis: |axis| * mfHalfHeight + mfRadius.
    const f32 lfExtentX = std::fabs(lfAxisX) * mfHalfHeight + mfRadius;
    const f32 lfExtentY = std::fabs(lfAxisY) * mfHalfHeight + mfRadius;
    const f32 lfExtentZ = std::fabs(lfAxisZ) * mfHalfHeight + mfRadius;

    arResult.mMin = math::vpu::Vector3(lfCentreX - lfExtentX,
                                       lfCentreY - lfExtentY,
                                       lfCentreZ - lfExtentZ);
    arResult.mMax = math::vpu::Vector3(lfCentreX + lfExtentX,
                                       lfCentreY + lfExtentY,
                                       lfCentreZ + lfExtentZ);
    return 1;
}

} // namespace collision
} // namespace rw
