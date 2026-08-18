// ============================================================================
// b5-decomp/src/GameSource/World/EntityModules/RaceCarEntityModule/
//   BrnRaceCarEntityModule_Render.cpp
//
// The race-car RENDER leg, reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnWorld::RaceCarEntityModule::GenerateDispatchLists @ 0x822E79F8
//   BrnWorld::RaceCarEntityModule::RenderRaceCar         @ 0x822CF6A0
//
// ---- SIGNATURES COME FROM THE ASM, NOT FROM THE OLD DECLARATIONS -----------
// GenerateDispatchLists' committed PC declaration carried five arguments; the console
// call site (WorldModule::GenerateDispatchLists @0x827D27A0..0x827D27C8) loads NINE:
//     r4 = lpInput   r5 = &maRaceCarEntityIds   r6 = 12   r7 = 19   r8 = 20   r9 = 0
//     v1 = fog scattering   v2 = fog colour + white level   v3 = camera position
// 12 is the dispatch-frame OBJECT list; 19/20 are the race-car OPAQUE and TRANSPARENT
// MESH lists (compare the world's own 2 / 11 / 15). Those three integers are forwarded
// verbatim into RenderRaceCar and are the only thing that tells the submission leaf
// where to put the car, so dropping them made the render leg unimplementable.
//
// RenderRaceCar's own prologue (@0x822CF6C4..0x822CF764) homes r3->r20 (this),
// r5->r16 (the RenderParams), r6->r25 (the vehicle graphics ResourcePtr) and spills
// r4/r7/r8/r9/r10 to arg_1C/arg_64/arg_6C/arg_74/arg_7C; the call site @0x822E8534
// adds two STACK arguments -- arg_84 = the frame's ShadowMap and arg_8F = the
// "render attached geometry" bool. v1 is the camera->car DISTANCE (the caller builds it
// with vmsum3fp128 + vrsqrtefp + a Newton step + vmulfp, i.e. sqrt of the squared
// distance, broadcast into all four lanes) -- NOT a direction vector.
//
// ---- WHAT THIS FILE DOES *NOT* RECONSTRUCT (all of it named, none of it faked) ----
// 1. GenerateDispatchLists' OTHER branch. The console function is
//        if (byte_82CDB6B9) { ...scene-manager/replay path... } else { ...this... }
//    where byte_82CDB6B9 is a build-config byte with exactly ONE xref in the whole XEX
//    (this test) and no writer. The arm reconstructed here is the `else`: an 8-slot
//    sweep of maActiveRaceCars. The `if` arm walks the scene-manager visible-entity ids
//    instead and additionally drives the replay serialiser
//    (BrnReplays::RaceCarEntitySerialiser::GetStaticLayout), the blobby-shadow buffer
//    (BrnBlobbyShadowManager::BrnBlobbyShadowBuffer::AddShadow), ShadowMap::CalcOptimisedLod
//    and RaceCarEntityModule::SubmitCoronasForRaceCar.
//    It is a later wave, not a stub: no empty branch is written for it here.
//    UPDATED 2026-08-17 (coronas step 1): SubmitCoronasForRaceCar @0x822D1600 IS now in the
//    tree -- bodied at the foot of this file -- and its CALL is wired into the `else` arm's
//    per-car loop at the console's own position within the per-car body (last, after
//    RenderRaceCar; asm order in the `if` arm: RenderRaceCar @0x822E7E4C ... AddShadow
//    @0x822E8040 ... the five gates @0x822E8044..0x822E808C ... SubmitCoronasForRaceCar
//    @0x822E80BC ... `b` to the loop tail @0x822E83A0). The replay serialiser, the blobby
//    shadow buffer and CalcOptimisedLod are still `if`-arm-only and still absent.
// 2. RenderRaceCar's CRACKED-GLASS loop. It walks the spec's shattered-glass part table
//    and calls BrnWorld::SetGlassFractureConstants per pane before its own AddToBin /
//    Submit, and it needs two .rodata constant vectors the function-only IDA export does
//    not carry.
//    ⚠️ The console ALSO calls SetGlassFractureConstants(0.0f, 1.0f, {0,0}, {0,0,0,0})
//    ONCE per car @0x822CFC08, immediately before shader constants 20/21 -- the "no
//    fracture" reset for the whole car. It is NOT reproduced here for a link reason, not
//    a knowledge one: the only definition of BrnWorld::SetGlassFractureConstants lives in
//    BrnRaceCarEntityModule_GlassFracture.cpp, and that TU is NOT on
//    tools/build/build_game_exe.bat (grepped: no `GlassFracture` line in the file), so
//    calling it would leave an unresolved external at link. The consequence is real and
//    named: shader constants 30/31/32 are never published by anything on this build, and
//    an unset external constant is SKIPPED rather than zeroed (shadowingdevice.cpp:847),
//    so the glass programs that declare them read the previous draw's registers -- the
//    same failure mode the verlet block below documents. DELETE-WHEN the GlassFracture TU
//    is mounted.
//    ⭐ Shader constant 24 -- the head/brake/reverse emission vector -- IS reconstructed
//    as of 2026-08-17 (car-lights wave); its three module debug floats at +100232..+100240
//    are now named + offset-pinned in BrnRaceCarEntityModule.h.
// 3. The IsNormal3x3 / IsOrthogonal3x3 / IsValid dev-assert blocks (~73% of the
//    function's 2008 instructions is assert scaffolding), including the wheel loop's
//    own rw::math::IsValid(lWheelMatrix).
//
// ---- THE WHEEL BLOCK IS NOW HERE, AND IT IS A FAITHFUL DECOMPILE ------------
// It submits ONE DrawRenderable command with instanceCount == the number of wheels,
// exactly as the console does. The PC's D3D9 back end cannot honour that in one draw
// (the fallback shader takes a single WVP per draw and the instanced draw leaf is a
// trap), so the N-instance command is expanded into N single-instance mesh commands in
// CgsGraphics::DrawRenderable::Interpret -- the same PC bring-up shim that already
// carries the per-object WVP there. The deviation belongs in that render back end, NOT
// in this function.
// ============================================================================

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h" // InputBuffer_GenerateDispatchLists

#include "GameShared/GameClasses/Graphics/CgsModel.h"                    // CgsGraphics::Model / Renderable
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"          // ShaderConstantTable
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h"      // DispatchFrame / DispatchBin / DispatchList
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h" // DrawRenderable::AddToBin
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameSource/World/ShadowMap/BrnShadowMap.h"                     // BrnWorld::ShadowMap
#include "SharedClasses/World/BrnVehicleGraphicsSpec.h"                  // BrnVehicle::GraphicsSpec
#include "SharedClasses/World/BrnWheelGraphicsSpec.h"                    // BrnWheel::GraphicsSpec
#include "rw/math/vpu/matrix44affine_operation.h"                        // rw::math::vpu::Mult / InverseOfMatrixWithOrthonormal3x3 / TransformPoint
#include "rw/math/vpu/vector3_operation.h"                               // rw::math::vpu::Negate
#include "GameSource/Physics/DeformationManager/DeformationPhysics/BrnStreamedDeformationSpec.h" // StreamedDeformationSpec (spec+1552) + ETagPointType

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::gpDebugPrint

#include <cmath>   // powf / sqrtf

// The global runtime shader-constant register (X360 symbol mShaderConstantTable; bodied
// by the CgsShaderConstants TU). Mirrors the committed extern in the sibling TUs.
namespace CgsGraphics { extern ::ShaderConstantTable mShaderConstantTable; }

// ============================================================================
// The two file-scope globals the wheel block reads. Both are console globals, and
// both were recovered from the image rather than guessed.
// ============================================================================

// dword_82CDB4A0. NAMED, not inferred: RaceCarEntityModuleDebugComponent::OnActivate
// @0x822C2538 registers it as the debug variable "Graphics/Vehicles.../Wheels to
// Render" with range [0, 4]. The shipped image holds 4. RenderRaceCar re-reads it on
// every loop iteration (a debug slider can move under the loop) and
// TrafficEntityModule::RenderTrafficCar @0x82728B08 shares it, which is why it is a
// plain global here and not a file-static.
s32 giWheelsToRender = 4;

// unk_82FAD6F0. The wheel-spin blur reference: constant 25's lane x is
// angularVelocity / gvWheelBlurConstants.x clamped to 1, and the same value picks the
// blurred-vs-static wheel technique.
// ⚠️ MEASURED: all 16 bytes read ZERO in the shipped image and a whole-export scan finds
// exactly ONE reference -- RenderRaceCar's own read. So on the console the divide is by
// zero, the quotient is +inf for any spinning wheel, and the vminfp clamp pins the
// constant at 1.0f; a stationary wheel gives 0/0. The PC keeps the console arithmetic
// (x87/SSE produce the same inf/NaN without trapping, and the fallback shader does not
// sample constant 25) rather than inventing a divisor. Named, zero-initialised, and
// left for the day a writer turns up -- the export set is known to have holes.
Vector4 gvWheelBlurConstants = { 0.0f, 0.0f, 0.0f, 0.0f };

// The console's wheel matrix / pointer stack arrays are four entries long
// (`__vector_constructor_iterator(v390, 64, 4, ...)`), which is also the debug
// variable's upper bound.
static const s32 KU_WHEELS_TO_RENDER_MAX = 4;

// ============================================================================
// The three constants shader constant 24 (`g_selfIlluminationMask`) is built from.
// All three were READ OUT OF THE SHIPPED IMAGE, not guessed: the wave decrypted and
// decompressed BURNOUT_X360_ARTIST.XEX's basefile (XEX2, encryption type 1 with the
// all-zero DEVKIT key, "basic" compression, load address 0x82000000) and dumped the
// addresses the asm names. Both of the conductor's control values pass on that image
// -- flt_82001C98 reads 0x3F800000 == 1.0f and dword_82F24240's first lane reads
// 0x00000500 == 1280 -- so the mapping is proven before anything below is read.
// The extractor is this wave's work/xex_extract.py.
// ============================================================================

// unk_82FAD990, the vmulfp128 factor (`lvx128 v13, r0, unk_82FAD990 ; vmulfp128 v118,
// v0, v13` @0x822CFB90..0x822CFBC4). It is .data, all-zero in the image, and written
// ONCE by the CRT static initialiser at 0x82C4BC30..0x82C4BC54:
//     lfs f0, flt_82004C88(r11) ; stfs f0, -0x10(r1) ; lvlx v0, r0, r10
//     vspltw v0, v0, 0          ; stvx128 v0, r0, unk_82FAD990
// i.e. splat(flt_82004C88) into all four lanes, and flt_82004C88 reads 0x41000000 ==
// 8.0f (dumped: DATA_DUMP.md, and re-read independently from the extracted image).
// Because it is a four-lane SPLAT the vector multiply is a scalar multiply, so it is
// homed as the scalar the source must have written rather than as a fake Vector4.
// The whole XEX holds exactly TWO references to it -- RenderRaceCar's read at
// 0x822CFB9C and that initialiser at 0x82C4BC48 (IDA xrefs_to, DATA_DUMP.md) -- which
// is why it is file-static here instead of a shared global.
static const f32 KF_SELF_ILLUMINATION_INTENSITY = 8.0f;

// The BASE light-state vector, `lvx128 v0, r0, r22` @0x822CFB34 with r22 = the .rodata
// constant unk_82181510 (set up @0x822CF8F8..0x822CF900 and never reassigned before the
// read). MEASURED (0, 1, 0, 0) -- and the address identifies itself: the extracted image
// holds the four rw::math::vpu::detail unit vectors back to back,
//   0x82181500 (1,0,0,0) = gIVector   0x82181510 (0,1,0,0) = gJVector
//   0x82181520 (0,0,1,0) = gKVector   0x82181530 (0,0,0,1)
// (the same gIVector the surrounding orthogonality asserts load by name @0x822CF904), so
// the compiler folded the source's own {0,1,0,0} literal onto the shared J vector.
// Lane meaning comes from the CONSUMER, not from the name -- see RenderRaceCar's banner.
static const f32 KF_LIGHT_CHANNEL_G_ALWAYS_ON = 1.0f;

// flt_82003F40, the `lfs f13, 0xD9C(r16) ; fcmpu ; bge -> vspltisw v0, 0` cut-off
// @0x822CFB1C..0x822CFB2C. MEASURED 0x3E800000 == 0.25f. Past this much squared
// deformation the car's lights are dead.
static const f32 KF_LIGHTS_OUT_DEFORMATION_SQUARED = 0.25f;

