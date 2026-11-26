#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnGameStateStreetManager.cpp"
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // ChallengeHighScoreEntry::Construct/SetScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengePlayerScoreEntry, ChallengeData::SetScore, ScoreType, operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                        // CGS_ASSERT
#define CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER "..\\..\\..\\GameSource\\GameState/StreetData/BrnGameStateStreetManager.h"
// includes folded in from the BrnGameStateStreetManager_w*.cpp partfiles (2026-09-15)
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer::GetResourceRequestInterface
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"         // EventReceiverQueue<3072,16>::Clear/GetCount/GetFirstEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // VariableEventQueue<3072,16>::AddEvent, CgsModule::Event
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"            // BrnResource::GameDataIO::RequestInterface<3072>::GetAILanes
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"                  // BrnResource::GameDataIO::GameDataEvent (response GetEventId read)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"      // CgsResource::Events::AcquireResourceRequest
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"           // CgsResource::ID::HashString + operator<<
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"        // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint (the console's road dump)
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                      // Rival::GetId / GetDistrict
#include "SharedClasses/StreetData/BrnStreetData.h"                  // StreetData::GetRoadCount / GetRoad / Road::GetId
#include "BrnCommonTypes.h"                                         // CgsID (the par record's rival-id out-param)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::AddMonitor
#include <cstring>   // memset
#include "SharedClasses/AI/AISectionsResourceType.h"     // BrnAI::AISection / AISectionsData / BrnAI::KI_AI_SECTION_EDGES
#include "GameSource/World/AI/BrnAIPortal.h"             // BrnAI::Portal (GetPositionX/Y/Z, GetLinkSectionIndex)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RC interface + RaceCarState (mTransform)
#include <cstdlib>   // qsort
#include <cmath>     // asin / sqrtf / fabsf
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"     // AICarOutputInterface (player route + node index)
#include "GameSource/World/AI/Route/BrnRoute.h"                       // BrnAI::Route / RouteNode
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (dev assert message)
#define CGS_ARTIST_FILE_BRNRACECARENTITYMODULEOUTPUTINTERFACE "..\\..\\..\\GameSource\\World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The three members recovered here are the
// StreetManager methods that DON'T depend on the StreetManager member layout: Prepare2
// is a pure forwarder, and the two Create* factories operate entirely on the score
// record handed in. The full member layout is now committed in the frozen header
// (StreetManager keystone, wave B); the remaining 36 ledger methods land as
// BrnGameStateStreetManager_wB_* partfiles.

namespace BrnGameState
{

// @ 0x823509D8. Prepare2 WAS DEFINED HERE; it was SPLIT OUT 2026-08-11 into the sibling
// BrnGameStateStreetManager_Prepare2.cpp so that GameStateModule::Prepare2 case 2 could make the
// console's single call without dragging this TU's two score-entry factories. MEASURED
// (cl /c + dumpbin /SYMBOLS vs the defined-symbol set of build\game\obj): mounting this whole TU
// costs SIX unresolved externals (BrnStreetData::operator++, ChallengeHighScoreEntry::Construct,
// ChallengePlayerScoreEntry::Construct, ChallengeData::SetScore, ScoreList::KAI_MIN_SCORES /
// KAI_MAX_SCORES) and every one belongs to the two factories below. Fold the split file back in
// when the ChallengeData score family lands. Do NOT re-add the body: two definitions is LNK2005.

// @ 0x82324A28
BrnStreetData::ChallengeHighScoreEntry StreetManager::CreateHighScoreEntryFromDownloadData(
    const CgsNetwork::PlayerName* lpPlayerNames,
    const BrnNetwork::RoadRulesMessageData* lpRoadRulesData )
{
    BrnStreetData::ChallengeHighScoreEntry lEntry;
    lEntry.Construct();

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        const CgsNetwork::PlayerName& lrPlayerName = lpPlayerNames[leScoreType];
        if ( lrPlayerName.macName[0] != '\0' )
        {
            const s32 liScore = lpRoadRulesData->maScores[leScoreType];
            if ( liScore != 0 )
            {
                lEntry.SetScore( leScoreType, liScore, &lrPlayerName );
            }
        }
    }

    return lEntry;
}

// @ 0x82324AC0
BrnStreetData::ChallengePlayerScoreEntry StreetManager::CreateUserChallengeScoreFromRoadRulesData(
    const BrnNetwork::RoadRulesMessageData* lpRoadRulesData )
{
    BrnStreetData::ChallengePlayerScoreEntry lEntry;
    lEntry.Construct();

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        const s32 liScore = lpRoadRulesData->maScores[leScoreType];
        if ( liScore != 0 )
        {
            lEntry.SetScore( leScoreType, liScore );
        }
    }

    return lEntry;
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wB_01.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ============================================================================
// GameSource/Gamestate/StreetData/BrnGameStateStreetManager.cpp  (wave B, group 2)
//
// Streamed-load state machines the GameStateModule pumps from Prepare:
//   LoadAIData       @ 0x8234FA70  (meAILoadStage over the <3072,16> receiver queue)
//   LoadDistrictMap  @ 0x8234FB98  (meDistrictMapLoadStage over the <3072,16> queue)
//   LoadStreetData   @ 0x8234F630  (meLoadStage; pumped from Prepare2, added 2026-08-11)
//
// OnProfileLoaded (0x82349E20) is in this group's ledger but is NOT bodied here:
// its body memcpys two 64-entry road-rules tables + reads the road-rules id/timestamp
// straight out of BrnProgression::Profile, and every one of those five members is
// PRIVATE in the committed BrnProfile.h with no public read-accessor. Bodying it
// faithfully would require adding getters to BrnProfile.h, which this partfile may not
// do -- reported blocked instead.
// ============================================================================

namespace
{
    // The console's bundle / resource names, read off the X360 rodata LoadStreetData's
    // switch loads (0x8234F630: `lis/addi` of the "STREETDATA.DAT" and "StreetData"
    // literals). Both legs use event id 1 (`li r?, 1`); pool 5 == the GameData pool.
    const char* const KPC_STREET_DATA_FILE_NAME     = "STREETDATA.DAT";
    const char* const KPC_STREET_DATA_RESOURCE_NAME = "StreetData";
    const s32         KI_STREET_DATA_EVENT_ID       = 1;
    const s32         KI_STREET_DATA_POOL_ID        = 5;

    // LoadDistrictMap's own literals (asm 0x8234FBF8-FC28: the "Districts" rodata string,
    // `li r10, 1` == the event id, `li r10, 5` == the GameData pool). Same pool/event id as
    // the street-data leg above; named separately because they are that function's operands.
    const char* const KPC_DISTRICT_MAP_RESOURCE_NAME = "Districts";
    const s32         KI_DISTRICT_MAP_EVENT_ID       = 1;
    const s32         KI_DISTRICT_MAP_POOL_ID        = 5;

    // The X360 baked assert path/line for the meLoadStage default case.
    const char* const KPC_STREET_MANAGER_FILE =
        "..\\..\\..\\GameSource\\Gamestate/StreetData/BrnGameStateStreetManager.cpp";
}

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// StreetManager::LoadStreetData  @ 0x8234F630   ⭐ THE STREETDATA.DAT LOADER
//
// The resumable five-stage machine Prepare2 gates on -- structurally the twin of
// ProgressionManager::LoadProgressionData @0x82399ED0 and TriggerQueryManager::Prepare
// @0x82398218 (LoadBundle -> acquire -> bind). It is the ONLY writer of mpStreetData
// (X360 `a1 + 7368` == +0x1CC8), so nothing else can make GetStreetData() answer
// non-null; the stage word is `*(a1 + 7632)` == meLoadStage (+0x1DD0).
//
//   0 E_LOAD_NOT_STARTED    : GetResourceRequestInterface(out)->LoadBundle(&rq, 1, pool 5,
//                             "STREETDATA.DAT", useHDCache 0); rq.Clear(); stage = 1;
//                             FALL THROUGH into case 1 (the console `goto LABEL_3`).
//     ⚠️ ORDER: the console issues the LoadBundle FIRST and clears the receiver queue
//     AFTER (0x8234F630's case 0 is `LoadBundle(...)` then `BaseEventReceiverQueue::
//     Clear(v4)`), the mirror image of LoadProgressionData's clear-then-request. The
//     request record is already queued on the OUTPUT request interface by then, so the
//     clear only drops stale replies; reproduced in the console's order.
//   1 E_LOAD_REQUESTED      : reply not in yet (`if (!v4[2])`) -> return false. Otherwise
//                             rq.Clear(); stage = 2; fall through (`goto LABEL_7`).
//   2 E_ACQUIRE_NOT_STARTED : AcquireResource(&rq, 1, pool 5, "StreetData"); stage = 3;
//                             FALL THROUGH into case 3 (the console `goto LABEL_8`) --
//                             which then finds the queue empty this tick and returns false.
//   3 E_ACQUIRE_REQUESTED   : reply not in yet -> return false. Otherwise walk EVERY queued
//                             event, CreateFromHandle(&mpStreetData, response handle), and
//                             (dev builds only) print the loaded road table; stage = 4;
//                             return true.
//   4 E_LOAD_COMPLETE       : return true.
//   default                 : assert "StreetManager::meLoadStage in a weird state"
//                             (BrnGameStateStreetManager.cpp:3394) then return TRUE -- the
//                             console falls into LABEL_36 (`result = 1`), not into a false.
//
// ⚠️ The Hex-Rays `HashString("StreetData") | 0x500000000LL` is the same store-fusion
// artifact LoadProgressionData / LoadDistrictMap document: pool id 5 and the
// zero-extended 32-bit resource id are two INDEPENDENT stores into the 24-byte acquire
// record (AddEvent type 4). RequestInterface<3072>::AcquireResource builds it -- the
// de-inlined form of the console's stack record.
//
// MEASURED against the shipped build/game/STREETDATA.DAT: bnd2 v2, platform 4, ONE
// resource, id 0xBC9CC502 == HashString("StreetData"), type 0x10018 (65560) ==
// BrnStreetData::StreetDataResourceType::GetTypeID.
// ----------------------------------------------------------------------------
bool StreetManager::LoadStreetData( GameStateModuleIO::OutputBuffer* lpOutput,
                                    CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
{
    switch ( meLoadStage )
    {
        case E_LOAD_NOT_STARTED:
        {
            lpOutput->GetResourceRequestInterface()->LoadBundle(
                lpReceiverQueue, KI_STREET_DATA_EVENT_ID, KI_STREET_DATA_POOL_ID,
                KPC_STREET_DATA_FILE_NAME, /*lbUseHDCache*/ false );

            lpReceiverQueue->Clear();
            meLoadStage = E_LOAD_REQUESTED;
        }
            // fall through -- the X360 `goto LABEL_3` polls on the same tick.

        case E_LOAD_REQUESTED:
            if ( lpReceiverQueue->GetCount() == 0 )
            {
                return false;   // still waiting for the bundle-load reply
            }
            lpReceiverQueue->Clear();
            meLoadStage = E_ACQUIRE_NOT_STARTED;
            // fall through (`goto LABEL_7`).

        case E_ACQUIRE_NOT_STARTED:
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                lpReceiverQueue, KI_STREET_DATA_EVENT_ID, KI_STREET_DATA_POOL_ID,
                KPC_STREET_DATA_RESOURCE_NAME );
            meLoadStage = E_ACQUIRE_REQUESTED;
            // fall through -- the console does NOT return here (`goto LABEL_8`); the queue
            // it just cleared is empty, so the poll below returns false on this tick.

        case E_ACQUIRE_REQUESTED:
        {
            // ⚠️ TWO tests, exactly as the console: `if (v9)` gates the whole stage (zero == the
            // response has not arrived, return false), and a SEPARATE `if (v9 > 0)` gates the walk.
            // A negative count therefore still completes the stage without binding anything --
            // reproduced rather than collapsed into a single `<= 0`.
            const s32 liQueuedCount = lpReceiverQueue->GetCount();
            if ( liQueuedCount == 0 )
            {
                return false;   // still waiting for the acquire response
            }

            const CgsModule::Event* lpEvent = NULL;
            s32                     liSize  = 0;
            if ( liQueuedCount > 0 )
            {
                lpReceiverQueue->GetFirstEvent( &lpEvent, &liSize );
            }

            while ( lpEvent != NULL )
            {
                // reinterpret_cast, not static_cast: CgsResource::Events::Event and
                // CgsModule::Event are unrelated roots and the receiver queue hands out the
                // module one (the idiom LoadProgressionData / TriggerQueryManager::Prepare use).
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEvent );

                // X360 `CreateFromHandle(a1 + 7368, v11 + 24)` -- payload +0x18 is the
                // response's {mpResourceMemory, mpSourceEntry} pair, i.e. a ResourceHandle.
                // Read BY MEMBER: the host handle is 16 bytes where the console's is 8, so
                // every literal offset shifts.
                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                mpStreetData = lHandle;   // ResourcePtr::operator=(handle) -> CreateFromHandle

                // The console advances the iterator BEFORE it prints (GetNextEvent at
                // 0x8234F6F4 sits between the CreateFromHandle and the "LOADING ROADS" banner);
                // the two do not interact, but the order is kept.
                const CgsModule::Event* lpNext = NULL;
                lpReceiverQueue->GetNextEvent( lpEvent, &lpNext, &liSize );

                // The console's own road-table dump, gated on the dev message filter. It is
                // NOT a bring-up diagnostic: the whole block sits inside the X360's
                // `if ((gxMessageFilterFlags & 1) != 0)` guards at 0x8234F7xx, and the three
                // literals below are that function's rodata.
                if ( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
                  && CgsDev::Log::gpDebugPrint != 0 )
                {
                    *CgsDev::Log::gpDebugPrint << "\n\n\n***LOADING ROADS*****************\n";

                    for ( BrnStreetData::RoadIndex liRoadIndex = 0;
                          liRoadIndex < mpStreetData->GetRoadCount();
                          ++liRoadIndex )
                    {
                        // GetRoad carries the console's own inlined
                        // "liIndex < miRoadCount && liIndex >= 0" bounds assert (BrnStreetData.h:621).
                        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liRoadIndex );

                        CgsResource::ID lRoadId;
                        lRoadId.SetHash( lpRoad->GetId() );

                        *CgsDev::Log::gpDebugPrint << "Road " << liRoadIndex << " has ID ";
                        *CgsDev::Log::gpDebugPrint << lRoadId;
                        *CgsDev::Log::gpDebugPrint << " & is named ";
                        *CgsDev::Log::gpDebugPrint << lpRoad->GetDebugName();
                        *CgsDev::Log::gpDebugPrint << "\n";
                    }

                    *CgsDev::Log::gpDebugPrint
                        << "\n***DONE LOADING ROADS*****************\n\n\n";
                }

                lpEvent = lpNext;
            }

            meLoadStage = E_LOAD_COMPLETE;
            return true;
        }

        case E_LOAD_COMPLETE:
            return true;

        default:
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert( "StreetManager::meLoadStage in a weird state",
                                        CGS_ASSERT_LOCATION(KPC_STREET_MANAGER_FILE, 3394) );
            CgsDev::Assert::EndAssert();
            // The console falls into LABEL_36 -- `result = 1` -- so a corrupt stage word
            // reports DONE rather than wedging the caller.
            return true;
    }
}

