#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // [diagnostic] Prepare's per-stage log line
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager::GetCurrentGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode::IsOnline
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::EGameModeType (E_MODE_*_SHOWTIME)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // BrnProgression::Profile::SetCarUnlockAlreadyShown
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::OutputBuffer (owned by pointer)
#include "SharedClasses/DataLists/VehicleList.h"                        // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/WheelList.h"                          // BrnResource::WheelList (GetWheelCount -- Prepare's list diagnostic)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry (parent id / livery + car type / stats)
#include "SharedClasses/Progression/BrnProgressionData.h"               // BrnProgression::ProgressionData::FindCarOpponentSet
#include "SharedClasses/Progression/BrnOpponentData.h"                  // BrnProgression::CarOpponentSet (opponent walk)
#include "GameShared/GameClasses/Containers/CgsArray.h"                 // CgsContainers::Array<s64,7> (opponent payload)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // RequestInterface<3072>::GetVehicleList/GetWheelList
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"             // GameDataAssetEvent (the list replies)
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // BrnTrigger::TriggerData (generic-region table)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // BrnTrigger::GenericRegion (E_TYPE_JUNK_YARD)
#include "SharedClasses/Trigger/BrnRegion.h"                            // BrnTrigger::BoxRegion::GetPosition
#include "SharedClasses/Trigger/BrnSpawnLocation.h"                     // BrnTrigger::SpawnLocation (cross-table check)
#include <cmath>                                                        // std::sqrt (FindNearestJunkyardID)

namespace BrnGameState
{
// The verbatim X360-baked source path every assert in this TU references.
static const char* const KAC_GSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";
static const char* const KAC_OPPONENTDATA_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\progression\\BrnOpponentData.h";

// ----------------------------------------------------------------------------
// X360-attested game-action event-type ids + payload sizes (the `li r5,<type>` / `li r6,<size>`
// immediates each AddEvent is given). The payload structs are GameStateModuleIO action records
// whose field layouts are not in this TU's exports, so each is posted as a sized buffer with the
// bytes the X360 actually writes reproduced at their attested in-payload offsets.
// ----------------------------------------------------------------------------
enum EGsmGameAction
{
    KI_ACTION_PLAYER_CAR_CHANGED   = 1,     // size 8   -- OnSpecialEventPlayerCarChange (the new car id)
    KI_ACTION_CAR_OPPONENT_SET     = 4,     // size 64  -- OnPlayerCarChange (Array<CgsID,7> of opponents)
    KI_ACTION_UNPAUSE              = 87,    // size 1   -- RequestUnpause
    KI_ACTION_APPLY_CAR_STATS      = 198,   // size 24  -- ApplyCarStats
    // ProcessGameEvents case 78 (`li r6,0x40; li r5,0x40` @0x823A45E0). Same id + size as the
    // CarSelectManager-side KI_ACTION_CAR_SELECTION_CHANGED; consumed by
    // MainDirector::ProcessInputQueue case 64.
    KI_ACTION_CAR_SELECTION_CHANGED = 64,   // size 64  -- CarSelectionChangedAction
};

// ----------------------------------------------------------------------------
// Construct / Destruct.
//
// The console's GameStateModule is a CgsModule::ModuleSingleBuffered, so its
// GameStateModuleIO::OutputBuffer is the DataStructure the base allocates from the module's
// own DataBuffer inside Prepare() -> CreateOutputDataStructure(). ⚠️ FLAG (PC bring-up seam):
// nothing on PC calls this module's Prepare(), so that allocation point never runs. The
// buffer is newed here instead and freed in Destruct(); the buffer TYPE, its Construct
// (X360 0x82382940) and its accessors are the real console ones -- only the allocation SITE
// moves. DELETE-WHEN the module's Prepare()/CreateOutputDataStructure() path is real.
//
// Constructing it is what makes the game-state -> director game-action route exist at all:
// BrnGameModule::BridgeGameStateToDirector @0x823CD170 Appends this buffer's +0x04 queue into
// the director input buffer's queue every frame, and MainDirector::ProcessInputQueue drains it.
// ----------------------------------------------------------------------------
void GameStateModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    if (mpOutputBuffer == 0)
    {
        mpOutputBuffer = new GameStateModuleIO::OutputBuffer();
        mpOutputBuffer->Construct();
    }

    // ⭐ X360 0x82380388 (this function) is the console's ONLY caller of
    // CarSelectManager::Construct @0x823564D0:
    //     BrnGameState::CarSelectManager::Construct(a1 + 183712, a1 + 42320, a1, a1 + 47920)
    // i.e. (&mCarSelectManager, &mTriggerQueryManager, this, &mProgressionManager) -- the three
    // owning pointers the junkyard FSM keeps for its whole life. Verbatim, same arguments.
    //
    // ⚠️ ORDER DEVIATION (harmless, and stated rather than hidden): the console runs this AFTER
    // TriggerQueryManager::Construct @0x82364BF0 and ProgressionManager::Construct, so the two
    // subobjects are already initialised when their addresses are taken. Neither of those has a
    // linked body on PC yet (BrnTriggerQueryManager.cpp is unmounted -- it costs 13 unresolved
    // externals, all from UpdateTriggers/ProcessPlayerTriggers; ProgressionManager::Construct has
    // no body in the tree at all), so they cannot be called here. CarSelectManager::Construct only
    // STORES the two pointers -- it never dereferences either -- so taking the address of a
    // not-yet-constructed subobject is well-defined and the stored value is already final.
    // DELETE-WHEN those two Constructs land: they must then run BEFORE this line.
    mCarSelectManager.Construct(&mTriggerQueryManager, this, &mProgressionManager);
}

