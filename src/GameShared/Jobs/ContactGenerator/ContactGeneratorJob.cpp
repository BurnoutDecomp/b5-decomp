// =================================================================================================
// GameShared/Jobs/ContactGenerator/ContactGeneratorJob.cpp
//
// ⭐⭐⭐ THE DRAIN. Reconstructed from BURNOUT_X360_ARTIST.XEX (traction-line wave, 2026-08-11).
// The console's own path for this TU is baked into every assert here:
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGeneratorJob.cpp
//
//   Execute                             @0x829267E0   (77)  :135
//   ExecuteLineWithTriangleListStream   @0x82921968  (589)  :1151 :1152  + ContactGeneratorJob.h:167
//   ExecuteSphereListWithTriangleList   @0x829226A8  (967)  :182 :233 :273 :313 :353   ⭐ walls leg 2
//   ExecuteSphereListWithTriangleListStream @0x829235C8 (100)  :381 :382
//   ExecuteSweptSphereListWithTriangleList  @0x829238E8 (1620) :497 :513 :539 :565 :591  ⭐ swept leg
//   ExecuteSweptSphereListWithTriangleListStream @0x82925238 (100)  :642 :643
//   ExecutePrimitiveListWithTriangleListStream @0x82926650 (100) :1104 :1105  ⭐ PROP stream
//   ExecutePrimitiveListWithTriangleList @0x82925908 (849) :1052 :1059 :1066 :1073 ⭐⭐ PROP
//                                                          NARROW PHASE (wave Q6, cluster pvt)
//   ExecuteBoxListWithTriangleList      @0x829218B8   (44)  :841   ⚠️ the CONSOLE'S OWN
//                                                          "Not implemented" arm (wave Q7, arms)
//   ExecutePrimitivePairList            @0x82925798   (92)  CgsPrimitivePairList.h:107
//                                                          ⭐⭐ the CAR-vs-CAR / body-part PAIR
//                                                          WALK (wave Q7, cluster arms)
//   LoadPrimitives                      @0x829210F0   (61)  :1405 :1406
//   LoadResultList                      @0x829211E8   (46)  :1552
//   AllocateMemory                      @0x829212A0   (54)  :1720
//   RestoreMemory                       @0x82921050   (39)  ContactGeneratorJob.h:167
//
// Declared in the header, bodied in the sibling partfile ContactGeneratorJob_wQ6_01.cpp
// (wave Q6, cluster gpi):  BuildGPInstance @0x829222A0 (258) · CollideGPInstances @0x829253C8
// (244). Both are called by ExecutePrimitiveListWithTriangleList AND by ExecutePrimitivePairList
// below -- the pair walk is the second (and, in the console, the FIRST-written) caller of each.
//
// Until this TU existed, the triangle cache filled with real Paradise City geometry every frame
// and NOTHING READ IT. This is the reader.
//
// ─── EVERY WORKER ARM IS REAL (2026-08-19, wave Q7) ──────────────────────────────────────────
// `Execute` is a 12-ENTRY jump table over the descriptor's type byte (0x8292681C `addi r11,r11,-5`
// then `cmplwi r11,0xB`, so entry i == type i+5, types 5..16). ELEVEN of those entries are worker
// arms; the remaining one, entry 10 == TYPE 15 (the ELEVENTH entry; entry 11 == type 16 is a real
// arm, ExecuteLineWithTriangleListStream), is the table's own default target -- the console has no
// type-15 job and routes it to the same "Unsupported collision job" assert as an out-of-range
// byte. As of this wave all ELEVEN arms have bodies. NINE are in this file (types 5, 6, 9, 10, 11,
// 12, 13, 14, 16); the other TWO -- ExecuteSphereListWithSphereList @0x829215B0 and its Stream
// twin @0x82923758 (types 7 and 8: the car-vs-car sphere narrow phase) -- are DECLARED in this
// TU's header and DEFINED in the sibling partfile ContactGeneratorJob_wQ7_01.cpp (wave Q7,
// cluster ss), split only because this file was owned by a concurrent session in the same wave.
// ⚠️ THAT PARTFILE MUST BE MOUNTED BESIDE THIS TU, exactly as ContactGeneratorJob_wQ6_01.cpp is:
// `cl /c` cannot see an unresolved external, so the compile gate stays green either way and the
// exe takes two LNK2019s instead. If cluster ss parked, the two declarations in the header have
// no definition anywhere and the conductor must restore a gate for that PAIR ONLY.
//
// ⚠️ "REAL" DOES NOT MEAN "DOES WORK" FOR ExecuteBoxListWithTriangleList. Those 44 instructions
// ARE an assert: the console ships `CGS_ASSERT(false, "Not implemented")` at :841 and there is
// no box-vs-triangle-list test anywhere in the X360 image. Landing it is landing the console's
// own refusal -- see its banner below. Nothing posts a type-9 descriptor either: `xrefs_to` on
// 0x829218B8 is ContactGeneratorJob::Execute and nothing else, and no `BaseCollisionGenerator::
// Run*` dispatcher writes 9 into the batch's +0x4CF job-type byte.
//
// ⚠️ THE `default:` ASSERT STAYS, AND IT IS NOW THE ONLY THING THIS SWITCH CAN REFUSE.
// `xrefs_to` on ContactGeneratorJob::Execute @0x829267E0 shows a SINGLE caller
// (ContactGeneratorEntry @0x82920F10) and every `BaseCollisionGenerator::Run*` dispatcher points
// its batches at that same entry, so a descriptor carrying types 0-4, 15, or anything above 16
// lands on "Unsupported collision job" (:135) -- the console's own tripwire for a corrupt or
// mis-typed batch.
//
// ─── THE ALGORITHM, AND WHERE IT CAME FROM ───────────────────────────────────────────────────
// ExecuteLineWithTriangleListStream inlines its whole intersection kernel -- `xrefs_from` on
// 0x82921968 lists only __savegprlr_14, Assert::PrintStringed, SimpleDataStreamConsumer::
// {Construct,AddResult,Destruct}, ContactGeneratorJob::AllocateMemory, DataStreamCommandReader::
// ReadCom and _blkmov. **There is no call into CgsGeometric at all.** In particular it does NOT
// run CgsGeometric::IntersectLinePolygonSoupNearestSingleSided @0x8283BC98: that function is a
// different algorithm (plane-crossing t + three scalar triple products, ONE Newton-Raphson step)
// on a different leg (PolygonSoupTesterJob::LineTestNearestSS / CollideLineAgainstPolySoupList-
// Nearest), and neither calls the other. Do not reuse one kernel for the other.
//
// What is inlined here is **Moller-Trumbore**, single-sided, unnormalised, SoA over four
// triangles per Triangle4 block, with `t` clamped to the segment [0,1].
//
// ─── PC LOWERING, STATED ONCE FOR THE WHOLE FILE ─────────────────────────────────────────────
// The console runs this as VMX128 with the four SIMD lanes holding FOUR DIFFERENT TRIANGLES and
// the line's components `vspltw`-broadcast. This tree's rw::math::vpu::Vector4 is a plain
// 16-byte {x,y,z,w} struct with no SIMD operations, and the established precedent for this exact
// family (CgsTriangle4.cpp, CgsPolygonSoupTests.cpp) is portable scalar float math. Everything
// below follows it, and that is also the safest possible answer to the standing "which lane?"
// hazard: once the console's SoA registers are written as `f32 v[4]`, a lane is an ARRAY INDEX
// and there is no swizzle left to get wrong.
// ⚠️ TWO PLACES WHERE THE ARITHMETIC IS NOT BIT-IDENTICAL TO THE CONSOLE, both flagged at the
// site: `vrsqrtefp` + 2 Newton-Raphson steps is lowered to `1/sqrt()`, and `vrefp` + 2
// Newton-Raphson steps is lowered to a divide. Both are MORE accurate than the console, both are
// the precedent CgsPolygonSoupTests.cpp already set, and neither changes an accept/reject
// decision except in the last couple of ulps.
// =================================================================================================

#include "GameShared/Jobs/ContactGenerator/ContactGeneratorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint (the gates + witness)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"          // Sphere (16B centre+radius)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSweptSphere.h"      // SweptSphere (the swept arm)
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"       // Triangle4 (the SoA block)
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h" // the sphere contact kernel
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsBoxListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithTriangleListStreamJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitiveListWithTriangleListStreamJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsPrimitivePairListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithSphereListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSweptSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsPrimitivePairList.h"
#include "vendor/renderware/collision/GPInstance.hpp"   // GPInstance + g_aGPVolumeMethods

#include <cmath>     // std::sqrt (the vrsqrtefp lowering) / std::fabs (the vandc sign clear)
#include <cstdlib>   // std::getenv (the BRN_PROP_DIAG latch)
#include <cstring>   // std::memcpy (reading a lane's raw bit pattern)

// includes folded in from the ContactGeneratorJob_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Geometric/Primitives/CgsBox.h"               // CgsGeometric::Box
#include "GameShared/GameClasses/Geometric/Primitives/CgsCapsule.h"           // CgsGeometric::Capsule
#include "GameShared/GameClasses/Geometric/Primitives/CgsCylinder.h"          // CgsGeometric::Cylinder
#include <stdlib.h>   // getenv (the [DIAG] latch, host only)
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"

// The six per-job-thread contexts. X360 base unk_831BBF80, stride 0x10300, bound 6 --
// all three read out of ContactGeneratorEntry @0x82920F10 (see ContactGenerator.cpp).
ContactGeneratorJob gaContactGeneratorJobs[KI_NUM_CONTACT_GENERATOR_JOBS];

using CgsSceneManager::CgsCollision::BoxListWithTriangleListJobDesc;
using CgsSceneManager::CgsCollision::CollisionJobDescription;
using CgsSceneManager::CgsCollision::CollisionResultList;
using CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc;
using CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListJobDesc;
using CgsSceneManager::CgsCollision::PrimitiveListWithTriangleListStreamJobDesc;
using CgsSceneManager::CgsCollision::PrimitivePairList;
using CgsSceneManager::CgsCollision::PrimitivePairListJobDesc;
using CgsSceneManager::CgsCollision::PrimitiveTestResult;
using CgsSceneManager::CgsCollision::SphereListWithSphereListJobDesc;
using CgsSceneManager::CgsCollision::SphereListWithTriangleListJobDesc;
using CgsSceneManager::CgsCollision::SphereListWithTriangleListStreamJobDesc;
using CgsSceneManager::CgsCollision::SweptSphereListWithTriangleListJobDesc;
using CgsSceneManager::CgsCollision::SweptSphereListWithTriangleListStreamJobDesc;
using CgsSceneManager::CgsCollision::TriangleList;

namespace
{
    // ---------------------------------------------------------------------------------------
    // ⚠️⚠️ THE TWO TOLERANCES ARE IDENTICALLY ZERO, AND THAT IS A MEASUREMENT, NOT A DEFAULT.
    //
    // The console loads two 16-byte constants once per triangle block:
    //   0x82921A08  lvx128 v17, r0, unk_8321D330      -- eps0, the minimum-determinant threshold
    //   0x82921A14  lvx128 v18, r0, unk_8321D310      -- eps1, the relative barycentric tolerance
    // Both are ZERO, discriminated three ways rather than asserted (re-run this wave, not
    // inherited from the decode wave that first found it):
    //   1. `x360rd.read(0x8321D310,16)` and `(0x8321D330,16)` both return 16 zero bytes.
    //   2. The whole surrounding 0x8321C000..0x8321F000 block is a zero-filled module-static
    //      region -- a word scan found 0 non-zero words in 3072.
    //   3. A full-text scan of ALL 30,084 X360 export JSONs for `8321D3[0-3]x` returns EXACTLY
    //      ONE file: 0x82921968.json -- this worker -- and it only ever `lvx128`s them.
    //      No writer exists among the exported functions.
    // ⇒ at runtime the acceptance test degenerates to the exact, tolerance-free form:
    //      det > 0 && u >= 0 && u <= det && v >= 0 && u+v <= det && tnum >= 0 && tnum <= det
    // in particular `det > 0` STRICTLY, with no minimum-determinant guard: an edge-on or
    // degenerate triangle is rejected by `det > 0` alone.
    //
    // ⚠️ RESIDUAL RISK, STATED: the X360 export set has holes, so a writer COULD live in one --
    // neighbours at 0x8321D340/0x8321D358/0x8321D36C are touched by nearby ContactGenerator
    // functions, so a module Construct in a hole is a live possibility. What is proved is that
    // the values are zero at load and that no EXPORTED function writes them. If a future wave
    // ever sees traction acting too permissive at grazing angles, this is the first place to
    // look -- which is why they are named constants with the console's addresses on them rather
    // than folded into the comparisons as literal 0.
    //
    // (The console permutes eps0 through two vperm controls and a vsldoi before comparing --
    // 0x82921A54 `vperm v11, v17, v17, v1`, 0x82921A5C `vperm v12, v17, v17, v7`, 0x82921A60
    // `vsldoi v20, v11, v12, 8` -- i.e. the comparison operand is a LANE MIX of eps0. Every
    // permutation of a vector whose four lanes are all zero is that same zero vector, so the
    // mix is unobservable here and is not reproduced. If eps0 ever turns out to be written with
    // four DIFFERENT lanes, the mix has to come back.)
    // ---------------------------------------------------------------------------------------
    // [aiwave 2026-09-03, lane P2b finding] NOT ZERO. unk_8321D330 / unk_8321D310 are .bss statics with
    // a dyn-init writer in an export hole (@0x82C71D50 / @0x82C71D78), seeded from flt_8200D5F0 == 1e-8
    // and flt_82004884 == 1e-5 (both read from image.bin). The VehicleManager player-stuck kernel
    // (BrnVehicleManager_PlayerStuck.cpp) is seeded @0x82C5B650/@0x82C5B678 from the SAME two floats
    // into its own twins 0x82FB9F10 (1e-8, the determinant floor) / 0x82FB9EF0 (1e-5, the barycentric
    // tolerance); the pairing here follows that read (higher address = determinant). With 0.0 every
    // near-parallel ray passed the determinant test. The lane-mix note above still holds (four equal lanes).
    const f32 KF_MIN_DETERMINANT = 1.0e-8f;        // unk_8321D330, all four lanes (flt_8200D5F0)
    const f32 KF_BARYCENTRIC_TOLERANCE = 1.0e-5f;  // unk_8321D310, all four lanes (flt_82004884)

    // flt_8210168C == 0x7F7FFFFF. Splatted into the per-line "nearest hit so far" array at
    // 0x82921B98..0x82921BCC before every command.
    const f32 KF_NO_HIT_DISTANCE = 3.4028235e38f;

