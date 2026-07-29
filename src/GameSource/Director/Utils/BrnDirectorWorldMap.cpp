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

    // -------------------------------------------------------------------------
    // LoadData @0x8225F5A0  -- ⚠️ DOCUMENTED QUIET GATE (director wave).
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
    // ⭐ WHY GATED -- THE ORIGINAL REASON IS WRONG AND HAS BEEN WITHDRAWN (2026-07-29).
    //
    // This gate used to claim a record-shape conflict: that the X360 builds the stage-0
    // request as {queue@0x00, eventId@0x04, poolId@0x08, id@0x10} at size 24, whereas the
    // committed BrnResource::GameDataIO::GameDataEvent
    // (GameSource/Resource/SharedIO/BrnGameDataEvents.h) has the first two fields the other way
    // round and is 32 bytes -- "one of the two is wrong".
    //
    // NEITHER IS WRONG. They are TWO DIFFERENT EVENT TYPES:
    //   * the stage-0 record is `CgsResource::Events::AcquireResourceRequest` (the pool-module
    //     ACQUIRE family), which really does put the receiver queue FIRST -- DWARF
    //     references/DecFIGS/dwarfdump/GameShared/.../CgsResourceIOEvents.h:221 (`PoolEvent`:
    //     mpUser, miEventId, miPoolId) + :314 (`AcquireResourceRequest`: mResourceId).
    //     Console size = 4+4+4+pad+8 = 24. Proof in LoadData's own asm @0x8225F5A0:
    //       0x8225F60C addi r10,r30,0x60 / 0x8225F620 stw r10,var_60   -> +0x00 = &mReceiverQueue
    //       0x8225F614 li r31,1          / 0x8225F634 stw r31,var_5C   -> +0x04 = eventId 1
    //       0x8225F624 li r10,5          / 0x8225F638 stw r10,var_58   -> +0x08 = poolId 5
    //       0x8225F62C std r11,var_50                                  -> +0x10 = the 64-bit id
    //       0x8225F618 li r6,0x18 / 0x8225F61C li r5,4 / bl AddEvent   -> type 4, size 24
    //     The consumer agrees: PoolModule::DoAcquireResourceRequest @0x828FCD48 reads
    //     0/4/8/0x10 in exactly that order. Three other sites build the identical record
    //     (StreetManager::LoadDistrictMap @0x8234FBE8, ColourCalibrationScreen::Update
    //     @0x8246AA9C, WorldModule @0x827D11D8).
    //   * `GameDataIO::GameDataEvent` is the GAMEDATA family and is CORRECT as committed
    //     (miEventId@0x00, mpReceiverQueue@0x04). Attested by the DecFIGS DWARF's member order
    //     for BrnGameDataEvents.h, by RequestInterface<512>::LoadTrafficLanes @0x82256288's own
    //     stores (eventId to +0x00, queue to +0x04, poolId +0x08, id +0x10, type +0x18), by
    //     ::GetAILanes @0x822563C0, and by the module-side readers
    //     (ProcessLoadWorldUnitRequest @0x8266F600/0x8266F608, ProcessLoadPVSRequest
    //     @0x8266F9E0). Its 32-byte size is pinned by AddEvent<LoadGameDataEvent> @0x82252EB8
    //     (`li r6,0x20`).
    // So the 24-vs-32 difference was never a contradiction -- it was two structs.
    //
    // WHAT STILL GATES IT (the honest, much smaller list):
    //   1. the stage-0 builder to call is `RequestInterface<N>::AcquireResource`
    //      (GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h:153, bodied in
    //      BrnGameDataRequestQueueImpl.h:303) -- i.e. exactly
    //          lpRequestInterface->AcquireResource(&mReceiverQueue, 1, 5, "TriggerData")
    //      -- but that builder currently ORs `(u64)poolId << 48` into the resource id. That OR
    //      is a HEX-RAYS FUSION ARTIFACT of the interleaved `li r10,<poolId>` / `std` pair, not
    //      a real store: CgsResource::ID::HashString @0x828D84A8 ends `clrldi r3,r11,32`, a
    //      zero-extended 32-bit CRC, and no attested site ORs anything into it. (The same
    //      artifact is already called out independently at
    //      GameSource/World/BrnWorldModule.cpp:191-194.) Issuing a request with a poisoned id
    //      would simply never resolve. The builder also carries an `mbCheckRefCount` member
    //      that lies outside the console's 24-byte record.
    //   2. stage 1/3/5 must read the reply BY MEMBER off the response type, not at the
    //      console's literal +0x18/+0x20 -- the host ResourceHandle is 16 bytes where the
    //      console's is 8, so every literal offset in the asm walk-through above shifts.
    //   3. whether the PC data set even carries TriggerData / the traffic-lane and AI-lane
    //      bundles in converted form is unverified; a request for an absent resource parks the
    //      state machine at stage 1 for ever (it returns false while the receiver queue is
    //      empty), which would wedge DirectorModule::Prepare.
    // The two other committed builders this function needs
    // (RequestInterface<512>::LoadTrafficLanes @0x82256288 / ::GetAILanes @0x822563C0) DO
    // exist and are ready.
    //
    // GATE BEHAVIOUR (deliberately non-lying, non-trapping):
    //   * returns TRUE so DirectorModule::Prepare completes and the director comes up, and
    //   * leaves meLoadingState UNTOUCHED at its constructed value -- it does NOT claim
    //     E_LOADING_STATE_LOADED. So GetTrafficData()'s own LOADED gate keeps returning null,
    //     which is the truthful "the director has no world map data yet" answer, and every
    //     WorldMap consumer (PositionFinder / BehaviourRoadRunner / the crash-nav states) sees
    //     that instead of a fabricated resource. All of those consumers currently sit behind
    //     the gated MainDirector::Update middle, so none of them runs this wave.
    //
    // DELETE-WHEN (revised 2026-07-29 -- the record-shape blocker is WITHDRAWN, see above):
    //   (a) drop the `| (poolId << 48)` fusion artifact and the `mbCheckRefCount` member from
    //       RequestInterface<N>::AcquireResource (BrnGameDataRequestQueueImpl.h:303-323 and
    //       CgsResourceIOEvents.h:202) -- note the same artifact also sits in
    //       BrnGameStateStreetManager_wB_01.cpp:137-141 and BrnGuiProfile.cpp:1498-1502, so it
    //       wants one coordinated pass, and confirming CgsResource::Pool::FindResource's id
    //       comparison WIDTH decides whether those sites are actively broken or merely unfaithful;
    //   (b) ⭐ RESOLVED 2026-07-29, AND IT MOVED TO THE FRONT OF THE QUEUE: the data FILES are
    //       all present in build/game -- B5TRAFFIC.BNDL (1,270,144 B; bnd2 v2, one zlib
    //       resource id 0xC43359DA that unpacks to 0x2AC100), AI.DAT (1,659,648 B) and
    //       TRIGGERS.DAT (166,400 B) -- but NOTHING EVER OPENS THEM. The PC dispatchers in
    //       GameSource/Resource/BrnGameDataModule.cpp:1310-1313 route the LANE and AI__ ids
    //       into DeferredGameDataRequest (:1255-1260), which logs, frees the event slot and
    //       posts NO RESPONSE. So the acquire never completes.
    //       ⚠️ CONSEQUENCE FOR WHOEVER LIFTS THIS GATE: transcribing LoadData while that is
    //       true WEDGES BOOT. State 2 fires LoadTrafficLanes, state 3 then sees
    //       GetLength() == 0 forever and returns false forever, so mePrepareStage never
    //       reaches 4, MainDirector never prepares, and the loading screen sticks at
    //       "stage 3 (DirectorModule)". That is strictly worse than this `return true`.
    //       DO (b) FIRST, and do it as its own item: body ProcessLoadTrafficLanesRequest
    //       @0x8266F398 + ProcessGetTrafficLanesRequest @0x826703B0 (and the AI pair
    //       @0x8266F4B0 / @0x826704C0) in BrnGameDataModule.cpp. They are low risk -- their
    //       PVS twins right beside them are already bodied and are a line-for-line model --
    //       and they are what actually unblocks everything downstream. Note it is a TWO-HOP
    //       fetch (Load streams the bundle, Get acquires the named resource from the pool;
    //       see the `case 30:` -> "GetTrafficLanes after load" re-dispatch at :1947-1949),
    //       so both halves must land before one handle comes back. Still unquantified: whether
    //       the 2.8 MB payload's Hull/Section/LaneRung/Pvs fixups survive the 4->8-byte host
    //       pointer widening.
    //       (The lane-walk CONSUMER side is already complete and linked: BrnTrafficData.cpp,
    //       BrnTrafficPvs.cpp, BrnTrafficSection.cpp, BrnTriggerData.cpp and BrnGenericRegion.cpp
    //       are all mounted, and Hull::GetSection/GetNeighbour are inline in BrnTrafficHull.h.
    //       TrafficLaneTruck::Prepare @0x82247A08 is ~20 lines whose ONLY gate is
    //       mLanePosition.mbValid. This is a data problem, not a code problem.)
    //   (c) then transcribe the state machine above, reading each reply by member
    //       (AcquireResourceResponse / GetTrafficLaneDataResponse::mTrafficLaneHandle /
    //       GetAIDataResponse::mAIDataHandle), NOT at the console's literal offsets. Note the
    //       two handle reads are at DIFFERENT record offsets on the console (+0x18 in state 1,
    //       +0x20 in states 3/5) because they are different reply types -- do not "tidy" that
    //       into one offset.
    // No new `GetTriggerData` builder is needed -- AcquireResource IS the one, and no such
    // symbol exists on the X360 either.
    // -------------------------------------------------------------------------
    bool WorldMap::LoadData(void* lpRequestInterface)
    {
        (void)lpRequestInterface;
        return true;
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