void GameStateModule::Destruct()
{
    if (mpOutputBuffer != 0)
    {
        delete mpOutputBuffer;
        mpOutputBuffer = 0;
    }

    CgsModule::ModuleSingleBuffered::Destruct();
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8239E578 -- GameStateModule::Prepare (vtable +64), the module's FIRST-pass prepare.
//
// THE SHAPE (console, statement for statement):
//     mbIsUpdating = true;                       // *(this + 292289) = 1
//     LockForWrite(lpOutputBuffer);
//     switch (mePrepareStage) { ...27 cases, each falling into the next on success... }
//     UnlockForWrite(lpOutputBuffer);
//     mbIsUpdating = false;
//     return <true only after stage 26>;
// Every stage that is still waiting for a resource reply breaks straight to the unlock tail and
// returns false, so the caller pumps it once per frame.
//
// THE CALLER, and why this exists at all: BrnGameModule::GamePrepare @0x823EFBD0 stage 4 does
//     CreateIOBuffer<GameStateModuleIO::OutputBuffer>(updateOutStack, &out, "GameState");
//     prepared = mGameStateModule.Prepare(out, updateOutStack, gameDataOut->GetAllocatorList());
//     if (!prepared) { LockForRead(out);
//                      gameDataIn->AppendRequestInterface<3072>(*out->Get());
//                      UnlockForRead(out); }
// -- i.e. the requests the stages below stage onto the output buffer's +0x3414
// RequestInterface<3072> leave through that append and are serviced by the GameData pump.
// (LoadingScriptedState::LoadGameState2 @0x823EF4D8 is the same bracket for the SECOND pass,
// GameStateModule::Prepare2 @0x8239ED10 -- Progression + Street. Not this wave.)
//
// ⚠️⚠️ RECONSTRUCTED SLICE -- say it plainly. ONE stage is real:
//   stage 3  E_PREPARESTAGE_LOAD_TRIGGER_DATA -> TriggerQueryManager::Prepare @0x82398218.
// Every other stage logs once and advances, naming its X360 call. In console order they are:
//   0  START                    ClearData @(not exported by name) + DebugComponent::Register x2
//                               (this+208544 / this+208376)
//   1  MANAGER                  ModuleSingleBuffered::Prepare  -- see the mbIsNewModule note below
//   2  MODE_DATA_ACQUIRING      pass-through on the console too (it only sets the stage word)
//   4  STUNT_MANAGER            StuntManager::Prepare(this+183952, out)
//   5/6 CHALLENGE_LIST          RequestInterface<3072>::GetFreeburnChallengeList(&rq, 0)
//                               -> reply type 53, mpChallengeList = reply.mHandle.mpResourceMemory
//   7/8 VEHICLE_LIST            GetVehicleList(&rq, 0) -> reply type 52, mpVehicleList = ...,
//                               then ProgressionManager::ApplyVehicleList + ModeManager::ApplyVehicleList
//   9/10 WHEEL_LIST             GetWheelList(&rq, 0) -> reply type 59, mpWheelList = ...
//   11/12 PLAYERCARCOLOURS      inline AcquireResourceRequest{&rq, 0, pool 5,
//                               HashString("CarColours")} -> CreateFromHandle(this+284400)
//   13 MODEMANAGER              ModeManager::Prepare(this+4128, mpChallengeList,
//                               allocatorList->GetHeapAllocator(0x1B))
//   14 TAKEDOWNMANAGER          TakedownManager::Prepare(this+568)
//   15 MUGSHOTMANAGER           (pass-through)
//   16 PAYBACKMANAGER           *(this+1448) = 0
//   17 INVITEMANAGER            VariableEventQueue<1536,16>::Prepare/Clear(this+2032)
//   18 FLYBYMANAGER             GameStateModuleIO::FlybyData::Prepare(this+186608)
//   19 NETWORKROUNDMANAGER      mReceiverQueue.Clear()
//   20 PROGRESSION              ProgressionManager::Prepare(this+47920)
//   21 RICH_PRESENCE            RichPresenceManagerBase::Prepare()
//   22 ACHIEVEMENT_MANAGER      AchievementManagerX360::Prepare(this+181680)
//   23 STREET_MANAGER           StreetManager::Prepare(this+284520, out, &rq)
//   24 IMAGE_MANAGER            GameStateImageManagerBase::Prepare(this+185520, heapAlloc 0x1B)
//   25 RUMBLE_MANAGER           RumbleManager::Prepare(this+46680)
//   26 DONE                     DriveThruManager::Prepare(this+44240, mTriggerQueryManager's
//                               TriggerData, the CarColours palette), publish the vehicle/wheel
//                               list pointers into two sub-managers, re-arm mePrepareStage to 1
//                               and clear the Prepare2 stage word.
// NONE of those is faked here: each is a log line, not a fabricated body. They land as their
// managers do. DO NOT read the log line as "done".
//
// ⚠️ FLAG (PC deviation, stage 1): `mbIsNewModule = true` before the base Prepare. The PC
// ModuleSingleBuffered::Construct leaves it FALSE, and with it false the base walks the
// old-style DataStructure path -- CreateInputDataStructure() fires its own
// "This is a new module type" assert and returns null, so Prepare would return false FOR EVER
// and GamePrepare would wedge. The GameState module is a new-style (IOBuffer) module on the
// console, and the committed GameDataModule::Prepare carries the identical line with the same
// reasoning (BrnGameDataModule.cpp:51, "[reliable] set before base Prepare"). The console sets
// it inside ClearData, which is stage 0's deferral.
// ----------------------------------------------------------------------------
// The GameData reply ids the two live list stages match (BrnGameDataModule's dispatch stages
// them at the slot: ProcessGetVehicleListRequest -> 52, ProcessGetWheelListRequest -> 59).
// ⚠️ The FreeburnChallengeList reply (53) is NOT here on purpose: its GameData handler is a
// DeferredGameDataRequest, so a stage waiting on it would never advance.
static const s32 KI_REPLY_VEHICLE_LIST = 52;
static const s32 KI_REPLY_WHEEL_LIST   = 59;

namespace
{
    // One log line per stage, once. (Same shape as the loading flow's LogScriptedStageOnce.)
    void LogPrepareStageOnce(s32 liStage, const char* lpcWhat)
    {
        static bool sbLogged[32] = { false };
        if (liStage < 0 || liStage >= 32 || sbLogged[liStage])
            return;
        sbLogged[liStage] = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::Prepare] stage " << liStage << " -- " << lpcWhat << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
// The shared body of Prepare's three "receive a resident data list" stages (vehicle @X360
// LABEL_17, wheel @LABEL_25, challenge @LABEL_9). The console writes all three out longhand;
// they are identical apart from the expected reply id and the two baked assert LINES, so they
// are folded here with those as parameters -- the same folding the committed
// GameDataModule::PrepareDataListResource already does for its two twins.
//
//   if (mReceiverQueue.GetLength() < 1) return false;            // still waiting
//   assert(event type == <replyId>)     "Invalid event id received\n"  <line A>
//   assert(reply->miEventId == 0)       "Invalid event id received\n"  <line B>
//   *lppOut = reply->mHandle.mpResourceMemory;                   // X360 `v13[8]`, i.e. +0x20
//
// ⚠️ The console's two asserts are built through the StrStream operator<< form
// (CgsDev::StrStream + StrStreamBase::operator<<), not FireAssert's literal; the message text
// is identical either way, so the plain Begin/Fire/End sequence is used with the X360 lines.
// ----------------------------------------------------------------------------
bool GameStateModule::ReceiveListResource(s32 liExpectedReplyId, s32 liAssertLineType,
                                          s32 liAssertLineEventId, void** lppOutResource)
{
    if (mReceiverQueue.GetLength() < 1)
        return false;

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

    if (liType != liExpectedReplyId)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liAssertLineType);
        CgsDev::Assert::EndAssert();
    }

    const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
        static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);

    if (lpReply == 0 || lpReply->miEventId != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liAssertLineEventId);
        CgsDev::Assert::EndAssert();
        return false;
    }

    // X360 `*(this + 284392) = v13[8]` -- payload +0x20 is mHandle.mpResourceMemory. Read BY
    // MEMBER (the host ResourceHandle is 16 bytes where the console's is 8, so every literal
    // offset past it shifts).
    *lppOutResource = lpReply->mHandle.mpResourceMemory;
    return true;
}

