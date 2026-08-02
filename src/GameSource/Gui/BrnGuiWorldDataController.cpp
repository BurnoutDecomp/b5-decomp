#include "GameSource/Gui/BrnGuiWorldDataController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT + BeginAssert/FireAssert/EndAssert
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"    // ResourcePtr<T>::operator->
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (not-found diagnostics)
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"            // BrnWorld::GlobalColourPalette / PlayerCarColourPalette
#include "SharedClasses/Progression/BrnProgressionData.h"             // BrnProgression::ProgressionData
#include "SharedClasses/Progression/BrnRaceEventData.h"               // BrnProgression::EventJunction / RaceEventData
#include "SharedClasses/Trigger/BrnTriggerData.h"                     // BrnTrigger::TriggerData
#include "SharedClasses/Trigger/BrnLandmark.h"                        // BrnTrigger::Landmark (GetId / GetRegionIndex)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                   // BrnTrigger::GenericRegion (GetId / GetBoxRegion)
#include "SharedClasses/Trigger/BrnRegion.h"                          // BrnTrigger::BoxRegion (region copy)
#include "GameSource/GameState/BrnGameStateTypes.h"                   // BrnGameState::LandmarkIndex (operator s32)
#include "GameSource/Resource/BrnGameDataModuleIO.h"                  // BrnResource::GameDataIO::InputBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"     // RequestInterface<32768>::GetVehicleList / GetFreeburnChallengeList
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"     // CgsResource::ID::HashString
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // AcquireResourceRequest / AcquireResourceResponse
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"           // GameDataAssetEvent (the GET reply record)

// BrnGui::WorldDataController -- readiness accessors reconstructed from BURNOUT_X360_ARTIST.XEX.
// Each asserts the controller has reached the ready state (the X360 compares meState against 11 /
// WFPLAYERCARCOLOURS; the baked rodata text names READY, reproduced verbatim) then forwards to a
// resource pointer via ResourcePtr<T>::operator-> const. Only the four verified-PASS accessors are
// homed here; GetRequiredWinsInRank + the Prepare acquire state machine land in later waves.

