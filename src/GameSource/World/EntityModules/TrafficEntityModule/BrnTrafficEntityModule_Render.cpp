// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/
//   BrnTrafficEntityModule_Render.cpp
//
// The TRAFFIC render leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::TrafficEntityModule::PreDispatchUpdate     @ 0x8274D900   (EXPORT HOLE)
//   BrnTraffic::TrafficEntityModule::GenerateDispatchLists @ 0x8273B280
//   BrnTraffic::TrafficEntityModule::RenderTrafficCar      @ 0x82728B08   (3,252 ln)
//
// Wave T1 round 2, cluster R2B. Round 1's cluster C6 landed the BrnWorldModule wire and
// correctly refused to ship these three bodies without declarations; the declarations now
// exist in BrnTrafficEntityModule.h (this change), the streamer accessors are landed
// header inlines, and the two PreDispatch buffers have real public members.
//
// ---- SIGNATURES COME FROM THE DWARF + THE ASM, NOT FROM THE OLD DECLARATIONS -----
// All three parameter lists are the DecFIGS DWARF's verbatim ones
// (dwarfdump/_compile/BrnTrafficUnity.cpp :19315 / :23926 / :23103), each cross-checked
// against the X360 call site and the callee prologue. The two corrections the committed
// 6-arg GenerateDispatchLists declaration needed (arg 2 is the ARRAY inside the buffer, and
// four vector arguments were missing) are written out on the declaration itself.
//
// ---- THE RENDER MODEL: TRAFFIC IS THE RACE CAR'S TWIN, NOT THE LEAK'S ------------
// The Feb-2007 leak (BrnTrafficEntityModule.cpp:8497) is the STRUCTURAL KEY -- its flow maps
// one-for-one onto ship pseudocode :633-656 -- but its IO contract is stale in two ways and
// the ship's is what b5-decomp follows:
//   * the leak computes visibility + LOD inline over a stack RenderSortInfo[600]; the ship
//     split that into PreDispatchUpdate -> Array<VehicleRenderInfo,64> ->
//     WorldModule::CalculateVehicleLODs (which writes mLOD) -> GenerateDispatchLists;
//   * the leak submits through CgsGraphics::Model::Render / RenderZOnly; the ship submits
//     through DrawRenderable::AddToBin + DispatchList::Submit, i.e. exactly the idiom the
//     committed BrnRaceCarEntityModule_Render.cpp already carries, into the SAME three
//     dispatch lists (12 object / 19 opaque mesh / 20 transparent mesh).
// So this file mirrors the race-car render TU deliberately: same packet shape, same
// technique-bit scheme, same instanced wheel draw, same shadow-pass split.
//
// ---- WHAT IS REAL HERE, AND WHAT IS A NAMED GATE ---------------------------------
// REAL (this is the path that puts a parked car on screen):
//   * PreDispatchUpdate -- PARTIAL, one named gate (G7). ⭐ FIX ROUND 2026-08-21: the first
//     draft of this file called it "complete" while omitting, unnamed, two console behaviours
//     the DecFIGS DWARF spells out at :19315 -- the FastBitArray duplicate suppression and the
//     SPECIES-DISPATCHED liveness predicate. The dedup is now REAL; two of the three species
//     arms are REAL; the third (trailer) is gate G7. An unnamed predicate substitution under a
//     "complete" banner is exactly what this file's own house rule forbids.
//   * GenerateDispatchLists, complete apart from the corona pass (below).
//   * RenderTrafficCar's entry gates, asset/spec resolution, paint colour, the
//     pitch/roll-composed body transform, shader constants 20/21/22/23/24/26, the BODY-PART
//     loop (locator * bodyTransform -> DoesStateExist -> AddToBin -> Submit, shadow and
//     camera variants) and the WHEEL loop (per-wheel matrix from the deformation spec's
//     wheel positions/scales -> one instanced AddToBin).
// NAMED GATES, each with the exact blocker (none of them is a fabricated body):
//   G1 the replay-serialiser pose source (module +468256 selects it);
//   G2 the PHYSICAL (crashing) vehicle arm -- wheel rotations/positions from the crashing-
//      parts list, and the damaged verlet-offset upload;
//   G3 the glass-fracture reset + the damaged-vehicle budget leg;
//   G4 the detached-body-part override table;
//   G5 SubmitCoronasForVehicle @0x82727BB0 / RenderTrafficLightCoronas @0x8271EC80 (wave 2);
//   G6 BrnBlobbyShadowBuffer::AddShadow -- the buffer has NO OWNER on this build;
//   G7 RETIRED 2026-08-21 (wave T1 round 4 sweep). Its blocker -- "Vehicle::GetCabIndex has
//      no declaration anywhere in b5-decomp/src" -- was true and is no longer: GetCabIndex
//      @0x8270E4C8 (DWARF BrnTrafficVehicle.h:339) is declared and bodied, and
//      PreDispatchUpdate's trailer arm now runs the console's own cab-param test instead of
//      the documented stand-in. (It had been mis-filed under the CgsStrStream.h catch-all
//      primary_file, the same ledger defect that hid FindVehicleTypeAttribKey_EXPENSIVE and
//      Vehicle::SetHasEntity.)
//
// ---- FOUR UNRECOVERED .rodata CONSTANTS, PARKED NOT GUESSED ----------------------
// unk_8300D000 (the constant-24 scale), unk_8300C9A0 / unk_8300C8F0 (the wheel-blur speed
// scale pair) and unk_8300CC60 (the blur technique threshold) all read ZERO in the shipped
// image and are seeded by UNNAMED MSVC dynamic-initialiser thunks in 0x82C6xxxx, which are
// not functions in the IDA DB and therefore invisible to every per-function export scan.
// Per the wave's own rule (scratchpad recovered_constants.md): an export-JSON grep can NEVER
// prove "no dyn-init thunk exists" -- recovering them needs an XrefsTo walk in the .i64.
// That walk was run for five of these globals this wave and recovered all five; these four
// were not in that set. They are named, seeded 0.0f, FLAGGED, and their consequence is
// stated at each use. NOT guessed from the race car's numerically-similar twins.
// ============================================================================

#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"

#include "GameShared/GameClasses/Graphics/CgsModel.h"                       // Model / Renderable / State
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"             // ShaderConstantTable
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"         // DispatchFrame / DispatchBin / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // DrawRenderable::AddToBin
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // CgsDev::Log::gpDebugPrint
#include "GameSource/World/ShadowMap/BrnShadowMap.h"                        // BrnWorld::ShadowMap
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"                     // BrnVehicle::GraphicsSpec
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"                       // BrnWheel::GraphicsSpec
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"               // TrafficData
#include "SharedClasses/Traffic/BrnTrafficVehicleType.h"                    // VehicleTypeData::muAssetId
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                // CgsSceneManager::EntityId
#include "rw/math/vpu/matrix44affine_operation.h"                           // Mult / MakeRotationX/Y/Z / Inverse / TransformPoint

#include <cmath>    // powf / sqrtf
#include <cstdlib>  // getenv  (the [T1-dispatch] diagnostics only)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied by
// the CgsShaderConstants TU). Same extern the sibling render TUs carry.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

// dword_82CDB4A0 -- "Graphics/Vehicles.../Wheels to Render", shipped value 4. It is
// DELIBERATELY NON-STATIC in BrnRaceCarEntityModule_Render.cpp because THIS function shares
// it (the console re-reads it at pseudocode :2218 and :2798), so it is declared, not
// re-minted, here.
extern s32 giWheelsToRender;

