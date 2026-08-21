#ifndef BRN_WORLD_PLACE_ON_TRACK_MANAGER_H
#define BRN_WORLD_PLACE_ON_TRACK_MANAGER_H

#include "types.hpp"
#include "BrnCommonTypes.h"   // Vector4
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex

// =============================================================================
// BrnWorld::PlaceOnTrackManager
//   GameSource/World/BrnPlaceOnTrackManager.{h,cpp}
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The ONE recovered function is
// ComputeBestPlaceOnT @ 0x822BE238 — given a query point and a list of candidate
// "place on track" nodes, it selects the nearest node, optionally restricted to nodes
// that pass a height test.
//
// LAYOUT NOTE / FLAG (modelled from the access pattern, no DWARF): the candidate list
// and node types have NO DWARF/leak home. Their shape is recovered ENTIRELY from the
// X360 access pattern in ComputeBestPlaceOnT:
//   * the list (`a2`) holds a node count at +0x04 and an inline node array at +0x10;
//   * each node is 64 bytes (`_R11 += 0x40` per iteration);
//   * node+0x00 is the position (a 16-byte vector, read with `lvx128`);
//   * node+0x30 holds a 16-bit flag word; bit 14 (`>> 14 & 1`) is the height-filter
//     gate (when the `a3` filter is on, a node is considered only if bit 14 is clear).
// These offsets are X360-attested by the loads; the FIELD NAMES are descriptive (the
// source names are unknown). Absolute offsets are NOT static_asserted (host vs X360
// pointer/vector widths differ); the 64-byte node stride is reproduced via the array
// element type. FLAG: PlaceOnTrackCandidate / PlaceOnTrackCandidateList are modelled
// minimally from the access pattern — grow them additively when their own TU lands.
// =============================================================================

// =============================================================================
// ⛔⛔ CORRECTION 2026-08-01 (drivable wave) -- WHAT THIS FILE MODELS.
//
// The banner above says the candidate list/node shapes were "recovered ENTIRELY from the
// X360 access pattern" with "NO DWARF/leak home". Both halves are now identified, and the
// identification is exact:
//
//   ComputeBestPlaceOnT @0x822BE238  ==  PlaceOnTrackManager::ComputeBestPlaceOnTrackIntersection
//        (DWARF BrnPlaceOnTrackManager.h:65, 3 args: the line-test result, the query
//         position, and a bool the Feb-2007 tree calls lbIgnoreFatal)
//   PlaceOnTrackCandidateList        ==  CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult
//   PlaceOnTrackCandidate            ==  CgsSceneManager::LineTestIntersection
//   muFlags bit 14                   ==  a CollisionTag bit (the "fatal surface" gate the
//                                        Feb-2007 body reads through CollisionTag)
//
// ⚠️ AND THE IDENTIFICATION EXPOSES A CONFLICT WITH A COMMITTED SIBLING. This header puts
// the intersection array at result+0x10; CgsSceneManagerIO_LineTestFineResult.hpp:62 puts
// it at result+0x38, and BOTH claim X360 attestation. The ARTIST asm of
// PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 settles it: the walk starts at word
// index 4 (`v21 = v4 + 4`, i.e. +0x10) and strides 16 words (64 bytes). **+0x10 is right**
// and the sibling's +0x38 is wrong -- it was fitted from ProcessLineTestFineResult
// @0x822D9FF8 alone. NOT changed here (the sibling is another group's file and its only
// consumer, the trigger module, is itself stubbed); recorded so the next wave to touch the
// scene fine-query round trip fixes ONE of the two rather than inventing a third.
//
// The two shapes are deliberately NOT merged into the CgsSceneManager types this wave: the
// whole fine-line-test round trip is severed in five places (nothing produces an
// OutEventLineTestFineResult on PC at all), so merging would only propagate the disputed
// offset. Merge when the round trip lands.
// =============================================================================

namespace BrnWorld
{
class RaceCarEntityModule;
class ActiveRaceCar;
namespace RaceCarEntityModuleIO
{
    struct InputBuffer_PrePhysics;
    struct OutputBuffer_PrePhysics;
}

// One candidate place-on-track node (64-byte stride, per the X360 loop).
struct PlaceOnTrackCandidate
{
    Vector4 mPosition;        // +0x00  node position (read via lvx128)
    // +0x10 NAMED 2026-08-01: PlaceOnTrackManager::PrePhysicsUpdate @0x822F6DF8 loads the
    // SECOND vector of the chosen record (`lvx128 v127, r30, 16`) as the reset NORMAL --
    // matching the Feb-2007 LineTestIntersection::mNormal. ComputeBestPlaceOnT itself
    // never touches it, which is why the earlier pass left it reserved.
    Vector4 mNormal;          // +0x10
    u8      maReserved20[16]; // +0x20..+0x2F  (line param / material+group tags)
    u16     muFlags;          // +0x30  flag word; bit 14 = height-filter gate
    u8      maReserved32[14]; // +0x32..+0x3F  pad to the 64-byte node stride
};

// The candidate list: a count and an inline node array. (X360 list base `a2`:
// muNumCandidates @ +0x04, maCandidates @ +0x10.)
struct PlaceOnTrackCandidateList
{
    u8                    maReserved00[4];  // +0x00 (untouched here)
    s32                   muNumCandidates;  // +0x04
    u8                    maReserved08[8];  // +0x08..+0x0F (untouched here)
    PlaceOnTrackCandidate maCandidates[1];  // +0x10  (flexible inline array)
};

// Bit 14 of PlaceOnTrackCandidate::muFlags. When the height filter is requested, a
// candidate is eligible only if this bit is CLEAR.
const u16 KU_PLACE_ON_TRACK_FILTER_BIT = (1u << 14);

class PlaceOnTrackManager
{
public:
    // ComputeBestPlaceOnT @ 0x822BE238 — pick the best candidate for lrQuery.
    //
    //   * lbApplyHeightFilter: when true, candidates whose filter bit (muFlags bit 14)
    //     is set are excluded from consideration; when false every candidate is
    //     eligible.
    //   * Returns the nearest eligible candidate that ALSO passes the height test if
    //     one exists; otherwise the nearest eligible candidate; otherwise nullptr
    //     (empty list / nothing eligible).
    //
    // The X360 ranks candidates by squared distance from the query point (the
    // `vmsum3fp128` 3-lane dot product of (node - query)). The height test compares
    // (query.y + 1.0) > node.y (the +1.0 tolerance is the shared rodata constant
    // flt_82001C98, attested as the literal 1.0f elsewhere) and only narrows the
    // preferred (height-passing) pick among eligible candidates.
    const PlaceOnTrackCandidate* ComputeBestPlaceOnT(
        const PlaceOnTrackCandidateList& lrCandidates,
        const Vector4& lrQuery,
        bool lbApplyHeightFilter) const;