bool GameStateModule::Prepare(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                              CgsModule::IOBufferStack*        lpUpdateOutputBufferStack,
                              const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
{
    // The console asserts nothing here; GamePrepare always hands it a live buffer. Guard anyway
    // -- a null would otherwise fault inside TriggerQueryManager::Prepare's own assert.
    if (lpOutputBuffer == 0)
    {
        CGS_ASSERT(false, "lpOutputBuffer");
        return false;
    }
    (void)lpUpdateOutputBufferStack;   // stages 13/24 (heap allocator) + the manager prepares
    (void)lpAllocatorList;             //   are the deferrals listed above

    // X360: `*(this + 292289) = 1` at entry, cleared at the single exit.
    mbIsUpdating = true;

    if (!mbReceiverQueueConstructed)
    {
        // The console's mReceiverQueue is Construct'd by ClearData (stage 0's deferral). Every
        // stage below names it as its reply target, and AddEvent on an unconstructed receiver
        // queue writes through a null buffer base. One-shot here until ClearData lands.
        mbReceiverQueueConstructed = true;
        mReceiverQueue.Construct();
    }

    lpOutputBuffer->LockForWrite();

    bool lbDone = false;

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        // X360: GameStateModule::ClearData(this); DebugComponent::Register(this+208544);
        // DebugComponent::Register(this+208376). [deferred -- ClearData is a large member-wipe
        // over members this slice does not model; the two components are debug-menu only.]
        LogPrepareStageOnce(0, "ClearData + 2 x DebugComponent::Register [deferred]");
        // fall through

    case E_PREPARESTAGE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        // See the FLAG above: the console's GameState module is a new-style module, so the base
        // prepare is a no-op that reports done. Setting the flag is what makes that true here.
        mbIsNewModule = true;
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            break;
        // fall through

    case E_PREPARESTAGE_MODE_DATA_ACQUIRING:
        // The console's case 2 only advances the stage word (LABEL_4 -> LABEL_5).
        mePrepareStage = E_PREPARESTAGE_LOAD_TRIGGER_DATA;
        // fall through

    case E_PREPARESTAGE_LOAD_TRIGGER_DATA:
        // ⭐ REAL. X360: `if (!TriggerQueryManager::Prepare(this + 42320, lpOutputBuffer,
        //                    this + 232384)) break;  *(this + 552) = 4;`
        // This is the LoadBundle("Triggers.dat", pool 5) -> acquire("TriggerData") ->
        // LoadTrafficLanes chain. It needs several pumps.
        mePrepareStage = E_PREPARESTAGE_LOAD_TRIGGER_DATA;
        if (!mTriggerQueryManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        mePrepareStage = E_PREPARESTAGE_STUNT_MANAGER;
        // fall through

    case E_PREPARESTAGE_STUNT_MANAGER:
        LogPrepareStageOnce(4, "StuntManager::Prepare [deferred]");
        // fall through
    case E_PREPARESTAGE_REQUEST_CHALLENGE_LIST:
    case E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST:
        LogPrepareStageOnce(5, "GetFreeburnChallengeList + receive (reply 53) [deferred]");
        // fall through
    case E_PREPARESTAGE_REQUEST_VEHICLE_LIST:
        // ⭐ REAL. X360 LABEL_16: `stage = 8; GetVehicleList(requests, &mReceiverQueue, 0);
        //                          mReceiverQueue.Clear();` then fall into the receive.
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetVehicleList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through

    case E_PREPARESTAGE_RECEIVE_VEHICLE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        if (!ReceiveListResource(KI_REPLY_VEHICLE_LIST, 556, 561,
                                 reinterpret_cast<void**>(&mpVehicleList)))
            break;
        // [deferred] ProgressionManager::ApplyVehicleList(this+47920) and
        // ModeManager::ApplyVehicleList(this+4128, mpVehicleList). Neither has a body in this
        // tree yet (only ChallengeManager::ApplyVehicleList is even declared). The pointer
        // itself IS installed, which is what every GameStateModule body that asserts on it
        // needs; the two republish hooks land with their managers.
        LogPrepareStageOnce(8, "vehicle list installed; 2 x ApplyVehicleList [deferred]");
        mePrepareStage = E_PREPARESTAGE_REQUEST_WHEEL_LIST;
        // fall through

    case E_PREPARESTAGE_REQUEST_WHEEL_LIST:
        // ⭐ REAL. X360 LABEL_24, the same shape with reply id 59.
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetWheelList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through

    case E_PREPARESTAGE_RECEIVE_WHEEL_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        if (!ReceiveListResource(KI_REPLY_WHEEL_LIST, 599, 604,
                                 reinterpret_cast<void**>(&mpWheelList)))
            break;
        mePrepareStage = E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS;
        // fall through
    case E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS:
    case E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS:
        LogPrepareStageOnce(11, "acquire \"CarColours\" (pool 5) + bind [deferred]");
        // fall through
    case E_PREPARESTAGE_MODEMANAGER:
    case E_PREPARESTAGE_TAKEDOWNMANAGER:
    case E_PREPARESTAGE_MUGSHOTMANAGER:
    case E_PREPARESTAGE_PAYBACKMANAGER:
    case E_PREPARESTAGE_INVITEMANAGER:
    case E_PREPARESTAGE_FLYBYMANAGER:
    case E_PREPARESTAGE_NETWORKROUNDMANAGER:
    case E_PREPARESTAGE_PROGRESSION:
    case E_PREPARESTAGE_RICH_PRESENCE:
    case E_PREPARESTAGE_ACHIEVEMENT_MANAGER:
    case E_PREPARESTAGE_STREET_MANAGER:
    case E_PREPARESTAGE_IMAGE_MANAGER:
    case E_PREPARESTAGE_RUMBLE_MANAGER:
        LogPrepareStageOnce(13, "the 13 manager prepares (Mode..Rumble) [deferred]");
        // fall through
    case E_PREPARESTAGE_DONE:
        // [diagnostic, one-shot] print BOTH ENDS of the two list stages -- a non-null pointer
        // is not proof the list decoded. Delete with the rest of the bring-up diagnostics.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::Prepare] lists: vehicles="
                << (mpVehicleList != 0 ? mpVehicleList->GetVehicleCount() : -1)
                << " wheels="
                << (mpWheelList != 0 ? mpWheelList->GetWheelCount() : -1) << "\n";
        }
        // ⭐ REAL now (the "list publish" half of the terminal stage). X360 @0x8239EC8C, straight
        // after DriveThruManager::Prepare:
        //     r9 = this + 0x2CDA0 (mCarSelectManager);  r8 = this + 0x2CE20 (mOnlineCarSelectManager)
        //     stw *(this+0x456EC), 0x18(r9)   <- CarSelectManager::mpWheelList
        //     stw *(this+0x456E8), 0x14(r9)   <- CarSelectManager::mpVehicleList
        //     stw *(this+0x456EC), 0x10(r8)   <- OnlineCarSelectManager::mpWheelList
        //     stw *(this+0x456E8), 0x0C(r8)   <- OnlineCarSelectManager::mpVehicleList
        // Those four stores ARE CarSelectManager::Prepare / OnlineCarSelectManager::Prepare, both
        // fully inlined (neither has a symbol in the image; the DWARF declares both as
        // `Prepare(const VehicleList*, const WheelList*)`). Called through the named methods here.
        //
        // This is what the whole junkyard flow was missing: SetupSpawnLocations, SpawnInStartCar,
        // GetProfileCarData and StartCarSelectState all resolve their car records through
        // CarSelectManager::mpVehicleList, and nothing had ever written it.
        //
        // [deferred] the OTHER half of this stage -- DriveThruManager::Prepare(this+44240,
        // mTriggerQueryManager's TriggerData, the CarColours palette) -- and the
        // OnlineCarSelectManager leg (its TU is unmounted).
        mCarSelectManager.Prepare(mpVehicleList, mpWheelList);

        // ⭐ AND THE PROGRESSION LAYER'S COPY (X360 ProgressionManager +133448). MEASURED:
        // the first live ResetPlayerCarAction chain fired the console's own "lpVehicleListEntry"
        // assert (BrnProgressionManager.cpp:1258) from OnPlayerCarChange, because the
        // ProgressionManager's vehicle-list pointer had NEVER been installed by anything --
        // its SetVehicleList had zero callers in the whole tree, and the header's own FLAG said
        // so ("nothing installs it yet -- Prepare2's caller does on the console"). Every
        // progression body that resolves a car record reads that pointer.
        // [FLAG PC bring-up] the console installs it from Prepare2's caller; this is the same
        // list, published from the stage that already publishes it to the two car-select
        // managers. DELETE-WHEN Prepare2's caller lands.
        mProgressionManager.SetVehicleList(mpVehicleList);

        // [diagnostic, one-shot] Prove the junkyard half of the trigger data end to end, WITHOUT
        // driving anything: count the E_TYPE_JUNK_YARD generic regions, run the console's own
        // FindNearestJunkyardID @0x8236BAC8 from the track's authored player-start position (the
        // exact vector SendSetupPlayerCarEvent @0x8239A918 feeds it), and check that the id it
        // picks also appears in the SPAWN table -- because that cross-table agreement is the
        // precondition for CarSelectManager::SetupSpawnLocations filling all five slots, and
        // therefore for the junkyard entry not null-dereferencing maSpawnLocations[1].
        // Delete with the rest of the bring-up diagnostics.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
            if (lpTriggerData != 0)
            {
                s32 liJunkyardRegions = 0;
                const s32 liRegionCount = lpTriggerData->GetGenericRegionCount();
                for (s32 li = 0; li < liRegionCount; ++li)
                {
                    if (lpTriggerData->GetGenericRegion(li)->GetType()
                        == BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD)
                        ++liJunkyardRegions;
                }

                const Vector3 lStart      = lpTriggerData->GetPlayerStartPosition();
                const CgsID   lJunkyardId = FindNearestJunkyardID(lStart);

                s32 liMatchingSpawns = 0;
                const s32 liSpawnCount = lpTriggerData->GetSpawnLocationCount();
                for (s32 li = 0; li < liSpawnCount; ++li)
                {
                    if (lpTriggerData->GetSpawnLocation(li)->GetJunkyardId() == lJunkyardId)
                        ++liMatchingSpawns;
                }

                *CgsDev::Log::gpDebugPrint
                    << "[GameStateModule] junkyard regions=" << liJunkyardRegions
                    << "/" << liRegionCount
                    << " playerStart=(" << lStart.x << ", " << lStart.y << ", " << lStart.z << ")"
                    << " nearestJunkyardId=" << static_cast<u64>(lJunkyardId)
                    << " spawnsForThatJunkyard=" << liMatchingSpawns << "\n";
            }
        }
        // ⭐ ARM THE CONSOLE'S OWN START-OF-GAME LATCH (+0x32DC4). PreWorldUpdate tests it, runs
        // SendSetupPlayerCarEvent and clears it. The console arms it from an event handler this
        // slice does not reconstruct; this is the first moment all three of that function's data
        // preconditions hold (mpVehicleList, mpWheelList and the TriggerQueryManager's TriggerData
        // are all installed by the stages above), so it is armed here.
        // [FLAG PC bring-up] the ARMING SITE is the deviation -- the latch and everything it
        // drives are console code. DELETE-WHEN the arming event handler lands.
        mbSendSetupPlayerCarPending = true;

        LogPrepareStageOnce(26, "car-select list publish REAL; DriveThruManager::Prepare [deferred] -- prepare DONE");
        // X360 tail: `*(this + 552) = 1; *(this + 560) = 0;` -- the machine re-arms at MANAGER
        // for a later re-prepare and clears the second-pass stage word (which this slice does
        // not model yet). Reproduced for the stage word it does have.
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        lbDone = true;
        break;

    default:
        CGS_ASSERT(false, "Invalid Stage\n");   // X360 BrnGameStateModule.cpp:825
        break;
    }

    lpOutputBuffer->UnlockForWrite();
    mbIsUpdating = false;
    return lbDone;
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8236BAC8 -- FindNearestJunkyardID.
//
// Linear scan of the track TriggerData's generic-region table for the E_TYPE_JUNK_YARD region
// whose box centre is nearest lPosition; returns that region's CgsID. The console:
//
//     td    = mTriggerQueryManager.GetTriggerData();        // this + 0xAB70 (43888)
//     count = td->miGenericRegionCount;                     // td + 0x48
//     best  = flt_82029B70;  id = kCGSID_NULL;
//     for (i = 0; i < count; ++i) {
//         assert(i < td->miGenericRegionCount);              // BrnTriggerData.h:495, inlined
//         r = td->mpGenericRegions + i * 0x38;               // td + 0x44, stride == sizeof
//         if (r->meType != 0) continue;                      // lbz +0x36; 0 == E_TYPE_JUNK_YARD
//         d = length(BoxRegion.position - lPosition);        // lfs +0x00/+0x04/+0x08
//         if (d < best) { best = d; id = (s64)(s32)r->mId; } // lwz +0x24, extsw
//     }
//     assert(id != kCGSID_NULL);                             // BrnGameStateModule.cpp:6723
//     return id;
//
// TWO MEASURED CONSTANTS, not guesses:
//   * flt_82029B70 == 0x7F7FFFFF == FLT_MAX (read out of .rdata with headless IDA, not inferred
//     from the idiom -- the brief's rule about guessed rodata literals).
//   * the 0x38 stride is exactly sizeof(GenericRegion) here too (36-byte BoxRegion + 8 + 12),
//     which is the check that our x64 GenericRegion did NOT drift from the console's.
//
// The X360 computes the TRUE distance, not the squared one: vmsum3fp128 gives the dot product and
// the two vnmsubfp/vmaddfp pairs are a Newton-refined rsqrt, with a vcmpeqfp/vsel guarding the
// zero-length case. Ordering is identical either way; the sqrt is kept so the value is the
// console's value.
// ----------------------------------------------------------------------------
CgsID GameStateModule::FindNearestJunkyardID(Vector3 lPosition) const
{
    const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
    const s32 liGenericRegionCount =
        (lpTriggerData != 0) ? lpTriggerData->GetGenericRegionCount() : 0;

    CgsID lJunkyardId = 0;
    f32   lfNearest   = 3.402823466e+38f;   // flt_82029B70 == FLT_MAX

    for (s32 li = 0; li < liGenericRegionCount; ++li)
    {
        const BrnTrigger::GenericRegion* lpRegion = lpTriggerData->GetGenericRegion(li);
        if (lpRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD)
            continue;

        const Vector3 lRegionPosition = lpRegion->GetBoxRegion()->GetPosition();
        const f32 lfDeltaX = lRegionPosition.x - lPosition.x;
        const f32 lfDeltaY = lRegionPosition.y - lPosition.y;
        const f32 lfDeltaZ = lRegionPosition.z - lPosition.z;
        const f32 lfSqDistance =
            lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
        const f32 lfDistance = (lfSqDistance != 0.0f) ? std::sqrt(lfSqDistance) : 0.0f;

        if (lfDistance < lfNearest)
        {
            lfNearest   = lfDistance;
            lJunkyardId = lpRegion->GetId();
        }
    }

    CGS_ASSERT(lJunkyardId != 0, "lJunkyardId != kCGSID_NULL");
    return lJunkyardId;
}

