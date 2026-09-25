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
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::WriteToLog (the [glassfx] witness)
#include "rw/math/vpu/matrix44affine_operation.h"                  // Matrix44Affine ops

#include <cmath>                                                   // std::fma, std::isfinite, std::sqrt
#include <cstdio>                                                  // std::snprintf (the [glassfx] witness)
#include <cstdlib>                                                 // std::getenv (the [glassfx] witness)

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

    // =====================================================================================
    // THE AFFINE INVERSE AND PRODUCT, AS THE CONSOLE RUNS THEM (FX-GLASSNAN, crash parity 2026-09-26)
    //
    // FireGlassEffect caches the shatter in its car's frame as
    //     mLocalTransform = lEffectTransform * rw::math::vpu::Inverse(lVehicleTransform)
    // with the AFFINE overloads of Inverse and operator* (the DecFIGS inline list of
    // BrnEffectsGlassManager.cpp:85 names rw::math::vpu::Inverse and rw::math::vpu::operator*),
    // and UpdateVehicleEffectPositions re-seats it every frame with the same operator*. The X360
    // inlines both (0x82295E70..0x82295FC0; 0x8228D3D8..0x8228D478 and 0x8228D5DC..0x8228D67C),
    // and so does the PS3 FireGlassEffect @0x10C128 (its crosses permute with
    // gCrossProductPermuteConstant). Neither calls the GENERAL 4x4 rw::math::vpu::Inverse
    // @0x825B2628: that one's only callers are ShadowMap::ComputeTSMMatrix @0x827BFF58,
    // ShadowMap::ComputeBoundingBoxMatrix @0x827D91B0 and three debug components.
    //
    // ⚠ WHY IT MATTERS: THE AFFINE INVERSE READS NO w LANE. A Matrix44Affine's w column is ZERO by
    // the console's own convention -- Matrix44Affine::SetIdentity, inlined in Reset @0x8228F298,
    // writes wAxis = (0,0,0,0) (0x8228F324..0x8228F3C8: flt_82001C98 = 1.0, flt_82001CC0 = 0.0) --
    // and the PC producers agree (types.h SetIdentity; vpu::Cross, TransformVector and
    // TransformPoint all write w = 0). The 4x4 inverse this function used to call reads all
    // sixteen lanes: with that w column its determinant is exactly 0, 1/det is inf, and every
    // entry is 0 * inf = NaN -- so mLocalTransform was NaN, the re-seat made the LION effect's
    // transform NaN, and every Glass_shattering particle spawned at NaN (the final live
    // verification, 12 of 12 glass spawns). The console never had that division.
    //
    // The rounding is the console's instruction by instruction (ROUNDING_RULE.md, rule per site):
    //   crosses   vpermwi128 0x63 + vmulfp128 + vnmsubfp + vpermwi128 0x63 (rules 4, 3)
    //   det       vmsum3fp128 0x82295EEC (rule 1)
    //   1/det     vrefp 0x82295F1C + two Newton steps 0x82295F24..0x82295F30 (rule 5)
    //   rows      vmulfp128 0x82295F34..0x82295F3C (rule 4)
    //   products  vmulfp128 then vmaddfp, vmaddfp (rules 4, 3); the translation row seeded with the
    //             right-hand wAxis (0x82295FA4 / 0x8228D424 / 0x8228D628).
    // FLAG (ROUNDING_RULE 6): the VMX flushes denormal inputs and results; not modelled here. Every
    // operand is a component of a unit basis, a world position or their product -- a denormal needs
    // a basis component below 1e-19, which no car pose reaches in play.
    // =====================================================================================
    namespace
    {
        // vnmsubfp (rule 3): -(a * c - b), rounded ONCE and then negated -- an exact cancellation
        // is -0; a NaN keeps its sign.
        f32 Vnmsub(f32 lfA, f32 lfC, f32 lfB)
        {
            const f32 lfDifference = std::fma(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // vmsum3fp128 (rule 1). FLAG (model): vmsum = one rounding of the f64 sum (xenia
        // DOT_PRODUCT_3). An overflow to a QNaN is not modelled: this is the determinant of a car's
        // basis, |det| == 1.
        f32 Dot3(const Vector3& lrA, const Vector3& lrB)
        {
            return static_cast<f32>(static_cast<f64>(lrA.x) * lrB.x + static_cast<f64>(lrA.y) * lrB.y
                                  + static_cast<f64>(lrA.z) * lrB.z);
        }

        // lhs x rhs as the inline runs it: P(a * P(b) - P(a) * b) with P = vpermwi128 0x63 (y, z, x, w)
        // -- one vmulfp128 m = a * P(b), then one vnmsubfp -(P(a) * b - m) per lane, then P again.
        // The w lane is -(a.w * b.w - m.w); nothing reads it.
        Vector3 CrossPermuted(const Vector3& lrA, const Vector3& lrB)
        {
            const f32 lfU0 = Vnmsub(lrA.y, lrB.x, lrA.x * lrB.y);
            const f32 lfU1 = Vnmsub(lrA.z, lrB.y, lrA.y * lrB.z);
            const f32 lfU2 = Vnmsub(lrA.x, lrB.z, lrA.z * lrB.x);
            const f32 lfU3 = Vnmsub(lrA.w, lrB.w, lrA.w * lrB.w);
            Vector3 lvResult;
            lvResult.x = lfU1;   // P(u) = (u.y, u.z, u.x, u.w)
            lvResult.y = lfU2;
            lvResult.z = lfU0;
            lvResult.w = lfU3;
            return lvResult;
        }

        // vrefp + two Newton-Raphson steps, each vnmsubfp r = 1 - e * x and vmaddfp e = e * r + e (the
        // 1.0 is `vspltisw v5, 1` + `vcfsx v5, v5, 0`, 0x82295EBC / 0x82295ECC).
        // FLAG (model, rule 5): the hardware estimate is taken as its correctly rounded value; the
        // two refinements then pin the result.
        f32 RefinedRecip(f32 lfX)
        {
            f32 lfEstimate = static_cast<f32>(1.0 / static_cast<f64>(lfX));
            for (u32 luStep = 0; luStep < 2u; ++luStep)
            {
                const f32 lfResidual = Vnmsub(lfEstimate, lfX, 1.0f);
                lfEstimate = std::fma(lfEstimate, lfResidual, lfEstimate);
            }
            return lfEstimate;
        }

        Vector3 MakeLanes(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            Vector3 lvResult;
            lvResult.x = lfX;
            lvResult.y = lfY;
            lvResult.z = lfZ;
            lvResult.w = lfW;
            return lvResult;
        }

        Vector3 Scale4(const Vector3& lrv, f32 lfScale)   // vmulfp128 by a splat, all four lanes
        {
            return MakeLanes(lrv.x * lfScale, lrv.y * lfScale, lrv.z * lfScale, lrv.w * lfScale);
        }

        // lrAccumulator + lrv * lfScale per lane, rounded once (vmaddfp against a splat).
        Vector3 MaddSplat4(const Vector3& lrv, f32 lfScale, const Vector3& lrAccumulator)
        {
            return MakeLanes(std::fma(lrv.x, lfScale, lrAccumulator.x), std::fma(lrv.y, lfScale, lrAccumulator.y),
                             std::fma(lrv.z, lfScale, lrAccumulator.z), std::fma(lrv.w, lfScale, lrAccumulator.w));
        }

        // rw::math::vpu::Inverse(const Matrix44Affine&) as FireGlassEffect inlines it
        // (0x82295E70..0x82295F54). The three crosses of the basis rows, the determinant
        // x . (y x z), its refined reciprocal, the adjugate transposed by vmrghw / vmrglw
        // (0x82295EF0..0x82295F0C) -- whose w lanes repeat the (z x x) lane, as the merges leave them
        // -- and the inverse translation -w times the inverse basis (the vxor sign flip at 0x82295E78,
        // one vmulfp128 0x82295F40, two vmaddfp 0x82295F4C / 0x82295F54). No lane w of any row is
        // read, and there is no singular-matrix guard.
        Matrix44Affine InverseAffine(const Matrix44Affine& lrMatrix)
        {
            const Vector3 lvCrossYZ = CrossPermuted(lrMatrix.yAxis, lrMatrix.zAxis);
            const Vector3 lvCrossXY = CrossPermuted(lrMatrix.xAxis, lrMatrix.yAxis);
            const Vector3 lvCrossZX = CrossPermuted(lrMatrix.zAxis, lrMatrix.xAxis);
            const f32 lfInverseDeterminant = RefinedRecip(Dot3(lrMatrix.xAxis, lvCrossYZ));

            Matrix44Affine lInverse;
            lInverse.xAxis = Scale4(MakeLanes(lvCrossYZ.x, lvCrossZX.x, lvCrossXY.x, lvCrossZX.x), lfInverseDeterminant);
            lInverse.yAxis = Scale4(MakeLanes(lvCrossYZ.y, lvCrossZX.y, lvCrossXY.y, lvCrossZX.y), lfInverseDeterminant);
            lInverse.zAxis = Scale4(MakeLanes(lvCrossYZ.z, lvCrossZX.z, lvCrossXY.z, lvCrossZX.z), lfInverseDeterminant);

            const Vector3& lrPosition = lrMatrix.wAxis;
            lInverse.wAxis = MaddSplat4(lInverse.zAxis, -lrPosition.z,
                                        MaddSplat4(lInverse.yAxis, -lrPosition.y,
                                                   Scale4(lInverse.xAxis, -lrPosition.x)));
            return lInverse;
        }

        // One basis row of a product: the row's x lane times the right's xAxis (vmulfp128), then
        // + y * yAxis and + z * zAxis (two vmaddfp).
        Vector3 RotateRow(const Vector3& lrRow, const Matrix44Affine& lrRhs)
        {
            return MaddSplat4(lrRhs.zAxis, lrRow.z, MaddSplat4(lrRhs.yAxis, lrRow.y, Scale4(lrRhs.xAxis, lrRow.x)));
        }

        // rw::math::vpu::operator*(const Matrix44Affine&, const Matrix44Affine&) as both functions
        // inline it: the left's three basis rows through RotateRow; its translation row is the same
        // cascade SEEDED with the right's wAxis (three vmaddfp). The left's w lanes are never read.
        Matrix44Affine MultiplyAffine(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
        {
            Matrix44Affine lResult;
            lResult.xAxis = RotateRow(lrLhs.xAxis, lrRhs);
            lResult.yAxis = RotateRow(lrLhs.yAxis, lrRhs);
            lResult.zAxis = RotateRow(lrLhs.zAxis, lrRhs);
            const Vector3& lrTranslation = lrLhs.wAxis;
            lResult.wAxis = MaddSplat4(lrRhs.zAxis, lrTranslation.z,
                                       MaddSplat4(lrRhs.yAxis, lrTranslation.y,
                                                  MaddSplat4(lrRhs.xAxis, lrTranslation.x, lrRhs.wAxis)));
            return lResult;
        }

        // ---- [glassfx] -- [DIAG] NOT IN THE X360 BINARY. BRN_GLASS_DIAG=1 only (the glass drain's own gate);
        // default off, read-only, capped. The first KU_GLASSFX_FIRE_LINES seated fires print the shatter's world
        // point, the slot's cached mLocalTransform translation and the car's; the first KU_GLASSFX_RESEAT_LINES
        // re-seats print the LION effect's new translation -- the locator every Glass_shattering particle spawns
        // from -- and its distance from its car. FX-GLASSNAN's live case (tests/FxGlassNanLive.ps1) reads them.
        // DELETE-WHEN-STABLE.
        const u32 KU_GLASSFX_FIRE_LINES   = 16u;
        const u32 KU_GLASSFX_RESEAT_LINES = 48u;

        bool GlassFxWitnessArmed()
        {
            static const bool sbArmed = []() {
                const char* const lpcValue = std::getenv("BRN_GLASS_DIAG");
                return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
            }();
            return sbArmed;
        }

        bool MatrixFinite(const Matrix44Affine& lrMatrix)
        {
            const Vector3* const lapRows[4] = { &lrMatrix.xAxis, &lrMatrix.yAxis, &lrMatrix.zAxis, &lrMatrix.wAxis };
            for (u32 luRow = 0; luRow < 4u; ++luRow)
            {
                if (!std::isfinite(lapRows[luRow]->x) || !std::isfinite(lapRows[luRow]->y) ||
                    !std::isfinite(lapRows[luRow]->z) || !std::isfinite(lapRows[luRow]->w))
                {
                    return false;
                }
            }
            return true;
        }

        void GlassFxFireWitness(u32 luSlot, u32 luHandle, EntityId lVehicleEntity, const Matrix44Affine& lrEffect,
                                const Matrix44Affine& lrLocal, const Matrix44Affine& lrVehicle, bool lbSeated)
        {
            static u32 suLines = 0;
            if (!GlassFxWitnessArmed() || suLines >= KU_GLASSFX_FIRE_LINES)
                return;
            ++suLines;
            char lacMsg[400];
            if (!lbSeated)
            {
                std::snprintf(lacMsg, sizeof(lacMsg), "[glassfx] fire #%u slot=%u handle=%u owner=0x%08X NOT SEATED "
                              "(stale handle)\n", suLines, luSlot, luHandle, static_cast<unsigned>(lVehicleEntity.muValue));
            }
            else
            {
                const f64 ldLocalDistance = std::sqrt(static_cast<f64>(lrLocal.wAxis.x) * lrLocal.wAxis.x
                                                    + static_cast<f64>(lrLocal.wAxis.y) * lrLocal.wAxis.y
                                                    + static_cast<f64>(lrLocal.wAxis.z) * lrLocal.wAxis.z);
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[glassfx] fire #%u slot=%u handle=%u owner=0x%08X effect.wa=(%.3f,%.3f,%.3f) "
                    "local.wa=(%.3f,%.3f,%.3f,%.3f) |local.wa|=%.3f car.wa=(%.3f,%.3f,%.3f) car.w=(%g,%g,%g,%g) "
                    "local.finite=%d\n",
                    suLines, luSlot, luHandle, static_cast<unsigned>(lVehicleEntity.muValue),
                    static_cast<f64>(lrEffect.wAxis.x), static_cast<f64>(lrEffect.wAxis.y), static_cast<f64>(lrEffect.wAxis.z),
                    static_cast<f64>(lrLocal.wAxis.x), static_cast<f64>(lrLocal.wAxis.y), static_cast<f64>(lrLocal.wAxis.z),
                    static_cast<f64>(lrLocal.wAxis.w), ldLocalDistance,
                    static_cast<f64>(lrVehicle.wAxis.x), static_cast<f64>(lrVehicle.wAxis.y), static_cast<f64>(lrVehicle.wAxis.z),
                    static_cast<f64>(lrVehicle.xAxis.w), static_cast<f64>(lrVehicle.yAxis.w), static_cast<f64>(lrVehicle.zAxis.w),
                    static_cast<f64>(lrVehicle.wAxis.w), MatrixFinite(lrLocal) ? 1 : 0);
            }
            CgsDev::Log::WriteToLog(lacMsg);
        }

        void GlassFxReseatWitness(u32 luSlot, EntityId lVehicleEntity, const char* lpcOwner,
                                  const Matrix44Affine& lrUpdated, const Matrix44Affine& lrVehicle)
        {
            static u32 suLines = 0;
            if (!GlassFxWitnessArmed() || suLines >= KU_GLASSFX_RESEAT_LINES)
                return;
            ++suLines;
            const f64 ldX = static_cast<f64>(lrUpdated.wAxis.x) - lrVehicle.wAxis.x;
            const f64 ldY = static_cast<f64>(lrUpdated.wAxis.y) - lrVehicle.wAxis.y;
            const f64 ldZ = static_cast<f64>(lrUpdated.wAxis.z) - lrVehicle.wAxis.z;
            char lacMsg[320];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[glassfx] reseat #%u slot=%u owner=0x%08X %s wa=(%.3f,%.3f,%.3f) car.wa=(%.3f,%.3f,%.3f) dist=%.3f "
                "finite=%d\n",
                suLines, luSlot, static_cast<unsigned>(lVehicleEntity.muValue), lpcOwner,
                static_cast<f64>(lrUpdated.wAxis.x), static_cast<f64>(lrUpdated.wAxis.y), static_cast<f64>(lrUpdated.wAxis.z),
                static_cast<f64>(lrVehicle.wAxis.x), static_cast<f64>(lrVehicle.wAxis.y), static_cast<f64>(lrVehicle.wAxis.z),
                std::sqrt(ldX * ldX + ldY * ldY + ldZ * ldZ), MatrixFinite(lrUpdated) ? 1 : 0);
            CgsDev::Log::WriteToLog(lacMsg);
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

        GlassFxFireWitness(static_cast<u32>(&lrNextEffect - maGlassEffects), luHandle, lVehicleEntity,   // [DIAG]
                           lEffectTransform, lrNextEffect.mLocalTransform, lVehicleTransform, lpLionEffect != NULL);
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
                        GlassFxReseatWitness(luEffectLoop, lVehicleEntity, "traffic", lUpdatedTransform,   // [DIAG]
                                             lrVehicleTransform);
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
                            GlassFxReseatWitness(luEffectLoop, lRaceCarEntityId, "racecar", lUpdatedTransform,   // [DIAG]
                                                 lrRaceCarTransform);
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
