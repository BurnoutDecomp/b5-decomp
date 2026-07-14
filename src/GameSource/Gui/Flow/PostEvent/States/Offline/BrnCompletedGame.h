#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

// BrnGui::CompletedGame - the offline post-event "completed game" presentation flow
// state. This header carries the class shape and the inline resource accessor (the one
// ledger function attributed to the header); the license/photo/animation/textfield
// components, the substate machines and the out-of-line state/virtual machinery are
// reconstructed with the class TU. Layout/virtuals from the DecFIGS DWARF
// (BrnCompletedGame.h).
namespace BrnGui
{
    struct CompletedGame : public CgsGui::State
    {
        CompletedGame();                                   // @0x82500828

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // @ 0x825151A8 - this state requests no extra resources up front; the X360 body
        // writes null/0 to both out-params (li r11,0; stw r11,0(r4); stw r11,0(r5)). The
        // per-presentation resource (mResourceToLoad) is populated and loaded later from
        // the substate machine, not handed out here.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = 0;
            *lpuNumberOfResources = 0;
        }
    };
}