namespace BrnGui
{

// ================================================================================================
// THE ACQUISITION STATE MACHINE (X360 0x82516770) -- what actually populates this object.
//
// Every resource the GUI reads about the world arrives through the GAME DATA request queue, i.e.
// `lpGameDataInput->GetRequestInterface()`, and every reply lands in THIS object's mReceiverQueue
// (each request carries &mReceiverQueue as its reply target). Two request shapes are used:
//
//   * an AcquireResourceRequest (queue id 4, 24 bytes) for the two resources looked up BY NAME
//     HASH -- "TriggerData" (eventId 1) and "CarColours" (eventId 0). GameDataModule::Update's
//     drain loop passes every id < 26 STRAIGHT THROUGH to the resource module's own queue
//     (X360 `if (i >= 26) ProcessGameDataEvent(...) else resourceIn->GetResourceQueue()->
//     AddEvent(...)`), so the pool answers these directly with an AcquireResourceResponse (id 4)
//     whose {mpResourceMemory, mpSourceEntry} pair IS a ResourceHandle -- the console hands
//     `payload + 0x18` straight to BaseResourcePtr::CreateFromHandle.
//   * a typed GameData GET (queue id 49) for the two LIST resources -- RequestInterface<32768>::
//     GetVehicleList (eventId 1) and ::GetFreeburnChallengeList (eventId 1). GameDataModule
//     answers those from its own resident tables (ProcessGetVehicleListRequest replies with
//     &GameDataModule::mVehicleList), and the console reads the pointer out of the reply's
//     mHandle.mpResourceMemory -- the X360 `lwz r11, 0x20(payload) / stw r11, 0x464(this)`.
//     This is the SAME reply GameStateModule::ReceiveListResource consumes for its own
//     mpVehicleList, so both consumers end up holding the identical resident table.
//
// Each stage is resumable: the machine issues its request, advances meState, and returns false
// while its reply has not arrived. The console's caller (GuiModule::Prepare stage 14) re-enters
// it every pass until it returns true.
//
// ⚠️ WHAT ACTUALLY HAPPENS ON THIS BUILD -- CORRECTED 2026-08-02, THE OLD NOTE WAS WRONG TWICE.
// It said the machine settles at PREPARING_ACQUIRING_PROGRESSION (7) because "no PC producer
// answers the CarColours acquire". Boot-measured, it settles at PREPARING_ACQUIRING_STREET_DATA
// (9): the CarColours acquire IS answered (the pool always answers an acquire, even when the
// resource is absent -- it replies with a null memory pointer), so stage 7 advances. The one
// stage that cannot complete is stage 9, waiting on GetFreeburnChallengeList, which
// GameDataModule::ProcessGetGameDataEvent routes to DeferredGameDataRequest ("CL__").
//
// ⛔ AND THE REPLY HANDLER IS DELIBERATELY STILL NOT IMPLEMENTED. The console's
// ProcessGetFreeburnChallengeListRequest @0x82666728 is trivial -- 20 instructions that post
// reply id 53 carrying `&mChallengeList` (the resident table at X360 this+462800), exactly like
// ProcessGetVehicleListRequest. What makes it correct on the console is GameDataModule::Prepare
// STAGE 10 (PrepareFreeburnChallengeList @0x8266C088) having FILLED that table first. Stage 10
// is deferred on this build and there is no challenge-list bundle in build/game to fill it
// from. Writing the reply handler alone would hand every consumer a live-looking, permanently
// EMPTY table -- a plausible-but-wrong answer indistinguishable from "this profile has no
// challenges", i.e. the silent-drop shape this project keeps getting bitten by. Land stage 10
// and the handler together, or not at all.
//
// ⚠️ Parking at 9 costs LESS than the old note implied. It does NOT affect the colour picker
// (see below), and it is not what fires any assert in the current whole-run set. What it does
// gate is the five genuinely meState-gated accessors (landmarks, events, progression), none of
// which is reached on the junkyard -> car-select -> handover path.
//
// ⚠️ AND: "the readiness-gated accessors (landmarks, events, colour palettes) will assert" was
// wrong for the colour palette. GetColourPaletteFromType @0x824BDA40 has NO meState compare --
// only `lType < eNumPalettes`. Its sibling GetProgressionData @0x82428818 DOES gate
// (`cmpwi r11, 0xB`); that is where the confusion came from. GetVehicleList/
// GetFreeburnChallengeList (0x824F3AF0/0x824F3AF8) are bare returns, so THREE accessors are
// ungated, not two.
//
// ⛔ STILL TRUE AND STILL A GAP: nothing in this function ever writes mpProgressionData (+0x444)
// or mpStreetData (+0x488) -- that is the CONSOLE's shape, verified instruction by instruction
// against 0x82516770, not a missing slice. Some other producer fills them; until it lands,
// WorldDataController::GetProgressionData() returns an unbound ResourcePtr. (It is NOT what
// fires the "lpProgressionData != NULL" assert in GameStateModule::OnPlayerCarChange -- that
// one reads ProgressionManager::mpProgressionData, a different object entirely, loaded from
// Progression.dat by ProgressionManager::LoadProgressionData @0x82399ED0.)
// ================================================================================================

// X360-inlined in GuiModule::Construct @0x82518028 (stores at guiModule+307836..+309028).
void WorldDataController::Construct()
{
    meState = E_WORLDDATACONTROLLERSTATE_CONSTRUCTED;   // X360 `*(gm + 307836) = 1`
    mReceiverQueue.Construct();                         // capacity 1024 / alignment 16, then Clear
    miResourceCount  = 0;
    mpVehicleList    = 0;                               // X360 `*(gm + 308960) = 0`
    mpChallengeList  = 0;                               // X360 `*(gm + 309028) = 0`
}

namespace
{
    // The console's two inlined receiver-queue reads, factored so each stage reads the same way it
    // does on the X360: `lpEvent = mReceiverQueue.mpBuffer + miStartOffset + 8` with the two
    // asserts around it. Returns the payload (or NULL on an empty queue).
    const CgsModule::Event* PeekFirstPayload(const CgsModule::BaseEventReceiverQueue& lrQueue)
    {
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        lrQueue.GetFirstEvent(&lpEvent, &liSize);
        return lpEvent;
    }
}

// X360 0x82516770. The resumable acquire machine. Line numbers in the asserts are the console's
// baked BrnGuiWorldDataController.cpp lines (102/110/113/140/147/152/184/188/214/221/226/243).
bool WorldDataController::Prepare(BrnResource::GameDataIO::InputBuffer* lpGameDataInput)
{
    switch (meState)
    {
    case E_WORLDDATACONTROLLERSTATE_CONSTRUCTED:
        meState = E_WORLDDATACONTROLLERSTATE_CONSTRUCTED;
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_TRIGGERS:
    {
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_TRIGGERS;
        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = 1;
        lRequest.miPoolId  = 5;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>("TriggerData")))));
        lpGameDataInput->GetRequestInterface()->mRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 4,
            static_cast<s32>(sizeof(lRequest)));
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_TRIGGERS:
    {
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_TRIGGERS;
        if (mReceiverQueue.GetLength() == 0)
            return false;
        CGS_ASSERT(mReceiverQueue.GetLength() == 1, "1 == mReceiverQueue.GetLength()");   // cpp:102

        const CgsModule::Event* lpEvent = PeekFirstPayload(mReceiverQueue);
        CGS_ASSERT(lpEvent != 0, "NULL != lpEvent");                                      // cpp:110
        const CgsResource::Events::AcquireResourceResponse* lpAcquire =
            reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
        CGS_ASSERT(lpAcquire != 0, "NULL != lpAcquire");                                  // cpp:113
        if (lpAcquire != 0)
        {
            // X360 `CreateFromHandle(this + 0x424, payload + 0x18)`: the response's trailing
            // {mpResourceMemory, mpSourceEntry} pair IS a ResourceHandle (same two members, same
            // order). Spelled through ResourcePtr<T>::operator=(const ResourceHandle&), which is
            // literally that one CreateFromHandle call (the protected base method's public face).
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
            lHandle.mpSourceEntry    = lpAcquire->mpSourceEntry;
            mpTriggerData = lHandle;
        }
        mReceiverQueue.Clear();
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_VEHICLES:
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_VEHICLES;
        lpGameDataInput->GetRequestInterface()->GetVehicleList(&mReceiverQueue, 1);
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_VEHICLES:
    {
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_VEHICLES;
        if (mReceiverQueue.GetLength() == 0)
            return false;
        CGS_ASSERT(mReceiverQueue.GetLength() == 1, "1 == mReceiverQueue.GetLength()");   // cpp:140

        const CgsModule::Event* lpEvent = PeekFirstPayload(mReceiverQueue);
        CGS_ASSERT(lpEvent != 0, "NULL != lpEvent");                                      // cpp:147
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
        // X360 `if (*payload != 1)` -- the echoed miEventId, which GetVehicleList set to 1.
        CGS_ASSERT(lpReply != 0 && lpReply->miEventId == 1, "Invalid event id received\n");  // cpp:152
        if (lpReply != 0)
            mpVehicleList = static_cast<const BrnResource::VehicleList*>(lpReply->mHandle.mpResourceMemory);
        mReceiverQueue.Clear();
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_PROGRESSION:
    {
        miResourceCount = 1;                       // X360 `stw r27, 0x420(r28)` (r27 == 1)
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_PROGRESSION;
        CgsResource::Events::AcquireResourceRequest lRequest;
        lRequest.mpUser    = &mReceiverQueue;
        lRequest.miEventId = 0;                    // X360 `li r10, 0` (NOT the triggers' 1)
        lRequest.miPoolId  = 5;
        lRequest.mResourceId.SetHash(static_cast<u64>(static_cast<u32>(
            CgsResource::ID::HashString(reinterpret_cast<const u8*>("CarColours")))));
        lpGameDataInput->GetRequestInterface()->mRequestQueue.AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), 4,
            static_cast<s32>(sizeof(lRequest)));
        mReceiverQueue.Clear();
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_PROGRESSION:
    {
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_PROGRESSION;
        // X360 `if (mReceiverQueue.GetLength() < miResourceCount) return 0;`
        if (mReceiverQueue.GetLength() < miResourceCount)
            return false;

        const CgsModule::Event* lpEvent = PeekFirstPayload(mReceiverQueue);
        CGS_ASSERT(lpEvent != 0, "NULL != lpEvent");                                      // cpp:184
        const CgsResource::Events::AcquireResourceResponse* lpAcquire =
            reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
        CGS_ASSERT(lpAcquire != 0, "NULL != lpAcquire");                                  // cpp:188
        if (lpAcquire != 0)
        {
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
            lHandle.mpSourceEntry    = lpAcquire->mpSourceEntry;
            mpPlayerCarColours = lHandle;
        }
        mReceiverQueue.Clear();
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_STREET_DATA:
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_FOR_STREET_DATA;
        lpGameDataInput->GetRequestInterface()->GetFreeburnChallengeList(&mReceiverQueue, 1);
        // fall through
    case E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_STREET_DATA:
    {
        meState = E_WORLDDATACONTROLLERSTATE_PREPARING_ACQUIRING_STREET_DATA;
        if (mReceiverQueue.GetLength() == 0)
            return false;
        CGS_ASSERT(mReceiverQueue.GetLength() == 1, "1 == mReceiverQueue.GetLength()");   // cpp:214

        const CgsModule::Event* lpEvent = PeekFirstPayload(mReceiverQueue);
        CGS_ASSERT(lpEvent != 0, "NULL != lpEvent");                                      // cpp:221
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
        CGS_ASSERT(lpReply != 0 && lpReply->miEventId == 1, "Invalid event id received\n");  // cpp:226
        if (lpReply != 0)
            mpChallengeList = static_cast<const ChallengeList*>(lpReply->mHandle.mpResourceMemory);
        mReceiverQueue.Clear();
    }
        // fall through
    case E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS:
        meState = E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS;
        return true;

    default:
        // X360 cpp:243, streamed: "Unhandled state [" << meState << "] in WorldDataController::Prepare"
        CGS_ASSERT(false, "Unhandled state in WorldDataController::Prepare");
        return false;
    }
}