    // ------------------------------------------------------------------------
    // Construct @ 0x822EA188 -- store the owning module, then seed mRandom
    // (CgsNumeric::Random::Construct + one RandomFloat, per the DWARF call list).
    // ------------------------------------------------------------------------
    void Construct(RaceCarEntityModule* lpRaceCarEntityModule);

    // ------------------------------------------------------------------------
    // PrePhysicsUpdate @ 0x822F6DF8 -- drain the scene-result queue, and for every
    // fine-line-test result whose query owner is KI_LINE_TEST_OWNER, place that car.
    // This is the ONLY caller of RaceCarEntityModule::ResetActiveRaceCar, which in turn
    // is the ONLY writer of ActiveRaceCar::E_STATE_ACTIVE in the whole XEX.
    // ------------------------------------------------------------------------
    void PrePhysicsUpdate(const RaceCarEntityModuleIO::InputBuffer_PrePhysics* lpInput,
                          RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);

private:
    // The tail of PrePhysicsUpdate's per-result body (asm 0x822F7274..0x822F7898), factored
    // so the bring-up leg below can run the CONSOLE's own code with the console's own
    // "no intersection found" input rather than paraphrasing it.
    void PlaceCarOnTrack(EActiveRaceCarIndex leActiveRaceCarIndex,
                         const PlaceOnTrackCandidate* lpBestIntersection,
                         RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);

    // [FLAG PC bring-up] NOT an X360 function. See the .cpp banner.
    void ApplyPendingRequestsWithoutSceneQueryBringUp(
                         RaceCarEntityModuleIO::OutputBuffer_PrePhysics* lpOutput);

    // [teleport] NOT an X360 function -- the harness `BRN_CAR_TELEPORT` trigger. It issues ONE
    // ActiveRaceCar::RequestPlaceOnTrack and nothing else; every store the move makes is the
    // console's own. PERMANENT harness capability, not a bring-up shim. See the .cpp banner.
    void ArmCarTeleportBringUp();

    // BrnPlaceOnTrackManager.h:67 (DWARF). Set by Construct.
    RaceCarEntityModule* mpRaceCarEntityModule;

    // BrnPlaceOnTrackManager.h:69 (DWARF) -- CgsNumeric::Random mRandom. Only
    // GetValuesForCarSelect (the car-select drop, not reconstructed) consumes it; kept as
    // correctly-named opaque storage rather than dragging CgsRandom into this header.
    u8 maRandomStorage[16];
};

// BrnPlaceOnTrackManager.h:57..59 (DWARF). The line test runs KF_LINE_TEST_LENGTH metres
// up and the same distance down through the requested position; KI_LINE_TEST_OWNER is the
// SceneQueryId owner byte PrePhysicsUpdate filters on (`BYTE1(*event) != 5`).
//
// ⛔ CORRECTED 2026-08-02 (car-placement wave): this was 1000.0f, which no consumer ever
// read, so nothing caught it. The console's value is the rodata float flt_820138DC that
// PlaceOnTrackManager::PostSceneUpdate @0x822D3168 splats into the .y lane of the two
// stack vectors it adds to / subtracts from mPlaceOnTrackPosition:
//     0x822D3244  lfs  f0, flt_820138DC@l(r27)
//     0x822D3250  stfs f0, var_12C(r1)      ; lineStart.y offset
//     0x822D3270  stfs f0, var_13C(r1)      ; lineEnd.y   offset
//     0x822D3290  vaddfp128 v127, v0, v13   ; mLineStart = pos + (0, +len, 0)
//     0x822D32A8  vsubfp128 v126, v0, v12   ; mLineEnd   = pos - (0, +len, 0)
// flt_820138DC == 50.0f, cross-checked at four unrelated call sites that print it as a
// literal (PropCollisions::UpdateLocatorVfx, BoostBurnout5::Prepare,
// ModeManagerDebugComponent::RenderHUD, RaceCarEntityModuleDebugComponent::
// RenderTrafficRelated). So the console's place-on-track line is 100 m long, not 2 km.
const f32 KF_PLACE_ON_TRACK_LINE_TEST_LENGTH = 50.0f;
const f32 KF_PLACE_ON_TRACK_Y_OFFSET         = 1.0f;
const u8  KI_PLACE_ON_TRACK_LINE_TEST_OWNER  = 5;

} // namespace BrnWorld

#endif // BRN_WORLD_PLACE_ON_TRACK_MANAGER_H