// ----------------------------------------------------------------------------
// StreetManager::LoadAIData  @ 0x8234FA70
//
// AI-lanes streaming machine. NOT_STARTED/REQUESTED/ACQUIRE_NOT_STARTED all issue the
// GetAILanes request onto the receiver queue, advance to ACQUIRE_REQUESTED and poll it
// in the same call; once the response arrives (GetEventId() == 1) bind mpAISectionData
// from the response handle and finish. Returns true only at E_AI_DATA_LOAD_COMPLETE.
// ----------------------------------------------------------------------------
bool StreetManager::LoadAIData( GameStateModuleIO::OutputBuffer* lpOutput,
                                CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
{
    switch ( meAILoadStage )
    {
        case E_AI_DATA_LOAD_NOT_STARTED:
        case E_AI_DATA_LOAD_REQUESTED:
        case E_AI_DATA_ACQUIRE_NOT_STARTED:
        {
            lpReceiverQueue->Clear();

            // The X360 gets the output buffer's resource-request interface (this+0x3414 ==
            // BrnResource::GameDataIO::RequestInterface<3072>) and calls GetAILanes on it. The
            // committed OutputBuffer accessor returns the placeholder ResourceRequestInterface
            // (same object; see BrnGameStateModuleIO.h "+0x3414 (RequestInterface<3072>)"), so it
            // is treated as the concrete request interface to issue the request-builder call.
            BrnResource::GameDataIO::RequestInterface<3072>* lpRequestInterface =
                reinterpret_cast<BrnResource::GameDataIO::RequestInterface<3072>*>(
                    lpOutput->GetResourceRequestInterface() );
            lpRequestInterface->GetAILanes( lpReceiverQueue, 1, 5 );

            meAILoadStage = E_AI_DATA_ACQUIRE_REQUESTED;
        }
        // Fall through to the case-3 poll.

        case E_AI_DATA_ACQUIRE_REQUESTED:
        {
            if ( lpReceiverQueue->GetCount() == 0 )
            {
                return false;   // still waiting for the AI-data response
            }

            const CgsModule::Event* lpEvent = NULL;
            s32 liEventSize = 0;
            lpReceiverQueue->GetFirstEvent( &lpEvent, &liEventSize );
            CGS_ASSERT( lpEvent != NULL, CGS_ASSERT_MESSAGE_AT("lpEvent != NULL", CGS_ARTIST_FILE, 3464) );

            const BrnResource::GameDataIO::GameDataEvent* lpAIDataResponse =
                reinterpret_cast<const BrnResource::GameDataIO::GameDataEvent*>( lpEvent );
            CGS_ASSERT( lpAIDataResponse->miEventId == 1, CGS_ASSERT_MESSAGE_AT("lpAIDataResponse->GetEventId() == 1", CGS_ARTIST_FILE, 3468) );

            // ARTIST 0x8234FA70 binds the received AI-lanes resource before
            // marking the load complete (same native-width handle as Progression).
            const BrnResource::GameDataIO::GameDataAssetEvent* asset =
                reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
            mpAISectionData = static_cast<const BrnResource::GameDataIO::GetAIDataResponse*>(asset)->GetAIDataHandle();

            meAILoadStage = E_AI_DATA_LOAD_COMPLETE;
            return true;
        }

        case E_AI_DATA_LOAD_COMPLETE:
            return true;

        default:
            return false;
    }
}

// ----------------------------------------------------------------------------
// StreetManager::LoadDistrictMap  @ 0x8234FB98
//
// District-map streaming machine. LOAD_REQUEST/LOAD_RESPONSE/ACQUIRE_REQUEST all queue
// the "Districts" AcquireResource request (24-byte record, event type 4) onto the
// <3072,16> request queue and advance to ACQUIRE_RESPONSE; that stage waits for the
// response, stamps mDistrictMapResourceHandle from it and advances to DONE. Returns true
// only at E_DISTRICT_MAP_DONE.
// ----------------------------------------------------------------------------
bool StreetManager::LoadDistrictMap( GameStateModuleIO::OutputBuffer* lpOutput,
                                     CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
{
    switch ( meDistrictMapLoadStage )
    {
        case E_DISTRICT_MAP_LOAD_REQUEST:
        case E_DISTRICT_MAP_LOAD_RESPONSE:
        case E_DISTRICT_MAP_ACQUIRE_REQUEST:
        {
            lpReceiverQueue->Clear();

            // Build the "Districts" acquire request: { mpUser = receiver queue, miEventId = 1,
            // miPoolId = 5, mResourceId = HashString("Districts") } and AddEvent it (type 4)
            // onto the request interface's <3072,16> queue (this+0x3414 ==
            // RequestInterface<3072>, its VariableEventQueue<3072,16> at offset 0).
            //
            // CONSOLE attestation (asm 0x8234FC08..0x8234FC38), store for store:
            //   stw r31, var_40 (+0)     = &the receiver queue
            //   stw r10(1), var_3C (+4)  = miEventId
            //   std r11,  var_30 (+0x10) = the RAW HashString("Districts") return value
            //   stw r10(5), var_38 (+8)  = miPoolId
            //   li r5,4 / li r6,0x18 -> AddEvent(type 4, size 24 == the 32-BIT sizeof)
            //
            // ⚠️⚠️ TWO LIVE BUGS FIXED HERE 2026-08-11 -- this is what printed
            // `[StreetManager] district map: handle=0` in the last boot log while
            // DISTRICTS.DAT was demonstrably resident ("LoadBundle 'Districts.dat' -> pool 5:
            // 1 resources"). Both were console-literal transcriptions that do not survive the
            // x64 host:
            //
            //  (1) THE ID WAS TAGGED `| 0x500000000`. That is a Hex-Rays STORE-FUSION artifact:
            //      the decompiler folded the separate `li r10, 5 / stw var_38` miPoolId store
            //      into the `std` of the hash. HashString @0x828D84A8 ends `clrldi r3, r3, 32`,
            //      so the id's high dword is ZERO, and Pool::FindResource compares the whole
            //      64-bit value -- a tagged id matches nothing, the pool replies with the
            //      both-null handle, and the bind below stamps NULL. The same artifact family
            //      was already retired in RequestInterface<N>::AcquireResource (the `<< 48`
            //      form), in BrnWorldModule::LoadAttribSysVault (`| 0x700000000`) and, this
            //      wave, in BrnWorldModule::LoadDistrictMap. The sibling loader in this very
            //      file -- LoadStreetData -- goes through the untagged AcquireResource builder,
            //      which is exactly why STREETDATA.DAT binds and the district map did not.
            //
            //  (2) RequestInterface posts sizeof(AcquireResourceRequest), keeping
            //      the producer paired with the shared record. The canonical record
            //      is 24 bytes here too: pointer widening consumes its header gap,
            //      and the full 64-bit ID is its only payload after PoolEvent.
            //
            // The request is issued through the de-inlined builder the console folded in
            // (RequestInterface<3072>::AcquireResource -- same type 4, same four fields, and
            // it is the identical call LoadStreetData's acquire leg already makes).
            lpOutput->GetResourceRequestInterface()->AcquireResource(
                lpReceiverQueue, KI_DISTRICT_MAP_EVENT_ID, KI_DISTRICT_MAP_POOL_ID,
                KPC_DISTRICT_MAP_RESOURCE_NAME );

            meDistrictMapLoadStage = E_DISTRICT_MAP_ACQUIRE_RESPONSE;
            return false;
        }

        case E_DISTRICT_MAP_ACQUIRE_RESPONSE:
        {
            if ( lpReceiverQueue->GetCount() <= 0 )
            {
                return false;   // still waiting for the acquire response
            }
            meDistrictMapLoadStage = E_DISTRICT_MAP_DONE;

            // ⭐ THE HANDLE BIND IS REAL NOW (2026-08-11). The console inlines GetFirstEvent
            // here -- `v8 = (a3[2] <= 0) ? 0 : a3[3] + *a3 + 8` is exactly the queue's
            // buffer-base + first-event-offset + the 8-byte event header, i.e. the payload
            // pointer -- and then reads the pair at `v8 + 24`:
            //     a1[1858] = *(v8+24);   a1[1859] = *(v8+28);
            // 1858*4 == 0x1D08 == mDistrictMapResourceHandle.mpResourceMemory, 1859*4 == 0x1D0C
            // == .mpSourceEntry. The record at payload +0x18 IS the AcquireResourceResponse's
            // {mpResourceMemory, mpSourceEntry} pair (PoolModule::DoAcquireResourceRequest
            // @0x828FCD48 builds it), so it is read BY MEMBER -- never at the console's literal
            // +0x18/+0x1C, because the host handle is 16 bytes where the console's is 8 and
            // every literal past it shifts. Same idiom as LoadStreetData above and
            // TriggerQueryManager::Prepare's acquire leg.
            const CgsModule::Event* lpEvent = NULL;
            s32                     liSize  = 0;
            lpReceiverQueue->GetFirstEvent( &lpEvent, &liSize );

            if ( lpEvent != NULL )
            {
                // reinterpret_cast, not static_cast: CgsResource::Events::Event and
                // CgsModule::Event are unrelated roots and the receiver queue hands out the
                // module one.
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>( lpEvent );

                mDistrictMapResourceHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                mDistrictMapResourceHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
            }
            return false;
        }

        case E_DISTRICT_MAP_DONE:
            return true;

        default:
            CGS_ASSERT( false, CGS_ASSERT_MESSAGE_AT("Unknown meDistrictMapLoadStage", CGS_ARTIST_FILE, 3573) );
            return false;
    }
}

// ----------------------------------------------------------------------------
// StreetManager::GetStreetData   (DWARF BrnGameStateStreetManager.h:326)
//
// [gateui r4] ADDITIVE 2026-08-20. Declaration-only until now, which made
// BrnRoadRulesManager.cpp unmountable (LNK2019 -- verify_r3_fix3gsm F1).
//
// There is NO X360 symbol for this accessor: it is absent from the ledger and
// every caller inlines it. RoadRulesManager::IsRoadLimitRegionValid @0x82335268
// shows the whole body at its call site --
//     0x82335280  lwz   r11, 0x14(r28)          ; this->mpStreetManager
//     0x82335284  addi  r3, r11, 0x1CC8         ; == 7368 == &mpStreetData
//     0x82335288  bl    BrnStreetData__StreetData___oper   ; ResourcePtr<>::operator->
// -- i.e. exactly `mpStreetData.operator->()`, whose own baked assert
// ("Can not instance resource pointer - it has no main memory resource")
// therefore stays the one this path fires. mpStreetData is the +0x1CC8 member
// (BrnGameStateStreetManager.h) and LoadStreetData above is its only writer.
//
// Written as an explicit `.operator->()` rather than `mpStreetData.GetMemoryResource()`
// so the console's assert line (CgsResourcePtr.h:544) is the one reproduced.
// ----------------------------------------------------------------------------
// (Bodied in the header, as the original: see BrnGameStateStreetManager.h.)

// ============================================================================================
// [event-starts producer wave 2026-08-27] Two reads this manager already OWNED but never
// published, both needed by GameStateModule::SendSetUpAllEventStartsMessage @0x823759D0.
// Homed here because this is the TU that BINDS both members (LoadAIData binds mpAISectionData,
// LoadDistrictMap stores mDistrictMapResourceHandle) -- keeping producer and reader in one file.
// ============================================================================================

// DWARF BrnGameStateStreetManager.h:468 -- declared since the keystone landed, bodied now.
// The X360 emits no standalone symbol (StreetManager::SetupParRivals @0x8233F5E0 reaches the
// handle by the inline `this + 0x1D08` adjust); this accessor IS that adjust.
const CgsResource::ResourceHandle* StreetManager::GetDistrictMapResourceHandle() const
{
    return &mDistrictMapResourceHandle;
}

} // namespace BrnGameState

#include "SharedClasses/AI/AISectionsResourceType.h"
namespace BrnGameState {
// ARTIST 0x82326478: the section's signed span index selects a Street.
BrnStreetData::RoadIndex StreetManager::GetRoadIndexFromAISectionIndex(u16 section)
{
    if (section == 0x7FFF) return BrnStreetData::KI_INVALID_ROAD_INDEX;
    const s16 span = mpAISectionData->GetAISection(section)->miSpanIndex;
    if (span == -1 || span >= mpStreetData->GetStreetCount()) return BrnStreetData::KI_INVALID_ROAD_INDEX;
    return mpStreetData->GetStreet(span)->GetRoadIndex();
}
// ARTIST 0x8230F8F8 (64-bit ID return).
CgsID StreetManager::GetParRivalId(s32 road, BrnStreetData::ScoreType type)
{
    CGS_ASSERT(static_cast<u32>(road) < KI_MAX_CHALLENGES, CGS_ASSERT_MESSAGE_AT("liRoadIndex >= 0 && liRoadIndex < KI_MAX_CHALLENGES", CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER, 972));
    CGS_ASSERT(static_cast<u32>(type) < BrnStreetData::E_SCORE_TYPE_COUNT, "Invalid score type");
    return maaParRivalIds[road][type];
}
}

namespace BrnGameState {
BrnStreetData::RoadIndex StreetManager::GetCurrentPlayerRoadIndex() { return miCurrentPlayerRoadIndex; }
}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wC_04.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_04.cpp
//   (wave C partfile -- group 4 "query + rivals + complete tally")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::FillInRoadRulesQuery                    @ 0x823365A8
//   BrnGameState::StreetManager::FindRivalsByDistrict                    @ 0x82336360
//   BrnGameState::StreetManager::GetNumberOfCompleteRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F350
//
// Every asm store / branch / call has a counterpart here; all state is reached
// through the frozen-header named members (BrnGameStateStreetManager.h), never a
// raw-offset cast:
//   * `GetStreetData()` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper/operator-_ (ResourcePtr<StreetData>::operator->);
//     the `lwz r11, 0x20(r3)` that follows it is the inlined StreetData::GetRoadCount
//     (miRoadCount @ StreetData+0x20), and the
//     `lwz r11,0x10(rX) / add / ld 0x10(r11)` triple is the inlined
//     StreetData::GetRoad(i)->GetId() (road table @ +0x10, 64-byte stride, mId @ +0x10)
//     -- the bounds assert baked at BrnStreetData.h:621 belongs to GetRoad, so it is
//     not duplicated here.
//   * the ProgressionManager (this+0x1D10) counter at +133464 (0x20958) is reached
//     through the committed Get/SetNumberOfCompleteRoadRulesRuledByPlayer accessors.
//   * `mpProgressionManager->GetProgressionData()` == the asm's null-checked
//     ResourcePtr<ProgressionData> read at pm+133348 (0x208E4); the rival table/count
//     (ProgressionData +0x28/+0x2C) is reached through GetRival/GetRivalCount, and the
//     baked "liIndex < miRivalCount" assert (BrnProgressionData.h:460) belongs to
//     GetRival, so it is not duplicated here either.
//
// TRAPS kept faithful:
//   * every count the X360 re-materialises on each loop-condition evaluation
//     (RoadRulesBatchQueryAction::miNumRoads, StreetData::GetRoadCount(),
//     ProgressionData::GetRivalCount()) stays IN the loop condition -- not hoisted.
//   * FillInRoadRulesQuery stores miNumRoads BEFORE the <= KI_MAX_CHALLENGES assert
//     (asm: `stw r11, 0x200(r26)` at 0x823365D0 precedes the `ble`).
//   * FindRivalsByDistrict dereferences the progression data without a null guard --
//     the X360 loads GetRivalCount off a possibly-NULL pointer (0x823363AC reads
//     0x2C(r28) with r28 == 0 on the null path); reproduced as-is, no added check.
//   * `(_cntlzw(HasPlayerBeaten...Score(...) - 1) & 0x20) != 0` is the compiler's
//     "== 1" test, i.e. == E_ROAD_RULE_COMPLETION_STATUS_BEATEN.
// ===========================================================================

namespace BrnGameState
{

// @ 0x823365A8. Fill the road-rules batch query action the GUI issues: the road count,
// one road id per road, the four per-road "beaten" booleans (par TIME/CRASH then friend
// TIME/CRASH -- the X360 issues the four calls in exactly that order), and finally the
// offline owns-all-roads flag taken from the ProgressionManager's complete-rules tally.
void StreetManager::FillInRoadRulesQuery( GameStateModuleIO::RoadRulesBatchQueryAction* lpRulesQueryAction )
{
    // ARTIST 0x823365C4 and 0x82336628 use the mutable ResourcePtr overload.
    lpRulesQueryAction->miNumRoads = mpStreetData->GetRoadCount();

    CGS_ASSERT( lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES,
                CGS_ASSERT_MESSAGE_AT("lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 4295) );

    for ( s32 liRoadIndex = 0; liRoadIndex < lpRulesQueryAction->miNumRoads; ++liRoadIndex )
    {
        lpRulesQueryAction->maRoadIds[liRoadIndex] = mpStreetData->GetRoad( liRoadIndex )->GetId();

        lpRulesQueryAction->mabPlayerBeatenParTime[liRoadIndex] =
            HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBeatenParCrash[liRoadIndex] =
            HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBestOnlineTime[liRoadIndex] =
            HasPlayerBeatenFriendScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;

        lpRulesQueryAction->mabPlayerBestOnlineCrash[liRoadIndex] =
            HasPlayerBeatenFriendScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
            == E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
    }

    // Signed compare (cmpwi cr6, r11, 0x40 / bge) of the ProgressionManager's
    // complete-road-rules tally against KI_MAX_CHALLENGES.
    lpRulesQueryAction->mbPlayerOwnsAllRoadsOffline =
        static_cast<s32>( mpProgressionManager->GetNumberOfCompleteRoadRulesRuledByPlayer() ) >= KI_MAX_CHALLENGES;
}

// @ 0x82336360. FindRivalsByDistrict WAS DEFINED HERE; it was SPLIT OUT 2026-08-11 into the
// sibling BrnGameStateStreetManager_FindRivalsByDistrict.cpp so that SetupParRivals' one
// unhomed callee could be mounted without this partfile's two other functions. MEASURED
// (cl /c + dumpbin /SYMBOLS vs the defined-symbol set of build\game\obj): mounting this whole
// partfile costs FOUR unresolved externals -- StreetManager::GetStreetData,
// ::HasPlayerBeatenParScore, ::HasPlayerBeatenFriendScore and Rival::GetDistrict -- and all but
// the last are pulled in ONLY by the two functions that remain here. Fold the split file back
// in when those three score accessors land. Do NOT re-add the body: two definitions is LNK2005.

// @ 0x8233F350. Tally the roads whose par score the local player has beaten for BOTH
// score types (a "complete" road rule), then mirror the tally into the ProgressionManager
// counter (+133464). Unlike the two single-type tallies this store is UNCONDITIONAL --
// the X360 has no max-guard here, so the counter can go down.
s32 StreetManager::GetNumberOfCompleteRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN
             && HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
                == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    mpProgressionManager->SetNumberOfCompleteRoadRulesRuledByPlayer( liNumberOfRoadsRuled );

    return liNumberOfRoadsRuled;
}

}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wC_05.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_05.cpp
//   (wave C partfile -- group 5 "tiny trio")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::GetChallengeParScore                    @ 0x82336168
//   BrnGameState::StreetManager::GetNumberOfParShowTimeRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F230
//   BrnGameState::StreetManager::GetNumberOfParTimeTrialRoadsRuledByLocalPlayer
//                                                                        @ 0x8233F2C0
//
// Every asm store / branch / call has a counterpart here; all state is reached
// through the frozen-header named members (BrnGameStateStreetManager.h), never a
// raw-offset cast:
//   * `mpStreetData->` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper (ResourcePtr<StreetData>::operator->);
//     the `lwz r11, 0x20(r3)` that follows it is the inlined
//     StreetData::GetRoadCount (miRoadCount @ StreetData+0x20).
//   * the tally writebacks target the ProgressionManager (this+0x1D10) counters
//     +133456 (0x20950, par-crash) and +133460 (0x20954, par-time) through the
//     committed Get/SetNumberOfPar{Crash,Time}RoadRulesRuledByPlayer accessors.
//
// TRAP kept faithful: the road count is re-materialised (operator-> + load) on
// EVERY loop-condition evaluation in the X360 code, so GetRoadCount() stays in
// the loop condition and is not hoisted into a local.
// ===========================================================================

namespace BrnGameState
{

// @ 0x82336168. Copy the compiled par-score record for a challenge road out of the
// street data. The bounds check belongs to StreetData::GetChallengeParScore (its own
// baked assert); this function only owns the null-destination assert (baked line 3750).
void StreetManager::GetChallengeParScore( BrnStreetData::ChallengeIndex liIndex,
                                          BrnStreetData::ChallengeParScoresEntry* lpData ) const
{
    CGS_ASSERT( lpData, CGS_ASSERT_MESSAGE_AT("lpData", CGS_ARTIST_FILE, 3750) );

    lpData->Copy( mpStreetData->GetChallengeParScore( liIndex ) );
}

// @ 0x8233F230. Tally the roads whose CRASH ("show time") par score the local player
// has beaten, then raise the ProgressionManager's par-crash counter to that tally --
// the X360 store is guarded by a signed greater-than compare, so the counter only ever
// grows here.
s32 StreetManager::GetNumberOfParShowTimeRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    if ( liNumberOfRoadsRuled >
         static_cast<s32>( mpProgressionManager->GetNumberOfParCrashRoadRulesRuledByPlayer() ) )
    {
        mpProgressionManager->SetNumberOfParCrashRoadRulesRuledByPlayer( liNumberOfRoadsRuled );
    }

    return liNumberOfRoadsRuled;
}

// @ 0x8233F2C0. TIME twin of the above, maxing the ProgressionManager's par-time
// counter (+133460).
s32 StreetManager::GetNumberOfParTimeTrialRoadsRuledByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( s32 liRoadIndex = 0; liRoadIndex < GetStreetData()->GetRoadCount(); ++liRoadIndex )
    {
        if ( HasPlayerBeatenParScore( liRoadIndex, BrnStreetData::E_SCORE_TYPE_TIME )
             == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
        {
            ++liNumberOfRoadsRuled;
        }
    }

    if ( liNumberOfRoadsRuled >
         static_cast<s32>( mpProgressionManager->GetNumberOfParTimeRoadRulesRuledByPlayer() ) )
    {
        mpProgressionManager->SetNumberOfParTimeRoadRulesRuledByPlayer( liNumberOfRoadsRuled );
    }

    return liNumberOfRoadsRuled;
}

}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wC_06.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_06.cpp
//   (wave C partfile -- group 6 "road-rule beaten predicates")
//
// [pause-stats wave 2026-08-29] The two predicates the three roads-ruled tallies in
// _wC_04 / _wC_05 count with. They were the last unresolved externals between the pause
// screen's stat panel and a green link:
//
//   BrnGameState::StreetManager::HasPlayerBeatenParScore    @ 0x823361D0  (52 insns)
//   BrnGameState::StreetManager::HasPlayerBeatenFriendScore @ 0x823362A0  (48 insns)
//
// Both are the same five-step shape and both return the three-valued
// StreetManager::ERoadRuleCompletionStatus, NOT a bool:
//   NO_DATA(0) / BEATEN(1) / NOT_BEATEN(2).
//
// ⚠️ THE TWO "MISSING DATA" ARMS ANSWER DIFFERENTLY, AND IT IS NOT SYMMETRY-BREAKING NOISE:
//   * no USER score      -> NOT_BEATEN in both (`li r3, 2` @0x82336228 / @0x823362F0)
//   * no PAR score       -> NO_DATA        (`li r3, 0` @0x82336294)
//   * no FRIEND score    -> BEATEN         (`li r3, 1` @0x82336354)
// i.e. an unset par score means "this road has no rule to beat", while an unset friend score
// means "you are ahead of your friends by default". Both are the binary's.
//
// ⚠️ CompareScores IS STATIC AND ITS FIRST ARGUMENT IS THE SCORE TYPE. The console loads
// `mr r3, r31` (the ScoreType parameter) into the `this` slot -- BrnChallengeData.h:144
// already documents and declares it static for exactly this reason. `> 0` means the user's
// score is worse, hence NOT_BEATEN; `<= 0` (equal counts) is BEATEN.
//
// ⚠️ THE PAR RECORD IS FETCHED THROUGH THE STREET DATA, NOT THROUGH
// StreetManager::GetChallengeParScore @0x82336168 -- the console INLINES that accessor here
// (`addi r3, this, 0x1CC8` + ResourcePtr operator-> + StreetData::GetChallengeParScore, then
// ChallengeParScoresEntry::Copy onto a stack local), which is why no call to it appears.
// ===========================================================================

namespace BrnGameState
{

// @ 0x823361D0. Has the local player beaten the authored PAR score for this road/score type?
ERoadRuleCompletionStatus
StreetManager::HasPlayerBeatenParScore( BrnStreetData::ChallengeIndex liIndex,
                                        BrnStreetData::ScoreType leScoreType )
{
    // `li r6, 0` -- the console asks by CHALLENGE index, not by road index.
    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, false );

    // The inlined StreetManager::GetChallengeParScore (see the banner).
    BrnStreetData::ChallengeParScoresEntry lParScore;
    lParScore.Copy( GetStreetData()->GetChallengeParScore( liIndex ) );

    if ( !lUserScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    if ( lParScore.ContainsData( leScoreType ) )
    {
        const s32 liPlayerScore = lUserScore.GetScore( leScoreType );
        s32 liParScore;
        CgsID lParRivalId;
        lParScore.GetScore( leScoreType, &liParScore, &lParRivalId );

        if ( BrnStreetData::ChallengeData::CompareScores( leScoreType, liPlayerScore, liParScore ) > 0 )
        {
            return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
        }
        return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
    }
    return E_ROAD_RULE_COMPLETION_STATUS_NO_DATA;
}

// @ 0x823362A0. The friend-table twin: has the local player beaten his friends' best?
ERoadRuleCompletionStatus
StreetManager::HasPlayerBeatenFriendScore( BrnStreetData::ChallengeIndex liIndex,
                                           BrnStreetData::ScoreType leScoreType )
{
    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, false );

    BrnStreetData::ChallengeHighScoreEntry lFriendScore;
    GetChallengeFriendHighScore( liIndex, &lFriendScore, false );

    if ( !lUserScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    // No friend entry -> BEATEN, unlike the par version's NO_DATA. See the banner.
    if ( !lFriendScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
    }

    const s32 liPlayerScore = lUserScore.GetScore( leScoreType );

    // ⓘ the friend record's second out-param is the friend's NAME, not a CgsID -- the
    // ChallengeHighScoreEntry twin of the par record's rival id (BrnChallengeHighScoreEntry.h:40).
    s32                     liFriendScore;
    CgsNetwork::PlayerName  lFriendName;
    lFriendScore.GetScore( leScoreType, &liFriendScore, &lFriendName );

    if ( BrnStreetData::ChallengeData::CompareScores( leScoreType, liPlayerScore, liFriendScore ) > 0 )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
}

}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wB_02.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_02.cpp  --  wave-B group 3 ("get-scores")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::GetChallengeUserScore       @ 0x82335E08
//   StreetManager::GetChallengeFriendHighScore @ 0x82335F30
//
// (GetChallengeParScore @ 0x82336168 is deferred: the frozen BrnStreetData.h models
// BrnStreetData::ChallengeParScoresEntry as a plain POD with no Copy() accessor, so
// its faithful `ChallengeParScoresEntry::Copy(lpData, ...)` store cannot be spelled
// without editing a frozen header -- see funcs_blocked.)
//
// Both copy a challenge record out of a live table into the caller's entry,
// optionally remapping a road index through KAA_SAVE_GAME_CHALLENGE_ROAD_IDS
// (the inlined qword_82029FA0 scan). mpStreetData->... is the committed
// ResourcePtr operator-> (X360 StreetData_::oper on &mpStreetData, guest +0x1CC8).
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// ARTIST 0x82335E74, 0x82335FE8 and 0x82349FB8 inline the slot lookup.
inline s32 StreetManager::GetSaveGameChallengeIndexByRoadID( ::CgsID lRoadID ) const
{
    for ( s32 liChallengeIndex = 0; liChallengeIndex < KI_MAX_CHALLENGES; ++liChallengeIndex )
    {
        if ( KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liChallengeIndex] == lRoadID )
        {
            return liChallengeIndex;
        }
    }

    return -1;
}

// @ 0x82335E08. Copy the local player's ChallengePlayerScoreEntry for a slot into
// lpData (constructed empty first). When lbByRoadIndex is set, liIndex is first the
// road index and is remapped to its save-game challenge slot via the road id scan;
// a road with no save-game slot leaves lpData at its Construct'd default.
void StreetManager::GetChallengeUserScore( BrnStreetData::ChallengeIndex liIndex,
                                           BrnStreetData::ChallengePlayerScoreEntry* lpData,
                                           bool lbByRoadIndex ) const
{
    CGS_ASSERT( lpData, CGS_ASSERT_MESSAGE_AT("lpData", CGS_ARTIST_FILE, 3593) );

    lpData->Construct();

    BrnStreetData::ChallengeIndex liChallengeIndex = liIndex;
    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liChallengeIndex = GetSaveGameChallengeIndexByRoadID( lpRoad->GetId() );
    }

    if ( liChallengeIndex >= 0 )
    {
        CGS_ASSERT( liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES,
                    CGS_ASSERT_MESSAGE_AT("liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 3606) );

        const BrnStreetData::ChallengePlayerScoreEntry* lpChallenge = &maChallengeData[liChallengeIndex];
        CGS_ASSERT( lpChallenge, CGS_ASSERT_MESSAGE_AT("lpChallenge", CGS_ARTIST_FILE, 3610) );
        lpData->Copy( lpChallenge );
    }
}

// @ 0x82335F30. Friend-table twin of GetChallengeUserScore over
// maNetworkChallengeData. NOTE the index bounds asserts fire up front, before the
// optional by-road-index remap.
void StreetManager::GetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex,
                                                 BrnStreetData::ChallengeHighScoreEntry* lpData,
                                                 bool lbByRoadIndex ) const
{
    CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES,
                CGS_ASSERT_MESSAGE_AT("liIndex < BrnGameState::KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 3624) );
    CGS_ASSERT( liIndex >= 0, CGS_ASSERT_MESSAGE_AT("liIndex >= 0", CGS_ARTIST_FILE, 3625) );
    CGS_ASSERT( lpData, CGS_ASSERT_MESSAGE_AT("lpData", CGS_ARTIST_FILE, 3626) );

    lpData->Construct();

    BrnStreetData::ChallengeIndex liSlotIndex = liIndex;
    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liSlotIndex = GetSaveGameChallengeIndexByRoadID( lpRoad->GetId() );
    }

    if ( liSlotIndex >= 0 )
    {
        const BrnStreetData::ChallengeHighScoreEntry* lpChallenge = &maNetworkChallengeData[liSlotIndex];
        CGS_ASSERT( lpChallenge, CGS_ASSERT_MESSAGE_AT("lpChallenge", CGS_ARTIST_FILE, 3641) );
        lpData->Copy( lpChallenge );
    }
}

} // namespace BrnGameState

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wB_00.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_00.cpp
//   (wave B partfile -- group 0 "lifecycle": Construct / Destruct / Prepare)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::Construct  @ 0x82335978
//   BrnGameState::StreetManager::Destruct   @ 0x82335D40
//   (BrnGameState::StreetManager::Prepare @0x82350900 was split out 2026-08-11 into the
//    sibling BrnGameStateStreetManager_Prepare.cpp -- see the note at the bottom of this file.)
// Every store / branch has an asm counterpart; members are the frozen-header
// named fields (BrnGameStateStreetManager.h), never raw-offset casts.
// ===========================================================================

namespace BrnGameState
{
    // ARTIST's constructor, recursive walk and junction reset drop the visited-section count.
    inline void StreetManager::ClearSectionWalkingData()
    {
        miNextFreeSectionSlot = 0;
    }

    // -----------------------------------------------------------------------
    // @ 0x82335978. Wire the three owner pointers, zero the score tables and the
    // whole walk / upcoming-roads state, construct the embedded debug component,
    // and register the five perf monitors.
    // -----------------------------------------------------------------------
    void StreetManager::Construct( GameStateModule* lpGameStateModule,
                                   BrnProgression::ProgressionManager* lpProgression,
                                   RoadRulesManager* lpRoadRulesManager )
    {
        mStreetManagerDebugComponent.Construct( this );

        CGS_ASSERT( lpProgression, CGS_ASSERT_MESSAGE_AT("lpProgression", CGS_ARTIST_FILE, 151) );
        mpProgressionManager = lpProgression;

        CGS_ASSERT( lpGameStateModule, CGS_ASSERT_MESSAGE_AT("lpGameStateModule", CGS_ARTIST_FILE, 153) );
        mpGameStateModule = lpGameStateModule;

        CGS_ASSERT( lpRoadRulesManager, CGS_ASSERT_MESSAGE_AT("lpRoadRulesManager", CGS_ARTIST_FILE, 155) );
        mpRoadRulesManager = lpRoadRulesManager;

        meLoadStage            = E_LOAD_NOT_STARTED;
        meAILoadStage          = E_AI_DATA_LOAD_NOT_STARTED;
        meDistrictMapLoadStage = E_DISTRICT_MAP_LOAD_REQUEST;

        // Zero both score tables (X360 order: player table then network table).
        memset( maChallengeData, 0, sizeof( maChallengeData ) );
        memset( maNetworkChallengeData, 0, sizeof( maNetworkChallengeData ) );

        mNewHighScoreBuffer.Clear();
        mbTooManyHighScoresToBuffer = false;
        miNumScoresLost             = 0;
        mfUnbufferTimer             = 0.0f;
        meActiveRoadRuleType        = BrnStreetData::E_SCORE_TYPE_COUNT;
        mbFirstDownload             = true;

        for ( BrnStreetData::ScoreType leEnumIndex = BrnStreetData::E_SCORE_TYPE_START;
              leEnumIndex < BrnStreetData::E_SCORE_TYPE_COUNT;
              leEnumIndex++ )
        {
            maRoadRulesChangedBitArrays[leEnumIndex].Construct();
            maRoadRulesChangedBitArrays[leEnumIndex].UnSetAll();
        }

        miCurrentPlayerRoadIndex       = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miLastPlayerRoadIndex          = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mCurrentPlayerSectionExitRight.SetZero();
        mFirstJunctionPosition.SetZero();
        miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
        ClearSectionWalkingData();
        miNewestRightPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mLeftRoadIndex                 = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mRightRoadIndex                = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mfHopefulRoadTimer             = 0.0f;
        mfGuidanceLockTime             = 0.0f;
        mbRightRoadHighlighted         = false;
        mfTimeInJunction               = 0.0f;
        mbLeftRoadHighlighted          = false;
        mfTotalTimeGoinTheWrongWay     = 0.0f;
        mbNeedToUpdateUpcomingRoads    = true;
        mbWrongWay                     = false;
        miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miHopefulForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miHopefulLeftPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miHopefulRightPlayerRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mLastNode.SetZero();
        mSecondLastNode.SetZero();
        miNewRoadNodeIndex             = 0;
        miLastRoadNodeIndex            = 0;
        mbNewRoadIsInterStateExit      = false;
        mbPlayerOnInterstateExit       = false;
        mbLockRightSign                = false;
        mbLockLeftSign                 = false;
        mbJunctionPosSet               = false;
        mbPlayerIsInAShortcut          = false;

        // Register the five perf monitors (page 5, no minimum, 1.0ms budget, lib-perf
        // tagged). The X360 uses the 5-parameter AddMonitor (the float budget reserves
        // the r6 GPR slot, so only r4/r5/f1/r7 are set at each call site).
        miSetRoadRuleChallengeDataPM =
            CgsDev::PerfMonCpu::AddMonitor( "SetRoadRuleChallengeData",
                                            CgsDev::E_PMP_5, false, 1.0f, true );
        miSetRoadRuleNetworkHighScoresPM =
            CgsDev::PerfMonCpu::AddMonitor( "SetRoadRuleNetworkHighScores",
                                            CgsDev::E_PMP_5, false, 1.0f, true );
        miUpcomingRoadsPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoads",
                                            CgsDev::E_PMP_5, false, 1.0f, true );
        miUpcomingRoadsSentMessagePM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsSendMessage",
                                            CgsDev::E_PMP_5, false, 1.0f, true );
        miUpcomingRoadsQSortPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsQSort",
                                            CgsDev::E_PMP_5, false, 1.0f, true );

        CGS_ASSERT( miSetRoadRuleChallengeDataPM >= 0,     CGS_ASSERT_MESSAGE_AT("miSetRoadRuleChallengeDataPM >= 0", CGS_ARTIST_FILE, 224) );
        CGS_ASSERT( miSetRoadRuleNetworkHighScoresPM >= 0, CGS_ASSERT_MESSAGE_AT("miSetRoadRuleNetworkHighScoresPM >= 0", CGS_ARTIST_FILE, 225) );
        CGS_ASSERT( miUpcomingRoadsPM >= 0,                CGS_ASSERT_MESSAGE_AT("miUpcomingRoadsPM >= 0", CGS_ARTIST_FILE, 226) );
        CGS_ASSERT( miUpcomingRoadsSentMessagePM >= 0,     CGS_ASSERT_MESSAGE_AT("miUpcomingRoadsSentMessagePM >= 0", CGS_ARTIST_FILE, 227) );
        CGS_ASSERT( miUpcomingRoadsQSortPM >= 0,           CGS_ASSERT_MESSAGE_AT("miUpcomingRoadsQSortPM >= 0", CGS_ARTIST_FILE, 228) );
    }

    // -----------------------------------------------------------------------
    // @ 0x82335D40. Clear the changed-bit arrays and the buffered-score state,
    // zero both score tables, drop the game-state / progression owner pointers,
    // destruct the embedded debug component.
    // -----------------------------------------------------------------------
    void StreetManager::Destruct()
    {
        for ( BrnStreetData::ScoreType leEnumIndex = BrnStreetData::E_SCORE_TYPE_START;
              leEnumIndex < BrnStreetData::E_SCORE_TYPE_COUNT;
              leEnumIndex++ )
        {
            maRoadRulesChangedBitArrays[leEnumIndex].UnSetAll();
        }

        miNumScoresLost             = 0;
        mbTooManyHighScoresToBuffer = false;
        mNewHighScoreBuffer.Clear();
        mfUnbufferTimer             = 0.0f;
        mbFirstDownload             = true;                                // stb +0x1CC5 after stw +0x1CC0
        meActiveRoadRuleType        = BrnStreetData::E_SCORE_TYPE_COUNT;   // (ARTIST 0x82335DD0/D4)

        memset( maChallengeData, 0, sizeof( maChallengeData ) );
        memset( maNetworkChallengeData, 0, sizeof( maNetworkChallengeData ) );

        mpGameStateModule    = NULL;
        mpProgressionManager = NULL;

        mStreetManagerDebugComponent.Destruct();
    }

    // -----------------------------------------------------------------------
    // @ 0x82350900. StreetManager::Prepare MOVED OUT (2026-08-11, district-map wave) to the
    // sibling TU BrnGameStateStreetManager_Prepare.cpp -- it is the ONE function of this group
    // GameStateModule::Prepare stage 23 needs, and this partfile costs one unresolved external
    // (BrnStreetData::operator++(ScoreType&, int), the street-DATA side's
    // SharedClasses/StreetData/BrnChallengeData.cpp) through Construct's and Destruct's
    // score-type loops. MEASURED with cl /c + dumpbin /SYMBOLS; see that file's banner. Fold
    // it back here when that symbol lands.
    // -----------------------------------------------------------------------
}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wB_09.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_09.cpp
//   (wave B partfile -- group 10 "walk-core":
//       WalkAISection / AreRoadsAFollowOnPair / FindUpcomingStreetsByRecursiveWalking
//    + the CompareSectionWalkData qsort comparator)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::CompareSectionWalkData                          @ 0x82317268
//   BrnGameState::StreetManager::AreRoadsAFollowOnPair            @ 0x82317290
//   BrnGameState::StreetManager::WalkAISection                    @ 0x8233EAE8
//   BrnGameState::StreetManager::FindUpcomingStreetsByRecursiveWalking @ 0x8234E8C8
//
// Every asm store / branch / early-out has a counterpart here; all state is the
// frozen-header named members (BrnGameStateStreetManager.h), never raw-offset
// casts. The three RC-interface accessors sub_82310398 / sub_823102F0 /
// sub_82310240 are the committed RCEntityActiveRaceCarOutputInterface
// GetPlayerDirection / GetPlayerPosition / GetPlayerRaceCarState (address order
// == the DWARF :393/:396/:399 declaration order; the dotted vector is the
// direction, the reference-position vector is the player position).
// ===========================================================================

