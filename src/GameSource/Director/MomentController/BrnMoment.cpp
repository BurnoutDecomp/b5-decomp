// BrnDirector camera-director "moment" -- bodies for the two ledger functions homed
// in BrnMoment.h. Reconstructed from the console executable, semantic-parity.
//
// Bodied here:
//   BrnDirector::Moment::Inhibit  (inline in BrnMoment.h)
//   BrnDirector::MomentBystanderSeesAction::Prepare
//
// Moment::Inhibit is defined inline in the header (it must call the virtual Release()
// by name); this TU provides the out-of-line MomentBystanderSeesAction members.

#include "GameSource/Director/MomentController/BrnMoment.h"

namespace BrnDirector
{
    // The console sets the base state word (this+0x174 == meState) to
    // E_STATE_INVALID_SEARCHING and returns true -- i.e. the bystander moment marks
    // itself "searching" once prepared.
    bool MomentBystanderSeesAction::Prepare(void* /*lrBehaviourController*/)
    {
        SetState(E_STATE_INVALID_SEARCHING);
        return true;
    }

    // Prepare (above) is the one MomentBystanderSeesAction member homed in this TU rather than
    // in the class's own Moments/BrnMomentBystanderSeesAction.cpp.
}
