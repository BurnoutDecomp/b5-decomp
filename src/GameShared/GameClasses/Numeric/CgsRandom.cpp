// ===========================================================================
// CgsNumeric::Random -- the out-of-line draw/seed methods.
//   class:CgsNumeric::Random
//
// The X360 build INLINES this LCG everywhere (no out-of-line bodies exist in
// the ledger); the semantics are attested at the inline sites:
//   * the LCG step: muSeed = muSeed * 0x5851F42D4C957F2D + 1, draw = hi 32 bits
//     (Vehicle::SetFlashingHeadlights @0x827537D0..E4: "return muSeed >> 32,
//     then muSeed = muSeed * 0x5851F42D4C957F2D + 1"; the same constant pair
//     1284865837 / 0x5851F42D in every inlined site).
//   * SetSeed: install the seed, then prime all eight ring slots -- see the body.
//     (Corrected 2026-09-23: this line used to read the multiplier's `insrdi` as an
//     OR of 0x5851F42D00000000 into the seed.)
//
// Homed 2026-07-05 because wave49's SelectionHistory::Randomize (CgsSoundUtils)
// calls SetSeed/RandomUInt(min,max) out-of-line, which broke the exe link (the
// methods were declared-only). This TU is their canonical home when the full
// CgsRandom ledger work lands; the bodies below are the attested LCG.
//
// The bounded draw RandomUInt(min,max) has no exported out-of-line X360 body either
// (always inlined, often strength-reduced); its expansions pin min + draw % (max - min + 1),
// an INCLUSIVE range -- see the body.
// ===========================================================================

#include "GameShared/GameClasses/Numeric/CgsRandom.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (bounded unsigned draw)

#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"   // VecFloat -- RandomVecFloat's return type
#include "rw/math/vpu/types.h"   // Vector3 -- RandomVector's DEFINITION needs the complete
                                 //   type (the header itself deliberately only forward-declares
                                 //   it, to stay standalone)

#include <xtl.h>
#include <xboxmath.h>

namespace CgsNumeric
{
    // The shared LCG multiplier (every inlined site: hi 0x5851F42D, lo 0x4C957F2D).
    static const u64 KU_RANDOM_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;

    // ========================================================================
    // THE BOUNDED UNSIGNED DRAW -- CORRECTED 2026-09-25 (crash parity FX-GATE).
    //
    // ⚠️ NO STANDALONE X360 SYMBOL: inlined at every call site, each carrying the one baked assert
    // "luMod > 0" (0x82001F88, CgsRandom.h:303 = `li r5, 0x12F`). The expansions read:
    //   * EffectsModule::HandleShowtimeTrafficBounce @0x82292808, RandomUInt(150, 300) with both
    //     bounds constant: 0x82292DA4 draw = hi32(OLD seed) ; 0x82292DD8 mulhwu 0x36406C81 ;
    //     0x82292DEC srwi 5 ; 0x82292DF4 mulli 0x97 ; 0x82292E04 subf ; 0x82292E10 addi 0x96
    //     -> 150 + draw % 151, so 300 is reachable (luMod = 300 - 150 + 1).
    //   * SelectionHistory::FindRandomOldest @0x82702840, RandomUInt(0, n >> 1): 0x8270294C srwi 1 ;
    //     0x82702950 addi 1 (luMod) ; 0x82702954 cmplwi ; bne past the :303 assert ; 0x82702990 draw ;
    //     0x827029B0 divwu / mullw / subf -> draw % luMod.
    //   * FaceOffMode::Start 0x82330598 (assert) / 0x82330608 divwu, DeformableObject::
    //     UpdateSpinningDetachment 0x8263AAC0 / 0x8263AB28, CheckForForcedDetachment 0x8263AE54 /
    //     0x8263AEBC -- RandomUInt(0, n - 1) with the -1 / +1 folded: draw % n.
    //   * SelectionHistory::Randomize @0x826C5900, RandomUInt(0, 511): 0x826C5B1C draw ;
    //     0x826C5B2C clrlwi 23 -> draw & 0x1FF (luMod = 512, constant, no assert).
    // Like RandomInt: the draw is the seed BEFORE the step, the reduction is unsigned and the
    // ring is untouched. There is no range guard: luMin == luMax still steps the seed (luMod = 1),
    // and luMod == 0 traps on the console (`twi 6, luMod, 0` after the assert, e.g. 0x827029A8).
    // ⚠️ THE OLD BODY reduced by (max - min), one short, so luMax was never drawn, and returned luMin
    // WITHOUT stepping the seed when the two were equal.
    u32 Random::RandomUInt(u32 luMin, u32 luMax)
    {
        const u32 luMod = luMax - luMin + 1u;
        CGS_ASSERT(luMod > 0u, "luMod > 0");

        return luMin + (RandomUInt() % luMod);
    }

