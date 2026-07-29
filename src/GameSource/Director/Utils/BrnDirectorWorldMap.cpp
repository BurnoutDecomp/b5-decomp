// BrnDirectorWorldMap.cpp -- BrnDirector::WorldMap query bodies.
//   GetLanePositionNearestPoint                 @ 0x8221CE98
//   GetLanePositionNearestPointWithDisplacement @ 0x8221D078
//   GetInterestingPointNear                     @ 0x8221D2A0
//   WalkLaneLeft                                @ 0x821F7800
//   GetTrafficData                              @ 0x82219128
//   GetSafePositionNearest                      @ 0x8223B750
//   GetSafePositionNearestPointWithDisplacement @ 0x8223BA78
//
// Store-for-store to the X360 VMX128 bodies. The closeness/direction predicates are the
// broadcast-lane vcmpgtfp / vmsum3fp results, reproduced as scalar tests over the committed
// rw::math::vpu MagnitudeSquared / Dot helpers. The mpTrafficData.operator->() calls (PVS
// lookup, then hull-array read) are preserved as separate dereferences, matching the asm.
// The X360 CalcTransformAtParameter (the unnamed sub_82219030, asserting at
// BrnTrafficSection.h:675) is the LaneRung* overload -- it computes position + direction + up.

#include "GameSource/Director/Utils/BrnDirectorWorldMap.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"    // CgsDev::Log (diagnostics)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"  // RequestInterface<512> (LoadData)
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"        // GetGameDataEvent (the lane replies)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // AcquireResourceResponse
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"   // BrnTraffic::TrafficData (mpPvs, GetHull)
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                // BrnTraffic::Pvs::GetHullIndexForPoint
// BrnTrafficSection.h MUST precede BrnTrafficHull.h: it sets BRNTRAFFIC_SECTION_DEFINED so the
// real 48-byte Section (with GetNumSegments/CalcPositionAtParameter) wins over Hull.h's placeholder.
#include "SharedClasses/Traffic/BrnTrafficSection.h"            // BrnTraffic::Section, Neighbour, Side
#include "SharedClasses/Traffic/BrnTrafficHull.h"               // BrnTraffic::Hull (GetSection, mpaRungs)
#include "SharedClasses/Trigger/BrnTriggerData.h"               // BrnTrigger::TriggerData (GetGenericRegion*)
#include "SharedClasses/Trigger/BrnGenericRegion.h"             // BrnTrigger::GenericRegion (+ TriggerRegion/BoxRegion)
#include "rw/math/vpu/vector3_operation.h"                      // operator-, operator*, Dot, MagnitudeSquared

namespace BrnDirector
{
    // Rung step (the X360 walks luRung as a u8 byte counter += 10 each iteration, and
    // the VMX lfParam accumulator += a broadcast 10.0f = unk_82FAAB20).
    static const u8       KU_RUNG_DISTANCE  = 10;
    static const VecFloat KVF_RUNG_DISTANCE = { 10.0f, 10.0f, 10.0f, 10.0f };

    // Drop the returned "safe" point this far below the lane along the lane's up vector
    // (X360 flt_82009A74 == 2.25, broadcast then vmulfp/vsubfp in GetSafePositionNearest).
    static const f32      KF_SAFE_POSITION_DROP = 2.25f;

    // Below this squared displacement magnitude, the displacement query degenerates to the
    // plain nearest-safe query (X360 flt_82001C98 == 1.0, vcmpgtfp vs |lDisplacement|^2).
    static const f32      KF_MIN_DISPLACEMENT_SQ = 1.0f;

    // LoadData's request constants (the X360 `li r31,1` / `li r10,5` pair at every stage).
    // The event id doubles as the reply's expected miEventId in stages 3 and 5.
    static const s32      KI_DATA_ACQUIRE_REQUEST = 1;
    static const s32      KI_LANE_DATA_POOL_ID    = 5;

    // -------------------------------------------------------------------------
    // Construct -- INLINED by the X360 into DirectorModule::Construct @0x8225C590
    // (see the header for the four attested stores). Bring up the receiver queue every
    // request/response rides and park the state machine at its first stage.
    // -------------------------------------------------------------------------
    void WorldMap::Construct()
    {
        mReceiverQueue.Construct();
        meLoadingState = E_LOADING_STATE_TRIGGERS_NOT_REQUESTED;
    }