// The scalar (fpu) vector-similarity helper the X360 walk calls out-of-line
// (rw::math::fpu::IsSimilar<float> @ the EATech reconstruction; the frozen
// RwMathVectorTemplates.h declares the sibling IsValid<T> but this helper's
// body lands with the rwmath follow-on). Declare-only call, matching the asm bl.
namespace rw { namespace math { namespace fpu {
    template <typename Type>
    bool IsSimilar( const Vector3Template<Type>& lrA,
                    const Vector3Template<Type>& lrB,
                    Type lEpsilon );
} } }

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82317268. qsort comparator over SectionWalkData: orders the collected
    // portals by descending absolute entry angle. ARTIST 0x82317268..88
    // returns +1 when |first| < |second|, and -1 otherwise (including ties).
    // Side exits must be visited before bends farther along the forward road.
    // -----------------------------------------------------------------------
    int32_t CompareSectionWalkData( const void* lpData1, const void* lpData2 )
    {
        const SectionWalkData* lpWalkData1 = static_cast<const SectionWalkData*>( lpData1 );
        const SectionWalkData* lpWalkData2 = static_cast<const SectionWalkData*>( lpData2 );

        const f32 lfAbsAngle1 = fabsf( lpWalkData1->mfEntryAngle );
        const f32 lfAbsAngle2 = fabsf( lpWalkData2->mfEntryAngle );

        return lfAbsAngle1 < lfAbsAngle2 ? 1 : -1;
    }

    // -----------------------------------------------------------------------
    // @ 0x82317290. Unordered scan of KAA_FOLLOW_ON_ROAD_PAIRS: true iff the two
    // road indices (distinct) form one of the hard-coded follow-on pairs.
    // -----------------------------------------------------------------------
    bool StreetManager::AreRoadsAFollowOnPair( BrnStreetData::RoadIndex liRoadIndex1,
                                               BrnStreetData::RoadIndex liRoadIndex2 )
    {
        if ( liRoadIndex1 == liRoadIndex2 )
            return false;

        for ( s32 liHardCodedListIndex = 0; liHardCodedListIndex < KI_NUM_PAIRED_ROADS; ++liHardCodedListIndex )
        {
            const BrnStreetData::RoadIndex liFirst  = KAA_FOLLOW_ON_ROAD_PAIRS[liHardCodedListIndex][0];
            const BrnStreetData::RoadIndex liSecond = KAA_FOLLOW_ON_ROAD_PAIRS[liHardCodedListIndex][1];

            if ( liFirst == liRoadIndex1 && liSecond == liRoadIndex2 )
                return true;
            if ( liFirst == liRoadIndex2 && liSecond == liRoadIndex1 )
                return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // @ 0x8233EAE8. Recursive, breadth-limited AI-section portal walk. Collects
    // the section's onward portals into a stack SectionWalkData array, records a
    // left / right / forward road candidate when a real road (not a follow-on of
    // the current road) is reached, then recurses through the portals (sorted by
    // absolute entry angle) up to KI_MAX_SECTION_WALK_DEPTH deep.
    // -----------------------------------------------------------------------
    void StreetManager::WalkAISection( const rw::math::fpu::Vector3Template<f32>& lrEntryPos,
                                       const rw::math::fpu::Vector3Template<f32>& lrEntryDir,
                                       const BrnAI::AISection* lpSection, u16 luSectionIndex,
                                       const rw::math::fpu::Vector3Template<f32>& lrPortalPos,
                                       const rw::math::fpu::Vector3Template<f32>& lrExitRight,
                                       ESectionEntryDirection leEntryDirection,
                                       s32 liDepth, s32 liDirectionPersistance, bool lbFirst )
    {
        // Both onward road candidates already resolved -- nothing more to do.
        if ( miNewestLeftPlayerRoadIndex != -1 && miNewestRightPlayerRoadIndex != -1 )
            return;

        PushSectionIndex( luSectionIndex );

        BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luSectionIndex );
        if ( AreRoadsAFollowOnPair( liRoadIndex, miCurrentPlayerRoadIndex ) )
            return;

        // Interstate-exit section: fold this section's road into the interstate
        // slot, stamp the first junction position, and mark the road sentinel -2.
        if ( lpSection->mx8Flags & 0x80 )
        {
            bool lbCurrentRoadIsInterState = false;
            if ( miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_1
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_2
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_3
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_4 )
                lbCurrentRoadIsInterState = true;

            if ( lbCurrentRoadIsInterState )
            {
                const bool lbJunctionAlreadySet = mbJunctionPosSet;
                miOriginalInterStateRoadIndex = liRoadIndex;
                liRoadIndex = -2;
                if ( !lbJunctionAlreadySet )
                {
                    mFirstJunctionPosition.Set( lrPortalPos.X(), lrPortalPos.Y(), lrPortalPos.Z() );
                    mbJunctionPosSet = true;
                }
            }
        }

        const bool lbRoadIsNewToPlayer = ( miCurrentPlayerRoadIndex != liRoadIndex );
        const bool lbSectionHasRoad    = ( liRoadIndex != -1 );

        if ( lbFirst )
        {
            if ( miCurrentPlayerRoadIndex == liRoadIndex )
                return;
        }

        if ( miCurrentPlayerRoadIndex != -1 && !lbSectionHasRoad && !mbJunctionPosSet )
        {
            mFirstJunctionPosition.Set( lrPortalPos.X(), lrPortalPos.Y(), lrPortalPos.Z() );
            mbJunctionPosSet = true;
        }

        // Record the onward road candidate (unless there's no real road or it's
        // still the player's current road -- both fall through to the walk).
        if ( lbSectionHasRoad && lbRoadIsNewToPlayer )
        {
            switch ( leEntryDirection )
            {
                case E_SECTION_ENTRY_DIRECTION_LEFT:
                case E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT:
                    if ( miNewestLeftPlayerRoadIndex == -1 && !mbPlayerOnInterstateExit )
                    {
                        miNewestLeftPlayerRoadIndex = liRoadIndex;
                        return;
                    }
                    break;
                case E_SECTION_ENTRY_DIRECTION_FORWARDS:
                    break;
                case E_SECTION_ENTRY_DIRECTION_RIGHT:
                case E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT:
                    if ( miNewestRightPlayerRoadIndex == -1 && !mbPlayerOnInterstateExit )
                    {
                        miNewestRightPlayerRoadIndex = liRoadIndex;
                        return;
                    }
                    break;
                default:
                    CGS_ASSERT( false, CGS_ASSERT_MESSAGE_AT("How did it get here?", CGS_ARTIST_FILE, 1050) );
                    break;
            }
        }

        // ---- walk the section's portals -----------------------------------
        if ( miNextFreeSectionSlot >= KI_MAX_SECTIONS_TO_WALK || liDepth >= KI_MAX_SECTION_WALK_DEPTH )
            return;
        if ( lpSection->mu8NumPortals == 0 )
            return;

        SectionWalkData laSectionWalkData[KI_MAX_SECTIONS_TO_WALK];
        s32 liNumPortalsToExplore = 0;

        for ( s32 liPortalIndex = 0; liPortalIndex < lpSection->mu8NumPortals; ++liPortalIndex )
        {
            const BrnAI::Portal* lpPortal = lpSection->GetPortal( static_cast<u8>( liPortalIndex ) );
            const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();
            CGS_ASSERT( luLinkSectionIndex != 0x7FFF, CGS_ASSERT_MESSAGE_AT("luLinkSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX", CGS_ARTIST_FILE, 1084) );

            // HasSectionAlreadyBeenWalked (inlined scan of mauWalkedSections).
            bool lbAlreadyWalked = false;
            if ( miNextFreeSectionSlot > 0 )
            {
                for ( s32 liWalked = 0; liWalked < miNextFreeSectionSlot; ++liWalked )
                {
                    if ( luLinkSectionIndex == mauWalkedSections[liWalked] )
                    {
                        lbAlreadyWalked = true;
                        break;
                    }
                }
            }
            if ( lbAlreadyWalked )
                continue;

            const BrnAI::AISection* lpLinkSection = mpAISectionData.operator->()->GetAISection( luLinkSectionIndex );
            const u8 lu8LinkFlags = lpLinkSection->mx8Flags;
            if ( ( lu8LinkFlags & 1 ) || ( lu8LinkFlags & 0x40 ) || ( lu8LinkFlags & 4 ) || ( lu8LinkFlags & 0x20 ) )
                continue;

            const f32 lfPortalX = lpPortal->GetPositionX();
            const f32 lfPortalY = lpPortal->GetPositionY();
            const f32 lfPortalZ = lpPortal->GetPositionZ();

            rw::math::fpu::Vector3Template<f32> lPortalPosition;
            lPortalPosition.Set( lfPortalX, lfPortalY, lfPortalZ );

            const f32 lfToPortalY = lrEntryDir.Y() - lfPortalY;
            const f32 lfToPortalX = lrEntryDir.X() - lfPortalX;
            const f32 lfToPortalZ = lrEntryDir.Z() - lfPortalZ;
            const f32 lfDistanceToPortal = sqrtf( lfToPortalY * lfToPortalY
                                                + lfToPortalX * lfToPortalX
                                                + lfToPortalZ * lfToPortalZ );

            if ( lfDistanceToPortal > KF_MAX_ROAD_LOOKAHEAD_DIST )
                continue;
            if ( rw::math::fpu::IsSimilar<f32>( lPortalPosition, lrPortalPos, 0.0000152587890625f ) )
                continue;

            // Build the walk record: portal position + normalised entry direction.
            SectionWalkData& lRecord = laSectionWalkData[liNumPortalsToExplore];
            lRecord.mEntryPos.Set( lfPortalX, lfPortalY, lfPortalZ );

            const f32 lfEntryDirX = lfPortalX - lrPortalPos.X();
            const f32 lfEntryDirY = lfPortalY - lrPortalPos.Y();
            const f32 lfEntryDirZ = lfPortalZ - lrPortalPos.Z();
            const f32 lfEntryInvLen = 1.0f / sqrtf( lfEntryDirX * lfEntryDirX
                                                  + lfEntryDirY * lfEntryDirY
                                                  + lfEntryDirZ * lfEntryDirZ );
            const f32 lfNormEntryX = lfEntryDirX * lfEntryInvLen;
            const f32 lfNormEntryY = lfEntryDirY * lfEntryInvLen;
            const f32 lfNormEntryZ = lfEntryDirZ * lfEntryInvLen;
            lRecord.mEntryDir.Set( lfNormEntryX, lfNormEntryY, lfNormEntryZ );

            // Accept only portals ahead of the section-entry vector.
            const f32 lfAheadDot = lrEntryPos.Y() * ( lfPortalY - lrEntryDir.Y() )
                                 + lrEntryPos.Z() * ( lfPortalZ - lrEntryDir.Z() )
                                 + lrEntryPos.X() * ( lfPortalX - lrEntryDir.X() );
            if ( lfAheadDot < 0.0f )
                continue;

            lRecord.mpLinkPortal   = lpPortal;
            lRecord.muSectionIndex = luLinkSectionIndex;
            lRecord.mpLinkSection  = lpLinkSection;

            // Signed entry angle = asin( sign(crossY) * |entryDir x exitRight| ).
            const f32 lfCrossY = lfNormEntryZ * lrExitRight.X() - lfNormEntryX * lrExitRight.Z();
            const f32 lfCrossZ = lfNormEntryX * lrExitRight.Y() - lfNormEntryY * lrExitRight.X();
            const f32 lfCrossX = lfNormEntryY * lrExitRight.Z() - lfNormEntryZ * lrExitRight.Y();
            const f32 lfCrossMag = sqrtf( lfCrossY * lfCrossY + lfCrossZ * lfCrossZ + lfCrossX * lfCrossX );

            f32 lfSign;
            if ( lfCrossY == 0.0f )
                lfSign = 0.0f;
            else
                lfSign = ( lfCrossY >= 0.0f ) ? 1.0f : -1.0f;
            lRecord.mfEntryAngle = static_cast<f32>( asin( static_cast<double>( lfSign * lfCrossMag ) ) );

            ++liNumPortalsToExplore;
        }

        if ( liNumPortalsToExplore == 0 )
            return;

        // Two-portal straight-through: the only onward portal -- recurse without
        // the qsort, deepening the direction persistance.
        if ( lpSection->mu8NumPortals == 2 && liNumPortalsToExplore == 1 )
        {
            ESectionEntryDirection leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
            if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                leNextDirection = leEntryDirection;

            WalkAISection( lrEntryPos, lrEntryDir,
                           laSectionWalkData[0].mpLinkSection, laSectionWalkData[0].muSectionIndex,
                           laSectionWalkData[0].mEntryPos, laSectionWalkData[0].mEntryDir,
                           leNextDirection, liDepth + 1, liDirectionPersistance + 1, lbRoadIsNewToPlayer );
            return;
        }

        CGS_ASSERT( liNumPortalsToExplore > 0, CGS_ASSERT_MESSAGE_AT("liNumPortalsToExplore > 0", CGS_ARTIST_FILE, 1187) );

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsQSortPM );
        qsort( laSectionWalkData, static_cast<size_t>( liNumPortalsToExplore ),
               sizeof( SectionWalkData ), CompareSectionWalkData );
        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsQSortPM );

        for ( s32 liExplore = 0; liExplore < liNumPortalsToExplore; ++liExplore )
        {
            SectionWalkData& lRecord = laSectionWalkData[liExplore];

            bool lbSkipPortal = false;
            ESectionEntryDirection leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;

            if ( lbRoadIsNewToPlayer )
            {
                const f32 lfEntryAngle = lRecord.mfEntryAngle;
                if ( lfEntryAngle < -KF_UPCOMING_ROAD_SIDEWAYS_ANGLE )
                {
                    if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT )
                        lbSkipPortal = true;
                    else if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT
                           && liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT;
                    else if ( lfEntryAngle < -KF_UPCOMING_ROAD_SIGNIFICANT_ANGLE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_LEFT;
                }
                else if ( lfEntryAngle > KF_UPCOMING_ROAD_SIDEWAYS_ANGLE )
                {
                    if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT )
                        lbSkipPortal = true;
                    else if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT
                           && liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT;
                    else if ( lfEntryAngle > KF_UPCOMING_ROAD_SIGNIFICANT_ANGLE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_RIGHT;
                }
                else
                {
                    if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = leEntryDirection;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
                }
            }
            else
            {
                if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                    leNextDirection = leEntryDirection;
                else
                    leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
            }

            if ( lbSkipPortal )
                continue;

            WalkAISection( lrEntryPos, lrEntryDir,
                           lRecord.mpLinkSection, lRecord.muSectionIndex,
                           lRecord.mEntryPos, lRecord.mEntryDir,
                           leNextDirection, liDepth + 1, 0, lbRoadIsNewToPlayer );

            if ( !CanContinueWalking() )
                break;
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x8234E8C8. Seed the recursive walk from the player's current section:
    // pick the most-forward onward portal, derive the section exit-right vector
    // (edge best aligned toward that portal), WalkAISection through it, then
    // apply the hopeful-road stickiness and (in-race) SendUpcomingRoadMessage.
    // -----------------------------------------------------------------------
    void StreetManager::FindUpcomingStreetsByRecursiveWalking(
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
        GameStateModuleIO::OutputBuffer* lpOutput,
        BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
        f32 lfSimTimeStep,
        u16 luSectionIndex,
        bool lbInRace )
    {
        const BrnAI::AISection* lpSection = mpAISectionData.operator->()->GetAISection( luSectionIndex );

        const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
        const f32 lfPlayerDirX = lPlayerDirection.mV.x;
        const f32 lfPlayerDirY = lPlayerDirection.mV.y;
        const f32 lfPlayerDirZ = lPlayerDirection.mV.z;

        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const f32 lfPlayerPosX = lPlayerPosition.mV.x;
        const f32 lfPlayerPosY = lPlayerPosition.mV.y;
        const f32 lfPlayerPosZ = lPlayerPosition.mV.z;

        u16 luBestPortalSectionIndex = 0x7FFF;
        f32 lfBestPortalDot = 0.0f;
        f32 lfBestPortalX = 0.0f;
        f32 lfBestPortalY = 0.0f;
        f32 lfBestPortalZ = 0.0f;

        miNewestForwardPlayerRoadIndex = -1;
        miNewestLeftPlayerRoadIndex = -1;
        ClearSectionWalkingData();
        miNewestRightPlayerRoadIndex = -1;

        PushSectionIndex( luSectionIndex );

        if ( lpSection->mu8NumPortals != 0 )
        {
            for ( s32 liPortalIndex = 0; liPortalIndex < lpSection->mu8NumPortals; ++liPortalIndex )
            {
                const BrnAI::Portal* lpPortal = lpSection->GetPortal( static_cast<u8>( liPortalIndex ) );
                const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();

                const BrnAI::AISectionsData* lpSectionsData = mpAISectionData.operator->();
                CGS_ASSERT( luLinkSectionIndex < lpSectionsData->muNumSections, "luSectionIndex < muNumSections" );

                const u8 lu8LinkFlags = lpSectionsData->mpaSections[luLinkSectionIndex].mx8Flags;
                bool lbSkipPortal = false;
                if ( ( lu8LinkFlags & 1 ) || ( lu8LinkFlags & 0x40 ) || ( lu8LinkFlags & 4 ) || ( lu8LinkFlags & 0x20 ) )
                    lbSkipPortal = true;

                if ( !lbSkipPortal )
                {
                    const f32 lfPortalX = lpPortal->GetPositionX();
                    const f32 lfPortalY = lpPortal->GetPositionY();
                    const f32 lfPortalZ = lpPortal->GetPositionZ();

                    const f32 lfDeltaX = lfPortalX - lfPlayerPosX;
                    const f32 lfDeltaY = lfPortalY - lfPlayerPosY;
                    const f32 lfDeltaZ = lfPortalZ - lfPlayerPosZ;
                    const f32 lfInvLen = 1.0f / sqrtf( lfDeltaY * lfDeltaY
                                                     + lfDeltaX * lfDeltaX
                                                     + lfDeltaZ * lfDeltaZ );
                    const f32 lfDot = lfDeltaX * lfInvLen * lfPlayerDirX
                                    + lfDeltaY * lfInvLen * lfPlayerDirY
                                    + lfDeltaZ * lfInvLen * lfPlayerDirZ;

                    if ( lfDot > lfBestPortalDot )
                    {
                        luBestPortalSectionIndex = lpPortal->GetLinkSectionIndex();
                        lfBestPortalDot = lfDot;
                        lfBestPortalX = lfPortalX;
                        lfBestPortalY = lfPortalY;
                        lfBestPortalZ = lfPortalZ;
                    }
                }
            }
        }

        if ( luBestPortalSectionIndex != 0x7FFF )
        {
            const BrnAI::AISectionsData* lpSectionsData = mpAISectionData.operator->();
            CGS_ASSERT( luBestPortalSectionIndex < lpSectionsData->muNumSections, "luSectionIndex < muNumSections" );
            const BrnAI::AISection* lpBestSection = &lpSectionsData->mpaSections[luBestPortalSectionIndex];

            // Find the section edge best aligned toward the chosen portal.
            f32 lfBestEdgeDot = 0.0f;
            f32 lfEdgeStartX = 0.0f;
            f32 lfEdgeStartY = 0.0f;
            f32 lfEdgeEndX = 0.0f;
            f32 lfEdgeEndY = 0.0f;

            for ( s32 liCornerIndex = 0; liCornerIndex < BrnAI::KI_AI_SECTION_EDGES; ++liCornerIndex )
            {
                CGS_ASSERT( liCornerIndex >= 0 && liCornerIndex < BrnAI::KI_AI_SECTION_EDGES,
                            "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES" );
                const s32 liNextCorner = ( liCornerIndex + 1 ) % BrnAI::KI_AI_SECTION_EDGES;
                CGS_ASSERT( liNextCorner >= 0 && liNextCorner < BrnAI::KI_AI_SECTION_EDGES,
                            "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES" );

                const f32 lfCornerX = lpBestSection->mpaCorners[liCornerIndex].x;
                const f32 lfCornerY = lpBestSection->mpaCorners[liCornerIndex].y;
                const f32 lfNextCornerX = lpBestSection->mpaCorners[liNextCorner].x;
                const f32 lfNextCornerY = lpBestSection->mpaCorners[liNextCorner].y;

                const f32 lfInvToPortal = 1.0f / sqrtf( ( lfBestPortalZ - lfCornerY ) * ( lfBestPortalZ - lfCornerY )
                                                      + ( lfBestPortalX - lfCornerX ) * ( lfBestPortalX - lfCornerX ) );
                const f32 lfInvEdgeLen  = 1.0f / sqrtf( ( lfNextCornerX - lfCornerX ) * ( lfNextCornerX - lfCornerX )
                                                      + ( lfNextCornerY - lfCornerY ) * ( lfNextCornerY - lfCornerY ) );
                const f32 lfEdgeDot = ( lfBestPortalZ - lfCornerY ) * lfInvToPortal * ( ( lfNextCornerY - lfCornerY ) * lfInvEdgeLen )
                                    + ( lfBestPortalX - lfCornerX ) * lfInvToPortal * ( ( lfNextCornerX - lfCornerX ) * lfInvEdgeLen );

                if ( lfEdgeDot > lfBestEdgeDot )
                {
                    lfBestEdgeDot = lfEdgeDot;
                    lfEdgeStartX = lfCornerX;
                    lfEdgeStartY = lfCornerY;
                    lfEdgeEndX = lfNextCornerX;
                    lfEdgeEndY = lfNextCornerY;
                }
            }

            // The normalised edge direction, and its horizontal perpendicular.
            const f32 lfEdgeInvLen = 1.0f / sqrtf( ( lfEdgeStartY - lfEdgeEndY ) * ( lfEdgeStartY - lfEdgeEndY )
                                                 + ( lfEdgeStartX - lfEdgeEndX ) * ( lfEdgeStartX - lfEdgeEndX ) );
            const f32 lfEdgeDirZ = ( lfEdgeStartY - lfEdgeEndY ) * lfEdgeInvLen;
            const f32 lfEdgeDirX = ( lfEdgeStartX - lfEdgeEndX ) * lfEdgeInvLen;

            const f32 lfPerpInvLen = 1.0f / sqrtf( ( -lfEdgeDirZ ) * ( -lfEdgeDirZ ) + ( lfEdgeDirX * lfEdgeDirX ) );
            f32 lfEntryDirX = lfPerpInvLen * -lfEdgeDirZ;
            f32 lfEntryDirY = lfPerpInvLen * 0.0f;
            f32 lfEntryDirZ = lfPerpInvLen * lfEdgeDirX;

            if ( ( ( lfBestPortalX - lfPlayerPosX ) * lfEntryDirX )
               + ( ( lfBestPortalZ - lfPlayerPosZ ) * lfEntryDirZ )
               + ( ( lfBestPortalY - lfPlayerPosY ) * lfEntryDirY ) < 0.0f )
            {
                lfEntryDirX = ( lfPerpInvLen * -lfEdgeDirZ ) * -1.0f;
                lfEntryDirY = ( lfPerpInvLen * 0.0f ) * -1.0f;
                lfEntryDirZ = ( lfPerpInvLen * lfEdgeDirX ) * -1.0f;
            }

            // The section exit-right member = the normalised edge direction, sign
            // aligned against the player car's right axis (transform x-axis).
            mCurrentPlayerSectionExitRight.Set( lfEdgeDirX, 0.0f, lfEdgeDirZ );
            const f32 lfExitRightInvLen = 1.0f / sqrtf( lfEdgeDirZ * lfEdgeDirZ + lfEdgeDirX * lfEdgeDirX + 0.0f * 0.0f );
            mCurrentPlayerSectionExitRight.Set( lfExitRightInvLen * lfEdgeDirX,
                                                lfExitRightInvLen * 0.0f,
                                                lfExitRightInvLen * lfEdgeDirZ );

            const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState = lpActiveRaceCarInterface->GetPlayerRaceCarState();
            if ( ( lpPlayerRaceCarState->mTransform.xAxis.mV.x * mCurrentPlayerSectionExitRight.X() )
               + ( mCurrentPlayerSectionExitRight.Z() * lpPlayerRaceCarState->mTransform.xAxis.mV.z )
               + ( mCurrentPlayerSectionExitRight.Y() * lpPlayerRaceCarState->mTransform.xAxis.mV.y ) < 0.0f )
            {
                mCurrentPlayerSectionExitRight.Set( mCurrentPlayerSectionExitRight.X() * -1.0f,
                                                    mCurrentPlayerSectionExitRight.Y() * -1.0f,
                                                    mCurrentPlayerSectionExitRight.Z() * -1.0f );
            }

            mbJunctionPosSet = false;
            mFirstJunctionPosition.Set( lfPlayerPosX, lfPlayerPosY, lfPlayerPosZ );

            rw::math::fpu::Vector3Template<f32> lSectionEntryDir;
            lSectionEntryDir.Set( lfEntryDirX, lfEntryDirY, lfEntryDirZ );
            rw::math::fpu::Vector3Template<f32> lPlayerPositionVec;
            lPlayerPositionVec.Set( lfPlayerPosX, lfPlayerPosY, lfPlayerPosZ );
            rw::math::fpu::Vector3Template<f32> lBestPortalPositionVec;
            lBestPortalPositionVec.Set( lfBestPortalX, lfBestPortalY, lfBestPortalZ );

            WalkAISection( lSectionEntryDir, lPlayerPositionVec,
                           lpBestSection, luBestPortalSectionIndex,
                           lBestPortalPositionVec, lSectionEntryDir,
                           E_SECTION_ENTRY_DIRECTION_FORWARDS, 0, 0, false );

            // Hopeful-road stickiness: only accept the new candidates once they
            // hold steady past the sticky-time threshold.
            const BrnStreetData::RoadIndex liNewestForward = miNewestForwardPlayerRoadIndex;
            if ( liNewestForward == miHopefulForwardPlayerRoadIndex
              && miNewestLeftPlayerRoadIndex == miHopefulLeftPlayerRoadIndex
              && miNewestRightPlayerRoadIndex == miHopefulRightPlayerRoadIndex )
            {
                mfHopefulRoadTimer += lfSimTimeStep;
            }
            else
            {
                miHopefulForwardPlayerRoadIndex = liNewestForward;
                mfHopefulRoadTimer = 0.0f;
                mbNeedToUpdateUpcomingRoads = true;
                miHopefulLeftPlayerRoadIndex = miNewestLeftPlayerRoadIndex;
                miHopefulRightPlayerRoadIndex = miNewestRightPlayerRoadIndex;
            }

            const f32 lfStickyThreshold = lbInRace ? KF_UPCOMING_ROADS_STICKY_TIME_RACE
                                                   : KF_UPCOMING_ROADS_STICKY_TIME;
            if ( mfHopefulRoadTimer > lfStickyThreshold )
            {
                if ( mbNeedToUpdateUpcomingRoads )
                {
                    const BrnStreetData::RoadIndex liHopefulLeft = miHopefulLeftPlayerRoadIndex;
                    mLeftRoadIndex = -1;
                    mRightRoadIndex = -1;
                    mbRightRoadHighlighted = false;
                    mbLeftRoadHighlighted = false;
                    miOriginalInterStateRoadIndex = -1;
                    if ( liHopefulLeft != -1 )
                        mLeftRoadIndex = liHopefulLeft;
                    const BrnStreetData::RoadIndex liHopefulRight = miHopefulRightPlayerRoadIndex;
                    if ( liHopefulRight != -1 )
                        mRightRoadIndex = liHopefulRight;

                    if ( lbInRace )
                        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );

                    mbNeedToUpdateUpcomingRoads = false;
                }
                else
                {
                    if ( lbInRace )
                        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );
                }
            }
        }
    }
}