// X360 @ 0x823116D0. Returns whether the currently-running game mode is one of the online modes. May
// only be called while the module is updating (asserts mbIsUpdating). Fetches the current game mode
// from the embedded ModeManager and forwards to GameMode::IsOnline(); if there is no current mode,
// returns false. (X360 reads the current-mode pointer inline as *(this + 0x1DB8) inside mModeManager
// and its mbIsOnline at *(mode + 172); de-inlined to the two logical calls.)
bool GameStateModule::IsOnlineGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode != nullptr)
    {
        return lpCurrentGameMode->IsOnline();
    }
    return false;
}

// The cached current-game-mode type. The X360 reads it as the raw scalar just below the embedded
// mModeManager (GameStateModule+7604 == mModeManager+0xD94, meCurrentGameModeType); de-inlined to
// the ModeManager's own named accessor -- same read, no offset poke.
GameStateModuleIO::EGameModeType GameStateModule::GetCurrentGameModeType() const
{
    return mModeManager.GetCurrentGameModeType();
}

// X360 @ 0x82311620. Returns the player's GLOBAL race-car index (its slot in the full world race-car
// table). May only be called while the module is updating (asserts mbIsUpdating).
s32 GameStateModule::GetPlayerGlobalRaceCarIndex()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return miPlayerGlobalRaceCarIndex;
}

