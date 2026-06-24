#pragma once

// ===========================================================================
// Csis::GlobalVariableHandle -- a handle into the "Csis" class/interface
// registry, a vendor library boundary in BURNOUT_X360_ARTIST.XEX (the Csis::*,
// CsisDef::* family used by the AEMS audio voice/factory code). Sibling of
// Csis::ClassHandle / Csis::FunctionHandle; this header is the canonical OWNING
// home for:
//
//     Csis::GlobalVariableHandle::SetFast  @ 0x82B0FB30   (bind to an InterfaceId)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 asm. `Csis` is a vendor library
// boundary, so its identifiers (Csis, GlobalVariableHandle, SetFast, SetHandle,
// InterfaceId, Result, CsisDef::GlobalVariableDesc) are preserved verbatim per
// the naming convention.
//
// LAYOUT (mirrors the sibling ClassHandle, confirmed by the size hints SetFast
// forwards to SetHandle):
//     +0x00  miPayload -- handle payload word (populated by the binding machinery)
//     +0x04  miIndex   -- registry slot index
//   The 28/14 size hints SetFast passes to SetHandle are the InterfaceId record
//   size (28) and the CsisDef::GlobalVariableDesc record size (14) it indexes.
//
// PLATFORM/VENDOR EXTERNS (flagged):
//   * Csis::SetHandle<GlobalVariableHandle, InterfaceId const,
//     CsisDef::GlobalVariableDesc> -- the templated binding free function
//     SetFast tail-calls. Its body lives in another (vendor) Csis TU; only this
//     one explicit specialisation is reached from this TU. The three trailing
//     integer arguments are (kind=0, interfaceRecordSize=28, descSize=14).
//   * Csis::InterfaceId / CsisDef::GlobalVariableDesc -- vendor registry record
//     types; only forward declarations are needed (Csis::Result, InterfaceId and
//     the generic SetHandle<> template are reused from CsisClassHandle.h).
// ===========================================================================

#include "SDKs/Csis/CsisClassHandle.h" // Csis::Result, Csis::InterfaceId, Csis::SetHandle<>

namespace CsisDef
{
// Vendor registry record type SetHandle indexes for global-variable handles
// (forward declaration only; full layout lives in the vendor CsisDef headers).
struct GlobalVariableDesc;
} // namespace CsisDef

namespace Csis
{

// ---------------------------------------------------------------------------
// Csis::GlobalVariableHandle
// ---------------------------------------------------------------------------
class GlobalVariableHandle
{
public:
    // @ 0x82B0FB30 -- bind this handle to an InterfaceId via SetHandle, tail-call
    // forwarding the binding-record sizes (kind=0, idSize=28, descSize=14).
    Result SetFast(const InterfaceId* pId);

    // +0x00 -- handle payload word (populated by the binding machinery).
    s32 miPayload;
    // +0x04 -- registry slot index.
    s32 miIndex;
};

} // namespace Csis