namespace BrnGameState {
// ARTIST 0x8230F990 / 0x8230FA20, the bounded portal-walk stack.
void StreetManager::PushSectionIndex(u16 section)
{
    CGS_ASSERT(miNextFreeSectionSlot < KI_MAX_SECTIONS_TO_WALK,
        CGS_ASSERT_MESSAGE_AT(
            "miNextFreeSectionSlot < KI_MAX_SECTIONS_TO_WALK", CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER, 1062 /* ARTIST 0x8230F9C0 */));
    CGS_ASSERT(section != 0x7FFF,
        CGS_ASSERT_MESSAGE_AT(
            "luNewSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX", CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER, 1063 /* ARTIST 0x8230F9E8 */));
    mauWalkedSections[miNextFreeSectionSlot++] = section;
}
bool StreetManager::CanContinueWalking() { return miNextFreeSectionSlot < KI_MAX_SECTIONS_TO_WALK; }
}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wB_10.cpp (wave B) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_10.cpp
//   (wave B partfile -- group 10 "upcoming-pipeline")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::UpdateUpcomingStreets        @ 0x82350A88
//   BrnGameState::StreetManager::FindUpcomingStreetsFromRoute @ 0x8234EF90
//   BrnGameState::StreetManager::CalculateDistanceToTurning   @ 0x823172F0
// (SendUpcomingRoadMessage @ 0x82348798 is BLOCKED -- see the structured
//  report: its 368-byte GUI message payload has no committed type and its
//  leading vtable/type-descriptor pointer (unk_82F30000) is an un-named
//  un-exported global, so a faithful store-for-store build would fabricate a
//  layout + symbol. It is only CALLED here, through the frozen-header decl.)
//
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (never raw-offset casts). All magic float
// immediates are the values attested in the image rodata.
// ===========================================================================

// ---------------------------------------------------------------------------
// Forward declarations for the two rw::math::fpu scalar-vector helpers the
// route walk calls in its dev asserts. The frozen RwMathVectorTemplates.h homes
// only IsValid<>; the DecFIGS DWARF spells IsSimilar<>/IsZero<> in the same
// namespace and their bodies land with that TU (spec: "declare-only calls fine").
// ---------------------------------------------------------------------------
namespace rw { namespace math { namespace fpu {
    template <typename Type>
    bool IsSimilar( const Vector3Template<Type>& lrA, const Vector3Template<Type>& lrB, Type lfTolerance );
    template <typename Type>
    bool IsZero( const Vector3Template<Type>& lrVector, Type lfTolerance );
} } }

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x823172F0. Planar (y-dropped) distance from the player to the current
    // route's turn node (this->miNewRoadNodeIndex - 2). Returns 0.0f (flt_82001CC0)
    // on the no-route / empty-route early-outs.
    // -----------------------------------------------------------------------
    f32 StreetManager::CalculateDistanceToTurning(
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface )
    {
        CGS_ASSERT( lpLastAICarOutputInterface != NULL, CGS_ASSERT_MESSAGE_AT("lpLastAICarOutputInterface != NULL", CGS_ARTIST_FILE, 1934) );

        const s32 liPlayerRoadTargetNodeIndex = miNewRoadNodeIndex - 2;
        if ( liPlayerRoadTargetNodeIndex >= 0 )
        {
            // GetPlayerRoute() folds to the interface pointer itself (mPlayerRoute @ +0);
            // view it as the now-committed BrnAI::Route (same address the X360 asserts twice).
            const BrnAI::Route* lpPlayerRoute =
                reinterpret_cast<const BrnAI::Route*>( lpLastAICarOutputInterface->GetPlayerRoute() );
            CGS_ASSERT( lpPlayerRoute != NULL, CGS_ASSERT_MESSAGE_AT("lpPlayerRoute != NULL", CGS_ARTIST_FILE, 1946) );

            if ( lpPlayerRoute->GetNodeCount() >= 1 )
            {
                const BrnAI::RouteNode* lpPlayerRoadTargetRouteNode =
                    lpPlayerRoute->GetNode( liPlayerRoadTargetNodeIndex );
                CGS_ASSERT( lpPlayerRoadTargetRouteNode != NULL, CGS_ASSERT_MESSAGE_AT("lpPlayerRoadTargetRouteNode != NULL", CGS_ARTIST_FILE, 1955) );

                // Node position onto the ground plane: (x, 0, y).
                const f32 lfTargetNodeX = lpPlayerRoadTargetRouteNode->GetX();
                const f32 lfTargetNodeZ = lpPlayerRoadTargetRouteNode->GetY();

                // Player position with its vertical lane dropped (v15[1] = 0).
                Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
                lPlayerPosition.mV.y = 0.0f;

                const f32 lfDeltaX = lPlayerPosition.mV.x - lfTargetNodeX;
                const f32 lfDeltaY = lPlayerPosition.mV.y - 0.0f;
                const f32 lfDeltaZ = lPlayerPosition.mV.z - lfTargetNodeZ;
                const f32 lfDistanceSquared = ( lfDeltaX * lfDeltaX )
                                            + ( lfDeltaY * lfDeltaY )
                                            + ( lfDeltaZ * lfDeltaZ );
                // vsel zero-guard: a zero squared-length selects 0 rather than 0 * (1/0).
                const f32 lfDistanceToTurning = ( lfDistanceSquared == 0.0f ) ? 0.0f : sqrtf( lfDistanceSquared );
                return lfDistanceToTurning;
            }
        }

        return 0.0f;   // flt_82001CC0
    }

    // -----------------------------------------------------------------------
    // @ 0x82350A88. Per-frame upcoming-streets pump: resolve the player's AI
    // section -> road index, junction-timeout bookkeeping, then the recursive /
    // route walks and the shortcut-flag game action (type 287).
    // -----------------------------------------------------------------------
    void StreetManager::UpdateUpcomingStreets(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            GameStateModuleIO::OutputBuffer* lpOutput,
            f32 lfSimTimeStep,
            bool lbUseRoute )
    {
        bool lbFoundRoad = false;

        CGS_ASSERT( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    CGS_ASSERT_MESSAGE_AT("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE_BRNRACECARENTITYMODULEOUTPUTINTERFACE, 967) );

        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        bool lbPlayerCarActive = false;
        if ( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID )
        {
            lbPlayerCarActive = lpActiveRaceCarInterface->IsPlayerCarActive();
        }

        if ( lbPlayerCarActive )
        {
            CGS_ASSERT( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        CGS_ASSERT_MESSAGE_AT("Player car index hasn't been set", CGS_ARTIST_FILE_BRNRACECARENTITYMODULEOUTPUTINTERFACE, 980) );
            CGS_ASSERT( lePlayerActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        CGS_ASSERT_MESSAGE_AT("lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID", CGS_ARTIST_FILE, 561) );
            CGS_ASSERT( lePlayerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        CGS_ASSERT_MESSAGE_AT("lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", CGS_ARTIST_FILE, 562) );

            const u16 luRaceCarAISection = lpActiveRaceCarInterface->GetRaceCarAISection( lePlayerActiveRaceCarIndex );
            const BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luRaceCarAISection );
            miCurrentPlayerRoadIndex = liRoadIndex;

            if ( liRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
            {
                // Junction path: invalidate the upcoming/hopeful roads and age the junction timer.
                const f32 lfNewTimeInJunction = mfTimeInJunction + lfSimTimeStep;

                miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
                miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
                ClearSectionWalkingData();
                miNewestRightPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mLeftRoadIndex                 = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mRightRoadIndex                = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mfHopefulRoadTimer             = 0.0f;
                mfTimeInJunction               = lfNewTimeInJunction;
                mbRightRoadHighlighted         = false;
                mbLeftRoadHighlighted          = false;
                mbNeedToUpdateUpcomingRoads    = true;
                miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;

                if ( lfNewTimeInJunction > KF_TIME_IN_JUNCTION_TO_KILL_UPCOMING )
                {
                    SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );
                }
            }
            else
            {
                miLastPlayerRoadIndex = liRoadIndex;
                lbFoundRoad           = true;
                mfTimeInJunction      = 0.0f;
            }

            if ( luRaceCarAISection != 0x7FFF )
            {
                if ( luRaceCarAISection >= mpAISectionData->muNumSections )
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                    lStrStream << "AI section index too large. Section = ";
                    lStrStream << static_cast<u32>( luRaceCarAISection );
                    lStrStream << " count = ";
                    lStrStream << mpAISectionData->muNumSections;
                    lStrStream << "\n";
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                    CgsDev::Assert::EndAssert();
                }

                const BrnAI::AISection* lpAISection = mpAISectionData->GetAISection( luRaceCarAISection );

                if ( lbFoundRoad )
                {
                    mbPlayerOnInterstateExit = ( lpAISection->mx8Flags >> 7 ) != 0;
                    FindUpcomingStreetsByRecursiveWalking( lpActiveRaceCarInterface, lpOutput,
                                                           lpLastAICarOutputInterface, lfSimTimeStep,
                                                           luRaceCarAISection, !lbUseRoute );
                    if ( lbUseRoute )
                    {
                        FindUpcomingStreetsFromRoute( lpActiveRaceCarInterface, lpLastAICarOutputInterface,
                                                      lpOutput, lfSimTimeStep );
                    }
                }

                // Shortcut-flag GUI game action (type 287): toggle when the player's
                // shortcut membership changes (AISection flag bit 0).
                if ( mbPlayerIsInAShortcut )
                {
                    if ( ( lpAISection->mx8Flags & 1 ) == 0 )
                    {
                        const u8 lu8InShortcut = 0;
                        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
                        lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lu8InShortcut ), 287, 1 );
                        mbPlayerIsInAShortcut = false;
                    }
                }
                else if ( ( lpAISection->mx8Flags & 1 ) != 0 )
                {
                    const u8 lu8InShortcut = 1;
                    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
                    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lu8InShortcut ), 287, 1 );
                    mbPlayerIsInAShortcut = true;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x8234EF90. Route-guided upcoming-streets walk: step the player's route
    // nodes ahead, classify each transition left/right against the section
    // exit-right vector, handle the interstate special cases, and push the
    // hopeful road onto the left/right sign when the guidance conditions hold.
    // -----------------------------------------------------------------------
    void StreetManager::FindUpcomingStreetsFromRoute(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            GameStateModuleIO::OutputBuffer* lpOutput,
            f32 lfSimTimeStep )
    {
        const BrnAI::Route* lpPlayerRoute =
            reinterpret_cast<const BrnAI::Route*>( lpLastAICarOutputInterface->GetPlayerRoute() );

        const BrnAI::Route::Status leStatus = lpPlayerRoute->GetStatus();
        if ( leStatus == BrnAI::Route::E_STATUS_UNINITIALISED || leStatus == BrnAI::Route::E_STATUS_BLOCKED )
        {
            return;
        }

        const BrnStreetData::RoadIndex liCurrentRoad = miCurrentPlayerRoadIndex;
        mbLeftRoadHighlighted  = false;
        mbRightRoadHighlighted = false;

        const bool lbCurrentRoadIsInterstate =
            ( liCurrentRoad == KI_INTERSTATE_SECTION_1 || liCurrentRoad == KI_INTERSTATE_SECTION_2
           || liCurrentRoad == KI_INTERSTATE_SECTION_3 || liCurrentRoad == KI_INTERSTATE_SECTION_4 );

        // Reset the sign locks unless there is a valid current road AND a known
        // upcoming left/right road: (curr == -1) || (left == -1 && right == -1).
        if ( liCurrentRoad != BrnStreetData::KI_INVALID_ROAD_INDEX
          && ( mLeftRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX
            || mRightRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX ) )
        {
            const s32 liCurrentRouteNode = lpLastAICarOutputInterface->GetPlayerRouteNodeIndex();

            BrnStreetData::RoadIndex liLeftRoadTracker  = BrnStreetData::KI_INVALID_ROAD_INDEX;
            BrnStreetData::RoadIndex liRightRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
            s32 liLeftRunCount  = 0;
            s32 liRightRunCount = 0;

            const s32 liNodeCount = lpPlayerRoute->GetNodeCount();
            mfGuidanceLockTime = lfSimTimeStep + mfGuidanceLockTime;

            const s32 liLookaheadEnd = liCurrentRouteNode + KI_ROUTE_ROAD_PREDICTION_LOOKAHEAD_NODES;
            if ( liCurrentRouteNode < liLookaheadEnd )   // overflow guard
            {
                const s32 liLoopEnd = liNodeCount - KI_NUM_END_OF_ROUTE_IGNORE_NODES;
                s32 liNodeIndex = liCurrentRouteNode;

                while ( liNodeIndex < liLoopEnd )
                {
                    const s32 liNextNodeIndex = liNodeIndex + 1;
                    const BrnAI::RouteNode* lpNode     = lpPlayerRoute->GetNode( liNodeIndex );
                    const BrnAI::RouteNode* lpNextNode = lpPlayerRoute->GetNode( liNextNodeIndex );

                    const u16 luSectionIndex = lpNode->GetSectionIndex();
                    // The X360 makes this first (discarded) GetAISection call for its
                    // bounds-assert side effect before resolving the road index.
                    mpAISectionData->GetAISection( luSectionIndex );

                    BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luSectionIndex );
                    const bool lbNextRoadIsInterstate =
                        ( liRoadIndex == KI_INTERSTATE_SECTION_1 || liRoadIndex == KI_INTERSTATE_SECTION_2
                       || liRoadIndex == KI_INTERSTATE_SECTION_3 || liRoadIndex == KI_INTERSTATE_SECTION_4 );

                    if ( !lbCurrentRoadIsInterstate && lbNextRoadIsInterstate )
                    {
                        goto LABEL_DONE;
                    }

                    const bool lbSectionIsInterstateExit =
                        ( ( mpAISectionData->GetAISection( luSectionIndex )->mx8Flags >> 7 ) != 0 );
                    if ( lbCurrentRoadIsInterstate && lbSectionIsInterstateExit )
                    {
                        miOriginalInterStateRoadIndex = liRoadIndex;
                        liRoadIndex = -2;
                    }

                    const BrnStreetData::RoadIndex liCurrentRoadNow = miCurrentPlayerRoadIndex;
                    // Interstate pair 12/44 followed by road 43 terminates the walk.
                    if ( ( liCurrentRoadNow == 12 || liCurrentRoadNow == 44 ) && liRoadIndex == 43 )
                    {
                        goto LABEL_HIGHLIGHT_FROM_LOCKS;
                    }

                    const f32 lfNodeX = lpNode->GetX();
                    const f32 lfNodeY = lpNode->GetY();
                    const f32 lfNextX = lpNextNode->GetX();
                    const f32 lfNextY = lpNextNode->GetY();

                    rw::math::fpu::Vector3Template<f32> lNodePos;
                    lNodePos.Set( lfNodeX, 0.0f, lfNodeY );
                    rw::math::fpu::Vector3Template<f32> lNextNodePos;
                    lNextNodePos.Set( lfNextX, 0.0f, lfNextY );

                    if ( liRoadIndex == liCurrentRoadNow || liRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
                    {
                        // Still on the current road: refresh the section exit-right vector.
                        if ( GetRoadIndexFromAISectionIndex( lpNextNode->GetSectionIndex() ) == miCurrentPlayerRoadIndex )
                        {
                            // Perpendicular of the node->nextNode segment on the ground plane.
                            mCurrentPlayerSectionExitRight.Set( -( lfNodeY - lfNextY ), 0.0f, ( lfNodeX - lfNextX ) );

                            CGS_ASSERT( !rw::math::fpu::IsZero( mCurrentPlayerSectionExitRight, 0.00000011920929f ),
                                        "!RwMathFPU::IsZero( mCurrentPlayerSectionExitRight )" );

                            const f32 lfX = mCurrentPlayerSectionExitRight.X();
                            const f32 lfY = mCurrentPlayerSectionExitRight.Y();
                            const f32 lfZ = mCurrentPlayerSectionExitRight.Z();
                            const f32 lfInvLength = 1.0f / sqrtf( ( lfZ * lfZ ) + ( ( lfX * lfX ) + ( lfY * lfY ) ) );
                            mCurrentPlayerSectionExitRight.Set( lfX * lfInvLength, lfY * lfInvLength, lfZ * lfInvLength );
                        }
                    }
                    else
                    {
                        // A turn: classify it left / right against the exit-right vector.
                        CGS_ASSERT( !rw::math::fpu::IsSimilar( lNodePos, lNextNodePos, 0.000015258789f ),
                                    "!RwMathFPU::IsSimilar( lNodePos, lNextNodePos )" );

                        const f32 lfInvLength = 1.0f
                            / sqrtf( ( ( lfNextX - lfNodeX ) * ( lfNextX - lfNodeX ) )
                                   + ( ( ( lfNextY - lfNodeY ) * ( lfNextY - lfNodeY ) )
                                     + ( ( 0.0f - 0.0f ) * ( 0.0f - 0.0f ) ) ) );
                        rw::math::fpu::Vector3Template<f32> lNodeVector;
                        lNodeVector.Set( ( lfNextX - lfNodeX ) * lfInvLength,
                                         ( 0.0f - 0.0f ) * lfInvLength,
                                         ( lfNextY - lfNodeY ) * lfInvLength );

                        CGS_ASSERT( rw::math::fpu::IsValid( lNodeVector ), "RwMathFPU::IsValid( lNodeVector )" );

                        const f32 lfDot =
                            ( lNodeVector.X() * mCurrentPlayerSectionExitRight.X() )
                          + ( ( mCurrentPlayerSectionExitRight.Z() * lNodeVector.Z() )
                            + ( mCurrentPlayerSectionExitRight.Y() * lNodeVector.Y() ) );

                        if ( lfDot >= 0.0f )
                        {
                            // Left turn.
                            liRightRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
                            liRightRunCount    = 0;

                            s32 liPreviousLeftRunCount;
                            if ( liRoadIndex == liLeftRoadTracker )
                            {
                                liPreviousLeftRunCount = liLeftRunCount;
                            }
                            else
                            {
                                miNewRoadNodeIndex = liNodeIndex;
                                mLastNode.Set( 0.0f, 0.0f, 0.0f );
                                liLeftRoadTracker = liRoadIndex;
                                liPreviousLeftRunCount = 0;
                                mSecondLastNode.Set( 0.0f, 0.0f, 0.0f );
                            }
                            liLeftRunCount = liPreviousLeftRunCount + 1;

                            if ( ( liPreviousLeftRunCount + 1 >= KI_MIN_NODES_FOR_DECISION || lbSectionIsInterstateExit )
                              && miHopefulLeftPlayerRoadIndex == liLeftRoadTracker )
                            {
                                mbLeftRoadHighlighted = true;
                                mLastNode.Set( lfNextX, 0.0f, lfNextY );
                                mSecondLastNode.Set( lfNodeX, 0.0f, lfNodeY );
                                miLastRoadNodeIndex = liNodeIndex;

                                if ( mbLockLeftSign && mfGuidanceLockTime < 1.0f )
                                {
                                    mbLeftRoadHighlighted = true;
                                    goto LABEL_SEND;
                                }

                                const f32 lfDistanceToTurning = CalculateDistanceToTurning( lpLastAICarOutputInterface,
                                                                                            lpActiveRaceCarInterface );
                                const ::EActiveRaceCarIndex lePlayerIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
                                const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
                                    lpActiveRaceCarInterface->GetRaceCarState( lePlayerIndex );
                                // ARTIST 8234F4CC reads speed @0x3CC, not maximum speed @0x3D0.
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfSpeedMPH ) * 4.0f ) > lfDistanceToTurning
                                    || lfDistanceToTurning < 50.0f )
                                  && lfDistanceToTurning != 0.0f )
                                {
                                    mfGuidanceLockTime = 0.0f;
                                    mbLockRightSign    = false;
                                    mbLockLeftSign     = true;
                                    goto LABEL_SEND;
                                }
                            }
                        }
                        else
                        {
                            // Right turn.
                            liLeftRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
                            liLeftRunCount    = 0;

                            s32 liPreviousRightRunCount;
                            if ( liRoadIndex == liRightRoadTracker )
                            {
                                liPreviousRightRunCount = liRightRunCount;
                            }
                            else
                            {
                                miNewRoadNodeIndex = liNodeIndex;
                                mLastNode.Set( 0.0f, 0.0f, 0.0f );
                                liRightRoadTracker = liRoadIndex;
                                liPreviousRightRunCount = 0;
                                mSecondLastNode.Set( 0.0f, 0.0f, 0.0f );
                            }
                            liRightRunCount = liPreviousRightRunCount + 1;

                            if ( ( liPreviousRightRunCount + 1 >= KI_MIN_NODES_FOR_DECISION || lbSectionIsInterstateExit )
                              && miHopefulRightPlayerRoadIndex == liRightRoadTracker )
                            {
                                mbRightRoadHighlighted = true;
                                mLastNode.Set( lfNextX, 0.0f, lfNextY );
                                mSecondLastNode.Set( lfNodeX, 0.0f, lfNodeY );
                                miLastRoadNodeIndex = liNodeIndex;

                                if ( mbLockRightSign && mfGuidanceLockTime < 1.0f )
                                {
                                    mbRightRoadHighlighted = true;
                                    goto LABEL_SEND;
                                }

                                const f32 lfDistanceToTurning = CalculateDistanceToTurning( lpLastAICarOutputInterface,
                                                                                            lpActiveRaceCarInterface );
                                const ::EActiveRaceCarIndex lePlayerIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
                                const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
                                    lpActiveRaceCarInterface->GetRaceCarState( lePlayerIndex );
                                // ARTIST 8234F3C0: predict four seconds at the current speed.
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfSpeedMPH ) * 4.0f ) > lfDistanceToTurning
                                    || lfDistanceToTurning < 50.0f )
                                  && lfDistanceToTurning != 0.0f )
                                {
                                    mfGuidanceLockTime = 0.0f;
                                    mbLockLeftSign     = false;
                                    mbLockRightSign    = true;
                                    goto LABEL_SEND;
                                }
                            }
                        }
                    }

                    // loc_8234F5C8 -- advance, bounded by the lookahead window.
                    liNodeIndex = liNextNodeIndex;
                    if ( liNodeIndex >= liLookaheadEnd )
                    {
                        break;
                    }
                }
            }

            goto LABEL_HIGHLIGHT_FROM_LOCKS;
        }

    // loc_8234F5E4
        mbLockLeftSign     = false;
        mbLockRightSign    = false;
        mfGuidanceLockTime = 0.0f;

    LABEL_HIGHLIGHT_FROM_LOCKS:   // loc_8234F5F8
        mbRightRoadHighlighted = mbLockRightSign;
        mbLeftRoadHighlighted  = mbLockLeftSign;

    LABEL_SEND:                   // loc_8234F608
        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, true, lpActiveRaceCarInterface );

    LABEL_DONE:                   // loc_8234F620
        return;
    }
}

// ============================================================================
// FOLDED FROM BrnGameStateStreetManager_wC_01.cpp (wave C) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_01.cpp
//   (wave C partfile -- group 1 "upcoming-road message")
//
//   BrnGameState::StreetManager::SendUpcomingRoadMessage @ 0x82348798
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout. The wave-B "unk_82F30000 payload vtable" blocker was a
// Hex-Rays misread: that `lis r30, 0x82F3` is the CgsDev::Assert::gpcMessageBuffer
// base used by the four streamed dev warnings, NOT a store into the message. The
// message itself is a plain 368-byte stack struct (frame base 0x270+var_200)
// posted with the attested AddEvent(..., 276, 368) literals.
//
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (never raw-offset casts).
// ===========================================================================

namespace
{
    // -----------------------------------------------------------------------
    // The contiguous 368-byte upcoming-road GUI message the X360 builds on the
    // stack and hands to VariableEventQueue<13312,16>::AddEvent(&msg, 276, 368).
    //
    // FLAG: the FIELD NAMES below are ours -- this GUI message has no DWARF
    // declaration and the X360 never names it. The offsets, widths, store order
    // and the (276, 368) post literals are byte-exact from the asm frame
    // (payload base = 0x270+var_200 == sp+0x70; see the per-field comments for
    // the matching `0x270+var_XX` slots).
    //
    // The two 16-byte vector slots are the two lvx128/stvx128 pairs at
    // 0x82348868 / 0x82348874 -- both fed from the SAME staging copy of
    // mFirstJunctionPosition (w lane zeroed).
    // -----------------------------------------------------------------------
    typedef BrnGameState::GameStateModuleIO::UpcomingRoadChangeAction UpcomingRoadMessage;

    // The interstate sentinel road id both sides store for a -2 road index. The
    // X360 materialises it inline (lis 0x6845 / ori 0x61A1 / lis 0x6800 /
    // insrdi 32,0 at 0x82348A14 and 0x82348BB4).
    const ::CgsID KU_INTERSTATE_ROAD_ID = 0x684561A168000000ULL;

