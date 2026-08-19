#include "vendor/renderware/collision/CollisionVolume.hpp"

#include "vendor/renderware/collision/AABBox.hpp"            // AABBox (the getBBox out-param)
#include "vendor/renderware/collision/Feature.hpp"           // Feature (getMaximumFeature)
#include "vendor/renderware/collision/GPInstance.hpp"        // GPInstance / Interval
#include "vendor/renderware/collision/LineSegIntersect.hpp"  // VolumeLineSegIntersectResult
#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/CylinderVolume.hpp"
#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "vendor/renderware/collision/AggregateVolume.hpp"

// ===========================================================================================
// THE SIX rw::collision::Volume DESCRIPTOR RECORDS, WITH THEIR METHOD SLOTS BOUND.
//
//   X360 .rdata  unk_82F91740 (SPHERE)  unk_82F918C0 (CAPSULE)  unk_82F919A4 (TRIANGLE)
//                unk_82F9176C (BOX)     unk_82F91894 (CYLINDER) unk_82F919D0 (AGGREGATE)
//
// Installed into rw::collision::gVolumeVTable[1..6] by Volume::InitializeVTable @0x82BB03A8
// (SDKs/EATech/rwcollision/volume.cpp). Every consumer that dispatches on a volume --
// SceneManagerModule's scene-update drain and VolumeManager::AddDynamicVolume through
// SDKs/EATech/rwcollision/volume_debug_access.h's Volume::GetBBox, VolumeQuery.cpp and
// VolumeBBoxQuery.cpp through their mpGetBBox slot, PrimitiveIntersect.cpp through
// mpCreateGPInstance -- reaches a body through this file.
//
// -------------------------------------------------------------------------------------------
// WHY THIS TU EXISTS AT ALL
// -------------------------------------------------------------------------------------------
// The descriptors used to live in SDKs/EATech/rwcollision/volume.cpp with all seven method
// slots NULL, because that TU cannot NAME any primitive class: including
// volume_debug_access.h and any of the four primitive headers in one TU is a hard C2011 on
// `rw::math::vpu::Vector3` (two vocabularies with the same qualified name -- MEASURED by
// scratchpad/waveQ5/probe_rwc3/probe_descriptor_blocked.cpp, and that is still true today).
//
// The measurement that unblocks it: the fork is ONE-SIDED. The VENDOR half compiles all six
// volume classes together with no conflict at all -- MEASURED,
// scratchpad/waveQ5/probe_vtbind/measure_coinclude.cpp, STATUS=pass. So the descriptors are
// defined HERE, where every primitive's methods are visible, while the SDK half keeps its
// `extern const Volume::VTable gVolumeHandler_*` declarations and the gVolumeVTable fill.
//
// The two halves bind to ONE symbol per descriptor -- MEASURED with dumpbin /symbols on two
// objects compiled with the canonical ship flags (scratchpad/waveQ5/vtbind/mangle_sdk.cpp and
// mangle_vendor.cpp):
//     SDK object     UNDEF  ?gVolumeHandler_82F91740@collision@rw@@3UVTable@Volume@12@B
//     vendor object  SECT3  ?gVolumeHandler_82F91740@collision@rw@@3UVTable@Volume@12@B
// and the two spellings of the record are LAYOUT-IDENTICAL -- MEASURED,
// scratchpad/waveQ5/probe_vtbind/measure_layout.cpp: MSVC x64 gives a pointer-to-member of a
// single-inheritance, non-polymorphic class size 8, so the SDK half's pointer-to-member slots
// and this half's free-function slots agree offset for offset. The run-time proof that a
// record written here reads back correctly through BOTH halves is
// scratchpad/waveQ5/probe_vtbind/ (probe_vendor_side.cpp + probe_sdk_side.cpp, linked and RUN).
//
// -------------------------------------------------------------------------------------------
// THE ADAPTERS: WHY EVERY SLOT GOES THROUGH A THIN static FUNCTION
// -------------------------------------------------------------------------------------------
// Not one primitive's member can be stored in a slot directly, for three separate reasons,
// and the adapters make each one explicit instead of hiding it behind a function-pointer
// cast (there is NO cast between function pointer types anywhere in this file -- every
// adapter has the slot's exact signature, so the compiler type-checks all 42 slots):
//
//   1. FOUR OF THE SIX CLASSES ARE STANDALONE. CapsuleVolume.hpp:56, CylinderVolume.hpp:49,
//      TriangleVolume.hpp:48 and AggregateVolume.hpp have no `: public Volume` base clause,
//      so `&CapsuleVolume::GetBBox` cannot convert to anything Volume-shaped. Their adapters
//      reinterpret_cast the `const Volume*`. That is sound because all five classes model the
//      SAME 96-byte serialised record at the SAME offsets, which is pinned by static_asserts
//      in each header (CapsuleVolume.hpp / CylinderVolume.hpp / TriangleVolume.hpp /
//      AggregateVolume.hpp) and re-pinned per-class at the foot of this file. BoxVolume and
//      SphereVolume DO derive from Volume, so their adapters use static_cast.
//
//   2. THE PARAMETER SPELLINGS DIVERGE. Three primitives take the transform as `const Vec4*`
//      and AggregateVolume takes `const math::vpu::Matrix44Affine*`. Those are the same
//      pointer: a Matrix44Affine is 64 bytes = four 16-byte rows and a Vec4 is one 16-byte
//      row -- MEASURED, scratchpad/waveQ5/probe_vtbind/measure_vocab*.cpp
//      (sizeof(Matrix44Affine)==64, sizeof(Vec4)==16, sizeof(math::vpu::Vector3)==16). The
//      aggregate adapter does that one reinterpret and says so.
//
//   3. THE getMaximumFeature / getInterval SLOTS DO NOT HAVE A UNIFORM CONSOLE ABI, and that
//      is a CONSOLE FACT, not a host artefact. DWARF box.h:39/:42 and triangle.h:45/:48
//      declare BOX's and TRIANGLE's as taking `Vector3` BY VALUE; sphere.h:24/:27,
//      capsule.h:33/:36, cylinder.h:39/:42 and aggregatevolume.h:30 take `const Vector3&`.
//      A by-value VMX vector rides a vector register and consumes no GPR, so the `Feature&`
//      out-parameter arrives in a DIFFERENT register per type -- visible in the raw asm:
//      SphereVolume::GetMaximumFeature @0x82BA81B0 stores through r6 while the otherwise
//      instruction-for-instruction identical BoxVolume::GetMaximumFeature @0x82BA87F0 stores
//      through r5 (likewise TriangleVolume @0x82BBACD0 `mr r31,r5` vs CylinderVolume
//      @0x82BAC988 `mr r31,r6`). Calling BOX's slot through SPHERE's prototype on the console
//      would have written the Feature over the caller's direction vector. It never happened
//      because the canonical `class Volume` (rwccore.h:1507-1567) exposes GetBBox,
//      GetBBoxDiag, CreateGPInstance, LineSegIntersect and Release and NOTHING that reaches
//      getInterval or getMaximumFeature -- which is also why AGGREGATE's getInterval slot is
//      genuinely 0 in the image. On x64 both spellings pass a pointer, so one slot signature
//      is faithful for all six; the adapters keep each primitive's own DWARF spelling and the
//      divergence is recorded rather than smoothed away.
//
// -------------------------------------------------------------------------------------------
// SLOT COVERAGE -- what is bound, what is a genuine NULL, what is PARKED
// -------------------------------------------------------------------------------------------
//  slot                [1] SPHERE  [2] CAPSULE  [3] TRIANGLE  [4] BOX     [5] CYLINDER  [6] AGG
//  getBBox             82BA8020 ✅ 82BAF9C8 ✅  82BBA038 ✅   82BA9FC8 ✅ 82BAD490 ✅   82BBBA10 ✅
//  getBBoxDiag         82BA8580 ✅ 82BAF6A8 ✅  82BBA110 ⛔P   82BA8890 ✅ 82BAC7B8 ✅   82BBBB58 ✅
//  getInterval         82BAC980 ✅ 82BAC980 ✅  82BAC980 ✅   82BAC980 ✅ 82BAC980 ✅   0 (image)
//  getMaximumFeature   82BA81B0 ✅ 82BAF808 ✅  82BBACD0 ✅   82BA87F0 ✅ 82BAC988 ✅   82B0F1B8 ✅
//  createGPInstance    82BA8100 ✅ 82BAF6F0 ✅  82BBAA00 ✅   82BA92E8 ✅ 82BAC7F8 ⛔P   82B0F1B8 ✅
//  lineSegIntersect    82BA82C8 ⛔P 82BAFCF8 ⛔P 82BBB970 ✅   82BA9478 ⛔P 82BAF688 ⛔P  0 (image)
//  release             82AD5078 ✅ 82AD5078 ✅  82AD5078 ✅   82AD5078 ✅ 82AD5078 ✅   82AD5078 ✅
//
//  ✅ = bound to a real body (34 of the 40 slots the image binds).
//  0 (image) = the shipped .rdata word really is zero -- reproduced, not a hole.
//  ⛔P = PARKED, six slots, each with its reason at the record below and none of them
//        stubbed: a NULL slot is an honest hole, a stub that returns a plausible value is a
//        wrong answer. None of the six is on the wave-Q5 car-vs-prop contact path (that path
//        is getBBox -> the sweeper, then createGPInstance -> PrimitiveIntersect).
//
// The three shared/ICF-folded targets are reproduced as three shared adapters, exactly the
// arrangement the console has (one body, many slots), NOT invented:
//   0x82AD5078  `blr`             the empty function (411 xrefs; the exporter's symbol
//                                 table gives this address a misleading name -- gotcha 6)
//   0x82BAC980  `li r3,1 ; blr`   a folded "return 1" (54 xrefs) -- the getInterval slot for
//                                 types 1..5. IDA mislabels it
//                                 `MassiveAdClient3::...MediaDownload`; do not believe it.
//   0x82B0F1B8  `li r3,0 ; blr`   a folded "return 0" (33 xrefs) -- AGGREGATE's
//                                 getMaximumFeature AND createGPInstance, the same body twice.
//
// The typeID / name / flags words of all six records are the ones dumped out of the shipped
// image (scratchpad/waveQ2/evidence/volume_vtables_ida_dump.txt, re-measured 2026-08-18 by
// waveQ5 rwc3); this change binds the method slots and touches nothing else about them.
// ===========================================================================================