namespace BrnWorld
{

// ============================================================================
// RenderRaceCar  @ 0x822CF6A0
//
// Submit one race car's body-part draws into the frame's race-car lists. Flow (asm
// order, asserts elided -- see the banner):
//   * bail out entirely when the car is hidden        (`if (!*(rp + 5131))`)
//   * publish the per-car shader constants: paint (20), pearlescent (21), the
//     deformation verlet block (22/23, only when damaged), the fog blend (26)
//   * for each body part: visibility gate -> the part's Model -> DoesStateExist(mLOD)
//     -> world matrix = partLocator * bodyTransform -> the detached-part queue may
//     override that matrix -> bind it as shader constant 0 -> BeginPacket ->
//     DrawRenderable::AddToBin -> DispatchList::Submit.
// ============================================================================
void
RaceCarEntityModule::RenderRaceCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                                    ActiveRaceCar::RenderParams* lpRenderParams,
                                    const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>* lpCarGraphics,
                                    const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>* lpWheelGraphics,
                                    s32 liObjectList,
                                    s32 liOpaqueMeshList,
                                    s32 liTransparentMeshList,
                                    const ShadowMap* lpShadowMap,
                                    bool lbRenderAttachedGeometry,
                                    f32  lfCameraDistance,
                                    Vector4 lvFogScattering,
                                    Vector4 lvFogColourPlusWhiteLevel )
{
    CGS_ASSERT( lpRenderParams != 0, "lpRenderParams" );
    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );

    // The X360 loads the four body-transform rows into v124/v126/v125/v117 straight from
    // r5 (the RenderParams) before anything else -- the matrix at offset 0 IS
    // mBodyTransform.
    const Matrix44Affine lBodyTransform = lpRenderParams->GetBodyTransform();

    // `if (!*(rp + 5131))` -- mbIsHidden gates the ENTIRE function body.
    if ( lpRenderParams->IsRaceCarHidden() )
    {
        return;
    }

    // Every spec read in the console body goes through ResourcePtr::operator-> (the
    // `BrnVehicle::GraphicsSpec_::operat(v56)` calls); it is hoisted once here.
    const BrnVehicle::GraphicsSpec* lpCarGraphicsSpec = lpCarGraphics->operator->();

    // ---- the DEBUG damage overrides (@0x822CFAB4..0x822CFB0C) ---------------
    // `if (DEBUG_mbOverrideDamage) { v119 = {crumple, 0, dust, 0};
    //    RenderParams::DEBUG_OverrideScratchAmount(DEBUG_mfVehicleScratchAmount); }`
    // v119 is the vector the console later uploads as shader constant 23
    // (`vmr128 v1, v119 ; SetShaderConstantData(23)` @0x822CFE64..0x822CFE70).
    //
    // ⚠️ ONE DELIBERATE DIFFERENCE, and it removes a console defect rather than adding a
    // PC one: on the console v119 is left UNINITIALISED when the debug bool is false, so
    // constant 23 is uploaded from whatever VMX register 119 held. Seeding it to zero here
    // is exactly what the existing constant-23 upload below already did before this wave,
    // so no shipped value changes; what changes is that the debug arm now has a real home.
    Vector4 lv4DamageConstants = { 0.0f, 0.0f, 0.0f, 0.0f };
    if ( DEBUG_mbOverrideDamage )
    {
        // `stfs module+0x18784 -> var_510 (lane X)` / `stfs module+0x18780 -> var_508
        // (lane Z)`, with f31 (== flt_82001CC0 == 0.0f) in lanes Y and W.
        lv4DamageConstants.x = DEBUG_mfVehicleCrumpleAmount;
        lv4DamageConstants.z = DEBUG_mfVehicleDustAmount;
        lpRenderParams->DEBUG_OverrideScratchAmount( DEBUG_mfVehicleScratchAmount );
    }

    // ========================================================================
    // ⭐ SHADER CONSTANT 24 -- `g_selfIlluminationMask` (@0x822CFB10..0x822CFBC4).
    // THE HEAD / BRAKE / REVERSE LIGHT EMISSION VECTOR. This is the block the file's
    // banner used to list as "not reconstructed".
    //
    // ---- WHAT THE CONSOLE DOES, instruction for instruction --------------------------
    //   0x822CFB10  lbz  r11, 0x1406(r16)        mRenderParams.mbIsEngineOff
    //   0x822CFB18  bne  -> 0x822CFB68                 -> vspltisw v0, 0   (lights out)
    //   0x822CFB20  lfs  f13, 0xD9C(r16)         mRenderParams.mfDeformationSquared
    //   0x822CFB24  lfs  f0,  flt_82003F40       0.25f
    //   0x822CFB2C  bge  -> 0x822CFB68                 -> vspltisw v0, 0   (lights out)
    //   0x822CFB34  lvx128 v0, r0, r22           v0 = unk_82181510 == (0, 1, 0, 0)
    //   0x822CFB30  lbz  r11, 0x1407(r16)        mRenderParams.mbIsBraking
    //   0x822CFB48    vrlimi128 v0, 1.0f, 8, 0        -> lane X = 1
    //   0x822CFB4C  lbz  r11, 0x1408(r16)        mRenderParams.mbIsReversing
    //   0x822CFB60    vrlimi128 v0, 1.0f, 2, 0        -> lane Z = 1
    //   0x822CFB84  lfsx module+0x18788/0x1878C/0x18790 -> var_510/50C/508, var_504 = 0.0f
    //                                            v12 = DEBUG selfIllumination (R, G, B, 0)
    //   0x822CFBB8  vmaxfp    v0, v12, v0        componentwise max
    //   0x822CFBC4  vmulfp128 v118, v0, v13      * splat(8.0f)
    // The vrlimi128 MASK->LANE mapping is this file's own, already recorded on the wheel
    // block below: mask 8 == lane X, mask 1 == lane W, so mask 2 == lane Z.
    //
    // ---- WHAT THE LANES MEAN -- READ OFF THE CONSUMER, NOT GUESSED -------------------
    // Exactly two PC programs declare `g_selfIlluminationMask`, both at PS c5, and both
    // belong to `Vehicle_GreyScale_Light_Textured_EnvMapped` (_Default and _Damaged) --
    // the technique the car's LIGHT-GLASS parts use. Its pixel shader (disassembled from
    // build/game/SHADERS.BNDL this wave, work/ps_vehicle_light_disasm.txt) does:
    //     texld r4, v2, s3          ; r4 = EmissiveTextureSampler.Sample(uv0)   -- a MASK
    //     dp4   r1.x, r4, c5        ; illumination = dot4(emissiveMask, g_selfIlluminationMask)
    //     mul   r1.x, r1.x, c1.w    ; * FogColourPlusWhiteLevel.w  (the HDR white level)
    //     max   r4.xyz, r1.xxxx, r2 ; the emission REPLACES the lit term where it is brighter
    //     mul   r3.xyz, r4, r5      ; ... and is then modulated by the paint-blended albedo
    // So the emissive texture is a FOUR-CHANNEL SELECTOR: each channel tags one light
    // group on the car's light lens, and constant 24 says how bright each group is. With
    // the measured base vector that makes the mapping
    //     lane X (mask R) = BRAKE          0 -> 8 while mbIsBraking
    //     lane Y (mask G) = ALWAYS-ON      8 whenever the lights are alive (head lamps +
    //                                      tail-light glow: the base vector's only 1.0f)
    //     lane Z (mask B) = REVERSE        0 -> 8 while mbIsReversing
    //     lane W (mask A) = never written by this build (base 0, no vrlimi touches it)
    // and the whole vector collapses to zero when the engine is off or the car is
    // deformed past 0.25 -- i.e. a dead or wrecked car has dead lights.
    //
    // ⚠️ WHAT A WRONG LANE ORDER WOULD LOOK LIKE, so a boot can falsify this: X<->Z
    // swapped puts the REVERSE lamps on under braking and the brake lamps on in reverse;
    // Y in the wrong lane means the head/tail lamps never come on while the brake lamps
    // still work; the whole vector one lane out (or the base vector taken as (1,0,0,0))
    // lights the WRONG lens colour, e.g. white reverse lamps glowing when the car is
    // simply driving forward.
    //
    // NOTHING HERE IS INVENTED: every branch is an ARTIST instruction, every scalar was
    // read out of the shipped image (see the three constants at the top of this file),
    // and every RenderParams member is one this header already names and pins.
    // ========================================================================
    Vector4 lv4LightState = { 0.0f, 0.0f, 0.0f, 0.0f };
    if ( !lpRenderParams->IsEngineOff()
         && lpRenderParams->GetDeformationSquared() < KF_LIGHTS_OUT_DEFORMATION_SQUARED )
    {
        // unk_82181510 == (0, 1, 0, 0): the always-on channel, and only that one.
        lv4LightState.y = KF_LIGHT_CHANNEL_G_ALWAYS_ON;

        if ( lpRenderParams->IsBraking() )
        {
            lv4LightState.x = 1.0f;     // vrlimi128 mask 8 -> lane X
        }
        if ( lpRenderParams->IsReversing() )
        {
            lv4LightState.z = 1.0f;     // vrlimi128 mask 2 -> lane Z
        }
    }

    // vmaxfp against the three debug floats (all zero at retail, so a no-op), then the
    // splat multiply, de-optimised back to the scalar it is.
    const Vector4 lv4DebugIllumination = { DEBUG_mfSelfIlluminationR,
                                           DEBUG_mfSelfIlluminationG,
                                           DEBUG_mfSelfIlluminationB,
                                           0.0f };
    const Vector4 lv4SelfIlluminationMask = {
        ( lv4DebugIllumination.x > lv4LightState.x ? lv4DebugIllumination.x : lv4LightState.x )
            * KF_SELF_ILLUMINATION_INTENSITY,
        ( lv4DebugIllumination.y > lv4LightState.y ? lv4DebugIllumination.y : lv4LightState.y )
            * KF_SELF_ILLUMINATION_INTENSITY,
        ( lv4DebugIllumination.z > lv4LightState.z ? lv4DebugIllumination.z : lv4LightState.z )
            * KF_SELF_ILLUMINATION_INTENSITY,
        ( lv4DebugIllumination.w > lv4LightState.w ? lv4DebugIllumination.w : lv4LightState.w )
            * KF_SELF_ILLUMINATION_INTENSITY };

    // ---- the shadow-pass selector ------------------------------------------
    // v320 = (shadowMap[0] && shadowMap[1]): the two LEADING ShadowMap bytes the asm reads
    // as `lbz r11, 0(a40)` / `lbz r11, 1(a40)` -- the same pair
    // WorldEntityModule::GenerateDispatchLists reads to select its shadow pass, i.e.
    // {mbRenderingShadowMap, mbUseZOnlyRenderingPath}. It selects the shadow variant of both
    // the technique index and the AddToBin routing (see below).
    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap()
                           && lpShadowMap->IsUsingZOnlyRenderingPath();

    // ---- per-car shader constants ------------------------------------------
    // 20 / 21: the paint + pearlescent tints, straight out of the render snapshot
    // (`lvx128 v1, r16, 2944` / `..., 2960`).
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 20, lpRenderParams->GetPaintColour() );
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 21, lpRenderParams->GetPearlescentColour() );

    // 24: the self-illumination mask, published here (`vmr128 v1, v118 ; li r4, 0x18 ; bl
    // SetShaderConstantData` @0x822CFC40..0x822CFC4C -- the very next call after 21) and
    // then LATCHED. The body-part loop toggles the register between this vector and zero
    // as it walks attached and detached parts, and the latch is what stops it re-uploading
    // the same value for every part: `li r11, 1 ; stb r11, var_51F(r1)` @0x822CFC88 arms it
    // to "the emission vector is currently in the register".
    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 24, lv4SelfIlluminationMask );
    bool lbSelfIlluminationArmed = true;

    // [PC diagnostic] print the value at the CONSUMING end. UpdateActiveRaceCarColours
    // prints the same two vectors where it writes them; this prints what actually reaches
    // shader constant 20/21, so a silent drop between the two shows up as a mismatch rather
    // than as a plausible-looking white car. (The existing [WorldShader] / [WorldSamplers]
    // tallies cannot answer this -- both saturate at 4096 draws, long before a car exists.)
    {
        // Latched on the VALUE, not on a "printed once" bool -- the paint starts at the
        // palette-0/colour-0 fallback and only becomes the car's authored colour once game
        // action 79 (CarSelectChangeColourAction) has been processed, several frames later.
        static f32 sfLastLoggedPaintX = -1.0f;
        static f32 sfLastLoggedPaintY = -1.0f;
        static f32 sfLastLoggedPaintZ = -1.0f;
        const Vector4& lrPaintProbe = lpRenderParams->GetPaintColour();
        if( ( lrPaintProbe.x != sfLastLoggedPaintX
              || lrPaintProbe.y != sfLastLoggedPaintY
              || lrPaintProbe.z != sfLastLoggedPaintZ )
            && CgsDev::Log::gpDebugPrint != 0 )
        {
            sfLastLoggedPaintX = lrPaintProbe.x;
            sfLastLoggedPaintY = lrPaintProbe.y;
            sfLastLoggedPaintZ = lrPaintProbe.z;
            const Vector4& lrPaint = lpRenderParams->GetPaintColour();
            const Vector4& lrPearl = lpRenderParams->GetPearlescentColour();
            *CgsDev::Log::gpDebugPrint
                << "[racecar-paint] RenderRaceCar -> shader constant 20 g_paintColour ("
                << lrPaint.x << ", " << lrPaint.y << ", " << lrPaint.z << ", " << lrPaint.w
                << ") / 21 g_pearlescentColour (" << lrPearl.x << ", " << lrPearl.y << ", "
                << lrPearl.z << ", " << lrPearl.w << ")\n";
        }
    }

    // 26: the per-car FOG blend. The console inlines a powf here (vlogefp / vexptefp plus
    // the usual polynomial refinement over the .rodata coefficient vectors at
    // 0x82014AC0..0x82014AF0); de-optimised back to the library call it is:
    //     t = powf( saturate( distance * scattering.x - scattering.y ), scattering.z )
    //         * scattering.w
    //     constant26 = { fogColour.xyz * t, 1 - t }
    // (`vrlimi128 v1, v13, 1, 0` writes the 1-t into lane w only.)
    {
        f32 lfBlend = lfCameraDistance * lvFogScattering.x - lvFogScattering.y;
        if ( lfBlend < 0.0f ) { lfBlend = 0.0f; }
        if ( lfBlend > 1.0f ) { lfBlend = 1.0f; }
        const f32 lfFog = powf( lfBlend, lvFogScattering.z ) * lvFogScattering.w;

        const Vector4 lv4Fog = { lvFogColourPlusWhiteLevel.x * lfFog,
                                 lvFogColourPlusWhiteLevel.y * lfFog,
                                 lvFogColourPlusWhiteLevel.z * lfFog,
                                 1.0f - lfFog };
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 26, lv4Fog );
    }

    // 22 / 23: the deformation verlet block. `v107 = mbDamaged; if (kbAllowDeformationDebug)
    // v107 = 1;` then `if (v107 || <module debug bool>)` uploads the 128-entry offset array
    // as constant 22 and its companion scale as 23.
    //
    // ⭐ LANDED 2026-08-12 (exploding-panels wave). The old banner here said this "cannot be
    // expressed yet" because SetShaderConstantArrayData was declaration-only for all five
    // overloads. THAT IS NO LONGER TRUE -- the Vector4* overload is bodied
    // (CgsShaderConstantTable.cpp:80) and carries this array verbatim: maVerletOffsets is
    // Vector3Plus[128] and Vector3Plus is a 16-byte lane register, the same quad-word the
    // overload's FastNonOverlappedVectorMemcpy copies, GetNumEntries() == the declared 128.
    //
    // WHY LEAVING IT UNSENT WAS NOT NEUTRAL -- this is the panel-stretch defect, and the
    // mechanism is MEASURED, not inferred. Sixteen vertex programs in SHADERS_PC.BNDL declare
    // `g_verletOffsets` at register c0 with count 128, and an external constant whose source
    // pointer is null is SKIPPED, not zeroed (shadowingdevice.cpp:847). So the car body
    // program read c0..c127 as they were left by the preceding world draws:
    //     c0..c11   ShadowMap_WorldToLight (three light matrices)
    //     c12..c15  ViewProjectionModified
    //     c16..c19  IrradianceQuadricA
    //     c20..c23  the LAST WORLD OBJECT'S WORLD MATRIX -- translations in the thousands of metres
    //     c24..c31  irradiance/shadow/scattering leftovers
    //     c32..c127 never written by any program in the bundle -> zero
    // mbDamaged is TRUE for every player car (RaceCar::ToBeRenderedDamaged, console
    // behaviour), so the DAMAGED technique ran and offset each vertex by
    // g_verletOffsets[boneIndex]: a vertex whose bone index was 32..127 stayed exactly put
    // while its neighbour indexing 0..31 was catapulted by a matrix row. One or two vertices
    // of a triangle flung metres away, the rest anchored -- a long thin sail, anchored at the
    // car because `world` (c148) IS published and the rigid transform was always correct.
    // The wheels (forced onto the flat fallback pair) and the non-deforming parts were
    // untouched, which is exactly the frame the user saw.
    //
    // The array itself is zero-seeded in RenderParams::Reset until BrnDeformationManager
    // lands; see the flagged block there.
    const bool lbDamaged = lpRenderParams->IsDamaged();

    // ⚠ DELIBERATE DEVIATION from the console's `if (mbDamaged || <debug>)` gate: the upload
    // is UNCONDITIONAL. Measured reason -- of the 16 vertex programs in SHADERS_PC.BNDL that
    // declare g_verletOffsets, fifteen are the `*_Damaged_*` variants but the sixteenth is
    // `ZOnlyVehicleSkinnedOpaqueSingleSided_VertexShader`, i.e. the SHADOW / Z-only skinned
    // path, which binds irrespective of mbDamaged. Under the console's gate an UNDAMAGED car
    // would leave slot 22 unpublished for that program and have its shadow-map geometry torn
    // by the same stale registers this fix exists to stop. An unpublished constant is not
    // zero, it is the previous draw's value, so "only when damaged" is not a safe economy on
    // PC. Cost is one 2 KB copy per car per pass.
    // RESTORE the console gate if the Z-only skinned program is ever shown to ignore the array.
    {
        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData(
            22, reinterpret_cast< const Vector4* >( lpRenderParams->GetVerletOffsets() ) );

        // 23 is the companion scale, and -- MEASURED against SHADERS_PC.BNDL's variable
        // tables -- it is a PIXEL-shader constant (PS c5, declared by 5 programs), so it
        // cannot move a vertex: it is shading only. It is uploaded as zero rather than left
        // unset purely so the register carries a deterministic value instead of whatever the
        // previous draw's PS c5 held.
        // DELETE-WHEN the deformation debug component's scale has a real source.
        // ⭐ 2026-08-17: the vector is now BUILT at the console's own position, from the
        // DEBUG_mbOverrideDamage arm at the top of this function (crumple -> lane X,
        // dust -> lane Z). At retail that arm never runs, so this still uploads the same
        // zeros it did before; what changed is that the console's own producer is present
        // instead of an unconditional literal.
        CgsGraphics::mShaderConstantTable.SetShaderConstantData( 23, lv4DamageConstants );
    }

    // ---- the body-part loop -------------------------------------------------
    // `if (*(this + 99147))` -- the module's own render switch (see the header's
    // two-point offset fit). dword_82CDB49C is a debug "draw only this part index"
    // filter (-1 == every part); it has no writer in the XEX, so the -1 path is the
    // only reachable one and the filter is not modelled.
    if ( mbRenderCarsDuringCrash )
    {
        const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrDetachedParts =
            lpRenderParams->GetDetachedPartQueue();

        const CgsGraphics::Model::State leLOD = lpRenderParams->GetLOD();

        // [DIAG racecar-lod] value-latched on the LOD (re-added per the LOD-system memory note):
        // the wheels drew their LOD-4 box proxy on Car Select because nothing lifted mLOD off
        // Reset's 4 there -- this line says what LOD each RenderRaceCar walk actually sees.
        {
            static s32 siLastLoggedLod = -1;
            if ( static_cast< s32 >( leLOD ) != siLastLoggedLod && CgsDev::Log::gpDebugPrint != 0 )
            {
                siLastLoggedLod = static_cast< s32 >( leLOD );
                *CgsDev::Log::gpDebugPrint << "[racecar-lod] RenderRaceCar sees mLOD "
                                            << static_cast< s32 >( leLOD )
                                            << " parts " << static_cast< s32 >( lpCarGraphicsSpec->muPartsCount )
                                            << "\n";
            }
        }

        for ( u32 luPartIdx = 0; luPartIdx < lpCarGraphicsSpec->muPartsCount; ++luPartIdx )
        {
            if ( !lpRenderParams->IsPartVisible( static_cast< u8 >( luPartIdx ) ) )
            {
                continue;
            }

            const CgsGraphics::Model* lpModel = lpCarGraphicsSpec->GetPartModel( luPartIdx );
            if ( lpModel == 0 )
            {
                continue;
            }

            if ( !lpModel->DoesStateExist( leLOD ) )
            {
                continue;
            }

            // World matrix = partLocator * bodyTransform. The asm broadcasts each locator row
            // component (vspltw) and accumulates it against the three body rotation rows, seeding
            // the translation row with the body translation row -- i.e. exactly rw::math::vpu::Mult
            // with the LOCATOR on the left.
            Matrix44Affine lPartWorldMatrix =
                rw::math::vpu::Mult( lpCarGraphicsSpec->GetPartLocators()[ luPartIdx ], lBodyTransform );

            // A part that has been knocked off the car carries its own world transform in the
            // detached-part queue; the queue entry's mbIsAttached then decides which of the two
            // draw paths runs. Parts with no queue entry stay attached (r28 seeds to 1).
            bool lbIsAttached = true;
            for ( s32 liEvent = 0; liEvent < lrDetachedParts.GetLength(); ++liEvent )
            {
                const DetachedPartRenderEvent& lrEvent = lrDetachedParts.GetEvent( liEvent );
                if ( lrEvent.miPartIndex == static_cast< s32 >( luPartIdx ) )
                {
                    lPartWorldMatrix = lrEvent.mTransform;
                    lbIsAttached     = lrEvent.mbIsAttached;
                    break;
                }
            }

            // An ATTACHED part is drawn only when the caller asked for the attached geometry
            // (the same bool that gates the glass and the wheels). A DETACHED part is always
            // drawn. Both paths converge on the submission below; what differs is the state of
            // shader constant 24.
            //
            // ⭐ THE CONSTANT-24 TOGGLE (@0x822D02B4..0x822D0318) -- LANDED 2026-08-17.
            // The console's shape, branch for branch:
            //     0x822D02B4  clrlwi r11, r28, 24 ; cmplwi ; beq -> 0x822D02F8
            //                   r28 == DetachedPartRenderEvent+0x44 == mbIsAttached,
            //                   seeded 1 for a part with no queue entry
            //     0x822D02F8    (DETACHED)  if (latch) { SetShaderConstantData(24, ZERO);
            //                               latch = 0; }
            //     0x822D02C4  (ATTACHED)  lbz arg_8F ; beq -> 0x822D094C   the `continue`
            //     0x822D02D0              if (!latch) { SetShaderConstantData(24, emission);
            //                                          latch = 1; }
            // A knocked-off panel therefore draws with its lamps DEAD -- which is the
            // console's own rule, and the reason the register has to be toggled at all
            // rather than published once per car.
            if ( !lbIsAttached )
            {
                if ( lbSelfIlluminationArmed )
                {
                    const Vector4 lv4NoSelfIllumination = { 0.0f, 0.0f, 0.0f, 0.0f };
                    CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                        24, lv4NoSelfIllumination );
                    lbSelfIlluminationArmed = false;
                }
            }
            else
            {
                if ( !lbRenderAttachedGeometry )
                {
                    continue;
                }
                if ( !lbSelfIlluminationArmed )
                {
                    CgsGraphics::mShaderConstantTable.SetShaderConstantData(
                        24, lv4SelfIlluminationMask );
                    lbSelfIlluminationArmed = true;
                }
            }

            // Technique index. Two bits: bit 0 == "not damaged", bit 1 == "shadow pass".
            // (`v169 = damaged ? (shadow ? 3 : 0) : (shadow ? 2 : 1)`, the console spelling it
            // with _cntlzw.)
            u8 lu8Technique;
            if ( lbDamaged )
            {
                lu8Technique = lbShadowPass ? 3u : 0u;
            }
            else
            {
                lu8Technique = lbShadowPass ? 2u : 1u;
            }

            const CgsGraphics::Renderable* lpRenderable = lpModel->GetRenderable( leLOD );
            CGS_ASSERT( lpRenderable != 0, "Missing renderable in a model" );

            CgsGraphics::DispatchList* lpDispatchList = lpDispatchFrame->GetList( liObjectList );
            CGS_ASSERT( lpDispatchList != 0, "lpDispatchList" );

            // Bind the part's world matrix for this draw's shader constants.
            CgsGraphics::mShaderConstantTable.SetShaderConstantData( 0, lPartWorldMatrix );

            const bool lbFirstInList = ( lpDispatchList->GetCount() & 0x7F ) == 0;

            lpDispatchFrame->GetBin().BeginPacket();
            if ( lbShadowPass )
            {
                // Shadow variant (@0x822D0724..0x822D0764): both mesh lists are the OPAQUE one
                // and the draw is Z-only. The trailer bytes are preZList = 0xFF, preZTechnique 0,
                // instanceCount 0, excludeMeshBits = mu8RenderDamageFlags.
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liOpaqueMeshList ),
                    1, lu8Technique, true,
                    0xFFu, 0u, 0, lpRenderParams->GetRenderDamageFlag() );
            }
            else
            {
                // Camera variant (@0x822D08E0..0x822D0924).
                CgsGraphics::DrawRenderable::AddToBin(
                    lpRenderable, lpDispatchFrame, lbFirstInList,
                    static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liTransparentMeshList ),
                    1, lu8Technique, false,
                    0xFFu, 0u, 0, lpRenderParams->GetRenderDamageFlag() );
            }

            lpDispatchList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
        }
    }

    // ---- [DIAG car-lights wave 2026-08-17] THE LIGHT-STATE WITNESS -------------
    // ⛔ DELETE-WHEN the head/brake/reverse lamps are confirmed on a booted run.
    // Latched on the STATE, not on a "printed once" bool: a car that never brakes and a car
    // that brakes print different lines, and every transition reprints exactly once. It is
    // also the only thing that can tell the three ways this block produces a black lamp
    // apart -- engine off, deformed past the cut-off, or simply not braking -- and it prints
    // mfDeformationSquared verbatim because that member has NO writer on this build (its
    // console producer, ActiveRaceCar::UpdateDeformationState @0x822D4A58, is the parked leg
    // BrnGame.log already reports), so a non-zero reading here is itself the finding.
    {
        const u32 luLightState =
            ( lpRenderParams->IsEngineOff()  ? 1u : 0u )
          | ( lpRenderParams->IsBraking()    ? 2u : 0u )
          | ( lpRenderParams->IsReversing()  ? 4u : 0u )
          | ( lpRenderParams->GetDeformationSquared() >= KF_LIGHTS_OUT_DEFORMATION_SQUARED
                                             ? 8u : 0u );
        static u32 suSeenLightStates = 0u;
        const u32  luBit = 1u << luLightState;
        if ( ( suSeenLightStates & luBit ) == 0u && CgsDev::Log::gpDebugPrint != 0 )
        {
            suSeenLightStates |= luBit;
            *CgsDev::Log::gpDebugPrint
                << "[racecar-lights] shader constant 24 g_selfIlluminationMask ("
                << lv4SelfIlluminationMask.x << ", " << lv4SelfIlluminationMask.y << ", "
                << lv4SelfIlluminationMask.z << ", " << lv4SelfIlluminationMask.w
                << ")  [x=brake y=alwaysOn z=reverse]  engineOff "
                << ( lpRenderParams->IsEngineOff() ? 1 : 0 )
                << " braking " << ( lpRenderParams->IsBraking() ? 1 : 0 )
                << " reversing " << ( lpRenderParams->IsReversing() ? 1 : 0 )
                << " deformation^2 " << lpRenderParams->GetDeformationSquared()
                << " (lights out at " << KF_LIGHTS_OUT_DEFORMATION_SQUARED << ")\n";
        }
    }

    // ---- [DIAG carrender wave 2026-08-12] THE PER-PART WITNESS -----------------
    // ⛔ DELETE-WHEN the wheels + panels are confirmed on a booted run.
    // Answers, in ONE line per part, the two questions the frame poses: "where does a body
    // part's world matrix come from?" and "is anything flung?". It re-walks the spec
    // read-only -- it cannot perturb the loop above.
    // Latched on the number of parts sitting further than 3 m from the body origin (plus the
    // detached-queue length), NOT on a "printed once" bool: a run where nothing is flung and
    // a run where six panels are flung print different lines, and a change mid-run reprints.
    if ( mbRenderCarsDuringCrash && CgsDev::Log::gpDebugPrint != 0 )
    {
        const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrDiagQueue =
            lpRenderParams->GetDetachedPartQueue();

        s32 liFarParts = 0;
        f32 lfMaxDist  = 0.0f;
        for ( u32 luDiag = 0; luDiag < lpCarGraphicsSpec->muPartsCount; ++luDiag )
        {
            const Matrix44Affine lDiagWorld =
                rw::math::vpu::Mult( lpCarGraphicsSpec->GetPartLocators()[ luDiag ], lBodyTransform );
            const f32 lfDX = lDiagWorld.wAxis.x - lBodyTransform.wAxis.x;
            const f32 lfDY = lDiagWorld.wAxis.y - lBodyTransform.wAxis.y;
            const f32 lfDZ = lDiagWorld.wAxis.z - lBodyTransform.wAxis.z;
            const f32 lfD  = sqrtf( lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ );
            if ( lfD > lfMaxDist ) { lfMaxDist = lfD; }
            if ( lfD > 3.0f )      { ++liFarParts; }
        }

        // The Y-OVER-TIME probe (observable (d)): one compact line per CENTIMETRE of body
        // travel, budgeted so a settling car prints its whole descent and a settled one goes
        // quiet. This is what proves a render change did not disturb the physics.
        {
            static s32 siLastBodyYcm = 0x7FFFFFFF;
            static s32 siYBudget     = 60;
            const s32  liBodyYcm     = static_cast< s32 >( lBodyTransform.wAxis.y * 100.0f );
            if ( liBodyYcm != siLastBodyYcm && siYBudget > 0 )
            {
                siLastBodyYcm = liBodyYcm;
                --siYBudget;
                *CgsDev::Log::gpDebugPrint
                    << "[carrender-y] bodyDraw y " << lBodyTransform.wAxis.y
                    << " wheel0 exists " << ( lpRenderParams->GetWheelExists( 0u ) ? 1 : 0 )
                    << " wheel0 y " << lpRenderParams->GetWheelTransform( 0u ).wAxis.y
                    << " archPartWorldY " << rw::math::vpu::Mult(
                           lpCarGraphicsSpec->GetPartLocators()[ 3 ], lBodyTransform ).wAxis.y
                    << "\n";
            }
        }

        static s32 siLastFarParts   = -1;
        static s32 siLastQueueLen   = -1;
        if ( liFarParts != siLastFarParts || lrDiagQueue.GetLength() != siLastQueueLen )
        {
            siLastFarParts = liFarParts;
            siLastQueueLen = lrDiagQueue.GetLength();

            *CgsDev::Log::gpDebugPrint
                << "[carrender] parts " << lpCarGraphicsSpec->muPartsCount
                << " sizeof(locator) " << static_cast< s32 >( sizeof( Matrix44Affine ) )
                << " detachedQueueLen " << lrDiagQueue.GetLength()
                << " farParts(>3m) " << liFarParts
                << " maxPartDist " << lfMaxDist
                << " body (" << lBodyTransform.wAxis.x << ", " << lBodyTransform.wAxis.y
                << ", " << lBodyTransform.wAxis.z << ")\n";

            for ( u32 luDiag = 0; luDiag < lpCarGraphicsSpec->muPartsCount; ++luDiag )
            {
                const Matrix44Affine& lrLoc = lpCarGraphicsSpec->GetPartLocators()[ luDiag ];
                const Matrix44Affine  lDiagWorld = rw::math::vpu::Mult( lrLoc, lBodyTransform );
                const CgsGraphics::Model* lpDiagModel = lpCarGraphicsSpec->GetPartModel( luDiag );

                // did the queue override this part?
                s32 liQueueHit = -1;
                for ( s32 liEv = 0; liEv < lrDiagQueue.GetLength(); ++liEv )
                {
                    if ( lrDiagQueue.GetEvent( liEv ).miPartIndex == static_cast< s32 >( luDiag ) )
                    {
                        liQueueHit = liEv;
                        break;
                    }
                }

                *CgsDev::Log::gpDebugPrint
                    << "[carrender]  part " << static_cast< s32 >( luDiag )
                    << " vis " << ( lpRenderParams->IsPartVisible( static_cast< u8 >( luDiag ) ) ? 1 : 0 )
                    << " model " << ( lpDiagModel != 0 ? 1 : 0 )
                    << " locT (" << lrLoc.wAxis.x << ", " << lrLoc.wAxis.y << ", " << lrLoc.wAxis.z
                    << ") locScaleRow0 (" << lrLoc.xAxis.x << ", " << lrLoc.xAxis.y << ", "
                    << lrLoc.xAxis.z << ")"
                    << " world (" << lDiagWorld.wAxis.x << ", " << lDiagWorld.wAxis.y << ", "
                    << lDiagWorld.wAxis.z << ") queueHit " << liQueueHit << "\n";
            }

            // The wheel half of the same witness: what the wheel block will read.
            for ( u32 luDiagW = 0; luDiagW < 4u; ++luDiagW )
            {
                const Matrix44Affine& lrWT = lpRenderParams->GetWheelTransform( luDiagW );
                const Matrix44Affine& lrWS = lpRenderParams->GetWheelScaleMatrix( luDiagW );
                *CgsDev::Log::gpDebugPrint
                    << "[carrender]  wheel " << static_cast< s32 >( luDiagW )
                    << " exists " << ( lpRenderParams->GetWheelExists( luDiagW ) ? 1 : 0 )
                    << " T (" << lrWT.wAxis.x << ", " << lrWT.wAxis.y << ", " << lrWT.wAxis.z
                    << ") scaleDiag (" << lrWS.xAxis.x << ", " << lrWS.yAxis.y << ", "
                    << lrWS.zAxis.z << ")\n";
            }
        }
    }
    // ---- end [DIAG carrender wave] --------------------------------------------

    // NOT reconstructed (see the banner), in console order after the body-part loop:
    //   * `if (lbRenderAttachedGeometry)`  -- the cracked-glass loop, which walks the
    //     spec's shattered-glass part table and calls BrnWorld::SetGlassFractureConstants
    //     per pane before its own AddToBin/Submit.

    // ---- the WHEEL block  (@0x822D0F60..0x822D15BC) -------------------------
    // `if (*(this + 99148) && a42)` -- the module's wheel switch and the caller's
    // "render attached geometry" bool. One INSTANCED draw for all four wheels: the
    // per-wheel world matrices go up as shader constant 6 and the per-wheel spin
    // constants as 7, then a single DrawRenderable command carries the instance count.

    // [DIAG wheel wave] The outcome code for the block below, reported once per DISTINCT
    // value at the end. Latched on the VALUE, never on a "printed once" bool: this block
    // has five different ways to draw nothing and a "did it run" flag could only ever
    // report the first car's answer.
    //   0 no wheel resource / block skipped   1 spec has no wheel model
    //   2 no LOD state on the model           3 no wheel reported as existing
    //   4..8 SUBMITTED with 1..5 instances
    s32 liWheelDiagCode = 0;

    if ( mbRenderWheels && lbRenderAttachedGeometry && lpWheelGraphics->HasMemoryResource() )
    {
        // [FLAG PC boot gate] `HasMemoryResource()` is NOT console. The console reaches
        // this block only for a car whose whole resource set is in (its own
        // RaceCarStreamer::GetWheelGraphicsResource asserts IsRaceCarLoaded() before
        // handing the pointer over), so it dereferences unguarded. This build renders
        // cars with PARTIAL sets through the BringUp getters -- see GenerateDispatchLists
        // -- and a car really can go E_STATE_ACTIVE before its wheel graphics reply
        // lands (measured: car 2 is placed on track and drawn several frames before
        // "STRM: Wheel graphics loaded: 2"). Without the gate that car fires
        // ResourcePtr::operator->'s "Can not instance resource pointer" assert on every
        // frame of the render walk. DELETE with the BringUp getters.
        const BrnWheel::GraphicsSpec* lpWheelGraphicsSpec = lpWheelGraphics->operator->();

        // `v230 = *(v229 + 4)` -- the spec's wheel model (its calliper twin at +8 is
        // not drawn by this block).
        const CgsGraphics::Model* lpWheelModel = lpWheelGraphicsSpec->GetWheelModel();

        // The wheels never draw at LOD 0: `if (v231 <= 1) v231 = 1`.
        CgsGraphics::Model::State leWheelLOD = lpRenderParams->GetLOD();
        if ( leWheelLOD <= CgsGraphics::Model::E_STATE_LOD_1 )
        {
            leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
        }

        liWheelDiagCode = 1;

        if ( lpWheelModel != 0 )
        {
            // Fall back to LOD 1 when the requested state is absent, then bail if even
            // that is missing (the console calls DoesStateExist twice, in that order).
            if ( !lpWheelModel->DoesStateExist( leWheelLOD ) )
            {
                leWheelLOD = CgsGraphics::Model::E_STATE_LOD_1;
            }

            liWheelDiagCode = 2;

            if ( lpWheelModel->DoesStateExist( leWheelLOD ) )
            {
                liWheelDiagCode = 3;
                // The three console stack arrays. maWheelMatrices / mapWheelMatrices are
                // four entries because the loop bound tops out at four; the CONSTANTS
                // array is sized to what the callee reads (see the note on
                // KU_MAX_INSTANCES_PER_GROUP below).
                Matrix44Affine        laWheelMatrices[ KU_WHEELS_TO_RENDER_MAX ];
                const Matrix44Affine* lapWheelMatrices[ KU_WHEELS_TO_RENDER_MAX ] = { 0, 0, 0, 0 };

                // ⚠️ The console declares this array FOUR entries long
                // (`__vector_constructor_iterator(v389, 16, 4, ...)`) but
                // SetupShaderConstantsForInstancing reads FIVE
                // (KU_MAX_INSTANCES_PER_GROUP) -- a 16-byte read off the end of the
                // caller's stack slot. Sized to the callee's contract and zeroed, which
                // changes no value the console produces (the loop can never fill a fifth
                // entry) and removes the overread.
                Vector4 laWheelConstants[ CgsGraphics::Model::KU_MAX_INSTANCES_PER_GROUP ] =
                    { { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f },
                      { 0.f, 0.f, 0.f, 0.f }, { 0.f, 0.f, 0.f, 0.f } };

                s32 liInstanceCount = 0;

                // Technique. Seeded 0 (the spinning/blurred variant) OUTSIDE the loop and
                // only ever raised, so it is sticky across wheels: 1 as soon as any wheel
                // is turning slower than the blur threshold, 2 for the shadow pass.
                u8 lu8Technique = 0;

                for ( s32 liWheel = 0; liWheel < giWheelsToRender; ++liWheel )
                {
                    if ( !lpRenderParams->GetWheelExists( static_cast< u32 >( liWheel ) ) )
                    {
                        continue;
                    }

                    // ⚠️ GetWheelTransform is ALREADY a world transform -- unlike the body
                    // parts, the body matrix is NOT re-applied here. The only composition is
                    // the per-wheel scale matrix on the LEFT:
                    //   row i = S[i].x*T[0] + S[i].y*T[1] + S[i].z*T[2] (+ T[3] on row 3)
                    // (the vmulfp128/vmaddfp chain @0x822D10D0..0x822D1134; `vmaddfp` prints
                    // as vD,vA,vB,vC and computes vA*vC+vB, which is what makes this
                    // Mult(scale, transform) and not the other order).
                    const Matrix44Affine lWheelMatrix = rw::math::vpu::Mult(
                        lpRenderParams->GetWheelScaleMatrix( static_cast< u32 >( liWheel ) ),
                        lpRenderParams->GetWheelTransform( static_cast< u32 >( liWheel ) ) );

                    // Shader constant 25, "g_wheelConstants": the wheel's spin blur factor
                    // in lane x and nothing else. `vrlimi128 v0, v13, 8, 0` -- mask 8 is
                    // lane X, and the vector it merges into (v123) is `vspltisw128 0`, so
                    // yzw are ZERO. (The previous wave's decode had the lanes the other way
                    // round; the mask bit settles it, and the fog block eight lines up uses
                    // mask 1 == lane w for contrast.)
                    const f32 lfSpin =
                        lpRenderParams->GetWheelAngularVelocity( static_cast< u32 >( liWheel ) )
                        / gvWheelBlurConstants.x;
                    Vector4 lv4WheelConstants = { lfSpin, 0.0f, 0.0f, 0.0f };
                    if ( lv4WheelConstants.x > 1.0f )   // vminfp128 against vcsxwfp(1) == 1.0f
                    {
                        lv4WheelConstants.x = 1.0f;
                    }
                    CgsGraphics::mShaderConstantTable.SetShaderConstantData( 25, lv4WheelConstants );

                    // `vcmpgtfp. v0, v0, v13` + the CR6 "all lanes" bit: a wheel turning
                    // slower than the threshold selects the non-blurred technique.
                    if ( gvWheelBlurConstants.x >
                         lpRenderParams->GetWheelAngularVelocity( static_cast< u32 >( liWheel ) ) )
                    {
                        lu8Technique = 1u;
                    }
                    if ( lbShadowPass )
                    {
                        lu8Technique = 2u;
                    }

                    // (The four rw::math::IsValid(lWheelMatrix) dev-assert blocks that follow
                    // in the console are elided, exactly as the body-part loop's
                    // IsNormal3x3/IsOrthogonal3x3 blocks are -- see the banner.)

                    // Everything is appended at the COMPACTED index, not at liWheel: the
                    // console advances r28/r26/v237 only when the wheel exists.
                    laWheelMatrices[ liInstanceCount ]   = lWheelMatrix;
                    laWheelConstants[ liInstanceCount ]  = lv4WheelConstants;
                    lapWheelMatrices[ liInstanceCount ]  = &laWheelMatrices[ liInstanceCount ];
                    ++liInstanceCount;
                }

                if ( liInstanceCount > 0 )
                {
                    liWheelDiagCode = 3 + liInstanceCount;

                    // ================================================================
                    // ✅ THE GATE NOW PASSES (2026-08-04, task 133). READ THIS FIRST -- the
                    // history below is kept because its warning still stands, but its
                    // conclusion ("this is a DATA gap ... fix the wheel model") was WRONG.
                    //
                    // The flag was never authored on disc in ANY build (X360 retail, our port
                    // and the BPR Remaster all carry mu8Flags == 0x00). It is COMPUTED AT LOAD
                    // by CgsResource::ModelResourceType::PostFixUp @0x828A7A68 -- an override
                    // b5-decomp simply never declared, so it bound to the empty base and the
                    // pass ran, did nothing and reported nothing. Declaring and implementing it
                    // re-opened this gate by itself, with no change here.
                    //
                    // The "spike through the roof" was NOT the wheel geometry being undrawable.
                    // Device::DrawIndexedMeshPC was submitting the FULL index range of a mesh
                    // whose index buffer holds mu8InstanceCount (=5) slices with the instance
                    // number in the high bits of every index -- so 4/5 of the index values
                    // pointed outside the 240-vertex vertex buffer. It now submits slice 0.
                    // See the banner on Device::DrawIndexedMeshPC.
                    //
                    // ⚠️ WHAT IS STILL MISSING, MEASURED (task 133): the wheels draw, at the
                    // four correct world positions, with correct geometry and an in-range index
                    // run -- and are INVISIBLE, because they are 0.74 m UNDERGROUND.
                    // PlaceCarOnTrack seats the car's ORIGIN on the collision surface
                    // (y = -3.525 in the junkyard, BrnPlaceOnTrackManager's own measured
                    // number) and nothing lifts the body by the wheel radius, because nothing
                    // simulates the suspension. Wheel centre = origin - 0.409 (authored
                    // WheelSpec), wheel radius = 0.5 * 0.663 = 0.3315 -> the tyre bottom is
                    // 0.7405 below the ground plane. Proven by drawing the instanced meshes
                    // with the depth test defeated (BRN_WHEEL_ZALWAYS=1): all four wheels
                    // appear, correctly placed and shaped, through the sand.
                    // DELETE-WHEN the suspension settle lands (the vehicle-physics wall).
                    // ================================================================
                    //
                    // ---- history (task 118) ----------------------------------------
                    // ⛔ THE CONSOLE'S OWN GATE, RE-INSTATED (2026-08-03, task 118).
                    //
                    // The console's check here is
                    //   CGS_ASSERT(lpWheelModel->GetFlag(E_FLAG_MODEL_USES_INSTANCE_SHADER),
                    //              "lpWheelModel->GetFlag(...)")
                    // and on the ported WHE_51916650_GR wheel model it FAILS: the model's
                    // mu8Flags (Model +0x11) reads 0, so bit 0 is clear. It is not a scrambled
                    // header -- mu8NumStates (+0x12) and mu8NumRenderables (+0x10) in the same
                    // word both read correctly (DoesStateExist and GetRenderable succeed on
                    // this very model), which a byte-reversed word could not do.
                    //
                    // ⚠️⚠️ The wheel wave downgraded that assert to a one-shot log on the
                    // reasoning that it "is a NON-GATING assert on the console ... and it has
                    // no effect on this build at all". THAT REASONING WAS WRONG IN EFFECT, and
                    // submitting the draw anyway is what destroyed the rendered world for a
                    // full day. MEASURED, not inferred (task 118, dumped frames, bisected to
                    // this commit):
                    //   * with the draw submitted as authored (instanceCount == 4) the frame is
                    //     screen-filling dark shards from the junkyard chase camera;
                    //   * with the SAME draw submitted at instanceCount == 1 -- i.e. one copy,
                    //     under shader constant 0, the car body's own KNOWN-GOOD world matrix
                    //     that renders the bodyshell correctly in the same frame -- this
                    //     renderable draws as a single distorted SPIKE through the car's roof.
                    //     So the matrices are not the fault: THE WHEEL RENDERABLE'S GEOMETRY
                    //     IS NOT DRAWABLE AS LOADED. Instancing it four times just turns one
                    //     spike into four screen-filling ones.
                    //   * a DrawRenderable::Interpret probe over every expansion candidate in a
                    //     whole run shows this renderable (7 meshes, bounding sphere r 0.837,
                    //     lists 19/20) is the ONLY object ever expanded -- no world object is
                    //     touched, so the instancing shim itself is behaving.
                    //
                    // So the console's flag is doing exactly the job it exists for: it says
                    // "this model is fit for the instanced wheel path", and on this data it is
                    // not. Honour it. This is a DATA gap, not a code one -- the moment the
                    // ported wheel model carries the flag the wheels submit again with no
                    // further change here, and the frame is the check that proves it.
                    // ⛔ Do NOT re-enable this draw by deleting the gate; fix the wheel model.
                    // ================================================================
                    if ( !lpWheelModel->GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER ) )
                    {
                        liWheelDiagCode = 9;   // "gated: model not fit for the instanced path"
                        static bool sbLoggedInstanceShaderFlag = false;
                        if ( !sbLoggedInstanceShaderFlag && CgsDev::Log::gpDebugPrint != 0 )
                        {
                            sbLoggedInstanceShaderFlag = true;
                            *CgsDev::Log::gpDebugPrint
                                << "[racecar-wheels] WHEEL DRAW GATED OFF: model does not carry"
                                   " E_FLAG_MODEL_USES_INSTANCE_SHADER (the console asserts here);"
                                   " renderables " << lpWheelModel->GetNumRenderables()
                                << " states " << lpWheelModel->GetNumLods()
                                << " version " << lpWheelModel->GetVersionNumber()
                                << " -- its geometry draws as spikes, see the banner"
                                   " [FLAG PC data gap]\n";
                        }
                    }
                    else
                    {
                    const CgsGraphics::Renderable* lpWheelRenderable =
                        lpWheelModel->GetRenderable( leWheelLOD );

                    CgsGraphics::DispatchList* lpWheelList = lpDispatchFrame->GetList( liObjectList );
                    CGS_ASSERT( lpWheelList != 0, "lpDispatchList" );

                    // Publish constants 6 (the instance matrices) and 7 (the per-instance
                    // spin vectors) BEFORE the packet opens -- AddToBin drains whatever the
                    // constant table has marked dirty at that moment.
                    CgsGraphics::Model::SetupShaderConstantsForInstancing(
                        liInstanceCount, lapWheelMatrices, laWheelConstants );

                    const bool lbFirstInList = ( lpWheelList->GetCount() & 0x7F ) == 0;

                    lpDispatchFrame->GetBin().BeginPacket();
                    // @0x822D1584: frustumEnable 0 (the wheels ride inside the body's own
                    // bounds), preZList 0xFF, preZTechnique 0, excludeMeshBits 0, and the
                    // instance count that makes this ONE command draw all the wheels.
                    CgsGraphics::DrawRenderable::AddToBin(
                        lpWheelRenderable, lpDispatchFrame, lbFirstInList,
                        static_cast< s8 >( liOpaqueMeshList ), static_cast< s8 >( liTransparentMeshList ),
                        0, lu8Technique, lbShadowPass,
                        0xFFu, 0u, liInstanceCount, 0u );

                    lpWheelList->Submit( 0, lpDispatchFrame->GetBin().EndPacket() );
                    }
                }
            }
        }
    }

    // [DIAG wheel wave] one line per DISTINCT outcome (a bitmask of codes already seen,
    // so a car that draws wheels and a car whose wheel resource has not arrived each get
    // reported exactly once instead of every frame for ever).
    {
        static u32 suSeenWheelDiagCodes = 0;
        const u32  luBit = 1u << static_cast< u32 >( liWheelDiagCode );
        if ( ( suSeenWheelDiagCodes & luBit ) == 0 && CgsDev::Log::gpDebugPrint != 0 )
        {
            suSeenWheelDiagCodes |= luBit;
            *CgsDev::Log::gpDebugPrint
                << "[racecar-wheels] RenderRaceCar wheel block outcome " << liWheelDiagCode
                << " (0 no resource, 1 no model, 2 no LOD state, 3 no wheel exists, "
                   "4..8 submitted with 1..5 instances, 9 GATED -- model lacks "
                   "E_FLAG_MODEL_USES_INSTANCE_SHADER); wheels-to-render "
                << giWheelsToRender << "\n";
        }
    }
}


