// =================================================================================================
// GameSource/Effects/Particles/ParticleModule_DebrisSpawn.cpp
//
//   BrnParticle::ParticleModule::SpawnDebris -- THE DEBRIS PRODUCER.
//
// The update-thread end of the debris chain, and the reason nothing ever spawned: every other
// link was written and this one had no body anywhere in the tree.
//
//   <a producer: JumpStateMachine::FireWheelDebris / EffectsModule::HandleCrashingTrail /
//                EffectsModule::BurstAreaEmitParticles>
//     -> ParticleModule::SpawnDebris                  (HERE)
//        -> the 32-entry spawn buffer (mu16SpawnBufferCount / mpSparkSpawnBuffer)
//        -> on the 32nd record: mInterThreadEventQueue.AllocateEventSafe(type 4, 32*80 + 16)
//   ParticleModule::PreRenderUpdate                   -- publishes any part-full remainder
//   ParticleModule::ProcessEventQueue, case 4         -- walks the batch
//     -> BrnDebrisArray::SpawnDebris
//
// THE RECORD is DebrisBatchSpawnEvent::DebrisSpawnData, 80 bytes, and this function writes all
// five of its fields:
//   +0x00 mvPositionPlusSize                {lvPosition.xyz,     lfSize}
//   +0x10 mvVelocityPlusSpawnTime           {lvVelocity.xyz,     lfSpawnTime}
//   +0x20 mvRotationAxisPlusRotationAmount  {lvRotationAxis.xyz, mRandom.RandomFloat(1, 4)}
//   +0x30 mvColour
//   +0x40 meType
// The original spells each of the first three as a read-modify-write pair (insert the xyz lanes,
// store, reload through a stack quad to drop the w scalar in, store again); the net effect is the
// brace-init below -- one 16-byte record write either way.
//
// THE ROTATION AMOUNT IS A RING DRAW, TAKEN FIRST. The original's inlined draw is
// CgsNumeric::Random::RandomFloat(lfMin, lfMax) exactly -- read the current ring slot, refill THAT
// slot, step the LCG, bump the cursor -- with a span of 3.0 over a minimum of 1.0, i.e.
// RandomFloat(1.0f, 4.0f). It runs BEFORE the buffer-space assert and before any record store,
// which is what fixes its place in the module's LCG stream. Note it draws from the ParticleModule's
// own mRandom; JumpStateMachine::FireWheelDebris draws from the EffectsModule's. Two independent
// streams, and the original keeps them that way.
//
// THE COLOUR HAS TWO ARMS, and only one of them uses the caller's lvColour:
//   * type != eDebrisArray_Coloured -> the array's own preset colour (mpParams->mColour, the
//     preset's +0x20). The caller's colour is ignored, which is why FireWheelDebris can pass
//     white for its type-2 burst.
//   * type == eDebrisArray_Coloured -> lvColour DESATURATED TOWARD ITS OWN MEAN and then
//     randomised. The five vectors that shape it are dynamically-initialised constants -- a
//     literal read of their storage returns zero by definition, because a startup initialiser
//     writes them -- and each value below was recovered by disassembling that initialiser:
//         RGB-mean weights  (1/3, 1/3, 1/3, 0)   what the 4-lane dot averages
//         0.25 splat                             the LOW  saturation fraction
//         0.75 splat                             the HIGH saturation fraction
//         0.5  splat                             the LOW  bound's w lane
//         0.9  splat                             the HIGH bound's w lane
//     so low  = mean + (colour - mean) * 0.25, with w = 0.5
//        high = mean + (colour - mean) * 0.75, with w = 0.9
//     and DebrisColourRandomiser::Randomise draws inside that pair (its own w lane is a
//     brightness scale; it forces the result's alpha back to 1).
// =================================================================================================

#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameSource/Effects/BrnEffectsDebrisColourRandomiser.h"   // Utils::DebrisColourRandomiser
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

#include <cstring>   // memcpy (the batch publish)

namespace BrnParticle
{
    namespace
    {
        // The spawn buffer holds KU_NUM_DEBRIS_IN_SPAWN_BUFFER records; the console asserts
        // against it and publishes the moment the count reaches it. Prepare allocates the buffer
        // as 2560 bytes at 16-byte alignment, which is exactly 32 * 80.
        const u16 KU16_NUM_DEBRIS_IN_SPAWN_BUFFER = 32;

        // The rotation-amount draw's bounds.
        const f32 KF_ROTATION_AMOUNT_MIN = 1.0f;
        const f32 KF_ROTATION_AMOUNT_MAX = 4.0f;

        // The colour-shaping constants recovered from the CRT init thunks (see the banner).
        const Vector4 KV_RGB_MEAN_WEIGHTS = { 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 0.0f };
        const f32     KF_COLOUR_SATURATION_LOW  = 0.25f;
        const f32     KF_COLOUR_SATURATION_HIGH = 0.75f;
        const f32     KF_COLOUR_BRIGHTNESS_LOW  = 0.5f;
        const f32     KF_COLOUR_BRIGHTNESS_HIGH = 0.9f;
    }

