#include "GameSource/Effects/Particles/Native/BrnParticleMath.h"
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
            return BrnParticleMath::VpuNegativeMultiplySubtract(lfA, lfB, lfD);
        }

        // vmsum3fp128: one rounding of the exact three-term dot (FLAG (model): the double sum is
        // exact for the products of two floats unless their exponents are far apart).
        template <class TA, class TB>
        inline f32 SimDot3(const TA& lrA, const TB& lrB)
        {
            return static_cast<f32>(static_cast<f64>(lrA.mV.v[0]) * lrB.mV.v[0] + static_cast<f64>(lrA.mV.v[1]) * lrB.mV.v[1]
                                  + static_cast<f64>(lrA.mV.v[2]) * lrB.mV.v[2]);
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
                lfEstimate = BrnParticleMath::VpuMultiplyAdd(lfHalf, SimVnmsub(lfSquared, lfEstimateSquared, 1.0f), lfEstimate);
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
            lvGravityStep.mV.v[0] = KF_DEBRIS_GRAVITY_X * lfTimeStep;
            lvGravityStep.mV.v[1] = KF_DEBRIS_GRAVITY_Y * lfTimeStep;
            lvGravityStep.mV.v[2] = KF_DEBRIS_GRAVITY_Z * lfTimeStep;
            lvGravityStep.mV.v[3] = KF_DEBRIS_GRAVITY_W * lfTimeStep;

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

                const rw::math::vpu::Vector3Plus lvPosition = lDebrisToUpdate.mPositionPlusRotVel;
                const rw::math::vpu::Vector3Plus lvVelocity = lDebrisToUpdate.mVelocityPlusScale;

                // velocity (+ gravity step), then damped: vaddfp128, vmulfp128.
                Vector3 lvDamped;
                lvDamped.mV.v[0] = (lvVelocity.mV.v[0] + lvGravityStep.mV.v[0]) * lfDamping;
                lvDamped.mV.v[1] = (lvVelocity.mV.v[1] + lvGravityStep.mV.v[1]) * lfDamping;
                lvDamped.mV.v[2] = (lvVelocity.mV.v[2] + lvGravityStep.mV.v[2]) * lfDamping;
                lvDamped.mV.v[3] = (lvVelocity.mV.v[3] + lvGravityStep.mV.v[3]) * lfDamping;

                // new position = damped * dt + old (ONE vmaddcfp128).
                Vector3 lvNewPosition;
                lvNewPosition.mV.v[0] = BrnParticleMath::VpuMultiplyAdd(lvDamped.mV.v[0], lfTimeStep, lvPosition.mV.v[0]);
                lvNewPosition.mV.v[1] = BrnParticleMath::VpuMultiplyAdd(lvDamped.mV.v[1], lfTimeStep, lvPosition.mV.v[1]);
                lvNewPosition.mV.v[2] = BrnParticleMath::VpuMultiplyAdd(lvDamped.mV.v[2], lfTimeStep, lvPosition.mV.v[2]);
                lvNewPosition.mV.v[3] = BrnParticleMath::VpuMultiplyAdd(lvDamped.mV.v[3], lfTimeStep, lvPosition.mV.v[3]);

                lrLine.mLineStartPosition.mV.v[0] = lvPosition.mV.v[0];   // the whole quadword, w (the spin rate) too
                lrLine.mLineStartPosition.mV.v[1] = lvPosition.mV.v[1];
                lrLine.mLineStartPosition.mV.v[2] = lvPosition.mV.v[2];
                lrLine.mLineStartPosition.mV.v[3] = lvPosition.mV.v[3];
                lrLine.mLineEndPos          = lvNewPosition;
                lrLine.mLineIntersectNormalPlusLineParms.mV.v[0] = 0.0f;   // {0, 0, 0, 1}: vspltisw 0 + vrlimi of 1.0
                lrLine.mLineIntersectNormalPlusLineParms.mV.v[1] = 0.0f;
                lrLine.mLineIntersectNormalPlusLineParms.mV.v[2] = 0.0f;
                lrLine.mLineIntersectNormalPlusLineParms.mV.v[3] = 1.0f;

                // The spin: angle += rotational velocity * |step| (ONE vmaddfp).
                Vector3 lvStep;
                lvStep.mV.v[0] = lvNewPosition.mV.v[0] - lvPosition.mV.v[0];
                lvStep.mV.v[1] = lvNewPosition.mV.v[1] - lvPosition.mV.v[1];
                lvStep.mV.v[2] = lvNewPosition.mV.v[2] - lvPosition.mV.v[2];
                lvStep.mV.v[3] = lvNewPosition.mV.v[3] - lvPosition.mV.v[3];
                const f32 lfDistance = SimGuardedLength3(lvStep);
                lDebrisToUpdate.mAxisPlusAngle.mV.v[3] =
                    BrnParticleMath::VpuMultiplyAdd(lvPosition.mV.v[3], lfDistance, lDebrisToUpdate.mAxisPlusAngle.mV.v[3]);

                // Position / velocity written back with their w lanes (rotational velocity, scale) kept.
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[0] = lvNewPosition.mV.v[0];
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[1] = lvNewPosition.mV.v[1];
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[2] = lvNewPosition.mV.v[2];
                lDebrisToUpdate.mVelocityPlusScale.mV.v[0]  = lvDamped.mV.v[0];
                lDebrisToUpdate.mVelocityPlusScale.mV.v[1]  = lvDamped.mV.v[1];
                lDebrisToUpdate.mVelocityPlusScale.mV.v[2]  = lvDamped.mV.v[2];
            }

            if (luNumLive == 0)
                return false;
            if (!lbCollisionEnabled || lpTriCache->IsEmpty())
                return true;

            lpTriCache->CollideWithTriangleCache(laLines, luNumLive);

            for (u32 luLine = 0; luLine < luNumLive; ++luLine)
            {
                const BrnEffects::BrnCrashLineTriangleCacheFormat& lrLine = laLines[luLine];
                const rw::math::vpu::Vector3Plus& lrHit = lrLine.mLineIntersectNormalPlusLineParms;
                // vcmpeqfp128. of (x, y, z, x) against zero: no normal written == no hit.
                if (lrHit.mV.v[0] == 0.0f && lrHit.mV.v[1] == 0.0f && lrHit.mV.v[2] == 0.0f)
                    continue;

                BrnDebris& lDebrisToUpdate = DebrisToUpdate(lpBucket, lau8LiveIndex[luLine]);
                CGS_ASSERT(lDebrisToUpdate.muBounceCount != 0, "lDebrisToUpdate.muBounceCount != 0");

                // Back to 95% of the way to the contact (ONE vmaddfp), the spin rate kept.
                const f32 lfFraction = lrHit.mV.v[3] * KF_DEBRIS_CONTACT_FRACTION;
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[0] =
                    BrnParticleMath::VpuMultiplyAdd(lrLine.mLineEndPos.mV.v[0] - lrLine.mLineStartPosition.mV.v[0], lfFraction, lrLine.mLineStartPosition.mV.v[0]);
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[1] =
                    BrnParticleMath::VpuMultiplyAdd(lrLine.mLineEndPos.mV.v[1] - lrLine.mLineStartPosition.mV.v[1], lfFraction, lrLine.mLineStartPosition.mV.v[1]);
                lDebrisToUpdate.mPositionPlusRotVel.mV.v[2] =
                    BrnParticleMath::VpuMultiplyAdd(lrLine.mLineEndPos.mV.v[2] - lrLine.mLineStartPosition.mV.v[2], lfFraction, lrLine.mLineStartPosition.mV.v[2]);

                // Reflect: v - (2n) * (n . v) -- vmsum3fp128, two vmulfp128, vsubfp.
                rw::math::vpu::Vector3Plus& lrVelocity = lDebrisToUpdate.mVelocityPlusScale;
                const f32 lfInto = SimDot3(lrHit, lrVelocity);
                lrVelocity.mV.v[0] = lrVelocity.mV.v[0] - (lrHit.mV.v[0] * 2.0f) * lfInto;
                lrVelocity.mV.v[1] = lrVelocity.mV.v[1] - (lrHit.mV.v[1] * 2.0f) * lfInto;
                lrVelocity.mV.v[2] = lrVelocity.mV.v[2] - (lrHit.mV.v[2] * 2.0f) * lfInto;

                // Lose energy: the array's per-axis bounciness, then a random 60%..100% (fmadds).
                const f32 lfFactor = BrnParticleMath::ScalarMultiplyAdd(lpRandom->RandomFloat(), KF_DEBRIS_BOUNCE_FACTOR_RANGE,
                                              KF_DEBRIS_BOUNCE_FACTOR_MIN);
                lrVelocity.mV.v[0] = (lrVelocity.mV.v[0] * lrBounciness.mV.v[0]) * lfFactor;
                lrVelocity.mV.v[1] = (lrVelocity.mV.v[1] * lrBounciness.mV.v[1]) * lfFactor;
                lrVelocity.mV.v[2] = (lrVelocity.mV.v[2] * lrBounciness.mV.v[2]) * lfFactor;

                // A random sideways kick scaled by the rebound speed: (drawX - 0.5, 0, drawZ - 0.5) * vy
                // + v (ONE vmaddfp). The console draws z FIRST (var_68), then x (var_60).
                const f32 lfDrawZ = lpRandom->RandomFloat();
                const f32 lfDrawX = lpRandom->RandomFloat();
                const f32 lfUp    = lrVelocity.mV.v[1];
                lrVelocity.mV.v[0] = BrnParticleMath::VpuMultiplyAdd(lfDrawX - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.mV.v[0]);
                lrVelocity.mV.v[1] = BrnParticleMath::VpuMultiplyAdd(KF_DEBRIS_KICK_CENTRE - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.mV.v[1]);
                lrVelocity.mV.v[2] = BrnParticleMath::VpuMultiplyAdd(lfDrawZ - KF_DEBRIS_KICK_CENTRE, lfUp, lrVelocity.mV.v[2]);

                lDebrisToUpdate.muBounceCount = static_cast<u8>(lDebrisToUpdate.muBounceCount - 1u);
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
