#include "GameSource/World/BrnPlaceOnTrackManager.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/Math/BrnMathUtils.h"             // BrnMath::BuildTransform / IsNormal
#include "rw/math/vpu/matrix44affine_operation.h"     // rw::math::vpu::IsValid(Matrix44Affine)
#include "rw/math/vpu/vector3_operation.h"            // rw::math::vpu::Cross / Normalize (GetValuesForCarSelect)
#include "SDKs/EATech/include/rw/math/vpu/vec_float.h"   // rw::math::vpu::VecFloat (Random::RandomVecFloat)
#include "GameShared/GameClasses/Core/CgsAssert.h"    // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint

#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h" // InEventLineTestFine (PostSceneUpdate)

// (2026-09-24, FX-GEOMETRIC) the four includes the retired WORLDCOL.BIN drop query needed --
// CgsDeviceManager.h, CgsFileSystem.h, CgsResourceBundle2.h, CgsPolygonSoup.h -- went with it.
#include <cstdio>   // [teleport] sscanf (the BRN_CAR_TELEPORT spec parse)
#include <cstdlib>  // [teleport] getenv
#include <cfloat>   // FLT_MAX
#include <cstring>  // [sweep] strchr
#include <cmath>    // sinf / cosf / std::sqrt

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
// tolerance"). flt_82014430 READ from the image 2026-09-18 (tools/re/x360rd.py): 0x37800000
// == 2^-16 == 1.52587890625e-05, not the 1e-4 rw convention this file carried until then.
// ---------------------------------------------------------------------------
static const f32 KF_PLACE_ON_TRACK_SIMILAR_EPSILON = 1.52587890625e-05f;

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
// The car-select drop tables -- DWARF BrnPlaceOnTrackManager.cpp:32/:34/:41/:48 (names from
// the PS3 DecFIGS export of GetValuesForCarSelect @0x1331E8). On X360 they are .bss splat
// slots (0x82FAD400 / 0x82FAD5B0 / 0x82FAD560 / 0x82FAD4E0) filled by the CRT dyn-init thunks
// @0x82C4BF10 / 0x82C4BF38 / 0x82C4BFC8 / 0x82C4C058 from these rodata floats (found with
// tools/re/findinit.py, read with tools/re/ppcdis.py + x360rd.py, 2026-09-18):
//   flt_82001C94 = 6.2831855   flt_82014A98 = 0.2   flt_820148D4 = 0.55
//   flt_8201497C = 0.05        flt_820147E0 = 0.1   flt_82001C98 = 1.0   flt_82001CC0 = 0.0
// Indexed by ResetPlayerCarAction::CarSelectType (0 dont-drop, 1 normal, 2 shutdown).
// ---------------------------------------------------------------------------
static const f32 KVF_TWO_PI = 6.2831855f;
static const Vector3 KA_CAR_SELECT_NORMAL_ADD[3] =
{
    Vector3{ 0.0f, 0.0f, 0.0f,  0.0f },
    Vector3{ 0.0f, 0.0f, 0.2f,  0.0f },
    Vector3{ 0.0f, 0.0f, 0.55f, 0.0f },
};
static const Vector3 KA_CAR_SELECT_NORMAL_RANDOMISE[3] =
{
    Vector3{ 0.0f,  0.0f, 0.0f,  0.0f },
    Vector3{ 0.05f, 0.0f, 0.05f, 0.0f },
    Vector3{ 0.1f,  0.0f, 0.1f,  0.0f },
};
static const Vector3 K_CAR_SELECT_DROP_WORLD_RIGHT = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };

