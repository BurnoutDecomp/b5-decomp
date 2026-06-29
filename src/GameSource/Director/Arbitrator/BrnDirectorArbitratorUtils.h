#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H

#include "types.hpp"
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"  // ArbitratorStateContainer::EState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"           // ArbStateSharedInfo

// ============================================================================
// GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h
//
// BrnDirector::ArbUtils -- the free-function helpers the arbitrator states use to hand control
// to a sibling state. DWARF home BrnDirectorArbitratorUtils.h:31. This TU (ArbStateRoaming)
// uses the ChangeToStateWithoutRelease<EState> overload.
//
// Signature recovered from the DecFIGS DWARF (BrnDirectorArbitratorUtils.h:78):
//   template<class TFromState>
//   void ChangeToStateWithoutRelease(ArbStateSharedInfo&,
//                                    ArbitratorStateContainer::EState leTargetState,
//                                    TFromState& lreFromStateField,
//                                    TFromState leFromStateWhenSwitched,
//                                    TFromState leFromStateWhenBlocked);
// X360 body @0x821FE2B8: look the target state up in the container's state table, assert it
// CanRun, Prepare it; on success ChangeToState(container, target) and set the caller's state
// field to leFromStateWhenSwitched, otherwise set it to leFromStateWhenBlocked.
//
// BODY = TRAP STUB (the template equivalent of `work stubs`): the real body lives with the
// ArbitratorUtils TU and dispatches through the container's vtable. Declaring it here with a
// trap body lets the per-TU `cl /c` gate compile ArbStateRoaming's calls without inventing
// the dispatch logic; the trap fires only if the link ever resolves to this placeholder.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace ArbUtils
    {
        template <class TFromState>
        void ChangeToStateWithoutRelease(ArbStateSharedInfo& lrSharedInfo,
                                         ArbitratorStateContainer::EState leTargetState,
                                         TFromState& lreFromStateField,
                                         TFromState leFromStateWhenSwitched,
                                         TFromState leFromStateWhenBlocked)
        {
            (void)lrSharedInfo;
            (void)leTargetState;
            (void)lreFromStateField;
            (void)leFromStateWhenSwitched;
            (void)leFromStateWhenBlocked;
            __debugbreak();   // trap stub -- real body lands with the ArbitratorUtils TU
        }
    }
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H
