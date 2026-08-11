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
//   * SetSeed (SelectionHistory<512,u16,u16,65536>::Randomize @0x826C5900 head):
//     the 32-bit seed word is OR'd under the multiplier's HIGH half
//     (seed | 0x5851F42D00000000), one LCG prime step runs, and the float-ring
//     oldest index resets to 0 (`*(a1+48) = (seed|K_hi)*K + 1; *(a1+56) = 0`).
//
// Homed 2026-07-05 because wave49's SelectionHistory::Randomize (CgsSoundUtils)
// calls SetSeed/RandomUInt(min,max) out-of-line, which broke the exe link (the
// methods were declared-only). This TU is their canonical home when the full
// CgsRandom ledger work lands; the bodies below are the attested LCG.
//
// FLAG (bounded draw): RandomUInt(min,max) has no exported out-of-line X360
// body (always inlined + strength-reduced, e.g. the %3 mulhwu idiom in
// SetFlashingHeadlights). The canonical modulo reduction over the raw draw is
// reconstructed by that idiom's intent: min + draw % (max - min). Its one
// caller today (Randomize's Fisher-Yates, itself CONFIDENCE-low) passes
// (0, 512).
// ===========================================================================

#include "GameShared/GameClasses/Numeric/CgsRandom.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (RandomInt's two baked bounds asserts)

#include "rw/math/vpu/types.h"   // Vector3 -- RandomVector's DEFINITION needs the complete
                                 //   type (the header itself deliberately only forward-declares
                                 //   it, to stay standalone)

namespace CgsNumeric
{
    // The shared LCG multiplier (every inlined site: hi 0x5851F42D, lo 0x4C957F2D).
    static const u64 KU_RANDOM_LCG_MULTIPLIER = 0x5851F42D4C957F2Dull;

    // Inline-site semantics (@0x826C5900 head): fold the seed under the multiplier's
    // high half, run one prime step, reset the float-ring cursor.
    void Random::SetSeed(u64 lu64Seed)
    {
        muSeed = (lu64Seed | 0x5851F42D00000000ull);
        muSeed = muSeed * KU_RANDOM_LCG_MULTIPLIER + 1u;
        muOldestBufferIndex = 0;
    }

    // Inline-site semantics (@0x827537D0..E4): draw the high word, then step.
    u32 Random::RandomUInt()
    {
        const u32 luDraw = static_cast<u32>(muSeed >> 32);
        muSeed = muSeed * KU_RANDOM_LCG_MULTIPLIER + 1u;
        return luDraw;
    }

    // FLAG: reconstructed by the inline sites' reduction intent (see the header
    // note) -- min + draw % span; a zero span returns min.
    u32 Random::RandomUInt(u32 luMin, u32 luMax)
    {
        const u32 luSpan = luMax - luMin;
        if (luSpan == 0u)
            return luMin;
        return luMin + (RandomUInt() % luSpan);
    }

    // ========================================================================
    // THE BOUNDED SIGNED-INT DRAW -- BODIED 2026-08-11 (SetupParRivals wave).
    //
    // ⚠️ NO STANDALONE X360 SYMBOL: the console inlines it at all 16 of its call sites (every
    // one of them carries the same pair of baked assert literals, which is how they were
    // enumerated -- grep the export set for `aLimaxLimin`). Two of those expansions were read
    // in full, and they agree instruction for instruction:
    //
    //   * CgsAlgorithms::Shuffle<u16, Stack<u16,1>> @0x8271B420 -- THE GENERAL FORM, because
    //     its liMin is a runtime value (r25) rather than a constant:
    //         0x8271B49C  cmpw  cr6, r28, r25 ; bge      -> assert(liMax >= liMin)   [.h:320]
    //         0x8271B474  subf  r11, r25, r28 ; addi r27, r11, 1
    //                                                     -> luMod = liMax - liMin + 1
    //         0x8271B4BC  cmplwi cr6, r27, 0  ; bne      -> assert(luMod > 0)        [.h:323]
    //         0x8271B4E0  ld    r11, 0x20(random)         -> the OLD seed
    //         0x8271B4F8  srdi  r9, r11, 32 ; clrlwi r11, r9, 0
    //                                                     -> draw == (u32)(OLD seed >> 32)
    //         0x8271B4FC  mulld/addi 1 / std 0x20         -> muSeed = old * K + 1
    //         0x8271B50C  divwu/mullw/subf                -> draw % luMod  (UNSIGNED)
    //         0x8271B518  add   r31, r11, r25             -> + liMin
    //   * StreetManager::SetupParRivals @0x8233F560 (0x8233F854..0x8233F8CC) -- the same
    //     sequence with liMin folded to the constant 0, so the trailing `add` disappears and
    //     the `subf` collapses into `addi r29, r28, 1`.
    //
    // ⚠️ THE DRAW IS THE SEED *BEFORE* THE STEP (`srdi` of the old value precedes the `std` of
    // the new one in both expansions) and the reduction is UNSIGNED (`divwu`), even though the
    // bounds are signed. Both are observable and are reproduced exactly. The `twllei r27, 0`
    // that guards the divide is the compiler's own trap for the assert above it; it has no
    // C++ counterpart.
    //
    // ⚠️ IT DOES NOT TOUCH THE RING BUFFER -- same as RandomBool. Neither expansion reads or
    // writes mafFloatBuffer/muOldestBufferIndex; only muSeed moves.
    //
    // FLAG (inlining reversal): the four instructions that draw-and-step are byte-identical to
    // RandomUInt()'s own attested expansion (@0x827537D0..E4), so they are written as that call
    // rather than re-inlined here. Semantics are unchanged; only the source structure is a
    // reconstruction choice.
    s32 Random::RandomInt(s32 liMin, s32 liMax)
    {
        CGS_ASSERT(liMax >= liMin, "liMax >= liMin");

        const u32 luMod = static_cast<u32>(liMax - liMin) + 1u;
        CGS_ASSERT(luMod > 0u, "luMod > 0");

        return liMin + static_cast<s32>(RandomUInt() % luMod);
    }