    // A Triangle4 lane's enable bit. Triangle4::mValidMasks (+0x90) is a four-lane bit mask,
    // ANDed into the hit mask at 0x82921F88 (`vand v12, v12, v6`). The producer side writes
    // 0xFFFFFFFF or 0x00000000 per lane (CgsPolygonSoupTests.cpp builds them exactly that way),
    // so a non-zero pattern means "this lane holds a real triangle".
    inline bool IsLaneEnabled(const Vector4& lrMasks, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrMasks.x)[liLane], sizeof(u32));
        return luBits != 0u;
    }

    // Read a lane out of a Vector4 as a raw word. The surface tag travels through
    // Triangle4::mSurfaceTags (+0xA0) as a float register, is `vspltw`-broadcast by the
    // nearest-select, and is written to the result as the FIRST WORD of that broadcast
    // (0x829221E0 -- "all four lanes are equal, so take lane 0"). Reading it as a bit pattern
    // rather than a float is what keeps a tag whose encoding happens to be a NaN or a denormal
    // intact; the console never touches it with an FP operation either, only vsel and vspltw.
    inline u32 LaneBits(const Vector4& lrV, s32 liLane)
    {
        u32 luBits = 0;
        std::memcpy(&luBits, &(&lrV.x)[liLane], sizeof(u32));
        return luBits;
    }

    // =======================================================================================
    // ⭐⭐ THE PROP NARROW PHASE'S TRIANGLE SIDE (wave Q6, cluster pvt).
    //
    // rw::collision::Vec4 (vendor/renderware/collision/FeatureEdge.hpp) and this tree's
    // Vector3 (rw::math::vpu::Vector3) are the SAME 16 bytes -- one VMX register, xyzw -- under
    // two C++ names, because the collision vocabulary was recovered in its own vendor home.
    // The console moves them with a single lvx128/stvx128 and never converts. These three
    // helpers are that move, written out; they are lowering scaffolding, not behaviour.
    // ---------------------------------------------------------------------------------------
    inline rw::collision::Vec4 ToVec4(const Vector3& lrV)
    {
        rw::collision::Vec4 lOut;
        lOut.x = lrV.x;
        lOut.y = lrV.y;
        lOut.z = lrV.z;
        lOut.w = lrV.w;   // the AOS rows carry w == 0 (GetAOSTriangle writes it); vsubfp/vmulfp
                          // below run on all four lanes, so w rides along exactly as on VMX.
        return lOut;
    }

    inline rw::collision::Vec4 SubVec4(const rw::collision::Vec4& lrA,
                                       const rw::collision::Vec4& lrB)
    {
        rw::collision::Vec4 lOut;
        lOut.x = lrA.x - lrB.x;
        lOut.y = lrA.y - lrB.y;
        lOut.z = lrA.z - lrB.z;
        lOut.w = lrA.w - lrB.w;
        return lOut;
    }

    inline rw::collision::Vec4 ScaleVec4(const rw::collision::Vec4& lrV, f32 lfScale)
    {
        rw::collision::Vec4 lOut;
        lOut.x = lrV.x * lfScale;
        lOut.y = lrV.y * lfScale;
        lOut.z = lrV.z * lfScale;
        lOut.w = lrV.w * lfScale;
        return lOut;
    }

    // vmsum3fp128 -- the three-lane dot product, broadcast on the console.
    inline f32 Dot3(const rw::collision::Vec4& lrA, const rw::collision::Vec4& lrB)
    {
        return (lrA.x * lrB.x) + (lrA.y * lrB.y) + (lrA.z * lrB.z);
    }

    // flt_82001CC0 == 00 00 00 00 == 0.0f. The GP triangle image the narrow phase builds
    // carries NO surface fatness -- the padding it collides with rides in the pair record's
    // mfPadding instead (the f1 argument of CollideGPInstances). Re-measured this wave off a
    // private .i64 copy with headless IDA 9.3 (scratchpad/waveQ6/ida_pvt/out.json), not
    // inherited from a banner.
    const f32 KF_GP_TRIANGLE_FATNESS = 0.0f;

    // flt_82004014 == 3D CC CC CD == 0.1f -- the tolerance the four per-lane unit-length
    // asserts compare |MagnitudeSquared(normal) - 1| against. DOUBLE-WITNESSED: the rodata
    // word, and the assert message text below, which literally ends "< 0.1f".
    const f32 KF_GP_TRIANGLE_NORMAL_TOLERANCE = 0.1f;

    // The four assert message strings, recovered whole (IDA truncates them to 39 characters in
    // the operand comment; these are the full bytes from the same private-.i64 run):
    //   0x82101C78 / 0x82101CE8 / 0x82101D58 / 0x82101DC8, fired at
    //   ContactGeneratorJob.cpp:1052 / :1059 / :1066 / :1073.
    // ⚠️ THEY ARE WHY THE FOUR LANES ARE NOT A COMPILER UNROLL: each names a DIFFERENT source
    // local (lGPTriangle0..lGPTriangle3), so the console SOURCE was hand-unrolled with four
    // named GP triangles. The reconstruction re-rolls the lane loop -- every other operand is
    // a pure lane index -- and keeps the four texts in this table so no message is lost. (The
    // sibling swept worker collapsed its four to one; this does not.)
    const char* const KAPC_GP_TRIANGLE_NORMAL_ASSERTS[4] =
    {
        "RwMathVPU::Abs(RwMathVPU::MagnitudeSquared(lGPTriangle0.Normal()) - RwMathVPU::GetVecFloat_One()) < 0.1f",
        "RwMathVPU::Abs(RwMathVPU::MagnitudeSquared(lGPTriangle1.Normal()) - RwMathVPU::GetVecFloat_One()) < 0.1f",
        "RwMathVPU::Abs(RwMathVPU::MagnitudeSquared(lGPTriangle2.Normal()) - RwMathVPU::GetVecFloat_One()) < 0.1f",
        "RwMathVPU::Abs(RwMathVPU::MagnitudeSquared(lGPTriangle3.Normal()) - RwMathVPU::GetVecFloat_One()) < 0.1f",
    };

    // ---------------------------------------------------------------------------------------
    // BuildGPTriangleInstance -- the OUTLINED rw::collision::GPTriangle::Initialize
    // (canonical rwccore.h:1360, `always_inline`) that the X360 compiler folded FOUR TIMES into
    // ExecutePrimitiveListWithTriangleList @0x82925908 (0x82925A48..0x829261B8, i.e. more than
    // half the function's 849 instructions). It is reconstructed store for store against that
    // asm, and it agrees field for field with the committed second witness in this tree,
    // rw::collision::TriangleVolume::CreateGPInstance @0x82BBAA00
    // (vendor/renderware/collision/TriangleVolume_wN_01.cpp:192) -- same edge triple, same
    // (L0,L1,L2,L0) mDimensions gather, same store set.
    //
    // The X360 store set, verbatim -- lane 0's copy is the one cited, and every address below is
    // the STORE instruction itself, not the `addi` that computed its address (offsets from
    // GP + 0x00, which is r1+0xE0 for lane 0):
    //   0x82925A94  stvx128 -> +0x00  mPos               = V0
    //   0x82925AC8  stvx128 -> +0x10  mFaceNormals[0]    = the AOS face normal
    //   0x82925AA4  stvx128 -> +0x20  mFaceNormals[1]    = V1   } the GPTriangle vertex
    //   0x82925AB8  stvx128 -> +0x30  mFaceNormals[2]    = V2   } aliasing
    //   0x82925C58  stvx128 -> +0x40  mEdgeDirections[0] = (V2-V0)/L0
    //   0x82925C68  stvx128 -> +0x50  mEdgeDirections[1] = (V1-V2)/L1
    //   0x82925C70  stvx128 -> +0x60  mEdgeDirections[2] = (V0-V1)/L2
    //   0x82925C9C  stvx128 -> +0x70  mDimensions        = (L0, L1, L2, L0)
    //   0x82925A9C  stfs    -> +0x80  mFatness           = flt_82001CC0 == 0.0f
    //   0x82925A64  stw     -> +0x84  mVolumeTag         = 0
    //   0x82925A74  stw     -> +0x88  mUserTag           = 0
    //   0x82925A54  stb     -> +0x8C  mNumFaceNormals    = 1
    //   0x82925A5C  stb     -> +0x8D  mNumEdgeDirections = 3
    //   0x82925A4C  stw     -> +0x90  mVolumeType        = 3 (GPInstance::TRIANGLE)
    //   0x82925ACC  stw     -> +0x94  mFlags             = 0x1F0
    //   0x82925AC0/AD4/AE4 stfs -> +0x98/+0x9C/+0xA0  mEdgeData[0..2] = the AOS edge cosines
    //   0x82925CA4..CB4  four stw -> +0xA4  mMethods = off_82F91920, i.e. the TRIANGLE row of
    //                    unk_82F918F0 == rw::collision::g_aGPVolumeMethods[TRIANGLE]
    //
    // ⚠️ THE EDGE TRIPLE'S ORDER AND DIRECTIONS ARE ASM-ATTESTED, NOT CONVENTIONAL:
    // e0 = V2-V0, e1 = V1-V2, e2 = V0-V1, and mDimensions lane w repeats L0 (the console
    // gathers the three lengths through the perm control unk_82CDA350, dumped this wave as
    // 00 01 02 03 | 14 15 16 17 | 00 01 02 03 | 00 01 02 03 == (vA.x, vB.y, vA.x, vA.x), then
    // a `vrlimi128 ...,2,0` overwrites lane z). Swapping any two of them would still compile
    // and would still produce contacts -- with the wrong edge convexity flags applied.
    //
    // ⚠️ NO DEGENERATE GUARD. The console divides by a zero edge length here exactly as
    // TriangleVolume::CreateGPInstance does (vrsqrtefp(0) == +inf); none is invented.
    // The console's `vrsqrtefp` + 1 Newton-Raphson round and `vrefp` + 2 rounds lower to
    // std::sqrt and a divide, which is this family's standing precedent (CgsTriangle4.cpp,
    // FeatureEdge.cpp, TriangleVolume_wN_01.cpp) and is strictly more accurate.
    //
    // FOLLOW-UP (not this owner's file): its real home is
    // vendor/renderware/collision/GPInstance.hpp as `GPTriangle::Initialize`, beside the three
    // GPTriangle callbacks -- the canonical header declares exactly this signature's 11-argument
    // form. It sits here only because this cluster owns no vendor file.
    // ---------------------------------------------------------------------------------------
    void BuildGPTriangleInstance(rw::collision::GPInstance&                  arInst,
                                 const CgsGeometric::Triangle4::AOSTriangle& arTriangle)
    {
        using rw::collision::GPInstance;
        using rw::collision::Vec4;

        const Vec4 lvP0 = ToVec4(arTriangle.mVertex0);
        const Vec4 lvP1 = ToVec4(arTriangle.mVertex1);
        const Vec4 lvP2 = ToVec4(arTriangle.mVertex2);

        const Vec4 lvE0 = SubVec4(lvP2, lvP0);
        const Vec4 lvE1 = SubVec4(lvP1, lvP2);
        const Vec4 lvE2 = SubVec4(lvP0, lvP1);

        arInst.mPos               = lvP0;
        arInst.mFaceNormals[0]    = ToVec4(arTriangle.mNormal);
        arInst.mFaceNormals[1]    = lvP1;
        arInst.mFaceNormals[2]    = lvP2;

        arInst.mFatness           = KF_GP_TRIANGLE_FATNESS;
        arInst.mVolumeTag         = 0;
        arInst.mUserTag           = 0;
        arInst.mNumFaceNormals    = 1;
        arInst.mNumEdgeDirections = 3;
        arInst.mVolumeType        = GPInstance::TRIANGLE;

        // 0x1F0 == FLAG_TRIANGLEDEFAULT (use-edge-cos + the three edge-convex bits) plus
        // FLAG_TRIANGLEONESIDED -- the console's `li r24, 0x1F0`, hoisted out of all four lanes.
        arInst.mFlags = static_cast<u32>(GPInstance::FLAG_TRIANGLEDEFAULT
                                       | GPInstance::FLAG_TRIANGLEONESIDED);

        arInst.mEdgeData[0]       = arTriangle.mfEdgeCosine0;
        arInst.mEdgeData[1]       = arTriangle.mfEdgeCosine1;
        arInst.mEdgeData[2]       = arTriangle.mfEdgeCosine2;

        const f32 lfLength0 = std::sqrt(Dot3(lvE0, lvE0));
        const f32 lfLength1 = std::sqrt(Dot3(lvE1, lvE1));
        const f32 lfLength2 = std::sqrt(Dot3(lvE2, lvE2));

        arInst.mDimensions.x = lfLength0;
        arInst.mDimensions.y = lfLength1;
        arInst.mDimensions.z = lfLength2;
        arInst.mDimensions.w = lfLength0;   // the perm control's fourth lane, measured

        arInst.mEdgeDirections[0] = ScaleVec4(lvE0, 1.0f / lfLength0);
        arInst.mEdgeDirections[1] = ScaleVec4(lvE1, 1.0f / lfLength1);
        arInst.mEdgeDirections[2] = ScaleVec4(lvE2, 1.0f / lfLength2);

        arInst.mMethods = rw::collision::g_aGPVolumeMethods[GPInstance::TRIANGLE];
    }

    // ---------------------------------------------------------------------------------------
    // The pair-list header KIND that ExecutePrimitivePairList @0x82925798 requires of every
    // record it walks. The console compares the cached header's mu8HeaderType against the
    // literal 1 (0x82925820 `cmplwi cr6, r11, 1`) and its assert text spells the enumerator:
    //     "mCurrentHeader.mu8HeaderType == E_LIST_TYPE_PRIMATIVE_PAIR"
    //     ..\..\GameClasses\SceneManager\Collision\Primitives\CgsPrimitivePairList.h:107
    // ⚠️ NAMED HERE, NOT WHERE IT BELONGS. E_LIST_TYPE_* is a CgsPrimitivePairList.h enum in the
    // console (the same header the assert's file string names) and that file is NOT this
    // cluster's to edit; the value is a one-line file-local constant here and "add the
    // E_LIST_TYPE_* enum beside PrimitivePairList::EVolumeType, then use it here" is a reported
    // follow-up. The VALUE is measured, not assumed -- it is the `cmplwi` immediate.
    // ---------------------------------------------------------------------------------------
    const u8 KU8_LIST_TYPE_PRIMATIVE_PAIR = 1;
}

// -------------------------------------------------------------------------------------------
// ContactGeneratorJob::AllocateMemory @0x829212A0 (54)
//
// A bump pointer inside the job object -- the name says "Allocate" but nothing is allocated.
//   v5  = ~(align - 1)                                 ; 0x829212BC  not r30, r11
//   v7  = (size   + align - 1) & v5                    ; the aligned SIZE
//   v10 = (cursor + align - 1) & v5                    ; the aligned CURSOR
//   if (v10 + v7 >= 0x10000) assert "Trying to use too much memory"   ; :1720
//   cursor = v7 + v10
//   return this + 32 + v10
// ⚠️ The overflow test is `>=`, and it is evaluated BEFORE the cursor moves -- reproduce both or
// the tripwire fires one allocation late.
// -------------------------------------------------------------------------------------------
void* ContactGeneratorJob::AllocateMemory(s32 liNumBytes, s32 liAlignment)
{
    const s32 liMask         = ~(liAlignment - 1);
    const s32 liAlignedSize  = (liNumBytes + liAlignment - 1) & liMask;
    const s32 liAlignedStart = (miAllocCursor + liAlignment - 1) & liMask;

    CGS_ASSERT(liAlignedStart + liAlignedSize < KI_ARENA_BYTES,
               "Trying to use too much memory");                       // :1720

    miAllocCursor = liAlignedStart + liAlignedSize;

    return &maArena[liAlignedStart];
}

// -------------------------------------------------------------------------------------------
// ContactGeneratorJob::RestoreMemory @0x82921050 (39)
// Pop the bump cursor back to the open scope's restore point. The -1 sentinel means "no scope
// is open", and the console asserts on it (ContactGeneratorJob.h:167) rather than skipping.
// -------------------------------------------------------------------------------------------
void ContactGeneratorJob::RestoreMemory()
{
    CGS_ASSERT(miMemoryRestorePoint != -1, "miMemoryRestorePoint != -1");   // h:167

    miAllocCursor = miMemoryRestorePoint;
}

// -------------------------------------------------------------------------------------------
// ContactGeneratorJob::Execute @0x829267E0 (77)
//
//   a1[4]     = a2      -> mpJobDescription      ; 0x829267F8  stw r4, 0x10(r3)
//   a1[16544] = 0       -> miAllocCursor         ; 0x10280
//   a1[16545] = -1      -> miMemoryRestorePoint  ; 0x10284
//   switch (*(a2 + 255))                          ; 0x82926818  lbz r11, 0xFF(r4)
//     the console subtracts 5 and does a 12-entry `bctr` (0x8292681C / 0x8292683C), so the arms
//     are types 5..16 with 15 absent -- exactly the table below.
//   default: assert "Unsupported collision job"   ; :135
// -------------------------------------------------------------------------------------------
void ContactGeneratorJob::Execute(void* lpvJobData)
{
    mpJobDescription     = static_cast<const CollisionJobDescription*>(lpvJobData);
    miAllocCursor        = 0;
    miMemoryRestorePoint = -1;

    switch (mpJobDescription->GetType())
    {
        // Case 5's `bl` leaves r4 = lpvJobData untouched: the non-stream worker takes the
        // descriptor as its parameter (the stream arm passes a stack-local one instead).
        case 5:
            ExecuteSphereListWithTriangleList(
                static_cast<const SphereListWithTriangleListJobDesc*>(mpJobDescription));
            break;
        case 6:  ExecuteSphereListWithTriangleListStream();     break;
        // Case 2 of the jump table (0x82926890). Like cases 5/11/13 the `bl` leaves r4 ==
        // lpvJobData untouched, and unlike the no-arg arms this worker READS it -- 0x829215C4
        // `mr r18, r4` is its fourth instruction and every descriptor field it touches comes
        // through r18. (Corrected with the body, wave Q7 cluster ss; this case used to call a
        // no-arg gate.)
        case 7:
            ExecuteSphereListWithSphereList(
                static_cast<const SphereListWithSphereListJobDesc*>(mpJobDescription));
            break;
        case 8:  ExecuteSphereListWithSphereListStream();       break;
        // Cases 4 and 5 of the jump table (0x829268A0 / 0x829268B8). Both leave r4 == lpvJobData
        // live too, and both bodies IGNORE it: each opens with `mr r31, r3` and re-reads
        // `lwz 0x10(r31)` == mpJobDescription. No-arg is the measured spelling.
        case 9:  ExecuteBoxListWithTriangleList();              break;
        case 10: ExecutePrimitivePairList();                    break;
        // Case 6 of the jump table (0x829268A8): the `bl` leaves r4 == lpvJobData untouched,
        // exactly as cases 5 and 13 do -- this worker takes the descriptor as its parameter.
        case 11:
            ExecutePrimitiveListWithTriangleList(
                static_cast<const PrimitiveListWithTriangleListJobDesc*>(mpJobDescription));
            break;
        case 12: ExecutePrimitiveListWithTriangleListStream();  break;
        // Case 13's `bl` at 0x82926880 leaves r4 = lpvJobData untouched, exactly as case 5's
        // does: the swept worker takes the descriptor as its parameter too.
        case 13:
            ExecuteSweptSphereListWithTriangleList(
                static_cast<const SweptSphereListWithTriangleListJobDesc*>(mpJobDescription));
            break;
        case 14: ExecuteSweptSphereListWithTriangleListStream();break;

        case CgsSceneManager::CgsCollision::E_COLLISIONJOB_LINE_WITH_TRIANGLE_LIST_STREAM:
            ExecuteLineWithTriangleListStream();
            break;

        default:
            CGS_ASSERT(false, "Unsupported collision job");     // :135
            break;
    }
}

