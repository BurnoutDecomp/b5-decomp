#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::Message base, IMessageProcessor, FreeMemSize

// ===========================================================================
// RealmcIface::MessageSetAutosaveDone -- the Realmc interface message that reports the set-autosave operation finished.
//
// It is a RealmcCore::Message (own final vtable [deleting destructor,
// RefCount::Unreferenced, Apply]). Apply hands the message to the shared
// RealmcCore::IMessageProcessor's +0x18 slot: the thunk swaps its two
// arguments so the processor becomes `this` and tail-calls that slot.
//
// There is no Feb-2007 leak source and no DWARF for this TU, so the SHAPE below
// is reconstructed purely from the console pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved verbatim.
//
// LAYOUT: the deleting destructor frees 20 bytes through the backend. The
// Message base is 8 bytes (vtable + refcount), so the class carries 3 own 4-byte words from +0x08. No constructor
// of this class is homed in the tree, so the words are kept for the size and
// named by offset only.
// ===========================================================================

namespace RealmcIface
{

class MessageSetAutosaveDone : public RealmcCore::Message
{
public:
    // SetAutosaveTask::Execute constructs this message inline in ARTIST and
    // stores the three result words at +8, +0xC and +0x10.
    MessageSetAutosaveDone(u32 uField08, u32 uField0C, u32 uField10)
        : RealmcCore::Message()
        , muField08(uField08)
        , muField0C(uField0C)
        , muField10(uField10)
    {
    }

    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x18 slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) ;

    u32 GetField08() const { return muField08; }
    u32 GetField0C() const { return muField0C; }
    u32 GetField10() const { return muField10; }

    // The deleting destructor restores the vtable and frees the object through
    // the Realmc backend with its size (the class operator delete).
    ~MessageSetAutosaveDone()  {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    u32 muField08;  // +0x08
    u32 muField0C;  // +0x0C
    u32 muField10;  // +0x10
};

} // namespace RealmcIface
