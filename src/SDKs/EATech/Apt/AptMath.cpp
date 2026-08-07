#include "SDKs/EATech/Apt/AptMath.h"

#include "SDKs/EATech/include/Apt/AptSharedPtr.h"   // gpAptSharedPtrPool (off_8324D808)
#include "SDKs/EATech/Apt/DogmaAllocator.h"          // DOGMA_PoolManager::Allocate

// ===========================================================================
//  AptMath -- APT clip-stack matrix math.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. X360 asm is
//  authoritative. See AptMath.h for the per-entry layout and addresses.
//
//  Process-wide clip-stack state (the X360 .data symbols):
//      word_8324E390   u16   top index               (msnClipStackIndex)
//      dword_8324E384  ptr   16-aligned entry base    (mpClipStackBase)
//      dword_8324E388  ptr   raw (unaligned) allocation (mpClipStackRaw)
//      word_8324E38C   u16   capacity / depth         (msnClipStackDepth)
// ===========================================================================

namespace
{
    // X360 .data: the single global clip stack.
    // (x64 widening, Phase-0 regime): the console stores the entry-array pointers as
    // 32-bit ints; on x64 the heap base is 64-bit, so the base/raw pointer slots are
    // widened to intptr_t (a 32-bit int would truncate the base -> the makeUnit write
    // corrupts memory).
    u16      gsnClipStackIndex = 0;   // word_8324E390
    intptr_t giClipStackBase   = 0;   // dword_8324E384  (16-aligned entry array)
    intptr_t giClipStackRaw    = 0;   // dword_8324E388  (raw allocation pointer)
    u16      gsnClipStackDepth = 0;   // word_8324E38C
}

