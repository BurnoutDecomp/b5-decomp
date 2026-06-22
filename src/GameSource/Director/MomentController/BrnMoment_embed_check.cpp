// Embed/exercise check for BrnDirector::Moment / MomentBystanderSeesAction
// (compile-only gate). Exercises the two bodied functions through a concrete moment.
// Not shipped.

#include "GameSource/Director/MomentController/BrnMoment.h"

namespace
{
    using BrnDirector::Moment;
    using BrnDirector::MomentBystanderSeesAction;

    bool Use()
    {
        MomentBystanderSeesAction m;

        bool prepared = m.Prepare(nullptr);   // @0x821F7560 -> SetState(searching)
        m.Inhibit();                           // @0x82208520 (inline; Release + SetState)

        // Touch the by-name state the two functions wrote.
        bool inhibited = m.IsInhibited();
        bool searching = (m.GetState() == Moment::E_STATE_INVALID_SEARCHING);

        return prepared && inhibited && searching;
    }
}