// X360 0x8248E6D8. Total landmark count in the loaded trigger data (TriggerData::miLandmarkCount @+0x34).
s32 WorldDataController::GetTotalNumberOfLandmarks() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpTriggerData->GetNumLandmarks();
}

// X360 0x824286E0. Number of online landmarks currently available (TriggerData::miOnlineLandmarkCount
// @+0x38). The X360 compares meState against 11 (E_..._WFPLAYERCARCOLOURS); the baked assert text
// names READY -- reproduced as-is.
s32 WorldDataController::GetTotalNumberOfOnlineLandmarks() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpTriggerData->GetOnlineLandmarkCount();
}

// X360 0x824BDA40. Returns the lType'th car-colour palette entry from the loaded global
// colour-palette resource. lType indexes maPalettes[4]; the console's own tail
// `slwi r11,r31,1 / add r11,r31,r11 / slwi r11,r11,2 / add r3,r11,r30` == base + 12*type is one
// of the two proofs that PlayerCarColourPalette is the 12-byte SERIALISED record.
// NOTE (verified): this accessor has NO meState gate -- unlike GetProgressionData below.
const BrnWorld::PlayerCarColourPalette*
WorldDataController::GetColourPaletteFromType(BrnWorld::EPalettesTypes lType) const
{
    const BrnWorld::GlobalColourPalette* const lpPalette = mpPlayerCarColours.operator->();
    CGS_ASSERT(lType < BrnWorld::eNumPalettes, "lType < eNumPalettes");
    return &lpPalette->maPalettes[lType];
}

