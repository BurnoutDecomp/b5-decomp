#pragma once

// ===========================================================================
// Csis::FunctionHandle -- a handle into the "Csis" class/interface registry, a
// vendor library boundary in BURNOUT_X360_ARTIST.XEX (the Csis::*, CsisDef::*
// family used by the AEMS audio voice/factory code). Sibling of
// Csis::ClassHandle (see CsisClassHandle.h); this header is the canonical
// OWNING home for:
//
//     Csis::FunctionHandle::SetFast  @ 0x82B0FB10   (bind to an InterfaceId)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm. `Csis` is a vendor library
// boundary, so its identifiers (Csis, FunctionHandle, SetFast, SetHandle,
// InterfaceId, Result, CsisDef::FunctionDesc) are preserved verbatim per the
// naming convention.
//
// LAYOUT (mirrors the sibling ClassHandle, confirmed by the size hints SetFast
// forwards to SetHandle):
//     +0x00  miPayload -- handle payload word (populated by the binding machinery)
//     +0x04  miIndex   -- registry slot index
//   The 20/10 size hints SetFast passes to SetHandle are the InterfaceId record
//   size (20) and the CsisDef::FunctionDesc record size (10) it indexes.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Csis::SetHandle<FunctionHandle, InterfaceId const, CsisDef::FunctionDesc>
//     -- the templated binding free function SetFast tail-calls. Its body lives
//     in another (vendor) Csis TU; only this one explicit specialisation is
//     reached from this TU. The three trailing integer arguments are
//     (kind=0, interfaceRecordSize=20, functionDescSize=10).
//   * Csis::InterfaceId / CsisDef::FunctionDesc -- vendor registry record types;
//     only forward declarations are needed (the reused decls live in
//     CsisClassHandle.h, which also declares the generic SetHandle template).
// ===========================================================================

#include "SDKs/Csis/CsisClassHandle.h" // Csis::Result, Csis::InterfaceId, CsisDef::FunctionDesc, Csis::SetHandle<>

namespace Csis
{

// ---------------------------------------------------------------------------
// Csis::FunctionHandle
// ---------------------------------------------------------------------------
class FunctionHandle
{
public:
    // @ 0x82B0FB10 -- bind this handle to an InterfaceId via SetHandle, tail-call
    // forwarding the binding-record sizes (kind=0, idSize=20, fnDescSize=10).
    Result SetFast(const InterfaceId* pId);

    // +0x00 -- handle payload word (populated by the binding machinery).
    s32 miPayload;
    // +0x04 -- registry slot index.
    s32 miIndex;
};

} // namespace Csis
