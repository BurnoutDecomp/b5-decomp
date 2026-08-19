// =================================================================================================
// GameShared/Jobs/ContactGenerator/ContactGeneratorJob_wQ6_01.cpp
//
// ⭐⭐⭐ THE TWO GP KERNELS OF THE PROP NARROW PHASE. A partfile of ContactGeneratorJob.cpp
// (wave Q6, cluster `gpi`, 2026-08-19) holding the two members that
// ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 calls and that had NO
// body anywhere in the tree:
//
//   ContactGeneratorJob::BuildGPInstance    @0x829222A0  (258)  :1660 :1699
//   ContactGeneratorJob::CollideGPInstances @0x829253C8  (244)  :1279 :1286 :1287 :1288
//
// ⚠️ 258 vs 254: `(0x829226A4 - 0x829222A0)/4 + 1` counts 258 WORDS in the .text run, but the
// export's `assembly` array has 254 ROWS, because the five-entry jump table at
// 0x829222D0..0x829222E0 is one `.long` row. Both numbers are right about different things; the
// sibling header quotes 258. No instruction is missing from the decode.
//
// The console's own path for both is the SAME TU as the rest of the family -- every assert here
// passes the string
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGeneratorJob.cpp
// so this is a partfile of that TU, split only because ContactGeneratorJob.cpp is owned by a
// concurrent session in this wave. FOLLOW-UP: fold it back into ContactGeneratorJob.cpp when
// nobody is editing that file; there is no reason for two TUs beyond the ownership split.
//
// ─── WHERE THIS SITS IN THE BREAKABLE-PROPS CHAIN ────────────────────────────────────────────
//   PropManager::DoPart/DoPropInstanceWorldContactGeneration
//     -> PrimitivePairListBuilder::AddPrimitive(...)             (packs a primitive into the blob)
//     -> AddPrimitiveListWithTriangleListToStream                (posts one command)
//     -> RunCollidePrimitiveListWithTriangleListStream           (desc type 12)
//        -> ContactGeneratorJob::Execute case 12
//           -> ExecutePrimitiveListWithTriangleListStream        (wave Q6 / pstream)
//              -> ExecutePrimitiveListWithTriangleList @0x82925908   (wave Q6 / pvt)
//                 -> BuildGPInstance     <-- THIS FILE: one packed primitive -> one GPInstance
//                 -> CollideGPInstances  <-- THIS FILE: GPInstance pair -> PrimitiveTestResults
// Nothing below this line runs the SAT/feature machinery itself: `CollideGPInstances` is a thin
// adaptor onto the already-real vendor kernel rw::collision::ComputeContactPoints @0x82BABDA8
// (src/vendor/renderware/collision/PrimitiveIntersect.cpp, mounted bat:2111), whose own banner
// already names this function as its caller.
//
// =================================================================================================
// ─── 1. BuildGPInstance @0x829222A0 -- REGISTER MAP AND DISPATCH ─────────────────────────────
//
// Read off the call site in ExecutePrimitiveListWithTriangleList (0x829261C8..0x829261EC), which
// is the only caller in the image:
//     0x829261C8  lbz  r30, iterator.mCurrentHeader.mu8PrimTypeA   -> r4  the TYPE BYTE
//     0x829261CC  lhz  r15, iterator.mCurrentHeader.mu16PrimitiveATag -> r7  the TAG
//     0x829261D0  addi r14, r1, var_1A0                            -> r6  the DESTINATION
//     0x829261D4  bl   PrimitivePairList::Itterator::GetPrimativeA -> r5  the PACKED DATA
//     0x829261EC  bl   BuildGPInstance
// so: r3 = this (NEVER READ -- case 4 overwrites r3 with `addi r3, r5, 0x40` as its first
// instruction), r4 = EVolumeType, r5 = the packed primitive, r6 = the GPInstance to fill,
// r7 = the caller tag (zero-extended from 16 bits at every arm: `clrlwi r7, r7, 16`).
//
// The body is a 5-case jump table on `r4 - 1` (`addi r11, r4, -1 ; cmplwi cr6, r11, 4 ; bgt
// default`), i.e. exactly PrimitivePairList::EVolumeType 1..5, with the INVALID (0) and any
// out-of-range value falling into the `false` assert. Jump-table targets, from the listing's own
// case labels: case 0 -> 0x829223B0 (SPHERE), 1 -> 0x8292242C (CAPSULE), 2 -> 0x829224F4
// (4TRIANGLES -> assert), 3 -> 0x829222E4 (BOX), 4 -> 0x82922534 (CYLINDER).
//
// ⚠️ NO RETURN VALUE. IDA types it `int` because r3 is live at the `b __restgprlr_26`, but every
// arm leaves r3 holding a dead scratch (case 1 leaves 1, case 5 leaves 0, case 4 leaves a stack
// address) and the call site never reads it. void.
//
// ─── THE FIVE ARMS, STORE FOR STORE ──────────────────────────────────────────────────────────
// Common to every non-asserting arm (offsets are the console's; the host reaches them by name):
//     stw r7,   0x84  mVolumeTag  = tag      <-- BOTH tag words get the SAME caller tag; this is
//     stw r7,   0x88  mUserTag    = tag          NOT the `mVolumeTag = this, mUserTag = 0` shape
//                                                the rw Volume::CreateGPInstance siblings use.
//     stw ...,  0x94  mFlags      = 0        (li r26/r10/r3, 0 -- never the volume's flag word)
//     4 x lwz/stw -> +0xA4  mMethods = g_aGPVolumeMethods[type]
//
//  SPHERE (type 1, 16-byte CgsGeometric::Sphere -- KAU16_VOLUME_SIZES[1] == 16):
//     stvx128 v0, r0, r6         mPos = src[+0x00]  ALL FOUR LANES (the radius rides in w)
//     stb 0, 0x8C / stb 0, 0x8D  mNumFaceNormals = mNumEdgeDirections = 0
//     stw 1, 0x90                mVolumeType = GPInstance::SPHERE
//     vspltw v13,v0,3 -> stfs    mFatness = src[+0x00].w  (the radius)
//     mMethods <- off_82F91900 == unk_82F918F0 + 1*0x10, i.e. ROW 1
//
//  CAPSULE (type 2, 32-byte CgsGeometric::Capsule -- KAU16_VOLUME_SIZES[2] == 32):
//     stvx128 v0, r0, r6         mPos = src[+0x00]  (position.xyz, radius in w)
//     stvx128 v13, r6, 0x40      mEdgeDirections[0] = src[+0x10] (direction.xyz, length in w)
//     stb 0, 0x8C / stb 1, 0x8D  mNumFaceNormals = 0, mNumEdgeDirections = 1
//     stw 2, 0x90                mVolumeType = GPInstance::CAPSULE
//     vspltw v11,v0,3  -> stfs   mFatness      = src[+0x00].w  (the radius)
//     the +0x70 READ-MODIFY-WRITE round trip (lvx128 +0x70 ; stvx128 stack ; stfs lane 0 ;
//     lvx128 stack ; stvx128 +0x70)  ->  mDimensions.x = src[+0x10].w  (the length)
//        ⚠️ LANES y/z/w OF mDimensions ARE LEFT EXACTLY AS THE CALLER'S BUFFER HAD THEM -- not
//        zeroed. Identical treatment to the committed CapsuleVolume::CreateGPInstance.
//     mMethods <- unk_82F918F0[type], the type RE-READ from +0x90 (`lwz r9, 0x90(r6)`)
//
//  4TRIANGLES (type 3): assert only, ContactGeneratorJob.cpp:1660,
//     "Are you mad? Triangle-Triangle collision" -- and NOTHING is written to the instance.
//     (The triangle side of this worker never comes through here: the caller builds its four
//     triangle GPInstances from Triangle4::GetAOSTriangle, not from the pair blob.)
//
//  BOX (type 4, 80-byte CgsGeometric::Box -- KAU16_VOLUME_SIZES[4] == 80):
//     stvx128 v11, r0, r6        mPos = src[+0x30]  (the transform's CENTRE row)
//     stvx128 v0,  r6, 0x10      mFaceNormals[0]    = src[+0x00]  (Right)
//     stvx128 v0,  r6, 0x40      mEdgeDirections[0] = src[+0x00]     the SAME row twice
//     stvx128 v13, r6, 0x20      mFaceNormals[1]    = src[+0x10]  (Up)
//     stvx128 v13, r6, 0x50      mEdgeDirections[1] = src[+0x10]
//     stvx128 v12, r6, 0x30      mFaceNormals[2]    = src[+0x20]  (At)
//     stvx128 v12, r6, 0x60      mEdgeDirections[2] = src[+0x20]
//     stvx128 v10, r6, 0x70      mDimensions        = src[+0x40]  ALL FOUR LANES
//     stb 3, 0x8C / stb 3, 0x8D  mNumFaceNormals = mNumEdgeDirections = 3
//     stw 4, 0x90                mVolumeType = GPInstance::BOX
//     vspltw v9,v9,3 -> stfs     mFatness = src[+0x40].w
//     mMethods <- off_82F91930 == unk_82F918F0 + 4*0x10, i.e. ROW 4
//
//  CYLINDER (type 5, 80-byte CgsGeometric::Cylinder -- KAU16_VOLUME_SIZES[5] == 80):
//     stvx128 v12, r0, r6        mPos = src[+0x30]  (the CENTRE row)
//     stvx128 v0,  r6, 0x10      mFaceNormals[0]    = src[+0x20]   <-- THE AXIS
//     stvx128 v0,  r6, 0x40      mEdgeDirections[0] = src[+0x20]   <-- the same row
//     stvx128 v11, r6, 0x20      mFaceNormals[1]    = src[+0x10]
//     stvx128 v13, r6, 0x30      mFaceNormals[2]    = src[+0x00]
//     stb 1, 0x8C / stb 1, 0x8D  mNumFaceNormals = mNumEdgeDirections = 1
//     stw 5, 0x90                mVolumeType = GPInstance::CYLINDER
//     lvlx v10, r5, 0x44 ; vspltw 0 ; two round trips through +0x70:
//                                mDimensions.x = the f32 at src[+0x44]   (Cylinder::mfLength)
//     lvlx v9,  r5, 0x40 ; vspltw 0:
//                                mDimensions.y = the f32 at src[+0x40]   (Cylinder::mfRadius)
//     lfs f0, flt_82001CC0 -> stfs 0x80   mFatness = 0.0f   (see the constant below)
//     mMethods <- unk_82F918F0[type], the type RE-READ from +0x90
//
// ⭐ THREE INDEPENDENT CROSS-CHECKS OF THAT TABLE, none of them this cluster's own reasoning.
//  1. The committed rw::collision Volume producers fill the SAME GPInstance slots from the SAME
//     kind of source and agree lane for lane:
//       BoxVolume::CreateGPInstance      @0x82BA92E8 (BoxVolume.cpp:663)   -- rows 0/1/2 into
//         mFaceNormals[0..2] AND mEdgeDirections[0..2], row 3 into mPos, 3/3 counts;
//       CapsuleVolume::CreateGPInstance  @0x82BAF5F8 (CapsuleVolume.cpp:161) -- axis into
//         mEdgeDirections[0], centre into mPos, 0/1 counts, mDimensions.x = half height and the
//         SAME "+0x70 lane-x-only round trip, y/z/w survive" note;
//       CylinderVolume::CreateGPInstance @0x82BAC7F8 (CylinderVolume.cpp:276) -- frame[1] into
//         mFaceNormals[1], frame[0] into mFaceNormals[2], frame[2] into BOTH mFaceNormals[0] and
//         mEdgeDirections[0], frame[3] into mPos, 1/1 counts, mDimensions.x then .y.
//     The cylinder permutation in particular (rows landing 2/1/0 in slots 0/1/2) is a strange
//     enough shape that two producers agreeing on it is real evidence.
//  2. PrimitivePairList::KAU16_VOLUME_SIZES (rodata word_820DA934, dumped from the image in
//     CgsPrimitivePairList.cpp:164) is { 0, 16, 32, 224, 80, 80 }. Every arm above reads exactly
//     up to its type's size and no further: 16 / 32 / 80 / 0x48-of-80.
//  3. The CgsGeometric primitive layouts landed by round 2's `addprim` cluster from the OTHER
//     end of the pipe (PrimitivePairListBuilder::AddPrimitive's per-type packers) match slot for
//     slot: Sphere{Vector4 mPositionRadius}, Capsule{Vector3Plus mPositionAndRadius,
//     mDirectionAndLength}, Box{Matrix44Affine mTransform, Vector3Plus mDimensionsAndFatness},
//     Cylinder{Matrix44Affine mTransform, f32 mfRadius @+0x40, f32 mfLength @+0x44}.
//
// ⭐ A MEASUREMENT THIS BODY CLOSES FOR A HEADER IT DOES NOT OWN.
// CgsCylinder.h documents CgsGeometric::Cylinder::GetDirection as "DECLARED ONLY ... the
// cylinder's axis is one of the three basis rows of mTransform and NOTHING MEASURED SAYS WHICH".
// This body says which: the CYLINDER arm puts src[+0x20] -- mTransform.At() / zAxis -- into
// GPInstance::mEdgeDirections[0] and mFaceNormals[0], which are the two slots GPCylinder's SAT
// callbacks read as THE AXIS, and CylinderVolume::CreateGPInstance independently puts ITS axis
// row (maFrame[2]) in the same two slots. Reported, NOT edited (CgsCylinder.h is not this
// cluster's file) -- see the owner record.
// Same shape, same caveat: the lane that CylinderVolume fills from `mfHalfHeight` is the lane
// this body fills from `CgsGeometric::Cylinder::mfLength`, so that field is a HALF-length in GP
// terms. Reported, not renamed.
//
// =================================================================================================
// ─── 2. CollideGPInstances @0x829253C8 -- THE ABI TRAP AND THE RECORD ────────────────────────
//
// ⚠️⚠️ GOTCHA 3 IS LIVE HERE, AND IT IS THE ONLY REASON THIS SIGNATURE IS NOT OBVIOUS. The
// prologue never reads r6 (`addi r6, r1, var_280` at 0x829253E4 overwrites it before any use)
// while it does use r7/r8/r9/r10 AND f1. On this ABI a float argument consumes BOTH an FPR and
// its positional GPR slot, so r6 is the SKIPPED slot of the third parameter:
//     r3 = this
//     r4 = const GPInstance*  gp1        (-> ComputeContactPoints' first operand)
//     r5 = const GPInstance*  gp2        (-> its second)
//     f1 = f32 padding                   (r6 slot BURNED; stored to the stack at 0x829253D8 and
//                                         passed by address, because the vendor kernel takes it
//                                         as `const f32&`)
//     r7 = u16   -> record +0x48   muPrimitive0Index
//     r8 = u16   -> record +0x4A   muPrimitive1Index
//     r9 = u16   -> record +0x4C   mu16TestIndex
//     r10 = CollisionResultList*
// Confirmed at all four call sites in ExecutePrimitiveListWithTriangleList (0x829262B8,
// 0x829263AC, 0x829264A4, 0x8292659C): every one sets r4/r5/r7/r8/r9/r10 and f1 and NEVER r6,
// and r8 is `r27*4 + lane` for lane 0..3 -- the triangle index of the Triangle4 block's lane,
// exactly the muPrimitive0Index/muPrimitive1Index vocabulary the sphere worker already uses.
//
// ⚠️ NO RETURN VALUE (IDA's `unsigned int` is the trailing PrintStringed's r3; the call sites
// read r28, never r3). void.
//
// THE 80-BYTE RECORD, built once on the stack and copied per contact pair with a 10 x ld/std
// loop at `results + 80*index` (`rotlwi r7, idx, 2 ; add ; slwi 4` == idx*80):
//     +0x00  mPrimitive0Normal  = lStackResult.normal      <-- the SAME vector in both normal
//     +0x10  mPrimitive1Normal  = lStackResult.normal          slots (one v0, two stvx128)
//     +0x20  mPrimitive0Contact = pointPairs[i].p1   (the gp1 side)
//     +0x30  mPrimitive1Contact = pointPairs[i].p2   (the gp2 side)
//     +0x40  muPrimitive0Tag    = lStackResult.volumeTag1
//     +0x44  muPrimitive1Tag    = lStackResult.volumeTag2
//     +0x48/+0x4A/+0x4C        = the three u16 arguments, stored ONCE before the kernel call
//     +0x4E  muPad              NEVER WRITTEN -- console stack garbage travels into the queued
//                               copy. Zero-initialised here instead (deterministic; garbage is
//                               not reproducible), exactly as ExecuteSphereListWithTriangleList
//                               already does for the same field.
// `lStackResult` is the console's own local name, recovered from the assert strings.
//
// THE COUNT BUMP, verbatim (0x82925754..0x82925784) -- TWO halfword stores, not one:
//     lhz num, +0xC ; lhz max, +0xA ; addi num+1 ; sth num+1, +0xC     <-- unconditional
//     if (num+1 >= max) r9 = max-1                                     <-- clamp
//     sth r9, +0xC                                                    <-- second store
// i.e. the live count is published, then clamped so an overflowing list keeps overwriting its
// LAST slot. Reproduced as the two stores it is. (The sphere/swept workers do the same clamp
// against a local header copy; this worker has no local header -- it reads and writes the
// caller's CollisionResultList in place, `lwz/lhz/sth 0(r29)/0xA(r29)/0xC(r29)`.)
//
// ⚠️ THE RESULTS BUFFER IS THE 80-BYTE CARVE, NOT CollisionResult'S 112. The list was allocated
// by BaseCollisionGenerator::PrepareNewPrimitiveTestResultsList (80 * max, meResultType == 0),
// so mpResults is reinterpreted as PrimitiveTestResult[] -- the same reinterpretation, for the
// same reason, that ContactGeneratorJob.cpp:626 already makes.
//
// ⚠️ NaN POLARITY (gotcha 4). The three validity asserts are `vcmpeqfp. vX, vX, vX` self-
// compares on lanes 0/1/2 only, one splat at a time: a lane equals itself iff it is not NaN, and
// the w lane is NEVER tested. Written as `x == x && y == y && z == z`, the idiom this tree
// already uses at CgsLine.cpp:26 and CgsTriangle4.cpp:39. Both compile under /fp:precise.
//
// PC LOWERING, stated once: the console's VMX whole-register moves (lvx128/stvx128) are written
// here as four-lane copies through the two vector vocabularies this TU straddles --
// rw::collision::Vec4 on the GPInstance/ContactPoints side and rw::math::vpu::Vector3(Plus) on
// the CgsGeometric/PrimitiveTestResult side. Both are {f32 x,y,z,w} at 16 bytes, so the copy is
// exact; the helpers below exist so that no lane is ever dropped by an accessor that zeroes w.
// =================================================================================================

