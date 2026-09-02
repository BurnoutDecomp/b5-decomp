// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/
//   BrnTrafficEntityModule_Render.cpp
//
// The TRAFFIC render leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::TrafficEntityModule::PreDispatchUpdate     @ 0x8274D900   (EXPORT HOLE)
//   BrnTraffic::TrafficEntityModule::GenerateDispatchLists @ 0x8273B280
//   BrnTraffic::TrafficEntityModule::RenderTrafficCar      @ 0x82728B08   (3,252 ln)
//
// All three parameter lists are the DecFIGS DWARF's
// (dwarfdump/_compile/BrnTrafficUnity.cpp :19315 / :23926 / :23103), each cross-checked
// against the X360 call site and the callee prologue.
//
// The render model follows the ship, not the Feb-2007 leak. The leak
// (BrnTrafficEntityModule.cpp:8497) is the structural key, mapping one-for-one onto ship
// pseudocode :633-656, but its IO contract is stale twice over: it computes visibility and LOD
// inline over a stack RenderSortInfo[600] where the ship splits that into PreDispatchUpdate ->
// Array<VehicleRenderInfo,64> -> WorldModule::CalculateVehicleLODs -> GenerateDispatchLists,
// and it submits through CgsGraphics::Model::Render where the ship uses
// DrawRenderable::AddToBin + DispatchList::Submit into the three dispatch lists (12 object /
// 19 opaque mesh / 20 transparent mesh). So this file mirrors BrnRaceCarEntityModule_Render.cpp:
// same packet shape, technique-bit scheme, instanced wheel draw and shadow-pass split.
//
// NAMED GATES, each with its blocker:
//   G1 the replay-serialiser pose source (module +468256 selects it);
//   G2 CLOSED -- a physical traffic car RENDERS: its wheels come from
//      TrafficPhysicsInfo::maWheelTransforms and its blobby ground shadow is suppressed;
//   G3 the glass-fracture reset + the damaged-vehicle budget leg;
//   G4 the detached-body-part override table;
//   G5 SubmitCoronasForVehicle @0x82727BB0 / RenderTrafficLightCoronas @0x8271EC80;
//   G6 BrnBlobbyShadowBuffer::AddShadow -- the buffer has no owner on this build.
// PreDispatchUpdate is otherwise real, including the FastBitArray duplicate suppression and the
// species-dispatched liveness predicate; GenerateDispatchLists is complete apart from G5;
// RenderTrafficCar's entry gates, asset/spec resolution, paint colour, body transform, shader
// constants 20/21/22/23/24/26, body-part loop and wheel loop are real.
//
// THE FOUR "UNRECOVERED" .rodata CONSTANTS ARE NOW RECOVERED (2026-08-23, wheel-blur bug wave).
// unk_8300D000 (the constant-24 scale), unk_8300C9A0 / unk_8300C8F0 (the wheel-blur speed scale
// pair) and unk_8300CC60 (the blur technique threshold) do read zero in the shipped image, and
// this file was right that they are seeded by unnamed MSVC dyn-init thunks in 0x82C6xxxx that no
// per-function export carries. They were read out of the DECRYPTED XEX2 BASEFILE instead -- no
// .i64 walk needed: decrypt (devkit key) + de-"basic"-compress the basefile at load address
// 0x82000000, scan .text for the instruction that materialises each address, and follow the
// thunk's `lfs` to its .rodata literal. Per-constant evidence sits at each definition below.
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
#include <cstdlib>  // getenv / atoi ([tdef-upload] witness, opt-in)

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied by
// the CgsShaderConstants TU). Same extern the sibling render TUs carry.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

// [DIAG] the renderer's present counter -- BRN_FRAME_DUMP names its BMPs bb_<guPresentCount>.bmp,
// so a [tdef-upload] line stamped with it names the very frame file its upload is drawn in.
// NOT X360. Same extern BrnUpdateVehiclesJob.cpp carries.
namespace renderengine { extern u32 guPresentCount; }

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

// (G-WHEELEXISTS closed 2026-09-02: TrafficPhysicsInfo::mabWheelExists is written every frame
// by ProcessDeformationData @0x8271DEB0, now bodied and live in _wT1_01.cpp; the physical
// wheel arm below reads the array directly, as the console does.)

// The verlet-offset array length shader constant 22 declares (128 registers). Same block the
// race car publishes; see the deliberate-deviation note at the publish site.
static const u32 KU_VERLET_OFFSET_COUNT = 128u;

