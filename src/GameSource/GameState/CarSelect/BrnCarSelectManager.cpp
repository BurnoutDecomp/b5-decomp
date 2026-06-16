#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGameState
{
// X360 0x823564D0. Initialise the car-select manager to its idle defaults: store the three owning
// pointers, clear the state/timer, null the list + spawn-location pointers + car ids, and seed the
// unlock flags. mCurrentCarToUnlock / mfCarUnlockFadedOutTargetTime are deliberately left
// uninitialised (no store at those offsets in the X360 code). Asserts use the literal file/line the
// build baked in (raw Begin/Fire/End rather than CGS_ASSERT, which would inject __FILE__/__LINE__).
void CarSelectManager::Construct(const TriggerQueryManager* lpTriggerQueryManager,
                                 GameStateModule* lpGameStateModule,
                                 BrnProgression::ProgressionManager* lpProgressionManager)
{
    CGS_ASSERT(lpTriggerQueryManager, "lpTriggerQueryManager");
    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");

    mpGameStateModule.Set(lpGameStateModule);
    mpTriggerQueryManager.Set(lpTriggerQueryManager);
    mpProgressionManager.Set(lpProgressionManager);

    mfStateTimer = 0.0f;
    meState      = E_STATE_NONE;

    mpVehicleList.Set(nullptr);
    mpWheelList.Set(nullptr);

    mJunkyardId = 0;
    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
    {
        maSpawnLocations[lu].Set(nullptr);
    }
    meLastSpawnLocationType = KI_SPAWNLOCATIONTYPE_NONE;

    mStartCarId             = 0;
    mDesiredCarId           = 0;
    mCacheDuringChangeCarId = 0;

    mbWaitingForStreaming = false;
    muUnlockCount         = 0;

    mbNoNormalUnlockCars           = false;
    mbTransitionInRequestStreaming = false;
    mbNeedToTeleportTrick          = false;
    mbInCarModScreen               = false;
    mbShutdownUnlockSequence       = false;
    mbCarUnlockEnabled             = true;
    mbDEBUG_DisableUnlock                = false;
    mbDEBUG_UnlockTrophyCarsForTesting   = false;
    mbDEBUG_UnlockShutdownCarsForTesting = false;
}

// X360 (EnterModification). Enter the car-modification flow; only valid from the car-select or
// request-car-change states (asserts otherwise, with the verbatim baked file/line), then delegates
// to the state set-up helper.
void CarSelectManager::EnterModification(InputBuffer::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(meState == E_STATE_CAR_SELECT || meState == E_STATE_REQUEST_CAR_CHANGE, "CarSelectManager: Need to be in E_STATE_CAR_SELECT or E_STATE_REQUEST_CAR_CHANGE state.");

    StartCarModificationState(lpActionQueue);
}
}
