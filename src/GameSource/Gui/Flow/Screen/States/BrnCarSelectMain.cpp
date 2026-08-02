// ===================================================================================
// BrnGui::CarSelectMain -- out-of-line bodies for the shared car-select screen state.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x824BBC20, GetResourcesToLoad @0x824B55B8, AppendAptComponents @0x824B5380,
//   HandleControllerInput @0x824B5410, UpdateGuiCache @0x824B54A8, SetupCar @0x824B5548,
//   SetupCarNameComponent @0x824C0EB0, TriggerSetupCar @0x824C8E08.
//
// The remaining 8 functions (OnEnter @0x824C8920, OnLeave @0x824C8B78, Update @0x824DC9C0,
// ProcesssIncomingEvents @0x824D73D8, ExitCarSelection @0x824C8CB8, HandleLaunchedEvent
// @0x824C8EF0, HandleLeftGameEvent @0x824C91C8, GetResourcesToLoadForCarSelect @0x824B56C0)
// live in the wave-G partfiles BrnCarSelectMain_wG_01..03.cpp alongside this file. Their
// former blockers were resolved in wave G: the maiEventToObserve[21] / overlay-string rodata
// was dumped from the image (values in _wG_01), the vtable slot map was recovered from the
// dispatch displacements (see the header banner), and the far GuiCache members were carved
// as named members (BrnGuiCache.h +0x4B40/+0x4B4E/+0x4B70).
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
    // The trailing event-id s32 is the DWARF declaration shape (cpp:544); the dispatcher
    // (ProcesssIncomingEvents, event 64) passes it like the other handlers and this body
    // ignores it.
    void CarSelectMain::UpdateGuiCache(const CgsModule::Event* lpCacheEvent, s32 /*liEventType*/)
    {
        // The cache-ready GUI event carries the acquired GuiCache pointer as its leading word.
        // External event blob: its layout is fixed by the event, not a reconstructable C++ class,
        // so the leading pointer is read positionally (X360 lwz r11,0(a2)).
        GuiCache* lpCache = *reinterpret_cast<GuiCache* const*>(lpCacheEvent);
        CGS_ASSERT(lpCache != 0, "lpCacheEvent->mpCachePointer");   // cpp:553

        mpGuiCache = lpCache;

        WorldDataController* lpWorldData = mpGuiCache->PeekWorldDataController();

        // ⚠️ PC-BUILD GUARD (2026-08-02). The console calls the ASSERTING accessor
        // (GetWorldDataController) and dereferences the result directly here.
        // NOTHING ON THIS BUILD POPULATES GuiCache::mpWorldDataController (+0x4064) -- the
        // GUI-side WorldDataController acquisition state machine is unreconstructed; the
        // committed BrnLicenseComponent.cpp already documents the same gap and works around
        // it. Before BrnGui::CarSelectVehicle was re-homed onto this class nothing reached
        // this line, so the null dereference was latent. Remove the guard when the GUI world
        // data lands.
        mpVehicleList = (lpWorldData != 0) ? lpWorldData->GetVehicleList() : 0;
    }

    // ---- SetupCar @ 0x824B5548 ----------------------------------------------------
    // POINTER param per the DWARF declaration (cpp:577) -- CarSelectVehicle overloads it
    // with (const CarSetupInfo*, bool) as a distinct new virtual, pinning the base shape.
    void CarSelectMain::SetupCar(const CarSetupInfo* lpSetupInfo)
    {
        mDesiredSetupInfo = *lpSetupInfo;  // copies mCarId + mbSelectable (X360 two-qword store)

        CGS_ASSERT(mDesiredSetupInfo.mbSelectable, "mDesiredSetupInfo.mbSelectable");   // cpp:600

        mbCarChangeInProgress = true;
    }

    // ---- SetupCarNameComponent @ 0x824C0EB0 ---------------------------------------
    void CarSelectMain::SetupCarNameComponent(CgsID lSelectedCarId)
    {
        typedef CgsLanguage::LanguageManager LM;

        // cpp:880 -- ⚠️ PC-BUILD GUARD (2026-08-02, car-select wave). The console asserts
        // mpVehicleList and then dereferences it; on this build the list is legitimately
        // absent (GuiCache::mpWorldDataController is never populated -- see UpdateGuiCache
        // above) and a dev assert BLOCKS the sim, so the bail replaces it. Restore the
        // assert when the GUI WorldDataController lands.
        if (mpVehicleList == 0)
            return;

        const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lSelectedCarId);

        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
        // cpp:883 -- same PC-BUILD GUARD reasoning as above (the console asserts and then
        // dereferences; an unresolved car id is reachable on this build).
        if (lpVehicleData == 0)
            return;

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

    // ================================================================================
    // The ten base virtuals with no out-of-line X360 symbol (2026-08-02).
    //
    // Until this wave nothing in the tree instantiated a CarSelectMain-derived state, so
    // the class's vtable was never emitted and these ten never had to link. Re-homing
    // BrnGui::CarSelectVehicle onto this class makes the base sub-object real, and MSVC
    // emits CarSelectMain's own vtable alongside it -- so every declared virtual now needs
    // a definition. None of them is dispatched on a live CarSelectVehicle (it overrides the
    // first six and the last four are empty on the console), and CarSelectMain itself is
    // never instantiated: BrnScreenFlow::Prepare only ever news the derived states.
    //
    //  * The FOUR event handlers below are EMPTY ON THE CONSOLE, proven not assumed: the
    //    derived vtable off_82075470 holds 0x8284CB38 in slots +0x48/+0x4C/+0x50/+0x54, and
    //    0x8284CB38 is a bare `blr` with 193 xrefs -- the image-wide ICF fold of an empty
    //    body, NOT _purecall. Making them `= 0` would route events 564/406/413/414 into
    //    _purecall on this exact screen.
    //  * GetResourcesToLoad() / GetNumberResourcesToLoad() return "no resources", which is
    //    exactly what the base's ATTESTED 2-argument sibling
    //    CarSelectMain::GetResourcesToLoad(ptr, count) @0x824B55B8 writes ({0, 0}).
    //  * IsLoading / PlayMovie / SetupComponents / HandleCarInfoResponseEvent are the
    //    neutral base behaviours (nothing pending, no movie, nothing to build, nothing to
    //    adopt). FLAG: the X360 emitted no body for these four -- the DWARF places them at
    //    cpp:954 / h:136 / cpp:511 / cpp:563 in the PS3 build, where the derived states also
    //    own the real work. Replace them if a CarSelectMain-derived state ever needs a
    //    non-trivial base default.
    // ================================================================================

    bool CarSelectMain::IsLoading() const
    {
        return false;
    }

    void CarSelectMain::PlayMovie()
    {
    }

    void CarSelectMain::SetupComponents()
    {
    }

    void CarSelectMain::HandleCarInfoResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                   s32 /*liEventType*/)
    {
    }

    // The four ICF-folded empty overrides (derived vtable slots +0x48..+0x54).
    void CarSelectMain::HandleCarAudioLoadComplete()
    {
    }

    void CarSelectMain::HandlePlayerInfoResponse(const CgsModule::Event* /*lpEvent*/,
                                                  s32 /*liEventType*/)
    {
    }

    void CarSelectMain::HandleUnlockedLiveryResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                           s32 /*liEventType*/)
    {
    }

    void CarSelectMain::HandlePlayerCarColourResponseEvent(const CgsModule::Event* /*lpEvent*/,
                                                            s32 /*liEventType*/)
    {
    }

    // The two 0-argument resource getters GetResourcesToLoadForCarSelect forwards to
    // (X360 slots +0x24 / +0x60).
    CgsGui::sResourceTuple* CarSelectMain::GetResourcesToLoad() const
    {
        return 0;
    }

    u32 CarSelectMain::GetNumberResourcesToLoad() const
    {
        return 0;
    }
}
