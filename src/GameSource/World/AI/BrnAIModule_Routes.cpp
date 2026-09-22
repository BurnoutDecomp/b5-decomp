// =================================================================================================
// BrnAIModule_Routes.cpp -- the AI MODULE's side of the per-frame ROUTE round trip
// (aiwave A5, 2026-09-03). Partfile of BrnAIModule.cpp.
//
//   BrnAI::AIModule::UpdateCarRoutes  @0x827955F0  (127 insns; DWARF BrnAIModule.cpp:1495
//                                                   `void UpdateCarRoutes(OutputBuffer*, const OutputBuffer*)`)
//   + the two queue appends AIModule::Update @0x8279B478 inlines around it (0x8279B744 and
//     0x8279B800) and the read-lock bracket they share with it (0x8279B7D4 / 0x8279B808).
//
// ⭐ UpdateCarRoutes IS AN AIModule MEMBER (again, 2026-09-22): the console's body reads the two
// private player cursors and the embedded RaceBalancingManager, and the aiwave free-function form
// that took them as arguments could neither reach the manager (so the rubber-band route update was
// an empty park) nor evaluate the console's own event-117 gate. The two queue appends stay free
// functions in BrnAI::AIModuleRoutes -- they touch no module state -- and AIModule::Update /
// PausedUpdate call them in the console's order.
//
// =================================================================================================
// THE TRANSIENT "Route" IO BUFFER PAIR -- what AIModule::Update @0x8279B478 does, precisely
// =================================================================================================
// Two CgsModule::IOHelper<T> RAII objects (CgsModuleIOHelper.h: {mpStack, mpBuffer}, ctor =
// mpStack->CreateIOBuffer<T>(&mpBuffer, name) + assert, dtor = mpStack->DestroyIOBuffer<T>(&mpBuffer)
// + assert), both named "Route", both on Update's own stack frame:
//
//   INPUT  IOHelper<RouteMapModuleIO::InputBuffer>  at sp+var_90  ({mpStack @+0, mpBuffer @+4})
//     0x8279B578  ctor @0x82794B80 (lpInputBufferStack, "Route")
//                   -> IOBufferStack::CreateIOBuffer<RouteMapModuleIO::InputBuffer> @0x82791878:
//                      Alloc(0x3B0 == 944 bytes, "Route") then InputBuffer::Construct (the
//                      RaceRouteRequest[1] and ExtrapolatedRouteRequest[12] queues)
//     0x8279B92C  dtor inlined: DestroyIOBuffer<RouteMapModuleIO::InputBuffer> @0x8278A200
//                   (IOBuffer::Destruct + Free(944)), asserted (CgsModuleIOHelper.h:57)
//     SCOPE: the WHOLE non-paused body -- created before the input/output locks, destroyed after
//     StopMonitor, i.e. it exists even on frames where mbPlayerDataSet is clear.
//     STACK: lpInputBufferStack (Update's 2nd arg == the AI module's INPUT stack).
//
//   OUTPUT IOHelper<RouteMapModuleIO::OutputBuffer> at sp+var_A0  ({mpStack @+0, mpBuffer @+4})
//     0x8279B71C  ctor sub_82794BE8 (lpOutputBufferStack, "Route")
//                   -> CreateIOBuffer<RouteMapModuleIO::OutputBuffer> @0x82791960: Alloc(82192 ==
//                      0x14110 bytes, "Route") then OutputBuffer::Construct (RouteResponse[16])
//     0x8279B810  dtor = IOHelper<RouteMapModuleIO::OutputBuffer>::~IOHelper @0x82791820 (the
//                   export named "BrnAI::RouteMapModule") -> DestroyIOBuffer<OutputBuffer>
//                   @0x8278A2D8 (IOBuffer::Destruct + Free(82192)), asserted
//     SCOPE: inside the mbPlayerDataSet gate only -- from just after SortTrafficIntoAICars to just
//     before UpdateDrivers.
//     STACK: lpOutputBufferStack (Update's 3rd arg == the AI module's OUTPUT stack).
//
//   Both are LIFO pushes on the module's own stacks, so the destroy order (output first at
//   0x8279B810, input last at 0x8279B92C) is the reverse of the create order -- CgsIOBufferStack's
//   Free asserts exactly that.
//
// The console sequence between them (all inside the mbPlayerDataSet gate):
//   0x8279B724  IOBuffer::LockForWrite(routeIn)   (the 1-arg CgsModule::LockBuffersForIO @0x823B6F30
//                                                 is `assert(p); LockForWrite(p)`)
//   0x8279B744  routeIn->GetRaceRouteRequestQueue()->Append(*aiIn->GetRaceRouteRequestQueue())
//                                                 == AppendRaceRouteRequests() below
//   0x8279B758  UpdateCars(dt, routeIn, aiOut)     (lane A1/A2)
//   0x8279B798  mRouteRequestManager.Update(maAICars, GetAICar(mePlayerGlobalRaceCarIndex),
//                 GetAISectionsData(), routeIn, &mBuzzBy)          (BrnRouteRequestManager.cpp)
//   0x8279B7A0  IOBuffer::UnlockForWrite(routeIn)
//   0x8279B7C8  mRouteMapModule.Update(lpInputBufferStack, lpOutputBufferStack, routeIn, routeOut)
//                                                 (vtbl+68 == RouteMapModule::Update @0x82793ED8)
//   0x8279B7D4  IOBuffer::LockForRead(routeOut)
//   0x8279B7E4  UpdateCarRoutes(aiOut, routeOut)  == UpdateCarRoutes() below
//   0x8279B800  aiOut->GetRouteResponseQueueForWrite()->Append(*routeOut->GetRouteResponseQueue())
//                                                 == AppendRouteResponses() below
//   0x8279B808  IOBuffer::UnlockForRead(routeOut)
//   0x8279B810  ~IOHelper<OutputBuffer>           (routeOut is gone from here on)
// =================================================================================================

