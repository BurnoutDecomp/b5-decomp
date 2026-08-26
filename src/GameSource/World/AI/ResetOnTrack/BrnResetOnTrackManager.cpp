#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAICar.h"                                 // AICar (the array element)
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"      // AIModuleResultInterface
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint

// BrnAI::ResetOnTrackManager out-of-line members: Construct (@0x82791A48) and GetAICar
// (@0x82765878), plus the file-scope static perf-mon handles.

namespace BrnAI
{
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
    // ⛔⛔ READ THIS BEFORE READING "SUCCESS" INTO ANYTHING BELOW.
    // On this build every request resolves to ResetOnTrackResult::E_STATE_FAILURE, and that is
    // NOT a stub -- it is the console's own answer for a car the AI is not driving, arrived at
    // through the console's own gates:
    //     ComputeResetOnTrack -> ComputeInitialCoordinatesStandard -> `if (!IsActive()) return 0`
    // AIModule::Construct puts all 35 AICars in E_AI_CAR_STATE_INACTIVE (the value AICar::Reset
    // @0x82792800 writes) because nothing on this build activates one: StoreDrivenCarData,
    // SortTrafficIntoAICars, UpdateCars and AICar::Update are all absent. On the CONSOLE the
    // player's car IS an active AI car, so the console usually SUCCEEDS and places the car on the
    // AI road network. The divergence is in the INPUT STATE, not in this code.
    // ⇒ the FAILURE result is what makes the chain work today: RaceCarEntityModule::
    // ProcessResetOnTrackResultQueue's failure arm calls ActiveRaceCar::GetResetCoords, i.e. the
    // car's own four-deep "last places I was on the road" ring. That is the console's designed
    // fallback, not an invention.
    // =============================================================================================

    // ---------------------------------------------------------------------------------------------
    // PushResetOnTrackRequest @0x82783CE8. Append one request to the 35-deep pending array.
    // (X360: assert miCount != -1 / miCount < 0x23 inside Array<T,35>::Append @0x82769E88.)
    // ---------------------------------------------------------------------------------------------
    void ResetOnTrackManager::PushResetOnTrackRequest(const AIModuleIO::ResetOnTrackRequest* lpRequest)
    {
        CGS_ASSERT(lpRequest != 0, "lpRequest != NULL");
        if (lpRequest == 0)
        {
            return;
        }
        mResetOnTrackRequestQueue.Append(*lpRequest);
    }

    // ---------------------------------------------------------------------------------------------
    // ComputeInitialCoordinatesStandard @0x82783DD8   -- A MINIMAL-COMPLETE SLICE
    //
    //   0x82783E28  AICar* car = GetAICar(leGlobalRaceCarIndex)
    //   0x82783E30  v9  = car->meCarState               (lwz  0x14C8)
    //   0x82783E34  v10 = car->muResetOnTrackSectionIndex (lhz 0x1530)
    //   0x82783E44  if (!(v9 == 0 || v9 == 1)) goto FAIL          <- IsActive()
    //   0x82783E4C  if (v10 == 0x7FFF)          goto FAIL         <- no reset section
    //   ---- everything past here is the GEOMETRY, and it is PARKED ----
    //   0x82783E54  data = mpAISectionData.operator->()
    //               sec  = AISectionsData::GetAISection(data, v10)
    //               start = *AISection::GetPortal(sec, car->maPortalIndices[0])   (lbz 0x1538)
    //               end   = *AISection::GetPortal(sec, car->maPortalIndices[1])   (lbz 0x1539)
    //               assert(sec != NULL) ; assert(!IsSimilar(start, end)) ; dir = Normalise(end-start)
    //               assert(IsValid(dir))
    //               pos = ComputeNearestPositionInSegment(car->GetLastGoodPosition(), start, end)
    //               if (sec->flags(+23) & 8)   pos += <lateral offset from ComputeAISectionWidth>
    //               if (MagnitudeSquared2D(car->GetLastGoodPosition() - pos) > 40000.0f) goto FAIL
    //               out->{mpAISection, mPosition, mDirection} = {sec, pos, dir} ; return true
    //   FAIL: return false
    //
    // ⛔ [FLAG PC bring-up] THE GEOMETRY ARM IS PARKED, and it is UNREACHABLE on this build:
    // the IsActive() gate above it is the console's own and every AICar is INACTIVE (see the pump
    // banner). It needs FOUR functions none of which exists in this tree --
    // AISectionsData::GetAISection @0x8230F6D0, AISection::GetPortal @0x8230F5D0,
    // ComputeNearestPositionInSegment @0x82768908, ComputeAISectionWidth @0x82778250 -- plus a
    // named AISection interior and two AICar members (muResetOnTrackSectionIndex @+0x1530 and the
    // two portal indices @+0x1538/+0x1539) that live in BrnAICar.h's explicit pads.
    // ⭐ The two console gates ARE reproduced rather than replaced by a bare `return false`, so
    // the day the AI car feed lands this function starts refusing for the RIGHT reason and the
    // park becomes the only thing left to fill in.
    // DELETE-WHEN the AI section-data readers land and BrnAICar.h names the three members.
    // ---------------------------------------------------------------------------------------------
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