// ============================================================================
// SubmitCoronasForRaceCar  @ X360 0x822D1600  -- ONE race car's LAMP FLARES.
//
// For every LIGHT LOCATOR the car carries, this posts one corona (an additive camera-facing
// flare sprite) through the corona manager's submission interface: the lamp's WORLD position,
// the direction the lamp FACES (AddCorona back-face-culls on `Dot(pos - camera, dir) < 0`), a
// scale, an opacity, and the BrnCoronaType archetype that carries the sprite's atlas page,
// size, bias distance and distance-fade curve.
//
// ---- THE ARGUMENTS ---------------------------------------------------------
// DWARF BrnRaceCarEntityModule.h:788 (definition BrnRaceCarEntityModule.cpp:3828) and the asm
// prologue agree exactly: (interface, physics-resource-ptr, render params, bool) const.
// The bool is the console's `(u16)mePlayerActiveRaceCarIndex == liActiveRaceCar`
// (@0x822E80A8..0x822E80B8) -- "this is the LOCAL PLAYER's car" -- and its ONLY effect is to
// select the eCoronaTypePlayerCar* archetype bank instead of the eCoronaTypeRaceCar* one.
// The console fills eight BrnCoronaType stack locals up front, in one of two straight-line
// arms (@0x822D16A0..0x822D16DC and @0x822D16E0..0x822D1714), with 8..15 or 16..23; the tree's
// BrnCoronaType enum (BrnCoronaManager.h, DWARF-named) has exactly those two banks in exactly
// that order, so each store below is one enumerator, not arithmetic on an ordinal.
//
// ---- THE FRAME (the one thing worth reading twice) -------------------------
// A light locator's position lives in the car's HANDLING-BODY space, and mBodyTransform is
// `mCentreOfMassTransform * physicsTransform` == `spec.mCarModelSpaceToHandlingBodySpaceTransform
// * physicsTransform` (ActiveRaceCar::CalcBodyTransform @0x822B8828). So the console composes
//     Inverse(spec+1552) * mBodyTransform  ==  the handling body's WORLD transform
// and transforms each locator by it. That is what the fused block @0x822D1754..0x822D1814 is:
// a 3x3 transpose built with vmrglw/vmrghw lane merges, an inverse translation
// (`vsubfp v11, 0, C3` then the transposed FMA cascade), and the four-row affine product --
// i.e. exactly rw::math::vpu::InverseOfMatrixWithOrthonormal3x3 followed by Mult, both of
// which this tree already homes with those same asm shapes described on them.
// MEASURED against the shipped data (all 130 VEH_*_AT.BIN files whose deformation spec parses):
// spec+1552's upper 3x3 is the IDENTITY in every single one -- it is a pure translation -- which
// is why the console can hand AddCorona the raw mBodyTransform.zAxis as the lamp direction
// without rotating it back through the same inverse.
//
// ---- THE PER-LOCATOR SWITCH ------------------------------------------------
// `switch (type - 1)` over 32 cases (asm @0x822D1874..0x822D1898), i.e. tag types 1..32, with
// types 20..30 falling to the default (they are the tyrewell / axle / articulation / attach
// tags -- not lamps). Each live case picks (a) the archetype, (b) whether the lamp faces
// FORWARD (+mBodyTransform.zAxis) or REARWARD (the console negates the row with
// `vspltisw v13,-1 ; vslw v13,v13,v13 ; vxor128 v2, v127, v13`, i.e. a sign-bit flip), and
// (c) an optional state gate. The scale argument is 1.0f on every path in this function; only
// the opacity varies (0.8 for the spot lamps, a triangle wave for the police strobes).
//
// TAG TYPES 31 AND 32 are handled -- 31 alongside the brake lamps, 32 as the standalone
// eCoronaTypePlayerCarPS3BluRayLights (the only user of archetype 24, staged into its own
// stack slot at @0x822D1B54..0x822D1B58). The DWARF tag taxonomy
// (BrnTagPointTypes.h:35, and the tree's own copy in BrnStreamedDeformationSpec.h) names those
// two E_TAGPOINT_FXGLASSSMASHPOINT1/2, which reads oddly next to brake lamps -- so it was
// CHECKED against the shipped data rather than argued about: across the same 130 parsed
// vehicle specs the light-locator tag histogram is types 1..19 ONLY, never 31 or 32, and the
// generic/camera lists use 26/27/41/42/46/47 exactly as the same enum names them. The enum is
// therefore right and these two arms are simply dead in the retail fleet; they are reproduced
// because the console has them, and flagged because a car that did author them would take
// them.
//
// ---- WHAT THIS FUNCTION READS FROM THE MODULE ------------------------------
// `mfTimeStep` (+0x18398) and `mPlayerVehicleControls.mbHorn` (+0x183DC) -- the two arguments
// of RenderParams::RequestBluesAndTwosStateSwitch @0x822A1C90, whose PPC signature is the
// classic float-skips-its-GPR-slot shape: `this` in r3, the timestep in f1 (r4 is never set),
// the force flag in r5. So the police strobe is switched by the LOCAL PLAYER'S HORN, on every
// car, which is the console's own wiring and not an inference.
// ============================================================================

