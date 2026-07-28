#pragma once

// ===========================================================================
// XGRAPHICS::IRProjection -- the "projection" node kind in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A IRInst
// (opcode 128): a projection instruction has exactly one output and one input
// operand (miNumOutputs = 1, miNumInputs = 1) and, unlike the load nodes, binds
// no operand VReg in its ctor. This header is the canonical OWNING home for the
// class TU IRProjection:
//
//     XGRAPHICS::IRProjection::IRProjection              @ 0x82C2C2B0 (inlined ctor)
//     XGRAPHICS::IRProjection::NewInst                   @ 0x82C2C2B0 (arena factory)
//     XGRAPHICS::IRProjection::InstType                  @ 0x82C2B590
//     XGRAPHICS::IRProjection::`scalar deleting dtor'    @ 0x82C2B5A0 (thunk, dropped)
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm. IRProjection adds no members of its own
// (construction only writes inherited IRInst fields + stamps its own vtable
// off_821B2440, and the console arena-block immediate 0x3C4 == 964 == console
// sizeof(IRInst) 960 plus the 4-byte arena prefix); the ctor was folded inline
// into NewInst by the compiler and is
// de-inlined here (per the outlining-inlined-functions guidance), mirroring the
// IRMov arena-factory idiom. `XGRAPHICS` is an X360 graphics-SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
//
// WHAT IRProjection ACTUALLY OVERRIDES (recovered by diffing its vtable
// off_821B2440 against the IRInst vtable off_821AF190 slot-for-slot; both tables
// are 26 words, the "ir_projection" string literal starts immediately after
// IRProjection's last slot at 0x821B24A8). Six slots diverge:
//
//   slot  IRInst @0x821AF190                 IRProjection @0x821B2440
//    1    0x827DDDA8 IRInst::OperationInputs   0x82C296C8  `li r3,1; blr`
//    2    0x82C28630 IRInst::OperationOutputs  0x82C296C8  `li r3,1; blr`
//    3    0x82C08F60 _purecall                 0x8284CB38  `blr` (empty body)
//   13    0x827E2F38 `li r3,0; blr`            0x82C296C8  `li r3,1; blr`
//   18    0x827E2F38 `li r3,0; blr`            0x82C296C8  `li r3,1; blr`
//   23    0x82C28638 IRInst::InstType          0x82C2B590 IRProjection::InstType
//
// Consequences of that diff, none of which may be denied by a later sweep:
//   * IRInst is ABSTRACT on console -- slot 3 (the slot IRMov fills with
//     IRMov::Assemble @0x82C280E8) holds _purecall in the IRInst table, and
//     IRProjection is one of its concrete implementers, supplying an empty body.
//   * Slots 1/2 (OperationInputs / OperationOutputs) are overridden with constant
//     `return 1` in ADDITION to the miNumInputs / miNumOutputs = 1 stores the ctor
//     makes -- the constants are not merely a consequence of those stores.
//   * Slots 13 and 18 are two unnamed IRInst predicates that IRProjection flips
//     from false to true. Their names are not recoverable from this TU.
//   * Every overriding body except InstType is a COMDAT-folded trivial function
//     with no address unique to IRProjection (the linker shared 0x82C296C8 /
//     0x8284CB38 with unrelated classes), which is why the ledger lists only
//     InstType, NewInst and the deleting-dtor thunk -- NOT because the overrides
//     do not exist.
//
// The overrides other than InstType are therefore NOT reconstructed here: the
// base IRInst reconstruction declares no Assemble / slot-13 / slot-18 virtuals to
// override, and inventing virtuals on this derived class alone would fabricate
// vtable slots the console does not have. See still_open in the TU record.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsIRInst.h" // XGRAPHICS::IRInst (base)