namespace BrnTraffic
{

// ============================================================================
// FILE-SCOPE CONSTANTS
// ============================================================================

// The console's wheel matrix / pointer stack arrays are four entries long, which is also
// the debug variable's upper bound (identical to the race car's KU_WHEELS_TO_RENDER_MAX).
static const s32 KI_TRAFFIC_WHEELS_TO_RENDER_MAX = 4;

// The verlet-offset array length shader constant 22 declares (128 registers). Same block the
// race car publishes; see the deliberate-deviation note at the publish site.
static const u32 KU_VERLET_OFFSET_COUNT = 128u;

// g_NullVerletOffsets -- the leak names this global verbatim at BrnTrafficEntityModule.cpp
// :8580 (`SetShaderConstantArrayData( E_VERLET_OFFSETS, g_NullVerletOffsets )`). TRAFFIC
// NEVER DEFORMS, so its verlet block is identically zero; the console folds that into a
// shared all-zero .rodata block. Zero-initialised here, which is the value, not a stand-in.
static const Vector4 gaNullVerletOffsets[ KU_VERLET_OFFSET_COUNT ] = {};

// ---- THE FOUR UNRECOVERED DYN-INIT CONSTANTS (see the file banner) ----------------------
// ⛔ FLAG (unrecovered .rodata): all four read ZERO in the shipped image and are written by
// unnamed dyn-init thunks in 0x82C6xxxx that no per-function export carries. Named at their
// console addresses so the .i64 xref walk that recovers them has an exact target list.

// unk_8300D000 -- `lvx128 v0, r0, unk_8300D000 ; vmulfp128 v1, v118, v0` @ pseudocode :1291,
// i.e. constant 24 == lRearLights * splat(this). The race car's positional twin
// (unk_82FAD990 -> flt_82004C88) measured 8.0f; that is NOT imported here, because the two
// are different globals and "numerically similar neighbour" is not attestation.
// CONSEQUENCE TODAY: zero. Harmless while lRearLights is itself all-zero (its producer,
// SubmitCoronasForVehicle, is gate G5), but it will keep the traffic lamps dark AFTER wave 2
// lands that producer -- so this constant must be recovered in the same wave.
static const f32 KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY = 0.0f;

// unk_8300C9A0 and unk_8300C8F0 -- the wheel-spin blur pair:
//   `vmulfp128 v124, v117, unk_8300C9A0` (v117 == Vehicle::GetSpeed())
//   `vmulfp128 v0,   v124, unk_8300C8F0` -> vminfp against 1.0f -> constant 25 lane X.
static const f32 KF_TRAFFIC_WHEEL_SPIN_SPEED_SCALE = 0.0f;
static const f32 KF_TRAFFIC_WHEEL_BLUR_SCALE       = 0.0f;

// unk_8300CC60 -- `vcmpgtfp128. v0, unk_8300CC60, v124`: a wheel turning slower than this
// selects the non-blurred technique (the exact shape of the race car's
// `gvWheelBlurConstants.x > angularVelocity` test).
static const f32 KF_TRAFFIC_WHEEL_BLUR_THRESHOLD = 0.0f;

// ============================================================================
// PreDispatchUpdate  @ 0x8274D900   (EXPORT HOLE -- reconstructed from the leak's
//                                    GenerateDispatchLists prologue re-shaped to the ship
//                                    split, plus the ship's own member promotions)
//
// Walk the frustum-filtered traffic entity ids the world module published, keep the ones
// whose vehicle is alive and inside the render cull radius, and hand the dispatch leg a
// near-to-far sorted, capped Array<VehicleRenderInfo,64>.
//
// SHIP vs LEAK, both real divergences:
//   * the leak reads a file-scope KF_RENDER_CULL_DISTANCE_SQ and KU_MAX_VEHICLES_TO_RENDER;
//     the ship PROMOTED BOTH TO MEMBERS -- mfRenderCullDistanceSq (DWARF :690) and
//     muMaxVehiclesToRender (:689). mfRenderCullDistanceSq's shipped value is RECOVERED:
//     62500.0f == 250 m squared, lane 2 of the vector at 0x8300CF10 seeded by the dyn-init
//     thunk @0x82C66F18 (this wave's recovered_constants.md).
//   * the leak's SECOND output -- a separate corona list culled at KF_CORONA_CULL_DISTANCE_SQ
//     -- has no counterpart in the ship's VehicleRenderInfo, because the ship's corona pass
//     re-walks this very array.
//
// mLOD is NOT written here. WorldModule::CalculateVehicleLODs writes it afterwards
// (`stw r9, 8(r3)` @0x827C3C34) into these same records, between the two locks -- which is
// why BrnWorldModule.cpp brackets PreDispatchUpdate and CalculateVehicleLODs together.
//
// ---- FIX ROUND 2026-08-21: RUNG 1 IS UNAVAILABLE HERE, RUNG 2 IS NOT --------------------
// @0x8274D900 really is an export hole -- confirmed by listing .ida-exports/
// BURNOUT_X360_ARTIST.XEX: the nearest neighbours are 0x8274C870 and 0x8274E508, so there is
// no pseudocode AND no assembly for this body at all. That makes the DecFIGS DWARF the
// HIGHEST rung available, and its scope/callee tree at dwarfdump/_compile/BrnTrafficUnity.cpp
// :19315-:19425 is therefore the specification, not a hint. Transcribed in source order:
//
//   Array<EntityId,650u>::GetLength
//   FastBitArray<601>::UnSetAll                          <-- cleared once per call
//   { luIndex                                            <-- the visible-id loop
//     { luVehicleIndex
//       Array<EntityId,650u>::operator[]
//       FastBitArray<601>::IsBitSet                      <-- skip an id already seen
//       { lPosition; lfDistanceSq
//         GetVehicleTransform / operator- / MagnitudeSquared
//         { bool lbAboutToDie                            <-- body of the distance-cull `if`
//           FastBitArray<601>::SetBit                    <-- set BEFORE the liveness test
//           GetVehicleSpecies
//           { const Param* lpParam;            Param::IsAlive }                    // :13768
//           { const StaticTrafficParam* lpParam;
//             GetStaticTrafficParamFromFullVehicleIndex; StaticTrafficParam::IsAlive } // :13776
//           { uint32_t luCab; GetVehicleSpecies; GetVehicle }                      // :13786
//           Vehicle::GetCabIndex; { const Param* lpParam; GetParam; Param::IsAlive } // :13789
//           { const VehicleRenderInfo& lRenderInfo } } } } }                        // :13800
//   { luVisibleCount; CgsNumeric::Min; std::sort<...>; Array<VehicleRenderInfo,64u>::Append }
//
// TWO console behaviours the first draft of this file dropped, both now restored:
//
// (1) THE DUPLICATE SUPPRESSION. maTrafficEntityIds is filled by
//     WorldModule::FilterFrustumTestResults from the scene manager's coarse-test results, and
//     the leak's equivalent walk (BrnTrafficEntityModule.cpp:7897-7947) loops over MULTIPLE
//     SceneResultQueue events -- an entity that lands in more than one result appears more
//     than once. The leak has no bit array; the SHIP spends a 601-bit one per call, which is
//     the ship ADDING a fix the leak lacked, not the DWARF drifting. Without it a duplicated
//     id is appended twice, the same parked car is submitted twice into dispatch lists
//     12/19/20, and it burns two of the muMaxVehiclesToRender slots -- silently, with no
//     assert and no non-finite value. That is the exact failure shape the [T1-dispatch] diag
//     below exists to catch, so the diag now counts suppressed duplicates too.
//     TEMPLATE ARGUMENT: the DWARF instantiation is FastBitArray<601>, which is DecFIGS'
//     KU_MAX_TOTAL_TRAFFIC. The SHIP value is 600 -- BrnTraffic::GetVehicleSpecies @0x821F4648
//     asserts `luIndex < 0x258` against the literal "luIndex < KU_MAX_TOTAL_TRAFFIC"
//     (BrnTrafficConstants.h carries the full derivation, and the module's own three
//     FastBitArray members already spell KU_MAX_TOTAL_TRAFFIC for the same reason). Rung 1
//     beats rung 2 on a value, so this is <KU_MAX_TOTAL_TRAFFIC>, not <601>.
//
// (2) THE SPECIES-DISPATCHED LIVENESS PREDICATE `lbAboutToDie`. The ship does NOT ask the
//     Vehicle; it asks the vehicle's PARAM, and which param depends on the species:
//       * E_SPECIES_STANDARD -> GetParam(luVehicle)->IsAlive()                    REAL below
//       * E_SPECIES_STATIC   -> GetStaticTrafficParamFromFullV(luVehicle)->IsAlive()  REAL below
//         (that accessor DOES exist here -- @0x827078D0, bodied in BrnTrafficEntityModule.cpp;
//         "GetStaticTrafficParamFromFullV" is the console's own symbol truncation of the
//         DWARF's GetStaticTrafficParamFromFullVehicleIndex, not a different function)
//       * E_SPECIES_TRAILER  -> GetVehicle(luVehicle)->GetCabIndex() then
//         GetParam(cab)->IsAlive()                                                GATE G7
//     Only the trailer arm is blocked, and only on one missing accessor. The STATIC arm is the
//     one parked traffic takes, and it is real.
// ============================================================================
void
TrafficEntityModule::PreDispatchUpdate( const BrnTrafficIO::InputBuffer_PreDispatch* lpInput,
                                        BrnTrafficIO::OutputBuffer_PreDispatch* lpOutput )
{
    CGS_ASSERT( lpInput != 0, "lpInput" );
    CGS_ASSERT( lpOutput != 0, "lpOutput" );

    ::Array< VehicleRenderInfo, 64u >& lrRenderInfos = lpOutput->maTrafficRenderInfos;
    lrRenderInfos.Clear();

    const Vector3 lCameraPosition = lpInput->mCameraPosition;
    const ::Array< CgsSceneManager::EntityId, 650u >& lrVisible = lpInput->maTrafficEntityIds;

    // DWARF :19315 -- `FastBitArray<601>::UnSetAll` at the top of the body, before the walk.
    // See the "(1) THE DUPLICATE SUPPRESSION" block in this function's header comment for why
    // the ship has this and the leak does not, and why the template argument is the SHIP's
    // KU_MAX_TOTAL_TRAFFIC (600) rather than the DWARF's DecFIGS-era 601.
    CgsContainers::FastBitArray< KU_MAX_TOTAL_TRAFFIC > lVehiclesAlreadySeen;
    lVehiclesAlreadySeen.UnSetAll();

    // The producer (WorldModule::FilterFrustumTestResults) has already kept only the
    // owner-byte-2 (traffic) ids, so every entry here names a traffic vehicle slot.
    const u32 luVisibleCount = lrVisible.GetLength();
    u32 luDuplicatesSuppressed = 0;                       // [T1-dispatch] witness only
    for ( u32 luVisible = 0; luVisible < luVisibleCount; ++luVisible )
    {
        const u32 luVehicle = lrVisible[ luVisible ].GetEntityIndex();

        // The console's own bound, asserted inside GetVehicle / GetVehicleTransform
        // ("luIndex < KU_MAX_TOTAL_TRAFFIC", GetVehicleSpecies @0x821F4648 `cmplwi 0x258`).
        // The console traps here; the host skips the record, because every read below --
        // the bit array, maVehicleTransforms and the param pools -- is indexed by it.
        CGS_ASSERT( luVehicle < KU_MAX_TOTAL_TRAFFIC, "luIndex < KU_MAX_TOTAL_TRAFFIC" );
        if ( luVehicle >= KU_MAX_TOTAL_TRAFFIC )
        {
            continue;
        }

        // DWARF :19315 -- `FastBitArray<601>::IsBitSet` guards the whole per-vehicle block.
        if ( lVehiclesAlreadySeen.IsBitSet( luVehicle ) )
        {
            ++luDuplicatesSuppressed;
            continue;
        }

        // Distance cull. maVehicleTransforms is read directly rather than through
        // GetVehicleTransform because the console inlines that accessor here (it is a bare
        // indexed read plus a validity assert), and no declaration for it exists in the tree.
        const Vector3& lrPos = maVehicleTransforms[ luVehicle ].Pos();
        const f32 lfDX = lrPos.x - lCameraPosition.x;
        const f32 lfDY = lrPos.y - lCameraPosition.y;
        const f32 lfDZ = lrPos.z - lCameraPosition.z;
        const f32 lfDistanceSq = lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ;

        if ( lfDistanceSq >= mfRenderCullDistanceSq )
        {
            continue;
        }

        // DWARF :19315 -- `SetBit` is the FIRST call inside the `{ bool lbAboutToDie }` scope,
        // i.e. inside the body of the distance-cull `if` and BEFORE the liveness dispatch. So
        // a vehicle that passed the cull marks itself seen whether or not it is then kept: the
        // bit means "considered this frame", not "appended this frame". Reproduced exactly.
        lVehiclesAlreadySeen.SetBit( luVehicle );

        // ---- the species-dispatched liveness predicate (DWARF :13765-:13789) -------------
        // NOT `Vehicle::IsAlive()`. The ship asks the vehicle's PARAM, and the pool the param
        // lives in is chosen by species. See "(2)" in the header comment.
        bool lbAboutToDie;
        const Vehicle::Species leSpecies = GetVehicleSpecies( luVehicle );
        if ( leSpecies == Vehicle::E_SPECIES_STANDARD )
        {
            // DWARF :13768. GetParam is the leak's :1457 inline; the console folds it in.
            const Param* lpParam = GetParam( luVehicle );
            lbAboutToDie = !lpParam->IsAlive();
        }
        else if ( leSpecies == Vehicle::E_SPECIES_STATIC )
        {
            // DWARF :13776 -- GetStaticTrafficParamFromFullVehicleIndex, which is this tree's
            // GetStaticTrafficParamFromFullV @0x827078D0 (console symbol truncation; the body
            // in BrnTrafficEntityModule.cpp is real). THIS IS THE PARKED-CAR ARM.
            const StaticTrafficParam* lpParam = GetStaticTrafficParamFromFullV( luVehicle );
            lbAboutToDie = !lpParam->IsAlive();
        }
        else
        {
            // ⭐⭐ GATE G7 RETIRED 2026-08-21 (wave T1 ROUND 4, item 4 sweep) -- the TRAILER
            // arm is now the console's own (DWARF :13786-:13789), and the stand-in below it
            // is gone.
            //
            // The banner that stood here said the blocker was "exactly one:
            // BrnTraffic::Vehicle::GetCabIndex has NO declaration anywhere in b5-decomp/src",
            // and that it was "two lines" once someone named it. Both were true, and the
            // reason it survived three rounds is that GetCabIndex's ledger row sits under the
            // same CgsStrStream.h CATCH-ALL primary_file that hid FindVehicleTypeAttribKey_
            // EXPENSIVE and Vehicle::SetHasEntity. It is X360 @0x8270E4C8, fourteen
            // instructions (`IsOfTrailerSpecies()` assert + `lhz 2(this)`), DWARF-declared at
            // BrnTrafficVehicle.h:339, and it is declared + bodied as of this round.
            //
            // The stand-in it replaces was the trailer's OWN alive flag. That was documented
            // and conservative, but it was not the console's test: the console asks the CAB's
            // PARAM, which is a different bit that can differ for a frame while a cab is
            // dying.
            const u32 luCab = GetVehicle( luVehicle )->GetCabIndex();
            lbAboutToDie = !GetParam( luCab )->IsAlive();
        }

        if ( lbAboutToDie )
        {
            continue;
        }

        // The array is structurally 64 slots; the console's own cap is
        // muMaxVehiclesToRender, applied after the sort. Appending past the capacity would
        // fire Array's "out of space" assert, so the insert is a bounded near-to-far
        // INSERTION SORT rather than an append-then-qsort: it produces exactly the console's
        // ordering and exactly the console's kept set (the nearest N), while never touching a
        // slot the container does not have. (The console can append freely because its
        // pre-sort scratch is the caller's 600-entry stack array; the ship's 64-entry output
        // array is the object it sorts INTO.)
        VehicleRenderInfo lInfo;
        lInfo.muEntityIndex = luVehicle;
        lInfo.mfDistanceSq  = lfDistanceSq;
        lInfo.mLOD          = CgsGraphics::Model::E_STATE_LOD_0;   // written by CalculateVehicleLODs

        u32 luInsertAt = lrRenderInfos.GetLength();
        while ( luInsertAt > 0
                && lrRenderInfos[ luInsertAt - 1u ].mfDistanceSq > lfDistanceSq )
        {
            --luInsertAt;
        }

        if ( lrRenderInfos.GetLength() < lrRenderInfos.GetCapacity() )
        {
            lrRenderInfos.Append( lInfo );
        }
        else if ( luInsertAt >= lrRenderInfos.GetLength() )
        {
            // Full, and this vehicle is farther than every kept one -- drop it.
            continue;
        }

        for ( u32 luShift = lrRenderInfos.GetLength() - 1u; luShift > luInsertAt; --luShift )
        {
            lrRenderInfos[ luShift ] = lrRenderInfos[ luShift - 1u ];
        }
        lrRenderInfos[ luInsertAt ] = lInfo;
    }

    // The console's cap, applied last (`muMaxVehiclesToRender`, DWARF :689). Because the list
    // is already near-to-far the truncation keeps the nearest N, which is what the qsort +
    // clamp does on the console.
    if ( lrRenderInfos.GetLength() > muMaxVehiclesToRender )
    {
        const u32 luKeep = muMaxVehiclesToRender;
        VehicleRenderInfo laKept[ 64 ];
        for ( u32 luCopy = 0; luCopy < luKeep; ++luCopy )
        {
            laKept[ luCopy ] = lrRenderInfos[ luCopy ];
        }
        lrRenderInfos.Clear();
        for ( u32 luCopy = 0; luCopy < luKeep; ++luCopy )
        {
            lrRenderInfos.Append( laKept[ luCopy ] );
        }
    }

    // [T1-rinfo] the probe C5 homed in BrnTrafficEntityModuleIO.h, called at the frame
    // position C5 asked for -- "right after PreDispatchUpdate produces the number".
    // ⛔ DELETE-WHEN parked traffic is confirmed on a booted run.
    BrnTrafficIO::T1Diag_ReportTrafficRenderInfoCount( *lpOutput );

    // ---- [DIAG T1-dispatch] the CULL witness -------------------------------
    // ⛔ DELETE-WHEN parked traffic is confirmed on a booted run. Gated on BRN_TRAFFIC_DIAG.
    // This exists because of a specific, twice-burned failure mode: BOTH gates in this
    // function are module members seeded by Construct @0x82740220 (cluster C4), and a
    // placeholder-zero mfRenderCullDistanceSq would make `dSq >= 0` true for EVERY vehicle --
    // an empty render list, no assert, no non-finite value, nothing to trip on. (Verified
    // seeded as 62500.0f / 32 in BrnTrafficEntityModule_wT1_01.cpp at the time of writing;
    // this line is what makes a regression in that seeding visible in one frame instead of in
    // a day.) Latched on the kept COUNT, so a steady scene stays quiet.
    // ⭐ FIX ROUND 2026-08-21: the suppressed-duplicate count is reported here too. Duplicate
    // ids in maTrafficEntityIds are the OTHER silent failure this function can have (the same
    // car submitted twice, burning two of the muMaxVehiclesToRender slots), and it is equally
    // assert-free -- so the witness for the dedup lives beside the witness for the cull.
    {
        static const bool skbTrafficDiag = ( std::getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        if ( skbTrafficDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            static s32 siLastKept = -1;
            const s32 liKept = lrRenderInfos.GetCount();
            if ( liKept != siLastKept )
            {
                siLastKept = liKept;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-dispatch] PreDispatchUpdate visible " << static_cast< s32 >( luVisibleCount )
                    << " -> kept " << liKept
                    << " (dupsSuppressed " << static_cast< s32 >( luDuplicatesSuppressed )
                    << ", cullDistSq " << mfRenderCullDistanceSq
                    << ", maxToRender " << static_cast< s32 >( muMaxVehiclesToRender )
                    << ", camera (" << lCameraPosition.x << ", " << lCameraPosition.y
                    << ", " << lCameraPosition.z << "))\n";
            }
        }
    }
}


// ============================================================================
// GenerateDispatchLists  @ 0x8273B280
//
// Body read off the assembly @0x8273B280..0x8273B564 (the pseudocode renders the 52-argument
// RenderTrafficCar call as garbage), in the console's own order:
//   1. mCameraLastFrame = *lpBrnCamera            (Camera::operator= @0x8273B2C8, module +0x72890)
//   2. mFuzzyBehaviours.DEBUGSetLastCameraPos(pos) (the inlined `stvx128 v127, r29, 0x71870`)
//      -- BOTH before any gate.
//   3. return unless meState == E_STATE_RUNNING (lwz 0x300, cmpwi 1)
//                and meEmptyTrafficPoolState == IDLE (lwz 0x2FC, cmpwi 0)
//   4. assert lpInput (:14038), LockForRead
//   5. length = laTrafficRenderInfos.GetLength(); fetch + assert the four render handles
//      (:14049 frame / :14050 shadow map / :14055 blobby / :14060 coronas)
//   6. TWO stack Vector4[length] arrays, ZERO-FILLED (stvx + blkmov)
//   7. if (!lpShadowMap->IsRenderingShadowMap()) { StartMonitor(+0x72A38);
//         per-vehicle SubmitCoronasForVehicle(coronas, idx, &front[i], &rear[i], pos, dir);
//         StopMonitor; RenderTrafficLightCoronas(coronas, pos, dir); }
//   8. s32 liNumDamagedVehiclesRendered = 0  -- ONE counter for the whole loop
//   9. per-vehicle RenderTrafficCar(...)
//  10. UnlockForRead
// ============================================================================
void
TrafficEntityModule::GenerateDispatchLists( const BrnTrafficIO::InputBuffer_Dispatch* lpInput,
                                            const ::Array< VehicleRenderInfo, 64u >& laTrafficRenderInfos,
                                            Vector4 lFogScattering,
                                            Vector4 lFogColourPlusWhiteLevel,
                                            Vector3 lCameraPosition,
                                            Vector3 lCameraDirection,
                                            s32 liModelOnlyDisplayList,
                                            s32 liOpaqueList,
                                            s32 liTransparentList,
                                            const BrnDirector::Camera::Camera& lBrnCamera )
{
    // ---- step 1, NAMED GATE ------------------------------------------------
    // The console copies the whole dispatch camera into mCameraLastFrame (module +0x72890,
    // DWARF :881) with BrnDirector::Camera::Camera::operator= @0x8273B2C8, unconditionally,
    // before the state gates. THE MEMBER IS [MEMBER HOLE 6] in BrnTrafficEntityModule.h: its
    // type is BrnDirector::Camera::Camera, whose owning header would pull the director camera
    // graph into this keystone header's include list (and thence into BrnWorldModule.h, i.e.
    // most of the world build) -- a deliberate C1 decision, not a missing type.
    // BEHAVIOURALLY INERT ON THIS BUILD: the member's only consumer is the
    // mbDEBUGPickVehicleFromCamera debug tool, which is not reconstructed.
    // ⛔ DELETE-WHEN [MEMBER HOLE 6] closes; the line is `mCameraLastFrame = lBrnCamera;`.
    (void)lBrnCamera;

    // ---- step 2 ------------------------------------------------------------
    // The unconditional store at module +0x71870. ⭐ This was parked in round 1 as "an unnamed
    // 16-byte member absent from C1's layout"; it is NOT a member of this class at all -- see
    // the arithmetic on the RenderTrafficCar declaration in BrnTrafficEntityModule.h. It is
    // FuzzyBehaviourLogic::mDEBUGLastCameraPos, i.e. this call, inlined by the console.
    // ⛔ NAMED GATE (link, not knowledge): FuzzyBehaviourLogic::DEBUGSetLastCameraPos is
    // declaration-only -- BrnTrafficFuzzyLogicBehaviours.cpp has no body for it and that TU is
    // not on tools/build/build_game_exe.bat -- so calling it would leave an unresolved
    // external. Debug-render-only member; omitting the store changes nothing that is drawn.
    // ⛔ DELETE-WHEN that TU is mounted with a real body:
    //     mFuzzyBehaviours.DEBUGSetLastCameraPos( lCameraPosition );

    // ---- step 3: the two state gates ---------------------------------------
    if ( meState != E_STATE_RUNNING
         || meEmptyTrafficPoolState != E_EMPTYTRAFFICPOOLSTATE_IDLE )
    {
        return;
    }

    CGS_ASSERT( lpInput != 0, "lpInput" );

    // ---- step 4/5 ----------------------------------------------------------
    lpInput->LockForRead();

    const u32 luLength = laTrafficRenderInfos.GetLength();

    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInput->GetDispatchFrame();
    BrnWorld::ShadowMap*        lpShadowMap     = lpInput->GetShadowMap();
    BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowRenderer =
        lpInput->GetBlobbyShadowBuffer();

    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );
    // ⛔ The console also asserts lpBlobbyShadowRenderer (:14055) and
    // lpCoronaSubmissionInterface (:14060). NEITHER IS ASSERTED HERE, and that is deliberate:
    // nothing in this tree owns a BrnBlobbyShadowManager and nothing anywhere calls
    // InputBuffer_Dispatch::SetBlobbyShadowBuffer, so the handle is legitimately null on this
    // build (gate G6). Asserting it would turn a known, documented gap into a per-frame stop.
    // ⛔ DELETE-WHEN a BrnBlobbyShadowManager owner exists.

