#include "SDKs/EATech/Apt/AptActionTryCatchFinallyBlock.h"

// ===========================================================================
//  AptAction_TryCatchFinallyBlock accessors.
//
//  Reconstructed from BURNOUT_X360_ARTIST.XEX and re-pinned against the x64 XB1
//  export (rung 1 for Apt): the accessors are const MEMBER functions
//  (?getTryBlockBase@AptAction_TryCatchFinallyBlock@@QEBAPEAEXZ etc.) and the
//  body starts at +0x18 (`lea rax,[rcx+18h]`) -- the console's +0x14 plus the
//  widened caught-var-name pointer. The block-size words at +0x00 / +0x04 are
//  added to the record base as RAW BYTE offsets.
// ===========================================================================

// x64 twin of @0x82AD5268: lea rax,[rcx+18h]
const u8* AptAction_TryCatchFinallyBlock::getTryBlockBase() const
{
    return reinterpret_cast<const u8*>(this) + sizeof(AptAction_TryCatchFinallyBlock);
}

// x64 twin of @0x82AD5270: base + trySize + 0x18
const u8* AptAction_TryCatchFinallyBlock::getCatchBlockBase() const
{
    return reinterpret_cast<const u8*>(this) + muTryCodeSize
         + sizeof(AptAction_TryCatchFinallyBlock);
}

// x64 twin of @0x82AD5280: base + trySize + catchSize + 0x18
const u8* AptAction_TryCatchFinallyBlock::getFinallyBlockBase() const
{
    return reinterpret_cast<const u8*>(this) + muTryCodeSize + muCatchCodeSize
         + sizeof(AptAction_TryCatchFinallyBlock);
}

// @0x82AD5C20: bit0 of the flags byte at +0x0C
int AptAction_TryCatchFinallyBlock::hasCatchBlock() const
{
    return muFlags & 1;
}

// @0x82AD5248: bit1
int AptAction_TryCatchFinallyBlock::hasFinallyBlock() const
{
    return (muFlags >> 1) & 1;
}

// @0x82AD5258: bit2
int AptAction_TryCatchFinallyBlock::putCaughtObjectInRegister() const
{
    return (muFlags >> 2) & 1;
}
