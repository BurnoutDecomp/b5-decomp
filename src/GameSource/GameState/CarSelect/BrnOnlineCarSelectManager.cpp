#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"             // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // CgsDev::Log::gpDebugPrint, CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"   // CgsDev::E_PRINTMODE_HEXONCE
#include "GameShared/GameClasses/Containers/CgsArray.h"        // Array<CgsID,128> (the selectable-car list)
#include "GameSource/GameState/BrnGameStateModule.h"            // GameStateModule (car change, streaming, showtime)
#include "GameSource/GameState/BrnGameActions.h"                // the typed action records
#include "GameSource/GameState/Progression/BrnProgressionManager.h" // ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"        // Profile::GetChosenLiveryIdForBaseCar
#include "GameSource/GameState/Progression/BrnDerivedCars.h"    // BrnProgression::DerivedCarArray
#include "SharedClasses/DataLists/VehicleList.h"                // VehicleList::GetVehicleData / GetVehicleFromId
#include "SharedClasses/DataLists/VehicleListEntry.h"           // IsLiveryColour / GetParentId / GetUnlockRank

namespace BrnGameState
{
namespace
{
// Game actions this manager posts whose ids have no enumerator in BrnGameActions.h: the reference
// enumerator plus the car-select band's +5 shift (the immediates the console passes to AddEvent).
const s32 KI_ACTION_CAR_SELECTION_CHANGED_ONLINE = 67;    // E_ACTION_CAR_SELECTION_CHANGED_ONLINE (62), 8 bytes
const s32 KI_ACTION_CAR_SELECT_ONLINE_END        = 83;    // E_ACTION_CAR_SELECT_ONLINE_END (78), 1 byte
const s32 KI_ACTION_PLAYER_RESET_ON_TRACK        = 119;   // E_ACTION_PLAYER_RESET_ON_TRACK (114), 1 byte

// KF_MAX_CAR_SELECT_DURATION: the seconds a player has to pick a car (rodata value 30.0).
const f32 KF_MAX_CAR_SELECT_DURATION = 30.0f;
}

// X360 0x823565C0. Initialise the online car-select manager to its idle defaults: store the two
// owning pointers, clear the internal state, null the list pointers, clear the online flag, zero both
// spawn vectors (the X360 emits two stvx128 register stores at +32 / +48), zero the four car-id slots
// (the asm `li r11,0; std r11,0x40/0x48/0x50/0x58` proves 0), and clear the streaming flag + change-car
// state. The assert uses the literal file/line the build baked in (raw Begin/Fire/End rather than
// CGS_ASSERT, which would inject __FILE__/__LINE__). mfTimeLeftInCarSelect (+24) and miVehicleClassLimit
// are deliberately left uninitialised -- no store at those offsets in the X360 code.
void OnlineCarSelectManager::Construct(GameStateModule* lpGameStateModule,
                                       BrnProgression::ProgressionManager* lpProgressionManager)
{
    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");

    meInternalState = E_INTERNAL_STATE_NONE;
    mpGameStateModule.Set(lpGameStateModule);
    mpProgressionManager.Set(lpProgressionManager);
    mpVehicleList.Set(nullptr);
    mpWheelList.Set(nullptr);
    mbIsInOnlineCarSelect = false;

    mSpawnPosition.SetZero();
    mSpawnDirection.SetZero();

    mStartCarId             = 0;
    mFreeburnCarId          = 0;
    mDesiredCarId           = 0;
    mCacheDuringChangeCarId = 0;

    mbWaitingForStreaming = false;
    meStateOfChangingCars = E_CAR_CHANGE_NONE;
}

// X360 0x8238EEA0. Enter the car-modification flow; only valid from the internal car-select state
// (asserts otherwise), then delegates to the modification state set-up helper. The X360 builds the
// assert message into a StrStream buffer; since the text is a plain string literal it is passed
// directly as the CGS_ASSERT message.
void OnlineCarSelectManager::EnterModification(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(meInternalState == E_INTERNAL_STATE_CAR_SELECT, "OnlineCarSelectManager: Need to be in E_INTERNAL_STATE_CAR_SELECT state.");

    StartCarModificationState(lpActionQueue);
}

// X360 0x82356650. Transition into the wait-for-host-to-choose state; only valid from the internal
// car-select state (asserts otherwise, verbatim baked file/line BrnOnlineCarSelectManager.h:354).
// The action-queue parameter is part of the DWARF shape but unused by the body (the X360 Hex-Rays
// elided it), so it is left unnamed-but-present.
void OnlineCarSelectManager::EnterWaitForHost(GameStateModuleIO::GameActionQueue* /*lpActionQueue*/)
{
    CGS_ASSERT(meInternalState == E_INTERNAL_STATE_CAR_SELECT, "OnlineCarSelectManager: Need to be in E_INTERNAL_STATE_CAR_SELECT state.");

    meInternalState = E_INTERNAL_STATE_WAIT_FOR_HOST_TO_CHOOSE;
}

// X360 0x82358AC8. Streaming-complete callback: when the car that finished streaming is the one we
// were waiting on (mDesiredCarId), emit the filter-gated debug line and clear the wait flag. The
// X360 does a full 64-bit cmpld of mDesiredCarId (+80) against the incoming id; modelled here as a
// logical CgsID equality on the named member. The debug spew is gated on
// CgsDev::Message::gxMessageFilterFlags bit 0 and streamed through the committed
// CgsDev::Log::gpDebugPrint (string-only operator<< chain).
void OnlineCarSelectManager::StreamingFinished(CgsID lActiveCarZeroId,
                                               GameStateModuleIO::GameActionQueue* /*lpActionQueue*/)
{
    if (mDesiredCarId == lActiveCarZeroId)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "StreamingFinished\n" << "\n";
        }
        mbWaitingForStreaming = false;
    }
}

