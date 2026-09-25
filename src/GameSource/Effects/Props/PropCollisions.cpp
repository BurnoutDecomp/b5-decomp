// ============================================================================
// GameSource/Effects/Props/PropCollisions.cpp
//
// The prop-strike VFX, reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   PropCollisions::Initialise                @ 0x822937A8
//   PropCollisions::BuildPropToMaterialTable  @ 0x82288640
//   PropCollisions::UpdateLocatorVfx          @ 0x822993A0
//   VFXRuntimeMaterialLef::CreateEffect       @ 0x822936F0
//   VFXRuntimeMaterialLef::TriggerLocators    @ 0x82299168
//
// ⭐⭐⭐ FX-CRASHVFX 2026-09-25 (item 6). The PropCollisions bodies are new. TriggerLocators is RE-DERIVED from
// the ARTIST words -- the 2026-07-04 body (Niaz, wave40) ran all four rows of the effect's frame through the
// prop transform. The console carries only the locator's POSITION through the prop transform (three fused
// vmaddfp, 0x822992D4..0x822992EC); the basis is the car's velocity direction and the two crosses of it with
// the world up axis, stored as they are. The body also normalised through rw::math::vpu::Normalize; the
// console refines vrsqrtefp twice (RefinedRsqrt) and has no zero guard. CreateEffect is unchanged: it matches
// 0x822936F0 word for word.
// ============================================================================

#include "GameSource/Effects/Props/PropCollisions.h"
#include "GameSource/Effects/Particles/ParticleModule.h"                        // BrnParticle::ParticleModule, LionEffect
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"        // BrnPhysics::Vehicle::RaceCarState
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h" // BrnWorld::PropEntityIO::PropVFXLocatorEvent
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                    // CgsModule::BaseEventQueue (GetLength / GetEvent)
#include "GameSource/Director/Camera/Camera.h"                                  // BrnDirector::Camera::Camera (GetTransform)
#include "SharedClasses/Physics/Props/BrnPropPhysicsDataHeader.h"               // BrnPhysics::Props::PropPhysicsDataHeader
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                           // CgsNumeric::Random
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT

#include <cmath>     // std::fma / std::sqrt
#include <cstdint>   // uintptr_t

// Static storage for the effect ring (one definition per TU).
namespace BrnEffects
{
    u32 VFXRuntimeMaterialLef::mNextEffect = 0;
    u32 VFXRuntimeMaterialLef::maEffectHandles[kuSizeOfEffectsArray] = { 0 };
}

namespace BrnParticle
{
    // VFXPropsResourceType.h:400, as BuildPropToMaterialTable inlines it: the two asserts, then the state.
    const VFXPropState* VFXProp::GetState(u32 luState) const
    {
        CGS_ASSERT(luState < muNumPropStates, "luState < muNumPropStates");   // VFXPropsResourceType.h:402
        CGS_ASSERT(mpPropStates != 0, "mpPropStates != NULL");               // :403
        return reinterpret_cast<const VFXPropState*>(static_cast<uintptr_t>(mpPropStates)) + luState;
    }
}

namespace BrnEffects
{
namespace
{
    typedef rw::math::vpu::Vector3        Vector3;
    typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

    // ---- the literals ----
    const f32 KF_PROP_VFX_VISIBLE_DISTANCE_SQR = 2500.0f;   // flt_8200D510 (`fcmpu f0, f29 ; blt` at 0x82299438): 50 m
    const f32 KF_PROP_VFX_SMASH_SPEED_MPH      = 50.0f;     // flt_820138DC (`fcmpu f0, f28 ; ble` at 0x82299484)