namespace rw
{
namespace collision
{

namespace
{
    // -----------------------------------------------------------------------------------
    // The three shared / ICF-folded console bodies.
    // -----------------------------------------------------------------------------------

    // X360 0x82BAC980 -- `li r3,1 ; blr`. The getInterval slot of types 1..5. It writes
    // NOTHING to arInterval: the console body is two instructions and never touches r5.
    RwBool FoldedGetIntervalReturnsTrue(const Volume* /*lpVolume*/, const Vec4& /*arDir*/,
                                        Interval& /*arInterval*/)
    {
        return 1;
    }

    // X360 0x82B0F1B8 -- `li r3,0 ; blr`. AGGREGATE binds this ONE body into TWO slots with
    // two different signatures, so it appears here twice; that is the console's arrangement,
    // not a duplication. An aggregate has no maximum feature and no GP instance of its own --
    // the traversal decomposes it into leaf primitives first (VolumeVolumeQuery::
    // GetPrimitiveIntersections -> VolumeBBoxQuery::AddVolumeRef -> AddPrimitiveRef).
    RwBool FoldedGetMaximumFeatureReturnsFalse(const Volume* /*lpVolume*/, RwBool /*abCcw*/,
                                               const Vec4& /*arDir*/, Feature& /*arFeature*/)
    {
        return 0;
    }

