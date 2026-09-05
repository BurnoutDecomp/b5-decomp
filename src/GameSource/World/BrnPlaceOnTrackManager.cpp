#include "GameSource/World/BrnPlaceOnTrackManager.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/Math/BrnMathUtils.h"             // BrnMath::BuildTransform / IsNormal
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::IsValid(Matrix44Affine)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint

// [FLAG PC bring-up] the world-collision drop query below reads the shipped WORLDCOL.BIN
// through the tree's own async file-system engine (the same call every bundle load makes)
// and walks it with the committed PolygonSoup layout. See the banner above
// BuildDropTestCandidatesBringUp.
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"
#include "GameShared/GameClasses/System/FileSystem/CgsFileSystem.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceBundle2.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"

#include <cstdio>   // [teleport] sscanf (the BRN_CAR_TELEPORT spec parse)
#include <cstdlib>  // [teleport] getenv
#include <cfloat>   // FLT_MAX
#include <cstddef>  // offsetof
#include <cstring>  // memset
#include <cmath>    // sqrtf

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
    // [teleport] the harness teleport, armed here so its request is answered by THIS frame's
    // ApplyPendingRequestsWithoutSceneQueryBringUp below. See the block at the bottom of this file.
    ArmCarTeleportBringUp();

    // [sweep] the deterministic crash sweep, armed on the same frame and answered by the
    // same ApplyPendingRequestsWithoutSceneQueryBringUp below. See the block at the bottom
    // of this file. Inert unless BRN_CRASH_SWEEP is set.
    ArmCrashSweepBringUp();

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
        // Vector3[3] tables.
        //
        // ⚠️⚠️ THE SECOND HALF OF THAT JUSTIFICATION HAS EXPIRED, AND IT IS NOW A VISIBLE
        //   DEFECT (task #127, 2026-08-04). It used to end "...and this build never enters the
        //   car-select screen". IT DOES -- every boot: "=== CarSelectManager: Car Select" fires
        //   at ~64 s on a plain dh_run/cs_run, and this very function places the car it frames.
        //   Corrected in place rather than left to expire silently.
        //
        // ⭐ WHAT THE MISSING BRANCH ACTUALLY DOES, read from the asm (not inferred):
        //   GetValuesForCarSelect's whole body is gated on `v14 = *(module + 100044)` with
        //   `if (v14 && v14 < 3)`, and its LAST act is
        //       _R11 = 1952 ; lvx128 v0, r26, r11 ; stvx128 v0, r0, r25
        //   i.e. it writes `lpActiveRaceCar + 1952` == mPlaceOnTrackPosition (+0x7A0) -- THE
        //   FREE-AIR QUERY ANCHOR -- back over lResetPosition, and randomises the up-normal
        //   from unk_82FAD5B0[16*v14] / unk_82FAD560[16*v14]. So on console the car-select car
        //   is DROPPED IN from the anchor with a random tilt and FALLS to the yard floor; the
        //   junkyard's authored anchors sit 3.3-6.2 m above their own floor for exactly that.
        //
        // ⛔ DO NOT "fix" the car-select camera by restoring this position here. The console's
        //   drop is a drop: the vehicle physics catches the car and settles it on its
        //   suspension. That physics is inert on this build (task #121, the physics wall), so
        //   restoring the anchor alone would hang the car in mid-air for ever -- which is the
        //   defect 28eb8e42 removed.
        //
        // ⭐⭐ WHY THIS MATTERS TO THE CAMERA. BehaviourRotateAboutVehicle (the car-select
        //   orbit) has NO pitch and NO height term of its own: BecomeSimilarTo flattens the
        //   orbit seed to the car's horizontal plane AND calls mRotationController.Construct(),
        //   which zeroes the pitch mover; Update then drives it paused with centering off, so
        //   with no stick the pitch stays 0 for ever and the eye's Y is IDENTICALLY the car's
        //   transform-origin Y (measured: dY 0.000000 every frame). The shot's height IS the
        //   car's height. Since this function writes the origin to the raw ground intersection
        //   with no seating, the eye sits on the junkyard floor and the screen shows the car's
        //   underside. The console's own AABB arm (GameBridgeWorldToX.cpp:288, min = -halfExtent
        //   / max = +halfExtent) says that origin is meant to be the BODY CENTRE, ~0.75 m up.
        //   ⇒ closing this needs the drop AND the seating, i.e. the physics -- not a camera edit
        //   and not a fabricated offset here.
    }
    else
    {
        // Console: "Failed to find valid place on track location - reverting to ring buffer"
        // then ActiveRaceCar::GetResetCoords(car, &lResetPosition, &lResetDirection) and
        // lResetNormal = the world Y axis (unk_82181510).
        //
        // ⚠️⚠️ CORRECTED 2026-08-26 (aicar_reset wave) -- THE HALF OF THIS NOTE THAT SAID
        // "it would place the car at the origin" WAS WRONG, AND IT IS THE HALF FOUR OTHER
        // BANNERS IN THIS TREE COPIED. GetResetCoords IS reconstructed now
        // (BrnActiveRaceCar.cpp), and the asm settles what an empty ring does:
        //     0x822BF318  lwz r11, 0x5A0(r31)          <- mPrevTransforms.miLength
        //     0x822BF320  bgt cr6, loc_822BF33C        <- length > 0 : read the ring
        //     0x822BF37C  li r11, 0x300 ; li r10, 0x2F0
        //     0x822BF384  lvx128 v0, r31, r11          <- length == 0 : mPhysicsState.mTransform
        // An empty ring falls back to the car's LIVE transform ({wAxis, zAxis}), NOT the origin.
        //
        // ⚠️ [FLAG PC bring-up] IT STAYS PARKED HERE ANYWAY, for the reason that survives: this
        // arm runs for a car ActiveRaceCar::Attach has just spawned, whose mPhysicsState is
        // itself unseeded at that moment -- so the fallback would hand out THAT, not a pose. The
        // requested position is used instead, which is what the Feb-2007 tree's own
        // no-intersection arm does and is exactly the pose the request carried.
        // ⭐ The ring itself is now WRITTEN per frame (ActiveRaceCar::UpdateResetTransform, landed
        // 2026-08-26), but it only fills while the car is inside the AI section system, which
        // needs the above-ground line-test round trip -- see that function's banner.
        // DELETE-WHEN the ring fills on a booted drive AND the scene fine-query round trip
        // produces real results (at which point this arm stops being the one that runs).
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
// [FLAG PC bring-up] THE DROP QUERY -- NOT an X360 function, and NOT an invented pose.
//
// ⭐⭐ WHY THIS EXISTS AT ALL, and what it fixes (measured 2026-08-02).
//
// The authored spawn location a place-on-track request carries is a QUERY ANCHOR IN FREE
// AIR, never a pose. PlaceOnTrackManager::PostSceneUpdate @0x822D3168 proves it, in asm:
//
//     v68 = (0, flt_820138DC, 0, 0)                 flt_820138DC == 50.0f
//     lvx128    v0, r31, 0x7A0                      v0 = mPlaceOnTrackPosition
//     vaddfp128 v127, v0, v13   ->  lEvent.mLineStart = pos + (0, +50, 0)
//     vsubfp128 v126, v0, v12   ->  lEvent.mLineEnd   = pos - (0, +50, 0)
//     InEventLineTestFine::AddEvent(...)
//
// -- a 100 m VERTICAL LINE TEST through the request. PrePhysicsUpdate then ranks the
// result with ComputeBestPlaceOnT and PlaceCarOnTrack writes the car to
// lpBestIntersection->mPosition: the GROUND, not the anchor.
//
// ⚠️ AND THE CAR-SELECT OVERRIDE DOES NOT SAVE THE ANCHOR ON THIS PATH.
// GetValuesForCarSelect @0x822D3470 does copy mPlaceOnTrackPosition back over the reset
// position (its tail is `li r11, 1952 ; lvx128 v0, r26, r11 ; stvx128 v0, r0, r25`), but
// that copy is INSIDE `if (v14 && v14 < 3)` where `v14 = *(module + 100044)`.
// HandleResetPlayerCarAction @0x82304FE8 seeds that member from the action's +0x3C
// (miInCarModification) and CarSelectManager::EnterJunkyardAtStartOfGame writes `stw 0`
// there. At start of game the override is a NO-OP and the intersection stands.
//
// MEASURED, both ends, over the shipped collision (the junkyard the player starts in):
//     authored anchor  maSpawnLocations[1] = (2986.933105, 1.009405, -2011.417969)
//     the only surface the console's own line crosses:      y = -3.525
//     => the car was rendering 4.534 m ABOVE the junkyard floor.
// The anchor is authored high on purpose: every junkyard's three car-select anchors sit
// 3.3-6.2 m above their own yard floor, so the downward test starts clear of the junk.
// (The spawn record itself is NOT the bug -- it is byte-identical in the X360 original,
// in our port, and in the shipped BPR Remaster's TRIGGERS.DAT.)
//
// WHAT THIS DOES. The scene fine-query round trip is severed in five places (see the
// banner below) AND the world collision is never even loaded on this build:
// ScriptedLoad stage 7 (LoadWorldCollision) is deferred, BrnGameDataModule's TRK_COLL arm
// is a DeferredGameDataRequest, TriangleCollisionManager::Prepare is inert, and the VMX
// intersection kernels (IntersectLinePolySoupTriangle*4) are not reconstructed. So this
// helper asks the SAME QUESTION of the SAME SHIPPED DATA directly: it reads WORLDCOL.BIN
// through the tree's own async file-system engine, walks its PolygonSoupLists with the
// committed CgsGeometric::PolygonSoup layout, and returns every upward-facing triangle the
// console's own line (anchor +/- (0, KF_PLACE_ON_TRACK_LINE_TEST_LENGTH, 0)) crosses as a
// PlaceOnTrackCandidate. The console's own ComputeBestPlaceOnT then picks between them and
// the console's own PlaceCarOnTrack does every store. Nothing here is tuned by eye: the Y
// the car ends up at is a vertex of the shipped collision mesh.
//
// WHAT IS STILL A STAND-IN, stated plainly:
//   * muFlags is left ZERO on every candidate, so the "fatal surface" gate
//     (KU_PLACE_ON_TRACK_FILTER_BIT) never fires. That bit is a CollisionTag bit the scene
//     manager sets; the polygon's own u32 surface tag is a different field and the mapping
//     is not recovered. Consequence: a candidate the console would have rejected as fatal
//     is eligible here.
//   * only WORLDCOL.BIN is consulted -- the console's fine query also sees dynamic scene
//     entities (props, other cars). Nothing on this build registers collision for those.
//   * single-sidedness is reproduced by discarding triangles whose geometric normal points
//     down (the console's IntersectLinePolySoupTriangleSingleSided4 does the same facing
//     test); no edge-cosine / material work is attempted.
//
// DELETE-WHEN the real round trip lands (any one of the five severed links closing far
// enough to deliver an OutEventLineTestFineResult): the walk in PrePhysicsUpdate then
// answers the request first and this leg finds nothing pending.
// ===========================================================================
namespace
{
    // WORLDCOL.BIN is a platform-4 bundle of 396 PolygonSoupList + 396 IdList resources
    // (one pair per collision zone). [game #71] in CgsResourceTypeRegistration.cpp.
    const u32   KU_POLYGON_SOUP_LIST_TYPE_ID = 0x43;
    const char* KAC_WORLD_COLLISION_BUNDLE   = "WORLDCOL.BIN";

    // The SERIALISED forms of the two records this walk reads. They are the committed x64
    // structures viewed before FixUp: every pointer slot still holds a byte OFFSET from the
    // start of the resource blob (CgsGeometric::PolygonSoupList::FixUp adds the load base to
    // exactly these three slots plus the two inside each soup). Declared here rather than
    // reusing the committed structs because those hold real pointers and this walk must not
    // relocate a buffer it does not own.
    struct SerialisedPolygonSoupList
    {
        f32 mafOverallAabb[8];   // +0x00  min xyzw / max xyzw
        u64 muPolySoupTable;     // +0x20  offset of the u64[miNumPolySoups] soup table
        u64 muPolySoupBoxes;     // +0x28  offset of the AxisAlignedBox4 SoA groups
        s32 miNumPolySoups;      // +0x30
        s32 miDataSize;          // +0x34
    };

    struct SerialisedPolygonSoup
    {
        s32 miPosX;              // +0x00  the committed CgsGeometric::PolygonSoup names
        s32 miPosY;              // +0x04
        s32 miPosZ;              // +0x08
        f32 mfScale;             // +0x0C
        u64 muPolygons;          // +0x10  offset of the 12-byte polygon records
        u64 muVertices;          // +0x18  offset of the 6-byte packed vertices
        u16 mu16Size;            // +0x20
        u8  mu8NumPolygons;      // +0x22
        u8  mu8NumQuads;         // +0x23
        u8  mu8NumVertices;      // +0x24
        u8  mau8Pad[3];          // +0x25..+0x27
    };

    // The x64 record sizes the transcoder emits (tools/assets/bundles/
    // world_support_transcode.py: PSL_X64_HDR / SOUP_X64_HDR).
    static_assert(sizeof(SerialisedPolygonSoupList) == 0x38, "PolygonSoupList x64 header");
    static_assert(sizeof(SerialisedPolygonSoup) == 0x28, "PolygonSoup x64 header");
    static_assert(sizeof(CgsResource::BundleV2::ResourceEntry) == 0x40, "BundleV2 entry");

    // The candidate buffer this leg hands to ComputeBestPlaceOnT. The console's own
    // OutEventLineTestFineResult is a fixed-capacity record too; 32 is far more than a
    // vertical line through one spawn point has ever produced (the junkyard start crosses
    // exactly one triangle, the drive-in five).
    const s32 KI_DROP_QUERY_MAX_CANDIDATES = 32;

    struct DropQueryResultBringUp
    {
        BrnWorld::PlaceOnTrackCandidateList mList;   // header + maCandidates[1]
        BrnWorld::PlaceOnTrackCandidate     maExtraCandidates[KI_DROP_QUERY_MAX_CANDIDATES - 1];
    };

    // maExtraCandidates must continue maCandidates[0] at the same 64-byte stride, because
    // ComputeBestPlaceOnT walks the array by stride from +0x10.
    static_assert(sizeof(BrnWorld::PlaceOnTrackCandidate) == 0x40, "candidate stride");
    static_assert(offsetof(BrnWorld::PlaceOnTrackCandidateList, maCandidates) == 0x10,
                  "candidate array offset");
    static_assert(offsetof(DropQueryResultBringUp, maExtraCandidates)
                      == offsetof(DropQueryResultBringUp, mList)
                         + offsetof(BrnWorld::PlaceOnTrackCandidateList, maCandidates)
                         + sizeof(BrnWorld::PlaceOnTrackCandidate),
                  "candidate array is not contiguous");

    // The shipped collision, read once and kept (the console keeps the world collision
    // resident too). Null means "not attempted yet"; mbTried guards a failed read.
    void* gpWorldCollisionBlob = 0;
    u32   guWorldCollisionSize = 0;
    bool  gbWorldCollisionTried = false;

    const void* GetWorldCollisionBringUp(u32* lpuOutSize)
    {
        if (!gbWorldCollisionTried)
        {
            gbWorldCollisionTried = true;
            CgsFileSystem::EnsureDeviceManagerUp();
            CgsFileSystem::DeviceManager* lpManager =
                CgsFileSystem::DeviceManager::GetIfInitialized();
            if (lpManager != 0)
            {
                gpWorldCollisionBlob =
                    lpManager->ReadWholeFile(KAC_WORLD_COLLISION_BUNDLE, &guWorldCollisionSize);
            }
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[PLACEONTRACK] [FLAG PC bring-up] world collision '"
                    << KAC_WORLD_COLLISION_BUNDLE << "' "
                    << (gpWorldCollisionBlob != 0 ? "read (" : "UNAVAILABLE (")
                    << static_cast<s32>(guWorldCollisionSize) << " bytes)\n";
            }
        }
        *lpuOutSize = guWorldCollisionSize;
        return gpWorldCollisionBlob;
    }

    // Where the vertical line through (lfX, *, lfZ) crosses the triangle, or false.
    // XZ point-in-triangle by barycentric weights, then the same weights interpolate Y --
    // exact for a plane, and the plane is what the console's ray/triangle kernel solves for.
    bool CrossVerticalLine(f32 lfX, f32 lfZ,
                           const f32 (&lrA)[3], const f32 (&lrB)[3], const f32 (&lrC)[3],
                           f32* lpfOutY)
    {
        const f32 lfDen = (lrB[2] - lrC[2]) * (lrA[0] - lrC[0])
                        + (lrC[0] - lrB[0]) * (lrA[2] - lrC[2]);
        if (lfDen > -1.0e-9f && lfDen < 1.0e-9f)
            return false;   // degenerate in XZ (a vertical wall) -- no crossing

        const f32 lfInv = 1.0f / lfDen;
        const f32 lfW0 = ((lrB[2] - lrC[2]) * (lfX - lrC[0])
                        + (lrC[0] - lrB[0]) * (lfZ - lrC[2])) * lfInv;
        const f32 lfW1 = ((lrC[2] - lrA[2]) * (lfX - lrC[0])
                        + (lrA[0] - lrC[0]) * (lfZ - lrC[2])) * lfInv;
        const f32 lfW2 = 1.0f - lfW0 - lfW1;
        if (lfW0 < 0.0f || lfW1 < 0.0f || lfW2 < 0.0f)
            return false;

        *lpfOutY = lfW0 * lrA[1] + lfW1 * lrB[1] + lfW2 * lrC[1];
        return true;
    }

    // Append one crossing, with the triangle's own unit normal oriented AGAINST the line
    // (i.e. upward), which is what a DOUBLE-SIDED line/triangle test returns and what
    // PlaceCarOnTrack needs -- it hands the normal to BrnMath::BuildTransform as the car's
    // up vector.
    //
    // ⚠️ MEASURED 2026-08-02, and it is why this is not the single-sided test: the shipped
    // collision does NOT wind ground surfaces consistently. The junkyard floor quad
    // (res bf2191aa soup 86 poly 3, y = -3.525) winds to (0, +1, 0); the road quad right
    // outside it (res b5515579 soup 1 poly 17, y = 1.065) winds to (0, -1, 0). A
    // facing-rejecting walk therefore drops half the drivable surfaces in the world.
    // CgsGeometric ships both IntersectLinePolySoupTriangleSingleSided4 AND ...DoubleSided;
    // the data says this query is the double-sided one.
    bool AppendCandidate(DropQueryResultBringUp& lrResult, f32 lfX, f32 lfY, f32 lfZ,
                         const f32 (&lrA)[3], const f32 (&lrB)[3], const f32 (&lrC)[3])
    {
        const f32 lfUx = lrB[0] - lrA[0], lfUy = lrB[1] - lrA[1], lfUz = lrB[2] - lrA[2];
        const f32 lfVx = lrC[0] - lrA[0], lfVy = lrC[1] - lrA[1], lfVz = lrC[2] - lrA[2];
        f32 lfNx = lfUy * lfVz - lfUz * lfVy;
        f32 lfNy = lfUz * lfVx - lfUx * lfVz;
        f32 lfNz = lfUx * lfVy - lfUy * lfVx;
        const f32 lfLen = sqrtf(lfNx * lfNx + lfNy * lfNy + lfNz * lfNz);
        if (lfLen <= 1.0e-6f)
            return false;
        lfNx /= lfLen; lfNy /= lfLen; lfNz /= lfLen;
        if (lfNy < 0.0f)
        {
            lfNx = -lfNx; lfNy = -lfNy; lfNz = -lfNz;   // face the incoming (downward) line
        }

        if (lrResult.mList.muNumCandidates >= KI_DROP_QUERY_MAX_CANDIDATES)
            return false;

        BrnWorld::PlaceOnTrackCandidate& lrNode =
            lrResult.mList.maCandidates[lrResult.mList.muNumCandidates];
        lrNode.mPosition.x = lfX;
        lrNode.mPosition.y = lfY;
        lrNode.mPosition.z = lfZ;
        lrNode.mPosition.w = 0.0f;
        lrNode.mNormal.x   = lfNx;
        lrNode.mNormal.y   = lfNy;
        lrNode.mNormal.z   = lfNz;
        lrNode.mNormal.w   = 0.0f;
        ++lrResult.mList.muNumCandidates;
        return true;
    }

    // Fill lrResult with every collision triangle the console's own place-on-track line
    // through lrAnchor crosses.
    void BuildDropTestCandidatesBringUp(const Vector3& lrAnchor, DropQueryResultBringUp& lrResult)
    {
        std::memset(&lrResult, 0, sizeof(lrResult));

        u32         luBlobSize = 0;
        const void* lpBlob     = GetWorldCollisionBringUp(&luBlobSize);
        if (lpBlob == 0 || luBlobSize < sizeof(CgsResource::BundleV2))
            return;

        const u8* lpcBase = static_cast<const u8*>(lpBlob);
        const CgsResource::BundleV2* lpBundle =
            reinterpret_cast<const CgsResource::BundleV2*>(lpcBase);
        if (lpBundle->muVersion != CgsResource::BundleV2::KU_VERSION
            || lpBundle->muPlatform != CgsResource::BundleV2::KU_PLATFORM
            || lpBundle->IsCompressed())
        {
            return;   // not the x64 image this walk understands
        }

        const f32 lfMinY = lrAnchor.y - KF_PLACE_ON_TRACK_LINE_TEST_LENGTH;
        const f32 lfMaxY = lrAnchor.y + KF_PLACE_ON_TRACK_LINE_TEST_LENGTH;

        const CgsResource::BundleV2::ResourceEntry* lpaEntries =
            reinterpret_cast<const CgsResource::BundleV2::ResourceEntry*>(
                lpcBase + lpBundle->muResourceEntriesOffset);

        for (u32 luEntry = 0; luEntry < lpBundle->muResourceEntriesCount; ++luEntry)
        {
            const CgsResource::BundleV2::ResourceEntry& lrEntry = lpaEntries[luEntry];
            if (lrEntry.muResourceTypeId != KU_POLYGON_SOUP_LIST_TYPE_ID)
                continue;

            const u32 luSize = lrEntry.mauUncompressedSizeAndAlignment[0]
                             & CgsResource::BundleV2::KU_SIZE_MASK;
            if (luSize < sizeof(SerialisedPolygonSoupList))
                continue;

            const u8* lpcList = lpcBase + lpBundle->mauResourceDataOffset[0]
                              + lrEntry.mauDiskOffset[0];
            const SerialisedPolygonSoupList* lpList =
                reinterpret_cast<const SerialisedPolygonSoupList*>(lpcList);

            const s32 liNumSoups = lpList->miNumPolySoups;
            if (liNumSoups <= 0)
                continue;

            // The list's own AABB: min xyzw at +0x00, max xyzw at +0x10.
            if (lrAnchor.x < lpList->mafOverallAabb[0] || lrAnchor.x > lpList->mafOverallAabb[4]
                || lrAnchor.z < lpList->mafOverallAabb[2] || lrAnchor.z > lpList->mafOverallAabb[6])
            {
                continue;
            }

            const u64* lpauSoupTable =
                reinterpret_cast<const u64*>(lpcList + lpList->muPolySoupTable);

            for (s32 liSoup = 0; liSoup < liNumSoups; ++liSoup)
            {
                const SerialisedPolygonSoup* lpSoup =
                    reinterpret_cast<const SerialisedPolygonSoup*>(
                        lpcList + lpauSoupTable[liSoup]);

                const s32 liNumVerts = lpSoup->mu8NumVertices;
                if (liNumVerts <= 0)
                    continue;

                // UnpackPolygonSoupVertices @0x8283B480: world = (pos_s32 + vert_u16) * scale.
                //
                // ⭐⭐ THE PACKED VERTEX IS ZERO-EXTENDED, NOT SIGN-EXTENDED (fixed 2026-08-16).
                // This read was `const s16*`. The console masks each lane with a vector constant
                // of {0xFFFF, 0xFFFF, 0xFFFF, 0} -- built at 0x82C6DBD0 and applied by the `vand`
                // inside UnpackPolygonSoupVertices -- so a lane of 0x8000 is +32768, never -32768.
                // The committed consumer CgsPolygonSoupTests.cpp:349 already zero-extends; this
                // was the one site that disagreed with it.
                // ⭐ ORACLE, not reasoning: the per-soup AABB shipped inside WORLDCOL.BIN is
                // console-authored, and decoding all 23,645 soups agrees with it on 23,645/23,645
                // UNSIGNED and on only 23,629 signed. The 16 disagreeing soups decode up to ~1 km
                // from where they belong.
                const u16* lpaPacked =
                    reinterpret_cast<const u16*>(lpcList + lpSoup->muVertices);
                f32 lafVerts[256][3];
                f32 lfMinX = 0.0f, lfMaxX = 0.0f, lfMinZ = 0.0f, lfMaxZ = 0.0f;
                for (s32 liVert = 0; liVert < liNumVerts; ++liVert)
                {
                    const u16* lpPacked = lpaPacked + liVert * 3;
                    lafVerts[liVert][0] = (lpSoup->miPosX + lpPacked[0]) * lpSoup->mfScale;
                    lafVerts[liVert][1] = (lpSoup->miPosY + lpPacked[1]) * lpSoup->mfScale;
                    lafVerts[liVert][2] = (lpSoup->miPosZ + lpPacked[2]) * lpSoup->mfScale;
                    if (liVert == 0 || lafVerts[liVert][0] < lfMinX) lfMinX = lafVerts[liVert][0];
                    if (liVert == 0 || lafVerts[liVert][0] > lfMaxX) lfMaxX = lafVerts[liVert][0];
                    if (liVert == 0 || lafVerts[liVert][2] < lfMinZ) lfMinZ = lafVerts[liVert][2];
                    if (liVert == 0 || lafVerts[liVert][2] > lfMaxZ) lfMaxZ = lafVerts[liVert][2];
                }
                if (lrAnchor.x < lfMinX || lrAnchor.x > lfMaxX
                    || lrAnchor.z < lfMinZ || lrAnchor.z > lfMaxZ)
                {
                    continue;
                }

                const u8* lpcPolys = lpcList + lpSoup->muPolygons;
                for (s32 liPoly = 0; liPoly < lpSoup->mu8NumPolygons; ++liPoly)
                {
                    // 12-byte record: u32 surface tag @0, u8 vertexIndex[4] @4,
                    // u8 edgeCosine[4] @8. Quads come first (mu8NumQuads of them); a
                    // triangle carries KU8_POLYGON_NO_VERTEX in slot 3.
                    //
                    // ⭐ THE QUAD IS A STRIP, NOT A FAN (measured 2026-08-02). Its four
                    // indices run 0-1-3-2 around the perimeter, so the two triangles are
                    // (0,1,2) and (1,3,2). Splitting it as a fan -- (0,1,2)+(0,2,3) --
                    // produces a BOW TIE: on the junkyard floor quad (res bf2191aa soup 86
                    // poly 3) the fan halves come out with normals (0,+1,0) and (0,-1,0)
                    // and the crossing the console's line actually makes is lost. That is
                    // exactly the bug the first cut of this walk shipped: 0 crossings at
                    // the junkyard anchor, 3 at the drive-in.
                    const u8* lpcPoly = lpcPolys + liPoly * CgsGeometric::KI_POLYGON_STRIDE;
                    const bool lbQuad = (liPoly < lpSoup->mu8NumQuads);
                    const s32 laiTris[2][3] = { { lpcPoly[4], lpcPoly[5], lpcPoly[6] },
                                                { lpcPoly[5], lpcPoly[7], lpcPoly[6] } };
                    const s32 liNumTris = lbQuad ? 2 : 1;

                    for (s32 liTri = 0; liTri < liNumTris; ++liTri)
                    {
                        const s32 liA = laiTris[liTri][0];
                        const s32 liB = laiTris[liTri][1];
                        const s32 liC = laiTris[liTri][2];
                        if (liA >= liNumVerts || liB >= liNumVerts || liC >= liNumVerts)
                            continue;

                        f32 lfY = 0.0f;
                        if (!CrossVerticalLine(lrAnchor.x, lrAnchor.z, lafVerts[liA],
                                               lafVerts[liB], lafVerts[liC], &lfY))
                            continue;
                        if (lfY < lfMinY || lfY > lfMaxY)
                            continue;   // outside the console's own 100 m line

                        AppendCandidate(lrResult, lrAnchor.x, lfY, lrAnchor.z,
                                        lafVerts[liA], lafVerts[liB], lafVerts[liC]);
                    }
                }
            }
        }
    }
}

