#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAICar.h"                                 // AICar (the array element)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"      // AIModuleResultInterface
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>

// BrnAI::ResetOnTrackManager out-of-line members: Construct (@0x82791A48) and GetAICar
// (@0x82765878), plus the file-scope static perf-mon handles.

namespace BrnAI
{
    // ARTIST 82768908: projection onto a finite 3D portal segment.
    Vector3 ResetOnTrackManager::ComputeNearestPositionInSegment(Vector3 lPosition, Vector3 lStart, Vector3 lEnd)
    {
        using namespace rw::math::vpu;
        const Vector3 segment = lEnd - lStart;
        CGS_ASSERT(!IsZero(segment, 1.5258789e-5f), "!RwMath::IsSimilar( lLineStart, lLineEnd )");
        const Vector3 direction = Normalize(segment);
        const f32 distance = Dot(direction, lPosition - lStart);
        if (distance <= 0.0f) return lStart;
        if (distance * distance >= MagnitudeSquared(segment)) return lEnd;
        return lStart + direction * distance;
    }

    // ARTIST 82778250: choose the pair of opposite footprint edges across the
    // travel direction and average their lengths. The corners are packed XZ pairs.
    f32 ResetOnTrackManager::ComputeAISectionWidth(const AISection* lpSection, Vector2 lDirection)
    {
        const auto* c = lpSection->mpaCorners;
        auto length = [](f32 x, f32 y) { return std::sqrt(x*x + y*y); };
        const f32 ax = c[1].x-c[0].x, ay = c[1].y-c[0].y;
        const f32 bx = c[2].x-c[1].x, by = c[2].y-c[1].y;
        const f32 a = length(ax, ay), b = length(bx, by);
        const f32 da = std::fabs((lDirection.x*ax + lDirection.y*ay) / a);
        const f32 db = std::fabs((lDirection.x*bx + lDirection.y*by) / b);
        return da > db ? 0.5f * (b + length(c[0].x-c[3].x, c[0].y-c[3].y))
                       : 0.5f * (a + length(c[3].x-c[2].x, c[3].y-c[2].y));
    }

    bool ResetOnTrackManager::UpdateResetOnTrackSectionUsingRoute(AICar* lpCar)
    {
        using namespace rw::math::vpu;
        const Route* route = lpCar->GetRoute();
        if (!lpCar->HasValidRoute() || route->GetNodeCount() < 2 || static_cast<s32>(lpCar->meRouteFindingStyle) != 1) return false;
        u16 index = static_cast<u16>(lpCar->miNextRouteNodeIndex);
        if (index == 0) index = 1;
        const RouteNode* previous = route->GetNode(index-1);
        const RouteNode* current = route->GetNode(index);
        const AISection* section = mpAISectionData->GetAISection(current->muSectionIndex);
        if (section->mu8NumPortals < 2 || (section->mx8Flags & 6)) return false;
        const u8 end = static_cast<u8>(current->muPad0x0E);
        u8 start = 255;
        for (u8 p = 0; p < section->mu8NumPortals; ++p)
            if (section->GetPortal(p)->GetLinkSectionIndex() == previous->muSectionIndex) { start = p; break; }
        if (start == 255 || start == end) return false;
        const Vector3 from = section->GetPortal(start)->GetPosition();
        const Vector3 to = section->GetPortal(end)->GetPosition();
        if (lpCar->mbIsPlayer && !(Dot(to-from, lpCar->GetVelocityDirection()) > -std::cos(1.919862151145935f))) return false;
        lpCar->UpdateResetOnTrackSection(static_cast<EResetSpeedType>(0), current->muSectionIndex, start, end);
        return true;
    }

    // ARTIST 82786338: reset pairs may redirect off-road/special sections;
    // ordinary sections need exactly two usable portals.
    void ResetOnTrackManager::UpdateResetOnTrackSectionUsingCurrentSection(AICar* lpCar)
    {
        using namespace rw::math::vpu;
        u16 sectionIndex = lpCar->muBestSectionIndex;
        if (sectionIndex == AICar::KI_INVALID_SECTION_INDEX) return;
        const SectionResetPair* pair = 0;
        bool foundPair = false, usePair = false;
        for (u32 i = 0; i < mpAISectionData->muNumSectionResetPairs; ++i)
        {
            pair = &mpAISectionData->mpaSectionResetPairs[i];
            if (pair->muStartSectionIndex != sectionIndex) continue;
            if (!lpCar->mbIsInResetPairSection || (pair->meResetSpeed >= 16 && pair->meResetSpeed <= 19))
            {
                usePair = true;
                sectionIndex = pair->muResetSectionIndex;
            }
            foundPair = true;
            break;
        }
        lpCar->mbIsInResetPairSection = foundPair;
        const AISection* section = mpAISectionData->GetAISection(sectionIndex);
        u8 first = 0, second = 1;
        u32 count = 0;
        if (!usePair)
        {
            for (u8 p = 0; p < section->mu8NumPortals; ++p)
            {
                const AISection* linked = mpAISectionData->GetAISection(section->GetPortal(p)->GetLinkSectionIndex());
                if (!linked->IsUnsuitableForResetOnTrackLink() || section->IsUnsuitableForResetOnTrackLink() || lpCar->mbIsInShowtime)
                {
                    if (count == 0) first = p; else second = p;
                    ++count;
                }
            }
        }
        if (!usePair && !(count == 2 && (lpCar->mbIsInShowtime || !(section->mx8Flags & 6)))) return;
        const Vector3 from = section->GetPortal(first)->GetPosition();
        const Vector3 to = section->GetPortal(second)->GetPosition();
        CGS_ASSERT(first != second, "luFirstGoodPortalIndex != luSecondGoodPortalIndex");
        const Vector3 direction = usePair ? Normalize(lpCar->GetPosition() - section->GetMiddle()) : lpCar->GetVelocityDirection();
        const EResetSpeedType resetSpeed = static_cast<EResetSpeedType>(usePair ? pair->meResetSpeed : 0);
        if (Dot(to-from, direction) > 0.0f) lpCar->UpdateResetOnTrackSection(resetSpeed, sectionIndex, first, second);
        else lpCar->UpdateResetOnTrackSection(resetSpeed, sectionIndex, second, first);
    }
    // File-scope static perf-mon handles (DWARF BrnResetOnTrackManager.h:349-351;
    // X360 dword_82F3026C / dword_82F30274 / dword_82F30270).
    s32 ResetOnTrackManager::miInitialCoordinatesPM;
    s32 ResetOnTrackManager::muAvoidHNGPM;
    s32 ResetOnTrackManager::muTestLineHNGPM;