// flt_82014930 -- the spot-lamp opacity, the one non-unit opacity in the function
// (@0x822D1AA0 `lfs f2, flt_82014930` on the tag-5/6/16/17 path). Hex-Rays renders the literal
// as 0.80000001, i.e. the f32 0.8f.
static const f32 KF_CORONA_SPOTLIGHT_OPACITY = 0.8f;

// The blues-and-twos strobe shaping constants, all read straight out of the asm:
//   flt_82CDB628 = 0.5   (DATA_DUMP.md, .data image word 0x3F000000) -- the HALF-CYCLE split:
//                        tag 18 (BLUESTWOS1) owns phase <= 0.5, tag 19 (BLUESTWOS2) phase >= 0.5.
//   flt_820147FC = 0.5   (@0x822D1998 `fmuls f0, f0, f26`) -- tag 18's quarter point is spelled
//                        `flt_82CDB628 * 0.5`, while tag 19's is the literal 0.25 below. Two
//                        spellings of the same number in the console; both are kept.
//   flt_82003F40 = 0.25  -- already homed in this TU as KF_LIGHTS_OUT_DEFORMATION_SQUARED.
//   flt_82004EF4 = 4.0 and flt_82014984 = 2.0 -- the triangle wave `t*4` / `-(t*4 - 2)`.
static const f32 KF_BLUES_AND_TWOS_HALF_CYCLE   = 0.5f;
static const f32 KF_BLUES_AND_TWOS_HALF_OF_HALF = 0.5f;
static const f32 KF_BLUES_AND_TWOS_RAMP         = 4.0f;
static const f32 KF_BLUES_AND_TWOS_PEAK         = 2.0f;

