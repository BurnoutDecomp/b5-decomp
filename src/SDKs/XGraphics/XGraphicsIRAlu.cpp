#include "SDKs/XGraphics/XGraphicsIRAlu.h"

#include "SDKs/XGraphics/XGraphicsCFG.h"                 // CFG (root-set owner)
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

// ===========================================================================
// XGRAPHICS::IRAlu -- bodies reconstructed from BURNOUT_X360_ARTIST.XEX. The
// PowerPC asm is authoritative for every store, immediate and branch. No
// reference source and no DWARF exist for this TU; see XGraphicsIRAlu.h for the
// shape notes. The X360 SSMDebugPrint("Assertion failed: %s (%s:%u)",..) idiom
// maps to the project CGS_ASSERT macro (which supplies __FILE__/__LINE__, so the
// original Xenon file/line is dropped -- same convention as XGraphicsIRInst.cpp).
//
// Three of the seven methods are BLOCKED (the microcode-assembler encoder trio:
// Assemble / AssembleDest / AssembleSrcRegConst); their block reasons are
// documented below in place of a body, following the fidelity policy (a partial
// body that drops a real asm side effect is not acceptable, and modelling the
// un-homed collaborators would require a raw-offset hack, an un-attested vtable
// slot, or fabricating a foreign type from a handful of grounded fields -- all
// forbidden).
// ===========================================================================