    // Never called; pins the request-queue offset from inside the class (offsetof on a private
    // member is only legal within class scope).
    void ResetOnTrackManager::_AssertLayout()
    {
        static_assert(offsetof(ResetOnTrackManager, mResetOnTrackRequestQueue) == 0x000,
                      "ROT queue offset drift");

        // ADDITIVE 2026-09-03 (aiwave A11). The two helper nodes stopped being `u8[0x10]` blobs
        // and became RouteNode -- the DWARF's own type for them (BrnResetOnTrackManager.h:340 /
        // :341). These pins are what make that a RENAME rather than a re-layout: RouteNode is
        // {f32,f32,f32,u16,u16} == 16 bytes at align 4, so it drops into exactly the seats the
        // 1-aligned blobs occupied, immediately after the 16-aligned mRandom.
        //
        // ⚠️ THE PINS ARE RELATIVE, NOT ABSOLUTE, AND THAT IS THE POINT. The banner in
        // BrnResetOnTrackManager.h quotes the CONSOLE's offsets (mHelperNodeNext @0x520,
        // mHelperNodePrev @0x530, the debug component @0x540, footprint 0xDB0); on THIS host the
        // same members sit 0x20 further along (measured: mRandom @0x510, mHelperNodeNext @0x540,
        // sizeof 0xDD0), because mpAISectionData is a ResourcePtr whose two pointers widen 4->8.
        // That gap is pre-existing and correct -- the object is never memcpy'd from console bytes
        // and nothing indexes it by a console stride. Pinning the console numbers here would have
        // pinned a lie; pinning the SPACING catches the only thing that could actually break
        // (a member sneaking in between, or RouteNode's alignment shifting the pair).
        static_assert(sizeof(RouteNode) == 0x10, "RouteNode is the console's 16-byte node");
        static_assert(offsetof(ResetOnTrackManager, mHelperNodePrev) -
                          offsetof(ResetOnTrackManager, mHelperNodeNext) == 0x10,
                      "ROT helper nodes must be adjacent 16-byte RouteNodes");
        static_assert(offsetof(ResetOnTrackManager, mResetOnTrackDebugComponent) -
                          offsetof(ResetOnTrackManager, mHelperNodePrev) == 0x10,
                      "ROT debug component follows mHelperNodePrev immediately");
        static_assert(offsetof(ResetOnTrackManager, mHelperNodeNext) -
                          offsetof(ResetOnTrackManager, mRandom) == 0x30,
                      "ROT helper nodes follow mRandom immediately (no padding, no new member)");
    }

    // =============================================================================================
    // Construct @0x82791A48   -- the ONLY construction site of this object in the whole image is
    // BrnAI::AIModule::Prepare @0x82798070 stage 3.
    //
    // Store-for-store against the X360 body (offsets are this-relative, r30 == this):
    //   0x82791A5C..0x82791C58   the mRandom (+0x4F0) prime: seed 0xC87CD8C91AD0891B, slot 0 =
    //                            1.0f, then seven AddRandomFloatToBuffer draws through the
    //                            0x5851F42D4C957F2D LCG, then ++muOldestBufferIndex. That whole
    //                            unrolled blob IS CgsNumeric::Random::Construct inlined -- the
    //                            committed body in CgsRandom.h is the same sequence.
    //   0x82791C5C  stw r31, 0x230(this)   mResetOnTrackRequestQueue.Clear()   (miCount = 0)
    //   0x82791C60..0x82791C70               mRecentResets.Construct(): mpData = this+0x260,
    //                            capacity 8, position/count/... = 0
    //   0x82791C74  bl CreateFromHandle(this+0x360, lAISectionData+0x14)
    //                            == mpAISectionData = lAISectionData (the committed
    //                            ResourcePtr::operator=(const ResourcePtr&) idiom exactly:
    //                            rebind from the SOURCE's {mpThis, muThreadId} pair)
    //   0x82791C7C  stw r28,  0x380(this)  mpaAICars = lpaAICars
    //   0x82791C84  stw r31,  0x388(this)  miResetCount = 0
    //   0x82791C94  stw -1,   0x384(this)  mePlayerGlobalRaceCarIndex = -1
    //   0x82791C80..0x82791CF4               the embedded ResetOnTrackDebugComponent's own
    //                            Construct(owner), inlined: owner @+0xC, two 16-deep ring
    //                            buffers (@+0x10 -> data +0x30, @+0x530 -> data +0x550), a
    //                            7-byte flag block @+0x858..0x85E, two zero words @+0x850/0x854
    //                            and 60 @+0x860
    //   0x82791CF8  bl CgsDev::DebugComponent::Register(this + 0x540)
    //   0x82791CFC..             the three "ROT, ..." PerfMonCpu::AddMonitor registrations
    //   tail                     the by-value lAISectionData parameter's ~ResourcePtr (the
    //                            intrusive-list unlink + self-link) -- emitted by the compiler
    //                            here, not written out.
    //
    // ⚠️ [FLAG PC boot gate] THE DEBUG COMPONENT BLOCK AND ITS Register ARE PARKED. Its interior
    // is `u8 mResetOnTrackDebugComponent[0x870]` -- no named members, so writing its two ring
    // buffers would mean poking raw offsets into an opaque blob, and Register() links the object
    // into the global debug list where the debug UI walks it every frame. Constructing it by
    // offset arithmetic and then publishing it is exactly [[valid-pointer-invalid-object]]. It is
    // pure debug surface; nothing on the reset-on-track path reads it. Restore it WITH the
    // component's own named layout.
    // ⚠️ [FLAG PC boot gate] the three PerfMonCpu::AddMonitor calls are parked with it -- their
    // handles are only read by the parked bodies (ComputeInitialCoordinates / AvoidObstacles /
    // TestLineHNG), and registering a monitor nothing starts or stops just adds a permanent
    // empty row to the profiler HUD. The static handles keep their -1-less default (0) exactly as
    // the console's .bss does before registration.
    // =============================================================================================
    void ResetOnTrackManager::Construct(CgsResource::ResourcePtr<AISectionsData> lAISectionData,
                                        AICar* lpaAICars)
    {
        mRandom.Construct();

        mResetOnTrackRequestQueue.Clear();
        mRecentResets.Construct();

        mpAISectionData = lAISectionData;

        mpaAICars                  = lpaAICars;
        mePlayerGlobalRaceCarIndex = static_cast<EGlobalRaceCarIndex>(-1);
        miResetCount               = 0;

        // [FLAG PC boot gate] the ResetOnTrackDebugComponent Construct + Register and the three
        // "ROT, ..." perf monitors -- see the banner.
    }