    // ========================================================================
    // THE TWO BOUNDED FLOAT DRAWS.
    //
    // ⚠️ NEITHER HAS AN X360 SYMBOL OF ITS OWN: the console inlines both at every call site,
    // so both are recovered from an inline expansion rather than from a standalone body. The
    // expansion used is CameraShake::Update @0x82221310, which inlines the SCALAR draw three
    // separate times (0x82221320..0x822213D4, 0x822213D8..0x82221444, 0x8222144C..0x82221500)
    // and the VECTOR draw once (0x822214F8..0x82221628). Three independent copies of the
    // scalar expansion inside one function is itself the cross-check: they agree instruction
    // for instruction.
    //
    // ⭐ THE VECTOR DRAW HAS A SECOND, ALREADY-COMMITTED WITNESS. Its ring packing
    // (insrwi 21,9 / insrwi 10,9 + srwi 19 / inslwi 23,9) is byte-identical to
    // BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ @0x82277EC8, which this tree
    // reconstructed months ago out of a DIFFERENT function. The two agree on the slot
    // arithmetic ((index + 3) & 4), on the two LCG steps, on which high word lands in which
    // mantissa field, and on the one-deep pipeline (the quad returned is the one the PREVIOUS
    // call primed). Only the final combine differs, and for the obvious reason: the randomiser
    // ends on its own `mVecA * mVecB + t`, the bounded draw on `(lMax - lMin) * t + lMin`.
    //
    // ⚠️⚠️ THE SCALAR DRAW IS *NOT* `AddRandomFloatToBuffer` WITH A RANGE MAP, AND THE
    // DIFFERENCE IS OBSERVABLE. AddRandomFloatToBuffer (in the header, and pinned by
    // Construct's shape -- it is the only reading under which all eight ring slots get primed)
    // bumps the index and THEN writes, so its pipeline is one call deep. The draw below
    // refills the CURRENT slot and bumps afterwards: the X360 computes `slwi r3, r8, 2` from
    // the index read at 0x82221320 and uses that SAME r3 for both `lfsx f11, r3, r6` and
    // `stwx r29, r3, r6`, and only then does `lwz/addi 1/clrlwi 29/stw` at
    // 0x822213AC..0x822213D4. That makes the value returned the one generated EIGHT calls ago,
    // which is the entire point of an eight-slot ring. Routing this through
    // AddRandomFloatToBuffer would have compiled, linked, run, and silently produced a
    // differently-correlated stream -- a defect nothing downstream could report.
    // ========================================================================

