// ===================================================================================
// BrnGui::CarSelectMain -- out-of-line bodies for the shared car-select screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824BBC20, GetResourcesToLoad @0x824B55B8, AppendAptComponents @0x824B5380,
//   HandleControllerInput @0x824B5410, UpdateGuiCache @0x824B54A8, SetupCar @0x824B5548,
//   SetupCarNameComponent @0x824C0EB0, TriggerSetupCar @0x824C8E08.
//
// NOT IN SCOPE (left for a later wave -- each needs an input this packet does not attest):
//   * OnEnter/OnLeave/ExitCarSelection post several opaque GUI out-queue events whose
//     header words are built with overlapping stack stores + the un-recovered
//     maiEventToObserve[21] (dword_82065E80) observed-event id table.
//   * HandleLaunchedEvent/HandleLeftGameEvent index the un-recovered launch-failed /
//     kick-reason overlay-name rodata tables (off_82F26C94 / off_82F26774) -- guessing
//     those dispatch strings would mis-route at runtime.
//   * Update/ProcesssIncomingEvents/GetResourcesToLoadForCarSelect drive virtual self-calls
//     by vtable slot (+0x24/+0x28/+0x30/+0x34/+0x38/+0x60) whose name mapping is not
//     groundable here, plus far GuiCache members not on the committed cache API.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnCarSelectMain.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"              // CgsID / CgsIDConvertToString
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SPrintf
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"        // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"         // CgsGui::GuiEvent<N> / GuiEventQueueLarge
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface out-queue
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // BrnGui::GuiFlow (E_GUIFLOW_SCREEN)
#include "GameSource/Gui/BrnGuiCache.h"                     // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiWorldDataController.h"       // BrnGui::WorldDataController
#include "SharedClasses/DataLists/VehicleList.h"            // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"       // BrnResource::VehicleListEntry

namespace BrnGui
{
    // The car-select trigger event posted to the state interface out-queue (X360 buffer
    // {8, 415, 16} + the selected car id, published on channel 40 as 24 bytes). Modelled as a
    // GuiEvent<415> carrying the id so the AddEvent header/id/size match the asm exactly.
    struct GuiEventTriggerCarSelect : public CgsGui::GuiEvent<415>
    {
        CgsID mCarId;
        explicit GuiEventTriggerCarSelect(CgsID lCarId)
            : CgsGui::GuiEvent<415>(8, 16)
            , mCarId(lCarId)
        {
        }
    };

    // ---- Construct @ 0x824BBC20 ---------------------------------------------------
    void CarSelectMain::Construct(CgsID liId, CgsFsm::ScriptedFsm* lpFsm)
    {
        CGS_ASSERT(lpFsm != 0, "Invalid ScriptedFsm ptr");   // cpp:133

        CgsGui::State::Construct(liId, lpFsm);

        mpGuiCache            = 0;                 // +0x288
        mbCarChangeInProgress = false;            // +0x7C4
        meCurrentState        = E_CARSELECT_INVALID;   // +0x7CC = -1

        mGameDataEventReceiverQueue.Construct();  // +0x7D4 (buffer=+0x18, capacity 256, align 16, Clear)

        mCurrentSetupInfo.mCarId      = static_cast<CgsID>(-1);
        mCurrentSetupInfo.mbSelectable = true;
        mDesiredSetupInfo.mCarId      = static_cast<CgsID>(-1);
        mDesiredSetupInfo.mbSelectable = true;
    }

