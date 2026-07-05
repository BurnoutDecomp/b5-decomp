// =============================================================================
// GameSource/Effects/BrnEffectsGlassManager.cpp
//
// BrnEffects::BrnGlassSmashEffect -- one glass-smash VFX slot.
// BrnEffects::BrnEffectsGlassManager -- the owning per-frame manager.
// =============================================================================
#include "GameSource/Effects/BrnEffectsGlassManager.h"
#include "GameSource/Effects/Particles/ParticleModule.h"          // BrnParticle::ParticleModule / LionEffect
#include "GameSource/Effects/Particles/BrnParticleDescription.h"  // BrnParticle::ParticleDescription::HashString
#include "GameSource/Effects/SharedIO/BrnEffectsModuleIO_InputBuffer.h"   // BrnEffects::EffectsIO::InputBuffer
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"  // VehicleOutputInterface::PhysicalTrafficStateQueue
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::PhysicalTrafficState
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"                  // Matrix44Affine ops

namespace rw { namespace math { namespace vpu {
    // rw::math::vpu::Inverse @ 0x825B2628 -- full 4x4 matrix inverse + determinant (the
    // undecoded _asmInverse VMX pipeline). Declared here to match the X360 `bl` (mirrors the
    // committed BrnShadowMap.cpp precedent); body lives in its vendor TU.
    Matrix44 Inverse(const Matrix44& lrMatrix, Vector4& lrDeterminant);
} } }

namespace BrnEffects
{
    // X360 read of the raw .data word at dword_82CDAFA0, stored into mVehicleID@+0x40
    // on Reset. Its committed name/value are UNATTESTED (no source, no DWARF, no other
    // committed use of 0x82CDAFA0). Modelled as an extern u32 so the store is byte-
    // faithful without fabricating a value; NAME the global properly and drop this
    // placeholder once its home lands. Almost certainly the invalid-entity sentinel
    // (0xFFFFFFFF) but that is NOT proven here, so it is left as an external read.
    extern const u32 guResetVehicleId_82CDAFA0;                    // dword_82CDAFA0

    // Reset @ 0x8228F298
    //   Stop any LION effect this slot still owns, then clear the slot to idle:
    //   identity local transform, invalid effect handle, no end-time, inactive,
    //   and the module's reset vehicle id.
    //
    //   luEffectInstance is the manager slot index (asserted < KU_MAX_GLASS_EFFECTS);
    //   it is validated but otherwise unused -- Reset operates on `this`.
    void BrnGlassSmashEffect::Reset(u32 luEffectInstance, BrnParticle::ParticleModule* lpParticleModule)
    {
        CGS_ASSERT(luEffectInstance < KU_MAX_GLASS_EFFECTS,
                   "luEffectInstance < KU_MAX_GLASS_EFFECTS");

        // Resolve the slot; GetLionEffect returns NULL unless the stored handle still
        // matches (the X360 build inlines this resolve + its own 'luArrayIndex <
        // KU_MAX_PLAYING_EFFECTS' assert here). Stop it only if ours.
        BrnParticle::LionEffect* lpLionEffect = lpParticleModule->GetLionEffect(mGlassEffectHandle);
        if (lpLionEffect != NULL)
        {
            lpParticleModule->StopLionEffect(lpLionEffect);
        }

        mLocalTransform.SetIdentity();
        mGlassEffectHandle    = BrnParticle::LionEffect::KU_HANDLE_INVALID;  // -1
        mfEffectEndTime       = 0.0f;
        mbEffectActive        = false;
        mVehicleID.muValue    = guResetVehicleId_82CDAFA0;                   // dword_82CDAFA0
    }

