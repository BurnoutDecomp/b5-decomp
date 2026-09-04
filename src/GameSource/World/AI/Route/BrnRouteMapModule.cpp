#include "BrnRouteMapModule.h"

#include "SharedClasses/AI/AISectionsResourceType.h"   // BrnAI::AISectionsData (complete type)
#include "GameSource/World/AI/BrnAIPortal.h"                          // BrnAI::Portal (node positions)
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"    // RacingLineGenerator::Extrapolate*
#include "GameShared/GameClasses/Module/CgsIOBufferStack.h"          // CgsModule::IOBufferStack
#include "GameShared/GameClasses/Core/CgsAssert.h"                   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // gpDebugPrint (witness)

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnAI::RouteMapModule::RouteMapModule           @0x827E23F0
//   BrnAI::RouteMapModule::Prepare                  @0x8277FDE8
//   BrnAI::RouteMapModule::Update                   @0x82793ED8  (aiwave A5, 2026-09-03)
//   BrnAI::RouteMapModule::ProcessRaceRoute         @0x8278C2E0  (aiwave A5, 2026-09-03)
//   BrnAI::RouteMapModule::ProcessExtrapolatedRoute @0x8278C4C8  (aiwave A5, 2026-09-03; PARKED
//                                                                 on the RacingLineGenerator)
//
// The ctor constructs the two read/write mutexes (handled by member construction),
// clears the intrusive-list anchor and wires its node pointers back to the
// anchor (empty circular list), and installs the static dispatch table.

namespace BrnAI
{
namespace
{
    // =====================================================================================
    // [FLAG PC witness] NOT IN THE X360 BINARY. The shared PER-EVENT-ID witness budget.
    //
    // ⭐ WHY A PER-ID BUDGET AND NOT "first N". Every route witness in this file used to be a
    // global first-8, and run6 (scratch/aiwave/run6/BrnGame.log) showed why that cannot answer
    // the question this campaign asks. The AI roster comes up in stages -- the player's AICar is
    // live from the junkyard hand-off, the five Road Rage rivals only from ADD_CAR_TO_MODE about
    // ten seconds later -- so all eight lines were spent on the PLAYER's own routes by log line
    // 3,028 of 802,050, four frames after the first rival appeared. The run therefore could not
    // distinguish "the rivals never asked" from "the rivals asked and got an empty road", which
    // is precisely the fork the lane was opened to resolve.
    //
    // The event id IS the asking car's AICar::miRaceCarIndex (the request builders store it at
    // ExtrapolatedRouteRequest +0x02 / RaceRouteRequest +0x6A -- X360 0x82789128 / 0x82791568),
    // so a per-id budget gives every car on the roster its own quota and a late-activating rival
    // still gets witnessed. The array is sized 36 (35 global race cars + one catch-all bucket
    // for a GUI / mode-manager id that is not a car index).
    // DELETE-WHEN rivals are seen driving their own routes.
    // =====================================================================================
    const s32 KI_ROUTE_WITNESS_PER_EVENT = 3;
    const s32 KI_ROUTE_WITNESS_TOTAL     = 64;