    RwBool FoldedCreateGPInstanceReturnsFalse(const Volume* /*lpVolume*/,
                                              GPInstance& /*arInstance*/,
                                              const Vec4* /*lpTransform*/)
    {
        return 0;
    }

    // X360 0x82AD5078 -- ONE instruction, `blr`. This is a full reconstruction of a real
    // console body, not an absence: the address is the XEX's ICF-folded empty function
    // (411 xrefs; AGENTS gotcha 6 names this exact address and warns that the exporter's
    // symbol table gives it a misleading name). Every one of the six release slots points
    // at it, which is the shipped build saying that rwcollision volumes are never
    // individually released: a volume lives and dies with the resource block its
    // Initialize was handed.
    void FoldedReleaseEmptyBody(Volume* /*lpVolume*/)
    {
    }

    // -----------------------------------------------------------------------------------
    // The five reinterpreting views. BoxVolume / SphereVolume need none (they really derive
    // from Volume); the other four are standalone classes over the same 96-byte record.
    // -----------------------------------------------------------------------------------
    const CapsuleVolume*   AsCapsule  (const Volume* lpV) { return reinterpret_cast<const CapsuleVolume*>(lpV);   }
    const CylinderVolume*  AsCylinder (const Volume* lpV) { return reinterpret_cast<const CylinderVolume*>(lpV);  }
    const TriangleVolume*  AsTriangle (const Volume* lpV) { return reinterpret_cast<const TriangleVolume*>(lpV);  }
    const AggregateVolume* AsAggregate(const Volume* lpV) { return reinterpret_cast<const AggregateVolume*>(lpV); }

