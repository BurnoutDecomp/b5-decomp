#pragma once

// ===========================================================================
// XGRAPHICS::VRegInfo -- a virtual-register value node in the XGRAPHICS shader-
// microcode intermediate representation (BURNOUT_X360_ARTIST.XEX). It is the
// polymorphic base of every value kind (FixedValue, TempValue, ExportValue,
// Interpolator, Physical, Resource, StandardIndex, AutoIndexVtx, HosCoord, ...)
// and is exactly the register descriptor an IR instruction's operand slots
// point at: `VReg` is an alias of this class, and IRInst reads operand->GetType/
// GetIndex/GetUsage (and ->miTypeInfoIndex) off it. Each node tracks the
// instructions that define/use it (two arena-backed InternalVectors) plus an SSA
// def stack, and is stamped with a graph-unique id from the owning CFG.
//
// This header is the canonical OWNING home for:
//     XGRAPHICS::VRegInfo::VRegInfo  @ 0x82C28FD8  (ctor)
//     XGRAPHICS::VRegInfo::~VRegInfo @ 0x82C294D8  (virtual dtor)
//     XGRAPHICS::VRegInfo::BumpDefs  @ 0x82C290E0
//     XGRAPHICS::VRegInfo::BumpUses  @ 0x82C29130
//
// There is NO reference source and NO DWARF for this TU. The layout is
// reconstructed from the ctor / Bump* / IRInst asm; the three operand accessor
// NAMES are recovered verbatim from the IRInst::Validate assertion strings
// (`GetType()`/`GetIndex()`/`GetUsage()`). `XGRAPHICS` is an X360 graphics-SDK
// boundary, so its identifiers are preserved verbatim per the naming convention.
//
// The `// +0xNN` are X360 (32-bit) reference offsets; the PC x64 recon accesses
// every field by NAME (semantic parity, not byte-matching), so the widened
// pointers do not preserve these literal displacements.
// ===========================================================================

#include "types.hpp"

namespace XGRAPHICS
{

class InternalVector; // use/def instruction vectors (homed in XGraphicsInternalVector.h)
class DefStack;       // SSA def stack               (homed in XGraphicsDefStack.h)
class CFG;            // owning compiler context     (homed in XGraphicsCFG.h)
struct IRInst;        // IR instruction              (homed in XGraphicsIRInst.h)

struct VRegInfo
{
    // vtable pointer occupies +0x00 (VRegInfo is polymorphic; see ~VRegInfo).
    u8              mbUnk04;          // +0x04  flag byte (Init 0)  FLAG unattested purpose
    u8              mbUnk05;          // +0x05  flag byte (Init 0)  FLAG unattested purpose
    s32             miId;             // +0x08  graph-unique virtual-register id
    s32             miIndex;          // +0x0C  GetIndex()  (ctor arg aiIndex)
    s32             miUsage;          // +0x10  GetUsage()  (Init -1)
    s32             miUsesCount;      // +0x14  # instructions recorded as uses
    s32             miDefsCount;      // +0x18  # instructions recorded as defs
    u8              mbUnk1C;          // +0x1C  flag byte (Init 0)  FLAG unattested purpose
    u8              mbUnk1D;          // +0x1D  flag byte (Init 0)  FLAG unattested purpose
    s32             miType;           // +0x20  GetType()   (ctor arg aiType)
    InternalVector* mpUses;           // +0x24  instructions that USE this vreg
    InternalVector* mpDefs;           // +0x28  instructions that DEFINE this vreg
    DefStack*       mpDefStack;       // +0x2C  SSA def stack (arena-prefixed alloc)
    u8              maUnk30[0x50 - 0x30]; // +0x30 .. +0x4F opaque (set by derived kinds)  FLAG
    s32             miTypeInfoIndex;  // +0x50  register-type-info index (IRInst::GetIndexingMode)

    // Operand accessors (non-virtual, const) -- names from IRInst::Validate asserts.
    s32 GetIndex() const { return miIndex; }
    s32 GetUsage() const { return miUsage; }
    s32 GetType()  const { return miType; }

    // @ 0x82C28FD8 -- construct a fresh vreg: cache index/type, empty use/def
    // vectors + def stack from the graph arena, and stamp the next graph id.
    VRegInfo(s32 aiIndex, s32 aiType, CFG* apContext);

    // @ 0x82C294D8 -- destroy the def stack and return it to its arena. The
    // deleting-destructor thunk (0x82C2A258) is compiler-generated and dropped
    // per project policy.
    virtual ~VRegInfo();

