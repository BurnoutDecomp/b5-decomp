// GameStateModule -- the junkyard car carousel's selectable-car list and its pre-stream request.
//
//   GetListOfPlayerSelectableVehicles    -- every car the player may pick right now (offline: the
//                                           owned race cars, sponsor cars once the progression rank
//                                           reaches their unlock rank; online: the cars inside the
//                                           event's vehicle-class limit).
//   RequestStreamingForVehicleSelection  -- finds the shown car in that list and posts game action
//                                           69 (CarSelectionRequestStreamingAction) carrying the car
//                                           plus up to seven neighbours on either side, so the world
//                                           streams the cars the carousel can reach next.
//
// Action 69's consumer is RaceCarEntityModule::HandleSelectionRequestStreamingAction: it keeps the
// slots that already hold a requested model, frees the rest, and hands each missing model to the
// race-car streamer with the request's per-entry priority.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameSource/GameState/BrnGameActions.h"                       // CarSelectionRequestStreamingAction (action 69)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::IsCarUnlocked / GetProgressionRank
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::FindCar
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"     // CarData::GetUnlockType
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"   // the online car-select limits
#include "GameSource/BurnoutConstants.h"                                // E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "SharedClasses/DataLists/VehicleList.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <stdlib.h>   // getenv (BRN_CARSEL_DIAG)