    // X360 0x82765878. Private helper; called by 17 sites (PlayerIsLookingBackwards,
    // ComputeInitialCoordinatesStandard, ResetAwayFromPlayer, ...).
    //
    // Two range asserts (E_GLOBAL_RACE_CAR_INDEX_0 <= index < E_GLOBAL_RACE_CAR_INDEX_COUNT ==
    // 35), then return &mpaAICars[index]: the X360 forms 0x1560*index + mpaAICars
    // (sizeof(AICar) == 0x1560 == 5472). AICar is opaque here (its full layout is another TU's),
    // so the element address is computed by the X360-attested byte stride rather than by
    // pointer subscript on the incomplete type -- the result is the same &mpaAICars[index].
    //
    // ⚠️ CORRECTION 2026-08-26 (aimodule wave) -- THE PREVIOUS NOTE HERE WAS WRONG, AND IT WAS
    // WRONG IN THE EXPENSIVE DIRECTION: it asserted "an x64 AICar is not 0x1560 bytes" and told
    // the next wave to rewrite this as `&mpaAICars[index]`. MEASURED, not reasoned:
    //     sizeof(BrnAI::AICar) == 5472 == 0x1560 on this host, exactly the console stride.
    // The committed BrnAICar.h is an explicitly-padded reproduction of the 32-bit layout whose
    // last member ends at 0x1551, and Vector3's 16-byte alignment rounds the object to 0x1560.
    // So the console constant below is byte-correct here and must NOT be "fixed".
    // ⛔ It is correct BY ACCIDENT OF THAT PAD MODEL, not by construction -- carve a pointer out
    // of one of BrnAICar.h's pads and it stops being true, silently, because every address the
    // wrong pitch produces is still inside the allocation. That is why BrnAICar.h now carries a
    // `static_assert(sizeof(AICar) == 0x1560)`: the compile gate is the tripwire, and if it ever
    // fires THEN this becomes `&mpaAICars[index]`.
    // ⚠️ CORRECTED 2026-08-26 (aicar_reset wave): the note that used to end this block said
    // "AIModule::Prepare passes this manager a NULL array today, so every caller of this helper
    // is unreachable until the array lands". THE ARRAY LANDED -- AIModule::maAICars[35] is a real
    // member and Prepare stage 3 passes it. This helper now returns a real object every call.
    AICar* ResetOnTrackManager::GetAICar(EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        CGS_ASSERT(leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0,
                   "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
        CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

        static const u32 KU_AI_CAR_STRIDE = 0x1560;  // sizeof(AICar) == 5472 (X360-attested)
        return reinterpret_cast<AICar*>(
            reinterpret_cast<u8*>(mpaAICars) + KU_AI_CAR_STRIDE * static_cast<u32>(leGlobalRaceCarIndex));
    }

    // =============================================================================================
    // ⭐⭐⭐ THE PUMP  (aicar_reset wave 2026-08-26)
    // =============================================================================================
    // Three functions -- Update, ProcessResetOnTrackRequest, ComputeResetOnTrack -- plus the one
    // placement strategy the CRASH EXIT actually asks for (ComputeInitialCoordinatesStandard,
    // reset type 1). Together they turn a queued ResetOnTrackRequest into a published
    // ResetOnTrackResult, which is the last thing standing between a wrecked car and
    // ActiveRaceCar::RequestPlaceOnTrack.
    //
    // ⚠⚠ THE PARAGRAPH THIS REPLACES IS 2026-08-26 HISTORY AND ITS HEADLINE IS NOW FALSE.
    // It said "on this build every request resolves to ResetOnTrackResult::E_STATE_FAILURE", and
    // it was true then for two independent reasons that have BOTH since gone away:
    //   * every AICar was E_AI_CAR_STATE_INACTIVE, because StoreDrivenCarData / SortTrafficInto-
    //     AICars / UpdateCars / AICar::Update were all absent. They have landed; the boot log
    //     shows the player car IN_RANGE and the rivals OUT_OF_RANGE, i.e. ACTIVE either way;
    //   * six of the seven placement strategies had no bodies. They have them now (aiwave A11).
    // ⭐ THE FAILURE ARM IS STILL THE CONSOLE'S DESIGNED FALLBACK, not an error path:
    // RaceCarEntityModule::ProcessResetOnTrackResultQueue answers a FAILURE by calling
    // ActiveRaceCar::GetResetCoords -- the car's own four-deep "last places I was on the road"
    // ring -- and that is what recovers a CRASHED car whose AI pose could not be computed. What
    // it cannot do is place a car that has never been on the road: a rival being put on the
    // STARTING GRID has an empty ring, which is why the type-6 strategy had to be real.
    // =============================================================================================

    // ---------------------------------------------------------------------------------------------
    // PushResetOnTrackRequest @0x82783CE8
    //
    //   0x82783D10  for (luIndex = 0; luIndex < mResetOnTrackRequestQueue.GetLength(); ++luIndex)
    //   0x82783D4C    if (queue[luIndex].meGlobalRaceCarIndex == lpRequest->meGlobalRaceCarIndex)
    //   0x82783D6C      queue[luIndex] = *lpRequest      (four `lwz`/`stw` pairs, +0/+4/+8/+0xC)
    //                   return
    //   0x82783D9C  if (luIndex >= 0x23) FireAssert("Overflow in ResetOnTrackManager request
    //                                                queue", BrnResetOnTrackManager.cpp:187)
    //   0x82783DCC  return Array<ResetOnTrackRequest,35>::Append(lpRequest)
    //
    // ⭐⭐⭐ THE SCAN IS A **DE-DUPLICATOR KEYED ON THE GLOBAL RACE CAR INDEX**, AND ITS ABSENCE WAS
    // THE `Array container out of space` OVERFLOW (aiwave A11, 2026-09-03). The whole reset pump is
    // built on RE-SENDING: RaceCarEntityModule::SendResetOnTrackRequests @0x822CE178 deliberately
    // does NOT clear mbToBeResetOnTrack (only the RESULT side does, at ProcessResetOnTrackResult-
    // Queue's `stb r19, 0x90(r31)`), so every unanswered car re-posts its request EVERY FRAME. The
    // manager drains exactly ONE request per frame (Update takes index count-1 and Erases it), so
    // with five rivals waiting the queue grows by four a frame -- and with this scan missing it
    // filled all 35 slots in ~75 frames and fired the container assert. WITH the scan the queue
    // holds at most one entry per car, i.e. at most 35 by construction, which is exactly why the
    // console's own overflow assert below can never fire on a 35-car world.
    // ⇒ the SENDER is console-correct and must NOT be given a "pending" latch; the de-dup lives
    // here. The X360 compares only the FIRST DWORD of the record (`lwz r11, 0(r29)` vs
    // `lwz r10, 0(r3)`), which is ResetOnTrackRequest::meGlobalRaceCarIndex (+0x00), and then
    // overwrites all four words -- so a newer request for the same car REPLACES the older one
    // rather than queueing behind it.
    // ---------------------------------------------------------------------------------------------
    void ResetOnTrackManager::PushResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest* lpRequest)
    {
        CGS_ASSERT(lpRequest != 0, "lpRequest != NULL");
        if (lpRequest == 0)
        {
            return;
        }

        u32 luIndex = 0;
        for (; luIndex < mResetOnTrackRequestQueue.GetLength(); ++luIndex)
        {
            if (mResetOnTrackRequestQueue[luIndex].GetGlobalRaceCarIndex() ==
                lpRequest->GetGlobalRaceCarIndex())
            {
                mResetOnTrackRequestQueue[luIndex] = *lpRequest;
                return;
            }
        }

        CGS_ASSERT(luIndex < 0x23u, "Overflow in ResetOnTrackManager request queue");   // :187

        mResetOnTrackRequestQueue.Append(*lpRequest);
    }

