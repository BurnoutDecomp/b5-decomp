// ============================================================================
// GameSource/Effects/Props/PropCollisions.cpp
//
// BrnEffects::VFXRuntimeMaterialLef bodies, reconstructed store-for-store from
// BURNOUT_X360_ARTIST.XEX:
//   CreateEffect     @ 0x822936F0
//   TriggerLocators  @ 0x82299168
//
// The ParticleModule Lion helpers are real instance methods (committed
// ParticleModule.h): GetLionEffect(handle), StopLionEffect(slot*),
// StartLionEffect(hash, name, worldIndex).
// ============================================================================

#include "GameSource/Effects/Props/PropCollisions.h"
#include "GameSource/Effects/Particles/ParticleModule.h"          // BrnParticle::ParticleModule, LionEffect
#include "GameShared/GameClasses/Numeric/CgsRandom.h"             // CgsNumeric::Random + LCG constants
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT
#include "rw/math/vpu/vector3_operation.h"                        // Normalize / Cross
#include "rw/math/vpu/matrix44affine_operation.h"                 // TransformVector / TransformPoint

#include <cstdint>   // uintptr_t

// Static storage for the effect ring (one definition per TU).
namespace BrnEffects
{
    u32 VFXRuntimeMaterialLef::mNextEffect = 0;
    u32 VFXRuntimeMaterialLef::maEffectHandles[kuSizeOfEffectsArray] = { 0 };
}

namespace BrnEffects
{
    // -----------------------------------------------------------------------
    // CreateEffect  (X360 0x822936F0)
    //
    // Round-robins one of the five VFX effect slots: when the slot is already
    // playing it is stopped, then a fresh LION effect is started under the given
    // (already-hashed) locator name, its handle is recorded, and the resolved
    // playing slot is returned. mNextEffect advances modulo kuSizeOfEffectsArray
    // (the asm's 0xCCCCCCCD reciprocal-multiply mod-5).
    // -----------------------------------------------------------------------
    BrnParticle::LionEffect* VFXRuntimeMaterialLef::CreateEffect(
        BrnParticle::ParticleModule& lParticleModule,
        u32 lHashName)
    {
        BrnParticle::LionEffect* lpLionEffect = NULL;

        if (lHashName != 0)
        {
            BrnParticle::LionEffect* lpCurrentEffect =
                lParticleModule.GetLionEffect(maEffectHandles[mNextEffect]);
            if (lpCurrentEffect != NULL)
            {
                lParticleModule.StopLionEffect(lpCurrentEffect);
            }

            const u32 lEffectHandle =
                lParticleModule.StartLionEffect(lHashName, "VFXRuntimeMaterialLef StartEffect", 0);
            maEffectHandles[mNextEffect] = lEffectHandle;
            lpLionEffect = lParticleModule.GetLionEffect(lEffectHandle);

            mNextEffect = (mNextEffect + 1) % kuSizeOfEffectsArray;
        }

        return lpLionEffect;
    }

