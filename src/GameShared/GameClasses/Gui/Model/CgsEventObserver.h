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

        virtual void PreWorldUpdate();
        virtual void Update();

    protected:
        StateInterface mStateInterface;
    };
}