    // Consumes one line from luEventId's quota; returns true when a line may be printed.
    bool RouteMapWitnessBudget(s32* lpaiPerEvent, s32& lriTotal, u16 luEventId)
    {
        const u32 luBucket = (luEventId < 35u) ? static_cast<u32>(luEventId) : 35u;
        if (lriTotal >= KI_ROUTE_WITNESS_TOTAL ||
            lpaiPerEvent[luBucket] >= KI_ROUTE_WITNESS_PER_EVENT)
        {
            return false;
        }
        ++lriTotal;
        ++lpaiPerEvent[luBucket];
        return true;
    }
}

RouteMapModule::RouteMapModule()
{
    mAnchorState = 0;
    mUnk6505     = 0;
    mUnk6506     = 0;

    mpListHead   = &mAnchorState;
    mpListTail   = &mAnchorState;
    mpListCursor = &mAnchorState;
    mUnk6510     = 0;

    // Guest static dispatch table at 0x820CDFC4.
    mpAllocatorIface = reinterpret_cast<void*>(0x820CDFC4);

    // The console's .bss resting value; ProcessRaceRoute latches the real ids per request.
    muEventId = 0;
    muOwnerId = 0;
}

// ---------------------------------------------------------------------------------------
// Prepare @0x8277FDE8 -- the AI module's stage 3 (its vtable slot 16; see the header).
//
//   0x8277FE04  bl  CgsResource::BaseResourcePtr::CreateFromHandle(this + 26016, &lHandle)
//   0x8277FE10  bl  CgsDev::DebugComponent::Register(this + 26052)
//   0x8277FE18  bl  BrnAI::ResourcePtr<AISectionsData>::GetMemoryResource(this + 26016)
//   0x8277FE24  bl  BrnAI::AStar::Construct(this + 552, <that>)
//   0x8277FE2C  b   CgsModule::ModuleSingleBuffered::Prepare(this)
//
// The CreateFromHandle call is spelled here as the ResourcePtr's own
// `operator=(const ResourceHandle&)`, which the committed CgsResourcePtr.h documents as
// compiling to exactly that ONE call at every attested assign site (FlaptManager::Construct,
// the ChallengeList/WheelList slot resets) -- no list unlink, no re-init around it.
//
// ⚠️ [FLAG PC boot gate] the `DebugComponent::Register(this + 26052)` step is PARKED: the
// route-map debug component that lives at that offset is not a member of this class yet (it
// sits inside mWorkingSetPad), and registering an object that was never constructed is the
// [[valid-pointer-invalid-object]] shape -- CgsDev::DebugComponent::Register links it into the
// global debug list, after which the debug UI walks it every frame. Nothing on the reset-on-track
// path needs it. Restore it WITH the component as a named member.
bool RouteMapModule::Prepare(CgsResource::ResourceHandle lHandle)
{
    mAISectionsData = lHandle;

    // [FLAG PC boot gate] CgsDev::DebugComponent::Register(this + 26052) -- see the banner.

    // ⭐ UN-PARKED 2026-09-03 (aiwave A5). The previous park cited a 5 x LNK2019 closure gap in
    // BrnAStar.cpp (AStarNodePool::FindNode, AStar::IsBlockSection, Route::AddNode,
    // Portal::GetLinkSectionIndex); FindNode / IsBlockSection / AddBlockSectionId / IsInProgress
    // are bodied there now, BrnAIPortal.cpp is mounted, and BrnRoute.cpp goes on the mount list
    // with this slice. The pathfinder is bound the moment the road network is, exactly as
    // 0x8277FE18..0x8277FE24 do it.
    mAStar.Construct(mAISectionsData.GetMemoryResource());

    return CgsModule::ModuleSingleBuffered::Prepare();
}

// The route map's bound road network (X360 offset 0x65A0; RouteMapDebugComponent::OnActivate
// @0x8277FE50 reads it through the same ResourcePtr accessor).
AISectionsData* RouteMapModule::GetAISectionsData() const
{
    return const_cast<AISectionsData*>(mAISectionsData.GetMemoryResource());
}

// ---------------------------------------------------------------------------------------
// Update @0x82793ED8 (slot 17 / vtbl+68; caller AIModule::Update 0x8279B7C8 and PausedUpdate).
//
//   r27 = this, r4 = lpInputBufferStack, r30 = lpOutputBufferStack, r26 = lpInputBuffer,
//   r25 = lpOutputBuffer
//   0x82793EF8..0x82793F88  the four != NULL asserts (BrnRouteMapModule.cpp:97..100)
//   0x82793F90  IOBuffer::LockForRead(lpInputBuffer) ; 0x82793F98 LockForWrite(lpOutputBuffer)
//   0x82793FA0  r31 = lpInputBuffer->GetRaceRouteRequestQueue()         (const, R @0x8276AEA8)
//   0x82793FAC  r30 = lpInputBuffer->GetExtrapolatedRouteRequestQueue() (const, R @0x8276AFF8)
//   0x82793FB8  r28 = lpOutputBuffer->GetRouteResponseQueueForWrite()   (W @0x8276B0A0)
//   0x82793FBC  if (race->miLength > 0)      ProcessRaceRoute(&race->GetEvent(0), r28)
//   0x8279402C  else if (mAStar.mbInProgress) ProcessRaceRoute(NULL, r28)   (`lbz 0x659D(this)`)
//   0x82794048  for (i < extrapolated->miLength) ProcessExtrapolatedRoute(&GetEvent(i), r28)
//   0x82794088  UnlockForRead(input) ; 0x82794090 UnlockForWrite(output)
//
// The two IOBufferStacks are asserted non-NULL and otherwise unused (the console body never
// reads them again) -- the buffers this module services are the ones its caller borrowed.
// The GetEvent(0) on the race queue is the inlined checked accessor (its "mpEvents != NULL" /
// "liIndex < GetLength()" tripwires at CgsBaseEventQueue.h:272/274 are inside the tree's
// BaseEventQueue<T>::GetEvent).
// ---------------------------------------------------------------------------------------
void RouteMapModule::Update(CgsModule::IOBufferStack* lpInputBufferStack,
                            CgsModule::IOBufferStack* lpOutputBufferStack,
                            const RouteMapModuleIO::InputBuffer* lpInputBuffer,
                            RouteMapModuleIO::OutputBuffer* lpOutputBuffer)
{
    CGS_ASSERT(lpInputBufferStack  != 0, "lpInputBufferStack != NULL");    // :97
    CGS_ASSERT(lpOutputBufferStack != 0, "lpOutputBufferStack != NULL");   // :98
    CGS_ASSERT(lpInputBuffer       != 0, "lpInputBuffer != NULL");         // :99
    CGS_ASSERT(lpOutputBuffer      != 0, "lpOutputBuffer != NULL");        // :100
    (void)lpInputBufferStack;
    (void)lpOutputBufferStack;

    // [GUARD] the console dereferences both buffers unconditionally after the asserts; a NULL
    // here is a spine wiring error, not a frame condition.
    if (lpInputBuffer == 0 || lpOutputBuffer == 0)
    {
        return;
    }

    lpInputBuffer->LockForRead();
    lpOutputBuffer->LockForWrite();

    const RouteMapModuleIO::RaceRouteRequestQueue*         lpRaceRouteRequestQueue =
        lpInputBuffer->GetRaceRouteRequestQueue();
    const RouteMapModuleIO::ExtrapolatedRouteRequestQueue* lpExtrapolatedRouteRequestQueue =
        lpInputBuffer->GetExtrapolatedRouteRequestQueue();
    RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue =
        reinterpret_cast<RouteMapModuleIO::RouteResponseQueue*>(
            lpOutputBuffer->GetRouteResponseQueueForWrite());

    if (lpRaceRouteRequestQueue->GetLength() > 0)
    {
        ProcessRaceRoute(&lpRaceRouteRequestQueue->GetEvent(0), lpRouteResponseQueue);
    }
    else if (mAStar.IsInProgress())
    {
        ProcessRaceRoute(0, lpRouteResponseQueue);
    }

    for (s32 liRequest = 0; liRequest < lpExtrapolatedRouteRequestQueue->GetLength(); ++liRequest)
    {
        ProcessExtrapolatedRoute(&lpExtrapolatedRouteRequestQueue->GetEvent(liRequest),
                                 lpRouteResponseQueue);
    }

    lpInputBuffer->UnlockForRead();
    lpOutputBuffer->UnlockForWrite();
}

// ---------------------------------------------------------------------------------------
// ProcessRaceRoute @0x8278C2E0 (DWARF BrnRouteMapModule.cpp:171).
//
//   r27 = this, r31 = lpRouteRequest, r21 = lpRouteResponseQueue, r30 = &mAStar (this+0x228)
//   0x8278C2FC  assert(mAStar.mbInProgress || lpRouteRequest != NULL)        (:173)
//   0x8278C330  if (!mAStar.mbInProgress) {                              -- a FRESH request
//   0x8278C344    muOwnerId = req->muOwnerId (+0x68)  -> sth 0x65C2(this)
//   0x8278C374    muEventId = req->muEventId (+0x6A)  -> sth 0x65C0(this)
//   0x8278C34C    start2D = (req->mStartPosition.x, .z)   (lvx128 +0x00 ; lfs +0 / lfs +8)
//   0x8278C364    end2D   = (req->mEndPosition.x, .z)     (lvx128 +0x10 ; lfs +0 / lfs +8)
//   0x8278C3B0    mAStar.Prepare(start2D, end2D, req->muStartSectionIndex (+0x20),
//                   req->muEndSectionIndex (+0x22), req->meQuality (+0x6C),
//                   req->meDistanceFunction (+0x70), req->mbUseAIShortcuts (+0x74))
//   0x8278C3DC    for (i < req->mauBlockSections.count (+0x64)) mAStar.AddBlockSectionId(id[i])
//                 (the Array "used before Construct" tripwire at CgsArray.h:336 is the
//                 container's own; the KI_MAX_BLOCK_SECTION_COUNT one is inside AddBlockSectionId)
//   0x8278C46C  mAStar.Compute()                                          -- ONE iteration
//   0x8278C470  if (!mAStar.mbInProgress) {                               -- the search ENDED
//   0x8278C488    RouteResponse on the stack: route.miNodeCount = 0, miDefaultStartNode = 0,
//                 meStatus = 0 (the three `stw r22(0)` at +0x1400/+0x1404/+0x1408), then
//                 response.muOwnerId = muOwnerId (+0x140C), muEventId = muEventId (+0x140E)
//   0x8278C4A0    mAStar.BuildRoute(&route)          (sub_8278C210 == the public dispatcher)
//   0x8278C4AC    route.Prepare(0)
//   0x8278C4B8    lpRouteResponseQueue->AddEvent(response)
//
// The same request keeps being serviced across frames: AIModule rebuilds the "Route" input
// buffer every frame, so on continuation frames the race queue is EMPTY and Update passes NULL
// -- which is why the ids are latched into the module on the first frame.
// ---------------------------------------------------------------------------------------
void RouteMapModule::ProcessRaceRoute(const RouteMapModuleIO::RaceRouteRequest* lpRouteRequest,
                                      RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue)
{
    CGS_ASSERT(mAStar.IsInProgress() || (lpRouteRequest != 0),
               "mAStar.IsInProgress() || (lpRouteRequest != NULL )");   // :173

    if (!mAStar.IsInProgress())
    {
        // [GUARD] the console would dereference a NULL request here (the assert above is not a
        // guard); the only way in is a spine wiring error.
        if (lpRouteRequest == 0)
        {
            return;
        }

        muOwnerId = lpRouteRequest->GetOwnerId();
        muEventId = lpRouteRequest->GetEventId();

        const Vector3 lStartPosition = lpRouteRequest->GetStartPosition();
        const Vector3 lEndPosition   = lpRouteRequest->GetEndPosition();
        const AStarVector2 lStart2D(lStartPosition.x, lStartPosition.z);
        const AStarVector2 lEnd2D(lEndPosition.x, lEndPosition.z);

        mAStar.Prepare(lStart2D, lEnd2D,
                       lpRouteRequest->GetStartSectionIndex(),
                       lpRouteRequest->GetEndSectionIndex(),
                       lpRouteRequest->GetQuality(),
                       lpRouteRequest->GetDistanceFunction(),
                       lpRouteRequest->UseAIShortcuts());

        const s32 liBlockSectionCount = lpRouteRequest->GetBlockSectionIdCount();
        for (s32 liBlock = 0; liBlock < liBlockSectionCount; ++liBlock)
        {
            mAStar.AddBlockSectionId(lpRouteRequest->GetBlockSectionId(liBlock));
        }
    }

    mAStar.Compute();

    if (!mAStar.IsInProgress())
    {
        RouteMapModuleIO::RouteResponse lRouteResponse;
        Route* lpRoute = lRouteResponse.GetRoute();
        lpRoute->miNodeCount        = 0;
        lpRoute->miDefaultStartNode = 0;
        lpRoute->meStatus           = Route::E_STATUS_UNINITIALISED;
        lRouteResponse.Construct(muOwnerId, muEventId);

        mAStar.BuildRoute(lpRoute);
        lpRoute->Prepare(0);

        lpRouteResponseQueue->AddEvent(lRouteResponse);

        {
            // [FLAG PC witness] NOT IN THE X360 BINARY. Completed race routes, budgeted PER
            // EVENT ID (== the asking car's AICar::miRaceCarIndex) rather than first-8 overall
            // -- see the banner on RouteMapWitnessBudget below.
            // DELETE-WHEN rivals are seen following routes in the log.
            static s32 saiRaceRouteWitness[36] = { 0 };
            static s32 siRaceRouteWitnessTotal = 0;
            if (RouteMapWitnessBudget(saiRaceRouteWitness, siRaceRouteWitnessTotal, muEventId)
                && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[route] race route done: owner " << static_cast<s32>(muOwnerId)
                    << " event " << static_cast<s32>(muEventId)
                    << " status " << static_cast<s32>(lpRoute->GetStatus())
                    << " nodes " << lpRoute->GetNodeCount()
                    << " distance " << lpRoute->GetDistance() << "\n";
            }
        }
    }
}

// ---------------------------------------------------------------------------------------
// ProcessExtrapolatedRoute @0x8278C4C8 (DWARF BrnRouteMapModule.cpp:231).
//
//   r28 = lpRouteRequest, r20 = lpRouteResponseQueue
//   0x8278C4F0  response.muOwnerId = req->muOwnerId (+0), muEventId = req->muEventId (+2);
//               route.miNodeCount = miDefaultStartNode = meStatus = 0
//   0x8278C50C  r30 = mAISectionsData.GetMemoryResource()
//   0x8278C52C  indices.miCount (+0x80) = 16   -- the stack ExtrapolatedIndexArray is FULL-count
//   0x8278C534  v2 = req->mCarPosition (+0x10, r24) ; 0x8278C53C v1 = req->mCarDirection (+0x20, r23)
//   0x8278C540  n = RacingLineGenerator::ExtrapolateRouteBackwards(6, req->muCurrentSectionIndex
//                 (+0x30, `clrlwi r4,..,16` = u16), v1 = direction, v2 = position, sections,
//                 &indices)
//   0x8278C568  for (i = n-1 .. 0): section = u16(indices[i].muSection), portal =
//               u8(indices[i].muPortal); assert section < muNumSections (AISectionsData.h:1230
//               then :1201 inside GetAISection); portal = section.GetPortal(portal);
//               node = { portal.x, portal.z, 0.0f (f31 = flt_82001CC0), u16 section @+0xC,
//               u8 portal @+0xE } ; route.AddNode(node)   (BACKWARDS entries are emitted in
//               reverse so the route runs behind -> ahead)
//   0x8278C64C  liDefaultStart = route.miNodeCount                    (the car's own section)
//   0x8278C650  n = (req->meRouteType (+0x34) == 0) ? ExtrapolateRouteForwards(8, s32 index, ...)
//                                                    : ExtrapolateTwistyRoute(8, ...)
//   0x8278C690  if (n == 0) liDefaultStart -= 1
//   0x8278C6A0  for (i = 0 .. n-1): same AddNode
//   0x8278C788  route.meStatus = E_STATUS_COMPLETE (1)
//   0x8278C78C  if (liDefaultStart >= miNodeCount) liDefaultStart = (miNodeCount > 0) ?
//                                                                  miNodeCount - 1 : 0
//   0x8278C7B0  route.Prepare(liDefaultStart) ; 0x8278C7BC lpRouteResponseQueue->AddEvent()
//
// ⭐ UN-PARKED 2026-09-03 (aiwave A6): the three producers are bodied in
// RacingLine/BrnRacingLineGenerator_Extrapolate.cpp. The previous banner had v1/v2 swapped
// (it read "v1 = mCarPosition"); r24 = req+0x10 feeds v2 and r23 = req+0x20 feeds v1, so v1 is
// the DIRECTION -- the order the DWARF prototypes (direction, position) and the Backwards body
// (which negates v1 into lCarBackwardsDirection) both attest.
//
// The console tail is unconditional: with zero sections generated (Backwards 0 or -1, ahead 0)
// liDefaultStart ends at -1, `-1 >= 0` is false so the clamp is skipped, Route::Prepare(-1) runs
// (its own "Can't default to node" assert only fires for liDefaultStartNode >= liNodeCount inside
// a `liNodeCount > 0` block, so an EMPTY route with -1 is silent and simply records -1), and the
// COMPLETE response with 0 nodes is posted. Reproduced as-is; the consumer (AICar::SetRoute ->
// SetNextRouteNodeIndex) owns what a -1 default start means.
// ---------------------------------------------------------------------------------------
namespace
{
    // 0x8278C568..0x8278C63C and 0x8278C6A0..0x8278C774: one route node per generated
    // (section, portal) pair -- the portal's ground-plane (x, z), distance slot 0.0f, and the
    // section (u16 @+0xC) / portal (u8 @+0xE) packed into the w lane exactly as AStar::BuildRoute
    // packs its nodes (and as AICar::GetCurrentNodeY reads them back).
    void AddExtrapolatedPortalNode(Route* lpRoute, const AISectionsData* lpAISectionsData,
                                   const SectionAndPortalIndices& lrIndices)
    {
        const u16 luSectionIndex = static_cast<u16>(lrIndices.muSection);   // clrlwi ..,16
        const u8  luPortalIndex  = static_cast<u8>(lrIndices.muPortal);     // clrlwi ..,24

        CGS_ASSERT(luSectionIndex < lpAISectionsData->muNumSections,
                   "luSectionIndex < muNumSections");                       // AISectionsData.h:1230
        const AISection* lpSection = lpAISectionsData->GetAISection(luSectionIndex);   // :1201
        const Portal*    lpPortal  = lpSection->GetPortal(luPortalIndex);

        Vector4 lNode;
        lNode.x = lpPortal->GetPositionX();
        lNode.y = lpPortal->GetPositionZ();
        lNode.z = 0.0f;                                                     // f31 = flt_82001CC0
        lNode.w = 0.0f;
        reinterpret_cast<u16*>(&lNode.w)[0] = luSectionIndex;               // sth ..,+0xC
        reinterpret_cast<u8*>(&lNode.w)[2]  = luPortalIndex;                // stb ..,+0xE
        lpRoute->AddNode(lNode);
    }