    // ---- the console's vector idioms (the same models EffectsModule.cpp names) ----
    // vmsum3fp128: the three products summed, then rounded once.
    f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                              + static_cast<f64>(lrA.z) * lrB.z);
    }

    // `vnmsubfp`: -(a * c - b), fused, a NaN keeping its sign.
    f32 Vnmsub(f32 lfA, f32 lfC, f32 lfB)
    {
        const f32 lfDifference = std::fma(lfA, lfC, -lfB);
        return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
    }

    // `vrsqrtefp` and two Newton-Raphson refinements: est * est, est * 0.5, `vnmsubfp` 1 - x est^2 (fused),
    // `vmaddfp` est + half * r (fused). FLAG (model): the hardware estimate is modelled as its correctly rounded
    // value; the two refinements then pin the result.
    f32 RefinedRsqrt(f32 lfX)
    {
        f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfX)));
        for (u32 luStep = 0; luStep < 2u; ++luStep)
        {
            const f32 lfSquared  = lfEstimate * lfEstimate;
            const f32 lfHalf     = lfEstimate * 0.5f;
            const f32 lfResidual = Vnmsub(lfX, lfSquared, 1.0f);
            lfEstimate = std::fma(lfHalf, lfResidual, lfEstimate);
        }
        return lfEstimate;
    }

    // `vmulfp128` by a splat, all four lanes.
    Vector3 Scale4(const Vector3& lrv, f32 lfScale)
    {
        Vector3 lvResult;
        lvResult.x = lrv.x * lfScale;
        lvResult.y = lrv.y * lfScale;
        lvResult.z = lrv.z * lfScale;
        lvResult.w = lrv.w * lfScale;
        return lvResult;
    }

    // `vmaddfp` a * splat(b) + c, each lane rounded once.
    Vector3 MaddSplat4(const Vector3& lrA, f32 lfB, const Vector3& lrC)
    {
        Vector3 lv;
        lv.x = std::fma(lrA.x, lfB, lrC.x);
        lv.y = std::fma(lrA.y, lfB, lrC.y);
        lv.z = std::fma(lrA.z, lfB, lrC.z);
        lv.w = std::fma(lrA.w, lfB, lrC.w);
        return lv;
    }

    // `lhs x rhs` spelled the console's way: P(a * P(b) - P(a) * b) with P = vpermwi 0x63 (y, z, x, w) -- one
    // `vmulfp128` m = a * P(b), then ONE `vnmsubfp` per lane, then the permute back.
    Vector3 CrossPermuted(const Vector3& lrA, const Vector3& lrB)
    {
        const f32 lfU0 = Vnmsub(lrA.y, lrB.x, lrA.x * lrB.y);
        const f32 lfU1 = Vnmsub(lrA.z, lrB.y, lrA.y * lrB.z);
        const f32 lfU2 = Vnmsub(lrA.x, lrB.z, lrA.z * lrB.x);
        const f32 lfU3 = Vnmsub(lrA.w, lrB.w, lrA.w * lrB.w);
        Vector3 lvResult;
        lvResult.x = lfU1;
        lvResult.y = lfU2;
        lvResult.z = lfU0;
        lvResult.w = lfU3;
        return lvResult;
    }

    // rw::math::vpu::GetVector3_YAxis() -- unk_82181510 (0, 1, 0, 0).
    Vector3 AxisY()
    {
        Vector3 lv;
        lv.x = 0.0f; lv.y = 1.0f; lv.z = 0.0f; lv.w = 0.0f;
        return lv;
    }

    Vector3 Sub4(const Vector3& lrA, const Vector3& lrB)
    {
        Vector3 lv;
        lv.x = lrA.x - lrB.x; lv.y = lrA.y - lrB.y; lv.z = lrA.z - lrB.z; lv.w = lrA.w - lrB.w;
        return lv;
    }
}

    // -----------------------------------------------------------------------
    // VFXRuntimeMaterialLef::Initialise (DWARF PropCollisions.cpp:33), as PropCollisions::Initialise inlines it:
    // five `stw -1` into dword_82FAB624..0x82FAB634, then `stw 0` into dword_82FAB698.
    // -----------------------------------------------------------------------
    void VFXRuntimeMaterialLef::Initialise()
    {
        for (u32 luEffect = 0; luEffect < kuSizeOfEffectsArray; ++luEffect)
            maEffectHandles[luEffect] = BrnParticle::LionEffect::KU_HANDLE_INVALID;
        mNextEffect = 0;
    }

    // -----------------------------------------------------------------------
    // CreateEffect  (X360 0x822936F0)
    //
    // Round-robins one of the five VFX effect slots: when the slot is already
    // playing it is stopped, then a fresh LION effect is started under the given
    // (already-hashed) locator name, its handle is recorded, and the resolved
    // playing slot is returned. mNextEffect advances modulo kuSizeOfEffectsArray
    // (the asm's 0xCCCCCCCD reciprocal-multiply mod-5) whether or not it resolved.
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
                lParticleModule.StartLionEffect(lHashName, "VFXRuntimeMaterialLef StartEffect", 0);   // 0x82293744
            maEffectHandles[mNextEffect] = lEffectHandle;
            lpLionEffect = lParticleModule.GetLionEffect(lEffectHandle);

            mNextEffect = (mNextEffect + 1) % kuSizeOfEffectsArray;
        }

        return lpLionEffect;
    }

    // -----------------------------------------------------------------------
    // TriggerLocators  (X360 0x82299168, DWARF PropCollisions.cpp:56)
    //
    // For each locator of the struck prop's material (a 0x50 stride), CreateEffect on the locator's hash, and
    // when it resolves:
    //   forward = v x rsqrt(|v|^2)   the car's velocity (+0x330); `vmsum3fp128`, `vrsqrtefp` + two refinements,
    //                                `vmulfp128` on all four lanes (0x822991FC..0x82299280). No zero guard.
    //   right   = Y x forward, normalised the same way (the up row unk_82181510, 0x82299284..0x822992D8);
    //   up      = forward x right   (0x822992F0..0x82299300);
    //   w       = ((prop.w + prop.x lx) + prop.y ly) + prop.z lz -- the locator through the prop transform, three
    //             fused vmaddfp on the splatted locator lanes (0x822992CC..0x822992EC);
    // SetTransform(right, up, forward, w), SetVelocity(v) (`ori 0x24`) and SetStateBlendFactor(lRandom.RandomFloat())
    // (0x8229932C..0x8229937C). The two time arguments are never read.
    // -----------------------------------------------------------------------
    void VFXRuntimeMaterialLef::TriggerLocators(
        BrnParticle::ParticleModule&                lParticleModule,
        f32                                         /*lfCurrentTimeStep*/,
        f32                                         /*lfCurrentTime*/,
        const rw::math::vpu::Matrix44Affine&        lPropTransform,
        const BrnParticle::VFXLocator*              lpLocatorArray,
        u32                                         lNumLocators,
        const BrnPhysics::Vehicle::RaceCarState*    lpRaceCarState,
        CgsNumeric::Random&                         lRandom)
    {
        // `clrlwi r11, r26, 28 ; cmplwi ; beq` (0x82299180): the array's low four address bits (line 59).
        CGS_ASSERT((static_cast<u32>(reinterpret_cast<uintptr_t>(lpLocatorArray)) & 15u) == 0u,
                   "( ( (uint32_t)lpLocatorArray ) & 15 ) == 0");

        for (u32 luLocator = 0; luLocator < lNumLocators; ++luLocator)
        {
            const BrnParticle::VFXLocator& lLocator = lpLocatorArray[luLocator];
            BrnParticle::LionEffect* const lpEffect = CreateEffect(lParticleModule, lLocator.GetHash());   // `lwz r4, 0x10(r26)`
            if (lpEffect == NULL)
                continue;

            const Vector3& lVelocity = lpRaceCarState->mLinearVelocity;
            const Vector3 lForward   = Scale4(lVelocity, RefinedRsqrt(Dot3(lVelocity, lVelocity)));
            const Vector3 lRightRaw  = CrossPermuted(AxisY(), lForward);
            const Vector3 lRight     = Scale4(lRightRaw, RefinedRsqrt(Dot3(lRightRaw, lRightRaw)));
            const Vector3 lUp        = CrossPermuted(lForward, lRight);

            Vector3 lPosition = MaddSplat4(lPropTransform.xAxis, lLocator.mPosition.x, lPropTransform.wAxis);
            lPosition = MaddSplat4(lPropTransform.yAxis, lLocator.mPosition.y, lPosition);
            lPosition = MaddSplat4(lPropTransform.zAxis, lLocator.mPosition.z, lPosition);

            Matrix44Affine lTransform;
            lTransform.xAxis = lRight;      // +0x10
            lTransform.yAxis = lUp;         // +0x20
            lTransform.zAxis = lForward;    // +0x30
            lTransform.wAxis = lPosition;   // +0x40
            lpEffect->SetTransform(lTransform);
            lpEffect->SetVelocity(lVelocity);
            lpEffect->SetStateBlendFactor(lRandom.RandomFloat());
        }
    }

    // -----------------------------------------------------------------------
    // PropCollisions::Initialise  (X360 0x822937A8, DWARF PropCollisions.cpp:144)
    //   BuildPropToMaterialTable, then VFXRuntimeMaterialLef::Initialise and mRandom.Construct() -- the default
    //   seed 0xC87CD8C91AD0891B, the first ring slot 1.0, seven AddRandomFloatToBuffer and the cursor back to 0
    //   (0x822937D8..0x822939CC), all inlined.
    // -----------------------------------------------------------------------
    void PropCollisions::Initialise()
    {
        BuildPropToMaterialTable();
        VFXRuntimeMaterialLef::Initialise();
        mRandom.Construct();
    }

    // -----------------------------------------------------------------------
    // PropCollisions::MapPropTypeToMaterial  (DWARF PropCollisions.cpp:272)
    // -----------------------------------------------------------------------
    const PropToVFXMaterialMapping* PropCollisions::MapPropTypeToMaterial(u32 luPropTypeFromSpy) const
    {
        const PropToVFXMaterialMapping* lpMaterial = 0;   // :279
        if (luPropTypeFromSpy < KU_MAX_PROP_TYPE_MAPPINGS)
            lpMaterial = &maPropToMaterialMappings[luPropTypeFromSpy];
        return lpMaterial;
    }

    // -----------------------------------------------------------------------
    // PropCollisions::BuildPropToMaterialTable  (X360 0x82288640, DWARF PropCollisions.cpp:285)
    //
    // Every prop type starts with no material. Then each VFX prop of the collection is matched to the first
    // physics prop type that has its 64-bit id (`ld r11, 0x30(type) ; cmpld`). The matched type takes the
    // prop's unbroken state's material (state 0) and its smashed state's (state 1), when the prop has them.
    // A state may carry no material or exactly one (the asserts at lines 324 / 332).
    // -----------------------------------------------------------------------
    void PropCollisions::BuildPropToMaterialTable()
    {
        typedef BrnPhysics::Props::PropPhysicsDataHeader PropPhysicsDataHeader;
        static_assert(KU_MAX_PROP_TYPE_MAPPINGS == BrnPhysics::Props::KU_MAX_PROP_TYPES,
                      "maPropToMaterialMappings[500] is KU_MAX_PROP_TYPES");

        CgsResource::ResourcePtr<PropPhysicsDataHeader> lpPhysics(mPropDataResourceHandle);   // :290 0x82288690
        const BrnParticle::VFXPropCollection* const lpPropCollection = mVFXPropCollection.GetMemoryResource();   // :294
        const BrnParticle::VFXProp* const lpaPropArray = lpPropCollection->GetTable();     // :295
        const u32 lNumProps = lpPropCollection->GetTableSize();                            // :296

        CGS_ASSERT(lpPhysics->GetNumberOfPropTypes() <= BrnPhysics::Props::KU_MAX_PROP_TYPES,
                   "lpPhysics->GetNumberOfPropTypes() <= BrnPhysics::Props::KU_MAX_PROP_TYPES");   // :298

        for (u32 j = 0; j < KU_MAX_PROP_TYPE_MAPPINGS; ++j)                                // :300
            maPropToMaterialMappings[j] = PropToVFXMaterialMapping();

        for (u32 i = 0; i < lNumProps; ++i)                                                // :306
        {
            const BrnParticle::VFXProp& lProp = lpaPropArray[i];                           // :308
            const u32 luNumPropStates = lProp.GetNumStates();                              // :309
            CGS_ASSERT(luNumPropStates <= 2, "luNumPropStates <= 2");                      // :310
            const u64 lID = lProp.GetPropID();                                             // :312

            for (u32 j = 0; ; ++j)                                                           // :315
            {
                // One instance check per pass (0x822887D0), then the count and GetType's two asserts
                // (BrnPropPhysicsDataHeader.h:173 / :174) through the same header.
                const PropPhysicsDataHeader* const lpHeader = lpPhysics.operator->();
                if (j >= lpHeader->GetNumberOfPropTypes())
                    break;
                if (lpHeader->GetType(j)->GetResourceId().GetHash() != lID)                  // `ld r11, 0x30(r11) ; cmpld`
                    continue;

                if (luNumPropStates >= 1)
                {
                    const BrnParticle::VFXPropState* const lpUnbrokenState = lProp.GetState(0);   // :321
                    CGS_ASSERT(((lpUnbrokenState->muNumVFXMaterials == 0) && (lpUnbrokenState->mpVFXMaterial == 0))
                               || ((lpUnbrokenState->muNumVFXMaterials == 1) && (lpUnbrokenState->mpVFXMaterial != 0)),
                               "( ( lpUnbrokenState->muNumVFXMaterials == 0 ) && ( lpUnbrokenState->mpVFXMaterial == NULL ) ) || "
                               "( ( lpUnbrokenState->muNumVFXMaterials == 1 ) && ( lpUnbrokenState->mpVFXMaterial != NULL ) )");   // :324
                    maPropToMaterialMappings[j].mpUnBrokenVFXMaterial = lpUnbrokenState->GetMaterial();
                }
                if (luNumPropStates >= 2)
                {
                    const BrnParticle::VFXPropState* const lpSmashedState = lProp.GetState(1);    // :329
                    CGS_ASSERT(((lpSmashedState->muNumVFXMaterials == 0) && (lpSmashedState->mpVFXMaterial == 0))
                               || ((lpSmashedState->muNumVFXMaterials == 1) && (lpSmashedState->mpVFXMaterial != 0)),
                               "( ( lpSmashedState->muNumVFXMaterials == 0 ) && ( lpSmashedState->mpVFXMaterial == NULL ) ) || "
                               "( ( lpSmashedState->muNumVFXMaterials == 1 ) && ( lpSmashedState->mpVFXMaterial != NULL ) )");   // :332
                    maPropToMaterialMappings[j].mpSmashingVFXMaterial = lpSmashedState->GetMaterial();
                }
                break;
            }
        }
    }

    // -----------------------------------------------------------------------
    // PropCollisions::ContactVisible  (DWARF PropCollisions.cpp:347) -- within 50 m of the camera. `fcmpu ; blt`
    // (0x82299434..0x82299438): an unordered distance is not visible.
    // -----------------------------------------------------------------------
    bool PropCollisions::ContactVisible(Vector3 lPoint, const BrnDirector::Camera::Camera* lpCamera)
    {
        const Vector3 lViewVec = Sub4(lPoint, lpCamera->GetTransform().wAxis);   // :349 `vsubfp v0, v13, v0`
        const f32 lViewDistSqr = Dot3(lViewVec, lViewVec);                      // :351 `vmsum3fp128`
        return lViewDistSqr < KF_PROP_VFX_VISIBLE_DISTANCE_SQR;
    }

    // -----------------------------------------------------------------------
    // PropCollisions::UpdateLocatorVfx  (X360 0x822993A0, DWARF PropCollisions.cpp:165)
    //
    // Every prop VFX locator event of this frame (PropEntityModule::ProcessContacts posts one per prop hit or
    // smash) that happened within 50 m of the camera fires its prop type's material: the smashing one for a
    // SMASH -- or for any event while the car goes faster than 50 mph -- else the unbroken one.
    // -----------------------------------------------------------------------
    void PropCollisions::UpdateLocatorVfx(f32 lfCurrentTimeStep,
                                          f32 lfCurrentTime,
                                          BrnParticle::ParticleModule& lParticleModule,
                                          const PropVFXLocatorQueue& lPropStateQueue,
                                          s32 /*lMaterialOverride*/,
                                          const RaceCarState* lpRaceCarState,
                                          const BrnDirector::Camera::Camera* lpCamera)
    {
        typedef BrnWorld::PropEntityIO::PropVFXLocatorEvent PropVFXLocatorEvent;

        const u32 lCount = static_cast<u32>(lPropStateQueue.GetLength());                 // :215 `lwz r19, 8(r26)`
        for (u32 lu32I = 0; lu32I < lCount; ++lu32I)                                      // :216
        {
            const PropVFXLocatorEvent& lEvent = lPropStateQueue.GetEvent(static_cast<s32>(lu32I));   // :218 0x8227BDA8
            const Matrix44Affine& lPropTransform = lEvent.GetTransform();                // :219
            const Vector3 lPropPosition = lPropTransform.wAxis;                          // :220 event +0x30
            if (!ContactVisible(lPropPosition, lpCamera))
                continue;

            const u32 luPropType = lEvent.GetPropType();                                 // :224 +0x40
            PropVFXLocatorEvent::EEventType leEventType = lEvent.GetEventType();          // :226 +0x44
            CGS_ASSERT((leEventType == PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH)
                       || (leEventType == PropVFXLocatorEvent::E_EVENTTYPE_PROPHIT),
                       "( leEventType == BrnWorld::PropEntityIO::PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH ) || "
                       "( leEventType == BrnWorld::PropEntityIO::PropVFXLocatorEvent::E_EVENTTYPE_PROPHIT )");   // :228
            // `lfs f0, 0x3CC(r25) ; fcmpu f0, f28 ; ble` -- a NaN speed keeps the event's own type.
            if (lpRaceCarState->mfSpeedMPH > KF_PROP_VFX_SMASH_SPEED_MPH)
                leEventType = PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH;

            const PropToVFXMaterialMapping* const lpMaterialMapping = MapPropTypeToMaterial(luPropType);   // :239
            if (lpMaterialMapping == 0)
                continue;
            const BrnParticle::VFXMaterial* const lpMaterial =                           // :242
                (leEventType == PropVFXLocatorEvent::E_EVENTTYPE_PROPSMASH) ? lpMaterialMapping->mpSmashingVFXMaterial
                                                                            : lpMaterialMapping->mpUnBrokenVFXMaterial;
            if (lpMaterial == 0)
                continue;
            const u32 lNumLocators = lpMaterial->mNumLocators;                            // :248 +0x04
            if (lNumLocators == 0)
                continue;
            VFXRuntimeMaterialLef::TriggerLocators(lParticleModule, lfCurrentTimeStep, lfCurrentTime, lPropTransform,
                                                   lpMaterial->GetLocators(), lNumLocators, lpRaceCarState, mRandom);
        }
    }
}