// X360 0x82428818. Accessor for the loaded progression resource. Asserts the controller has
// reached the ready state, then returns the resource pointer (ResourcePtr operator-> const).
const BrnProgression::ProgressionData* WorldDataController::GetProgressionData() const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    return mpProgressionData.operator->();
}

// X360 0x82501270. Linear scan of the loaded trigger data's landmark table for the landmark whose
// region index equals lLandmarkIndex, returning it. On an empty table or a miss it builds a
// diagnostic into a local assert buffer and fires (the X360 streams into the global assert buffer;
// matching the committed BrnTriggerData.cpp precedent, we build into a stack buffer instead --
// behaviourally identical), then returns NULL.
const BrnTrigger::Landmark*
WorldDataController::GetLandmarkInfoFromIndex(BrnGameState::LandmarkIndex lLandmarkIndex) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");
    CGS_ASSERT(static_cast<s32>(lLandmarkIndex) >= 0, "0 <= lLandmarkIndex");

    for (s32 liIndex = 0; liIndex < mpTriggerData->GetLandmarkCount(); ++liIndex)
    {
        if (mpTriggerData->GetLandmark(liIndex)->GetRegionIndex() == static_cast<s32>(lLandmarkIndex))
            return mpTriggerData->GetLandmark(liIndex);
    }

    // Not found: build the diagnostic into a local assert buffer and fire.
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Unable to find landmark with index: " << static_cast<s32>(lLandmarkIndex);
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameSource\\Gui/BrnGuiWorldDataController.cpp",
            414);
        CgsDev::Assert::EndAssert();
    }

    return NULL;
}