    // ---- GetResourcesToLoad @ 0x824B55B8 ------------------------------------------
    void CarSelectMain::GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                           u32* lpuNumberOfResources) const
    {
        CGS_ASSERT(lppResourceTuples != 0, "Invalid pointer");    // cpp:935
        CGS_ASSERT(lpuNumberOfResources != 0, "Invalid pointer"); // cpp:936

        *lppResourceTuples    = 0;
        *lpuNumberOfResources = 0;
    }

    // ---- AppendAptComponents @ 0x824B5380 -----------------------------------------
    void CarSelectMain::AppendAptComponents()
    {
        CGS_ASSERT(mpGuiCache, "lpGuiCache");   // cpp:502

        // The three always-present help/logo apt clips (X360 sub_824F87C0 == the name-taking
        // GuiCache::AppendExpectedAptComponent entry, flow 0).
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "ManufacturerLogo_mc");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemContinue_mc");
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, "HelpItemBack_mc");
    }

    // ---- HandleControllerInput @ 0x824B5410 ---------------------------------------
    void CarSelectMain::HandleControllerInput(const CgsModule::Event* lpEvent, s32 /*liController*/)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event in CarSelectMain::HandleControllerInput");  // cpp:535
    }

    // ---- UpdateGuiCache @ 0x824B54A8 ----------------------------------------------
    void CarSelectMain::UpdateGuiCache(const CgsModule::Event* lpCacheEvent)
    {
        // The cache-ready GUI event carries the acquired GuiCache pointer as its leading word.
        // External event blob: its layout is fixed by the event, not a reconstructable C++ class,
        // so the leading pointer is read positionally (X360 lwz r11,0(a2)).
        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpCacheEvent);
        CGS_ASSERT(lpCache != 0, "lpCacheEvent->mpCachePointer");   // cpp:553

        mpGuiCache = lpCache;

        WorldDataController* lpWorldData = mpGuiCache->GetWorldDataController();
        CGS_ASSERT(lpWorldData != 0, "mpWorldDataController");

        mpVehicleList = lpWorldData->GetVehicleList();
    }

    // ---- SetupCar @ 0x824B5548 ----------------------------------------------------
    void CarSelectMain::SetupCar(const CarSetupInfo& lrSetupInfo)
    {
        mDesiredSetupInfo = lrSetupInfo;   // copies mCarId + mbSelectable (X360 two-qword store)

        CGS_ASSERT(mDesiredSetupInfo.mbSelectable, "mDesiredSetupInfo.mbSelectable");   // cpp:600

        mbCarChangeInProgress = true;
    }

    // ---- SetupCarNameComponent @ 0x824C0EB0 ---------------------------------------
    void CarSelectMain::SetupCarNameComponent(CgsID lSelectedCarId)
    {
        typedef CgsLanguage::LanguageManager LM;

        CGS_ASSERT(mpVehicleList, "mpVehicleList");   // cpp:880

        const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lSelectedCarId);

        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        CGS_ASSERT(lpVehicleData, "lpVehicleData");   // cpp:883

        char lacId[16];
        char lacCarText[32];
        char lacManText[80];

        // Car name: a livery variant (finish type 2) or a parentless car uses its own id,
        // otherwise the parent car's id.
        const CgsID lParentId = lpVehicleData->GetParentId();
        const CgsID lCarNameId =
            (lpVehicleData->GetLiveryType() == 2 || lParentId == 0) ? lSelectedCarId : lParentId;

        CgsIDConvertToString(lCarNameId, lacId);
        CgsCore::SPrintf(lacCarText, 31, "CAR_CAPS_%s", lacId);
        lacCarText[31] = 0;
        mCarName.SetLocalisedText(lacCarText, LM::E_FORMAT_ID_LOOKUP);

        // Manufacturer name: always keyed off the parent car's id when the car has one.
        const CgsID lManufacturerId = (lParentId != 0) ? lParentId : lSelectedCarId;

        CgsIDConvertToString(lManufacturerId, lacId);
        CgsCore::SPrintf(lacManText, 31, "CAR_MAN_CAPS_%s", lacId);
        lacManText[31] = 0;
        mManufacturerName.SetLocalisedText(lacManText, LM::E_FORMAT_ID_LOOKUP);
    }

    // ---- TriggerSetupCar @ 0x824C8E08 ---------------------------------------------
    void CarSelectMain::TriggerSetupCar()
    {
        mCurrentSetupInfo = mDesiredSetupInfo;   // commit the pending selection

        if (mCurrentSetupInfo.mbSelectable)
        {
            CGS_ASSERT(mCurrentSetupInfo.mCarId != 0, "Trying to select a car will null id");   // cpp:655

            GuiEventTriggerCarSelect lEvent(mCurrentSetupInfo.mCarId);
            mpStateInterface->GetOutputEventQueue()->AddEvent(&lEvent, 40, 24);
        }
    }
}
