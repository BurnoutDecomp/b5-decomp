#pragma once

// ===========================================================================
//  AptActionTryCatchFinallyBlock.h  --  EATech Apt (ActionScript player)
//  ActionTry bytecode-record accessor.
//
//  ActionScript's ActionTry opcode lays out a header followed by three variable
//  length code blocks (try / catch / finally). AptAction_TryCatchFinallyBlock is
//  a STRUCT whose const member functions compute the in-stream base address of
//  each block and read the presence/behaviour flags byte -- the x64 XB1 manglings
//  (?getTryBlockBase@AptAction_TryCatchFinallyBlock@@QEBAPEAEXZ etc., QEBA =
//  const member) prove the member-function linkage, and B4Extern corroborates
//  the record shape.
//
//  RECORD LAYOUT (x64; the resolved in-memory record _parseStream builds --
//  pointer fields widen, so the body starts at +0x18, NOT the console's +0x14):
//      +0x00  u32    muTryCodeSize      (added to base for the catch block)
//      +0x04  u32    muCatchCodeSize    (added for the finally block)
//      +0x08  u32    muFinallyCodeSize  (B4; unread by the six accessors)
//      +0x0C  u8     muFlags  (bit0 hasCatch, bit1 hasFinally, bit2 catchInRegister)
//      +0x0F  u8     muCaughtRegister   (B4)
//      +0x10  char*  mpszCaughtVarName  (B4 szCaughtVarName; widened 4->8)
//      +0x18  ...    start of the try block bytecode
//        (x64 getTryBlockBase @0x82AD5268-twin: `lea rax,[rcx+18h]` -- the +4
//         delta vs console is exactly the widened name pointer.)
// ===========================================================================

#include <cstddef>   // offsetof (_AssertLayout)

#include "types.hpp"

struct AptAction_TryCatchFinallyBlock
{
    u32   muTryCodeSize;       // +0x00
    u32   muCatchCodeSize;     // +0x04
    u32   muFinallyCodeSize;   // +0x08 (B4; unread by the accessors)
    u8    muFlags;             // +0x0C (bit0 hasCatch / bit1 hasFinally / bit2 catchInRegister)
    u8    muPad0D[2];          // +0x0D
    u8    muCaughtRegister;    // +0x0F (B4)
    const char* mpszCaughtVarName;   // +0x10 (B4 szCaughtVarName)

    // Layout pinned against the x64 XB1 export (never called; member body gives
    // complete-class offsetof context).
    static void _AssertLayout()
    {
        static_assert(offsetof(AptAction_TryCatchFinallyBlock, muTryCodeSize)     == 0x00, "x64: getCatchBlockBase reads [this+0]");
        static_assert(offsetof(AptAction_TryCatchFinallyBlock, muCatchCodeSize)   == 0x04, "x64: getFinallyBlockBase reads [this+4]");
        static_assert(offsetof(AptAction_TryCatchFinallyBlock, muFlags)           == 0x0C, "x64: has*/putCaughtObjectInRegister read byte [this+0Ch]");
        static_assert(offsetof(AptAction_TryCatchFinallyBlock, mpszCaughtVarName) == 0x10, "B4 szCaughtVarName; the widened pointer puts the body at +0x18");
        static_assert(sizeof(AptAction_TryCatchFinallyBlock) == 0x18, "x64: getTryBlockBase = lea rax,[this+18h]");
    }

    // The try block starts immediately after the (x64) 0x18-byte header.
    const u8* getTryBlockBase() const;

    // The catch block follows the try block: this + muTryCodeSize + 0x18.
    const u8* getCatchBlockBase() const;

    // The finally block follows try+catch: this + muTryCodeSize + muCatchCodeSize + 0x18.
    const u8* getFinallyBlockBase() const;

    int hasCatchBlock() const;              // bit0 of muFlags
    int hasFinallyBlock() const;            // bit1 of muFlags
    int putCaughtObjectInRegister() const;  // bit2 of muFlags
};
