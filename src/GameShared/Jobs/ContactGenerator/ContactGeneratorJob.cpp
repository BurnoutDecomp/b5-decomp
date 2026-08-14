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
//   LoadPrimitives                      @0x829210F0   (61)  :1405 :1406
//   LoadResultList                      @0x829211E8   (46)  :1552
//   AllocateMemory                      @0x829212A0   (54)  :1720
//   RestoreMemory                       @0x82921050   (39)  ContactGeneratorJob.h:167
//
// Until this TU existed, the triangle cache filled with real Paradise City geometry every frame
// and NOTHING READ IT. This is the reader.
//
// ─── WHY THE OTHER TEN ARMS ARE GATES AND NOT DELETIONS ──────────────────────────────────────
// `Execute` is a 12-way jump table over the descriptor's type byte. Only type 16
// (E_COLLISIONJOB_LINE_WITH_TRIANGLE_LIST_STREAM) is reconstructed here, because only type 16 is
// posted by anything in this tree. The other eleven cases are kept as NAMED one-shot boot gates
// rather than folded into the `default:` assert, for one measured reason: `xrefs_to` on
// ContactGeneratorJob::Execute @0x829267E0 shows a SINGLE caller (ContactGeneratorEntry
// @0x82920F10), and every one of the seven `BaseCollisionGenerator::Run*` dispatchers points its
// batches at that same entry. So the day any other Run* is bodied, its descriptor arrives here
// and the gate NAMES the missing arm instead of tripping a generic "Unsupported collision job".
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
#include "GameShared/GameClasses/Geometric/Primitives/CgsTriangle4.h"       // Triangle4 (the SoA block)
#include "GameShared/GameClasses/Geometric/Intersection/CgsTriangleSphere.h" // the sphere contact kernel
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsCollisionJobDescription.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsLineWithTriangleListStreamJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithTriangleListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"

#include <cmath>     // std::sqrt (the vrsqrtefp lowering)
#include <cstring>   // std::memcpy (reading a lane's raw bit pattern)

// The six per-job-thread contexts. X360 base unk_831BBF80, stride 0x10300, bound 6 --
// all three read out of ContactGeneratorEntry @0x82920F10 (see ContactGenerator.cpp).
ContactGeneratorJob gaContactGeneratorJobs[KI_NUM_CONTACT_GENERATOR_JOBS];

using CgsSceneManager::CgsCollision::CollisionJobDescription;
using CgsSceneManager::CgsCollision::CollisionResultList;
using CgsSceneManager::CgsCollision::LineWithTriangleListStreamJobDesc;
using CgsSceneManager::CgsCollision::PrimitiveTestResult;
using CgsSceneManager::CgsCollision::SphereListWithTriangleListJobDesc;
using CgsSceneManager::CgsCollision::SphereListWithTriangleListStreamJobDesc;
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
    const f32 KF_MIN_DETERMINANT = 0.0f;         // unk_8321D330, all four lanes
    const f32 KF_BARYCENTRIC_TOLERANCE = 0.0f;   // unk_8321D310, all four lanes

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
        case 7:  ExecuteSphereListWithSphereList();             break;
        case 8:  ExecuteSphereListWithSphereListStream();       break;
        case 9:  ExecuteBoxListWithTriangleList();              break;
        case 10: ExecutePrimitivePairList();                    break;
        case 11: ExecutePrimitiveListWithTriangleList();        break;
        case 12: ExecutePrimitiveListWithTriangleListStream();  break;
        case 13: ExecuteSweptSphereListWithTriangleList();      break;
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
// command's CollisionResultList. The list is what EndVehicleContactGeneration (still a gate)
// will harvest.
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
    // harvest (EndVehicleContactGeneration) lands and makes contacts visible downstream.
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
// The other eight Execute arms -- NAMED BOOT GATES, not silent returns and not a shared default.
// Each names its own X360 home and insn count so that the day something posts that descriptor
// type, the log says which worker is missing rather than "unsupported".
// =============================================================================================
#define BRN_CONTACT_JOB_GATE(TEXT)                                                       \
    do { static bool s_bLogged = false;                                                  \
    if (!s_bLogged) { s_bLogged = true;                                                  \
        if (CgsDev::Message::gxMessageFilterFlags & 1)                                   \
            *CgsDev::Log::gpDebugPrint << TEXT; } } while (0)

void ContactGeneratorJob::ExecuteSphereListWithSphereList()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecuteSphereListWithSphereList "
                         "@0x829215B0 not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecuteSphereListWithSphereListStream()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecuteSphereListWithSphereList"
                         "Stream @0x82923758 not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecuteBoxListWithTriangleList()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecuteBoxListWithTriangleList "
                         "@0x829218B8 (44) not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecutePrimitivePairList()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecutePrimitivePairList "
                         "@0x82925798 (92) not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecutePrimitiveListWithTriangleList()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecutePrimitiveListWithTriangleList "
                         "@0x82925908 not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecutePrimitiveListWithTriangleListStream()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecutePrimitiveListWithTriangle"
                         "ListStream @0x82926650 (100) not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecuteSweptSphereListWithTriangleList()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecuteSweptSphereListWithTriangle"
                         "List @0x829238E8 not reconstructed [FLAG PC boot gate]\n");
}

void ContactGeneratorJob::ExecuteSweptSphereListWithTriangleListStream()
{
    BRN_CONTACT_JOB_GATE("conductor gate: ContactGeneratorJob::ExecuteSweptSphereListWithTriangle"
                         "ListStream @0x82925238 (100) not reconstructed [FLAG PC boot gate]\n");
}

#undef BRN_CONTACT_JOB_GATE
