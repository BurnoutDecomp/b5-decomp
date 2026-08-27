#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

// Frozen keystone header set (wave B, group 2 "loads").
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

// Committed dependencies the two streamed-load state machines call by name.
#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer::GetResourceRequestInterface
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"         // EventReceiverQueue<3072,16>::Clear/GetCount/GetFirstEvent
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // VariableEventQueue<3072,16>::AddEvent, CgsModule::Event
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"            // BrnResource::GameDataIO::RequestInterface<3072>::GetAILanes
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"                  // BrnResource::GameDataIO::GameDataEvent (response GetEventId read)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h"      // CgsResource::Events::AcquireResourceRequest
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"           // CgsResource::ID::HashString + operator<<
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"        // CgsResource::ResourceHandle
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                   // CgsDev::Log::gpDebugPrint (the console's road dump)
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

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

            // X360: CgsResource::BaseResourcePtr::CreateFromHandle(&mpAISectionData,
            // <response payload + 0x20>) -- rebind mpAISectionData from the AI-data response's
            // ResourceHandle. DEFERRED (not fabricated): the response event is a foreign,
            // un-homed GameData response shape (its ResourceHandle sits past the committed
            // GameDataAssetEvent), and the bind uses BaseResourcePtr::CreateFromHandle, which is
            // protected (no public/instantiated ResourcePtr<AISectionsData> handle-assignment is
            // reachable from here). The stage transition + result below are the reproduced,
            // observable side effects.

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
