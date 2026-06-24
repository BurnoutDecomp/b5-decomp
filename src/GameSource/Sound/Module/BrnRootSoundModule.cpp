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
        // Faithful order from 0x826AF350 (steps that have a real PC backing are implemented; the
        // uncommitted ones are marked grow-ins):
        //   [skip] X360 step 1 (BaseCollisionGenerator::Destruct(3)) is a Hex-Rays/ICF artifact
        //          (r3=3 constant, not `this`) -- not a real call.

        // [step 3] base module construct (resets stages, builds the data buffers).
        CgsModule::ModuleSingleBuffered::Construct();

        // [steps 4-5] GROW-IN: the X360 virtual-inits two embedded sub-objects -- this+0x4B8
        //   (vtable slot 16, arg 6) and this+0x280 (slot 0) -- which live inside the embedded
        //   SoundLogicModule (~79KB) the minimal RootSoundModule does not yet mirror. Added with
        //   the audio engine (the Playback/Logic sub-modules).
        // [steps 6-7] GROW-IN: BrnSound::Debug::DebugComponent::Construct(this+0x4D38, +0x4B8,
        //   +0x280) + CgsDev::DebugComponent::Register -- the PC BrnDebugComponent only models a
        //   default ctor (no Construct(a,b)); wired with the sub-objects above.

        // [step 8] the event-receiver queue @ X360 this+0x13900 (capacity 5120, align 16). Real +
        //   faithful: Construct() performs the X360 capacity/alignment/Clear sequence.
        mEventQueue.Construct();

        // [steps 2,9] GROW-IN: trailing state fields at X360 +0x14D18 / +0x14D1C..+0x14D2C (one
        //   seeded to 7) -- no PC members for them in the minimal layout yet.

        // [step 10] the constructed flag (X360 *(this+4)=1).
        mbConstructed = true;

        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "[Sound] RootSoundModule::Construct\n";
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