    // -------------------------------------------------------------------------
    // LoadData @0x8225F5A0  -- ✅ LIVE (the gate was deleted 2026-07-29; see the body).
    //
    // DirectorModule::Prepare @0x822712D8 pumps this once per prepare tick at its stage 3
    // and will not advance to MainDirector::Prepare until it returns true. The X360 body is
    // a 7-state machine over meLoadingState (a1[62] == +0xF8 in the export, which is exactly
    // this class's meLoadingState -- the offsets in the export are WORD indices, so a1[62] is
    // byte 248):
    //
    //   0 TRIGGERS_NOT_REQUESTED : build a 24-byte GetGameData-family request on the stack
    //       { mpReceiverQueue = &mReceiverQueue, miEventId = 1, miPoolId = 5,
    //         mId = CgsResource::ID::HashString("TriggerData") | 0x5'00000000 }
    //       and VariableEventQueue<512,16>::AddEvent(lpRequestInterface, &ev, /*type*/4,
    //       /*size*/24); -> state 1; return false.
    //   1 LOADING_TRIGGERS       : if the receiver queue is empty return false. Otherwise walk
    //       every queued event, CgsResource::BaseResourcePtr::CreateFromHandle(&mpTriggerData,
    //       &ev->mHandle), then Clear the queue; -> state 2, fall through.
    //   2 TRAFFIC_NOT_REQUESTED  : RequestInterface<512>::LoadTrafficLanes(&mReceiverQueue,
    //       /*eventId*/1, /*poolId*/5); -> state 3, fall through.
    //   3 LOADING_TRAFFIC        : assert GetLength() < 1 unless exactly 1; on the single
    //       response assert its resource id == 55 and its event id == KI_DATA_ACQUIRE_REQUEST
    //       (1), then CreateFromHandle(&mpTrafficData, &ev->mHandle); -> state 6(!), Clear.
    //       [the state-6 store here is the asm's, then it immediately falls into the AI leg]
    //   4 AI_DATA_ACQUIRE_NOT_STARTED : Clear; RequestInterface<512>::GetAILanes(
    //       &mReceiverQueue, 1, 5); -> state 5, fall through.
    //   5 AI_DATA_ACQUIRE_REQUESTED   : if the queue is empty return false; assert lpEvent !=
    //       NULL and its event id == 1, then CreateFromHandle(&mpAISectionData, &ev->mHandle);
    //       -> state 6, fall through.
    //   6 LOADED                 : return true.
    //   default                  : assert "unhandled case" (line 190); return false.
    //
    // ⭐ THE GATE IS DELETED (2026-07-29, second half of the DJ fly-by wave). Everything
    // this banner used to list as a blocker is closed:
    //   1. THE EVENT RECORDS. Stage 0 builds a `CgsResource::Events::AcquireResourceRequest`
    //      (the pool ACQUIRE family: mpUser@0x00, miEventId@0x04, miPoolId@0x08,
    //      mResourceId@0x10, size 24, type 4 -- DWARF CgsResourceIOEvents.h:221/:314, and the
    //      consumer PoolModule::DoAcquireResourceRequest @0x828FCD48 reads exactly that
    //      order). Stages 2/4 build `GameDataIO::GameDataEvent` (miEventId@0x00,
    //      mpReceiverQueue@0x04, size 32) through RequestInterface<512>::LoadTrafficLanes
    //      @0x82256288 / ::GetAILanes @0x822563C0. The 24-vs-32 "conflict" was two structs.
    //   2. The `| (poolId << 48)` Hex-Rays fusion artifact was dropped from
    //      RequestInterface<N>::AcquireResource (HashString @0x828D84A8 ends
    //      `clrldi r3,r11,32`; miPoolId is its own field), so the acquire id resolves.
    //   3. All four module-side handlers are real and both hops are wired
    //      (ProcessLoadTrafficLanesRequest @0x8266F398 -> "B5Traffic.bndl" resp 30 ->
    //      ProcessGetTrafficLanesRequest @0x826703B0 -> "BaseTraffic" resp 55; and the AI
    //      twins @0x8266F4B0 / @0x826704C0 -> "AI.dat" / "WorldMapData"). Names cross-checked
    //      by CRC32-lowercase against the shipped resource ids.
    //   4. The three lane FILES are ported to platform 4 with widened 64-bit pointer slots,
    //      all seven Fix* bodies are real, and all three resource types (65537/65538/65539)
    //      are registered.
    //   5. ⭐ THE FRAME IO BRACKET -- the last switch -- is threaded:
    //      MainGameFlowStateInitialLoadingScreen::Update @0x823EF688 locks the game module's
    //      GameData input/output pair around its stage switch and hands both to
    //      LoadingScriptedState::LoadDirectorModule @0x823E74C0, which appends this state
    //      machine's staged requests into the GameData input every tick Prepare reports
    //      "still preparing"; and BrnGameModule's per-frame resource tick (the console's
    //      ResourceUpdateThread @0x823BC9B8) services them.
    //
    // The replies are read BY MEMBER off the response types, never at the console's literal
    // +0x18/+0x20 -- the host ResourceHandle is 16 bytes where the console's is 8, so every
    // literal offset past it shifts, and the two reads really are different record types.
    // -------------------------------------------------------------------------
    bool WorldMap::LoadData(BrnResource::GameDataIO::RequestInterface<512>* lpRequests)
    {
        // ✅ THE GATE IS GONE (2026-07-29, second half of the DJ fly-by wave). The requests are
        // staged where the console stages them -- the director OUTPUT buffer's own
        // RequestInterface<512> (`OutputBuffer::GetResour()` -> mResourceInterface @+0x2E0),
        // which is now its real type and is Constructed by OutputBuffer::Construct. The other
        // half of the bracket is LoadingScriptedState::LoadDirectorModule @0x823E74C0: on every
        // tick DirectorModule::Prepare reports "still preparing" it read-locks this output
        // buffer and runs `GameDataIO::InputBuffer::AppendRequestInterface<512>` into the
        // frame's GameData input, which the resource pump then services. Both halves are now
        // real, so nothing here needs a PC leaf.
        //
        // The caller (DirectorModule::Prepare @0x822712D8) holds the output buffer's WRITE
        // lock for the whole call, which is exactly what GetResour() asserted before handing
        // this interface over -- so no locking happens here, matching the asm.
        CGS_ASSERT(lpRequests != 0, "lpRequestInterface != NULL");

        // [diagnostic, one-shot] which of the three lane resources actually resolved.
        // Delete with the rest of the lane bring-up diagnostics.
        static bool sbTriggerResolved = false;
        static bool sbTrafficResolved = false;

        switch (meLoadingState)
        {
        case E_LOADING_STATE_TRIGGERS_NOT_REQUESTED:
            // AcquireResource IS the "get trigger data" builder -- no GetTriggerData symbol
            // exists on the X360 either. Event id 1 == KI_DATA_ACQUIRE_REQUEST, pool 5.
            lpRequests->AcquireResource(&mReceiverQueue, KI_DATA_ACQUIRE_REQUEST,
                                        KI_LANE_DATA_POOL_ID, "TriggerData");
            meLoadingState = E_LOADING_STATE_LOADING_TRIGGERS;
            return false;

        case E_LOADING_STATE_LOADING_TRIGGERS:
        {
            // Drain the acquire reply. The response is an AcquireResourceResponse, whose
            // {mpResourceMemory, mpSourceEntry} pair IS a ResourceHandle -- read BY MEMBER,
            // not at the console's literal record offset (the host handle is 16 bytes where
            // the console's is 8, so every literal offset past it shifts).
            // ⚠️ GetFirstEvent returns the event TYPE (-1 at end), NOT a bool -- the console's
            // own "is the reply here yet" test is the COUNT (its baked assert text is
            // "mReceiverQueue.GetLength() == 1"). Testing the return value as a bool is
            // exactly backwards: an empty queue answers -1, which is truthy.
            if (mReceiverQueue.GetCount() < 1)
                return false;

            const CgsModule::Event* lpEvent = nullptr;
            s32                     liSize  = 0;
            mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

            while (lpEvent != nullptr)
            {
                // reinterpret_cast, not static_cast: CgsResource::Events::Event and
                // CgsModule::Event are unrelated roots, and the receiver queue hands out the
                // module one. Same idiom as BrnGuiColourCalibrationScreen.cpp:137 and
                // BrnGameDataModule.cpp:417.
                const CgsResource::Events::AcquireResourceResponse* lpResponse =
                    reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);

                CgsResource::ResourceHandle lHandle;
                lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
                lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
                mpTriggerData = lHandle;
                sbTriggerResolved = (lHandle.mpResourceMemory != 0);

                const CgsModule::Event* lpNext = nullptr;
                mReceiverQueue.GetNextEvent(lpEvent, &lpNext, &liSize);
                lpEvent = lpNext;
            }
            mReceiverQueue.Clear();
            meLoadingState = E_LOADING_STATE_TRAFFIC_NOT_REQUESTED;
        }
        // fall through -- the X360 drops straight into the traffic request on the same tick.

        case E_LOADING_STATE_TRAFFIC_NOT_REQUESTED:
            lpRequests->LoadTrafficLanes(&mReceiverQueue, KI_DATA_ACQUIRE_REQUEST,
                                         KI_LANE_DATA_POOL_ID);
            meLoadingState = E_LOADING_STATE_LOADING_TRAFFIC;
            return false;

        case E_LOADING_STATE_LOADING_TRAFFIC:
        {
            // Count first -- GetFirstEvent's return is the event TYPE, not a success flag.
            if (mReceiverQueue.GetCount() < 1)
                return false;

            const CgsModule::Event* lpEvent = nullptr;
            s32                     liSize  = 0;
            mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

            CGS_ASSERT(lpEvent != nullptr, "lpEvent != NULL");
            const BrnResource::GameDataIO::GetGameDataEvent* lpResponse =
                static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>(lpEvent);
            CGS_ASSERT(lpResponse->miEventId == KI_DATA_ACQUIRE_REQUEST,
                       "lpResponse->miEventId == KI_DATA_ACQUIRE_REQUEST");
            mpTrafficData = lpResponse->mHandle;
            sbTrafficResolved = (lpResponse->mHandle.mpResourceMemory != 0);

            mReceiverQueue.Clear();
            meLoadingState = E_AI_DATA_ACQUIRE_NOT_STARTED;
        }
        // fall through.

        case E_AI_DATA_ACQUIRE_NOT_STARTED:
            mReceiverQueue.Clear();
            lpRequests->GetAILanes(&mReceiverQueue, KI_DATA_ACQUIRE_REQUEST,
                                   KI_LANE_DATA_POOL_ID);
            meLoadingState = E_AI_DATA_ACQUIRE_REQUESTED;
            return false;

        case E_AI_DATA_ACQUIRE_REQUESTED:
        {
            // Count first -- GetFirstEvent's return is the event TYPE, not a success flag.
            if (mReceiverQueue.GetCount() < 1)
                return false;

            const CgsModule::Event* lpEvent = nullptr;
            s32                     liSize  = 0;
            mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

            CGS_ASSERT(lpEvent != nullptr, "lpEvent != NULL");
            const BrnResource::GameDataIO::GetGameDataEvent* lpResponse =
                static_cast<const BrnResource::GameDataIO::GetGameDataEvent*>(lpEvent);
            CGS_ASSERT(lpResponse->miEventId == KI_DATA_ACQUIRE_REQUEST,
                       "lpResponse->miEventId == KI_DATA_ACQUIRE_REQUEST");
            mpAISectionData = lpResponse->mHandle;

            mReceiverQueue.Clear();
            meLoadingState = E_LOADING_STATE_LOADED;

            // [diagnostic, one-shot] report which of the three lane resources actually
            // resolved. Delete with the rest of the lane bring-up diagnostics.
            if (CgsDev::Message::gxMessageFilterFlags & 1)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[WorldMap] LOADED -- traffic=" << (sbTrafficResolved ? 1 : 0)
                    << " trigger=" << (sbTriggerResolved ? 1 : 0)
                    << " ai=" << (lpResponse->mHandle.mpResourceMemory != 0 ? 1 : 0) << "\n";
            }
        }
        // fall through.