// The player car the game state currently has in the world.
void OnlineCarSelectManager::GetCurrentPlayerVehicle(CgsID& lrCarID) const
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "GetCurrentPlayerVehicle\n" << "\n";
    }

    lrCarID = mpGameStateModule.Get()->GetActivePlayerCarId();
}

// Enter online car select from the idle state. In the console's order: remember the class limit and
// the host-choice flag, start the selection clock, hand the player car back to the entity module
// (action 7), stop boost earning and boosting (70 / 71), keep the spawn pose, and in a showtime game
// mode post the reset-on-track request (119). The start car is the car in the world; when neither it
// nor (for a colour livery) its parent is selectable under the class limit, the first selectable car
// of exactly the class limit, in the profile's chosen livery, replaces it. Then the car-select state.
void OnlineCarSelectManager::EnterOnlineCarSelect(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                  const Vector3 lPlayerPosition,
                                                  const Vector3 lPlayerDirection,
                                                  s32 liVehicleClassLimit, bool lbHostChoiceAndNotHost)
{
    Array<CgsID, 128> lCars;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "EnterOnlineCarSelect\n" << "\n";
    }

    CGS_ASSERT(meInternalState == E_INTERNAL_STATE_NONE, "OnlineCarSelectManager: State is invalid");

    mbHostChoiceAndNotHost = lbHostChoiceAndNotHost;
    miVehicleClassLimit    = liVehicleClassLimit;

    // The console writes only the control word and the drive-thru byte of this record.
    GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction = {};
    lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_NONE;
    lSetPlayerCarDriverAction.mbIsDriveThru = false;

    mfTimeLeftInCarSelect = KF_MAX_CAR_SELECT_DURATION;
    mbIsInOnlineCarSelect = true;

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                            GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                            sizeof(lSetPlayerCarDriverAction));

    GameStateModuleIO::AllowBoostEarningAction lAllowBoostEarningAction;
    lAllowBoostEarningAction.mbAllowBoostEarning = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllowBoostEarningAction),
                            GameStateModuleIO::E_ACTION_ALLOW_BOOST_EARNING,
                            sizeof(lAllowBoostEarningAction));

    // The stop-boosting and reset-on-track records carry no payload; the console posts the same
    // zero byte it just posted for boost earning.
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllowBoostEarningAction),
                            GameStateModuleIO::E_ACTION_STOP_BOOSTING, 1);

    mSpawnPosition  = lPlayerPosition;
    mSpawnDirection = lPlayerDirection;

    if (mpGameStateModule.Get()->IsShowtimeGameMode())
    {
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllowBoostEarningAction),
                                KI_ACTION_PLAYER_RESET_ON_TRACK, 1);
    }

    GetCurrentPlayerVehicle(mStartCarId);
    mFreeburnCarId = mStartCarId;

    lCars.Construct();
    mpGameStateModule.Get()->GetListOfPlayerSelectableVehicles(lCars);

    if (lCars.FindFirstInstanceOf(mStartCarId) == -1)
    {
        const BrnResource::VehicleListEntry* lpCurrentVehicleData = mpVehicleList.Get()->GetVehicleData(mStartCarId);
        CGS_ASSERT(lpCurrentVehicleData != 0, "lpCurrentVehicleData != NULL");

        bool lbParentIsSelectable = false;
        if (lpCurrentVehicleData->IsLiveryColour())
        {
            CGS_ASSERT(lpCurrentVehicleData->GetParentId() != 0,
                       "lpCurrentVehicleData->GetParentId() != kCGSID_NULL");
            const CgsID lParentId = lpCurrentVehicleData->GetParentId();
            lbParentIsSelectable = lCars.FindFirstInstanceOf(lParentId) != -1;
        }

        if (!lbParentIsSelectable)
        {
            const BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
            CGS_ASSERT(lpProfile, "lpProfile");

            for (u32 luCar = 0; luCar < lCars.GetLength(); ++luCar)
            {
                const BrnResource::VehicleListEntry* lpVehicleData =
                    mpVehicleList.Get()->GetVehicleData(lCars[luCar]);
                if (lpVehicleData->GetUnlockRank() == miVehicleClassLimit)
                {
                    mStartCarId = lpProfile->GetChosenLiveryIdForBaseCar(lCars[luCar]);
                    CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");
                    mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(mStartCarId, 0, lpActionQueue, false);
                    break;
                }
            }
        }
    }

    StartCarSelectState(lpActionQueue);
}

