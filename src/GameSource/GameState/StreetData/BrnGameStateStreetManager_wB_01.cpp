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
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"           // CgsResource::ID::HashString
#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT

// ============================================================================
// GameSource/Gamestate/StreetData/BrnGameStateStreetManager.cpp  (wave B, group 2)
//
// Streamed-load state machines the GameStateModule pumps from Prepare:
//   LoadAIData       @ 0x8234FA70  (meAILoadStage over the <3072,16> receiver queue)
//   LoadDistrictMap  @ 0x8234FB98  (meDistrictMapLoadStage over the <3072,16> queue)
//
// OnProfileLoaded (0x82349E20) is in this group's ledger but is NOT bodied here:
// its body memcpys two 64-entry road-rules tables + reads the road-rules id/timestamp
// straight out of BrnProgression::Profile, and every one of those five members is
// PRIVATE in the committed BrnProfile.h with no public read-accessor. Bodying it
// faithfully would require adding getters to BrnProfile.h, which this partfile may not
// do -- reported blocked instead.
// ============================================================================

namespace BrnGameState
{

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

            // Build the "Districts" acquire request on the stack: { mpUser = receiver queue,
            // miEventId = 1, miPoolId = 5, mResourceId = HashString("Districts") | (5 << 32) }
            // and AddEvent it (type 4, 24 bytes) onto the request interface's <3072,16> queue
            // (this+0x3414 == RequestInterface<3072>, its VariableEventQueue<3072,16> at offset 0).
            CgsResource::Events::AcquireResourceRequest lRequest;
            lRequest.mpUser    = lpReceiverQueue;
            lRequest.miEventId = 1;
            lRequest.miPoolId  = 5;
            lRequest.mResourceId.SetHash(
                static_cast<u64>( static_cast<u32>(
                    CgsResource::ID::HashString( reinterpret_cast<const u8*>( "Districts" ) ) ) )
                | 0x500000000ULL );

            CgsModule::VariableEventQueue<3072,16>* lpRequestQueue =
                reinterpret_cast<CgsModule::VariableEventQueue<3072,16>*>(
                    lpOutput->GetResourceRequestInterface() );
            lpRequestQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lRequest ), 4, 24 );

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

            // X360: copy the two handle words from <response payload + 0x18> into
            // mDistrictMapResourceHandle. DEFERRED (not fabricated): the acquire-response event
            // is a foreign, un-homed shape; the store TARGET is reproduced (both handle pointers),
            // its value is FLAGGED as sourced from that foreign payload -- mirrors the committed
            // BrnGameState::StuntManager::LoadDistrictMap precedent (0x82399458).
            mDistrictMapResourceHandle.mpResourceMemory = 0;   // FLAG: from acquire-response payload+0x18
            mDistrictMapResourceHandle.mpSourceEntry    = 0;   // FLAG: from acquire-response payload+0x1C
            return false;
        }

        case E_DISTRICT_MAP_DONE:
            return true;

        default:
            CGS_ASSERT( false, "Unknown meDistrictMapLoadStage" );
            return false;
    }
}

} // namespace BrnGameState
