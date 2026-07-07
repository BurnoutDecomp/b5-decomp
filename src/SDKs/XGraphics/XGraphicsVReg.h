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

    // @ 0x82C28E58 -- static factory dispatching to the per-kind maker.
    // BLOCKED: needs the unrecovered VRegTable factory dispatch table
    // (rodata off_821B5108, 3-word entries of registered factory pointers);
    // the entry layout and contents are not in the dossier, so reconstructing
    // it would fabricate rodata. Left declared-only.
    // static VRegInfo* Make(CFG* apContext, s32 aiKind);
};

// The IR-instruction operand descriptor IS a VRegInfo (identical operand-facing
// offsets: GetType@+0x20 / GetIndex@+0x0C / GetUsage@+0x10 / miTypeInfoIndex@+0x50).
using VReg = VRegInfo;

} // namespace XGRAPHICS
