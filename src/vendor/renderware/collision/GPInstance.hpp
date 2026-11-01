#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
#include "SDKs/EATech/include/rw/math/vpu/vector3.h"
#include "vendor/renderware/collision/Feature.hpp"   // Vec4 / RwBool / FeatureEdge / Feature
#include "vendor/renderware/collision/VolRef.hpp"

// ===========================================================================
// rw::collision GP ("generalised primitive") narrow-phase vocabulary -- the
// reconstruction mirror of the vendor volume.h / rwccore.h declarations:
// GPInstance, Interval, the primitive-pair result records, the separating-
// axis (SAT) family, the rim-candidate helpers and the batch kernels.
//
// SHAPE AUTHORITY: the DecFIGS DWARF (references/DecFIGS/dwarfdump/SDKs/EATech/
// include/cmn/rw/collision/volume.h + primitivepairquery.h) and the Feb-2007
// vendor header (references/Feb-2007/BrnEntityModuleUnity/EARenderWare/stable/
// platform/include/native/ps3-ppu-gcc/rwccore.h:1011-3021) agree on every type
// and member name below; every OFFSET is X360-asm-attested by this wave's
// bodies (GPInstanceBatchIntersect{1xN,Nx1}, PrimitiveBatchIntersect,
// ComputeContactPoints, FindBestSeparatingDirection + _FindCandidates,
// FindBestSeparatingDirectionBoxBox, AddRimTo*Candidates).
//
// X360 DELTA vs the PS3 DWARF: on PS3 the per-type callbacks live in the
// static GPInstance::sVolumeMethods[6] table (pointers-to-member); the X360
// build caches a plain-function-pointer copy of that VolumeMethods block
// INSIDE each instance at +0xA4..+0xB3 (attested by the indirect calls
// `lwz r11, 0xA4/0xA8/0xAC(rInst)` across this TU), giving the console its
// 0xC0 instance stride. The pointer members widen on x64, so offsets past
// +0xA4 (and the total size) diverge from the console layout exactly as the
// committed pointer-bearing VolumeQuery layout does; all indexing is by
// sizeof(GPInstance).
//
// The 16-byte vector rows are carried as the shared scalar rw::collision::Vec4
// (rw::math::Vector3 / VecFloat in the vendor source) per this directory's
// precedent; VecFloat-returning functions return the broadcast's scalar lane
// as f32.
// ===========================================================================

namespace rw
{
namespace collision
{

class AABBox;   // vendor/renderware/collision/AABBox.hpp (GetBBox callback)

// ---------------------------------------------------------------------------
// rw::collision::Interval -- a projection interval onto an axis (DWARF
// volume.h:339; canonical rwccore.h:989). min/max are broadcast VecFloats;
// the GetInterval-style callbacks write min @+0x00 and max @+0x10. The
// console object is 16-byte aligned, so its array stride is 0x30 -- attested
// by FindBestSeparatingDirection @ 0x82BB4660 walking its interval arrays
// with `addi rX, rX, 0x30` -- hence the explicit alignment tail here.
// ---------------------------------------------------------------------------
struct Interval
{
    rw::math::vpu::VecFloat min;          // +0x00  lower projection bound (lane-broadcast)
    rw::math::vpu::VecFloat max;          // +0x10  upper projection bound (lane-broadcast)
    f32  padding[2];   // +0x20  DWARF: float32_t[2] padding
};

CGS_STATIC_ASSERT((sizeof(Interval) == 0x30), "Interval console array stride must be 0x30");

// ---------------------------------------------------------------------------
// rw::collision::GPInstance -- the register-friendly image of one collision
// primitive (DWARF volume.h:422; canonical rwccore.h:1011).
// ---------------------------------------------------------------------------
struct GPInstance
{
    enum VolumeType                    // DWARF volume.h:430
    {
        UNUSED           = 0,
        SPHERE           = 1,
        CAPSULE          = 2,
        TRIANGLE         = 3,
        BOX              = 4,
        CYLINDER         = 5,
        NUMINTERNALTYPES = 6,
        FORCEENUMSIZEINT = 2147483647
    };