    // The bounded scalar draw, statement for statement:
    //   t = mafFloatBuffer[muOldestBufferIndex] - 1.0f   lfsx / fsubs against flt_82001C98
    //   refill the CURRENT slot from the seed's high word  inslwi r29, hi, 23,9 ; stwx r29,r3,r6
    //   step the LCG                                       mulld / addi 1 / std 0x20
    //   bump the index                                     addi 1 ; clrlwi 29 ; stw 0x28
    //   return (lfMax - lfMin) * t + lfMin                 fsubs, then ONE fmadds
    // ⚠️ The console evaluates `lfMax - lfMin` before the ring read completes. The order does not
    //   change the result.
    // ⭐ THE RANGE MAP IS FUSED (FX-AIBUZZ, crash parity 2026-09-25). fmadds rounds ONCE, so the
    //   PC's `(lfMax - lfMin) * t + lfMin`, which rounds the product first, missed by an ulp on
    //   some inputs. Five console expansions were read, and every one is `fsubs` (max - min), then
    //   `fmadds D, range, t, min` (D = range * t + min):
    //     CameraShake::Update             fsubs @0x82221360, fmadds f0, f0, f11, f12  @0x822213BC
    //     BehaviourFixedCam (dutch)       fsubs @0x8222A0C8, fmadds f0, f0, f12, f13  @0x8222A0F8
    //     ParticleModule (rotation speed) fsubs @0x82281A90, fmadds f3, f12, f13, f0  @0x82281AD0
    //     ClutchControl                   fsubs @0x826CDFBC, fmadds f12, f10, f11, f12 @0x826CDFC0
    //                                     fsubs @0x826CE078, fmadds f12, f11, f0, f12  @0x826CE07C
    //   The native scalar expression below emits that fmadds. The volatile range
    //   preserves the separately rounded fsubs at the original call sites.
    f32 Random::RandomFloat(f32 lfMin, f32 lfMax)
    {
        const volatile f32 lfRange = lfMax - lfMin;
        const f32 lfUnitValue = mafFloatBuffer[muOldestBufferIndex] - 1.0f;

        mauIntegerBuffer[muOldestBufferIndex] =
            ConvertUnsignedFixed32ToFloatRepresentation(static_cast<u32>(muSeed >> 32));
        muSeed              = muSeed * KU_RANDOM_MULTIPLIER + 1;
        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);

