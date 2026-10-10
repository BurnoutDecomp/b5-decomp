#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::Message base, IMessageProcessor, FreeMemSize

// ===========================================================================
// RealmcIface::MessageBootupDone -- the Realmc interface message that reports the bootup check finished.
//
// It is a RealmcCore::Message (own final vtable [deleting destructor,
// RefCount::Unreferenced, Apply]). Apply hands the message to the shared
// RealmcCore::IMessageProcessor's +0x3C slot: the thunk swaps its two
// arguments so the processor becomes `this` and tail-calls that slot.
//
// The SHAPE below
// is reconstructed purely from the console pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved verbatim.
//
// LAYOUT: the deleting destructor frees 12 bytes through the backend. The
// Message base is 8 bytes (vtable + refcount), so the class carries 1 own 4-byte word from +0x08. No constructor
// of this class is homed in the tree, so the words are kept for the size and
// named by offset only.
// ===========================================================================

namespace RealmcIface
{

class MessageBootupDone : public RealmcCore::Message
{
public:
    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x3C slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) override;

    // The deleting destructor restores the vtable and frees the object through
    // the Realmc backend with its size (the class operator delete).
    ~MessageBootupDone() override {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    u32 muField08;  // +0x08
};

} // namespace RealmcIface