    // A Matrix44Affine is four 16-byte rows and a Vec4 is one -- MEASURED (see the banner),
    // so the transform argument is the same pointer in both vocabularies. Only the aggregate
    // needs the conversion; the other five primitives already spell it `const Vec4*`.
    const math::vpu::Matrix44Affine* AsAffine(const Vec4* lpRows)
    {
        return reinterpret_cast<const math::vpu::Matrix44Affine*>(lpRows);
    }

    // The three primitives whose GetBBoxDiag returns the EATech `math::vpu::Vector3` rather
    // than this directory's `Vec4`. Both are one 16-byte, four-lane row (MEASURED), so this
    // is a lane-for-lane copy and not a reinterpretation.
    Vec4 AsVec4(const math::vpu::Vector3& arVector)
    {
        const Vec4 lvOut = { arVector.mV.mafLane[0], arVector.mV.mafLane[1],
                             arVector.mV.mafLane[2], arVector.mV.mafLane[3] };
        return lvOut;
    }

    // -----------------------------------------------------------------------------------
    // [1] SPHERE -- unk_82F91740. All four homed bodies live in BoxVolume.cpp.
    // -----------------------------------------------------------------------------------
    RwBool SphereGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abTight, AABBox& arBBox)
    {
        return static_cast<const SphereVolume*>(lpV)->GetBBox(lpTransform, abTight, arBBox);
    }

    Vec4 SphereGetBBoxDiag(const Volume* lpV)
    {
        return static_cast<const SphereVolume*>(lpV)->GetBBoxDiag();
    }

    RwBool SphereGetMaximumFeature(const Volume* lpV, RwBool abCcw, const Vec4& arDir,
                                   Feature& arFeature)
    {
        return static_cast<const SphereVolume*>(lpV)->GetMaximumFeature(abCcw, arDir, arFeature);
    }

    RwBool SphereCreateGPInstance(const Volume* lpV, GPInstance& arInstance, const Vec4* lpTransform)
    {
        return static_cast<const SphereVolume*>(lpV)->CreateGPInstance(arInstance, lpTransform);
    }

