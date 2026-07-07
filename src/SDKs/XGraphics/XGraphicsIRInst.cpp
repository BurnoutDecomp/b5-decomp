#include "SDKs/XGraphics/XGraphicsIRInst.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ===========================================================================
// XGRAPHICS::IRInst -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRInst.h for the
// member layout notes. The X360 SSMDebugPrint("Assertion failed: %s (%s:%u)",..)
// idiom maps to the project CGS_ASSERT macro (which supplies __FILE__/__LINE__,
// so the original Xenon file/line is dropped).
//
// EXTERNAL rodata tables referenced verbatim from the attested asm (their
// CONTENTS live in un-recovered X360 rodata; only the strides/attested fields
// this TU indexes are named -- nothing is fabricated, the tables are external
// data referenced through extern symbols, exactly as the asm loads them):
//   * gaIROpcodeInfo @ 0x82FA57F8 -- per-opcode descriptor, 0x34-byte stride.
//     +0x00 muFlags (Init tests bit 2); +0x30 muMakeKind (Make dispatch index).
//   * gaIRMakeTable  @ 0x821B1EA0 -- per-kind factory dispatch, 8-byte stride,
//     factory fn pointer at entry +0.
//   * gaIRRegTypeInfo @ 0x821B5100 -- per-register-type info, 12-byte stride,
//     the indexing-mode byte at entry +7 (byte_821B5107).
// ===========================================================================