// g_NullVerletOffsets -- the leak names this global verbatim at BrnTrafficEntityModule.cpp
// :8580 (`SetShaderConstantArrayData( E_VERLET_OFFSETS, g_NullVerletOffsets )`). In the
// Feb-2007 leak traffic never deformed and this all-zero block was the only one it published;
// the SHIP publishes the live TrafficPhysicsInfo::maSkinningOffsets_Scratch block for a
// deforming car (see the deforming arm in RenderTrafficCar) and this zero block is the PC
// backend's stand-in for "not published" on every other car (deviation noted at the site).
// Zero-initialised here, which is the value, not a stand-in.
static const Vector4 gaNullVerletOffsets[ KU_VERLET_OFFSET_COUNT ] = {};

// ---- the four unrecovered dyn-init constants (see the file banner) ----------------------
// ---- RECOVERED 2026-08-23 (wheel-blur bug wave) -----------------------------------------
// All four do read zero in the shipped image and ARE written by unnamed dyn-init thunks in
// 0x82C6xxxx that no per-function export carries -- so the old "unrecoverable without an .i64
// xref walk" note was half right. The missing half: the thunks are plainly visible in the
// DECRYPTED XEX2 BASEFILE. Decrypt (devkit key) + de-"basic"-compress the basefile at load
// address 0x82000000, scan .text for the instruction that materialises each .bss address, and
// follow the thunk's `lfs` to its .rodata literal. Each definition below carries its thunk.

// unk_8300D000 -- `lvx128 v0, r0, unk_8300D000 ; vmulfp128 v1, v118, v0` @ pseudocode :1291,
// i.e. constant 24 == lRearLights * splat(this).
// RECOVERED = 8.0f. Dyn-init thunk @0x82C66758:
//     lis r11,0x820C / lfs f0, flt_820BA8E0 (0x41000000 == 8.0f) / lis r11,0x8301 /
//     stfs f0,-0x10(r1) / lvlx v0,r0,r10 / addi r11,r11,0xD000 (-> 0x8300D000) /
//     vspltw v0,v0,0 / stvx128 v0,r0,r11 / blr
// The race car's positional twin (unk_82FAD990 -> flt_82004C88) also measures 8.0f; that
// agreement is now a CROSS-CHECK, not the evidence -- this global was read on its own.
// Still inert today, because lRearLights is all-zero while its producer
// SubmitCoronasForVehicle is gate G5; landing that producer is what makes it visible.
static const f32 KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY = 8.0f;

// unk_8300C9A0 and unk_8300C8F0 -- the wheel-spin blur pair:
//   `vmulfp128 v124, v117, unk_8300C9A0` (v117 == Vehicle::GetSpeed())   @0x8272A9CC
//   `vmulfp128 v0,   v124, unk_8300C8F0` -> vminfp against 1.0f -> constant 25 lane X.
// RECOVERED:
//   unk_8300C9A0 = 3.3333333f  -- dyn-init @0x82C66C78 from flt_8205873C (0x40555555).
//        That is 1 / 0.3, i.e. metres-per-second -> radians-per-second for a 0.3 m traffic
//        wheel, which is what makes v124 an ANGULAR VELOCITY and lets it be compared against
//        the same 30 rad/s threshold the race car uses.
//   unk_8300C8F0 = 1.0f / 30.0f == 0.033333335f -- dyn-init @0x82C65C48, which READS
//        unk_8300CC60 back, divides flt_82001C98 (1.0f) by it and splats the quotient. It is
//        literally the reciprocal of the threshold below, so the product
//        (speed * 3.3333) * (1/30) is `angularVelocity / 30` -- the race car's divide,
//        strength-reduced to a multiply.
static const f32 KF_TRAFFIC_WHEEL_SPIN_SPEED_SCALE = 3.3333333f;
static const f32 KF_TRAFFIC_WHEEL_BLUR_SCALE       = 1.0f / 30.0f;

// unk_8300CC60 -- `vcmpgtfp128. v0, unk_8300CC60, v124` @0x8272AA10: a wheel turning slower
// than this selects the non-blurred technique (the exact shape of the race car's
// `gvWheelBlurConstants.x > angularVelocity` test).
// RECOVERED = 30.0f. Dyn-init thunk @0x82C65C20 from flt_820BA5E8 (0x41F00000 == 30.0f) --
// the SAME number, reached the same way, as the race car's unk_82FAD6F0 (flt_8201499C).
// WITH IT AT 0.0f THE TEST WAS `0 > 0`, FALSE, so lu8WheelTechnique never left 0 (the BLURRED
// variant) and every traffic wheel rendered blurred, parked cars included -- the traffic half
// of the reported "wheels in the junkyard are blurred" bug.
static const f32 KF_TRAFFIC_WHEEL_BLUR_THRESHOLD = 30.0f;