        // [FLAG PC bring-up] the second gate (muResetOnTrackSectionIndex @+0x1530 ==
        // KI_INVALID_SECTION_INDEX) and the whole geometry arm below it -- see the banner. The
        // member has no name in BrnAICar.h yet, so it is not read here rather than read by raw
        // offset. UNREACHABLE today: the IsActive() gate above always refuses first.
        return false;
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
    // ⭐ TYPE 1 IS THE ONE THE CRASH EXIT SENDS. RaceCarEntityModule::ProcessRaceCarCrashComplete-
    // Events builds its RequestResetOnTrack with type 1 for the player (type 3 only for an AI car
    // in a game mode with flag 0x80000000), so the STANDARD strategy is the reachable arm and the
    // other six are cold on this build.
    //
    // ⛔ [FLAG PC bring-up] SIX STRATEGIES + AvoidObstacles ARE PARKED (none exists in this tree:
    // ResetFixedDistanceBehindPlayer @0x82790628, ...AheadOfPlayer @0x827907D8,
    // ...AtStartOfRace @0x827908F0, ResetAheadFromSideTurnings @0x827909F0, ResetAwayFromPlayer
    // @0x82784148, PlayerIsLookingBackwards @0x82778000, AvoidObstacles @0x827941E0 -- together
    // ~1,000 pseudocode lines over the same absent AI section-data readers). They are reported
    // once each rather than silently falling through to `false`: a silent false here would be
    // indistinguishable from "the AI looked and found nothing", which is a DIFFERENT claim.
    // ⭐ AvoidObstacles is unreachable for a second, independent reason: it only runs when a
    // strategy SUCCEEDED, and none can today.
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
            case E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE:
            case E_RESET_TYPE_AHEAD_PLAYER_ON_COMING:
            case E_RESET_TYPE_FROM_TURNINGS_ROAD_RAGE:
            case E_RESET_TYPE_BEHIND_PLAYER_RACE_START:
            case E_RESET_TYPE_AWAY_FROM_PLAYER:
            {
                // [FLAG PC bring-up] the six parked strategies -- see the banner.
                static bool sbReportedParkedStrategies = false;
                if (!sbReportedParkedStrategies)
                {
                    sbReportedParkedStrategies = true;
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[rot] PARKED strategy: ResetOnTrackManager::ComputeResetOnTrack "
                               "reset type " << static_cast<s32>(lpRequest->GetResetType())
                            << " has no reconstructed placement function (X360 0x82790628 / "
                               "0x827907D8 / 0x827908F0 / 0x827909F0 / 0x82784148). Answering "
                               "'no coordinates', which is NOT the same claim as 'the AI looked "
                               "and found none'.\n";
                    }
                }
                lbFoundCoordinates = false;
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