#include "GameShared/Jobs/ContactGenerator/ContactGeneratorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                            // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                    // gpDebugPrint ([DIAG])
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"               // CgsGeometric::Box
#include "GameShared/GameClasses/Geometric/Primitives/CgsCapsule.h"           // CgsGeometric::Capsule
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"          // CgsGeometric::Cylinder
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"            // CgsGeometric::Sphere
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "vendor/renderware/collision/GPInstance.hpp"                         // the GP vocabulary

#include <stdlib.h>   // getenv (the [DIAG] latch, host only)

using CgsSceneManager::CgsCollision::CollisionResultList;
using CgsSceneManager::CgsCollision::PrimitivePairList;
using CgsSceneManager::CgsCollision::PrimitiveTestResult;
using rw::collision::GPInstance;

namespace
{
    // ---------------------------------------------------------------------------------------
    // Lane-exact bridges between the two 16-byte vector vocabularies. The console moves these
    // rows with a single lvx128/stvx128 pair -- ALL FOUR LANES, including w -- so none of these
    // may go through an accessor that manufactures a zero w (CgsGeometric's GetPosition() /
    // GetDimensions() do exactly that, which is why the packed w lanes below are re-attached
    // from the matching GetRadius()/GetLength()/GetFatness() broadcast instead).
    // ---------------------------------------------------------------------------------------
    template <typename TSourceVector>
    inline rw::collision::Vec4 AsVec4(const TSourceVector& arSource)
    {
        rw::collision::Vec4 lvResult;
        lvResult.x = arSource.x;
        lvResult.y = arSource.y;
        lvResult.z = arSource.z;
        lvResult.w = arSource.w;
        return lvResult;
    }

