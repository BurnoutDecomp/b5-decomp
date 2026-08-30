#pragma once

// ===========================================================================
// SDKs/Csis/CsisGlobalVariable.h
//
// Csis::GlobalVariable -- the free-function-style manager over Csis global
// variables (a vendor library boundary in BURNOUT_X360_ARTIST.XEX; the Csis::*,
// CsisDef::* family used by the AEMS audio code). Sibling of the committed
// Csis::GlobalVariableHandle (CsisGlobalVariableHandle.h). This header is the
// canonical OWNING home for:
//
//     Csis::GlobalVariable::SetFast         @ 0x82B0FEB0
//     Csis::GlobalVariable::SubscribeFast   @ 0x82B0FF30
//     Csis::GlobalVariable::UnsubscribeFast @ 0x82B0FFB8
//
// Callers: SetFast <- UpdateSetGlobalVariable; SubscribeFast <-
// SNDAEMSI_CreateModuleInstance; UnsubscribeFast <- SNDAEMSI_updatedestroy.
//
// COHERENCE NOTE: Csis::GlobalVariableHandle is ALREADY defined in the committed
// CsisGlobalVariableHandle.h as { s32 miPayload; s32 miIndex; }. This header
// REUSES that class (it does NOT redefine it) and only COMPLETES the
// forward-declared CsisDef::GlobalVariableDesc. All three bodies read the
// handle's first 4-byte word (asm: lwz r11,0(pHandle)) as the resolved
// GlobalVariableDesc* that ValidHandle stamps there.
//
// GlobalVariableDesc 32-bit layout the X360 asm proves:
//   +0x00  phead   -- head of the GlobalVariableSubscriber list (CListDStack)
//   +0x04  curVal  -- current scalar value (Parameter) (addi r11,4 -> &curVal;
//                     lwz 4(r11) -> curVal; lwz 0(r11) -> head)
//
// `Csis`/`CsisDef` are a vendor boundary, so their identifiers are preserved
// verbatim per the naming convention.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Csis::ValidHandle<GlobalVariableHandle, CsisDef::GlobalVariableDesc> --
//     templated handle-validation free function all three bodies call. Body lives
//     in another vendor Csis TU; only this specialisation is reached here.
//     Signature from the demangled target + the `li r4,0` second arg:
//     Result ValidHandle(THandle* pHandle, int flags). Returns a signed Result;
//     the bodies branch on `< 0`.
// ===========================================================================

#include "SDKs/Csis/CsisGlobalVariableHandle.h" // Csis::GlobalVariableHandle, Csis::Result, CsisDef::GlobalVariableDesc (fwd)

namespace CsisDef
{
// ---------------------------------------------------------------------------
// The scalar global-variable value cell (int/float overlaid).
// ---------------------------------------------------------------------------
union Parameter
{
    s32   intVal;
    float floatVal;
    u64   nativeWord;
};

} // namespace CsisDef

namespace Csis
{

// ---------------------------------------------------------------------------
// GlobalVariableSubscriber -- one node on a GlobalVariableDesc's intrusive
// client list. NOT a plain link pair: the asm reads a callback + user-data past
// the two link words, so the record is {pNext, pPrev, pfnCallback, pUserData}
// = 16 bytes (offsets 0/4/8/0xC, proven by SubscribeFast's stw 0,4/stw 0/
// lwz 8/lwz 0xC and SetFast's callback dispatch). The callback receives the
// desc's current value and the node's user data.
// ---------------------------------------------------------------------------
struct GlobalVariableSubscriber
{
    typedef void (*Callback)(CsisDef::Parameter* pValue, void* pUserData);

    GlobalVariableSubscriber* pNext;        // +0x00
    GlobalVariableSubscriber* pPrev;        // +0x04
    Callback                  pfnCallback;  // +0x08
    void*                     pUserData;    // +0x0C
};

} // namespace Csis

namespace CsisDef
{
// ---------------------------------------------------------------------------
// CsisDef::GlobalVariableDesc -- COMPLETES the forward declaration in
// CsisGlobalVariableHandle.h. Only the fields the Fast bodies touch are modelled.
// ---------------------------------------------------------------------------
struct GlobalVariableDesc
{
    Csis::GlobalVariableSubscriber* phead; // +0x00
    Parameter                       curVal; // +0x08
    const char*                     pName;  // +0x10
    s32                             token;  // +0x18
    u32                             padding;
};

} // namespace CsisDef

namespace Csis
{

// ---------------------------------------------------------------------------
// Csis::ValidHandle -- vendor templated handle-validation free function (FLAGGED
// vendor extern; declared, not defined, in this TU). Returns a signed Result;
// callers branch on `< 0`.
// ---------------------------------------------------------------------------
template <class THandle, class TDesc>
Result ValidHandle(THandle* pHandle, int flags);

// Read the resolved GlobalVariableDesc* out of a validated handle's first word
// (asm: lwz r11,0(pHandle)). Modelled as a reinterpret of word 0 rather than a
// named member so it does not fight the committed handle's { miPayload; miIndex }
// layout.
inline CsisDef::GlobalVariableDesc* ResolveDesc(GlobalVariableHandle* pHandle)
{
    return *reinterpret_cast<CsisDef::GlobalVariableDesc**>(pHandle);
}

// ---------------------------------------------------------------------------
// Csis::GlobalVariable -- stateless manager; all methods are static and operate
// on a caller-supplied GlobalVariableHandle.
// ---------------------------------------------------------------------------
class GlobalVariable
{
public:
    // @ 0x82B0FEB0 -- set the variable's value; if it changed, broadcast the new
    // value to every subscribed client. No-op (returns 0) when unchanged.
    static Result SetFast(GlobalVariableHandle* pHandle, const CsisDef::Parameter* pValue);

    // @ 0x82B0FF30 -- push a subscriber onto the front of the client list and
    // immediately fire its callback with the current value.
    static Result SubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode);

    // @ 0x82B0FFB8 -- unlink a subscriber from the client list.
    static Result UnsubscribeFast(GlobalVariableHandle* pHandle, GlobalVariableSubscriber* pNode);
};

} // namespace Csis