    // -----------------------------------------------------------------------
    // TriggerLocators  (X360 0x82299168)
    //
    // Fire the LION effect on each locator of the struck prop, seeding each
    // effect's world transform (velocity-aligned basis through the prop transform),
    // override velocity (the RAW race-car velocity), and a random state-blend draw.
    // -----------------------------------------------------------------------
    void VFXRuntimeMaterialLef::TriggerLocators(
        BrnParticle::ParticleModule&                lParticleModule,
        f32                                         lfCurrentTimeStep,
        f32                                         lfCurrentTime,
        const rw::math::vpu::Matrix44Affine&        lPropTransform,
        const BrnParticle::VFXLocator*              lpLocatorArray,
        u32                                         lNumLocators,
        const BrnPhysics::Vehicle::RaceCarState*    lpRaceCarState,
        CgsNumeric::Random&                         lRandom)
    {
        // lfCurrentTimeStep / lfCurrentTime are threaded from UpdateLocatorVfx but are
        // never consumed by the X360 body (they survive only as passed-through regs).
        (void)lfCurrentTimeStep;
        (void)lfCurrentTime;

        CGS_ASSERT((reinterpret_cast<uintptr_t>(lpLocatorArray) & 15) == 0,
                   "( ( (uint32_t)lpLocatorArray ) & 15 ) == 0");

        if (lNumLocators == 0)
            return;

        // Up-seed basis vector. X360 rodata unk_82181510 == rw::math::vpu::GetVector3_YAxis()
        // (DWARF PropCollisions.cpp inlines GetVector3_YAxis here); i.e. (0,1,0,0).
        const rw::math::vpu::Vector3 lUp = { 0.0f, 1.0f, 0.0f, 0.0f };

        // Race-car world-space velocity (RaceCarState + 0x330, read as a full SIMD vector).
        const rw::math::vpu::Vector3 lRaceCarVelocity =
            *reinterpret_cast<const rw::math::vpu::Vector3*>(
                reinterpret_cast<const u8*>(lpRaceCarState) + 0x330);

        for (u32 luLocator = 0; luLocator < lNumLocators; ++luLocator)
        {
            const BrnParticle::VFXLocator& lLocator = lpLocatorArray[luLocator];

            BrnParticle::LionEffect* lpEffect =
                CreateEffect(lParticleModule, lLocator.GetHash());
            if (lpEffect == NULL)
                continue;

            // Orthonormal basis from the (normalised) velocity forward and the Y-axis up
            // seed. Matches the console Normalize / Cross / Cross spine.
            const rw::math::vpu::Vector3 lZAxis = rw::math::vpu::Normalize(lRaceCarVelocity);
            const rw::math::vpu::Vector3 lXAxis =
                rw::math::vpu::Normalize(rw::math::vpu::Cross(lZAxis, lUp));
            const rw::math::vpu::Vector3 lYAxis = rw::math::vpu::Cross(lZAxis, lXAxis);

            // Re-express the local basis in world space through the prop transform and stamp
            // it into the effect slot as its world transform. Store order matches the asm:
            // zAxis row (+0x30) first, then xAxis (+0x10), then wAxis (+0x40), then yAxis (+0x20).
            rw::math::vpu::Matrix44Affine lTransform;
            lTransform.xAxis = rw::math::vpu::TransformVector(lPropTransform, lXAxis);
            lTransform.yAxis = rw::math::vpu::TransformVector(lPropTransform, lYAxis);
            lTransform.zAxis = rw::math::vpu::TransformVector(lPropTransform, lZAxis);
            lTransform.wAxis = rw::math::vpu::TransformPoint(lPropTransform, lLocator.mPosition);
            lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
            lpEffect->mTransform = lTransform;

            // Override velocity: the RAW (un-normalised) race-car velocity, plus the
            // OVERRIDE_VELOCITY flag. The velocity Vector3 lives at LionEffect + 0x50
            // (committed opaque mPad50 span). The asm stores the still-unnormalised velocity
            // (RaceCarState+0x330) here -- NOT the normalised forward.
            {
                f32* lpVelocity = reinterpret_cast<f32*>(
                    reinterpret_cast<u8*>(lpEffect) + 0x50);
                lpVelocity[0] = lRaceCarVelocity.x;
                lpVelocity[1] = lRaceCarVelocity.y;
                lpVelocity[2] = lRaceCarVelocity.z;
            }
            lpEffect->muFlags |=
                static_cast<u16>(BrnParticle::LionEffect::EPPE_FLAG_CHANGED
                               | BrnParticle::LionEffect::EPPE_FLAG_OVERRIDE_VELOCITY);

            // State-blend seed: an inlined CgsNumeric::Random::RandomFloat() draw
            // (buffer[oldest] - 1.0f), advancing the 64-bit LCG ring. lRandom is a non-const
            // Random& (DWARF signature) so the draw mutates it directly.
            {
                u8* lpRandomBase = reinterpret_cast<u8*>(&lRandom);
                u64& lruSeed         = *reinterpret_cast<u64*>(lpRandomBase + 0x20);
                u32& lruOldestIndex  = *reinterpret_cast<u32*>(lpRandomBase + 0x28);
                f32* lpFloatBuffer   = reinterpret_cast<f32*>(lpRandomBase);
                u32* lpIntBuffer     = reinterpret_cast<u32*>(lpRandomBase);

                const u32 luOldHigh    = static_cast<u32>(lruSeed >> 32);
                const f32 lfStateBlend = lpFloatBuffer[lruOldestIndex] - 1.0f;

                lruSeed = lruSeed * CgsNumeric::KU_RANDOM_MULTIPLIER + 1;
                lpIntBuffer[lruOldestIndex] =
                    CgsNumeric::KU_IEEE_754_REPRESENTATION_FLOAT_ONE | (luOldHigh >> 9);
                lruOldestIndex = (lruOldestIndex + 1)
                               & (CgsNumeric::KU_FLOAT_BUFFER_SIZE - 1);

                lpEffect->mfStateBlend = lfStateBlend;
            }
            lpEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
        }
    }
}