namespace BrnGameState
{
namespace
{
// [FLAG PC witness] BRN_CARSEL_DIAG=1 prints every action-69 carousel request this file posts
// (first 60). Read once.
bool IsCarSelDiagEnabled()
{
    static const bool sbEnabled = []()
    {
        const char* lpcValue = getenv("BRN_CARSEL_DIAG");
        return lpcValue != 0 && lpcValue[0] != '\0' && lpcValue[0] != '0';
    }();
    return sbEnabled;
}

// [FLAG PC harness lever] BRN_CARSEL_NOPRESTREAM=1 withholds the carousel post below, so a run can
// measure the car-change wait with and without the pre-stream on one exe. Read once.
bool IsCarSelPrestreamWithheld()
{
    static const bool sbWithheld = []()
    {
        const char* lpcValue = getenv("BRN_CARSEL_NOPRESTREAM");
        return lpcValue != 0 && lpcValue[0] != '\0' && lpcValue[0] != '0';
    }();
    return sbWithheld;
}
}

// ============================================================================
// GetListOfPlayerSelectableVehicles
//
// Walks the whole vehicle list. A car is a candidate when its race-vehicle flag (gameplay data
// flags bit 0, read through IsTrophyCar()) is set, it is not a colour livery of another car, and
// -- online -- its vehicle class (+0x99) is inside the event's class limit (or the host chooses
// for everyone). Offline the candidate must be in the profile's garage; a sponsor car also needs
// the progression rank to have reached its unlock rank. Online it must be unlocked (or the host
// chooses). Online, the first race vehicle at exactly the class limit that is a base car (no
// parent) is appended last when it is not already in the list.
// ============================================================================
void GameStateModule::GetListOfPlayerSelectableVehicles(Array<CgsID, 128>& lrCars)
{
    s32  liVehicleClassLimit        = 9;
    bool lbFoundVehicleAtClassLimit = false;

    lrCars.Clear();

    const bool lbIsOnlineCarSelect = mOnlineCarSelectManager.IsInOnlineCarSelect();
    if (lbIsOnlineCarSelect)
    {
        liVehicleClassLimit = mOnlineCarSelectManager.GetVehicleClassLimit();
    }

    for (s32 liCarIndex = 0; liCarIndex < mpVehicleList->GetVehicleCount(); ++liCarIndex)
    {
        const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(liCarIndex);
        const bool lbInclude = !lpVehicleData->IsLiveryColour();

        s32  liVehicleClass;
        bool lbCorrectVehicleClass;
        if (lbIsOnlineCarSelect)
        {
            liVehicleClass        = lpVehicleData->GetUnlockRank();
            lbCorrectVehicleClass = liVehicleClass <= liVehicleClassLimit
                                 || mOnlineCarSelectManager.GetHostChoiceAndNotHost();
        }
        else
        {
            liVehicleClass        = 9;
            lbCorrectVehicleClass = true;
        }

        if (!lpVehicleData->IsTrophyCar() || !lbInclude || !lbCorrectVehicleClass)
        {
            continue;
        }

        const CgsID lCarId = lpVehicleData->GetId();
        if (lbIsOnlineCarSelect)
        {
            if (mProgressionManager.IsCarUnlocked(lCarId) || mOnlineCarSelectManager.GetHostChoiceAndNotHost())
            {
                lrCars.Append(lCarId);
            }
        }
        else
        {
            const BrnProgression::CarData* lpCarData = mProgressionManager.GetProfile()->FindCar(lCarId);
            if (lpCarData != 0)
            {
                if (lpCarData->GetUnlockType() != BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR)
                {
                    lrCars.Append(lCarId);
                }
                else if (mProgressionManager.GetProgressionRank() >= lpVehicleData->GetUnlockRank())
                {
                    lrCars.Append(lCarId);
                }
            }
        }

        // Computed on every included car exactly as the console does; nothing in this build
        // reads it afterwards.
        if (!lbFoundVehicleAtClassLimit && lbIsOnlineCarSelect
            && (mProgressionManager.IsCarUnlocked(lCarId) || mOnlineCarSelectManager.GetHostChoiceAndNotHost()))
        {
            lbFoundVehicleAtClassLimit = (liVehicleClassLimit == liVehicleClass);
        }
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "DONE\n";
    }

    if (lbIsOnlineCarSelect)
    {
        for (s32 liCarIndex = 0; liCarIndex < mpVehicleList->GetVehicleCount(); ++liCarIndex)
        {
            const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(liCarIndex);
            if (lpVehicleData->IsTrophyCar()
                && lpVehicleData->GetUnlockRank() == liVehicleClassLimit
                && lpVehicleData->GetParentId() == 0)
            {
                if (!lrCars.Contains(lpVehicleData->GetId()))
                {
                    lrCars.Append(lpVehicleData->GetId());
                }
                break;
            }
        }
    }
}

// ============================================================================
// RequestStreamingForVehicleSelection
//
// Request layout (action 69): entry 0 is the shown car (flags 5, priority 0). The next car in
// the list goes to entry 1 (flags 2, priority 1); when there are cars behind the shown one,
// entry 2 is kept for the first of them and the cars ahead are capped at four. The cars ahead
// carry flags 2, the cars behind flags 0; every neighbour's priority is its distance from the
// shown car. At most KI_MAX_ACTIVE_RACE_CARS (8) entries.
// ============================================================================
void GameStateModule::RequestStreamingForVehicleSelection(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                          CgsID lCurrentVehicle)
{
    Array<CgsID, 128> lCars;
    lCars.Construct();
    GetListOfPlayerSelectableVehicles(lCars);

    if (lCars.GetLength() < 2)
    {
        return;
    }

    GameStateModuleIO::CarSelectionRequestStreamingAction lRequestAction = {};
    lRequestAction.maCars[0]            = lCurrentVehicle;
    s32 liCount                         = 1;
    lRequestAction.maiPriorities[0]     = 0;
    lRequestAction.mauExtraInfoFlags[0] = 5;

    s32 liCurrentVehicleIndex = lCars.FindFirstInstanceOf(lCurrentVehicle);
    if (liCurrentVehicleIndex == -1)
    {
        // A colour livery is not in the list itself: look up its parent car instead.
        const BrnResource::VehicleListEntry* lpCurrentVehicleData = mpVehicleList->GetVehicleData(lCurrentVehicle);
        CGS_ASSERT(lpCurrentVehicleData != 0, "lpCurrentVehicleData != NULL");
        if (lpCurrentVehicleData == 0)
        {
            return;   // [PC GUARD] the console reads the entry straight after the assert
        }
        if (lpCurrentVehicleData->IsLiveryColour())
        {
            CGS_ASSERT(lpCurrentVehicleData->GetParentId() != 0,
                       "lpCurrentVehicleData->GetParentId() != kCGSID_NULL");
            liCurrentVehicleIndex = lCars.FindFirstInstanceOf(lpCurrentVehicleData->GetParentId());
        }
    }

    // The console streams "<id> was not found in the vehicle list" into the assert buffer.
    char lacUncompressedID[KI_CGSID_STRING_LEN];
    CgsIDUnCompress(lCurrentVehicle, lacUncompressedID);
    CGS_ASSERT(liCurrentVehicleIndex != -1, " was not found in the vehicle list");
    if (liCurrentVehicleIndex == -1)
    {
        return;   // [PC GUARD] the console walks on with index -1 and reads past the list
    }

    const u32 luCurrentVehicleIndex = static_cast<u32>(liCurrentVehicleIndex);
    const u32 luMaxNeighbours       = E_ACTIVE_RACE_CAR_INDEX_COUNT - 1;
    const u32 luMaxCountPerSide     = E_ACTIVE_RACE_CAR_INDEX_COUNT / 2;

    u32 luNextCount = 0;
    if (lCars.GetLength() != 0)
    {
        luNextCount = lCars.GetLength() - luCurrentVehicleIndex - 1;
        if (luNextCount >= luMaxNeighbours)
        {
            luNextCount = luMaxNeighbours;
        }
    }
    u32 luPrevCount = luCurrentVehicleIndex;
    if (luPrevCount >= luMaxNeighbours)
    {
        luPrevCount = luMaxNeighbours;
    }

    if (luNextCount != 0)
    {
        lRequestAction.maCars[1]            = lCars.GetItem(luCurrentVehicleIndex + 1);
        lRequestAction.maiPriorities[1]     = 1;
        lRequestAction.mauExtraInfoFlags[1] = 2;
        liCount = 2;

        u32 luSlot = 2;
        if (luPrevCount != 0)
        {
            if (luNextCount >= luMaxCountPerSide)
            {
                luNextCount = luMaxCountPerSide;
            }
            luSlot = 3;   // entry 2 belongs to the first car behind
        }

        for (u32 luIndex = 2; luIndex <= luNextCount; ++luIndex, ++luSlot)
        {
            ++liCount;
            lRequestAction.maCars[luSlot]            = lCars.GetItem(luCurrentVehicleIndex + luIndex);
            lRequestAction.mauExtraInfoFlags[luSlot] = 2;
            lRequestAction.maiPriorities[luSlot]     = static_cast<s8>(luIndex);
            CGS_ASSERT(liCount <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "liCount <= BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
        }
    }

    if (luPrevCount != 0)
    {
        const u32 luSlot = (luNextCount != 0) ? 2 : 1;
        lRequestAction.maCars[luSlot]            = lCars.GetItem(luCurrentVehicleIndex - 1);
        lRequestAction.mauExtraInfoFlags[luSlot] = 0;
        lRequestAction.maiPriorities[luSlot]     = 1;
        ++liCount;

        if (luPrevCount > 1)
        {
            u32 luIndex = 1;
            do
            {
                lRequestAction.maCars[liCount] = lCars.GetItem(luCurrentVehicleIndex - luIndex - 1);
                ++luIndex;
                lRequestAction.mauExtraInfoFlags[liCount] = 0;
                lRequestAction.maiPriorities[liCount]     = static_cast<s8>(luIndex);
                ++liCount;
            } while (liCount != E_ACTIVE_RACE_CAR_INDEX_COUNT && luIndex < luPrevCount);
        }
    }

    lRequestAction.miCount = liCount;
    CGS_ASSERT(liCount <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "liCount <= BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    if (IsCarSelDiagEnabled() && CgsDev::Log::gpDebugPrint != 0)
    {
        static s32 siLines = 0;
        if (siLines < 60)
        {
            ++siLines;
            *CgsDev::Log::gpDebugPrint << "[FLAG PC witness] [carsel] carousel action69 car=" << lacUncompressedID
                                       << " listIndex=" << liCurrentVehicleIndex
                                       << " selectable=" << lCars.GetLength()
                                       << " count=" << liCount << " entries:";
            for (s32 liEntry = 0; liEntry < liCount; ++liEntry)
            {
                char lacEntryId[KI_CGSID_STRING_LEN];
                CgsIDConvertToString(lRequestAction.maCars[liEntry], lacEntryId);
                *CgsDev::Log::gpDebugPrint << " [" << liEntry << "]" << lacEntryId
                                           << "/f" << static_cast<s32>(lRequestAction.mauExtraInfoFlags[liEntry])
                                           << "/p" << static_cast<s32>(lRequestAction.maiPriorities[liEntry]);
            }
            *CgsDev::Log::gpDebugPrint << "\n";
        }
    }

    if (IsCarSelPrestreamWithheld())
    {
        return;
    }

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequestAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_REQUEST_STREAMING,
                            sizeof(lRequestAction));
}
}
