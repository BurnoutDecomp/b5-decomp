#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "vendor/renderware/collision/Feature.hpp"           // Vec4 / RwBool / FeatureEdge / Feature
#include "vendor/renderware/collision/AABBox.hpp"             // AABBox (GetBBox out)
#include "vendor/renderware/collision/LineSegIntersect.hpp"   // VolumeLineSegIntersectResult

// ===========================================================================
// rw::collision::TriangleVolume -- the RenderWare collision "triangle volume":
// three vertices plus a lazily-recomputed unit face normal and the per-triangle
// GP-instance parameters (per-edge cosine, surface fatness, feature/behaviour
// flags). Clustered-mesh line/bbox queries build one of these per unit
// (rw::collision::ClusteredMesh::GetUnitVolumes) and drive the narrow phase
// through it.
//
// OWNING HOME for the TriangleVolume functions the X360 binary defines:
//     rw::collision::TriangleVolume::Initialize        @ 0x82BB0680
//     rw::collision::TriangleVolume::GetNormal         @ 0x82BB0580
//     rw::collision::TriangleVolume::GetPoints         @ 0x82BB11F8
//     rw::collision::TriangleVolume::GetBBox           @ 0x82BBA038
//     rw::collision::TriangleVolume::GetMaximumFeature @ 0x82BBACD0
//     rw::collision::TriangleVolume::LineSegIntersect  @ 0x82BBB970
//     rw::collision::TriangleVolume::CreateGPInstance  @ 0x82BBAA00  (declared
//        below; body pending -- the former rodata blocker is CRACKED: the
//        unk_82CDA350 vperm control is dumped and off_82F91920 is row [3] of
//        the recovered VolumeMethods table unk_82F918F0. See
//        scratchpad/waveN/TriangleVolume.spec.md for the recovered tables.)
//
// NO DWARF / Feb-2007 source exists for this TU. The LAYOUT below is entirely
// X360-asm-attested (member OFFSETS are ground truth, pinned by the
// static_asserts; the member NAMES are inferred and documented). All accesses
// are by named member per this directory's precedent (FeatureEdge / Feature /
// GPInstance). The 16-byte vector rows are carried as the shared scalar
// rw::collision::Vec4 (rw::math::Vector3 / VecFloat in the vendor source), the
// same vocabulary FeatureEdge / TriangleLineSegIntersect trade in.
// ===========================================================================

namespace rw
{
namespace collision
{

struct GPInstance;   // vendor/renderware/collision/GPInstance.hpp (reference-only
                     // here; same forward-decl precedent as CapsuleVolume.hpp)

class TriangleVolume
{
public:
    // Canonical default ctor (members left uninitialised until Initialize).
    TriangleVolume() {}

    // @ 0x82BB0680 -- stamp the triangle (arP0/arP1/arP2 ride in VMX v1/v2/v3)
    // into the volume referenced by *appVolume and return that volume (NULL when
    // the slot is empty). X360 __fastcall: r3 = the slot holding the volume
    // pointer (`lwz r11, 0(r3)`), v1/v2/v3 = the three vertices by value.
    // (The slot is a Volume**/VolRef-style handle; its +0x00 word is the
    // volume pointer, attested by the single dereference.)
    static TriangleVolume* Initialize(TriangleVolume** appVolume,
                                      const Vec4& arP0, const Vec4& arP1, const Vec4& arP2);

    // @ 0x82BB0580 -- write the triangle's world-space unit normal to arNormal.
    // Recomputes and caches the local normal on demand (the mFlags 0x2
    // "dirty" bit), then rotates it by the optional 3x3 of lpTransform (NULL =
    // identity). const: the recompute writes only the mutable cache.
    void GetNormal(Vec4& arNormal, const Vec4* lpTransform) const;

    // @ 0x82BB11F8 -- write the three (optionally transformed) triangle
    // vertices to arP0/arP1/arP2. lpTransform NULL copies the local vertices;
    // otherwise each is affine-transformed by lpTransform's four rows.
    void GetPoints(Vec4& arP0, Vec4& arP1, Vec4& arP2, const Vec4* lpTransform) const;

    // @ 0x82BBA038 -- write the (optionally transformed) fattened AABB of the
    // triangle to arResult and return 1. abTight is accepted but unused by the
    // triangle (r5 is dead in the asm). lpTransform NULL uses local space.
    RwBool GetBBox(const Vec4* lpTransform, RwBool abTight, AABBox& arResult) const;