// =============================================================================================
// ContactGeneratorJob::ExecuteLineWithTriangleListStream @0x82921968 (589)  ⭐⭐⭐ THE TRACTION
// LINE TEST -- the function that finally reads the triangle cache.
//
// SHAPE: drain the descriptor's command stream. Per 176-byte command: for each Triangle4 block
// (4 triangles, SoA) compute the four unit face normals once, then for each of the command's
// miNumLines segments run Moller-Trumbore against all four triangles and keep the nearest
// accepted hit. Then write one 192-byte result -- position, normal, surface tag and a hit flag
// per line -- and post it back at the command's own slot index.
//
//   0x82921988  lwz r28, 0x10(this)          -> mpJobDescription      (:1151 "No job description\n")
//   0x829219A8  lwz ...,  0x00(desc)         -> mpStreamProducer      (:1152 "No stream producer\n")
//   0x82921A38  SimpleDataStreamConsumer::Construct(&consumer, producer, 0, 0)
//   0x82921A64  AllocateMemory(256, 256)     -> the command scratch
//   0x82921A78  AllocateMemory(256, 256)     -> the result scratch
//   0x82921AB4  stw r11, 0(r25)              -> miMemoryRestorePoint = miAllocCursor
//   loop:       DataStreamCommandReader::ReadCom(&consumer.mReader, command, &index)
//   0x82922204  SimpleDataStreamConsumer::AddResult(&consumer, result, index)
//   0x82922274  stw r11, 0(r27)              -> RestoreMemory()
//
// ⚠️ BOTH SCRATCH RECORDS ARE A WHOLE 256-BYTE ARENA SLICE, as the console asks
// (`li r4, 0x100 / li r5, 0x100` twice) -- NOT sizeof(record). That is not slack: the producer's
// aligned result stride is (192 + 127) & ~127 == 256, and SimpleDataStreamConsumer::AddResult
// copies a WHOLE STRIDE out of this buffer. A 192-byte scratch would have AddResult read 64
// bytes past its end, every command.
//
// ⚠️ THE RESULT IS CLEARED FOR EVERY LINE BEFORE THE HIT TEST, MISS INCLUDED (0x82922144..
// 0x82922190). Skipping the clear would leave the previous command's answer in place and hand a
// stale road surface to AddTractionPoint -- a textbook silent-drop. Kept unconditional.
// =============================================================================================
void ContactGeneratorJob::ExecuteLineWithTriangleListStream()
{
    typedef LineWithTriangleListStreamJobDesc Desc;

    const Desc* lpDesc = static_cast<const Desc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                      // :1151
    CGS_ASSERT(lpDesc->GetStreamProducer() != NULL, "No stream producer\n"); // :1152

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetStreamProducer(), NULL, 0);

    Desc::StreamCommand* lpCommand =
        static_cast<Desc::StreamCommand*>(AllocateMemory(256, 256));
    Desc::StreamResult* lpResult =
        static_cast<Desc::StreamResult*>(AllocateMemory(256, 256));

    // 0x82921AA8..0x82921AB4: the scope opens AFTER both scratch slices are taken, so the
    // per-command restore returns to here and not to zero.
    miMemoryRestorePoint = miAllocCursor;

    u32 luCommandIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luCommandIndex) == 0)
    {
        const s32 liNumBatches  = lpCommand->miNumTriangleBatches;    // command +0xA4 (BLOCKS of 4)
        const CgsGeometric::Triangle4* lpaTriangles = lpCommand->mpTriangles;

        // ⭐ PC SAFETY GATE -- NOT THE CONSOLE'S. The console indexes five-slot STACK arrays with
        // miNumLines and does not bound it; a corrupt command would smash its own frame. The
        // three shipped producers write 4 (AddRaceCarTractionLineTests @0x825E9640,
        // AddPlayerStuckInCollisionLineTests @0x825E9B28) and 5
        // (AddTrafficTractionLineTests @0x8261D580), so this can only fire on corruption -- but
        // an unbounded write into a fixed array on a live path is a memory bug whatever the
        // console does, and this project has retired seven of those.
        s32 liNumLines = lpCommand->miNumLines;                       // command +0xA8
        CGS_ASSERT(liNumLines <= Desc::KI_MAX_LINES_PER_COMMAND,
                   "traction command miNumLines exceeds the record's five slots");
        if (liNumLines > Desc::KI_MAX_LINES_PER_COMMAND)
        {
            liNumLines = Desc::KI_MAX_LINES_PER_COMMAND;
        }

        // ---- per-line running state (the console's four stack arrays, five slots each) ------
        // 0x82921B30 v87   : byte-per-line hit flag, cleared to 0
        // 0x82921B98 v99   : nearest `t` so far, splatted to FLT_MAX
        // 0x82921B48 v102/3: nearest normal, cleared to 0
        // 0x82921B70 v100/1: nearest surface tag, cleared to 0 (a whole vector on the console
        //                    because the cascade selects it with vsel; only lane 0 is ever read)
        u8      lau8Hit[Desc::KI_MAX_LINES_PER_COMMAND];
        f32     lafBestT[Desc::KI_MAX_LINES_PER_COMMAND];
        Vector4 laBestNormal[Desc::KI_MAX_LINES_PER_COMMAND];
        u32     lauBestTag[Desc::KI_MAX_LINES_PER_COMMAND];

        for (s32 liLine = 0; liLine < liNumLines; ++liLine)
        {
            lau8Hit[liLine]    = 0;
            lafBestT[liLine]   = KF_NO_HIT_DISTANCE;
            lauBestTag[liLine] = 0;
            laBestNormal[liLine].SetZero();
        }

        for (s32 liBatch = 0; liBatch < liNumBatches; ++liBatch)
        {
            const CgsGeometric::Triangle4& lrBlock = lpaTriangles[liBatch];

            // ---- the four unit face normals, once per block (0x82921CC4..0x82921DFC) --------
            // The console transposes the SoA rows into four AoS vertices first, then for each
            // triangle:
            //   a = P0 - P1 ; b = P0 - P2
            //   c = perm(a*perm(b,yzxw) - perm(a,yzxw)*b, yzxw)   == cross(a, b)
            //   N = c * rsqrt(dot4(c,c))                          vrsqrtefp + TWO NR steps
            // `perm ..., 0x63` was decoded from the raw instruction word (0x19834AD0 -> imm7
            // 0x63 == lanes (1,2,0,3) == yzxw), NOT from IDA's rendering, because IDA prints
            // VMX128 source registers 32 too high. cross(P0-P1, P0-P2) == cross(P1-P0, P2-P0)
            // == cross(e1, e2), i.e. the front face is the one with det > 0.
            //
            // ⚠️ PC LOWERING, FLAGGED: `vrsqrtefp` + 2 NR is a ~23-bit reciprocal square root;
            // `1/sqrt()` below is exact to the last ulp. Same precedent, same reason, and same
            // wording as CgsPolygonSoupTests.cpp:465.
            // ⚠️ The console's dot is `vmsum4fp128`, a FOUR-component dot. The w lane of `c` is
            // `a.w*b.y - a.y*b.w` with a.w == b.w == 0 (both are differences of vertices whose
            // w lane the transpose set to 1.0f), so it contributes nothing; the three-component
            // dot below is the same number.
            Vector4 laNormal[4];
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                const f32 lfAx = (&lrBlock.mVertex0X.x)[liLane] - (&lrBlock.mVertex1X.x)[liLane];
                const f32 lfAy = (&lrBlock.mVertex0Y.x)[liLane] - (&lrBlock.mVertex1Y.x)[liLane];
                const f32 lfAz = (&lrBlock.mVertex0Z.x)[liLane] - (&lrBlock.mVertex1Z.x)[liLane];
                const f32 lfBx = (&lrBlock.mVertex0X.x)[liLane] - (&lrBlock.mVertex2X.x)[liLane];
                const f32 lfBy = (&lrBlock.mVertex0Y.x)[liLane] - (&lrBlock.mVertex2Y.x)[liLane];
                const f32 lfBz = (&lrBlock.mVertex0Z.x)[liLane] - (&lrBlock.mVertex2Z.x)[liLane];

                const f32 lfCx = (lfAy * lfBz) - (lfAz * lfBy);
                const f32 lfCy = (lfAz * lfBx) - (lfAx * lfBz);
                const f32 lfCz = (lfAx * lfBy) - (lfAy * lfBx);

                const f32 lfLenSq = (lfCx * lfCx) + (lfCy * lfCy) + (lfCz * lfCz);
                const f32 lfInvLen = 1.0f / std::sqrt(lfLenSq);   // vrsqrtefp + 2 NR

                laNormal[liLane].x = lfCx * lfInvLen;
                laNormal[liLane].y = lfCy * lfInvLen;
                laNormal[liLane].z = lfCz * lfInvLen;
                laNormal[liLane].w = 0.0f;
            }

            for (s32 liLine = 0; liLine < liNumLines; ++liLine)
            {
                const Vector4& lrStart = lpCommand->maLineStart[liLine];
                const Vector4& lrEnd   = lpCommand->maLineEnd[liLine];

                const f32 lfDx = lrEnd.x - lrStart.x;   // 0x82921E88  vsubfp v13, v13, v12
                const f32 lfDy = lrEnd.y - lrStart.y;
                const f32 lfDz = lrEnd.z - lrStart.z;

                // The console runs all four lanes then folds; here each lane is one iteration.
                // ⭐ The console's scalar early-out at 0x82921F8C (four vspltw + vcmpeqfp. +
                // mfocrf, "is any lane's mask non-zero") is a pure optimisation that changes no
                // result -- per-lane iteration subsumes it exactly.
                for (s32 liLane = 0; liLane < 4; ++liLane)
                {
                    const f32 lfP0x = (&lrBlock.mVertex0X.x)[liLane];
                    const f32 lfP0y = (&lrBlock.mVertex0Y.x)[liLane];
                    const f32 lfP0z = (&lrBlock.mVertex0Z.x)[liLane];

                    // e1 = P1 - P0, e2 = P2 - P0  (0x82921E54..0x82921E90)
                    const f32 lfE1x = (&lrBlock.mVertex1X.x)[liLane] - lfP0x;
                    const f32 lfE1y = (&lrBlock.mVertex1Y.x)[liLane] - lfP0y;
                    const f32 lfE1z = (&lrBlock.mVertex1Z.x)[liLane] - lfP0z;
                    const f32 lfE2x = (&lrBlock.mVertex2X.x)[liLane] - lfP0x;
                    const f32 lfE2y = (&lrBlock.mVertex2Y.x)[liLane] - lfP0y;
                    const f32 lfE2z = (&lrBlock.mVertex2Z.x)[liLane] - lfP0z;

                    // P = cross(dir, e2)   (0x82921EA0..0x82921EC0)
                    const f32 lfPx = (lfE2z * lfDy) - (lfE2y * lfDz);
                    const f32 lfPy = (lfE2x * lfDz) - (lfE2z * lfDx);
                    const f32 lfPz = (lfE2y * lfDx) - (lfE2x * lfDy);

                    // T = lineStart - P0   (0x82921ED0..0x82921ED8)
                    const f32 lfTx = lrStart.x - lfP0x;
                    const f32 lfTy = lrStart.y - lfP0y;
                    const f32 lfTz = lrStart.z - lfP0z;

                    // det = dot(e1, P)     (0x82921EDC / 0x82921EF0 / 0x82921F00)
                    const f32 lfDet = (lfE1x * lfPx) + (lfE1y * lfPy) + (lfE1z * lfPz);
                    // u   = dot(T,  P)     -- UNNORMALISED; the real barycentric is u/det
                    const f32 lfU   = (lfTx * lfPx) + (lfTy * lfPy) + (lfTz * lfPz);

                    // Q = cross(T, e1)     (0x82921EE0..0x82921F18)
                    const f32 lfQx = (lfTy * lfE1z) - (lfTz * lfE1y);
                    const f32 lfQy = (lfTz * lfE1x) - (lfTx * lfE1z);
                    const f32 lfQz = (lfTx * lfE1y) - (lfTy * lfE1x);

                    // v    = dot(Q, dir)   (0x82921F1C / F28 / F38)
                    const f32 lfV    = (lfQx * lfDx) + (lfQy * lfDy) + (lfQz * lfDz);
                    // tnum = dot(e2, Q)    (0x82921F20 / F2C / F58)
                    const f32 lfTNum = (lfE2x * lfQx) + (lfE2y * lfQy) + (lfE2z * lfQz);

                    // The acceptance test, in the console's own shape (0x82921F14..0x82921F88).
                    // `lo` and `hi` are written out rather than folded to 0/det so that the two
                    // tolerances above stay visible and substitutable.
                    //   lo = (-det) * eps1 ; hi = det - lo
                    // ⚠️ Every upper bound is `!(x > hi)`, NOT `x <= hi`. `vcmpgtfp` is false for
                    // a NaN and `vnot` turns that into true, so a NaN operand is ACCEPTED by the
                    // upper bounds and rejected by the lower ones -- writing `<=` would silently
                    // change NaN handling. Reproduced as shipped.
                    const f32 lfLo = (-lfDet) * KF_BARYCENTRIC_TOLERANCE;
                    const f32 lfHi = lfDet - lfLo;

                    const bool lbAccept =
                        (lfDet > KF_MIN_DETERMINANT)
                        && (lfU >= lfLo)    && !(lfU > lfHi)
                        && (lfV >= lfLo)    && !((lfU + lfV) > lfHi)
                        && (lfTNum >= lfLo) && !(lfTNum > lfHi)
                        && IsLaneEnabled(lrBlock.mValidMasks, liLane);

                    if (!lbAccept)
                    {
                        continue;
                    }

                    // ⚠️ THE LINE IS FLAGGED AS HIT HERE, BEFORE THE NEAREST-SELECT -- exactly
                    // where the console sets it (`v87[v41] = 1` at 0x82922038, inside the
                    // "any lane's mask is non-zero" block and ABOVE the vsel cascade). An
                    // accepted lane that then loses the distance race still counts as a hit.
                    lau8Hit[liLine] = 1;

                    // t = tnum / det  (0x8292200C vrefp + TWO NR steps + 0x82922054 vmulfp).
                    // ⚠️ PC LOWERING, FLAGGED: a divide, for the same reason and with the same
                    // precedent as the rsqrt above. `det > 0` is already established here, so
                    // the divisor cannot be zero on this path.
                    // ⚠️ `tnum <= det` above clamped t to 1: THE TRACTION LINE IS A SEGMENT,
                    // NOT A RAY. A wheel does not find ground beyond the end of its probe.
                    const f32 lfT = lfTNum / lfDet;

                    // The nearest-select (0x82922060..0x82922110). The console runs a strictly
                    // ordered lane 0 -> lane 3 cascade of `vsel`s whose predicate is
                    // `!(t[k] >= best) && mask[k]`.
                    // ⚠️ THE COMPARISON IS STRICT AND THE ORDER MATTERS: on an exact tie the
                    // LOWER LANE INDEX wins, i.e. the earlier triangle in the block. Iterating
                    // lanes 0..3 with `!(t >= best)` reproduces that exactly; `t < best` would
                    // too, but `!(>=)` also keeps the console's NaN behaviour.
                    if (!(lfT >= lafBestT[liLine]))
                    {
                        lafBestT[liLine]     = lfT;
                        laBestNormal[liLine] = laNormal[liLane];
                        lauBestTag[liLine]   = LaneBits(lrBlock.mSurfaceTags, liLane);
                    }
                }
            }
        }

        // ---- write-out (0x82922144..0x82922204) ---------------------------------------------
        for (s32 liLine = 0; liLine < liNumLines; ++liLine)
        {
            lpResult->maHitPosition[liLine].SetZero();
            lpResult->maHitNormal[liLine].SetZero();
            lpResult->mauSurfaceTag[liLine] = 0;
            lpResult->mabHit[liLine]        = lau8Hit[liLine];

            if (lau8Hit[liLine] != 0)
            {
                // ⭐ The hit POSITION is recomputed from start + dir*t rather than carried
                // through the search, which is why `t` is the only thing the nearest-select has
                // to keep (0x829221C8 vsubfp + 0x829221D4 vmaddfp). All four lanes, w included:
                // the console's vmaddfp is a full 16-byte operation and the command's w lane is
                // whatever the producer wrote.
                const Vector4& lrStart = lpCommand->maLineStart[liLine];
                const Vector4& lrEnd   = lpCommand->maLineEnd[liLine];
                const f32      lfT     = lafBestT[liLine];

                lpResult->maHitPosition[liLine].x = ((lrEnd.x - lrStart.x) * lfT) + lrStart.x;
                lpResult->maHitPosition[liLine].y = ((lrEnd.y - lrStart.y) * lfT) + lrStart.y;
                lpResult->maHitPosition[liLine].z = ((lrEnd.z - lrStart.z) * lfT) + lrStart.z;
                lpResult->maHitPosition[liLine].w = ((lrEnd.w - lrStart.w) * lfT) + lrStart.w;

                lpResult->maHitNormal[liLine]   = laBestNormal[liLine];
                lpResult->mauSurfaceTag[liLine] = lauBestTag[liLine];
            }
        }

        lConsumer.AddResult(lpResult, static_cast<s32>(luCommandIndex));

        RestoreMemory();
    }

    lConsumer.Destruct();
}

// =============================================================================================
// ⭐⭐⭐ THE SPHERE CONTACT ARMS (walls leg 2, 2026-08-14) -- the first code in this tree that
// PRODUCES world contacts. Chain: DoRaceCarWorldContactGeneration posts one 32-byte (host 48)
// command per live car per frame -> RunCollideSphereListWithTriangleListStream wires a batch at
// ContactGeneratorEntry with desc type 6 -> Execute case 6 -> the stream arm below -> per
// command the non-stream worker -> IntersectTriangle4Sphere_HackyBurnoutVersion per
// (Triangle4 batch x sensor sphere) -> one 80-byte PrimitiveTestResult per hit lane into the
// command's CollisionResultList. The list is harvested by EndVehicleContactGeneration ->
// AddContactResultsToQueue (REAL as of walls leg 3, 2026-08-14).
// =============================================================================================