    // -----------------------------------------------------------------------------------
    // [2] CAPSULE -- unk_82F918C0. Bodies in CapsuleVolume.cpp (read-only for this cluster).
    // -----------------------------------------------------------------------------------
    RwBool CapsuleGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abTight, AABBox& arBBox)
    {
        return AsCapsule(lpV)->GetBBox(lpTransform, abTight, arBBox);
    }

    Vec4 CapsuleGetBBoxDiag(const Volume* lpV)
    {
        return AsVec4(AsCapsule(lpV)->GetBBoxDiag());
    }

    // CapsuleVolume::GetMaximumFeature is spelled `void` in its header while the DWARF
    // (capsule.h:36) and the asm (`li r3, 1` at 0x82BAF...) both say RwBool-returning-1.
    // The adapter supplies the 1 rather than leaving the caller to read whatever the void
    // body left in RAX; the header spelling is CapsuleVolume.cpp's to correct, not this
    // file's. Same for CYLINDER and TRIANGLE below.
    RwBool CapsuleGetMaximumFeature(const Volume* lpV, RwBool abCcw, const Vec4& arDir,
                                    Feature& arFeature)
    {
        AsCapsule(lpV)->GetMaximumFeature(abCcw, arDir, arFeature);
        return 1;
    }

    RwBool CapsuleCreateGPInstance(const Volume* lpV, GPInstance& arInstance, const Vec4* lpTransform)
    {
        return AsCapsule(lpV)->CreateGPInstance(arInstance, lpTransform);
    }