    // @ 0x82BBACD0 -- build the triangle's maximum contact feature (its three
    // boundary edges) into arResult. arDir is accepted but unused (a triangle's
    // whole face is always its maximum feature; r4 is dead in the asm).
    void GetMaximumFeature(const Vec4& arDir, Feature& arResult) const;

    // @ 0x82BBB970 -- intersect the segment [aLineStart, aLineEnd] (rides in
    // VMX v1/v2) against the (optionally transformed) fat triangle, filling
    // lpResult. Returns TriangleLineSegIntersect's hit code. X360 __fastcall:
    // r4 = lpTransform, r5 = lpResult, f1 = afFatness.
    s32 LineSegIntersect(const Vec4* lpTransform, VolumeLineSegIntersectResult* lpResult,
                         f32 afFatness, Vec4 aLineStart, Vec4 aLineEnd) const;

    // @ 0x82BBAA00 -- fill arInst with the triangle's GP narrow-phase image:
    // mPos = (optionally transformed) vert 0, mFaceNormals[0] = the GetNormal
    // face normal, mFaceNormals[1]/[2] = verts 1/2 (the GPTriangle aliasing),
    // mEdgeDirections[0..2] = the unit edges (P2-P0, P1-P2, P0-P1),
    // mDimensions = the three edge lengths (w lane = |P2-P0|, per the dumped
    // unk_82CDA350 vperm control), mEdgeData[0..2] = mafEdgeCos, plus the
    // fatness/tag/count/type/flag words and the TRIANGLE VolumeMethods row.
    // Returns 1. X360 __fastcall: r3 = this, r4 = &arInst, r5 = lpTransform
    // (NULL = the local frame; rows are w-masked to (0,0,0 | 1) when present).
    // Body pending: see scratchpad/waveN/TriangleVolume.spec.md.
    RwBool CreateGPInstance(GPInstance& arInst, const Vec4* lpTransform) const;

    // Degenerate-area epsilon the GetNormal vcmpgtfp guards the normalise with
    // (keep the raw cross product when |n|^2 does not exceed it).
    // MEASURED: X360 unk_8327EF10 is splatted at static-init (thunk 0x82C73C70)
    // from .rdata 0x82180554 = 0x34000000 = 2^-23 = FLT_EPSILON.
    static const f32 KF_DEGENERATE_EPSILON;   // X360 unk_8327EF10 = 2^-23

    // --- members (X360-asm-attested offsets; inferred names) ----------------
    Vec4         maVerts[3];    // +0x00 / +0x10 / +0x20  triangle vertices
    mutable Vec4 mNormal;       // +0x30  cached unit face normal (GetNormal)
    u32          mInitWord;     // +0x40  console 32-bit POINTER IMAGE of the
                                //        triangle Volume vtable (&unk_82F919A4,
                                //        registry dword_8327EEE0[3]; see .cpp --
                                //        FLAG host width: what GetVolumeVTable
                                //        reads back from Volume+0x40)
    f32          mafEdgeCos[3]; // +0x44 / +0x48 / +0x4C  per-edge cosine (-1 = unset)
    f32          mfFatness;     // +0x50  surface fatness (0 by default)
    u32          mu54;          // +0x54  (Initialize: 0)
    u32          mu58;          // +0x58  (Initialize: 0)
    mutable u32  mFlags;        // +0x5C  feature/behaviour flags (0x1E3 default;
                                //        bit 0x2 = "normal dirty")
};

static_assert(offsetof(TriangleVolume, maVerts)   == 0x00, "TriangleVolume::maVerts");
static_assert(offsetof(TriangleVolume, mNormal)   == 0x30, "TriangleVolume::mNormal");
static_assert(offsetof(TriangleVolume, mInitWord) == 0x40, "TriangleVolume::mInitWord");
static_assert(offsetof(TriangleVolume, mafEdgeCos)== 0x44, "TriangleVolume::mafEdgeCos");
static_assert(offsetof(TriangleVolume, mfFatness) == 0x50, "TriangleVolume::mfFatness");
static_assert(offsetof(TriangleVolume, mFlags)    == 0x5C, "TriangleVolume::mFlags");

} // namespace collision
} // namespace rw