        // [FLAG PC bring-up] AvoidObstacles @0x827941E0 -- parked, and unreachable (nothing can
        // set lbFoundCoordinates today). The console's `return AvoidObstacles(lpRequest, out)`.
        return true;
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
            // [FLAG PC bring-up] the SUCCESS arm -- see the banner. Unreachable today; the
            // direction override switch and the "not type 1" AICar tail both need unnamed
            // BrnAICar.h members. The result below carries the computed pose with the request's
            // own speed, which is the console's `default:` switch arm.
            lResult.Construct(AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS,
                              lpRequest->GetGlobalRaceCarIndex(),
                              lpRequest->GetResetSpeed(),
                              lCoords.mPosition,
                              lCoords.mDirection);
            lpResults->GetResetOnTrackResultQueue()->AddEvent(lResult);

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
    // ⛔ [FLAG PC bring-up] THE 35-CAR SECTION REFRESH IS PARKED. Both of its targets are absent
    // (UpdateResetOnTrackSectionUsingRoute @0x82786100, ...UsingCurrentSection @0x82786338 --
    // 346 pseudocode lines over the same unmounted AI section-data readers) and its two extra
    // flag reads (AICar +0x1541 / +0x1543) have no names in BrnAICar.h. ⭐ THE GATE ABOVE IT IS
    // REPRODUCED, so the park is provably unreached: every AICar is INACTIVE.
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

            // Called for its two range asserts and (on the console) to have the pointer ready for
            // the section read below; the read itself is type-gated.
            const AICar* lpPlayerAICar = GetAICar(mePlayerGlobalRaceCarIndex);
            (void)lpPlayerAICar;

            CGS_ASSERT(!(lrRequest.GetGlobalRaceCarIndex() == mePlayerGlobalRaceCarIndex
                         && lrRequest.GetResetType() != E_RESET_TYPE_STANDARD),
                       "lpRequest->GetResetType()");   // :138

            bool lbProcess = (lrRequest.GetResetType() == E_RESET_TYPE_STANDARD);

            if (!lbProcess)
            {
                // [FLAG PC bring-up] the non-STANDARD gate reads the AI car's best/default section
                // indices (AICar::GetBestSectionIndex(), +0x1534 falling back to +0x1532). Both
                // members exist and are static_asserted in BrnAICar.h, and both are 0 on this
                // build rather than KI_INVALID_SECTION_INDEX because AICar::Construct is an export
                // hole (see AIModule::Construct). Reading them would therefore pass a gate the
                // console's own initialisation would FAIL. Refusing is the honest answer and it
                // costs nothing: the crash exit only ever sends type 1.
                static bool sbReportedNonStandardGate = false;
                if (!sbReportedNonStandardGate)
                {
                    sbReportedNonStandardGate = true;
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[rot] non-STANDARD request (type "
                            << static_cast<s32>(lrRequest.GetResetType())
                            << ") declined: its console gate reads AICar section indices that "
                               "AICar::Construct (an ARTIST export hole) never seeded.\n";
                    }
                }
            }

            if (lbProcess)
            {
                ProcessResetOnTrackRequest(&lrRequest, lpResults, lfTime);
                mResetOnTrackRequestQueue.Erase(luIndex);
            }
        }

        // [FLAG PC bring-up] the 35-car reset-on-track section refresh -- see the banner. The
        // console's own state gate is reproduced so the park is provably unreached.
        for (s32 liCar = 0; liCar < 35; ++liCar)
        {
            const AICar* lpAICar =
                GetAICar(static_cast<EGlobalRaceCarIndex>(liCar));
            if (lpAICar->IsActive())
            {
                static bool sbReportedParkedSectionRefresh = false;
                if (!sbReportedParkedSectionRefresh)
                {
                    sbReportedParkedSectionRefresh = true;
                    if (CgsDev::Log::gpDebugPrint != 0)
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[rot] PARKED: ResetOnTrackManager::UpdateResetOnTrackSectionUsing"
                               "{Route,CurrentSection} (X360 0x82786100 / 0x82786338) are not "
                               "reconstructed -- AI car " << liCar << " is ACTIVE and its "
                               "reset-on-track section will not update.\n";
                    }
                }
            }
        }
    }
}