    // The two literal generation counts (`li r3, 6` @0x8278C528 / `li r3, 8` @0x8278C66C).
    const s32 KI_EXTRAPOLATED_SECTIONS_BEHIND = 6;
    const s32 KI_EXTRAPOLATED_SECTIONS_AHEAD  = 8;
}

void RouteMapModule::ProcessExtrapolatedRoute(
    const RouteMapModuleIO::ExtrapolatedRouteRequest* lpRouteRequest,
    RouteMapModuleIO::RouteResponseQueue* lpRouteResponseQueue)
{
    // [GUARD] the console reads the request unconditionally; NULL is a wiring error.
    if (lpRouteRequest == 0)
    {
        return;
    }

    // 0x8278C4F0..0x8278C508: the response on the stack -- ids from the request, empty route.
    RouteMapModuleIO::RouteResponse lRouteResponse;
    Route* lpRoute = lRouteResponse.GetRoute();
    lpRoute->miNodeCount        = 0;
    lpRoute->miDefaultStartNode = 0;
    lpRoute->meStatus           = Route::E_STATUS_UNINITIALISED;
    lRouteResponse.Construct(lpRouteRequest->GetOwnerId(), lpRouteRequest->GetEventId());

    const AISectionsData* lpAISectionsData = mAISectionsData.GetMemoryResource();

    // [GUARD] the console dereferences the road network unconditionally; no network bound is
    // a Prepare-order error, not a frame condition.
    if (lpAISectionsData == 0)
    {
        return;
    }

    ExtrapolatedIndexArray lGeneratedIndices;
    lGeneratedIndices.SetFullCount();                                       // stw 16, +0x80

    const Vector2 lCarPosition  = lpRouteRequest->GetCarPosition();          // v2
    const Vector2 lCarDirection = lpRouteRequest->GetCarDirection();         // v1

    s32 liNumGenerated = RacingLineGenerator::ExtrapolateRouteBackwards(
        KI_EXTRAPOLATED_SECTIONS_BEHIND,
        static_cast<u16>(lpRouteRequest->GetCurrentSectionIndex()),
        lCarDirection, lCarPosition, lpAISectionsData, lGeneratedIndices);

    for (s32 liIndex = liNumGenerated - 1; liIndex >= 0; --liIndex)
    {
        AddExtrapolatedPortalNode(lpRoute, lpAISectionsData,
                                  lGeneratedIndices[static_cast<u32>(liIndex)]);
    }

    s32 liDefaultStartNode = lpRoute->GetNodeCount();                       // 0x8278C64C

    if (lpRouteRequest->GetRouteType() == RouteMapModuleIO::E_EXTRAPOLATED_NORMAL)
    {
        liNumGenerated = RacingLineGenerator::ExtrapolateRouteForwards(
            KI_EXTRAPOLATED_SECTIONS_AHEAD,
            static_cast<s32>(lpRouteRequest->GetCurrentSectionIndex()),
            lCarDirection, lCarPosition, lpAISectionsData, lGeneratedIndices);
    }
    else
    {
        liNumGenerated = RacingLineGenerator::ExtrapolateTwistyRoute(
            KI_EXTRAPOLATED_SECTIONS_AHEAD,
            static_cast<s32>(lpRouteRequest->GetCurrentSectionIndex()),
            lCarDirection, lCarPosition, lpAISectionsData, lGeneratedIndices);
    }

    if (liNumGenerated == 0)
    {
        liDefaultStartNode -= 1;                                            // 0x8278C690
    }

    for (s32 liIndex = 0; liIndex < liNumGenerated; ++liIndex)
    {
        AddExtrapolatedPortalNode(lpRoute, lpAISectionsData,
                                  lGeneratedIndices[static_cast<u32>(liIndex)]);
    }

    lpRoute->meStatus = Route::E_STATUS_COMPLETE;                           // 0x8278C788

    const s32 liNodeCount = lpRoute->GetNodeCount();
    if (liDefaultStartNode >= liNodeCount)
    {
        liDefaultStartNode = (liNodeCount > 0) ? (liNodeCount - 1) : 0;    // 0x8278C798..0x8278C7A4
    }

    lpRoute->Prepare(liDefaultStartNode);
    lpRouteResponseQueue->AddEvent(lRouteResponse);

    {
        // [FLAG PC witness] NOT IN THE X360 BINARY. Extrapolated responses, budgeted PER EVENT
        // ID (== the asking car's AICar::miRaceCarIndex): the one line that says whether THAT
        // car's road was actually generated (nodes > 0) and where its default start sits.
        // An EMPTY route (0 nodes) is the failure this campaign is hunting, so it draws from
        // its own second budget and stays visible after the first one is spent.
        // DELETE-WHEN rivals are seen driving extrapolated routes.
        static s32 saiExtrapolatedWitness[36] = { 0 };
        static s32 siExtrapolatedWitnessTotal = 0;
        static s32 saiEmptyRouteWitness[36]   = { 0 };
        static s32 siEmptyRouteWitnessTotal   = 0;
        const bool lbEmptyRoute = (liNodeCount <= 0);
        const bool lbWitnessed =
            RouteMapWitnessBudget(saiExtrapolatedWitness, siExtrapolatedWitnessTotal,
                                  lpRouteRequest->GetEventId());
        const bool lbWitnessedEmpty =
            lbEmptyRoute && RouteMapWitnessBudget(saiEmptyRouteWitness, siEmptyRouteWitnessTotal,
                                                  lpRouteRequest->GetEventId());
        if ((lbWitnessed || lbWitnessedEmpty) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[route] extrapolated route done: owner "
                << static_cast<s32>(lpRouteRequest->GetOwnerId())
                << " event " << static_cast<s32>(lpRouteRequest->GetEventId())
                << " section " << static_cast<s32>(lpRouteRequest->GetCurrentSectionIndex())
                << " type " << static_cast<s32>(lpRouteRequest->GetRouteType())
                << " nodes " << liNodeCount
                << " default " << liDefaultStartNode
                // GetDistance() reads maNodes[0].z, which Route::Prepare only writes when
                // liNodeCount > 0 -- on an empty route that lane is untouched stack.
                << " distance " << ((liNodeCount > 0) ? lpRoute->GetDistance() : 0.0f) << "\n";
        }
    }
}
}