// X360 @ 0x82356870. Returns whether the active race car in slot leRaceCarIndex is currently crashing.
// Asserts the module is updating and that the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT); reproduces the three X360 asserts verbatim (the module-updating one,
// then the two range guards) before returning the cached per-slot crash flag.
bool GameStateModule::IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex)
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    return maRaceCarCrashing[leRaceCarIndex];
}

// X360 @ 0x823567A8. True when the current game mode is a showtime mode (offline or online). May only
// be called while the module is updating (asserts mbIsUpdating). The X360 reads the current game-mode
// type inline (this+7604) and tests == E_MODE_OFFLINE_SHOWTIME (2) || == E_MODE_ONLINE_SHOWTIME (16);
// de-inlined to the GetCurrentGameModeType() accessor (same read) to avoid a raw offset access.
bool GameStateModule::IsShowtimeGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    return leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
        || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
}

// X360 @ 0x82356978. True when the simulation is currently paused. miSimPauseFlags is a bitfield of
// active pause reasons; a nonzero value means paused. When lbCheckGameMode is set and the current mode
// is online, some pause-reason bits are ignored: the X360 masks off bits {2,3,5} (mask 0xFFFFFFD3, the
// strict path -- returns immediately) when lbStrictMask is set, otherwise bits {1,2,3,5}
// (mask 0xFFFFFFD1). (The two bool parameters are named from the asm's branch structure; their exact
// call-site meaning is confirmed when the pause callers -- RequestPause/RequestUnpause -- are homed.)
bool GameStateModule::IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const
{
    s32 liPauseFlags = miSimPauseFlags;
    if (lbCheckGameMode)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode != nullptr && lpCurrentGameMode->IsOnline())
        {
            if (lbStrictMask)
            {
                return (liPauseFlags & ~0x2C) != 0;   // X360 mask 0xFFFFFFD3 -- clear bits {2,3,5}
            }
            liPauseFlags &= ~0x2E;                     // X360 mask 0xFFFFFFD1 -- clear bits {1,2,3,5}
        }
    }
    return liPauseFlags != 0;
}

// X360 @ 0x823566F8. Hands back the module's per-frame output GUI event queue (the
// CgsModule::VariableEventQueue<18432,16> that the PaybackManager and other managers publish their
// HUD/GUI events onto). May only be called while the module is updating (asserts mbIsUpdating).
CgsModule::VariableEventQueue<18432, 16>* GameStateModule::GetOutputGuiEventQueue()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return &mOutputGuiEventQueue;
}

// X360 @ 0x82363698. Mark car lCarId as already-shown in the unlock sequence. The X360 resolves the
// player Profile through the embedded progression manager (GetProfile inlines to the by-value Profile
// sub-object), asserts it is non-null with the verbatim message, then forwards to
// Profile::SetCarUnlockAlreadyShown.
void GameStateModule::SetCarUnlockAlreadyShown(CgsID lCarId)
{
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != nullptr, "mProgressionManager.GetProfile()");
    lpProfile->SetCarUnlockAlreadyShown(lCarId);
}

// ============================================================================================
// THE JUNKYARD / CAR-SELECT PRODUCER SURFACE (2026-08-01).
//
// Everything below is what CarSelectManager reaches for through mpGameStateModule. Each body is
// recovered from the X360 ASM, not the Hex-Rays prototype: on the X360 a CgsID is a full 64-bit
// value in ONE 64-bit GPR (`stdx r4, this, 0x456D8`), and Hex-Rays renders those register pairs
// as a single `__int64 a2`, dropping every argument after it without a trace. Three of the five
// signatures below would have been wrong if taken from the pseudocode.
// ============================================================================================

// The loaded vehicle list (X360 `lwzx rN, this, 0x456E8`).
BrnResource::VehicleList* GameStateModule::GetVehicleList()
{
    return mpVehicleList;
}

// X360 read at GameStateModule+0x456EC (284396) -- installed by Prepare's stage 9/10.
BrnResource::WheelList* GameStateModule::GetWheelList()
{
    return mpWheelList;
}

// The active player car / wheel ids the CarSelect FSM compares against its desired car
// (X360 raw reads at this+0x456D8 / +0x456E0; both are written by OnSpecialEventPlayerCarChange).
CgsID GameStateModule::GetActivePlayerCarId() const
{
    return mActivePlayerCarId;
}

CgsID GameStateModule::GetActivePlayerWheelId() const
{
    return mActivePlayerWheelId;
}

// The module's cached active-race-car snapshot (X360 embedded interface at this+0x397E0).
const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    GameStateModule::GetLastActiveRaceCarInterface() const
{
    return &mLastActiveRaceCarInterface;
}

// ⭐ The raw `*(this + 232288)` nonzero test three console call sites open-code. That word IS
// miSimPauseFlags (see the header note), so this is exactly IsSimPaused(false, false).
bool GameStateModule::IsTrainingPauseSuppressed() const
{
    return miSimPauseFlags != 0;
}

// --------------------------------------------------------------------------------------------
// RequestUnpause (X360 0x82382138).
// Clear the leUnpauseModule pause-reason bits from miSimPauseFlags. The console samples
// IsSimPaused BEFORE and AFTER the clear and only broadcasts the unpause action (87, 1B) when the
// answer actually changed; the two asserts guard the "we asked to unpause and ended up paused"
// inversions. The X360 keeps `lbWasPausedBefore = (flags != 0)` from the PRE-clear word and
// `lbPausedAfterClear = (newFlags != 0)` from the post-clear word, which is why the second assert
// can fire even on the no-change path.
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestUnpause(s32 leUnpauseModule, GameStateModuleIO::GameActionQueue* lpQueue)
{
    if (leUnpauseModule == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leUnpauseModule != E_PAUSE_NONE", KAC_GSM_FILE, 6159);
        CgsDev::Assert::EndAssert();
    }

    const bool lbSimPausedBefore = IsSimPaused(false, false);

    const s32  liPreviousFlags = miSimPauseFlags;
    const s32  liNewFlags      = liPreviousFlags & ~leUnpauseModule;
    const bool lbWasPausedBefore = (liPreviousFlags != 0);
    miSimPauseFlags = liNewFlags;

    const bool lbSimPausedAfter  = IsSimPaused(false, false);
    const bool lbStillPaused     = (liNewFlags != 0);

    if (lbSimPausedAfter == lbSimPausedBefore)
    {
        if (lbStillPaused != lbWasPausedBefore && lbStillPaused)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Paused from RequestUnpause...?", KAC_GSM_FILE, 6187);
            CgsDev::Assert::EndAssert();
        }
        return;
    }

    if (lbSimPausedAfter)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Sim paused from RequestUnpause...?", KAC_GSM_FILE, 6182);
        CgsDev::Assert::EndAssert();
    }

    u8 lacUnpause[1] = { 0 };   // X360 posts the uninitialised 1-byte local verbatim
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacUnpause), KI_ACTION_UNPAUSE, 1);
}