    // FireGlassEffect @ 0x82295D50
    //   Claim the next round-robin glass slot, reset it, start the LION
    //   'Glass_shattering' effect at world index 0, and -- only if that effect's
    //   handle resolves to its slot -- seat it: the LION effect's world transform is
    //   lEffectTransform (the shatter's world placement) and the slot caches the shatter
    //   transform RELATIVE to the vehicle (lEffectTransform * inverse(lVehicleTransform))
    //   plus its end-time / owning vehicle / active flag, so per-frame updates can
    //   re-attach it as the vehicle moves. When StartLionEffect hands back a stale/
    //   recycled handle the slot is left reset (only Reset ran).
    void BrnEffectsGlassManager::FireGlassEffect(const Matrix44Affine& lEffectTransform,
                                                 const Matrix44Affine& lVehicleTransform,
                                                 EntityId lVehicleEntity,
                                                 f32 lfCurrentTime)
    {
        // Grab the next slot (round-robin over the 8 slots) and reset it, then advance
        // the cursor (X360: muNextGlassEffect = (muNextGlassEffect + 1) & 7).
        BrnGlassSmashEffect& lrNextEffect = maGlassEffects[muNextGlassEffect];
        lrNextEffect.Reset(muNextGlassEffect, mpParticleModule);
        muNextGlassEffect = (muNextGlassEffect + 1) & (KU_MAX_GLASS_EFFECTS - 1);

        // Start the glass-shatter LION effect (world index 0). Returns the new
        // playing-effect handle, cached on the slot.
        const u32 luNameHash = BrnParticle::ParticleDescription::HashString(KPC_GLASS_SHATTER_EFFECT);
        const u32 luHandle   = mpParticleModule->StartLionEffect(luNameHash, KPC_GLASS_SHATTER_EFFECT, 0);
        lrNextEffect.mGlassEffectHandle = luHandle;

        // Only seat the slot if the handle actually resolves to its playing-effect slot
        // (X360 inlines ParticleModule::GetLionEffect's resolve + its
        // 'luArrayIndex < KU_MAX_PLAYING_EFFECTS' assert here; StartLionEffect can fail
        // and hand back a stale/recycled handle). On mismatch the asm jumps straight to
        // the epilogue, leaving the slot in its just-Reset state.
        BrnParticle::LionEffect* lpLionEffect = mpParticleModule->GetLionEffect(luHandle);
        if (lpLionEffect != NULL)
        {
            // The LION effect renders at the shatter's world transform.
            lpLionEffect->SetTransform(lEffectTransform);

            // Cache the shatter transform in the vehicle's local frame so it can be
            // re-composed with the vehicle transform each frame
            // (X360: rw::math::vpu::Inverse -> full 4x4 inverse with determinant). The
            // vehicle transform is affine so the inverse is too; the X360 stores the four
            // inverse rows straight into the affine slot (Matrix44 and Matrix44Affine share
            // the same 4x16-byte row layout).
            Vector4 lDeterminant;
            const Matrix44 lInverseVehicle44 =
                rw::math::vpu::Inverse(reinterpret_cast<const Matrix44&>(lVehicleTransform),
                                       lDeterminant);
            const Matrix44Affine& lInverseVehicle =
                reinterpret_cast<const Matrix44Affine&>(lInverseVehicle44);
            lrNextEffect.mLocalTransform = lEffectTransform * lInverseVehicle;

            // Seat the rest of the slot (X360 scalar tail, INSIDE the handle-match block;
            // kfLionGlassFillInEffectTime == 1.0f).
            lrNextEffect.mfEffectEndTime = lfCurrentTime + kfLionGlassFillInEffectTime;
            lrNextEffect.mVehicleID      = lVehicleEntity;
            lrNextEffect.mbEffectActive  = true;
        }
    }

