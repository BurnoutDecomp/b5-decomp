#pragma once

#include <cstddef>   // offsetof (layout pins)

#include "types.hpp"
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
    Vec4 min;          // +0x00  lower projection bound (lane-broadcast)
    Vec4 max;          // +0x10  upper projection bound (lane-broadcast)
    f32  padding[2];   // +0x20  DWARF: float32_t[2] padding
    f32  mPad28[2];    // +0x28  16-byte alignment tail (console aligned(16))
};

static_assert(sizeof(Interval) == 0x30, "Interval console array stride must be 0x30");

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

    // --- per-type callbacks (X360: plain function pointers cached inline; the
    //     canonical member-function signatures are volume.h:472-475). The
    //     console passes the direction in VMX v1; here it rides as an explicit
    //     const-reference parameter in the canonical argument order.

    // console +0xA4 -- build the primitive's maximum feature along arDir into
    // arFeature. abCcw is 1 for the first/A side of a pair, 0 for the
    // second/B side (which receives the negated direction).
    typedef void (*GetMaximumFeatureFn)(const GPInstance* lpThis, RwBool abCcw,
                                        const Vec4& arDir, Feature& arFeature);
    // console +0xA8 -- write the primitive's projection interval along arDir.
    typedef void (*GetIntervalFn)(const GPInstance* lpThis,
                                  const Vec4& arDir, Interval& arInterval);
    // console +0xAC -- batched form: one interval per direction.
    typedef void (*GetIntervalsFn)(const GPInstance* lpThis, const Vec4* lapDirs,
                                   u32 auNumDirs, Interval* lapIntervals);
    // console +0xB0 -- write the primitive's bounding box (not called by the
    // bodies of this wave).
    typedef void (*GetBBoxFn)(const GPInstance* lpThis, AABBox& arBBox);

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
            Vec4 p1;   // +0x00  contact point on the first primitive
            Vec4 p2;   // +0x10  contact point on the second primitive
        };

        u32       volumeTag1;      // +0x00  first->mVolumeTag
        u32       volumeTag2;      // +0x04  second->mVolumeTag
        u32       userTag1;        // +0x08  first->mUserTag
        u32       userTag2;        // +0x0C  second->mUserTag
        u32       numPoints;       // +0x10  contact-pair count
        u32       mPad14[3];       // +0x14  16-byte alignment of `normal`
        Vec4      normal;          // +0x20  NEGATED contact normal
        PointPair pointPairs[16];  // +0x30  fattened point pairs
    };

    // --- members (DWARF volume.h:551-563; console offsets in the comments) --
    Vec4          mPos;               // +0x00  primitive centre / triangle vert 0
    Vec4          mFaceNormals[3];    // +0x10  unit face axes
    Vec4          mEdgeDirections[3]; // +0x40  edge directions (cylinder axis in [0])
    Vec4          mDimensions;        // +0x70  half-extents / (half-length, radius)
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

static_assert(offsetof(GPInstance, mFatness)           == 0x80, "GPInstance::mFatness");
static_assert(offsetof(GPInstance, mVolumeTag)         == 0x84, "GPInstance::mVolumeTag");
static_assert(offsetof(GPInstance, mUserTag)           == 0x88, "GPInstance::mUserTag");
static_assert(offsetof(GPInstance, mNumFaceNormals)    == 0x8C, "GPInstance::mNumFaceNormals");
static_assert(offsetof(GPInstance, mNumEdgeDirections) == 0x8D, "GPInstance::mNumEdgeDirections");
static_assert(offsetof(GPInstance, mVolumeType)        == 0x90, "GPInstance::mVolumeType");
static_assert(offsetof(GPInstance, mFlags)             == 0x94, "GPInstance::mFlags");
static_assert(offsetof(GPInstance, mEdgeData)          == 0x98, "GPInstance::mEdgeData");

// ---------------------------------------------------------------------------
// GP primitive images (canonical rwccore.h:1182-1266: no extra data members
// on any of them). The per-type callbacks below are the bodies behind each
// type's VolumeMethods slots. X360 SHAPE DELTA (per the header note above):
// the canonical PS3 build declares them as non-static const member functions
// reached through pointers-to-member; the X360 build caches PLAIN function
// pointers, so they are reconstructed as static members matching the
// committed GetMaximumFeatureFn / GetIntervalFn / GetIntervalsFn typedefs
// exactly (the un-recovered FixupSepDirMethods-style registration TU owns the
// table fill). GetBBox siblings exist in the binary at other addresses
// outside these TUs and are NOT declared here.
// ---------------------------------------------------------------------------

