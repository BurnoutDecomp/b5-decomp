#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::Message base, IMessageProcessor, FreeMemSize

// ===========================================================================
// RealmcCore::MessageClear -- the Realmc core message that asks its processor to clear.
//
// It is a RealmcCore::Message (own final vtable [deleting destructor,
// RefCount::Unreferenced, Apply]). Apply hands the message to the shared
// RealmcCore::IMessageProcessor's +0x44 slot: the thunk swaps its two
// arguments so the processor becomes `this` and tail-calls that slot.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the console pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved verbatim.
//
// LAYOUT: the deleting destructor frees 8 bytes through the backend. The
// Message base is 8 bytes (vtable + refcount), so the class carries no members of its own.
// ===========================================================================

namespace RealmcCore
{

class MessageClear : public RealmcCore::Message
{
public:
    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x44 slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) override;

    // The deleting destructor restores the vtable and frees the object through
    // the Realmc backend with its size (the class operator delete).
    ~MessageClear() override {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }
};

} // namespace RealmcCore
