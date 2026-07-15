#pragma once

// ===========================================================================
// XGRAPHICS::CFG -- the shader-microcode compiler's control-flow-graph / build
// context in BURNOUT_X360_ARTIST.XEX. It owns the compiler XGRAPHICS::Arena and
// hands out sequential virtual-register ids as XGRAPHICS::VRegInfo nodes are
// created (see VRegInfo::VRegInfo, CFG::BuildUsesAndDefs). It is the `a4`
// context threaded through every value-kind constructor. It also owns the
// compiler's VRegTable, the current Block, a "root set" of live values, the
// physical-register allocation bitset, and the parameter-generation state.
//
// This header is the OWNING home for the CFG class TU. It is a PARTIAL / SEED
// reconstruction: only the members the attested methods touch are named, and
// the interior is opaque padding sized to place them at their X360 byte
// displacements. `XGRAPHICS` is an X360 graphics-SDK boundary, so its
// identifiers are preserved verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference displacements taken from the
// method asm; the PC x64 recon accesses every field by NAME (semantic parity,
// not byte-matching), so the widened pointers do not preserve these literal
// offsets. The pad buffers document the X360 layout only.
//
// ATTESTED members (X360 byte displacements):
//   +0x0AC mpVRegTable      -- the compiler's virtual-register table
//                              (SetUpParamGen: lwz r3,0xAC(this) -> FindOrCreate).
//   +0x584 miNextTempIndex  -- monotonic temp-value index counter (read + post-
//                              incremented in TempValue::TempValue).
//   +0x58C miNextLoopIndexedConst -- monotonic loop-indexed-const-set index
//                              counter (read + post-incremented in
//                              LoopIndexedConstSet::LoopIndexedConstSet).
//   +0x590 miNextLoopIndex  -- monotonic loop-indexed-set index counter (read +
//                              post-incremented in LoopIndexedInputSet::LoopIndexedInputSet;
//                              shared by the loop-indexed set value kinds).
//   +0x594 miNextLoopIndexedOutput -- monotonic loop-indexed-OUTPUT-set index counter
//                              (read + post-incremented in
//                              LoopIndexedOutputSet::LoopIndexedOutputSet; own counter,
//                              distinct from the const/input ones at +0x58C/+0x590).
//   +0x598 miNextVRegId     -- monotonic vreg-id counter (read + post-incremented,
//                              the pre-increment value becomes VRegInfo::miId).
//   +0x5AC mpArena          -- the arena every graph object (VRegInfo, its use/def
//                              vectors, its def stack) is carved from.
//   +0x814 mpRootSet        -- InternalVector of live "root" values; AddToRootSet
//                              appends to it (lwz r3,0x814(this); Grow; *slot=val).
//   +0x818 mbParamGenSetUp  -- param-generation set-up flag (SetUpParamGen -> 1).
//   +0x81C miParamGenUsage  -- cached usage of the param-gen vreg
//                              (SetUpParamGen: *(this+0x81C) = vreg->miUsage).
//   +0x858 mpPhysicalRegs   -- physical-register allocation bitset (2 header words
//                              then one u32 per 32 registers; ReservePhysicalRegister
//                              clears the bit for a reserved register).
//   +0x860 mbIsVertexShader -- true when compiling a vertex shader (IsVertexShader).
// FLAG: every region between the named fields is opaque padding (no DWARF / no
// reference); only these offsets are grounded in the asm.
// ===========================================================================

#include "types.hpp"
#include "SDKs/XGraphics/XGraphicsArena.h" // XGRAPHICS::Arena

namespace XGRAPHICS
{

class InternalVector; // root-set container (homed in XGraphicsInternalVector.h)
class VRegTable;      // virtual-register table (homed in XGraphicsVRegTable.h)
struct VRegInfo;      // virtual-register value node (homed in XGraphicsVReg.h)
using VReg = VRegInfo;
struct IRInst;        // IR instruction (homed in XGraphicsIRInst.h)

class CFG
{
public:
    // @ 0x82C266B8 -- append `auValue` to the root-value set, growing the backing
    // InternalVector, and return a pointer to the freshly written slot.
    u32* AddToRootSet(u32 auValue);

