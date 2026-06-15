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
}