// ---------------------------------------------------------------------------------------------
// ContactGeneratorJob::LoadPrimitives @0x829210F0 (61)
// The SPU build's "DMA the triangle list down" -- on X360 (and here) a plain header copy.
// Both asserts kept verbatim (:1405/:1406).
// ---------------------------------------------------------------------------------------------
void ContactGeneratorJob::LoadPrimitives(const TriangleList* lpSourceTriangleList,
                                         TriangleList*       lpDestinationTriangleList)
{
    CGS_ASSERT(lpSourceTriangleList != NULL, "lpSourceTriangleList != NULL");           // :1405
    CGS_ASSERT(lpDestinationTriangleList != NULL, "lpDestinationTriangleList != NULL"); // :1406

    *lpDestinationTriangleList = *lpSourceTriangleList;   // {base, count}, 2 dwords on console
}

// ---------------------------------------------------------------------------------------------
// ContactGeneratorJob::LoadResultList @0x829211E8 (46)
// Copy the 16-byte CollisionResultList header; assert the copied results pointer (:1552 --
// the console checks the DESTINATION after the first word lands, hence the message).
// ---------------------------------------------------------------------------------------------
void ContactGeneratorJob::LoadResultList(const CollisionResultList* lpSourceResultList,
                                         CollisionResultList*       lpDestinationResultList)
{
    *lpDestinationResultList = *lpSourceResultList;

    CGS_ASSERT(lpDestinationResultList->mpResults != NULL,
               "lpDestinationResultList->GetResultsMemory() != NULL");                  // :1552
}

// =============================================================================================
// ContactGeneratorJob::ExecuteSphereListWithTriangleList @0x829226A8 (967) ⭐⭐⭐ THE CONTACT
// WORKER -- runs the narrow-phase kernel and queues the results.
//
// SHAPE (all offsets read from the asm, not the pseudocode -- IDA typed the u16 header loads
// as words; `lhz r25, numResults` / `lhz r11, maxNumResults` at 0x82922730/0x829227B8 settle
// them):
//   ld/std the descriptor's SphereList; LoadPrimitives(&desc->mTriangleList, local);
//   LoadResultList(desc->mpResultsList, localHeader); splat desc->mfRadius (the padding);
//   for each Triangle4 batch (stride 0xE0, triangle index base r15 = 2 so lane L is 4*batch+L):
//     for each sphere (stride 0x10, index r22 resets per batch, base reloaded per batch):
//       assert numResults < maxNumResults (:182, per iteration, BEFORE the kernel);
//       mask = IntersectTriangle4Sphere_HackyBurnoutVersion(sphere, batch, padding, 4 groups);
//       for each hit lane (vspltw + vcmpeqfp vs zero):
//         tail-fill the lane's 80-byte record: muPrimitive0Tag = batch's surface tag lane
//         (stw), muPrimitive1Tag = 0 (stw), muPrimitive0Index = 4*batch+lane (sth),
//         muPrimitive1Index = sphere index (sth);
//         copy the record to results[numResults] (10 ld/std = 80 bytes);
//         numResults = min(numResults + 1, max - 1); localHeader.mu16NumResults = numResults;
//         if (!record.IsValid()) -> the console builds a 0x2000-byte StrStream dump
//         ("Invalid normal generated: \n" + sphere/normal/triangle points via GetAOSTriangle)
//         and PrintStringed's it (:233/:273/:313/:353 per lane). Lowered to CGS_ASSERT with
//         the console's own message; the dump scaffolding (StrStream + GetAOSTriangle) is
//         dev-assert-path only and is NOT reconstructed here.
//   write the 16-byte local header back to desc->mpResultsList (unconditional, 0x8292358C).
//
// ⚠️ mu16TestIndex/muPad are NOT written by the console (stack garbage travels into the queued
// copy); zero-initialised here once per record instead -- deterministic, flagged.
// ⚠️ The results array is the meResultType==0 / 80-byte-stride carve
// (PrepareNewPrimitiveTestResultsList Mallocs 80*max), so the base pointer is reinterpreted as
// PrimitiveTestResult[] -- CollisionResult's 112-byte stride belongs to the OTHER record type.
// =============================================================================================
void ContactGeneratorJob::ExecuteSphereListWithTriangleList(
    const SphereListWithTriangleListJobDesc* lpDesc)
{
    using CgsGeometric::Sphere;
    using CgsGeometric::Triangle4;

    const CgsSceneManager::CgsCollision::SphereList lSpheres = lpDesc->mSphereList;

    TriangleList lTriangles;
    LoadPrimitives(&lpDesc->mTriangleList, &lTriangles);

    CollisionResultList lHeader;
    LoadResultList(lpDesc->mpResultsList, &lHeader);

    // stfs f0 -> vspltw128 v124: the padding reach, broadcast.
    VecFloat lPadding;
    lPadding.x = lPadding.y = lPadding.z = lPadding.w = lpDesc->mfRadius;

    PrimitiveTestResult* lpaResults =
        reinterpret_cast<PrimitiveTestResult*>(lHeader.mpResults);

    u16 lu16NumResults = lHeader.mu16NumResults;
    const u16 lu16MaxResults = lHeader.mu16MaxNumResults;

    for (s32 liBatch = 0; liBatch < lTriangles.miNumTriangles; ++liBatch)
    {
        const Triangle4& lrBlock = lTriangles.mpTriangles[liBatch];

        const Sphere* lpSphere =
            reinterpret_cast<const Sphere*>(lSpheres.mpSpheres);   // base reloaded per batch

        for (s32 liSphere = 0; liSphere < lSpheres.miNumSpheres; ++liSphere, ++lpSphere)
        {
            CGS_ASSERT(lu16NumResults < lu16MaxResults,
                       "lResultsList.GetNumResults() < lResultsList.GetMaxNumResults()"); // :182

            // The four per-lane result records the kernel writes its groups into. The
            // console carves these on its stack; the tail fields are filled only for
            // lanes that hit.
            PrimitiveTestResult laRecord[4] = {};

            const Triangle4::Mask4 lHitMask =
                CgsGeometric::IntersectTriangle4Sphere_HackyBurnoutVersion(
                    *lpSphere, lrBlock, lPadding,
                    // group per lane: ContactNormal(+0x10 mPrimitive1Normal),
                    // TriangleNormal(+0x00 mPrimitive0Normal), SphereContactPoint
                    // (+0x30 mPrimitive1Contact), TriangleContactPoint(+0x20
                    // mPrimitive0Contact) -- offsets from the X360 call site.
                    laRecord[0].mPrimitive1Normal, laRecord[0].mPrimitive0Normal,
                    laRecord[0].mPrimitive1Contact, laRecord[0].mPrimitive0Contact,
                    laRecord[1].mPrimitive1Normal, laRecord[1].mPrimitive0Normal,
                    laRecord[1].mPrimitive1Contact, laRecord[1].mPrimitive0Contact,
                    laRecord[2].mPrimitive1Normal, laRecord[2].mPrimitive0Normal,
                    laRecord[2].mPrimitive1Contact, laRecord[2].mPrimitive0Contact,
                    laRecord[3].mPrimitive1Normal, laRecord[3].mPrimitive0Normal,
                    laRecord[3].mPrimitive1Contact, laRecord[3].mPrimitive0Contact);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                // vspltw v0, mask, lane + vcmpeqfp vs zero: a zero lane is a miss.
                if (LaneBits(lHitMask, liLane) == 0u)
                {
                    continue;
                }

                PrimitiveTestResult& lrRecord = laRecord[liLane];
                lrRecord.muPrimitive0Tag   = LaneBits(lrBlock.mSurfaceTags, liLane);
                lrRecord.muPrimitive1Tag   = 0;
                lrRecord.muPrimitive0Index = static_cast<u16>((4 * liBatch) + liLane);
                lrRecord.muPrimitive1Index = static_cast<u16>(liSphere);

                lpaResults[lu16NumResults] = lrRecord;   // 80-byte copy at 80*index

                // idx+1, clamped to max-1: an overflowing list overwrites its last slot
                // (the assert above already fired once per overflowing iteration).
                u16 lu16Next = static_cast<u16>(lu16NumResults + 1);
                if (lu16Next >= lu16MaxResults)
                {
                    lu16Next = static_cast<u16>(lu16MaxResults - 1);
                }
                lu16NumResults = lu16Next;
                lHeader.mu16NumResults = lu16NumResults;

                CGS_ASSERT(lrRecord.IsValid(), "Invalid normal generated: \n"); // :233/:273/:313/:353
            }
        }
    }

    // The unconditional 16-byte header write-back (0x8292358C..0x829235B0) -- this is what
    // publishes mu16NumResults to the harvest.
    *lpDesc->mpResultsList = lHeader;

    // ⭐ THE WITNESS (PC boot gate mechanism, log-once): the first time this worker leaves a
    // non-empty result list, say so with the count. Not the console's; deleted when the
    // harvest (EndVehicleContactGeneration -- REAL as of walls leg 3) makes contacts visible downstream.
    if (lu16NumResults > 0)
    {
        static bool s_bLoggedFirstContacts = false;
        if (!s_bLoggedFirstContacts)
        {
            s_bLoggedFirstContacts = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "⭐ sphere-vs-triangle CONTACTS LIVE: "
                    << static_cast<s32>(lu16NumResults)
                    << " PrimitiveTestResult(s) in the command's result list "
                       "[FLAG PC boot witness]. Reported once, not per frame\n";
        }
    }
}

// =============================================================================================
// ContactGeneratorJob::ExecuteSphereListWithTriangleListStream @0x829235C8 (100)
// Drain the command stream; per command, run the non-stream worker on a stack-local
// descriptor. NO AddResult: the results travel through the command's own CollisionResultList
// (the poster carved it via PrepareNewPrimitiveTestResultsList).
//
//   0x829235E?  asserts :381 "No job description\n" / :382 "No stream producer\n"
//   Construct(consumer, producer, 0, 0); AllocateMemory(128, 128)   -- the command scratch;
//     128 is the producer's stride round (32+127)&~127 on the console AND (48+127)&~127
//     here -- the whole stride is read per command, same doctrine as the line worker's 256.
//   restore point opens AFTER the alloc; per ReadCom command:
//     SphereListWithTriangleListJobDesc::Prepare(local, &cmd->mSphereList, &cmd->mTriList,
//                                                cmd->mpResultList, cmd->mfPadding);
//     ExecuteSphereListWithTriangleList(&local); RestoreMemory();
//   Destruct.
// =============================================================================================
void ContactGeneratorJob::ExecuteSphereListWithTriangleListStream()
{
    typedef SphereListWithTriangleListStreamJobDesc Desc;

    const Desc* lpDesc = static_cast<const Desc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                      // :381
    CGS_ASSERT(lpDesc->GetStreamProducer() != NULL, "No stream producer\n"); // :382

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetStreamProducer(), NULL, 0);

    Desc::StreamCommand* lpCommand =
        static_cast<Desc::StreamCommand*>(AllocateMemory(128, 128));

    miMemoryRestorePoint = miAllocCursor;

    u32 luCommandIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luCommandIndex) == 0)
    {
        SphereListWithTriangleListJobDesc lLocalDesc;
        lLocalDesc.Prepare(&lpCommand->mSphereList, &lpCommand->mTriList,
                           lpCommand->mpResultList, lpCommand->mfPadding);

        ExecuteSphereListWithTriangleList(&lLocalDesc);

        RestoreMemory();
    }

    lConsumer.Destruct();
}

// =============================================================================================
// ⭐⭐⭐ THE SWEPT (CONTINUOUS) CONTACT ARMS — swept leg, 2026-08-16.
//
// Chain: DoRaceCarWorldContactGeneration sees IsUsingSweptSpheres() (forward speed above
// ~6 m/s and not crashing) and posts a SweptSphereList command instead of a SphereList one ->
// RunCollideSweptSphereListWithTriangleListStream @0x828118A8 wires a batch with desc type 14
// -> Execute case 14 -> the stream arm below -> per command the non-stream worker ->
// IntersectTriangle4SweptSphere @0x8283EF50 per (Triangle4 batch x swept sphere) -> one
// 80-byte PrimitiveTestResult per hit lane into the command's CollisionResultList, harvested
// by EndVehicleContactGeneration -> AddContactResultsToQueue exactly as the in-place arm's are.
//
// ⚠️ THIS IS NOT A SECOND ROUTE TO THE IN-PLACE WORKER AND MUST NEVER BECOME ONE. The console
// chooses swept above walking pace precisely because an in-place test tunnels: at 30 m/s a
// sensor sphere moves ~0.5 m per frame, so testing it where it happens to be at the end of the
// step misses every wall it passed through. Routing fast cars down the sphere arm would be an
// invented arm, and it would look like it worked right up until a wall.
// =============================================================================================

// ---------------------------------------------------------------------------------------------
// ContactGeneratorJob::ExecuteSweptSphereListWithTriangleList @0x829238E8
//
// SHAPE — the sphere worker @0x829226A8's twin, with three differences, all read from the asm:
//   * the primitive stride is 0x20, not 0x10 (`slwi r11, r22, 5` at 0x82923A74) — a SweptSphere
//     is two packed Vector3Plus lanes;
//   * the kernel takes NO padding argument (see CgsTriangleSphere.h), so the descriptor's
//     mfRadius is loaded by Prepare and then never read by this worker. That is not an omission
//     here: there is no `lfs` of +0xF4 anywhere in the 1620-instruction body;
//   * the four `IsValid` asserts carry their own line numbers, one per unrolled lane.
//
//   0x82923910  ld  r11, 0(r30)         -> the SweptSphereList {base,count} pair
//   0x82923904  addi r4, r30, 8         -> LoadPrimitives(&desc->mTriangleList, local)
//   0x82923920  lwz r4, 0xF0(r30)       -> LoadResultList(desc->mpResultsList, localHeader)
//   0x829239E4  outer: batch < lTriangles.miNumTriangles   (`lwz r11, var_474`)
//   0x82923A0C  inner: sphere < lSpheres.miNumSpheres      (`lwz r11, var_488+4`)
//   0x82923A1C  assert :497  "lResultsList.GetNumResults() < lResultsList.GetMaxNumResults()"
//   0x82923AEC  bl  CgsGeometric::IntersectTriangle4SweptSphere
//   per hit lane: IsValid assert FIRST (:513/:539/:565/:591), then the four tail fields, then
//   the ten ld/std that copy the 80-byte record to results[numResults], then the clamp.
//   0x829251FC  the unconditional 16-byte header write-back to desc->mpResultsList
//
// ⚠️ THE ORDER IS THE CONSOLE'S, AND IT DIFFERS FROM THE SPHERE WORKER'S: here `IsValid` is
// checked BEFORE the tag/index fields are written and before the copy (0x82923B24 vs
// 0x82923B88). Reproduced as read rather than harmonised with the sibling.
//
// ⚠️ The console builds a 0x2000-byte StrStream dump on the assert path (GetAOSTriangle +
// "Invalid normal generated: \n" + "Sphere direction and length" + AppendFormat x8 per lane).
// That scaffolding is dev-assert-path only and is NOT reconstructed, same call as the sphere
// worker's; the assert keeps the console's own expression text as its message.
// ---------------------------------------------------------------------------------------------
void ContactGeneratorJob::ExecuteSweptSphereListWithTriangleList(
    const SweptSphereListWithTriangleListJobDesc* lpDesc)
{
    using CgsGeometric::SweptSphere;
    using CgsGeometric::Triangle4;

    const CgsSceneManager::CgsCollision::SweptSphereList lSpheres = lpDesc->mSweptSphereList;

    TriangleList lTriangles;
    LoadPrimitives(&lpDesc->mTriangleList, &lTriangles);

    CollisionResultList lHeader;
    LoadResultList(lpDesc->mpResultsList, &lHeader);

    PrimitiveTestResult* lpaResults =
        reinterpret_cast<PrimitiveTestResult*>(lHeader.mpResults);

    u16 lu16NumResults = lHeader.mu16NumResults;
    const u16 lu16MaxResults = lHeader.mu16MaxNumResults;

    for (s32 liBatch = 0; liBatch < lTriangles.miNumTriangles; ++liBatch)
    {
        const Triangle4& lrBlock = lTriangles.mpTriangles[liBatch];

        const SweptSphere* lpSphere =
            reinterpret_cast<const SweptSphere*>(lSpheres.mpaSweptSpheres);

        for (s32 liSphere = 0; liSphere < lSpheres.miNumSpheres; ++liSphere, ++lpSphere)
        {
            CGS_ASSERT(lu16NumResults < lu16MaxResults,
                       "lResultsList.GetNumResults() < lResultsList.GetMaxNumResults()"); // :497

            PrimitiveTestResult laRecord[4] = {};

            const Triangle4::Mask4 lHitMask =
                CgsGeometric::IntersectTriangle4SweptSphere(
                    *lpSphere, lrBlock,
                    // group per lane, same order and same record offsets as the sphere
                    // worker's: ContactNormal(+0x10), TriangleNormal(+0x00),
                    // SphereContactPoint(+0x30), TriangleContactPoint(+0x20) — resolved from
                    // the X360 call site's register/stack argument slots at 0x82923A70.
                    laRecord[0].mPrimitive1Normal, laRecord[0].mPrimitive0Normal,
                    laRecord[0].mPrimitive1Contact, laRecord[0].mPrimitive0Contact,
                    laRecord[1].mPrimitive1Normal, laRecord[1].mPrimitive0Normal,
                    laRecord[1].mPrimitive1Contact, laRecord[1].mPrimitive0Contact,
                    laRecord[2].mPrimitive1Normal, laRecord[2].mPrimitive0Normal,
                    laRecord[2].mPrimitive1Contact, laRecord[2].mPrimitive0Contact,
                    laRecord[3].mPrimitive1Normal, laRecord[3].mPrimitive0Normal,
                    laRecord[3].mPrimitive1Contact, laRecord[3].mPrimitive0Contact);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                // vspltw + vcmpeqfp against zero: a zero lane is a miss.
                if (LaneBits(lHitMask, liLane) == 0u)
                {
                    continue;
                }

                PrimitiveTestResult& lrRecord = laRecord[liLane];

                CGS_ASSERT(lrRecord.IsValid(), "lResult0.IsValid()"); // :513/:539/:565/:591

                lrRecord.muPrimitive0Tag   = LaneBits(lrBlock.mSurfaceTags, liLane);
                lrRecord.muPrimitive1Tag   = 0;
                lrRecord.muPrimitive0Index = static_cast<u16>((4 * liBatch) + liLane);
                lrRecord.muPrimitive1Index = static_cast<u16>(liSphere);

                lpaResults[lu16NumResults] = lrRecord;   // 80-byte copy at 80*index

                u16 lu16Next = static_cast<u16>(lu16NumResults + 1);
                if (lu16Next >= lu16MaxResults)
                {
                    lu16Next = static_cast<u16>(lu16MaxResults - 1);
                }
                lu16NumResults = lu16Next;
                lHeader.mu16NumResults = lu16NumResults;
            }
        }
    }

    *lpDesc->mpResultsList = lHeader;

    // ⭐ THE WITNESS (PC boot gate mechanism, log-once) — the twin of the sphere worker's, and
    // the line that proves the swept branch is no longer a hole. Not the console's; delete it
    // once world collision is proven map-wide.
    if (lu16NumResults > 0)
    {
        static bool s_bLoggedFirstSweptContacts = false;
        if (!s_bLoggedFirstSweptContacts)
        {
            s_bLoggedFirstSweptContacts = true;
            if (CgsDev::Message::gxMessageFilterFlags & 1)
                *CgsDev::Log::gpDebugPrint
                    << "⭐ SWEPT sphere-vs-triangle CONTACTS LIVE: "
                    << static_cast<s32>(lu16NumResults)
                    << " PrimitiveTestResult(s) in the command's result list "
                       "[FLAG PC boot witness]. Reported once, not per frame\n";
        }
    }
}

