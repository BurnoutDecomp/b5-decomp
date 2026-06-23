#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"            // CgsDev::DebugComponent base
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/Windows/CgsLogWindow.h"      // CgsDev::DebugUI::LogWindow
#include "GameSource/Sound/Debug/BrnSoundDebugStatistics.h"                                   // BrnSound::Debug::Statistics

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSound::Debug::DebugComponent::DebugComponent @ 0x827E0720  (EXECUTED in the boot trace)
//   BrnSound::Debug::DebugComponent::GetName        @ 0x827E0798  -> "Sound"
//   BrnSound::Debug::DebugComponent::GetPath        @ 0x827E07A8  -> "Sound Module"
//
// The debug component for the sound module: it shows up in the in-game debug UI under
// "Sound Module" and owns a scrolling LogWindow plus a block of playback Statistics. The
// constructor (0x827E0720) installs the component vtable, constructs two small embedded
// queue-like accumulators (each: vtable + a count word the base Clear zeroes), constructs
// the LogWindow and the Statistics block.
//
// Constructed by BrnSound::Module::RootSoundModule::RootSoundModule.

namespace BrnSound
{
    namespace Debug
    {
        // One of the two small embedded accumulators the ctor builds at guest +0x0C and +0x14.
        // Each is an 8-byte polymorphic object: a vtable pointer (the X360 stores a shared
        // empty vtable, then the object's own vtable -- off_820CDEF0 at +0x0C, off_820CDEF8 at
        // +0x14) followed by an entry-count word that the queue base's Clear zeroes.
        //
        // FLAG: the X360 ctor calls CgsContainers::BasePriorityQueue::Clear on each member, but
        // the members are only 8 bytes (vtable + count) -- the full BasePriorityQueue is 20 bytes,
        // so the Hex-Rays attribution is the compiler folding the count=0 store into a Clear call.
        // The exact element type of these queues is uncommitted (out of scope); the observable
        // ctor effect (install vtable, zero the count) is reproduced by name here.
        struct SoundDebugEventQueue
        {
            void* mpVtable;       // guest +0x00  (object vtable; ctor installs the real one last)
            u32   muNumEntries;   // guest +0x04  (zeroed by the queue base's Clear)
        };

        class DebugComponent : public CgsDev::DebugComponent
        {
        public:
            // 0x827E0720 -- install the component vtable (handled by the C++ vtable of this derived
            // type), zero the two embedded event-queue counts, and construct the LogWindow and the
            // Statistics block.
            DebugComponent();

        protected:
            // The debug-UI identity hooks (override the CgsDev::DebugComponent virtuals).
            const char* GetName() const override { return "Sound"; }   // 0x827E0798
            const char* GetPath() const override { return "Sound Module"; } // 0x827E07A8

        private:
            // Two small embedded queue-like accumulators (guest +0x0C and +0x14).
            SoundDebugEventQueue mEventQueueA;   // guest +0x0C (vtable off_820CDEF0)
            SoundDebugEventQueue mEventQueueB;   // guest +0x14 (vtable off_820CDEF8)

            CgsDev::DebugUI::LogWindow mLogWindow;   // guest +0x1C  scrolling log window
            BrnSound::Debug::Statistics mStatistics; // guest +0x358 playback statistics block
        };
    }
}
