// BrnTriggerQueryManager_Prepare.cpp
//   BrnGameState::TriggerQueryManager::Prepare  @ 0x82398218
//
// ⭐ THIS IS THE CONSOLE'S TRIGGERS.DAT LOADER -- the one thing the whole junkyard/car-select
// chain was waiting on. `CarSelectManager::SetupSpawnLocations` walks
// `mpTriggerQueryManager->GetTriggerData()->GetSpawnLocation(i)`, and until this state machine
// runs that ResourcePtr is null, so `maSpawnLocations` stays empty and
// `SpawnInStartCar`'s `maSpawnLocations[1].Get()->GetType()` is a null deref.
//
// SIBLING TU (not BrnTriggerQueryManager.cpp) ON PURPOSE, and it is MEASURED: mounting the
// owning TU costs 13 unresolved externals, all of them pulled in by UpdateTriggers /
// ProcessPlayerTriggers (RoadRulesManager::IsRoadLimitRegionValid + OnRoadLimit,
// DriveThruManager::HandleDriveThru, StuntManager::LatchJumpElement,
// TriggerManagementInputInterface::AddTriggerRegion/RemoveTrigger, Killzone::GetTrigger,
// GenericRegion::GetGroupId, BoxRegion::ComputeDirection and three
// RCEntityActiveRaceCarOutputInterface accessors) -- none of which Prepare touches. Splitting
// the body out costs ZERO. Established repo pattern (BrnCarSelectManager_CarChange.cpp,
// BrnGameStateStreetManager_wB_01.cpp, BrnChallengeManager_wC_04.cpp). Fold this back into the
// owning TU when those 13 land.
//
// GROUND TRUTH: the X360 ARTIST asm at 0x82398218 (the jump table at 0x82398280 has exactly 7
// cases). Every baked assert string + line below is read off that body; the source file the
// console stamps is
//   d:\p4\b5_main\burnout\main\code\gamesource\unity\../GameState/TriggerQueryManager/BrnTriggerQueryManager.cpp
//
// ⚠️ The Hex-Rays rendering of case 2 shows `CgsResource::ID::HashString("TriggerData") |
// 0x500000000LL`. That is the KNOWN fusion artifact, not the id: the asm has two INDEPENDENT
// stores into the stack record --
//     0x82398324  std  r11, var_50(r1)   ; r11 = HashString("TriggerData")  -> mResourceId @+0x10
//     0x82398328  stw  r10, var_58(r1)   ; r10 = 5                          -> miPoolId    @+0x08
// -- and HashString @0x828D84A8 ends `clrldi r3,r11,32`, so the stored id is the plain
// zero-extended 32-bit CRC. Same artifact family already corrected in
// BrnGameDataRequestQueueImpl.h:312 and BrnGameDataModule.cpp's two GET handlers.

#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert Begin/Fire/End
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // [diagnostic] gpDebugPrint
#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetResourceRequestInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"         // RequestInterface<3072>::LoadBundle/AcquireResource/LoadTrafficLanes
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"               // GetGameDataEvent (the traffic-lane reply)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"   // AcquireResourceResponse
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle

namespace BrnGameState
{
namespace
{
    // The console's baked __FILE__ for this TU's asserts.
    const char* const KAC_TQM_FILE =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\"
        "../GameState/TriggerQueryManager/BrnTriggerQueryManager.cpp";

    // The bundle + resource names, read straight out of the asm (`aTriggersDat` @0x823982A8,
    // `aTriggerdata` @0x823982F8). Both are the console's exact spellings.
    const char* const KPC_TRIGGERS_FILE_NAME     = "Triggers.dat";
    const char* const KPC_TRIGGER_RESOURCE_NAME  = "TriggerData";

    // The X360 immediates. The trigger legs use event id 1 (`li r28,1` at the top, reused as
    // both the LoadBundle and the acquire event id); the traffic leg uses 2 (`li r5,2`), which
    // is also what its reply is asserted against. Pool 5 == the "GameData" pool.
    const s32 KI_TRIGGER_EVENT_ID = 1;
    const s32 KI_TRAFFIC_EVENT_ID = 2;
    const s32 KI_LANE_DATA_POOL_ID = 5;