// ---------------------------------------------------------------------------------------------
// ContactGeneratorJob::ExecuteSweptSphereListWithTriangleListStream @0x82925238 (100)
//
// Instruction-for-instruction the sphere stream arm @0x829235C8 with three symbols changed:
// the assert line numbers (:642/:643 for :381/:382), the Prepare, and the worker. Same
// AllocateMemory(128,128) for the command scratch, same restore point AFTER the alloc, same
// absence of AddResult — the results travel in the command's own CollisionResultList.
//
//   0x82925338  AllocateMemory(128, 128)
//   0x82925358  stwx  -> miMemoryRestorePoint = miAllocCursor
//   0x82925370  addi r29, r31, 8         -> &cmd->mTriList
//   0x82925378  lfs  f1, 0x10(r31)       -> cmd->mfPadding
//   0x82925380  lwz  r6, 0x14(r31)       -> cmd->mpResultList
//   0x82925388  SweptSphereListWithTriangleListJobDesc::Prepare(local, &cmd->mSphereList, ...)
//   0x82925394  ExecuteSweptSphereListWithTriangleList(&local)
//   0x8292539C  RestoreMemory()
// ---------------------------------------------------------------------------------------------
void ContactGeneratorJob::ExecuteSweptSphereListWithTriangleListStream()
{
    typedef SweptSphereListWithTriangleListStreamJobDesc Desc;

    const Desc* lpDesc = static_cast<const Desc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                      // :642
    CGS_ASSERT(lpDesc->GetStreamProducer() != NULL, "No stream producer\n"); // :643

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetStreamProducer(), NULL, 0);

    Desc::StreamCommand* lpCommand =
        static_cast<Desc::StreamCommand*>(AllocateMemory(128, 128));

    miMemoryRestorePoint = miAllocCursor;

    u32 luCommandIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luCommandIndex) == 0)
    {
        SweptSphereListWithTriangleListJobDesc lLocalDesc;
        lLocalDesc.Prepare(&lpCommand->mSphereList, &lpCommand->mTriList,
                           lpCommand->mpResultList, lpCommand->mfPadding);

        ExecuteSweptSphereListWithTriangleList(&lLocalDesc);

        RestoreMemory();
    }

    lConsumer.Destruct();
}

// =============================================================================================
// ⭐⭐⭐ THE PROP ARM — ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream
// @0x82926650 (100), landed 2026-08-19 (wave Q6, cluster pstream). This is the type-12 worker:
// the drain end of the stream BrnPhysics::Props::PropManager::BeginPropWorldContactGeneration
// creates and DoPart/DoPropInstanceWorldContactGeneration post into, i.e. the reason a smashed
// prop's parts can collide with the world at all instead of free-falling.
//
// Grounding: the RAW `assembly` array of
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82926650.json (100 lines, matching
// (0x829267E0-0x82926650)/4). Hex-Rays pseudocode not consulted.
//
// SHAPE — instruction-for-instruction the sphere/swept stream arms with four symbols changed
// (the two assert line numbers, the Prepare, the worker):
//   0x82926668  lwz r28, 0x10(this)      -> mpJobDescription   assert :1104 "No job description\n"
//   0x829266CC  lwz r11, 0(r28)          -> mpStreamProducer   assert :1105 "No stream producer\n"
//               (0x450 == 1104, 0x451 == 1105 -- the `li r5` immediates)
//   0x82926740  SimpleDataStreamConsumer::Construct(&consumer, producer, 0, 0)
//   0x82926750  AllocateMemory(0x80, 0x80)                  -> the command scratch
//   0x82926778  stwx  -> miMemoryRestorePoint = miAllocCursor  (AFTER the alloc, as the twins do)
//   loop:       DataStreamCommandReader::ReadCom(&consumer.mReader, command, &index)
//   0x82926788  addi r29, r31, 0xC       -> &cmd->mTriangleList      (command +0x0C)
//   0x82926790  lbz  r7,  0x18(r31)      -> cmd->mbUseOptimisedBoxTests
//   0x82926794  mr   r4,  r31            -> &cmd->mPairList          (command +0x00)
//   0x82926798  lwz  r6,  0x14(r31)      -> cmd->mpResultsList       (command +0x14)
//   0x829267A0  PrimitiveListWithTriangleListJobDesc::Prepare(local, pairList, triList,
//                                                             resultList, flag)
//   0x829267AC  ExecutePrimitiveListWithTriangleList(&local)   (r4 == the local descriptor)
//   0x829267B4  RestoreMemory()
//   0x829267D4  SimpleDataStreamConsumer::Destruct()
//
// ⚠️ THOSE FOUR COMMAND OFFSETS ARE THE CONSUMER-SIDE PROOF of the StreamCommand layout that
// CgsPrimitiveListWithTriangleListStreamJobDesc.h models from the poster side. Both directions
// agree, which is why that header's member order is a measurement and not a reading of the DWARF
// alone.
//
// ⚠️ 128 IS THE PRODUCER'S ALIGNED STRIDE, NOT sizeof(StreamCommand). The console asks for a
// whole 128-byte arena slice (`li r4, 0x80 / li r5, 0x80`) because the poster's stride round is
// (32 + 127) & ~127 == 128 on the console AND (48 + 127) & ~127 == 128 here, and ReadCom copies a
// WHOLE STRIDE into this buffer. A sizeof-sized scratch would be read past its end every command
// -- the same doctrine the sphere arm's banner states for its own 128 and the line arm's for 256.
//
// ⭐ THE RESIDUAL THIS BANNER USED TO CARRY IS CLOSED (2026-08-19, wave Q6 cluster pvt): what
// this arm delegates to -- ExecutePrimitiveListWithTriangleList @0x82925908 (849) -- is a REAL
// BODY immediately below, no longer a named gate. Its measured closure (xrefs_from on
// 0x82925908): ContactGeneratorJob::{LoadPrimitives, LoadResultList, BuildGPInstance
// @0x829222A0, CollideGPInstances @0x829253C8}, PrimitivePairList::Itterator::{Prepare
// @0x82812128, GetPrimativeA @0x828121D8, MoveToNextHeader @0x82812210} and
// CgsGeometric::Triangle4::GetAOSTriangle. Everything there is real except the two workers
// BuildGPInstance / CollideGPInstances, which are DECLARED in this TU's header and DEFINED in
// the sibling partfile ContactGeneratorJob_wQ6_01.cpp (wave Q6, cluster gpi) -- that partfile
// must be mounted beside this one or the exe takes two LNK2019s.
// =============================================================================================
void ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream()
{
    typedef PrimitiveListWithTriangleListStreamJobDesc Desc;

    const Desc* lpDesc = static_cast<const Desc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                           // :1104
    CGS_ASSERT(lpDesc->GetDataStreamProducer() != NULL, "No stream producer\n");  // :1105

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetDataStreamProducer(), NULL, 0);

    Desc::StreamCommand* lpCommand =
        static_cast<Desc::StreamCommand*>(AllocateMemory(128, 128));

    miMemoryRestorePoint = miAllocCursor;

    u32 luCommandIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luCommandIndex) == 0)
    {
        PrimitiveListWithTriangleListJobDesc lLocalDesc;
        lLocalDesc.Prepare(&lpCommand->mPairList, &lpCommand->mTriangleList,
                           lpCommand->mpResultsList, lpCommand->mbUseOptimisedBoxTests);

        ExecutePrimitiveListWithTriangleList(&lLocalDesc);

        RestoreMemory();
    }

    lConsumer.Destruct();
}

// =============================================================================================
// ⭐⭐⭐ ContactGeneratorJob::ExecutePrimitiveListWithTriangleList @0x82925908 (849)
// THE PROP NARROW PHASE. Wave Q6, cluster pvt (2026-08-19).
//
// This is the kernel at the bottom of the whole breakable-prop world-collision leg:
//   PropManager::BeginPropWorldContactGeneration
//     -> CreateCollidePrimitiveListWithTriangleListStream
//     -> Do{Part,PropInstance}WorldContactGeneration
//          -> PrimitivePairListBuilder::AddPrimitive (the prop's volumes -> pair records)
//          -> BaseCollisionGenerator::AddPrimitiveListWithTriangleListToStream
//     -> RunCollidePrimitiveListWithTriangleListStream  (descriptor type 12)
//          -> ContactGeneratorJob::Execute case 12 -> ExecutePrimitiveListWithTriangleListStream
//               -> THIS FUNCTION, once per stream command
// While it was a gate, a smashed prop's parts generated NO world contacts and free-fell until
// PropManager::ReadUpdatedBodies printed "prop fell out of the world" and deleted them.
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82925908.json
// (849 lines == (0x8292664C-0x82925908)/4), dumped to scratchpad/waveQ6/asm_82925908.txt.
// Hex-Rays pseudocode NOT consulted. Rodata and the four assert strings were read out of a
// PRIVATE copy of the .i64 with headless IDA 9.3 this wave (scratchpad/waveQ6/ida_pvt/).
//
// SHAPE (every citation is an address in that listing):
//   0x82925944  the descriptor's PrimitivePairList copied word for word (3 console dwords)
//   0x82925960  LoadPrimitives(&desc->mTriangleList, &lTriangles)      (r4 = desc + 0x0C)
//   0x82925970  LoadResultList(desc->mpResultsList, &lResultsList)     (r4 = desc + 0xF0)
//   0x8292597C  ⚠️ the batch count is TRUNCATED TO 16 BITS (`clrlwi r11,r11,16`) and compared
//               unsigned against a 16-bit index that is re-masked on every increment
//               (0x829265E8) -- both loop variables are u16 in the console source, and that is
//               reproduced rather than widened.
//   0x8292598C  a zero batch count skips STRAIGHT to the header write-back (loc_829265FC), so
//               the write-back is unconditional -- reproduced.
//   per Triangle4 batch (stride 0xE0, `mulli r11, r27, 0xE0` @0x82925A00):
//     0x82925A08..44  GetAOSTriangle(lane, &laAOSTriangles[lane]) x4
//     0x82925A48..0x829261B8  the four GP triangle images, built inline (see
//                             BuildGPTriangleInstance above -- over half the function)
//     0x829261BC  PrimitivePairList::Itterator::Prepare(&lIterator, &lPairList)
//     0x829261C0  r25 = &block.mValidMasks  (Triangle4 + 0x90), hoisted out of the pair loop
//     per pair record (a DO-WHILE: the body runs before MoveToNextHeader decides):
//       0x829261C8  the cached header's type byte      -> Itterator::GetTypeA()
//       0x829261CC  the cached header's A tag halfword -> Itterator::GetPrimitiveTagA()
//       0x829261D4  Itterator::GetPrimativeA()
//       0x829261EC  BuildGPInstance(typeA, primA, &lGPPrimitive, tagA)
//       per lane 0..3 (0x829261F0 / 0x829262E8 / 0x829263E0 / 0x829264D8):
//         `vspltw v0, validMasks, lane ; vcmpeqfp. v0, v0, 0` and SKIP when all-equal --
//         i.e. run the lane only when its valid-mask word is non-zero.
//         assert |MagnitudeSquared(gpTriangle.mFaceNormals[0]) - 1| < 0.1f   (:1052/:1059/
//           :1066/:1073; `vandc` against 0x80000000 is the fabs, `vcmpgtfp.` the compare)
//         CollideGPInstances(&lGPPrimitive, &laGPTriangles[lane], iterator.GetPadding(),
//                            iterator.GetCurrentTestIndex(), 4*batch + lane, collisionIndex,
//                            &lResultsList)
//         collisionIndex = (u16)(collisionIndex + 1)                    (0x829262DC..E4)
//     0x829265D4  } while (Itterator::MoveToNextHeader())
//   0x829265FC..0x82926620  the 16-byte CollisionResultList header written back through
//                           desc->mpResultsList -- this is what publishes mu16NumResults.
//
// ⚠️ THE RESULT RECORDS ARE NOT WRITTEN HERE. CollideGPInstances appends each 80-byte
// PrimitiveTestResult into the list itself (`lhz 0xC(r10)` / `lwz 0(r10)` / 80-byte stride /
// clamp-to-max-1 @0x829256F0..0x82925784), which is why this worker keeps no local count the
// way the sphere and swept workers do. Do not "harmonise" it with them.
//
// ⚠️ Primitive0 IS THE PAIR-LIST PRIMITIVE AND Primitive1 IS THE TRIANGLE here -- the OPPOSITE
// of the sphere worker's convention. Proven by CollideGPInstances' three `sth`s: r7
// (Itterator::GetCurrentTestIndex) lands at result +0x48 muPrimitive0Index and r8 (4*batch +
// lane) at +0x4A muPrimitive1Index.
//
// ⚠️ f1 IS THE THIRD ARGUMENT AND r6 IS UNUSED at all four call sites (AGENTS gotcha 3). The
// value is the pair record's mfPadding, re-read from the iterator per lane exactly as the
// console does.
// =============================================================================================
void ContactGeneratorJob::ExecutePrimitiveListWithTriangleList(
    const PrimitiveListWithTriangleListJobDesc* lpDesc)
{
    using CgsGeometric::Triangle4;
    using rw::collision::GPInstance;

    // 0x82925944..0x8292595C -- the three dwords of the descriptor's pair list, copied to a
    // local. The Itterator is prepared against THIS copy, once per triangle batch.
    const PrimitivePairList lPairList = lpDesc->mPrimitivePairList;

    TriangleList lTriangles;
    LoadPrimitives(&lpDesc->mTriangleList, &lTriangles);

    CollisionResultList lResultsList;
    LoadResultList(lpDesc->mpResultsList, &lResultsList);

    // 0x8292597C `clrlwi r11, r11, 16` -- see the banner: both the bound and the index are u16.
    const u16 lu16NumTriangleBatches = static_cast<u16>(lTriangles.miNumTriangles);

    // r28: a running count of GP-instance collisions attempted across the WHOLE call (it is
    // never reset per batch or per pair record), handed to CollideGPInstances as the result
    // record's mu16TestIndex.
    u16 lu16CollisionIndex = 0;

    for (u16 lu16Batch = 0; lu16Batch < lu16NumTriangleBatches; ++lu16Batch)
    {
        const Triangle4& lrBlock = lTriangles.mpTriangles[lu16Batch];

        // The four lanes, unpacked to AOS and then to GP triangle images. On the console these
        // are eight separate stack objects, not two arrays: the GP images sit at r1+0xE0 /
        // 0x1A0 / 0x320 / 0x260 for lanes 0/1/2/3 (0xC0 apart, but NOT in lane order -- lane 3's
        // is BELOW lane 2's) and the AOS triangles at r1+0x480 / 0x3E0 / 0x4D0 / 0x430 (0x50
        // apart, likewise scrambled). That is stack-allocation order and nothing reads them by
        // stride, so they become arrays here -- every use is a lane index.
        Triangle4::AOSTriangle laAOSTriangles[4];
        GPInstance             laGPTriangles[4];

        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            lrBlock.GetAOSTriangle(liLane, laAOSTriangles[liLane]);
            BuildGPTriangleInstance(laGPTriangles[liLane], laAOSTriangles[liLane]);
        }

        PrimitivePairList::Itterator lIterator;
        lIterator.Prepare(&lPairList);

        // ⚠️ A DO-WHILE, NOT A WHILE, AND THAT IS THE CONSOLE'S SHAPE, NOT A TRANSCRIPTION SLIP.
        // The loop top loc_829261C4 is reached by FALLTHROUGH from Prepare (0x829261BC), and
        // Itterator::Prepare @0x82812128 has NO empty-list guard -- it caches the blob's first
        // 16 bytes and asserts their checksum whatever mu16NumTests says. So an EMPTY pair list
        // still runs this body once, with a zeroed header, and BuildGPInstance then takes its
        // default "false" assert arm (:1699). Reproduced as read; do not invent a
        // `GetNumTests() != 0` guard to quiet it -- the tripwire is the console's own, and
        // silencing it would hide an empty pair list, which is exactly the failure this leg has.
        do
        {
            // Read the cached header BEFORE the primitive pointer, as the console does.
            const PrimitivePairList::EVolumeType leTypeA = lIterator.GetTypeA();
            const u16                            lu16TagA = lIterator.GetPrimitiveTagA();

            GPInstance lGPPrimitive;
            BuildGPInstance(leTypeA, lIterator.GetPrimativeA(), &lGPPrimitive, lu16TagA);

            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                // vspltw the lane out of Triangle4::mValidMasks and compare against 0.0f: a
                // zero word means "this lane holds no real triangle".
                if (LaneBits(lrBlock.mValidMasks, liLane) == 0u)
                {
                    continue;
                }

                const GPInstance& lrGPTriangle = laGPTriangles[liLane];

                // |MagnitudeSquared(n) - 1| < 0.1f, all four lanes (the operand is a broadcast,
                // so the console's all-lanes `vcmpgtfp.` is one scalar test here).
                const f32 lfNormalMagnitudeSquared = Dot3(lrGPTriangle.mFaceNormals[0],
                                                          lrGPTriangle.mFaceNormals[0]);
                CGS_ASSERT(std::fabs(lfNormalMagnitudeSquared - 1.0f)
                               < KF_GP_TRIANGLE_NORMAL_TOLERANCE,
                           KAPC_GP_TRIANGLE_NORMAL_ASSERTS[liLane]);   // :1052/:1059/:1066/:1073

                CollideGPInstances(&lGPPrimitive,
                                   &lrGPTriangle,
                                   lIterator.GetPadding(),
                                   lIterator.GetCurrentTestIndex(),
                                   static_cast<u16>((4 * lu16Batch) + liLane),
                                   lu16CollisionIndex,
                                   &lResultsList);

                lu16CollisionIndex = static_cast<u16>(lu16CollisionIndex + 1);
            }
        }
        while (lIterator.MoveToNextHeader());
    }

    // ⭐ [DIAG] NOT IN THE X360 BINARY -- wave Q6, behind BRN_PROP_DIAG, one-shot. The latch is
    // evaluated ONCE (a getenv per command would be a syscall on the job thread's hot path) and
    // it fires on the first call that actually had a triangle batch to run, so the counts
    // describe real work rather than an empty command. Read it together with
    // [Q6-worldc] (the producer side) and "prop fell out of the world": prims/tris non-zero
    // with results == 0 means the pair list is arriving EMPTY, which is the
    // PrimitivePairListBuilder::AddPrimitive leg, not this one.
    if (lu16NumTriangleBatches != 0)
    {
        static const bool sbPropDiag  = (std::getenv("BRN_PROP_DIAG") != 0);
        static bool       sbFirstPass = true;
        if (sbPropDiag && sbFirstPass && CgsDev::Log::gpDebugPrint != 0)
        {
            sbFirstPass = false;
            *CgsDev::Log::gpDebugPrint
                << "[Q6-narrow] first prim-vs-tri batch: prims="
                << static_cast<s32>(lPairList.mu16NumTests)
                << " tris=" << static_cast<s32>(lu16NumTriangleBatches)
                << " results=" << static_cast<s32>(lResultsList.mu16NumResults)
                << "\n";
        }
    }

    // 0x829265FC..0x82926620 -- the unconditional 16-byte header write-back.
    *lpDesc->mpResultsList = lResultsList;
}

