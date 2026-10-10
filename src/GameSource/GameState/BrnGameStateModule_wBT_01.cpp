// GameStateModule::ProcessStreamingCompleteEvent -- one module finished streaming.
//
// ProcessGameEvents' case 9 hands it the StreamingCompleteEvent. It records the module's answer;
// while the module waits for streaming it finishes the wait once all four modules have answered,
// otherwise a GUI-screen answer clears the loading state. A race-car answer then also completes
// the car change the junkyard or the online car select is waiting on.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameEvents.h"                          // StreamingCompleteEvent
#include "GameSource/GameState/BrnGameActions.h"                         // SetLoadingStateAction
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager::StreamingFinished
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"    // OnlineCarSelectManager::StreamingFinished

namespace BrnGameState
{

void GameStateModule::ProcessStreamingCompleteEvent(const GameStateModuleIO::StreamingCompleteEvent* lpStreamingCompleteEvent,
                                                    GameStateModuleIO::GameActionQueue*              lpActionQueue)
{
    CGS_ASSERT(lpStreamingCompleteEvent->meModule >= 0, "lpStreamingCompleteEvent->meModule >= 0");
    CGS_ASSERT(lpStreamingCompleteEvent->meModule < GameStateModuleIO::StreamingCompleteEvent::E_MODULE_COUNT,
               "lpStreamingCompleteEvent->meModule < GsmIO::StreamingCompleteEvent::E_MODULE_COUNT");

    mabModuleStreamingComplete[lpStreamingCompleteEvent->meModule] = true;

    bool lbAllModulesComplete = true;
    for (s32 liModule = 0; liModule < GameStateModuleIO::StreamingCompleteEvent::E_MODULE_COUNT; ++liModule)
    {
        if (!mabModuleStreamingComplete[liModule])
        {
            lbAllModulesComplete = false;
            break;
        }
    }

    if (mbWaitingForStreaming && lbAllModulesComplete)
    {
        FinishStreaming(lpActionQueue);
    }
    else if (!mbWaitingForStreaming &&
             lpStreamingCompleteEvent->meModule == GameStateModuleIO::StreamingCompleteEvent::E_MODULE_GUI_SCREEN)
    {
        GameStateModuleIO::SetLoadingStateAction lSetLoadingStateAction;
        lSetLoadingStateAction.mbStartLoading = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetLoadingStateAction),
                                GameStateModuleIO::E_ACTION_SET_LOADING_STATE,
                                static_cast<s32>(sizeof(lSetLoadingStateAction)));
    }

    if (mabModuleStreamingComplete[GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY])
    {
        if (mCarSelectManager.IsInJunkyard())
        {
            if (mCarSelectManager.IsWaitingForStreaming())
            {
                CGS_ASSERT(lpStreamingCompleteEvent->meModule ==
                               GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY,
                           "Signalled streaming complete from wrong module, get Rob C");
                mCarSelectManager.StreamingFinished(lpStreamingCompleteEvent->mUserId, lpActionQueue);
            }
        }
        else if (mOnlineCarSelectManager.IsInOnlineCarSelect() && mOnlineCarSelectManager.IsWaitingForStreaming())
        {
            CGS_ASSERT(lpStreamingCompleteEvent->meModule ==
                           GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY,
                       "Signalled streaming complete from wrong module, get Rob C");
            mOnlineCarSelectManager.StreamingFinished(lpStreamingCompleteEvent->mUserId, lpActionQueue);
        }
    }
}

} // namespace BrnGameState
