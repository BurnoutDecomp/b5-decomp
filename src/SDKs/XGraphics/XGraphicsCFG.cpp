#include "SDKs/XGraphics/XGraphicsCFG.h"

#include "SDKs/XGraphics/XGraphicsInternalVector.h" // InternalVector::Grow, muCount
#include "SDKs/XGraphics/XGraphicsVReg.h"           // VRegInfo::miUsage
#include "SDKs/XGraphics/XGraphicsVRegTable.h"      // VRegTable::FindOrCreate

// ===========================================================================
// XGRAPHICS::CFG -- reconstructed from BURNOUT_X360_ARTIST.XEX. See XGraphicsCFG.h
// for the attested layout. Each store / branch below is store-for-store with the
// X360 asm; every member is accessed by NAME (semantic parity, not byte-matching).
//
// CFG::BuildUsesAndDefs (0x82C26738) is intentionally NOT in this TU -- see the
// blocked-reason note in XGraphicsCFG.h.
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x82C266B8 -- append a value to the root set. The root set is a plain
// InternalVector of u32 slots; Grow(count) extends it by one and returns the new
// slot, into which the value is written. The written value is a live-value handle
// (a truncated pointer on X360, matching InternalVector's u32-slot nature).
u32* CFG::AddToRootSet(u32 auValue)
{
    u32* lpSlot = mpRootSet->Grow(mpRootSet->muCount);
    *lpSlot = auValue;
    return lpSlot;
}

// @ 0x82C266F0 -- set up the parameter-generation vreg. Find-or-create the vreg
// for register type 18 (index 0), flag param-gen as set up, cache the vreg's
// usage, and return the vreg.
VReg* CFG::SetUpParamGen()
{
    VReg* lpReg = mpVRegTable->FindOrCreate(18, 0);
    mbParamGenSetUp = true;              // +0x818 = 1
    miParamGenUsage = lpReg->miUsage;    // +0x81C = *(vreg + 0x10)
    return lpReg;
}

// @ 0x82C26608 -- true when this graph is compiling a vertex shader (byte load).
bool CFG::IsVertexShader()
{
    return mbIsVertexShader != 0;
}

// @ 0x82C28600 -- reserve a physical register by clearing its bit in the
// allocation bitset. The bitset has two leading header words, then one u32 per 32
// registers, so register `auReg` lives in word (auReg >> 5) + 2, bit auReg & 31.
CFG* CFG::ReservePhysicalRegister(u32 auReg)
{
    mpPhysicalRegs[(auReg >> 5) + 2] &= ~(1u << (auReg & 0x1F));
    return this;
}

} // namespace XGRAPHICS