namespace XGRAPHICS
{

// ---- external opcode-info / dispatch / register-type-info tables ------------

struct IROpcodeInfo
{
    u32 muFlags;             // +0x00  (Init tests bit 2)
    u8  maPad04[0x30 - 4];   // +0x04 .. +0x2F opaque external rodata
    u32 muMakeKind;          // +0x30  factory dispatch index (dword_82FA5828)
};
static_assert(sizeof(IROpcodeInfo) == 0x34, "gaIROpcodeInfo stride must be 0x34");

typedef IRInst* (*IRMakeFn)(int aiOpcode);
struct IRMakeTableEntry
{
    IRMakeFn mpfnMake;       // +0x00  per-kind factory (dword_821B1EA0[2*kind])
    u32      muReserved;     // +0x04  (X360 second table dword; unused here)
};

struct IRRegTypeInfo
{
    u8 maPad00[7];           // +0x00 .. +0x06 opaque external rodata
    u8 muIndexingMode;       // +0x07  byte_821B5107
    u8 maPad08[12 - 8];      // +0x08 .. +0x0B opaque
};
static_assert(sizeof(IRRegTypeInfo) == 12, "gaIRRegTypeInfo stride must be 12");

extern "C" IROpcodeInfo    gaIROpcodeInfo[];  // @ 0x82FA57F8 (dword_82FA57F8)
extern "C" IRMakeTableEntry gaIRMakeTable[];  // @ 0x821B1EA0 (dword_821B1EA0)
extern "C" IRRegTypeInfo   gaIRRegTypeInfo[]; // @ 0x821B5100 (off_821B5100)

// Modifier byte patterns Init writes (the X360 rodata dwords, decomposed to
// bytes so GetModifier's per-byte reads are endian-safe on the PC target):
//   dword_821B1F48 = 0x00010203  -> {0,1,2,3}  (per-operand default)
//   dword_821B1F50 = 0x00000000  -> {0,0,0,0}  (operand 0 default)
//   dword_821B1F58 = 0x00000000  -> {0,0,0,0}
//   dword_821B1F5C = 0x03030303  -> {3,3,3,3}
static const u8 KMODIFIER_DEFAULT[4] = { 0, 1, 2, 3 };
static const u8 KMODIFIER_ZERO[4]    = { 0, 0, 0, 0 };
static const u8 KMODIFIER_THREE[4]   = { 3, 3, 3, 3 };

static inline void SetModifier(u8 apDst[4], const u8 apSrc[4])
{
    apDst[0] = apSrc[0];
    apDst[1] = apSrc[1];
    apDst[2] = apSrc[2];
    apDst[3] = apSrc[3];
}

// @ 0x82C2A9C0 -- factory. Reads the opcode's make-kind out of the opcode-info
// table and tail-calls that kind's factory function, passing the opcode along
// (r3 = opcode is never overwritten before the bctr).
IRInst* IRInst::Make(int aiOpcode)
{
    u32 luKind = gaIROpcodeInfo[aiOpcode].muMakeKind;
    return gaIRMakeTable[luKind].mpfnMake(aiOpcode);
}

// @ 0x82C2AC70 -- default-initialise a freshly allocated node. Every store below
// matches an asm store (the opcode-flag branch gates the per-operand modifier).
IRInst* IRInst::Init()
{
    // ---- fixed (operand-0 / scalar) stores --------------------------------
    muId         = 0;      // stwbrx of 0 (byte-reversed; value 0)
    miNumOutputs = 0;
    miNumInputs  = 0;
    muUnk34      = 0;
    maOperand[0]     = -1;
    maOperandType[0] = 48;
    SetModifier(maModifier[0], KMODIFIER_ZERO); // dword_821B1F50
    maRegs[0]    = nullptr;
    maUnk98[0]   = 0;
    maUnk9E[0]   = 0;
    maUnkC8[0]   = 0;
    muUnkA4      = 0;
    SetModifier(maDefaultModifier, KMODIFIER_DEFAULT); // +0x3B0 = dword_821B1F48

    // ---- per-operand slots 1..5 -------------------------------------------
    const bool lbSplitModifier = ((gaIROpcodeInfo[miOpcode].muFlags >> 2) & 1) != 0;
    for (int liSlot = 1; liSlot < 6; ++liSlot)
    {
        maOperand[liSlot]     = -1;
        maOperandType[liSlot] = 48;
        SetModifier(maModifier[liSlot], KMODIFIER_DEFAULT); // dword_821B1F48
        if (lbSplitModifier)
        {
            if (liSlot == 1)
                SetModifier(maModifier[1], KMODIFIER_THREE); // dword_821B1F5C
            else
                SetModifier(maModifier[liSlot], KMODIFIER_ZERO); // dword_821B1F58
        }
        maUnk98[liSlot] = 0;
        maUnk9E[liSlot] = 0;
        maRegs[liSlot]  = nullptr;
        maUnkC8[liSlot] = 0;
    }

    // ---- per-channel source registers + swizzles --------------------------
    for (int liChannel = 0; liChannel < 32; ++liChannel)
    {
        maSourceRegs[liChannel] = nullptr;
        maSwizzle[liChannel][0] = 3;
        maSwizzle[liChannel][1] = 3;
        maSwizzle[liChannel][2] = 3;
        maSwizzle[liChannel][3] = 3;
    }

    // ---- trailing zeroed banks (unattested purpose) -----------------------
    for (int li = 0; li < 4; ++li)
    {
        maUnk380[2 * li]     = 0;
        maUnk380[2 * li + 1] = 0;
        maUnk3A0[li]         = 0;
    }

    return this;
}

// @ 0x82C28630
s32 IRInst::OperationOutputs()
{
    return miNumOutputs;
}

// @ 0x827DDDA8
s32 IRInst::OperationInputs()
{
    return miNumInputs;
}

// @ 0x82C28750 -- byte auByte of operand slot auSlot's modifier.
u8 IRInst::GetModifier(u32 auSlot, u32 auByte)
{
    return maModifier[auSlot][auByte];
}

// @ 0x82C2ADD0 -- bind operand slot to a virtual register, caching its type and
// index. (The extra Hex-Rays varargs in the pseudocode are the SSMDebugPrint
// spill slots for the assert; the real signature is (slot, reg).)
IRInst* IRInst::SetOperandWithVReg(int aiSlot, VReg* apReg)
{
    CGS_ASSERT(aiSlot < 6, "arg < (5 + 1)");
    maOperandType[aiSlot] = apReg->GetType();  // *(reg+0x20)
    s32 liIndex           = apReg->GetIndex(); // *(reg+0x0C)
    maRegs[aiSlot]        = apReg;
    maOperand[aiSlot]     = liIndex;
    return this;
}

// @ 0x82C2AC10 -- indexing mode for operand slot: look the operand's register
// type up in the register-type-info table (falling back to the cached operand
// type when the slot has no source register) and fold the mode byte to 0/1/2.
int IRInst::GetIndexingMode(int aiSlot)
{
    VReg* lpReg = maSourceRegs[aiSlot];
    s32 liType  = lpReg ? lpReg->miTypeInfoIndex // *(reg+0x50)
                        : maOperandType[aiSlot];
    u8 luMode   = gaIRRegTypeInfo[liType].muIndexingMode; // byte_821B5107[12*type]
    if (luMode == 1)
        return 2;
    return luMode == 2; // 1 when mode==2, else 0
}

// @ 0x82C28648 -- the scalar deleting destructor is a compiler-generated thunk
// (it restamps the vtable then routes the storage back to XGRAPHICS::Arena via
// Arena::Free(*(this-1))); per project policy deleting-destructor thunks are
// dropped, not written. The destructor proper performs no member teardown in the
// asm, so the class destructor is trivial.
IRInst::~IRInst()
{
}

} // namespace XGRAPHICS
