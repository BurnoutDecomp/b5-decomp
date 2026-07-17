#include "GameShared/GameClasses/Gui/Model/CgsEventObserver.h"

// CgsGui::EventObserver::Construct @ 0x8285B7D0
// CgsGui::EventObserver::Prepare   @ 0x8284FBF0
// Reconstructed from BURNOUT_X360_ARTIST.XEX. EventObserver delegates to its
// embedded StateInterface: Construct builds the interface (which clears its access
// pointers and constructs the large output event queue) then points it back at this
// observer; Prepare hands the interface its access pointers and resource allocator.
// The X360 pseudocode inlines StateInterface::Construct / ::Prepare (clearing the
// three leading pointers and constructing the queue in place); we restore the
// member-function calls.

namespace CgsGui
{
    void EventObserver::Construct()
    {
        mStateInterface.Construct();
        mStateInterface.SetEventObserver(this);
    }

    bool EventObserver::Prepare(GuiAccessPointers* lpAccessPointers, rw::IResourceAllocator* lpAllocator)
    {
        mStateInterface.Prepare(lpAllocator, lpAccessPointers);
        return true;
    }

    // The base virtual defaults. The concrete observers (the flows, the components)
    // override the per-frame pair; the base bodies are the no-op defaults the X360
    // vtable carries for observers that don't. ProcessEvents (the interpreter's
    // vtable[0] dispatch target) hands the observer its per-frame filtered queue; the
    // base default consumes nothing (a flow routes its queue through SetInEventQueue
    // instead -- the queue is delivered to the state machine's in-queue by the module).
    void EventObserver::ProcessEvents(CgsModule::VariableEventQueue<18432, 16>* /*lpEventQueue*/) {}
    void EventObserver::PreWorldUpdate() {}
    void EventObserver::Update() {}
}
