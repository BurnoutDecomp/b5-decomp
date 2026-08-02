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

                // UnpackPolygonSoupVertices @0x8283B480: world = (pos_s32 + vert_s16) * scale.
                const s16* lpaPacked =
                    reinterpret_cast<const s16*>(lpcList + lpSoup->muVertices);
                f32 lafVerts[256][3];
                f32 lfMinX = 0.0f, lfMaxX = 0.0f, lfMinZ = 0.0f, lfMaxZ = 0.0f;
                for (s32 liVert = 0; liVert < liNumVerts; ++liVert)
                {
                    const s16* lpPacked = lpaPacked + liVert * 3;
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

} // namespace BrnWorld