        return lfRange * lfUnitValue + lfMin;
    }

    // RandomVecFloat -- DWARF CgsRandom.h:94. Pinned by its expansion in
    // PlaceOnTrackManager::GetValuesForCarSelect @0x822D3540..0x822D3590 (and the PS3 DecFIGS
    // twin @0x1332F0): the ring slot is the VECTOR slot ((muOldestBufferIndex + 3) & 4), the
    // value is LANE 0 of the quad already there (`lvx128` + `vperm` splat of word 0, then
    // `vsubfp` 1.0), ONE LCG step refills that one lane from the OLD seed's high word, and the
    // cursor becomes slot + 1. It is RandomFloat() on the vector slot, not RandomVector().
    rw::math::vpu::VecFloat Random::RandomVecFloat()
    {
        const u32 luSlot = (muOldestBufferIndex + 3) & 4;
        const f32 lfRandomFraction = mafFloatBuffer[luSlot] - 1.0f;
        const u64 luOldSeed = muSeed;

        muSeed = luOldSeed * KU_RANDOM_MULTIPLIER + 1;
        mauIntegerBuffer[luSlot] =
            ConvertUnsignedFixed32ToFloatRepresentation(static_cast<u32>(luOldSeed >> 32));
        muOldestBufferIndex = luSlot + 1;

        return rw::math::vpu::VecFloat(lfRandomFraction);
    }

    // The bounded VECTOR draw (X360 0x822214F8..0x82221628):
    //   slot = (muOldestBufferIndex + 3) & 4       addi r8,r7,3 ; rlwinm r8,r8,0,29,29
    //   t    = the quad already in that slot, -1.0 lvx128 v8 ; vsubfp v8, v8, ONE
    //   TWO LCG steps, their two high words packed across THREE slots
    //   muOldestBufferIndex = slot + 3
    //   return (lMax - lMin) * t + lMin            vsubfp v6, v10, v12 ; vmaddfp v12,v8,v12,v6
    // ⭐ THE COMBINE IS FUSED, lane by lane (FX-AIBUZZ, crash parity 2026-09-25). IDA prints the
    //   classic vmaddfp in raw field order D, A, B, C, and it means D = A * C + B. So
    //   `vmaddfp v12, v8, v12, v6` @0x822215CC (word 0x118861AE: vD 12, vA 8, vB 12, vC 6) is
    //   v12 = t * (max - min) + min. Here v6 = v10 - v12 (vsubfp @0x82221570), v12 = min
    //   (= -max, vxor sign flip @0x8222155C), and v8 = ring quad - 1.0 (vsubfp @0x82221590).
    //   vmaddfp rounds each lane ONCE. The PC's `(max - min) * t + min` rounded the product
    //   first and missed by an ulp on some inputs, w included (the console computes all four
    //   lanes). The same range * t + min combine, in ONE vmaddfp, closes both randomisers that
    //   pack the ring identically: Vector3Randomiser::RandomiseXYZ @0x82277FA8 and
    //   Vector4Randomiser::RandomiseXYZW @0x822780BC. Both are `vmaddfp v0, v13, v12, v0` with
    //   v13 = +0x10 (range) and v12 = +0 (min), and BrnEffectsUtils.cpp already spells them
    //   std::fma. JumpStateMachine::FireWheelDebris does the same on a RandomVecFloat splat
    //   (`vmaddfp v13, v5, v13, v11` @0x82293C20, v5 = max - min @0x82293C08). Hence std::fmaf
    //   in the native vector instruction below.
    // ⚠️ THE FOURTH LANE OF THE RING SLOT IS NEVER REFILLED -- only slot+0/+1/+2 are written,
    //   so lane w carries whatever the previous vector draw left there. Read, and returned,
    //   exactly as the console does; every consumer uses xyz only.
    // ⚠️ The index is stored TWICE (as `slot` at 0x82221540 and as `slot + 3` at 0x82221628)
    //   because each of the three ring writes re-reads it and indexes off it. Reproduced as
    //   two assignments rather than folded into one, so the intermediate state matches.
    rw::math::vpu::Vector3 Random::RandomVector(rw::math::vpu::Vector3 lMin,
                                                rw::math::vpu::Vector3 lMax)
    {
        const u64 lu64Seed0 = muSeed;
        const u64 lu64Seed1 = lu64Seed0 * KU_RANDOM_MULTIPLIER + 1;
        const u64 lu64Seed2 = lu64Seed1 * KU_RANDOM_MULTIPLIER + 1;

        const u32 luSeed0High = static_cast<u32>(lu64Seed0 >> 32);
        const u32 luSeed1High = static_cast<u32>(lu64Seed1 >> 32);

        const u32 luSlot = (muOldestBufferIndex + 3) & 4;
        muOldestBufferIndex = luSlot;

        const __vector4 lUnitValue = __vsubfp(__lvx(mafFloatBuffer, luSlot * sizeof(f32)), XMVectorReplicate(1.0f));

        muSeed = lu64Seed2;

        const u32 luOne = KU_IEEE_754_REPRESENTATION_FLOAT_ONE;
        mauIntegerBuffer[luSlot + 0] = luOne | ((luSeed1High << 2) & 0x7FFFFC);
        mauIntegerBuffer[luSlot + 1] = luOne | ((luSeed0High << 13) & 0x7FE000)
                                             | (luSeed1High >> 19);
        mauIntegerBuffer[luSlot + 2] = luOne | (luSeed0High >> 9);
        muOldestBufferIndex = luSlot + 3;

        rw::math::vpu::Vector3 lResult;
        const __vector4 lMinimum = __lvx(&lMin, 0);
        const __vector4 lRange = __vsubfp(__lvx(&lMax, 0), lMinimum);
        __stvx(__vmaddfp(lUnitValue, lRange, lMinimum), &lResult, 0);
        return lResult;
    }
}