// ===========================================================================
// [FLAG PC bring-up] ApplyPendingRequestsWithoutSceneQueryBringUp -- NOT an X360 function.
//
// WHY IT EXISTS, measured. RequestPlaceOnTrack latches a request; the console answers it
// by round-tripping a fine line test through the scene manager. On PC that round trip is
// severed in FIVE independent places, every one of them a stub or an absent body:
//   1. PlaceOnTrackManager::PostSceneUpdate @0x822D3168 -- absent (this wave leaves it so)
//   2. RaceCarEntityModule::PostSceneUpdate @0x822FE3F0 -- ⚠️ THIS RUNG IS STALE AND IS
//      CORRECTED 2026-08-26 (resetpump): it has NOT been a WorldLinkStubs stub since the
//      crash-exit wave (2026-08-25) -- it is a real minimal-complete SLICE in
//      BrnRaceCarEntityModule_CrashExit.cpp. What is still absent is the leg of it that would
//      post here: PlaceOnTrackManager::PostSceneUpdate, i.e. rung 1. The SEVERANCE IS REAL,
//      the reason given for it was out of date.
//   3. OutputBuffer_PostScene::GetSceneFineLineTestQueue -- declaration-only; the member is
//      a 16400-byte opaque blob with no AddEvent (its Construct now runs -- resetpump wave --
//      but the member type itself is still a `maReserved` slice)
//   4. WorldModule::BridgeRaceCarModuleToSceneModule_PostScene -- WorldLinkStubs.cpp:2488 stub
//   5. SceneManagerModule::ProcessSceneQueries -- WorldLinkStubs.cpp:2287 stub;
//      ProcessFineQueries/ProcessLineTestFine absent; FineIntersectionTestModule::
//      ComputeLineTestFine is an EMPTY body with zero callers, so nothing in the program
//      ever produces an OutEventLineTestFineResult at all.
// The consumer half (the bridge back, the queue, its Construct, the accessor) is REAL, so
// the walk above is live code -- it just never sees an event.
//
// WHAT THIS DOES: for every attached car with a pending request, run the console's own
// place-on-track line test against the shipped world collision (see THE DROP QUERY above),
// rank the crossings with the console's own ComputeBestPlaceOnT, and hand the winner to the
// console's own PlaceCarOnTrack. It invents no pose and no state: the Y the car lands on is
// a vertex of the shipped collision mesh, and every store after that point is console code.
// When the collision cannot be read the leg falls back to the console's own "no usable
// intersection" arm, which keeps the anchor -- the behaviour this leg had before 2026-08-02,
// and the reason the junkyard car rendered 4.534 m above its floor.
//
// This is what replaced PromoteAttachedCarToActiveBringUp, which forced
// muState / mLOD / mbDamaged directly.
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

        if( !lpActiveRaceCar->IsAttached() || !lpActiveRaceCar->ToBePlacedOnTrack() )
        {
            continue;
        }

        // The console's own query point and its own line (PostSceneUpdate @0x822D3168).
        const Vector3& lrAnchor = lpActiveRaceCar->GetPlaceOnTrackPosition();

        DropQueryResultBringUp lDropQuery;
        BuildDropTestCandidatesBringUp( lrAnchor, lDropQuery );

        // `lbIgnoreFatal` exactly as the console's own walk computes it.
        const bool lbIgnoreFatal = !mpRaceCarEntityModule->IsInCarSelectScreen();
        const Vector4 lQuery = Vector4{ lrAnchor.x, lrAnchor.y, lrAnchor.z, 0.0f };
        const PlaceOnTrackCandidate* lpBestIntersection =
            ComputeBestPlaceOnT( lDropQuery.mList, lQuery, lbIgnoreFatal );

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[PLACEONTRACK] [FLAG PC bring-up] no scene fine-query round trip; race car "
                << liCar << " line ("
                << lrAnchor.x << ", " << ( lrAnchor.y + KF_PLACE_ON_TRACK_LINE_TEST_LENGTH )
                << ", " << lrAnchor.z << ") -> ("
                << lrAnchor.x << ", " << ( lrAnchor.y - KF_PLACE_ON_TRACK_LINE_TEST_LENGTH )
                << ", " << lrAnchor.z << ") over the shipped world collision: "
                << lDropQuery.mList.muNumCandidates << " crossings";
            if( lpBestIntersection != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "; anchor y " << lrAnchor.y << " -> ground y "
                    << lpBestIntersection->mPosition.y << " (drop "
                    << ( lrAnchor.y - lpBestIntersection->mPosition.y ) << " m) normal ("
                    << lpBestIntersection->mNormal.x << ", " << lpBestIntersection->mNormal.y
                    << ", " << lpBestIntersection->mNormal.z << ")\n";
            }
            else
            {
                *CgsDev::Log::gpDebugPrint
                    << "; NONE -- falling back to the console's own no-intersection arm "
                       "(the car stays at the authored anchor)\n";
            }
        }

        PlaceCarOnTrack( leActiveRaceCarIndex, lpBestIntersection, lpOutput );
    }
}

