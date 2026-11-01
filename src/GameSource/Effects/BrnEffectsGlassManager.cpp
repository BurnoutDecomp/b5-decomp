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

namespace BrnEffects
{
    // X360 read of the raw .data word at dword_82CDAFA0, stored into mVehicleID@+0x40
    // on Reset. Its committed name/value are UNATTESTED (no source, no DWARF, no other
    // committed use of 0x82CDAFA0). Modelled as an extern u32 so the store is byte-
    // faithful without fabricating a value; NAME the global properly and drop this
    // placeholder once its home lands. Almost certainly the invalid-entity sentinel
    // (0xFFFFFFFF) but that is NOT proven here, so it is left as an external read.
    // ⭐ READ OUT OF THE IMAGE 2026-09-02 (tyre-mark wave), so it is no longer an external
    // with no home: `x360rd.py 82CDAFA0` returns 0xFFFFFFFF. The comment above guessed exactly
    // that and was right, but the guess is now a measurement -- and the link needed the
    // definition (LNK2019 on the first effects link). It is the invalid-entity sentinel.
    const u32 guResetVehicleId_82CDAFA0 = 0xFFFFFFFFu;              // dword_82CDAFA0

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

    // FireGlassEffect inlines the affine inverse and product at 0x82295E70..0x82295FC0.
    // UpdateVehicleEffectPositions uses the same product at 0x8228D3D8 and 0x8228D5DC.
    // Keep the VMX operations and their rounding in the order used by ARTIST.
    namespace
    {
        Vector3 CrossPermuted(const Vector3& a, const Vector3& b)
        {
            const __vector4 ayzx = __vpermwi(a.mV, 0x63);
            const __vector4 byzx = __vpermwi(b.mV, 0x63);
            return Vector3(__vpermwi(__vnmsubfp(ayzx, b.mV, __vmulfp(a.mV, byzx)), 0x63));
        }

        Matrix44Affine InverseAffine(const Matrix44Affine& m)
        {
            const Vector3 yz = CrossPermuted(m.yAxis, m.zAxis);
            const Vector3 xy = CrossPermuted(m.xAxis, m.yAxis);
            const Vector3 zx = CrossPermuted(m.zAxis, m.xAxis);
            const __vector4 det = __vspltw(__vmsum3fp(m.xAxis.mV, yz.mV), 0);
            const __vector4 one = __vcfsx(__vspltisw(1), 0);
            __vector4 inv = __vrefp(det);
            inv = __vmaddfp(inv, __vnmsubfp(inv, det, one), inv);
            inv = __vmaddfp(inv, __vnmsubfp(inv, det, one), inv);

            // Transpose the cofactor rows using the console's word merges.
            const __vector4 hi = __vmrghw(yz.mV, xy.mV);
            const __vector4 lo = __vmrglw(yz.mV, xy.mV);
            const __vector4 hi2 = __vmrghw(zx.mV, zx.mV);
            const __vector4 lo2 = __vmrglw(zx.mV, zx.mV);
            Matrix44Affine result;
            result.xAxis.mV = __vmulfp(__vmrghw(hi, hi2), inv);
            result.yAxis.mV = __vmulfp(__vmrglw(hi, hi2), inv);
            result.zAxis.mV = __vmulfp(__vmrghw(lo, lo2), inv);
            // Negate all translation lanes with the sign-bit mask at 0x82295E50.
            const __vector4 sign = __vslw(__vspltisw(1), __vspltisw(-1));
            const __vector4 pos = __vxor(m.wAxis.mV, sign);
            __vector4 w = __vmulfp(result.xAxis.mV, __vspltw(pos, 0));
            w = __vmaddfp(result.yAxis.mV, __vspltw(pos, 1), w);
            result.wAxis.mV = __vmaddfp(result.zAxis.mV, __vspltw(pos, 2), w);
            return result;
        }

        Vector3 RotateRow(const Vector3& row, const Matrix44Affine& rhs)
        {
            __vector4 v = __vmulfp(rhs.xAxis.mV, __vspltw(row.mV, 0));
            v = __vmaddfp(rhs.yAxis.mV, __vspltw(row.mV, 1), v);
            return Vector3(__vmaddfp(rhs.zAxis.mV, __vspltw(row.mV, 2), v));
        }