    inline rw::collision::Vec4 MakeVec4(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        rw::collision::Vec4 lvResult;
        lvResult.x = lfX;
        lvResult.y = lfY;
        lvResult.z = lfZ;
        lvResult.w = lfW;
        return lvResult;
    }

    inline Vector3 AsVector3(const rw::collision::Vec4& arSource)
    {
        Vector3 lvResult;
        lvResult.x = arSource.x;
        lvResult.y = arSource.y;
        lvResult.z = arSource.z;
        lvResult.w = arSource.w;   // carried, not dropped -- the console moves 16 bytes
        return lvResult;
    }

    inline Vector3Plus AsVector3Plus(const rw::collision::Vec4& arSource)
    {
        Vector3Plus lvResult;
        lvResult.x = arSource.x;
        lvResult.y = arSource.y;
        lvResult.z = arSource.z;
        lvResult.w = arSource.w;
        return lvResult;
    }

    // The inlined `rw::math::IsValid( <Vector3> )` the three CollideGPInstances asserts spell.
    // vcmpeqfp. self-compare per lane, lanes x/y/z only (the console never splats lane 3).
    inline bool IsValidVector(const rw::collision::Vec4& arVector)
    {
        return arVector.x == arVector.x
            && arVector.y == arVector.y
            && arVector.z == arVector.z;
    }