namespace AptMath
{

// @0x82AD5D08 -- base + stride * top index (x64 stride 0x80: XB1 `shl rax,7` @0x140834240).
intptr_t ClipStackGetTop()
{
    return sizeof(AptClipMatrixEntry) * gsnClipStackIndex + giClipStackBase;
}

// @0x82AD5CB0 -- pre-increment index, return new top pointer.
intptr_t ClipStackPush()
{
    gsnClipStackIndex = static_cast<u16>(gsnClipStackIndex + 1);
    return sizeof(AptClipMatrixEntry) * gsnClipStackIndex + giClipStackBase;
}

// @0x82AD5CD8 -- pre-decrement index, return new top pointer.
intptr_t ClipStackPop()
{
    gsnClipStackIndex = static_cast<u16>(gsnClipStackIndex - 1);
    return sizeof(AptClipMatrixEntry) * gsnClipStackIndex + giClipStackBase;
}

// @0x82ADC548 -- write the unit transform + opaque-white colour to the top entry.
//
// flt_82001C98 = 1.0, flt_82001CC0 = 0.0, flt_82010C20 = 255.0. The store set is
// reproduced verbatim from the asm (16-float identity at +0x00..+0x3C, the four
// 255.0 colour-scale stores at +0x44/+0x48/+0x4C/+0x50 -- written +0x48,+0x4C,
// +0x50 then +0x44 -- and the four 0.0 colour-translate stores at +0x58..+0x64,
// written +0x5C,+0x60,+0x64 then +0x58). The +0x40 and +0x54 gaps stay untouched.
void ClipStackMakeUnit()
{
    AptClipMatrixEntry* lpEntry =
        reinterpret_cast<AptClipMatrixEntry*>(sizeof(AptClipMatrixEntry) * gsnClipStackIndex + giClipStackBase);

    const f32 lfOne  = 1.0f;   // flt_82001C98
    const f32 lfZero = 0.0f;   // flt_82001CC0
    const f32 lf255  = 255.0f; // flt_82010C20

    lpEntry->mafMatrix[0]  = lfOne;
    lpEntry->mafMatrix[1]  = lfZero;
    lpEntry->mafMatrix[2]  = lfZero;
    lpEntry->mafMatrix[3]  = lfZero;
    lpEntry->mafMatrix[4]  = lfZero;
    lpEntry->mafMatrix[5]  = lfOne;
    lpEntry->mafMatrix[6]  = lfZero;
    lpEntry->mafMatrix[7]  = lfZero;
    lpEntry->mafMatrix[8]  = lfZero;
    lpEntry->mafMatrix[9]  = lfZero;
    lpEntry->mafMatrix[10] = lfOne;
    lpEntry->mafMatrix[11] = lfZero;
    lpEntry->mafMatrix[12] = lfZero;
    lpEntry->mafMatrix[13] = lfZero;
    lpEntry->mafMatrix[14] = lfZero;
    lpEntry->mafMatrix[15] = lfOne;

    lpEntry->mafScaleArgb[1] = lf255;   // +0x48
    lpEntry->mafScaleArgb[2] = lf255;   // +0x4C
    lpEntry->mafScaleArgb[3] = lf255;   // +0x50
    lpEntry->mafScaleArgb[0] = lf255;   // +0x44

    lpEntry->mafTransArgb[1] = lfZero;  // +0x5C
    lpEntry->mafTransArgb[2] = lfZero;  // +0x60
    lpEntry->mafTransArgb[3] = lfZero;  // +0x64
    lpEntry->mafTransArgb[0] = lfZero;  // +0x58
}

// @0x82AE2470 -- allocate and reset the clip stack for `nDepth` entries.
intptr_t ClipStackInit(int nDepth)
{
    const u16 lsnDepth = static_cast<u16>(nDepth);

    // pool->Allocate(stride*nDepth + 16); x64 stride 0x80 (XB1 shl 7; console was 112). the raw pointer is kept then 16-aligned up.
    // (x64 widening, Phase-0 regime): the raw pointer is captured as intptr_t (not
    // int) so the 64-bit heap base is not truncated; the 16-align math is identical.
    intptr_t liRaw = reinterpret_cast<intptr_t>(
        gpAptSharedPtrPool->Allocate(static_cast<size_t>(sizeof(AptClipMatrixEntry) * nDepth + 0x10)));

    giClipStackRaw = liRaw;

    intptr_t liBase = liRaw;
    if ((liRaw & 0xF) != 0)              // clrlwi. r10, r11, 28
    {
        liBase = (liRaw + 0x10) & ~static_cast<intptr_t>(0xF);  // 16-align up
    }
    giClipStackBase = liBase;

    gsnClipStackDepth = lsnDepth;        // sth r31
    gsnClipStackIndex = 0;               // sth r11(=0)

    ClipStackMakeUnit();
    return ClipStackGetTop();            // the unit-entry pointer (base + 0)
}

// @0x82AD5D40 -- 2D affine matrix multiply of two 4x4 (16-float) clip matrices.
//
// The X360 first copies both 16-float sources into locals, then composes the
// linear 2x2 (rows 0,1 cols 0,1) and the translation row (row 3 cols 0,1). Only
// those six destination components are written; the rest of `result` is left as
// the caller provided. Naming: `a2` is the left/base matrix, `a3` the right.
f32* MatMul2d(f32* result, const f32* a2, const f32* a3)
{
    // row0/row1 linear 2x2
    result[0] = (a2[4] * a3[1]) + (a2[0] * a3[0]);
    result[1] = (a2[5] * a3[1]) + (a2[1] * a3[0]);
    result[4] = (a3[5] * a2[4]) + (a3[4] * a2[0]);
    result[5] = (a3[5] * a2[5]) + (a3[4] * a2[1]);

    // row3 translation (cols 0,1), carrying a2's own translation through
    result[12] = ((a3[13] * a2[4]) + (a3[12] * a2[0])) + a2[12];
    result[13] = ((a3[13] * a2[5]) + (a3[12] * a2[1])) + a2[13];

    return result;
}

} // namespace AptMath
