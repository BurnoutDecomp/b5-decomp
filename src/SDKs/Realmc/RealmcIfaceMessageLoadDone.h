#pragma once

// ===========================================================================
// RealmcIface::MessageLoadDone -- a Realmc save/load interface message that
// notifies the message processor that a memory-card load has finished. Part of the
// RealmcIface message family in BURNOUT_X360_ARTIST.XEX; structurally it is a
// RealmcCore::MessageString subclass (it carries an id + an owned string, plus
// three of its own trailing words) whose Apply hands the notification to the
// shared RealmcCore::IMessageProcessor.
//
// This header is the canonical OWNING home for the reconstructed members:
//
//     RealmcIface::MessageLoadDone::Apply                      @ 0x82B570B8
//     RealmcIface::MessageLoadDone::`vector deleting destructor' @ 0x82B570D8
//        (compiler-generated from the virtual dtor + the class operator delete)
//     RealmcIface::MessageLoadDone::MessageLoadDone (RealmcIfaceMessageLoadDone.cpp)
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the X360 pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers (RealmcIface, MessageLoadDone, Apply) are
// preserved verbatim per the naming convention.
// ===========================================================================

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::MessageString base, IMessageProcessor, FreeMemSize

namespace RealmcIface
{

// ---------------------------------------------------------------------------
// RealmcIface::MessageLoadDone -- derives RealmcCore::MessageString (own final
// vtable off_8214896C).
//
// LAYOUT (from the ctor stores @0x82B57018 + the deleting-dtor free size):
//   +0x00 .. +0x1B  RealmcCore::MessageString base (RefCount vtable/refcount +
//                   muId@+8 + maText RealmcString@+0xC) -- 28 bytes (0x1C) on X360,
//                   confirmed by MessageString::~MessageString @0x82C46028 freeing
//                   28 bytes. The three own words follow the base directly (no pad).
//   +0x1C           muField1C -- the ctor's 4th argument, a word LoadTask::Execute
//                   reads from its own +0x14.
//   +0x20           mpField20 -- the ctor's 5th argument, the ADDRESS of
//                   LoadTask::Execute's +0x18 member (so pointer-typed here).
//   +0x24           miField24 -- the ctor's 2nd argument, the card status
//                   LoadTask::Execute computed (CardResultToCardStatus, or 8).
// The 3rd argument (LoadTask's +0x1D8 character buffer) is only the source of
// the MessageString text; it is not stored.
//
// sizeof == 0x28 (40) on X360 -- the `vector deleting destructor' @0x82B570D8
// frees 40 bytes, i.e. the 28-byte base + the three own 4-byte words. (On the PC
// target the pointer members widen; size is not byte-matched, per the project's
// semantic-parity rule.)
// ---------------------------------------------------------------------------
class MessageLoadDone : public RealmcCore::MessageString
{
public:
    // Build the MessageString base from a temporary
    // basic_string<char, RealmcCore::allocator> made from pText (the temporary
    // is freed once the base has copied it), then store the three own words.
    MessageLoadDone(std::uint32_t uId, int iField24, const char* pText,
                    u32 uField1C, void* pField20);

    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x30 slot (the thunk swaps its two arguments so
    //                       the processor becomes `this`).
    void Apply(RealmcCore::IMessageProcessor* pProcessor) override;

    // Backs the X360 `vector deleting destructor' @0x82B570D8: it restores this
    // class's vtable (off_8214896C), chains the RealmcCore::MessageString base
    // dtor, then frees 40 bytes when the delete flag bit0 is set. The virtual
    // dtor + the class operator delete below reproduce it compiler-generated.
    ~MessageLoadDone() override {}
    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    // The three own words follow the 28-byte (0x1C) MessageString base directly;
    // there is NO +0x18 pad (that offset is base territory -- maText's tail).
    u32   muField1C;  // +0x1C  (ctor arg 4; FLAG role unrecovered)
    void* mpField20;  // +0x20  (ctor arg 5; FLAG role/type unrecovered)
    int   miField24;  // +0x24  (ctor arg 2, the card status)
};

} // namespace RealmcIface