// The GP sphere image (canonical rwccore.h:1182: the core primitive is a
// point -- the radius rides in mFatness and is applied by the callers).
struct GPSphere : public GPInstance
{
    // @ 0x82BA80A8 -- VolumeMethods +0xA8 (GetIntervalFn).
    static void GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval);
    // @ 0x82BA80C0 -- VolumeMethods +0xAC (GetIntervalsFn).
    static void GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals);
};

// The GP capsule image (canonical rwccore.h:1193: the core is the axis
// segment -- mEdgeDirections[0] is the axis, mDimensions.x the half-height,
// the radius rides in mFatness).
struct GPCapsule : public GPInstance
{
    // @ 0x82BAFA80 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    static void GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                  const Vec4& arDir, Feature& arFeature);
    // @ 0x82BAFB68 -- VolumeMethods +0xA8 (GetIntervalFn).
    static void GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval);
    // @ 0x82BAFC10 -- VolumeMethods +0xAC (GetIntervalsFn).
    static void GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals);
};

// The GP box image (canonical rwccore.h:1207: mFaceNormals are the unit face
// axes, mDimensions the half-extents).
struct GPBox : public GPInstance
{
    // @ 0x82BA8918 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    static void GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                  const Vec4& arDir, Feature& arFeature);
    // @ 0x82BA8650 -- VolumeMethods +0xA8 (GetIntervalFn).
    static void GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval);
    // @ 0x82BA9238 -- VolumeMethods +0xAC (GetIntervalsFn).
    static void GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals);
};

// The GP cylinder image (canonical rwccore.h:1253: no extra data members --
// mEdgeDirections[0] is the axis, mDimensions.x/.y the half-length/radius;
// per the canonical GPCylinder::Initialize rwccore.h:1390 mFaceNormals[0] is
// the axis with [1]/[2] the two perpendicular frame axes).
struct GPCylinder : public GPInstance
{
    // @ 0x82BAE430 -- VolumeMethods +0xA4 (GetMaximumFeatureFn).
    static void GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                  const Vec4& arDir, Feature& arFeature);
    // @ 0x82BAD7F8 -- VolumeMethods +0xA8 (GetIntervalFn).
    static void GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval);
    // @ 0x82BAD938 -- VolumeMethods +0xAC (GetIntervalsFn).
    static void GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals);
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
    static void GetMaximumFeature(const GPInstance* lpThis, RwBool abCcw,
                                  const Vec4& arDir, Feature& arFeature);
    // @ 0x82BBA6A0 -- VolumeMethods +0xA8 (GetIntervalFn).
    static void GetInterval(const GPInstance* lpThis,
                            const Vec4& arDir, Interval& arInterval);
    // @ 0x82BBA6E0 -- VolumeMethods +0xAC (GetIntervalsFn).
    static void GetIntervals(const GPInstance* lpThis, const Vec4* lapDirs,
                             u32 auNumDirs, Interval* lapIntervals);
};

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

    u32     v1;                          // +0x000  first Volume (console pointer image)
    u32     tag1;                        // +0x004  first user tag
    u32     v2;                          // +0x008  second Volume (console pointer image)
    u32     tag2;                        // +0x00C  second user tag
    s32     vNindex;                     // +0x010  source pair index; -1 = fatness-culled
    u8      mPad014[0xC];                // +0x014  16-byte alignment of f1
    Feature f1;                          // +0x020  support feature on the first side
    Feature f2;                          // +0x260  support feature on the second side
    Vec4    sepDir;                      // +0x4A0  coarse separating direction (pass 1)
    f32     sepDist;                     // +0x4B0  (untouched by the batch kernels)
    u8      mPad4B4[0xC];                // +0x4B4  16-byte alignment of normal
    Vec4    normal;                      // +0x4C0  final contact normal
    Vec4    pointOn1;                    // +0x4D0  reference contact point, first side
    Vec4    pointOn2;                    // +0x4E0  reference contact point, second side
    f32     distance;                    // +0x4F0  separation along the direction
    u8      mPad4F4[0xC];                // +0x4F4
    Vec4    pointsOn1[MAXPOINTCOUNT];    // +0x500  contact points, first side
    Vec4    pointsOn2[MAXPOINTCOUNT];    // +0x600  contact points, second side
    f32     distances[MAXPOINTCOUNT];    // +0x700  per-point separation along normal
    u32     numPoints;                   // +0x740  contact-point count (0 = rejected)
    u8      mPad744[0xC];                // +0x744  pad to the 0x750 stride
};

