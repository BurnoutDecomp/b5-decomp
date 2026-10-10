#pragma once

// ===========================================================================
// RealmcIface::XenonMessageFilter -- the Xenon message filter the memory-card
// interface installs on its MemcardState. It derives RealmcCore::MessageFilter
// and answers two message types itself, so their tasks never wait on the game
// thread:
//   * a MessageTrc (a TCR message box) while autosave is active: answered with
//     the box's option count when that is 1 or 2;
//   * a MessageClear during particular running-task combinations or while
//     the matching message is hidden: answered with 1.
// Reset clears the filter's one byte of per-task state before a task body runs.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (no Feb-2007 leak source, no DWARF).
// `Realmc` is a vendor library boundary, so its identifiers (RealmcIface,
// XenonMessageFilter, ProcessMessage, Reset) are preserved verbatim.
//
// VTABLE (dumped, 24 slots): the deleting destructor; the MessageFilter empty
// handlers except +0x44 (MessageClear) and +0x48 (MessageTrc, the image labels
// it XenonMessageFilter::ProcessMessage); MessageFilter::FilterMessage at +0x58;
// XenonMessageFilter::Reset at +0x5C.
//
// LAYOUT (from the ctor stores + the deleting-dtor free size = 20):
//   +0x00  vtable pointer
//   +0x04  mpHandler   (inherited -- the RealmcCore::MemcardState the filter
//                       queries for its autosave state, task stack and hidden
//                       messages)
//   +0x08  maResponse  (inherited -- the embedded ResponsePtr the handlers
//                       rebind to the answer they build)
//   +0x10  mbState     (this class's own byte: cleared by the ctor and by Reset,
//                       tested by the MessageClear handler. FLAG: role/name
//                       unrecovered -- no writer of a non-zero value is homed.)
//
// The console sizeof is 0x14 (20): the deleting destructor frees 20 bytes (the
// 16-byte MessageFilter base + the byte, padded). (On the LLP64 PC target the
// pointer members widen, so byte size/offsets are NOT matched.)
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"           // MessageFilter base, Response, ResponsePtr, AllocateMem, FreeMemSize
#include "SDKs/Realmc/RealmcMemcardState.h"   // RealmcCore::MemcardState (mpHandler's real type)
#include "SDKs/Realmc/RealmcTrc.h"            // RealmcCore::MessageTrc / Trc (the option count)
#include "SDKs/Realmc/RealmcCoreMessageClear.h"  // RealmcCore::MessageClear

namespace RealmcIface
{

class XenonMessageFilter : public RealmcCore::MessageFilter
{
public:
    // Run the MessageFilter base ctor (forwarding the handler), install the final
    // vtable, then clear the +0x10 state byte.
    explicit XenonMessageFilter(RealmcCore::MemcardState* pMemcardState);

    // Processor slot +0x44 -- answer a MessageClear with Response(1) when
    //   (main task 2 and current task 16), or (main task 2, current task 2 and
    //   mbState clear), or (main task 8 and hidden-message bit 0x200), or
    //   (main task 16 and hidden-message bit 0x2000). Each check that passes
    //   rebinds maResponse to a fresh Response(1).
    void ProcessMessage(RealmcCore::MessageClear* pMessage) override;

    // Processor slot +0x48 -- while the handler's autosave state is active,
    //   answer a MessageTrc whose box has 1 or 2 options with Response(1) or
    //   Response(2); otherwise leave maResponse alone.
    void ProcessMessage(RealmcCore::MessageTrc* pMessage) override;

    // The base's other handlers stay visible for calls through this type.
    using RealmcCore::MessageFilter::ProcessMessage;

    // MessageFilter slot +0x5C -- clear the +0x10 state byte.
    void Reset() override;

    // The deleting destructor: the MessageFilter base dtor does the teardown
    // (Release the embedded ResponsePtr, restore vtables); then 20 bytes are
    // freed through FreeMemSize (the class operator delete, host sizeof).
    ~XenonMessageFilter() override;

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    bool mbState;   // +0x10 (FLAG: role/name unrecovered; see LAYOUT)
};

} // namespace RealmcIface