// Enter the livery screen from online car select: the car-modification state, the modification
// screen action (76) and, when the desired car has more than one livery version, the streaming
// request (69) for every version, base car first with the wait-for-streaming flags.
void OnlineCarSelectManager::StartCarModificationState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "StartCarModificationState\n" << "\n";
    }

    CGS_ASSERT(meInternalState == E_INTERNAL_STATE_CAR_SELECT,
               "OnlineCarSelectManager: State needs to be in E_INTERNAL_STATE_CAR_SELECT.");

    meInternalState = E_INTERNAL_STATE_CAR_MODIFICATION;

    if (mDesiredCarId == 0)
    {
        mDesiredCarId = mStartCarId;
    }
    CGS_ASSERT(mDesiredCarId != 0, "mDesiredCarId != kCGSID_NULL");

    GameStateModuleIO::CarSelectModificationScreen lModScreenAction;
    lModScreenAction.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START;
    lModScreenAction.mbEntering      = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lModScreenAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_MODIFICATION_SCREEN,
                            sizeof(lModScreenAction));

    BrnProgression::DerivedCarArray lDerivedCarArray;
    lDerivedCarArray.ConstructColourLiveryList(mpVehicleList.Get(), mDesiredCarId);

    // The console streams "We weren't expecting " << n << " livery versions." into the message.
    CGS_ASSERT(lDerivedCarArray.GetLength() <= BrnProgression::KU_MAX_AMOUNT_OF_DERIVED_CARS,
               "We weren't expecting ");

    lDerivedCarArray.DEBUG_PrintArray();

    if (lDerivedCarArray.GetLength() > 1)
    {
        GameStateModuleIO::CarSelectionRequestStreamingAction lRequestStreaming = {};
        lRequestStreaming.miCount = 0;
        for (u32 luCarIndex = 0; luCarIndex < lDerivedCarArray.GetLength(); ++luCarIndex)
        {
            lRequestStreaming.maCars[luCarIndex]            = lDerivedCarArray.GetItem(luCarIndex);
            lRequestStreaming.maiPriorities[luCarIndex]     = 1;
            lRequestStreaming.mauExtraInfoFlags[luCarIndex] = 0;
            ++lRequestStreaming.miCount;
        }
        lRequestStreaming.mauExtraInfoFlags[0] = 5;

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequestStreaming),
                                GameStateModuleIO::E_ACTION_CAR_SELECTION_REQUEST_STREAMING,
                                sizeof(lRequestStreaming));
    }
}

// Stop choosing and wait for the session: once only, the wait-for-online state and the online
// end action (83). The console posts that one-byte record without writing it; it is zero here.
void OnlineCarSelectManager::StartWaitForOnline(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (meInternalState == E_INTERNAL_STATE_WAIT_FOR_ONLINE)
    {
        return;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "StartWaitForOnline\n" << "\n";
    }

    meInternalState = E_INTERNAL_STATE_WAIT_FOR_ONLINE;

    const u8 lu8OnlineEnd = 0;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8OnlineEnd),
                            KI_ACTION_CAR_SELECT_ONLINE_END, sizeof(lu8OnlineEnd));
}

