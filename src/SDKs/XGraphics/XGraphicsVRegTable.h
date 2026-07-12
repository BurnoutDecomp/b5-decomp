#pragma once

// ===========================================================================
// XGRAPHICS::VRegTable -- the shader-microcode compiler's virtual-register table
// in BURNOUT_X360_ARTIST.XEX. The CFG owns one (CFG +0x0AC) and routes every
// register lookup/creation through it. Each register lookup is keyed by a
// (index, type) pair: the table keeps ONE reusable "prototype" VRegInfo node
// (mpScratch), stamps that node's index/type for the query, and hashes it into
// an InternalHashTable of already-created vregs. A miss is materialised by
// VRegInfo::Make and inserted.
//
// This header is the OWNING home for the four VRegTable methods:
//     XGRAPHICS::VRegTable::Find           @ 0x82C284A8
//     XGRAPHICS::VRegTable::Create         @ 0x82C284E0
//     XGRAPHICS::VRegTable::RemoveConstant @ 0x82C28530  (BLOCKED -- see .cpp)
//     XGRAPHICS::VRegTable::FindOrCreate   @ 0x82C285A8
//
// There is NO reference source and NO DWARF for this TU, so the SHAPE below is
// reconstructed PURELY from the X360 asm of these methods. The layout is grounded
// by the field loads/stores in the four bodies (there is no ctor asm in this TU,
// so no ctor is written -- the members are declared for by-NAME access only).
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// ATTESTED members (X360 byte offsets; every field is 4-byte-wide on X360; the PC
// x64 recon accesses every field by NAME -- semantic parity, not byte-matching, so
// the widened pointers do not preserve these literal displacements):
//   +0x00 mpContext       -- graph context, forwarded to VRegInfo::Make
//   +0x04 mpMainTable     -- general vreg hash table (Find/Create/FindOrCreate)
//   +0x08 mapConstTables[4]-- per-component-count constant tables (RemoveConstant)
//   +0x18 mpScratch       -- reusable lookup-prototype VRegInfo node
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

class CFG;                // owning compiler context (homed in XGraphicsCFG.h)
class InternalHashTable;  // open-hashing vreg table (homed in XGraphicsInternalHashTable.h)
struct VRegInfo;          // virtual-register value node (homed in XGraphicsVReg.h)
struct IRInst;            // IR instruction node (homed in XGraphicsIRInst.h)

class VRegTable
{
public:
    // @ 0x82C284A8 caller (CFG::BuildUsesAndDefs) -- look up the vreg described by
    // register `aiReg` of the operand-descriptor word array `apRegDescs`: stamp the
    // prototype node with the descriptor's index (word aiReg+14) and type (word
    // aiReg+20) and hash it into the main table. Returns 0 when absent.
    VRegInfo* Find(const u32* apRegDescs, s32 aiReg);

    // @ 0x82C284E0 -- materialise a fresh vreg of register type `aiType` / index
    // `aiIndex` via the VRegInfo factory, insert it into the main table, and return
    // it. Does NOT consult the table first (FindOrCreate does that).
    VRegInfo* Create(s32 aiType, s32 aiIndex);

    // @ 0x82C285A8 caller (CFG::SetUpParamGen) -- find the vreg of register type
    // `aiType` / index `aiIndex`, creating it (Create) if absent, and return it.
    VRegInfo* FindOrCreate(s32 aiType, s32 aiIndex);

    // @ 0x82C28530 caller (XGRAPHICS::IRLoadConst::Kill) -- release the const node
    // `apConst` from the compiler's constant tables. The node's +0x3A0 component
    // array (IRInst::maUnk3A0) holds 1..4 non-null component words; that count picks
    // the per-component-count table mapConstTables[count-1], from which the node is
    // removed (keyed by the node pointer). Now homable: IRInst provides the +0x3A0
    // array by name and InternalHashTable::Remove is reconstructed.
    void RemoveConstant(IRInst* apConst);

    CFG*               mpContext;          // +0x00 graph context -> VRegInfo::Make
    InternalHashTable* mpMainTable;        // +0x04 general vreg lookup table
    InternalHashTable* mapConstTables[4];  // +0x08..+0x14 per-count constant tables
    VRegInfo*          mpScratch;          // +0x18 reusable lookup-prototype node
};

} // namespace XGRAPHICS