    // X360 flt_82001CC0 -- the fatness the CYLINDER arm stores (`lfs f0, flt_82001CC0@l(r10)`).
    // The four bytes at that address are 00 00 00 00, measured twice and independently: this
    // wave's headless-idat dump (scratchpad/waveQ6/ida_pstream/out.json, recorded in
    // pstream.owner.md 2.3) and the committed FeatureEdge::KF_START_THRESHOLD, which the linker
    // folded onto the SAME rodata slot (FeatureEdge.cpp:31). It is spelled as its own constant
    // here rather than reused from FeatureEdge because the two are unrelated quantities that
    // merely share a zero.
    // WHY A CYLINDER GETS ZERO FATNESS AND A BOX DOES NOT: CgsGeometric::Cylinder simply has no
    // fatness member (Matrix44Affine + mfRadius + mfLength and 8 bytes of tail padding), while
    // CgsGeometric::Box packs one into mDimensionsAndFatness.w. The console is not discarding a
    // value here; there is none to read.
    const f32 KF_GP_CYLINDER_FATNESS = 0.0f;

    // The console's assert bound, spelled as the assert string spells it (ContactGeneratorJob.cpp
    // :1279): sizeof(pointPairs) / sizeof(PointPair). `cmplwi cr6, r31, 0x10 ; ble` -> 16.
    const u32 KU_MAX_CONTACT_POINT_PAIRS =
        static_cast<u32>(sizeof(GPInstance::ContactPoints::pointPairs)
                         / sizeof(GPInstance::ContactPoints::PointPair));
}