    if ( lpDispatchFrame == 0 || lpShadowMap == 0 )
    {
        lpInput->UnlockForRead();
        return;
    }

    // ---- step 6 ------------------------------------------------------------
    // The two per-vehicle light vectors, zero-filled exactly as the console zero-fills its two
    // stack Vector4[length] arrays before the corona pass. Sized to the array's structural
    // capacity rather than to `length` -- same values, no variable-length stack array.
    //
    // ⚠️ THESE ARE NOT A STUB. They are the console's own state whenever the corona pass is
    // skipped (which the console itself does on every shadow-map pass), and they are OUT
    // params of SubmitCoronasForVehicle that RenderTrafficCar CONSUMES -- not a corona-only
    // side channel. Passing zeros while gate G5 is open is faithful, not invented.
    Vector4 laFrontLights[ 64 ];
    Vector4 laRearLights[ 64 ];
    for ( u32 luZero = 0; luZero < luLength && luZero < 64u; ++luZero )
    {
        laFrontLights[ luZero ].SetZero();
        laRearLights[ luZero ].SetZero();
    }

    // ---- step 7: the corona pass -- NAMED GATE G5 --------------------------
    // The console's shape, kept here as the record of what must land in wave 2:
    //   if ( !lpShadowMap->IsRenderingShadowMap() )
    //   {
    //       CgsDev::PerfMonCpu::StartMonitor( <ship-only monitor id at module +0x72A38> );
    //       for ( i = 0; i < luLength; ++i )
    //           SubmitCoronasForVehicle( lpInput->GetCoronaSubmissionInterface(),
    //                                    laTrafficRenderInfos[i].muEntityIndex,
    //                                    lCameraPosition, lCameraDirection,
    //                                    laFrontLights[i], laRearLights[i] );   // r6/r7 OUT
    //       CgsDev::PerfMonCpu::StopMonitor( ... );
    //       RenderTrafficLightCoronas( lpInput->GetCoronaSubmissionInterface(),
    //                                  lCameraPosition, lCameraDirection );
    //   }
    // BLOCKERS: SubmitCoronasForVehicle @0x82727BB0 (DWARF :22445) and
    // RenderTrafficLightCoronas @0x8271EC80 (DWARF :7711, an EXPORT HOLE) are wave 2 and have
    // no declaration in this tree; the monitor id at +0x72A38 is a ship-only member with no
    // DWARF name. Note the ORDER is load-bearing and matches the race-car leg's own
    // mbRenderingShadowMap `continue`: the corona pass runs BEFORE the draw pass and ONLY off
    // the shadow pass -- the same double-post trap the coronas step-2 fix documents.
    // ⛔ DELETE-WHEN wave 2 lands the two producers.