static_assert(offsetof(PrimitivePairIntersectResult, f1)        == 0x020, "PPIR::f1");
static_assert(offsetof(PrimitivePairIntersectResult, f2)        == 0x260, "PPIR::f2");
static_assert(offsetof(PrimitivePairIntersectResult, sepDir)    == 0x4A0, "PPIR::sepDir");
static_assert(offsetof(PrimitivePairIntersectResult, sepDist)   == 0x4B0, "PPIR::sepDist");
static_assert(offsetof(PrimitivePairIntersectResult, normal)    == 0x4C0, "PPIR::normal");
static_assert(offsetof(PrimitivePairIntersectResult, pointOn1)  == 0x4D0, "PPIR::pointOn1");
static_assert(offsetof(PrimitivePairIntersectResult, pointOn2)  == 0x4E0, "PPIR::pointOn2");
static_assert(offsetof(PrimitivePairIntersectResult, distance)  == 0x4F0, "PPIR::distance");
static_assert(offsetof(PrimitivePairIntersectResult, pointsOn1) == 0x500, "PPIR::pointsOn1");
static_assert(offsetof(PrimitivePairIntersectResult, pointsOn2) == 0x600, "PPIR::pointsOn2");
static_assert(offsetof(PrimitivePairIntersectResult, distances) == 0x700, "PPIR::distances");
static_assert(offsetof(PrimitivePairIntersectResult, numPoints) == 0x740, "PPIR::numPoints");
static_assert(sizeof(PrimitivePairIntersectResult)              == 0x750, "PPIR stride");

// ---------------------------------------------------------------------------
// rwc_FeatureIntersectionPrism -- the Find*Prism family's result block
// (canonical rwccore.h:1801). Offsets attested across the family's asm:
// normal @+0x200 (FindFaceEdgePrism's override store), m_numpts @+0x210
// (every worker), normalOverride @+0x214 (caller-zeroed; set by
// FindFaceEdgePrism's parallel-overlap path).
// ---------------------------------------------------------------------------
struct rwc_FeatureIntersectionPrism
{
    Vec4   m_ptsOn1[2 * Feature::MAXEDGECOUNT];   // +0x000  points on feature 1
    Vec4   m_ptsOn2[2 * Feature::MAXEDGECOUNT];   // +0x100  points on feature 2
    Vec4   normal;                                // +0x200  override contact normal
    s32    m_numpts;                              // +0x210  point-pair count
    RwBool normalOverride;                        // +0x214  caller zeroes; worker sets
};

static_assert(offsetof(rwc_FeatureIntersectionPrism, normal)         == 0x200, "prism::normal");
static_assert(offsetof(rwc_FeatureIntersectionPrism, m_numpts)       == 0x210, "prism::m_numpts");
static_assert(offsetof(rwc_FeatureIntersectionPrism, normalOverride) == 0x214, "prism::normalOverride");

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
// (VecFloat broadcast on console; the scalar lane here). Negative return =
// penetration depth.
// ---------------------------------------------------------------------------

// X360 off_82F91800 -- the 6x6 [type1][type2] dispatch table of the family
// (row stride 6 attested by `mulli r11, rN, 6` in both batch kernels and
// ComputeContactPoints). The table itself lives in the un-recovered GP
// registration TU (canonical FixupSepDirMethods, rwccore.h:2955) -- extern
// declaration only, contents NOT reconstructed here.
typedef f32 (*FindBestSeparatingDirectionFn)(Vec4& arBestSepDir,
                                             const GPInstance& arGP1,
                                             const GPInstance& arGP2);
extern const FindBestSeparatingDirectionFn
    gapFindBestSeparatingDirection[GPInstance::NUMINTERNALTYPES][GPInstance::NUMINTERNALTYPES];