    // The road-index value the walk stores for "the player is leaving the
    // interstate" (miOriginalInterStateRoadIndex holds the real road; see
    // FindUpcomingStreetsFromRoute, which writes -2 into mLeft/mRightRoadIndex).
    // Compared as a raw immediate by the X360 (cmpwi r29, -2).
    const BrnStreetData::RoadIndex KI_INTERSTATE_EXIT_ROAD_INDEX = -2;
}

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82348798. Packs the upcoming-road GUI message -- the left/right road
    // ids plus each side's par / friend / user score records -- and posts it on
    // the GUI output queue. Perf-monitored end to end with
    // miUpcomingRoadsSentMessagePM.
    //
    // NOTE the frozen X360 arity: lpLastAICarOutputInterface (r5) and
    // lpActiveRaceCarInterface (r7) are carried but never read by this body.
    // -----------------------------------------------------------------------
    void StreetManager::SendUpcomingRoadMessage(
            GameStateModuleIO::OutputBuffer* lpOutput,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            bool lbForce,
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface )
    {
        (void)lpLastAICarOutputInterface;
        (void)lpActiveRaceCarInterface;

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsSentMessagePM );

        // The message is deliberately NOT cleared: the -1 / -2 road-index paths
        // below leave their side's score records as whatever the stack held, and
        // the X360 does exactly the same (no memset in the prologue).
        UpcomingRoadMessage lMessage;

        // The two 16-byte vector slots: mFirstJunctionPosition promoted to a
        // 4-lane vector twice (the two staging buffers at var_210 and var_220).
        Vector3 lJunctionPosition(mFirstJunctionPosition.X(), mFirstJunctionPosition.Y(), mFirstJunctionPosition.Z());

        Vector3 lSecondJunctionPosition(mFirstJunctionPosition.X(), mFirstJunctionPosition.Y(), mFirstJunctionPosition.Z());

        lMessage.miCurrentRoadIndex = miLastPlayerRoadIndex;
        lMessage.miLeftRoadIndex    = mLeftRoadIndex;
        lMessage.miRightRoadIndex   = mRightRoadIndex;

        lMessage.mLeftRoadId  = 0;
        lMessage.mRightRoadId = 0;
        lMessage.mu8LeftRoadIsInterstate  = 0;
        lMessage.mu8RightRoadIsInterstate = 0;
        lMessage.mfLeftSignValue  = 0.0f;
        lMessage.mfRightSignValue = 0.0f;
        lMessage.miCurrentRoadHighlightState = -1;
        lMessage.miLeftRoadHighlightState    = -1;

        lMessage.mJunctionPosition       = lJunctionPosition;
        lMessage.mSecondJunctionPosition = lSecondJunctionPosition;

        // ---- left side ------------------------------------------------------
        const BrnStreetData::RoadIndex liLeftRoadIndex = mLeftRoadIndex;
        if ( liLeftRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
        {
            lMessage.mLeftRoadId  = 0;
            mbLeftRoadHighlighted = false;
        }
        else if ( liLeftRoadIndex == KI_INTERSTATE_EXIT_ROAD_INDEX )
        {
            lMessage.mu8LeftRoadIsInterstate = 1;
            lMessage.mLeftRoadId             = KU_INTERSTATE_ROAD_ID;
        }
        else
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liLeftRoadIndex );
            const ::CgsID lLeftRoadId = lpRoad->GetId();

            lMessage.mu8LeftRoadIsInterstate = 0;
            lMessage.mLeftRoadId             = lLeftRoadId;

            // Persisted save-game challenge slot for this road (miss -> -1, which
            // makes the user/friend getters early out).
            BrnStreetData::ChallengeIndex liSaveGameSlotIndex = -1;
            {
                BrnStreetData::ChallengeIndex liSlot = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lLeftRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSlot;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSlot = -1;
                        break;
                    }
                }
                liSaveGameSlotIndex = liSlot;
            }

            // Par scores index by the ROAD index; the user/friend tables by the
            // save-game slot (lbByRoadIndex == false).
            GetChallengeParScore( mLeftRoadIndex, &lMessage.mLeftParScore );
            GetChallengeUserScore( liSaveGameSlotIndex, &lMessage.mLeftUserScore, false );
            GetChallengeFriendHighScore( liSaveGameSlotIndex, &lMessage.mLeftFriendHighScore, false );

            if ( !lMessage.mLeftParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_TIME ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find time road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mLeftRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }

            if ( !lMessage.mLeftParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_CRASH ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find crash road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mLeftRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }
        }

        // ---- right side -----------------------------------------------------
        // (the right-hand warnings ALSO read "on left side" in the retail image --
        //  a copy-paste in the original source; kept verbatim.)
        const BrnStreetData::RoadIndex liRightRoadIndex = mRightRoadIndex;
        if ( liRightRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
        {
            lMessage.mRightRoadId  = 0;
            mbRightRoadHighlighted = false;
        }
        else if ( liRightRoadIndex == KI_INTERSTATE_EXIT_ROAD_INDEX )
        {
            lMessage.mu8RightRoadIsInterstate = 1;
            lMessage.mRightRoadId             = KU_INTERSTATE_ROAD_ID;
        }
        else
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liRightRoadIndex );
            const ::CgsID lRightRoadId = lpRoad->GetId();

            lMessage.mu8RightRoadIsInterstate = 0;
            lMessage.mRightRoadId             = lRightRoadId;

            BrnStreetData::ChallengeIndex liSaveGameSlotIndex = -1;
            {
                BrnStreetData::ChallengeIndex liSlot = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lRightRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSlot;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSlot = -1;
                        break;
                    }
                }
                liSaveGameSlotIndex = liSlot;
            }

            GetChallengeParScore( mRightRoadIndex, &lMessage.mRightParScore );
            GetChallengeUserScore( liSaveGameSlotIndex, &lMessage.mRightUserScore, false );
            GetChallengeFriendHighScore( liSaveGameSlotIndex, &lMessage.mRightFriendHighScore, false );

            if ( !lMessage.mRightParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_TIME ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find time road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mRightRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }

            if ( !lMessage.mRightParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_CRASH ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find crash road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mRightRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }
        }

        // ---- highlight states ------------------------------------------------
        if ( !lbForce )
        {
            lMessage.mfLeftSignValue             = 0.0f;
            lMessage.miLeftRoadHighlightState    = 0;
            lMessage.miRightRoadHighlightState   = 0;
            lMessage.miCurrentRoadHighlightState = 0;
        }
        else if ( mbLeftRoadHighlighted )
        {
            lMessage.miRightRoadHighlightState   = 1;
            lMessage.mRightRoadId                = 0;
            lMessage.miLeftRoadHighlightState    = 2;
            lMessage.miCurrentRoadHighlightState = 0;
        }
        else
        {
            lMessage.miCurrentRoadHighlightState = 0;
            lMessage.miLeftRoadHighlightState    = 1;
            lMessage.mLeftRoadId                 = 0;

            if ( mbRightRoadHighlighted )
            {
                lMessage.miRightRoadHighlightState = 2;
            }
            else
            {
                lMessage.mfLeftSignValue           = 0.0f;
                lMessage.miRightRoadHighlightState = 1;
                lMessage.mRightRoadId              = 0;
                lMessage.mfRightSignValue          = -1.0f;
            }
        }

        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
        lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lMessage ), 276, 368 );

        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsSentMessagePM );
    }
}

// FOLDED FROM BrnGameStateStreetManager_FindRivalsByDistrict.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_FindRivalsByDistrict.cpp
//   BrnGameState::StreetManager::FindRivalsByDistrict  @ 0x82336360
//
// ⭐ SetupParRivals' ONLY unhomed callee. Split out of the wave-C group-4 partfile
// BrnGameStateStreetManager_wC_04.cpp (which also carries FillInRoadRulesQuery @0x823365A8
// and GetNumberOfCompleteRoadsRuledByLocalPlayer @0x8233F350) ON PURPOSE, and it is MEASURED
// (cl /c with the build's own flags + dumpbin /SYMBOLS against the defined-symbol set of
// build\game\obj): mounting the whole wC_04 partfile costs FOUR unresolved externals --
// StreetManager::GetStreetData, ::HasPlayerBeatenParScore, ::HasPlayerBeatenFriendScore and
// BrnProgression::Rival::GetDistrict -- all four pulled in by those two siblings' road-score
// tallies. FindRivalsByDistrict touches only the last of them (now a header inline in
// BrnRival.h), so this split costs ZERO. Established repo pattern
// (BrnGameStateStreetManager_Prepare.cpp, BrnTriggerQueryManager_Prepare.cpp,
// BrnCarSelectManager_CarChange.cpp). Fold back into wC_04 when the three score accessors land.
//
// The body below is MOVED, not copied -- wC_04 no longer defines it (a duplicate definition
// would be LNK2005 the moment both TUs mount).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"  // ProgressionManager::GetProgressionData
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                      // Rival::GetId / GetDistrict

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x82336360. Collect up to liMaxRivals rival ids whose district matches leDistrict from the
// progression data's rival table, returning how many were written.
//
// Store for store against the asm:
//   0x8233636C-A8  the null-checked ResourcePtr read at mpProgressionManager (+0x1D10) + 133348
//                  == the committed ProgressionManager::GetProgressionData() accessor.
//   0x823363AC     `lwz r11, 0x2C(r28)` == ProgressionData::GetRivalCount(), re-materialised on
//                  EVERY loop-condition evaluation (0x82336420 reloads it) -- NOT hoisted here.
//   0x823363D0     `cmpw r27, r23 / bge` -- the found-count cut-out is tested at the TOP of the
//                  iteration, BEFORE the rival is fetched, so the last iteration never reaches
//                  GetRival's bounds assert.
//   0x823363D8-F4  the baked "liIndex < miRivalCount" assert (BrnProgressionData.h:460) belongs
//                  to ProgressionData::GetRival -- it is NOT duplicated here.
//   0x823363F8-08  `lwz 0x28 / add r31 (stride 0x38) / lbz 0x14 / extsb` == GetRival(i)->
//                  GetDistrict(), compared SIGNED against the wanted district.
//   0x82336410-1C  `ld r11, 0(r11) / std r11, 0(r30) / addi r30, r30, 8` == an 8-byte CgsID
//                  (Rival::GetId()) written to the caller's array, which is why lpaRivalIds is
//                  CgsID* and the count post-increments.
//
// ⚠️ NO NULL GUARD HERE, ON EITHER POINTER -- kept faithful, DELIBERATELY, and this is not an
// oversight left over from the 2026-08-11 null-back-pointer crash. Two reads could fault:
//   * mpProgressionManager -- the member the first post-un-park boot AV'd on. Its writer is now
//     GameStateModule::Construct -> StreetManager::Construct, which asserts it non-null.
//   * lpProgressionData -- the X360 itself loads GetRivalCount off a possibly-NULL pointer
//     (0x823363AC reads 0x2C(r28) with r28 == 0 on the null path). SetupParRivals guards that
//     one too, upstream.
// SetupParRivals @0x8233F560 is this function's only caller in the whole image (verified by
// grep over the tree and by the X360 xrefs), so guarding once upstream covers it without
// putting a check in a body the console does not have one in. If a SECOND caller is ever
// mounted, it must carry the same two guards -- or this function grows them and the note goes.
// ---------------------------------------------------------------------------
s32 StreetManager::FindRivalsByDistrict( s32 leDistrict, ::CgsID* lpaRivalIds, s32 liMaxRivals )
{
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    s32 liNumberOfRivalsFound = 0;

    for ( s32 liRivalIndex = 0; liRivalIndex < lpProgressionData->GetRivalCount(); ++liRivalIndex )
    {
        if ( liNumberOfRivalsFound >= liMaxRivals )
        {
            break;
        }

        const BrnProgression::Rival* lpRival = lpProgressionData->GetRival( liRivalIndex );

        if ( static_cast<s32>( lpRival->GetDistrict() ) == leDistrict )
        {
            lpaRivalIds[liNumberOfRivalsFound] = lpRival->GetId();
            ++liNumberOfRivalsFound;
        }
    }

    return liNumberOfRivalsFound;
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_Prepare.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_Prepare.cpp
//   BrnGameState::StreetManager::Prepare  @ 0x82350900
//
// ⭐ THIS IS THE DISTRICT-MAP LEG. GameStateModule::Prepare @0x8239E578 stage 23
// (E_PREPARESTAGE_STREET_MANAGER) is its only caller:
//     *(this+552) = 23;
//     if (!StreetManager::Prepare(this + 284520, out, this + 232384)) break;
// and Prepare is what drives LoadDistrictMap @0x8234FB98 -- the ONLY writer of
// mDistrictMapResourceHandle, which StreetManager::SetupParRivals @0x8233F560
// dereferences unconditionally.
//
// SIBLING TU (split out of the wave-B group-0 partfile BrnGameStateStreetManager_wB_00.cpp,
// which also carries Construct @0x82335978 and Destruct @0x82335D40) ON PURPOSE, and it is
// MEASURED (cl /c with the build's own flags + dumpbin /SYMBOLS against the linked obj set):
// mounting the whole wB_00 partfile costs ONE unresolved external --
// BrnStreetData::operator++(ScoreType&, int), whose body lives in the street-DATA side's
// SharedClasses/StreetData/BrnChallengeData.cpp -- pulled in by Construct's and Destruct's
// score-type loops. Prepare touches none of it, so this split costs ZERO. Established repo
// pattern (BrnTriggerQueryManager_Prepare.cpp, BrnCarSelectManager_CarChange.cpp,
// BrnGameStateStreetManager_wB_01.cpp). Fold back into wB_00 when that one symbol lands.
//
// ⚠️ StreetManager::Construct is still not called by GameStateModule::Construct (see the
// DELETE-WHEN block there): the embedded sub-object's stage words / owner pointers come from
// the frozen header's in-class initialisers instead, which is exactly why
// meDistrictMapLoadStage starts at E_DISTRICT_MAP_LOAD_REQUEST rather than an indeterminate
// value on the first tick.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"  // the embedded component
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // [diagnostic] gpDebugPrint
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h"    // [diagnostic] BinaryFileResource::GetData

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82350900. Streamed prepare: pump LoadAIData then LoadDistrictMap; once
    // both complete, (re)construct + register the debug component and report done.
    //
    // The asm is literally:
    //     assert(lpOutput)         BrnGameStateStreetManager.cpp:281
    //     assert(lpReceiverQueue)  BrnGameStateStreetManager.cpp:282
    //     if (LoadAIData(...) && LoadDistrictMap(...)) {
    //         <DebugComponent base Construct on this+0x1DF0 -- the COMDAT-folded symbol IDA
    //          names BaseCollisionGenerator::Destruct>
    //         *(this+0x1E00) = this;   // mpStreetManager
    //         *(this+0x1DFC) = 0;      // miNumberOfRoadRulesToWin
    //         DebugComponent::Register(this + 0x1DF0);
    //         return 1;
    //     }
    //     return 0;
    // -- the two stores are StreetManagerDebugComponent::Construct's inlined body (its
    // declaration in BrnStreetManagerDebugComponent.h documents both attestation sites), so
    // they go through the named method here.
    // -----------------------------------------------------------------------
    bool StreetManager::Prepare( GameStateModuleIO::OutputBuffer* lpOutput,
                                 CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue )
    {
        CGS_ASSERT( lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 281) );
        CGS_ASSERT( lpReceiverQueue, CGS_ASSERT_MESSAGE_AT("lpReceiverQueue", CGS_ARTIST_FILE, 282) );

        if ( LoadAIData( lpOutput, lpReceiverQueue ) && LoadDistrictMap( lpOutput, lpReceiverQueue ) )
        {
            mStreetManagerDebugComponent.Construct( this );
            mStreetManagerDebugComponent.Register();

            // [diagnostic, one-shot] RESIDENT IS NOT USABLE. This walks the EXACT path
            // SetupParRivals @0x8233F560 takes -- handle -> the BinaryFileResource pointer the
            // handle slot holds -> GetData() -> the packed {u16 width, u16 height} grid header
            // WorldMap2D::Construct binds -- so a 256x256 line here is proof the district map is
            // genuinely usable, not merely "acquired". Delete with the rest of the bring-up
            // diagnostics.
            //
            // ⭐ IT ALREADY EARNED ITS KEEP (2026-08-11). The previous boot printed
            // `[StreetManager] district map: handle=0` two lines after the bundle loaded, and
            // that was NOT a diagnostic-timing artifact -- the placement is right. Traced:
            // LoadDistrictMap's ACQUIRE_RESPONSE stage only advances to DONE inside the
            // `GetCount() > 0` branch that also does the bind (console 0x8234FC50 case 3), and
            // Prepare only reaches this block once LoadDistrictMap has returned true, i.e. one
            // full tick AFTER the bind ran. So `handle=0` meant the bind stored a null, which
            // means the pool replied with the both-null handle, which means the acquire itself
            // never matched -- and it didn't: the request carried a POOL-TAGGED resource id and
            // was posted at the console's 32-bit record size. Both fixed in
            // BrnGameStateStreetManager_wB_01.cpp (full evidence there).
            // NEXT BOOT MUST PRINT: `district map: handle=1 grid=256x256`. Anything else and
            // SetupParRivals' PC guard will fire and name itself; do not re-park the leg on a
            // "timing" story without re-reading that stage machine.
            //
            // Latched: GameStateModule::Prepare re-arms its stage word on completion, so this
            // block is otherwise re-entered on every subsequent prepare pass.
            static bool sbDistrictMapLogged = false;
            if ( !sbDistrictMapLogged
              && ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
              && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbDistrictMapLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[StreetManager] district map: handle="
                    << ( mDistrictMapResourceHandle.mpResourceMemory != 0 ? 1 : 0 );

                if ( mDistrictMapResourceHandle.mpResourceMemory != 0 )
                {
                    const CgsResource::BinaryFileResource* lpBinaryFileResource =
                        *reinterpret_cast<const CgsResource::BinaryFileResource* const*>(
                            mDistrictMapResourceHandle.mpResourceMemory );

                    if ( lpBinaryFileResource != 0 )
                    {
                        // The blob's own header, as WorldMap2D::Construct @0x82907FD0 reads it.
                        const u16* lpuGridHeader =
                            static_cast<const u16*>( lpBinaryFileResource->GetData() );
                        *CgsDev::Log::gpDebugPrint
                            << " grid=" << static_cast<u32>( lpuGridHeader[0] )
                            << "x"      << static_cast<u32>( lpuGridHeader[1] );
                    }
                    else
                    {
                        *CgsDev::Log::gpDebugPrint << " grid=NO-MEMORY-RESOURCE";
                    }
                }
                *CgsDev::Log::gpDebugPrint << "\n";
            }

            return true;
        }

        return false;
    }
}

// FOLDED FROM BrnGameStateStreetManager_Prepare2.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_Prepare2.cpp
//   BrnGameState::StreetManager::Prepare2  @ 0x823509D8
//   (the DWARF :222 3-param `Prepare`; kept under its X360 symbol name)
//
// ⭐ THE STREET LEG OF GameStateModule::Prepare2 case 2. Two things and nothing else:
//     if (LoadStreetData(this, out, rq)) { SetupParRivals(this, tqm); return 1; }
//     return 0;
//
// SPLIT OUT of BrnGameStateStreetManager.cpp (which also carries the two score-entry
// factories CreateHighScoreEntryFromDown @0x82324A28 and CreateUserScoreEntryFromDown) ON
// PURPOSE, and it is MEASURED (cl /c with the build's own flags + dumpbin /SYMBOLS against the
// defined-symbol set of build\game\obj): mounting that whole TU costs SIX unresolved externals
// -- BrnStreetData::operator++(ScoreType&, int), ChallengeHighScoreEntry::Construct,
// ChallengePlayerScoreEntry::Construct, ChallengeData::SetScore and the two ScoreList
// KAI_MIN_SCORES / KAI_MAX_SCORES tables -- every one of them pulled in by those two factories'
// score-type loops. Prepare2 touches none of it, so this split costs ZERO. Established repo
// pattern (BrnGameStateStreetManager_Prepare.cpp, BrnTriggerQueryManager_Prepare.cpp,
// BrnCarSelectManager_CarChange.cpp). Fold back in when the ChallengeData score family lands.
//
// The body below is MOVED, not copied -- BrnGameStateStreetManager.cpp no longer defines it.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"   // CGS_ASSERT

namespace BrnGameState
{

// @ 0x823509D8. Pure forwarder: the two entry asserts (BrnGameStateStreetManager.cpp:281/:282
// in the console's baked literals), the gated LoadStreetData pump, and -- only once it reports
// complete -- the one-shot SetupParRivals pass. Returns false while the street data is still
// streaming, which is what makes the caller pump it once per frame.
bool StreetManager::Prepare2( GameStateModuleIO::OutputBuffer* lpOutput,
                              CgsModule::EventReceiverQueue<3072,16>* lpReceiverQueue,
                              const TriggerQueryManager* lpTriggerQueryManager )
{
    CGS_ASSERT( lpOutput != NULL,
        CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 314 /* ARTIST 0x82350A0C */));
    CGS_ASSERT( lpReceiverQueue != NULL,
        CGS_ASSERT_MESSAGE_AT("lpReceiverQueue", CGS_ARTIST_FILE, 315 /* ARTIST 0x82350A30 */));

    if ( !LoadStreetData( lpOutput, lpReceiverQueue ) )
    {
        return false;
    }