    // ARTIST 82783DD8: project the last good position onto the stored recovery
    // section, apply its authored lateral bias, and reject distant stale sections.
    bool ResetOnTrackManager::ComputeInitialCoordinatesStandard(ResetOnTrackCoords* lpOutCoords,
                                                               EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        CGS_ASSERT(lpOutCoords != 0, "lpOutCoords != NULL");

        const AICar* lpAICar = GetAICar(leGlobalRaceCarIndex);

        // The console's own two gates, in its own order.
        if (!lpAICar->IsActive())
        {
            return false;
        }

        if (lpAICar->muResetOnTrackSectionIndex == AICar::KI_INVALID_SECTION_INDEX) return false;
        using namespace rw::math::vpu;
        const AISection* section = mpAISectionData->GetAISection(lpAICar->muResetOnTrackSectionIndex);
        const Vector3 start = section->GetPortal(lpAICar->muResetOnTrackStartPortal)->GetPosition();
        const Vector3 end = section->GetPortal(lpAICar->muResetOnTrackEndPortal)->GetPosition();
        CGS_ASSERT(!IsZero(start-end, 1.5258789e-5f), "!RwMath::IsSimilar( lStartPortalPosition, lEndPortalPosition )");
        const Vector3 direction = Normalize(end-start);
        CGS_ASSERT(IsValid(direction), "RwMath::IsValid( lDirection )");
        Vector3 position = ComputeNearestPositionInSegment(lpAICar->GetLastGoodPosition(), start, end);
        if (section->mx8Flags & 8)
        {
            const f32 width = ComputeAISectionWidth(section, Vector2{direction.x,direction.z,0,0});
            position = position + Cross(direction, Vector3{0,1,0,0}) * (0.3f * width);
        }
        const Vector3 delta = lpAICar->GetLastGoodPosition() - position;
        if (delta.x*delta.x + delta.z*delta.z > 40000.0f) return false;
        lpOutCoords->mpAISection = section;
        lpOutCoords->mPosition = position;
        lpOutCoords->mDirection = direction;
        return true;
    }

