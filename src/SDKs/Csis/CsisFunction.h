#pragma once

// ===========================================================================
// Csis::Function -- a "Csis" registry function/notification handle, a vendor
// library boundary in BURNOUT_X360_ARTIST.XEX (the Csis::*, CsisDef::* family
// used by the AEMS audio voice/factory code). Sibling of Csis::ClassHandle /
// Csis::FunctionHandle. This header is the canonical OWNING home for:
//
//     Csis::Function::CallFast        @ 0x82B0FB40  (fan a call out to subscribers)
//     Csis::Function::SubscribeFast   @ 0x82B0FBA8  (register a subscriber node)
//     Csis::Function::UnsubscribeFast @ 0x82B0FC18  (deregister a subscriber node)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Csis` is a vendor
// library boundary, so its identifiers (Csis, Function, ClassHandle, ValidHandle,
// Result, CsisDef::FunctionDesc) are preserved verbatim per the naming
// convention.
//
// LAYOUT (from asm):
//   All three methods first call ValidHandle<ClassHandle, FunctionDesc>(this, 0),
//   then read `*this` (byte offset +0 = miPayload) as a pointer to the
//   descriptor, whose +0 word is the head of a doubly-linked subscriber list. So
//   Function shares the ClassHandle shape (miPayload@0, miIndex@4) and is
//   modelled as deriving from ClassHandle so ValidHandle<ClassHandle,...> accepts
//   `this` (an implicit Function*->ClassHandle* upcast, matching the mangled
//   PAVClassHandle call target).
//
// SUBSCRIBER NODE (Csis::Function::Subscriber) -- CallFast/SubscribeFast/
// UnsubscribeFast operate on nodes NOT owned by Function (the caller allocates
// them, e.g. SNDAEMSI_CreateModuleInstance / SNDAEMSI_updatedestroy). Attested
// node fields:
//     +0x00  mpNext     -- next subscriber (CallFast lwz 0; Subscribe *a2; list link)
//     +0x04  mpPrev     -- prev subscriber (Subscribe a2[1]; Unsubscribe a2[1])
//     +0x08  mpfnCallback -- void (*)(int param, void* context) (CallFast lwz 8)
//     +0x0C  mpContext  -- opaque context passed to the callback (CallFast lwz 0xC)
//   16 bytes on X360; the +8/+0xC pair is only touched by CallFast, +0/+4 only by
//   Subscribe/Unsubscribe. Field stride is not independently attested (no memcpy),
//   so the 16-byte size is the union of the attested offsets. Pointers widen on
//   the 64-bit host, so this is a logical model (no offsetof layout asserts).
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Csis::ValidHandle<ClassHandle, CsisDef::FunctionDesc> -- the templated
//     handle-validation free function every method calls with (this, 0). Its body
//     lives in another (vendor) Csis TU; only this one explicit specialisation is
//     reached here. Returns Csis::Result (>=0 valid, <0 error). Declared, not
//     defined, in this TU.
//   * CsisDef::FunctionDesc -- vendor registry record type; forward declaration
//     only (reused from CsisClassHandle.h). Its +0 word is the subscriber head.
// ===========================================================================

#include "SDKs/Csis/CsisClassHandle.h" // Csis::Result, Csis::ClassHandle, CsisDef::FunctionDesc

namespace Csis
{

// ---------------------------------------------------------------------------
// Csis::ValidHandle -- the vendor templated handle-validation free function.
// Defined in another Csis TU; this is the single explicit specialisation the
// Function methods use. Signature reconstructed from the demangled call target:
//   Result Csis::ValidHandle<ClassHandle, CsisDef::FunctionDesc>(
//       ClassHandle* pHandle, int flags);
// (FLAGGED vendor extern -- declared, not defined, in this TU.)
// ---------------------------------------------------------------------------
template <class THandle, class TDesc>
Result ValidHandle(THandle* pHandle, int flags);

// ---------------------------------------------------------------------------
// Csis::Function
// ---------------------------------------------------------------------------
class Function : public ClassHandle
{
public:
    // A single subscriber in the descriptor's doubly-linked notification list.
    // The Function class does not own these nodes; callers allocate/free them.
    struct Subscriber
    {
        Subscriber* mpNext;                       // +0x00  next in list
        Subscriber* mpPrev;                       // +0x04  prev in list
        void (*mpfnCallback)(int iParam, void* pContext); // +0x08  invoked by CallFast
        void*       mpContext;                    // +0x0C  opaque callback context
    };

    // @ 0x82B0FB40 -- validate the handle, then invoke every subscriber's
    // callback as fn(iParam, context). Returns the ValidHandle result, or -4 if
    // the handle is valid but the subscriber list is empty.
    Result CallFast(int iParam);

    // @ 0x82B0FBA8 -- validate the handle, then head-insert pNode into the
    // descriptor's doubly-linked subscriber list. Returns 0 on success.
    Result SubscribeFast(Subscriber* pNode);

    // @ 0x82B0FC18 -- validate the handle, then unlink pNode from the descriptor's
    // doubly-linked subscriber list. Returns 0 on success.
    Result UnsubscribeFast(Subscriber* pNode);
};

} // namespace Csis
