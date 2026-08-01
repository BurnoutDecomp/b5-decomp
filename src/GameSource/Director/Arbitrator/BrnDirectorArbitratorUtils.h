#ifndef GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H
#define GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                              // CGS_ASSERT
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorStateContainer.h"  // ArbitratorStateContainer::EState
#include "GameSource/Director/Arbitrator/BrnDirectorArbitratorState.h"           // ArbStateSharedInfo, ArbitratorState

// ============================================================================
// GameSource/Director/Arbitrator/BrnDirectorArbitratorUtils.h
//
// BrnDirector::ArbUtils -- the free-function helpers the arbitrator states use to hand control
// to a sibling state. Both are HEADER TEMPLATES on the console: every assert inside them cites
// this same .h path string (aDP4B5MainBurno_13), and DecFIGS homes them at
// BrnDirectorArbitratorUtils.h:78 (ChangeToStateWithoutRelease) and :48 (ChangeToState). Their
// assert line numbers -- 83 (0x53) and 53 (0x35) respectively -- sit five lines past each of
// those declaration lines, which is exactly where the CanRun tripwire falls in each body.
//
// BODIED 2026-08-01 from the X360 asm (@0x821FE2B8 and @0x821FE728). They were __debugbreak()
// trap stubs until now, so EVERY arbitrator hand-off in the game trapped on arrival.
//
// ⚠️⚠️ THE COMMITTED DECLARATION HAD ITS LAST TWO PARAMETERS INVERTED versus the console, and
// every call site in the tree matched the (wrong) declaration. The tree was internally
// consistent only because the body trapped before it could act on either value. Both the
// declaration and all eleven call sites are corrected in this wave. The evidence:
//
//   BODY @0x821FE2B8 -- r3=lrSharedInfo r4=leTargetState r5=&lreFromStateField r6->r23 r7->r24
//     0x821FE3FC  beq  cr6, loc_821FE418     ; Prepare() returned FALSE
//     0x821FE408  bl   ArbitratorStateContainer::ChangeToState
//     0x821FE40C  stw  r24, 0(r25)           ; SWITCHED  -> writes r24, i.e. ARG 5
//     0x821FE418  stw  r23, 0(r25)           ; BLOCKED   -> writes r23, i.e. ARG 4
//
//   CALLER @0x82219C58 (ProcessPossibleStateChanges), all nine sites identical in shape:
//     li r7, 0            ; arg 5 == E_STATE_INACTIVE
//     li r6, 7/8/9/A/B/C/D/E/F  ; arg 4 == the matching E_STATE_CHANGING_TO_*
//
//   So arg 4 is the BLOCKED value (park on CHANGING_TO_<dest> and retry next frame) and arg 5
//   is the SWITCHED value (go INACTIVE because the sibling state now owns the frame). That is
//   also the only semantically coherent reading: the CHANGING_TO_* values exist precisely to
//   be the retry state after a declined Prepare.
//
// ⚠️ The DecFIGS DWARF carries NO parameter names for either template (types only:
// `extern void ChangeToStateWithoutRelease<...>(ArbStateSharedInfo&,
// ArbitratorStateContainer::EState, EState&, EState, EState)`), so the previous
// leFromStateWhenSwitched/leFromStateWhenBlocked ordering was never DWARF-attested -- it was a
// guess that read plausibly. The asm is the authority.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
    namespace ArbUtils
    {
        // --------------------------------------------------------------------
        // @0x821FE2B8 -- BrnDirectorArbitratorUtils.h:78. Hand the frame to a sibling
        // arbitrator state WITHOUT releasing the caller.
        //
        //   0x821FE2D0  addi r11, r26, 0xD68 ; slwi r11,r11,2 ; lwzx r31, r11, r10
        //               -- r10 == sharedInfo+0x14 (mpStateContainer); the container's state
        //               pointer table lives at +0xD68*4 == +0x35A0, so this is GetState(target).
        //   0x821FE2F4  lwz  r11, 0x14(vtable)   -- slot 5 == CanRun (base vtable order:
        //               0 Construct, 1 Prepare, 2 Update, 3 Release, 4 Destruct, 5 CanRun,
        //               6 GetName). Non-gating: the assert falls through into Prepare either way.
        //   0x821FE374  lwz  r11, 0x18(vtable)   -- slot 6 == GetName, streamed into the message.
        //   0x821FE3E8  lwz  r11, 4(vtable)      -- slot 1 == Prepare, the real gate.
        // --------------------------------------------------------------------
        template <class TFromState>
        void ChangeToStateWithoutRelease(ArbStateSharedInfo& lrSharedInfo,
                                         ArbitratorStateContainer::EState leTargetState,
                                         TFromState& lreFromStateField,
                                         TFromState leFromStateWhenBlocked,
                                         TFromState leFromStateWhenSwitched)
        {
            ArbitratorState* lpTargetState = lrSharedInfo.mpStateContainer->GetState(leTargetState);

            // :83 -- non-gating tripwire. The console streams
            // "Trying to switch to state " << lpTargetState->GetName() << " when it can't be run"
            // into the assert buffer; CGS_ASSERT takes a fixed string, so the name is dropped
            // from the text only. The CanRun() call itself is preserved because the console
            // makes it unconditionally, before the assert decides anything.
            CGS_ASSERT(lpTargetState->CanRun(lrSharedInfo),
                       "Trying to switch to a state when it can't be run");

            if (lpTargetState->Prepare(lrSharedInfo))
            {
                lrSharedInfo.mpStateContainer->SetCurrentState(leTargetState);
                lreFromStateField = leFromStateWhenSwitched;
            }
            else
            {
                lreFromStateField = leFromStateWhenBlocked;
            }
        }

        // --------------------------------------------------------------------
        // @0x821FE728 -- BrnDirectorArbitratorUtils.h:48. The release-on-switch overload.
        // Identical to the above through the Prepare gate, then:
        //   0x821FE874  bl   ArbitratorStateContainer::ChangeToState
        //   0x821FE884  lwz  r11, 0xC(vtable-of-r25) ; slot 3 == Release, on the CALLER
        // and -- verified, not an omission -- it does NOT write lreFromStateField on success.
        // Only the blocked path stores:
        //   0x821FE898  stw  r23, 0(r24)   ; r23 == arg 5, r24 == &lreFromStateField (arg 4)
        // so there is exactly one from-state value here and the committed order was correct.
        // (The caller's own Release() seeds whatever post-switch state it wants.)
        // --------------------------------------------------------------------
        template <class TFromState>
        void ChangeToState(ArbitratorState* lprCallingState,
                           ArbStateSharedInfo& lrSharedInfo,
                           ArbitratorStateContainer::EState leTargetState,
                           TFromState& lreFromStateField,
                           TFromState leFromStateWhenBlocked)
        {
            ArbitratorState* lpTargetState = lrSharedInfo.mpStateContainer->GetState(leTargetState);

            // :53 -- same non-gating tripwire as above.
            CGS_ASSERT(lpTargetState->CanRun(lrSharedInfo),
                       "Trying to switch to a state when it can't be run");

            if (lpTargetState->Prepare(lrSharedInfo))
            {
                lrSharedInfo.mpStateContainer->SetCurrentState(leTargetState);
                lprCallingState->Release(lrSharedInfo);
            }
            else
            {
                lreFromStateField = leFromStateWhenBlocked;
            }
        }
    }
}

#endif // GAMESOURCE_DIRECTOR_ARBITRATOR_BRN_DIRECTOR_ARBITRATOR_UTILS_H