// --------------------------------------------------------------------------------------------
// ApplyCarStats (X360 0x82381188).
// Publish the newly-selected car's gameplay stats out of its VehicleListEntry as the 24-byte
// action 198. The six payload words and their sources are read straight off the asm:
//   [+0]  = entry byte @0xE8 + 1  -> no; see the store map below (byte offsets, not indices).
// X360 store map (`li r6, 0x18` == 24 bytes, `li r5, 0xC6` == 198):
//   payload +0x00 <- lbz entry+0x99      payload +0x0C <- lbz entry+0x98
//   payload +0x04 <- lbz entry+0x9B      payload +0x10 <- lfs entry+0x90 (f32)
//   payload +0x08 <- lbz entry+0x9A      payload +0x14 <- lbz entry+0xE8
// ⚠️ FLAG: entry+0x90..+0x9B is inside VehicleListEntry's leading opaque header (maPad0), whose
// individual gameplay fields are not named yet -- CanAutoRepair()/IsTrophyCar()/GetUnlockRank()
// are the three bits of it that have been recovered so far. The five reads here are therefore
// taken as raw bytes/word from that region rather than through named accessors; entry+0xE8 IS
// named (GetCarType()). DELETE-WHEN the VehicleListEntry gameplay-data sub-object is homed.
// --------------------------------------------------------------------------------------------
void GameStateModule::ApplyCarStats(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpQueue)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_GSM_FILE, 4720);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);

    struct ApplyCarStatsAction
    {
        s32 miStatA;        // entry +0x99
        s32 miStatB;        // entry +0x9B
        s32 miStatC;        // entry +0x9A
        s32 miStatD;        // entry +0x98
        f32 mfStatE;        // entry +0x90
        s32 miCarType;      // entry +0xE8 == GetCarType()
    };
    ApplyCarStatsAction lAction;
    lAction.miStatA   = lpcEntryBytes[0x99];
    lAction.miStatB   = lpcEntryBytes[0x9B];
    lAction.miStatC   = lpcEntryBytes[0x9A];
    lAction.miStatD   = lpcEntryBytes[0x98];
    std::memcpy(&lAction.mfStatE, lpcEntryBytes + 0x90, sizeof(f32));
    lAction.miCarType = lpVehicleListEntry->GetCarType();

    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                      KI_ACTION_APPLY_CAR_STATS, sizeof(ApplyCarStatsAction));
}

// --------------------------------------------------------------------------------------------
// GetOriginalCarId (X360 0x823758E8).
// Walk lCarId up its VehicleListEntry parent chain to the base ("original") car a livery variant
// derives from. The console walks AT MOST TWO levels -- car -> parent -> grandparent -- and
// returns the deepest non-null id it reaches. The leading redundant lookup exists only to carry
// the console's own assert.
// --------------------------------------------------------------------------------------------
CgsID GameStateModule::GetOriginalCarId(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;

    {
        const s32 liIndex = lpVehicleList->GetVehicleIndex(lCarId);
        if (liIndex < 0 || lpVehicleList->GetVehicleData(liIndex) == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("NULL != mpVehicleList->GetVehicleData(lCarId)", KAC_GSM_FILE, 5545);
            CgsDev::Assert::EndAssert();
            return lCarId;   // the X360 falls through into a null deref; bail instead of faulting
        }
    }

    const s32 liCarIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCarEntry =
        (liCarIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCarIndex);
    const CgsID lParentId = lpCarEntry->GetParentId();
    if (lParentId == 0)
    {
        return lCarId;
    }

    const s32 liParentIndex = lpVehicleList->GetVehicleIndex(lParentId);
    const BrnResource::VehicleListEntry* lpParentEntry =
        (liParentIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liParentIndex);
    const CgsID lGrandParentId = (lpParentEntry != 0) ? lpParentEntry->GetParentId() : 0;
    return (lGrandParentId != 0) ? lGrandParentId : lParentId;
}

// --------------------------------------------------------------------------------------------
// OnSpecialEventPlayerCarChange (X360 0x8238FB40).
// The single point every player-car swap funnels through: cache the new car + wheel ids, tell the
// progression layer, publish the car's stats, stamp the car's TYPE onto the profile, and broadcast
// the 8-byte "player car changed" action (1).
// ARG SHAPE FROM ASM: r3=this, r4=carId(std @0x456D8), r5=wheelId(std @0x456E0), r6=queue,
// r7=the bool forwarded to ProgressionManager::OnPlayerCarChange.
// --------------------------------------------------------------------------------------------
void GameStateModule::OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                                    GameStateModuleIO::GameActionQueue* lpQueue,
                                                    bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    mProgressionManager.OnPlayerCarChange(lCarId, lWheelId, lbUpdateProfile);
    ApplyCarStats(lCarId, lpQueue);

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpPlayerCarVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lPlayerCarVehicleListEntry != NULL", KAC_GSM_FILE, 4694);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // X360 `lbz r29, 0xE8(entry)` -> the car TYPE byte, stored into the profile's cached
    // "current car type" word (Profile +117948, mProfile.meCurrentCarType).
    const u8 luCarType = lpPlayerCarVehicleListEntry->GetCarType();

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile != NULL", KAC_GSM_FILE, 4698);
        CgsDev::Assert::EndAssert();
        return;
    }
    lpProfile->SetCurrentCarType(static_cast<s32>(luCarType));

    CgsID lNewCarId = lCarId;
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNewCarId),
                      KI_ACTION_PLAYER_CAR_CHANGED, sizeof(CgsID));
}

// --------------------------------------------------------------------------------------------
// OnPlayerCarChange (X360 0x82396B88).
// The offline junkyard-exit path. Does everything OnSpecialEventPlayerCarChange does, then looks
// up the AI opponent set for the car's ORIGINAL (base) id at the player's current progression rank
// and broadcasts up to seven opponent car ids as the 64-byte action 4.
// ARG SHAPE FROM ASM: identical to OnSpecialEventPlayerCarChange -- r3..r7 are forwarded to it
// unchanged (the X360 does not even reload them).
// --------------------------------------------------------------------------------------------
void GameStateModule::OnPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                        GameStateModuleIO::GameActionQueue* lpQueue,
                                        bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    // The X360 zeroes the opponent array's count word BEFORE the forwarded call (the local lives
    // across it), i.e. Array<CgsID,7>::Construct().
    Array<s64, 7> laOpponentCarIds;
    laOpponentCarIds.Construct();

    OnSpecialEventPlayerCarChange(lCarId, lWheelId, lpQueue, lbUpdateProfile);

    const BrnProgression::ProgressionData* lpProgressionData = mProgressionManager.GetProgressionData();
    if (lpProgressionData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProgressionData != NULL", KAC_GSM_FILE, 4636);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        const CgsID lOriginalCarId = GetOriginalCarId(lCarId);
        // X360 `extsb r5, r3` -- the rank is sign-extended from a BYTE before the lookup.
        const s32 liProgressionRank =
            static_cast<s32>(static_cast<s8>(mProgressionManager.GetProgressionRank()));

        const BrnProgression::CarOpponentSet* lpCarOpponentSet =
            lpProgressionData->FindCarOpponentSet(lOriginalCarId, liProgressionRank);
        if (lpCarOpponentSet != 0)
        {
            s32 liOpponents = lpCarOpponentSet->GetOpponentCount();
            if (liOpponents >= static_cast<s32>(Array<s64, 7>::KU_SIZE))
            {
                liOpponents = static_cast<s32>(Array<s64, 7>::KU_SIZE);
            }
            for (s32 liCarOpponentIndex = 0; liCarOpponentIndex < liOpponents; ++liCarOpponentIndex)
            {
                if (liCarOpponentIndex < 0 || liCarOpponentIndex >= lpCarOpponentSet->GetOpponentCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liCarOpponentIndex >= 0 && liCarOpponentIndex < miOpponentCount",
                        KAC_OPPONENTDATA_FILE, 224);
                    CgsDev::Assert::EndAssert();
                }
                laOpponentCarIds.Append(
                    static_cast<s64>(lpCarOpponentSet->GetCarOpponent(liCarOpponentIndex)->GetCarId()));
            }
        }
    }

    // X360 `li r6, 0x40` -- the whole Array<CgsID,7> (7*8 elements + count) goes on the wire.
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&laOpponentCarIds),
                      KI_ACTION_CAR_OPPONENT_SET, sizeof(laOpponentCarIds));
}

