#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"  // ChallengeHighScoreEntry::Construct/SetScore
#include "SharedClasses/StreetData/BrnChallengeData.h"                    // ChallengePlayerScoreEntry, ChallengeData::SetScore, ScoreType, operator++
#include "GameSource/GameState/BrnCgsPlayerName.h"                        // CgsNetwork::PlayerName
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT

// includes folded in from the BrnGameStateStreetManager_w*.cpp partfiles (2026-09-15)
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
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
BrnStreetData::ChallengeHighScoreEntry*
StreetManager::CreateHighScoreEntryFromDown(
    BrnStreetData::ChallengeHighScoreEntry* lpEntry,
    int                                     liUnused,
    const CgsNetwork::PlayerName*           lpPlayerNames,
    const int32_t*                          lpScores )
{
    (void)liUnused;   // reserved by the ABI, unreferenced by the X360 body

    lpEntry->Construct();

    // BrnStreetData::operator++ (out-of-line here) carries the inlined
    // "leEnumIndex <= E_SCORE_TYPE_COUNT" bounds assert the X360 fired each pass.
    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lpPlayerNames[leScoreType].macName[0] != '\0' && lpScores[leScoreType] != 0 )
        {
            lpEntry->SetScore( leScoreType, lpScores[leScoreType], &lpPlayerNames[leScoreType] );
        }
    }

    return lpEntry;
}

