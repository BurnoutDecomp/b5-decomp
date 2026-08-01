#include "GameSource/World/BrnPlaceOnTrackManager.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/Math/BrnMathUtils.h"             // BrnMath::BuildTransform / IsNormal
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::IsValid(Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint

#include <cfloat>   // FLT_MAX
#include <cstring>  // memset

// =============================================================================
// BrnWorld::PlaceOnTrackManager — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPlaceOnTrackManager.h for the
// candidate-list/node shape recovered from the access pattern.
//
// dep_flags: none un-homed. Self-contained (math only).
// =============================================================================

namespace BrnWorld
{

// ---------------------------------------------------------------------------
// The scene-manager output-event type PlaceOnTrackManager::PrePhysicsUpdate filters on
// (`FirstEvent != 1`). The committed SceneManagerIO enum names it
// E_OUT_EVENT_LINE_TEST_FINE_RESULT; spelled locally so this file does not have to pull
// the whole scene-manager IO header in for one ordinal.
static const s32 KI_OUT_EVENT_LINE_TEST_FINE_RESULT = 1;

// ---------------------------------------------------------------------------
// The console's own per-lane "are these two vectors the same" test, inlined at three
// points of PrePhysicsUpdate (asm `vsubfp128` + `vandc` against the sign mask +
// `vcmpgtfp.` against flt_82014430, then the >>5 &1 read of CR6 == "no lane exceeded the
// tolerance"). flt_82014430 is an un-valued .rdata float; the rw convention for
// IsSimilar's epsilon elsewhere in this tree is 1e-4f, which is what is used here.
// [FLAG] the epsilon is the convention, not the read constant.
// ---------------------------------------------------------------------------
static const f32 KF_PLACE_ON_TRACK_SIMILAR_EPSILON = 1.0e-4f;

static bool AreVectorsSimilar(const Vector3& lrA, const Vector3& lrB)
{
    const f32 lfDx = lrA.x - lrB.x;
    const f32 lfDy = lrA.y - lrB.y;
    const f32 lfDz = lrA.z - lrB.z;
    return ((lfDx < 0.0f ? -lfDx : lfDx) <= KF_PLACE_ON_TRACK_SIMILAR_EPSILON)
        && ((lfDy < 0.0f ? -lfDy : lfDy) <= KF_PLACE_ON_TRACK_SIMILAR_EPSILON)
        && ((lfDz < 0.0f ? -lfDz : lfDz) <= KF_PLACE_ON_TRACK_SIMILAR_EPSILON);
}


// ---------------------------------------------------------------------------
// ComputeBestPlaceOnT  @ 0x822BE238
//
// Store-for-store reconstruction of the X360 loop. The X360 is hand-vectorised but
// each VMX step is a simple per-lane operation whose result is consumed as a scalar,
// so the loop lowers faithfully to scalar C++:
//
//   bestDist = bestFilteredDist = FLT_MAX;      // f13 / f12 = flt_8201442C (FLT_MAX)
//   best = bestFiltered = 0;                    // r3  / r6
//   n = list.muNumCandidates;                   // *(a2+4)
//   if (n <= 0) return 0;
//   for each node (stride 0x40, first at a2+0x10):
//       sqDist = dot3(node.pos - query, node.pos - query);   // vsubfp + vmsum3fp128
//       eligible = filterOff ? true : ((node.muFlags >> 14) & 1) == 0;
//       if (eligible) {
//           heightPass = (query.y + 1.0f) > node.pos.y;       // vcmpgtfp, +1.0 = flt_82001C98
//           if (heightPass && sqDist < bestFilteredDist) {
//               bestFilteredDist = sqDist; bestFiltered = node;
//           }
//           if (sqDist < bestDist) { bestDist = sqDist; best = node; }
//       }
//   }
//   if (bestFiltered) return bestFiltered;
//   if (!best)        return 0;
//   return best;
//
// Notes:
//  * `dot3` is the X360 vmsum3fp128 of the (node - query) delta with itself = the
//    squared 3D distance. The query's vector is whatever lanes the caller passes; the
//    height test uses lane 1 (the .y component) of both query and node.
//  * The +1.0f height tolerance is the shared rodata constant flt_82001C98, attested
//    as the literal 1.0f at every other use site (see BrnStaticSoundMap.cpp).
//  * FLT_MAX is the rodata constant flt_8201442C (3.4028235e38).
//  * `bestFiltered` ranks only the filter-bit-eligible candidates that also pass the
//    height test; `best` ranks all filter-bit-eligible candidates. The filtered pick
//    wins when present.
//  * Comparisons use `<` for the nearest update and `>` for the height test, matching
//    the X360 `bge`/`bge` skip branches (NaN/equal keep the incumbent).
// ---------------------------------------------------------------------------
const PlaceOnTrackCandidate* PlaceOnTrackManager::ComputeBestPlaceOnT(
    const PlaceOnTrackCandidateList& lrCandidates,
    const Vector4& lrQuery,
    bool lbApplyHeightFilter) const
{
    const PlaceOnTrackCandidate* lpBest         = 0;
    const PlaceOnTrackCandidate* lpBestFiltered = 0;
    f32 lfBestDist         = FLT_MAX;
    f32 lfBestFilteredDist = FLT_MAX;

    const s32 liCount = lrCandidates.muNumCandidates;
    if (liCount <= 0)
    {
        return 0;
    }

    for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
    {
        const PlaceOnTrackCandidate& lrNode = lrCandidates.maCandidates[liIndex];

        // Squared 3D distance: dot3(node.pos - query).
        const f32 lfDx = lrNode.mPosition.x - lrQuery.x;
        const f32 lfDy = lrNode.mPosition.y - lrQuery.y;
        const f32 lfDz = lrNode.mPosition.z - lrQuery.z;
        const f32 lfSqDist = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;

        const bool lbEligible =
            lbApplyHeightFilter ? ((lrNode.muFlags & KU_PLACE_ON_TRACK_FILTER_BIT) == 0)
                                : true;

        if (lbEligible)
        {
            const bool lbHeightPass = (lrQuery.y + 1.0f) > lrNode.mPosition.y;

            if (lbHeightPass && lfSqDist < lfBestFilteredDist)
            {
                lfBestFilteredDist = lfSqDist;
                lpBestFiltered     = &lrNode;
            }

            if (lfSqDist < lfBestDist)
            {
                lfBestDist = lfSqDist;
                lpBest     = &lrNode;
            }
        }
    }

    if (lpBestFiltered)
    {
        return lpBestFiltered;
    }
    if (!lpBest)
    {
        return 0;
    }
    return lpBest;
}

// ===========================================================================
// Construct @ 0x822EA188.
//
// [FLAG] the console's two mRandom calls (CgsNumeric::Random::Construct + one
// RandomFloat to prime the stream, per the DecFIGS call list) are not made: mRandom is
// still opaque storage here, and its ONLY consumer is GetValuesForCarSelect @0x822D3470
// (the car-select drop), which is not reconstructed. Zeroing the storage keeps it
// deterministic instead of leaving module memory in it.
// ===========================================================================
void PlaceOnTrackManager::Construct(RaceCarEntityModule* lpRaceCarEntityModule)
{
    mpRaceCarEntityModule = lpRaceCarEntityModule;
    std::memset(maRandomStorage, 0, sizeof(maRandomStorage));
}

// ===========================================================================
// PrePhysicsUpdate @ 0x822F6DF8   (drivable wave 2026-08-01)
//
// ⭐ THE GATE. This is the only caller of RaceCarEntityModule::ResetActiveRaceCar
// @0x822F4880, which is the only writer of ActiveRaceCar::E_STATE_ACTIVE in the XEX.
// Everything that happens to a car between "its resources finished streaming" and "it is
// a live, simulated car" goes through these thirty lines.
//
// THE WALK (asm 0x822F6E2C..0x822F7890). Drain the pre-physics scene-result queue; take
// only events whose TYPE is E_OUT_EVENT_LINE_TEST_FINE_RESULT (1) **and** whose query-id
// OWNER byte is KI_LINE_TEST_OWNER (5) -- the console tests the owner as the raw second
// byte of the event (`BYTE1(*v4) != 5`), not through SceneQueryId::GetOwner. The query
// id's low byte is the active-race-car index that asked.
//
// ⚠️ VERSION DRIFT vs the Feb-2007 tree, ARTIST is authority on all three:
//   * the transform is built by BrnMath::BuildTransform(pos, direction, normal), NOT by
//     the hand-rolled Normalize/Cross chain the 2007 source shows;
//   * the velocity handed to ResetActiveRaceCar is lResetDirection * speed, NOT
//     lTransform.ZAxis() * speed;
//   * ARTIST adds the two "normal and reset direction are too similar" guards and the
//     car-select branch (GetValuesForCarSelect), neither of which exists in 2007.
// ===========================================================================
void PlaceOnTrackManager::PrePhysicsUpdate(
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    const RaceCarEntityModuleIO::SceneResultQueue* lpSceneResultQueue =
        lpInput->GetSceneResultQueue();

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpSceneResultQueue->GetFirstEvent( &lpEvent, &liSize );

    while( lpEvent != 0 )
    {
        // `FirstEvent == 1 && BYTE1(*event) == 5`
        if( liType == KI_OUT_EVENT_LINE_TEST_FINE_RESULT
            && reinterpret_cast<const u8*>( lpEvent )[1] == KI_PLACE_ON_TRACK_LINE_TEST_OWNER )
        {
            const PlaceOnTrackCandidateList* lpLineTestResult =
                reinterpret_cast<const PlaceOnTrackCandidateList*>( lpEvent );

            // The query id's low byte is the slot that asked (`v6 = *v4`).
            const EActiveRaceCarIndex leActiveRaceCarIndex =
                static_cast<EActiveRaceCarIndex>( reinterpret_cast<const u8*>( lpEvent )[0] );

            const ActiveRaceCar* lpActiveRaceCar =
                mpRaceCarEntityModule->GetActiveRaceCar( leActiveRaceCarIndex );

            // `v9 = *(module + 100041) == 0` -- the console's lbIgnoreFatal is
            // !mbInCarSelectScreen.
            const bool lbIgnoreFatal = !mpRaceCarEntityModule->IsInCarSelectScreen();

            // The console passes the request position in v1 (`lvx128 v1, car, 1952`).
            const Vector3& lrQueryPosition = lpActiveRaceCar->GetPlaceOnTrackPosition();
            const Vector4  lQuery = Vector4{ lrQueryPosition.x, lrQueryPosition.y,
                                             lrQueryPosition.z, 0.0f };

            const PlaceOnTrackCandidate* lpBestIntersection = ComputeBestPlaceOnT(
                *lpLineTestResult, lQuery, lbIgnoreFatal );

            PlaceCarOnTrack( leActiveRaceCarIndex, lpBestIntersection, lpOutput );
        }

        liType = lpSceneResultQueue->GetNextEvent( lpEvent, &lpEvent, &liSize );
    }

    ApplyPendingRequestsWithoutSceneQueryBringUp( lpOutput );
}

// ===========================================================================
// The per-result tail of PrePhysicsUpdate, factored out (asm 0x822F7274..0x822F7898).
// `lpBestIntersection == 0` is the console's own "the line test found nothing usable"
// arm -- it logs "Failed to find valid place on track location - reverting to ring
// buffer" and falls back.
// ===========================================================================
void PlaceOnTrackManager::PlaceCarOnTrack(
        EActiveRaceCarIndex leActiveRaceCarIndex,
        const PlaceOnTrackCandidate* lpBestIntersection,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    ActiveRaceCar* lpActiveRaceCar =
        mpRaceCarEntityModule->GetActiveRaceCar( leActiveRaceCarIndex );

    Vector3 lResetPosition;
    Vector3 lResetNormal;
    Vector3 lResetDirection = lpActiveRaceCar->GetPlaceOnTrackDirection();

    if( lpBestIntersection != 0 )
    {
        // asm: intersection+0x00 -> position, intersection+0x10 -> normal.
        lResetPosition = Vector3{ lpBestIntersection->mPosition.x, lpBestIntersection->mPosition.y,
                                  lpBestIntersection->mPosition.z, 0.0f };
        lResetNormal   = Vector3{ lpBestIntersection->mNormal.x, lpBestIntersection->mNormal.y,
                                  lpBestIntersection->mNormal.z, 0.0f };

        // [FLAG] the car-select branch: when mbInCarSelectScreen the console overrides all
        // three vectors through GetValuesForCarSelect @0x822D3470 (a randomised drop pose
        // built from KA_CAR_SELECT_NORMAL_ADD / KA_CAR_SELECT_NORMAL_RANDOMISE and mRandom).
        // Not reconstructed -- it is ~60 vector intrinsics against three un-homed rodata
        // Vector3[3] tables, and this build never enters the car-select screen.
    }
    else
    {
        // Console: "Failed to find valid place on track location - reverting to ring buffer"
        // then ActiveRaceCar::GetResetCoords(car, &lResetPosition, &lResetDirection) and
        // lResetNormal = the world Y axis (unk_82181510).
        //
        // ⚠️ [FLAG PC bring-up] GetResetCoords is NOT reconstructed, and substituting it
        // blindly here would be actively WRONG on this build: it reads the car's
        // mPrevTransforms ring buffer, which ActiveRaceCar::Attach has just Clear()ed for a
        // freshly spawned car -- it would place the car at the origin. The requested
        // position is used instead, which is what the Feb-2007 tree's own no-intersection
        // arm does and is exactly the pose the request carried.
        // DELETE-WHEN GetResetCoords lands AND the scene fine-query round trip produces
        // real results (at which point this arm stops being the one that runs).
        lResetPosition = lpActiveRaceCar->GetPlaceOnTrackPosition();
        lResetNormal   = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    }

    CGS_ASSERT( BrnMath::IsNormal( lResetNormal ), "BrnMath::IsNormal( lResetNormal )" );  // :221
    CGS_ASSERT( BrnMath::IsNormal( lpActiveRaceCar->GetPlaceOnTrackDirection() ),
                "BrnMath::IsNormal( lpActiveRaceCar->GetPlaceOnTrackDirection() )" );      // :222

    // The two ARTIST-only degeneracy guards (asm 0x822F72F0..0x822F73A8): if the normal and
    // the reset direction are within KF_SIMILAR of each other the normal is replaced by the
    // world Y axis; if they are STILL too similar the direction is replaced by the world X
    // axis. The console compares the per-lane absolute difference against flt_82014430.
    if( AreVectorsSimilar( lResetNormal, lResetDirection ) )
    {
        if( CgsDev::Log::gpDebugPrint != 0 )
            *CgsDev::Log::gpDebugPrint
                << "    Normal and reset direction are too similar - setting normal to y axis\n";
        lResetNormal = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };

        if( AreVectorsSimilar( lResetNormal, lResetDirection ) )
        {
            if( CgsDev::Log::gpDebugPrint != 0 )
                *CgsDev::Log::gpDebugPrint
                    << "    Normal and reset direction are still too similar - setting "
                       "direction to x axis\n";
            lResetDirection = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        }
    }

    // BrnMath::BuildTransform(lTransform, lPosition, lAt, lUp) -- v1/v2/v3 in that order.
    Matrix44Affine lTransform;
    BrnMath::BuildTransform( lTransform, lResetPosition, lResetDirection, lResetNormal );

    CGS_ASSERT( rw::math::vpu::IsValid( lTransform ), "RwMath::IsValid( lTransform )" );   // :240
    CGS_ASSERT( AreVectorsSimilar( lTransform.wAxis, lResetPosition ),
                "RwMath::IsSimilar( lTransform.GetW(), lResetPosition )" );                // :241
    CGS_ASSERT( lpActiveRaceCar->GetPlaceOnTrackSpeed() >= 0.0f,
                "lpActiveRaceCar->GetPlaceOnTrackSpeed() >= 0.0f" );                       // :242

    const f32 lfSpeed = lpActiveRaceCar->GetPlaceOnTrackSpeed();
    lpActiveRaceCar->ClearPlaceOnTrack();                                       // +0x7C4 = 0

    // [FLAG] `if (index == mePlayerActiveRaceCarIndex)` the console also posts game event
    // 13 (one byte) into lpOutput->GetGameEventQueue() -- a VariableEventQueue<1536,16>.
    // The event's id has no recovered name and nothing on PC drains that queue yet.

    // asm: v1 = lResetDirection * splat(mfPlaceOnTrackSpeed).
    const Vector3 lVelocity = Vector3{ lResetDirection.x * lfSpeed,
                                       lResetDirection.y * lfSpeed,
                                       lResetDirection.z * lfSpeed, 0.0f };

    mpRaceCarEntityModule->ResetActiveRaceCar( leActiveRaceCarIndex, lTransform, lVelocity,
                                               lpOutput->GetVehicleInputInterface() );

    if( CgsDev::Log::gpDebugPrint != 0 )
        *CgsDev::Log::gpDebugPrint << "[PLACEONTRACK] Place on track request complete\n";
}

// ===========================================================================
// [FLAG PC bring-up] ApplyPendingRequestsWithoutSceneQueryBringUp -- NOT an X360 function.
//
// WHY IT EXISTS, measured. RequestPlaceOnTrack latches a request; the console answers it
// by round-tripping a fine line test through the scene manager. On PC that round trip is
// severed in FIVE independent places, every one of them a stub or an absent body:
//   1. PlaceOnTrackManager::PostSceneUpdate @0x822D3168 -- absent (this wave leaves it so)
//   2. RaceCarEntityModule::PostSceneUpdate @0x822FE3F0 -- WorldLinkStubs.cpp:1358 stub
//   3. OutputBuffer_PostScene::GetSceneFineLineTestQueue -- declaration-only; the member is
//      a 16400-byte opaque blob with no AddEvent
//   4. WorldModule::BridgeRaceCarModuleToSceneModule_PostScene -- WorldLinkStubs.cpp:2488 stub
//   5. SceneManagerModule::ProcessSceneQueries -- WorldLinkStubs.cpp:2287 stub;
//      ProcessFineQueries/ProcessLineTestFine absent; FineIntersectionTestModule::
//      ComputeLineTestFine is an EMPTY body with zero callers, so nothing in the program
//      ever produces an OutEventLineTestFineResult at all.
// The consumer half (the bridge back, the queue, its Construct, the accessor) is REAL, so
// the walk above is live code -- it just never sees an event.
//
// WHAT THIS DOES: for every attached car with a pending request, run the CONSOLE'S OWN
// PlaceCarOnTrack with the console's own "no usable intersection" input. It invents no
// pose and no state: the position is the one the request carried, and every store after
// that point is console code. This is what replaces PromoteAttachedCarToActiveBringUp,
// which forced muState / mLOD / mbDamaged directly.
//
// DELETE-WHEN any ONE of the five above is closed far enough to deliver a result event;
// the walk then handles the request first and this leg finds nothing pending.
// ===========================================================================
void PlaceOnTrackManager::ApplyPendingRequestsWithoutSceneQueryBringUp(
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    for( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
    {
        const EActiveRaceCarIndex leActiveRaceCarIndex =
            static_cast<EActiveRaceCarIndex>( liCar );
        const ActiveRaceCar* lpActiveRaceCar =
            mpRaceCarEntityModule->GetActiveRaceCar( leActiveRaceCarIndex );

        if( lpActiveRaceCar->IsAttached() && lpActiveRaceCar->ToBePlacedOnTrack() )
        {
            if( CgsDev::Log::gpDebugPrint != 0 )
                *CgsDev::Log::gpDebugPrint
                    << "[PLACEONTRACK] [FLAG PC bring-up] no scene fine-query round trip; "
                       "answering the pending request for race car " << liCar
                    << " with the console's own no-intersection arm\n";

            PlaceCarOnTrack( leActiveRaceCarIndex, 0, lpOutput );
        }
    }
}

} // namespace BrnWorld