    // UpdateVehicleEffectPositions @ 0x8228D208
    //   Per-frame maintenance of every live glass-smash slot, in three phases:
    //     1. Expire slots whose fill-in time has elapsed (stop the LION effect).
    //     2. Re-attach traffic-vehicle glass effects to the current traffic transforms.
    //     3. Re-attach race-car glass effects to the current race-car transforms.
    //   Each attach re-composes the slot's cached local transform with the live vehicle
    //   transform and pushes it onto the LION effect (SetTransform sets CHANGED).
    void BrnEffectsGlassManager::UpdateVehicleEffectPositions(const EffectsIO::InputBuffer* lpInput,
                                                              f32 lfCurrentTime)
    {
        // ---- Phase 1: expire timed-out slots ----
        for (u32 luEffectLoop = 0; luEffectLoop < KU_MAX_GLASS_EFFECTS; ++luEffectLoop)
        {
            BrnGlassSmashEffect& lrEffect = maGlassEffects[luEffectLoop];
            if (lrEffect.mbEffectActive && lrEffect.mfEffectEndTime < lfCurrentTime)
            {
                // Stop the LION effect only if the slot's handle still resolves (the
                // X360 inlines the GetLionEffect resolve before StopLionEffect).
                BrnParticle::LionEffect* lpLionEffect =
                    mpParticleModule->GetLionEffect(lrEffect.mGlassEffectHandle);
                if (lpLionEffect != NULL)
                {
                    mpParticleModule->StopLionEffect(lpLionEffect);
                }
                lrEffect.mbEffectActive     = false;
                lrEffect.mGlassEffectHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;  // -1
            }
        }

        // ---- Phase 2: traffic vehicles ----
        const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue* lpVehicleQueue =
            static_cast<const BrnPhysics::Vehicle::VehicleOutputInterface::PhysicalTrafficStateQueue*>(
                lpInput->GetVehiclePhysicalStateQueue());
        CGS_ASSERT(lpVehicleQueue != NULL, "lpVehicleQueue != NULL");

        const s32 lnNumVehicles = lpVehicleQueue->GetLength();
        for (s32 lnVehicleLoop = 0; lnVehicleLoop < lnNumVehicles; ++lnVehicleLoop)
        {
            const BrnPhysics::Vehicle::PhysicalTrafficState lVehicleEvent(
                lpVehicleQueue->GetEvent(lnVehicleLoop));
            const Matrix44Affine& lrVehicleTransform = lVehicleEvent.mTransform;
            const EntityId lVehicleEntity            = lVehicleEvent.mEntityID;

            for (u32 luEffectLoop = 0; luEffectLoop < KU_MAX_GLASS_EFFECTS; ++luEffectLoop)
            {
                BrnGlassSmashEffect& lrEffect = maGlassEffects[luEffectLoop];
                if (lrEffect.mbEffectActive && lrEffect.mVehicleID.muValue == lVehicleEntity.muValue)
                {
                    BrnParticle::LionEffect* lpLionEffect =
                        mpParticleModule->GetLionEffect(lrEffect.mGlassEffectHandle);
                    if (lpLionEffect != NULL)
                    {
                        const Matrix44Affine lUpdatedTransform =
                            lrEffect.mLocalTransform * lrVehicleTransform;
                        lpLionEffect->SetTransform(lUpdatedTransform);
                    }
                }
            }
        }

        // ---- Phase 3: race cars ----
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpRaceCarQueue =
            static_cast<const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*>(
                lpInput->GetActiveRaceCarInterface());
        CGS_ASSERT(lpRaceCarQueue != NULL, "lpRaceCarQueue != NULL");

        for (s32 lnRaceCarLoop = 0; lnRaceCarLoop < 8; ++lnRaceCarLoop)
        {
            const EActiveRaceCarIndex leCurrentRaceCarIndex =
                static_cast<EActiveRaceCarIndex>(lnRaceCarLoop);
            CGS_ASSERT(leCurrentRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leCurrentRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            if (lpRaceCarQueue->IsRaceCarActive(leCurrentRaceCarIndex))
            {
                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
                    lpRaceCarQueue->GetRaceCarState(leCurrentRaceCarIndex);
                // NOTE: X360 reads the id at +0x3C8 (968) of the RaceCarState; the committed
                // BrnVehicleEvents.h maps 968->mfSpeedMPH and 964->mEntityId. Kept as the
                // semantically-correct mEntityId; reconcile the RaceCarState offset in its home.
                const EntityId lRaceCarEntityId          = lpRaceCarState->mEntityId;
                const Matrix44Affine& lrRaceCarTransform = lpRaceCarState->mTransform;

                for (u32 luEffectLoop = 0; luEffectLoop < KU_MAX_GLASS_EFFECTS; ++luEffectLoop)
                {
                    BrnGlassSmashEffect& lrEffect = maGlassEffects[luEffectLoop];
                    if (lrEffect.mbEffectActive &&
                        lrEffect.mVehicleID.muValue == lRaceCarEntityId.muValue)
                    {
                        BrnParticle::LionEffect* lpLionEffect =
                            mpParticleModule->GetLionEffect(lrEffect.mGlassEffectHandle);
                        if (lpLionEffect != NULL)
                        {
                            const Matrix44Affine lUpdatedTransform =
                                lrEffect.mLocalTransform * lrRaceCarTransform;
                            lpLionEffect->SetTransform(lUpdatedTransform);
                        }
                    }
                }
            }
        }
    }
}