namespace XGRAPHICS
{

// @ 0x821AF1F8 -- external static ALU instruction-type descriptor. Unlike the
// other IR node kinds, whose InstType returns a plain tag STRING, IRAlu::InstType
// returns the ADDRESS of this static descriptor object, whose contents live in
// un-recovered X360 rodata (IDA could not type it -- unk_821AF1F8). Only its
// address is used (returned unchanged); nothing about its layout is read or
// fabricated, so it is referenced through an extern symbol exactly as the asm
// loads it (lis/addi of unk_821AF1F8), matching the extern-rodata precedent in
// XGraphicsIRInst.cpp.
extern "C" u8 gXGraphicsIRAluInstType[]; // @ 0x821AF1F8 (unk_821AF1F8)

// @ 0x82C28698 -- return the address of the static ALU instruction-type
// descriptor (lis/addi unk_821AF1F8; blr).
void* IRAlu::InstType()
{
    return gXGraphicsIRAluInstType;
}

// @ 0x82C286C0 -- set operand slot `aiSlot`, component `aiComponent`, of the
// per-operand modifier bank to `aiValue`. The X360 folds the +0x80 base into the
// index math (this + (aiSlot + 32) * 4 + aiComponent), i.e. maModifier[aiSlot]
// [aiComponent]; the PC recon accesses it by name. A value in [4,5] is invalid
// and asserts (the asm branches straight to the assert on that range, skipping
// the store).
IRAlu* IRAlu::SetMask(s32 aiSlot, s32 aiComponent, s32 aiValue)
{
    if (aiValue < 4 || aiValue > 5)
    {
        maModifier[aiSlot][aiComponent] = (u8)aiValue;
    }
    else
    {
        CGS_ASSERT(false, "false"); // IR/IRinst.hpp:1686
    }
    return this;
}

// @ 0x82C28708 -- set the swizzle byte for operand slot `aiSlot`, component
// `aiComponent`, to `aiValue`. Compiled store-for-store identically to SetMask
// (same modifier-bank target, same [4,5] assert); the only asm difference is the
// assert's source line (IR/IRinst.hpp:1699 vs :1686).
IRAlu* IRAlu::SetSwizzle(s32 aiSlot, s32 aiComponent, s32 aiValue)
{
    if (aiValue < 4 || aiValue > 5)
    {
        maModifier[aiSlot][aiComponent] = (u8)aiValue;
    }
    else
    {
        CGS_ASSERT(false, "false"); // IR/IRinst.hpp:1699
    }
    return this;
}

// @ 0x82C2B000 -- retarget this ALU node as an export node. Caches operand slot 0
// (index + type), zeroes the output count, ORs the export node-flags bits into
// the node-flags word, and tail-calls CFG::AddToRootSet to register the node as a
// live root value.
//   v4                    = this->muUnkE4;       // lwz 0xE4
//   this->maOperand[0]    = aiOperand;           // stw 0x38
//   this->maOperandType[0]= aiOperandType;       // stw 0x50
//   this->miNumOutputs    = 0;                    // stw 0x10
//   this->muUnkE4         = v4 | 0x42;            // ori 0x42; stw 0xE4
//   return apContext->AddToRootSet((u32)this);   // b CFG::AddToRootSet
u32* IRAlu::MarkInstructionAsExport(CFG* apContext, s32 aiOperandType, s32 aiOperand)
{
    maOperand[0]     = aiOperand;
    maOperandType[0] = aiOperandType;
    miNumOutputs     = 0;
    muUnkE4         |= 0x42u;
    // The X360 root set stores node handles as 32-bit values; AddToRootSet's
    // committed signature takes a u32, so the node pointer is passed truncated
    // (documented wart of the u32 root-set seed, not a new invention).
    return apContext->AddToRootSet((u32)(uintptr_t)this);
}

// @ 0x82C27AC0 -- IRAlu::Assemble is NOT reconstructed here (BLOCKED).
//
// Assemble(this = alu node, emitter, aluWord) encodes the node into a microcode
// ALU instruction word. It:
//   * records this node against its instruction slot through an un-homed foreign
//     microcode-emitter context: v5 = &nodeTable[(aluWord - *(emitter+4))/12],
//     then *v5 ? v5[1] = this : *v5 = this. *(emitter+4) is the base of the
//     12-byte ALU-word array and *(emitter+52) the parallel 8-byte node-table --
//     the emitter type is not homed; those two fields are its only grounding. This
//     is the identical collaborator that keeps IRExport::Assemble blocked.
//   * dispatches the operand count through an UN-ATTESTED vtable slot:
//     (*(*this + 4))(this) (the second vtable slot). IRInst models only its
//     virtual dtor; reaching slot +4 by name would require inventing a phantom
//     vtable slot (or a raw *(vtable + n) offset hack) -- both forbidden.
//   * indexes external opcode -> ALU-op-code rodata (dword_821B1298[miOpcode]),
//     un-recovered content that would have to be modelled as a fabricated table.
// The per-channel operand bit-packing into the (serialised GPU microcode) ALU
// word and the CGS_ASSERT-mapped assertions are groundable, but the emitter type,
// the un-attested vtable slot, and the external op-code table cannot be
// reconstructed faithfully. The whole method is left blocked per the fidelity
// policy; it is documented in the header for the record only.

// @ 0x82C27218 -- IRAlu::AssembleDest is NOT reconstructed here (BLOCKED).
//
// AssembleDest(this, aluWord) encodes this node's destination operand into the
// ALU word. It:
//   * reaches the CFG through a raw +0xAB0 hop across the un-homed foreign
//     "function context" at IRInst::mpContext: CFG::Number(*(mpContext + 0xAB0),
//     this) and *(*(mpContext + 0xAB0) + 0x854) -- the +0xAB0 collaborator is the
//     same un-homed type that keeps IRExport::IRExport / IRLoadConst::Kill /
//     CFG::BuildUsesAndDefs blocked, and +0x854 is an un-attested CFG field.
//   * calls XGRAPHICS::CFG::Number, which has no home in b5-decomp/src (external/
//     unknown -- not in the ledger as a reconstructable TU).
//   * calls IRInst::ExportType and IRInst::GetIndexingMode (both declared) plus a
//     write-mask/indexing bit-pack over the microcode word.
// The bit-pack + assertions are groundable, but the raw +0xAB0 hop across the
// un-homed foreign context, the un-attested CFG +0x854 field, and the un-homed
// CFG::Number cannot be reconstructed faithfully. The whole method is left blocked
// per the fidelity policy; it is documented in the header for the record only.

// @ 0x82C26CA8 -- IRAlu::AssembleSrcRegConst is NOT reconstructed here (BLOCKED).
//
// AssembleSrcRegConst(this, aluWord, aiSlot, aiChannel) encodes a source-register
// or constant operand into the ALU word. It:
//   * reaches the CFG through the SAME raw +0xAB0 hop across the un-homed foreign
//     function context: CFG::Number(*(mpContext + 0xAB0), maSourceRegs[aiSlot])
//     and CFG::IsVertexShader(*(mpContext + 0xAB0)).
//   * calls XGRAPHICS::CFG::Number (external/unknown, un-homed).
//   * tail-calls sub_82C26BE0 -- an un-named, un-homed function with no
//     reconstructable home (external/unknown), so even the return path is
//     un-groundable.
// The operand/const bit-packing and the CGS_ASSERT-mapped assertions are
// groundable, but the +0xAB0 hop, the un-homed CFG::Number, and the un-homed
// sub_82C26BE0 tail-call cannot be reconstructed faithfully. The whole method is
// left blocked per the fidelity policy; it is documented in the header for the
// record only.

} // namespace XGRAPHICS
