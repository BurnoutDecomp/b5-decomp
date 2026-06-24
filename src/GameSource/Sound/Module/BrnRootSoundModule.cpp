#include "GameSource/Sound/Module/BrnRootSoundModule.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint / Message filter

// BrnSound::Module::RootSoundModule -- see the header. Minimal-then-grow reconstruction of the root
// sound module's Construct (0x826AF350) + Prepare (0x826FABF8). The real audio engine is grown on top.

namespace BrnSound
{
namespace Module
{
    // 0x826AF350. The X360 ctor-path: base ModuleSingleBuffered::Construct, zero a block of state
    // fields, init two embedded sub-objects (+0x4B8 / +0x280), build + register a Debug component,
    // construct an event-receiver queue (+0x13900), and set the constructed flag *(this+4)=1.
    // [minimal] mark the module constructed; the sub-objects + debug component + event queue are
    // grown with the audio engine (S6).
    void RootSoundModule::Construct()
    {
        mbConstructed = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] RootSoundModule::Construct (minimal)\n";
    }

    // 0x826FABF8 (vtable+64). The X360 stage machine constructs the playback + logic audio modules
    // from the allocator, wires the IO buffers, and returns false until fully prepared. [minimal]
    // capture the allocator for the grow-in engine and report prepared immediately so the
    // loading-screen Sound stage advances (no real audio engine yet).
    bool RootSoundModule::Prepare(rw::core::GeneralResourceAllocator* lpAllocator,
                                  void* /*lpRootInputBuffer*/, void* /*lpRootOutputBuffer*/,
                                  void* /*lpInputData*/, void* /*lpOutputData*/)
    {
        mpAllocator = lpAllocator;
        mbPrepared  = true;
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] RootSoundModule::Prepare (minimal) allocator="
                                       << (lpAllocator ? "ok" : "null") << " -> prepared\n";
        return true;   // fully prepared (loading screen advances)
    }
}
}
