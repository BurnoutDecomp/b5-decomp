#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"

namespace CgsModule { struct Event; }

// BrnGui::Video - the screen "video" flow state (FMV_VIDEO): plays the Criterion movie, lets
// the player skip it, and goes back when it finishes. Bodies in BrnVideo.cpp, apart from the
// inline resource accessor.
namespace BrnGui
{
    struct Video : public CgsGui::State
    {
        enum ESubState
        {
            E_SUBSTATE_ENTERED       = 0,
            E_SUBSTATE_VIDEO_PLAYING = 1,
            E_SUBSTATE_COUNT         = 2,
        };

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // This state loads no GUI resources of its own: only the count out-param is written
        // (zero); the tuple pointer is left untouched.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            (void)lppResourceTuples;
            *lpuNumberOfResources = 0;
        }

    private:
        // A controller action while the movie plays: the skip action stops it.
        void HandleControllerInput(const CgsModule::Event* lpEvent);

        // The two input channels this state listens on: controller actions and video finished.
        static const s32 maiEventToObserve[2];
        static const s32 miNumEventsObserved;

        ESubState meSubState;   // console +0x38
    };
}