    // ---------------------------------------------------------------------------------------------
    // ComputeResetOnTrack @0x82797D78
    //
    //   0x82797D90  assert(lpRequest != NULL)                (BrnResetOnTrackManager.cpp:383)
    //   0x82797DA0  StartMonitor(miInitialCoordinatesPM)
    //   0x82797DB4  switch (lpRequest->GetResetType())
    //       1  ComputeInitialCoordinatesStandard(out, lpRequest->GetGlobalRaceCarIndex())
    //       2  ResetFixedDistanceBehindPlayer(out, lpRequest->GetResetDistance())
    //       3  PlayerIsLookingBackwards() ? ResetAheadFromSideTurnings(out)
    //                                     : ResetFixedDistanceBehindPlayer(out, distance)
    //       4  ResetFixedDistanceAheadOfPlayer(out, distance)
    //       5  ResetAheadFromSideTurnings(out)
    //       6  ResetFixedDistanceBehindPlayerAtStartOfRace(out, distance)
    //       7  ResetAwayFromPlayer(out)
    //       default  FireAssert("Bad reset type used !\n")     (:443)
    //   0x82797E88  StopMonitor
    //   0x82797E94  if (!found) return false
    //               return (type == 5) ? true : AvoidObstacles(lpRequest, out)
    //
    // ⭐ TYPE 1 IS WHAT THE CRASH EXIT SENDS; TYPE 6 IS WHAT THE STARTING GRID SENDS.
    // RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents builds its RequestResetOnTrack with
    // type 1 for the player (type 3 only for an AI car in a game mode with flag 0x80000000), and
    // RaceCarEntityModule::PlaceRaceCarOnLoad @0x822CE588 sends every OPPONENT a type 6 with a
    // negative distance. Both arms are live.
    //
    // ⭐⭐ 2026-09-03 (aiwave A11): THE SIX NON-STANDARD STRATEGIES AND AvoidObstacles ARE REAL.
    // The block that stood here said all seven were parked "over the same absent AI section-data
    // readers"; every one of those readers has since landed (AISectionsData::GetAISection,
    // AISection::GetPortal/PassesThrough, Portal::GetBoundaryLine, BoundaryLine::GetInterp/
    // GetLength, RacingLineGenerator::ExtrapolateRoute*), and BrnAICar.h now names the five AICar
    // members they read. The bodies live in BrnResetOnTrackManager_Strategies.cpp and
    // BrnResetOnTrackManager_AvoidObstacles.cpp. TWO leaves remain parked and BOTH report
    // themselves once: ResetAheadFromSideTurnings (needs ScanForwardsAndAlongJunction) and
    // PlayerIsLookingBackwards (reads mCamera, which nothing fills) -- type 5 is the only reset
    // type that still cannot be answered.
    // [FLAG PC boot gate] the two PerfMonCpu Start/StopMonitor calls -- miInitialCoordinatesPM is
    // never registered (see Construct's flag), so starting a monitor on handle 0 would time an
    // unnamed row.
    // ---------------------------------------------------------------------------------------------
    bool ResetOnTrackManager::ComputeResetOnTrack(ResetOnTrackCoords* lpOutCoords,
                                                  const AIModuleIO::ResetOnTrackRequest* lpRequest)
    {
        CGS_ASSERT(lpRequest != 0, "lpRequest != NULL");   // BrnResetOnTrackManager.cpp:383
        if (lpRequest == 0)
        {
            return false;
        }

        bool lbFoundCoordinates = false;

        switch (lpRequest->GetResetType())
        {
            case E_RESET_TYPE_STANDARD:
            {
                lbFoundCoordinates =
                    ComputeInitialCoordinatesStandard(lpOutCoords,
                                                      lpRequest->GetGlobalRaceCarIndex());
                break;
            }

            case E_RESET_TYPE_BEHIND_PLAYER:
            {
                lbFoundCoordinates =
                    ResetFixedDistanceBehindPlayer(lpOutCoords, lpRequest->GetResetDistance());
                break;
            }

            case E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE:
            {
                // The console latches the distance into f31 BEFORE the branch (0x82797E5C), so the
                // `looking backwards` arm still consumes it -- kept as one expression here.
                if (PlayerIsLookingBackwards())
                {
                    lbFoundCoordinates = ResetAheadFromSideTurnings(lpOutCoords);
                }
                else
                {
                    lbFoundCoordinates =
                        ResetFixedDistanceBehindPlayer(lpOutCoords, lpRequest->GetResetDistance());
                }
                break;
            }

            case E_RESET_TYPE_AHEAD_PLAYER_ON_COMING:
            {
                lbFoundCoordinates =
                    ResetFixedDistanceAheadOfPlayer(lpOutCoords, lpRequest->GetResetDistance());
                break;
            }

            case E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE:
            {
                lbFoundCoordinates = ResetAheadFromSideTurnings(lpOutCoords);
                break;
            }

            case E_RESET_TYPE_BEHIND_PLAYER_RACE_START:
            {
                // ⭐ The THIRD argument is the REQUESTING car's global index (X360 0x82797EB0
                // `lwz r6, 0(r25)`), not the player's -- it is what gives each rival its own side
                // of the starting grid. r5 is unset at the call site because f1 burns it.
                lbFoundCoordinates =
                    ResetFixedDistanceBehindPlayerAtStartOfRace(lpOutCoords,
                                                                lpRequest->GetResetDistance(),
                                                                lpRequest->GetGlobalRaceCarIndex());
                break;
            }

            case E_RESET_TYPE_AWAY_FROM_PLAYER:
            {
                lbFoundCoordinates = ResetAwayFromPlayer(lpOutCoords);
                break;
            }

            default:
            {
                CGS_ASSERT(false, "Bad reset type used !\n");   // :443
                lbFoundCoordinates = false;
                break;
            }
        }

        if (!lbFoundCoordinates)
        {
            return false;
        }

        if (lpRequest->GetResetType() == E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE)
        {
            return true;
        }

        // ⭐ 2026-09-03 (aiwave A11): AvoidObstacles @0x827941E0 is REAL now
        // (BrnResetOnTrackManager_AvoidObstacles.cpp). It is not a tidy-up: it carries the
        // console's own "is the AI even modelling this car" gate, so its answer IS
        // ComputeResetOnTrack's answer for six of the seven reset types.
        return AvoidObstacles(lpRequest, lpOutCoords);
    }

