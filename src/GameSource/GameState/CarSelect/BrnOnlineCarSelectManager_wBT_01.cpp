// OnlineCarSelectManager: entering the model carousel and the helpers it posts through.
//
// StartCarSelectState is reached from EnterOnlineCarSelect and from ProcessGameEvents' car-select
// MODEL step. From the modification screen it re-streams the desired car; on first entry it drops
// the player into the start car. Either way the carousel is announced to the GUI (actions 75 and
// 76) and the current pick goes to the network (action 84). TeleportCurrentVehicle and
// SaveChosenLiveryForCar serve the car-change state machine in BrnOnlineCarSelectManager.cpp.

#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameStateModule.h"                     // GameStateModule::RequestStreamingForVehicleSelection
#include "GameSource/GameState/BrnGameActions.h"                         // the posted action records
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::GetCarCount / GetCarData
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"      // BrnProgression::CarData
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // VehicleListEntry::GetLiveryType / GetParentId

namespace BrnGameState
{
namespace
{
// The deformation sent with a car the profile does not own (the start-of-game wreck amount).
const f32 KF_UNOWNED_CAR_DEFORMATION_AMOUNT = 0.85f;
}

void OnlineCarSelectManager::StartCarSelectState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "StartCarSelectState\n" << "\n";
    }

    CGS_ASSERT(!(meInternalState == E_INTERNAL_STATE_COUNT || meInternalState == E_INTERNAL_STATE_WAIT_FOR_ONLINE),
               "OnlineCarSelectManager: Invalid state on entering car select state.");

    if (meInternalState == E_INTERNAL_STATE_CAR_SELECT || meInternalState == E_INTERNAL_STATE_WAIT_FOR_HOST_TO_CHOOSE)
    {
        return;
    }

    if (meInternalState == E_INTERNAL_STATE_CAR_MODIFICATION)
    {
        mpGameStateModule.Get()->RequestStreamingForVehicleSelection(lpActionQueue, mDesiredCarId);
    }
    else
    {
        SpawnInStartCar(lpActionQueue);
        mDesiredCarId = mStartCarId;
    }

    meStateOfChangingCars   = E_CAR_CHANGE_NONE;
    mCacheDuringChangeCarId = 0;
    meInternalState         = E_INTERNAL_STATE_CAR_SELECT;

    s32 liCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liCarSelectType),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_READY,
                            static_cast<s32>(sizeof(liCarSelectType)));

    GameStateModuleIO::CarSelectModificationScreen lModificationScreen;
    lModificationScreen.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
    lModificationScreen.mbEntering      = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lModificationScreen),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_MODIFICATION_SCREEN,
                            static_cast<s32>(sizeof(lModificationScreen)));

    SendOnlineChangeCarAction(lpActionQueue, false);
}

void OnlineCarSelectManager::SpawnInStartCar(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "SpawnInStartCar\n" << "\n";
    }

    // Drop the start car at the spawn point with its default wheels, no scoring slot and the
    // no-deform sentinel, flagged as a car-select car. +0x38 is not written on this path.
    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.mPosition                  = mSpawnPosition;
    lResetPlayerCarAction.mDirection                 = mSpawnDirection;
    lResetPlayerCarAction.mCarModelId                = mStartCarId;
    lResetPlayerCarAction.mWheelModelId              = 0;
    lResetPlayerCarAction.mePlayerScoringIndex       = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
    lResetPlayerCarAction.mfDeformationAmount        = -1.0f;
    lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
    lResetPlayerCarAction.mbInCarSelectScreen        = true;
    lResetPlayerCarAction.mbCarSelectDontStreamAudio = true;
    lResetPlayerCarAction.muReserved0x42             = 0;
    lResetPlayerCarAction.mbKeepResetSection         = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    GameStateModuleIO::CarSelectionChangedOnlineAction lSelectionChangedAction;
    lSelectionChangedAction.mCarId = mStartCarId;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSelectionChangedAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_ONLINE,
                            static_cast<s32>(sizeof(lSelectionChangedAction)));

    s32 liColourIndex;
    s32 liPaletteIndex;
    mpProgressionManager.Get()->GetCarColourAndPalette(mStartCarId, &liColourIndex, &liPaletteIndex);
    GameStateModuleIO::CarSelectChangeColourAction lChangeColourAction;
    lChangeColourAction.muPaletteIndex = static_cast<u32>(liPaletteIndex);
    lChangeColourAction.muColourIndex  = static_cast<u32>(liColourIndex);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeColourAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
                            static_cast<s32>(sizeof(lChangeColourAction)));
}