// A car pick from the carousel. While a change is being carried out the pick is cached for
// UpdateChangeCarState. In the livery screen the pick is the livery itself (saved as the chosen
// livery); otherwise it is the base car (the parent of a colour livery while waiting for the host)
// in the profile's chosen livery. Either way the change is requested.
void OnlineCarSelectManager::RequestChangeCar(GameStateModuleIO::GameActionQueue* /*lpActionQueue*/,
                                              const CgsID& lCardId)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "RequestChangeCar\n" << "\n";
    }

    CGS_ASSERT(lCardId != 0, "lCardId != kCGSID_NULL");

    if (meStateOfChangingCars == E_CAR_CHANGE_BUSY)
    {
        mCacheDuringChangeCarId = lCardId;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "Busy changing car\n" << "\n";
        }
        return;
    }

    const BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    CGS_ASSERT(lpProfile, "lpProfile");

    if (meInternalState == E_INTERNAL_STATE_CAR_MODIFICATION)
    {
        SaveChosenLiveryForCar(lCardId);
        mDesiredCarId = lCardId;
    }
    else
    {
        CgsID lBaseCarID = lCardId;
        if (meInternalState == E_INTERNAL_STATE_WAIT_FOR_HOST_TO_CHOOSE)
        {
            const BrnResource::VehicleListEntry* lpVehicleListEntry =
                mpVehicleList.Get()->GetVehicleFromId(lBaseCarID);
            CGS_ASSERT(lpVehicleListEntry, "lpVehicleListEntry");
            if (lpVehicleListEntry->IsLiveryColour())
            {
                lBaseCarID = lpVehicleListEntry->GetParentId();
            }
        }
        mDesiredCarId = lpProfile->GetChosenLiveryIdForBaseCar(lBaseCarID);
    }
    meStateOfChangingCars = E_CAR_CHANGE_REQUEST;
}

// The player's final choice: wait for the session and send the pick as final.
void OnlineCarSelectManager::SubmitFinalCarSelection(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "SubmitFinalCarSelection\n" << "\n";
    }

    StartWaitForOnline(lpActionQueue);
    SendOnlineChangeCarAction(lpActionQueue, true);
}

// A requested change, once nothing is streaming: in car select pre-stream the new car, teleport
// the current vehicle out, and mark the change busy and the streamer pending.
void OnlineCarSelectManager::UpdateRequestCarChangeState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(meStateOfChangingCars == E_CAR_CHANGE_REQUEST,
               "CarSelectManager: State needs to be in E_STATE_REQUEST_CAR_CHANGE.");

    if (mbWaitingForStreaming)
    {
        return;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "New car : " << CgsDev::E_PRINTMODE_HEXONCE
                                   << static_cast<u64>(mDesiredCarId) << "\n" << "\n";
    }

    if (meInternalState == E_INTERNAL_STATE_CAR_SELECT)
    {
        mpGameStateModule.Get()->RequestStreamingForVehicleSelection(lpActionQueue, mDesiredCarId);
    }

    TeleportCurrentVehicle(lpActionQueue);

    meStateOfChangingCars = E_CAR_CHANGE_BUSY;
    mbWaitingForStreaming = true;
}

// A busy change, once the new car has streamed: tell the session (84) and the GUI (67) about the
// car, put it in the world, and leave the busy state. A player choosing for himself then either
// carries out the pick cached during the change or, with the clock run out, waits for the session.
void OnlineCarSelectManager::UpdateChangeCarState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(meStateOfChangingCars == E_CAR_CHANGE_BUSY,
               "CarSelectManager: State needs to be in E_INTERNAL_STATE_BUSY_CHANGING_CAR.");

    if (mbWaitingForStreaming)
    {
        return;
    }

    SendOnlineChangeCarAction(lpActionQueue, mbHostChoiceAndNotHost);

    const CgsID lCarSelectOnlineAction = mDesiredCarId;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectOnlineAction),
                            KI_ACTION_CAR_SELECTION_CHANGED_ONLINE, sizeof(lCarSelectOnlineAction));

    CGS_ASSERT(mpGameStateModule.Get(), "mpGameStateModule");
    mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(mDesiredCarId, 0, lpActionQueue, false);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "Changing Car then back to internal state : "
                                   << static_cast<s32>(meInternalState) << "\n" << "\n";
    }

    meStateOfChangingCars = E_CAR_CHANGE_NONE;

    if (mbHostChoiceAndNotHost)
    {
        return;
    }

    if (mfTimeLeftInCarSelect > 0.0f)
    {
        if (mCacheDuringChangeCarId != 0)
        {
            RequestChangeCar(lpActionQueue, mCacheDuringChangeCarId);
            mCacheDuringChangeCarId = 0;
        }
    }
    else
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        {
            *CgsDev::Log::gpDebugPrint << "RG :: CS Manager : " << "TIMEOUT" << "\n";
        }
        StartWaitForOnline(lpActionQueue);
    }
}
}