// @ 0x82324AC0
BrnStreetData::ChallengePlayerScoreEntry*
StreetManager::CreateUserChallengeScoreFr(
    BrnStreetData::ChallengePlayerScoreEntry* lpEntry,
    int                                       liUnused,
    const int32_t*                            lpScores )
{
    (void)liUnused;   // reserved by the ABI, unreferenced by the X360 body

    lpEntry->Construct();

    for ( BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
          leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
          leScoreType++ )
    {
        if ( lpScores[leScoreType] != 0 )
        {
            lpEntry->SetScore( leScoreType, lpScores[leScoreType] );
        }
    }

    return lpEntry;
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
                                        KPC_STREET_MANAGER_FILE, 3394 );
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
        [[fallthrough]];   // X360 falls straight into the case-3 poll

        case E_AI_DATA_ACQUIRE_REQUESTED:
        {
            if ( lpReceiverQueue->GetCount() == 0 )
            {
                return false;   // still waiting for the AI-data response
            }

            const CgsModule::Event* lpEvent = NULL;
            s32 liEventSize = 0;
            lpReceiverQueue->GetFirstEvent( &lpEvent, &liEventSize );
            CGS_ASSERT( lpEvent != NULL, "lpEvent != NULL" );

            const BrnResource::GameDataIO::GameDataEvent* lpAIDataResponse =
                reinterpret_cast<const BrnResource::GameDataIO::GameDataEvent*>( lpEvent );
            CGS_ASSERT( lpAIDataResponse->miEventId == 1, "lpAIDataResponse->GetEventId() == 1" );

            // ARTIST 0x8234FA70 binds the received AI-lanes resource before
            // marking the load complete (same native-width handle as Progression).
            const auto* asset = reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
            mpAISectionData = asset->mHandle;

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
            //  (2) THE POST SIZE WAS THE CONSOLE'S LITERAL 24. On x64 the record is 32 bytes
            //      (8-byte mpUser, 8-aligned CgsID at +0x10, mbCheckRefCount at +0x18), so a
            //      24-byte copy stopped at the end of mResourceId and left mbCheckRefCount
            //      reading whatever stale bytes the queue buffer held --
            //      PoolModule::DoAcquireResourceRequest passes it straight into
            //      FindResource's ref-count gate. Post sizeof(), the convention every committed
            //      producer uses (the consumer reads the record BY NAME, so producer and
            //      consumer agree by construction).
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
            CGS_ASSERT( false, "Unknown meDistrictMapLoadStage" );
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
const BrnStreetData::StreetData* StreetManager::GetStreetData()
{
    return mpStreetData.operator->();
}

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

// ⚠️ [FLAG PC bring-up] NOT AN X360 ACCESSOR -- and the deviation is the OWNER, not the data.
// SendSetUpAllEventStartsMessage reads the AI-section resource through the PROGRESSION manager's
// own copy (`sub_82367718(gameStateModule + 181300)` @0x82375A44/@0x82375C60 ==
// ResourcePtr<AISectionsData>::operator-> on mProgressionManager + 133380, since
// mProgressionManager sits at GameStateModule + 47920 and 47920 + 133380 == 181300 exactly).
// ProgressionManager::mpAISectionData HAS NO BINDER ANYWHERE IN THIS TREE -- grep it: the only
// two hits are comments, and ProgressionManager::LoadProgressionData binds mpProgressionData and
// nothing else. Calling operator-> on it would fire the ResourcePtr's own "Can not instance
// resource pointer - it has no main memory resource" assert and hand back a null the console's
// shape then dereferences inside BuildAISectionPointMap.
// THIS manager's mpAISectionData is the SAME resource (both are the AI-lanes acquire) and IS
// bound, by LoadAIData in this very file. So the producer reads it from here instead, and says so.
// DELETE-WHEN ProgressionManager::mpAISectionData gets its console binder (the same
// ComputeLandmarkAISectionIndices @0x82370008 frontier BrnProgressionManager.cpp:601 names).
const BrnAI::AISectionsData* StreetManager::GetAISectionData() const
{
    if (!mpAISectionData.HasMemoryResource())
    {
        return 0;
    }
    return mpAISectionData.operator->();
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
    CGS_ASSERT(static_cast<u32>(road) < KI_MAX_CHALLENGES, "liRoadIndex >= 0 && liRoadIndex < KI_MAX_CHALLENGES");
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
    lpRulesQueryAction->miNumRoads = GetStreetData()->GetRoadCount();

    CGS_ASSERT( lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES,
                "lpRulesQueryAction->miNumRoads <= KI_MAX_CHALLENGES" );

    for ( s32 liRoadIndex = 0; liRoadIndex < lpRulesQueryAction->miNumRoads; ++liRoadIndex )
    {
        lpRulesQueryAction->maRoadIds[liRoadIndex] = GetStreetData()->GetRoad( liRoadIndex )->GetId();

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
    CGS_ASSERT( lpData, "lpData" );

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

    if ( !lParScore.ContainsData( leScoreType ) )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NO_DATA;
    }

    const s32 liPlayerScore = lUserScore.GetScore( leScoreType );

    s32   liParScore = 0;
    CgsID lParRivalId = 0;
    lParScore.GetScore( leScoreType, &liParScore, &lParRivalId );

    // `cmpwi r3, 0 / bgt` -- strictly greater means the player has NOT beaten it.
    if ( BrnStreetData::ChallengeData::CompareScores( leScoreType, liPlayerScore, liParScore ) > 0 )
    {
        return E_ROAD_RULE_COMPLETION_STATUS_NOT_BEATEN;
    }

    return E_ROAD_RULE_COMPLETION_STATUS_BEATEN;
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
    s32                     liFriendScore = 0;
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

// @ 0x82335E08. Copy the local player's ChallengePlayerScoreEntry for a slot into
// lpData (constructed empty first). When lbByRoadIndex is set, liIndex is first the
// road index and is remapped to its save-game challenge slot via the road id scan;
// a road with no save-game slot leaves lpData at its Construct'd default.
void StreetManager::GetChallengeUserScore( BrnStreetData::ChallengeIndex liIndex,
                                           BrnStreetData::ChallengePlayerScoreEntry* lpData,
                                           bool lbByRoadIndex ) const
{
    CGS_ASSERT( lpData, "lpData" );

    lpData->Construct();

    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liIndex = 0;
        const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
        while ( *lpSlotRoadId != lpRoad->GetId() )
        {
            ++lpSlotRoadId;
            ++liIndex;
            if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
            {
                return;
            }
        }
    }

    if ( liIndex >= 0 )
    {
        CGS_ASSERT( liIndex < BrnGameState::KI_MAX_CHALLENGES,
                    "liChallengeIndex < BrnGameState::KI_MAX_CHALLENGES" );

        const BrnStreetData::ChallengePlayerScoreEntry* lpChallenge = &maChallengeData[liIndex];
        CGS_ASSERT( lpChallenge, "lpChallenge" );
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
                "liIndex < BrnGameState::KI_MAX_CHALLENGES" );
    CGS_ASSERT( liIndex >= 0, "liIndex >= 0" );
    CGS_ASSERT( lpData, "lpData" );

    lpData->Construct();

    BrnStreetData::ChallengeIndex liSlotIndex = liIndex;
    if ( lbByRoadIndex )
    {
        const BrnStreetData::Road* lpRoad = mpStreetData->GetRoad( liIndex );

        liSlotIndex = 0;
        const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
        while ( *lpSlotRoadId != lpRoad->GetId() )
        {
            ++lpSlotRoadId;
            ++liSlotIndex;
            if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
            {
                return;
            }
        }
    }

    if ( liSlotIndex >= 0 )
    {
        const BrnStreetData::ChallengeHighScoreEntry* lpChallenge = &maNetworkChallengeData[liSlotIndex];
        CGS_ASSERT( lpChallenge, "lpChallenge" );
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
    // -----------------------------------------------------------------------
    // @ 0x82335978. Wire the three owner pointers, zero the score tables and the
    // whole walk / upcoming-roads state, construct the embedded debug component,
    // and register the five perf monitors.
    // -----------------------------------------------------------------------
    void StreetManager::Construct( GameStateModule* lpGameStateModule,
                                   BrnProgression::ProgressionManager* lpProgression,
                                   RoadRulesManager* lpRoadRulesManager )
    {
        // Embedded debug component: base Construct, miNumberOfRoadRulesToWin = 0,
        // mpStreetManager = this (the COMDAT-folded BaseCollisionGenerator::Destruct
        // at this+0x1DF0 is the DebugComponent base Construct).
        mStreetManagerDebugComponent.Construct( this );

        CGS_ASSERT( lpProgression, "lpProgression" );
        mpProgressionManager = lpProgression;

        CGS_ASSERT( lpGameStateModule, "lpGameStateModule" );
        mpGameStateModule = lpGameStateModule;

        CGS_ASSERT( lpRoadRulesManager, "lpRoadRulesManager" );
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
            maRoadRulesChangedBitArrays[leEnumIndex].UnSetAll();
        }

        miCurrentPlayerRoadIndex       = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miLastPlayerRoadIndex          = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;
        mCurrentPlayerSectionExitRight.SetZero();
        mFirstJunctionPosition.SetZero();
        miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
        miNextFreeSectionSlot          = 0;
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
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miSetRoadRuleNetworkHighScoresPM =
            CgsDev::PerfMonCpu::AddMonitor( "SetRoadRuleNetworkHighScores",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoads",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsSentMessagePM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsSendMessage",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );
        miUpcomingRoadsQSortPM =
            CgsDev::PerfMonCpu::AddMonitor( "UpcomingRoadsQSort",
                                            static_cast<CgsDev::PerfMonCpuPage>( 5 ), false, 1.0f, true );

        CGS_ASSERT( miSetRoadRuleChallengeDataPM >= 0,     "miSetRoadRuleChallengeDataPM >= 0" );
        CGS_ASSERT( miSetRoadRuleNetworkHighScoresPM >= 0, "miSetRoadRuleNetworkHighScoresPM >= 0" );
        CGS_ASSERT( miUpcomingRoadsPM >= 0,                "miUpcomingRoadsPM >= 0" );
        CGS_ASSERT( miUpcomingRoadsSentMessagePM >= 0,     "miUpcomingRoadsSentMessagePM >= 0" );
        CGS_ASSERT( miUpcomingRoadsQSortPM >= 0,           "miUpcomingRoadsQSortPM >= 0" );
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
        meActiveRoadRuleType        = BrnStreetData::E_SCORE_TYPE_COUNT;
        mbFirstDownload             = true;

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
    // portals by ascending absolute entry angle (DecFIGS: two rw::math::fpu::
    // Abs<float> reads of mfEntryAngle, deoptimised here to fabsf).
    // -----------------------------------------------------------------------
    int32_t CompareSectionWalkData( const void* lpData1, const void* lpData2 )
    {
        const SectionWalkData* lpWalkData1 = static_cast<const SectionWalkData*>( lpData1 );
        const SectionWalkData* lpWalkData2 = static_cast<const SectionWalkData*>( lpData2 );

        const f32 lfAbsAngle1 = fabsf( lpWalkData1->mfEntryAngle );
        const f32 lfAbsAngle2 = fabsf( lpWalkData2->mfEntryAngle );

        if ( lfAbsAngle1 < lfAbsAngle2 )
            return -1;
        if ( lfAbsAngle1 > lfAbsAngle2 )
            return 1;
        return 0;
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
                    CGS_ASSERT( false, "How did it get here?" );
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
            CGS_ASSERT( luLinkSectionIndex != 0x7FFF, "luLinkSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX" );

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

        CGS_ASSERT( liNumPortalsToExplore > 0, "liNumPortalsToExplore > 0" );

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
        const f32 lfPlayerDirX = lPlayerDirection.x;
        const f32 lfPlayerDirY = lPlayerDirection.y;
        const f32 lfPlayerDirZ = lPlayerDirection.z;

        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const f32 lfPlayerPosX = lPlayerPosition.x;
        const f32 lfPlayerPosY = lPlayerPosition.y;
        const f32 lfPlayerPosZ = lPlayerPosition.z;

        u16 luBestPortalSectionIndex = 0x7FFF;
        f32 lfBestPortalDot = 0.0f;
        f32 lfBestPortalX = 0.0f;
        f32 lfBestPortalY = 0.0f;
        f32 lfBestPortalZ = 0.0f;

        miNewestForwardPlayerRoadIndex = -1;
        miNewestLeftPlayerRoadIndex = -1;
        miNextFreeSectionSlot = 0;
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
            if ( ( lpPlayerRaceCarState->mTransform.xAxis.x * mCurrentPlayerSectionExitRight.X() )
               + ( mCurrentPlayerSectionExitRight.Z() * lpPlayerRaceCarState->mTransform.xAxis.z )
               + ( mCurrentPlayerSectionExitRight.Y() * lpPlayerRaceCarState->mTransform.xAxis.y ) < 0.0f )
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
    CGS_ASSERT(miNextFreeSectionSlot < KI_MAX_SECTIONS_TO_WALK, "miNextFreeSectionSlot < KI_MAX_SECTIONS_TO_WALK");
    CGS_ASSERT(section != 0x7FFF, "luNewSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX");
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
        CGS_ASSERT( lpLastAICarOutputInterface != NULL, "lpLastAICarOutputInterface != NULL" );

        const s32 liPlayerRoadTargetNodeIndex = miNewRoadNodeIndex - 2;
        if ( liPlayerRoadTargetNodeIndex >= 0 )
        {
            // GetPlayerRoute() folds to the interface pointer itself (mPlayerRoute @ +0);
            // view it as the now-committed BrnAI::Route (same address the X360 asserts twice).
            const BrnAI::Route* lpPlayerRoute =
                reinterpret_cast<const BrnAI::Route*>( lpLastAICarOutputInterface->GetPlayerRoute() );
            CGS_ASSERT( lpPlayerRoute != NULL, "lpPlayerRoute != NULL" );

            if ( lpPlayerRoute->GetNodeCount() >= 1 )
            {
                const BrnAI::RouteNode* lpPlayerRoadTargetRouteNode =
                    lpPlayerRoute->GetNode( liPlayerRoadTargetNodeIndex );
                CGS_ASSERT( lpPlayerRoadTargetRouteNode != NULL, "lpPlayerRoadTargetRouteNode != NULL" );

                // Node position onto the ground plane: (x, 0, y).
                const f32 lfTargetNodeX = lpPlayerRoadTargetRouteNode->GetX();
                const f32 lfTargetNodeZ = lpPlayerRoadTargetRouteNode->GetY();

                // Player position with its vertical lane dropped (v15[1] = 0).
                Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
                lPlayerPosition.y = 0.0f;

                const f32 lfDeltaX = lPlayerPosition.x - lfTargetNodeX;
                const f32 lfDeltaY = lPlayerPosition.y - 0.0f;
                const f32 lfDeltaZ = lPlayerPosition.z - lfTargetNodeZ;
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
                    "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        bool lbPlayerCarActive = false;
        if ( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID )
        {
            lbPlayerCarActive = lpActiveRaceCarInterface->IsPlayerCarActive();
        }

        if ( lbPlayerCarActive )
        {
            CGS_ASSERT( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        "Player car index hasn't been set" );
            CGS_ASSERT( lePlayerActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        "lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID" );
            CGS_ASSERT( lePlayerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

            const u16 luRaceCarAISection = lpActiveRaceCarInterface->GetRaceCarAISection( lePlayerActiveRaceCarIndex );
            const BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luRaceCarAISection );
            miCurrentPlayerRoadIndex = liRoadIndex;

            if ( liRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
            {
                // Junction path: invalidate the upcoming/hopeful roads and age the junction timer.
                const f32 lfNewTimeInJunction = mfTimeInJunction + lfSimTimeStep;

                miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
                miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
                miNextFreeSectionSlot          = 0;
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
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfMaxSpeedMPH ) * 4.0f ) > lfDistanceToTurning
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
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfMaxSpeedMPH ) * 4.0f ) > lfDistanceToTurning
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
    using UpcomingRoadMessage = BrnGameState::GameStateModuleIO::UpcomingRoadChangeAction;

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
        Vector3 lJunctionPosition;
        lJunctionPosition.x = mFirstJunctionPosition.X();
        lJunctionPosition.y = mFirstJunctionPosition.Y();
        lJunctionPosition.z = mFirstJunctionPosition.Z();
        lJunctionPosition.w = 0.0f;

        Vector3 lSecondJunctionPosition;
        lSecondJunctionPosition.x = mFirstJunctionPosition.X();
        lSecondJunctionPosition.y = mFirstJunctionPosition.Y();
        lSecondJunctionPosition.z = mFirstJunctionPosition.Z();
        lSecondJunctionPosition.w = 0.0f;

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
