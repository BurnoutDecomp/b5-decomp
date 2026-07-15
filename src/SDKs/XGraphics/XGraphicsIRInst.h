#pragma once

// ===========================================================================
// XGRAPHICS::IRInst -- one instruction node in the XGRAPHICS shader-microcode
// intermediate representation (BURNOUT_X360_ARTIST.XEX). It IS-A DListNode (it
// splices itself into a Block's intrusive IR-node lists), carries up to six
// operand slots (each a VReg* plus a cached index/type/modifier), a bank of
// per-channel source registers + swizzles, and an owner-Block back-pointer.
// This header is the canonical OWNING home for the class TU IRInst.
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm of the IRInst methods. The X360 asm is
// authoritative for every store/branch; because the reconstruction is by NAMED
// member (semantic parity, not byte-matching), the widened x64 pointers do not
// preserve the literal X360 offsets. Where a field's PURPOSE is not attested it
// is named maUnk<hex>/muUnk<hex> and FLAGged; its ONLY grounding is the value
// IRInst::Init writes into it. `XGRAPHICS` is an X360 graphics-SDK boundary, so
// its identifiers are preserved verbatim per the naming convention.
//
// ATTESTED members (X360 byte offsets, from the method asm):
//   +0x00 DListNode sub-object (vtable/link words)
//   +0x0C muId              -- node id (Init stores byte-reversed 0)
//   +0x10 miNumOutputs      -- OperationOutputs() returns this
//   +0x14 miNumInputs       -- OperationInputs()  returns this
//   +0x18 miOpcode          -- operation index into the opcode-info table
//   +0x1C maRegs[6]         -- operand register descriptors (VReg*)
//   +0x38 maOperand[6]      -- per-operand cached VReg::GetIndex() (Init = -1)
//   +0x50 maOperandType[6]  -- per-operand cached VReg::GetType()  (Init = 48)
//   +0x80 maModifier[6][4]  -- per-operand write-mask/modifier bytes
//   +0xA8 muUnkA8           -- FLAG scalar word (IRAllocPos::NewInst zeroes it)
//   +0xE8 maSourceRegs[32]  -- per-channel source registers (VReg*)
//   +0x16C maSwizzle[32][4] -- per-channel swizzle (Init = 3)
//   +0x3B0 maDefaultModifier[4] -- default modifier bytes {0,1,2,3}
//   +0x3B4 mpOwningBlock    -- owner Block (Block::Append/Insert stamps it)
//   +0x3B8 mpContext        -- CFG/compiler context (opaque; foreign layout)
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsDListNode.h" // XGRAPHICS::DListNode
#include "SDKs/XGraphics/XGraphicsVReg.h"      // XGRAPHICS::VReg