namespace XGRAPHICS
{

class CFG; // owning compiler context (arena source; homed in XGraphicsCFG.h)

struct IRProjection : public IRInst
{
    // @ 0x82C2C2B0 (folded into NewInst) -- construct a projection node: delegate
    // to the IRInst base ctor with opcode 128 and mark it single-output /
    // single-input. No operand slots are touched (unlike IRMov).
    //
    // `apContext` IS CONSUMED BY THE BASE CTOR. NewInst sets up three argument
    // registers -- `mr r5, r31` (context) / `li r4, 0x80` (opcode) / `bl
    // XGRAPHICS__IRInst__IRInst` -- and the callee at 0x82C2B030 uses r5:
    //     lwz  r10, 0x560(r5) ; stw r10, 0xE0(r3)   // node id := context counter
    //     lwz  r10, 0x560(r5) ; addi r10, r10, 1
    //     stw  r10, 0x560(r5)                       // post-increment the counter
    //     stw  r5,  0x3B8(r3)                       // mpContext := context
    // so the context is stored in the node and the graph's node counter advances.
    // The locally reconstructed IRInst declares only the 1-arg form `IRInst(s32)`
    // (XGraphicsIRInst.h, and that ctor has NO definition anywhere in the tree),
    // so this ctor cannot yet forward the context; class:XGRAPHICS::IRInst is a
    // separate, currently `blocked` TU and is not editable from here. The missing
    // forward is recorded as a cross-TU blocker rather than papered over by
    // re-doing the base ctor's stores in this derived ctor.
    explicit IRProjection(CFG* apContext);

    // @ 0x82C2B5A0 -- destructor. The scalar-deleting-destructor thunk is
    // compiler-generated: it stamps the DLIST-NODE base vtable &off_821AEB78 (slot
    // 0 of that table is XGRAPHICS::DListNode::`vector deleting destructor'; the
    // intermediate IRInst vtable stamp was eliminated as a dead store, and the
    // IRInst thunk at 0x82C28648 stamps the very same word), then, when the
    // deleting flag is set, routes the storage back to its arena via the
    // ArenaFreePrefixed idiom -- Arena::Free(*(this - 1)). Thunks are dropped per
    // project policy; the destructor proper performs no member teardown in the
    // asm, so the class destructor is trivial.
    virtual ~IRProjection();

    // @ 0x82C2B590 -- instruction-type tag string ("ir_projection").
    //
    // On console this is a VIRTUAL OVERRIDE: 0x82C2B590 has exactly one reference
    // in the whole image, the vtable word 0x821B249C = off_821B2440 + 0x5C, i.e.
    // slot 23 -- the same slot index that holds IRInst::InstType (0x82C28638) in
    // off_821AF190 and IRAlu::InstType (0x82C28698) in the IRMov table. It is
    // declared non-virtual here ONLY because the base declares it non-virtual
    // (XGraphicsIRInst.h); adding `virtual` on this derived declaration alone
    // would mint a bogus extra vtable slot instead of an override. Making
    // IRInst::InstType virtual is a cross-TU edit -- see still_open.
    const char* InstType();

    // @ 0x82C2C2B0 -- arena factory: carve a prefixed IRProjection block from the
    // context's arena and construct one in place. Returns null on the X360 Malloc
    // -4 OOM sentinel (surfaced by ArenaAllocPrefixed). The dispatch `aiOpcode`
    // argument is ignored -- a projection's opcode is fixed at 128.
    static IRProjection* NewInst(s32 aiOpcode, CFG* apContext);

    // Layout pin. IRProjection adds NO members of its own -- the console block
    // size immediate (0x3C4 = the plain IRInst size 0x3C0 plus the 4-byte arena
    // prefix) is shared by all 18 IR-node factories and every ctor store lands
    // in an inherited field. That "adds nothing" fact is pointer-width invariant,
    // so it is safe to pin on this x64 gate; the literal 0x3C4 is NOT, and is
    // recorded in comments only.
    static void _AssertLayout()
    {
        static_assert(sizeof(IRProjection) == sizeof(IRInst),
                      "IRProjection must add no members of its own");
    }
};

} // namespace XGRAPHICS
