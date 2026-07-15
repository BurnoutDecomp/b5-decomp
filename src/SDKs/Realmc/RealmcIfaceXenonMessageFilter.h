#pragma once

// ===========================================================================
// RealmcIface::XenonMessageFilter -- a Realmc save/load interface message filter
// that watches for a memory-card autosave message and, when autosave is active,
// posts a matching RealmcCore::Response back onto its held message slot. It
// derives the committed Realmc filter base RealmcCore::MessageFilter (shared
// abstract base RealmcCore::IRealmcMessageFilter; the base's own final vtable is
// off_821BA310, this class installs its own final vtable off_82148728) and adds a
// single 1-byte state flag at +0x10.
//
// It is the structural sibling of RealmcIface::GameCallbackProcessor
// (RealmcIfaceGameCallbackProcessor.{h,cpp}) -- another Realmc message processor
// that posts a Response and resets its held message -- and is mirrored on it.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no Feb-2007 leak source, no DWARF).
// `Realmc` is a vendor library boundary, so its identifiers (RealmcIface,
// XenonMessageFilter, ProcessMessage, Reset) are preserved verbatim per the
// naming convention. Per-function X360 addresses:
//
//     RealmcIface::XenonMessageFilter::XenonMessageFilter          @ 0x82B54BE0
//     RealmcIface::XenonMessageFilter::ProcessMessage              @ 0x82B54C38
//     RealmcIface::XenonMessageFilter::Reset                       @ 0x82B54C28
//     RealmcIface::XenonMessageFilter::`vector deleting destructor'@ 0x82B54D20
//        (compiler-generated from the virtual dtor + the class operator delete)
//
// LAYOUT (from the ctor stores @0x82B54BE0 + the deleting-dtor free size = 20):
//   +0x00  vtable pointer  (base IRealmcMessageFilter vtable off_82148660 then
//                           the final XenonMessageFilter vtable off_82148728 --
//                           MSVC's base-then-final derived-ctor sequence; the
//                           dtor installs off_82148728 then lets the
//                           MessageFilter base dtor restore off_82148660)
//   +0x04  mpHandler       (inherited RealmcCore::MessageFilter::mpHandler -- a
//                           RealmcCore::MemcardState*; ProcessMessage reads it as
//                           `lwz r3, 4(this)` and calls GetAutosaveState on it)
//   +0x08  maMessage       (inherited RealmcCore::MessageFilter::maMessage -- the
//                           embedded RealmcCore::MessagePtr ProcessMessage rebinds
//                           to the freshly built Response)
//   +0x10  mbState         (this class's own 1-byte flag -- `stb r10(0), 0x10(this)`;
//                           cleared to 0 by the ctor and by Reset, and NOT read by
//                           any of this TU's four functions. FLAG: role/name
//                           unrecovered -- modelled as an opaque 1-byte state flag.)
//
// sizeof == 0x14 (20) on X360 -- the `vector deleting destructor' @0x82B54D20
// frees 20 bytes (the 16-byte MessageFilter base + the 1-byte flag, padded). (On
// the LLP64 PC target the pointer members widen, so byte size/offsets are NOT
// matched, per the project's semantic-parity rule -- documented above, not asserted.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"           // MessageFilter base, Response, ResponsePtr, MessagePtr, AllocateMem, FreeMemSize
#include "SDKs/Realmc/RealmcMemcardState.h"   // RealmcCore::MemcardState (mpHandler's real type; GetAutosaveState)

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// RealmcIface::XenonMessageFilter -- derives the committed RealmcCore::MessageFilter
// (which itself derives the abstract RealmcCore::IRealmcMessageFilter). ProcessMessage
// is this class's own message-processing entry; declaring it virtual mirrors the
// sibling RealmcIface::GameCallbackProcessor and this filter framework's polymorphic
// use. FLAG: no vtable dump exists, so ProcessMessage's virtual-ness / slot is
// inferred from the sibling pattern (the class DOES install its own final vtable
// off_82148728, and the overridden dtor alone already requires one).
// ---------------------------------------------------------------------------
class XenonMessageFilter : public RealmcCore::MessageFilter
{
public:
    // @ 0x82B54BE0 -- run the RealmcCore::MessageFilter base ctor (forwarding the
    //                 handler), install the final vtable (MSVC ctor prologue), then
    //                 clear the +0x10 state flag: `stb 0, 0x10(this)`.
    //                 mpHandler is the RealmcCore::MemcardState the filter queries.
    explicit XenonMessageFilter(RealmcCore::MemcardState* pMemcardState);

    // @ 0x82B54C38 -- if the handler's autosave state is active, read the incoming
    //                 message's result word (+0x1C == 1 or 2), allocate + construct a
    //                 RealmcCore::Response carrying that value, wrap it in a stack
    //                 ResponsePtr, and rebind the held MessagePtr (maMessage) to it;
    //                 the stack ResponsePtr is then torn down. Returns the autosave
    //                 state. FLAG: the incoming message type is un-homed, so its
    //                 result field is read at the attested byte offset +0x1C.
    virtual bool ProcessMessage(void* pMessage);

    // @ 0x82B54C28 -- clear the +0x10 state flag: `stb 0, 0x10(this)`.
    void Reset();

    // @ 0x82B54D20 (backing `vector deleting destructor') -- the MessageFilter base
    //                 dtor does the real teardown (Release the embedded MessagePtr,
    //                 restore vtables); this override's own body is empty. Frees 20
    //                 bytes (sizeof) through operator delete when the delete flag is set.
    virtual ~XenonMessageFilter();

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    bool mbState;   // +0x10 (FLAG: role/name unrecovered; cleared by ctor + Reset, never read here)
};

} // namespace RealmcIface