    // ---------------------------------------------------------------------------------------------
    // ProcessResetOnTrackRequest @0x82799D38
    //
    //   0x82799D54  if (ComputeResetOnTrack(&coords, lpRequest))
    //   0x82799D68     AICar* car = GetAICar(lpRequest->GetGlobalRaceCarIndex())
    //   0x82799DB4     switch (car-><+0x14D0>)   -- 20 cases picking a DIRECTION OVERRIDE and a
    //                     reset SPEED; the default arm takes lpRequest->GetResetSpeed()
    //   0x82799F5C     result = { coords.mPosition, <direction>, E_STATE_SUCCESS,
    //                             lpRequest->GetGlobalRaceCarIndex(), <speed> }
    //                  AIModuleIO::ResetOnTrackResult queue .AddEvent(&result)
    //   0x82799F80     if (lpRequest->GetResetType() != 1)
    //                     car->{mfWrongWayTime(+0x14F4) = 0, +0x1408 = 0, +0x1400 = 0,
    //                           muBestSectionIndex(+0x1534) = muDefaultSectionIndex(+0x1532) = 0x7FFF}
    //   0x82799FB4     mRecentResets.Push({ coords.mPosition, lfTime })
    //                else
    //   0x82799FD0     result = { 0, 0, E_STATE_FAILURE, lpRequest->GetGlobalRaceCarIndex(),
    //                             lpRequest->GetResetSpeed() }
    //                  AIModuleIO::ResetOnTrackResult queue .AddEvent(&result)
    //   0x8279A000  ++miResetCount                                    (stw  this+904 == 0x388)
    //   0x8279A014  assert(queue->mpEvents != NULL) ; assert(queue->miMaxLength > 0)
    //   0x8279A05C  ResetOnTrackDebugComponent::PushResetInfo(this + 1344, lpRequest, queue)
    //
    // ⭐ THE FAILURE RECORD CARRIES THE REQUEST'S OWN SPEED, NOT ZERO. Its consumer
    // (ProcessResetOnTrackResultQueue) subtracts a constant and clamps at zero before handing it
    // to RequestPlaceOnTrack, so writing a zero here would discard the requested reset speed
    // with nothing to show for it -- the recovered car would be placed stationary.
    //
    // ⛔ [FLAG PC bring-up] THE SUCCESS ARM IS PARKED WHOLE and it is UNREACHABLE (ComputeReset-
    // OnTrack cannot succeed on this build -- see the pump banner). Its 20-case switch reads
    // AICar+0x14D0, which lives in one of BrnAICar.h's explicit pads and has no name yet, and its
    // "not type 1" tail writes four more unnamed members. Landing it would mean minting five
    // members out of raw offsets for code that cannot execute.
    // ⛔ [FLAG PC boot gate] ResetOnTrackDebugComponent::PushResetInfo -- the debug component is
    // `u8[0x870]` here and Construct deliberately never built it (see Construct's flag).
    // ---------------------------------------------------------------------------------------------
    void ResetOnTrackManager::ProcessResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest* lpRequest,
                                                        AIModuleResultInterface* lpResults,
                                                        f32 lfTime)
    {
        CGS_ASSERT(lpRequest != 0, "lpRequest != NULL");
        CGS_ASSERT(lpResults != 0, "lpResults != NULL");
        if (lpRequest == 0 || lpResults == 0)
        {
            return;
        }

        ResetOnTrackCoords lCoords;
        lCoords.mpAISection = 0;
        lCoords.mPosition   = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        lCoords.mDirection  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

        AIModuleIO::ResetOnTrackResult lResult;

        if (ComputeResetOnTrack(&lCoords, lpRequest))
        {
            AICar* car = GetAICar(lpRequest->GetGlobalRaceCarIndex());
            f32 speed = lpRequest->GetResetSpeed();
            // ARTIST 82799D84 jump table; speed constants initialized at 82C692B0/D0.
            const f32 slow = 60.0f * 0.44704f;
            const f32 fast = 140.0f * 0.44704f;
            switch (car->meResetSpeedType)
            {
            case E_RESET_SPEED_TYPE_NONE: case E_RESET_SPEED_TYPE_NONE_AND_IGNORE: speed = 0; break;
            case E_RESET_SPEED_TYPE_SLOW: speed = slow; break;
            case E_RESET_SPEED_TYPE_FAST: speed = fast; break;
            case E_RESET_SPEED_TYPE_SLOW_NORTH_FACE: speed = slow; lCoords.mDirection = {0,0,-1,0}; break;
            case E_RESET_SPEED_TYPE_SLOW_SOUTH_FACE: speed = slow; lCoords.mDirection = {0,0,1,0}; break;
            case E_RESET_SPEED_TYPE_SLOW_EAST_FACE: speed = slow; lCoords.mDirection = {-1,0,0,0}; break;
            case E_RESET_SPEED_TYPE_SLOW_WEST_FACE: speed = slow; lCoords.mDirection = {1,0,0,0}; break;
            case E_RESET_SPEED_TYPE_SLOW_REVERSE: case E_RESET_SPEED_TYPE_REVERSE_AND_IGNORE_SLOW:
                speed = slow; lCoords.mDirection = rw::math::vpu::Negate(lCoords.mDirection); break;
            case E_RESET_SPEED_TYPE_STOP_REVERSE: case E_RESET_SPEED_TYPE_REVERSE_AND_IGNORE:
                speed = 0; lCoords.mDirection = rw::math::vpu::Negate(lCoords.mDirection); break;
            case E_RESET_SPEED_TYPE_STOP_NORTH_FACE: speed = 0; lCoords.mDirection = {0,0,-1,0}; break;
            case E_RESET_SPEED_TYPE_STOP_SOUTH_FACE: speed = 0; lCoords.mDirection = {0,0,1,0}; break;
            case E_RESET_SPEED_TYPE_STOP_EAST_FACE: speed = 0; lCoords.mDirection = {-1,0,0,0}; break;
            case E_RESET_SPEED_TYPE_STOP_WEST_FACE: case E_RESET_SPEED_TYPE_WEST_AND_IGNORE:
                speed = 0; lCoords.mDirection = {1,0,0,0}; break;
            case E_RESET_SPEED_TYPE_STOP_NORTH_EAST_FACE: speed = 0; lCoords.mDirection = {-0.707f,0,0.707f,0}; break;
            case E_RESET_SPEED_TYPE_STOP_SOUTH_WEST_FACE: speed = 0; lCoords.mDirection = {0.707f,0,-0.707f,0}; break;
            default: break;
            }
            lResult.Construct(AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS,
                              lpRequest->GetGlobalRaceCarIndex(),
                              speed,
                              lCoords.mPosition,
                              lCoords.mDirection);
            lpResults->GetResetOnTrackResultQueue()->AddEvent(lResult);

            if (lpRequest->GetResetType() != E_RESET_TYPE_STANDARD)
            {
                car->mfWrongWayTime = 0.0f;
                car->GetRoute()->meStatus = Route::E_STATUS_UNINITIALISED;
                car->GetRoute()->miNodeCount = 0;
                car->muBestSectionIndex = AICar::KI_INVALID_SECTION_INDEX;
                car->muDefaultSectionIndex = AICar::KI_INVALID_SECTION_INDEX;
            }

            RecentResetEntry lEntry;
            lEntry.mPosition = lCoords.mPosition;
            lEntry.mfTime    = lfTime;
            mRecentResets.Push(&lEntry);
        }
        else
        {
            // The console zeroes both vectors with a single `vspltisw v0, 0` and stores the
            // request's OWN speed. The consumer reads neither vector on this arm (it calls
            // ActiveRaceCar::GetResetCoords instead), but they are written because the console
            // writes them -- an uninitialised 32 bytes on a queue is how a plausible-looking
            // wrong pose gets read as data.
            lResult.Construct(AIModuleIO::ResetOnTrackResult::E_STATE_FAILURE,
                              lpRequest->GetGlobalRaceCarIndex(),
                              lpRequest->GetResetSpeed(),
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f },
                              Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });
            lpResults->GetResetOnTrackResultQueue()->AddEvent(lResult);
        }

        ++miResetCount;

        // [FLAG PC boot gate] ResetOnTrackDebugComponent::PushResetInfo(this + 1344, ...) and its
        // two queue tripwires -- see the banner.

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            // [DIAG rot] NOT IN THE X360 BINARY. One line per resolved request -- the witness that
            // separates "the pump ran" from "the pump found a pose", which are different claims
            // and the whole reason this wave exists.
            *CgsDev::Log::gpDebugPrint
                << "[rot] request resolved: car " << static_cast<s32>(lpRequest->GetGlobalRaceCarIndex())
                << " type " << static_cast<s32>(lpRequest->GetResetType())
                << " speed " << lpRequest->GetResetSpeed()
                << " -> " << (lResult.GetState() == AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS
                              ? "SUCCESS (AI pose)" : "FAILURE (consumer uses GetResetCoords)")
                << " resetCount " << miResetCount << "\n";
        }
    }

    // ---------------------------------------------------------------------------------------------
    // Update @0x8279A890
    //
    //   0x8279A8A4  assert(mResetOnTrackRequestQueue.miCount != -1)   (CgsArray.h:336)
    //   0x8279A8CC  liPending = mResetOnTrackRequestQueue.GetCount()  -- READ BEFORE anything else
    //   0x8279A8D4  mePlayerGlobalRaceCarIndex = lePlayer             (stw  this+900)
    //   0x8279A8DC  Camera::operator=(this + 0x38C, <the 4th argument>)   [PARKED -- see the header]
    //   0x8279A8E4  if (mRecentResets.GetLength() > 0
    //                   && lfTime - mRecentResets[len-1].mfTime > 3.0f)  mRecentResets.Pop()
    //   0x8279A930  if (liPending > 0) {
    //                 idx = liPending - 1;                            -- THE NEWEST, ONE PER FRAME
    //                 req = &mResetOnTrackRequestQueue[idx];
    //                 car = GetAICar(mePlayerGlobalRaceCarIndex);
    //                 if (req->index == mePlayerGlobalRaceCarIndex && req->type != 1) <streamed assert>
    //                 if (req->type == 1) goto PROCESS;               (:138)
    //                 section = car->muBestSectionIndex; if (0x7FFF) section = muDefaultSectionIndex;
    //                 if (section != 0x7FFF) { PROCESS:
    //                     ProcessResetOnTrackRequest(req, lpResults, lfTime);
    //                     mResetOnTrackRequestQueue.Erase(idx); } }
    //   0x8279A9F8  for each of the 35 AI cars (`for (i = 0; i < 191520; i += 5472)`):
    //                 if (car->meCarState == 0 || == 1)
    //                     if (car-><+0x1543> || (!car-><+0x1541> && !car->mbIsCrashing
    //                                            && !UpdateResetOnTrackSectionUsingRoute(car)))
    //                         UpdateResetOnTrackSectionUsingCurrentSection(car);
    //
    // ⭐⭐ ONE REQUEST PER FRAME, AND IT IS THE NEWEST (LIFO). There is no loop around the drain
    // -- the console takes index `count - 1` and Erases it. Reproduced exactly; a "drain the whole
    // queue" loop would be a behaviour change dressed as a tidy-up.
    // ⭐ THE TYPE-1 SHORT CIRCUIT IS WHY A CRASHED PLAYER GETS SERVICED AT ALL. Every other type
    // is gated behind the AI car having a usable section index, which no car has on this build;
    // type 1 (STANDARD -- what the crash exit sends) skips that gate entirely.
    // ⭐ `mResetOnTrackRequestQueue.GetCount()` is latched BEFORE the ring aging, exactly as the
    // console latches r29 at 0x8279A8CC. Nothing between them changes it, but the order is the
    // console's.
    //
    // Refresh each active car's recovery section after processing the request queue.
    // [FLAG PC boot gate] the streamed three-value assert at :138 -- the tree drops streamed
    // assert payloads by policy; the condition itself is kept as a plain CGS_ASSERT.
    // ---------------------------------------------------------------------------------------------
    void ResetOnTrackManager::Update(AIModuleResultInterface* lpResults,
                                     EGlobalRaceCarIndex lePlayer,
                                     f32 lfTime)
    {
        CGS_ASSERT(mResetOnTrackRequestQueue.GetCount() != -1,
                   "Array used before Construct/Clear was called");   // CgsArray.h:336

        const s32 liPendingCount = mResetOnTrackRequestQueue.GetCount();

        mePlayerGlobalRaceCarIndex = lePlayer;

        // [FLAG PC bring-up] Camera::operator=(mCamera, <the dropped 4th argument>) -- see the
        // declaration's banner in BrnResetOnTrackManager.h.

        // Age the recent-reset ring: when the NEWEST entry is more than 3 s old, drop the OLDEST.
        if (mRecentResets.GetLength() > 0)
        {
            const RecentResetEntry& lrNewest =
                mRecentResets[static_cast<u32>(mRecentResets.GetLength() - 1)];
            if ((lfTime - lrNewest.mfTime) > 3.0f)
            {
                mRecentResets.Pop(0);
            }
        }

        if (liPendingCount > 0)
        {
            const u32 luIndex = static_cast<u32>(liPendingCount - 1);
            const AIModuleIO::ResetOnTrackRequest& lrRequest = mResetOnTrackRequestQueue[luIndex];

            // Called for its two range asserts and to have the pointer ready for the section read
            // below; the read itself is type-gated (2026-09-03: it is a real read now).
            const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);

            CGS_ASSERT(!(lrRequest.GetGlobalRaceCarIndex() == mePlayerGlobalRaceCarIndex
                         && lrRequest.GetResetType() != E_RESET_TYPE_STANDARD),
                       "lpRequest->GetResetType()");   // :138

            bool lbProcess = (lrRequest.GetResetType() == E_RESET_TYPE_STANDARD);

            if (!lbProcess)
            {
                // ⭐⭐ 2026-09-03 (aiwave A11): THE CONSOLE'S OWN GATE, RESTORED.
                // X360 0x8279A8F0..0x8279A910: `lhz r37, 0x1534(car)` (muBestSectionIndex), falling
                // back to `lhz 0x1532` (muDefaultSectionIndex) when it holds the invalid sentinel,
                // then `if (section != 0x7FFF) PROCESS`. The car it reads is the PLAYER'S AI car,
                // not the requesting one -- every non-STANDARD strategy places relative to the
                // player, so no player section means no frame of reference.
                //
                // ⚠️ WHAT THE PARK THAT STOOD HERE GOT WRONG, AND WHY IT MATTERED. It said both
                // members "are 0 on this build rather than KI_INVALID_SECTION_INDEX because
                // AICar::Construct is an export hole", and refused unconditionally. That premise
                // has been false since AICar::Update landed: BrnAICar_Update.cpp writes both
                // members from the under-car AI section and from the route node, and the boot log
                // shows the player's car on a real section (`[route] extrapolated route done:
                // owner 0 ... section 5557`). The refusal was therefore not conservative -- it was
                // the reason EVERY rival's type-6 request sat in the queue forever, re-posted each
                // frame by the (console-correct) sender until the array overflowed.
                const u16 luPlayerSection = lpPlayerAICar->GetBestSectionIndex();
                lbProcess = (luPlayerSection != AICar::KI_INVALID_SECTION_INDEX);

                if (!lbProcess && CgsDev::Log::gpDebugPrint != 0)
                {
                    // [DIAG rot] NOT IN THE X360 BINARY. First-N only. Separates "the manager
                    // declined" from "the manager never saw it", which is the exact ambiguity
                    // this lane existed to remove.
                    static s32 siReportedNoPlayerSection = 0;
                    if (siReportedNoPlayerSection < 4)
                    {
                        ++siReportedNoPlayerSection;
                        *CgsDev::Log::gpDebugPrint
                            << "[rot] non-STANDARD request (type "
                            << static_cast<s32>(lrRequest.GetResetType())
                            << ", car " << static_cast<s32>(lrRequest.GetGlobalRaceCarIndex())
                            << ") HELD: the player AI car (global "
                            << static_cast<s32>(mePlayerGlobalRaceCarIndex)
                            << ") has no best/default AI section yet -- the console's own gate at "
                               "X360 0x8279A900. It will resolve on the frame the player car "
                               "acquires a section.\n";
                    }
                }
            }

            if (lbProcess)
            {
                ProcessResetOnTrackRequest(&lrRequest, lpResults, lfTime);
                mResetOnTrackRequestQueue.Erase(luIndex);
            }
        }

        for (s32 i = 0; i < 35; ++i)
        {
            AICar* car = GetAICar(static_cast<EGlobalRaceCarIndex>(i));
            if (car->IsActive() && (car->mbIsInShowtime || (!car->mbIsInAir && !car->mbIsCrashing
                && !UpdateResetOnTrackSectionUsingRoute(car))))
                UpdateResetOnTrackSectionUsingCurrentSection(car);
        }
    }
}