    // ---- step 8/9 ----------------------------------------------------------
    // ONE budget for the whole loop. The console seeds it to 0 before the loop and passes its
    // ADDRESS to every vehicle, so at most five damaged traffic cars per frame take the
    // expensive damaged-mesh path (the `*a52 < 5` gate at pseudocode :1254).
    s32 liNumDamagedVehiclesRendered = 0;

    for ( u32 luRender = 0; luRender < luLength; ++luRender )
    {
        const VehicleRenderInfo& lrInfo = laTrafficRenderInfos[ luRender ];

        RenderTrafficCar( lpDispatchFrame,
                          lrInfo.muEntityIndex,
                          lCameraPosition,
                          lFogScattering,
                          lFogColourPlusWhiteLevel,
                          lpBlobbyShadowRenderer,
                          liModelOnlyDisplayList,
                          liOpaqueList,
                          liTransparentList,
                          lpShadowMap,
                          lrInfo.mLOD,
                          laFrontLights[ luRender ],
                          laRearLights[ luRender ],
                          &liNumDamagedVehiclesRendered );
    }

    // ---- step 10 -----------------------------------------------------------
    lpInput->UnlockForRead();

    // ---- [DIAG T1-dispatch] the PRODUCER-side witness ----------------------
    // ⛔ DELETE-WHEN parked traffic is confirmed on a booted run. Gated on BRN_TRAFFIC_DIAG.
    // The consuming-end counterpart (records reaching object list 12) already lives in
    // BrnWorldModule.cpp; this is the other half, so "the leg ran and drew nothing" and "the
    // leg never ran" read differently. Latched on the VALUE, never on a printed-once bool.
    {
        static const bool skbTrafficDiag = ( std::getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        if ( skbTrafficDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            static u32 suLastLength = 0xFFFFFFFFu;
            if ( luLength != suLastLength )
            {
                suLastLength = luLength;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-dispatch] GenerateDispatchLists walked " << static_cast< s32 >( luLength )
                    << " renderInfo(s); shadowPass "
                    << ( lpShadowMap->IsRenderingShadowMap() ? 1 : 0 )
                    << " blobbyBuffer " << ( lpBlobbyShadowRenderer != 0 ? 1 : 0 )
                    << " lists " << liModelOnlyDisplayList << "/" << liOpaqueList
                    << "/" << liTransparentList << "\n";
            }
        }
    }
}