// =============================================================================================
// ContactGeneratorJob::ExecuteBoxListWithTriangleList @0x829218B8 (44)   ⚠️ THE CONSOLE'S OWN
// "NOT IMPLEMENTED" ARM. Wave Q7, cluster arms (2026-08-19).
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x829218B8.json
// (44 lines == (0x82921964-0x829218B8)/4 + 1). All three of its `xrefs_from` are accounted for
// below: LoadPrimitives @0x829210F0, LoadResultList @0x829211E8, Assert::PrintStringed.
//
// THIS ARM DOES NO COLLISION WORK ON THE CONSOLE EITHER, and that is the finding, not a park:
//   0x829218D4  lwz  r30, 0x10(r31)          -> mpJobDescription
//   0x829218D8  addi r4,  r30, 8             -> &desc->mTriangleList   (the derived payload's
//                                               second member; see the type note below)
//   0x829218DC  bl   LoadPrimitives          (r5 = a stack local, never read again)
//   0x829218E8  lwz  r4,  0xF0(r30)          -> desc->mpResultsList    (the SHARED base seat)
//   0x829218EC  bl   LoadResultList          (r5 = the SAME stack local -- the compiler
//                                             coalesced two dead locals onto one slot)
//   0x829218F0..0x8292194C  CGS_ASSERT(false, "Not implemented")       ContactGeneratorJob.cpp:841
//   0x82921964  blr
// and nothing else -- no triangle loop, no kernel, no result written. There is no box-vs-
// triangle-list narrow phase anywhere in the X360 image to reconstruct.
//
// ⚠️ ONE SOURCE-LEVEL CGS_ASSERT, TWO PrintStringed CALLS. The X360 build emits the assert body
// twice at the same line number (0x82921910 and 0x82921938, both `li r5, 0x349` == 841). That is
// this compiler's fixed expansion of one assert and NOT two asserts: the same doubling appears
// in LoadPrimitives @0x829210F0, whose two DISTINCT asserts (:1405 and :1406) each emit the pair.
// So this is one `CGS_ASSERT(false, "Not implemented")`, matching the tree's existing precedent
// for console "not implemented" defaults (CgsDevice.cpp:58 and its siblings).
//
// ⭐ THE ARM IS NOW STORE FOR STORE (wave Q7, fixer round 1). It shipped one wave with the
// `LoadPrimitives(&desc->mTriangleList, &local)` call at 0x829218DC dropped, because that call
// needs the DERIVED descriptor type and that type had no home in this tree. The home now exists:
// JobDescription/CgsBoxListWithTriangleListJobDesc.h, transcribed from the DecFIGS DWARF
// (h:58 `struct BoxListWithTriangleListJobDesc : public CollisionJobDescription`, h:95-98
// `Data { BoxList mBoxList; TriangleList mTriangleList; }`, h:37-40 `struct BoxList {
// CgsGeometric::Box* mpaBoxes; int32_t miNumBoxes; }`), which puts mTriangleList at the payload's
// +0x08 and INDEPENDENTLY confirms the `addi r4, r30, 8` above. It is a pure-declaration header:
// no new TU, no mount line, and its two accessors are inline, so this TU gains no new external.
// WHAT THE RESTORED CALL DOES: nothing observable, which is why dropping it was inert rather than
// wrong. LoadPrimitives' whole body is two asserts plus `*dst = *src`; here the source is an
// interior address of a live object (never NULL) and the destination is a stack local (never
// NULL), so neither assert can fire and the copy's destination is dead -- the compiler in fact
// coalesced it onto the SAME stack slot the result-list copy uses. It is landed anyway because
// the arm is a 44-instruction refusal and a refusal is worth having exactly, not approximately.
// =============================================================================================
void ContactGeneratorJob::ExecuteBoxListWithTriangleList()
{
    // 0x829218D4 `lwz r30, 0x10(r31)` -- the descriptor, then its DERIVED type for the payload.
    const BoxListWithTriangleListJobDesc* lpDesc =
        static_cast<const BoxListWithTriangleListJobDesc*>(mpJobDescription);

    // 0x829218D8/0x829218DC `addi r4, r30, 8 ; bl LoadPrimitives` -- the triangle list copied
    // into a stack local the console never reads again (the compiler coalesced it with the
    // result-list local below onto one slot).
    TriangleList lTriangleList;
    LoadPrimitives(&lpDesc->GetTriangleList(), &lTriangleList);

    // 0x829218E8 `lwz r4, 0xF0(r30)` -- the results list, reached through the SHARED base seat
    // (CollisionJobDescription::mpResultsList). The copy's destination is dead too;
    // LoadResultList's :1552 assert is its whole effect.
    CollisionResultList lResultsList;
    LoadResultList(mpJobDescription->GetResultsList(), &lResultsList);

    CGS_ASSERT(false, "Not implemented");                                            // :841
}

// =============================================================================================
// ⭐⭐ ContactGeneratorJob::ExecutePrimitivePairList @0x82925798 (92)
// THE PRIMITIVE-PAIR WALK -- job type 10. Wave Q7, cluster arms (2026-08-19).
//
// GROUNDING: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82925798.json
// (92 lines == (0x82925904-0x82925798)/4 + 1). Every one of its `xrefs_from` is accounted for
// below: LoadResultList, PrimitivePairList::Itterator::{Prepare @0x82812128, GetPrimativeA
// @0x828121D8, GetPrimativeB @0x828121E8, MoveToNextHeader @0x82812210}, BuildGPInstance
// @0x829222A0, CollideGPInstances @0x829253C8, Assert::PrintStringed.
//
// ─── WHAT THE PAIR-LIST FAMILY ACTUALLY IS -- MEASURED, BECAUSE AN EARLIER NOTE GOT IT WRONG ──
// A stale note in this campaign called this arm "prop-vs-prop". IT IS NOT. Measured from
// `xrefs_to`, this wave, on the two builder entry points that CREATE primitive pairs:
//   PrimitivePairListBuilder::AddPrimitivePair @0x82814708 <- exactly three callers:
//       DeformationManager::AddRaceCarBodyPartPair  @0x82605928
//       DeformationManager::AddHingedBodyPartPairs  @0x82605A98
//       VehicleManager::DoCarCarContactGeneration   @0x8261BB38
//   sub_828149F8 (the second AddPrimitivePair* overload, same AllocateMemory/sub_82814480
//       closure) <- exactly one caller: DeformationManager::AddRaceCarWheelPair @0x82605BE8
// and all four of THOSE are called from one place: VehicleManager::StartVehicleContactGeneration
// @0x8262AEE8. On the consumer side, `xrefs_to` on the synchronous dispatcher
// BaseCollisionGenerator::CollidePrimitivePairList @0x82814138 (the only thing that prepares a
// type-10 descriptor, via PrimitivePairListJobDesc::Prepare @0x82810478) is likewise exactly
// two: VehicleManager::StartVehicleContactGeneration @0x8262AEE8 and StartPartContactGeneration
// @0x8262C220.
// ⇒ THE PAIR-LIST FAMILY IS THE CAR SIDE: car-vs-car, plus the deformable body-part and
//   detached-wheel pairs of the SAME cars. NO PROP FUNCTION APPEARS ANYWHERE IN IT -- not a
//   producer, not a consumer, not a caller of a caller. A prop or a broken-off prop part
//   collides with the WORLD through the type-11/12 primitive-list-vs-TRIANGLE-list arms above
//   (PropManager::Do{Part,PropInstance}WorldContactGeneration -> PrimitivePairListBuilder::
//   AddPrimitive -- the SINGLE-primitive entry, not AddPrimitivePair -- ->
//   AddPrimitiveListWithTriangleListToStream), and with cars through the car side's own
//   contact generation. No prop-vs-prop producer exists in the image; if one is ever found,
//   this paragraph is the thing to correct.
//
// SHAPE (every citation is an address in that listing):
//   0x829257B0..0x829257C4  the descriptor's PrimitivePairList copied word for word (3 console
//                           dwords) into a local -- BEFORE the result list is loaded
//   0x829257C8/CC           LoadResultList(desc->mpResultsList, &lResultsList)   (r4 = desc+0xF0)
//   0x829257D8              PrimitivePairList::Itterator::Prepare(&lIterator, &lPairList)
//   per pair record (a DO-WHILE: the body runs before MoveToNextHeader decides):
//     0x829257F0  the cached header's A type byte (+0x00) -> Itterator::GetTypeA()
//     0x829257F4  the cached header's A tag halfword (+0x0C) -> Itterator::GetPrimitiveTagA()
//     0x829257FC  Itterator::GetPrimativeA()
//     0x82925814  BuildGPInstance(typeA, primA, &lGPInstanceA, tagA)      (dest r1+var_100)
//     0x8292581C  the cached header's B tag halfword (+0x0E) -> GetPrimitiveTagB()
//     0x82925818/20  assert mCurrentHeader.mu8HeaderType == 1        (CgsPrimitivePairList.h:107)
//     0x8292587C  the cached header's B type byte (+0x01) -> Itterator::GetTypeB()
//     0x82925884  Itterator::GetPrimativeB()
//     0x8292589C  BuildGPInstance(typeB, primB, &lGPInstanceB, tagB)      (dest r1+var_1C0)
//     0x829258A8  lfs f1, iterator+0x08                 -> Itterator::GetPadding()
//     0x829258A0  lhz r9,  iterator+0x14                -> Itterator::GetCurrentTestIndex()
//     0x829258C0  CollideGPInstances(&A, &B, padding, idx, idx+1, idx, &lResultsList)
//   0x829258C8  } while (Itterator::MoveToNextHeader())
//   0x829258D8..0x829258FC  the 16-byte CollisionResultList header written back through
//                           desc->mpResultsList -- this is what publishes mu16NumResults.
//
// ⚠️ ONE HALFWORD FEEDS ALL THREE INDEX ARGUMENTS. `lhz r9` is copied to r7, `addi r8, r7, 1`
// makes the second, and r9 itself is still live as the third at the call: the result record gets
// muPrimitive0Index = testIndex, muPrimitive1Index = testIndex + 1, mu16TestIndex = testIndex.
// That is NOT the type-11 worker's convention (there, primitive1 is the triangle lane) and it is
// not a transcription slip -- the pair walk has no lanes to number, so the console numbers the
// two sides of the pair off the record's own index. Reproduced exactly.
//
// ⚠️ r6 IS SKIPPED AT THE CollideGPInstances CALL. f1 carries the third argument (the pair
// record's mfPadding): a PPC float parameter consumes its GPR slot without using it (AGENTS
// gotcha 3). Same trap, same call, same answer as the type-11 worker's four call sites.
//
// ⚠️ A DO-WHILE, NOT A WHILE, AND THAT IS THE CONSOLE'S SHAPE. The loop top loc_829257EC is
// reached by FALLTHROUGH from Prepare (0x829257D8), and Itterator::Prepare @0x82812128 has NO
// empty-list guard -- it caches the blob's first 16 bytes and asserts their checksum whatever
// mu16NumTests says. An EMPTY pair list therefore still runs this body once with a zeroed
// header, and BuildGPInstance then takes its default "false" assert arm (:1699). Do not invent a
// `GetNumTests() != 0` guard to quiet it: the tripwire is the console's own, and silencing it
// would hide an empty pair list, which is the exact failure mode this leg has.
//
// ⚠️ THE RESULT RECORDS ARE NOT WRITTEN HERE. CollideGPInstances appends each 80-byte
// PrimitiveTestResult into the list itself, which is why this arm keeps no local result count --
// same as the type-11 worker, and deliberately unlike the sphere/swept workers.
//
// ⚠️ THE ASSERT AT CgsPrimitivePairList.h:107 IS LANDED AT THE CALL SITE, NOT IN ITS HOME. The
// console fires it from an INLINED accessor of that header (its file string is that header, and
// it sits between the A-side BuildGPInstance and the B-side type read) -- so it is either
// GetTypeB() or GetPrimitiveTagB(); the asm does not discriminate between them, because the
// compiler hoisted the tag load above the branch. It is written out here, in the console's
// firing position, rather than guessed into one of the two accessors in a header this cluster
// does not own. FOLLOW-UP: move it into whichever accessor a future DWARF/source witness names.
// =============================================================================================
void ContactGeneratorJob::ExecutePrimitivePairList()
{
    using rw::collision::GPInstance;

    const PrimitivePairListJobDesc* lpDesc =
        static_cast<const PrimitivePairListJobDesc*>(mpJobDescription);

    // 0x829257B0..0x829257C4 -- the three console dwords of the descriptor's pair list, copied
    // to a local. The Itterator is prepared against THIS copy, exactly as the type-11 worker
    // does with its own.
    const PrimitivePairList lPairList = lpDesc->mPrimitivePairList;

    CollisionResultList lResultsList;
    LoadResultList(lpDesc->mpResultsList, &lResultsList);

    PrimitivePairList::Itterator lIterator;
    lIterator.Prepare(&lPairList);

    do
    {
        // The A side, in the console's read order: both cached-header fields first, then the
        // packed-data pointer (0x829257F0 / 0x829257F4 / 0x829257FC).
        const PrimitivePairList::EVolumeType leTypeA  = lIterator.GetTypeA();
        const u16                            lu16TagA = lIterator.GetPrimitiveTagA();

        GPInstance lGPInstanceA;
        BuildGPInstance(leTypeA, lIterator.GetPrimativeA(), &lGPInstanceA, lu16TagA);

        // 0x8292581C -- B's tag is read BEFORE the header-kind check (the compiler hoisted it
        // out of the branch); kept in the console's order.
        const u16 lu16TagB = lIterator.GetPrimitiveTagB();

        CGS_ASSERT(lIterator.mCurrentHeader.mu8HeaderType == KU8_LIST_TYPE_PRIMATIVE_PAIR,
                   "mCurrentHeader.mu8HeaderType == E_LIST_TYPE_PRIMATIVE_PAIR");
                                                              // CgsPrimitivePairList.h:107

        const PrimitivePairList::EVolumeType leTypeB = lIterator.GetTypeB();

        GPInstance lGPInstanceB;
        BuildGPInstance(leTypeB, lIterator.GetPrimativeB(), &lGPInstanceB, lu16TagB);

        // 0x829258A0 / 0x829258AC / 0x829258BC -- one halfword, three index arguments.
        const u16 lu16TestIndex = lIterator.GetCurrentTestIndex();

        CollideGPInstances(&lGPInstanceA,
                           &lGPInstanceB,
                           lIterator.GetPadding(),
                           lu16TestIndex,
                           static_cast<u16>(lu16TestIndex + 1),
                           lu16TestIndex,
                           &lResultsList);
    }
    while (lIterator.MoveToNextHeader());

    // ⭐ [DIAG] NOT IN THE X360 BINARY -- wave Q7, behind BRN_PROP_DIAG, one-shot. The latch is
    // evaluated ONCE (a getenv per job would be a syscall on the job thread) and it fires on the
    // FIRST call of all, empty list included: a type-10 job that arrives with pairs=0 is itself
    // the finding, so this is deliberately not gated on having done work.
    // Reading it: `headers` is how many records the Itterator actually walked
    // (GetCurrentTestIndex() + 1 -- MoveToNextHeader leaves the cursor on the last record), and
    // it reads 1 even for an EMPTY list because of the do-while above; `pairs` is what the
    // poster declared in the list header. pairs=0 headers=1 therefore means "an empty pair list
    // was posted", which is a VehicleManager::StartVehicleContactGeneration / DoCarCar
    // ContactGeneration problem, not this arm's. results=0 with pairs>0 means the two GP
    // instances never overlapped, which is normal for most frames.
    {
        static const bool sbPropDiag  = (std::getenv("BRN_PROP_DIAG") != 0);
        static bool       sbFirstPass = true;
        if (sbPropDiag && sbFirstPass && CgsDev::Log::gpDebugPrint != 0)
        {
            sbFirstPass = false;
            *CgsDev::Log::gpDebugPrint
                << "[Q7-pairs] first primitive-pair batch: headers="
                << static_cast<s32>(lIterator.GetCurrentTestIndex() + 1)
                << " pairs=" << static_cast<s32>(lPairList.mu16NumTests)
                << " results=" << static_cast<s32>(lResultsList.mu16NumResults)
                << "\n";
        }
    }

    // 0x829258D8..0x829258FC -- the unconditional 16-byte header write-back.
    *lpDesc->mpResultsList = lResultsList;
}