    // ========================================================================
    // THE TWO BOUNDED FLOAT DRAWS -- BODIED 2026-08-02 (rotate-helper wave).
    //
    // Both were declaration-only, and together they were two of the three named blockers on
    // mounting BrnDirector::Camera::Utils::CameraShake::Update -- i.e. on retiring the empty
    // `{}` that DirectorLinkStubs.cpp had been resolving that function to.
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
    // ⚠️ The console evaluates `lfMax - lfMin` before the ring read completes and folds the
    //   range map into a single fmadds; the order does not change the result and is why there
    //   is no separate multiply in the asm.
    f32 Random::RandomFloat(f32 lfMin, f32 lfMax)
    {
        const f32 lfUnitValue = mafFloatBuffer[muOldestBufferIndex] - 1.0f;

        mauIntegerBuffer[muOldestBufferIndex] =
            ConvertUnsignedFixed32ToFloatRepresentation(static_cast<u32>(muSeed >> 32));
        muSeed              = muSeed * KU_RANDOM_MULTIPLIER + 1;
        muOldestBufferIndex = (muOldestBufferIndex + 1) & (KU_FLOAT_BUFFER_SIZE - 1);

        return (lfMax - lfMin) * lfUnitValue + lfMin;
    }

    // The bounded VECTOR draw (X360 0x822214F8..0x82221628):
    //   slot = (muOldestBufferIndex + 3) & 4       addi r8,r7,3 ; rlwinm r8,r8,0,29,29
    //   t    = the quad already in that slot, -1.0 lvx128 v8 ; vsubfp v8, v8, ONE
    //   TWO LCG steps, their two high words packed across THREE slots
    //   muOldestBufferIndex = slot + 3
    //   return (lMax - lMin) * t + lMin            vsubfp v6, v10, v12 ; vmaddfp v12,v8,v12,v6
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

        rw::math::vpu::Vector3 lUnitValue;
        lUnitValue.x = mafFloatBuffer[luSlot + 0] - 1.0f;
        lUnitValue.y = mafFloatBuffer[luSlot + 1] - 1.0f;
        lUnitValue.z = mafFloatBuffer[luSlot + 2] - 1.0f;
        lUnitValue.w = mafFloatBuffer[luSlot + 3] - 1.0f;

        muSeed = lu64Seed2;

        const u32 luOne = KU_IEEE_754_REPRESENTATION_FLOAT_ONE;
        mauIntegerBuffer[luSlot + 0] = luOne | ((luSeed1High << 2) & 0x7FFFFC);
        mauIntegerBuffer[luSlot + 1] = luOne | ((luSeed0High << 13) & 0x7FE000)
                                             | (luSeed1High >> 19);
        mauIntegerBuffer[luSlot + 2] = luOne | (luSeed0High >> 9);
        muOldestBufferIndex = luSlot + 3;

        rw::math::vpu::Vector3 lResult;
        lResult.x = (lMax.x - lMin.x) * lUnitValue.x + lMin.x;
        lResult.y = (lMax.y - lMin.y) * lUnitValue.y + lMin.y;
        lResult.z = (lMax.z - lMin.z) * lUnitValue.z + lMin.z;
        lResult.w = (lMax.w - lMin.w) * lUnitValue.w + lMin.w;
        return lResult;
    }
}