// flt_82CDB62C -- the scale applied to mfLightOpacityFlipFlop before it is used as the strobe
// PHASE (`fmuls f29, f13, f0` @0x822D1838).
// RECOVERED from the .data image at 0x82CDB62C (word 3F800000 == 1.0f; its neighbour
// flt_82CDB628 reads 3F000000 == the 0.5 DATA_DUMP.md attests, so the page is not
// runtime-initialised -- verify_coronaproducer F4). Cross-checks: RequestBluesAndTwosStateSwitch
// wraps mfLightOpacityFlipFlop at 1.0 (`lfs f13, flt_82001C98 ; fcmpu ; fsubs` @0x822A1CD4),
// tag 18 covers phase <= 0.5 with a triangle that peaks at phase 0.25, and tag 19 covers phase
// >= 0.5 with the same triangle on (phase - 0.5) -- the two arms tile [0,1] exactly once at this
// scale. (A wrong value would not crash: it desynchronises the two strobe halves.)
static const f32 KF_BLUES_AND_TWOS_PHASE_SCALE = 1.0f;   // X360 flt_82CDB62C @0x82CDB62C, image word 3F800000

void
RaceCarEntityModule::SubmitCoronasForRaceCar(
    BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface,
    const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>& lrPhysicsResource,
    ActiveRaceCar::RenderParams* lpRenderParams,
    bool lbIsPlayerCar ) const
{
    CGS_ASSERT( lpCoronaSubmissionInterface != 0, "lpCoronaSubmissionInterface != NULL" );  // :3905
    CGS_ASSERT( lpRenderParams != 0, "lpRenderParams != NULL" );                            // :3906

    // [FLAG] NOT in the console -- its asserts are non-gating tripwires and it walks on. On PC
    // a null interface or null params would be a null-deref two lines down, and the call site
    // already refuses to call with a null interface, so this is the belt to that brace.
    if ( lpCoronaSubmissionInterface == 0 || lpRenderParams == 0 )
    {
        return;
    }

    // The four light-state bytes, in the console's own load order
    // (@0x822D1684/0x822D1688/0x822D1690/0x822D1698).
    const bool lbIsBraking         = lpRenderParams->IsBraking();          // +5127
    const bool lbIsReversing       = lpRenderParams->IsReversing();        // +5128
    const bool lbIsIndicatingRight = lpRenderParams->IsIndicatingRight();  // +5130
    const bool lbIsIndicatingLeft  = lpRenderParams->IsIndicatingLeft();   // +5129

    // THE ARCHETYPE BANK. Eight locals, filled before the loop, exactly as the console's two
    // straight-line arms do -- the player's own car gets its own eight archetypes.
    const BrnCoronaType leHeadLight  = lbIsPlayerCar ? eCoronaTypePlayerCarHeadLight
                                                     : eCoronaTypeRaceCarHeadLight;        // var_130
    const BrnCoronaType leRearLight  = lbIsPlayerCar ? eCoronaTypePlayerCarRearLight
                                                     : eCoronaTypeRaceCarRearLight;        // var_12C
    const BrnCoronaType leBrakeLight = lbIsPlayerCar ? eCoronaTypePlayerCarBrakeLight
                                                     : eCoronaTypeRaceCarBrakeLight;       // var_134
    const BrnCoronaType leIndicator  = lbIsPlayerCar ? eCoronaTypePlayerCarIndicator
                                                     : eCoronaTypeRaceCarIndicator;        // var_140
    const BrnCoronaType leReversing  = lbIsPlayerCar ? eCoronaTypePlayerCarReversingLight
                                                     : eCoronaTypeRaceCarReversingLight;   // var_128
    const BrnCoronaType leBluesRed   = lbIsPlayerCar ? eCoronaTypePlayerCarBluesTwosRed
                                                     : eCoronaTypeRaceCarBluesTwosRed;     // var_13C
    const BrnCoronaType leBluesBlue  = lbIsPlayerCar ? eCoronaTypePlayerCarBluesTwosBlue
                                                     : eCoronaTypeRaceCarBluesTwosBlue;    // var_138
    const BrnCoronaType leSpotlight  = lbIsPlayerCar ? eCoronaTypePlayerCarSpotlights
                                                     : eCoronaTypeRaceCarSpotlights;       // var_124
    // var_120 -- staged inside the tag-32 arm itself (@0x822D1B58 `stw r23` with r23 == 0x18),
    // and it is the SAME archetype whether or not this is the player's car.
    const BrnCoronaType leBluRayLight = eCoronaTypePlayerCarPS3BluRayLights;

    // The locator -> world matrix (see the banner). The console's resource-pointer deref
    // (@0x822D1720) carries its own "Can not instance resource pointer" assert; the tree's
    // ResourcePtr::operator-> is that accessor.
    const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec = lrPhysicsResource.operator->();
    if ( lpSpec == 0 )
    {
        return;   // [FLAG] the console asserts instead; a PC car can legitimately be mid-stream
    }

    const Matrix44Affine& lrBodyTransform = lpRenderParams->GetBodyTransform();
    const Matrix44Affine  lLocatorToWorld =
        rw::math::vpu::Mult(
            rw::math::vpu::InverseOfMatrixWithOrthonormal3x3( lpSpec->mCarModelSpaceToHandlingBodySpaceTransform ),
            lrBodyTransform );

    // The two lamp directions. v127 is mBodyTransform.zAxis loaded once @0x822D175C and reused
    // for every locator; the rearward one is the same row with its sign bits flipped.
    const Vector3 lForward  = lrBodyTransform.zAxis;
    const Vector3 lRearward = rw::math::vpu::Negate( lrBodyTransform.zAxis );

    // The police strobe. RequestBluesAndTwosStateSwitch advances the timers and reports whether
    // the strobe is lit; the phase is read back AFTER it (@0x822D1824), i.e. post-advance.
    const bool lbBluesAndTwosOn =
        lpRenderParams->RequestBluesAndTwosStateSwitch( mfTimeStep, mPlayerVehicleControls.mbHorn );
    const f32 lfStrobePhase =
        lpRenderParams->GetLightOpacityFlipFlop() * KF_BLUES_AND_TWOS_PHASE_SCALE;

    u32 luSubmitted = 0;   // [DIAG] one-shot boot proof only -- see the tail

    const u32 luNumLocators = lpRenderParams->GetNumLightLocators();
    for ( u32 luLocator = 0; luLocator < luNumLocators; ++luLocator )
    {
        const BrnPhysics::Deformation::ETagPointType leTagType =
            lpRenderParams->GetLightLocatorType( luLocator );

        BrnCoronaType leCoronaType = leHeadLight;
        bool          lbSubmit     = false;
        bool          lbForward    = true;
        f32           lfOpacity    = 1.0f;

        switch ( leTagType )
        {
        // ---- always-on running lamps ---------------------------------------
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_FRONTRUNNINGLEFT:    // 1
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_FRONTRUNNINGRIGHT:   // 2
            leCoronaType = leHeadLight;  lbForward = true;  lbSubmit = true;
            break;

        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_REARRUNNINGLEFT:     // 3
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_REARRUNNINGRIGHT:    // 4
            leCoronaType = leRearLight;  lbForward = false; lbSubmit = true;
            break;

        // ---- spot lamps: the only non-unit opacity in the function --------
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_FRONTSPOTLEFT:       // 5
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_FRONTSPOTRIGHT:      // 6
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_SPOTLIGHT1:          // 16
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_SPOTLIGHT2:          // 17
            leCoronaType = leSpotlight;  lbForward = true;  lbSubmit = true;
            lfOpacity    = KF_CORONA_SPOTLIGHT_OPACITY;
            break;

        // ---- indicators: front pair faces forward, rear pair rearward -----
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_INDICATORFRONTLEFT:  // 7
            if ( lbIsIndicatingLeft )  { leCoronaType = leIndicator; lbForward = true;  lbSubmit = true; }
            break;
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_INDICATORFRONTRIGHT: // 8
            if ( lbIsIndicatingRight ) { leCoronaType = leIndicator; lbForward = true;  lbSubmit = true; }
            break;
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_INDICATORREARLEFT:   // 9
            if ( lbIsIndicatingLeft )  { leCoronaType = leIndicator; lbForward = false; lbSubmit = true; }
            break;
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_INDICATORREARRIGHT:  // 10
            if ( lbIsIndicatingRight ) { leCoronaType = leIndicator; lbForward = false; lbSubmit = true; }
            break;

        // ---- brake lamps ---------------------------------------------------
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_BRAKELEFT:           // 11
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_BRAKERIGHT:          // 12
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_BRAKECENTRE:         // 13
        case BrnPhysics::Deformation::E_TAGPOINT_FXGLASSSMASHPOINT1:         // 31 -- see the banner
            if ( lbIsBraking ) { leCoronaType = leBrakeLight; lbForward = false; lbSubmit = true; }
            break;

        // ---- reversing lamps -----------------------------------------------
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_REVERSELEFT:         // 14
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_REVERSERIGHT:        // 15
            if ( lbIsReversing ) { leCoronaType = leReversing; lbForward = false; lbSubmit = true; }
            break;

        // ---- the police strobe: TWO coronas per locator, one each way ------
        // The console emits the rearward one inline (@0x822D19E8 / @0x822D1A78) and then falls
        // into the shared tail with the direction swapped back to +z, so a light bar is visible
        // from in front of the car as well as behind it. Both calls take the same position and
        // the same triangle-wave opacity.
        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_BLUESTWOS1:          // 18
            if ( lbBluesAndTwosOn && lfStrobePhase <= KF_BLUES_AND_TWOS_HALF_CYCLE )
            {
                const f32 lfQuarter = KF_BLUES_AND_TWOS_HALF_CYCLE * KF_BLUES_AND_TWOS_HALF_OF_HALF;
                lfOpacity = ( lfStrobePhase > lfQuarter )
                          ? -( lfStrobePhase * KF_BLUES_AND_TWOS_RAMP - KF_BLUES_AND_TWOS_PEAK )
                          :  ( lfStrobePhase * KF_BLUES_AND_TWOS_RAMP );

                lpCoronaSubmissionInterface->AddCorona( rw::math::vpu::TransformPoint(
                                                            lLocatorToWorld,
                                                            lpRenderParams->GetLightLocatorPos( luLocator ) ),
                                                        lRearward, 1.0f, lfOpacity, leBluesRed );
                ++luSubmitted;

                leCoronaType = leBluesRed;  lbForward = true;  lbSubmit = true;
            }
            break;

        case BrnPhysics::Deformation::E_TAGPOINT_LIGHTS_BLUESTWOS2:          // 19
            if ( lbBluesAndTwosOn && lfStrobePhase >= KF_BLUES_AND_TWOS_HALF_CYCLE )
            {
                // The console spells this quarter point as the LITERAL 0.25 (flt_82003F40, the
                // same rodata float as the lights-out threshold) while tag 18 spells its own as
                // `flt_82CDB628 * 0.5`. Both spellings are kept as written.
                const f32 lfPhaseInHalf = lfStrobePhase - KF_BLUES_AND_TWOS_HALF_CYCLE;
                lfOpacity = ( lfPhaseInHalf > KF_LIGHTS_OUT_DEFORMATION_SQUARED )
                          ? -( lfPhaseInHalf * KF_BLUES_AND_TWOS_RAMP - KF_BLUES_AND_TWOS_PEAK )
                          :  ( lfPhaseInHalf * KF_BLUES_AND_TWOS_RAMP );

                lpCoronaSubmissionInterface->AddCorona( rw::math::vpu::TransformPoint(
                                                            lLocatorToWorld,
                                                            lpRenderParams->GetLightLocatorPos( luLocator ) ),
                                                        lRearward, 1.0f, lfOpacity, leBluesBlue );
                ++luSubmitted;

                leCoronaType = leBluesBlue; lbForward = true;  lbSubmit = true;
            }
            break;

        // ---- the standalone archetype-24 lamp ------------------------------
        case BrnPhysics::Deformation::E_TAGPOINT_FXGLASSSMASHPOINT2:         // 32 -- see the banner
            leCoronaType = leBluRayLight; lbForward = false; lbSubmit = true;
            break;

        default:
            // Tag types 20..30 (tyrewells / axle / articulation / attach) and everything past
            // 32: the console's jump-table default, which just advances to the next locator.
            break;
        }

        if ( lbSubmit )
        {
            lpCoronaSubmissionInterface->AddCorona(
                rw::math::vpu::TransformPoint( lLocatorToWorld,
                                               lpRenderParams->GetLightLocatorPos( luLocator ) ),
                lbForward ? lForward : lRearward,
                1.0f,               // the scale argument is 1.0f on every path of this function
                lfOpacity,
                leCoronaType );
            ++luSubmitted;
        }
    }

    // [DIAG coronas step 1] one shot per boot -- the wave's own acceptance line. A car with a
    // dark, working subsystem and a car with no light locators at all look identical on screen,
    // so the count and the locator inventory are printed once rather than inferred.
    static bool sbLoggedFirstCoronaSubmit = false;
    if ( !sbLoggedFirstCoronaSubmit && CgsDev::Log::gpDebugPrint != 0 )
    {
        sbLoggedFirstCoronaSubmit = true;
        *CgsDev::Log::gpDebugPrint
            << "[corona] first submit: " << luSubmitted << " coronas from "
            << luNumLocators << " light locators (types";
        for ( u32 luLocator = 0; luLocator < luNumLocators; ++luLocator )
        {
            *CgsDev::Log::gpDebugPrint
                << " " << static_cast<s32>( lpRenderParams->GetLightLocatorType( luLocator ) );
        }
        *CgsDev::Log::gpDebugPrint
            << ") playerCar " << ( lbIsPlayerCar ? 1 : 0 )
            << " braking " << ( lbIsBraking ? 1 : 0 )
            << " reversing " << ( lbIsReversing ? 1 : 0 )
            << " bluesAndTwos " << ( lbBluesAndTwosOn ? 1 : 0 )
            << " phase " << lfStrobePhase << "\n";
    }
}