    // The GameData response id the traffic-lane GET answers with
    // (BrnGameDataModule::ProcessGetTrafficLanesRequest stages 55 at its slot); the console
    // compares the received event's TYPE word against `0x37` at 0x82398454.
    const s32 KI_RESPONSE_GET_TRAFFIC_LANES = 55;
}

// ----------------------------------------------------------------------------
// X360 0x82398218.
//
//   0 E_TRIGGER_LOAD_NOT_STARTED    : RequestInterface<3072>::LoadBundle(&rq, 1, 5,
//                                     "Triggers.dat", /*useHDCache*/false); -> 1, fall through
//   1 E_TRIGGER_LOAD_REQUESTED      : reply not in yet -> return false. Otherwise Clear; -> 2,
//                                     fall through
//   2 E_TRIGGER_ACQUIRE_NOT_STARTED : build the 24-byte AcquireResourceRequest
//                                     {mpUser=&rq, miEventId=1, miPoolId=5,
//                                      mResourceId=HashString("TriggerData")}, AddEvent type 4;
//                                     -> 3, RETURN FALSE (the console returns here)
//   3 E_TRIGGER_ACQUIRE_REQUESTED   : reply not in yet -> return false. Walk every queued event:
//                                     BaseResourcePtr::CreateFromHandle(&mpTriggerData,
//                                     &response.mHandle); Construct+Register the debug
//                                     component; zero the three group ids. Clear; -> 4, fall
//                                     through
//   4 E_TRIGGER_LOAD_TRAFFIC        : LoadTrafficLanes(&rq, 2, 5); -> 5, RETURN FALSE
//   5 E_TRIGGER_WFLOAD_TRAFFIC      : exactly one reply expected; assert its type == 55 and its
//                                     miEventId == 2; CreateFromHandle(&mpTrafficData,
//                                     &reply.mHandle); Clear; -> 6; return TRUE
//   6 E_TRIGGER_LOAD_DONE           : return true
//   default                         : assert (the console's message is a copy-paste,
//                                     "AIModule::LoadMapData in a weird state", line 261);
//                                     return false
//
// The X360 falls THROUGH 0->1 and 3->4 (no return between them) and RETURNS after the two
// request-issuing cases (2 and 4), so a full load takes at least three pumps. Reproduced.
// ----------------------------------------------------------------------------
bool TriggerQueryManager::Prepare(GameStateModuleIO::OutputBuffer* lpOutput,
                                  CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue)
{
    if (lpOutput == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutput", KAC_TQM_FILE, 137);
        CgsDev::Assert::EndAssert();
        return false;
    }

    switch (meTriggerLoadStage)
    {
    case E_TRIGGER_LOAD_NOT_STARTED:
        // The console reaches the request interface through the output buffer
        // (`GameStateModuleIO::OutputBuffer::GetResourceRequestInterface()`, the unnamed
        // sub the exports render as `OutputBuffer::` -- it is the +0x3414 member, whose real
        // type is RequestInterface<3072>). The caller holds the WRITE lock for the whole call
        // (GameStateModule::Prepare's LockForWrite), which is exactly what that accessor asserts.
        lpOutput->GetResourceRequestInterface()->LoadBundle(
            lpReceiverQueue, KI_TRIGGER_EVENT_ID, KI_LANE_DATA_POOL_ID,
            KPC_TRIGGERS_FILE_NAME, /*lbUseHDCache*/ false);
        meTriggerLoadStage = E_TRIGGER_LOAD_REQUESTED;
        // fall through -- the X360 drops straight into the poll on the same tick.

    case E_TRIGGER_LOAD_REQUESTED:
        if (lpReceiverQueue->GetCount() == 0)
            return false;
        lpReceiverQueue->Clear();
        meTriggerLoadStage = E_TRIGGER_ACQUIRE_NOT_STARTED;
        // fall through.

    case E_TRIGGER_ACQUIRE_NOT_STARTED:
        // The X360 builds this record inline; AcquireResource is the identical builder
        // (same field order, same AddEvent type 4 / 24-byte console record).
        lpOutput->GetResourceRequestInterface()->AcquireResource(
            lpReceiverQueue, KI_TRIGGER_EVENT_ID, KI_LANE_DATA_POOL_ID,
            KPC_TRIGGER_RESOURCE_NAME);
        meTriggerLoadStage = E_TRIGGER_ACQUIRE_REQUESTED;
        return false;   // X360: `li r3,0` + return, NOT a fall-through

    case E_TRIGGER_ACQUIRE_REQUESTED:
    {
        if (lpReceiverQueue->GetCount() == 0)
            return false;

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        lpReceiverQueue->GetFirstEvent(&lpEvent, &liSize);

        // The X360 asserts the drained event pointer (the loop is entered only when it is
        // non-null, and the assert is re-fired at the top of every iteration).
        while (lpEvent != 0)
        {
            if (lpEvent == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpAcquire", KAC_TQM_FILE, 188);
                CgsDev::Assert::EndAssert();
            }

            // reinterpret_cast, not static_cast: CgsResource::Events::Event and
            // CgsModule::Event are unrelated roots and the receiver queue hands out the module
            // one. Same idiom as BrnDirectorWorldMap.cpp:183 / BrnGameDataModule.cpp:417.
            const CgsResource::Events::AcquireResourceResponse* lpResponse =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);

            // X360 `ld r11, 0x18(r29)` -- the response's {mpResourceMemory, mpSourceEntry}
            // PAIR *is* a ResourceHandle. Read BY MEMBER, never at the console's literal
            // offset: the host handle is 16 bytes where the console's is 8.
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
            mpTriggerData = lHandle;   // ResourcePtr::operator=(handle) -> CreateFromHandle

            // [deferred] TriggerQueryManagerDebugComponent::Construct(this) +
            // CgsDev::DebugComponent::Register(this) (X360 0x823983C0/0x823983C8). The
            // component is the class's FIRST member (the console passes `this` for both the
            // component and the owner), and its own TU is unmounted -- mounting it drags in the
            // debug-UI option tables and its unbodied RenderWorld override. Debug-menu only; no
            // gameplay effect. Same deferral GameDataModule::Prepare already carries for its
            // two components. DELETE-WHEN BrnTriggerQueryManagerDebugComponent.cpp is mounted.

            // X360 0x823983D4/D8/E4: `std r28, 0x160/0x168/0x170(r31)` with r28 == 0 -- the
            // three player trigger-group ids are cleared every time the trigger data is bound.
            mPlayerSigTakedownGroupID = 0;
            mPlayerSuperJumpGroupID   = 0;
            mPlayerRoadLimitGroupID   = 0;

            const CgsModule::Event* lpNext = 0;
            lpReceiverQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }

        lpReceiverQueue->Clear();
        meTriggerLoadStage = E_TRIGGER_LOAD_TRAFFIC;
    }
    // fall through.

    case E_TRIGGER_LOAD_TRAFFIC:
        lpOutput->GetResourceRequestInterface()->LoadTrafficLanes(
            lpReceiverQueue, KI_TRAFFIC_EVENT_ID, KI_LANE_DATA_POOL_ID);
        meTriggerLoadStage = E_TRIGGER_WFLOAD_TRAFFIC;
        return false;   // X360: `li r3,0` + return

    case E_TRIGGER_WFLOAD_TRAFFIC:
    {
        // The console tests `count == 1` and, when the count is > 1, fires
        // "lpReceiverQueue->GetLength() < 1" (line 243) before returning false; a count of 0
        // just returns false.
        if (lpReceiverQueue->GetCount() != 1)
        {
            if (lpReceiverQueue->GetCount() > 1)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpReceiverQueue->GetLength() < 1", KAC_TQM_FILE, 243);
                CgsDev::Assert::EndAssert();
            }
            return false;
        }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        const s32 liType = lpReceiverQueue->GetFirstEvent(&lpEvent, &liSize);

        if (liType != KI_RESPONSE_GET_TRAFFIC_LANES)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "TriggerQueryManager::Prepare has received a resource with an ID that wasn't requested",
                KAC_TQM_FILE, 233);
            CgsDev::Assert::EndAssert();
        }

        const BrnResource::GameDataIO::GetGameDataEvent* lpAcquire =
            static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>(lpEvent);

        if (lpAcquire->miEventId != KI_TRAFFIC_EVENT_ID)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpAcquire->GetEventId() == 2", KAC_TQM_FILE, 237);
            CgsDev::Assert::EndAssert();
        }

        // X360 `addi r4, r29, 0x20` -- the GameData reply's ResourceHandle. By member.
        mpTrafficData = lpAcquire->mHandle;

        lpReceiverQueue->Clear();
        meTriggerLoadStage = E_TRIGGER_LOAD_DONE;

        // [diagnostic, one-shot] the same shape as WorldMap::LoadData's LOADED line. Delete
        // with the rest of the lane bring-up diagnostics.
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            *CgsDev::Log::gpDebugPrint
                << "[TriggerQueryManager] LOADED -- trigger="
                << (mpTriggerData.HasMemoryResource() ? 1 : 0)
                << " traffic=" << (mpTrafficData.HasMemoryResource() ? 1 : 0) << "\n";
        }
        return true;
    }

    case E_TRIGGER_LOAD_DONE:
        return true;

    default:
        // The console's own copy-paste: this TU's default case carries the AIModule message.
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("AIModule::LoadMapData in a weird state", KAC_TQM_FILE, 261);
        CgsDev::Assert::EndAssert();
        return false;
    }
}
}
