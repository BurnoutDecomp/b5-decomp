#pragma once

#include "types.hpp"
#include "SDKs/Realmc/RealmcCore.h"   // RealmcCore::Message base, IMessageProcessor, FreeMemSize
#include "SDKs/Realmc/RealmcLoadEntryInfo.h" // LoadTask passes its entry array at +0x0C

// ===========================================================================
// RealmcIface::MessageCheckLoadedData -- the Realmc interface message that asks for the loaded data to be checked.
//
// It is a RealmcCore::Message (own final vtable [deleting destructor,
// RefCount::Unreferenced, Apply]). Apply hands the message to the shared
// RealmcCore::IMessageProcessor's +0x10 slot: the thunk swaps its two
// arguments so the processor becomes `this` and tail-calls that slot.
//
// The SHAPE below
// is reconstructed purely from the console pseudocode + asm. `Realmc` is a vendor
// library boundary, so its identifiers are preserved verbatim.
//
// LAYOUT: the deleting destructor frees 16 bytes through the backend. The
// Message base is 8 bytes (vtable + refcount), so the class carries 2 own 4-byte words from +0x08. No constructor
// of this class is homed in the tree, so the words are kept for the size and
// named by offset only.
// ===========================================================================

namespace RealmcIface
{

class MessageCheckLoadedData : public RealmcCore::Message
{
public:
    MessageCheckLoadedData(u32 uCount, LoadEntryInfo* pEntries)
        : RealmcCore::Message(), muField08(uCount), mpEntries(pEntries) {}

    // Message vtable +8 -- pProcessor->ProcessMessage(this), the processor's
    //                       +0x10 slot.
    void Apply(RealmcCore::IMessageProcessor* pProcessor) ;

    u32 GetCount() const { return muField08; }
    LoadEntryInfo* GetEntries() const { return mpEntries; }

    // The deleting destructor restores the vtable and frees the object through
    // the Realmc backend with its size (the class operator delete).
    ~MessageCheckLoadedData()  {}

    static void operator delete(void* lpBlock, size_t luSize)
    {
        RealmcCore::FreeMemSize(lpBlock, static_cast<u32>(luSize));
    }

private:
    u32 muField08;  // +0x08
    LoadEntryInfo* mpEntries; // +0x0C
};

} // namespace RealmcIface

CGS_STATIC_ASSERT((sizeof(RealmcIface::MessageCheckLoadedData) == 16), "ARTIST MessageCheckLoadedData size");
