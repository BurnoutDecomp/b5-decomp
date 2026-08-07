#pragma once

// ===========================================================================
//  AptActionDefineFunction2.h  --  EATech Apt (ActionScript player)
//  ActionDefineFunction2 bytecode-record accessor.
//
//  ActionScript's ActionDefineFunction2 opcode carries a 16-bit register/scope
//  flags word. AptAction_DefineFunction2 is a namespace of free helpers (no
//  `this`; the X360 symbol is AptAction_DefineFunction2::getDF2Flag) that read
//  fields out of the raw, in-stream action record.
//
//  NO DWARF exists for this class (X360-only). The 16-bit flags word at +0x0A
//  is derived STRICTLY from the X360 binary:
//      AptAction_DefineFunction2::getDF2Flag  @ 0x82AD5230
// ===========================================================================

#include "types.hpp"

namespace AptAction_DefineFunction2
{
    // @0x82AD5230 (X360) / x64 @0x14084E310 -- test one of the DefineFunction2
    // flags. x64: the signed 16-bit flags word is at record +0x0E (the console's
    // +0x0A plus the widened leading szName pointer).
    //
    // The record's flags live in a SIGNED 16-bit word at +0x0A (X360 `lha`).
    // Returns the masked bit: flags & (1 << nFlagBit). `pRecord` is the raw
    // action-record pointer; only the +0x0A halfword is touched.
    int getDF2Flag(const void* pRecord, char nFlagBit);
}