    enum VolumeFlag                    // canonical rwccore.h:1027
    {
        FLAG_TRIANGLEONESIDED     = 0x0010,
        FLAG_TRIANGLEEDGE0CONVEX  = 0x0020,
        FLAG_TRIANGLEEDGE1CONVEX  = 0x0040,
        FLAG_TRIANGLEEDGE2CONVEX  = 0x0080,
        FLAG_TRIANGLEUSEEDGECOS   = 0x0100,
        FLAG_TRIANGLEVERT0DISABLE = 0x0200,
        FLAG_TRIANGLEVERT1DISABLE = 0x0400,
        FLAG_TRIANGLEVERT2DISABLE = 0x0800,
        FLAG_TRIANGLEDEFAULT      = FLAG_TRIANGLEUSEEDGECOS + FLAG_TRIANGLEEDGE0CONVEX
                                  + FLAG_TRIANGLEEDGE1CONVEX + FLAG_TRIANGLEEDGE2CONVEX,
        FLAG_TRIANGLEOLDMASK      = FLAG_TRIANGLEONESIDED + FLAG_TRIANGLEEDGE0CONVEX
                                  + FLAG_TRIANGLEEDGE1CONVEX + FLAG_TRIANGLEEDGE2CONVEX
    };

    // ARTIST stores these single-inheritance member pointers in four
    // 32-bit slots. Direction values use the native Vector3 register ABI.
    typedef void (GPInstance::*GetMaximumFeatureFn)(RwBool, rw::math::vpu::Vector3, Feature&) const;
    typedef void (GPInstance::*GetIntervalFn)(rw::math::vpu::Vector3, Interval&) const;
    typedef void (GPInstance::*GetIntervalsFn)(const rw::math::vpu::Vector3*, u32, Interval*) const;
    typedef void (GPInstance::*GetBBoxFn)(AABBox&) const;

    struct VolumeMethods               // DWARF volume.h:484
    {
        GetMaximumFeatureFn mGetMaximumFeature;   // console +0xA4
        GetIntervalFn       mGetInterval;         // console +0xA8
        GetIntervalsFn      mGetIntervals;        // console +0xAC
        GetBBoxFn           mGetBBox;             // console +0xB0
    };

    // ------------------------------------------------------------------------
    // rw::collision::GPInstance::ContactPoints -- the pair-contact result
    // ComputeContactPoints fills (DWARF volume.h:501; the Feb-2007 header
    // carries 8 pairs, the DWARF/X360 shape is 16 -- ComputeContactPoints'
    // point loop writes pairs at +0x30 + 0x20*i with the count at +0x10).
    // ------------------------------------------------------------------------
    struct ContactPoints
    {
        struct PointPair
        {
            rw::math::vpu::Vector3 p1;   // +0x00  contact point on the first primitive
            rw::math::vpu::Vector3 p2;   // +0x10  contact point on the second primitive
        };

        u32       volumeTag1;      // +0x00  first->mVolumeTag
        u32       volumeTag2;      // +0x04  second->mVolumeTag
        u32       userTag1;        // +0x08  first->mUserTag
        u32       userTag2;        // +0x0C  second->mUserTag
        u32       numPoints;       // +0x10  contact-pair count
        u32       mPad14[3];       // +0x14  16-byte alignment of `normal`
        rw::math::vpu::Vector3 normal;          // +0x20  NEGATED contact normal
        PointPair pointPairs[16];  // +0x30  fattened point pairs
    };