    // -----------------------------------------------------------------------------------
    // [3] TRIANGLE -- unk_82F919A4. Bodies in TriangleVolume.cpp / TriangleVolume_wN_01.cpp.
    // -----------------------------------------------------------------------------------
    RwBool TriangleGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abTight, AABBox& arBBox)
    {
        return AsTriangle(lpV)->GetBBox(lpTransform, abTight, arBBox);
    }

    // TriangleVolume::GetMaximumFeature omits the leading `abCcw` that DWARF triangle.h:48
    // declares -- the reconstruction read the asm's r4 as the direction, which is what the
    // BY-VALUE `Vector3` shape makes it look like (see reason 3 in the banner). Both
    // parameters are dead in the body, so the adapter drops abCcw and the header spelling is
    // TriangleVolume.hpp's owner to reconcile.
    RwBool TriangleGetMaximumFeature(const Volume* lpV, RwBool /*abCcw*/, const Vec4& arDir,
                                     Feature& arFeature)
    {
        AsTriangle(lpV)->GetMaximumFeature(arDir, arFeature);
        return 1;
    }

    RwBool TriangleCreateGPInstance(const Volume* lpV, GPInstance& arInstance, const Vec4* lpTransform)
    {
        return AsTriangle(lpV)->CreateGPInstance(arInstance, lpTransform);
    }

    // TriangleVolume::LineSegIntersect is spelled in the CONSOLE'S REGISTER ORDER
    // (`(tm, result, fatness, start, end)`) because on the X360 the two points ride VMX
    // v1/v2 by value and take no GPR -- DWARF triangle.h:51 confirms
    // `LineSegIntersect(Vector3, Vector3, const Matrix44Affine*, VolumeLineSegIntersectResult&,
    // float32_t)`, i.e. the points ARE the first two parameters. The slot signature follows
    // the DWARF; this adapter is the re-order, and it also takes the result by reference and
    // hands it on as the pointer that body wants.
    RwBool TriangleLineSegIntersect(const Volume* lpV, const Vec4& arPt1, const Vec4& arPt2,
                                    const Vec4* lpTransform,
                                    VolumeLineSegIntersectResult& arResult, f32 afFatness)
    {
        return AsTriangle(lpV)->LineSegIntersect(lpTransform, &arResult, afFatness, arPt1, arPt2);
    }

    // -----------------------------------------------------------------------------------
    // [4] BOX -- unk_82F9176C. All four homed bodies live in BoxVolume.cpp.
    // -----------------------------------------------------------------------------------
    RwBool BoxGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abTight, AABBox& arBBox)
    {
        return static_cast<const BoxVolume*>(lpV)->GetBBox(lpTransform, abTight, arBBox);
    }

    Vec4 BoxGetBBoxDiag(const Volume* lpV)
    {
        return static_cast<const BoxVolume*>(lpV)->GetBBoxDiag();
    }

    // BoxVolume::GetMaximumFeature takes its direction BY VALUE (DWARF box.h:42) -- see
    // reason 3 in the banner. Dead either way.
    RwBool BoxGetMaximumFeature(const Volume* lpV, RwBool abCcw, const Vec4& arDir,
                                Feature& arFeature)
    {
        return static_cast<const BoxVolume*>(lpV)->GetMaximumFeature(abCcw, arDir, arFeature);
    }

    RwBool BoxCreateGPInstance(const Volume* lpV, GPInstance& arInstance, const Vec4* lpTransform)
    {
        return static_cast<const BoxVolume*>(lpV)->CreateGPInstance(arInstance, lpTransform);
    }

    // -----------------------------------------------------------------------------------
    // [5] CYLINDER -- unk_82F91894. Bodies in CylinderVolume.cpp (read-only for this cluster).
    // -----------------------------------------------------------------------------------
    RwBool CylinderGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abTight, AABBox& arBBox)
    {
        return AsCylinder(lpV)->GetBBox(lpTransform, abTight, arBBox);
    }

    Vec4 CylinderGetBBoxDiag(const Volume* lpV)
    {
        return AsVec4(AsCylinder(lpV)->GetBBoxDiag());
    }

    RwBool CylinderGetMaximumFeature(const Volume* lpV, RwBool abCcw, const Vec4& arDir,
                                     Feature& arFeature)
    {
        AsCylinder(lpV)->GetMaximumFeature(abCcw, arDir, arFeature);
        return 1;
    }

    // -----------------------------------------------------------------------------------
    // [6] AGGREGATE -- unk_82F919D0. Bodies in AggregateVolume.cpp.
    // -----------------------------------------------------------------------------------
    RwBool AggregateGetBBox(const Volume* lpV, const Vec4* lpTransform, RwBool abInstanced,
                            AABBox& arBBox)
    {
        return AsAggregate(lpV)->GetBBox(AsAffine(lpTransform), abInstanced, arBBox);
    }

    Vec4 AggregateGetBBoxDiag(const Volume* lpV)
    {
        return AsVec4(AsAggregate(lpV)->GetBBoxDiag());
    }
}

// ===========================================================================================
// The six records.
// ===========================================================================================

const Volume::VTable gVolumeHandler_82F91740 =                  // type 1 SPHERE
{
    E_VOLUMETYPE_SPHERE,
    SphereGetBBox,                          // 82BA8020  SphereVolume::GetBBox
    SphereGetBBoxDiag,                      // 82BA8580  SphereVolume::GetBBoxDiag
    FoldedGetIntervalReturnsTrue,           // 82BAC980  ICF-folded `li r3,1 ; blr`
    SphereGetMaximumFeature,                // 82BA81B0  SphereVolume::GetMaximumFeature
    SphereCreateGPInstance,                 // 82BA8100  SphereVolume::CreateGPInstance
    // ⛔ PARKED -- SphereVolume::LineSegIntersect @0x82BA82C8, 136 insns. A ray/sphere kernel
    // around rwcSphereLineSegIntersect @0x82BA81D8 (bodied, LineSegIntersect.cpp:700) plus two
    // hand-written Newton-Raphson chains (vrefp + 2 rounds for the 1/t reciprocal, vrsqrtefp +
    // 2 rounds for the normal) and a vcmpgtfp./mfocrf CR-bit predicate. Not on the car-vs-prop
    // contact path -- the only consumer of this slot is rw::collision::VolumeLineQuery, itself
    // still a link stub (SDKs/EATech/AptRenderLinkStubs.cpp:601). Raw asm dumped to
    // scratchpad/waveQ5/vtbind/asm/0x82BA82C8.txt so the next owner does not re-export it.
    0,
    FoldedReleaseEmptyBody,                  // 82AD5078  ICF-folded empty `blr`
    "SphereVolume",
    0
};

