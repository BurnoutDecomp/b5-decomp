#include "SDKs/XGraphics/XGraphicsIRExport.h"

// ===========================================================================
// XGRAPHICS::IRExport -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRExport.h for
// the shape notes. Each store below is store-for-store with the X360 asm.
//
// Two of the four methods are BLOCKED (ctor + Assemble); their block reasons are
// documented below in place of a body, following the fidelity policy (a partial
// body that drops a real asm side effect is not acceptable, and modelling the
// un-homed collaborators would require a raw-offset hack or fabricating a foreign
// type from a handful of grounded fields -- both forbidden).
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C2B300 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the base IRInst vtable to &off_821AEB78, then, on the deleting flag,
// frees the arena-prefixed block via ArenaFreePrefixed -- Arena::Free(*(this-1)));
// per project policy deleting-destructor thunks are dropped, not written. The
// destructor proper performs no member teardown in the asm, so the class
// destructor is trivial (same idiom as IRMov / IRLoadInterp).
IRExport::~IRExport()
{
}

// @ 0x82C2B2F0 -- returns the instruction-type tag string "export".
const char* IRExport::InstType()
{
    return "export";
}

// @ 0x82C2B280 -- IRExport::IRExport is NOT reconstructed here (BLOCKED).
//
// The asm is:
//   IRInst::IRInst(this, 48, context);   // 1-arg base consumes opcode 48; the
//                                         // context in r5 is ignored by the base
//   this->muUnkE4     |= 0x12u;           // lwz 0xE4; ori 0x12; stw 0xE4
//   *this               = &off_821B2270;  // implicit IRExport vtable stamp
//   this->miNumOutputs  = 1;              // stw 1, 0x10(this)
//   CFG* cfg = *(context + 0xAB0);        // lwz r3, 0xAB0(context)
//   cfg->AddToRootSet((u32)this);         // register the export node as a root
//   this->miNumInputs   = 1;              // stw 1, 0x14(this)
//   this->miOpcode      = 48;             // stw 0x30, 0x18(this)
//   this->miNumOutputs  = 0;              // stw 0, 0x10(this)
//
// The flag/vtable/count stores and the IRInst(48) delegation are all groundable,
// but the node's DISTINCTIVE side effect -- the CFG root-set registration -- is
// not reconstructable faithfully:
//   * It reaches the CFG (whose AddToRootSet IS homed, XGraphicsCFG.h) through a
//     raw +0xAB0 dereference of the ctor's `context` argument -- the un-homed
//     foreign "function context" type at IRInst::mpContext. That type has no
//     name, DWARF, reference or attested layout beyond this single +0xAB0 CFG*
//     field; it is the same collaborator that keeps IRLoadConst::Kill and
//     CFG::BuildUsesAndDefs blocked. Modelling it means either a forbidden
//     raw-offset pointer hack or fabricating a foreign type from one grounded
//     field -- both on the fidelity "do not fabricate" list.
// Dropping the AddToRootSet call would drop a real asm side effect (the export
// node would never enter the compiler's live-value root set), so a partial body
// is not acceptable either. The whole ctor is left blocked; it is documented in
// the header for the record only.

// @ 0x82C27EA0 -- IRExport::Assemble is NOT reconstructed here (BLOCKED).
//
// Assemble(this = export node, emitter, aluInst) encodes the export into a
// microcode ALU instruction. It:
//   * indexes an un-homed foreign microcode-assembler/emitter context by raw
//     offset -- *(emitter + 4) is the base of the 12-byte ALU-instruction array
//     and *(emitter + 52) a parallel 8-byte node-table -- to record this node
//     against its instruction slot ( v5 = &nodeTable[(aluInst - base)/12] ;
//     *v5 ? v5[1] = this : *v5 = this ). That emitter type is not homed; its two
//     touched fields are its only grounding.
//   * calls XGRAPHICS::IRAlu::AssembleSrcRegConst and XGRAPHICS::IRAlu::AssembleDest
//     -- both un-homed AND un-reconstructed (the IRAlu class has no home in
//     b5-decomp/src). Under the compile gate their declarations do not exist.
// The rest of the body (the per-channel operand bit-packing into the ALU
// instruction word, and the two SSMDebugPrint assertions -- which would map to
// CGS_ASSERT as in XGraphicsIRInst.cpp; the ALU instruction being serialised GPU
// microcode, raw byte access there is the documented data-blob exception) is
// groundable, but faithful reconstruction still requires fabricating the foreign
// emitter type + the IRAlu class from a handful of grounded fields -- forbidden
// per the fidelity policy. The whole method is left blocked; it is documented in
// the header for the record only.

} // namespace XGRAPHICS