// ===========================================================================
// [teleport] ArmCarTeleportBringUp -- NOT an X360 function, and DELIBERATELY PERMANENT.
//
// ⭐ WHAT IT IS. `BRN_CAR_TELEPORT="x,y,z[,headingDeg]"` puts the player's car at a world
// position, once, as soon as it is actually driving. It exists because a boot-drive harness that
// can only start at the junkyard can only ever test what is within 275 seconds of the junkyard --
// which is how the SMASH flavour of the gate-UI ladder got proven and the BILLBOARD flavour (120
// type-12 GenericRegions, none of them on the junkyard-exit route) did not.
//
// ⭐⭐ IT IS A TRIGGER, NOT A MECHANISM. Every metre of the actual move is the game's own code:
//     ActiveRaceCar::RequestPlaceOnTrack @0x822BFB58   <- THE ONLY THING THIS BLOCK CALLS
//       -> PlaceOnTrackManager::PrePhysicsUpdate       -- the 100 m vertical line test through the
//          request (on PC: ApplyPendingRequestsWithoutSceneQueryBringUp, over the SHIPPED
//          WORLDCOL.BIN, which is why the Y a teleport lands on is a vertex of the collision mesh
//          and never a number chosen here)
//       -> ComputeBestPlaceOnT @0x822BE238             -- the console's own candidate ranking
//       -> PlaceCarOnTrack                             -- BrnMath::BuildTransform(pos, at, up)
//       -> RaceCarEntityModule::ResetActiveRaceCar @0x822F4880, its IsActive() arm
//       -> VehicleInputInterface::ResetRaceCar @0x822CC2A0
//       -> VehicleManager::ProcessResetEvents @0x82617820
//       -> VehiclePhysics::SetTransformFromPositionOnRoad @0x825D1C00  (the analytic rest seat)
//        + VehiclePhysics::Reset @0x825FDD78                            (kill all motion, re-seed)
// Nothing here writes a transform, a velocity, a force or a physics field, and nothing here
// bypasses the seat -- which is the whole reason the teleported car drives normally afterwards
// instead of sitting 0.74 m in the road or exploding on the first suspension tick.
//
// ⭐ WHY THE TRIGGER IS DISTANCE-BASED AND NOT A TIMER. The car reaches E_STATE_ACTIVE at CAR
// SELECT, tens of seconds before the flow reaches DRIVING, and boot timing drifts by seconds run
// to run -- exactly the reason every mark in tools/diagnostics/flow_run.ps1 is anchored to a flow
// state and not to a frame index. So the arm waits for the car to have MOVED
// KF_TELEPORT_ARM_DISTANCE from where it was first seen ACTIVE, i.e. for the drive to have
// actually started. Self-synchronising, no clock, no assumption about the physics tick rate.
// `BRN_CAR_TELEPORT_ARM_DISTANCE` overrides it (metres); 0 means "fire on the first ACTIVE frame".
//
// ⚠️ THE STREAMER. A long jump is NOT free: the world streams around the player, and the
// teleport does not wait for it. The car lands on WORLDCOL.BIN (which is resident whole, read
// once by the drop query) so it never falls through the world, but the visible world and the
// breakable props around the destination arrive over the following seconds. A run plan must
// leave the car sitting for a few seconds before it is asked to smash anything -- which is what
// flow_run.ps1's -DriveDelay already provides.
//
// ⛔ DELETE-WHEN: NOTHING. This is a permanent harness capability, not a bring-up shim. It is
// inert unless BRN_CAR_TELEPORT is set, it adds one strcmp-free getenv on the first pre-physics
// update and one bool test per update after that, and a default run is byte-identical.
// ===========================================================================
void PlaceOnTrackManager::ArmCarTeleportBringUp()
{
    // Metres the car must have travelled since it was first seen ACTIVE before the teleport fires.
    static const f32 KF_TELEPORT_ARM_DISTANCE = 8.0f;

    enum ETeleportStage
    {
        E_TELEPORT_UNREAD = 0,   // the env var has not been looked at yet
        E_TELEPORT_OFF,          // not set / unparseable -- never look again
        E_TELEPORT_WAITING,      // parsed; waiting for the car to start driving
        E_TELEPORT_REQUESTED     // the request has been issued; done for this process
    };

    static ETeleportStage seStage = E_TELEPORT_UNREAD;
    static Vector3 sTarget     = { 0.0f, 0.0f, 0.0f, 0.0f };
    static Vector3 sDirection  = { 0.0f, 0.0f, 1.0f, 0.0f };
    static Vector3 sArmOrigin  = { 0.0f, 0.0f, 0.0f, 0.0f };
    static f32     sfArmDistance = KF_TELEPORT_ARM_DISTANCE;
    static bool    sbArmOriginSeen = false;

    if( seStage == E_TELEPORT_OFF || seStage == E_TELEPORT_REQUESTED )
    {
        return;
    }

    if( seStage == E_TELEPORT_UNREAD )
    {
        seStage = E_TELEPORT_OFF;

        const char* lpcSpec = std::getenv( "BRN_CAR_TELEPORT" );
        if( lpcSpec == 0 || lpcSpec[0] == '\0' )
        {
            return;
        }

        // "x,y,z" or "x,y,z,headingDeg". A malformed spec is a HARD REFUSAL with a log line, never
        // a silent partial teleport -- a run that quietly did not move the car would be scored as
        // "the billboard never fired" rather than as a typo.
        f32 lfX = 0.0f, lfY = 0.0f, lfZ = 0.0f, lfHeadingDeg = 0.0f;
        const int liFields = std::sscanf( lpcSpec, "%f,%f,%f,%f", &lfX, &lfY, &lfZ, &lfHeadingDeg );
        if( liFields < 3 )
        {
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[teleport] FAIL: BRN_CAR_TELEPORT=\"" << lpcSpec
                    << "\" is not \"x,y,z[,headingDeg]\" -- the car is NOT moved\n";
            }
            return;
        }

        // The heading is degrees CLOCKWISE FROM +Z looking down, i.e. the same convention the
        // world's own authored directions use: at = (sin h, 0, cos h). h=0 faces +Z.
        const f32 KF_DEG_TO_RAD = 0.0174532925199433f;
        const f32 lfHeading = lfHeadingDeg * KF_DEG_TO_RAD;

        sTarget    = Vector3{ lfX, lfY, lfZ, 0.0f };
        sDirection = Vector3{ std::sin( lfHeading ), 0.0f, std::cos( lfHeading ), 0.0f };

        const char* lpcArm = std::getenv( "BRN_CAR_TELEPORT_ARM_DISTANCE" );
        if( lpcArm != 0 && lpcArm[0] != '\0' )
        {
            float lfArm = KF_TELEPORT_ARM_DISTANCE;
            if( std::sscanf( lpcArm, "%f", &lfArm ) == 1 && lfArm >= 0.0f )
            {
                sfArmDistance = lfArm;
            }
        }

        seStage = E_TELEPORT_WAITING;

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[teleport] armed: BRN_CAR_TELEPORT -> (" << lfX << ", " << lfY << ", " << lfZ
                << ") heading=" << lfHeadingDeg << " deg at=(" << sDirection.x << ", "
                << sDirection.y << ", " << sDirection.z << "); fires once the player car has "
                   "driven " << sfArmDistance << " m from its spawn\n";
        }
    }

    if( mpRaceCarEntityModule == 0 )
    {
        return;
    }

    const EActiveRaceCarIndex lePlayerIndex =
        mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex();
    if( lePlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }

    ActiveRaceCar* lpPlayerCar = mpRaceCarEntityModule->GetActiveRaceCar( lePlayerIndex );
    if( lpPlayerCar == 0 || !lpPlayerCar->IsActive() || lpPlayerCar->ToBePlacedOnTrack() )
    {
        return;   // not live yet, or a placement it did not ask for is already in flight
    }

    // The readback's own copy of the physics pose -- the same member the [uoi] probe prints.
    const Vector3& lrHere = lpPlayerCar->GetPhysicsState()->mTransform.wAxis;

    if( !sbArmOriginSeen )
    {
        sbArmOriginSeen = true;
        sArmOrigin = lrHere;
    }

    const f32 lfDX = lrHere.x - sArmOrigin.x;
    const f32 lfDY = lrHere.y - sArmOrigin.y;
    const f32 lfDZ = lrHere.z - sArmOrigin.z;
    if( ( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ ) < ( sfArmDistance * sfArmDistance ) )
    {
        return;   // still parked where it spawned -- the drive has not started
    }

    // ⭐ THE ONE CALL. Speed 0.0f: PlaceCarOnTrack multiplies the reset direction by it, so the
    // car arrives at rest and VehiclePhysics::Reset re-seeds every motion register from that
    // zero. The console asserts `GetPlaceOnTrackSpeed() >= 0.0f` on the way through.
    lpPlayerCar->RequestPlaceOnTrack( sTarget, sDirection, 0.0f );
    seStage = E_TELEPORT_REQUESTED;

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[teleport] car -> (" << sTarget.x << ", " << sTarget.y << ", " << sTarget.z
            << ") heading=(" << sDirection.x << ", " << sDirection.y << ", " << sDirection.z
            << ") from (" << lrHere.x << ", " << lrHere.y << ", " << lrHere.z << ")\n";
    }
}