#include "GameSource/World/AI/BrnAIModule.h"                             // AIModule / AICar
#include "GameSource/World/AI/BrnAIModule_Routes.h"                      // the two free appends
#include "GameSource/World/AI/BrnAIDriver.h"                             // AIDriver::GetCar (event-117 gate)
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"                  // AIModuleIO::InputBuffer
#include "GameSource/World/AI/SharedIO/BrnAIModuleIO_OutputBuffer.h"     // AIModuleIO::OutputBuffer
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"               // the "Route" buffer pair
#include "GameSource/World/AI/Route/BrnRoute.h"                          // BrnAI::Route
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<1536,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnAI
{
namespace AIModuleRoutes
{
namespace
{
    // DWARF GameSource/GameState/BrnGameEvents.h:127 `E_EVENT_PLAYER_ROUTE_UPDATED = 117` (the
    // `li r5, 0x75` at 0x827957D8). [FLAG header_request] the tree's BrnGameEvents.h enum does
    // not carry 117 yet; DELETE-WHEN it does and use the enumerator.
    const s32 KI_EVENT_PLAYER_ROUTE_UPDATED = 117;

    // =====================================================================================
    // [FLAG PC witness] NOT IN THE X360 BINARY. The PER-CAR delivery-witness budget.
    //
    // The old witness here was a global first-8 and run6 spent every one of its lines on
    // "AICar 0 received route" (scratch/aiwave/run6/BrnGame.log lines 2547..3028) -- which is
    // both what a correct player delivery looks like AND what the miRaceCarIndex==0 identity
    // bug looks like when five rivals' responses are all addressed to car 0. A per-car budget
    // separates the two: with the identity fixed, each rival index gets its own lines; without
    // it, only bucket 0 ever fills. DELETE-WHEN rivals are seen driving their own routes.
    // =====================================================================================
    const s32 KI_ROUTE_DELIVERY_PER_CAR = 3;
    const s32 KI_ROUTE_DELIVERY_TOTAL   = 64;

    bool RouteDeliveryWitnessBudget(s32* lpaiPerCar, s32& lriTotal, u16 luEventId)
    {
        const u32 luBucket = (luEventId < 35u) ? static_cast<u32>(luEventId) : 35u;
        if (lriTotal >= KI_ROUTE_DELIVERY_TOTAL || lpaiPerCar[luBucket] >= KI_ROUTE_DELIVERY_PER_CAR)
        {
            return false;
        }
        ++lriTotal;
        ++lpaiPerCar[luBucket];
        return true;
    }
}

// =================================================================================================
// 0x8279B728..0x8279B744 -- the GUI / mode-manager race-route requests that arrived through the
// AI module's own input buffer are copied onto the transient "Route" input's 1-slot queue.
//   r29 = routeIn ; bl RouteMapModuleIO::InputBuffer::GetRaceRouteRequestQueue @0x8276AE00 (W)
//   r28 = aiIn    ; bl AIModuleIO::InputBuffer::GetRaceRouteRequestQueue     @0x8276D488 (R)
//   bl BaseEventQueue<RaceRouteRequest>::Append @0x8277B588  (the "Base event queue overflow"
//   tripwire is the queue's own: the destination holds ONE request)
// PRE: lpRouteInputBuffer write-locked (0x8279B724), lpInputBuffer read-locked (0x8279B580).
// =================================================================================================
void AppendRaceRouteRequests(RouteMapModuleIO::InputBuffer* lpRouteInputBuffer,
                             const AIModuleIO::InputBuffer* lpInputBuffer)
{
    CGS_ASSERT(lpRouteInputBuffer != 0, "lpRouteInputBuffer != NULL");
    CGS_ASSERT(lpInputBuffer      != 0, "lpInputBuffer != NULL");
    if (lpRouteInputBuffer == 0 || lpInputBuffer == 0)
    {
        return;
    }

    RouteMapModuleIO::RaceRouteRequestQueue*       lpDestination = lpRouteInputBuffer->GetRaceRouteRequestQueue();
    const RouteMapModuleIO::RaceRouteRequestQueue* lpSource      = lpInputBuffer->GetRaceRouteRequestQueue();

    lpDestination->Append(*lpSource);
}

}   // namespace AIModuleRoutes

// =================================================================================================
// AIModule::UpdateCarRoutes @0x827955F0 -- hand every AI-owned RouteResponse of the frame to its car.
//
//   r29 = this, r22 = lpOutputBuffer (aiOut), r31 = lpRouteOutputBuffer (routeOut)
//   0x8279560C  assert(lpRouteOutputBuffer != NULL)                             (:1503)
//   0x82795638  r25 = routeOut->GetRouteResponseQueue()  (const, R @0x8276B148 -- asserts the
//               read lock the caller took at 0x8279B7D4)
//   0x82795640  assert(lpRouteResponseQueue != NULL)                            (:1507)
//   0x82795664  r26 = queue->miLength ; for (i = 0; i < r26; ++i)
//   0x8279568C    r30 = &queue->GetEvent(i)  (@0x823AC158, the 5136-stride checked accessor)
//   0x82795694    assert(lpRouteResponse != NULL)                               (:1517)
//   0x827956B4    if (response->muOwnerId (+0x140C) != 0) continue     -- E_OWNER_AI only;
//                 GUI / mode-manager routes are for the bridge, not for a car
//   0x827956C4    car = GetAICar(response->muEventId (+0x140E))          -- the race-car index
//                 the request builder packed (BrnRouteRequestManager.cpp)
//   0x827956D0    if (car->meCarState (+0x14C8) not in {IN_RANGE, OUT_OF_RANGE}) continue
//   0x8279570C    car->UpdateRoute(response->GetRoute(), GetAISectionsData())   @0x8276F2C8
//   0x82795710    if (car->miOpponentIndex (+0x153A) != -1 && !car->mbIsPlayer (+0x1549)
//   0x8279573C        && car->mbIsInGameMode (+0x154B))
//   0x8279575C      mRaceBalancingManager.UpdateOpponentRoute(car, GetAISectionsData())
//                                        (add r3, r29, 0x3D9D0; the accessor is CALLED AGAIN)
//   0x8279577C  driver = GetAIDriver(mePlayerActiveRaceCarIndex)  (+0x4E9F8)
//   0x82795780  if (driver->mpCar (+0x1CE0) != NULL)             -- no mbIsActive test here
//   0x8279579C    car = GetAICar(mePlayerGlobalRaceCarIndex)          (+0x4E9FC, re-read here)
//   0x827957A0    if (car->mRoute.meStatus (+0x1408) != UNINITIALISED && miNodeCount (+0x1400) > 0)
//   0x827957E0      aiOut->GetGameEventQueue()->AddEvent(<1 uninitialised stack byte>, 117, 1)
//                                                                    == E_EVENT_PLAYER_ROUTE_UPDATED
//
// ⛔ CORRECTED 2026-09-22 (crash parity G04-D4 / G04-D5 / G05-D5):
//   * the UpdateOpponentRoute call was an empty [FLAG PC bring-up] block (its "no named home"
//     reason was stale: mRaceBalancingManager is a member and its TU is mounted), so no rival's
//     race-balancing route was ever Prepared and every balanced race ran its rivals at the
//     KF_DEFAULT_SPEED fallback;
//   * the event-117 gate took a caller-computed `GetAICar(mePlayerGlobalRaceCarIndex)` from before
//     HandleManagementEvents, so after a DEACTIVATE_RACE_CAR of the player's car (driver mpCar
//     nulled, global index stale) the PC kept posting 117 where the console posts nothing. The
//     console's own gate -- the player's ACTIVE-slot driver's mpCar -- is evaluated here, after
//     the response loop, exactly where the console reads it.
// =================================================================================================
void AIModule::UpdateCarRoutes(AIModuleIO::OutputBuffer* lpOutputBuffer,
                               const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer)
{
    CGS_ASSERT(lpRouteOutputBuffer != 0, "lpRouteOutputBuffer != NULL");   // :1503
    if (lpRouteOutputBuffer == 0)
    {
        return;   // [GUARD]
    }

    const RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue =
        lpRouteOutputBuffer->GetRouteResponseQueue();
    CGS_ASSERT(lpRouteResponseQueue != 0, "lpRouteResponseQueue != NULL");   // :1507
    if (lpRouteResponseQueue == 0)
    {
        return;   // [GUARD]
    }

    const s32 liResponseCount = lpRouteResponseQueue->GetLength();
    for (s32 liResponse = 0; liResponse < liResponseCount; ++liResponse)
    {
        const RouteMapModuleIO::RouteResponse* lpRouteResponse =
            &lpRouteResponseQueue->GetEvent(liResponse);
        CGS_ASSERT(lpRouteResponse != 0, "lpRouteResponse != NULL");   // :1517

        if (lpRouteResponse->GetOwnerId() != static_cast<u16>(RouteMapModuleIO::E_OWNER_AI))
        {
            continue;
        }

        AICar* lpAICar = GetAICar(lpRouteResponse->GetEventId());
        if (lpAICar == 0)   // [GUARD] GetAICar's PC-only out-of-range bail (see BrnAIModule_ResetPump.cpp)
        {
            // [FLAG PC witness] an AI-owned response whose event id is not a valid car index --
            // i.e. a request built with a bad miRaceCarIndex. Silent on the console.
            // DELETE-WHEN rivals are seen driving their own routes.
            static s32 siBadIndexWitness = 0;
            if (siBadIndexWitness < 4 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siBadIndexWitness;
                *CgsDev::Log::gpDebugPrint
                    << "[route] DISCARDED: AI route response for event id "
                    << static_cast<s32>(lpRouteResponse->GetEventId())
                    << " -- GetAICar() refused it (>= 35) [FLAG PC witness]\n";
            }
            continue;
        }

        if (!lpAICar->IsActive())
        {
            // [FLAG PC witness] the console's own meCarState gate (0x827956D0). A response that
            // lands on an INACTIVE car is a route the asking car will never see -- which is the
            // shape of a stale/mis-addressed event id. DELETE-WHEN rivals drive their own routes.
            static s32 siInactiveWitness = 0;
            if (siInactiveWitness < 8 && CgsDev::Log::gpDebugPrint != 0)
            {
                ++siInactiveWitness;
                *CgsDev::Log::gpDebugPrint
                    << "[route] DISCARDED: route response for AICar "
                    << static_cast<s32>(lpRouteResponse->GetEventId())
                    << " -- that car is INACTIVE (state "
                    << static_cast<s32>(lpAICar->GetState()) << ") [FLAG PC witness]\n";
            }
            continue;
        }

        lpAICar->UpdateRoute(lpRouteResponse->GetRoute(), GetAISectionsData());           // 0x827956FC/0x8279570C

        {
            // [FLAG PC witness] NOT IN THE X360 BINARY. Deliveries, budgeted PER CAR (see the
            // banner on KI_ROUTE_DELIVERY_PER_CAR): proves THIS car RECEIVED a route -- the
            // other end of RouteMapModule's "[route] extrapolated route done".
            // DELETE-WHEN rivals are seen driving.
            static s32 saiDeliveryWitness[36] = { 0 };
            static s32 siDeliveryWitnessTotal = 0;
            if (AIModuleRoutes::RouteDeliveryWitnessBudget(saiDeliveryWitness, siDeliveryWitnessTotal,
                                                           lpRouteResponse->GetEventId())
                && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[route] AICar " << static_cast<s32>(lpRouteResponse->GetEventId())
                    << " received route: nodes " << lpRouteResponse->GetRoute()->GetNodeCount()
                    << " status " << static_cast<s32>(lpRouteResponse->GetRoute()->GetStatus())
                    << " nextNode " << lpAICar->GetNextRouteNodeIndex() << "\n";
            }
        }

        if (lpAICar->GetOpponentIndex() != -1 && !lpAICar->IsPlayerCar() && lpAICar->mbIsInGameMode)   // 0x82795710..0x82795744
        {
            mRaceBalancingManager.UpdateOpponentRoute(lpAICar, GetAISectionsData());       // 0x82795748..0x8279575C
        }
    }

    // ---- the player's own route -> E_EVENT_PLAYER_ROUTE_UPDATED (0x8279576C..0x827957E0) ----
    AIDriver* lpPlayerDriver = GetAIDriver(mePlayerActiveRaceCarIndex);                      // 0x8279577C
    if (lpPlayerDriver != 0 /*[GUARD] GetAIDriver's host-only bail*/ && lpPlayerDriver->GetCar() != 0)   // 0x82795780 lwz 0x1CE0
    {
        const AICar* lpPlayerAICar = GetAICar(static_cast<u32>(mePlayerGlobalRaceCarIndex));   // 0x8279579C
        if (lpPlayerAICar != 0 /*[GUARD]*/ && lpPlayerAICar->HasValidRoute())                // 0x827957A0..0x827957B8
        {
            CGS_ASSERT(lpOutputBuffer != 0, "lpOutputBuffer != NULL");
            if (lpOutputBuffer != 0)
            {
                // The console posts ONE UNINITIALISED stack byte as the payload (`addi r4, r1,
                // var_60` with no store; `li r6, 1`): the event carries no data, only its type. A
                // zeroed byte here -- the consumer never reads it.
                u8 luPlayerRouteUpdatedEvent = 0;
                CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue =
                    reinterpret_cast<CgsModule::VariableEventQueue<1536, 16>*>(
                        lpOutputBuffer->GetGameEventQueue());
                lpGameEventQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&luPlayerRouteUpdatedEvent),
                    AIModuleRoutes::KI_EVENT_PLAYER_ROUTE_UPDATED, 1);
            }
        }
    }
}

