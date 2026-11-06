// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/TrafficEntityModule/
//   BrnTrafficEntityModule_Render.cpp
//
// The TRAFFIC render leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnTraffic::TrafficEntityModule::PreDispatchUpdate     @ 0x8274D900   (EXPORT HOLE)
//   BrnTraffic::TrafficEntityModule::GenerateDispatchLists @ 0x8273B280
//   BrnTraffic::TrafficEntityModule::RenderTrafficCar      @ 0x82728B08   (3,252 ln)
//   BrnTraffic::TrafficEntityModule::RenderTrafficLightCoronas @ 0x8271EC80 (EXPORT HOLE; L3 2026-09-27)
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
//   G2 a physical traffic car RENDERS: its wheels come from
//      TrafficPhysicsInfo::maWheelTransforms; its main-view blobby-shadow producer uses the
//      road-test normal/height and VehicleTypeRuntime bounds when PhysicsInfo is present;
//   G3 the glass-fracture reset + the damaged-vehicle budget leg;
//   G4 the detached-body-part override table;
//   G6 BrnBlobbyShadowBuffer::AddShadow -- main-view producer recovered from the ARTIST
//      VehicleTypeRuntime bounds, vehicle transform and optional physics road-test normal;
//   G7 RenderTrafficLightCoronas' replay-playback source (the serialiser at module +0x724C0).
// PreDispatchUpdate is otherwise real, including the FastBitArray duplicate suppression and the
// species-dispatched liveness predicate; GenerateDispatchLists submits vehicle coronas;
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
#include "rw/math/vpu/matrix44affine_operation.h"                           // Mult / MakeRotationX/Y/Z
#include "rw/math/vpu/vector3_operation.h"                                  // GetVector3_YAxis
#include "GameShared/GameClasses/Graphics/CgsCamera.h"                     // CgsGraphics::Camera (RenderTrafficLightCoronas)
#include "SharedClasses/Traffic/BrnTrafficPvs.h"                           // Pvs::GetHullIndexForPoint / ForIndices (the same)

#include <algorithm>
#include <float.h>
#include <cmath>    // powf / sqrtf
#include <cstdlib>  // getenv / atoi ([tdef-upload] witness, opt-in)
#include <cstdio>
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
// SubmitCoronasForVehicle supplies the rear-light vector before the draw pass.
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

// ARTIST 0x82728D88 reads 0x820BA5E8 for replay speed. The two speed
// limits in the road-noise ramp are 30 and 100 mph multiplied by 0x82F31928.
static const f32 KF_TRAFFIC_REPLAY_SPEED = 30.0f;
static const f32 KF_TRAFFIC_MPH_TO_METRES_PER_SECOND = 0.44704f;
static const f32 KF_TRAFFIC_ROAD_NOISE_START = 30.0f * KF_TRAFFIC_MPH_TO_METRES_PER_SECOND;
static const f32 KF_TRAFFIC_ROAD_NOISE_END = 100.0f * KF_TRAFFIC_MPH_TO_METRES_PER_SECOND;
static const f32 KF_TRAFFIC_ROAD_NOISE_SPEED_SCALE =
    1.0f / (KF_TRAFFIC_ROAD_NOISE_END - KF_TRAFFIC_ROAD_NOISE_START);
static const f32 KF_TRAFFIC_MAX_ROAD_NOISE_AMPLITUDE = 0.223f; // 0x8200D508
static const f32 KF_TRAFFIC_FRONT_WHEEL_ROTATION_OFFSET = 0.2f; // 0x82004744

// The wheel matrix products accumulate each XYZ coefficient over the right
// operand's rows. Translation starts with the right operand's translation.
static __vector4 MultiplyWheelBasisRow(__vector4 lvRow, const Matrix44Affine& lrRight)
{
    __vector4 lvResult = __vmulfp(__vspltw(lvRow, 0), lrRight.xAxis.mV);
    lvResult = __vmaddfp(__vspltw(lvRow, 1), lrRight.yAxis.mV, lvResult);
    return __vmaddfp(__vspltw(lvRow, 2), lrRight.zAxis.mV, lvResult);
}

static Matrix44Affine MultiplyWheelTransform(const Matrix44Affine& lrLeft,
                                             const Matrix44Affine& lrRight)
{
    Matrix44Affine lResult;
    lResult.xAxis.mV = MultiplyWheelBasisRow(lrLeft.xAxis.mV, lrRight);
    lResult.yAxis.mV = MultiplyWheelBasisRow(lrLeft.yAxis.mV, lrRight);
    lResult.zAxis.mV = MultiplyWheelBasisRow(lrLeft.zAxis.mV, lrRight);
    const __vector4 lvRow = lrLeft.wAxis.mV;
    __vector4 lvTranslation = __vmaddfp(__vspltw(lvRow, 0), lrRight.xAxis.mV, lrRight.wAxis.mV);
    lvTranslation = __vmaddfp(__vspltw(lvRow, 1), lrRight.yAxis.mV, lvTranslation);
    lResult.wAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lrRight.zAxis.mV, lvTranslation);
    return lResult;
}