// X360 0x82501480. Linear scan of the loaded trigger data's landmark table for the landmark whose
// id equals lLandmarkID, returning it. On an empty table or a miss it builds a diagnostic and fires
// (see GetLandmarkInfoFromIndex on the stack-buffer substitution), then returns NULL.
const BrnTrigger::Landmark*
WorldDataController::GetLandmarkInfoFromID(CgsID lLandmarkID) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    for (s32 liIndex = 0; liIndex < mpTriggerData->GetLandmarkCount(); ++liIndex)
    {
        if (mpTriggerData->GetLandmark(liIndex)->GetId() == lLandmarkID)
            return mpTriggerData->GetLandmark(liIndex);
    }

    // Not found: build the diagnostic into a local assert buffer and fire.
    {
        char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
        lStream << "Unable to find landmark with id: " << lLandmarkID;
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            lacMessageBuffer,
            "..\\..\\..\\GameSource\\Gui/BrnGuiWorldDataController.cpp",
            445);
        CgsDev::Assert::EndAssert();
    }

    return NULL;
}

// X360 0x82501698. Returns the OFFLINE race-event record whose owning junction's id equals
// luEventId. Walks the progression event-junction table (the X360 inlined the junction lookup); on
// no match returns NULL. GetOfflineEvent() == the junction's offline event pointer (+0x04).
const BrnProgression::RaceEventData*
WorldDataController::GetEventInfoFromEventId(u32 luEventId) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    const u32 luJunctionCount = mpProgressionData->GetEventJunctionCount();
    for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
    {
        const BrnProgression::EventJunction* const lpJunction = mpProgressionData->GetEventJunction(luIndex);
        if (lpJunction->GetID() == luEventId)
            return lpJunction->GetOfflineEvent();
    }

    return NULL;
}

// X360 0x82501740. Returns the ONLINE race-event record whose owning junction's id equals
// luEventId. Same junction walk as GetEventInfoFromEventId, returning GetOnlineEvent() (+0x08).
const BrnProgression::RaceEventData*
WorldDataController::GetOnlineEventInfoFromEventId(u32 luEventId) const
{
    CGS_ASSERT(meState >= E_WORLDDATACONTROLLERSTATE_WFPLAYERCARCOLOURS,
        "E_WORLDDATACONTROLLERSTATE_READY <= meState");

    const u32 luJunctionCount = mpProgressionData->GetEventJunctionCount();
    for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
    {
        const BrnProgression::EventJunction* const lpJunction = mpProgressionData->GetEventJunction(luIndex);
        if (lpJunction->GetID() == luEventId)
            return lpJunction->GetOnlineEvent();
    }

    return NULL;
}

// X360 0x82501AC0. Finds the generic region whose id equals lTriggerID and copies its box region
// into *lpRegion. Fires "Can't find any triggers in the data" on an empty table and "requested
// trigger wasn't found" when the scan exhausts without a match. NOTE: unlike the landmark/event
// accessors this one does NOT gate on the ready meState (the X360 asm has no state check here).
void
WorldDataController::GetTriggerVolumeRegion(CgsID lTriggerID, BrnTrigger::BoxRegion* lpRegion) const
{
    s32 liRegionIndex = 0;
    if (mpTriggerData->GetGenericRegionCount() > 0)
    {
        for (;;)
        {
            const BrnTrigger::GenericRegion* const lpGenericRegion =
                mpTriggerData->GetGenericRegion(liRegionIndex);
            if (lpGenericRegion->GetId() == lTriggerID)
            {
                *lpRegion = *lpGenericRegion->GetBoxRegion();
                break;
            }
            ++liRegionIndex;
            if (liRegionIndex >= mpTriggerData->GetGenericRegionCount())
                break;
        }
    }
    else
    {
        CGS_ASSERT(false, "Can't find any triggers in the data\n");
    }

    CGS_ASSERT(liRegionIndex < mpTriggerData->GetGenericRegionCount(), "requested trigger wasn't found");
}

// X360 0x824F3AF0. Accessor for the loaded vehicle-list resource (raw pointer member @X360 +0x464).
const BrnResource::VehicleList* WorldDataController::GetVehicleList() const
{
    return mpVehicleList;
}

// X360 0x824F3AF8. Accessor for the loaded freeburn-challenge-list resource (pointer member @X360 +0x4A8).
const ChallengeList* WorldDataController::GetFreeburnChallengeList() const
{
    return mpChallengeList;
}

}