    // @ 0x82C290E0 -- record `apInst` as a defining instruction of this vreg.
    void BumpDefs(IRInst* apInst);

    // @ 0x82C29130 -- record `apInst` as a using instruction of this vreg,
    // deduped across its input operand slots (1 .. aiNumOperands-1).
    void BumpUses(s32 aiNumOperands, IRInst* apInst);

    // @ 0x82C28E58 -- static factory: allocate + construct a VRegInfo of the value
    // kind selected by register type `aiType` in graph context `apContext`, stamped
    // with `aiIndex`, and return it. Signature grounded from the VRegTable::Create
    // call site (Make(aiIndex, aiType, mpContext) -- mirrors this class's ctor arg
    // order). The body is an unchecked tail-dispatch through the register-type-info
    // table (gaVRegTypeInfo below, X360 rodata @ 0x821B5100, fully dumped -- see
    // scratchpad/waveM/VRegInfo.spec.md): mulli r10,r4,0xC selects the 12-byte
    // record for `aiType`, lwzx loads the factory pointer at record+0x08, bctr
    // tail-calls it with r3/r4/r5 (aiIndex, aiType, apContext) passed through.
    static VRegInfo* Make(s32 aiIndex, s32 aiType, CFG* apContext);
};

// The IR-instruction operand descriptor IS a VRegInfo (identical operand-facing
// offsets: GetType@+0x20 / GetIndex@+0x0C / GetUsage@+0x10 / miTypeInfoIndex@+0x50).
using VReg = VRegInfo;

// ---------------------------------------------------------------------------
// The register-type-info table (X360 rodata @ 0x821B5100, 48 records of 12
// bytes; RECOVERED by full IDA dump -- entries + string pool + factory targets,
// see scratchpad/waveM/VRegInfo.spec.md). The register-type index (`aiType`
// passed to VRegInfo::Make, and the miTypeInfoIndex each value node carries at
// X360 +0x50) IS the row index of this table. Exactly three ARTIST functions
// consume it, each through one field:
//   VRegInfo::Make @ 0x82C28E58            -> mpfnNewItem   (record +0x08)
//   IRInst::GetIndexingMode @ 0x82C2AC10   -> muIndexedKind (record +0x07)
//   IRAlu::AssembleSrcRegConst @ 0x82C26CA8-> muConstFile   (record +0x06)
// No consumer bound-checks the index (the dispatch is unchecked); the count 48
// is proven by (a) the all-zero record at row 48 breaking the name-pointer
// pattern, (b) the name-string pool directly below the table bottoming out at
// row 47's "MRSC" @ 0x821B5038, and (c) rows 0..47 all holding a named
// XGRAPHICS::*::NewItem factory (or the one measured NULL at row 4 "EI").
// ---------------------------------------------------------------------------

// Factory signature every table row dispatches through (r3/r4/r5 pass straight
// through Make's bctr, so each NewItem receives Make's own arguments).
typedef VRegInfo* (*VRegFactoryFn)(s32 aiIndex, s32 aiType, CFG* apContext);

// Number of rows in gaVRegTypeInfo (see count proof above).
const s32 KI_NUM_VREG_TYPES = 48;

struct VRegTypeInfo
{
    const char*   mpName;        // +0x00 register-file mnemonic ("R", "C", ... "MRSC")
    u8            muUnk04;       // +0x04 measured 0/1; no ARTIST consumer  FLAG purpose unattested
    u8            muUnk05;       // +0x05 measured 0/1; no ARTIST consumer  FLAG purpose unattested
    u8            muConstFile;   // +0x06 const-register-file class: 0 none, 1 C, 2 K, 3 CLI, 4 CIX/CIY/CIZ/CIW
    u8            muIndexedKind; // +0x07 0 plain, 1 addr-indexed (CI*) -> mode 2, 2 loop-indexed (CLI/VLI/OLI) -> mode 1
    VRegFactoryFn mpfnNewItem;   // +0x08 value-kind factory VRegInfo::Make tail-calls
};

// Defined in XGraphicsVReg.cpp beside VRegInfo::Make (its only dispatch
// consumer); IRInst::GetIndexingMode and IRAlu::AssembleSrcRegConst read the
// two byte fields when those TUs are reconstructed.
extern const VRegTypeInfo gaVRegTypeInfo[KI_NUM_VREG_TYPES];

} // namespace XGRAPHICS