const Volume::VTable gVolumeHandler_82F918C0 =                  // type 2 CAPSULE
{
    E_VOLUMETYPE_CAPSULE,
    CapsuleGetBBox,                         // 82BAF9C8  CapsuleVolume::GetBBox
    CapsuleGetBBoxDiag,                     // 82BAF6A8  CapsuleVolume::GetBBoxDiag
    FoldedGetIntervalReturnsTrue,           // 82BAC980
    CapsuleGetMaximumFeature,               // 82BAF808  CapsuleVolume::GetMaximumFeature
    CapsuleCreateGPInstance,                // 82BAF6F0  CapsuleVolume::CreateGPInstance
    // ⛔ PARKED -- CapsuleVolume::LineSegIntersect @0x82BAFCF8, 428 insns. Already parked with
    // a full reason by its own TU's owner (CapsuleVolume.hpp:29-45, "BLOCKED"): a
    // register-allocated VMX line/segment kernel (__savevmx_120, CR-bit predicate extraction)
    // calling rwcSphereLineSegIntersect + rwcCylinderLineSegIntersect. No declaration exists
    // to bind, and the header is read-only for this cluster.
    0,
    FoldedReleaseEmptyBody,                  // 82AD5078
    "CapsuleVolume",
    0
};

const Volume::VTable gVolumeHandler_82F919A4 =                  // type 3 TRIANGLE
{
    E_VOLUMETYPE_TRIANGLE,
    TriangleGetBBox,                        // 82BBA038  TriangleVolume::GetBBox
    // ⛔ PARKED -- TriangleVolume::GetBBoxDiag @0x82BBA110. The address is in the descriptor
    // dump but there is NO progress/identity.json row for it and NO per-address JSON: it is an
    // EXPORT HOLE, and AGENTS gotcha 6 forbids bodying a function from an .i64 label alone.
    // Closing it needs a targeted headless idat run on a private copy; the shape is almost
    // certainly the sibling one (twice the local half-extent fold), but "almost certainly" is
    // not a reconstruction.
    0,
    FoldedGetIntervalReturnsTrue,           // 82BAC980
    TriangleGetMaximumFeature,              // 82BBACD0  TriangleVolume::GetMaximumFeature
    TriangleCreateGPInstance,               // 82BBAA00  TriangleVolume::CreateGPInstance
    TriangleLineSegIntersect,               // 82BBB970  TriangleVolume::LineSegIntersect
    FoldedReleaseEmptyBody,                  // 82AD5078
    "TriangleVolume",
    0
};

const Volume::VTable gVolumeHandler_82F9176C =                  // type 4 BOX
{
    E_VOLUMETYPE_BBOX,
    BoxGetBBox,                             // 82BA9FC8  BoxVolume::GetBBox
    BoxGetBBoxDiag,                         // 82BA8890  BoxVolume::GetBBoxDiag
    FoldedGetIntervalReturnsTrue,           // 82BAC980
    BoxGetMaximumFeature,                   // 82BA87F0  BoxVolume::GetMaximumFeature
    BoxCreateGPInstance,                    // 82BA92E8  BoxVolume::CreateGPInstance
    // ⛔ PARKED -- BoxVolume::LineSegIntersect @0x82BA9478, 723 insns, the largest body left in
    // this directory after the cylinder SAT helper: six slab/cap arms over
    // rwcSphereLineSegIntersect, rwcPlaneLineSegIntersect @0x82BA8818 (which has NO body in the
    // tree either -- a second hole behind this one) and rwcCylinderLineSegIntersect, with
    // __savevmx_122 hand register allocation. Same "no consumer on this path" argument as the
    // sphere's. Raw asm: scratchpad/waveQ5/vtbind/asm/0x82BA9478.txt.
    0,
    FoldedReleaseEmptyBody,                  // 82AD5078
    "BoxVolume",
    0
};

