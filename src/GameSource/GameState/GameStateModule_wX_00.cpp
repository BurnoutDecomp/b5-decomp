// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_wX_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h).
//
// ProcessGameEventsLandmarkRouteRequestBringUp: the CASE-84 arm of
// GameStateModule::ProcessGameEvents, the sat-nav's route question. The console arm is one call:
//     SendRouteRequestAction(this, the event, the action queue, E_OWNER_GUI)
// (the owner is the literal 1; the event and the queue are the ones the dispatcher already
// holds). No assert, no guard: the arm runs in every mode.
// Producer of event 84: BrnGameModule::BridgeGuiToGameState, which repacks GuiTracker::Update's
// CalculateRoute (GUI out 494) into a LandmarkRouteRequestEvent. SendRouteRequestAction posts
// action 50; the AI module's route planner answers with an E_OWNER_GUI route response, which
// BrnGameModule::BridgeWorldRouteInformationToGui hands back to the tracker as GUI 211.
// Extracted like its sibling arms (GameStateModule_gUI_00.cpp): one walk of the merged pre-world
// queue, before the Clear, which PreWorldUpdateStuntBringUp owns.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<1536,16>
#include "GameSource/GameState/BrnGameEvents.h"                    // LandmarkRouteRequestEvent / E_EVENT_LANDMARK_ROUTE_REQUEST
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"          // BrnAI::RouteMapModuleIO::E_OWNER_GUI
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // the BRN_SATNAV_DIAG witness

#include <cstdlib>   // getenv

namespace BrnGameState
{

void GameStateModule::ProcessGameEventsLandmarkRouteRequestBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liType == GameStateModuleIO::E_EVENT_PLAYER_ENTERS_RACE_MAP ||
            liType == GameStateModuleIO::E_EVENT_LANDMARK_RACES_REQUEST)
        {
            // Original ProcessGameEvents823A1894 handles both15/85 identically.
            SendSetLandmarkRacesAction(lpActionQueue);
        }
        else if (liType == GameStateModuleIO::E_EVENT_LANDMARK_ROUTE_REQUEST)
        {
            SendRouteRequestAction(
                reinterpret_cast<const GameStateModuleIO::LandmarkRouteRequestEvent*>(lpEvent),
                lpActionQueue,
                BrnAI::RouteMapModuleIO::E_OWNER_GUI);

            // [FLAG PC witness] BRN_SATNAV_DIAG, first 32: one line per GUI route question.
            static const bool sbDiag      = (getenv("BRN_SATNAV_DIAG") != 0);
            static s32        siLinesLeft = 32;
            if (sbDiag && siLinesLeft > 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                --siLinesLeft;
                const GameStateModuleIO::LandmarkRouteRequestEvent* lpRouteRequestEvent =
                    reinterpret_cast<const GameStateModuleIO::LandmarkRouteRequestEvent*>(lpEvent);
                *CgsDev::Log::gpDebugPrint
                    << "[satnav] event 84 -> SendRouteRequestAction owner GUI leg "
                    << static_cast<s32>(lpRouteRequestEvent->mu16EventID) << "\n";
            }
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
}

}