    // --- members (DWARF volume.h:551-563; console offsets in the comments) --
    rw::math::vpu::Vector3 mPos;               // +0x00  primitive centre / triangle vert 0
    rw::math::vpu::Vector3 mFaceNormals[3];    // +0x10  unit face axes
    rw::math::vpu::Vector3 mEdgeDirections[3]; // +0x40  edge directions (cylinder axis in [0])
    rw::math::vpu::Vector3 mDimensions;        // +0x70  half-extents / (half-length, radius)
    f32           mFatness;           // +0x80  surface fatness (radius pad)
    u32           mVolumeTag;         // +0x84  owning Volume tag word (VolRef +0x70)
    u32           mUserTag;           // +0x88  user tag word
    u8            mNumFaceNormals;    // +0x8C  0..3
    u8            mNumEdgeDirections; // +0x8D  0..3
    VolumeType    mVolumeType;        // +0x90  dispatch index 0..5
    u32           mFlags;             // +0x94  VolumeFlag bits
    f32           mEdgeData[3];       // +0x98  per-edge data (triangle edge cos)
    VolumeMethods mMethods;           // console +0xA4  (x64: pointer-widened; the
                                      // X360 instance stride is 0xC0)
};

CGS_STATIC_ASSERT((offsetof(GPInstance, mFatness)           == 0x80), "GPInstance::mFatness");
CGS_STATIC_ASSERT((offsetof(GPInstance, mVolumeTag)         == 0x84), "GPInstance::mVolumeTag");
CGS_STATIC_ASSERT((offsetof(GPInstance, mUserTag)           == 0x88), "GPInstance::mUserTag");
CGS_STATIC_ASSERT((offsetof(GPInstance, mNumFaceNormals)    == 0x8C), "GPInstance::mNumFaceNormals");
CGS_STATIC_ASSERT((offsetof(GPInstance, mNumEdgeDirections) == 0x8D), "GPInstance::mNumEdgeDirections");
CGS_STATIC_ASSERT((offsetof(GPInstance, mVolumeType)        == 0x90), "GPInstance::mVolumeType");
CGS_STATIC_ASSERT((offsetof(GPInstance, mFlags)             == 0x94), "GPInstance::mFlags");
CGS_STATIC_ASSERT((offsetof(GPInstance, mEdgeData)          == 0x98), "GPInstance::mEdgeData");
CGS_STATIC_ASSERT((offsetof(GPInstance, mMethods) == 0xA4), "GPInstance::mMethods");
CGS_STATIC_ASSERT((sizeof(GPInstance::VolumeMethods) == 0x10), "GPInstance methods size");
CGS_STATIC_ASSERT((sizeof(GPInstance) == 0xC0), "GPInstance size");

// ---------------------------------------------------------------------------
// GP primitives have no extra data members; their callbacks use native
// member pointers and Vector3 register-value arguments.
//
// Each concrete GetBBox callback is an independently declared member.
// ARTIST folds the five empty bodies to 0x82AD5078.
// ---------------------------------------------------------------------------

// The GP sphere image (canonical rwccore.h:1182: the core primitive is a
// point -- the radius rides in mFatness and is applied by the callers).
struct GPSphere : public GPInstance
{
    void GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const;
    void GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const;
    void GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const;
    void GetBBox(AABBox& arBBox) const;
};

// The GP capsule image (canonical rwccore.h:1193: the core is the axis
// segment -- mEdgeDirections[0] is the axis, mDimensions.x the half-height,
// the radius rides in mFatness).
struct GPCapsule : public GPInstance
{
    void GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const;
    void GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const;
    void GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const;
    void GetBBox(AABBox& arBBox) const;
};

// The GP box image (canonical rwccore.h:1207: mFaceNormals are the unit face
// axes, mDimensions the half-extents).
struct GPBox : public GPInstance
{
    // @ 0x82BA8918 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    void GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const;
    // @ 0x82BA8650 -- VolumeMethods +0xA8 (GetIntervalFn).
    void GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const;
    // @ 0x82BA9238 -- VolumeMethods +0xAC (GetIntervalsFn).
    void GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const;
    void GetBBox(AABBox& arBBox) const;
};

// The GP cylinder image (canonical rwccore.h:1253: no extra data members --
// mEdgeDirections[0] is the axis, mDimensions.x/.y the half-length/radius;
// per the canonical GPCylinder::Initialize rwccore.h:1390 mFaceNormals[0] is
// the axis with [1]/[2] the two perpendicular frame axes).
struct GPCylinder : public GPInstance
{
    // @ 0x82BAE430 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    void GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const;
    // @ 0x82BAD7F8 -- VolumeMethods +0xA8 (GetIntervalFn).
    void GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const;
    void GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const;
    void GetBBox(AABBox& arBBox) const;
};

// The GP triangle image (canonical rwccore.h:1229: no extra data members --
// the three vertices alias mPos (+0x00), mFaceNormals[1] (+0x20) and
// mFaceNormals[2] (+0x30); mFaceNormals[0] (+0x10) is the unit face normal
// with mNumFaceNormals == 1; mEdgeDirections[0..2] (+0x40/+0x50/+0x60) are the
// three edge directions and mDimensions (+0x70) carries the three edge lengths
// in lanes x/y/z. The vertex aliases are the same the committed
// AABBoxBuilder::CreateFromTriangle and TriangleVolume trade in).
struct GPTriangle : public GPInstance
{
    // @ 0x82BBA158 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    void GetMaximumFeature(RwBool abCcw, rw::math::vpu::Vector3 arDir, Feature& arFeature) const;
    // @ 0x82BBA6A0 -- VolumeMethods +0xA8 (GetIntervalFn).
    void GetInterval(rw::math::vpu::Vector3 arDir, Interval& arInterval) const;
    // @ 0x82BBA6E0 -- VolumeMethods +0xAC (GetIntervalsFn).
    void GetIntervals(const rw::math::vpu::Vector3* lapDirs, u32 auNumDirs, Interval* lapIntervals) const;
    void GetBBox(AABBox& arBBox) const;
};

// @ 0x82BBA748 -- point-vs-triangle nearest-feature query, HOMED in
// GPTriangle.cpp (waveQ5 C1). A standalone leaf that IDA merges into the
// GPTriangle::GetIntervals chunk because it starts one instruction after that
// function's `blr`; the console TU is one run holding the GP triangle
// callbacks AND the triangle line tests, and its only call site is
// FatTriangleLineSegIntersect @ 0x82BBB10C. X360: r3/r4/r5 = out nearest
// point / out s / out t, v1..v4 = the query point and V0/V1/V2 by value;
// returns the region code (0..2 vertex V0/V1/V2, 3..5 edge V0V1/V0V2/V1V2,
// 6 face interior). The nearest point is V0 + (V1-V0)*s + (V2-V0)*t.
//
// NOTE for the LineSegIntersect owner: the parallel declaration at
// LineSegIntersect.hpp:180 carries an extra `f32* lpfPlaneDistance` fourth
// parameter that the console function does not have (it never writes f3 --
// see the body's banner). That declaration has no definition anywhere; this
// one does.
s32 TriangleNearestPointRegion(rw::math::vpu::Vector3* lpNearest, f32* lpfParamS, f32* lpfParamT,
                               rw::math::vpu::Vector3 aPoint, rw::math::vpu::Vector3 aV0, rw::math::vpu::Vector3 aV1, rw::math::vpu::Vector3 aV2);

// ---------------------------------------------------------------------------
// rw::collision::PrimitivePairIntersectResult -- one narrow-phase result slot
// (DWARF primitivepairquery.h:47; canonical rwccore.h:2957). Console stride
// 0x750 == the "1872 bytes/result" report-buffer math of the committed
// VolumeQuery.cpp; both batch kernels walk it with `mulli r11, rN, 0x750`.
//
// WIDTH NOTE: v1/v2 are `const Volume*` in the DWARF; they are carried as the
// console 32-bit pointer image (the batch kernels copy GPInstance::mVolumeTag
// words into them verbatim) so the record keeps its exact 0x750 stride.
//
// ASM vs DWARF DELTA (attested twice, 0x82BAB4A8 + 0x82BAACD8): the pass-1
// dispatch separation is stored to `distance` (+0x4F0) and re-read from there
// by pass 3 (`addi r30, r26, 0x4F0` / `lfs f0, 0x50(r23)` with r23 at +0x4A0);
// `sepDist` (+0x4B0) is NOT touched by the batch kernels. The single-pair
// entry point PrimitivePairIntersect @ 0x82BAC130 is the OTHER half of the
// pairing: it DOES write `sepDist` (stfs f31, 0x4B0) with the coarse
// separation, and also writes `distance` (+0x4F0) with the final reference
// separation along the contact normal -- both stores are correct per their
// own asm; consumers must read the field their producer fills.
// ---------------------------------------------------------------------------
struct PrimitivePairIntersectResult
{
    enum
    {
        MAXPOINTCOUNT = 2 * Feature::MAXEDGECOUNT
    };