// ===========================================================================
// [sweep] ArmCrashSweepBringUp -- NOT an X360 function. THE DETERMINISTIC CRASH HARNESS.
//
// ⭐⭐ WHY IT EXISTS, and what it fixes. The crash campaign's recipe up to 2026-09-05 was
// "teleport to a launch point, hold the throttle, and steer right 21.5 SECONDS later"
// (flow_run.ps1 -SteerScript). Seconds are WALL CLOCK; the sim is a fixed 1/60 step. So the
// frame a steering change lands on depends on how fast the host happened to be running --
// and the previous wave measured exactly that: box_B2 and box_B3 produced BYTE-IDENTICAL
// numbers to each other (entry 142.89 mph, no roll-over) while box_B1, the same build and
// the same recipe with frame dumping ON, entered at 139.82 mph and rolled the car through a
// full 360. A harness whose result depends on whether frames are being dumped cannot answer
// a FREQUENCY question, and that wave said so.
//
// ⭐ THE FIX IS TO REMOVE THE CLOCK, NOT TO ADD A SEED. Note what those two runs prove: the
// sim IS deterministic -- identical input frames gave identical floats. The clock was the
// only stochastic term. So this trigger specifies a crash by its PHYSICS, not by a drive:
//
//     BRN_CRASH_SWEEP        = "x,y,z"                    the launch point
//     BRN_CRASH_SWEEP_SHOTS  = "h0:s0,h1:s1,..."          heading (deg) : speed (m/s), per shot
//                              ...or "x/y/z/h:s" per entry, to fire that shot from ITS OWN
//                              launch point. THAT FORM IS WHAT MAKES AN ANGLE SWEEP HONEST:
//                              fanning the heading from ONE launch point changes WHICH piece of
//                              world the car meets (measured -- from one waterfront launch,
//                              heading 234 hit at (3172.6,-2003.2), heading 250 hit a different
//                              object 19 m away, and heading 220 drove 112 m and hit NOTHING),
//                              whereas launch = target - D * dir(h) aims every shot at the SAME
//                              wall and varies only the angle of incidence.
//     BRN_CRASH_SWEEP_SETTLE = frames between shots       (default 150 == 2.5 s)
//     BRN_CRASH_SWEEP_MAX    = frames before a shot is forced anyway (default 900 == 15 s)
//     BRN_CRASH_SWEEP_ARM_DISTANCE = metres driven before shot 0 (default 8, as the teleport)
//
// Each shot is ONE ActiveRaceCar::RequestPlaceOnTrack( launch, heading, speed ) -- the same
// single call the BRN_CAR_TELEPORT trigger already makes, with the console's own THIRD
// argument (mfPlaceOnTrackSpeed) finally non-zero. PlaceCarOnTrack multiplies the reset
// direction by it (`v1 = lResetDirection * splat(mfPlaceOnTrackSpeed)`) and hands that to
// ResetActiveRaceCar, so the car ARRIVES MOVING at exactly the requested speed on exactly
// the requested heading, seated by VehiclePhysics::SetTransformFromPositionOnRoad and
// re-seeded by VehiclePhysics::Reset. Nothing here writes a transform, a velocity, a force
// or a physics field; impact speed and impact angle stop being emergent properties of a
// drive and become INPUTS.
//
// ⭐ WHY IT IS FRAME-COUNTED AND NOT TIMED. After shot k the car's whole motion state is
// re-seeded by the console's own reset, so the trajectory from there depends only on (a) the
// seeded pose/velocity and (b) the per-frame input, which the harness holds CONSTANT. The
// shot cadence is therefore counted in PrePhysicsUpdate calls -- sim frames -- so the shot
// list replays identically whatever the host's frame rate does. The one arm that is not
// frame-counted is shot 0's (it waits for the car to have DRIVEN the arm distance, exactly
// as the teleport does, because nothing else tells us the flow has reached DRIVING); that
// only decides WHEN the sweep starts, not what any shot does.
//
// ⭐ THE SETTLE GATE IS A STATE GATE, NOT A TIMER. The next shot waits for
// ActiveRaceCar::IsCrashing() to go false -- i.e. for the console's own crash record to
// close and the reset pump to recover the car -- with BRN_CRASH_SWEEP_MAX as a backstop so a
// car that never recovers cannot stall the sweep. Both terms are deterministic functions of
// the sim, so the shot FRAMES are reproducible too, not just the shot parameters.
//
// ⛔ WHAT IT STILL CANNOT DO, said plainly: RequestPlaceOnTrack takes ONE direction, which
// becomes both the facing and the velocity. So a shot is always a car travelling the way it
// points -- this sweep covers ANGLE OF INCIDENCE, and cannot express a sideways slide or a
// spinning car meeting a wall. Traffic is also not reset between shots, so a shot that meets
// a traffic car is a different experiment from one that meets the wall; the [sweep] marker
// exists so the log can be segmented per shot and such a shot identified and dropped.
//
// ⛔ DELETE-WHEN: NOTHING. Permanent harness capability, inert unless BRN_CRASH_SWEEP is set
// (one getenv on the first pre-physics update, one enum test per update after).
// ===========================================================================
void PlaceOnTrackManager::ArmCrashSweepBringUp()
{
    static const s32 KI_MAX_SWEEP_SHOTS = 48;
    static const f32 KF_SWEEP_DEG_TO_RAD = 0.0174532925199433f;

    enum ESweepStage
    {
        E_SWEEP_UNREAD = 0,   // the env var has not been looked at yet
        E_SWEEP_OFF,          // not set / unparseable -- never look again
        E_SWEEP_WAITING,      // parsed; waiting for the drive to start
        E_SWEEP_RUNNING,      // firing shots
        E_SWEEP_DONE          // shot list exhausted
    };

    static ESweepStage seStage           = E_SWEEP_UNREAD;
    static Vector3     sLaunch           = { 0.0f, 0.0f, 0.0f, 0.0f };
    static f32         safHeadingDeg[ KI_MAX_SWEEP_SHOTS ];
    static f32         safSpeed[ KI_MAX_SWEEP_SHOTS ];
    static Vector3     saLaunch[ KI_MAX_SWEEP_SHOTS ];
    static s32         siShotCount       = 0;
    static s32         siNextShot        = 0;
    static s32         siSettleFrames    = 150;
    static s32         siMaxFrames       = 900;
    static f32         sfArmDistance     = 8.0f;
    static Vector3     sArmOrigin        = { 0.0f, 0.0f, 0.0f, 0.0f };
    static bool        sbArmOriginSeen   = false;
    static s32         siFramesSinceShot = 0;
    static s32         siFrame           = 0;
    // [sweep] SEAT VERIFICATION -- see the block that consumes them, below the shot.
    static s32         siSeatCheck       = -1;   // frames until the seat is verified; -1 == idle
    static Vector3     sSeatWanted       = { 0.0f, 0.0f, 0.0f, 0.0f };
    static s32         siSeatShot        = 0;

    if( seStage == E_SWEEP_OFF || seStage == E_SWEEP_DONE )
    {
        return;
    }

    if( seStage == E_SWEEP_UNREAD )
    {
        seStage = E_SWEEP_OFF;

        const char* lpcSpec = std::getenv( "BRN_CRASH_SWEEP" );
        if( lpcSpec == 0 || lpcSpec[0] == '\0' )
        {
            return;
        }

        f32 lfX = 0.0f, lfY = 0.0f, lfZ = 0.0f;
        if( std::sscanf( lpcSpec, "%f,%f,%f", &lfX, &lfY, &lfZ ) != 3 )
        {
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[sweep] FAIL: BRN_CRASH_SWEEP=\"" << lpcSpec
                    << "\" is not \"x,y,z\" -- the sweep is OFF\n";
            }
            return;
        }
        sLaunch = Vector3{ lfX, lfY, lfZ, 0.0f };

        // "h:s,h:s,..." -- heading in degrees clockwise from +Z (the same convention the
        // teleport uses: at = (sin h, 0, cos h)), speed in metres per second.
        const char* lpcShots = std::getenv( "BRN_CRASH_SWEEP_SHOTS" );
        if( lpcShots != 0 && lpcShots[0] != '\0' )
        {
            const char* lpcCursor = lpcShots;
            while( *lpcCursor != '\0' && siShotCount < KI_MAX_SWEEP_SHOTS )
            {
                f32 lfHeadingDeg = 0.0f, lfSpeed = 0.0f;
                f32 lfShotX = 0.0f, lfShotY = 0.0f, lfShotZ = 0.0f;
                const int liShotFields = std::sscanf( lpcCursor, "%f/%f/%f/%f:%f",
                                                      &lfShotX, &lfShotY, &lfShotZ,
                                                      &lfHeadingDeg, &lfSpeed );
                if( liShotFields == 5 )
                {
                    saLaunch[ siShotCount ] = Vector3{ lfShotX, lfShotY, lfShotZ, 0.0f };
                }
                else
                {
                    if( std::sscanf( lpcCursor, "%f:%f", &lfHeadingDeg, &lfSpeed ) != 2 )
                    {
                        break;
                    }
                    saLaunch[ siShotCount ] = sLaunch;
                }
                safHeadingDeg[ siShotCount ] = lfHeadingDeg;
                safSpeed[ siShotCount ]      = ( lfSpeed < 0.0f ) ? 0.0f : lfSpeed;
                ++siShotCount;

                const char* lpcComma = std::strchr( lpcCursor, ',' );
                if( lpcComma == 0 )
                {
                    break;
                }
                lpcCursor = lpcComma + 1;
            }
        }

        if( siShotCount == 0 )
        {
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[sweep] FAIL: BRN_CRASH_SWEEP_SHOTS is empty or unparseable "
                       "(expected \"heading:speed,heading:speed,...\") -- the sweep is OFF\n";
            }
            return;
        }

        const char* lpcSettle = std::getenv( "BRN_CRASH_SWEEP_SETTLE" );
        if( lpcSettle != 0 && lpcSettle[0] != '\0' )
        {
            int liSettle = siSettleFrames;
            if( std::sscanf( lpcSettle, "%d", &liSettle ) == 1 && liSettle > 0 )
            {
                siSettleFrames = liSettle;
            }
        }

        const char* lpcMax = std::getenv( "BRN_CRASH_SWEEP_MAX" );
        if( lpcMax != 0 && lpcMax[0] != '\0' )
        {
            int liMax = siMaxFrames;
            if( std::sscanf( lpcMax, "%d", &liMax ) == 1 && liMax > 0 )
            {
                siMaxFrames = liMax;
            }
        }
        if( siMaxFrames < siSettleFrames )
        {
            siMaxFrames = siSettleFrames;
        }

        const char* lpcArm = std::getenv( "BRN_CRASH_SWEEP_ARM_DISTANCE" );
        if( lpcArm != 0 && lpcArm[0] != '\0' )
        {
            float lfArm = sfArmDistance;
            if( std::sscanf( lpcArm, "%f", &lfArm ) == 1 && lfArm >= 0.0f )
            {
                sfArmDistance = lfArm;
            }
        }

        seStage = E_SWEEP_WAITING;

        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[sweep] armed: launch (" << sLaunch.x << ", " << sLaunch.y << ", "
                << sLaunch.z << ") shots=" << siShotCount << " settle=" << siSettleFrames
                << " maxFrames=" << siMaxFrames << " armDistance=" << sfArmDistance << "\n";
            for( s32 liShot = 0; liShot < siShotCount; ++liShot )
            {
                *CgsDev::Log::gpDebugPrint
                    << "[sweep] plan " << liShot << " heading " << safHeadingDeg[ liShot ]
                    << " deg speed " << safSpeed[ liShot ] << " m/s from ("
                    << saLaunch[ liShot ].x << ", " << saLaunch[ liShot ].y << ", "
                    << saLaunch[ liShot ].z << ")\n";
            }
        }
    }

    if( mpRaceCarEntityModule == 0 )
    {
        return;
    }

    const EActiveRaceCarIndex lePlayerIndex =
        mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex();
    if( lePlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )
    {
        return;
    }

    ActiveRaceCar* lpPlayerCar = mpRaceCarEntityModule->GetActiveRaceCar( lePlayerIndex );
    if( lpPlayerCar == 0 || !lpPlayerCar->IsActive() || lpPlayerCar->ToBePlacedOnTrack() )
    {
        return;   // not live yet, or a placement it did not ask for is already in flight
    }

    const Vector3& lrHere = lpPlayerCar->GetPhysicsState()->mTransform.wAxis;

    if( seStage == E_SWEEP_WAITING )
    {
        if( !sbArmOriginSeen )
        {
            sbArmOriginSeen = true;
            sArmOrigin      = lrHere;
        }

        const f32 lfDX = lrHere.x - sArmOrigin.x;
        const f32 lfDY = lrHere.y - sArmOrigin.y;
        const f32 lfDZ = lrHere.z - sArmOrigin.z;
        if( ( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ ) < ( sfArmDistance * sfArmDistance ) )
        {
            return;   // still parked where it spawned -- the drive has not started
        }

        seStage           = E_SWEEP_RUNNING;
        siFramesSinceShot = siSettleFrames;   // fire shot 0 on this frame
    }

    ++siFrame;
    ++siFramesSinceShot;

    // ---- [sweep] SEAT VERIFICATION ------------------------------------------------------------
    // ⛔⛔ AN OFF-ROAD LAUNCH IS A SILENTLY DEAD RUN, AND IT COST A WAVE A WHOLE GRID (2026-09-05).
    // crash_sweep_batch.ps1's banner already warned that "the launch must land ON ROAD:
    // place-on-track's drop query returns no candidate off it and the car is seated at the
    // requested Y instead" -- but nothing SAID SO AT RUN TIME. Measured on run mwB_h210_s50_r1
    // (a 160 m run-up whose launch fell off the road): the car was seated at y -12.606 against a
    // requested -3.700, at 0.000000 mph, and sat there for 4,000 frames. The log contained a
    // perfectly ordinary "[sweep] shot 0/1 ... forced 0" line and then simply no crash, which reads
    // exactly like "this angle does not roll the car" -- a NEGATIVE RESULT MANUFACTURED BY THE
    // HARNESS. The same silence would follow a shot whose launch is inside geometry.
    // So the sweep now checks its own placement three frames after the request (the placement is a
    // REQUEST -- it is consumed by the reset pump on a later frame, so the check cannot be
    // immediate) and prints one line either way. Frame-counted like everything else here.
    if( siSeatCheck > 0 )
    {
        --siSeatCheck;
        if( siSeatCheck == 0 )
        {
            const f32 lfDX = lrHere.x - sSeatWanted.x;
            const f32 lfDY = lrHere.y - sSeatWanted.y;
            const f32 lfDZ = lrHere.z - sSeatWanted.z;
            const f32 lfDist = std::sqrt( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ );
            // ⚠️ THE TEST IS ON **Y** ALONE, and that is deliberate. The horizontal distance is
            // useless as a discriminator here: the shot arrives MOVING, so three frames later a
            // perfectly good seat is already 1-3 m down its own heading (measured: 0.4 m at
            // 50 m/s in mwA_h210_s50_r1, 1.9 m at 60 m/s in mwA_h240_s60_r1), and a threshold
            // loose enough to allow that would also allow a bad seat that happens to be nearby.
            // The vertical is the axis the failure actually shows on: a good seat is snapped to the
            // road surface and lands within ~0.3 m of the requested Y, while mwB_h210_s50_r1's bad
            // one was 8.9 m below it. 3 m is an order of magnitude clear of both.
            const bool lbBad = ( lfDY < -3.0f || lfDY > 3.0f );
            if( CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint
                    << ( lbBad ? "[sweep] SEAT BAD shot " : "[sweep] seat ok shot " ) << siSeatShot
                    << " wanted (" << sSeatWanted.x << ", " << sSeatWanted.y << ", " << sSeatWanted.z
                    << ") got (" << lrHere.x << ", " << lrHere.y << ", " << lrHere.z
                    << ") off " << lfDist << " m\n";
                if( lbBad )
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[sweep] SEAT BAD: the launch point is almost certainly OFF ROAD -- "
                           "PlaceCarOnTrack found no road candidate and seated the car at the "
                           "requested Y. This shot is NOT a sample; do not read its silence as "
                           "physics. Move the launch onto a road.\n";
                }
            }
        }
    }
    // ---- end [sweep] seat verification ---------------------------------------------------------

    const bool lbCrashing = lpPlayerCar->IsCrashing();
    const bool lbForced   = ( siFramesSinceShot >= siMaxFrames );
    if( !lbForced && !( siFramesSinceShot >= siSettleFrames && !lbCrashing ) )
    {
        return;
    }

    if( siNextShot >= siShotCount )
    {
        seStage = E_SWEEP_DONE;
        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "[sweep] done: " << siShotCount << " shots fired\n";
        }
        return;
    }

    const f32     lfHeading  = safHeadingDeg[ siNextShot ] * KF_SWEEP_DEG_TO_RAD;
    const Vector3 lDirection = Vector3{ std::sin( lfHeading ), 0.0f,
                                        std::cos( lfHeading ), 0.0f };

    // ⭐ THE ONE CALL -- the same one the teleport makes, with the console's own speed
    // argument. Everything the move does after this is the console's code.
    lpPlayerCar->RequestPlaceOnTrack( saLaunch[ siNextShot ], lDirection,
                                      safSpeed[ siNextShot ] );

    if( CgsDev::Log::gpDebugPrint != 0 )
    {
        *CgsDev::Log::gpDebugPrint
            << "[sweep] shot " << siNextShot << "/" << siShotCount
            << " heading " << safHeadingDeg[ siNextShot ]
            << " speed " << safSpeed[ siNextShot ]
            << " launch (" << saLaunch[ siNextShot ].x << ", " << saLaunch[ siNextShot ].y
            << ", " << saLaunch[ siNextShot ].z << ")"
            << " sweepFrame " << siFrame
            << " forced " << ( lbForced ? 1 : 0 )
            << " wasCrashing " << ( lbCrashing ? 1 : 0 )
            << " from (" << lrHere.x << ", " << lrHere.y << ", " << lrHere.z << ")\n";
    }

    // arm the seat check for this shot (consumed three frames from now; see the block above)
    siSeatCheck = 3;
    sSeatWanted = saLaunch[ siNextShot ];
    siSeatShot  = siNextShot;

    ++siNextShot;
    siFramesSinceShot = 0;
}

} // namespace BrnWorld