    SetupParRivals( lpTriggerQueryManager );
    return true;
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_SetupParRivals.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_SetupParRivals.cpp
//   BrnGameState::StreetManager::SetupParRivals  @ 0x8233F560
//
// ⭐ THE SECOND HALF OF Prepare2. StreetManager::Prepare2 @0x823509D8 is exactly
// `if (LoadStreetData(out, rq)) { SetupParRivals(tqm); return 1; } return 0;` and this is the
// half GameStateModule::Prepare2 case 2 had been parked on since the district-map wave.
//
// SPLIT OUT of the wave-C group-2 partfile BrnGameStateStreetManager_wC_02.cpp (which also
// carries ProcessScoreRequestEvent @0x8234A240) ON PURPOSE, and it is MEASURED (cl /c with the
// build's own flags + dumpbin /SYMBOLS against the defined-symbol set of build\game\obj):
// mounting the whole wC_02 partfile costs THIRTEEN unresolved externals -- ChallengeData::
// GetScore/SetScore/CompareScores, ChallengeParScoresEntry::Copy/GetScore, ChallengeHighScore
// Entry pieces, StreetManager::GetHighScoreEntry/GetChallengeUserScore/GetParRivalId,
// OutputBuffer::GetGuiOutputQueue, CgsNetwork::PlayerName::Construct, CgsCore::SPrintf and the
// CgsDev::StrStream chain (which also drags _purecall / type_info / operator delete) -- and
// EVERY ONE of them belongs to ProcessScoreRequestEvent. SetupParRivals touches none of it, so
// this split costs ZERO. Established repo pattern (BrnGameStateStreetManager_Prepare.cpp,
// BrnTriggerQueryManager_Prepare.cpp, BrnCarSelectManager_CarChange.cpp). Fold back into wC_02
// when the score-entry family lands.
//
// The body below is MOVED, not copied -- wC_02 no longer defines it.
//
// ===========================================================================
// ⭐ MEMBER AUDIT -- EVERY `this->` READ ON THIS LEG AND ITS WRITER (2026-08-11).
//
// Written after the first boot AV'd on the one member nobody had checked. StreetManager is not
// Construct()ed on PC at the time (Construct runs now), so "the member exists" does not
// imply "something wrote it". Anyone extending this leg must extend this table.
//
//   member                       read by            writer on the PC path                status
//   ---------------------------  -----------------  -----------------------------------  ------
//   mpProgressionManager         this, _FindRivals  GameStateModule::Construct ->         ⭐ WIRED
//                                                   StreetManager::Construct
//   mpStreetData (ResourcePtr)   this               StreetManager::LoadStreetData         ✅ gated:
//                                                   @0x8234F630 (wB_01, mounted)          Prepare2
//                                                   -- the ONLY writer in the image       returns
//                                                                                         false
//                                                                                         until it
//                                                                                         binds
//   mDistrictMapResourceHandle   this               StreetManager::LoadDistrictMap        ✅ +guard
//                                                   @0x8234FB98 (wB_01, mounted), pumped  below
//                                                   by Prepare stage 23
//   maaParRivalIds               this               THIS function (pure output)           ✅ n/a
//   mpGameStateModule            -- not read on this leg (wired anyway, it is free)
//   mpRoadRulesManager           -- not read on this leg; stays 0, see the header note
//
// Members reached THROUGH those pointers are audited at their own guards below:
// ProgressionManager::mpProgressionData (writer: ProgressionManager::LoadProgressionData, run by
// GameStateModule::Prepare2 case 0/1, which completes before case 2 falls into the street leg).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"    // ProgressionManager::GetProgressionData
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TriggerQueryManager::GetTriggerData
#include "SharedClasses/Progression/BrnProgressionData.h"              // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                        // BrnProgression::Rival::GetId
#include "SharedClasses/StreetData/BrnStreetData.h"                    // StreetData::GetRoadCount / GetRoad / Road::GetRoadLimitId0
#include "SharedClasses/StreetData/BrnChallengeData.h"                 // BrnStreetData::E_SCORE_TYPE_COUNT
#include "SharedClasses/Trigger/BrnTriggerData.h"                      // BrnTrigger::TriggerData::GetGenericRegion(Count)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                    // BrnTrigger::GenericRegion (+ TriggerRegion / BoxRegion)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                // CgsWorld::WorldMap2D / KU_INVALID_WORLD_MAP_VALUE
#include "GameShared/GameClasses/Numeric/CgsRandom.h"                  // CgsNumeric::Random
#include "GameShared/GameClasses/System/Resource/CgsBinaryFileResource.h" // CgsResource::BinaryFileResource::GetData
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace
{
    // -------------------------------------------------------------------
    // SetupParRivals' two lvx128 sources (@0x8233F5C8 / 0x8233F5D4) are the
    // TU's own zero-initialised .data vectors unk_82FADD60 / unk_82FADAA0,
    // filled by its static initialisers @0x82C4C2E0 / 0x82C4C320 from the
    // rodata floats flt_8200D4EC/E8 (-4208, -3846) and flt_8200D4F4/F0
    // (8270, 6101). SetupParRivals is their only consumer: they are the
    // district-map's world rectangle handed to WorldMap2D::Construct.
    // FLAG: the identifiers are ours (no DWARF name survives); the VALUES are
    // byte-exact from the image.
    // -------------------------------------------------------------------
    const Vector2 KV2_DISTRICT_MAP_WORLD_ORIGIN(-4208.0f, -3846.0f);   // unk_82FADAA0
    const Vector2 KV2_DISTRICT_MAP_WORLD_SIZE( 8270.0f,  6101.0f);   // unk_82FADD60

    // The LCG state SetupParRivals draws its rival picks from is materialised as a 64-bit
    // immediate at 0x8233F5D8..0x8233F5E8 (lis/ori 0x2EC654DA + insrdi of 0xB5E330D0 into the
    // high half) and stepped in r27 (0x8233F898 `srdi` / 0x8233F89C `mulld`). That immediate IS
    // the state CgsNumeric::Random::Construct() leaves: Construct is SetSeed(DWARF default seed
    // 2413850050), whose eight priming steps take 2413850050 to exactly 0xB5E330D02EC654DA. The
    // ring buffer is never read here, so the compiler folded the whole prime into the constant;
    // the source is a bare Construct(). [FX-GS2 2026-09-23, G12-D11] A SetSeed(0xB5E330D02EC654DA)
    // used to follow the Construct(), and it moved the seed off the console's value.
}

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x8233F560. Places the two par rivals for every road: the road's
// RoadLimitId0 selects the matching generic trigger region, that region's box
// position is sampled against the "Districts" WorldMap2D, and two rivals are
// drawn at random out of the district's rival set (falling back to rival 0 when
// the position is off-map or the district has no rivals).
// ---------------------------------------------------------------------------
void StreetManager::SetupParRivals( const TriggerQueryManager* lpTriggerQueryManager )
{
    const BrnTrigger::TriggerData* lpTriggerData = lpTriggerQueryManager->GetTriggerData();

    // The committed accessor IS the asm's null-checked ResourcePtr read
    // (mpProgressionManager + 133348).
    const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();

    // The accessor tests the ProgressionData resource pointer as ARTIST does. The fallback
    // below dereferences its result; GameStateModule::Prepare2 prepares progression before street.

    CgsNumeric::Random lRandom;
    lRandom.Construct();   // muSeed == 0xB5E330D02EC654DA, the console's r27 (see the note above)

    // ARTIST dereferences the acquired Districts resource without a null test (0x8233F5BC..
    // 0x8233F5F4); StuntManager::Prepare acquires it before StreetManager::Prepare2 runs.
    const CgsResource::BinaryFileResource* lpBinaryFileResource =
        *reinterpret_cast<const CgsResource::BinaryFileResource* const*>( mDistrictMapResourceHandle.mpResourceMemory );

    CgsWorld::WorldMap2D lWorldMap;
    lWorldMap.Construct( lpBinaryFileResource->GetData(),
                         KV2_DISTRICT_MAP_WORLD_ORIGIN,
                         KV2_DISTRICT_MAP_WORLD_SIZE );

    CGS_ASSERT( mpStreetData->GetRoadCount() <= KI_MAX_CHALLENGES,
                CGS_ASSERT_MESSAGE_AT("mpStreetData->GetRoadCount() <= KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 3978) );

    for ( BrnStreetData::RoadIndex liRoadIndex = 0;
          liRoadIndex < mpStreetData->GetRoadCount();
          ++liRoadIndex )
    {
        // GetRoad carries its own "liIndex < miRoadCount && liIndex >= 0" bounds assert.
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liRoadIndex );

        for ( s32 liGenericRegionIndex = 0;
              liGenericRegionIndex < lpTriggerData->GetGenericRegionCount();
              ++liGenericRegionIndex )
        {
            // GetGenericRegion carries its own "liGenericRegionIndex < miGenericRegionCount" assert.
            const BrnTrigger::GenericRegion* lpGenericRegion =
                lpTriggerData->GetGenericRegion( liGenericRegionIndex );

            // 0x8233F758 `ld r10, 0x18(r23)` == Road::GetRoadLimitId0() (an 8-byte CgsID),
            // compared 64-bit-wide against the sign-extended 32-bit GenericRegion::mId.
            if ( lpGenericRegion->GetId() == lpRoad->GetRoadLimitId0() )
            {
                // ⚠️ THE SWIZZLE IS THE CALL SITE'S, AND IT IS NOT (x, y).
                // The console loads the region's three position floats into consecutive stack
                // slots (0x8233F770/84/88 -> var_100/var_FC/var_F8), lvx128's them into v0, and
                // then does `vperm v1, v0, v0, v7` (0x8233F7A8) BEFORE the bl to
                // WorldMap2D::GetValue -- i.e. GetValue(Vector3)'s ground-plane swizzle, inlined
                // here. GetValue itself reads only lanes 0 and 1 (`vspltw v10,v1,0` /
                // `vspltw v9,v1,1` @0x82907FF8), so if the wanted lanes were already 0 and 1 the
                // compiler would have passed v0 straight through and emitted no vperm at all.
                // The permute therefore PROVES the sampled pair is not (x, y); a 2D world map
                // over a Y-up world samples (x, z), which is also what the map's own rectangle
                // says (origin (-4208, -3846), size (8270, 6101) -- Paradise City's x/z extents,
                // not its height range).
                // FLAG: the control vector itself (unk_82CDA450) is data the exports do not
                // carry, so the ORDER (x, z) rather than (z, x) is inferred from that rectangle,
                // not read out of the image. Built explicitly here rather than through
                // CgsWorldMap2D.cpp's Vector3 overload, which flattens to (.x, .y).
                const Vector3 lRegionPosition = lpGenericRegion->GetBoxRegion()->GetPosition();
                const Vector2 lSamplePosition(lRegionPosition.mV.x, lRegionPosition.mV.z);

                const u8 luDistrict = lWorldMap.GetValue( lSamplePosition );

                ::CgsID laRivalIds[2];
                s32     liRivalsFound = 0;

                if ( luDistrict != CgsWorld::KU_INVALID_WORLD_MAP_VALUE
                  && ( liRivalsFound = FindRivalsByDistrict( luDistrict, laRivalIds, 2 ) ) != 0 )
                {
                    for ( s32 liParRival = 0; liParRival < BrnStreetData::E_SCORE_TYPE_COUNT; ++liParRival )
                    {
                        // RandomInt owns the baked "liMax >= liMin" / "luMod > 0" asserts
                        // (CgsRandom.h:320/:323 -- the console fires them once per pick, which
                        // is why the call is inside the loop rather than hoisted).
                        maaParRivalIds[liRoadIndex][liParRival] =
                            laRivalIds[ lRandom.RandomInt( 0, liRivalsFound - 1 ) ];
                    }
                }
                else
                {
                    CGS_ASSERT( lpProgressionData->GetRivalCount() > 0,
                                "lpProgressionData->GetRivalCount() > 0" );

                    for ( s32 liParRival = 0; liParRival < BrnStreetData::E_SCORE_TYPE_COUNT; ++liParRival )
                    {
                        // GetRival(0) is re-evaluated per pick -- its "liIndex < miRivalCount"
                        // bounds assert fires once per stored id in the asm.
                        maaParRivalIds[liRoadIndex][liParRival] = lpProgressionData->GetRival( 0 )->GetId();
                    }
                }
            }
        }
    }

}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wB_03.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_03.cpp
//   (wave B partfile -- group 3 "set-scores":
//        SetChallengeUserScore / CopyUserChallengeData / ClearAllChallengeData)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::SetChallengeUserScore  @ 0x82336070
//   BrnGameState::StreetManager::CopyUserChallengeData  @ 0x82317510
//   BrnGameState::StreetManager::ClearAllChallengeData  @ 0x82324CA8
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (BrnGameStateStreetManager.h), never raw-offset
// casts.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <cstring>   // memcpy

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82336070. Store one local-player challenge score entry into a slot.
    // When lbUpdateHighScore is set the passed index is a ROAD index: it is
    // resolved through the save-game challenge road-id table first (a miss
    // early-outs without writing). Once resolved, a bounds-checked slot in
    // maChallengeData receives the entry via ChallengePlayerScoreEntry::Copy.
    // -----------------------------------------------------------------------
    void StreetManager::SetChallengeUserScore( BrnStreetData::ChallengeIndex liRoadIndex,
                                               const BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
                                               bool lbUpdateHighScore )
    {
        CGS_ASSERT( lpEntry, CGS_ASSERT_MESSAGE_AT("lpData", CGS_ARTIST_FILE, 3654) );

        if ( lbUpdateHighScore )
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liRoadIndex );

            BrnStreetData::ChallengeIndex liChallengeIndex = 0;
            const ::CgsID* lpTableEntry = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
            while ( *lpTableEntry != lpRoad->GetId() )
            {
                ++lpTableEntry;
                ++liChallengeIndex;
                if ( lpTableEntry >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                {
                    return;
                }
            }
            liRoadIndex = liChallengeIndex;
        }

        if ( liRoadIndex >= 0 )
        {
            CGS_ASSERT( liRoadIndex < KI_MAX_CHALLENGES,
                        CGS_ASSERT_MESSAGE_AT("liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 3665) );
            maChallengeData[liRoadIndex].Copy( lpEntry );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82317510. Bulk-copy the whole 64-entry local-player challenge table
    // out to lpDest, guarded on the destination buffer being large enough.
    // -----------------------------------------------------------------------
    void StreetManager::CopyUserChallengeData( BrnStreetData::ChallengePlayerScoreEntry* lpDest,
                                               s32 liSizeInBytes )
    {
        CGS_ASSERT( lpDest, CGS_ASSERT_MESSAGE_AT("lpDest", CGS_ARTIST_FILE, 3941) );

        if ( liSizeInBytes >= static_cast<s32>( sizeof( maChallengeData ) ) )
        {
            memcpy( lpDest, maChallengeData, sizeof( maChallengeData ) );
        }
        else
        {
            // X360 composes this through a gpcMessageBuffer StrStream; per the
            // project convention the streamed message lowers to CGS_ASSERT.
            CGS_ASSERT( false, " not enough space to copy into " );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82324CA8. Reset every challenge row: re-Construct the network
    // high-score entry (base ChallengeData::Construct + blank owner names) then
    // the local-player score entry, for all KI_MAX_CHALLENGES rows.
    // -----------------------------------------------------------------------
    // ARTIST 0x8234A1D8 calls the out-of-line reset at 0x82324CA8.
    __declspec(noinline) void StreetManager::ClearAllChallengeData()
    {
        for ( BrnStreetData::ChallengeIndex liChallenge = 0;
              liChallenge < KI_MAX_CHALLENGES;
              ++liChallenge )
        {
            maNetworkChallengeData[liChallenge].Construct();
            maChallengeData[liChallenge].Construct();
        }
    }
}

// FOLDED FROM BrnGameStateStreetManager_wB_04.cpp
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/Progression/BrnProgressionManager.h"        // ProgressionManager::OnTrophyUnlock / CheckForSpecialCarUnlocks / GetAchievementManager
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnSetAllRoadRules
#include "SharedClasses/StreetData/BrnStreetData.h"                        // StreetData::GetRoadCount
#include "SharedClasses/StreetData/BrnChallengeData.h"                     // BrnStreetData::ScoreType
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_04.cpp  --  wave-B group 5 ("tallies+trophies")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::UpdateTrophyUnlockOnRoadRuleWin  @ 0x82341780
//
// After the local player wins a road rule of the given ScoreType, this re-tallies
// the ruled-road counts (order: show-time/crash, time-trial/time, complete), asserts
// each against the total road count (StreetData::GetRoadCount, X360 read of
// StreetData+0x20 through mpStreetData.operator->), then fans out the road-rule
// trophies via the ProgressionManager (X360 mpProgressionManager @ +0x1D10):
//   CRASH: if all show-time roads ruled -> trophy 7 + OnSetAllRoadRules(CRASH),
//          then trophy 25;
//   TIME:  if all time-trial roads ruled -> trophy 6 + OnSetAllRoadRules(TIME),
//          then trophy 24;
//   any:   if all roads complete -> trophy 8; always trophy 23 +
//          ProgressionManager::CheckForSpecialCarUnlocks().
// OnSetAllRoadRules targets mpProgressionManager->GetAchievementManager() (X360
// *(pm+133432)).
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// @ 0x82341780.
void StreetManager::UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::ScoreType leScoreType )
{
    s32 liNumberOfShowTimeRoadsRuled  = GetNumberOfParShowTimeRoadsRuledByLocalPlayer();
    s32 liNumberOfTimeTrailRoadsRuled = GetNumberOfParTimeTrialRoadsRuledByLocalPlayer();
    s32 liNumberOfCompleteRoadsRuled  = GetNumberOfCompleteRoadsRuledByLocalPlayer();

    s32 liTotalNumberOfRoads = GetStreetData()->GetRoadCount();

    CGS_ASSERT( liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads,  CGS_ASSERT_MESSAGE_AT("liNumberOfShowTimeRoadsRuled <= liTotalNumberOfRoads", CGS_ARTIST_FILE, 2807) );
    CGS_ASSERT( liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads, CGS_ASSERT_MESSAGE_AT("liNumberOfTimeTrailRoadsRuled <= liTotalNumberOfRoads", CGS_ARTIST_FILE, 2808) );

    switch ( leScoreType )
    {
    case BrnStreetData::E_SCORE_TYPE_CRASH:
        if ( liNumberOfShowTimeRoadsRuled == liTotalNumberOfRoads )
        {
            mpProgressionManager->OnTrophyUnlock( 7 );
            mpProgressionManager->GetAchievementManager()->OnSetAllRoadRules( BrnStreetData::E_SCORE_TYPE_CRASH );
        }
        mpProgressionManager->OnTrophyUnlock( 25 );
        break;

    case BrnStreetData::E_SCORE_TYPE_TIME:
        if ( liNumberOfTimeTrailRoadsRuled == liTotalNumberOfRoads )
        {
            mpProgressionManager->OnTrophyUnlock( 6 );
            mpProgressionManager->GetAchievementManager()->OnSetAllRoadRules( BrnStreetData::E_SCORE_TYPE_TIME );
        }
        mpProgressionManager->OnTrophyUnlock( 24 );
        break;

    default:
        break;
    }

    if ( liNumberOfCompleteRoadsRuled == liTotalNumberOfRoads )
        mpProgressionManager->OnTrophyUnlock( 8 );

    mpProgressionManager->OnTrophyUnlock( 23 );
    mpProgressionManager->CheckForSpecialCarUnlocks();
}

}   // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wB_05.cpp
// ============================================================================
// b5-decomp/src/GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_05.cpp
// ============================================================================
// BrnGameState::StreetManager -- wave B partfile (group: online-small handlers).
//
// SOURCE-OF-TRUTH: the X360 ARTIST asm is the spine (stores/branches/constants);
// the frozen StreetManager layout header + BrnGameEvents.h event layouts + the
// committed BrnProgression::Profile members supply the named vocabulary. No raw
// `*(p+off)` casts: every offset the asm folds (GetProfile() == pm+0x170,
// muLastRoadRulesResetTime, the road-rules id split, etc.) is mapped to its
// committed named member.
//
//   ProcessUploadEvent            @ 0x8233F3E8  -- mark the uploaded index range clean
//   ProcessDownloadEvent          @ 0x82324B40  -- null-assert only (retail body empty)
//   ProcessOnlineGameLaunchedEvent@ 0x82324B88  -- clear buffer / bits / lost-score count
// ============================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"  // embedded debug component
#include "GameSource/GameState/BrnGameEvents.h"                              // OnlineRoadRules* event layouts
#include "GameSource/GameState/BrnGameActions.h"                             // road-rules game-action family
#include "GameSource/GameState/BrnGameStateModule.h"                         // GameStateModule
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"    // rw::math::fpu::Vector3Template<f32>

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// ProcessUploadEvent  (X360 0x8233F3E8)
//
// For every challenge index in [mStartUploadIndex, mEndUploadIndex): read the
// player's current entry, clear its dirty bits (the score was just uploaded, so
// it no longer needs re-upload), and store it back WITHOUT a high-score update
// (lbUpdateHighScore == false). The X360 folds the clean into a single 8-byte
// zero store over the entry's leading BitArray<2u> mDirty field.
// ----------------------------------------------------------------------------
void StreetManager::ProcessUploadEvent(
    const GameStateModuleIO::OnlineRoadRulesUploadedEvent* lpUploadedEvent )
{
    CGS_ASSERT( lpUploadedEvent, CGS_ASSERT_MESSAGE_AT("lpUploadedEvent", CGS_ARTIST_FILE, 3037) );

    for ( BrnStreetData::ChallengeIndex liChallengeIndex = lpUploadedEvent->mStartUploadIndex;
          liChallengeIndex < lpUploadedEvent->mEndUploadIndex;
          ++liChallengeIndex )
    {
        BrnStreetData::ChallengePlayerScoreEntry lEntry;
        GetChallengeUserScore( liChallengeIndex, &lEntry, false );
        lEntry.mDirty.UnSetAll();
        SetChallengeUserScore( liChallengeIndex, &lEntry, false );
    }
}

// ----------------------------------------------------------------------------
// ProcessDownloadEvent  (X360 0x82324B40)
//
// Retail body is the null-assert only -- the timestamp store the source once
// carried compiled away, so there is nothing else to reconstruct.
// ----------------------------------------------------------------------------
void StreetManager::ProcessDownloadEvent(
    const GameStateModuleIO::OnlineRoadRulesDownloadedEvent* lpDownloadedEvent )
{
    CGS_ASSERT( lpDownloadedEvent,
        CGS_ASSERT_MESSAGE_AT(
            "lpDownloadedEvent",
            CGS_ARTIST_FILE, 3065 /* ARTIST 0x82324B5C */));
}

// ----------------------------------------------------------------------------
// ProcessOnlineGameLaunchedEvent  (X360 0x82324B88)
//
// Reset the new-high-score buffering state at online-game launch: empty the
// buffer, drop the "too many to buffer" flag and the lost-score count, then clear
// every road-rules-changed bit array (one per score type).
// ----------------------------------------------------------------------------
void StreetManager::ProcessOnlineGameLaunchedEvent()
{
    mNewHighScoreBuffer.Clear();
    mbTooManyHighScoresToBuffer = false;
    miNumScoresLost             = 0;

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        maRoadRulesChangedBitArrays[leScoreType].UnSetAll();
    }
}

}   // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wB_06.cpp
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                    // BuddyRemovedEvent / OnlineRoadRulesPersonalBestRecvEvent
#include "GameSource/GameState/BrnGameActions.h"                   // RoadRulesUpdateTargetScoreAction (action 280)
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer (GetGameActionQueue / GetGuiOutputQueue)
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"    // RoadRulesManager::GetCurrentRoadID
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

// ---------------------------------------------------------------------------
// BrnGameStateStreetManager_wB_06.cpp  --  wave-B group 7 ("online-big")
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout:
//   StreetManager::ProcessNetworkHighScoreEvent @ 0x82349F10
//   StreetManager::ProcessBuddyRemoved          @ 0x8234A5A8
//
// Both handlers scan KAA_SAVE_GAME_CHALLENGE_ROAD_IDS for the road's persisted
// challenge slot (the inlined qword_82029FA0 table walk) and, when the current
// road's records changed, post the 104-byte road-score record game action
// (type 280) onto the OutputBuffer game-action queue. mpStreetData->... is the
// committed ResourcePtr operator-> (X360 StreetData_::oper on &mpStreetData).
// ---------------------------------------------------------------------------

namespace BrnGameState
{

// @ 0x82349F10. Applies a downloaded online personal-best record to the friend
// high-score table. Copies the 56-byte score off the event, resolves the road's
// save-game challenge slot, and -- when the record is a friend's best that becomes
// a new high score for the road the player is currently on -- posts the road-score
// game action (type 280) so the GUI refreshes.
void StreetManager::ProcessNetworkHighScoreEvent(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent* lpPersonalBestEvent )
{
    CGS_ASSERT( lpPersonalBestEvent, CGS_ASSERT_MESSAGE_AT("lpPersonalBestEvent", CGS_ARTIST_FILE, 2973) );

    BrnStreetData::ChallengeHighScoreEntry lPersonalBest = lpPersonalBestEvent->mPersonalBestScore;

    const BrnStreetData::ChallengeIndex liChallengeIndex = lpPersonalBestEvent->mPersonalBestChallengeIndex;
    CGS_ASSERT( liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && liChallengeIndex >= 0,
                CGS_ASSERT_MESSAGE_AT("lChallengeIndex < BrnGameState::KI_MAX_CHALLENGES && lChallengeIndex >= 0", CGS_ARTIST_FILE, 2978) );

    const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liChallengeIndex );

    BrnStreetData::ChallengeIndex liSlotIndex = GetSaveGameChallengeIndexByRoadID( lpRoad->GetId() );

    if ( liSlotIndex >= 0 )
    {
        if ( lpPersonalBestEvent->mbWasPBByFriend )
        {
            const bool lbIsNewHighScore = CheckForNewHighScore( liSlotIndex, &lPersonalBest );

            BrnStreetData::ChallengeHighScoreEntry lFriendEntry;
            lFriendEntry.Construct();
            GetChallengeFriendHighScore( liSlotIndex, &lFriendEntry, false );
            lFriendEntry.UpdateEntry( &lPersonalBest, 0 );
            SetChallengeFriendHighScore( liSlotIndex, &lFriendEntry );

            if ( lbIsNewHighScore )
            {
                CGS_ASSERT( mpRoadRulesManager, CGS_ASSERT_MESSAGE_AT("mpRoadRulesManager", CGS_ARTIST_FILE, 3004) );

                if ( mpRoadRulesManager->GetCurrentRoadID() == KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSlotIndex] )
                {
                    GameStateModuleIO::RoadRulesUpdateTargetScoreAction lRecord;
                    GetChallengeUserScore( liSlotIndex, &lRecord.mUserScores, false );
                    GetChallengeFriendHighScore( liSlotIndex, &lRecord.mFriendScores, false );
                    lRecord.mRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSlotIndex];

                    CGS_ASSERT( lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 3016) );
                    CGS_ASSERT( lpOutput->GetGameActionQueue(), CGS_ASSERT_MESSAGE_AT("lpOutput->GetGameActionQueue()", CGS_ARTIST_FILE, 3017) );

                    GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
                    lpQueue->AddEvent( &lRecord,
                       GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE, sizeof( lRecord ) );
                }
            }
        }
    }
}

// @ 0x8234A5A8. A buddy was removed from the friends list. Scrub their recorded
// scores; if that changed the current road's records, re-post the road-score game
// action (type 280) so the GUI drops the buddy's entry. Always post the
// buddy-scrubbed flag action (type 55) afterwards.
void StreetManager::ProcessBuddyRemoved(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent )
{
    CGS_ASSERT( lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 4217) );
    CGS_ASSERT( lpOutput->GetGameActionQueue(), CGS_ASSERT_MESSAGE_AT("lpOutput->GetGameActionQueue()", CGS_ARTIST_FILE, 4218) );

    const bool lbScoresCleared = ClearAllChallengeDataForBuddy( &lpBuddyRemovedEvent->mRemovedBuddyName );

    CGS_ASSERT( mpRoadRulesManager, CGS_ASSERT_MESSAGE_AT("mpRoadRulesManager", CGS_ARTIST_FILE, 4225) );

    if ( lbScoresCleared )
    {
        const ::CgsID lCurrentRoadID = mpRoadRulesManager->GetCurrentRoadID();

        const s32 liRoadIndex = GetSaveGameChallengeIndexByRoadID( lCurrentRoadID );

        GameStateModuleIO::RoadRulesUpdateTargetScoreAction lRecord;
        GetChallengeUserScore( liRoadIndex, &lRecord.mUserScores, false );
        GetChallengeFriendHighScore( liRoadIndex, &lRecord.mFriendScores, false );
        lRecord.mRoadId = mpRoadRulesManager->GetCurrentRoadID();

        GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
        lpQueue->AddEvent( &lRecord,
                       GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE, sizeof( lRecord ) );
    }

    GameStateModuleIO::RequestAutoSaveAction lRequestAutoSave;
    lRequestAutoSave.mbForceAutosave = false;
    GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
    lpQueue->AddEvent( &lRequestAutoSave, GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE, sizeof( lRequestAutoSave ) );
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wB_07.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_07.cpp
//   (wave B partfile -- group 7 "server-sync")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::UpdateFriendHighScores           @ 0x82348C80
//
// Store-for-store against the frozen StreetManager layout header; every store /
// branch / early-out has an asm counterpart, and every offset the X360 folds
// (GetStreetData() == mpStreetData.operator->, the maNetworkChallengeData row
// stride, the save-game challenge-slot scan, the +0x1CC5 mbFirstDownload store)
// is mapped to its committed named member -- no raw `*(p+off)` casts.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"           // RoadRulesManager::GetCurrentRoadID
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"   // ChallengeHighScoreEntry Get/Set/ClearScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengeData::ContainsData, ScoreType, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                       // StreetData::GetRoad / Road::GetId
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"               // RoadRulesDownloadEvent / RoadRulesMessageData
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                  // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"             // BaseEventQueue<T>::GetLength/GetEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82348C80. Apply a batch of downloaded friend high-score records to the
    // network challenge table. For every record in the downloaded-events queue:
    // build a ChallengeHighScoreEntry from the record's names/scores, and -- for
    // roads that carry a save-game challenge slot -- merge it into the network
    // table row (per score type: adopt the downloaded score, else clear it). When
    // the merge produced a new high score AND the record is for the road the player
    // is currently on, re-post the road-score game action (type 280).
    // -----------------------------------------------------------------------
    void StreetManager::UpdateFriendHighScores( const void* lpDownloadedQueue,
                                                GameStateModuleIO::OutputBuffer* lpOutput )
    {
        CGS_ASSERT( lpOutput, "lpOutput" );

        // X360 builds an empty scratch player name up front (Construct("") then a
        // redundant macName[0] = 0). It is unreferenced past construction -- kept for
        // fidelity with the retail body.
        CgsNetwork::PlayerName lEmptyName;
        lEmptyName.Construct( "" );
        lEmptyName.macName[0] = '\0';

        const CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>* lpQueue =
            static_cast<const CgsModule::EventQueue<BrnNetwork::RoadRulesDownloadEvent, 40>*>( lpDownloadedQueue );

        for ( s32 liRecord = 0; liRecord < lpQueue->GetLength(); ++liRecord )
        {
            // 56-byte record copy (X360 copies the queue element to the stack before use).
            BrnNetwork::RoadRulesDownloadEvent lDownloadRecord = lpQueue->GetEvent( liRecord );

            const BrnStreetData::RoadIndex liRoadIndex = lDownloadRecord.mRoadRulesData.mChallengeIndex;
            const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liRoadIndex );   // carries its own bounds assert

            // Inlined save-game challenge-slot scan (the qword_82029FA0 walk): a road
            // that has no persisted challenge slot skips the merge but still runs the
            // last-slot bookkeeping below.
            bool lbRoadHasSaveSlot = false;
            const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
            while ( true )
            {
                if ( *lpSlotRoadId == lpRoad->GetId() )
                {
                    lbRoadHasSaveSlot = true;
                    break;
                }
                ++lpSlotRoadId;
                if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                {
                    break;
                }
            }

            if ( lbRoadHasSaveSlot )
            {
                BrnStreetData::ChallengeHighScoreEntry lDownloadedEntry =
                    CreateHighScoreEntryFromDownloadData( lDownloadRecord.maPlayerNames,
                                                          &lDownloadRecord.mRoadRulesData );

                // X360 re-materializes the road count AFTER building the downloaded entry
                // (0x82348E18-0x82348E28) and early-outs the whole merge, the (280,104)
                // post AND the last-slot bookkeeping below for an out-of-range index
                // (reachable via the continue-on-assert path of the GetRoad bounds check).
                if ( liRoadIndex >= mpStreetData->GetRoadCount() )
                {
                    continue;
                }

                const bool lbIsNewHighScore = CheckForNewHighScore( liRoadIndex, &lDownloadedEntry );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                      leScoreType++ )
                {
                    if ( lDownloadedEntry.ContainsData( leScoreType ) )
                    {
                        int32_t                liScore;
                        CgsNetwork::PlayerName lScoreName;
                        lDownloadedEntry.GetScore( leScoreType, &liScore, &lScoreName );
                        maNetworkChallengeData[liRoadIndex].SetScore( leScoreType, liScore, &lScoreName );
                    }
                    else
                    {
                        maNetworkChallengeData[liRoadIndex].ClearScore( leScoreType );
                    }
                }

                if ( lbIsNewHighScore )
                {
                    CGS_ASSERT( mpRoadRulesManager, CGS_ASSERT_MESSAGE_AT("mpRoadRulesManager", CGS_ARTIST_FILE, 2055) );

                    if ( mpRoadRulesManager->GetCurrentRoadID() == KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liRoadIndex] )
                    {
                        GameStateModuleIO::RoadRulesUpdateTargetScoreAction lAction;
                        GetChallengeUserScore( liRoadIndex, &lAction.mUserScores, false );
                        GetChallengeFriendHighScore( liRoadIndex, &lAction.mFriendScores, false );
                        lAction.mRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liRoadIndex];

                        CGS_ASSERT( lpOutput, "lpOutput" );
                        CGS_ASSERT( lpOutput->GetGameActionQueue() != NULL, CGS_ASSERT_MESSAGE_AT("lpOutput->GetGameActionQueue()", CGS_ARTIST_FILE, 2068) );

                        lpOutput->GetGuiOutputQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>( &lAction ),
                            GameStateModuleIO::E_ACTION_ROAD_RULES_UPDATE_TARGET_ROAD_SCORE, sizeof( lAction ) );
                    }
                }
            }

            // The last challenge slot having been reached clears the first-download flag.
            if ( liRoadIndex == 63 )
            {
                mbFirstDownload = false;
            }
        }
    }
}