// --------------------------------------------------------------------------------------------
// RequestStreamingForVehicleSelection (X360 0x82382550).
//
// ⛔ HONEST PARTIAL -- READ THIS BEFORE TRUSTING THE CALL SITES.
//
// The console body builds the junkyard carousel's PRE-STREAM window: it asks
// GetListOfPlayerSelectableVehicles (X360 0x82376500) for the player's full selectable-car list
// into an Array<CgsID,128>, finds lCarId in it (falling back to the car's PARENT id when lCarId is
// a livery variant -- entry+0xE9 in {1,3,4}), then walks outward from that index collecting up to
// KI_MAX_ACTIVE_RACE_CARS(8) neighbours with a per-entry direction tag, and posts the resulting
// 88-byte action 69.
//
// WHAT IS REPRODUCED HERE: the lookup of the requested car, its livery->parent fallback, and both
// of the console's asserts. WHAT IS NOT: the selectable-list build and the neighbour window,
// because GetListOfPlayerSelectableVehicles is 183 instructions of its own and reaches four
// GameStateModule members that are not modelled on this slice (+183860 / +183937 / +183944 -- the
// online-event car-restriction state -- plus the profile car walk).
//
// WHY THIS IS NOT A SILENT DROP: (a) it logs, once per changed car, exactly what it did not send;
// (b) NOTHING IN THIS BUILD CONSUMES ACTION 69 -- the console consumer is
// RaceCarEntityModule::HandleSelectionRequestStreamingAction @0x822E9918, which is not
// reconstructed (grep for it: the only hit in the tree is a comment). So today the only observable
// difference between this and the full body is the log line.
// DELETE-WHEN GetListOfPlayerSelectableVehicles lands (then the window build comes back with it).
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestStreamingForVehicleSelection(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    if (lpVehicleList == 0)
    {
        return;
    }

    CgsID lStreamCarId = lCarId;

    const s32 liCurrentVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCurrentVehicleData =
        (liCurrentVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCurrentVehicleIndex);
    if (lpCurrentVehicleData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCurrentVehicleData != NULL", KAC_GSM_FILE, 7461);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        // A livery variant is not itself in the selectable list -- the console re-looks-up its
        // parent (X360: entry+0xE9 in {1,3,4} -> use GetParentId()).
        const u8 luLiveryType = lpCurrentVehicleData->GetLiveryType();
        if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
        {
            if (lpCurrentVehicleData->GetParentId() == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpCurrentVehicleData->GetParentId() != kCGSID_NULL",
                                           KAC_GSM_FILE, 7465);
                CgsDev::Assert::EndAssert();
            }
            lStreamCarId = lpCurrentVehicleData->GetParentId();
        }
    }

    static CgsID slLastLoggedCarId = 0;
    if (slLastLoggedCarId != lStreamCarId && CgsDev::Log::gpDebugPrint != 0)
    {
        slLastLoggedCarId = lStreamCarId;
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] RequestStreamingForVehicleSelection(" << static_cast<u32>(lStreamCarId)
            << "): the 88-byte carousel pre-stream action (69) is NOT posted -- "
               "GetListOfPlayerSelectableVehicles is not reconstructed. Nothing in this build "
               "consumes action 69 either (HandleSelectionRequestStreamingAction is absent), so "
               "no consumer is being starved today.\n";
    }
}

// ============================================================================
// FindPlayerScoringIndexForActiveRaceCar  @ 0x82363450
//
// Linear scan of the scoring module's eight per-player records (the console walks
// `scoring + 20548 + 344*i`, comparing the leading word against the requested
// active-race-car index) for the player scoring slot that owns leActiveRaceCarIndex.
// ⚠️ The MISS arm returns E_PLAYER_SCORING_INDEX_0, not an invalid sentinel -- `result = 0`
// at the console's LABEL_7 -- and that is exactly what start-of-game relies on: nothing is
// mapped yet, so the player's car takes scoring slot 0.
//
// [FLAG PC bring-up] the scoring module's per-player record array is not homed on this slice
// (the console reaches it as `this + 7632`, deep inside the un-modelled mid-object span), so
// the scan itself has nothing to walk and the function returns the console's own miss value.
// It is written as the miss arm, NOT as a fabricated scan. DELETE-WHEN BrnScoringSystem's
// per-player record array is homed here.
GameStateModuleIO::EPlayerScoringIndex
GameStateModule::FindPlayerScoringIndexForActiveRaceCar(::EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    (void)leActiveRaceCarIndex;
    return GameStateModuleIO::E_PLAYER_SCORING_INDEX_0;
}

// ============================================================================
// SendSetupPlayerCarEvent  @ 0x8239A918   -- THE START-OF-GAME JUNKYARD ENTRY
//
// Console body, statement for statement (0x8239A918..0x8239AA30):
//   1. cache the track's authored player-start pose:
//        this+48336 = TriggerData::GetPlayerStartPosition()   (lvx128 memory+0x10)
//        this+48352 = TriggerData::GetPlayerStartDirection()  (lvx128 memory+0x20)
//   2. entry = VehicleList::GetVehicleData(mpVehicleList, 0);  carId = entry->GetId()
//   3. wheelId = WheelList::GetWheelData(mpWheelList,
//                    FindWheelIndexFromName(entry->GetDefaultWheelName()))->mID  (miss -> left 0)
//   4. junkyardId = FindNearestJunkyardID(playerStartPosition)
//   5. scoringIdx = FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex())
//   6. CarSelectManager::EnterJunkyardAtStartOfGame(queue, junkyardId, carId, wheelId,
//                                                  scoringIdx, &mCachedCarSelectChangedAction)
//   7. ProgressionManager::OnDriveThru(junkyardId, 0, 0)
//   8. this+232306 = 1     (the "waiting to REALLY enter the junkyard" flag ProcessGameEvents
//                           case 78 tests before ReallyEnterJunkyardAtStartOfGame)
//
// ⭐ STEP 8 IS NOW REAL (2026-08-01). It is the console's own member -- DWARF
// BrnGameStateModule.h:811 mbWaitingToPutPlayerInJunkyard -- and its reader, the extracted
// case-78 arm, is ProcessGameEventsReallyEnterJunkyardBringUp() below. Without this store the
// junkyard entry stops half-done: the player's car is placed at maSpawnLocations[1] and NOTHING
// ever posts the transition-in action, so the director's meJunkyardState stays E_JY_INACTIVE and
// ArbStateCarSelect is never reached. That was the whole gap.
//
// [FLAG PC bring-up] steps 1 (the two cached pose members) and 7 are DROPPED, not paraphrased:
// the two pose members sit inside this slice's un-modelled span and have no reconstructed reader,
// and ProgressionManager::OnDriveThru is not reconstructed. Step 4 reads the start position
// straight from the TriggerData, which is the same value step 1 would have cached.
void GameStateModule::SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
    if (lpTriggerData == 0 || mpVehicleList == 0 || mpWheelList == 0)
    {
        return;
    }

    const Vector3 lPlayerStart = lpTriggerData->GetPlayerStartPosition();

    const BrnResource::VehicleListEntry* lpEntry = mpVehicleList->GetVehicleData(0);
    if (lpEntry == 0)
    {
        return;
    }
    const CgsID lCarModelId = lpEntry->GetId();

    CgsID lWheelId = 0;
    const s32 liWheelIndex = mpWheelList->FindWheelIndexFromName(lpEntry->GetDefaultWheelName());
    if (liWheelIndex != -1)
    {
        const BrnResource::WheelListEntry* lpWheelEntry = mpWheelList->GetWheelData(liWheelIndex);
        if (lpWheelEntry != 0)
        {
            lWheelId = lpWheelEntry->mID;
        }
    }

    const CgsID lJunkyardId = FindNearestJunkyardID(lPlayerStart);
    const GameStateModuleIO::EPlayerScoringIndex leScoringIndex =
        FindPlayerScoringIndexForActiveRaceCar(mePlayerActiveRaceCarIndex);

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[GameStateModule::SendSetupPlayerCarEvent] junkyard=" << static_cast<u64>(lJunkyardId)
            << " car=" << static_cast<u64>(lCarModelId)
            << " wheel=" << static_cast<u64>(lWheelId)
            << " scoringIdx=" << static_cast<s32>(leScoringIndex)
            << " playerStart=(" << lPlayerStart.x << ", " << lPlayerStart.y << ", "
            << lPlayerStart.z << ")\n";
    }

    mCarSelectManager.EnterJunkyardAtStartOfGame(lpActionQueue, lJunkyardId, lCarModelId, lWheelId,
                                                leScoringIndex, &mCachedCarSelectChangedAction);

    // Step 8 -- X360 `li r11,1; stb r11, <this+0x38B72>`. Arm the "waiting to REALLY enter the
    // junkyard" latch that ProcessGameEvents case 78 tests.
    mbWaitingToPutPlayerInJunkyard = true;
}

