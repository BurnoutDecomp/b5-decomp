// GameStateModule::ProcessGameEvents_Group1 -- the dispatcher's player-car, car-select and
// profile-car arms (GameStateModule_ProcessGameEvents.cpp routes these sixteen ids here).
//
// Each arm is the console's: it sets the player car up or moves it, changes its colours, drives
// the junkyard (CarSelectManager) or the online car select (OnlineCarSelectManager) through the
// GUI's car-select steps, and answers the GUI's car and profile queries with game actions. The
// cases are in the console's source order.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<CgsID,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"               // CarSelectionResponseAction bits
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                           // CgsIDUnCompress / KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PreWorldInputBuffer, OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // the event payloads
#include "GameSource/GameState/BrnGameActions.h"                         // the posted action records
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"             // PlayerInfo
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"    // OnlineCarSelectManager
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem / CarData
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"      // BrnProgression::CarData
#include "GameSource/GameState/Progression/BrnProgressionLiveryData.h"   // BrnProgression::LiveryData
#include "GameSource/GameState/Progression/BrnDerivedCars.h"             // DerivedCarArray
#include "SharedClasses/DataLists/VehicleList.h"                         // VehicleList::GetVehicleFromId
#include "SharedClasses/DataLists/VehicleListEntry.h"                    // IsLiveryColour / GetParentId

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents_Group1(s32 liEventType, const CgsModule::Event* lpEvent,
                                               GameStateModuleIO::GameActionQueue* lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* /*lpPreWorldInput*/,
                                               GameStateModuleIO::OutputBuffer* lpOutput)
{
    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_SETUP_PLAYER_CAR:
    {
        const GameStateModuleIO::SetupPlayerCarEvent* lpSetupPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::SetupPlayerCarEvent*>(lpEvent);
        CGS_ASSERT(!mCarSelectManager.IsInJunkyard(), "!mCarSelectManager.IsInJunkyard()");

        OnPlayerCarChange(lpSetupPlayerCarEvent->mCarModelId, lpSetupPlayerCarEvent->mWheelModelId,
                          lpActionQueue, true);

        GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
        lResetPlayerCarAction.mPosition            = lpSetupPlayerCarEvent->mPosition;
        lResetPlayerCarAction.mDirection           = lpSetupPlayerCarEvent->mDirection;
        lResetPlayerCarAction.mCarModelId          = lpSetupPlayerCarEvent->mCarModelId;
        lResetPlayerCarAction.mWheelModelId        = lpSetupPlayerCarEvent->mWheelModelId;
        lResetPlayerCarAction.mePlayerScoringIndex =
            FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex());
        lResetPlayerCarAction.mfDeformationAmount  =
            mProgressionManager.GetProfile()->GetPlayerBaseDeformAmount(lpSetupPlayerCarEvent->mCarModelId);
        lResetPlayerCarAction.miBaseDeformationType      = (lResetPlayerCarAction.mfDeformationAmount > 0.0f) ? 1 : -1;
        lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
        lResetPlayerCarAction.mbInCarSelectScreen        = false;
        lResetPlayerCarAction.mbCarSelectDontStreamAudio = false;
        lResetPlayerCarAction.muReserved0x42             = 0;
        lResetPlayerCarAction.mbKeepResetSection         = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                                GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                                static_cast<s32>(sizeof(lResetPlayerCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_REQUEST_CAR_UNLOCK_EVENT:
    {
        const GameStateModuleIO::RequestCarUnlockEvent* lpRequestCarUnlockEvent =
            reinterpret_cast<const GameStateModuleIO::RequestCarUnlockEvent*>(lpEvent);
        CGS_ASSERT(lpRequestCarUnlockEvent != NULL, "lpRequestCarUnlockEvent != NULL");
        mProgressionManager.AddCar(lpRequestCarUnlockEvent->mCarModelId,
                                   BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR);
        break;
    }

    case GameStateModuleIO::E_EVENT_TELEPORT_PLAYER_CAR:
    {
        const GameStateModuleIO::TeleportPlayerCarEvent* lpTeleportPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::TeleportPlayerCarEvent*>(lpEvent);

        // Same car, new pose: no scoring slot and the no-deform sentinel. +0x38 is not written on
        // this path (as in CarSelectManager::EnterJunkyardAtStartOfGame).
        GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
        lResetPlayerCarAction.mPosition                  = lpTeleportPlayerCarEvent->mPosition;
        lResetPlayerCarAction.mDirection                 = lpTeleportPlayerCarEvent->mDirection;
        lResetPlayerCarAction.mCarModelId                = mActivePlayerCarId;
        lResetPlayerCarAction.mWheelModelId              = mActivePlayerWheelId;
        lResetPlayerCarAction.mePlayerScoringIndex       = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
        lResetPlayerCarAction.mfDeformationAmount        = -1.0f;
        lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
        lResetPlayerCarAction.mbInCarSelectScreen        = false;
        lResetPlayerCarAction.mbCarSelectDontStreamAudio = false;
        lResetPlayerCarAction.muReserved0x42             = 0;
        lResetPlayerCarAction.mbKeepResetSection         = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                                GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                                static_cast<s32>(sizeof(lResetPlayerCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR:
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
        }
        HandleChangePlayerCarEvent(reinterpret_cast<const GameStateModuleIO::ChangePlayerCarEvent*>(lpEvent),
                                   lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_SELECT_PLAYER_CAR:
    {
        const GameStateModuleIO::SelectPlayerCarEvent* lpSelectPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::SelectPlayerCarEvent*>(lpEvent);
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.RequestChangeCar(lpSelectPlayerCarEvent->mCarModelId);
        }
        else if (mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            mOnlineCarSelectManager.RequestChangeCar(lpActionQueue, lpSelectPlayerCarEvent->mCarModelId);
        }
        else
        {
            CGS_ASSERT(false, "Should be either in the junkyard or an online event car select\n");
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR_COLOUR:
    {
        const GameStateModuleIO::ChangePlayerCarColourEvent* lpChangePlayerCarColourEvent =
            reinterpret_cast<const GameStateModuleIO::ChangePlayerCarColourEvent*>(lpEvent);
        CGS_ASSERT(( mCarSelectManager.IsInJunkyard() ) || ( mOnlineCarSelectManager.IsInOnlineCarSelect() ),
                   "( mCarSelectManager.IsInJunkyard() ) || ( mOnlineCarSelectManager.IsInOnlineCarSelect() )");

        BrnProgression::CarData* lpCarData = mProgressionManager.GetProfile()->FindCar(mActivePlayerCarId);

        GameStateModuleIO::CarSelectChangeColourAction lChangeColourAction;
        lChangeColourAction.muPaletteIndex = lpChangePlayerCarColourEvent->muPaletteIndex;
        lChangeColourAction.muColourIndex  = lpChangePlayerCarColourEvent->muColourIndex;
        if (lpCarData != 0)
        {
            lpCarData->SetColourIndex(static_cast<s32>(lpChangePlayerCarColourEvent->muColourIndex));
            lpCarData->SetPaletteIndex(static_cast<s32>(lpChangePlayerCarColourEvent->muPaletteIndex));
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeColourAction),
                                GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
                                static_cast<s32>(sizeof(lChangeColourAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_CAR_COLOUR_REQUEST:
    {
        const GameStateModuleIO::PlayerCarColourRequestEvent* lpPlayerCarColourRequestEvent =
            reinterpret_cast<const GameStateModuleIO::PlayerCarColourRequestEvent*>(lpEvent);

        s32 liColourIndex;
        s32 liPaletteIndex;
        mProgressionManager.GetCarColourAndPalette(lpPlayerCarColourRequestEvent->mCarId,
                                                   &liColourIndex, &liPaletteIndex);

        GameStateModuleIO::PlayerCarColourResponseAction lColourResponseAction;
        lColourResponseAction.muPaletteIndex = static_cast<u32>(liPaletteIndex);
        lColourResponseAction.muColourIndex  = static_cast<u32>(liColourIndex);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lColourResponseAction),
                                GameStateModuleIO::E_ACTION_PLAYER_CAR_COLOUR_RESPONSE,
                                static_cast<s32>(sizeof(lColourResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_NETWORK_CAR:
    {
        const GameStateModuleIO::ChangeNetworkCarEvent* lpChangeNetworkCarEvent =
            reinterpret_cast<const GameStateModuleIO::ChangeNetworkCarEvent*>(lpEvent);
        ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();

        const CarData* lpScoringCarData = lpScoringSystem->GetCarData(lpChangeNetworkCarEvent->mNetworkPlayerID);
        if (lpScoringCarData == 0)
        {
            break;
        }
        const ::EActiveRaceCarIndex leActiveRaceCarIndex = lpScoringCarData->GetActiveRaceCarIndex();
        if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            break;
        }
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            mLastActiveRaceCarInterface.GetRaceCarState(leActiveRaceCarIndex);
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

        // The car stays where the remote player's car is: the state's translation row is the
        // position, its at row the direction; the float is the payload's +0x18.
        const Vector3 lAt       = lpRaceCarState->mTransform.At();
        const Vector3 lPosition = lpRaceCarState->mTransform.Pos();

        GameStateModuleIO::SetupNetworkCarAction lSetupNetworkCarAction;
        lSetupNetworkCarAction.Construct(lpScoringSystem->GetPlayerScoringIndex(lpChangeNetworkCarEvent->mNetworkPlayerID),
                                         leActiveRaceCarIndex, lPosition, lAt,
                                         lpChangeNetworkCarEvent->mCarModelId,
                                         lpChangeNetworkCarEvent->mWheelModelId,
                                         lpChangeNetworkCarEvent->mf18);

        char lacCarName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpChangeNetworkCarEvent->mCarModelId, lacCarName);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "NWCC: Network car in slot " << static_cast<s32>(leActiveRaceCarIndex)
                                       << " changed to " << static_cast<u64>(lpChangeNetworkCarEvent->mCarModelId)
                                       << "(" << lacCarName << ")\n";
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetupNetworkCarAction),
                                GameStateModuleIO::E_ACTION_SETUP_NETWORK_CAR,
                                static_cast<s32>(sizeof(lSetupNetworkCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_STREAMING_COMPLETE:
        ProcessStreamingCompleteEvent(reinterpret_cast<const GameStateModuleIO::StreamingCompleteEvent*>(lpEvent),
                                      lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_GAME_START:
        if (mbWaitForStreaming)
        {
            OnProfileLoaded(lpOutput, lpActionQueue);
            WaitForStreaming(lpActionQueue);
        }
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_INFO_REQUEST:
    {
        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

        GameStateModuleIO::PlayerInfoResponseAction lPlayerInfoResponseAction;
        lPlayerInfoResponseAction.mPlayerInfo.Construct(lpProfile->GetName(), mActivePlayerCarId, 0, 0,
                                                        lpProfile->GetCarCount(), 0, 0);
        if (lpProfile->FindCar(mActivePlayerCarId) == 0)
        {
            lPlayerInfoResponseAction.mbIsCarDamaged = true;
        }
        else
        {
            lPlayerInfoResponseAction.mbIsCarDamaged =
                (lpProfile->GetPlayerBaseDeformAmount(mActivePlayerCarId) != 0.0f);
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerInfoResponseAction),
                                GameStateModuleIO::E_ACTION_PLAYER_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lPlayerInfoResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_UNLOCKED_LIVERY_REQUEST:
    {
        const GameStateModuleIO::UnlockedLiveryRequest* lpUnlockedLiveryEvent =
            reinterpret_cast<const GameStateModuleIO::UnlockedLiveryRequest*>(lpEvent);
        CGS_ASSERT(lpUnlockedLiveryEvent, "lpUnlockedLiveryEvent");

        BrnProgression::DerivedCarArray lDerivedCars;
        lDerivedCars.ConstructColourLiveryList(mpVehicleList, lpUnlockedLiveryEvent->mCgsID);

        // Online, the family is only unlocked when the player owns the car asked about.
        if (!mOnlineCarSelectManager.IsInOnlineCarSelect() ||
            mProgressionManager.IsCarUnlocked(lpUnlockedLiveryEvent->mCgsID))
        {
            mProgressionManager.UnlockDerivedCarCollection(lDerivedCars);
        }

        // Every unlocked member, and online every base car of the family as well.
        GameStateModuleIO::UnlockedLiveryResponseAction lUnlockedLiveryResponseAction;
        lUnlockedLiveryResponseAction.maCars.Clear();
        for (u32 luCar = 0; luCar < lDerivedCars.GetLength(); ++luCar)
        {
            const CgsID lCarId = lDerivedCars.GetItem(luCar);
            if (mProgressionManager.IsCarUnlocked(lCarId) ||
                (mOnlineCarSelectManager.IsInOnlineCarSelect() &&
                 mpVehicleList->GetVehicleFromId(lCarId)->GetParentId() == 0))
            {
                lUnlockedLiveryResponseAction.maCars.Append(lCarId);
            }
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUnlockedLiveryResponseAction),
                                GameStateModuleIO::E_ACTION_UNLOCKED_LIVERY_RESPONSE,
                                static_cast<s32>(sizeof(lUnlockedLiveryResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CAR_SELECTION_REQUEST:
    {
        const u32 KU_NUM_CARS = GameStateModuleIO::CarSelectionResponseAction::KU_MAX_CARS_IN_RESPONSE_ACTION;

        GameStateModuleIO::CarSelectionResponseAction lCarSelectionResponseAction;
        GetListOfPlayerSelectableVehicles(lCarSelectionResponseAction.maCars);
        lCarSelectionResponseAction.mHasBeenDrivenBitArray.UnSetAll();
        lCarSelectionResponseAction.mWreckedArray.UnSetAll();
        lCarSelectionResponseAction.miMaxAvailableCars = mProgressionManager.GetMaxCarCount();

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const s32 liCarCount = static_cast<s32>(lCarSelectionResponseAction.maCars.GetLength());
        for (s32 liCar = 0; liCar < liCarCount; ++liCar)
        {
            const u32 luCar = static_cast<u32>(liCar);

            // Wrecked: a car the profile does not own yet, or one it owns still deformed.
            const BrnProgression::CarData* lpCarData =
                lpProfile->FindCar(lCarSelectionResponseAction.maCars.GetItem(luCar));
            if (lpCarData != 0)
            {
                if (lpCarData->GetUnlockDeformationAmount() > 0.0f)
                {
                    CGS_ASSERT(luCar < KU_NUM_CARS, "Index < Number of bits");
                    lCarSelectionResponseAction.mWreckedArray.SetBit(luCar);
                }
                else
                {
                    CGS_ASSERT(luCar < KU_NUM_CARS, "luIndex < NUMBITS");
                    lCarSelectionResponseAction.mWreckedArray.UnSetBit(luCar);
                }
            }
            else
            {
                CGS_ASSERT(luCar < KU_NUM_CARS, "Index < Number of bits");
                lCarSelectionResponseAction.mWreckedArray.SetBit(luCar);
            }

            // Driven: the livery choice of the car's base car has distance on it. A colour
            // livery is keyed on its parent.
            const BrnResource::VehicleListEntry* lpVehicleListEntry =
                mpVehicleList->GetVehicleFromId(lCarSelectionResponseAction.maCars.GetItem(luCar));
            CGS_ASSERT(lpVehicleListEntry, "lpVehicleListEntry");
            const CgsID lBaseCarId = lpVehicleListEntry->IsLiveryColour()
                                         ? lpVehicleListEntry->GetParentId()
                                         : lCarSelectionResponseAction.maCars.GetItem(luCar);
            const BrnProgression::LiveryData* lpLiveryData = lpProfile->GetChosenLiveryDataForBaseCar(lBaseCarId);
            if (lpLiveryData != 0)
            {
                if (lpLiveryData->mfDistanceDriven > 0.0f)
                {
                    CGS_ASSERT(luCar < KU_NUM_CARS, "Index < Number of bits");
                    lCarSelectionResponseAction.mHasBeenDrivenBitArray.SetBit(luCar);
                }
                else
                {
                    CGS_ASSERT(luCar < KU_NUM_CARS, "luIndex < NUMBITS");
                    lCarSelectionResponseAction.mHasBeenDrivenBitArray.UnSetBit(luCar);
                }
            }
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectionResponseAction),
                                GameStateModuleIO::E_ACTION_CAR_SELECTION_RESPONSE,
                                static_cast<s32>(sizeof(lCarSelectionResponseAction)));

        RequestStreamingForVehicleSelection(
            lpActionQueue,
            mLastActiveRaceCarInterface.GetCarModelId(mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex()));
        break;
    }

    case GameStateModuleIO::E_EVENT_RESET_PROFILE_REQUEST:
        mProgressionManager.GetProfile()->Construct();
        SendSetupPlayerCarEvent(lpActionQueue);
        SendSetUpAllEventStartsMessage(lpOutput);
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
        }
        break;

    case GameStateModuleIO::E_EVENT_CARSELECT_STATE_CHANGED:
    {
        const GameStateModuleIO::CarSelectStateChangedEvent* lpCarSelectStateChangedEvent =
            reinterpret_cast<const GameStateModuleIO::CarSelectStateChangedEvent*>(lpEvent);

        if (lpCarSelectStateChangedEvent->meCarSelectType == GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD)
        {
            CGS_ASSERT(mCarSelectManager.IsInJunkyard(), "mCarSelectManager.IsInJunkyard()");
            switch (lpCarSelectStateChangedEvent->meState)
            {
            case GameStateModuleIO::E_CAR_SELECT_STATE_MODEL:
                mCarSelectManager.StartCarSelectState(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_LIVERY:
                mCarSelectManager.EnterModification(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_EXIT:
                // [DIAG] harness witness, not console code: flow_run marks the junkyard exit on it.
                if (CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "[GameStateModule::ProcessGameEvents case 94] action 4 -> ExitJunkyard" << "\n";
                }
                mCarSelectManager.ExitJunkyard(lpActionQueue);
                break;
            default:
                CGS_ASSERT(false, "Unhandled enumeration");
                break;
            }
        }
        else if (lpCarSelectStateChangedEvent->meCarSelectType == GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START)
        {
            CGS_ASSERT(mOnlineCarSelectManager.IsInOnlineCarSelect(), "mOnlineCarSelectManager.IsInOnlineCarSelect()");
            switch (lpCarSelectStateChangedEvent->meState)
            {
            case GameStateModuleIO::E_CAR_SELECT_STATE_MODEL:
                if (!mOnlineCarSelectManager.GetHostChoiceAndNotHost())
                {
                    mOnlineCarSelectManager.StartCarSelectState(lpActionQueue);
                }
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_LIVERY:
                CGS_ASSERT(mOnlineCarSelectManager.GetHostChoiceAndNotHost() == false,
                           "mOnlineCarSelectManager.GetHostChoiceAndNotHost() == false");
                mOnlineCarSelectManager.EnterModification(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_WAIT_FOR_HOST:
                mOnlineCarSelectManager.EnterWaitForHost(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_ACCEPT_CAR:
                if (!mOnlineCarSelectManager.GetHostChoiceAndNotHost())
                {
                    CGS_ASSERT(!mOnlineCarSelectManager.IsWaitingForStreaming(),
                               "!mOnlineCarSelectManager.IsWaitingForStreaming()");
                    mOnlineCarSelectManager.SubmitFinalCarSelection(lpActionQueue);
                }
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_EXIT:
                mOnlineCarSelectManager.ExitOnlineCarSelect(lpActionQueue);
                break;
            default:
                CGS_ASSERT(false, "Unhandled enumeration");
                break;
            }
        }
        else
        {
            CGS_ASSERT(false, "Invalid Car Select Type\n");
        }
        break;
    }

    case GameStateModuleIO::E_GUI_HAS_STARTED_GAME:
        // [DIAG] harness witness, not console code: the returning-player case orders the junkyard
        // entry against it.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[jyentry] game event 78 (GUI has started game) received; waiting="
                                       << (mbWaitingToPutPlayerInJunkyard ? 1 : 0) << "\n";
        }
        if (mbWaitingToPutPlayerInJunkyard)
        {
            mCarSelectManager.ReallyEnterJunkyardAtStartOfGame(lpActionQueue);
            CGS_ASSERT(mCachedCarSelectChangedAction.mJunkyardId != 0,
                       "mCachedCarSelectChangedAction.mJunkyardId != kCGSID_NULL");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&mCachedCarSelectChangedAction),
                                    GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED,
                                    static_cast<s32>(sizeof(mCachedCarSelectChangedAction)));
            mbWaitingToPutPlayerInJunkyard = false;
        }
        break;
    }
}

} // namespace BrnGameState