// FOLDED FROM BrnGameStateStreetManager_wC_00.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_00.cpp
//   (wave C partfile -- group 0 "new road score")
//
// Faithful de-optimisation of BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::ProcessNewRoadScore  @ 0x823496C8
//   BrnGameState::StreetManager::UpdateBufferedHighScores (road-rules wave; the
//     second producer of the new-high-score record, so it shares this file)
//
// Every asm store / branch / early-out / assert has a counterpart here, and all
// state is reached through the frozen-header named members
// (BrnGameStateStreetManager.h) -- never a raw-offset cast:
//   * `mpStreetData->` == the asm's `addi rX, this, 0x1CC8` +
//     BrnStreetData::StreetData_::oper (ResourcePtr<StreetData>::operator->).
//   * `mpGameStateModule->GetDeveloperChallengeManager()` == the inlined
//     `mpGameStateModule + 185712` (0x2D570) embedded-subobject adjust that the
//     asm asserts non-null and then calls OnSetRoadRule on.
//   * `mpGameStateModule->GetActivePlayerCarId()` == the `ldx rX, pModule, 0x456D8`
//     64-bit car-id read feeding both SetCarID stores.
//   * `mpProgressionManager->GetProfile()` == pm+0x170; the +612 best-showtime
//     max-update is Get/SetNewHighShowtimeScore.
//   * `mpProgressionManager->GetAchievementManager()` == *(pm+133432).
//   * the two inlined bit pokes are BitArray<64u>::SetBit / ::UnSetBit on a stack
//     BitArray and on maRoadRulesChangedBitArrays[meActiveRoadRuleType]
//     (this+0x1CA8 + type*8); their CgsBitArray.h:222/:241 bounds asserts belong
//     to those methods and are not duplicated here.
//
// TRAPS kept faithful:
//   * the save-slot scan compares FULL 64-bit road CgsIDs against
//     KAA_SAVE_GAME_CHALLENGE_ROAD_IDS; a miss returns without touching anything.
//   * the two result flags come out of `cntlzw`-pair tests, i.e. exactly
//     `ChallengeData::CompareScores(...) < 0` -- reproduced branch for branch.
//   * ChallengeData::GetScore is re-read (not cached) for each comparison, as the
//     X360 code does.
//   * every Get/SetChallenge* call on this path passes the by-road-index mapping
//     flag TRUE.
//   * the assert message at line 2699 spells `lpOutputBuffer->...` even though the
//     frozen parameter is lpOutput -- kept VERBATIM.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"                                    // GetDeveloperChallengeManager / GetActivePlayerCarId
#include "GameSource/GameState/BrnGameStateModuleIO.h"                                  // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/BrnGameActions.h"                                        // RoadRulesNewHighScoreAction (281) / RoadRulesNewRulersAction (285)
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h"// DeveloperChallengeManager::OnSetRoadRule
#include "GameSource/GameState/Progression/BrnProgressionManager.h"                     // ProgressionManager accessors
#include "GameSource/GameState/Progression/BrnProfile.h"                                // Profile::Get/SetNewHighShowtimeScore
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnSetRoadRule
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"                 // ChallengeHighScoreEntry Construct/GetScore/IsWholeChallengeOwnedBySamePlayer
#include "SharedClasses/StreetData/BrnChallengeData.h"                                  // ChallengeData / ChallengePlayerScoreEntry / ChallengeParScoresEntry
#include "SharedClasses/StreetData/BrnStreetData.h"                                     // StreetData::GetRoad / GetChallengeParScore, Road::GetId
#include "GameSource/GameState/BrnCgsPlayerName.h"                                      // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                              // CgsContainers::BitArray<64u>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                        // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <cstddef>   // offsetof

namespace
{
    // The 48-byte record the X360 builds at var_160 and posts with
    // AddEvent(&record, 233, 48): the freshly-recorded score entry, the road's
    // save-game challenge slot, then a zero byte. (X360 stores: Copy @+0,
    // stw slot @+0x28, stb 0 @+0x2C.)
    // FLAG: the field identifiers are ours -- no DWARF name survives for this
    // GUI payload; the offsets/sizes are byte-exact from the image.
    struct NewRoadScoreRecord
    {
        BrnStreetData::ChallengePlayerScoreEntry mScoreEntry;           // +0x00 (40B)
        s32                                      miSaveGameSlotIndex;   // +0x28
        u8                                       mu8Flag;               // +0x2C
    };
    CGS_STATIC_ASSERT(( sizeof(NewRoadScoreRecord) == 48), "new-road-score record is 0x30" );
}

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// @ 0x823496C8. Apply a freshly-scored local challenge entry to a road's slot.
//
// Resolves the road's persisted save-game challenge slot (a road with no slot
// early-outs), pulls the road's user / par / friend records, notifies the
// developer-challenge manager, keeps the profile's best show-time score, ranks the
// new score against the stored user score, par and the friend high score, and posts
// the resulting GUI actions (55/233/281/285/149). A genuine new high score also
// runs the trophy / achievement fan-out.
// ----------------------------------------------------------------------------
void StreetManager::ProcessNewRoadScore( GameStateModuleIO::OutputBuffer* lpOutput,
                                         BrnStreetData::ChallengePlayerScoreEntry lNewScoreEntry,
                                         BrnStreetData::ScoreType leScoreType,
                                         BrnStreetData::ChallengeIndex liChallengeIndex,
                                         bool lbCheckFriendScore )
{
    bool lbOnlineLossButOfflineWin = false;

    const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liChallengeIndex );

    BrnStreetData::ChallengeIndex liSaveGameSlotIndex = 0;
    const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
    while ( *lpSlotRoadId != lpRoad->GetId() )
    {
        ++lpSlotRoadId;
        ++liSaveGameSlotIndex;
        if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
        {
            return;
        }
    }

    if ( liSaveGameSlotIndex >= 0 )
    {
        BrnStreetData::ChallengePlayerScoreEntry lUserScore;
        GetChallengeUserScore( liChallengeIndex, &lUserScore, true );

        BrnStreetData::ChallengeParScoresEntry lParScores;
        lParScores.Copy( mpStreetData->GetChallengeParScore( liChallengeIndex ) );

        BrnStreetData::ChallengeHighScoreEntry lFriendHighScore;
        GetChallengeFriendHighScore( liChallengeIndex, &lFriendHighScore, true );

        BrnStreetData::ChallengePlayerScoreEntry lNewEntry;
        lNewEntry.Construct();

        bool lbNewHighScore = false;

        if ( lNewScoreEntry.ContainsData( leScoreType ) )
        {
            const s32 liNewScore = lNewScoreEntry.GetScore( leScoreType );

            bool lbUserScoreBeatsPar = false;

            CGS_ASSERT( mpGameStateModule, CGS_ASSERT_MESSAGE_AT("mpGameStateModule", CGS_ARTIST_FILE, 2614) );
            CGS_ASSERT( mpGameStateModule->GetDeveloperChallengeManager(),
                        CGS_ASSERT_MESSAGE_AT("mpGameStateModule->GetDeveloperChallengeManager()", CGS_ARTIST_FILE, 2615) );

            // The full 64-bit road id, as the console passes it.
            mpGameStateModule->GetDeveloperChallengeManager()->OnSetRoadRule(
                leScoreType,
                liNewScore,
                mpStreetData->GetRoad( liChallengeIndex )->GetId() );

            if ( leScoreType == BrnStreetData::E_SCORE_TYPE_CRASH )
            {
                BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
                CGS_ASSERT( lpProfile != NULL, CGS_ASSERT_MESSAGE_AT("lpProfile != NULL", CGS_ARTIST_FILE, 2621) );

                if ( liNewScore > lpProfile->GetNewHighShowtimeScore() )
                {
                    lpProfile->SetNewHighShowtimeScore( liNewScore );
                }
            }

            s32     liParScore;
            ::CgsID lParRivalId;
            lParScores.GetScore( leScoreType, &liParScore, &lParRivalId );

            bool lbBeatsStoredUserScore;
            if ( lUserScore.ContainsData( leScoreType ) )
            {
                lbBeatsStoredUserScore =
                    lUserScore.CompareScores( leScoreType, liNewScore, lUserScore.GetScore( leScoreType ) ) < 0;
                lbUserScoreBeatsPar =
                    lParScores.CompareScores( leScoreType, lUserScore.GetScore( leScoreType ), liParScore ) < 0;
            }
            else
            {
                lbBeatsStoredUserScore = true;
            }

            if ( lbBeatsStoredUserScore )
            {
                lUserScore.SetScore( leScoreType, liNewScore );

                CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
                CGS_ASSERT( mpProgressionManager->GetProfile(), CGS_ASSERT_MESSAGE_AT("mpProgressionManager->GetProfile()", CGS_ARTIST_FILE, 2656) );

                const ::CgsID lActivePlayerCarId = mpGameStateModule->GetActivePlayerCarId();
                lUserScore.SetCarID( leScoreType, lActivePlayerCarId );

                if ( lParScores.CompareScores( leScoreType, liNewScore, liParScore ) < 0 )
                {
                    lNewEntry.SetScore( leScoreType, liNewScore );
                    lNewEntry.SetCarID( leScoreType, lActivePlayerCarId );

                    if ( lbCheckFriendScore && lFriendHighScore.ContainsData( leScoreType ) )
                    {
                        s32                    liFriendScore;
                        CgsNetwork::PlayerName lFriendHolderName;
                        lFriendHighScore.GetScore( leScoreType, &liFriendScore, &lFriendHolderName );

                        if ( lFriendHighScore.CompareScores( leScoreType, liNewScore, liFriendScore ) < 0 )
                        {
                            lbNewHighScore = true;
                        }
                        else
                        {
                            lbOnlineLossButOfflineWin = !lbUserScoreBeatsPar;
                        }
                    }
                    else
                    {
                        lbNewHighScore = !lbUserScoreBeatsPar;
                    }

                    const u8 lu8ScoreRecorded = 0;
                    lpOutput->GetGuiOutputQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>( &lu8ScoreRecorded ), 55, 1 );
                }
            }
        }

        CGS_ASSERT( lpOutput->GetGameActionQueue(), CGS_ASSERT_MESSAGE_AT("lpOutputBuffer->GetGameActionQueue()", CGS_ARTIST_FILE, 2699) );

        if ( lNewEntry.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
        {
            NewRoadScoreRecord lRecord;
            lRecord.mScoreEntry.Copy( &lNewEntry );
            lRecord.miSaveGameSlotIndex = liSaveGameSlotIndex;
            lRecord.mu8Flag             = 0;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lRecord ), 233, 48 );
        }

        if ( lbNewHighScore || lbOnlineLossButOfflineWin )
        {
            BrnStreetData::ChallengeHighScoreEntry lHighScore;
            lHighScore.Construct();
            GetHighScoreEntry( liChallengeIndex, &lHighScore, true );

            GameStateModuleIO::RoadRulesNewHighScoreAction lNotification;
            lNotification.mPlayerName.Construct( "" );
            lNotification.mRoadId                   = mpStreetData->GetRoad( liChallengeIndex )->GetId();
            lNotification.mbIsLocalPlayer           = true;
            lNotification.meScoreType               = leScoreType;
            lNotification.mbIsWholeRoadOwned        = lHighScore.IsWholeChallengeOwnedBySamePlayer();
            lNotification.mbWasRulePlayersBefore    = false;
            lNotification.mbMultipleScores          = false;
            lNotification.miNumScoresLost           = 0;
            lNotification.miNumRoadsNowRuled        = GetNumberOfRoadsRulesByLocalPlayer();
            lNotification.mbOnlineLossButOfflineWin = lbOnlineLossButOfflineWin;

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lNotification ),
                GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_HIGH_SCORE, sizeof( lNotification ) );

            if ( leScoreType == meActiveRoadRuleType )
            {
                GameStateModuleIO::RoadRulesNewRulersAction lChangedRoads;
                lChangedRoads.mRoadRulesChangedBitArray.UnSetAll();
                lChangedRoads.mRoadRulesChangedBitArray.SetBit( liSaveGameSlotIndex );

                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lChangedRoads ),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_RULERS, sizeof( lChangedRoads ) );

                maRoadRulesChangedBitArrays[meActiveRoadRuleType].UnSetBit( liSaveGameSlotIndex );
            }

            mNewHighScoreBuffer.EraseMatchingEntries( lNotification.mRoadId, leScoreType );
        }

        SetChallengeUserScore( liChallengeIndex, &lUserScore, true );

        if ( lbNewHighScore )
        {
            UpdateTrophyUnlockOnRoadRuleWin( leScoreType );

            CGS_ASSERT( mpProgressionManager, "mpProgressionManager" );
            CGS_ASSERT( mpProgressionManager->GetAchievementManager(),
                        CGS_ASSERT_MESSAGE_AT("mpProgressionManager->GetAchievementManager()", CGS_ARTIST_FILE, 2762) );

            mpProgressionManager->GetAchievementManager()->OnSetRoadRule(
                leScoreType, mpStreetData->GetRoad( liChallengeIndex )->GetId() );

            s32 liTrophyCode;
            if ( HasPlayerBeatenParScore( liChallengeIndex, BrnStreetData::E_SCORE_TYPE_TIME )
                     == E_ROAD_RULE_COMPLETION_STATUS_BEATEN
                 && HasPlayerBeatenParScore( liChallengeIndex, BrnStreetData::E_SCORE_TYPE_CRASH )
                     == E_ROAD_RULE_COMPLETION_STATUS_BEATEN )
            {
                liTrophyCode = 31;
            }
            else
            {
                liTrophyCode = 29 + ( ( leScoreType != BrnStreetData::E_SCORE_TYPE_TIME ) ? 1 : 0 );
            }

            lpOutput->GetGuiOutputQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &liTrophyCode ), 149, 4 );

            mpProgressionManager->CheckForSpecialCarUnlocks();
            mpProgressionManager->SendGameCompletionResults( lpOutput->GetGuiOutputQueue() );
        }
    }
}

// ----------------------------------------------------------------------------
// Drains the new-high-score buffer one entry per KF_TIME_BETWEEN_HIGH_SCORE_UNBUFFERS
// seconds while nothing is paused and a road rule is active. When more scores arrived
// than the buffer holds, one summary record (with the lost-score count) goes out instead
// and the whole buffer is dropped. Either way the roads whose rulers changed for the
// active score type are then published (game action 285) and their bits cleared.
// ----------------------------------------------------------------------------
void StreetManager::UpdateBufferedHighScores( f32 lfSimTimeStep,
                                              bool lbIsAnythingPaused,
                                              GameStateModuleIO::OutputBuffer* lpOutput )
{
    CGS_ASSERT( lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 2311) );

    if ( lbIsAnythingPaused )
    {
        return;
    }

    if ( mNewHighScoreBuffer.GetLength() > 0 && meActiveRoadRuleType != BrnStreetData::E_SCORE_TYPE_COUNT )
    {
        mfUnbufferTimer += lfSimTimeStep;

        // The console tests `timer < limit` and carries on for NaN.
        if ( !( mfUnbufferTimer < KF_TIME_BETWEEN_HIGH_SCORE_UNBUFFERS ) )
        {
            GameStateModuleIO::RoadRulesNewHighScoreAction lNewHighScore;

            if ( mbTooManyHighScoresToBuffer )
            {
                lNewHighScore.miNumScoresLost    = miNumScoresLost;
                lNewHighScore.miNumRoadsNowRuled = GetNumberOfRoadsRulesByLocalPlayer();
                lNewHighScore.mbMultipleScores   = true;

                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lNewHighScore ),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_HIGH_SCORE, sizeof( lNewHighScore ) );

                mfUnbufferTimer = 0.0f;
                mNewHighScoreBuffer.Clear();
                mbTooManyHighScoresToBuffer = false;
                miNumScoresLost             = 0;
            }
            else
            {
                mfUnbufferTimer = 0.0f;

                lNewHighScore.mPlayerName             = mNewHighScoreBuffer[0].mPlayerName;
                lNewHighScore.mRoadId                 = mNewHighScoreBuffer[0].mRoadID;
                lNewHighScore.meScoreType             = mNewHighScoreBuffer[0].meScoreType;
                lNewHighScore.mbIsWholeRoadOwned      = mNewHighScoreBuffer[0].mbIsRoadWhollyOwnedByOnePlayer;
                lNewHighScore.mbIsLocalPlayer         = false;
                lNewHighScore.mbMultipleScores        = false;
                lNewHighScore.miNumScoresLost         = 0;
                lNewHighScore.mbWasRulePlayersBefore  = mNewHighScoreBuffer[0].mbWasRoadRuledByPlayerBefore;
                lNewHighScore.miNumRoadsNowRuled      = GetNumberOfRoadsRulesByLocalPlayer();
                lNewHighScore.mbOnlineLossButOfflineWin = false;

                mNewHighScoreBuffer.Erase( 0 );

                CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );
                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lNewHighScore ),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_HIGH_SCORE, sizeof( lNewHighScore ) );
            }

            if ( !maRoadRulesChangedBitArrays[meActiveRoadRuleType].IsZero() )
            {
                GameStateModuleIO::RoadRulesNewRulersAction lNewRulers;
                lNewRulers.mRoadRulesChangedBitArray.UnSetAll();
                lNewRulers.mRoadRulesChangedBitArray.ORArrays( &lNewRulers.mRoadRulesChangedBitArray,
                                                               &maRoadRulesChangedBitArrays[meActiveRoadRuleType] );

                CGS_ASSERT( lpOutput->GetGameActionQueue(), "lpOutput->GetGameActionQueue()" );
                lpOutput->GetGuiOutputQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lNewRulers ),
                    GameStateModuleIO::E_ACTION_ROAD_RULES_NEW_RULERS, sizeof( lNewRulers ) );

                maRoadRulesChangedBitArrays[meActiveRoadRuleType].UnSetAll();
            }
        }
    }
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wC_02.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_02.cpp
//   (wave C partfile -- group 2)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::ProcessScoreRequestEvent  @ 0x8234A240
//   (BrnGameState::StreetManager::SetupParRivals @0x8233F560 was split out 2026-08-11 into
//    the sibling BrnGameStateStreetManager_SetupParRivals.cpp -- see the note at its former
//    place below. Its two vector immediates and its LCG seed went with it.)
//
// Every store / branch / early-out / assert has an asm counterpart; members are
// the frozen-header named fields (never raw-offset casts).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                        // GameStateModuleIO::RoadRulesScoreRequestEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer (GetGameActionQueue / GetGuiOutputQueue)
#include "GameSource/GameState/BrnGameActions.h"                       // RoadRulesRoadScoreAction (action 284)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"       // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"           // CgsDev::StrStream (streamed dev assert)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <string.h>

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp).
// Committed precedent: GameSource/Game/X360/BrnSystemHWX360.cpp:11.
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

namespace BrnGameState
{

// ---------------------------------------------------------------------------
// @ 0x8233F560. SetupParRivals WAS DEFINED HERE; it was SPLIT OUT 2026-08-11 into the sibling
// BrnGameStateStreetManager_SetupParRivals.cpp so that GameStateModule::Prepare2 case 2 could
// stop parking on it. MEASURED (cl /c + dumpbin /SYMBOLS vs the defined-symbol set of
// build\game\obj): mounting this whole partfile costs THIRTEEN unresolved externals, and every
// one of them belongs to ProcessScoreRequestEvent below (the score-entry family, PlayerName,
// SPrintf and the StrStream dev-assert chain). SetupParRivals touches none of them, so the split
// costs ZERO. Fold the split file back in when the score-entry family lands. Do NOT re-add the
// body: two definitions is LNK2005.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// @ 0x8234A240. Answers a GUI road-score request: formats the road's user and
// high scores against the compiled par score (falling back to the par rival's
// numeric id as the displayed name) and posts the 48-byte response (type 284).
// EVERY score access uses meActiveRoadRuleType -- never a parameter.
// ---------------------------------------------------------------------------
void StreetManager::ProcessScoreRequestEvent(
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::RoadRulesScoreRequestEvent* lpScoreRequestEvent )
{
    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_COUNT )
    {
        return;
    }

    CGS_ASSERT( lpOutput, CGS_ASSERT_MESSAGE_AT("lpOutput", CGS_ARTIST_FILE, 3128) );
    CGS_ASSERT( lpScoreRequestEvent, CGS_ASSERT_MESSAGE_AT("lpRequestEvent", CGS_ARTIST_FILE, 3129) );

    GameStateModuleIO::RoadRulesRoadScoreAction lResponse;
    lResponse.mPlayerName.Construct( "" );

    const BrnStreetData::ChallengeIndex liChallengeIndex = lpScoreRequestEvent->mRoadChallengeIndex;

    lResponse.mChallengeIndex        = liChallengeIndex;
    lResponse.mbAIRulesRoadOffline   = false;
    lResponse.mbAIRulesRoadOnline    = false;
    lResponse.mbLocalPlayerRulesRoad = false;
    lResponse.mRoadID               = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liChallengeIndex];

    if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_TIME )
    {
        lResponse.meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_TIME;
    }
    else if ( meActiveRoadRuleType == BrnStreetData::E_SCORE_TYPE_CRASH )
    {
        lResponse.meActiveRoadRule = E_ACTIVE_ROAD_RULE_OFFLINE_CRASH;
    }
    else
    {
        char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
        CgsDev::Assert::BeginAssert();
        CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
        lStrStream << "Unhandled road rule type ";
        lStrStream << static_cast<s32>( meActiveRoadRuleType );
        lStrStream << " in StreetManager::ProcessScoreRequestEvent\n";
        // 0x8234A34C-58: r4 = the recovered path literal, r5 = 0xC54 (line 3156). Passed
        // verbatim (CgsID.cpp:80 precedent) so the recovered X360 site is not discarded.
        CgsDev::Assert::FireAssert(
            lacMessage,
            CGS_ASSERT_LOCATION(CGS_ARTIST_FILE,
            3156) );
        CgsDev::Assert::EndAssert();
    }

    BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
    GetHighScoreEntry( liChallengeIndex, &lHighScoreEntry, false );

    BrnStreetData::ChallengePlayerScoreEntry lUserScoreEntry;
    GetChallengeUserScore( liChallengeIndex, &lUserScoreEntry, false );

    BrnStreetData::ChallengeParScoresEntry lParScoresEntry;
    lParScoresEntry.Copy( mpStreetData->GetChallengeParScore( liChallengeIndex ) );

    s32     liParScore   = 0;
    ::CgsID lParRivalId  = 0;
    lParScoresEntry.GetScore( meActiveRoadRuleType, &liParScore, &lParRivalId );

    // ---- user score ------------------------------------------------------
    bool lbUserScoreBeatsPar = false;
    s32  liUserScore         = 0;
    if ( lUserScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        liUserScore = lUserScoreEntry.GetScore( meActiveRoadRuleType );
        lbUserScoreBeatsPar =
            ( lUserScoreEntry.CompareScores( meActiveRoadRuleType, liUserScore, liParScore ) < 0 );
    }

    if ( lbUserScoreBeatsPar )
    {
        lResponse.miOfflineScore = liUserScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miOfflineScore       = liParScore;
        lResponse.mbAIRulesRoadOffline = true;
    }

    // ---- high score ------------------------------------------------------
    CgsNetwork::PlayerName lHighScoreHolderName;
    s32  liHighScore          = 0;
    bool lbHighScoreBeatsPar  = false;
    if ( lHighScoreEntry.ContainsData( meActiveRoadRuleType ) )
    {
        lHighScoreEntry.GetScore( meActiveRoadRuleType, &liHighScore, &lHighScoreHolderName );
        lbHighScoreBeatsPar =
            ( lHighScoreEntry.CompareScores( meActiveRoadRuleType, liHighScore, liParScore ) < 0 );
    }

    if ( lbHighScoreBeatsPar )
    {
        if ( strnicmp( lHighScoreHolderName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, 16 ) != 0 )
        {
            lResponse.mPlayerName.Construct( lHighScoreHolderName.macName );
        }
        else
        {
            lResponse.mbLocalPlayerRulesRoad = true;
        }
        lResponse.miOnlineScore = liHighScore;
    }
    else
    {
        char lacRivalName[13];
        CgsCore::SPrintf( lacRivalName, 12, "%llu", GetParRivalId( liChallengeIndex, meActiveRoadRuleType ) );
        lacRivalName[12] = 0;
        lResponse.mPlayerName.Construct( lacRivalName );

        lResponse.miOnlineScore       = liParScore;
        lResponse.mbAIRulesRoadOnline = true;
    }

    CGS_ASSERT( lpOutput->GetGameActionQueue(), CGS_ASSERT_MESSAGE_AT("lpOutput->GetGameActionQueue()", CGS_ARTIST_FILE, 3272) );

    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lResponse ),
                       GameStateModuleIO::E_ACTION_ROAD_RULES_ROAD_SCORE, sizeof( lResponse ) );
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wC_03.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_03.cpp
//   (wave C partfile -- group 3 "profile / server score sync")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::OnProfileLoaded                  @ 0x82349E20
//   BrnGameState::StreetManager::ProcessConnectedOnlineEvent      @ 0x8234A148
//   BrnGameState::StreetManager::UpdateUserScoresFromServerRecords@ 0x82348FC0
//
// Store-for-store against the frozen StreetManager layout header: every store,
// branch, early-out and assert has an asm counterpart and every offset the X360
// folds is spelled through a committed named accessor -- the profile road-rules
// accessors (Profile::GetNetworkChallengeData / GetUserChallengeData /
// GetNetworkChallengeDownloadTimestamp / Get/SetLastRoadRulesResetTime /
// GetRoadRulesID), StreetData::GetRoad (which owns its own bounds assert) and
// the OutputBuffer queue accessors. No raw `*(p + off)` casts.
//
// All three handlers publish the same 16-byte road-rules score-summary game
// action (type 232) onto the GUI output queue; OnProfileLoaded and
// ProcessConnectedOnlineEvent stamp the profile's download timestamp into it,
// UpdateUserScoresFromServerRecords posts a literal 0 there.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameEvents.h"                    // OnlineRoadRulesConnectInfoEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // OutputBuffer::GetGameActionQueue / GetGuiOutputQueue
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// ProgressionManager::GetProfile / CheckForSpecialCarUnlocks / SendGameCompletionResults
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile road-rules accessors
#include "SharedClasses/StreetData/BrnChallengeData.h"             // ChallengeData::ContainsData / GetScore / SetScore / SetClean / CompareScores, ScoreType, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                // StreetData::GetRoad / Road::GetId
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"        // BrnNetwork::RoadRulesMessageData
#include "GameShared/GameClasses/Module/CgsEventQueue.h"           // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"       // BaseEventQueue<T>::GetLength / GetEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<13312,16>::AddEvent / CgsModule::Event
#include "GameShared/GameClasses/Core/CgsID.h"                     // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"

