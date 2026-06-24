#pragma once

// DXTDecodeJob — the "decode a DXT-compressed texture" job (dispatched via the DXTDecodeEntry
// trampoline). Only the Execute entry shim is bodied in this TU: it latches the job-desc
// pointer into the job object, then forwards four desc fields to the dxt_decode codec.
//
// The actual DXT decompression (dxt_decode) is a codec and lives in its own TU; it is
// declared-only here so the Execute shim can name it. This TU bodies ONLY the dispatch
// wrapper, not the codec.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, Execute @0x82C07590):
//   DXTDecodeJob this[0] @ +0x00 -- mpDesc : the job-desc pointer (stw r11/r4, 0(r3))
//
//   DXTDecodeJobDesc (the *a2 the entry passes), fields the asm forwards to dxt_decode:
//     +0x00 -> dxt_decode arg2  (lwz r4, 0(r11))
//     +0x04 -> dxt_decode arg1  (lwz r3, 4(r11))
//     +0x10 -> dxt_decode arg3  (lwz r5, 0x10(r11))
//     +0x14 -> dxt_decode arg4  (lwz r6, 0x14(r11))

#include "types.hpp"

// The DXT decode job descriptor. Only the four fields the dispatch shim forwards are
// individually named at their asm offsets; the remaining slots are reserved padding so the
// named fields land at the asm-exact displacements (their meanings are not attested by this
// dispatch-only TU -- see dep_flags).
struct DXTDecodeJobDesc
{
    u32 mu32Field00; // +0x00  -> dxt_decode arg2
    u32 mu32Field04; // +0x04  -> dxt_decode arg1
    u32 mu32Reserved08;
    u32 mu32Reserved0C;
    u32 mu32Field10; // +0x10  -> dxt_decode arg3
    u32 mu32Field14; // +0x14  -> dxt_decode arg4
};

// dxt_decode — the DXT decompression codec (declared-only; bodied in its own codec TU).
// Signature/arity recovered from the Execute call site (four 32-bit args in r3..r6).
extern "C" int dxt_decode(u32 a1, u32 a2, u32 a3, u32 a4);

struct DXTDecodeJob
{
    // Execute @0x82C07590 — latch the job-desc pointer, then run the codec on its fields.
    int Execute(const DXTDecodeJobDesc* apDesc);

    const DXTDecodeJobDesc* mpDesc; // +0x00  (asm: stw r4, 0(r3))
};