static Matrix44Affine ScaleWheelTransform(const Vector3& lrScale, const Matrix44Affine& lrTransform)
{
    Matrix44Affine lScale;
    const __vector4 lvZero = __vspltisw(0);
    lScale.xAxis.mV = __vrlimi(lvZero, lrScale.mV, 8, 0);
    lScale.yAxis.mV = __vrlimi(lvZero, lrScale.mV, 4, 0);
    lScale.zAxis.mV = __vrlimi(lvZero, lrScale.mV, 2, 0);
    lScale.wAxis.mV = lvZero;
    return MultiplyWheelTransform(lScale, lrTransform);
}

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
namespace
{
struct TrafficVehicleRenderInfoLess
{
    bool operator()(const VehicleRenderInfo& lhs, const VehicleRenderInfo& rhs) const
    {
        return lhs.mfDistanceSq < rhs.mfDistanceSq;
    }
};
}

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
    //
    // ⭐⭐ IT IS THE SoA MEMBER, NOT A LOCAL (fixed 2026-09-06, traffic-demotion wave).
    // This body was reconstructed from the DWARF alone because @0x8274D900 is an export hole,
    // and the DWARF names only the TYPE -- so "a FastBitArray is cleared here and bits are set
    // in it" was modelled as a stack-local duplicate filter. The image says otherwise. Read
    // with tools/re/ppcdis.py, the console's clear is
    //     0x8274D998  slwi r10, r11, 0
    //     0x8274D9A0  addi r10, r10, 0x508C        ; 8 * 0x508C == 164960
    //     0x8274D9A8  slwi r10, r10, 3
    //     0x8274D9AC  stdx r22(0), r10, r30        ; ten doublewords, r30 == this
    // and its set is
    //     0x8274DBF4  addis r28, r29, 3 ; addi r28, r28, -0x7BA0   ; this + 0x28460 == 164960
    //     0x8274DCC4  stdx  r10, r11, r28
    // 164960 == mVehicleSoaData + 400 == mVehiclesRenderedLastFrame (the sixth 80-byte set;
    // TryClearupOffscreenTraffic @0x8273CAB8 names it in its own baked assert string). So the
    // duplicate filter and "which cars did the render pass touch last frame" are ONE bit set,
    // and the console publishes it. WHAT THE LOCAL COST: four readers saw a permanently empty
    // array -- TryClearupOffscreenTraffic (which then treats every car as never-rendered),
    // ClearupCrashedTraffic and NukeTrafficJams' kill selection (_wT5_01.cpp / _wT6_01.cpp),
    // and PostPhysicsUpdate's 80-byte copy into the crash-traffic input interface (+0xCA8).
    if (mbReplayPlayback)
    {
        // ARTIST 0x8274DDE0..E0AC walks recorded vehicle bits and poses,
        // independently of live scene IDs and the live rendered-last-frame set.
        const BrnReplays::TrafficEntitySerialiserStaticLayout* lpFrame =
            mTrafficEntitySerialiser.GetStaticLayout();
        VehicleRenderInfo laVisible[600];
        u32 luVisible = 0;
        for (s32 liVehicle = lpFrame->mVehicleUpdateBits.GetFirstBitSet();
             liVehicle >= 0;
             liVehicle = lpFrame->mVehicleUpdateBits.GetNextBitSet(liVehicle))
        {
            const u32 luVehicle = static_cast<u32>(liVehicle);
            const Vector3& lrPos = mTrafficEntitySerialiser.GetVehicleTransform(luVehicle)->mTransform.Pos();
            const __vector4 lvDelta = __vsubfp(lrPos.mV, lCameraPosition.mV);
            const f32 lfDistanceSq = __vmsum3fp(lvDelta, lvDelta).x;
            if (!(lfDistanceSq < mfRenderCullDistanceSq))
                continue;

            VehicleRenderInfo& lrInfo = laVisible[luVisible++];
            lrInfo.muEntityIndex = luVehicle;
            lrInfo.mfDistanceSq = lfDistanceSq;
            lrInfo.mLOD = CgsGraphics::Model::E_STATE_LOD_0;
        }
        std::sort(laVisible, laVisible + luVisible, TrafficVehicleRenderInfoLess());
        const u32 luCount = luVisible < muMaxVehiclesToRender ? luVisible : muMaxVehiclesToRender;
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
            lrRenderInfos.Append(laVisible[luIndex]);
    }
    else
    {
    mVehicleSoaData.mVehiclesRenderedLastFrame.UnSetAll();

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
        if ( mVehicleSoaData.mVehiclesRenderedLastFrame.IsBitSet( luVehicle ) )
        {
            continue;
        }

        // Distance cull. maVehicleTransforms is read directly rather than through
        // GetVehicleTransform because the console inlines that accessor here (it is a bare
        // indexed read plus a validity assert), and no declaration for it exists in the tree.
        const Vector3& lrPos = maVehicleTransforms[ luVehicle ].Pos();
        const __vector4 lvDelta = __vsubfp(lrPos.mV, lCameraPosition.mV);
        const f32 lfDistanceSq = __vmsum3fp(lvDelta, lvDelta).x;

        if ( lfDistanceSq >= mfRenderCullDistanceSq )
        {
            continue;
        }

        // DWARF :19315 -- `SetBit` is the FIRST call inside the `{ bool lbAboutToDie }` scope,
        // i.e. inside the body of the distance-cull `if` and BEFORE the liveness dispatch. So
        // a vehicle that passed the cull marks itself seen whether or not it is then kept: the
        // bit means "considered this frame", not "appended this frame". Reproduced exactly.
        mVehicleSoaData.mVehiclesRenderedLastFrame.SetBit( luVehicle );

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

// ARTIST 82727E3C..F08: magnitude with two Newton steps, then
// 1 + max(distance - 8, 0) * flt_820BE730 (1/142).
static f32 TrafficCoronaScale(Vector3 lPosition, Vector3 lCameraPosition)
{
    const __vector4 delta = __vsubfp(lPosition.mV, lCameraPosition.mV);
    const __vector4 squared = __vmsum3fp(delta, delta);
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    __vector4 reciprocal = __vrsqrtefp(squared);
    for (s32 i = 0; i != 2; ++i)
        reciprocal = __vmaddfp(__vmulfp(reciprocal, half),
                               __vnmsubfp(squared, __vmulfp(reciprocal, reciprocal), one),
                               reciprocal);
    const __vector4 distance = __vsel(__vmulfp(squared, reciprocal), zero, __vcmpeqfp(zero, squared));
    const Vector4 eight(8.0f, 8.0f, 8.0f, 8.0f);
    const Vector4 factor(0.0070422534f, 0.0070422534f, 0.0070422534f, 0.0070422534f);
    Vector4 result;
    result.mV = __vmaddfp(__vsubfp(__vcfsx(__vspltisw(2), 0), one),
        __vmulfp(__vmaxfp(__vsubfp(distance, eight.mV), zero), factor.mV), one);
    return result.mV.x;
}

// ARTIST 82727BB0. Replay records have the same 4112-byte physical snapshot
// as TrafficPhysicsInfo (SetPhysicsData copies that entire array).
void TrafficEntityModule::SubmitCoronasForVehicle(
    BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
    u32 luVehicle, Vector3 lCameraPosition, Vector3 /*lCameraDirection*/,
    Vector4& lFrontLights, Vector4& lRearLights)
{
    if (mbTrafficIsHidden)
        return;
    CGS_ASSERT(lpCoronaSubmissionInterface != 0, "lpCoronaSubmissionInterface != NULL");

    const TrafficPhysicsInfo* lpPhysicsInfo = 0;
    Matrix44Affine lTransform;
    u32 luVehicleType;
    f32 lfHeadlightWarmth;
    f32 lfSpotHeadlightWarmth;
    f32 lfIndicatorWarmth;
    bool lbBrake, lbLeft, lbRight, lbAlarm;
    if (mbReplayPlayback)
    {
        const BrnReplays::TrafficVehicleDataRecord* lpData = mTrafficEntitySerialiser.GetVehicleData(luVehicle);
        const s8 liPhysics = lpData->miPhysicsIndex;
        if (liPhysics >= 0)
            lpPhysicsInfo = mTrafficEntitySerialiser.GetPhysicsInfo(liPhysics);
        lTransform = mTrafficEntitySerialiser.GetVehicleTransform(luVehicle)->mTransform;
        luVehicleType = lpData->muVehicleType;
        const u8 flags = lpData->muFlags;
        lfHeadlightWarmth = (flags & 1) ? 1.0f : 0.0f;
        lfIndicatorWarmth = 1.0f;
        lfSpotHeadlightWarmth = lfHeadlightWarmth;
        lbBrake = (flags & 2) != 0;
        lbLeft = (flags & 4) != 0;
        lbRight = (flags & 8) != 0;
        lbAlarm = (flags & 16) != 0;
    }
    else
    {
        const Vehicle* lpVehicle = GetVehicle(luVehicle);
        if (!lpVehicle->HasEntity())
            return;
        if (lpVehicle->IsPhysical())
        {
            lpPhysicsInfo = GetTrafficPhysicsInfoForVehicl(luVehicle);
            if (lpPhysicsInfo->mbIsFatallyCrashing)
                return;
        }
        lTransform = GetVehicleTransform(luVehicle);
        luVehicleType = lpVehicle->GetVehicleType();
        lfHeadlightWarmth = lpVehicle->GetHeadlightWarmth();
        lfIndicatorWarmth = lpVehicle->GetIndicatorBulbWarmth();
        lfSpotHeadlightWarmth = lpVehicle->GetHeadlightWarmth();
        lbBrake = lpVehicle->AreBrakelightsOn();
        lbLeft = lpVehicle->IsLeftIndicatorOn();
        lbRight = lpVehicle->IsRightIndicatorOn();
        lbAlarm = lpVehicle->IsAlarmOn();
    }

    const f32 lfFrontRunningOpacity = 1.0f - lfHeadlightWarmth;
    const f32 lfDifference = 0.8f - lfSpotHeadlightWarmth;
    const f32 lfNonNegative = static_cast<f32>(__fsel(-lfDifference, 0.0f, lfDifference));
    const f32 lfSpotOpacity = static_cast<f32>(__fsel(0.8f - lfNonNegative, lfNonNegative, 0.8f));
    const f32 lfScale = TrafficCoronaScale(lTransform.wAxis, lCameraPosition);
    bool lbRunning = true;
    bool lbRear = true;
    if (GetVehicleSpecies(luVehicle) == Vehicle::E_SPECIES_STATIC)
    {
        if (lbAlarm)
            lbRunning = lbRear = lbBrake = lbLeft = lbRight = false;
        else
        {
            lbRunning = lbLeft && ((luVehicle & 4) == 4);
            lbRear = lbBrake = lbRight = lbLeft;
        }
    }
    if (lbRunning) lRearLights.SetY(1.0f);
    if (lbBrake) lRearLights.SetX(1.0f);

    const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec> lSpec(
        maTrafficVehiclePhysicsSpecs[luVehicleType].GetResourceHandle());
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec = lSpec.operator->();
    const Matrix44Affine lLocatorToWorld = rw::math::vpu::Mult(
        rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lpSpec->mCarModelSpaceToHandlingBodySpaceTransform),
        lTransform);
    const BrnPhysics::Deformation::LocatorPointSpecList& lrLocators = lpSpec->mLightTags;
    const u32 luCount = lpPhysicsInfo ? static_cast<u32>(lpPhysicsInfo->miNumLightLocators) : lrLocators.GetNumLocatorPoints();
    const Vector3 lForward = lTransform.zAxis;
    const Vector3 lBackward = rw::math::vpu::Negate(lForward);
    for (u32 i = 0; i < luCount; ++i)
    {
        s32 liType;
        Vector3 lPosition;
        if (lpPhysicsInfo)
        {
            liType = static_cast<s8>(lpPhysicsInfo->maLightTagPointTypes[i]);
            CGS_ASSERT(liType <= BrnPhysics::Deformation::E_TAGPOINT_COUNT,
                       "lpPhysicsInfo->maLightTagPointTypes[luTagPoint] <= BrnPhysics::Deformation::E_TAGPOINT_COUNT");
            lPosition = lpPhysicsInfo->maLightLocatorPositions[i];
        }
        else
        {
            liType = lrLocators.GetLocatorTy(i);
            lPosition = lrLocators.GetLocatorXf(i)->wAxis;
        }
        BrnCoronaType leType = eCoronaTypeTrafficHeadLight;
        Vector3 lDirection = lForward;
        f32 lfOpacity = 1.0f;
        bool lbSubmit = true;
        switch (liType)
        {
        case 1: case 2:
            if (lbRunning)
            {
                const Vector3 lWorld = rw::math::vpu::TransformPoint(lLocatorToWorld, lPosition);
                lpCoronaSubmissionInterface->AddCorona(lWorld, lForward, lfScale, 1.0f, eCoronaTypeTrafficHeadLight);
                lpCoronaSubmissionInterface->AddCorona(lWorld, lForward, lfScale, lfFrontRunningOpacity, eCoronaTypeTrafficHeadLight);
                lFrontLights.SetY(1.0f);
            }
            continue;
        case 3: case 4: lbSubmit = lbRear; leType = eCoronaTypeTrafficRearLight; lDirection = lBackward; break;
        case 5: case 6:
            lbSubmit = lbRunning; leType = eCoronaTypeTrafficSpotlights; lfOpacity = lfSpotOpacity;
            if (lbSubmit) lPosition = lrLocators.GetLocatorXf(i)->wAxis;
            break;
        case 7: case 9: lbSubmit = lbLeft; leType = eCoronaTypeTrafficIndicator; lfOpacity = lfIndicatorWarmth; if (liType == 9) lDirection = lBackward; break;
        case 8: case 10: lbSubmit = lbRight; leType = eCoronaTypeTrafficIndicator; lfOpacity = lfIndicatorWarmth; if (liType == 10) lDirection = lBackward; break;
        case 11: case 12: case 13: lbSubmit = lbBrake; leType = eCoronaTypeTrafficBrakeLight; lDirection = lBackward; break;
        default: lbSubmit = false; break;
        }
        if (lbSubmit)
            lpCoronaSubmissionInterface->AddCorona(rw::math::vpu::TransformPoint(lLocatorToWorld, lPosition), lDirection, lfScale, lfOpacity, leType);
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

    // ---- step 2 --------------------------------------------------------------
    // ARTIST 0x8273B2DC unconditionally stores the camera position to module +0x71870,
    // FuzzyBehaviourLogic::mDEBUGLastCameraPos, before either state gate.
    mFuzzyBehaviours.DEBUGSetLastCameraPos(lCameraPosition);

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
    BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface =
        lpInput->GetCoronaSubmissionInterface();
    CGS_ASSERT(lpCoronaSubmissionInterface != 0, "lpCoronaSubmissionInterface");

    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );
    // X360 :14055 asserts lpBlobbyShadowRenderer. Renderer Update publishes the external
    // BrnBlobbyShadowManager buffer and the renderer/world/entity bridges forward it to this
    // input; preserve the console's nonnull precondition now that that typed handoff is present.
    CGS_ASSERT( lpBlobbyShadowRenderer != 0, "lpBlobbyShadowRenderer" );

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
    // that RenderTrafficCar consumes.
    Vector4 laFrontLights[ 64 ];
    Vector4 laRearLights[ 64 ];
    for ( u32 luZero = 0; luZero < luLength && luZero < 64u; ++luZero )
    {
        laFrontLights[ luZero ].SetZero();
        laRearLights[ luZero ].SetZero();
    }

    // ---- step 7: the corona pass (0x8273B438..0x8273B4C4) --------------------
    // The console's shape:
    //   if ( !lpShadowMap->IsRenderingShadowMap() )                     lbz 0(r23) ; bne -> step 8
    //   {
    //       CgsDev::PerfMonCpu::StartMonitor( <ship-only monitor id at module +0x72A38> );
    //       for ( i = 0; i < luLength; ++i )
    //           SubmitCoronasForVehicle( lpInput->GetCoronaSubmissionInterface(),
    //                                    laTrafficRenderInfos[i].muEntityIndex,
    //                                    lCameraPosition, lCameraDirection,
    //                                    laFrontLights[i], laRearLights[i] );   // r6/r7 OUT
    //       CgsDev::PerfMonCpu::StopMonitor( ... );
    //       RenderTrafficLightCoronas( lpInput->GetCoronaSubmissionInterface(),
    //                                  lCameraPosition, lCameraDirection );  // bl 0x8271EC80 at 0x8273B4C4
    //   }
    // The monitor at +0x72A38 remains unnamed; submit the light data before drawing.
    if ( !lpShadowMap->IsRenderingShadowMap() )
    {
        for (u32 i = 0; i < luLength; ++i)
            SubmitCoronasForVehicle(lpCoronaSubmissionInterface,
                                    laTrafficRenderInfos[i].muEntityIndex,
                                    lCameraPosition, lCameraDirection,
                                    laFrontLights[i], laRearLights[i]);
        RenderTrafficLightCoronas( lpCoronaSubmissionInterface, lCameraPosition, lCameraDirection );
    }

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
// RenderTrafficLightCoronas  @ 0x8271EC80   (L3 RACEINTRO, 2026-09-27)
//
// The owner's report: "The races intro doesn't show the traffic light going green". A traffic light's lit lamps
// are its coronas, and this is the pass that submits them. It had no body and no call, so no traffic light ever
// lit up on the PC -- not the start light through the countdown, not a free-roam light cycling.
//
// DWARF BrnTrafficEntityModule.h :1845 (locals .cpp 3378..3425 on the PS3; the X360 tripwire cites .cpp:3359).
// A hole in the export set: read off the words 0x8271EC80..0x8271F290 with tools/re/ppcdis.py.
//   r3 this (r27) ; r4 lpCoronaSubmissionInterface (r15, spilled at 0x3EC) ; v1 lCameraPosition (v125) ;
//   v2 lCameraDirection (v124)
//   1. The light manager and the active-hull set: `lbzx +0x72520` -- live (0): &mTrafficLightManager (+0x53790)
//      and &mActiveHullsForLocalPlayer (+0x3EA9C); replay uses the corresponding frame members. The .cpp:3359 (0xD1F)
//      tripwire "lpTrafficLightManager".
//   2. PerfMonCpu::StartMonitor / StopMonitor (+0x72A30, then +0x72A34 around step 4): debug-only, left out.
//   3. Every ACTIVE hull (the Set's GetLength / operator[] tripwires, CgsSet.h:227 / :257 / :258):
//      lpHull = GetHull(id) (the BrnTrafficEntityModule.h:2229 tripwire), then RenderLightsForHull(lpHull,
//      &mpData->mTrafficLights, lpCoronaSubmissionInterface, lCameraPosition, lCameraDirection,
//      VecFloat(KF_RENDER_CULL_CORONA_DISTANCE_SQ)) (bl 0x8275DBF0 at 0x8271EE88): the lights in their own
//      states -- or, through an event countdown, in the countdown's.
//   4. The camera frustum's other hulls:
//      lCamera: mCameraLastFrame.CopyToCgsCamera (bl 0x8220AC48 at 0x8271EEC0), then
//        SetFarClipPlane(KF_RENDER_CULL_CORONA_DISTANCE), inlined as `stfs f0, 0x2E0(r1)` (camera +0x160) and
//        bl UpdatePerspectiveProjectionMatrix (0x8271EED4);
//      laFrustrumPoints: lCamera.GetFrustumPerspectiveVertices (bl 0x827E70E0 at 0x8271EEE0);
//      lFrustrumMin / lFrustrumMax start at {FLT_MAX x3, 0} / {-FLT_MAX x3, 0} (flt_820BA23C / flt_82035570, w
//        stored 0), then vminfp / vmaxfp with each of the eight points (0x8271EF34..0x8271EF4C);
//      mpData->mpPvs->GetHullIndexForPoint(lFrustrumMin, MinX, MinZ) and (lFrustrumMax, MaxX, MaxZ)
//        (bl 0x827106B8 at 0x8271EF74 / 0x8271EF98; both clamp into the grid);
//      for liHullZ = MinZ..MaxZ, liHullX = MinX..MaxX (cmpw, both inclusive):
//        the hull at GetHullIndexForIndices(liHullX, liHullZ) (inlined, the BrnTrafficPvs.h:188 tripwire);
//        lpHull = GetHull(it) (h:2229, BEFORE the set test);
//        when the hull is NOT active (CgsSet.h:332 Contains; Find bl 0x8270C598 at 0x8271F1C0 == -1):
//          RenderAllLightsToBeInStateForHull(lpHull, &mpData->mTrafficLights, 1 << E_TRAFFICLIGHTSTATE_GREEN
//          (li r6, 4 at 0x8271F244), lpCoronaSubmissionInterface, lCameraPosition, lCameraDirection,
//          VecFloat(KF_RENDER_CULL_CORONA_DISTANCE_SQ)) (bl 0x8275DE50 at 0x8271F25C).
//      So every light that no active hull simulates shows GREEN.
// ============================================================================
namespace
{
    // DWARF BrnTrafficTweakConstants.h:177 KF_RENDER_CULL_CORONA_DISTANCE: the frustum camera's far plane --
    // flt_8200A034 == 500.0f (0x43FA0000), loaded at 0x8271EECC.
    const f32 KF_RENDER_CULL_CORONA_DISTANCE = 500.0f;

    // DWARF BrnTrafficTweakConstants.h:232 KF_RENDER_CULL_CORONA_DISTANCE_SQ: the coronas' distance cull --
    // 0x82F2FE60 == 250000.0f (0x48742400), splatted by lvlx + vspltw128 at 0x8271EE54 / 0x8271F1D0. Its one
    // reference in the image is this function (tools/re/findinit.py: no writer).
    const f32 KF_RENDER_CULL_CORONA_DISTANCE_SQ = 250000.0f;

    // The frustum bounds' starting values: flt_820BA23C == 3.40282347e+38f (0x7F7FFFFF, FLT_MAX), negated by
    // flt_82035570 (0xFF7FFFFF).
    const f32 KF_FRUSTRUM_BOUND_START = FLT_MAX;

    u32 guTrafficLightCoronaPassCount     = 0;
    s32 giTrafficLightCoronaPassLinesLeft = 24;
}

void
TrafficEntityModule::RenderTrafficLightCoronas( BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
                                                Vector3 lCameraPosition,
                                                Vector3 lCameraDirection )
{
    const TrafficLightManager* lpTrafficLightManager = &mTrafficLightManager;
    const ActiveHullSet* lpActiveHulls = &mActiveHullsForLocalPlayer;
    if ( mbReplayPlayback )
    {
        // ARTIST 0x8271ECD8–0x8271ED08 selects both recorded inputs.
        const BrnReplays::TrafficEntitySerialiserStaticLayout* lpFrame =
            mTrafficEntitySerialiser.GetStaticLayout();
        lpTrafficLightManager = &lpFrame->mTrafficLightManager;
        lpActiveHulls = &lpFrame->mActiveHullsForLocalPlayer;
    }
    const ActiveHullSet& lrActiveHulls = *lpActiveHulls;
    CGS_ASSERT( lpTrafficLightManager != 0, "lpTrafficLightManager" );              // .cpp:3359

    const Vector4 lvCullDistance(KF_RENDER_CULL_CORONA_DISTANCE_SQ, KF_RENDER_CULL_CORONA_DISTANCE_SQ,
                                 KF_RENDER_CULL_CORONA_DISTANCE_SQ, KF_RENDER_CULL_CORONA_DISTANCE_SQ);
    const rw::math::vpu::VecFloat lfCullDistSq(lvCullDistance.mV);

    // ---- step 3: the active hulls ------------------------------------------
    for ( u32 luActiveHull = 0; luActiveHull < lrActiveHulls.GetLength(); ++luActiveHull )
    {
        const Hull* lpHull = GetHull( lrActiveHulls[ luActiveHull ] );
        lpTrafficLightManager->RenderLightsForHull( lpHull, &mpData->mTrafficLights, lpCoronaSubmissionInterface,
                                                    lCameraPosition, lCameraDirection, lfCullDistSq );
    }

    // ---- step 4: every other hull in the camera frustum, GREEN ----------------
    CgsGraphics::Camera lCamera;
    mCameraLastFrame.CopyToCgsCamera( &lCamera );
    lCamera.SetFarClipPlane( KF_RENDER_CULL_CORONA_DISTANCE );

    Vector3 laFrustrumPoints[ 8 ];
    lCamera.GetFrustumPerspectiveVertices( laFrustrumPoints );

    Vector3 lFrustrumMin(KF_FRUSTRUM_BOUND_START, KF_FRUSTRUM_BOUND_START, KF_FRUSTRUM_BOUND_START);
    Vector3 lFrustrumMax(-KF_FRUSTRUM_BOUND_START, -KF_FRUSTRUM_BOUND_START, -KF_FRUSTRUM_BOUND_START);
    for ( u32 luIndex = 0; luIndex < 8; ++luIndex )
    {
        const Vector3& lrPoint = laFrustrumPoints[ luIndex ];
        lFrustrumMin.mV = __vminfp(lFrustrumMin.mV, lrPoint.mV);
        lFrustrumMax.mV = __vmaxfp(lFrustrumMax.mV, lrPoint.mV);
    }

    s32 liHullIndexMinX = 0;
    s32 liHullIndexMinZ = 0;
    s32 liHullIndexMaxX = 0;
    s32 liHullIndexMaxZ = 0;
    mpData->mpPvs->GetHullIndexForPoint( lFrustrumMin, liHullIndexMinX, liHullIndexMinZ );
    mpData->mpPvs->GetHullIndexForPoint( lFrustrumMax, liHullIndexMaxX, liHullIndexMaxZ );

    u32 luDiagOtherHulls = 0;   // [DIAG] NOT X360
    for ( s32 liHullZ = liHullIndexMinZ; liHullZ <= liHullIndexMaxZ; ++liHullZ )
    {
        for ( s32 liHullX = liHullIndexMinX; liHullX <= liHullIndexMaxX; ++liHullX )
        {
            const u32   luHullIndex = mpData->mpPvs->GetHullIndexForIndices( liHullX, liHullZ );
            const Hull* lpHull      = GetHull( luHullIndex );
            if ( !lrActiveHulls.Contains( static_cast<u16>( luHullIndex ) ) )
            {
                lpTrafficLightManager->RenderAllLightsToBeInStateForHull(
                    lpHull, &mpData->mTrafficLights, 1u << E_TRAFFICLIGHTSTATE_GREEN, lpCoronaSubmissionInterface,
                    lCameraPosition, lCameraDirection, lfCullDistSq );
                ++luDiagOtherHulls;
            }
        }
    }

    // [DIAG] NOT X360 -- see TrafficLightCoronaPassDiagEnabled.
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
//        GetVehicleData / GetPhysicsInfo / GetVehicleTransform on it).
//
// ---- THE LIVE ARM (@0x82728BF8 onward) ----------------------------------------------
//   GetVehicle(idx) -> flags byte at +5: bit0 (ALIVE) and bit1 must BOTH be set, else bail;
//   bit3 (PHYSICAL) -> GetTrafficPhysicsInfoForVehicl (asserting non-null, :15106) and the
//   "not physical" latch cleared. Then GetVehicleType, GetVehicleTransform,
//   GetPitch_Roll_Steering_WheelRot and GetSpeed in the original.
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

    const Vehicle* lpVehicle = 0;
    const TrafficPhysicsInfo* lpPhysicsInfo = 0;
    bool lbNotPhysical;
    u32 luVehicleType;
    Matrix44Affine lBodyTransform;
    Vector4 lPitchRollSteerWheel;
    VecFloat lvfVehicleSpeed;

    // ARTIST 0x82728CFC..0x82728DE4. The signed slot and the physical
    // state bit are independent: the slot can be absent on a physical car.
    if (mbReplayPlayback)
    {
        const BrnReplays::TrafficVehicleDataRecord* lpVehicleData =
            mTrafficEntitySerialiser.GetVehicleData(luEntityIdx);
        const s8 liPhysicsIndex = lpVehicleData->miPhysicsIndex;
        if (liPhysicsIndex >= 0)
        {
            lpPhysicsInfo = mTrafficEntitySerialiser.GetPhysicsInfo(liPhysicsIndex);
            if (!lpPhysicsInfo)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpPhysicsInfo",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/"
                    "TrafficEntityModule/BrnTrafficEntityModule.cpp", 15125);
                CgsDev::Assert::EndAssert();
            }
        }
        luVehicleType = lpVehicleData->muVehicleType;
        const Matrix44Affine& lrRecordedTransform =
            mTrafficEntitySerialiser.GetVehicleTransform(luEntityIdx)->mTransform;
        lBodyTransform.xAxis.mV = lrRecordedTransform.xAxis.mV;
        lBodyTransform.yAxis.mV = lrRecordedTransform.yAxis.mV;
        lBodyTransform.zAxis.mV = lrRecordedTransform.zAxis.mV;
        lBodyTransform.wAxis.mV = lrRecordedTransform.wAxis.mV;
        lPitchRollSteerWheel.mV = __vspltisw(0);
        lvfVehicleSpeed = VecFloat(KF_TRAFFIC_REPLAY_SPEED);
        lbNotPhysical = (lpVehicleData->muFlags & BrnReplays::TrafficVehicleDataRecord::E_FLAG_NOT_PHYSICAL) != 0;
    }
    else
    {
        lpVehicle = GetVehicle(luEntityIdx);
        if (!lpVehicle || !lpVehicle->IsAlive() || (lpVehicle->GetFlags() & 0x02u) == 0u)
        {
            return;
        }

        lbNotPhysical = !lpVehicle->IsPhysical();
        if (!lbNotPhysical)
        {
            lpPhysicsInfo = GetTrafficPhysicsInfoForVehicl(luEntityIdx);
            CGS_ASSERT(lpPhysicsInfo != 0, "lpPhysicsInfo"); // ARTIST .cpp:15106
        }
        luVehicleType = lpVehicle->GetVehicleType();
    }

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

    // ARTIST 0x82728EA8..0x82728EC0 binds a local alias from the resource handle.
    const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec> lpPhysicsSpec(
        static_cast<const CgsResource::BaseResourcePtr&>(maTrafficVehiclePhysicsSpecs[luVehicleType]));

    const CgsGraphics::Model* lpWheelModel = lpWheelGraphicsSpec->GetWheelModel();
    CGS_ASSERT( lpWheelModel != 0, "lpWheelModel" );

    // ARTIST 0x82728EF8 retains this runtime record from the paint path through AddShadow. The
    // recovered producer reads its bbox offset, half-size and axle offsets before fog and draw.
    const VehicleTypeRuntime* lpVehicleTypeRuntime = GetVehicleTypeRuntime( luVehicleType );

    if (lpVehicle)
    {
        lBodyTransform = maVehicleTransforms[luEntityIdx];
        lPitchRollSteerWheel = lpVehicle->GetPitch_Roll_Steering_WheelRot();
        lvfVehicleSpeed = lpVehicle->GetSpeed();
    }
    const f32 lfPitch    = lPitchRollSteerWheel.mV.x;
    const f32 lfRoll     = lPitchRollSteerWheel.mV.y;
    const f32 lfSteering = lPitchRollSteerWheel.mV.z;
    const f32 lfWheelRot = lPitchRollSteerWheel.mV.w;

    const Matrix44Affine lBodyRollTransform =
        rw::math::vpu::Mult(
            rw::math::vpu::Mult( rw::math::vpu::MakeRotationX( lfPitch ),
                                 rw::math::vpu::MakeRotationZ( lfRoll ) ),
            lBodyTransform );

    // ---- the shadow-pass selector (identical to the race car's) ------------
    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap()
                           && lpShadowMap->IsUsingZOnlyRenderingPath();

    // ARTIST 827293B8..82729554: submit the main-view ground shadow.
    if (!lpShadowMap->IsRenderingShadowMap())
    {
        const Vector3 lOffset = lpVehicleTypeRuntime->GetBBoxOffset();
        const Vector3 lHalfSize = lpVehicleTypeRuntime->GetBBoxHalfSize();
        const Vector4 lAxles = lpVehicleTypeRuntime->GetCabPivot_TrailerPivot_BackAxle_FwdAxle();
        const __vector4 lOffsetZ = __vspltw(lOffset.mV, 2);
        const __vector4 lHalfZ = __vspltw(lHalfSize.mV, 2);
        __vector4 lPacked = __vrlimi(lAxles.mV, __vaddfp(lOffsetZ, lHalfZ), 8, 0);
        lPacked = __vrlimi(lPacked, __vsubfp(lOffsetZ, lHalfZ), 4, 0);
        CGS_ASSERT(lOffset.mV.x < 0.1f, "Vehicle has asymmetric bounding box round Z");
        const Vector4 lExtents(lPacked);
        const rw::math::vpu::VecFloat lWidth(__vspltw(lHalfSize.mV, 0));
        if (lbNotPhysical)
        {
            const rw::math::vpu::VecFloat lHeight(__vspltisw(0));
            lpBlobbyShadowRenderer->AddShadow(lBodyTransform, lExtents, lWidth, lHeight);
        }
        else if (lpPhysicsInfo != 0)
        {
            const __vector4 lNormal = lpPhysicsInfo->mvRoadTestNormal_HeightAboveRoad.mV;
            const __vector4 lForward = lBodyTransform.zAxis.mV;
            const __vector4 lNormalYZX = __vpermwi(lNormal, 0x63);
            const __vector4 lForwardYZX = __vpermwi(lForward, 0x63);
            __vector4 lRight = __vmulfp(lForward, lNormalYZX);
            lRight = __vnmsubfp(lForwardYZX, lNormal, lRight);
            lRight = __vpermwi(lRight, 0x63);
            __vector4 lNewForward = __vmulfp(lNormal, __vpermwi(lRight, 0x63));
            lNewForward = __vnmsubfp(lNormalYZX, lRight, lNewForward);
            lNewForward = __vpermwi(lNewForward, 0x63);
            const __vector4 lHeightVector = __vspltw(lNormal, 3);
            const __vector4 lUp = __vrlimi(__vspltisw(0), __vcfsx(__vspltisw(1), 0), 4, 0);
            Matrix44Affine lShadowTransform;
            lShadowTransform.xAxis.mV = lRight;
            lShadowTransform.yAxis.mV = lNormal;
            lShadowTransform.zAxis.mV = lNewForward;
            lShadowTransform.wAxis.mV = __vsubfp(lBodyTransform.wAxis.mV, __vmulfp(lUp, lHeightVector));
            lpBlobbyShadowRenderer->AddShadow(lShadowTransform, lExtents, lWidth,
                                               rw::math::vpu::VecFloat(lHeightVector));
        }
    }

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
    Vector2 lv2NoFractureUVScale;
    lv2NoFractureUVScale.mV = __vspltisw(0);
    const Vector4 lv4NoFractureUVOffsets(__vspltisw(0));
    const bool lbDeforming = (lpPhysicsInfo != 0)
        && lpPhysicsInfo->mbIsDeforming
        && (*lpiUpdatedNumDamagedVehiclesRendered < 5);
    u8 lu8Technique;
    if ( lbDeforming )
    {
        ++*lpiUpdatedNumDamagedVehiclesRendered;
        SetGlassFractureConstants( 0.0f, 1.0f, lv2NoFractureUVScale, lv4NoFractureUVOffsets );

        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData(
            22, lpPhysicsInfo->maSkinningOffsets_Scratch);
        const Vector4 lv4DamageConstants(__vspltisw(0));
        CgsGraphics::mShaderConstantTable.SetShaderConstantData(23, lv4DamageConstants);
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
    const Vector4 lv4PaintColour = lpVehicleTypeRuntime->PickPaintColourForVehicle(
        luEntityIdx, lpData->GetNumPaintColours(), lpData->mpaPaintColours);
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
        const Vector4 lv4Intensity(KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY, KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY,
                                  KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY, KF_TRAFFIC_SELF_ILLUMINATION_INTENSITY);
        const Vector4 lv4SelfIlluminationMask(__vmulfp(lRearLights.mV, lv4Intensity.mV));
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 24, lv4SelfIlluminationMask );
    }

    // ---- shader constant 26: the per-car FOG blend ------------------------
    // The console inlines a powf (the vlogefp / vexptefp pair plus the polynomial refinement
    // over the .rodata coefficient vectors at 0x82000BD0..0x82000C20, pseudocode :700-:1177);
    // The existing scalar reconstruction below remains unverified against that VMX pipeline:
    //     t = powf( saturate( distance * scattering.x - scattering.y ), scattering.z )
    //         * scattering.w
    //     constant26 = { fogColour.xyz * t, 1 - t }        (`vrlimi128 v1, v13, 1, 0`)
    // The distance is camera -> car, which the race car receives ready-made and traffic
    // computes here from lCameraPosition (that is what v120 is for).
    {
        const f32 lfDX = lBodyTransform.wAxis.mV.x - lCameraPosition.mV.x;
        const f32 lfDY = lBodyTransform.wAxis.mV.y - lCameraPosition.mV.y;
        const f32 lfDZ = lBodyTransform.wAxis.mV.z - lCameraPosition.mV.z;
        const f32 lfCameraDistance = sqrtf( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ );

        f32 lfBlend = lfCameraDistance * lFogScattering.mV.x - lFogScattering.mV.y;
        if ( lfBlend < 0.0f ) { lfBlend = 0.0f; }
        if ( lfBlend > 1.0f ) { lfBlend = 1.0f; }
        const f32 lfFog = powf( lfBlend, lFogScattering.mV.z ) * lFogScattering.mV.w;

        const Vector4 lv4Fog(lFogColourPlusWhiteLevel.mV.x * lfFog,
                             lFogColourPlusWhiteLevel.mV.y * lfFog,
                             lFogColourPlusWhiteLevel.mV.z * lfFog, 1.0f - lfFog);
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
            const CgsGraphics::Model::State lePartLod = lLOD;
            if ( !lpModel->DoesStateExist( lePartLod ) )
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

            const CgsGraphics::Renderable* lpRenderable = lpModel->GetRenderable( lePartLod );
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

    // ARTIST 0x8272A984..0x8272B800, including the non-instanced wheel model path.
    const CgsGraphics::Model::State lWheelLOD =
        (lLOD > CgsGraphics::Model::E_STATE_LOD_1) ? lLOD : CgsGraphics::Model::E_STATE_LOD_1;
    if (!lpWheelModel->DoesStateExist(lWheelLOD))
        return;

    const __vector4 lvZero = __vspltisw(0);
    const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lvAngularVelocity = __vmulfp(lvfVehicleSpeed.mV,
        VecFloat(KF_TRAFFIC_WHEEL_SPIN_SPEED_SCALE).mV);
    const __vector4 lvBlur = __vminfp(__vmulfp(lvAngularVelocity,
        VecFloat(KF_TRAFFIC_WHEEL_BLUR_SCALE).mV), lvOne);
    const Vector4 lWheelBlur(__vrlimi(lvZero, lvBlur, 8, 0));
    CgsGraphics::mShaderConstantTable.SetShaderConstantData(25, lWheelBlur);

    unsigned int luCompare;
    __vcmpgtfpR(VecFloat(KF_TRAFFIC_WHEEL_BLUR_THRESHOLD).mV, lvAngularVelocity, &luCompare);
    u8 lu8WheelTechnique = (luCompare & 0x80u) ? 1u : 0u;

    Matrix44Affine laWheelTransforms[KI_TRAFFIC_WHEELS_TO_RENDER_MAX];
    const Matrix44Affine* lapWheelTransforms[KI_TRAFFIC_WHEELS_TO_RENDER_MAX] = {0, 0, 0, 0};
    Vector4 laWheelConstants[CgsGraphics::Model::KU_MAX_INSTANCES_PER_GROUP];
    for (u32 luEntry = 0; luEntry < CgsGraphics::Model::KU_MAX_INSTANCES_PER_GROUP; ++luEntry)
        laWheelConstants[luEntry].mV = lvZero;
    s32 liWheelCount = 0;
    u32 luWheelLimit = static_cast<u32>(giWheelsToRender);
    if (lpPhysicsInfo)
    {
        // Both live and replay physics records supply the same existence flags
        // at +0xFC8 and world-space transforms at +0xD30.
        for (u8 luWheel = 0; luWheel < luWheelLimit; ++luWheel)
        {
            if (!lpPhysicsInfo->mabWheelExists[luWheel])
                continue;

            const BrnPhysics::Deformation::WheelSpec* lpWheel = lpPhysicsSpec->GetWheelSpec(luWheel);
            laWheelTransforms[liWheelCount] =
                ScaleWheelTransform(lpWheel->mScale, lpPhysicsInfo->maWheelTransforms[luWheel]);
            lapWheelTransforms[liWheelCount] = &laWheelTransforms[liWheelCount];
            laWheelConstants[liWheelCount].mV = lWheelBlur.mV;
            ++liWheelCount;
            luWheelLimit = static_cast<u32>(giWheelsToRender);
        }
    }
    else
    {
        const __vector4 lvBiasedSpeed = __vmulfp(__vsubfp(lvfVehicleSpeed.mV,
            VecFloat(KF_TRAFFIC_ROAD_NOISE_START).mV), VecFloat(KF_TRAFFIC_ROAD_NOISE_SPEED_SCALE).mV);
        const __vector4 lvMaxRoadNoise = __vmulfp(__vminfp(lvOne, __vmaxfp(lvZero, lvBiasedSpeed)),
            VecFloat(KF_TRAFFIC_MAX_ROAD_NOISE_AMPLITUDE).mV);
        const __vector4 lvRoadUp = rw::math::vpu::GetVector3_YAxis().mV;

        // 0x8272AA4C reads the translation of the model-to-handling transform.
        const Matrix44Affine& lrModelToHandling = lpPhysicsSpec->mCarModelSpaceToHandlingBodySpaceTransform;
        const __vector4 lvSign = __vslw(__vspltisw(-1), __vspltisw(-1));
        Matrix44Affine lFrontWheel = MultiplyWheelTransform(
            rw::math::vpu::MakeRotationX(lfWheelRot + KF_TRAFFIC_FRONT_WHEEL_ROTATION_OFFSET),
            rw::math::vpu::MakeRotationY(-lfSteering));
        Matrix44Affine lRearWheel = rw::math::vpu::MakeRotationX(lfWheelRot);
        lFrontWheel.wAxis.mV = __vxor(lrModelToHandling.wAxis.mV, lvSign);
        lRearWheel.wAxis.mV = lFrontWheel.wAxis.mV;
        lFrontWheel = MultiplyWheelTransform(lFrontWheel, lBodyTransform);
        lRearWheel = MultiplyWheelTransform(lRearWheel, lBodyTransform);

        for (u8 luWheel = 0; luWheel < luWheelLimit; ++luWheel)
        {
            const BrnPhysics::Deformation::WheelSpec* lpWheel = lpPhysicsSpec->GetWheelSpec(luWheel);
            // The effect RNG's vector draw advances once per wheel, even when
            // the road-noise amplitude is zero. The vertical offset is squared.
            const __vector4 lvUnitRandom = mEffectRand.RandomVecFloat().mV;
            const __vector4 lvSignedRandom = __vsubfp(__vaddfp(lvUnitRandom, lvUnitRandom), lvOne);
            const __vector4 lvNoise = __vmulfp(lvSignedRandom, lvMaxRoadNoise);
            const __vector4 lvWheelPosition =
                __vmaddfp(lvRoadUp, __vmulfp(lvNoise, lvNoise), lpWheel->mPosition.mV);

            Matrix44Affine lWheel = (luWheel < 2) ? lFrontWheel : lRearWheel;
            if ((luWheel & 1u) == 0)
            {
                const __vector4 lvNegativeOne = __vcfsx(__vspltisw(-1), 0);
                lWheel.xAxis.mV = __vmulfp(lWheel.xAxis.mV, lvNegativeOne);
                lWheel.zAxis.mV = __vmulfp(lWheel.zAxis.mV, lvNegativeOne);
            }
            lWheel = ScaleWheelTransform(lpWheel->mScale, lWheel);
            lWheel.wAxis.mV = __vaddfp(lWheel.wAxis.mV, MultiplyWheelBasisRow(lvWheelPosition, lBodyTransform));

            laWheelTransforms[luWheel] = lWheel;
            lapWheelTransforms[luWheel] = &laWheelTransforms[luWheel];
            laWheelConstants[luWheel].mV = lWheelBlur.mV;
            luWheelLimit = static_cast<u32>(giWheelsToRender);
        }

        // ARTIST 0x8272B268 always submits four in this arm. A smaller
        // debug count leaves null pointers in the instanced submission.
        liWheelCount = KI_TRAFFIC_WHEELS_TO_RENDER_MAX;
    }

    if (lbShadowPass)
        lu8WheelTechnique = 2;
    if (liWheelCount == 0)
        return;

    const bool lbInstancing = lpWheelModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER);
    // The shader provider uploads five stream vectors for at most four
    // wheels. The main-view stack slot after the four held the half-size.
    if (lbInstancing && !lpShadowMap->IsRenderingShadowMap())
        laWheelConstants[KI_TRAFFIC_WHEELS_TO_RENDER_MAX].mV = lpVehicleTypeRuntime->GetBBoxHalfSize().mV;

    const s32 liDrawCount = lbInstancing ? 1 : liWheelCount;
    for (s32 liWheel = 0; liWheel < liDrawCount; ++liWheel)
    {
        if (!lbInstancing)
            CgsGraphics::mShaderConstantTable.SetShaderConstantData(25, laWheelConstants[liWheel]);

        const CgsGraphics::Renderable* lpWheelRenderable = lpWheelModel->GetRenderable(lWheelLOD);
        CGS_ASSERT(lpWheelRenderable != 0, "lpRenderable");
        CgsGraphics::DispatchList* lpWheelList = lpDispatchFrame->GetList(liModelOnlyDisplayList);
        CGS_ASSERT(lpWheelList != 0, "lpDispatchList");

        if (lbInstancing)
            CgsGraphics::Model::SetupShaderConstantsForInstancing(liWheelCount, lapWheelTransforms, laWheelConstants);
        else
            CgsGraphics::mShaderConstantTable.SetShaderConstantData(0, laWheelTransforms[liWheel]);

        const bool lbFirst = (lpWheelList->GetCount() & 0x7F) == 0;
        lpDispatchFrame->GetBin().BeginPacket();
        CgsGraphics::DrawRenderable::AddToBin(
            lpWheelRenderable, lpDispatchFrame, lbFirst,
            static_cast<s8>(liOpaqueList), static_cast<s8>(liTransparentList),
            lbInstancing ? 0 : 1, lu8WheelTechnique, lbShadowPass,
            0xFFu, 0u, lbInstancing ? liWheelCount : 0, 0u);
        lpWheelList->Submit(0, lpDispatchFrame->GetBin().EndPacket());
    }
}

}  // namespace BrnTraffic
