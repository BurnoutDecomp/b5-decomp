#include "SDKs/XGraphics/XGraphicsIRLoadTemp.h"

// ===========================================================================
// XGRAPHICS::IRLoadTemp -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX.
// The PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRLoadTemp.h
// for the shape notes.
// ===========================================================================

namespace XGRAPHICS
{

// The IRLoadTemp opcode the base IRInst ctor is invoked with (r4 = 0x76).
static const s32 KI_OPCODE_LOAD_TEMP = 118;

// @ 0x82C2B478 -- construct a load-temp node over `apTemp`.
//   IRInst::IRInst(this, 118);          // base ctor + implicit vtable stamp
//   this->miNumOutputs   = 1;           // stw 1, 0x10(r3)
//   this->maOperandType[0] = apTemp->GetType();  // lwz 0x20(r31); stw 0x50(r3)
//   this->maRegs[0]      = apTemp;      // stw r31, 0x1C(r3)
//   this->maOperand[0]   = apTemp->GetIndex();    // lwz 0xC(r31); stw 0x38(r3)
//   apTemp->mbUnk1C      = 1;           // stb 1, 0x1C(r31)
//   this->miNumInputs    = 0;           // stw 0, 0x14(r3)
IRLoadTemp::IRLoadTemp(VReg* apTemp)
    : IRInst(KI_OPCODE_LOAD_TEMP)
{
    miNumOutputs     = 1;
    maOperandType[0] = apTemp->GetType();
    maRegs[0]        = apTemp;
    maOperand[0]     = apTemp->GetIndex();
    apTemp->mbUnk1C  = 1;
    miNumInputs      = 0;
}

// @ 0x82C2B428 -- the scalar deleting destructor is a compiler-generated thunk
// (restamps the vtable to &off_821AEB78, then Arena::Free(*(this-1)) when the
// deleting flag is set); per project policy deleting-destructor thunks are
// dropped, not written. The destructor proper performs no member teardown in
// the asm, so the class destructor is trivial.
IRLoadTemp::~IRLoadTemp()
{
}

// @ 0x82C2B418 -- returns the instruction-type tag string "load temp".
const char* IRLoadTemp::InstType()
{
    return "load temp";
}

} // namespace XGRAPHICS