// ============================================================================
// GenerateDispatchLists  @ 0x822E79F8  (the `else` arm -- see the banner)
//
//   for (i = 0; i < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++i)
//   {
//       ActiveRaceCar& car = maActiveRaceCars[i];
//       if (!car.IsActive())                                       continue;
//       if (!car.mbRenderThisFrame || car.mbIsInCarSelectOnline)   continue;
//       distance = |cameraPosition - car.GetTransform().wAxis|;
//       gfx   = ResourcePtr(mRaceCarStreamer.GetGraphicsResource(i));
//       wheel = ResourcePtr(mRaceCarStreamer.GetWheelGraphicsResource(i));
//       RenderRaceCar(frame, &car.mRenderParams, &gfx, &wheel,
//                     objectList, opaqueMeshList, transparentMeshList,
//                     shadowMap, true, distance, fogScattering, fogColour);
//   }
//
// The console builds the two ResourcePtr locals with BaseResourcePtr::CreateFromHandle
// and unlinks them from the resource's intrusive user list right after the call (that is
// what the four pointer-fixup blocks after each RenderRaceCar call are); reproduced here
// as plain ResourcePtr copies, which carry the same acquire/release semantics without
// poking the list by hand.
// ============================================================================
void
RaceCarEntityModule::GenerateDispatchLists(
    RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpInput,
    const Array< CgsSceneManager::EntityId, 32u >& lrVisibleEntities,
    s32  liObjectList,
    s32  liOpaqueMeshList,
    s32  liTransparentMeshList,
    bool lbEnvironmentMapPass,
    Vector4 lvFogScattering,
    Vector4 lvFogColourPlusWhiteLevel,
    Vector3 lvCameraPosition )
{
    CGS_ASSERT( lpInput != 0, "lpInput != NULL" );
    (void)lrVisibleEntities;    // consumed only by the scene-query arm (see the banner)
    (void)lbEnvironmentMapPass;

    lpInput->LockForRead();

    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInput->GetDispatchFrame();
    const ShadowMap*            lpShadowMap     = lpInput->GetShadowMap();
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );

    if ( lpDispatchFrame != 0 && lpShadowMap != 0 )
    {
        for ( s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar )
        {
            ActiveRaceCar& lrActiveRaceCar = maActiveRaceCars[ liCar ];

            if ( !lrActiveRaceCar.IsActive() )
            {
                continue;
            }
            if ( !lrActiveRaceCar.ShouldRenderThisFrame() )
            {
                continue;
            }

            // |camera - car|: the console's vmsum3fp128 / vrsqrtefp / Newton-step / vmulfp
            // chain over (cameraPosition - transform.wAxis), with the vcmpeqfp/vsel128
            // selecting zero when the squared length is exactly zero.
            const Matrix44Affine lCarTransform = lrActiveRaceCar.GetTransform();
            const f32 lfDeltaX   = lvCameraPosition.x - lCarTransform.wAxis.x;
            const f32 lfDeltaY   = lvCameraPosition.y - lCarTransform.wAxis.y;
            const f32 lfDeltaZ   = lvCameraPosition.z - lCarTransform.wAxis.z;
            const f32 lfLengthSq = lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
            const f32 lfDistance = ( lfLengthSq != 0.0f ) ? sqrtf( lfLengthSq ) : 0.0f;

            // The console COPIES both ResourcePtrs into stack locals here
            // (BaseResourcePtr::CreateFromHandle) and unlinks them from the resource's
            // intrusive alias list right after the call. That round trip only registers a
            // temporary alias; ResourcePtr's copy constructor has no reconstructed body in
            // this tree, so the read-only render use takes the streamer's own references.
            //
            // [FLAG PC bring-up] ...but through the BringUp getters, not the console's.
            // GetGraphicsResource / GetWheelGraphicsResource both assert
            // IsRaceCarLoaded(), i.e. ALL FIVE resource bits, and on this build two of
            // them (physics + attributes) come from the unported VEH_<id>_AT.bin and a
            // third (wheel graphics) from a still-deferred GameData handler -- so the
            // console's own accessors fire a dev assert every single frame for a car that
            // is otherwise perfectly renderable. The BringUp pair returns the same member
            // without the all-resources precondition. RESTORE the console accessors the
            // moment AddVehicleData's attribute + wheel legs resolve.
            const RaceCarStreamer::GraphicsResourcePtr&      lrCarGraphics =
                mRaceCarStreamer.GetGraphicsResourceBringUp( liCar );
            const RaceCarStreamer::WheelGraphicsResourcePtr& lrWheelGraphics =
                mRaceCarStreamer.GetWheelGraphicsResourceBringUp( liCar );

            // RenderRaceCar Submits into the OBJECT list (12); 19/20 are mesh-bin routing
            // ids that the dispatch interpreter fans the packet out to, not lists this
            // producer appends to directly.
            const s32 liBefore = lpDispatchFrame->GetList( liObjectList )->GetCount();

            RenderRaceCar( lpDispatchFrame,
                           lrActiveRaceCar.GetRenderParams(),
                           &lrCarGraphics,
                           &lrWheelGraphics,
                           liObjectList,
                           liOpaqueMeshList,
                           liTransparentMeshList,
                           lpShadowMap,
                           true,
                           lfDistance,
                           lvFogScattering,
                           lvFogColourPlusWhiteLevel );

            // ==============================================================
            // THE SHADOW-PASS GATE -- console @0x822E7E50-0x822E7E58, RECOVERED
            // (coronas step 2, group `coronacalib`). THIS IS THE DOUBLE-POST FIX.
            //
            //   0x822E7E4C  bl     RenderRaceCar
            //   0x822E7E50  lbz    r11, 0(r19)        ; r19 = lpInput->GetShadowMap()
            //   0x822E7E54  cmplwi cr6, r11, 0        ;   ShadowMap +0x00 == mbRenderingShadowMap
            //   0x822E7E58  bne    cr6, loc_822E83A0  ; -> the LOOP TAIL: for this car, skip
            //                                         ;    EVERYTHING after RenderRaceCar
            //   0x822E8040  bl     BrnBlobbyShadowBuffer::AddShadow      (below the branch)
            //   0x822E804C..0x822E808C  the five corona gates             (below the branch)
            //   0x822E80BC  bl     SubmitCoronasForRaceCar               (below the branch)
            //
            // ShadowMap +0x00 IS mbRenderingShadowMap: ShadowMap::Construct writes it with
            // `stb r28, 0(this)` (BrnShadowMap.cpp:192), it is the first of the two leading
            // ShadowMap bytes this very file already reads at :336 for the shadow technique,
            // and the console reads the same slot two instructions earlier (@0x822E7D9C
            // `lbz r11, 0(r19)`) to choose ShadowMap::CalcOptimisedLod.
            //
            // WHY IT MATTERS ON PC. WorldModule::GenerateDispatchListsBringUp drives THIS
            // function from TWO call sites -- the main-view leg (BrnWorldModule.cpp:6622) and
            // the per-cascade leg (:7129, inside the arm that raises the latch at :7071
            // `mShadowMap.SetRenderingShadowMap(true)`) -- all with the SAME
            // `sRaceCarDispatchInput`, whose corona submission interface is set ONCE in the
            // main leg (:6619) and never cleared. Without this branch a cascade leg posts the
            // car's lamp flares again into the same corona buffer, and the pass is ADDITIVE:
            // step 1's boot log proves at least one extra post reached the buffer --
            // `[corona] first submit: 4 coronas` against `[corona] first draw: 8 quads` for
            // ONE race car (BrnGame.log:4508/4509, `[racecar-lod] banded 1 cars`). Which
            // cascade legs post on a given frame is what the `[corona-calib] dispatch leg`
            // probe below measures; this branch removes EVERY cascade post regardless.
            //
            // (The console's SECOND gate at 0x822E7E5C -- `clrlwi r11, r21, 24 / beq
            // loc_822E83A0`, where r21 is 1 unless the input buffer's camera flag word bit 2
            // is set AND this car is the player's -- is NOT reproduced: its source is an
            // unidentified 64-bit field at camera+0x140 (`ld r11, 0x140(r3)` @0x822E7CEC after
            // an InputBuffer getter) and inventing it would be fabrication. It can only ever
            // REMOVE the local player's own flares in a camera mode that hides the player car,
            // which the boot frames are not in -- the player car renders in full. Recorded in
            // the report as a named follow-up.)
            //
            // The blobby-shadow AddShadow leg is not reconstructed on PC yet; when it lands it
            // belongs BELOW this branch, exactly where the console has it.
            // ==============================================================
            const bool lbRenderingShadowMap = lpShadowMap->IsRenderingShadowMap();

            // [DIAG corona-calib -- coronas step 2] THE MEASUREMENT the calibration needs:
            // which dispatch legs reach the per-car body in one frame, and which of them the
            // gate above now rejects. A burst of the first 16 legs of the boot, then silent
            // for ever. Read it together with `[corona] first draw: N quads`: N must equal the
            // sum of the `main` legs' submitted counts and nothing else.
            // DELETE-WHEN the corona calibration is signed off.
            {
                static u32 suCoronaCalibLegs = 0u;
                if ( suCoronaCalibLegs < 16u && CgsDev::Log::gpDebugPrint != 0 )
                {
                    ++suCoronaCalibLegs;
                    *CgsDev::Log::gpDebugPrint
                        << "[corona-calib] dispatch leg #" << static_cast< s32 >( suCoronaCalibLegs )
                        << " car " << liCar
                        << " site=" << ( lbRenderingShadowMap ? "shadow-cascade" : "main" )
                        << " objectList " << liObjectList
                        << " coronaPost=" << ( lbRenderingShadowMap ? 0 : 1 ) << "\n";
                }
            }

            if ( lbRenderingShadowMap )
            {
                continue;
            }

            // [DIAG pose wave] one-shot proof that the console leg reached the submission
            // leaf, and with what pose. Cheap enough to leave in: it fires once per boot.
            static bool sbLoggedFirstSubmission = false;
            if ( !sbLoggedFirstSubmission && CgsDev::Log::gpDebugPrint != 0 )
            {
                sbLoggedFirstSubmission = true;
                const Matrix44Affine& lrBody =
                    lrActiveRaceCar.GetRenderParams()->GetBodyTransform();
                *CgsDev::Log::gpDebugPrint
                    << "[racecar-render] GenerateDispatchLists -> RenderRaceCar: car "
                    << liCar << " at (" << lrBody.wAxis.x << ", " << lrBody.wAxis.y
                    << ", " << lrBody.wAxis.z << ") dist " << lfDistance
                    << " object list " << liObjectList << " " << liBefore << " -> "
                    << lpDispatchFrame->GetList( liObjectList )->GetCount()
                    << " (mesh bins " << liOpaqueMeshList << "/"
                    << liTransparentMeshList << ")\n";
            }

            // ---- THE LAMP FLARES (coronas step 1) -------------------------
            // Console position: the LAST thing the per-car body does. In the scene-manager
            // arm that is RenderRaceCar @0x822E7E4C ... AddShadow @0x822E8040 ... five gates
            // ... SubmitCoronasForRaceCar @0x822E80BC ... `b` to the loop tail @0x822E83A0;
            // here it is the same relative position inside the arm that is live on PC.
            //
            // THE FIVE CONSOLE GATES, in the console's own order and spelling:
            //   @0x822E804C  lbzx r11, r22, 0x1834F   mbRenderRaceCarCoronas
            //   @0x822E805C  lfs  f13, 0xD9C(r29)     mRenderParams.mfDeformationSquared < 0.25
            //   @0x822E806C  lbz  r11, 0x44A(r28)     !GetPhysicsState()->mbCrashing
            //   @0x822E8078  lbz  r11, 0x3646(r23)    !mRenderParams.mbIsEngineOff  (car +0x1BE6)
            //   @0x822E8084  lbz  r11, 0x364B(r23)    !mRenderParams.mbIsRaceCarHidden (+0x1BEB)
            // (the two ActiveRaceCar-relative offsets resolve through mRenderParams @+0x7E0:
            //  0x3646 - 0x1A60 - 0x7E0 == 0x1406 and 0x364B - 0x1A60 - 0x7E0 == 0x140B).
            if ( mbRenderRaceCarCoronas
                 && lrActiveRaceCar.GetRenderParams()->GetDeformationSquared()
                        < KF_LIGHTS_OUT_DEFORMATION_SQUARED
                 && !lrActiveRaceCar.GetPhysicsState()->mbCrashing
                 && !lrActiveRaceCar.GetRenderParams()->IsEngineOff()
                 && !lrActiveRaceCar.GetRenderParams()->IsRaceCarHidden() )
            {
                // [FLAG PC bring-up gate] NOT in the console. The console asserts the
                // interface non-null inside the callee and walks on. On this build the slot
                // is seeded EVERY FRAME by BrnWorldModule::GenerateDispatchListsBringUp
                // (SetCoronaSubmissionInterface beside SetShadowMap, from the pointer
                // BrnGameModule::DoDispatch stages out of BrnRendererModule::
                // GetCoronaSubmissionInterfaceBringUp -- the console's RendererIO ->
                // GameBridgeRendererToX -> BrnWorldIO copy chain does not exist here, and
                // GameBridgeRendererToX.cpp is not on tools/build/build_game_exe.bat). That
                // pointer is never null, but BrnCoronaManager::Construct runs LAZILY inside
                // BrnRendererModule::Render, so a non-null interface does NOT imply a real
                // corona buffer -- and AddCorona's tail writes straight through
                // CoronaBuffer::Iterator::mpData. IsReady() is the manager's own answer to
                // "is my buffer real"; see CROSS-GROUP S3-A. DELETE the IsReady() term (not
                // the null check -- that one is this build's) when the corona manager is
                // constructed unconditionally.
                BrnCoronaManager::BrnSubmissionInterface* lpCoronaSubmissionInterface =
                    lpInput->GetCoronaSubmissionInterface();

                if ( lpCoronaSubmissionInterface != 0 && lpCoronaSubmissionInterface->IsReady() )
                {
                    // [FLAG PC bring-up] the BringUp resource getter, for the same reason the
                    // graphics pair above uses it: the console's GetPhysicsResource asserts
                    // IsRaceCarLoaded() (all five resource bits) and would fire a dev assert
                    // every frame on this build. RESTORE the console accessor with the pair
                    // above.
                    SubmitCoronasForRaceCar(
                        lpCoronaSubmissionInterface,
                        mRaceCarStreamer.GetPhysicsResourceBringUp( liCar ),
                        lrActiveRaceCar.GetRenderParams(),
                        mePlayerActiveRaceCarIndex == static_cast<EActiveRaceCarIndex>( liCar ) );
                }
                else
                {
                    static bool sbLoggedNoCoronaInterface = false;
                    if ( !sbLoggedNoCoronaInterface && CgsDev::Log::gpDebugPrint != 0 )
                    {
                        sbLoggedNoCoronaInterface = true;
                        *CgsDev::Log::gpDebugPrint
                            << "[corona] race-car coronas SKIPPED: submission interface "
                            << ( lpCoronaSubmissionInterface != 0 ? "present but NOT READY "
                                                                    "(BrnCoronaManager::Construct "
                                                                    "has not run / the corona "
                                                                    "buffer is null)"
                                                                  : "is NULL (nothing called "
                                                                    "SetCoronaSubmissionInterface)" )
                            << " -- no lamp flares this run [FLAG PC boot gate]\n";
                    }
                }
            }
        }
    }

    lpInput->UnlockForRead();
}

}