#include <cstring>   // memcpy

namespace BrnGameState
{

    // -----------------------------------------------------------------------
    // @ 0x82349E20. The profile finished loading: adopt its persisted road-rules
    // tables wholesale (network high scores first, then the local player's
    // scores) and publish the score summary so the GUI can rebuild its lists.
    // -----------------------------------------------------------------------
    void StreetManager::OnProfileLoaded( GameStateModuleIO::OutputBuffer* lpOutput )
    {
        CGS_ASSERT( mpProgressionManager, CGS_ASSERT_MESSAGE_AT("mpProgressionManager", CGS_ARTIST_FILE, 2938) );
        CGS_ASSERT( mpProgressionManager->GetProfile(), CGS_ASSERT_MESSAGE_AT("mpProgressionManager->GetProfile()", CGS_ARTIST_FILE, 2939) );

        BrnProgression::ProgressionManager* lpProgressionManager = mpProgressionManager;
        memcpy( maNetworkChallengeData, lpProgressionManager->GetProfile()->GetNetworkChallengeData( 0 ), sizeof( maNetworkChallengeData ) );
        const BrnStreetData::ChallengePlayerScoreEntry* lpUserScores = lpProgressionManager->GetProfile()->GetUserChallengeData( 0 );
        memcpy( maChallengeData, lpUserScores, sizeof( maChallengeData ) );

        GameStateModuleIO::RoadRulesChallengeScoresAction lUserChallengeScoresAction;
        lUserChallengeScoresAction.Construct(
            mpProgressionManager->GetProfile()->GetRoadRulesID(),
            mpProgressionManager->GetProfile()->GetNetworkChallengeDownloadTimestamp(),
            this );

        GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
        lpQueue->AddEvent( &lUserChallengeScoresAction,
            GameStateModuleIO::E_ACTION_ROAD_RULES_CHALLENGE_SCORES, sizeof( lUserChallengeScoresAction ) );
    }

    // -----------------------------------------------------------------------
    // @ 0x8234A148. Connected to the road-rules service: when the server reports
    // a different reset time than the profile last saw, every stored challenge
    // record is stale -- wipe them, publish the (now empty) score summary, and
    // only then stamp the new reset time into the profile.
    // -----------------------------------------------------------------------
    void StreetManager::ProcessConnectedOnlineEvent(
            GameStateModuleIO::OutputBuffer* lpOutput,
            const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpConnectInfoEvent )
    {
        CGS_ASSERT( mpProgressionManager, CGS_ASSERT_MESSAGE_AT("mpProgressionManager", CGS_ARTIST_FILE, 3084) );
        CGS_ASSERT( mpProgressionManager->GetProfile(), CGS_ASSERT_MESSAGE_AT("mpProgressionManager->GetProfile()", CGS_ARTIST_FILE, 3085) );

        if ( mpProgressionManager->GetProfile()->GetLastRoadRulesResetTime()
             != lpConnectInfoEvent->muLastRoadRulesResetTime )
        {
            ClearAllChallengeData();

            GameStateModuleIO::RoadRulesChallengeScoresAction lUserChallengeScoresAction;
            lUserChallengeScoresAction.Construct( mpProgressionManager->GetProfile()->GetRoadRulesID(),
                mpProgressionManager->GetProfile()->GetNetworkChallengeDownloadTimestamp(), this );

            GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
            lpQueue->AddEvent( &lUserChallengeScoresAction,
                GameStateModuleIO::E_ACTION_ROAD_RULES_CHALLENGE_SCORES, sizeof( lUserChallengeScoresAction ) );

            mpProgressionManager->GetProfile()->SetLastRoadRulesResetTime(
                lpConnectInfoEvent->muLastRoadRulesResetTime );
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x82348FC0. Merge the road-rules scores the server sent back for THIS
    // profile into the local player's challenge table. Records stamped with a
    // different road-rules session id belong to another profile: they are
    // dropped and raise the "foreign records" notification at the end.
    //
    // Per record the server score wins when the local table has nothing for that
    // score type, or when the server score ranks better; the adopted score is
    // credited to the "PUSMC01" car and marked clean (it is already on the
    // server, so it must not be re-uploaded).
    // -----------------------------------------------------------------------
    void StreetManager::UpdateUserScoresFromServerRecords( const void* lpLocalDownloadedQueue,
                                                           GameStateModuleIO::OutputBuffer* lpOutput )
    {
        bool lbDroppedForeignRecords = false;

        const CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>* lpQueue =
            static_cast<const CgsModule::EventQueue<BrnNetwork::RoadRulesMessageData, 40>*>( lpLocalDownloadedQueue );

        if ( lpQueue->GetLength() > 0 )
        {
            CGS_ASSERT( mpProgressionManager, CGS_ASSERT_MESSAGE_AT("mpProgressionManager", CGS_ARTIST_FILE, 2198) );
            CGS_ASSERT( mpProgressionManager->GetProfile(), CGS_ASSERT_MESSAGE_AT("mpProgressionManager->GetProfile()", CGS_ARTIST_FILE, 2199) );

            for ( s32 liRecord = 0; liRecord < lpQueue->GetLength(); ++liRecord )
            {
                // 24-byte record copy (the X360 copies the queue element to the
                // stack before touching it); GetEvent carries its own asserts.
                BrnNetwork::RoadRulesMessageData lRecord = lpQueue->GetEvent( liRecord );

                if ( lRecord.mu64RoadRulesID != mpProgressionManager->GetProfile()->GetRoadRulesID() )
                {
                    lbDroppedForeignRecords = true;
                    continue;
                }

                const ::CgsID lPusmc01Car = CgsIDCompress( "PUSMC01" );

                const BrnStreetData::ChallengeIndex liRoadIndex = lRecord.mChallengeIndex;
                const ::CgsID lRoadId = mpStreetData->GetRoad( liRoadIndex )->GetId();   // GetRoad owns its bounds assert

                // Inlined GetSaveGameChallengeIndexByRoadID: the save-game slot
                // scan is a pure gate here -- a road with no persisted challenge
                // slot is skipped, the slot value itself is never used.
                s32 liSaveGameSlotIndex = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSaveGameSlotIndex;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSaveGameSlotIndex = -1;
                        break;
                    }
                }
                if ( liSaveGameSlotIndex < 0 )
                {
                    continue;
                }

                BrnStreetData::ChallengePlayerScoreEntry lServerEntry =
                    CreateUserChallengeScoreFromRoadRulesData( &lRecord );

                BrnStreetData::ChallengePlayerScoreEntry lLocalEntry;
                lLocalEntry.Construct();
                GetChallengeUserScore( liRoadIndex, &lLocalEntry, false );

                for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                      leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                      leScoreType++ )
                {
                    if ( lServerEntry.ContainsData( leScoreType ) )
                    {
                        const int32_t liServerScore = lServerEntry.GetScore( leScoreType );

                        if ( !lLocalEntry.ContainsData( leScoreType )
                             || lServerEntry.CompareScores( leScoreType,
                                                            liServerScore,
                                                            lLocalEntry.GetScore( leScoreType ) ) < 0 )
                        {
                            lLocalEntry.SetScore( leScoreType, liServerScore );
                            lLocalEntry.SetCarID( leScoreType, lPusmc01Car );
                            lLocalEntry.SetClean( leScoreType );
                        }
                    }
                    else if ( lLocalEntry.ContainsData( leScoreType ) )
                    {
                        // Server has nothing for this type: re-store the local score.
                        lLocalEntry.SetScore( leScoreType, lLocalEntry.GetScore( leScoreType ) );
                    }
                }

                SetChallengeUserScore( liRoadIndex, &lLocalEntry, false );
            }

            GameStateModuleIO::RoadRulesChallengeScoresAction lUserChallengeScoresAction;
            lUserChallengeScoresAction.Construct( mpProgressionManager->GetProfile()->GetRoadRulesID(),
                0, this );

            GameStateModuleIO::GameActionQueue* lpQueue = lpOutput->GetGameActionQueue();
            lpQueue->AddEvent( &lUserChallengeScoresAction,
                GameStateModuleIO::E_ACTION_ROAD_RULES_CHALLENGE_SCORES, sizeof( lUserChallengeScoresAction ) );

            if ( lbDroppedForeignRecords )
            {
                // FLAG: the X360 posts a stack byte it never writes (the type-234
                // notification carries no payload); zero-initialised here so the
                // reconstruction has no indeterminate read.
                u8 lu8ForeignRecordsNotification = 0;
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>( &lu8ForeignRecordsNotification ), 234, 1 );
            }

            UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_TIME );
            UpdateTrophyUnlockOnRoadRuleWin( BrnStreetData::E_SCORE_TYPE_CRASH );

            mpProgressionManager->CheckForSpecialCarUnlocks();
            mpProgressionManager->SendGameCompletionResults( lpOutput->GetGuiOutputQueue() );
        }
    }
}

// FOLDED FROM BrnGameStateStreetManager_wR_01.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_01.cpp
//   (road-rules wave partfile -- the per-frame pump and its score helpers)
//
//   BrnGameState::StreetManager::Update
//   BrnGameState::StreetManager::UpdateRoadRulesProfileScores
//   BrnGameState::StreetManager::GetHighScoreEntry
//   BrnGameState::StreetManager::GetNumberOfRoadsRulesByLocalPlayer
//
// UpdateBufferedHighScores, which Update also calls, lives in _wC_00 beside
// ProcessNewRoadScore: both post the same new-high-score record.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                        // PreWorldInputBuffer::GetNetworkToGameStateInterface
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // NetworkToGameStateInterface queue accessors
#include "GameSource/GameState/Progression/BrnProgressionManager.h"           // SetRoadRuleNetworkHighScores / SetRoadRuleChallengeData
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"       // ChallengeHighScoreEntry Construct / SetScore / GetScore / UpdateEntry
#include "SharedClasses/StreetData/BrnChallengeData.h"                        // ChallengeData::ContainsData / GetScore, ScoreType operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                            // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"     // CgsDev::PerfMonCpu::StartMonitor / StopMonitor
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                            // CGS_ASSERT

#include <string.h>

// Case-insensitive bounded compare (console strnicmp -> MSVC _strnicmp), the same
// spelling as _wC_02.
#if defined(_MSC_VER)
#  define strnicmp _strnicmp
#endif

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// The per-frame pump. The debug component ticks first; then, unless the current
// mode disables the upcoming-road signs, the wrong-way timer and the upcoming-street
// walk (under the "UpcomingRoads" monitor); then the downloaded friend records and
// the server's copy of the local scores are merged, the new-high-score buffer is
// drained and the live tables are copied into the profile.
// ----------------------------------------------------------------------------
void StreetManager::Update( bool lbIsAnythingPaused,
                            f32 lfSimTimeStep,
                            const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                            GameStateModuleIO::OutputBuffer* lpOutput,
                            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
                            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
                            bool lbCurrentGameModeHasRoute,
                            bool lbCurrentGameModeDisablesUpcoming,
                            f32 lfTimeGoingTheWrongWay )
{
    mStreetManagerDebugComponent.Update( lpOutput );

    CGS_ASSERT( lpInput, CGS_ASSERT_MESSAGE_AT("lpInput", CGS_ARTIST_FILE, 466) );
    CGS_ASSERT( lpInput->GetNetworkToGameStateInterface(), CGS_ASSERT_MESSAGE_AT("lpInput->GetNetworkToGameStateInterface()", CGS_ARTIST_FILE, 467) );

    if ( !lbCurrentGameModeDisablesUpcoming )
    {
        // A zero, negative or NaN wrong-way time resets the running total; the flag
        // raises once the total is no longer below ten seconds.
        if ( lfTimeGoingTheWrongWay > 0.0f )
        {
            mfTotalTimeGoinTheWrongWay += lfTimeGoingTheWrongWay;
            mbWrongWay = !( mfTotalTimeGoinTheWrongWay < 10.0f );
        }
        else
        {
            mfTotalTimeGoinTheWrongWay = 0.0f;
            mbWrongWay                 = false;
        }

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsPM );
        UpdateUpcomingStreets( lpActiveRaceCarInterface, lpLastAICarOutputInterface, lpOutput,
                               lfSimTimeStep, lbCurrentGameModeHasRoute );
        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsPM );
    }

    UpdateFriendHighScores( lpInput->GetNetworkToGameStateInterface()->GetRoadRulesDownloadedQueue(), lpOutput );
    UpdateUserScoresFromServerRecords( lpInput->GetNetworkToGameStateInterface()->GetLocalRoadRulesDownloadedQueue(), lpOutput );
    UpdateBufferedHighScores( lfSimTimeStep, lbIsAnythingPaused, lpOutput );
    UpdateRoadRulesProfileScores();
}

// ----------------------------------------------------------------------------
// Copies the live network and local score tables into the profile, each copy under
// its own monitor.
// ----------------------------------------------------------------------------
void StreetManager::UpdateRoadRulesProfileScores()
{
    CGS_ASSERT( mpProgressionManager,
        CGS_ASSERT_MESSAGE_AT(
            "mpProgressionManager",
            CGS_ARTIST_FILE, 2443 /* ARTIST 0x823174A8 */));

    CgsDev::PerfMonCpu::StartMonitor( miSetRoadRuleNetworkHighScoresPM );
    mpProgressionManager->SetRoadRuleNetworkHighScores( maNetworkChallengeData );
    CgsDev::PerfMonCpu::StopMonitor( miSetRoadRuleNetworkHighScoresPM );

    CgsDev::PerfMonCpu::StartMonitor( miSetRoadRuleChallengeDataPM );
    mpProgressionManager->SetRoadRuleChallengeData( maChallengeData );
    CgsDev::PerfMonCpu::StopMonitor( miSetRoadRuleChallengeDataPM );
}

// ----------------------------------------------------------------------------
// The road's best score per type across the friend table and the local player: the
// friend record, updated with an entry holding the local player's scores under the
// local-player name.
// ----------------------------------------------------------------------------
void StreetManager::GetHighScoreEntry( BrnStreetData::ChallengeIndex liIndex,
                                       BrnStreetData::ChallengeHighScoreEntry* lpEntry,
                                       bool lbByRoadIndex )
{
    GetChallengeFriendHighScore( liIndex, lpEntry, lbByRoadIndex );

    BrnStreetData::ChallengeHighScoreEntry lLocalPlayerEntry;
    lLocalPlayerEntry.Construct();

    CgsNetwork::PlayerName lLocalPlayerName;
    lLocalPlayerName.Construct( KAC_LOCAL_PLAYER_NAME_TEXT );

    BrnStreetData::ChallengePlayerScoreEntry lUserScore;
    GetChallengeUserScore( liIndex, &lUserScore, lbByRoadIndex );

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lUserScore.ContainsData( leScoreType ) )
        {
            lLocalPlayerEntry.SetScore( leScoreType, lUserScore.GetScore( leScoreType ), &lLocalPlayerName );
        }
    }

    lpEntry->UpdateEntry( &lLocalPlayerEntry, 0 );
}

// ----------------------------------------------------------------------------
// Counts every (road, score type) whose high score is held by the local player.
// ----------------------------------------------------------------------------
s32 StreetManager::GetNumberOfRoadsRulesByLocalPlayer()
{
    s32 liNumberOfRoadsRuled = 0;

    for ( BrnStreetData::ChallengeIndex liIndex = 0; liIndex < KI_MAX_CHALLENGES; ++liIndex )
    {
        BrnStreetData::ChallengeHighScoreEntry lHighScoreEntry;
        GetHighScoreEntry( liIndex, &lHighScoreEntry, false );

        for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
              leScoreType++ )
        {
            if ( lHighScoreEntry.ContainsData( leScoreType ) )
            {
                int32_t                liScore;
                CgsNetwork::PlayerName lHolderName;
                lHighScoreEntry.GetScore( leScoreType, &liScore, &lHolderName );

                if ( strnicmp( lHolderName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, 16 ) == 0 )
                {
                    ++liNumberOfRoadsRuled;
                }
            }
        }
    }

    return liNumberOfRoadsRuled;
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wR_10.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_10.cpp
//   (road-rules wave partfile -- the friend high-score table)
//
//   StreetManager::CheckForNewHighScore
//   StreetManager::ClearAllChallengeDataForBuddy
//   StreetManager::SetChallengeFriendHighScore
//
// All three work on maNetworkChallengeData, the per-road friend high-score
// table (64 ChallengeHighScoreEntry records indexed by save-game challenge slot).
//
// Container asserts: the console inlines FastBitArray<2>::SetBit / IsBitSet,
// BitArray<64u>::SetBit and the Array<BufferedNewHighScore,5> count check at
// each call site, together with their streamed "Index N is out of range" /
// "Index: N, Number of bits: 64" / "Array used before Construct/Clear was
// called" assert blocks. Those belong to the container methods, which are
// called here by name instead of being re-expanded (the ChallengeManager
// convention). The per-pass "leEnumIndex <= E_SCORE_TYPE_COUNT" assert is the
// BrnStreetData::operator++ guard, reached through the committed operator.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"   // ChallengeHighScoreEntry GetScore/UpdateEntry/ClearScore/Copy/IsWholeChallengeOwnedBySamePlayer
#include "SharedClasses/StreetData/BrnChallengeData.h"                     // ScoreType, ContainsData, operator++
#include "SharedClasses/StreetData/BrnStreetData.h"                        // StreetData::GetRoad, Road::GetId
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"            // RoadRulesManager::GetCurrentRoadID
#include "GameSource/GameState/BrnCgsPlayerName.h"                         // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"             // CgsContainers::FastBitArray<2>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<64u>::SetBit
#include "GameShared/GameClasses/Network/CgsNetworkUtils.h"                // CgsNetwork::UsernameCompare
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"                         // CGS_ASSERT

#include <string.h>   // _strnicmp (the console's strnicmp)

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Merge a friend's downloaded score record into the stored friend high score
// for one save-game challenge slot, and queue every score type it beats.
//
// First records which score types of the current high score belong to the
// local player (owner name == KAC_LOCAL_PLAYER_NAME_TEXT, case-insensitive,
// 16 chars). Then merges lpNewScore into a local copy of the current entry;
// for every score type the merge took, a BufferedNewHighScore is built (owner
// name, the slot's road id, score type, whole-road ownership, and whether the
// local player held that score before), any stale buffered entry for the same
// road + score type is dropped, and the new one is appended -- or, when the
// buffer is full, the overflow flag is raised and a lost local-player score is
// counted (only once the first download is over). The slot's bit in that score
// type's road-rules-changed array is set either way.
//
// Returns true when at least one score type was beaten. The merged entry is a
// local: the callers write the friend table themselves.
// ----------------------------------------------------------------------------
bool StreetManager::CheckForNewHighScore( s32 liSaveGameChallengeIndex,
                                          BrnStreetData::ChallengeHighScoreEntry* lpNewScore )
{
    BrnStreetData::ChallengeHighScoreEntry lCurrentHighScoreEntry;
    CgsNetwork::PlayerName                 lPlayerName;
    int32_t                                liScore;
    BrnStreetData::ScoreType               leScoreType;
    CgsContainers::FastBitArray<2>         lNewHighScoreTypes;
    CgsContainers::FastBitArray<2>         lScoresRuledByLocalPlayer;
    bool                                   lbNewHighScore;

    GetHighScoreEntry( liSaveGameChallengeIndex, &lCurrentHighScoreEntry, false );

    lScoresRuledByLocalPlayer.UnSetAll();

    for ( leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lCurrentHighScoreEntry.ContainsData( leScoreType ) )
        {
            lCurrentHighScoreEntry.GetScore( leScoreType, &liScore, &lPlayerName );

            if ( _strnicmp( lPlayerName.macName, KAC_LOCAL_PLAYER_NAME_TEXT, CgsNetwork::KI_USERNAME_LENGTH ) == 0 )
            {
                lScoresRuledByLocalPlayer.SetBit( static_cast<u32>( leScoreType ) );
            }
        }
    }

    lNewHighScoreTypes.UnSetAll();
    lbNewHighScore = false;

    if ( lCurrentHighScoreEntry.UpdateEntry( lpNewScore, &lNewHighScoreTypes ) )
    {
        for ( leScoreType = BrnStreetData::E_SCORE_TYPE_START;
              leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
              leScoreType++ )
        {
            if ( lNewHighScoreTypes.IsBitSet( static_cast<u32>( leScoreType ) ) )
            {
                BufferedNewHighScore lNewHighScoreToBuffer;

                lCurrentHighScoreEntry.GetScore( leScoreType, &liScore, &lPlayerName );
                lNewHighScoreToBuffer.mPlayerName.Construct( lPlayerName.macName );
                lNewHighScoreToBuffer.meScoreType = leScoreType;
                lNewHighScoreToBuffer.mRoadID     = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[liSaveGameChallengeIndex];
                lNewHighScoreToBuffer.mbIsRoadWhollyOwnedByOnePlayer =
                    lCurrentHighScoreEntry.IsWholeChallengeOwnedBySamePlayer();
                lNewHighScoreToBuffer.mbWasRoadRuledByPlayerBefore =
                    lScoresRuledByLocalPlayer.IsBitSet( static_cast<u32>( leScoreType ) );

                mNewHighScoreBuffer.EraseMatchingEntries( lNewHighScoreToBuffer.mRoadID, leScoreType );

                if ( mNewHighScoreBuffer.IsFull() )
                {
                    mbTooManyHighScoresToBuffer = true;

                    if ( lNewHighScoreToBuffer.mbWasRoadRuledByPlayerBefore && !mbFirstDownload )
                    {
                        ++miNumScoresLost;
                    }
                }
                else
                {
                    mNewHighScoreBuffer.Append( lNewHighScoreToBuffer );
                }

                maRoadRulesChangedBitArrays[leScoreType].SetBit( static_cast<u32>( liSaveGameChallengeIndex ) );

                lbNewHighScore = true;
            }
        }
    }

    return lbNewHighScore;
}

// ----------------------------------------------------------------------------
// A buddy left the friends list: blank every friend high score they own.
//
// Walks all 64 friend high-score slots that hold any score; for each score type
// owned by lpBuddyName (lobby-name compare) the score is cleared. Returns true
// when one of the cleared scores sat on the road the player is currently on
// (the slot index is used as the road index here, as the console does), so the
// caller can re-post that road's score record to the GUI.
// ----------------------------------------------------------------------------
bool StreetManager::ClearAllChallengeDataForBuddy( const CgsNetwork::PlayerName* lpBuddyName )
{
    bool lbClearedScoreForCurrentRoad = false;

    for ( BrnStreetData::ChallengeIndex lChallengeIndex = 0;
          lChallengeIndex < KI_MAX_CHALLENGES;
          ++lChallengeIndex )
    {
        BrnStreetData::ChallengeHighScoreEntry& lrEntry = maNetworkChallengeData[lChallengeIndex];

        if ( lrEntry.ContainsData( BrnStreetData::E_SCORE_TYPE_COUNT ) )
        {
            for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                  leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                  leScoreType++ )
            {
                if ( lrEntry.ContainsData( leScoreType ) )
                {
                    CgsNetwork::PlayerName lPlayerName;
                    int32_t                liScore;

                    lrEntry.GetScore( leScoreType, &liScore, &lPlayerName );

                    if ( CgsNetwork::UsernameCompare( lPlayerName.macName, lpBuddyName->macName ) == 0 )
                    {
                        // The road id is read before the current road id is fetched.
                        const ::CgsID lRoadId = mpStreetData->GetRoad( lChallengeIndex )->GetId();

                        if ( mpRoadRulesManager->GetCurrentRoadID() == lRoadId )
                        {
                            lbClearedScoreForCurrentRoad = true;
                        }

                        lrEntry.ClearScore( leScoreType );
                    }
                }
            }
        }
    }

    return lbClearedScoreForCurrentRoad;
}

// ----------------------------------------------------------------------------
// Overwrite one friend high-score slot with lpData.
// ----------------------------------------------------------------------------
void StreetManager::SetChallengeFriendHighScore( BrnStreetData::ChallengeIndex liIndex,
                                                 const BrnStreetData::ChallengeHighScoreEntry* lpData )
{
    CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES,
        CGS_ASSERT_MESSAGE_AT(
            "liIndex < BrnGameState::KI_MAX_CHALLENGES", CGS_ARTIST_FILE, 3674 /* ARTIST 0x82324C30 */));
    CGS_ASSERT( liIndex >= 0,
        CGS_ASSERT_MESSAGE_AT("liIndex >= 0", CGS_ARTIST_FILE, 3675 /* ARTIST 0x82324C54 */));
    CGS_ASSERT( lpData,
        CGS_ASSERT_MESSAGE_AT("lpData", CGS_ARTIST_FILE, 3676 /* ARTIST 0x82324C78 */));

    maNetworkChallengeData[liIndex].Copy( lpData );
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateStreetManager_wR_11.cpp
// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wR_11.cpp
//   (road-rules wave partfile -- the buffered new-high-score list)
//
//   NewHighScoreBuffer::EraseMatchingEntries
//
// Kept in its own partfile so it can be dropped without touching _wR_10 if
// another lane lands the same body.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N>::GetLength / operator[] / Erase

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Drop every buffered entry for (lRoadID, leScoreType).
//
// The index still advances after an Erase, so the entry that slides into the
// erased slot is not re-tested: two adjacent matches leave the second one in
// the buffer. That is the console's loop, kept as is.
// ----------------------------------------------------------------------------
void NewHighScoreBuffer::EraseMatchingEntries( ::CgsID lRoadID, BrnStreetData::ScoreType leScoreType )
{
    for ( u32 luIndex = 0; luIndex < GetLength(); ++luIndex )
    {
        if ( lRoadID == (*this)[luIndex].mRoadID &&
             leScoreType == (*this)[luIndex].meScoreType )
        {
            Erase( luIndex );
        }
    }
}

} // namespace BrnGameState

#undef CGS_ARTIST_FILE
#undef CGS_ARTIST_FILE_BRNGAMESTATESTREETMANAGER
#undef CGS_ARTIST_FILE_BRNRACECARENTITYMODULEOUTPUTINTERFACE