    // ---------------------------------------------------------------------------------------------
    // SpawnDebris
    // ---------------------------------------------------------------------------------------------
    void ParticleModule::SpawnDebris(Native::EDebrisArrayID leDebrisType,
                                     Vector3 lvPosition,
                                     Vector3 lvVelocity,
                                     Vector3 lvRotationAxis,
                                     Vector4 lvColour,
                                     f32 lfSize,
                                     f32 lfSpawnTime)
    {
        // The ring draw comes first -- before the assert, before any store.
        const f32 lfRotationAmount = mRandom.RandomFloat(KF_ROTATION_AMOUNT_MIN,
                                                        KF_ROTATION_AMOUNT_MAX);

        CGS_ASSERT(mu16SpawnBufferCount < KU16_NUM_DEBRIS_IN_SPAWN_BUFFER,
                   "mDebrisSpawnBufferHeader.mu16DebrisCount < KU_NUM_DEBRIS_IN_SPAWN_BUFFER");

        DebrisBatchSpawnEvent::DebrisSpawnData* const lpRecord =
            static_cast<DebrisBatchSpawnEvent::DebrisSpawnData*>(mpSparkSpawnBuffer)
            + mu16SpawnBufferCount;

        lpRecord->mvPositionPlusSize               = { lvPosition.x, lvPosition.y, lvPosition.z, lfSize };
        lpRecord->mvVelocityPlusSpawnTime          = { lvVelocity.x, lvVelocity.y, lvVelocity.z, lfSpawnTime };
        lpRecord->mvRotationAxisPlusRotationAmount = { lvRotationAxis.x, lvRotationAxis.y,
                                                       lvRotationAxis.z, lfRotationAmount };
        lpRecord->meType                           = leDebrisType;

        if (leDebrisType != Native::eDebrisArray_Coloured)
        {
            // The array's own preset colour.
            lpRecord->mvColour = maDebris[leDebrisType].Params()->mColour;
        }
        else
        {
            // Desaturate the caller's colour toward its own RGB mean, once for each bound, then
            // draw inside the pair. The mean is a 4-lane dot with a zero w weight, i.e. the plain
            // mean of R, G and B, splatted across all four lanes.
            const f32 lfMean = lvColour.x * KV_RGB_MEAN_WEIGHTS.x
                             + lvColour.y * KV_RGB_MEAN_WEIGHTS.y
                             + lvColour.z * KV_RGB_MEAN_WEIGHTS.z
                             + lvColour.w * KV_RGB_MEAN_WEIGHTS.w;

            const Vector4 lvDeviation = { lvColour.x - lfMean, lvColour.y - lfMean,
                                          lvColour.z - lfMean, lvColour.w - lfMean };

            const Vector4 lvLow  = { lvDeviation.x * KF_COLOUR_SATURATION_LOW  + lfMean,
                                     lvDeviation.y * KF_COLOUR_SATURATION_LOW  + lfMean,
                                     lvDeviation.z * KF_COLOUR_SATURATION_LOW  + lfMean,
                                     KF_COLOUR_BRIGHTNESS_LOW };
            const Vector4 lvHigh = { lvDeviation.x * KF_COLOUR_SATURATION_HIGH + lfMean,
                                     lvDeviation.y * KF_COLOUR_SATURATION_HIGH + lfMean,
                                     lvDeviation.z * KF_COLOUR_SATURATION_HIGH + lfMean,
                                     KF_COLOUR_BRIGHTNESS_HIGH };

            BrnEffects::Utils::DebrisColourRandomiser lRandomiser;
            lRandomiser.Prepare(lvLow, lvHigh);
            lRandomiser.Randomise(lpRecord->mvColour, mRandom);
        }

        mu16SpawnBufferCount = static_cast<u16>(mu16SpawnBufferCount + 1);

        // A full buffer publishes itself immediately; a part-full one waits for PreRenderUpdate.
        if (mu16SpawnBufferCount == KU16_NUM_DEBRIS_IN_SPAWN_BUFFER)
        {
            const s32 liSize = static_cast<s32>(KU16_NUM_DEBRIS_IN_SPAWN_BUFFER) * 80 + 16;
            void* const lpData = mInterThreadEventQueue.AllocateEventSafe(
                                     eParticleEvent_DebrisBatchSpawn, liSize);
            if (lpData != 0)
            {
                CGS_ASSERT((reinterpret_cast<uintptr_t>(lpData) & 0xF) == 0,
                           "(((uint32_t)lpData) & 0xf) == 0");
                memcpy(lpData, &mu16SpawnBufferCount, 16);
                memcpy(static_cast<u8*>(lpData) + 16, mpSparkSpawnBuffer,
                       static_cast<size_t>(mu16SpawnBufferCount) * 80);
            }
            mu16SpawnBufferCount = 0;
        }
    }
}
