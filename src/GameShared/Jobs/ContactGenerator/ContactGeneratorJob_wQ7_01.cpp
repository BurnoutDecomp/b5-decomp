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

#include "GameShared/Jobs/ContactGenerator/ContactGeneratorJob.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint ([DIAG])
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"          // CgsGeometric::Sphere
#include "GameShared/GameClasses/Memory/DataStream/CgsSimpleDataStreamConsumer.h"
#include "GameShared/GameClasses/SceneManager/Collision/ContactGenerator/JobDescription/CgsSphereListWithSphereListJobDesc.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsCollisionResult.h"
#include "GameShared/GameClasses/SceneManager/Collision/Primitives/CgsSphereList.h"

#include <cmath>     // std::sqrt (the vrsqrtefp + 2 NR lowering)
#include <cstdlib>   // std::getenv (the BRN_PROP_DIAG latch)

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