// =================================================================================================
// ContactGeneratorJob::BuildGPInstance @0x829222A0 (258 words / 254 listed rows -- see the file
// banner for why both numbers are right)
//
// Turn one packed primitive from a PrimitivePairList blob into an rw::collision::GPInstance.
// `this` is never read (see the banner); the member spelling is the console's.
// =================================================================================================
void ContactGeneratorJob::BuildGPInstance(PrimitivePairList::EVolumeType leVolumeType,
                                          const void*                    lpcPrimitiveData,
                                          GPInstance*                    lpInstance,
                                          u16                            lu16Tag)
{
    // The console's r4 is a 32-bit register holding the pair header's zero-extended
    // mu8PrimTypeA byte; the VALUES are PrimitivePairList::EVolumeType, which is how the
    // declaration spells it, and the five arms below are the jump table's five cases.
    switch (leVolumeType)
    {
        // -------------------------------------------------------------------------------------
        case PrimitivePairList::E_VOLUME_TYPE_SPHERE:            // jumptable case 0 -> 0x829223B0
        {
            const CgsGeometric::Sphere* lpSphere =
                static_cast<const CgsGeometric::Sphere*>(lpcPrimitiveData);

            lpInstance->mPos               = AsVec4(lpSphere->mPositionRadius);
            lpInstance->mVolumeTag         = lu16Tag;
            lpInstance->mUserTag           = lu16Tag;
            lpInstance->mNumFaceNormals    = 0;
            lpInstance->mNumEdgeDirections = 0;
            lpInstance->mFlags             = 0;
            lpInstance->mVolumeType        = GPInstance::SPHERE;
            lpInstance->mFatness           = lpSphere->mPositionRadius.w;   // vspltw lane 3
            lpInstance->mMethods           = rw::collision::g_aGPVolumeMethods[GPInstance::SPHERE];
            break;
        }

        // -------------------------------------------------------------------------------------
        case PrimitivePairList::E_VOLUME_TYPE_CAPSULE:           // jumptable case 1 -> 0x8292242C
        {
            const CgsGeometric::Capsule* lpCapsule =
                static_cast<const CgsGeometric::Capsule*>(lpcPrimitiveData);

            // The two packed rows, rebuilt with their w lanes intact (the accessors split them).
            const Vector3 lvPosition  = lpCapsule->GetPosition();
            const Vector3 lvDirection = lpCapsule->GetDirection();
            const f32     lfRadius    = lpCapsule->GetRadius().w;
            const f32     lfLength    = lpCapsule->GetLength().w;

            lpInstance->mPos               = MakeVec4(lvPosition.x, lvPosition.y,
                                                      lvPosition.z, lfRadius);
            lpInstance->mEdgeDirections[0] = MakeVec4(lvDirection.x, lvDirection.y,
                                                      lvDirection.z, lfLength);
            lpInstance->mVolumeTag         = lu16Tag;
            lpInstance->mUserTag           = lu16Tag;
            lpInstance->mNumFaceNormals    = 0;
            lpInstance->mNumEdgeDirections = 1;
            lpInstance->mFlags             = 0;
            lpInstance->mVolumeType        = GPInstance::CAPSULE;
            lpInstance->mFatness           = lfRadius;

            // The +0x70 round trip: lane x only; y/z/w survive untouched.
            lpInstance->mDimensions.x      = lfLength;

            // The console re-reads the type word it just stored; keep that read.
            lpInstance->mMethods = rw::collision::g_aGPVolumeMethods[lpInstance->mVolumeType];
            break;
        }

        // -------------------------------------------------------------------------------------
        case PrimitivePairList::E_VOLUME_TYPE_4TRIANGLES:        // jumptable case 2 -> 0x829224F4
        {
            // Assert and return; the instance is left exactly as the caller had it.
            CGS_ASSERT(false, "Are you mad? Triangle-Triangle collision");            // :1660
            break;
        }

        // -------------------------------------------------------------------------------------
        case PrimitivePairList::E_VOLUME_TYPE_BOX:               // jumptable case 3 -> 0x829222E4
        {
            const CgsGeometric::Box* lpBox =
                static_cast<const CgsGeometric::Box*>(lpcPrimitiveData);

            const Matrix44Affine lTransform  = lpBox->GetTransform();
            const Vector3        lvDimensions = lpBox->GetDimensions();
            const f32            lfFatness    = lpBox->GetFatness().w;

            lpInstance->mPos               = AsVec4(lTransform.Pos());
            lpInstance->mFaceNormals[0]    = AsVec4(lTransform.Right());
            lpInstance->mEdgeDirections[0] = AsVec4(lTransform.Right());
            lpInstance->mFaceNormals[1]    = AsVec4(lTransform.Up());
            lpInstance->mEdgeDirections[1] = AsVec4(lTransform.Up());
            lpInstance->mFaceNormals[2]    = AsVec4(lTransform.At());
            lpInstance->mEdgeDirections[2] = AsVec4(lTransform.At());

            // mDimensions takes the WHOLE packed row -- half-extents in xyz, fatness in w.
            lpInstance->mDimensions        = MakeVec4(lvDimensions.x, lvDimensions.y,
                                                      lvDimensions.z, lfFatness);

            lpInstance->mVolumeTag         = lu16Tag;
            lpInstance->mUserTag           = lu16Tag;
            lpInstance->mNumFaceNormals    = 3;
            lpInstance->mNumEdgeDirections = 3;
            lpInstance->mFlags             = 0;
            lpInstance->mVolumeType        = GPInstance::BOX;
            lpInstance->mFatness           = lfFatness;
            lpInstance->mMethods           = rw::collision::g_aGPVolumeMethods[GPInstance::BOX];
            break;
        }

        // -------------------------------------------------------------------------------------
        case PrimitivePairList::E_VOLUME_TYPE_CYLINDER:          // jumptable case 4 -> 0x82922534
        {
            const CgsGeometric::Cylinder* lpCylinder =
                static_cast<const CgsGeometric::Cylinder*>(lpcPrimitiveData);

            const Matrix44Affine lTransform = lpCylinder->GetTransform();

            lpInstance->mPos               = AsVec4(lTransform.Pos());
            lpInstance->mFaceNormals[0]    = AsVec4(lTransform.At());     // THE AXIS
            lpInstance->mEdgeDirections[0] = AsVec4(lTransform.At());     // the same row
            lpInstance->mFaceNormals[1]    = AsVec4(lTransform.Up());
            lpInstance->mFaceNormals[2]    = AsVec4(lTransform.Right());

            lpInstance->mVolumeTag         = lu16Tag;
            lpInstance->mUserTag           = lu16Tag;
            lpInstance->mNumFaceNormals    = 1;   // 1, while THREE face-normal rows are written
            lpInstance->mNumEdgeDirections = 1;
            lpInstance->mFlags             = 0;
            lpInstance->mVolumeType        = GPInstance::CYLINDER;

            // The two +0x70 round trips: lanes x then y; z/w survive untouched.
            lpInstance->mDimensions.x      = lpCylinder->GetLength().w;   // f32 @ src+0x44
            lpInstance->mDimensions.y      = lpCylinder->GetRadius().w;   // f32 @ src+0x40

            lpInstance->mFatness           = KF_GP_CYLINDER_FATNESS;      // flt_82001CC0 == 0.0f

            // The console re-reads the type word it just stored; keep that read.
            lpInstance->mMethods = rw::collision::g_aGPVolumeMethods[lpInstance->mVolumeType];
            break;
        }

        // -------------------------------------------------------------------------------------
        default:                                     // jumptable default case -> 0x82922640
        {
            // E_VOLUME_TYPE_INVALID and anything past CYLINDER. The console's expression text is
            // literally "false" -- an unconditional CGS_ASSERT( false ) in the source.
            CGS_ASSERT(false, "false");                                               // :1699
            break;
        }
    }
}