const Volume::VTable gVolumeHandler_82F91894 =                  // type 5 CYLINDER
{
    E_VOLUMETYPE_CYLINDER,
    CylinderGetBBox,                        // 82BAD490  CylinderVolume::GetBBox
    CylinderGetBBoxDiag,                    // 82BAC7B8  CylinderVolume::GetBBoxDiag
    FoldedGetIntervalReturnsTrue,           // 82BAC980
    CylinderGetMaximumFeature,              // 82BAC988  CylinderVolume::GetMaximumFeature
    // ⛔ PARKED -- CylinderVolume::CreateGPInstance @0x82BAC7F8, 98 insns. NO body and NO
    // declaration exist: CylinderVolume.hpp declares Initialize / GetBBoxDiag / GetBBox /
    // GetMaximumFeature and stops (its member comment at :95 already anticipates a
    // CreateGPInstance reading all four frame rows). The body's home is CylinderVolume.cpp,
    // which is READ-ONLY for this cluster, and homing it anywhere else would fork a class that
    // has a home (AGENTS gotcha 7). Raw asm dumped for the next owner:
    // scratchpad/waveQ5/vtbind/asm/0x82BAC7F8.txt.
    0,
    // ⛔ PARKED -- CylinderVolume::LineSegIntersect @0x82BAF688. No body, no declaration, and
    // NO per-address export JSON either (checked: .ida-exports has no 0x82BAF688.json), so it
    // needs a targeted headless idat run before anyone can even size it.
    0,
    FoldedReleaseEmptyBody,                  // 82AD5078
    "CylinderVolume",
    0
};

const Volume::VTable gVolumeHandler_82F919D0 =                  // type 6 AGGREGATE
{
    E_VOLUMETYPE_AGGREGATE,
    AggregateGetBBox,                       // 82BBBA10  AggregateVolume::GetBBox
    AggregateGetBBoxDiag,                   // 82BBBB58  AggregateVolume::GetBBoxDiag
    0,                                      // getInterval -- GENUINELY 0 in the image
    FoldedGetMaximumFeatureReturnsFalse,    // 82B0F1B8  ICF-folded `li r3,0 ; blr`
    FoldedCreateGPInstanceReturnsFalse,     // 82B0F1B8  the SAME folded body, second slot
    0,                                      // lineSegIntersect -- GENUINELY 0 in the image
    FoldedReleaseEmptyBody,                  // 82AD5078
    "AggregateVolume",
    0
};

// ===========================================================================================
// PER-CLASS LAYOUT PINS -- what makes the four reinterpret_casts above sound.
//
// Every one of these classes models the same 96-byte serialised record; each header already
// pins its own members, and these six lines pin the ONE fact the adapters depend on: that the
// record is 96 bytes wide in every spelling, so a `const Volume*` handed to any adapter
// addresses the same bytes the target class expects. (CapsuleVolume / CylinderVolume /
// TriangleVolume also pin their +0x40 / +0x44 / +0x50 / +0x5C members in their own headers.)
// ===========================================================================================
static_assert(sizeof(Volume)          == 96, "rw::collision::Volume console stride");
static_assert(sizeof(BoxVolume)       == 96, "BoxVolume");
static_assert(sizeof(SphereVolume)    == 96, "SphereVolume");
static_assert(sizeof(CapsuleVolume)   == 96, "CapsuleVolume");
static_assert(sizeof(CylinderVolume)  == 96, "CylinderVolume");
static_assert(sizeof(TriangleVolume)  == 96, "TriangleVolume");
static_assert(sizeof(AggregateVolume) == 96, "AggregateVolume");

} // namespace collision
} // namespace rw