    const Volume* v1;                          // +0x000  first Volume (console pointer image)
    u32     tag1;                        // +0x004  first user tag
    const Volume* v2;                          // +0x008  second Volume (console pointer image)
    u32     tag2;                        // +0x00C  second user tag
    s32     vNindex;                     // +0x010  source pair index; -1 = fatness-culled
    Feature f1;                          // +0x020  support feature on the first side
    Feature f2;                          // +0x260  support feature on the second side
    rw::math::vpu::Vector3 sepDir;                      // +0x4A0  coarse separating direction (pass 1)
    f32     sepDist;                     // +0x4B0  (untouched by the batch kernels)
    rw::math::vpu::Vector3 normal;                      // +0x4C0  final contact normal
    rw::math::vpu::Vector3 pointOn1;                    // +0x4D0  reference contact point, first side
    rw::math::vpu::Vector3 pointOn2;                    // +0x4E0  reference contact point, second side
    f32     distance;                    // +0x4F0  separation along the direction
    rw::math::vpu::Vector3 pointsOn1[MAXPOINTCOUNT];    // +0x500  contact points, first side
    rw::math::vpu::Vector3 pointsOn2[MAXPOINTCOUNT];    // +0x600  contact points, second side
    f32     distances[MAXPOINTCOUNT];    // +0x700  per-point separation along normal
    u32     numPoints;                   // +0x740  contact-point count (0 = rejected)
};

CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, f1)        == 0x020), "PPIR::f1");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, f2)        == 0x260), "PPIR::f2");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, sepDir)    == 0x4A0), "PPIR::sepDir");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, sepDist)   == 0x4B0), "PPIR::sepDist");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, normal)    == 0x4C0), "PPIR::normal");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, pointOn1)  == 0x4D0), "PPIR::pointOn1");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, pointOn2)  == 0x4E0), "PPIR::pointOn2");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, distance)  == 0x4F0), "PPIR::distance");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, pointsOn1) == 0x500), "PPIR::pointsOn1");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, pointsOn2) == 0x600), "PPIR::pointsOn2");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, distances) == 0x700), "PPIR::distances");
CGS_STATIC_ASSERT((offsetof(PrimitivePairIntersectResult, numPoints) == 0x740), "PPIR::numPoints");
CGS_STATIC_ASSERT((sizeof(PrimitivePairIntersectResult)              == 0x750), "PPIR stride");