        case E_LOADING_STATE_LOADED:
            return true;

        default:
            CGS_ASSERT(false, "unhandled case");
            return false;
        }
    }

    // BrnDirectorWorldMap.h:82 / body @0x8221CE98.
    // Sample the lane rungs of the traffic hull containing lPosition and return the rung
    // point closest to lPosition in squared distance (no direction bias -- the plain sibling
    // of GetLanePositionNearestPointWithDisplacement). Sections are sub-sampled with a stride
    // of max(numSections/50, 1); rungs are stepped KU_RUNG_DISTANCE at a time.
    WorldMap::LanePosition WorldMap::GetLanePositionNearestPoint(Vector3 lPosition) const
    {
        LanePosition lNearest;
        lNearest.mfSquaredDistance = 100000.0f;   // stfs 0x10 (flt_820080E8)
        lNearest.mbValid           = false;       // stb  0x1E

        // hullIndex = pvs->GetHullIndexForPoint(lPosition); pvs == trafficData->mpPvs (+8).
        lNearest.muHullIndex = reinterpret_cast<const BrnTraffic::Pvs*>(
            mpTrafficData.operator->()->mpPvs)->GetHullIndexForPoint(lPosition);

        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        if (lpHull->muNumSections != 0)
        {
            u8 luSection = 0;
            do
            {
                const BrnTraffic::Section* lpSection = lpHull->GetSection(luSection);

                u8 luRung = 0;
                while (luRung < lpSection->GetNumSegments())
                {
                    // The X360 broadcasts (float)luRung into the parameter lane each iteration.
                    const f32      lfParam = static_cast<f32>(luRung);
                    const VecFloat lParam  = { lfParam, lfParam, lfParam, lfParam };

                    Vector3 lProspectivePosition;
                    lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lParam, luRung,
                                                       lProspectivePosition);

                    const f32 lfSqDistance =
                        rw::math::vpu::MagnitudeSquared(lPosition - lProspectivePosition);

                    if (lfSqDistance < lNearest.mfSquaredDistance)
                    {
                        lNearest.mfSquaredDistance   = lfSqDistance;
                        lNearest.muSection           = luSection;
                        lNearest.mfParamAlongSection = lfParam;
                        lNearest.muRung              = luRung;
                        lNearest.mPosition           = lProspectivePosition;
                        lNearest.mbValid             = true;
                    }

                    luRung = static_cast<u8>(luRung + KU_RUNG_DISTANCE);
                }

                u32 luStep = lpHull->muNumSections / 50u;
                if (static_cast<s32>(luStep) <= 1)
                    luStep = 1;
                luSection = static_cast<u8>(luSection + luStep);
            }
            while (luSection < lpHull->muNumSections);
        }

        return lNearest;
    }

    // BrnDirectorWorldMap.h:87 / body @0x8221D078.
    // Sample the lane rungs of the traffic hull containing lPosition and return the
    // rung point that is (a) closest to lPosition in squared distance and (b) ahead
    // of lPosition in the lDisplacement direction (Dot(lDisplacement, point-lPosition)
    // > 0). Sections are sub-sampled with a stride of max(numSections/50, 1); rungs
    // are stepped KU_RUNG_DISTANCE at a time.
    WorldMap::LanePosition WorldMap::GetLanePositionNearestPointWithDisplacement(
        Vector3 lPosition, Vector3 lDisplacement) const
    {
        LanePosition lNearest;
        lNearest.mfSquaredDistance = 100000.0f;   // stfs 0x10
        lNearest.mbValid           = false;       // stb  0x1E

        // hullIndex = pvs->GetHullIndexForPoint(lPosition); pvs == trafficData->mpPvs (+8).
        lNearest.muHullIndex = reinterpret_cast<const BrnTraffic::Pvs*>(
            mpTrafficData.operator->()->mpPvs)->GetHullIndexForPoint(lPosition);

        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        VecFloat lNearestDistance = { 100000.0f, 100000.0f, 100000.0f, 100000.0f };

        if (lpHull->muNumSections != 0)
        {
            u8 luSection = 0;
            do
            {
                const BrnTraffic::Section* lpSection = lpHull->GetSection(luSection);

                VecFloat lRung  = { 0.0f, 0.0f, 0.0f, 0.0f };
                u8       luRung = 0;
                while (luRung < lpSection->GetNumSegments())
                {
                    Vector3 lProspectivePosition;
                    lpSection->CalcPositionAtParameter(lpHull->mpaRungs, lRung, luRung,
                                                       lProspectivePosition);

                    const f32 lfSqDistance =
                        rw::math::vpu::MagnitudeSquared(lPosition - lProspectivePosition);

                    if (lNearestDistance.x > lfSqDistance)
                    {
                        if (rw::math::vpu::Dot(lDisplacement,
                                               lProspectivePosition - lPosition) > 0.0f)
                        {
                            lNearestDistance.x           = lfSqDistance;
                            lNearest.mPosition           = lProspectivePosition;
                            lNearest.muSection           = luSection;
                            lNearest.muRung              = luRung;
                            lNearest.mbValid             = true;
                            lNearest.mfParamAlongSection = static_cast<f32>(luRung);
                        }
                    }

                    lRung.x += KVF_RUNG_DISTANCE.x;
                    lRung.y += KVF_RUNG_DISTANCE.y;
                    lRung.z += KVF_RUNG_DISTANCE.z;
                    lRung.w += KVF_RUNG_DISTANCE.w;
                    luRung = static_cast<u8>(luRung + KU_RUNG_DISTANCE);
                }

                u32 luStep = lpHull->muNumSections / 50u;
                if (static_cast<s32>(luStep) <= 1)
                    luStep = 1;
                luSection = static_cast<u8>(luSection + luStep);
            }
            while (luSection < lpHull->muNumSections);
        }

        lNearest.mfSquaredDistance = lNearestDistance.x;
        return lNearest;
    }

    // BrnDirectorWorldMap.h:90 / body @0x82219128.
    // Assert the world map has finished loading, then hand back the main-memory
    // TrafficData resource. mpTrafficData is the first member, so GetMemoryResource
    // is called on `this`+0 in the X360.
    const BrnTraffic::TrafficData* WorldMap::GetTrafficData() const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");
        return mpTrafficData.GetMemoryResource();
    }

    // BrnDirectorWorldMap.h -- the two hull-array queries the lane walkers use. Both take the
    // same LOADED assert the console re-emits at each inlined site (asm 0x8222AC50 fires it at
    // BrnDirectorWorldMap.h:96 for the count query and :93 for the hull fetch).
    const BrnTraffic::Hull* WorldMap::GetTrafficHullData(u32 luHull) const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");
        return mpTrafficData.GetMemoryResource()->GetHull(luHull);
    }

    u32 WorldMap::GetNumTrafficHulls() const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");
        return mpTrafficData.GetMemoryResource()->muNumHulls;
    }

    // BrnDirectorWorldMap.h:121 / body @0x821F7800.
    // Follow left-lane neighbours across hull-section joins. Each hop rewrites the current
    // (section, rung, parameter) into the neighbour section's frame -- new = theirStartRung +
    // (old - ourStartRung) -- and stops when the section has no further left neighbour for the
    // current rung (FindNeighbourForRung returns 0xFFFF).
    void WorldMap::WalkLaneLeft(const BrnTraffic::Hull* lpHull, u8* lpu8Section,
                                u8* lpu8Rung, f32* lpfParameterOnSection) const
    {
        const BrnTraffic::Section* lpSection = lpHull->GetSection(*lpu8Section);

        while (true)
        {
            // fctidz: the rung index is the parameter truncated toward zero.
            const u16 luNeighbour = lpSection->FindNeighbourForRung(
                static_cast<u32>(*lpfParameterOnSection), BrnTraffic::E_LEFT, lpHull);
            if (luNeighbour == 0xFFFFu)
                break;

            CGS_ASSERT(lpHull->mpaNeighbourData != nullptr, "mpaNeighbourData");
            CGS_ASSERT(luNeighbour < lpHull->muNumNeighbours, "luIndex < muNumNeighbours");
            const BrnTraffic::Neighbour* lpNeighbour = &lpHull->mpaNeighbourData[luNeighbour];

            // Cross into the neighbour section (GetSection asserts luIndex < muNumSections).
            *lpu8Section = lpNeighbour->muSection;
            lpSection    = lpHull->GetSection(*lpu8Section);

            // Rung, in the neighbour's frame (two stores, matching the two stb in the asm).
            *lpu8Rung = static_cast<u8>(*lpu8Rung - lpNeighbour->muOurStartRung);
            *lpu8Rung = static_cast<u8>(lpNeighbour->muTheirStartRung + *lpu8Rung);
            CGS_ASSERT(*lpu8Rung <= lpSection->muNumRungs,
                       "*lpu8Rung <= lpSection->muNumRungs");

            // Parameter, likewise (two stfs).
            *lpfParameterOnSection = *lpfParameterOnSection
                                   - static_cast<f32>(lpNeighbour->muOurStartRung);
            *lpfParameterOnSection = static_cast<f32>(lpNeighbour->muTheirStartRung)
                                   + *lpfParameterOnSection;
            CGS_ASSERT(*lpfParameterOnSection <= static_cast<f32>(lpSection->muNumRungs),
                       "*lpfParameterOnSection <= (float32_t)lpSection->muNumRungs");
        }
    }

    // BrnDirectorWorldMap.h:109 / body @0x8221D2A0.
    // Scan the trigger data's generic regions for a Picture-Paradise region (type 18) whose
    // box centre is within lfRadius of lNearTo. On the first hit, write the region box's
    // dimensions to lpExtentsOut and its world transform to lpTransformOut, and return true.
    bool WorldMap::GetInterestingPointNear(Vector3 lNearTo, f32 lfRadius,
                                           Vector3* lpExtentsOut,
                                           Matrix44Affine* lpTransformOut) const
    {
        CGS_ASSERT(lpExtentsOut  != nullptr, "lpExtentsOut != NULL");
        CGS_ASSERT(lpTransformOut != nullptr, "lpTransformOut != NULL");

        const s32 liGenericRegionCount =
            mpTriggerData.operator->()->GetGenericRegionCount();
        const f32 lfSquaredRadius = lfRadius * lfRadius;   // fmuls f31,f31, broadcast v127

        for (s32 liLoop = 0; liLoop < liGenericRegionCount; ++liLoop)
        {
            const BrnTrigger::GenericRegion* lpRegion =
                mpTriggerData.operator->()->GetGenericRegion(liLoop);
            if (lpRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_PICTURE_PARADISE)
                continue;

            const Vector3 lPosition = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                          ->GetBoxRegion()->GetPosition();
            if (rw::math::vpu::MagnitudeSquared(lPosition - lNearTo) <= lfSquaredRadius)
            {
                *lpExtentsOut = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                    ->GetBoxRegion()->GetDimensions();
                *lpTransformOut = mpTriggerData.operator->()->GetGenericRegion(liLoop)
                                      ->GetBoxRegion()->ComputeTransform();
                return true;
            }
        }

        return false;
    }

    // BrnDirectorWorldMap.h:61 / body @0x8223B750.
    // Nearest safe camera position to lPosition. Find the lane rung nearest lPosition, then
    // walk that section forward or backward (depending on which way the lane tangent points
    // relative to lPosition) to the rung whose lane frame straddles lPosition, cross to the
    // leftmost lane, and drop the result KF_SAFE_POSITION_DROP below the lane along its up.
    bool WorldMap::GetSafePositionNearest(Vector3 lPosition, Vector3& lOut) const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");

        lOut = lPosition;   // stvx128 v127 -> lOut (initial store, overwritten below)

        LanePosition lNearest = GetLanePositionNearestPoint(lPosition);
        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        Vector3 lResult = lNearest.mPosition;   // used verbatim when the rung is not valid

        if (lNearest.mbValid)
        {
            const BrnTraffic::Section* lpSection = lpHull->GetSection(lNearest.muSection);

            Vector3 lUp = { 0.0f, 0.0f, 0.0f, 0.0f };   // v55 (up) zeroed
            Vector3 lDirection;

            const VecFloat lParam0 = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam0, lNearest.muRung,
                                                lDirection);

            if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) > 0.0f)
            {
                // Tangent points "behind" lPosition -> advance to later rungs.
                while (static_cast<u32>(lNearest.muRung + 1) < lpSection->GetNumSegments())
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung + 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) <= 0.0f)
                        break;
                }
            }
            else
            {
                // Tangent points "ahead" of lPosition -> step back to earlier rungs.
                while (lNearest.muRung > 1)
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung - 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lPosition - lNearest.mPosition) >= 0.0f)
                        break;
                }
            }

            WalkLaneLeft(lpHull, &lNearest.muSection, &lNearest.muRung,
                         &lNearest.mfParamAlongSection);

            const BrnTraffic::Section* lpFinalSection = lpHull->GetSection(lNearest.muSection);
            const VecFloat lParamF = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpFinalSection->CalcTransformAtParameter(lpHull->mpaRungs, lParamF, lNearest.muRung,
                                                     lNearest.mPosition, lDirection, lUp);

            lResult = lNearest.mPosition - lUp * KF_SAFE_POSITION_DROP;
        }

        lOut = lResult;
        return lNearest.mbValid;
    }

    // BrnDirectorWorldMap.h:67 / body @0x8223BA78.
    // As GetSafePositionNearest, but biases the lane walk by lDisplacement. For a negligible
    // displacement it forwards to GetSafePositionNearest; otherwise the direction test adds the
    // displacement, so the walk settles where the lane frame straddles lPosition + lDisplacement.
    bool WorldMap::GetSafePositionNearestPointWithDisplacement(Vector3 lPosition,
                                                               Vector3 lDisplacement,
                                                               Vector3& lOut) const
    {
        CGS_ASSERT(meLoadingState == E_LOADING_STATE_LOADED,
                   "meLoadingState == E_LOADING_STATE_LOADED");

        lOut = lPosition;   // stvx128 v126 -> lOut (initial store)

        if (rw::math::vpu::MagnitudeSquared(lDisplacement) < KF_MIN_DISPLACEMENT_SQ)
            return GetSafePositionNearest(lPosition, lOut);

        LanePosition lNearest =
            GetLanePositionNearestPointWithDisplacement(lPosition, lDisplacement);
        const BrnTraffic::Hull* lpHull =
            mpTrafficData.operator->()->GetHull(lNearest.muHullIndex);

        Vector3 lResult = lNearest.mPosition;

        if (lNearest.mbValid)
        {
            const BrnTraffic::Section* lpSection = lpHull->GetSection(lNearest.muSection);

            Vector3 lUp = { 0.0f, 0.0f, 0.0f, 0.0f };
            Vector3 lDirection;

            const VecFloat lParam0 = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpSection->CalcDirectionAtParameter(lpHull->mpaRungs, lParam0, lNearest.muRung,
                                                lDirection);

            // vmsum3fp(dir,disp) - vmsum3fp(dir, pos - lPosition) == Dot(dir, disp + lPosition - pos).
            if (rw::math::vpu::Dot(lDirection, lDisplacement)
                    - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) > 0.0f)
            {
                while (static_cast<u32>(lNearest.muRung + 1) < lpSection->GetNumSegments())
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung + 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lDisplacement)
                            - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) <= 0.0f)
                        break;
                }
            }
            else
            {
                while (lNearest.muRung > 1)
                {
                    lNearest.muRung              = static_cast<u8>(lNearest.muRung - 1);
                    lNearest.mfParamAlongSection = static_cast<f32>(lNearest.muRung);

                    const VecFloat lParam = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                              lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
                    lpSection->CalcTransformAtParameter(lpHull->mpaRungs, lParam, lNearest.muRung,
                                                        lNearest.mPosition, lDirection, lUp);

                    if (rw::math::vpu::Dot(lDirection, lDisplacement)
                            - rw::math::vpu::Dot(lDirection, lNearest.mPosition - lPosition) >= 0.0f)
                        break;
                }
            }

            WalkLaneLeft(lpHull, &lNearest.muSection, &lNearest.muRung,
                         &lNearest.mfParamAlongSection);

            const BrnTraffic::Section* lpFinalSection = lpHull->GetSection(lNearest.muSection);
            const VecFloat lParamF = { lNearest.mfParamAlongSection, lNearest.mfParamAlongSection,
                                       lNearest.mfParamAlongSection, lNearest.mfParamAlongSection };
            lpFinalSection->CalcTransformAtParameter(lpHull->mpaRungs, lParamF, lNearest.muRung,
                                                     lNearest.mPosition, lDirection, lUp);

            lResult = lNearest.mPosition - lUp * KF_SAFE_POSITION_DROP;
        }

        lOut = lResult;
        return lNearest.mbValid;
    }
}
