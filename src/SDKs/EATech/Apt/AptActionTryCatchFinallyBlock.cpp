#include "SDKs/EATech/Apt/AptActionTryCatchFinallyBlock.h"

// ===========================================================================
//  AptAction_TryCatchFinallyBlock accessors.
//
//  Store-for-store reconstruction from BURNOUT_X360_ARTIST.XEX. X360 asm is
//  authoritative. The block-size words at +0x00 / +0x04 are added to the record
//  base as RAW BYTE offsets (X360 `add r11, <size>, r3`), so all arithmetic is
//  done in bytes. See AptActionTryCatchFinallyBlock.h for the header layout.
// ===========================================================================

namespace AptAction_TryCatchFinallyBlock
{

// @0x82AD5268:  addi r3, r3, 0x14
int getTryBlockBase(int iRecord)
{
    return iRecord + KU_BODY_OFFSET;
}

// @0x82AD5270:  lwz r11, 0(r3); add r11, r11, r3; addi r3, r11, 0x14
int getCatchBlockBase(const void* pRecord)
{
    const u8* lpBase = static_cast<const u8*>(pRecord);
    const u32 luTrySize = *reinterpret_cast<const u32*>(lpBase + 0x00);   // serialized .apt Try record: trySize @+0
    return static_cast<int>(reinterpret_cast<int>(lpBase) + luTrySize + KU_BODY_OFFSET);
}

// @0x82AD5280:  lwz r11, 4(r3); lwz r10, 0(r3); add r11,r11,r10;
//               add r11,r11,r3; addi r3, r11, 0x14
int getFinallyBlockBase(const void* pRecord)
{
    const u8* lpBase = static_cast<const u8*>(pRecord);
    const u32 luTrySize   = *reinterpret_cast<const u32*>(lpBase + 0x00);   // serialized .apt Try record: trySize @+0
    const u32 luCatchSize = *reinterpret_cast<const u32*>(lpBase + 0x04);   // serialized .apt Try record: catchSize @+4
    return static_cast<int>(
        reinterpret_cast<int>(lpBase) + luCatchSize + luTrySize + KU_BODY_OFFSET);
}

// @0x82AD5C20:  lbz r11, 0xC(r3); clrlwi r3, r11, 31    -> bit0
int hasCatchBlock(const void* pRecord)
{
    const u8 luFlags = *(static_cast<const u8*>(pRecord) + 0xC);
    return luFlags & 1;
}

// @0x82AD5248:  lbz r11, 0xC(r3); extrwi r3, r11, 1,30  -> bit1
int hasFinallyBlock(const void* pRecord)
{
    const u8 luFlags = *(static_cast<const u8*>(pRecord) + 0xC);
    return (luFlags >> 1) & 1;
}

// @0x82AD5258:  lbz r11, 0xC(r3); extrwi r3, r11, 1,29  -> bit2
int putCaughtObjectInRegister(const void* pRecord)
{
    const u8 luFlags = *(static_cast<const u8*>(pRecord) + 0xC);
    return (luFlags >> 2) & 1;
}

} // namespace AptAction_TryCatchFinallyBlock