    // @ 0x82C266F0 -- find-or-create the parameter-generation vreg (register type
    // 18, index 0) in the VRegTable, flag param-gen as set up, cache the vreg's
    // usage, and return the vreg.
    VReg* SetUpParamGen();

    // @ 0x82C26608 -- true when this graph is compiling a vertex shader.
    bool IsVertexShader();

    // @ 0x82C28600 -- clear the allocation bit for physical register `auReg` in the
    // register-allocation bitset (marking it reserved / no longer free). Returns
    // this.
    CFG* ReservePhysicalRegister(u32 auReg);

    // @ 0x82C26738 -- CFG::BuildUsesAndDefs is BLOCKED, not reconstructed here.
    // UPDATE (W42): VRegTable::Find / Create are now homed (XGraphicsVRegTable),
    // and the per-opcode attribute table dword_82FA57F8 is reachable through the
    // gaIROpcodeInfo extern (see XGraphicsIRInst.cpp) -- so neither of those is
    // the blocker. Two IRREDUCIBLE blockers remain, each in the fidelity "do not
    // fabricate" list:
    //   (1) The VRegInfo that Find/Create return has its operand resolved through
    //       a VIRTUAL call on that VRegInfo's own vtable -- slot +0x20 on the
    //       Find-hit path (lwz r11,0x20(r11); bctrl; args: reg, *(inst+0x80), cfg)
    //       and slot +0x1C on the Create path (lwz r11,0x1C(r11); bctrl; args: reg,
    //       *(inst+0x80), cfg, funcCtx). VRegInfo models only its virtual dtor;
    //       the six sibling slots (bytes 0x04..0x18) are not grounded anywhere, so
    //       reaching slots 0x1C/0x20 by NAME would require inventing phantom vtable
    //       slots (or a raw *(vtable+n) offset hack) -- both forbidden.
    //   (2) It threads a foreign "function" context at CFG +0x0C (lwz r11,0xC(this))
    //       and decrements a temp-id counter at that object's +0x5E4 (lwz/stw
    //       0x5E4(r11)). That type is not reconstructed; poking +0x5E4 would be a
    //       raw-offset hack. Left blocked per the fidelity policy.
    // int BuildUsesAndDefs(IRInst* apInst);

    // ---- layout (X360 displacements; opaque padding between named fields) -----
    u8              maPad000[0x0AC];         // +0x000 .. +0x0AB opaque
    VRegTable*      mpVRegTable;             // +0x0AC  virtual-register table
    u8              maPad0B0[0x584 - 0x0B0]; // +0x0B0 .. +0x583 opaque
    s32             miNextTempIndex;         // +0x584  next temp-value index (post-inc)
    u8              maPad588[0x58C - 0x588]; // +0x588 .. +0x58B opaque
    s32             miNextLoopIndexedConst;  // +0x58C  next loop-indexed-const-set index (post-inc)
    s32             miNextLoopIndex;         // +0x590  next loop-indexed-set index (post-inc)
    s32             miNextLoopIndexedOutput; // +0x594  next loop-indexed-output-set index (post-inc)
    s32             miNextVRegId;            // +0x598  next virtual-register id (post-inc)
    u8              maPad59C[0x5AC - 0x59C]; // +0x59C .. +0x5AB opaque
    Arena*          mpArena;                 // +0x5AC  arena all graph objects allocate from
    u8              maPad5B0[0x814 - 0x5B0]; // +0x5B0 .. +0x813 opaque
    InternalVector* mpRootSet;               // +0x814  live "root" value set
    u8              mbParamGenSetUp;         // +0x818  param-generation set-up flag
    u8              maPad819[0x81C - 0x819]; // +0x819 .. +0x81B opaque
    s32             miParamGenUsage;         // +0x81C  cached param-gen vreg usage
    u8              maPad820[0x858 - 0x820]; // +0x820 .. +0x857 opaque
    u32*            mpPhysicalRegs;          // +0x858  physical-register alloc bitset
    u8              maPad85C[0x860 - 0x85C]; // +0x85C .. +0x85F opaque
    u8              mbIsVertexShader;        // +0x860  compiling a vertex shader
};

} // namespace XGRAPHICS