namespace AIModuleRoutes
{
// =================================================================================================
// 0x8279B7E8..0x8279B800 -- every RouteResponse of the frame (AI-owned or not) is copied onto the
// AI module's OUTPUT buffer, where WorldModule::BridgeAIModuleToOutput (AppendRouteResponseQueue)
// carries it to the game-state / GUI bridges (BridgeWorldToGameState, BridgeWorldRouteInformationToGui).
//   bl 0x8276B148  routeOut->GetRouteResponseQueue()          (const, R; read lock asserted)
//   bl 0x8276DB18  aiOut->GetRouteResponseQueueForWrite()      (W; the AI output is write-locked
//                                                              by the spine at 0x8279B588)
//   bl 0x8277B668  BaseEventQueue<RouteResponse>::Append
// PRE: lpRouteOutputBuffer read-locked, lpOutputBuffer write-locked.
// =================================================================================================
void AppendRouteResponses(AIModuleIO::OutputBuffer* lpOutputBuffer,
                          const RouteMapModuleIO::OutputBuffer* lpRouteOutputBuffer)
{
    CGS_ASSERT(lpOutputBuffer      != 0, "lpOutputBuffer != NULL");
    CGS_ASSERT(lpRouteOutputBuffer != 0, "lpRouteOutputBuffer != NULL");
    if (lpOutputBuffer == 0 || lpRouteOutputBuffer == 0)
    {
        return;
    }

    const RouteMapModuleIO::RouteResponseQueue* lpSource = lpRouteOutputBuffer->GetRouteResponseQueue();
    RouteMapModuleIO::RouteResponseQueue* lpDestination =
        reinterpret_cast<RouteMapModuleIO::RouteResponseQueue*>(lpOutputBuffer->GetRouteResponseQueueForWrite());

    lpDestination->Append(*lpSource);
}

}   // namespace AIModuleRoutes
}   // namespace BrnAI