// @ 0x82BB4038 -- fill lapCandidates with the SAT candidate directions for the
// pair (face normals, normalised edge crosses, capsule fallbacks, centre
// delta); returns the end pointer of the filled array, or NULL if none
// survived the degeneracy filter. (Same TU as FindBestSeparatingDirection;
// the caller provides 15 usable 16-byte slots.)
Vec4* FindBestSepDir_FindCandidates(Vec4* lapCandidates,
                                    const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB4660 -- generic SAT driver: collect candidates, batch-project both
// primitives (mMethods.mGetIntervals), keep the axis with the greatest
// interval separation.
f32 FindBestSeparatingDirection(Vec4& arBestSepDir,
                                const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB47F0 -- box/box SAT over the 6 face normals + 9 edge crosses.
f32 FindBestSeparatingDirectionBoxBox(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// PENDING (not delivered this wave; body in the same ledger TU family). The
// tri/box worker FindBestSeparatingDirectionBoxTri @ 0x82BAA1A0 tail-calls.
f32 FindBestSeparatingDirectionTriBox(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BAA1A0 -- canonical inline rwccore.h:3151 emitted out of line: run the
// tri/box worker swapped, then flip the direction.
f32 FindBestSeparatingDirectionBoxTri(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BAA1F0 -- canonical inline rwccore.h:3158 emitted out of line: the
// normalised centre1->centre2 axis; returns the centre distance (0 when
// coincident).
f32 FindBestSeparatingDirectionSphSph(Vec4& arBestSepDir,
                                      const GPInstance& arGP1, const GPInstance& arGP2);

// @ 0x82BB76E8 -- cylinder SAT driver (canonical rwccore.h:3831): build the
// rim-aware candidate table, then evaluate it.
f32 FindBestSepDirWithCylinder(Vec4& arBestSepDir,
                               const GPCylinder& arGPCylinder, const GPInstance& arGPOther);

// @ 0x82BAA250 -- canonical inline rwccore.h:3834 emitted out of line: pure
// tail-call thunk onto FindBestSepDirWithCylinder (cylinder-first operand
// order is the worker's native order).
f32 FindBestSeparatingDirCylVol(Vec4& arBestSepDir,
                                const GPInstance& arGPCylinder, const GPInstance& arGPOther);

// ---------------------------------------------------------------------------
// Cylinder-rim SAT candidate helpers (X360-era additions; no Feb-2007
// canonical declarations -- names are the ledger's).
// ---------------------------------------------------------------------------

// @ 0x82BB53F8 -- candidate separating direction between a cylinder rim
// circle (centre, unit axis, lane-broadcast radius) and the edge segment
// [aEdgeP1, aEdgeP2]: a 10-step damped regula-falsi on the rim-to-edge gap.
// The X360 passes all five vectors by value in VMX v1..v5 and returns the
// direction in v1.
Vec4 RimToEdge(Vec4 aRimCentre, Vec4 aRimAxis, Vec4 aRimRadius,
               Vec4 aEdgeP1, Vec4 aEdgeP2);

// @ 0x82BB56C8 -- test one rim-circle pair for a separating-direction
// candidate (interlock rejection, coplanar closed form, or a Heron-seeded
// bracketed regula-falsi on the angular score); on success writes one
// 16-byte candidate to lpCandidate and returns nonzero. (All vector
// parameters by value: at the 0x82BB61C0 call site every argument register
// carries the address of a fresh caller-stack copy.)
RwBool RimToRim(Vec4 aRimCentre1, Vec4 aRimAxis1, Vec4 aRimRadius1,
                Vec4 aRimCentre2, Vec4 aRimAxis2, Vec4 aRimRadius2,
                Vec4 aSepDir, Vec4* lpCandidate);

// @ 0x82BB6118 -- build the two end points of an edge given as
// point + dir*(offset +/- halfLength), run RimToEdge on them, and append the
// returned candidate to lapCandidateDirs (16-byte stride), bumping the count.
void AddRimToEdgeCandidate(const Vec4& arRimCentre, const Vec4& arRimAxis,
                           const Vec4& arRimRadius, const Vec4& arEdgePoint,
                           const Vec4& arEdgeDirection,
                           f32 afEdgeOffset, f32 afEdgeHalfLength,
                           Vec4* lapCandidateDirs, u32* lpuNumCandidateDirs);

// @ 0x82BB61C0 -- enumerate rim-to-rim candidates between two cylinder-style
// GP instances (all four +/- rim-end pairings when the axes are not parallel).
void AddRimToRimCandidates(const GPInstance* lpGP1, const GPInstance* lpGP2,
                           Vec4* lapCandidateDirs, u32* lpuNumCandidateDirs);

// ---------------------------------------------------------------------------
// Feature-pair prism intersection family (canonical rwccore.h:1810-1836).
// Register contract (X360 __fastcall): r3=&res, r4=ptsOn1, r5=ptsOn2, r6=&f1,
// r7=&f2, r8=&sepDir; the dispatcher aims ptsOn1/ptsOn2 at res.m_ptsOn1/2 in
// the pairing that keeps each array with its own source feature. The features
// are non-const (the workers fold region codes into Feature::region).
// ---------------------------------------------------------------------------

// @ 0x82BB77C0 -- edge vs point.
RwBool FindEdgePointPrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                          Feature& arEdgeFeature, Feature& arPointFeature, const Vec4& arSepDir);

// PENDING (not delivered this wave; called by the dispatcher) -- edge vs edge.
RwBool FindEdgeEdgePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                         Feature& arEdgeFeature1, Feature& arEdgeFeature2, const Vec4& arSepDir);

// @ 0x82BB7F18 -- face vs point.
RwBool FindFacePointPrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOnF, Vec4* lapPtsOnP,
                          Feature& arFaceFeature, Feature& arPointFeature, const Vec4& arSepDir);

// @ 0x82BB83A8 -- face vs edge.
RwBool FindFaceEdgePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOnF, Vec4* lapPtsOnE,
                         Feature& arFaceFeature, Feature& arEdgeFeature, const Vec4& arSepDir);

// @ 0x82BB8798 -- generic face vs face (simultaneous edge-ring walk).
RwBool FindFaceFacePrism(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                         Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir);

// @ 0x82BB8CA0 -- 4-edge face vs 3-edge face specialisation.
RwBool FindFaceFacePrism4x3(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir);

// @ 0x82BB9400 -- 4-edge face vs 4-edge face specialisation.
RwBool FindFaceFacePrism4x4(rwc_FeatureIntersectionPrism& arRes, Vec4* lapPtsOn1, Vec4* lapPtsOn2,
                            Feature& arFaceFeature1, Feature& arFaceFeature2, const Vec4& arSepDir);

// @ 0x82BB9BD8 -- dispatch on the two features' edge counts (0 = point,
// 1 = edge, >1 = face) to the specialised worker, resolving the point/point
// case inline. Returns nonzero when an intersection prism was found.
RwBool FindFeatureIntersectionPrism(rwc_FeatureIntersectionPrism& arRes,
                                    Feature& arFeature1, Feature& arFeature2,
                                    const Vec4& arSepDir);

// @ 0x82BB7828 -- clip arInterval1 against arInterval2 into *lpResult and
// report whether the clipped interval is non-empty (strict, all four lanes).
// Called by FindEdgeEdgePrism.
u32 FindIntervalOverlap(Interval* lpResult,
                        const Interval* lpInterval1, const Interval* lpInterval2);

// ---------------------------------------------------------------------------
// Narrow-phase entry points.
// ---------------------------------------------------------------------------

// PENDING X360 sub_82BAA600 (not delivered this wave; body sits between
// FindBestSeparatingDirCylVol and GPInstanceBatchIntersectNx1). Validates a
// proposed contact normal against a one-sided GPTriangle instance (this=r3,
// normal=v1); zero return discards the contact. FLAGGED: the name is
// descriptive, not SDK-attested (the binary symbol is unnamed).
u32 GPTriangleAcceptContactNormal(const GPInstance* lpTriangle, const Vec4& arNormal);

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
// float padding, const Vector4* sepDir)). The transforms ride as const void*
// per the committed CreateGPInstanceFn slot type (the Matrix44Affine layout
// has not landed in this home). apSepDir, when non-NULL, carries the caller's
// separating direction in xyz and the separation distance in w (lfs f31,
// 0xC(r30)). Called by BrnPhysics::Vehicle::VehicleManager::
// PredictCarCarIntersection and the CgsSceneManager OverlapCullingModule.
RwBool PrimitivePairIntersect(PrimitivePairIntersectResult& arResult,
                              const Volume* apVolume1, const void* apMtx1,
                              const Volume* apVolume2, const void* apMtx2,
                              f32 afPadding, const Vec4* apSepDir);

} // namespace collision
} // namespace rw