// =============================================================================================
// ⭐⭐⭐ THIS FILE NO LONGER HAS A SINGLE GATE, AND THE MACHINERY IS GONE WITH THEM.
// Retired 2026-08-19 (wave Q7, cluster arms): the four remaining BRN_CONTACT_JOB_GATE bodies
// (ExecuteSphereListWithSphereList, ...Stream, ExecuteBoxListWithTriangleList,
// ExecutePrimitivePairList) and the BRN_CONTACT_JOB_GATE macro + its #undef. Two of those four
// are real bodies above; the OTHER TWO -- the sphere-sphere pair, job types 7 and 8 -- are
// declared in this TU's header and defined in ContactGeneratorJob_wQ7_01.cpp (wave Q7, cluster
// ss), exactly as BuildGPInstance / CollideGPInstances are defined in ContactGeneratorJob_wQ6_01
// .cpp. BOTH PARTFILES MUST BE MOUNTED WITH THIS TU -- four LNK2019s otherwise, and `cl /c`
// cannot see any of them, so a green compile gate proves nothing about the link.
// The earlier prop narrow-phase gate went the same way one wave before this
// (2026-08-19, wave Q6 cluster pvt: ExecutePrimitiveListWithTriangleList @0x82925908).
// =============================================================================================

// ============================================================================
// FOLDED FROM ContactGeneratorJob_wQ6_01.cpp (wave Q6) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
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

// (preprocessor / using lines carried from the partfile's include region)
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

// ============================================================================
// FOLDED FROM ContactGeneratorJob_wQ7_01.cpp (wave Q7) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =================================================================================================
// GameShared/Jobs/ContactGenerator/ContactGeneratorJob_wQ7_01.cpp
//
// ⭐⭐⭐ THE SPHERE-LIST vs SPHERE-LIST NARROW PHASE — collision job types 7 and 8, i.e. the
// CAR-vs-CAR contact leg. A partfile of ContactGeneratorJob.cpp (wave Q7, cluster `ss`,
// 2026-08-19) holding the two arms of `Execute`'s 12-way switch that were still NAMED ONE-SHOT
// BOOT GATES at the foot of that file:
//
//   ContactGeneratorJob::ExecuteSphereListWithSphereList       @0x829215B0  (193)  :721
//   ContactGeneratorJob::ExecuteSphereListWithSphereListStream @0x82923758  (100)  :429 :430
//
// The console's own path for both is the SAME TU as the rest of the family — every assert here
// passes the string
//   D:\P4\B5_MAIN\Burnout\MAIN\Code\GameShared\Jobs\ContactGenerator\ContactGeneratorJob.cpp
// (rodata 0x82101940) — so this is a partfile of that TU, split only because
// ContactGeneratorJob.{h,cpp} is owned by a concurrent session in this wave. FOLLOW-UP: fold it
// back into ContactGeneratorJob.cpp when nobody is editing that file, exactly as
// ContactGeneratorJob_wQ6_01.cpp's banner already asks for itself. There is no reason for three
// TUs beyond the ownership split.
//
// ─── WHERE THIS SITS IN THE CAR-vs-CAR CHAIN ─────────────────────────────────────────────────
//   VehicleManager::DoCarCarContactGeneration @0x8261BB38
//     -> BaseCollisionGenerator::AddSphereListWithSphereListToStream @0x828119F0   (one command)
//     -> BaseCollisionGenerator::RunCollideSphereListWithSphereListStream @0x82811C00
//        (`li r23, 8` -> descriptor +0xFF, i.e. job type 8)
//        -> ContactGeneratorJob::Execute @0x829267E0 case 8
//           -> ExecuteSphereListWithSphereListStream   <-- THIS FILE (drains the command stream)
//              -> ExecuteSphereListWithSphereList      <-- THIS FILE (the worker, per command)
//                 -> one 80-byte PrimitiveTestResult per touching sphere pair into the
//                    command's own CollisionResultList
// While both were gates, every car-vs-car sphere pair the vehicle manager posted was silently
// dropped: the stream drained nothing and the result list stayed at zero.
//
// ─── GROUNDING ───────────────────────────────────────────────────────────────────────────────
// * @0x829215B0: the RAW `assembly` array of .ida-exports/BURNOUT_X360_ARTIST.XEX/0x829215B0.json
//   (193 rows == (0x829218B0-0x829215B0)/4 + 1), dumped to scratchpad/waveQ7/ss/asm_829215B0.txt.
//   Hex-Rays pseudocode NOT consulted.
// * @0x82923758: **AN EXPORT HOLE — there is no per-address JSON for it.** Disassembled this wave
//   with headless IDA 9.3 on a PRIVATE COPY of the .i64 (scratchpad/waveQ7/ss/ida/, script
//   dump_ss.py, output out.json key `asm_82923758`); IDA reports start 0x82923758 / end
//   0x829238E8 / 100 instructions, which is exactly the gap between
//   ExecuteSphereListWithTriangleListStream @0x829235C8 and
//   ExecuteSweptSphereListWithTriangleList @0x829238E8. Nothing is missing from the decode.
// * The same run re-read every string operand both bodies reference (out.json `refs_*`): the file
//   path 0x82101940, "lResult.IsValid()" @0x821019D0 (17 chars, NOT truncated), "No job
//   description\n" @0x821019F8 and "No stream producer\n" @0x821019E4.
// * NO RODATA CONSTANT IS READ BY EITHER BODY. Every vector constant the worker needs is
//   MATERIALISED, not loaded: `vspltisw v0,1` + `vcsxwfp128 v126,v0,0` == 1.0f,
//   `vcsxwfp128 v125,v0,1` == 0.5f (a shift-by-one convert), `vspltisw128 v127,0` == 0.0f, and
//   `vspltisw v0,-1` + `vcfsx v0,v0,0` == -1.0f. So there is no unk_/flt_ table to guess at and
//   none is guessed at (gotcha 5 has nothing to bite on here).
//
// ─── ⚠️⚠️ THE SIGNATURE, AND HOW IT WAS MEASURED ─────────────────────────────────────────────
// `ExecuteSphereListWithSphereList` TAKES THE DESCRIPTOR AS A PARAMETER. It is not the no-arg
// form the header carried while it was a gate; owner `arms` corrected the declaration in the
// same wave, from the same two witnesses, independently. Both are in the asm:
//   1. 0x829215C4  `mr r18, r4` is the FIRST thing the body does after its prologue, and every
//      later descriptor read goes through r18 (`ld 0(r18)` / `ld 8(r18)` / `lwz 0xF0(r18)` /
//      `lfs 0xF4(r18)`). A no-arg member would have to reach them through
//      this->mpJobDescription (`lwz 0x10(r3)`), and that load does not exist in the 193 rows.
//   2. Execute's jump table calls it at 0x82926890 with r4 == lpvJobData untouched — exactly as
//      cases 5 (sphere/triangle), 6 (prop) and 8 (swept) do for the three workers that already
//      take a descriptor — and the Stream arm below calls it at 0x829238B4 with
//      `addi r4, r1, var_130`, i.e. ITS OWN STACK-LOCAL descriptor.
// ⚠️ Witness 2 is the load-bearing one: on the stream path this->mpJobDescription is the STREAM
// descriptor (type 8) while r4 is the per-command NON-stream descriptor (type 7). A no-arg body
// reading mpJobDescription would therefore read the WRONG OBJECT on every streamed command —
// a silent, compile-clean, lint-clean corruption of exactly the kind this wave keeps finding.
//
// ─── PC LOWERING, STATED ONCE ────────────────────────────────────────────────────────────────
// The console runs the worker's inner test on VMX with every operand BROADCAST (the separation
// is a `vmsum3fp128`, the two radii and the padding are `vspltw`s), so all four lanes always
// carry the same number and the SoA "which lane?" hazard the sibling triangle workers face does
// not exist here: the lowering is plain scalar f32, which is this family's standing precedent
// (CgsTriangle4.cpp, CgsPolygonSoupTests.cpp, ContactGeneratorJob.cpp). Whole-vector moves
// (lvx128/stvx128) stay four-lane copies so no `w` is dropped.
// ⚠️ ONE PLACE WHERE THE ARITHMETIC IS NOT BIT-IDENTICAL TO THE CONSOLE, flagged at the site:
// `vrsqrtefp` + TWO Newton-Raphson rounds is lowered to `1.0f / std::sqrt()`. Same lowering, same
// wording and same reason as ContactGeneratorJob.cpp:553 — more accurate than the console, and it
// cannot flip an accept/reject decision except in the last couple of ulps.
// =================================================================================================

// (preprocessor / using lines carried from the partfile's include region)
using CgsSceneManager::CgsCollision::CollisionResultList;
using CgsSceneManager::CgsCollision::PrimitiveTestResult;
using CgsSceneManager::CgsCollision::SphereList;
using CgsSceneManager::CgsCollision::SphereListWithSphereListJobDesc;
using CgsSceneManager::CgsCollision::SphereListWithSphereListStreamJobDesc;