// =================================================================================================
// ContactGeneratorJob::CollideGPInstances @0x829253C8 (244)
//
// Run the narrow-phase contact kernel over one GPInstance pair and queue one 80-byte
// PrimitiveTestResult per contact point pair into the caller's CollisionResultList.
// =================================================================================================
void ContactGeneratorJob::CollideGPInstances(const GPInstance*    lpGPInstance0,
                                             const GPInstance*    lpGPInstance1,
                                             f32                  lfPadding,
                                             u16                  lu16Primitive0Index,
                                             u16                  lu16Primitive1Index,
                                             u16                  lu16TestIndex,
                                             CollisionResultList* lpResultsList)
{
    // The record is carved ONCE: the console writes its three index halfwords BEFORE the kernel
    // call (0x829253E0/E8/F0) and only refreshes the vectors and the two tag words per pair.
    // ⚠️ The `= {}` also zeroes muPad (+0x4E), which the console never writes -- see the banner.
    PrimitiveTestResult lRecord = {};
    lRecord.muPrimitive0Index = lu16Primitive0Index;   // sth r7
    lRecord.muPrimitive1Index = lu16Primitive1Index;   // sth r8
    lRecord.mu16TestIndex     = lu16TestIndex;         // sth r9

    // The console's own local name, recovered from the assert strings. Deliberately NOT
    // zero-initialised: it is a 560-byte stack local on the hot path and the kernel fills every
    // field it publishes, exactly as on the console. Nothing below reads it unless the kernel
    // returned a non-zero count, which is the guard that makes that safe.
    GPInstance::ContactPoints lStackResult;

    const u32 luNumContacts = rw::collision::ComputeContactPoints(*lpGPInstance0, *lpGPInstance1,
                                                                  lfPadding, lStackResult);

    // The console compares against 16 and asserts on GREATER; the `>= 0` half of the source
    // expression is vacuous on an unsigned count and emits no code.
    CGS_ASSERT(luNumContacts <= KU_MAX_CONTACT_POINT_PAIRS,
               "luNumContacts >= 0 && luNumContacts <= sizeof( lStackResult.pointPairs ) / "
               "sizeof(rw::collision::GPInstance::ContactPoints::PointPair)");        // :1279

    if (luNumContacts == 0)
    {
        return;
    }

    // The console rotates this into `if (numPoints) do { ... } while (i < numPoints)` and
    // RELOADS the bound every pass; a plain for-loop is the same thing.
    for (u32 luCurrentContactPoint = 0;
         luCurrentContactPoint < lStackResult.numPoints;
         ++luCurrentContactPoint)
    {
        const GPInstance::ContactPoints::PointPair& lrPair =
            lStackResult.pointPairs[luCurrentContactPoint];

        CGS_ASSERT(IsValidVector(lrPair.p1),
                   "rw::math::IsValid( lStackResult.pointPairs[liCurrentContactPoint].p1 )"); // :1286
        CGS_ASSERT(IsValidVector(lrPair.p2),
                   "rw::math::IsValid( lStackResult.pointPairs[liCurrentContactPoint].p2 )"); // :1287
        CGS_ASSERT(IsValidVector(lStackResult.normal),
                   "rw::math::IsValid( lStackResult.normal )");                               // :1288

        // Both normal slots get the SAME contact normal (one v0, two stvx128) -- the same shape
        // the sphere kernel produces, where the two "sides" share one contact direction.
        lRecord.mPrimitive0Normal  = AsVector3(lStackResult.normal);
        lRecord.mPrimitive1Normal  = AsVector3(lStackResult.normal);
        lRecord.mPrimitive0Contact = AsVector3Plus(lrPair.p1);
        lRecord.mPrimitive1Contact = AsVector3Plus(lrPair.p2);
        lRecord.muPrimitive0Tag    = lStackResult.volumeTag1;
        lRecord.muPrimitive1Tag    = lStackResult.volumeTag2;

        // The 80-byte carve, not CollisionResult's 112 -- see the banner.
        PrimitiveTestResult* lpaResults =
            reinterpret_cast<PrimitiveTestResult*>(lpResultsList->mpResults);

        lpaResults[lpResultsList->mu16NumResults] = lRecord;   // 10 x ld/std at base + 80*index

        // idx+1 published first, then clamped to max-1: an overflowing list keeps overwriting
        // its last slot. Two halfword stores, exactly as the console emits them.
        u16 lu16Next = static_cast<u16>(lpResultsList->mu16NumResults + 1);
        lpResultsList->mu16NumResults = lu16Next;
        if (lu16Next >= lpResultsList->mu16MaxNumResults)
        {
            lu16Next = static_cast<u16>(lpResultsList->mu16MaxNumResults - 1);
        }
        lpResultsList->mu16NumResults = lu16Next;

        // ---------------------------------------------------------------------------------
        // [DIAG] NOT IN THE X360 BINARY -- ONE-SHOT, behind BRN_PROP_DIAG, getenv latched once
        // (a getenv per contact would be a syscall on the hot path).
        //
        // This is the line that says the prop NARROW PHASE produced geometry. Read it as a pair
        // with "[Q6-worldc] first prop-vs-world contact pass" (the poster) and with
        // "Warning!! prop fell out of the world": worldc firing and this one NOT firing means
        // pairs are being posted but no primitive ever collides; both firing while parts still
        // fall means the contact is generated but not consumed downstream.
        // ---------------------------------------------------------------------------------
        {
            static const bool sbPropDiag     = (getenv("BRN_PROP_DIAG") != 0);
            static bool       sbFirstContact = true;
            if (sbPropDiag && sbFirstContact && CgsDev::Log::gpDebugPrint != 0)
            {
                sbFirstContact = false;
                *CgsDev::Log::gpDebugPrint
                    << "[Q6-gpi] first GP narrow-phase contact: gp1 type "
                    << static_cast<s32>(lpGPInstance0->mVolumeType)
                    << " vs gp2 type " << static_cast<s32>(lpGPInstance1->mVolumeType)
                    << ", " << static_cast<s32>(lStackResult.numPoints)
                    << " point pair(s), result slot "
                    << static_cast<s32>(lpResultsList->mu16NumResults) << "\n";
            }
        }
    }
}