// ============================================================================
// PreDispatchUpdate  @ 0x8274D900   (EXPORT HOLE -- reconstructed from the leak's
//                                    GenerateDispatchLists prologue re-shaped to the ship
//                                    split, plus the ship's own member promotions)
//
// Walk the frustum-filtered traffic entity ids the world module published, keep the ones
// whose vehicle is alive and inside the render cull radius, and hand the dispatch leg a
// near-to-far sorted, capped Array<VehicleRenderInfo,64>.
//
// Ship vs leak, two real divergences: the leak reads a file-scope KF_RENDER_CULL_DISTANCE_SQ
// and KU_MAX_VEHICLES_TO_RENDER where the ship promoted both to members,
// mfRenderCullDistanceSq (DWARF :690, shipped 62500.0f == 250 m squared, lane 2 of the vector
// at 0x8300CF10 seeded by the dyn-init thunk @0x82C66F18) and muMaxVehiclesToRender (:689);
// and the leak's second output, a corona list culled at KF_CORONA_CULL_DISTANCE_SQ, has no
// counterpart here because the ship's corona pass re-walks this array.
//
// mLOD is NOT written here. WorldModule::CalculateVehicleLODs writes it afterwards
// (`stw r9, 8(r3)` @0x827C3C34) into these same records, between the two locks, which is why
// BrnWorldModule.cpp brackets PreDispatchUpdate and CalculateVehicleLODs together.
//
// @0x8274D900 is an export hole: the nearest neighbours in
// .ida-exports/BURNOUT_X360_ARTIST.XEX are 0x8274C870 and 0x8274E508, so there is no
// pseudocode and no assembly for this body. The DecFIGS DWARF is therefore the highest rung
// available, and its scope/callee tree at dwarfdump/_compile/BrnTrafficUnity.cpp
// :19315-:19425 is the specification. Transcribed in source order:
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
// Two console behaviours worth calling out:
//
// (1) The duplicate suppression. maTrafficEntityIds is filled by
//     WorldModule::FilterFrustumTestResults from the scene manager's coarse-test results, and
//     an entity landing in more than one result appears more than once. The leak has no bit
//     array; the ship spends a 601-bit one per call. Without it a duplicated id is appended
//     twice, the same parked car is submitted twice into dispatch lists 12/19/20, and it burns
//     two muMaxVehiclesToRender slots, silently.
//     The DWARF instantiation is FastBitArray<601>, DecFIGS' KU_MAX_TOTAL_TRAFFIC, but the ship
//     value is 600 (GetVehicleSpecies @0x821F4648 asserts `luIndex < 0x258`), so this is
//     <KU_MAX_TOTAL_TRAFFIC>, not <601>.
//
// (2) The species-dispatched liveness predicate `lbAboutToDie`. The ship does not ask the
//     Vehicle, it asks the vehicle's param, and which param depends on species:
//       E_SPECIES_STANDARD -> GetParam(luVehicle)->IsAlive()
//       E_SPECIES_STATIC   -> GetStaticTrafficParamFromFullV(luVehicle)->IsAlive()
//       E_SPECIES_TRAILER  -> GetVehicle(luVehicle)->GetCabIndex(), then GetParam(cab)->IsAlive()
//     GetStaticTrafficParamFromFullV @0x827078D0 is the console's own truncation of the DWARF's
//     GetStaticTrafficParamFromFullVehicleIndex, not a different function.
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
    // See note (1) in this function's header for the ship-vs-DWARF extent.
    CgsContainers::FastBitArray< KU_MAX_TOTAL_TRAFFIC > lVehiclesAlreadySeen;
    lVehiclesAlreadySeen.UnSetAll();

    // The producer (WorldModule::FilterFrustumTestResults) has already kept only the
    // owner-byte-2 (traffic) ids, so every entry here names a traffic vehicle slot.
    const u32 luVisibleCount = lrVisible.GetLength();
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
            // DWARF :13776. This is the parked-car arm.
            const StaticTrafficParam* lpParam = GetStaticTrafficParamFromFullV( luVehicle );
            lbAboutToDie = !lpParam->IsAlive();
        }
        else
        {
            // The trailer arm, DWARF :13786-:13789. GetCabIndex is X360 @0x8270E4C8, fourteen
            // instructions (an IsOfTrailerSpecies() assert plus `lhz 2(this)`), DWARF-declared
            // at BrnTrafficVehicle.h:339. The console asks the CAB's param, not the trailer's
            // own alive flag; the two can differ for a frame while a cab is dying.
            const u32 luCab = GetVehicle( luVehicle )->GetCabIndex();
            lbAboutToDie = !GetParam( luCab )->IsAlive();
        }

        if ( lbAboutToDie )
        {
            continue;
        }

        // The array is structurally 64 slots and the console's cap, muMaxVehiclesToRender, is
        // applied after the sort. Appending past capacity fires Array's "out of space" assert,
        // so this is a bounded near-to-far insertion sort rather than append-then-qsort: same
        // ordering, same kept set (the nearest N), no slot the container does not have. The
        // console can append freely because its pre-sort scratch is the caller's 600-entry
        // stack array; here the 64-entry output array is the object being sorted into.
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
    // ---- step 1 -------------------------------------------------------------
    // Camera::operator= @0x8273B2C8 (module +0x72890, DWARF :881), before the state gates.
    // Its Pos row is the +0x728C0 lane the two proximity culls and UpdateVehicles' job split
    // read, so this store must stay ahead of them.
    mCameraLastFrame = lBrnCamera;

    // ---- step 2, NAMED GATE (link, not knowledge) --------------------------
    // The unconditional store at module +0x71870 is FuzzyBehaviourLogic::mDEBUGLastCameraPos,
    // not a member of this class (see the arithmetic on the RenderTrafficCar declaration in
    // BrnTrafficEntityModule.h). FuzzyBehaviourLogic::DEBUGSetLastCameraPos is declaration-only
    // and BrnTrafficFuzzyLogicBehaviours.cpp is not on tools/build/build_game_exe.bat, so
    // calling it leaves an unresolved external. Debug-render-only; omitting it draws the same.
    // DELETE-WHEN that TU is mounted with a real body:
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
    // The console also asserts lpBlobbyShadowRenderer (:14055) and lpCoronaSubmissionInterface
    // (:14060). Neither is asserted here: nothing in this tree owns a BrnBlobbyShadowManager and
    // nothing calls InputBuffer_Dispatch::SetBlobbyShadowBuffer, so the handle is legitimately
    // null on this build (gate G6), and asserting it turns a known gap into a per-frame stop.
    // DELETE-WHEN a BrnBlobbyShadowManager owner exists.

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
    // These are not a stub. They are the console's own state whenever the corona pass is skipped,
    // which it is on every shadow-map pass, and they are out params of SubmitCoronasForVehicle
    // that RenderTrafficCar consumes. Passing zeros while gate G5 is open is faithful.
    Vector4 laFrontLights[ 64 ];
    Vector4 laRearLights[ 64 ];
    for ( u32 luZero = 0; luZero < luLength && luZero < 64u; ++luZero )
    {
        laFrontLights[ luZero ].SetZero();
        laRearLights[ luZero ].SetZero();
    }

    // ---- step 7: the corona pass -- NAMED GATE G5 --------------------------
    // The console's shape, kept as the record of what must land:
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
    // BLOCKERS: SubmitCoronasForVehicle @0x82727BB0 (DWARF :22445) and RenderTrafficLightCoronas
    // @0x8271EC80 (DWARF :7711, an export hole) have no declaration in this tree; the monitor id
    // at +0x72A38 is a ship-only member with no DWARF name. The order is load-bearing: the corona
    // pass runs before the draw pass and only off the shadow pass, matching the race-car leg's
    // own mbRenderingShadowMap `continue`.
    // DELETE-WHEN the two producers land.

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
//   bit3 (PHYSICAL) -> GetTrafficPhysicsInfoForVehicl (asserting non-null, :15106) and the
//   "not physical" latch cleared. Then GetVehicleType, GetVehicleTransform,
//   GetPitch_Roll_Steering_WheelRot, GetSpeed.
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
    // NAMED GATE. When module +468256 is set the console takes the vehicle's type, damage record
    // and transform from BrnReplays::TrafficEntitySerialiser (module +468160) instead of from the
    // live pools, and derives the "not physical" latch from the serialised flag bit 5 rather than
    // from Vehicle::mxFlags. BLOCKER: the serialiser is not reconstructed
    // (TrafficEntitySerialiser::GetVehicleData / GetPhysicsInfo / GetVehicleTransform are
    // undeclared here) and the member at +468256 is unnamed. The live arm below runs outside
    // replay playback. DELETE-WHEN the replay wave lands.

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
    // FLAG (partial gate read): the console requires flags bit 1 as well as bit 0
    // (`rlwinm r10, r11, 0,30,30` @0x82728C10, beq -> bail). Vehicle's PC flag enum does not name
    // bit 1 yet, and GetFlags() is the only raw accessor, so the test goes through it rather than
    // inventing a predicate. Naming stays with whoever owns BrnTrafficVehicle.h.
    if ( ( lpVehicle->GetFlags() & 0x02u ) == 0u )
    {
        return;
    }

    // ---- G2 (CLOSED): the physical (promoted) arm --------------------------
    // `rlwinm r11, r11, 0,28,28` @0x82728C1C -> Vehicle::IsPhysical(); the console then fetches
    // GetTrafficPhysicsInfoForVehicl(idx), asserts it non-null (pseudocode :3188-:3197, baked
    // .cpp line 15106) and clears the "not physical" latch (`LOBYTE(v68) = 0` @0x82733200's
    // twin at :3200). That latch does three things downstream, and all three are now honoured:
    //   * the wheel block takes maWheelTransforms instead of composing the locator (see below);
    //   * the blobby ground shadow is suppressed at the tail (`if (v68) ... AddShadow`);
    //   * the damaged verlet-offset upload is enabled -- still G3, see that gate.
    // The accessor landed with the keystone (_wT3_00.cpp) and returns the real
    // TrafficPhysicsInfo*; the record itself has always been modelled and Constructed
    // (_wT1_03.cpp). Nothing here is gated any more.
    const bool lbIsPhysical = lpVehicle->IsPhysical();
    const TrafficPhysicsInfo* lpPhysicsInfo = 0;
    if ( lbIsPhysical )
    {
        lpPhysicsInfo = GetTrafficPhysicsInfoForVehicl( luEntityIdx );
        CGS_ASSERT( lpPhysicsInfo != 0, "lpPhysicsInfo" );                       // baked .cpp 15106
    }

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
    // `CreateFromHandle(local, 32*VehicleType + this + 482156)`, a 32-byte stride because a
    // console CgsResource::ResourcePtr is 32 bytes. That stride is X360 and must not survive: on
    // the host the array is indexed by name and the host ResourcePtr has its own size.
    // FLAG (extent): the DWARF declares this array KU_MAX_VEHICLE_ASSETS (64) long, but the
    // console indexes it by vehicle type, bounded by KU_MAX_VEHICLE_TYPES (96). The leak and the
    // asm agree on the index; the extent is still to reconcile. Bounded here so a type past 64
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

    // ---- the deforming-vehicle arm: glass-fracture reset, the LIVE verlet block, the
    //      damaged-vehicle budget and the technique index (0x82729794..0x8272984C) ------------
    // G3 CLOSED 2026-09-02 (traffic-deformation wave). The console:
    //     if ( physicsInfo && physicsInfo->mbIsDeforming            // `lbz 0xFE5(info)`
    //          && *lpiUpdatedNumDamagedVehiclesRendered < 5 )
    //     {
    //         ++*lpiUpdatedNumDamagedVehiclesRendered;
    //         BrnTraffic::SetGlassFractureConstants( 0.0f, 1.0f, {0,0}, {0,0,0,0} ); // f31, f30, v127
    //         SetShaderConstantArrayData( 22, &physicsInfo->maSkinningOffsets_Scratch[0] ); // info+0x530
    //         SetShaderConstantData( 23, {0,0,0,0} );
    //         technique = shadow ? 3 : 0;
    //     }
    //     else
    //     {
    //         BrnTraffic::SetGlassFractureConstants( 0.0f, 1.0f, {0,0}, {0,0,0,0} );
    //         technique = shadow ? 2 : 1;
    //     }
    // The callee is the traffic module's OWN SetGlassFractureConstants @0x82714848 (not the
    // BrnWorld twin the old note named), bodied in
    // BrnTrafficEntityModule_ProcessDeformationData.cpp; mbIsDeforming is OR-accumulated by
    // HandleExternalResponses and the skinning offsets are copied by ProcessDeformationData
    // (both live), so the arm's three inputs all exist on this build now.
    //
    // The technique index is two bits, exactly the race car's scheme: bit 0 == "not
    // deforming", bit 1 == "shadow pass".
    const Vector2 lv2NoFractureUVScale   = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector4 lv4NoFractureUVOffsets = { 0.0f, 0.0f, 0.0f, 0.0f };
    const bool lbDeforming = ( lpPhysicsInfo != 0 )
                          && lpPhysicsInfo->mbIsDeforming
                          && ( *lpiUpdatedNumDamagedVehiclesRendered < 5 );
    u8 lu8Technique;
    if ( lbDeforming )
    {
        ++*lpiUpdatedNumDamagedVehiclesRendered;
        SetGlassFractureConstants( 0.0f, 1.0f, lv2NoFractureUVScale, lv4NoFractureUVOffsets );

        // ---- [tdef-upload] NOT X360; opt-in on the BRN_DEFORM_TRACE latch (the same one the
        // race car's [deform-upload] control shares). The statistic the GPU is about to see --
        // max / sum / non-zero count over the 128 rows -- off the very array handed to
        // constant 22, printed only when it CHANGES, so a deforming traffic car's upload can
        // be read against the physics side's [tdef] line for the same car on the same frame.
        // DELETE-WHEN the traffic-deformation question is banked.
        {
            static s32 siUploadOn = -1;
            if ( siUploadOn < 0 )
            {
                const char* lpcEnv = getenv( "BRN_DEFORM_TRACE" );
                siUploadOn = ( lpcEnv != 0 && atoi( lpcEnv ) > 0 ) ? 1 : 0;
            }
            if ( siUploadOn == 1 && CgsDev::Log::gpDebugPrint != 0 )
            {
                static f32 sfLastSum = -1.0f;
                static u32 sluLines  = 0;
                const Vector3Plus* lpV = lpPhysicsInfo->maSkinningOffsets_Scratch;
                f32 lfMax = 0.0f;
                f32 lfSum = 0.0f;
                s32 liNnz = 0;
                for ( u32 luRow = 0; luRow < TrafficPhysicsInfo::KU_NUM_SKINNING_OFFSETS; ++luRow )
                {
                    const f32 lfMag = sqrtf( lpV[ luRow ].x * lpV[ luRow ].x
                                           + lpV[ luRow ].y * lpV[ luRow ].y
                                           + lpV[ luRow ].z * lpV[ luRow ].z );
                    if ( lfMag > lfMax ) { lfMax = lfMag; }
                    if ( lfMag > 0.0f ) { lfSum += lfMag; ++liNnz; }
                }
                if ( lfSum != sfLastSum && sluLines < 400u )
                {
                    ++sluLines;
                    sfLastSum = lfSum;
                    *CgsDev::Log::gpDebugPrint
                        << "[tdef-upload] frame=" << static_cast< s32 >( renderengine::guPresentCount )
                        << " veh " << luEntityIdx
                        << " technique " << ( lbShadowPass ? 3 : 0 )
                        << " budget " << *lpiUpdatedNumDamagedVehiclesRendered
                        << " maxVerlet " << lfMax
                        << " sumVerlet " << lfSum
                        << " nnz " << liNnz
                        << " fatal " << ( lpPhysicsInfo->mbIsFatallyCrashing ? 1 : 0 )
                        << " glassFlags " << static_cast< u32 >( lpPhysicsInfo->mu8RenderDamageFlags )
                        << "\n";
                }
            }
        }

        // The console's Vector3Plus* overload @0x822B3458 is the same 16-byte-row copy as the
        // Vector4* one this tree bodies; the race-car render TU passes its Vector3Plus block
        // through the same cast.
        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData(
            22, reinterpret_cast< const Vector4* >( lpPhysicsInfo->maSkinningOffsets_Scratch ) );
        {
            const Vector4 lv4DamageConstants = { 0.0f, 0.0f, 0.0f, 0.0f };
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 23, lv4DamageConstants );
        }
        lu8Technique = lbShadowPass ? 3u : 0u;
    }
    else
    {
        SetGlassFractureConstants( 0.0f, 1.0f, lv2NoFractureUVScale, lv4NoFractureUVOffsets );
        lu8Technique = lbShadowPass ? 2u : 1u;
    }

    // ---- shader constants 20 / 21: the paint ------------------------------
    // Both slots take the same vector on traffic (`vmr128 v1, v121` twice, pseudocode
    // :1284-:1288), unlike the race car, which publishes a separate pearlescent tint. The vector
    // is VehicleTypeRuntime::PickPaintColourForVehicle(luEntityIdx, numColours,
    // mpData->mpaPaintColours), seeded by the vehicle index so a parked car keeps its colour.
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
    // (pseudocode :1289-:1294) with v118 == lRearLights, the 5th vector argument. Traffic derives
    // its lamp emission from the corona producer's out-vector, where the race car uses per-car
    // brake/reverse booleans, which is why the DWARF names the pair lFrontLights / lRearLights.
    // FLAG: the scale is one of this file's four unrecovered dyn-init constants; see its
    // declaration at the head of the file for the consequence.
    // FLAG: lFrontLights has no observed consumer in the reconstructed legs (only v5 is re-homed
    // at the prologue). Carried rather than dropped, because SubmitCoronasForVehicle writes both
    // and a leg inside one of the gates above presumably reads it. Do not delete it to silence
    // the warning.
    (void)lFrontLights;
    {
        const Vector4 lv4SelfIlluminationMask = {
            lRearLights.x * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.y * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.z * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
            lRearLights.w * KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 24, lv4SelfIlluminationMask );
    }

    // ---- shader constants 22 / 23 on the NON-deforming arm ------------------
    // The leak publishes g_NullVerletOffsets here unconditionally (:8580) and a zeroed damage
    // vector as 23 (:8577); the ship folds the live block into the deforming arm above, and
    // publishes NOTHING for 22/23 on a car that is not deforming.
    //
    // DELIBERATE DEVIATION, the same one the race-car render TU carries: leaving the block
    // unpublished is not neutral on the PC backend. Sixteen vertex programs in SHADERS_PC.BNDL
    // declare g_verletOffsets at c0 count 128, and an external constant whose source pointer
    // is null is SKIPPED, not zeroed (shadowingdevice.cpp:847), so the program reads whatever
    // the preceding draw left in c0..c127 -- the panel-stretch defect. The null block costs one
    // 2 KB copy per car. It is published ONLY when the deforming arm did not just publish the
    // live block (2026-09-02: it used to follow unconditionally, which would have overwritten
    // the live offsets with zeros on the very cars that deform).
    if ( !lbDeforming )
    {
        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData( 22, gaNullVerletOffsets );
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
    // G4 (detached parts). Before the loop the console builds a 128-slot lookup from part index
    // to detached-part record (pseudocode :1319-:1345, asserting luPartCount < 128 at :15368 and
    // luDetachedPartIndex < luPartCount at :15374) and, for a part that has a record, takes the
    // record's four matrix rows as the world matrix instead of composing the locator. BLOCKER:
    // the detached-part queue is TrafficPhysicsInfo::mDetachedPartQueue, which nothing on this
    // build ever fills -- detached parts are the deformation wave, not this one. A car with no
    // detached parts takes the composed path below, which is the console's own else-arm.
    // DELETE-WHEN traffic deformation lands.
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


    if ( lrPhysicsSpecPtr.HasMemoryResource() && lpWheelModel != 0 )
    {
        const BrnPhysics::Deformation::StreamedDeformationSpec* lpPhysicsSpec =
            lrPhysicsSpecPtr.operator->();

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
            // Shader constant 25, "g_wheelConstants": the spin blur factor in lane x and
            // nothing else (`vrlimi128 v11, v0, 8, 0` -- mask 8 is lane X, so yzw stay zero).
            // Both scales and the threshold are now RECOVERED (see their definitions at the
            // head of this file); the console arithmetic below is unchanged, only the numbers
            // it operates on stopped being placeholder zeros.
            const f32 lfSpeed = lpVehicle->GetSpeed().x;

            // lfScaledSpeed IS an angular velocity in rad/s (m/s divided by the 0.3 m wheel
            // radius), which is why it can be compared against the same 30 rad/s threshold
            // the race car applies to GetWheelAngularVelocity.
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
            // wheels on the console (seeded outside the loop, only ever raised). The console
            // compares all four lanes of a VecFloat splat, so this scalar test is the same
            // predicate.
            CGS_ASSERT( KF_TRAFFIC_WHEEL_BLUR_THRESHOLD > 0.0f,
                        "KF_TRAFFIC_WHEEL_BLUR_THRESHOLD > 0.0f" );
            u8 lu8WheelTechnique = 0u;
            if ( KF_TRAFFIC_WHEEL_BLUR_THRESHOLD > lfScaledSpeed )
            {
                lu8WheelTechnique = 1u;
            }
            if ( lbShadowPass )
            {
                lu8WheelTechnique = 2u;
            }

            // [FLAG PC bring-up] ONE-SHOT traffic wheel-blur witness -- the traffic twin of the
            // race car's [wheel-blur] line. One line for the first traffic car drawn, never
            // per frame. DELETE-WHEN the blur is confirmed on a moving traffic car.
            {
                static bool sbLoggedTrafficWheelBlur = false;
                if ( !sbLoggedTrafficWheelBlur && CgsDev::Log::gpDebugPrint != 0 )
                {
                    sbLoggedTrafficWheelBlur = true;
                    *CgsDev::Log::gpDebugPrint
                        << "[traffic-wheel-blur] threshold " << KF_TRAFFIC_WHEEL_BLUR_THRESHOLD
                        << " rad/s | speed " << lfSpeed
                        << " m/s -> angVel " << lfScaledSpeed
                        << " -> c25.x " << lfBlur
                        << " technique " << static_cast< s32 >( lu8WheelTechnique )
                        << " (0 blurred, 1 sharp, 2 shadow)\n";
                }
            }

            // The inverse of the car-model -> handling-body transform, hoisted out of the loop
            // (the console rebuilds it per wheel; the value is loop-invariant and the leak
            // spells it as a per-iteration local purely because it was written that way).
            const Matrix44Affine lHandlingBodyToCarModel =
                rw::math::vpu::Inverse( lpPhysicsSpec->mCarModelSpaceToHandlingBodySpaceTransform );

            Matrix44Affine        laWheelMatrices[ KI_TRAFFIC_WHEELS_TO_RENDER_MAX ];
            const Matrix44Affine* lapWheelMatrices[ KI_TRAFFIC_WHEELS_TO_RENDER_MAX ] = { 0, 0, 0, 0 };

            // Sized to what SetupShaderConstantsForInstancing reads
            // (KU_MAX_INSTANCES_PER_GROUP == 5), not to what the console declares (4): the
            // console overreads its own stack slot by 16 bytes. Same correction the race-car TU
            // carries; it changes no value the console produces.
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
                // ---- THE PHYSICAL ARM (pseudocode :2209-:2300, `if (v439)`) ----------------
                // A promoted car's wheels are already posed by the physics side: the console
                // takes TrafficPhysicsInfo::maWheelTransforms[i] (info+3376+64*i) and applies
                // ONLY the spec's per-wheel scale on the left -- no road noise, no steer, no
                // wheel-rotation compose, and NO left-side mirror (the physics transforms are
                // already per-corner). The per-wheel `if` is mabWheelExists[i] (info+4040+i),
                // so a shed wheel simply does not draw.
                if ( lpPhysicsInfo != 0 )
                {
                    // Console: `if (info[4040 + i])` -- mabWheelExists[i], written every frame
                    // by ProcessDeformationData @0x8271DEB0 from the deformation wheel state
                    // (LIVE since 2026-09-02; the G-WHEELEXISTS force is gone). A wheel the
                    // physics side reports as not existing simply does not draw.
                    if ( !lpPhysicsInfo->mabWheelExists[ liWheel ] )
                    {
                        continue;
                    }

                    const BrnPhysics::Deformation::WheelSpec* lpPhysicalWheelSpec =
                        lpPhysicsSpec->GetWheelSpec( liWheel );
                    if ( lpPhysicalWheelSpec == 0 )
                    {
                        continue;
                    }

                    Matrix44Affine lPhysicalScale;
                    lPhysicalScale.SetIdentity();
                    lPhysicalScale.xAxis.x = lpPhysicalWheelSpec->mScale.x;
                    lPhysicalScale.yAxis.y = lpPhysicalWheelSpec->mScale.y;
                    lPhysicalScale.zAxis.z = lpPhysicalWheelSpec->mScale.z;

                    laWheelMatrices[ liInstanceCount ] = rw::math::vpu::Mult(
                        lPhysicalScale, lpPhysicsInfo->maWheelTransforms[ liWheel ] );
                    laWheelConstants[ liInstanceCount ] = lv4WheelConstants;
                    lapWheelMatrices[ liInstanceCount ] = &laWheelMatrices[ liInstanceCount ];
                    ++liInstanceCount;
                    continue;
                }


                // BrnPhysics::Vehicle::VehiclePhysics wheel order (the leak's enumerators):
                // 0 front-left, 1 front-right, 2 rear-left, 3 rear-right.
                const bool lbIsFrontWheel = ( liWheel == 0 || liWheel == 1 );
                const bool lbIsLeftWheel  = ( liWheel == 0 || liWheel == 2 );

                // NAMED GATE (road noise). The console draws a per-wheel road-noise amplitude
                // from mEffectRand (its LCG is inlined at pseudocode :2823, `1284865837 * seed
                // + 1`, writing the module's ring buffer at +4992/+5000) and offsets the wheel
                // by it in Y, scaled by a speed ramp between KF_VEHICLE_MIN/MAX_ROAD_NOISE_SPEED.
                // BLOCKER: that ramp's endpoints and KF_VEHICLE_ROAD_NOISE_AMPLITUDE_SCALE are
                // module tuning members seeded by Construct @0x82740220 and are not named in the
                // PC layout. A parked car's amplitude is zero anyway (speed 0 clamps the ramp),
                // so this only matters for driving traffic.
                // DELETE-WHEN the three constants are named.
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

            if ( liInstanceCount > 0 )
            {
                // The console's own gate: `CGS_ASSERT( lpWheelModel->GetFlag(
                // E_FLAG_MODEL_USES_INSTANCE_SHADER ) )`, whose assert string is baked into the
                // traffic body too (pseudocode :1502, "lbInstancing == GetFlag(...)"). A model
                // without the flag is not fit for the instanced path, and submitting it anyway
                // draws screen-filling shards. ModelResourceType::PostFixUp computes the flag at
                // load, so a correctly-ported wheel bundle carries it.
                if ( lpWheelModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ) )
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
    // NAMED GATE. The console's tail (pseudocode :3183, leak :8777-:8786) is
    //     if ( !lpShadowMap->IsRenderingShadowMap() && !lpVehicle->IsPhysical() )
    //         lpBlobbyShadowRenderer->AddShadow( <quad built from the four contact points> );
    // TWO BLOCKERS: nothing on this build owns a BrnBlobbyShadowManager (no code calls
    // InputBuffer_Dispatch::SetBlobbyShadowBuffer, so the handle is always null, which is why
    // GenerateDispatchLists does not reproduce the console's entry assert @0x8273B39C); and the
    // PC AddShadow signature is (Matrix44Affine&, Vector4&, VecFloat, VecFloat), a transform plus
    // packed extents, where the leak's four-position form is stale and the console builds those
    // arguments in a dense VMX cross-product block that is not decoded. The contact points are
    // collected above, so the leg is a pure addition once the owner lands.
    // DELETE-WHEN a BrnBlobbyShadowManager owner exists and the argument construction is decoded
    // from @0x8272B868..0x8272B930.
    // `if (v68)` -- the "not physical" latch. A promoted car does NOT get a blobby shadow (it
    // is in the real shadow map by then), which is what the leak spells as
    // `if (!lpVehicle->IsPhysical()) AddShadow(...)`.
    if ( lpBlobbyShadowRenderer != 0 && !lpShadowMap->IsRenderingShadowMap() && !lbIsPhysical )
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
}

}  // namespace BrnTraffic