void OnlineCarSelectManager::SendOnlineChangeCarAction(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                       bool                                lbFinalSelection)
{
    const BrnProgression::CarData* lpCarData = GetProfileCarData(mDesiredCarId);
    const f32 lfDeformationAmount = (lpCarData != 0) ? lpCarData->GetUnlockDeformationAmount()
                                                     : KF_UNOWNED_CAR_DEFORMATION_AMOUNT;

    s32 liColourIndex;
    s32 liPaletteIndex;
    mpProgressionManager.Get()->GetCarColourAndPalette(mDesiredCarId, &liColourIndex, &liPaletteIndex);

    GameStateModuleIO::CarSelectOnlineSelectCarAction lSelectCarAction;
    lSelectCarAction.mCarID                = mDesiredCarId;
    lSelectCarAction.mfDeformationAmount   = lfDeformationAmount;
    lSelectCarAction.mu16ColourIndex       = static_cast<u16>(liColourIndex);
    lSelectCarAction.mu16PaintFinishIndex  = static_cast<u16>(liPaletteIndex);
    lSelectCarAction.mbFinalSelection      = lbFinalSelection;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSelectCarAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_ONLINE_SELECT_CAR,
                            static_cast<s32>(sizeof(lSelectCarAction)));
}

const BrnProgression::CarData* OnlineCarSelectManager::GetProfileCarData(CgsID& lrCarID) const
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "GetProfileCarData\n" << "\n";
    }

    CGS_ASSERT(mpProgressionManager.Get(), "mpProgressionManager");
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");

    const s32 liCarCount = lpProfile->GetCarCount();
    for (s32 liCar = 0; liCar < liCarCount; ++liCar)
    {
        const BrnProgression::CarData* lpCarData = lpProfile->GetCarData(liCar);
        if (lpCarData->GetId() == lrCarID)
        {
            return lpCarData;
        }
    }
    return 0;
}

// Pin lCarId as the chosen livery of its base car on the profile: a livery variant (livery type
// 1, 3 or 4) is keyed under its parent car, anything else under itself.
void OnlineCarSelectManager::SaveChosenLiveryForCar(CgsID lCarId)
{
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList.Get();
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
    CGS_ASSERT(lpVehicleListEntry, "lpVehicleListEntry");

    const u8 luLiveryType = lpVehicleListEntry->GetLiveryType();
    if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
    {
        lpProfile->SetChosenLiveryIdForBaseCar(lpVehicleListEntry->GetParentId(), lCarId);
    }
    else
    {
        lpProfile->SetChosenLiveryIdForBaseCar(lCarId, lCarId);
    }
}

// Re-place the player at the online car-select spawn point in the desired car (with its owned
// deformation, or the unowned-car amount) and send its colour and palette.
void OnlineCarSelectManager::TeleportCurrentVehicle(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    const BrnProgression::CarData* lpCarData = GetProfileCarData(mDesiredCarId);
    const f32 lfDeformationAmount = (lpCarData != 0) ? lpCarData->GetUnlockDeformationAmount()
                                                     : KF_UNOWNED_CAR_DEFORMATION_AMOUNT;

    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.mPosition                  = mSpawnPosition;
    lResetPlayerCarAction.mDirection                 = mSpawnDirection;
    lResetPlayerCarAction.mCarModelId                = mDesiredCarId;
    lResetPlayerCarAction.mWheelModelId              = 0;
    lResetPlayerCarAction.mePlayerScoringIndex       = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
    lResetPlayerCarAction.mfDeformationAmount        = lfDeformationAmount;
    lResetPlayerCarAction.miBaseDeformationType      = 1;
    lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
    lResetPlayerCarAction.mbInCarSelectScreen        = true;
    lResetPlayerCarAction.mbCarSelectDontStreamAudio = true;
    lResetPlayerCarAction.muReserved0x42             = 0;
    lResetPlayerCarAction.mbKeepResetSection         = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    s32 liColourIndex;
    s32 liPaletteIndex;
    mpProgressionManager.Get()->GetCarColourAndPalette(mDesiredCarId, &liColourIndex, &liPaletteIndex);
    GameStateModuleIO::CarSelectChangeColourAction lChangeColourAction;
    lChangeColourAction.muPaletteIndex = static_cast<u32>(liPaletteIndex);
    lChangeColourAction.muColourIndex  = static_cast<u32>(liColourIndex);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeColourAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
                            static_cast<s32>(sizeof(lChangeColourAction)));
}

} // namespace BrnGameState