// ============================================================================
// RenderTrafficCar  @ 0x82728B08
//
// Submit ONE traffic car's body-part and wheel draws into the frame's vehicle lists.
//
// ---- THE PROLOGUE, READ OFF THE ASM (@0x82728B24..0x82728B98), NOT THE PSEUDOCODE ----
//   r3 this / r4 DispatchFrame (spilled arg_1C) / r5 luEntityIdx / r6 BlobbyShadowBuffer /
//   r7 liModelOnlyDisplayList (arg_6C) / r8 liOpaqueList (arg_74) / r9 liTransparentList
//   (arg_7C) / r10 ShadowMap; v1 -> v120 = lCameraPosition, v2 spilled to r1+0x980 =
//   lFogScattering, v3 -> v119 = lFogColourPlusWhiteLevel, v4 = lFrontLights,
//   v5 -> v118 = lRearLights; stack var_914 = lLOD, var_8EC = the shared budget pointer.
//   The DWARF (:23103) names every one of those and agrees exactly.
//
// ---- THE TWO ENTRY GATES ------------------------------------------------------------
//   `lbzx r9, r25, 0x725E8` @0x82728B80 -> module +468456 == mbTrafficIsHidden (DWARF :789).
//        Non-zero BAILS THE WHOLE FUNCTION (bne -> the epilogue).
//   `lbzx r11, r25, 0x72520` @0x82728BEC -> module +468256 selects the REPLAY-SERIALISER pose
//        source (BrnReplays::TrafficEntitySerialiser lives at +468160; the arm calls
//        GetVehicleData / GetPhysicsInfo / GetVehicleTransform on it). Gate G1.
//
// ---- THE LIVE ARM (@0x82728BF8 onward) ----------------------------------------------
//   GetVehicle(idx) -> flags byte at +5: bit0 (ALIVE) and bit1 must BOTH be set, else bail;
//   bit3 (PHYSICAL) -> GetTrafficPhysicsInfoForVehicl + "not physical" latch cleared (gate
//   G2). Then GetVehicleType, GetVehicleTransform, GetPitch_Roll_Steering_WheelRot, GetSpeed.
// ============================================================================
void
TrafficEntityModule::RenderTrafficCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                                       u32 luEntityIdx,
                                       Vector3 lCameraPosition,
                                       Vector4 lFogScattering,
                                       Vector4 lFogColourPlusWhiteLevel,
                                       BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobbyShadowRenderer,
                                       s32 liModelOnlyDisplayList,
                                       s32 liOpaqueList,
                                       s32 liTransparentList,
                                       BrnWorld::ShadowMap* lpShadowMap,
                                       CgsGraphics::Model::State lLOD,
                                       Vector4 lFrontLights,
                                       Vector4 lRearLights,
                                       s32* lpiUpdatedNumDamagedVehiclesRendered )
{
    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );
    CGS_ASSERT( lpiUpdatedNumDamagedVehiclesRendered != 0,
                "lpiUpdatedNumDamagedVehiclesRendered" );

    // ---- entry gate 1: the whole function ---------------------------------
    if ( mbTrafficIsHidden )
    {
        return;
    }

    // ---- entry gate 2 (G1): the replay-serialiser pose source -------------
    // ⛔ NAMED GATE. When module +468256 is set the console takes the vehicle's TYPE, damage
    // record and TRANSFORM from BrnReplays::TrafficEntitySerialiser (module +468160) instead
    // of from the live pools, and derives the "not physical" latch from the serialised flag
    // bit 5 rather than from Vehicle::mxFlags. BLOCKER: the serialiser is not reconstructed
    // (TrafficEntitySerialiser::GetVehicleData / GetPhysicsInfo / GetVehicleTransform have no
    // declarations in this tree) and the module member at +468256 is not named in C1's layout.
    // The live arm below is the one that runs outside replay playback -- i.e. the arm that
    // matters for a parked car in a normal race. ⛔ DELETE-WHEN the replay wave lands.

    const Vehicle* lpVehicle = GetVehicle( luEntityIdx );
    if ( lpVehicle == 0 )
    {
        return;
    }

    // `lbz r11, 5(r29)` then two independent bit tests, BOTH of which must pass
    // (@0x82728C00..0x82728C18). Bit 0 is E_FLAG_ALIVE; the second bit is the same one
    // Vehicle::IsAlive()'s sibling predicates read, and it gates rendering specifically.
    if ( !lpVehicle->IsAlive() )
    {
        return;
    }
    // ⛔ FLAG (partial gate read): the console requires flags bit 1 as well as bit 0
    // (`rlwinm r10, r11, 0,30,30` @0x82728C10, beq -> bail). Vehicle's PC flag enum does not
    // yet name bit 1, and GetFlags() is the only raw accessor. The test is spelled through it
    // rather than invented as a predicate, so the console's gate is honoured exactly and the
    // naming stays with whoever owns BrnTrafficVehicle.h.
    if ( ( lpVehicle->GetFlags() & 0x02u ) == 0u )
    {
        return;
    }

    // ---- G2: the PHYSICAL (crashing) arm ----------------------------------
    // ⛔ NAMED GATE. `rlwinm r11, r11, 0,28,28` @0x82728C1C -> Vehicle::IsPhysical(): the
    // console fetches GetTrafficPhysicsInfoForVehicl(idx) (asserting it non-null, :15106) and
    // clears the "not physical" latch, which then (a) sources every wheel's rotation and
    // world position from maCrashingVehiclePartsList instead of from the deformation spec,
    // (b) enables the damaged verlet-offset upload, and (c) SUPPRESSES the blobby shadow.
    // BLOCKER: TrafficEntityModule::GetTrafficPhysicsInfoForVehicl @0x82714500 is
    // declaration-only and returns void in this tree (BrnTrafficEntityModule.h marks it
    // "(FLAG)"), and maCrashingVehiclePartsList has no home. A PARKED car is never physical,
    // so this gate does not stand between the wave and a car on screen.
    // ⛔ DELETE-WHEN the crash/deformation traffic wave lands.
    const bool lbIsPhysical = lpVehicle->IsPhysical();
    if ( lbIsPhysical )
    {
        static bool sbLoggedPhysicalGate = false;
        if ( !sbLoggedPhysicalGate && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedPhysicalGate = true;
            *CgsDev::Log::gpDebugPrint
                << "[T1-dispatch] RenderTrafficCar: PHYSICAL traffic vehicle skipped -- the"
                   " crashing-parts pose source is not reconstructed"
                   " (GetTrafficPhysicsInfoForVehicl @0x82714500 is declaration-only)"
                   " [FLAG PC partial gate]\n";
        }
        return;
    }
    // "not physical" is also what enables the blobby shadow at the tail (pseudocode :3183
    // mirrors the leak's `if (!lpVehicle->IsPhysical()) AddShadow(...)`).

    const u32 luVehicleType = static_cast< u32 >( lpVehicle->GetVehicleType() );

    // ---- the asset id, the streamer gates, the two specs -------------------
    // Leak :8524-8543 maps one-for-one onto ship pseudocode :633-656.
    CGS_ASSERT( mpData.HasMemoryResource(), "mpData" );
    const TrafficData* lpData = mpData.operator->();
    CGS_ASSERT( luVehicleType < lpData->muNumVehicleTypes,
                "lpVehicle->muVehicleType < mpData->muNumVehicleTypes" );

    const u32 luAssetId = static_cast< u32 >( lpData->mpaVehicleTypes[ luVehicleType ].muAssetId );

    // Unconditional, and BEFORE the loaded test: this is what feeds the streamer's 64-bit
    // per-asset rendering history, i.e. what makes an asset the module has been trying to draw
    // become a streaming priority. Skipping it would starve the very asset that is missing.
    mStreamer.NotifyAssetRenderedThisFrame( luAssetId );

    if ( !mStreamer.IsTrafficAssetLoaded( luAssetId ) || mbDEBUGDontRenderMeshes )
    {
        return;
    }

    const BrnVehicle::GraphicsSpec* lpGraphicsSpec      = mStreamer.GetGraphicsSpec( luAssetId );
    const BrnWheel::GraphicsSpec*   lpWheelGraphicsSpec = mStreamer.GetWheelGraphicsSpec( luAssetId );
    if ( lpGraphicsSpec == 0 || lpWheelGraphicsSpec == 0 )
    {
        return;
    }

    // maTrafficVehiclePhysicsSpecs[type]. The console reads it as
    // `CreateFromHandle(local, 32*VehicleType + this + 482156)` -- a 32-byte stride because a
    // CONSOLE CgsResource::ResourcePtr is 32 bytes. THAT STRIDE IS X360 AND MUST NOT SURVIVE:
    // on the host the array is indexed by name and the host ResourcePtr has its own size.
    // ⛔ FLAG (extent): the DWARF declares this array KU_MAX_VEHICLE_ASSETS (64) long but the
    // console indexes it by VEHICLE TYPE, whose bound is KU_MAX_VEHICLE_TYPES (96). Both the
    // leak (`maTrafficVehiclePhysicsSpecs[ liModelId ]`, liModelId == muVehicleType) and the
    // asm agree on the INDEX; the extent is C1's to reconcile. Bounded here so a type past 64
    // declines to draw rather than reading off the end.
    if ( luVehicleType >= KU_MAX_VEHICLE_ASSETS )
    {
        return;
    }
    const CgsResource::ResourcePtr< BrnPhysics::Deformation::StreamedDeformationSpec >&
        lrPhysicsSpecPtr = maTrafficVehiclePhysicsSpecs[ luVehicleType ];

    const CgsGraphics::Model* lpWheelModel = lpWheelGraphicsSpec->GetWheelModel();
    CGS_ASSERT( lpWheelModel != 0, "lpWheelModel" );

    // ---- the body transform: pitch, then roll, then the vehicle transform ---
    // Leak :8545-8551 verbatim, and the ship's inlined VMX polynomial cascade
    // (@0x82728D40 onward -- the vrfin + minimax that IS SinCos) is exactly this composed
    // pair. maVehicleTransforms is read directly: the console's GetVehicleTransform is an
    // indexed read plus a validity assert, inlined at this site, and no declaration for it
    // exists in the tree (SetVehicleTransform is C4's; the getter was never declared).
    const Matrix44Affine lBodyTransform = maVehicleTransforms[ luEntityIdx ];

    const Vector4 lPitchRollSteerWheel = lpVehicle->GetPitch_Roll_Steering_WheelRot();
    const f32 lfPitch    = lPitchRollSteerWheel.x;
    const f32 lfRoll     = lPitchRollSteerWheel.y;
    const f32 lfSteering = lPitchRollSteerWheel.z;
    const f32 lfWheelRot = lPitchRollSteerWheel.w;

    const Matrix44Affine lBodyRollTransform =
        rw::math::vpu::Mult(
            rw::math::vpu::Mult( rw::math::vpu::MakeRotationX( lfPitch ),
                                 rw::math::vpu::MakeRotationZ( lfRoll ) ),
            lBodyTransform );

    // ---- the shadow-pass selector (identical to the race car's) ------------
    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap()
                           && lpShadowMap->IsUsingZOnlyRenderingPath();

    // ---- G3: glass fracture + the damaged-vehicle budget -------------------
    // ⛔ NAMED GATE. The console's shape at pseudocode :1254-1281:
    //     if ( physicsInfo && physicsInfo->mbDamaged && *lpiUpdated... < 5 )
    //     {
    //         ++*lpiUpdatedNumDamagedVehiclesRendered;
    //         BrnWorld::SetGlassFractureConstants( 0.0f, 1.0f, {0,0}, {0,0,0,0} );
    //         SetShaderConstantArrayData( 22, <physicsInfo + 1328> );   // the live verlet block
    //         SetShaderConstantData( 23, {0,0,0,0} );
    //     }
    //     else { BrnWorld::SetGlassFractureConstants( 0.0f, 1.0f, ... ); }
    // TWO BLOCKERS, both link-level rather than knowledge-level: (a) the only definition of
    // BrnWorld::SetGlassFractureConstants lives in BrnRaceCarEntityModule_GlassFracture.cpp,
    // which is NOT on tools/build/build_game_exe.bat (the race-car render TU carries the same
    // gate for the same reason), so calling it is an unresolved external; (b) the damaged
    // verlet block hangs off the traffic physics info, which is gate G2.
    // CONSEQUENCE, and it is the same one the race-car TU documents: shader constants 30/31/32
    // are never published by anything on this build, and an unset external constant is SKIPPED
    // rather than zeroed, so the glass programs read the previous draw's registers.
    // ⛔ DELETE-WHEN the GlassFracture TU is mounted.
    (void)lpiUpdatedNumDamagedVehiclesRendered;

    // ---- the technique index -----------------------------------------------
    // Two bits, exactly the race car's scheme: bit 0 == "not damaged", bit 1 == "shadow pass"
    // (`v188 = damaged ? (shadow ? 3 : 0) : (shadow ? 2 : 1)`). Traffic on this build is never
    // damaged (gate G2 above returns before here for a physical car), so the undamaged pair is
    // the only reachable one -- written as the full expression rather than folded, so the
    // damaged arm is already correct when G2 closes.
    const bool lbDamaged = false;
    u8 lu8Technique;
    if ( lbDamaged )
    {
        lu8Technique = lbShadowPass ? 3u : 0u;
    }
    else
    {
        lu8Technique = lbShadowPass ? 2u : 1u;
    }

    // ---- shader constants 20 / 21: the paint ------------------------------
    // ⚠️ BOTH SLOTS TAKE THE SAME VECTOR on traffic (`vmr128 v1, v121` twice, pseudocode
    // :1284-:1288) -- unlike the race car, which publishes a separate pearlescent tint. The
    // vector is VehicleTypeRuntime::PickPaintColourForVehicle( luEntityIdx, numColours,
    // mpData->mpaPaintColours ), seeded by the VEHICLE INDEX so a given parked car keeps its
    // colour frame to frame.
    Vector4 lv4PaintColour = { 1.0f, 1.0f, 1.0f, 1.0f };
    {
        const VehicleTypeRuntime* lpVehicleTypeRuntime = GetVehicleTypeRuntime( luVehicleType );
        if ( lpVehicleTypeRuntime != 0 && lpData->mpaPaintColours != 0 )
        {
            lv4PaintColour = lpVehicleTypeRuntime->PickPaintColourForVehicle(
                luEntityIdx, lpData->GetNumPaintColours(), lpData->mpaPaintColours );
        }
    }
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 20, lv4PaintColour );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 21, lv4PaintColour );

    // ---- shader constant 24: the self-illumination mask -------------------
    // `lvx128 v0, r0, unk_8300D000 ; vmulfp128 v1, v118, v0 ; SetShaderConstantData(24)`
    // (pseudocode :1289-:1294) with v118 == lRearLights, the 5th vector argument. So TRAFFIC
    // DERIVES ITS LAMP EMISSION FROM THE CORONA PRODUCER'S OUT-VECTOR rather than from
    // per-car brake/reverse booleans the way the race car does -- which is why the DWARF
    // names that pair lFrontLights / lRearLights and why they are not a corona-only channel.
    // ⛔ FLAG: the scale is one of this file's four unrecovered dyn-init constants. See its
    // declaration at the head of the file for the exact consequence.
    // ⛔ FLAG: lFrontLights has NO OBSERVED CONSUMER in the reconstructed legs -- only v5
    // (lRearLights) is re-homed at the prologue. It is carried faithfully rather than dropped,
    // because SubmitCoronasForVehicle writes both and the front vector is presumably consumed
    // by a leg inside one of the gates above. Do not delete it to silence the warning.
    (void)lFrontLights;
    {
        const Vector4 lv4SelfIlluminationMask = {
            lRearLights.x * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.y * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.z * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.w * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 24, lv4SelfIlluminationMask );
    }

    // ---- shader constants 22 / 23: the verlet block -----------------------
    // TRAFFIC NEVER DEFORMS: the leak publishes g_NullVerletOffsets here unconditionally
    // (:8580) and a zeroed damage vector as 23 (:8577).
    //
    // ⚠ DELIBERATE DEVIATION, and it is the SAME one the race-car render TU already carries,
    // for the same measured reason: the ship folds the constant-22 publish into the DAMAGED
    // arm only. Leaving it unpublished is NOT neutral -- sixteen vertex programs in
    // SHADERS_PC.BNDL declare g_verletOffsets at c0 count 128, and an external constant whose
    // source pointer is null is SKIPPED, not zeroed (shadowingdevice.cpp:847), so the program
    // reads whatever the preceding world draws left in c0..c127 (c20..c23 being the last world
    // object's world matrix, translations in the thousands of metres). That is exactly the
    // panel-stretch defect. Publishing the leak's own null block costs one 2 KB copy per car.
    CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData( 22, gaNullVerletOffsets );
    {
        const Vector4 lv4DamageConstants = { 0.0f, 0.0f, 0.0f, 0.0f };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 23, lv4DamageConstants );
    }

    // ---- shader constant 26: the per-car FOG blend ------------------------
    // The console inlines a powf (the vlogefp / vexptefp pair plus the polynomial refinement
    // over the .rodata coefficient vectors at 0x82000BD0..0x82000C20, pseudocode :700-:1177);
    // de-optimised back to the library call, byte-identical in shape to the race car's:
    //     t = powf( saturate( distance * scattering.x - scattering.y ), scattering.z )
    //         * scattering.w
    //     constant26 = { fogColour.xyz * t, 1 - t }        (`vrlimi128 v1, v13, 1, 0`)
    // The distance is camera -> car, which the race car receives ready-made and traffic
    // computes here from lCameraPosition (that is what v120 is for).
    {
        const f32 lfDX = lBodyTransform.wAxis.x - lCameraPosition.x;
        const f32 lfDY = lBodyTransform.wAxis.y - lCameraPosition.y;
        const f32 lfDZ = lBodyTransform.wAxis.z - lCameraPosition.z;
        const f32 lfCameraDistance = sqrtf( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ );

        f32 lfBlend = lfCameraDistance * lFogScattering.x - lFogScattering.y;
        if ( lfBlend < 0.0f ) { lfBlend = 0.0f; }
        if ( lfBlend > 1.0f ) { lfBlend = 1.0f; }
        const f32 lfFog = powf( lfBlend, lFogScattering.z ) * lFogScattering.w;

        const Vector4 lv4Fog = { lFogColourPlusWhiteLevel.x * lfFog,
                                 lFogColourPlusWhiteLevel.y * lfFog,
                                 lFogColourPlusWhiteLevel.z * lfFog,
                                 1.0f - lfFog };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 26, lv4Fog );
    }

    // ========================================================================
    // THE BODY-PART LOOP
    //
    // ⛔ G4 (detached parts). Before the loop the console builds a 128-slot lookup from part
    // index -> detached-part record (pseudocode :1319-:1345, asserting luPartCount < 128 at
    // :15368 and luDetachedPartIndex < luPartCount at :15374) and, for a part that HAS a
    // record, takes the record's own four matrix rows as the world matrix instead of composing
    // the locator. BLOCKER: the traffic detached-part queue hangs off the traffic physics info
    // (gate G2) and has no home in this tree. A parked car has no detached parts, so every
    // part takes the composed path below -- which is the console's own else-arm, not a
    // simplification of it. ⛔ DELETE-WHEN G2 closes.
    // ========================================================================
    {
        const u32 luPartCount = lpGraphicsSpec->muPartsCount;
        CGS_ASSERT( luPartCount < 128u, "luPartCount < 128" );

        for ( u32 luPartIdx = 0; luPartIdx < luPartCount; ++luPartIdx )
        {
            const CgsGraphics::Model* lpModel = lpGraphicsSpec->GetPartModel( luPartIdx );
            if ( lpModel == 0 )
            {
                continue;
            }
            if ( !lpModel->DoesStateExist( lLOD ) )
            {
                continue;
            }

            // World matrix = partLocator * bodyRollTransform. The asm broadcasts each locator
            // row component (vspltw) and accumulates against the three body rotation rows,
            // seeding the translation row with the body translation -- i.e. exactly
            // rw::math::vpu::Mult with the LOCATOR on the left, the same order the race car's
            // body-part loop uses and the same order the leak spells (:8592).
            const Matrix44Affine lPartWorldMatrix =
                rw::math::vpu::Mult( lpGraphicsSpec->GetPartLocators()[ luPartIdx ],
                                     lBodyRollTransform );

            const CgsGraphics::Renderable* lpRenderable = lpModel->GetRenderable( lLOD );
            CGS_ASSERT( lpRenderable != 0, "Missing renderable in a model" );
            if ( lpRenderable == 0 )
            {
                continue;
            }

            CgsGraphics::DispatchList* lpDispatchList =
                lpDispatchFrame->GetList( liModelOnlyDisplayList );
            CGS_ASSERT( lpDispatchList != 0, "lpDispatchList" );
            if ( lpDispatchList == 0 )
            {
                continue;
            }

            // Bind the part's world matrix for this draw's shader constants (the console's
            // `sub_822B33B8(&mShaderConstantTable, 0, ...)` at pseudocode :1501/:1675).
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lPartWorldMatrix );

            const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

            lpDispatchFrame->GetBin().BeginPacket();
            if ( lbShadowPass )
            {
                // Shadow variant (pseudocode :1508): BOTH mesh lists are the OPAQUE one and
                // the draw is Z-only. Trailer bytes preZList 0xFF / preZTechnique 0 /
                // instanceCount 0 / excludeMeshBits 0 -- traffic has no damage-flag mask.
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueList ), static_cast< s8 >( liOpaqueList ),
                    1, lu8Technique, true,
                    0xFFu, 0u, 0, 0u );
            }
            else
            {
                // Camera variant (pseudocode :1682).
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueList ), static_cast< s8 >( liTransparentList ),
                    1, lu8Technique, false,
                    0xFFu, 0u, 0, 0u );
            }

            lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
        }
    }

    // ========================================================================
    // THE WHEEL BLOCK  (pseudocode :2177-:3120)
    //
    // Same shape as the race car's: build up to four world matrices plus their per-instance
    // constant vectors, then submit ONE instanced DrawRenderable. The per-wheel matrix is the
    // leak's algorithm (:8629-:8737) and the ship's asm agrees leg for leg:
    //   identity -> road-noise Y offset -> front wheels steer about Y by -steering ->
    //   rotate about X by the wheel rotation (front wheels +0.2 rad) -> * bodyTransform ->
    //   translate by the spec wheel position pushed through the inverse of the car-model ->
    //   handling-body transform -> * the per-wheel scale, with the LEFT wheels mirrored.
    // ========================================================================

    // The wheel contact positions the blobby shadow is built from. Collected even while gate
    // G6 is open, because they are the console's own by-product of the loop and collecting
    // them costs nothing -- see the gate at the tail.
    Vector3 laWheelContactPositions[ KI_TRAFFIC_WHEELS_TO_RENDER_MAX ];
    for ( s32 liInit = 0; liInit < KI_TRAFFIC_WHEELS_TO_RENDER_MAX; ++liInit )
    {
        laWheelContactPositions[ liInit ].SetZero();
    }

    // [DIAG T1-dispatch] the wheel-block outcome code, reported once per DISTINCT value:
    //   0 no deformation spec / block skipped   1 spec has no wheel model
    //   2 no LOD state on the model             3 no wheel matrix built
    //   4..8 SUBMITTED with 1..5 instances      9 GATED (model lacks the instance-shader flag)
    s32 liWheelDiagCode = 0;

    if ( lrPhysicsSpecPtr.HasMemoryResource() && lpWheelModel != 0 )
    {
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpPhysicsSpec =
            lrPhysicsSpecPtr.operator->();

        liWheelDiagCode = 1;

        // The wheels never draw at LOD 0 on the race car; traffic's own clamp is
        // `if (a42 <= 1) v440 = 1` at pseudocode :636 -- the SAME floor, applied to the LOD
        // the caller passed, and it is the value the whole wheel block uses.
        CgsGraphics::Model::State leWheelLOD = lLOD;
        if ( leWheelLOD <= CgsGraphics::Model::E_STATE_LOD_1 )
        {
            leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
        }
        if ( !lpWheelModel->DoesStateExist( leWheelLOD ) )
        {
            leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
        }

        if ( lpWheelModel->DoesStateExist( leWheelLOD ) )
        {
            liWheelDiagCode = 2;

            // Shader constant 25, "g_wheelConstants": the spin blur factor in lane x and
            // nothing else (`vrlimi128 v11, v0, 8, 0` -- mask 8 is lane X, so yzw stay zero).
            // ⛔ FLAG: both scales are unrecovered dyn-init constants (see the head of this
            // file), so the product is 0 today and the blurred technique is always selected.
            // The console arithmetic is kept rather than a divisor invented, exactly as the
            // race-car TU keeps its own all-zero gvWheelBlurConstants read.
            const f32 lfSpeed = lpVehicle->GetSpeed().x;
            const f32 lfScaledSpeed = lfSpeed * KF_TRAFFIC_WHEEL_SPIN_SPEED_SCALE;
            f32 lfBlur = lfScaledSpeed * KF_TRAFFIC_WHEEL_BLUR_SCALE;
            if ( lfBlur > 1.0f )   // vminfp128 against vcsxwfp(1) == 1.0f
            {
                lfBlur = 1.0f;
            }
            const Vector4 lv4WheelConstants = { lfBlur, 0.0f, 0.0f, 0.0f };
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 25, lv4WheelConstants );

            // `vcmpgtfp128. v0, unk_8300CC60, v124` + the CR6 "all lanes" bit: a wheel turning
            // slower than the threshold selects the non-blurred technique. Sticky across
            // wheels on the console (seeded outside the loop, only ever raised).
            u8 lu8WheelTechnique = 0u;
            if ( KF_TRAFFIC_WHEEL_BLUR_THRESHOLD > lfScaledSpeed )
            {
                lu8WheelTechnique = 1u;
            }
            if ( lbShadowPass )
            {
                lu8WheelTechnique = 2u;
            }

            // The inverse of the car-model -> handling-body transform, hoisted out of the loop
            // (the console rebuilds it per wheel; the value is loop-invariant and the leak
            // spells it as a per-iteration local purely because it was written that way).
            const Matrix44Affine lHandlingBodyToCarModel =
                rw::math::vpu::Inverse( lpPhysicsSpec->mCarModelSpaceToHandlingBodySpaceTransform );

            Matrix44Affine        laWheelMatrices[ KI_TRAFFIC_WHEELS_TO_RENDER_MAX ];
            const Matrix44Affine* lapWheelMatrices[ KI_TRAFFIC_WHEELS_TO_RENDER_MAX ] = { 0, 0, 0, 0 };

            // ⚠️ Sized to what SetupShaderConstantsForInstancing READS
            // (KU_MAX_INSTANCES_PER_GROUP == 5), not to what the console DECLARES (4) -- the
            // console overreads its own stack slot by 16 bytes. Same correction the race-car
            // TU already carries; it changes no value the console produces.
            Vector4 laWheelConstants[ CgsGraphics::Model::KU_MAX_INSTANCES_PER_GROUP ] =
                { { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f },
                  { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f } };

            s32 liInstanceCount = 0;

            // giWheelsToRender is re-read on every iteration on the console (a debug slider can
            // move under the loop), so it is read here and not hoisted.
            for ( s32 liWheel = 0;
                  liWheel < giWheelsToRender && liWheel < KI_TRAFFIC_WHEELS_TO_RENDER_MAX;
                  ++liWheel )
            {
                // BrnPhysics::Vehicle::VehiclePhysics wheel order (the leak's enumerators):
                // 0 front-left, 1 front-right, 2 rear-left, 3 rear-right.
                const bool lbIsFrontWheel = ( liWheel == 0 || liWheel == 1 );
                const bool lbIsLeftWheel  = ( liWheel == 0 || liWheel == 2 );

                // ⛔ NAMED GATE (road noise). The console draws a per-wheel road-noise
                // amplitude from mEffectRand (its LCG is inlined at pseudocode :2823 --
                // `1284865837 * seed + 1`, writing the module's own ring buffer at +4992/+5000)
                // and offsets the wheel by it in Y, scaled by a speed ramp between
                // KF_VEHICLE_MIN/MAX_ROAD_NOISE_SPEED. It is OMITTED, not faked: the ramp's two
                // endpoints and KF_VEHICLE_ROAD_NOISE_AMPLITUDE_SCALE are module tuning members
                // seeded by Construct @0x82740220 (cluster C4) and are not yet named in the PC
                // layout. A PARKED car's amplitude is identically zero (speed 0 clamps the ramp
                // to 0), so this omission is invisible for this wave's milestone and only
                // matters for driving traffic. ⛔ DELETE-WHEN C4 names the three constants.
                const f32 lfRoadNoiseAmplitude = 0.0f;

                Matrix44Affine lWheelMatrix;
                lWheelMatrix.SetIdentity();
                lWheelMatrix.wAxis.y += lfRoadNoiseAmplitude;

                if ( lbIsFrontWheel )
                {
                    // `rw::math::RotateAroundYAxis( m, steering * -1 )` (leak :8676).
                    lWheelMatrix = rw::math::vpu::Mult(
                        lWheelMatrix, rw::math::vpu::MakeRotationY( -lfSteering ) );
                }

                // The front wheels lead the rears by a fixed 0.2 rad (leak :8666).
                const f32 lfThisWheelRotation = lbIsFrontWheel ? ( lfWheelRot + 0.2f )
                                                               : lfWheelRot;
                lWheelMatrix = rw::math::vpu::Mult(
                    lWheelMatrix, rw::math::vpu::MakeRotationX( lfThisWheelRotation ) );

                // The wheel's authored position, expressed back in car-model space, then
                // rotated (NOT translated) into the world by the body transform -- which is
                // why the leak adds `Mult3x3( lWheelPos, lBodyTransform )` to the W axis
                // AFTER the `*= lBodyTransform` rather than transforming the point once.
                const BrnPhysics::Deformation::WheelSpec* lpWheelSpec =
                    lpPhysicsSpec->GetWheelSpec( liWheel );
                if ( lpWheelSpec == 0 )
                {
                    continue;
                }

                const Vector3 lWheelPosInCarModel =
                    rw::math::vpu::TransformPoint( lHandlingBodyToCarModel, lpWheelSpec->mPosition );

                lWheelMatrix = rw::math::vpu::Mult( lWheelMatrix, lBodyTransform );

                lWheelMatrix.wAxis.x += lWheelPosInCarModel.x * lBodyTransform.xAxis.x
                                      + lWheelPosInCarModel.y * lBodyTransform.yAxis.x
                                      + lWheelPosInCarModel.z * lBodyTransform.zAxis.x;
                lWheelMatrix.wAxis.y += lWheelPosInCarModel.x * lBodyTransform.xAxis.y
                                      + lWheelPosInCarModel.y * lBodyTransform.yAxis.y
                                      + lWheelPosInCarModel.z * lBodyTransform.zAxis.y;
                lWheelMatrix.wAxis.z += lWheelPosInCarModel.x * lBodyTransform.xAxis.z
                                      + lWheelPosInCarModel.y * lBodyTransform.yAxis.z
                                      + lWheelPosInCarModel.z * lBodyTransform.zAxis.z;

                // The contact point: the wheel centre dropped by half the wheel's Y scale
                // along the body's UP axis (leak :8713-8714). This is the ONLY thing the
                // blobby shadow needs from the loop.
                laWheelContactPositions[ liWheel ].x =
                    lWheelMatrix.wAxis.x - lBodyTransform.yAxis.x * ( lpWheelSpec->mScale.y * 0.5f );
                laWheelContactPositions[ liWheel ].y =
                    lWheelMatrix.wAxis.y - lBodyTransform.yAxis.y * ( lpWheelSpec->mScale.y * 0.5f );
                laWheelContactPositions[ liWheel ].z =
                    lWheelMatrix.wAxis.z - lBodyTransform.yAxis.z * ( lpWheelSpec->mScale.y * 0.5f );
                laWheelContactPositions[ liWheel ].w = 0.0f;

                // The per-wheel scale, applied on the LEFT (leak :8717-8720).
                Matrix44Affine lWheelScaleMatrix;
                lWheelScaleMatrix.SetIdentity();
                lWheelScaleMatrix.xAxis.x = lpWheelSpec->mScale.x;
                lWheelScaleMatrix.yAxis.y = lpWheelSpec->mScale.y;
                lWheelScaleMatrix.zAxis.z = lpWheelSpec->mScale.z;
                lWheelMatrix = rw::math::vpu::Mult( lWheelScaleMatrix, lWheelMatrix );

                // The left-hand wheels are the right-hand mesh MIRRORED through X and Z
                // (leak :8723-8727) -- one wheel model serves both sides.
                if ( lbIsLeftWheel )
                {
                    lWheelMatrix.xAxis.x = -lWheelMatrix.xAxis.x;
                    lWheelMatrix.xAxis.y = -lWheelMatrix.xAxis.y;
                    lWheelMatrix.xAxis.z = -lWheelMatrix.xAxis.z;
                    lWheelMatrix.zAxis.x = -lWheelMatrix.zAxis.x;
                    lWheelMatrix.zAxis.y = -lWheelMatrix.zAxis.y;
                    lWheelMatrix.zAxis.z = -lWheelMatrix.zAxis.z;
                }

                // Appended at the COMPACTED index, exactly as the console advances its
                // instance cursor only for a wheel it actually built.
                laWheelMatrices[ liInstanceCount ]  = lWheelMatrix;
                laWheelConstants[ liInstanceCount ] = lv4WheelConstants;
                lapWheelMatrices[ liInstanceCount ] = &laWheelMatrices[ liInstanceCount ];
                ++liInstanceCount;
            }

            liWheelDiagCode = 3;

            if ( liInstanceCount > 0 )
            {
                liWheelDiagCode = 3 + liInstanceCount;

                // ⛔ THE CONSOLE'S OWN GATE, honoured here for the reason the race-car TU
                // documents at length: `CGS_ASSERT( lpWheelModel->GetFlag(
                // E_FLAG_MODEL_USES_INSTANCE_SHADER ) )` (the assert string is baked into the
                // traffic body too, pseudocode :1502 "lbInstancing == GetFlag(...)"). A model
                // without the flag is not fit for the instanced path and submitting it anyway
                // draws screen-filling shards. The flag is COMPUTED AT LOAD by
                // ModelResourceType::PostFixUp, so a correctly-ported wheel bundle carries it.
                if ( !lpWheelModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ) )
                {
                    liWheelDiagCode = 9;
                }
                else
                {
                    const CgsGraphics::Renderable* lpWheelRenderable =
                        lpWheelModel->GetRenderable( leWheelLOD );
                    CgsGraphics::DispatchList* lpWheelList =
                        lpDispatchFrame->GetList( liModelOnlyDisplayList );
                    CGS_ASSERT( lpWheelList != 0, "lpDispatchList" );

                    if ( lpWheelRenderable != 0 && lpWheelList != 0 )
                    {
                        // Constants 6 (the instance matrices) and 7 (the per-instance spin
                        // vectors) go up BEFORE the packet opens -- AddToBin drains whatever
                        // the constant table has marked dirty at that moment.
                        CgsGraphics::Model::SetupShaderConstantsForInstancing(
                            liInstanceCount, lapWheelMatrices, laWheelConstants );

                        const bool lbFirstInList = ( lpWheelList->GetCount() & 0x7F ) == 0;

                        lpDispatchFrame->GetBin().BeginPacket();
                        // Pseudocode :2994: frustumEnable 0 (the wheels ride inside the body's
                        // own bounds), preZList 0xFF, preZTechnique 0, excludeMeshBits 0, and
                        // the instance count that makes this ONE command draw every wheel.
                        CgsGraphics::DrawRenderable::AddToBin(
                            lpWheelRenderable, lpDispatchFrame, lbFirstInList,
                            static_cast< s8 >( liOpaqueList ),
                            static_cast< s8 >( liTransparentList ),
                            0, lu8WheelTechnique, lbShadowPass,
                            0xFFu, 0u, liInstanceCount, 0u );

                        lpWheelList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
                    }
                }
            }
        }
    }

    // ---- G6: the blobby ground shadow --------------------------------------
    // ⛔ NAMED GATE, and the guard is the point. The console's tail (pseudocode :3183, the
    // leak :8777-:8786) is
    //     if ( !lpShadowMap->IsRenderingShadowMap() && !lpVehicle->IsPhysical() )
    //         lpBlobbyShadowRenderer->AddShadow( <quad built from the four contact points> );
    // TWO reasons it does not run: (a) NOTHING ON THIS BUILD OWNS A BrnBlobbyShadowManager --
    // no code anywhere calls InputBuffer_Dispatch::SetBlobbyShadowBuffer, so the handle is
    // always null and the console's own entry assert (@0x8273B39C) is deliberately not
    // reproduced in GenerateDispatchLists; and (b) the PC AddShadow signature is
    // (Matrix44Affine&, Vector4&, VecFloat, VecFloat) -- a transform plus packed extents --
    // where the leak's four-position form is stale, and the console's construction of those
    // four arguments is a dense VMX cross-product block that is NOT decoded here. Inventing
    // the arguments would be fabrication; the contact points ARE collected above so the leg is
    // a pure addition when the owner lands.
    // ⛔ DELETE-WHEN a BrnBlobbyShadowManager owner exists AND the argument construction is
    // decoded from @0x8272B868..0x8272B930.
    if ( lpBlobbyShadowRenderer != 0 && !lpShadowMap->IsRenderingShadowMap() )
    {
        static bool sbLoggedBlobbyGate = false;
        if ( !sbLoggedBlobbyGate && CgsDev::Log::gpDebugPrint != 0 )
        {
            sbLoggedBlobbyGate = true;
            *CgsDev::Log::gpDebugPrint
                << "[T1-dispatch] RenderTrafficCar: a blobby shadow buffer EXISTS but the"
                   " AddShadow argument construction is not reconstructed -- no traffic ground"
                   " shadow is posted [FLAG PC partial gate]\n";
        }
    }
    (void)laWheelContactPositions;

    // ---- [DIAG T1-dispatch] the first-car one-shot + the wheel outcome -----
    // ⛔ DELETE-WHEN parked traffic is confirmed on a booted run. Gated on BRN_TRAFFIC_DIAG.
    {
        static const bool skbTrafficDiag = ( std::getenv( "BRN_TRAFFIC_DIAG" ) != 0 );
        if ( skbTrafficDiag && CgsDev::Log::gpDebugPrint != 0 )
        {
            // The single line that says the whole chain (spawn -> streamer -> pre-dispatch ->
            // dispatch -> submission) closed for the first time. A genuine one-shot: it can
            // only ever be true once, and it is the event, not a per-frame number.
            static bool sbLoggedFirstCar = false;
            if ( !sbLoggedFirstCar )
            {
                sbLoggedFirstCar = true;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-dispatch] FIRST RenderTrafficCar: vehicle " << static_cast< s32 >( luEntityIdx )
                    << " type " << static_cast< s32 >( luVehicleType )
                    << " asset " << static_cast< s32 >( luAssetId )
                    << " parts " << static_cast< s32 >( lpGraphicsSpec->muPartsCount )
                    << " lod " << static_cast< s32 >( lLOD )
                    << " at (" << lBodyTransform.wAxis.x << ", " << lBodyTransform.wAxis.y
                    << ", " << lBodyTransform.wAxis.z << ")"
                    << " physicsSpec " << ( lrPhysicsSpecPtr.HasMemoryResource() ? 1 : 0 )
                    << "\n";
            }

            // Latched on the VALUE: this block has five different ways to draw no wheels and a
            // printed-once flag could only ever report the first car's answer.
            static u32 suSeenWheelDiagCodes = 0u;
            const u32  luBit = 1u << static_cast< u32 >( liWheelDiagCode );
            if ( ( suSeenWheelDiagCodes & luBit ) == 0u )
            {
                suSeenWheelDiagCodes |= luBit;
                *CgsDev::Log::gpDebugPrint
                    << "[T1-dispatch] RenderTrafficCar wheel block outcome " << liWheelDiagCode
                    << " (0 no deformation spec, 1 no wheel model, 2 no LOD state, "
                       "3 no wheel built, 4..8 submitted with 1..5 instances, "
                       "9 GATED -- model lacks E_FLAG_MODEL_USES_INSTANCE_SHADER); "
                       "wheels-to-render " << giWheelsToRender << "\n";
            }
        }
    }
}

}  // namespace BrnTraffic