        Matrix44Affine MultiplyAffine(const Matrix44Affine& lhs, const Matrix44Affine& rhs)
        {
            Matrix44Affine result;
            result.xAxis = RotateRow(lhs.xAxis, rhs);
            result.yAxis = RotateRow(lhs.yAxis, rhs);
            result.zAxis = RotateRow(lhs.zAxis, rhs);
            __vector4 w = __vmaddfp(rhs.xAxis.mV, __vspltw(lhs.wAxis.mV, 0), rhs.wAxis.mV);
            w = __vmaddfp(rhs.yAxis.mV, __vspltw(lhs.wAxis.mV, 1), w);
            result.wAxis.mV = __vmaddfp(rhs.zAxis.mV, __vspltw(lhs.wAxis.mV, 2), w);
            return result;
        }
    }

    // FireGlassEffect @ 0x82295D50
    //   Claim the next round-robin glass slot, reset it, start the LION
    //   'Glass_shattering' effect at world index 0, and -- only if that effect's
    //   handle resolves to its slot -- seat it: the LION effect's world transform is
    //   lEffectTransform (the shatter's world placement) and the slot caches the shatter
    //   transform RELATIVE to the vehicle (lEffectTransform * the AFFINE inverse of
    //   lVehicleTransform -- see the banner above) plus its end-time / owning vehicle /
    //   active flag, so per-frame updates can re-attach it as the vehicle moves. When
    //   StartLionEffect hands back a stale/recycled handle the slot is left reset (only
    //   Reset ran).
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
            // re-composed with the vehicle transform each frame: the AFFINE inverse of the car
            // (rw::math::vpu::Inverse(Matrix44Affine), inlined 0x82295E70..0x82295F54 -- NOT the
            // general 4x4 inverse @0x825B2628, which this used to call and which divides by a
            // zero determinant for an affine's zero w column) and the affine product
            // (rw::math::vpu::operator*, inlined 0x82295F44..0x82295FB8), both rounded as the
            // console rounds them (the banner above).
            const Matrix44Affine lInverseVehicleTransform = InverseAffine(lVehicleTransform);
            lrNextEffect.mLocalTransform = MultiplyAffine(lEffectTransform, lInverseVehicleTransform);

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
                        // rw::math::vpu::operator*, inlined 0x8228D3D8..0x8228D478 (the banner above).
                        const Matrix44Affine lUpdatedTransform =
                            MultiplyAffine(lrEffect.mLocalTransform, lrVehicleTransform);
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
                // X360 reads the id at +0x3C8 (968) of the RaceCarState (0x8228D558): that is
                // mEntityId @968 in BrnVehicleEvents.h since the "+4 drift" was settled (the u64
                // mCarAssetAttribKey @960), and the transform at +0x1F0 is mTransform @496.
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
                            // rw::math::vpu::operator*, inlined 0x8228D5DC..0x8228D67C.
                            const Matrix44Affine lUpdatedTransform =
                                MultiplyAffine(lrEffect.mLocalTransform, lrRaceCarTransform);
                            lpLionEffect->SetTransform(lUpdatedTransform);
                        }
                    }
                }
            }
        }
    }

    // =====================================================================================
    // Construct(ParticleModule*) / Destruct  (DWARF BrnEffectsGlassManager.h:63)
    //
    // No X360 export: both are INLINED into EffectsModule::Construct @0x8228FE98 /
    // ::Destruct @0x8227FD78. Construct binds the module the slots start and stop their LION
    // effects through and puts every slot in the idle state -- which is BrnGlassSmashEffect::
    // Reset @0x8228F298 without the StopLionEffect leg, since a fresh slot owns no effect yet
    // (its handle is not KU_HANDLE_INVALID until this runs). The round-robin cursor starts at 0.
    // Destruct has nothing to release: the slot array is embedded and the module pointer is
    // borrowed.
    // =====================================================================================
    void BrnEffectsGlassManager::Construct(BrnParticle::ParticleModule* lpParticleModule)
    {
        mpParticleModule  = lpParticleModule;
        muNextGlassEffect = 0;

        for (u32 luEffect = 0; luEffect < KU_MAX_GLASS_EFFECTS; ++luEffect)
        {
            BrnGlassSmashEffect& lrEffect = maGlassEffects[luEffect];
            lrEffect.mLocalTransform.SetIdentity();
            lrEffect.mGlassEffectHandle = BrnParticle::LionEffect::KU_HANDLE_INVALID;
            lrEffect.mfEffectEndTime    = 0.0f;
            lrEffect.mbEffectActive     = false;
            lrEffect.mVehicleID.muValue = guResetVehicleId_82CDAFA0;
        }
    }

    void BrnEffectsGlassManager::Destruct()
    {
    }
}
