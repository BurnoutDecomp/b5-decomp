// BrnDirector camera-director "moment" -- bodies for the two ledger functions homed
// in BrnMoment.h. Reconstructed from BURNOUT_X360_ARTIST.XEX, semantic-parity.
//
// Bodied here:
//   BrnDirector::Moment::Inhibit                    @0x82208520  (inline in BrnMoment.h)
//   BrnDirector::MomentBystanderSeesAction::Prepare @0x821F7560
//
// Moment::Inhibit is defined inline in the header (it must call the virtual Release()
// by name); this TU provides the out-of-line MomentBystanderSeesAction members.

#include "GameSource/Director/MomentController/BrnMoment.h"

namespace BrnDirector
{
    // @0x821F7560. The X360 sets the base state word (this+0x174 == meState) to
    // E_STATE_INVALID_SEARCHING and returns true -- i.e. the bystander moment marks
    // itself "searching" once prepared.
    bool MomentBystanderSeesAction::Prepare(void* /*lrBehaviourController*/)
    {
        SetState(E_STATE_INVALID_SEARCHING);
        return true;
    }

    // FLAG (placeholder name): the bystander moment's debug name string lives in
    // un-homed rodata; returning a stable literal here keeps the embed concrete
    // without fabricating the exact X360 string as fact.
    const char* MomentBystanderSeesAction::GetName() const
    {
        return "MomentBystanderSeesAction";
    }
}
