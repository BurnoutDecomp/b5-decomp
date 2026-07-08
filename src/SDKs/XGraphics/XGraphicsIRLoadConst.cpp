#include "SDKs/XGraphics/XGraphicsIRLoadConst.h"

// ===========================================================================
// XGRAPHICS::IRLoadConst -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRLoadConst.h
// for the shape notes. Each store below is store-for-store with the X360 asm.
// ===========================================================================

namespace XGRAPHICS
{

// The IRLoadConst opcode the base IRInst ctor is invoked with (r4 = 0x75).
static const s32 KI_OPCODE_LOAD_CONST = 117;

// @ 0x82C2B3B0 -- construct a load-const node over `apConst`.
//   IRInst::IRInst(this, 117);          // base ctor + implicit vtable stamp
//   *this               = &off_821B22D8; // implicit IRLoadConst vtable stamp
//   this->miNumOutputs   = 1;           // stw 0 then stw 1, 0x10(r3)
//   this->maOperandType[0] = apConst->GetType();  // lwz 0x20(r31); stw 0x50(r3)
//   this->maRegs[0]      = apConst;     // stw r31, 0x1C(r3)
//   this->maOperand[0]   = apConst->GetIndex();   // lwz 0xC(r31); stw 0x38(r3)
//   apConst->mbUnk1C     = 1;           // stb 1, 0x1C(r31)
//   this->miNumInputs    = 0;           // stw 0, 0x14(r3)
IRLoadConst::IRLoadConst(VReg* apConst)
    : IRInst(KI_OPCODE_LOAD_CONST)
{
    miNumOutputs     = 1;
    maOperandType[0] = apConst->GetType();
    maRegs[0]        = apConst;
    maOperand[0]     = apConst->GetIndex();
    apConst->mbUnk1C = 1;
    miNumInputs      = 0;
}

// @ 0x82C2B350 -- returns the instruction-type tag string "load const".
const char* IRLoadConst::InstType()
{
    return "load const";
}

// @ 0x82C2AEE8 -- IRLoadConst::Kill is NOT reconstructed here (BLOCKED).
//
// The asm is:
//   if ( this->maOperandType[0] == 11 )                      // real constant
//       (*(this->mpContext + 0xAB0))->mpVRegTable->RemoveConstant(this);
//   this->muUnkE4 &= ~1u;                                    // clear node flag
//   return this->DListNode::Remove();
//
// The clear-flag + Remove tail is fully groundable, but the constant-removal
// branch cannot be reconstructed faithfully:
//   (a) it reaches the CFG (and thence the VRegTable at CFG +0xAC) through a raw
//       +0xAB0 dereference of IRInst::mpContext -- an un-homed foreign "function
//       context" type with no name, DWARF, reference, or any other attested
//       member. Modelling it means either a forbidden raw-offset pointer hack or
//       fabricating a foreign type from a single grounded field. This is the same
//       collaborator that keeps CFG::BuildUsesAndDefs blocked.
//   (b) it calls XGRAPHICS::VRegTable::RemoveConstant @ 0x82C28530, which is
//       itself un-reconstructed (blocked).
// Dropping the branch would drop a real asm side-effect (a foreign call that
// mutates the constant table), so a partial body is not acceptable either. The
// whole method is left blocked per the fidelity policy; it is declared in the
// header for the record only.

} // namespace XGRAPHICS
