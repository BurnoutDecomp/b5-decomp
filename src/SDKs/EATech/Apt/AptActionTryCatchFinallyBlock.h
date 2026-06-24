#pragma once

// ===========================================================================
//  AptActionTryCatchFinallyBlock.h  --  EATech Apt (ActionScript player)
//  ActionTry bytecode-record accessor.
//
//  ActionScript's ActionTry opcode lays out a header followed by three variable
//  length code blocks (try / catch / finally). AptAction_TryCatchFinallyBlock is
//  a namespace of free helpers (no `this`; the X360 symbols take the raw record
//  pointer in r3) that compute the in-stream base address of each block and read
//  the presence/behaviour flags byte.
//
//  NO DWARF exists for this class (X360-only). The header layout is derived
//  STRICTLY from the X360 binary:
//      AptAction_TryCatchFinallyBlock::getTryBlockBase           @ 0x82AD5268
//      AptAction_TryCatchFinallyBlock::getCatchBlockBase         @ 0x82AD5270
//      AptAction_TryCatchFinallyBlock::getFinallyBlockBase       @ 0x82AD5280
//      AptAction_TryCatchFinallyBlock::hasCatchBlock             @ 0x82AD5C20
//      AptAction_TryCatchFinallyBlock::hasFinallyBlock           @ 0x82AD5248
//      AptAction_TryCatchFinallyBlock::putCaughtObjectInRegister @ 0x82AD5258
//
//  HEADER LAYOUT (the bytes read by the accessors; body starts at +0x14):
//      +0x00  u32  try-block byte size      (added to base for the catch block)
//      +0x04  u32  catch-block byte size    (added for the finally block)
//      +0x0C  u8   flags  (bit0 hasCatch, bit1 hasFinally, bit2 catchInRegister)
//      +0x14  ...  start of the try block bytecode
// ===========================================================================

#include "types.hpp"

namespace AptAction_TryCatchFinallyBlock
{
    // Body begins 0x14 bytes past the record start.
    enum { KU_BODY_OFFSET = 0x14 };

    // @0x82AD5268 -- the try block starts immediately after the 0x14 header.
    int getTryBlockBase(int iRecord);

    // @0x82AD5270 -- the catch block follows the try block:
    //   record + tryBlockSize(+0x00) + 0x14.
    int getCatchBlockBase(const void* pRecord);

    // @0x82AD5280 -- the finally block follows try+catch:
    //   record + tryBlockSize(+0x00) + catchBlockSize(+0x04) + 0x14.
    int getFinallyBlockBase(const void* pRecord);

    // @0x82AD5C20 -- bit0 of the flags byte at +0x0C.
    int hasCatchBlock(const void* pRecord);

    // @0x82AD5248 -- bit1 of the flags byte at +0x0C.
    int hasFinallyBlock(const void* pRecord);

    // @0x82AD5258 -- bit2 of the flags byte at +0x0C.
    int putCaughtObjectInRegister(const void* pRecord);
}
