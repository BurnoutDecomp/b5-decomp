// ============================================================================
// GameSource/Effects/Particles/Native/BrnDebrisArrayLite.cpp
//
// THE DEBRIS SIMULATION's job side (FX-CRASHVFX, crash parity 2026-09-25) -- see the header for the
// chain and the one scheduling difference. Reconstructed from the ARTIST words:
//   DebrisUpdateJob::Execute          @0x82C08298
//   BrnDebrisArrayLite::Update        @0x82C08B58
//   the per-bucket integrator         sub_82C08410 (UpdateDebrisBucket below; an export hole,
//                                     disassembled from the raw image with tools/re/ppcdis.py)
//   FXBucket<BrnDebris,32> particle   sub_82C08378 (the checked `&maParticleData[i]`)
// Replaces the former "HONEST STUB" (a bare CGS_ASSERT with an invented six-int signature) that was
// never mounted.
//
// Every literal is the image's: the gravity splat unk_832BAD30 (0, -9.8, 0, 0) is CRT-initialised
// .bss (thunk 0x82C74718, RUN by the test's generator to read it); the per-lane lifetimes
// flt_82F93D08 {7.0, 9.00000095, 7.99999905, 10.0} are image-initialised .data (the same table
// BrnDebrisRenderer::RenderDebrisArray fades against); the contact fraction flt_82004FDC 0.95, the
// bounce factor range flt_82188C30 0.4 over flt_82004D00 0.6, and the 0.5 / 1.0 / 2.0 are
// immediates (vcsxwfp128 / vspltisw) or flt_82001DA0 / flt_82001C98.
// Every fused op is spelled fused (vmaddfp / vmaddcfp128 / vnmsubfp128 round once); vnmsubfp128 is
// vD = -(vA * vB - vD). tests/run_fxcrashvfx_debris_sim.py pins the whole job against the console's
// own words on emu64 (scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu/gen_debris_sim_data.py).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnDebrisArrayLite.h"
#include "GameSource/Effects/BrnCrashTriangleCache.h"               // BrnCrashLineTriangleCacheFormat / CollideWithTriangleCache
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT

#include <cmath>   // std::fma, std::sqrt

namespace BrnParticle
{
namespace Native
{
    // [DIAG] NOT IN THE X360 BINARY. DELETE-WHEN-STABLE. See the header.
    u32 gauDebrisSimIntegrated   = 0;
    u32 gauDebrisSimCollideCalls = 0;
    u32 gauDebrisSimBounces      = 0;

    namespace
    {
        typedef BrnDebrisArray::DebrisBucket DebrisBucket;

        // unk_832BAD30 -- CRT-initialised by thunk 0x82C74718: (0, -9.8, 0, 0).
        const f32 KF_DEBRIS_GRAVITY_X = 0.0f;
        const f32 KF_DEBRIS_GRAVITY_Y = -9.80000019f;                 // 0xC11CCCCD
        const f32 KF_DEBRIS_GRAVITY_Z = 0.0f;
        const f32 KF_DEBRIS_GRAVITY_W = 0.0f;

        // flt_82F93D08 -- the per-lane lifetimes, indexed by (particle index & 3).
        const f32 KAF_DEBRIS_SIM_LIFETIME[4] = { 7.0f, 9.00000095f, 7.99999905f, 10.0f };

        // flt_82004FDC -- a hit moves the chunk to 95% of the way to the contact point.
        const f32 KF_DEBRIS_CONTACT_FRACTION = 0.949999988f;
        // flt_82188C30 / flt_82004D00 -- each bounce keeps a random 60%..100% of its speed.
        const f32 KF_DEBRIS_BOUNCE_FACTOR_RANGE = 0.399999976f;
        const f32 KF_DEBRIS_BOUNCE_FACTOR_MIN   = 0.600000024f;
        // flt_82001DA0 -- the lateral kick's centre (draw - 0.5).
        const f32 KF_DEBRIS_KICK_CENTRE = 0.5f;

        // vnmsubfp128: -(a*b - d), rounded ONCE, then NEGATED -- zeros too (a QNaN keeps its sign).
        inline f32 SimVnmsub(f32 lfA, f32 lfB, f32 lfD)
        {
            const f32 lfDifference = std::fma(lfA, lfB, -lfD);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // vmsum3fp128: one rounding of the exact three-term dot (FLAG (model): the double sum is
        // exact for the products of two floats unless their exponents are far apart).
        template <class TA, class TB>
        inline f32 SimDot3(const TA& lrA, const TB& lrB)
        {
            return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                                  + static_cast<f64>(lrA.z) * lrB.z);
        }