namespace XGRAPHICS
{

class Block; // owner back-pointer (homed in XGraphicsBlock.h)

// The IR node kind IRInst::Make dispatches to. Only the two attested fields of
// the external opcode-info table are named; see XGraphicsIRInst.cpp.
struct IRInst : public DListNode
{
    u32   muId;                 // +0x0C  node id (byte-reversed 0 at Init)
    s32   miNumOutputs;         // +0x10  OperationOutputs()
    s32   miNumInputs;          // +0x14  OperationInputs()
    s32   miOpcode;             // +0x18  operation index (into the opcode table)
    VReg* maRegs[6];            // +0x1C  operand register descriptors
    s32   muUnk34;              // +0x34  FLAG unattested purpose (Init = 0)
    s32   maOperand[6];         // +0x38  cached VReg::GetIndex() (Init = -1)
    s32   maOperandType[6];     // +0x50  cached VReg::GetType()  (Init = 48)
    u8    maModifier[6][4];     // +0x80  per-operand modifier bytes
    u8    maUnk98[6];           // +0x98  FLAG per-operand byte flags (Init = 0)
    u8    maUnk9E[6];           // +0x9E  FLAG per-operand byte flags (Init = 0)
    u8    muUnkA4;              // +0xA4  FLAG (Init = 0)
    // +0xA8 is a node-flags/scalar word IRInst::Init does not touch; the derived
    // IRAllocPos / IRAllocColor / IRAllocMem ctors (each inlined into their NewInst)
    // zero-stamp it (stw 0, 0xA8). Its only grounding is that zero store, so its
    // purpose is FLAGged unattested -- it is named/typed solely to give that store a
    // NAMED target.
    u32   muUnkA8;              // +0xA8  FLAG unattested (IRAllocPos/Color/Mem NewInst = 0)
    s32   maUnkC8[6];           // +0xC8  FLAG per-operand (Init = 0)
    // +0xE0 is an unmodeled gap word (no attested reader/writer). muUnkE4 @+0xE4
    // is a node-flags word: IRInst::Init does not touch it, but the IRLoadInterp
    // ctor sets bit 0x40 into it (lwz 0xE4; ori 0x40; stw 0xE4). Its only
    // grounding is that OR, so its wider purpose is FLAGged unattested.
    u32   muUnkE0;              // +0xE0  FLAG unattested gap word
    u32   muUnkE4;              // +0xE4  FLAG node flags (IRLoadInterp sets 0x40)
    VReg* maSourceRegs[32];     // +0xE8  per-channel source registers
    s32   maSwizzle[32][4];     // +0x16C per-channel swizzle (Init = 3)
    s32   maUnk380[8];          // +0x380 FLAG (Init = 0)
    s32   maUnk3A0[4];          // +0x3A0 FLAG (Init = 0)
    u8    maDefaultModifier[4]; // +0x3B0 default modifier bytes {0,1,2,3}
    Block* mpOwningBlock;       // +0x3B4 owner block back-pointer
    void*  mpContext;           // +0x3B8 CFG/compiler context (opaque foreign)

    // ---- reconstructed bodies (XGraphicsIRInst.cpp) -----------------------

    // Base constructor taking the operation index. Attested as the callee
    // `IRInst::IRInst(this, opcode)` at the head of every derived node ctor
    // (e.g. IRLoadTemp passes opcode 118). Declared here as the delegation
    // target for the derived-node ctors; its body is its own TU.
    explicit IRInst(s32 aiOpcode);

    // @ 0x82C2A9C0 -- factory: dispatch to the per-opcode Make function.
    static IRInst* Make(int aiOpcode);

    // @ 0x82C2AC70 -- zero/default-initialise a freshly allocated node.
    IRInst* Init();

    // @ 0x82C28630 -- number of output operands.
    s32 OperationOutputs();

    // @ 0x827DDDA8 -- number of input operands.
    s32 OperationInputs();

    // @ 0x82C28750 -- read modifier byte (auByte) of operand slot (auSlot).
    u8 GetModifier(u32 auSlot, u32 auByte);

    // @ 0x82C2ADD0 -- bind operand slot (aiSlot) to virtual register (apReg),
    // caching its type and index.
    IRInst* SetOperandWithVReg(int aiSlot, VReg* apReg);

    // @ 0x82C2AC10 -- resolve the indexing mode (0/1/2) for operand slot.
    int GetIndexingMode(int aiSlot);

    // @ 0x82C28648 -- polymorphic destructor (the scalar-deleting-destructor
    // thunk is compiler-generated and dropped per project policy).
    virtual ~IRInst();

    // ---- declared-only (see XGraphicsIRInst.cpp for the blocked reasons) ----

    // @ 0x82C28638 -- instruction-type tag string.
    const char* InstType();

    // @ 0x82C2AF48 -- map a register type to its export type.
    static int ExportType(int aiType, void* apContext);

    // @ 0x82C2AE48 -- unlink the node and drop it from its def list.
    void* Kill();

    // @ 0x82C2A9F0 -- assert-heavy operand/owner consistency check.
    bool Validate();
};

} // namespace XGRAPHICS