// ---------------------------------------------------------------------------
// rwc_FeatureIntersectionPrism -- the Find*Prism family's result block
// (canonical rwccore.h:1801). Offsets attested across the family's asm:
// normal @+0x200 (FindFaceEdgePrism's override store), m_numpts @+0x210
// (every worker), normalOverride @+0x214 (caller-zeroed; set by
// FindFaceEdgePrism's parallel-overlap path).
// ---------------------------------------------------------------------------
struct rwc_FeatureIntersectionPrism
{
    rw::math::vpu::Vector3 m_ptsOn1[2 * Feature::MAXEDGECOUNT];   // +0x000  points on feature 1
    rw::math::vpu::Vector3 m_ptsOn2[2 * Feature::MAXEDGECOUNT];   // +0x100  points on feature 2
    rw::math::vpu::Vector3 normal;                                // +0x200  override contact normal
    s32    m_numpts;                              // +0x210  point-pair count
    RwBool normalOverride;                        // +0x214  caller zeroes; worker sets
};

CGS_STATIC_ASSERT((offsetof(rwc_FeatureIntersectionPrism, normal)         == 0x200), "prism::normal");
CGS_STATIC_ASSERT((offsetof(rwc_FeatureIntersectionPrism, m_numpts)       == 0x210), "prism::m_numpts");
CGS_STATIC_ASSERT((offsetof(rwc_FeatureIntersectionPrism, normalOverride) == 0x214), "prism::normalOverride");

// ---------------------------------------------------------------------------
// rw::collision::VolRef1xN -- one variable-length overlap-report group (DWARF
// volume.h:1131). GetPrimitiveBBoxOverlaps stages a packed run of these;
// PrimitiveBatchIntersect consumes them. On the console every field is one
// 4-byte word (group advance = vRefsNCount + 3 words); the x64 layout widens
// the pointers, so the group advance below goes through NextGroup().
// ---------------------------------------------------------------------------
struct VolRef1xN
{
    VolRef* vRef1;           // word 0  the "1"-side volume reference
    u32     vRefsNCount;     // word 1  count of trailing "N"-side references
    RwBool  volumesSwapped;  // word 2  nonzero => operands swapped (N side first)
    VolRef* vRefsN[1];       // word 3+ the "N"-side references (vRefsNCount of them)

    // The next packed group starts right after vRefsN[vRefsNCount-1]
    // (console: `lpuGroup += lpuGroup[1] + 3`).
    VolRef1xN* NextGroup()
    {
        return reinterpret_cast<VolRef1xN*>(&vRefsN[vRefsNCount]);
    }
};

// ---------------------------------------------------------------------------
// Separating-axis (SAT) family. Canonical shapes rwccore.h:3144-3166 /
// :3830-3845: each writes the best separating direction (oriented from gp1
// toward gp2) through arBestSepDir and returns the signed separation along it
// (native VecFloat returned in v1). Negative return =
// penetration depth.
// ---------------------------------------------------------------------------

