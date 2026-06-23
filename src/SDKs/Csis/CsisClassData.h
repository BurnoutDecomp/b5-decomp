#ifndef CSIS_CLASSDATA_H
#define CSIS_CLASSDATA_H

#include "types.hpp"

// ===========================================================================
// SDKs/Csis/CsisClassData.h
//
// Csis::ClassData -- the per-class registry record in the "Csis" class/interface
// system (a vendor library boundary in BURNOUT_X360_ARTIST.XEX). This header is the
// canonical OWNING home for:
//
//     Csis::ClassData::SendParameters @ 0x82B0F160
//
// LAYOUT: grounded by the Csis DWARF
// (references/DecFIGS/dwarfdump/SDKs/Packages/csis/2.12.00/source/cmn/csisi.h):
//     struct ClassData {
//         SystemDesc::ClassDesc* mpClassDesc;   // +0x0
//         int                    mRefCount;     // +0x4
//         CListDStack            mUpdateClients; // +0x8   (update-notification list)
//         CListDStack            mDestroyClients;// (destroy-notification list)
//     };
//
// The X360 asm of SendParameters reads the update-client list HEAD directly at
// ClassData +0x8 (lwz r31,8(r3)) and walks node->next at node +0x0 (lwz r31,0(r31)),
// so in the X360 build CListDStack's head pointer sits at the struct's own offset 0
// (the DWARF's PS3 slinklist places a padTo64Bit word first; the X360 variant does
// not -- the asm is authoritative). Each notification-client node carries:
//     +0x0 mpNext     (Node*)              -- forward link
//     +0x8 mpfnNotify (NotifyFn)           -- the callback
//     +0xC mpContext  (void*)              -- callback context
// (offsets proven by SendParameters: callee = node[2] @ +0x8, arg = node[3] @ +0xC.)
//
// `Csis` is a vendor library boundary, so its identifiers (Csis, ClassData,
// SendParameters, CListDStack, Parameter, SystemDesc::ClassDesc) are preserved
// verbatim per the naming convention.
// ===========================================================================

namespace SystemDesc
{
// Forward declaration only -- ClassData stores a pointer to it; its full layout is
// out of this TU's scope (defined in the vendor SystemDesc headers).
struct ClassDesc;
}

namespace Csis
{

// The parameter payload SendParameters broadcasts to the registered clients. Its
// concrete layout lives in the vendor Csis headers; SendParameters only forwards a
// pointer to it, so a forward declaration is sufficient.
struct Parameter;

// A single update-/destroy-notification client node. The X360 walks it as an
// intrusive singly-linked list (next at +0x0) carrying a callback + context.
// Reconstructed from SendParameters' node accesses.
struct ClassDataClient
{
    // The notification callback: invoked as mpfnNotify(pParameter, mpContext). The
    // return value is threaded through (SendParameters returns the last call's
    // result; the X360 leaves it in r3).
    typedef int (*NotifyFn)(Parameter* pParameter, void* pContext);

    ClassDataClient* mpNext;     // +0x0  forward link (0 == end)
    s32              miReserved; // +0x4  (unread by SendParameters; padding/owner field)
    NotifyFn         mpfnNotify; // +0x8  callback
    void*            mpContext;  // +0xC  callback context
};

// Intrusive client-list head. X360 variant: the head pointer is the struct's first
// (and only load-bearing) word, so SendParameters reaches it at ClassData +0x8.
struct CListDStack
{
    ClassDataClient* mpHead; // +0x0
};

// ---------------------------------------------------------------------------
// Csis::ClassData
// ---------------------------------------------------------------------------
struct ClassData
{
    // @ 0x82B0F160 -- broadcast pParameter to every registered update client,
    // calling each client's notify callback with (pParameter, client->context).
    // Returns the value left by the last callback (or the input pointer's value if
    // the list is empty), matching the X360 r3-passthrough.
    int SendParameters(Parameter* pParameter);

    SystemDesc::ClassDesc* mpClassDesc;     // +0x0
    s32                    mRefCount;       // +0x4
    CListDStack            mUpdateClients;  // +0x8
    CListDStack            mDestroyClients; // +0xC
};

} // namespace Csis

#endif // CSIS_CLASSDATA_H