// ---------------------------------------------------------------------------
// XMMatrixRotationAxis @0x82203610 (== XMMatrixRotationNormal(Normalize(axis), angle)) followed
// by the console's row-vector transform of a vector by that matrix (asm 0x822D35D0..0x822D3608:
// vspltw x/y/z ; row0*x ; vmaddfp row1*y ; vmaddfp row2*z). Rows of XMMatrixRotationNormal with
// s = sin, c = cos, t = 1 - c and n the unit axis:
//   r0 = ( t.nx.nx + c,     t.nx.ny + s.nz,  t.nx.nz - s.ny )
//   r1 = ( t.nx.ny - s.nz,  t.ny.ny + c,     t.ny.nz + s.nx )
//   r2 = ( t.nx.nz + s.ny,  t.ny.nz - s.nx,  t.nz.nz + c    )
// ---------------------------------------------------------------------------
static Vector3 RotateAboutAxis( const Vector3& lrV, const Vector3& lrAxis, f32 lfAngle )
{
    const Vector3 lN = rw::math::vpu::Normalize( lrAxis );
    const f32 lfS = sinf( lfAngle );
    const f32 lfC = cosf( lfAngle );
    const f32 lfT = 1.0f - lfC;
    const f32 lfX = lN.x, lfY = lN.y, lfZ = lN.z;

    const Vector3 lRow0 = Vector3{ lfT * lfX * lfX + lfC,       lfT * lfX * lfY + lfS * lfZ, lfT * lfX * lfZ - lfS * lfY, 0.0f };
    const Vector3 lRow1 = Vector3{ lfT * lfX * lfY - lfS * lfZ, lfT * lfY * lfY + lfC,       lfT * lfY * lfZ + lfS * lfX, 0.0f };
    const Vector3 lRow2 = Vector3{ lfT * lfX * lfZ + lfS * lfY, lfT * lfY * lfZ - lfS * lfX, lfT * lfZ * lfZ + lfC,       0.0f };

    return Vector3{ lRow0.x * lrV.x + lRow1.x * lrV.y + lRow2.x * lrV.z,
                    lRow0.y * lrV.x + lRow1.y * lrV.y + lRow2.y * lrV.z,
                    lRow0.z * lrV.x + lRow1.z * lrV.y + lRow2.z * lrV.z, 0.0f };
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
// Construct @0x822EA188: store the module, then the two mRandom calls the DecFIGS call list
// names -- CgsNumeric::Random::Construct (index 0, seed, slot 0 = 1.0, seven refills, bump)
// and ONE RandomFloat to prime the stream (the pseudocode's trailing draw + `(idx + 1) & 7`).
// Landed 2026-09-18 with GetValuesForCarSelect, mRandom's only consumer.
// ===========================================================================
void PlaceOnTrackManager::Construct(RaceCarEntityModule* lpRaceCarEntityModule)
{
    mpRaceCarEntityModule = lpRaceCarEntityModule;
    mRandom.Construct();
    mRandom.RandomFloat();
}

// ---------------------------------------------------------------------------
// The query's two fixed flags. Both are literals in the console body; the world bit has a DWARF
// name in a header this file does not own (MOVE-WHEN CgsTriangleCacheManager.h carries it).
//   K_ENTITY_TYPE_FLAG_WORLD   `li r17, 2`    @0x822D31C8 -- DWARF CgsSceneManager::
//                              K_ENTITY_TYPE_FLAG_WORLD = 2 (CgsTriangleCacheManager.h:44): the
//                              scene answers from the static world only (ProcessLineTestFine's
//                              world-only arm -> the triangle-collision line test)
//   KU8_ALL_VOLUME_TYPE_FLAGS  `li r18, 0xFF` @0x822D31D8 -- every volume type (no DWARF name)
// ---------------------------------------------------------------------------
static const u32 K_ENTITY_TYPE_FLAG_WORLD  = 2u;
static const u8  KU8_ALL_VOLUME_TYPE_FLAGS = 0xFFu;

// ===========================================================================
// PostSceneUpdate @ 0x822D3168   (crash parity FX-GEOMETRIC, 2026-09-24 -- no body before)
// DWARF BrnPlaceOnTrackManager.h:49 / .cpp:101..:131 (locals leActiveRaceCarIndex, lpActiveRaceCar,
// lSceneQueryID, lEvent); PS3 twin 0x14A31C (the same order, the debug push out of line).
//
// THE PRODUCER of the place-on-track query. Every other link of the round trip was already live
// (RaceCarEntityModule::PostSceneUpdate's slot, BridgeRaceCarModuleToSceneModule_PostScene,
// SceneManagerModule::ProcessLineTestFine / ProcessTriangleCollisionLineTests, the result bridge,
// the PrePhysicsUpdate walk below) or landed today (CollideLineAgainstPolySoupList's two Geometric
// kernels, 94bcd871). Until now the PC answered requests with ApplyPendingRequestsWithoutScene-
// QueryBringUp, a WORLDCOL.BIN walk of its own -- RETIRED in this same change (see the note at the
// walk): the console's walk places on EVERY owner-5 answer without re-testing mbToBePlacedOnTrack,
// so the two together would have placed a car twice.
//
//   0x822D3210  for slot 0..7 (the BurnoutConstants.h:39 post-increment tripwire @0x822D3428):
//   0x822D321C    car = mpRaceCarEntityModule->GetActiveRaceCar(slot)
//   0x822D3224    IsAttached() && lbz +0x7C4 (mbToBePlacedOnTrack), else next slot
//   0x822D3244    offset = (0, flt_820138DC == 50.0, 0, int 0) -- KF_PLACE_ON_TRACK_LINE_TEST_LENGTH
//   0x822D3290    lEvent.mLineStart = lvx +0x7A0 (mPlaceOnTrackPosition) + offset   (vaddfp128, 4 lanes)
//   0x822D32A8    lEvent.mLineEnd   = mPlaceOnTrackPosition - offset                 (vsubfp128)
//   0x822D3254    lEvent.mQueryId   = `clrlwi 16 ; oris 5` == Set(KI_LINE_TEST_OWNER, slot)
//                 lEvent.mx32EntityTypeFlags = 2, mExcludeEntityId = -1 (SetInvalid),
//                 meExclusionMode = 0 (E_EXCLUDE_ENTITY_ONLY), mxVolumeTypeFlags = 0xFF
//   0x822D32C0    (gxMessageFilterFlags & 1) "[PLACEONTRACK] Generating line test to place race car "
//                 << slot << " on track\n"; then "    lEvent.mLineStart=" "(%f, %f, %f)"
//                 ", lEvent.mLineEnd=" "(%f, %f, %f)" "\n" (off_82F31964 is the format)
//   0x822D33D4    lpOutput->GetSceneFineLineTestQueue() -- the write accessor 0x822B5560 with its
//                 "Not locked for writing" tripwire (BrnRaceCarEntityModuleIO.h:386) -- ->AddEvent
//                 (BaseEventQueue<InEventLineTestFine>::AddEvent 0x822105C0)
//   0x822D33E0    stvx128 start/end -> module + 0x17CF0 + 0x70/+0x80: RaceCarEntityModuleDebugComponent::
//                 PushLineTest(start, end) inlined (PS3 calls it @0x123E2C). [FLAG] the debug component
//                 is not in this tree -- a debug-render history only, nothing reads it back.
//   0x822D33FC    (gxMessageFilterFlags & 1) "[PLACEONTRACK] Line test requested\n"
// The flag is NOT cleared here: PlaceCarOnTrack clears it when the answer is consumed, later in the
// same frame (WorldModule::EntityModulePostSceneUpdate runs the race-car scene queries straight after
// this update and bridges the results into the pre-physics input the walk reads).
// ===========================================================================
void PlaceOnTrackManager::PostSceneUpdate( RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput )
{
    for( EActiveRaceCarIndex leActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leActiveRaceCarIndex++ )
    {
        ActiveRaceCar* lpActiveRaceCar = mpRaceCarEntityModule->GetActiveRaceCar( leActiveRaceCarIndex );

        if( !lpActiveRaceCar->IsAttached() || !lpActiveRaceCar->ToBePlacedOnTrack() )
        {
            continue;
        }

        CgsSceneManager::SceneQueryId lSceneQueryID;
        lSceneQueryID.Set( KI_PLACE_ON_TRACK_LINE_TEST_OWNER, static_cast<u16>( leActiveRaceCarIndex ) );

        // The console builds both offsets as (0, 50, 0, int 0) and adds / subtracts all four lanes.
        const Vector3& lrPosition = lpActiveRaceCar->GetPlaceOnTrackPosition();
        const Vector3  lOffset    = Vector3{ 0.0f, KF_PLACE_ON_TRACK_LINE_TEST_LENGTH, 0.0f, 0.0f };

        CgsSceneManager::SceneManagerIO::InEventLineTestFine lEvent;
        lEvent.mLineStart = Vector3{ lrPosition.x + lOffset.x, lrPosition.y + lOffset.y,
                                     lrPosition.z + lOffset.z, lrPosition.w + lOffset.w };
        lEvent.mLineEnd   = Vector3{ lrPosition.x - lOffset.x, lrPosition.y - lOffset.y,
                                     lrPosition.z - lOffset.z, lrPosition.w - lOffset.w };
        lEvent.mQueryId            = lSceneQueryID;
        lEvent.mx32EntityTypeFlags = K_ENTITY_TYPE_FLAG_WORLD;
        lEvent.mExcludeEntityId.SetInvalid();
        lEvent.meExclusionMode     = CgsSceneManager::SceneManagerIO::E_EXCLUDE_ENTITY_ONLY;
        lEvent.mxVolumeTypeFlags   = KU8_ALL_VOLUME_TYPE_FLAGS;

        if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "[PLACEONTRACK] Generating line test to place race car "
                                       << static_cast<s32>( leActiveRaceCarIndex ) << " on track\n";
        }
        if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "    lEvent.mLineStart=";
            CgsDev::Log::gpDebugPrint->AppendFormat( "(%f, %f, %f)", lEvent.mLineStart.x,
                                                     lEvent.mLineStart.y, lEvent.mLineStart.z );
            *CgsDev::Log::gpDebugPrint << ", lEvent.mLineEnd=";
            CgsDev::Log::gpDebugPrint->AppendFormat( "(%f, %f, %f)", lEvent.mLineEnd.x,
                                                     lEvent.mLineEnd.y, lEvent.mLineEnd.z );
            *CgsDev::Log::gpDebugPrint << "\n";
        }

        lpOutput->GetSceneFineLineTestQueue()->AddEvent( lEvent );

        // [FLAG] RaceCarEntityModuleDebugComponent::PushLineTest( lEvent.mLineStart, lEvent.mLineEnd )
        // -- inlined on the console (module + 0x17CF0, +0x70 / +0x80); the component is absent here.

        if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "[PLACEONTRACK] Line test requested\n";
        }
    }
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
// OWNER is KI_LINE_TEST_OWNER (5). The console reads the id WORD and extracts the owner as
// bits [16..23] (`extrwi 8,8` @0x822F6F78) and the asking slot as the low half (`clrlwi 16`
// @0x822F6F84) -- SceneQueryId::GetOwner / GetIndex, inlined. (Hex-Rays prints the owner test
// as `BYTE1(*v4) != 5`, a big-endian MEMORY byte; this file transcribed that literally as
// byte [1] until 2026-09-24, which on this host is bits [8..15] and never matched.)
//
// ⚠️ VERSION DRIFT vs the earlier source revision; the retail binary is authority on all three:
//   * the transform is built by BrnMath::BuildTransform(pos, direction, normal), NOT by
//     the hand-rolled Normalize/Cross chain the earlier revision shows;
//   * the velocity handed to ResetActiveRaceCar is lResetDirection * speed, NOT
//     lTransform.ZAxis() * speed;
//   * ARTIST adds the two "normal and reset direction are too similar" guards and the
//     car-select branch (GetValuesForCarSelect), neither of which exists in 2007.
// ===========================================================================
void PlaceOnTrackManager::PrePhysicsUpdate(
        const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
        RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput )
{
    // [teleport] the harness teleport, armed here. Its request is answered by the console's own
    // round trip: the NEXT frame's PostSceneUpdate posts the line test, the scene answers it and
    // that frame's walk below places the car (one frame later than the retired bring-up did).
    // See the block at the bottom of this file.
    ArmCarTeleportBringUp();

    // [sweep] the deterministic crash sweep, armed on the same frame and answered the same way.
    // See the block at the bottom of this file. Inert unless BRN_CRASH_SWEEP is set.
    ArmCrashSweepBringUp();

    const RaceCarEntityModuleIO::SceneResultQueue* lpSceneResultQueue =
        lpInput->GetSceneResultQueue();

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpSceneResultQueue->GetFirstEvent( &lpEvent, &liSize );

    while( lpEvent != 0 )
    {
        // The result record: the scene-result queue hands back a pointer into its own packed
        // byte buffer (the sanctioned external-byte-stream case).
        const PlaceOnTrackCandidateList* lpLineTestResult =
            reinterpret_cast<const PlaceOnTrackCandidateList*>( lpEvent );

        // 0x822F6F6C `cmpwi r3, 1` (the event type) ; 0x822F6F74 `lwz r11, 0(r22)` ;
        // 0x822F6F78 `extrwi r10, r11, 8, 8` ; 0x822F6F7C `cmplwi r10, 5` -- the query id's OWNER,
        // bits [16..23] of the WORD. CORRECTED 2026-09-24 (FX-SCENEMGR): this read the raw bytes
        // [1] (owner) and [0] (slot) -- Hex-Rays' BYTE1 is a big-endian memory byte. On this host
        // byte 1 of Set(5, slot) == 0x0005000S is bits [8..15] == 0, so no place-on-track result
        // could ever match, and a foreign id with 5 in bits [8..15] (0x0000050S) would have been
        // taken for slot S's answer.
        if( liType == KI_OUT_EVENT_LINE_TEST_FINE_RESULT
            && lpLineTestResult->mQueryId.GetOwner() == KI_PLACE_ON_TRACK_LINE_TEST_OWNER )
        {
            // 0x822F6F84 `clrlwi r20, r11, 16` -- the query id's INDEX is the slot that asked.
            const EActiveRaceCarIndex leActiveRaceCarIndex =
                static_cast<EActiveRaceCarIndex>( lpLineTestResult->mQueryId.GetIndex() );

            const ActiveRaceCar* lpActiveRaceCar =
                mpRaceCarEntityModule->GetActiveRaceCar( leActiveRaceCarIndex );

            // `v9 = *(module + 100041) == 0` -- the console's lbIgnoreFatal is
            // !mbInCarSelectScreen.
            const bool lbIgnoreFatal = !mpRaceCarEntityModule->IsInCarSelectScreen();

            // 0x822F6FBC..0x822F7048 -- the console's own two prints (gxMessageFilterFlags & 1),
            // added 2026-09-24 with the producer: they are what a log shows for every answer, an
            // empty one included. (The per-intersection "      Intersection 0: " << record lines that
            // follow on the console print through CgsSceneManager::operator<<(LineTestIntersection),
            // which this tree has no body for -- not reproduced.)
            if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint << "[PLACEONTRACK] Received line test result for race car "
                                           << static_cast<s32>( leActiveRaceCarIndex ) << "\n";
            }
            if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint << "    lpLineTestResult->miNumIntersections="
                                           << lpLineTestResult->muNumCandidates << "\n";
            }

            // [FLAG] 0x822F7058..0x822F70A4: the console clears the RaceCarEntityModuleDebugComponent's
            // intersection history (`stw 0 -> module + 97824`) and Appends every intersection position
            // to its Vector3<10> list (module + 0x17CF0 + 144) -- debug render only; the component is
            // not in this tree.

            // The console passes the request position in v1 (`lvx128 v1, car, 1952`).
            const Vector3& lrQueryPosition = lpActiveRaceCar->GetPlaceOnTrackPosition();
            const Vector4  lQuery = Vector4{ lrQueryPosition.x, lrQueryPosition.y,
                                             lrQueryPosition.z, 0.0f };

            const PlaceOnTrackCandidate* lpBestIntersection = ComputeBestPlaceOnT(
                *lpLineTestResult, lQuery, lbIgnoreFatal );

            // The console's "    Selected intersection " << index << " as place on track destination\n"
            // (index = (best - &list.maCandidates[0]), `subf ; srawi 6`), printed before the car-select arm.
            if( lpBestIntersection != 0 && ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0
                && CgsDev::Log::gpDebugPrint != 0 )
            {
                *CgsDev::Log::gpDebugPrint << "    Selected intersection "
                                           << static_cast<s32>( lpBestIntersection - lpLineTestResult->maCandidates )
                                           << " as place on track destination\n";
            }

            PlaceCarOnTrack( leActiveRaceCarIndex, lpBestIntersection, lpOutput );
        }

        liType = lpSceneResultQueue->GetNextEvent( lpEvent, &lpEvent, &liSize );
    }

    // ⛔ RETIRED 2026-09-24 (crash parity FX-GEOMETRIC): the frame's tail used to run
    // ApplyPendingRequestsWithoutSceneQueryBringUp -- a PC-only answer to every pending request from
    // a WORLDCOL.BIN walk of its own, because nothing on this build produced the query above. The
    // console has no such tail: PostSceneUpdate @0x822D3168 is the producer (landed in the same
    // change) and this walk the only consumer. Keeping both would place a car TWICE (the walk does
    // not re-test mbToBePlacedOnTrack -- 0x822F6F6C..0x822F6F84 test only the type and the owner).
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

        // X360 0x822F7250..0x822F7274: `if (mbInCarSelectScreen)` the three vectors go
        // through GetValuesForCarSelect @0x822D3470 -- the junkyard DROP pose (the reset
        // position becomes the authored anchor above the yard floor and the car falls onto
        // its suspension). Landed 2026-09-18; until then every car swap seated the new car
        // on the ground intersection and it "appeared out of nowhere".
        if( mpRaceCarEntityModule->IsInCarSelectScreen() )
        {
            GetValuesForCarSelect( lpBestIntersection, lpActiveRaceCar,
                                   &lResetPosition, &lResetNormal, &lResetDirection );
            if( CgsDev::Log::gpDebugPrint != 0 )
                *CgsDev::Log::gpDebugPrint << "    Selecting special values for car select\n";
        }
        // The console's own "    Selected reset data: lResetPosition=" line (v205 in the
        // export), printed lane by lane; the intersection's own height beside it so a log
        // shows the DROP height at a glance.
        if( CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint << "    Selected reset data: lResetPosition=("
                << lResetPosition.x << ", " << lResetPosition.y << ", " << lResetPosition.z
                << ") lResetNormal=(" << lResetNormal.x << ", " << lResetNormal.y << ", "
                << lResetNormal.z << ") intersection.y=" << lpBestIntersection->mPosition.y
                << " carSelect=" << ( mpRaceCarEntityModule->IsInCarSelectScreen() ? 1 : 0 )
                << " resetType=" << mpRaceCarEntityModule->GetCarSelectResetType() << "\n";
        }
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
        // requested position is used instead, which is what the earlier revision's own
        // no-intersection arm does and is exactly the pose the request carried.
        // ⭐ The ring itself is now WRITTEN per frame (ActiveRaceCar::UpdateResetTransform, landed
        // 2026-08-26), but it only fills while the car is inside the AI section system, which
        // needs the above-ground line-test round trip -- see that function's banner.
        // DELETE-WHEN the ring fills on a booted drive AND the scene fine-query round trip
        // produces real results (at which point this arm stops being the one that runs).
        // ⚠️ 2026-09-24 (FX-GEOMETRIC): the round trip DOES produce real results now
        // (PostSceneUpdate + the scene's CollideLineAgainstPolySoupList), so this arm runs only
        // when the console's 100 m single-sided line genuinely finds no usable surface. The park
        // itself is unchanged (not this change's to retire); the console's own print below says
        // when it runs, so a live log can measure it.
        if( ( CgsDev::Message::gxMessageFilterFlags & 1 ) != 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            *CgsDev::Log::gpDebugPrint
                << "    Failed to find valid place on track location - reverting to ring buffer\n";
        }
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

    // X360 0x822F7808..0x822F7838: for the PLAYER's car (`index == GetActiveRaceCar(
    // mePlayerActiveRaceCarIndex)->meActiveRaceCarIndex`) the console posts game event 13
    // -- one byte, mbInCarSelectScreen -- into the output buffer's game-event queue (+149312,
    // the "Not locked for writing" accessor @0x822B5EA0). Posted since 2026-09-18.
    // [FLAG] no PC consumer names id 13 yet (GameStateModule::ProcessGameEvents has no arm).
    {
        const ActiveRaceCar* lpPlayerCar = mpRaceCarEntityModule->GetActiveRaceCar(
            mpRaceCarEntityModule->GetPlayerActiveRaceCarIndex() );
        if( lpPlayerCar != 0 && lpPlayerCar->GetActiveRaceCarIndex() == leActiveRaceCarIndex )
        {
            const u8 lbInCarSelectScreen = mpRaceCarEntityModule->IsInCarSelectScreen() ? 1 : 0;
            lpOutput->GetGameEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>( &lbInCarSelectScreen ), 13, 1 );
        }
    }

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
// GetValuesForCarSelect @0x822D3470 (DWARF BrnPlaceOnTrackManager.cpp:359; PS3 DecFIGS
// @0x1331E8 with the table names). The car-select DROP pose, landed 2026-09-18.
//
//   if (meCarSelectResetType && meCarSelectResetType < 3)            `v14 && v14 < 3`
//     angle     = RandomVecFloat() * KVF_TWO_PI                        [0, 2pi)
//     rot       = XMMatrixRotationAxis(axis = lResetNormal, angle)     (the GROUND normal)
//     normal    = (lResetNormal + KA_CAR_SELECT_NORMAL_ADD[type]) * rot
//     GetRandomVector(mRandom, normal, normal, KA_CAR_SELECT_NORMAL_RANDOMISE[type])
//     normal    = Normalize(normal)                                    vmsum3fp + vrsqrtefp
//     if (type == 2) direction = Normalize(Cross(normal, K_CAR_SELECT_DROP_WORLD_RIGHT))
//     position  = lpActiveRaceCar->mPlaceOnTrackPosition               `lvx128 v0, r26, 1952`
//
// The position is the REQUEST anchor, not lpBestIntersection->mPosition: the junkyard's
// authored car-select anchors sit 3.3-6.2 m above their own floor, so the car is released
// in the air with a small random tilt and the vehicle physics catches it.
// ===========================================================================
void PlaceOnTrackManager::GetValuesForCarSelect(
        const PlaceOnTrackCandidate* lpBestIntersection,
        const ActiveRaceCar* lpActiveRaceCar,
        Vector3* const lResetPosition,
        Vector3* const lResetNormal,
        Vector3* const lResetDirection )
{
    CGS_ASSERT( lpBestIntersection != 0, "lpBestIntersection" );   // :361
    CGS_ASSERT( lpActiveRaceCar != 0,    "lpActiveRaceCar" );      // :362
    if( lpActiveRaceCar == 0 )
    {
        return;
    }

    const s32 liType = mpRaceCarEntityModule->GetCarSelectResetType();   // +0x186CC (100044)
    if( liType != 0 && liType < 3 )
    {
        const f32 lfAngle = mRandom.RandomVecFloat().GetFloat() * KVF_TWO_PI;

        const Vector3 lAxis = *lResetNormal;                       // `lvx128 v1, r0, r30` before the call
        Vector3 lNormal = Vector3{ lResetNormal->x + KA_CAR_SELECT_NORMAL_ADD[liType].x,
                                   lResetNormal->y + KA_CAR_SELECT_NORMAL_ADD[liType].y,
                                   lResetNormal->z + KA_CAR_SELECT_NORMAL_ADD[liType].z, 0.0f };
        lNormal = RotateAboutAxis( lNormal, lAxis, lfAngle );

        GetRandomVector( mRandom, lNormal, lNormal, KA_CAR_SELECT_NORMAL_RANDOMISE[liType] );
        *lResetNormal = rw::math::vpu::Normalize( lNormal );

        if( liType == 2 )
        {
            *lResetDirection = rw::math::vpu::Normalize(
                rw::math::vpu::Cross( *lResetNormal, K_CAR_SELECT_DROP_WORLD_RIGHT ) );
        }

        *lResetPosition = lpActiveRaceCar->GetPlaceOnTrackPosition();   // +0x7A0 (1952)
    }
}

