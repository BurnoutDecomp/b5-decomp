#pragma once

// ===========================================================================
// XGRAPHICS::VReg -- a virtual-register operand descriptor referenced by the
// XGRAPHICS shader-microcode intermediate-representation instruction nodes
// (BURNOUT_X360_ARTIST.XEX). An IR instruction's operand slots each point at a
// VReg (see IRInst::SetOperandWithVReg / IRInst::Validate / IRInst::GetIndexingMode).
//
// There is NO reference source and NO DWARF for this TU. Only the four fields the
// IRInst methods actually read are attested by the X360 asm; everything else is
// opaque padding sized to place them. The three accessor names are recovered
// verbatim from the IRInst::Validate assertion strings, which spell out
// `aRegs[i]->GetType()`, `aRegs[i]->GetIndex()` and `aRegs[i]->GetUsage()`.
// `XGRAPHICS` is an X360 graphics-SDK boundary, so its identifiers are preserved
// verbatim per the naming convention.
//
// ATTESTED offsets (PowerPC displacements from IRInst::SetOperandWithVReg,
// IRInst::Validate and IRInst::GetIndexingMode):
//   +0x0C  miIndex          -- GetIndex()  (lwz r,0xC(reg) ; SetOperandWithVReg)
//   +0x10  miUsage          -- GetUsage()  (lwz r,0x10(reg); Validate)
//   +0x20  miType           -- GetType()   (lwz r,0x20(reg); Validate/SetOperand)
//   +0x50  miTypeInfoIndex  -- index into the register-type-info table used by
//                              IRInst::GetIndexingMode (lwz r,0x50(reg)).
// FLAG: interior gaps are opaque padding (no DWARF); only these four offsets are
// grounded in the asm.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

struct VReg
{
    u8  maPad00[0x0C];        // +0x00 .. +0x0B opaque
    s32 miIndex;              // +0x0C  GetIndex()
    s32 miUsage;              // +0x10  GetUsage()
    u8  maPad14[0x20 - 0x14]; // +0x14 .. +0x1F opaque
    s32 miType;               // +0x20  GetType()
    u8  maPad24[0x50 - 0x24]; // +0x24 .. +0x4F opaque
    s32 miTypeInfoIndex;      // +0x50  register-type-info index (GetIndexingMode)

    s32 GetIndex() const { return miIndex; }
    s32 GetUsage() const { return miUsage; }
    s32 GetType()  const { return miType; }
};

} // namespace XGRAPHICS
