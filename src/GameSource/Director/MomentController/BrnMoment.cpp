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

// ---- [FX-DIRECTOR 2026-09-24] the abstract base's two defined virtuals -------------------------
// DWARF BrnMoment.cpp:43 / :51 define them out of line. No console instance is dispatched: every
// concrete vftable overrides slot 3 with its own SetParameters, and slot 5 of all twelve is the
// shared empty body 0x8284CB38 (`blr`). The base keeps the same empty Destruct; its SetParameters
// has no attested body (FLAG: nothing -- the concrete overrides store the pointer themselves).
namespace BrnDirector
{
    void Moment::SetParameters(const Parameters* /*lpParameters*/)
    {
    }

    void Moment::Destruct()
    {
    }
}
