#pragma once

// CgsGui::EventObserver - the base a GUI flow/component derives from to watch and
// emit GUI events. It owns a StateInterface (the channel to the rest of the game:
// the large output event queue plus the access pointers/allocator) and forwards
// Construct/Prepare to it. Member set/types from the DecFIGS DWARF
// (CgsEventObserver.h); behaviour confirmed against the X360 Construct @0x8285B7D0
// / Prepare @0x8284FBF0. The object is polymorphic — Construct leaves offset 0 for
// the vptr and places mStateInterface at +4 — so the two no-argument virtuals the
// DWARF lists anchor the vtable. (The DWARF also lists
// SetInEventQueue(InputBuffer::GuiEventQueue*); it is absent from the X360 ledger
// and would pull in the InputBuffer cascade, so it is left out here.)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // CgsGui::StateInterface, GuiAccessPointers

namespace rw { struct IResourceAllocator; }

namespace CgsGui
{
    class EventObserver
    {
    public:
        void Construct();
        bool Prepare(GuiAccessPointers* lpAccessPointers, rw::IResourceAllocator* lpAllocator);

        // EXTENSION (X360 vtable order): the interpreter's ProcessInEvents dispatches the
        // accumulated per-observer queue through the observer's FIRST virtual
        // (CgsEventInterpreterModule.cpp ProcessInEvents @0x8285B448 calls `(***mpObserver)(
        // mpObserver, queue)`). The DWARF lists only PreWorldUpdate/Update; this leading
        // ProcessEvents virtual is required for the X360-faithful vtable[0] dispatch. Declared
        // (not defined) so the interpreter's per-TU compile gate can reference it; the queue is
        // the interpreter's per-observer working queue (a VariableEventQueue<18432,16>).
        virtual void ProcessEvents(CgsModule::VariableEventQueue<18432, 16>* lpEventQueue);

        virtual void PreWorldUpdate();
        virtual void Update();

        // EXTENSION: named access to the observer's outbound event queue (its StateInterface's
        // large output queue). The interpreter's ProcessOutEvents reaches this queue (the X360
        // reads it at this+16, i.e. mStateInterface.mOutEventQueue) to drain each observer's
        // emitted events; exposing it by name avoids an offset reinterpret_cast at the call site.
        GuiStackEventQueue::GuiEventQueueLarge* GetOutputEventQueue()
        {
            return mStateInterface.GetOutputEventQueue();
        }

    protected:
        StateInterface mStateInterface;
    };
}