// =================================================================================================
// ContactGeneratorJob::ExecuteSphereListWithSphereList @0x829215B0 (193)   :721
//
// Every sphere of list A against every sphere of list B; one 80-byte PrimitiveTestResult per
// touching pair into the descriptor's CollisionResultList.
//
// SHAPE (every citation is an address in scratchpad/waveQ7/ss/asm_829215B0.txt):
//   0x829215CC  ld  r11, 0(desc)  -> the {base,count} pair of sphere list A, copied to a local
//   0x829215D8  ld  r11, 8(desc)  -> ... and of sphere list B
//   0x829215E0  LoadResultList(desc->mpResultsList, &lResultsList)      (r4 = `lwz 0xF0(desc)`)
//   0x829215E8  lfs f0, 0xF4(desc) -> the padding, splatted into v122
//   0x82921600  clrlwi r17, r10, 16  ⚠️ BOTH COUNTS AND BOTH LOOP INDICES ARE u16 (the indices are
//               re-masked on every increment at 0x82921858 / 0x8292186C) — reproduced, not widened
//   0x82921624  a ZERO A-count branches STRAIGHT to the header write-back, so that write-back is
//               unconditional — reproduced
//   0x82921664  outer: sphere A = the 16 bytes at listA.mpSpheres + 16*a, copied to a stack local
//   0x82921688  a zero B-count skips the inner loop
//   0x829216A8  inner:
//     0x829216C4  d   = B - A                      (ALL FOUR LANES, so d.w == rB - rA)
//     0x829216CC  v13 = rA + rB                    (two vspltw of lane 3 + vaddfp)
//     0x829216D0  v0  = dot3(d, d)                 (vmsum3fp128, broadcast)
//     0x829216D4  v6  = rA + rB + padding
//     0x829216D8..0x82921708  invLen = vrsqrtefp(dot3) refined by TWO Newton-Raphson rounds
//     0x8292170C  dir      = d * invLen            (the unit A->B direction, all four lanes)
//     0x82921710  distance = dot3 * invLen         (== sqrt(dot3))
//     0x82921718  stvx128 -> record +0x10  mPrimitive1Normal  = dir
//     0x8292171C  vmaddfp v9 = dir*rA + A          -> the contact point ON A
//     0x82921724  vsubfp  v12 = B - dir*rB         -> the contact point ON B
//     0x82921738  stvx128 -> record +0x20  mPrimitive0Contact = {A-side point, w = 0}
//     0x8292174C  stvx128 -> record +0x30  mPrimitive1Contact = {B-side point, w = 0}
//     0x82921750  vsel: a ZERO dot3 forces distance to 0 (see the NaN note below)
//     0x82921754..0x8292176C  accept  <=>  !(distance > rA + rB + padding)
//     0x82921780  record +0x00  mPrimitive0Normal = dir * -1.0f
//     0x82921788  PrimitiveTestResult::IsValid  -> assert "lResult.IsValid()"          (:721)
//     0x829217EC  record +0x48/+0x4A = the two u16 sphere indices; +0x40/+0x44 = 0
//     0x82921818  the ten ld/std that copy the 80-byte record to results[numResults]
//     0x8292182C  numResults+1, clamped to max-1, published into the LOCAL header
//   0x8292187C  the unconditional 16-byte header write-back through desc->mpResultsList
//
// ⚠️ PRIMITIVE0 IS SPHERE A AND PRIMITIVE1 IS SPHERE B, and the two normals are OPPOSITE, not
// equal. `sth r27 -> +0x48` is the A index and `sth r31 -> +0x4A` is the B index (both registers
// are the compiler's induction copies of the loop counters r25/r28 and hold the CURRENT index at
// the store — checked iteration by iteration). +0x10 gets `dir` (A->B) and +0x00 gets `-dir`, i.e.
// each side's normal points AWAY from the other. That is a real difference from the two triangle
// workers, where CollideGPInstances/IntersectTriangle4Sphere write the SAME vector into both
// normal slots; do not "harmonise" them.
//
// ⚠️ NO `numResults < maxNumResults` ASSERT. The sphere/triangle worker checks that once per
// (batch x sphere) at :182 and the swept one at :497; this worker has NO such check — the only
// assert in its 193 instructions is the IsValid one. Reproduced as read.
//
// ⚠️ NO LoadPrimitives CALL. The two sphere lists are copied with plain 8-byte loads/stores
// (0x829215CC / 0x829215D8); only the RESULT list goes through a Load* helper. `xrefs_from`
// on 0x829215B0 lists exactly LoadResultList, PrimitiveTestResult::IsValid, Assert::PrintStringed
// and the three save/restore thunks (__savegprlr_17 / __savevmx_122 / __restvmx_122;
// __restgprlr_17 arrives via the tail `b`) — nothing else.
//
// ⚠️ mu16TestIndex (+0x4C) and muPad (+0x4E) ARE NEVER WRITTEN by this worker either, so on the
// console they carry stack garbage into every queued copy. Zero-initialised once here instead
// (deterministic; garbage is not reproducible) — the same call, for the same reason, that
// ExecuteSphereListWithTriangleList and CollideGPInstances already make.
//
// ⚠️ THE THREE VECTOR FIELDS AT +0x10/+0x20/+0x30 ARE STORED **BEFORE** THE ACCEPTANCE TEST
// (0x82921718 / 0x82921738 / 0x8292174C all sit above the `bne` at 0x8292176C) while +0x00 is
// stored after it. Kept in that order. It is not observable — an accepted pair rewrites all four
// before the copy — but it is what the console does and the next reader should not have to
// re-derive it.
//
// ⚠️ ONE STORE IS DELIBERATELY NOT REPRODUCED, and this note is why the next reader diffing the
// listing does not have to re-derive it: 0x829216B4 `stw r30, var_180(r1)` writes zero into the
// stack slot that held sphere list A's {base,count} pair, once per INNER iteration. Both halves
// of that slot were already latched into registers before the loops (`lwz r19, var_180` at
// 0x8292162C for the base, `lwz r10, var_180+4` at 0x829215F4 for the count) and nothing reads
// the slot again in the remaining 190 instructions. It is a dead compiler artifact — a spilled
// zero for a value the optimiser folded away — not a field of anything.
//
// ⚠️ TWO COINCIDENT SPHERE CENTRES ARE WHY THE IsValid ASSERT EXISTS. With dot3 == 0 the
// reciprocal square root is infinite, so `dir` is 0*inf == NaN, and the `vsel` at 0x82921750
// forces the distance to 0 — which is `<= rA+rB+padding`, so the pair is ACCEPTED with NaN
// normals and IsValid then fails. That is the console's own behaviour, tripwire included; no
// degenerate guard is invented here (the same call BuildGPTriangleInstance makes for its zero
// edge length).
// =================================================================================================
void ContactGeneratorJob::ExecuteSphereListWithSphereList(
    const SphereListWithSphereListJobDesc* lpDesc)
{
    using CgsGeometric::Sphere;

    // 0x829215CC / 0x829215D8 — one 8-byte load each on the console (a {base,count} pair is two
    // dwords there); reached by name here, so the host's wider pointer costs nothing.
    const SphereList lSphereListA = lpDesc->mSphereListA;
    const SphereList lSphereListB = lpDesc->mSphereListB;

    CollisionResultList lResultsList;
    LoadResultList(lpDesc->mpResultsList, &lResultsList);

    // 0x829215E8 `lfs f0, 0xF4(desc)` + 0x82921620 `vspltw128 v122, v0, 0` — the reach the pair
    // test adds to the two radii. Scalar here: every lane of v122 carries this same float.
    const f32 lfPadding = lpDesc->mfRadius;

    // 0x82921600 / 0x82921604 / 0x82921634 — `clrlwi ..., 16` on both counts.
    const u16 lu16NumSpheresA = static_cast<u16>(lSphereListA.miNumSpheres);
    const u16 lu16NumSpheresB = static_cast<u16>(lSphereListB.miNumSpheres);

    // 0x8292162C / 0x82921630 — both bases are hoisted out of both loops on the console.
    const Sphere* const lpaSpheresA = reinterpret_cast<const Sphere*>(lSphereListA.mpSpheres);
    const Sphere* const lpaSpheresB = reinterpret_cast<const Sphere*>(lSphereListB.mpSpheres);

    // 0x82921644 — the results buffer is the meResultType == 0 / 80-byte carve
    // (PrepareNewPrimitiveTestResultsList Mallocs 80*max), so the base is reinterpreted as
    // PrimitiveTestResult[]; CollisionResult's 112-byte stride belongs to the OTHER record type.
    PrimitiveTestResult* const lpaResults =
        reinterpret_cast<PrimitiveTestResult*>(lResultsList.mpResults);

    // 0x82921648 / 0x8292163C — the live count and the capacity, read ONCE from the local header.
    u16       lu16NumResults = lResultsList.mu16NumResults;
    const u16 lu16MaxResults = lResultsList.mu16MaxNumResults;

    // ONE stack record for the whole call (the console's var_130), refilled per accepted pair.
    // The `= {}` is what zeroes mu16TestIndex/muPad — see the banner.
    PrimitiveTestResult lRecord = {};

    for (u16 lu16SphereA = 0; lu16SphereA < lu16NumSpheresA; ++lu16SphereA)
    {
        // 0x82921678..0x82921684 — sphere A is COPIED to a stack local once per outer iteration
        // (two ld/std), then loaded as a vector; sphere B is read straight out of its array.
        const Sphere lSphereA = lpaSpheresA[lu16SphereA];

        const f32 lfRadiusA = lSphereA.mPositionRadius.w;      // vspltw v9, v10, 3

        for (u16 lu16SphereB = 0; lu16SphereB < lu16NumSpheresB; ++lu16SphereB)
        {
            const Sphere& lrSphereB = lpaSpheresB[lu16SphereB];

            const f32 lfRadiusB = lrSphereB.mPositionRadius.w; // vspltw v8, v12, 3

            // 0x829216C4 `vsubfp v11, v12, v10` — a WHOLE-VECTOR subtract, so the w lane carries
            // (rB - rA) and rides through every multiply below exactly as it does on the console.
            const f32 lfDeltaX = lrSphereB.mPositionRadius.x - lSphereA.mPositionRadius.x;
            const f32 lfDeltaY = lrSphereB.mPositionRadius.y - lSphereA.mPositionRadius.y;
            const f32 lfDeltaZ = lrSphereB.mPositionRadius.z - lSphereA.mPositionRadius.z;
            const f32 lfDeltaW = lrSphereB.mPositionRadius.w - lSphereA.mPositionRadius.w;

            // 0x829216CC / 0x829216D4 — the reach this pair is tested against.
            const f32 lfReach = (lfRadiusA + lfRadiusB) + lfPadding;

            // 0x829216D0 `vmsum3fp128` — three lanes only, broadcast to all four.
            const f32 lfSeparationSquared =
                (lfDeltaX * lfDeltaX) + (lfDeltaY * lfDeltaY) + (lfDeltaZ * lfDeltaZ);

            // ⚠️ PC LOWERING, FLAGGED: `vrsqrtefp` + 2 Newton-Raphson rounds (0x829216D8..
            // 0x82921708) is a ~23-bit reciprocal square root; `1/std::sqrt()` is exact to the
            // last ulp. Same precedent and same wording as ContactGeneratorJob.cpp:553.
            // NO GUARD: a zero separation gives +inf here exactly as the console's estimate does,
            // and the NaN that follows is caught by the IsValid assert below, not by an invented
            // early-out (see the banner).
            const f32 lfInverseLength = 1.0f / std::sqrt(lfSeparationSquared);

            // 0x8292170C — the unit A->B direction. All four lanes, w included.
            const f32 lfDirectionX = lfDeltaX * lfInverseLength;
            const f32 lfDirectionY = lfDeltaY * lfInverseLength;
            const f32 lfDirectionZ = lfDeltaZ * lfInverseLength;
            const f32 lfDirectionW = lfDeltaW * lfInverseLength;

            // 0x82921718 — record +0x10. Sphere B's separation direction is A->B.
            lRecord.mPrimitive1Normal.x = lfDirectionX;
            lRecord.mPrimitive1Normal.y = lfDirectionY;
            lRecord.mPrimitive1Normal.z = lfDirectionZ;
            lRecord.mPrimitive1Normal.w = lfDirectionW;

            // 0x8292171C `vmaddfp v9, v13, v10, v9` — A + dir*rA, the point on A's surface facing
            // B. 0x82921724 `vsubfp v12, v12, v11` — B - dir*rB, the point on B's facing A.
            // ⚠️ BOTH CONTACT POINTS LAND WITH w == 0 and that is measured, not assumed: the
            // console runs `vrlimi128 <v>, v127, 1, 0` on each (0x8292172C / 0x82921740) with
            // v127 == 0 and the immediate mask selecting the w lane, right before the store. The
            // vrlimi that PRECEDES each of those (0x82921728 / 0x82921734) inserts the PREVIOUS
            // iteration's w and is immediately overwritten by the zero — dead, and not reproduced.
            lRecord.mPrimitive0Contact.x = lSphereA.mPositionRadius.x + (lfDirectionX * lfRadiusA);
            lRecord.mPrimitive0Contact.y = lSphereA.mPositionRadius.y + (lfDirectionY * lfRadiusA);
            lRecord.mPrimitive0Contact.z = lSphereA.mPositionRadius.z + (lfDirectionZ * lfRadiusA);
            lRecord.mPrimitive0Contact.w = 0.0f;

            lRecord.mPrimitive1Contact.x = lrSphereB.mPositionRadius.x - (lfDirectionX * lfRadiusB);
            lRecord.mPrimitive1Contact.y = lrSphereB.mPositionRadius.y - (lfDirectionY * lfRadiusB);
            lRecord.mPrimitive1Contact.z = lrSphereB.mPositionRadius.z - (lfDirectionZ * lfRadiusB);
            lRecord.mPrimitive1Contact.w = 0.0f;

            // 0x82921710 + the vsel at 0x82921750: the separation is `dot3 * invLen` (== sqrt of
            // dot3), with a HARD ZERO substituted when dot3 compared equal to 0. Written as the
            // select it is, not as a plain sqrt, because the two differ exactly at dot3 == 0.
            f32 lfSeparation = lfSeparationSquared * lfInverseLength;
            if (lfSeparationSquared == 0.0f)
            {
                lfSeparation = 0.0f;
            }

            // 0x82921754 `vcmpgtfp` + 0x82921758 `vnot` — the accept test is the NEGATION of a
            // GREATER-THAN, and that is not the same as `<=` (gotcha 4): `vcmpgtfp` is FALSE for a
            // NaN operand, so `vnot` makes a NaN separation ACCEPTED. Reproduced as shipped.
            const bool lbTouching = !(lfSeparation > lfReach);
            if (!lbTouching)
            {
                continue;
            }

            // 0x8292177C `vspltisw v0,-1 ; vcfsx v0,v0,0` == -1.0f, 0x82921780 `vmulfp128` —
            // record +0x00. Sphere A's separation direction is B->A, the opposite of B's.
            lRecord.mPrimitive0Normal.x = lfDirectionX * -1.0f;
            lRecord.mPrimitive0Normal.y = lfDirectionY * -1.0f;
            lRecord.mPrimitive0Normal.z = lfDirectionZ * -1.0f;
            lRecord.mPrimitive0Normal.w = lfDirectionW * -1.0f;

            // 0x82921788 — the console's only assert in this worker. Its message is the source
            // expression, read whole out of the image at 0x821019D0.
            CGS_ASSERT(lRecord.IsValid(), "lResult.IsValid()");                          // :721

            // 0x829217EC / 0x829217F0 / 0x829217FC / 0x82921800 — the four tail fields.
            lRecord.muPrimitive0Index = lu16SphereA;   // sth r27
            lRecord.muPrimitive1Index = lu16SphereB;   // sth r31
            lRecord.muPrimitive0Tag   = 0;             // stw r30
            lRecord.muPrimitive1Tag   = 0;             // stw r30

            // 0x82921804..0x82921828 — ten ld/std at `base + 80*index` (`n + 4n` then `<< 4`).
            lpaResults[lu16NumResults] = lRecord;

            // 0x8292182C..0x82921850 — idx+1, clamped to max-1, then published into the LOCAL
            // header. An overflowing list keeps overwriting its last slot; there is no assert on
            // this path (see the banner).
            u16 lu16Next = static_cast<u16>(lu16NumResults + 1);
            if (lu16Next >= lu16MaxResults)
            {
                lu16Next = static_cast<u16>(lu16MaxResults - 1);
            }
            lu16NumResults = lu16Next;
            lResultsList.mu16NumResults = lu16NumResults;
        }
    }

    // ⭐ [DIAG] NOT IN THE X360 BINARY — wave Q7, behind BRN_PROP_DIAG, one-shot. The getenv is
    // latched ONCE (a getenv per command would be a syscall on the job thread's hot path) and the
    // line fires on the first call that actually had a sphere list to walk, so the counts describe
    // real work rather than an empty command. THERE IS NO SECOND LINE to read it against — the
    // stream arm emits no diag of its own. The honest reading: this line never firing while the
    // car-car poster is live points at DoCarCarContactGeneration /
    // AddSphereListWithSphereListToStream, not at this arm.
    if (lu16NumSpheresA != 0)
    {
        static const bool sbPropDiag  = (std::getenv("BRN_PROP_DIAG") != 0);
        static bool       sbFirstPass = true;
        if (sbPropDiag && sbFirstPass && CgsDev::Log::gpDebugPrint != 0)
        {
            sbFirstPass = false;
            *CgsDev::Log::gpDebugPrint
                << "[Q7-ss] first sphere-sphere batch: spheresA="
                << static_cast<s32>(lu16NumSpheresA)
                << " spheresB=" << static_cast<s32>(lu16NumSpheresB)
                << " results=" << static_cast<s32>(lu16NumResults)
                << "\n";
        }
    }

    // 0x8292187C..0x829218A0 — the unconditional 16-byte header write-back. This is what publishes
    // mu16NumResults to whoever harvests the command's result list.
    *lpDesc->mpResultsList = lResultsList;
}

// =================================================================================================
// ContactGeneratorJob::ExecuteSphereListWithSphereListStream @0x82923758 (100)   :429 :430
//
// Instruction-for-instruction the sphere/swept/prop stream arms with four symbols changed (the two
// assert line numbers, the Prepare and the worker). ⚠️ NO AddResult: the results travel through
// the command's own CollisionResultList, which the poster
// (BaseCollisionGenerator::AddSphereListWithSphereListToStream @0x828119F0) carved with
// PrepareNewPrimitiveTestResultsList.
//
//   0x82923770  lwz r28, 0x10(this)   -> mpJobDescription   assert :429 "No job description\n"
//   0x829237D4  lwz r11, 0(r28)       -> mpStreamProducer   assert :430 "No stream producer\n"
//               (0x1AD == 429, 0x1AE == 430 — the `li r5` immediates at 0x82923780 / 0x829237E4)
//   0x82923848  SimpleDataStreamConsumer::Construct(&consumer, producer, 0, 0)
//   0x82923858  AllocateMemory(0x80, 0x80)                    -> the command scratch
//   0x82923880  stwx  -> miMemoryRestorePoint = miAllocCursor   (AFTER the alloc, as the twins do)
//   loop:       DataStreamCommandReader::ReadCom(&consumer.mReader, command, &index)
//   0x82923890  addi r29, r31, 8       -> &cmd->mSphereListB      (command +0x08)
//               (hoisted out of the drain loop; the loop head is 0x82923894 `mr r5, r29`, which is
//                also the branch target of the `beq` — the &B computation is done once, not per
//                command. The swept sibling spells the same hoist at 0x82925338.)
//   0x8292389C  mr   r4, r31           -> &cmd->mSphereListA      (command +0x00)
//   0x82923898  lfs  f1, 0x10(r31)     -> cmd->mfPadding          (command +0x10)
//   0x829238A0  lwz  r6, 0x14(r31)     -> cmd->mpResultList       (command +0x14)
//   0x829238A8  SphereListWithSphereListJobDesc::Prepare(local, &A, &B, resultList, padding)
//   0x829238B4  ExecuteSphereListWithSphereList(&local)      (r4 == the stack-local descriptor)
//   0x829238BC  RestoreMemory()
//   0x829238DC  SimpleDataStreamConsumer::Destruct()
//
// ⚠️ THOSE FOUR COMMAND OFFSETS ARE THE CONSUMER-SIDE PROOF of the StreamCommand layout that
// CgsSphereListWithSphereListJobDesc.h models from the DWARF (h:108-113) and from the poster side.
// Both directions agree — A @+0x00, B @+0x08, padding @+0x10, result list @+0x14 on the console.
//
// ⚠️ GOTCHA 3 AT THE Prepare CALL: the padding is `f1`, so its positional GPR slot (r7) is BURNED
// and the result list travels in r6 as the THIRD integer argument. The committed
// `Prepare(const SphereList*, const SphereList*, CollisionResultList*, f32)` is exactly that
// order, so nothing needs re-ordering here — but read it before "fixing" the argument list.
//
// ⚠️ 128 IS A WHOLE ARENA SLICE, NOT A STRIDE THE RUNTIME ROUNDS TO — MEASURED, and the opposite
// of what the sibling stream arms' banners claim. The console asks for 128 bytes outright
// (`li r4, 0x80 / li r5, 0x80`) and that literal is kept here verbatim. The per-command COPY
// LENGTH is the producer's miCommandSize, which is sizeof(StreamCommand) passed through VERBATIM:
// SimpleDataStreamProducer::Construct rounds the command BUFFER TOTAL to 128 and rounds the RESULT
// stride (miAlignedResultSize) to 128, but not the per-command size; DataStreamCommandPoster::
// Construct then stores `miCommandSize = liCommandSize` and ReadCom copies exactly that many bytes
// — 24 on the console, 48 on this host. So the 128 is comfortable HEADROOM, not a round-up, and
// the static_assert in the body enforces the headroom instead of asserting it in prose.
// (⚠️ The identical false "aligned stride" doctrine is PRE-EXISTING in the three sibling stream
// arms' banners; it is wrong there too, for the same measured reason.)
// =================================================================================================
void ContactGeneratorJob::ExecuteSphereListWithSphereListStream()
{
    typedef SphereListWithSphereListStreamJobDesc Desc;

    // The console's AllocateMemory(0x80, 0x80) is kept verbatim below. ReadCom copies
    // miCommandSize == sizeof(StreamCommand) bytes into it (24 console / 48 host), so 128 is
    // headroom — enforce that rather than trusting the banner.
    static_assert(sizeof(Desc::StreamCommand) <= 128,
                  "the console's 128-byte command scratch must hold a whole StreamCommand");

    const Desc* lpDesc = static_cast<const Desc*>(mpJobDescription);

    CGS_ASSERT(lpDesc != NULL, "No job description\n");                      // :429
    CGS_ASSERT(lpDesc->GetStreamProducer() != NULL, "No stream producer\n"); // :430

    CgsMemory::SimpleDataStreamConsumer lConsumer;
    lConsumer.Construct(lpDesc->GetStreamProducer(), NULL, 0);

    Desc::StreamCommand* lpCommand =
        static_cast<Desc::StreamCommand*>(AllocateMemory(128, 128));

    miMemoryRestorePoint = miAllocCursor;

    u32 luCommandIndex = 0;
    while (lConsumer.ReadCo(lpCommand, &luCommandIndex) == 0)
    {
        SphereListWithSphereListJobDesc lLocalDesc;
        lLocalDesc.Prepare(&lpCommand->mSphereListA, &lpCommand->mSphereListB,
                           lpCommand->mpResultList, lpCommand->mfPadding);

        ExecuteSphereListWithSphereList(&lLocalDesc);

        RestoreMemory();
    }

    lConsumer.Destruct();
}