        // The distance a chunk moved this step: vmsum3fp128, vrsqrtefp + two Newton steps with the
        // 0.5 splat, x * estimate, and the vcmpeqfp / vsel that maps a zero step to 0.
        // FLAG (model): the hardware estimate is taken as its correctly rounded value.
        inline f32 SimGuardedLength3(const Vector3& lrv)
        {
            const f32 lfSquared = SimDot3(lrv, lrv);
            if (lfSquared == 0.0f)
                return 0.0f;
            f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfSquared)));
            for (u32 luStep = 0; luStep < 2u; ++luStep)
            {
                const f32 lfEstimateSquared = lfEstimate * lfEstimate;       // vmulfp128
                const f32 lfHalf            = lfEstimate * 0.5f;             // vmulfp128 by the 0.5 splat
                lfEstimate = std::fma(lfHalf, SimVnmsub(lfSquared, lfEstimateSquared, 1.0f), lfEstimate);
            }
            return lfSquared * lfEstimate;
        }

        // sub_82C08378 -- FXBucket<BrnDebris,32>'s checked particle accessor (FXBuckets.h:0x8F).
        inline BrnDebris& DebrisToUpdate(DebrisBucket* lpBucket, u32 luParticle)
        {
            CGS_ASSERT(luParticle < DebrisBucket::KuMaxNumParticles, "luParticle < KuMaxNumParticles");
            return lpBucket->maParticleData[luParticle];
        }

        // =============================================================================================
        // sub_82C08410 -- integrate one bucket (BrnDebrisRenderer.cpp:0x300..0x35C by its asserts).
        // Returns the console's r3: 0 when nothing in the bucket was live, 1 otherwise.
        // =============================================================================================
        bool UpdateDebrisBucket(DebrisBucket* lpBucket,
                                const BrnEffects::BrnCrashTriangleCache* lpTriCache,
                                f32 lfCurrentTime,
                                bool lbCollisionEnabled,
                                CgsNumeric::Random* lpRandom,
                                f32 lfTimeStep,
                                const Vector3& lrBounciness,
                                f32 lfDragResistance)
        {
            CGS_ASSERT(lpBucket != 0, "lpBucket != NULL");
            CGS_ASSERT((reinterpret_cast<uintptr_t>(lpBucket) & 0xF) == 0, "( (uint32_t)lpBucket & 0xf ) == 0");
            CGS_ASSERT(lpTriCache != 0, "lpTriCache != NULL");
            CGS_ASSERT(lfTimeStep > 0.0f, "lfTimeStep > 0.0f");

            // v123 = 1 - drag * dt (the per-step velocity damping), v124 = gravity * dt.
            const f32 lfDamping = 1.0f - lfDragResistance * lfTimeStep;
            Vector3 lvGravityStep;
            lvGravityStep.x = KF_DEBRIS_GRAVITY_X * lfTimeStep;
            lvGravityStep.y = KF_DEBRIS_GRAVITY_Y * lfTimeStep;
            lvGravityStep.z = KF_DEBRIS_GRAVITY_Z * lfTimeStep;
            lvGravityStep.w = KF_DEBRIS_GRAVITY_W * lfTimeStep;

            // The live particles' indices (var_1370 bytes) and their line segments (var_1310, stride 0x30).
            u8 lau8LiveIndex[DebrisBucket::KuMaxNumParticles];
            BrnEffects::BrnCrashLineTriangleCacheFormat laLines[DebrisBucket::KuMaxNumParticles];
            u32 luNumLive = 0;

            for (u32 luIndex = 0;; luIndex = (luIndex + 1) & 0xFF)   // the index is a byte (clrlwi 24)
            {
                CGS_ASSERT(lpBucket->mu16NumberOfParticlesInBucket <= DebrisBucket::KuMaxNumParticles,
                           "mu16NumberOfParticlesInBucket <= KuMaxNumParticles");
                if (luIndex >= lpBucket->mu16NumberOfParticlesInBucket)
                    break;

                const f32 lfLifetime = KAF_DEBRIS_SIM_LIFETIME[luIndex & 3u];
                const f32 lfAge      = lfCurrentTime - lpBucket->maParticleBirthTimes[luIndex];
                BrnDebris& lDebrisToUpdate = DebrisToUpdate(lpBucket, luIndex);

                // `fcmpu f31, f29(0.0) ; ble` (0x82C08758 / 0x82C0875C) then `fcmpu f31, lifetime ; bgt`
                // (0x82C08760 / 0x82C08764). `ble` is TAKEN on an unordered compare, so a NaN age is SKIPPED by the
                // first test; `bgt` is not taken, so the lifetime test alone would let it through.
                // CORRECTED 2026-09-25 (FX-CRASHVFX, REVIEW-I on 858d86d7): this read `lfAge <= 0.0f`, which is
                // false for a NaN, so a NaN-aged piece was integrated.
                if (!(lfAge > 0.0f) || lfAge > lfLifetime || lDebrisToUpdate.muBounceCount == 0)
                    continue;

                lau8LiveIndex[luNumLive] = static_cast<u8>(luIndex);
                BrnEffects::BrnCrashLineTriangleCacheFormat& lrLine = laLines[luNumLive];
                ++luNumLive;
                ++gauDebrisSimIntegrated;   // [DIAG] DELETE-WHEN-STABLE

                const rw::math::vpu::Vector3Plus lvPosition = lDebrisToUpdate.mPositionPlusRotVel;
                const rw::math::vpu::Vector3Plus lvVelocity = lDebrisToUpdate.mVelocityPlusScale;

                // velocity (+ gravity step), then damped: vaddfp128, vmulfp128.
                Vector3 lvDamped;
                lvDamped.x = (lvVelocity.x + lvGravityStep.x) * lfDamping;
                lvDamped.y = (lvVelocity.y + lvGravityStep.y) * lfDamping;
                lvDamped.z = (lvVelocity.z + lvGravityStep.z) * lfDamping;
                lvDamped.w = (lvVelocity.w + lvGravityStep.w) * lfDamping;

                // new position = damped * dt + old (ONE vmaddcfp128).
                Vector3 lvNewPosition;
                lvNewPosition.x = std::fma(lvDamped.x, lfTimeStep, lvPosition.x);
                lvNewPosition.y = std::fma(lvDamped.y, lfTimeStep, lvPosition.y);
                lvNewPosition.z = std::fma(lvDamped.z, lfTimeStep, lvPosition.z);
                lvNewPosition.w = std::fma(lvDamped.w, lfTimeStep, lvPosition.w);

                lrLine.mLineStartPosition.x = lvPosition.x;   // the whole quadword, w (the spin rate) too
                lrLine.mLineStartPosition.y = lvPosition.y;
                lrLine.mLineStartPosition.z = lvPosition.z;
                lrLine.mLineStartPosition.w = lvPosition.w;
                lrLine.mLineEndPos          = lvNewPosition;
                lrLine.mLineIntersectNormalPlusLineParms.x = 0.0f;   // {0, 0, 0, 1}: vspltisw 0 + vrlimi of 1.0
                lrLine.mLineIntersectNormalPlusLineParms.y = 0.0f;
                lrLine.mLineIntersectNormalPlusLineParms.z = 0.0f;
                lrLine.mLineIntersectNormalPlusLineParms.w = 1.0f;

                // The spin: angle += rotational velocity * |step| (ONE vmaddfp).
                Vector3 lvStep;
                lvStep.x = lvNewPosition.x - lvPosition.x;
                lvStep.y = lvNewPosition.y - lvPosition.y;
                lvStep.z = lvNewPosition.z - lvPosition.z;
                lvStep.w = lvNewPosition.w - lvPosition.w;
                const f32 lfDistance = SimGuardedLength3(lvStep);
                lDebrisToUpdate.mAxisPlusAngle.w =
                    std::fma(lvPosition.w, lfDistance, lDebrisToUpdate.mAxisPlusAngle.w);

                // Position / velocity written back with their w lanes (rotational velocity, scale) kept.
                lDebrisToUpdate.mPositionPlusRotVel.x = lvNewPosition.x;
                lDebrisToUpdate.mPositionPlusRotVel.y = lvNewPosition.y;
                lDebrisToUpdate.mPositionPlusRotVel.z = lvNewPosition.z;
                lDebrisToUpdate.mVelocityPlusScale.x  = lvDamped.x;
                lDebrisToUpdate.mVelocityPlusScale.y  = lvDamped.y;
                lDebrisToUpdate.mVelocityPlusScale.z  = lvDamped.z;
            }

            if (luNumLive == 0)
                return false;
            if (!lbCollisionEnabled || lpTriCache->IsEmpty())
                return true;

            lpTriCache->CollideWithTriangleCache(laLines, luNumLive);
            ++gauDebrisSimCollideCalls;   // [DIAG] DELETE-WHEN-STABLE

            for (u32 luLine = 0; luLine < luNumLive; ++luLine)
            {
                const BrnEffects::BrnCrashLineTriangleCacheFormat& lrLine = laLines[luLine];
                const rw::math::vpu::Vector3Plus& lrHit = lrLine.mLineIntersectNormalPlusLineParms;
                // vcmpeqfp128. of (x, y, z, x) against zero: no normal written == no hit.
                if (lrHit.x == 0.0f && lrHit.y == 0.0f && lrHit.z == 0.0f)
                    continue;

                BrnDebris& lDebrisToUpdate = DebrisToUpdate(lpBucket, lau8LiveIndex[luLine]);
                CGS_ASSERT(lDebrisToUpdate.muBounceCount != 0, "lDebrisToUpdate.muBounceCount != 0");

                // Back to 95% of the way to the contact (ONE vmaddfp), the spin rate kept.
                const f32 lfFraction = lrHit.w * KF_DEBRIS_CONTACT_FRACTION;
                lDebrisToUpdate.mPositionPlusRotVel.x =
                    std::fma(lrLine.mLineEndPos.x - lrLine.mLineStartPosition.x, lfFraction, lrLine.mLineStartPosition.x);
                lDebrisToUpdate.mPositionPlusRotVel.y =
                    std::fma(lrLine.mLineEndPos.y - lrLine.mLineStartPosition.y, lfFraction, lrLine.mLineStartPosition.y);
                lDebrisToUpdate.mPositionPlusRotVel.z =
                    std::fma(lrLine.mLineEndPos.z - lrLine.mLineStartPosition.z, lfFraction, lrLine.mLineStartPosition.z);

                // Reflect: v - (2n) * (n . v) -- vmsum3fp128, two vmulfp128, vsubfp.
                rw::math::vpu::Vector3Plus& lrVelocity = lDebrisToUpdate.mVelocityPlusScale;
                const f32 lfInto = SimDot3(lrHit, lrVelocity);
                lrVelocity.x = lrVelocity.x - (lrHit.x * 2.0f) * lfInto;
                lrVelocity.y = lrVelocity.y - (lrHit.y * 2.0f) * lfInto;
                lrVelocity.z = lrVelocity.z - (lrHit.z * 2.0f) * lfInto;

                // Lose energy: the array's per-axis bounciness, then a random 60%..100% (fmadds).
                const f32 lfFactor = std::fma(lpRandom->RandomFloat(), KF_DEBRIS_BOUNCE_FACTOR_RANGE,
                                              KF_DEBRIS_BOUNCE_FACTOR_MIN);
                lrVelocity.x = (lrVelocity.x * lrBounciness.x) * lfFactor;
                lrVelocity.y = (lrVelocity.y * lrBounciness.y) * lfFactor;
                lrVelocity.z = (lrVelocity.z * lrBounciness.z) * lfFactor;

                // A random sideways kick scaled by the rebound speed: (drawX - 0.5, 0, drawZ - 0.5) * vy
                // + v (ONE vmaddfp). The console draws z FIRST (var_68), then x (var_60).
                const f32 lfDrawZ = lpRandom->RandomFloat();
                const f32 lfDrawX = lpRandom->RandomFloat();
                const f32 lfUp    = lrVelocity.y;
                lrVelocity.x = std::fma(lfDrawX - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.x);
                lrVelocity.y = std::fma(KF_DEBRIS_KICK_CENTRE - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.y);
                lrVelocity.z = std::fma(lfDrawZ - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.z);

                lDebrisToUpdate.muBounceCount = static_cast<u8>(lDebrisToUpdate.muBounceCount - 1u);
                ++gauDebrisSimBounces;   // [DIAG] DELETE-WHEN-STABLE
            }
            return true;
        }
    }

    // =================================================================================================
    // BrnDebrisArrayLite::Update @0x82C08B58 -- every live bucket, in list order (bucket->mpNextBucket).
    // =================================================================================================
    void BrnDebrisArrayLite::Update(const BrnEffects::BrnCrashTriangleCache* lpTriCache, f32 lfTimeStep,
                                    f32 lfCurrentTime, CgsNumeric::Random* lpRandom)
    {
        for (DebrisBucket* lpBucket = mpBucketList; lpBucket != 0;
             lpBucket = static_cast<DebrisBucket*>(lpBucket->mpNextBucket))
        {
            UpdateDebrisBucket(lpBucket, lpTriCache, lfCurrentTime, mbCollisionEnabled, lpRandom, lfTimeStep,
                               mBounciness, mfDragResistance);
        }
    }

    // =================================================================================================
    // DebrisUpdateJob::Execute @0x82C08298 -- every array of the job (DebrisUpdateJob.cpp:0x2F).
    // =================================================================================================
    void DebrisUpdateJob::Execute(DebrisUpdateJobData* lpJobData)
    {
        const u32 luNumArrays = lpJobData->muNumDebrisArrays;
        CGS_ASSERT(luNumArrays > 0 && luNumArrays <= DebrisUpdateJobData::KU_NUM_DEBRIS_ARRAYS_PER_JOB,
                   "( luNumArrays > 0 ) && ( luNumArrays <= DebrisUpdateJobData::KU_NUM_DEBRIS_ARRAYS_PER_JOB )");
        for (u32 luArray = 0; luArray < luNumArrays; ++luArray)
        {
            lpJobData->maDebrisArrays[luArray].Update(lpJobData->mpTriCache, lpJobData->mfTimeStep,
                                                      lpJobData->mfCurrentTime, &lpJobData->mRandom);
        }
    }
}
}