// ===========================================================================
// BrnWorld::GetRandomVector @0x822BE358 (DWARF BrnPlaceOnTrackManager.cpp:418).
// out = in + (RandomFloat(-range.x, +range.x), RandomFloat(-range.y, +range.y),
//             RandomFloat(-range.z, +range.z)); the negated bounds are the `vslw`/`vxor` sign
// flips of the range's three splats. The console draws the Z lane FIRST, then Y, then X
// (PPC right-to-left evaluation of the three constructor arguments: the ring reads at
// 0x822BE3D8 / 0x822BE430 / 0x822BE47C feed `fmadds` into z, y, x respectively). Written out
// in that order so the ring advances identically on every compiler.
// ===========================================================================
void GetRandomVector( CgsNumeric::Random& lrRandom, Vector3& lrOut,
                      const Vector3& lrIn, const Vector3& lrRange )
{
    const f32 lfZ = lrRandom.RandomFloat( -lrRange.z, lrRange.z );
    const f32 lfY = lrRandom.RandomFloat( -lrRange.y, lrRange.y );
    const f32 lfX = lrRandom.RandomFloat( -lrRange.x, lrRange.x );
    lrOut = Vector3{ lrIn.x + lfX, lrIn.y + lfY, lrIn.z + lfZ, 0.0f };
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
//       -> PlaceOnTrackManager::PostSceneUpdate @0x822D3168 -- the 100 m vertical line test through
//          the request, answered by the scene's own fine line test over the resident world
//          collision (the PC's WORLDCOL.BIN bring-up that stood here until 2026-09-24 is retired),
//          which is why the Y a teleport lands on is a surface of the collision mesh and never a
//          number chosen here
//       -> PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 -- the same frame's answer
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
// teleport does not wait for it. The car lands on the world collision (WORLDCOL.BIN is resident
// whole in the scene manager -- ScriptedLoad stage 7 loads all 792 resources and builds the spacial
// partition the line test queries) so it never falls through the world, but the visible world and the
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