// X360 off_82F91800 -- the 6x6 [type1][type2] dispatch table of the family
// (row stride 6 attested by `mulli r11, rN, 6` in both batch kernels and
// ComputeContactPoints). RECOVERED + DEFINED 2026-08-18 (waveQ5 C1): the
// registration TU is GPRegistration.cpp (canonical FixupSepDirMethods,
// rwccore.h:2955); all 36 slots are dumped there entry by entry.
typedef rw::math::vpu::VecFloat (*FindBestSeparatingDirectionFn)(rw::math::vpu::Vector3& arBestSepDir,
                                             const GPInstance& arGP1,
                                             const GPInstance& arGP2);
extern const FindBestSeparatingDirectionFn
    gapFindBestSeparatingDirection[GPInstance::NUMINTERNALTYPES][GPInstance::NUMINTERNALTYPES];

// X360 unk_82F918F0 -- the sibling per-VolumeType VolumeMethods table each
// volume's CreateGPInstance copies into GPInstance::mMethods (stride 0x10 == 4
// pointers; row 0 == VOLUMETYPEUNUSED is all-zero). Same home, GPRegistration.cpp.
extern const GPInstance::VolumeMethods
    g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];

// @ 0x82BB4038 -- fill lapCandidates with the SAT candidate directions for the
// pair (face normals, normalised edge crosses, capsule fallbacks, centre
// delta); returns the end pointer of the filled array, or NULL if none
// survived the degeneracy filter. (Same TU as FindBestSeparatingDirection;
// the caller provides 15 usable 16-byte slots.)
rw::math::vpu::Vector3* FindBestSepDir_FindCandidates(rw::math::vpu::Vector3* lapCandidates,
                                    const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB4660 -- generic SAT driver: collect candidates, batch-project both
// primitives (mMethods.mGetIntervals), keep the axis with the greatest
// interval separation.
rw::math::vpu::VecFloat FindBestSeparatingDirection(rw::math::vpu::Vector3& arBestSepDir,
                                const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB47F0 -- box/box SAT over the 6 face normals + 9 edge crosses.
rw::math::vpu::VecFloat FindBestSeparatingDirectionBoxBox(rw::math::vpu::Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// PENDING (not delivered this wave; body in the same ledger TU family). The
// tri/box worker FindBestSeparatingDirectionBoxTri @ 0x82BAA1A0 tail-calls.
rw::math::vpu::VecFloat FindBestSeparatingDirectionTriBox(rw::math::vpu::Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BAA1A0 -- canonical inline rwccore.h:3151 emitted out of line: run the
// tri/box worker swapped, then flip the direction.
rw::math::vpu::VecFloat FindBestSeparatingDirectionBoxTri(rw::math::vpu::Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BAA1F0 -- canonical inline rwccore.h:3158 emitted out of line: the
// normalised centre1->centre2 axis; returns the centre distance (0 when
// coincident).
rw::math::vpu::VecFloat FindBestSeparatingDirectionSphSph(rw::math::vpu::Vector3& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB76E8 -- cylinder SAT driver (canonical rwccore.h:3831): build the
// rim-aware candidate table, then evaluate it.
rw::math::vpu::VecFloat FindBestSepDirWithCylinder(rw::math::vpu::Vector3& arBestSepDir,
                               const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

// @ 0x82BAA250 -- canonical inline rwccore.h:3834 emitted out of line: pure
// tail-call thunk onto FindBestSepDirWithCylinder (cylinder-first operand
// order is the worker's native order).
rw::math::vpu::VecFloat FindBestSeparatingDirCylVol(rw::math::vpu::Vector3& arBestSepDir,
                                const GPInstance& arGPCylinder, const GPInstance& arGPOther);

// @ 0x82BAA258 -- the mirrored thunk (the CYLINDER-as-gp2 column of the
// dispatch table): call the worker with the operands swapped so the cylinder
// is first, then flip the sign of the stored direction. Same shape as
// FindBestSeparatingDirectionBoxTri @ 0x82BAA1A0.
rw::math::vpu::VecFloat FindBestSeparatingDirVolCyl(rw::math::vpu::Vector3& arBestSepDir,
                                const GPInstance& arGPOther, const GPInstance& arGPCylinder);

// ---------------------------------------------------------------------------
// Cylinder-rim SAT candidate helpers (X360-era additions; no Feb-2007
// canonical declarations -- names are the ledger's).
// ---------------------------------------------------------------------------

// @ 0x82BB53F8 -- candidate separating direction between a cylinder rim
// circle (centre, unit axis, lane-broadcast radius) and the edge segment
// [aEdgeP1, aEdgeP2]: a 10-step damped regula-falsi on the rim-to-edge gap.
// The X360 passes all five vectors by value in VMX v1..v5 and returns the
// direction in v1.
rw::math::vpu::Vector3 RimToEdge(rw::math::vpu::Vector3 aRimCentre, rw::math::vpu::Vector3 aRimAxis, rw::math::vpu::VecFloat aRimRadius,
               rw::math::vpu::Vector3 aEdgeP1, rw::math::vpu::Vector3 aEdgeP2);

// 82BB56C8 reads reference arguments r3..r9 and writes the Vector3 at r10.
// Radius VecFloat is inferred from the broadcast producer and register ABI.
RwBool RimToRim(const rw::math::vpu::Vector3& aRimCentre1, const rw::math::vpu::Vector3& aRimAxis1, const rw::math::vpu::VecFloat& aRimRadius1,
                const rw::math::vpu::Vector3& aRimCentre2, const rw::math::vpu::Vector3& aRimAxis2, const rw::math::vpu::VecFloat& aRimRadius2,
                const rw::math::vpu::Vector3& aSepDir, rw::math::vpu::Vector3* lpCandidate);

void AddRimToEdgeCandidate(const rw::math::vpu::Vector3& arRimCentre, const rw::math::vpu::Vector3& arRimAxis,
                           const rw::math::vpu::VecFloat& arRimRadius, const rw::math::vpu::Vector3& arEdgePoint,
                           const rw::math::vpu::Vector3& arEdgeDirection, f32 afEdgeOffset, f32 afEdgeHalfLength,
                           rw::math::vpu::Vector3* lapCandidateDirs, u32* lpuNumCandidateDirs);
void AddRimToRimCandidates(const GPInstance* lpGP1, const GPInstance* lpGP2,
                           rw::math::vpu::Vector3* lapCandidateDirs, u32* lpuNumCandidateDirs);

// ---------------------------------------------------------------------------
// Feature-pair prism intersection family (canonical rwccore.h:1810-1836).
// Register contract (X360 __fastcall): r3=&res, r4=ptsOn1, r5=ptsOn2, r6=&f1,
// r7=&f2, r8=&sepDir; the dispatcher aims ptsOn1/ptsOn2 at res.m_ptsOn1/2 in
// the pairing that keeps each array with its own source feature. The features
// are non-const (the workers fold region codes into Feature::region).
// ---------------------------------------------------------------------------

// @ 0x82BB77C0 -- edge vs point.
RwBool FindEdgePointPrism(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOn1, rw::math::vpu::Vector3* lapPtsOn2,
                          Feature& arEdgeFeature, Feature& arPointFeature, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB78A8 -- edge vs edge (skew closest approach, or the parallel
// interval clip through FindIntervalOverlap).
RwBool FindEdgeEdgePrism(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOn1, rw::math::vpu::Vector3* lapPtsOn2,
                         Feature& arEdgeFeature1, Feature& arEdgeFeature2, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB7F18 -- face vs point.
RwBool FindFacePointPrism(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOnF, rw::math::vpu::Vector3* lapPtsOnP,
                          Feature& arFaceFeature, Feature& arPointFeature, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB83A8 -- face vs edge.
RwBool FindFaceEdgePrism(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOnF, rw::math::vpu::Vector3* lapPtsOnE,
                         Feature& arFaceFeature, Feature& arEdgeFeature, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB8798 -- generic face vs face (simultaneous edge-ring walk).
RwBool FindFaceFacePrism(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOn1, rw::math::vpu::Vector3* lapPtsOn2,
                         Feature& arFaceFeature1, Feature& arFaceFeature2, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB8CA0 -- 4-edge face vs 3-edge face specialisation.
RwBool FindFaceFacePrism4x3(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOn1, rw::math::vpu::Vector3* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB9400 -- 4-edge face vs 4-edge face specialisation.
RwBool FindFaceFacePrism4x4(rwc_FeatureIntersectionPrism& arRes, rw::math::vpu::Vector3* lapPtsOn1, rw::math::vpu::Vector3* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB9BD8 -- dispatch on the two features' edge counts (0 = point,
// 1 = edge, >1 = face) to the specialised worker, resolving the point/point
// case inline. Returns nonzero when an intersection prism was found.
RwBool FindFeatureIntersectionPrism(rwc_FeatureIntersectionPrism& arRes,
                                    Feature& arFeature1, Feature& arFeature2,
                                    const rw::math::vpu::Vector3& arSepDir);

// @ 0x82BB7828 -- clip arInterval1 against arInterval2 into *lpResult and
// report whether the clipped interval is non-empty (strict, all four lanes).
// Called by FindEdgeEdgePrism.
u32 FindIntervalOverlap(Interval* lpResult,
                        const Interval* lpInterval1, const Interval* lpInterval2);

// ---------------------------------------------------------------------------
// Narrow-phase entry points.
// ---------------------------------------------------------------------------

// @ 0x82BAA600 -- HOMED in PrimitiveIntersect.cpp (waveQ5 C1), where the
// binary puts it: between FindBestSeparatingDirCylVol and
// GPInstanceBatchIntersectNx1, with its three same-TU static helpers
// (0x82BAA2A8 / 0x82BAA378 / 0x82BAA4A8) that nothing else calls.
// Validates a proposed contact normal against a GPTriangle instance
// (this = r3, normal = v1): the per-edge convexity flags, the per-vertex
// disable flags and the FLAG_TRIANGLEUSEEDGECOS edge cosines decide whether a
// contact whose normal falls outside the triangle's own face cone is real
// geometry or a neighbour's. A zero return discards the contact; a nonzero
// return accepts it -- and the console literally returns the tested flag BIT
// (0x20/0x40/0x80) on the no-edge-cos paths, so treat the result as a
// truthiness, never as 0/1.
// FLAGGED: the name is descriptive, not SDK-attested (the binary symbol is
// unnamed and no DWARF/Feb-2007 declaration exists for it).
u32 GPTriangleAcceptContactNormal(const GPInstance* lpTriangle, rw::math::vpu::Vector3 arNormal);

// @ 0x82BABDA8 -- narrow-phase contact generation for one GP-instance pair
// (canonical rwccore.h:1180). Returns the number of contact-point pairs
// written to arResult (0 on any rejection).
u32 ComputeContactPoints(const GPInstance& arGP1, const GPInstance& arGP2,
                         const f32& arPadding, GPInstance::ContactPoints& arResult);

// @ 0x82BAB4A8 -- batch-intersect one instance against N (canonical
// rwccore.h:2985). Surviving pairs are compacted to the front of lapResults;
// returns the surviving count.
s32 GPInstanceBatchIntersect1xN(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance& arInst1,
                                const GPInstance* lapInstsN, s32 aiNum,
                                f32 afPadding);

// @ 0x82BAACD8 -- batch-intersect N instances against one (canonical
// rwccore.h:2990).
s32 GPInstanceBatchIntersectNx1(PrimitivePairIntersectResult* lapResults,
                                s32 aiResBufMaxSize,
                                const GPInstance* lapInsts1, s32 aiNum,
                                const GPInstance& arInst2,
                                f32 afPadding);

// @ 0x82BABC78 -- narrow-phase batch driver over the staged overlap-report
// groups (canonical rwccore.h:3021 declares VolRefPair*; the X360 asm consumes
// the variable-length DWARF VolRef1xN groups -- see the definition). Instances
// each group's volumes into lapInstancingBuffer via the collision-volume
// vtable, then hands the group to the matching batch kernel. Returns the total
// number of intersections written.
s32 PrimitiveBatchIntersect(PrimitivePairIntersectResult* lapResults,
                            s32 aiResBufMaxSize,
                            GPInstance* lapInstancingBuffer,
                            VolRef1xN* lapPairs, s32 aiNumPairs,
                            f32 afPadding);

// vendor/renderware/collision/CollisionVolume.hpp (128-byte image).
struct Volume;

// @ 0x82BAC130 -- single-pair narrow-phase entry point (canonical
// rwccore.h:3001: RwBool PrimitivePairIntersect(PrimitivePairIntersectResult&,
// const Volume*, const Matrix44Affine*, const Volume*, const Matrix44Affine*,
// float padding, const Vector4* sepDir)). apSepDir, when non-NULL, carries the caller's
// separating direction in xyz and the separation distance in w (lfs f31,
// 0xC(r30)). Called by BrnPhysics::Vehicle::VehicleManager::
// PredictCarCarIntersection and the CgsSceneManager OverlapCullingModule.
RwBool PrimitivePairIntersect(PrimitivePairIntersectResult& arResult,
                              const Volume* apVolume1, const rw::math::vpu::Matrix44Affine* apMtx1,
                              const Volume* apVolume2, const rw::math::vpu::Matrix44Affine* apMtx2,
                              f32 afPadding, const rw::math::vpu::Vector4* apSepDir);

} // namespace collision
} // namespace rw