// ============================================================================
// ProcessGameEventsReallyEnterJunkyardBringUp
//   -- the extracted case-78 arm of ProcessGameEvents @0x823A0A18 (0x823A4590..0x823A45F8).
// See the header for the full FLAG (why the GUI-event trigger is not used on this build).
//
// The console arm, instruction for instruction:
//   0x823A4590  r29 = this + 0x38B72 (232306)
//   0x823A4598  lbz  r11, 0(r29);  if (!r11) break                  -- mbWaitingToPutPlayerInJunkyard
//   0x823A45A4  r3 = this + 0x2CDA0 (183712) == &mCarSelectManager
//   0x823A45A8  r4 = the game ACTION queue
//   0x823A45B0  bl  CarSelectManager::ReallyEnterJunkyardAtStartOfGame
//   0x823A45B4  r30 = this + 0x38B80 (232320) == &mCachedCarSelectChangedAction
//   0x823A45BC  ld   r11, 0(r30);  if (!r11) FireAssert(.., BrnGameStateModule.cpp, 0x1003=4099)
//   0x823A45E0  AddEvent(queue, r30, 0x40, 0x40)                    -- action 64, 64 bytes
//   0x823A45F4  stb  r18(==0), 0(r29)                               -- clear the latch
//
// ⚠️ THE ASSERT IS A 64-BIT TEST. Hex-Rays renders it `if (!*(v23 + 232324))` -- the classic
// big-endian misrender of a `ld` at +232320 as a word read of its low half. The asm computes
// r30 == this+232320 and does `ld r11, 0(r30)`, i.e. it tests the whole CgsID mJunkyardId. The
// same r30 is then handed to AddEvent as the record base, which only makes sense at +232320.
// ============================================================================
void GameStateModule::ProcessGameEventsReallyEnterJunkyardBringUp(
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (!mbWaitingToPutPlayerInJunkyard)
    {
        return;
    }

    mCarSelectManager.ReallyEnterJunkyardAtStartOfGame(lpActionQueue);

    // X360 assert literal, as IDA renders it: "mCachedCarSelectChangedAction.mJunkyard"... --
    // the string is truncated in the export at 39 characters; the tail below is this repo's
    // completion of it, in the file's own house style. The TEST is the console's.
    CGS_ASSERT(mCachedCarSelectChangedAction.mJunkyardId != 0,
               "mCachedCarSelectChangedAction.mJunkyardId != kCGSID_NULL");

    // The 64-byte CarSelectionChangedAction the entry filled in. MainDirector::ProcessInputQueue
    // case 64 is its consumer: it is the ONLY writer of the director GameState's mJunkyardId and
    // mbJunkyardPosIsLeft.
    lpActionQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&mCachedCarSelectChangedAction),
        KI_ACTION_CAR_SELECTION_CHANGED,
        static_cast<s32>(sizeof(mCachedCarSelectChangedAction)));

    mbWaitingToPutPlayerInJunkyard = false;

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[GameStateModule::ProcessGameEvents case 78] ReallyEnterJunkyardAtStartOfGame done;"
            << " junkyard=" << static_cast<u64>(mCachedCarSelectChangedAction.mJunkyardId)
            << " posIsLeft=" << (mCachedCarSelectChangedAction.mbJunkyardPosIsLeft ? 1 : 0) << "\n";
    }
}

// ============================================================================
// PreWorldUpdateSetupPlayerCarBringUp -- the extracted one-shot leg of
// PreWorldUpdate @0x823A5328 (0x823A5510..0x823A5540). See the header for the FLAG.
// ============================================================================
void GameStateModule::PreWorldUpdateSetupPlayerCarBringUp(bool lbMayCompleteJunkyardEntry)
{
    // Two legs of PreWorldUpdate live here now, each behind its own console latch, in the
    // console's own body order: the one-shot setup leg @0x823A5510, then the case-78 arm of
    // ProcessGameEvents @0x823A58B8.
    //
    // ⚠️ lbMayCompleteJunkyardEntry IS THE ORDERING STAND-IN FOR GAME EVENT 78, and it is
    // MEASURED, not defensive. See the header FLAG for why the GUI's own event cannot drive the
    // arm on this build; what the caller supplies instead is the GUI's OTHER first-boot signal
    // (the new-profile intro being live). Firing without it is not merely early, it is WRONG:
    // ArbStateCarSelect::Prepare picks its opening arm from mbNewProfileIntroActive, so an entry
    // that completes before the GUI has raised that flag puts the state in the junkyard
    // E_STATE_INTRO instead of E_STATE_GAME_INTRO_PART_ONE -- and then the intro's own fly-by
    // request trips that state's `!mbGameIntroFlybyActive` tripwire (:381) on EVERY frame of the
    // intro. Measured on this build: 163 asserts in a 98-second run.
    if (mpOutputBuffer == 0)
    {
        return;
    }
    if (!mbSendSetupPlayerCarPending &&
        !(mbWaitingToPutPlayerInJunkyard && lbMayCompleteJunkyardEntry))
    {
        return;
    }

    // The console gets the queue from GameStateModuleIO::OutputBuffer (its `Ou` accessor at
    // 0x823A54E4) and asserts it non-null at BrnGameStateModule.cpp:1149.
    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");
    mbIsUpdating = true;                   // the module asserts this in its own accessors
    if (mbSendSetupPlayerCarPending)
    {
        mbSendSetupPlayerCarPending = false;   // the console's `stb r17, 0(r28)` -- one-shot
        SendSetupPlayerCarEvent(lpActionQueue);
    }
    if (lbMayCompleteJunkyardEntry)
    {
        ProcessGameEventsReallyEnterJunkyardBringUp(lpActionQueue);
    }
    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();

    // [FLAG PC bring-up] SendSetUpAllEventStartsMessage (the console's partner call on this same
    // latch) is not reconstructed; it publishes the event-start table to the GUI and has no
    // consumer on this build.
}

// ============================================================================
// PreWorldUpdateCarSelectBringUp -- the extracted CAR-SELECT leg of PreWorldUpdate
// @0x823A5328 (0x823A5904..0x823A5958). See the header for the FLAG and the asm.
// ============================================================================
void GameStateModule::PreWorldUpdateCarSelectBringUp(f32 lfGameTimestep)
{
    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console's gate: a 64-bit load of CarSelectManager::mJunkyardId (this + 0x2CDC0 ==
    // mCarSelectManager + 0x20), non-zero == "the player is in a junkyard".
    if (!mCarSelectManager.IsInJunkyard())
    {
        return;
    }

    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");   // BrnGameStateModule.cpp:1149
    mbIsUpdating = true;
    mCarSelectManager.Update(lpActionQueue, 0, lfGameTimestep);
    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

}
