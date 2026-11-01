// Native race-car rendering: GenerateDispatchLists 0x822E79F8, RenderRaceCar 0x822CF6A0.

#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"
#include "GameSource/Replays/Serialisers/BrnReplayRaceCarSerialiserFrame.h"
#include "GameSource/Graphics/BrnBlobbyShadowManager.h"
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



// The selector backing DeformationDebugComponent::OnActivate's "Render car part" control.
namespace BrnPhysics { namespace Deformation { extern s32 giDebugOnlyRenderThisCarPart; } }

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
//
// ✅ RECOVERED 2026-08-23 (wheel-blur bug wave). VALUE = 30.0f in ALL FOUR LANES.
// The previous banner claimed "all 16 bytes read ZERO in the shipped image and a whole-export
// scan finds exactly ONE reference", and concluded the console really divides by zero. BOTH
// halves of that were wrong, and the second one is why: THE WRITER IS NOT A FUNCTION EXPORT.
// It is an unnamed C++ dynamic initialiser -- the same class of writer that hid s_atlasUVs
// (unk_82FAFC10) from an export-only scan -- so grepping .ida-exports could never find it.
//
// The writer, read out of the decrypted XEX2 basefile (devkit key, "basic" compression,
// load address 0x82000000; controls flt_82001C98 == 1.0f and dword_82F24240 == 1280 both pass):
//
//   0x82C4B160  lis     r11, 0x8201
//   0x82C4B164  addi    r10, r1, -0x10
//   0x82C4B168  lfs     f0, 0x499C(r11)                      ; flt_8201499C == 0x41F00000 == 30.0f
//   0x82C4B16C  lis     r11, 0x82FB
//   0x82C4B170  stfs    f0, -0x10(r1)
//   0x82C4B174  lvlx    v0, r0, r10
//   0x82C4B178  addi    r11, r11, -0x2910                    ; r11 = 0x82FAD6F0
//   0x82C4B17C  vspltw  v0, v0, 0                            ; splat the scalar into all 4 lanes
//   0x82C4B180  stvx128 v0, r0, r11                          ; gvWheelBlurConstants = VecFloat(30)
//   0x82C4B184  blr
//
// It is REACHED, not dead: its address is entry 0x82CD0370 of the module's static-initialiser
// pointer table (a monotonically increasing run of 0x82C4Axxx/0x82C4Bxxx thunks at
// 0x82CD0300..), i.e. the CRT dynamic-init list the loader walks before main.
//
// Two independent attestations that 30.0f is the right number and that the unit is rad/s:
//   1. The very next initialiser, 0x82C4B188, reads THIS global back, divides flt_82001C98
//      (1.0f) by it and splats the quotient into unk_82FAD540 -- the classic
//      `const VecFloat KVF_X(30.0f); const VecFloat KVF_ONE_OVER_X(1.0f/KVF_X);` pair.
//   2. The TRAFFIC twin is the same number reached the same way: BrnTrafficEntityModule's
//      blur threshold unk_8300CC60 is seeded 30.0f by 0x82C65C20 (from flt_820BA5E8), its
//      reciprocal unk_8300C8F0 by 0x82C65C48, and its speed->spin scale unk_8300C9A0 is
//      3.3333333f == 1/0.3 m (flt_8205873C) -- i.e. traffic converts m/s to rad/s with a
//      0.3 m wheel radius and then compares against the SAME 30 rad/s. 30 rad/s on a
//      0.33 m race-car tyre is ~10 m/s (~22 mph), which is where a tyre visually smears.
//
// ⚠️ WHY THIS WAS A BUG, NOT A CURIOSITY: with the divisor left at 0.0f the quotient was
// 0/0 == NaN for a parked car and +inf for a moving one, and -- far worse -- the technique
// test `gvWheelBlurConstants.x > angularVelocity` was `0 > w`, FALSE for every non-negative
// w including a stationary wheel, so lu8Technique never left 0 (the BLURRED variant) and
// EVERY wheel in the game rendered blurred, junkyard and car-select included. Same family as
// KF_DEFAULT_ASPECTRATIO == 0.0f making every camera 180 degrees tall.
//
// Declared Vector4 here (the committed type) but written to all four lanes because the console
// global is a VecFloat splat and the console's technique test is an ALL-LANE `vcmpgtfp.` +
// CR6 bit 24; with four equal lanes that is identical to the lane-x test spelled below.
//
// The producer side is live as of 2026-09-12: ActiveRaceCar::CalculateWheelAngularVelocities
// fills RenderParams::mafWheelAngularVelocities every tick from the published per-wheel spin
// rate, so the race car now has BOTH halves -- sharp at rest and blurred past 30 rad/s.
Vector4 gvWheelBlurConstants(30.0f, 30.0f, 30.0f, 30.0f);

// The console's wheel matrix / pointer stack arrays are four entries long
// (`__vector_constructor_iterator(v390, 64, 4, ...)`), which is also the debug
// variable's upper bound.
static const s32 KU_WHEELS_TO_RENDER_MAX = 4;

// ============================================================================
// unk_82FAD420 -- THE PER-PANE CRACK UV OFFSET TABLE (shader constant 31,
// `g_glassFractureUVOffsets`). Eight Vector4s, one per iteration of the cracked-glass
// loop (`addi r24, r24, 0x10` @0x822D0F54).
//
// ⚠️ ANOTHER SILENT-ZERO CONSTANT: the 128 bytes read 0x00 straight out of the image
// because a CRT dynamic initialiser fills them before main, so a literal scan finds only
// the READER. tools/re/findinit.py 0x82FAD420 reports exactly two sites that materialise
// the address -- 0x822D0990 (this loop) and 0x82C4BE38 (the writer) -- and disassembling
// the writer at 0x82C4BD40..0x82C4BEBC shows it building eight quadwords on the stack from
// .rodata floats and copying them to +0x00..+0x70 with `stvx128 v0, r11, {0,0x10,..,0x70}`.
// The 32 sources, in the writer's own store order, decode to:
//
//   -0x80 8200D584 -0.0    -0x7c 820147FC  0.5    -0x78 82005450  0.9    -0x74 820147E0 0.1
//   -0x70 82020A84 -0.2    -0x6c 8200D5FC -0.7    -0x68 8201496C  0.3    -0x64 82014930 0.8
//   -0x60 82004C78 -0.5    -0x5c 82014930  0.8    -0x58 820147F4  0.6    -0x54 82005450 0.9
//   -0x50 8200D530 -0.1    -0x4c 82004C68  0.7    -0x48 820147FC  0.5    -0x44 82004C68 0.7
//   -0x40 8200D5A4 -0.9    -0x3c 8200D5A4 -0.9    -0x38 82014930  0.8    -0x34 8200473C 0.4
//   -0x30 82004C78 -0.5    -0x2c 8201496C  0.3    -0x28 820147F4  0.6    -0x24 820147F4 0.6
//   -0x20 8200D564 -0.8    -0x1c 820147FC  0.5    -0x18 820147FC  0.5    -0x14 820147E0 0.1
//   -0x10 82020A80 -0.3    -0x0c 82020A80 -0.3    -0x08 82005450  0.9    -0x04 8200473C 0.4
//
// Read with tools/re/x360rd.py; every value is an exact tenth, which is itself a check.
// They are two UV offsets per pane (xy for the first crack fetch, zw for the second), and
// their job is to decorrelate the pattern so eight panes of one car do not crack alike.
// ============================================================================
static const Vector4 KAV4_GLASS_FRACTURE_UV_OFFSETS[8] =
{
    Vector4(-0.0f,  0.5f,  0.9f,  0.1f),
    Vector4(-0.2f, -0.7f,  0.3f,  0.8f),
    Vector4(-0.5f,  0.8f,  0.6f,  0.9f),
    Vector4(-0.1f,  0.7f,  0.5f,  0.7f),
    Vector4(-0.9f, -0.9f,  0.8f,  0.4f),
    Vector4(-0.5f,  0.3f,  0.6f,  0.6f),
    Vector4(-0.8f,  0.5f,  0.5f,  0.1f),
    Vector4(-0.3f, -0.3f,  0.9f,  0.4f),
};

// BrnWorld::SetGlassFractureConstants @0x822BD280 -- defined in the sibling TU
// BrnRaceCarEntityModule_GlassFracture.cpp, whose DWARF declaration home is
// BrnRaceCarEntityModule.h. Declared here rather than added to that header for the same
// reason the ShaderConstantTable extern above is spelled locally: this TU needs the
// symbol, not the header's whole surface.
namespace BrnPhysics { namespace Deformation { extern s32 giDebugOnlyRenderThisCarPart; extern bool kbAllowDeformationDebug; } }

namespace BrnWorld
{
    void SetGlassFractureConstants(float lfFractureStrength,
                                   float lfEqualisationFactor,
                                   const rw::math::fpu::Vector2Template<float>& lvUVScale,
                                   Vector4 lvUVOffsets);
}

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
static __vector4 RaceCarPack3(__vector4 a, __vector4 b, __vector4 c)
{
    static const __declspec(align(16)) unsigned char mask[16] =
        {0,1,2,3,20,21,22,23,0,1,2,3,0,1,2,3};
    return __vrlimi(__vperm(a, b, __lvx(mask, 0)), c, 2, 0);
}

static bool RaceCarMagnitudeExceeds(__vector4 v, float tolerance)
{
    const __vector4 allOnes = __vspltisw(-1);
    const __vector4 sign = __vslw(allOnes, allOnes);
    const __vector4 magnitude = __vandc(__vmsum3fp(v, v), sign);
    const __vector4 threshold = __vspltw(__lvlx(&tolerance, 0), 0);
    unsigned int cr;
    __vcmpgtfpR(magnitude, threshold, &cr);
    return (cr & 0x80u) != 0;
}

static bool RaceCarIsNormal(const Matrix44Affine& m)
{
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    return !RaceCarMagnitudeExceeds(RaceCarPack3(
        __vsubfp(__vmsum3fp(m.xAxis.mV, m.xAxis.mV), one),
        __vsubfp(__vmsum3fp(m.yAxis.mV, m.yAxis.mV), one),
        __vsubfp(__vmsum3fp(m.zAxis.mV, m.zAxis.mV), one)), 0.01f);
}

static bool RaceCarIsOrthogonal(const Matrix44Affine& m, float tolerance)
{
    const __vector4 x = m.xAxis.mV, y = m.yAxis.mV, z = m.zAxis.mV;
    const __vector4 t0 = __vmrghw(x, z), t1 = __vmrghw(y, y);
    const __vector4 t2 = __vmrglw(x, z), t3 = __vmrglw(y, y);
    const __vector4 c0 = __vmrghw(t0, t1), c1 = __vmrglw(t0, t1), c2 = __vmrghw(t2, t3);
    const Vector4 ix(1.0f,0.0f,0.0f,0.0f);
    const Vector4 iy(0.0f,1.0f,0.0f,0.0f);
    const Vector4 iz(0.0f,0.0f,1.0f,0.0f);
    __vector4 a = __vmaddfp(c1, __vspltw(x,1), __vmulfp(c0, __vspltw(x,0)));
    __vector4 b = __vmaddfp(c1, __vspltw(y,1), __vmulfp(c0, __vspltw(y,0)));
    __vector4 c = __vmaddfp(c1, __vspltw(z,1), __vmulfp(c0, __vspltw(z,0)));
    a = __vsubfp(__vmaddfp(c2, __vspltw(x,2), a), ix.mV);
    b = __vsubfp(__vmaddfp(c2, __vspltw(y,2), b), iy.mV);
    c = __vsubfp(__vmaddfp(c2, __vspltw(z,2), c), iz.mV);
    return !RaceCarMagnitudeExceeds(RaceCarPack3(__vmsum3fp(a,a), __vmsum3fp(b,b), __vmsum3fp(c,c)), tolerance);
}

static bool RaceCarWheelRowValid(__vector4 row)
{
    unsigned int cr;
    __vector4 lane = __vspltw(row, 0); __vcmpeqfpR(lane, lane, &cr);
    if (!(cr & 0x80u)) return false;
    lane = __vspltw(row, 1); __vcmpeqfpR(lane, lane, &cr);
    if (!(cr & 0x80u)) return false;
    lane = __vspltw(row, 2); __vcmpeqfpR(lane, lane, &cr);
    return (cr & 0x80u) != 0;
}

static void RaceCarMatrixAssert(const char* message, const Matrix44Affine& m, int line)
{
    CgsDev::Assert::BeginAssert();
    CgsDev::StrStream stream(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
    stream << message;
    stream.AppendFormat("\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n(%f, %f, %f, %f)\n",
        m.xAxis.mV.x,m.xAxis.mV.y,m.xAxis.mV.z,m.xAxis.mV.w,
        m.yAxis.mV.x,m.yAxis.mV.y,m.yAxis.mV.z,m.yAxis.mV.w,
        m.zAxis.mV.x,m.zAxis.mV.y,m.zAxis.mV.z,m.zAxis.mV.w,
        m.wAxis.mV.x,m.wAxis.mV.y,m.wAxis.mV.z,m.wAxis.mV.w);
    CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp", line);
    CgsDev::Assert::EndAssert();
}

void
RaceCarEntityModule::RenderRaceCar( CgsGraphics::DispatchFrame* lpDispatchFrame,
                                    rw::math::vpu::VecFloat lCameraDistance,
                                    Vector4 lvFogScattering,
                                    Vector4 lvFogColourPlusWhiteLevel,
                                    const ActiveRaceCar::RenderParams* lpRenderParams,
                                    const CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec>& lrCarGraphics,
                                    const CgsResource::ResourcePtr<BrnWheel::GraphicsSpec>& lrWheelGraphics,
                                    s32 liObjectList,
                                    s32 liOpaqueMeshList,
                                    s32 liTransparentMeshList,
                                    ShadowMap* lpShadowMap,
                                    bool lbRenderAttachedGeometry ) const
{
    CGS_ASSERT( lpRenderParams != 0, "lpRenderParams" );
    CGS_ASSERT( lpDispatchFrame != 0, "lpDispatchFrame" );
    CGS_ASSERT( lpShadowMap != 0, "lpShadowMap" );

    // The X360 loads the four body-transform rows into v124/v126/v125/v117 straight from
    // r5 (the RenderParams) before anything else -- the matrix at offset 0 IS
    // mBodyTransform.
    const Matrix44Affine lBodyTransform = lpRenderParams->GetBodyTransform();
    if (!mbInReplay && !RaceCarIsNormal(lBodyTransform))
        RaceCarMatrixAssert("Non-normal racecar body transform: ", lBodyTransform, 3457);
    if (!mbInReplay && !RaceCarIsOrthogonal(lBodyTransform, 0.1f))
        RaceCarMatrixAssert("Body transform is not orthogonal ", lBodyTransform, 3458);

    // `if (!*(rp + 5131))` -- mbIsHidden gates the ENTIRE function body.
    if ( lpRenderParams->IsRaceCarHidden() )
    {
        return;
    }

    // Every spec read in the console body goes through ResourcePtr::operator-> (the
    // `BrnVehicle::GraphicsSpec_::operat(v56)` calls); it is hoisted once here.
    const BrnVehicle::GraphicsSpec* lpCarGraphicsSpec = lrCarGraphics.operator->();

    // ---- the DEBUG damage overrides (@0x822CFAB4..0x822CFB0C) ---------------
    // `if (DEBUG_mbOverrideDamage) { v119 = {crumple, 0, dust, 0};
    //    RenderParams::DEBUG_OverrideScratchAmount(DEBUG_mfVehicleScratchAmount); }`
    // v119 is the vector the console later uploads as shader constant 23
    // (`vmr128 v1, v119 ; SetShaderConstantData(23)` @0x822CFE64..0x822CFE70).
    //
    // ARTIST 822CF708/70C/718/720 zeros all four lanes of var_4D0; 822CF738
    // addresses that stack vector and 822CF760 loads it into v119. The override-off
    // value is therefore the original zero vector, not an uninitialized register.
    Vector4 lv4DamageConstants(0.0f, 0.0f, 0.0f, 0.0f);
    if ( DEBUG_mbOverrideDamage )
    {
        // `stfs module+0x18784 -> var_510 (lane X)` / `stfs module+0x18780 -> var_508
        // (lane Z)`, with f31 (== flt_82001CC0 == 0.0f) in lanes Y and W.
        lv4DamageConstants.mV.x = DEBUG_mfVehicleCrumpleAmount;
        lv4DamageConstants.mV.z = DEBUG_mfVehicleDustAmount;
        // The original const input is passed to this non-const debug method at 822CFB0C.
        const_cast<ActiveRaceCar::RenderParams*>(lpRenderParams)->DEBUG_OverrideScratchAmount( DEBUG_mfVehicleScratchAmount );
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
    Vector4 lv4LightState(0.0f, 0.0f, 0.0f, 0.0f);
    if ( !lpRenderParams->IsEngineOff()
         && lpRenderParams->GetDeformationSquared() < KF_LIGHTS_OUT_DEFORMATION_SQUARED )
    {
        // unk_82181510 == (0, 1, 0, 0): the always-on channel, and only that one.
        lv4LightState.mV.y = KF_LIGHT_CHANNEL_G_ALWAYS_ON;

        if ( lpRenderParams->IsBraking() )
        {
            lv4LightState.mV.x = 1.0f;     // vrlimi128 mask 8 -> lane X
        }
        if ( lpRenderParams->IsReversing() )
        {
            lv4LightState.mV.z = 1.0f;     // vrlimi128 mask 2 -> lane Z
        }
    }

    // vmaxfp against the three editable floats, then vmulfp. Both flush
    // subnormals; the vector maximum propagates unordered operands.
    const Vector4 lv4DebugIllumination(DEBUG_mfSelfIlluminationR,
                                       DEBUG_mfSelfIlluminationG,
                                       DEBUG_mfSelfIlluminationB, 0.0f);
    const Vector4 lv4SelfIlluminationMask(__vmulfp(
        __vmaxfp(lv4DebugIllumination.mV, lv4LightState.mV),
        __vcfsx(__vspltisw(8), 0)));

    // ---- the shadow-pass selector ------------------------------------------
    // v320 = (shadowMap[0] && shadowMap[1]): the two LEADING ShadowMap bytes the asm reads
    // as `lbz r11, 0(a40)` / `lbz r11, 1(a40)` -- the same pair
    // WorldEntityModule::GenerateDispatchLists reads to select its shadow pass, i.e.
    // {mbRenderingShadowMap, mbUseZOnlyRenderingPath}. It selects the shadow variant of both
    // the technique index and the AddToBin routing (see below).
    const bool lbShadowPass = lpShadowMap->IsRenderingShadowMap()
                           && lpShadowMap->IsUsingZOnlyRenderingPath();

    // ---- per-car shader constants ------------------------------------------
    // ⭐ THE "NO FRACTURE" RESET, restored 2026-09-06. `fmr f1, f31(=0.0f) ; fmr f2, f29(=1.0f)
    // ; addi r5, r1, var_510 (= {0,0}) ; vmr128 v1, v123 (= vspltisw 0) ; bl
    // SetGlassFractureConstants` @0x822CFBE0..0x822CFC08 -- one call per car, immediately
    // before constants 20/21. It is what makes an INTACT pane intact: with strength 0 the
    // helper writes inv = 0 into constant 30, and the pixel shader's
    // `mad_sat r1.xy, r4, c5.y, -c5.x` then collapses to saturate(0) on every glass pixel.
    // Without it the register keeps the LAST cracked pane's strength -- and an unset external
    // shader constant is SKIPPED rather than zeroed (shadowingdevice.cpp:847), so before this
    // wave constants 30/31/32 were never published by anything at all and every glass program
    // read whatever the previous draw left in c5 / c159 / c160.
    // ⛔ WAS BLOCKED ON A MOUNT, NOT ON KNOWLEDGE: the only definition of
    // SetGlassFractureConstants lives in BrnRaceCarEntityModule_GlassFracture.cpp, which was
    // absent from tools/build/build_game_exe.bat. That mount lands with this change.
    {
        const rw::math::fpu::Vector2Template<float> lv2NoFractureScale(0.0f, 0.0f);
        const Vector4 lv4NoFractureOffsets(0.0f, 0.0f, 0.0f, 0.0f);
        SetGlassFractureConstants( 0.0f, 1.0f, lv2NoFractureScale, lv4NoFractureOffsets );
    }

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

    // ARTIST 0x822CFC50..0x822CFE18. Keep VMX estimates, masks and fused operations.
    {
        static const __declspec(align(16)) u32 auPowC0[4] = {0x3c1d8c54u,0xbaae1854u,0x391aa7d7u,0xb7364261u};
        static const __declspec(align(16)) u32 auPowC1[4] = {0x3f800000u,0xbf317218u,0x3e75fdedu,0xbd6357cau};
        static const __declspec(align(16)) u32 auPowC2[4] = {0x3e77adbdu,0xbe0cd4fbu,0x3d5541c6u,0xbc188b0bu};
        static const __declspec(align(16)) u32 auPowC3[4] = {0x3fb8aa0eu,0xbf389e52u,0x3ef5162du,0xbeb1d204u};
        const __vector4 v123 = __vspltisw(0);
        const __vector4 v127 = __vspltisw(-1);
        __vector4 v8 = __vspltisw(1);
        __vector4 v4 = __vspltisw(-9);
        __vector4 v7 = lvFogScattering.mV;
        const __vector4 v121 = __vcfsx(v8, 0);
        __vector4 v0 = __vspltw(v7, 0);
        __vector4 v13 = __vspltw(v7, 1);
        __vector4 v3 = __vslw(v127, v127);
        const __vector4 v6 = __vspltw(__vupkd3d(v123, VPACK_NORMSHORT2), 3);
        __vector4 v12 = __vmulfp(lCameraDistance.mV, v0);
        v0 = __vspltw(v7, 2);
        __vector4 v10 = __lvx(auPowC3, 0);
        __vector4 v31 = __vspltw(v10, 1);
        __vector4 v1 = __vspltw(v10, 0);
        const __vector4 v27 = __vspltw(v10, 3);
        v13 = __vsubfp(v12, v13);
        v12 = __vslw(v127, v4);
        __vector4 v9 = __lvx(auPowC2, 0);
        __vector4 v29 = __vspltw(v9, 1);
        __vector4 v30 = __vspltw(v9, 0);
        const __vector4 v26 = __vspltw(v9, 3);
        v13 = __vmaxfp(v123, v13);
        __vector4 v11 = __vminfp(v121, v13);
        __vector4 v5 = __vandc(v11, v3);
        v13 = __vsel(v5, v6, v12);
        __vector4 v28 = __vlogefp(v5);
        v13 = __vsubfp(v13, v6);
        v12 = __vmulfp(v13, v13);
        v1 = __vmaddfp(v13, v31, v1);
        v31 = __vspltw(v10, 2);
        v30 = __vmaddfp(v13, v29, v30);
        v10 = __lvx(auPowC1, 0);
        v29 = __vspltw(v9, 2);
        v5 = __vspltw(v10, 0);
        v9 = __lvx(auPowC0, 0);
        __vector4 v2 = __vmulfp(v13, v12);
        v1 = __vmaddfp(v12, v31, v1);
        v31 = __vmulfp(v12, v12);
        v12 = __vmaddfp(v12, v29, v30);
        v13 = __vmulfp(v13, v0);
        v30 = __vrfim(v28);
        v1 = __vmaddfp(v2, v27, v1);
        v29 = __vspltw(v10, 1);
        v12 = __vmaddfp(v2, v26, v12);
        const __vector4 v25 = __vspltw(v9, 0);
        v2 = __vmulfp(v30, v0);
        const __vector4 v24 = __vspltw(v9, 1);
        v28 = __vspltw(v10, 2);
        v30 = __vrfiz(v0);
        v10 = __vspltw(v10, 3);
        v3 = __vand(v11, v3);
        v4 = __vslw(v127, v4);
        v7 = __vspltw(v7, 3);
        v12 = __vmaddfp(v31, v12, v1);
        v13 = __vmaddfp(v13, v12, v2);
        v12 = __vrfim(v13);
        v13 = __vsubfp(v13, v12);
        v2 = __vexptefp(v12);
        v12 = __vmulfp(v13, v13);
        v1 = __vmaddfp(v13, v29, v5);
        v31 = __vmaddfp(v13, v24, v25);
        v13 = __vmulfp(v13, v12);
        v1 = __vmaddfp(v12, v28, v1);
        v10 = __vmaddfp(v13, v10, v1);
        v1 = __vspltw(v9, 2);
        v9 = __vspltw(v9, 3);
        v1 = __vmaddfp(v12, v1, v31);
        v12 = __vmulfp(v12, v12);
        v31 = __vctsxs(v0, 0);
        v13 = __vmaddfp(v13, v9, v1);
        v1 = __vcmpeqfp(v0, v30);
        v9 = __vcmpeqfp(v0, v123);
        v0 = __vcmpgtfp(v123, v0);
        v12 = __vmaddfp(v12, v13, v10);
        v13 = __vand(v31, v8);
        v31 = __vcmpgtfp(v123, v11);
        v8 = __vsrw(v4, v8);
        v10 = __vcmpeqfp(v11, v123);
        v13 = __vslw(v13, v127);
        v11 = __vand(v3, v13);
        v13 = __vrefp(v12);
        v4 = __vandc(v31, v1);
        v0 = __vandc(v10, v0);
        v1 = __vor(v123, v11);
        v10 = __vor(v10, v9);
        v0 = __vsel(v8, v1, v0);
        v10 = __vor(v4, v10);
        v9 = __vsel(v0, v6, v9);
        v0 = __vnmsubfp(v12, v13, v5);
        v0 = __vmaddfp(v13, v0, v13);
        v12 = __vnmsubfp(v12, v0, v5);
        v8 = __vcmpeqfp(v0, v0);
        v0 = __vmaddfp(v0, v12, v0);
        v0 = __vsel(v13, v0, v8);
        v0 = __vmulfp(v2, v0);
        v0 = __vor(v0, v11);
        v0 = __vsel(v0, v9, v10);
        v0 = __vmulfp(v0, v7);
        v13 = __vsubfp(v121, v0);
        Vector4 lv4Fog;
        lv4Fog.mV = __vrlimi(__vmulfp(lvFogColourPlusWhiteLevel.mV, v0), v13, 1, 0);
        CgsGraphics::mShaderConstantTable.SetShaderConstantData(26, lv4Fog);
    }

    // ARTIST uploads the verlet offsets and damage constants when deformation is enabled.
    const bool lbDamaged = lpRenderParams->IsDamaged() || BrnPhysics::Deformation::kbAllowDeformationDebug;
    if (lbDamaged || DEBUG_mbOverrideDamage)
    {
        CgsGraphics::mShaderConstantTable.SetShaderConstantArrayData(22, lpRenderParams->GetVerletOffsets());
        CgsGraphics::mShaderConstantTable.SetShaderConstantData(23, lv4DamageConstants);
    }

    // ARTIST 822CFE78..822D15BC: body, glass and wheel dispatch.
    if (mbRenderCarsDuringCrash)
    {
        const u32 luPartCount = lrCarGraphics->muPartsCount;
        for (u32 luPart = 0; luPart < luPartCount; ++luPart)
        {
            const s32 liSelectedPart = BrnPhysics::Deformation::giDebugOnlyRenderThisCarPart;
            if (liSelectedPart != -1 && liSelectedPart != static_cast<s32>(luPart)) continue;
            if (!lpRenderParams->IsPartVisible(static_cast<u8>(luPart))) continue;
            const CgsGraphics::Model* lpPartModel = lrCarGraphics->GetPartModel(luPart);
            if (!lpPartModel) continue;
            const CgsGraphics::Model::State leLOD = lpRenderParams->GetLOD();
            if (!lpPartModel->DoesStateExist(leLOD)) continue;
            const Matrix44Affine& lrNormalLocator = lrCarGraphics->GetPartLocators()[luPart];
            if (!RaceCarIsNormal(lrNormalLocator))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("IsNormal3x3( lpCarGraphicsSpec->mpPartLocators[ luPartIdx ], 0.01f )",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp", 3583);
                CgsDev::Assert::EndAssert();
            }
            const Matrix44Affine& lrOrthogonalLocator = lrCarGraphics->GetPartLocators()[luPart];
            if (!RaceCarIsOrthogonal(lrOrthogonalLocator, 1.1920928955078125e-7f))
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("IsOrthogonal3x3( lpCarGraphicsSpec->mpPartLocators[ luPartIdx ] )",
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp", 3584);
                CgsDev::Assert::EndAssert();
            }
            const Matrix44Affine& lrLocator = lrCarGraphics->GetPartLocators()[luPart];
            Matrix44Affine lPart;
            __vector4 lvRow = lrLocator.xAxis.mV;
            __vector4 lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lPart.xAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.yAxis.mV;
            lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lPart.yAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.zAxis.mV;
            lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lPart.zAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.wAxis.mV;
            lvResult = __vmaddfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV, lBodyTransform.wAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lPart.wAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            bool lbAttached = true;
            const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrQueue = lpRenderParams->GetDetachedPartQueue();
            for (s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent)
            {
                const DetachedPartRenderEvent& lrEvent = lrQueue.GetEvent(liEvent);
                if (lrEvent.miPartIndex == static_cast<s32>(luPart))
                {
                    lPart = lrEvent.mTransform;
                    lbAttached = lrEvent.mbIsAttached;
                    break;
                }
            }
            if (lbAttached)
            {
                if (!lbRenderAttachedGeometry) continue;
                if (!lbSelfIlluminationArmed)
                {
                    CgsGraphics::mShaderConstantTable.SetShaderConstantData(24, lv4SelfIlluminationMask);
                    lbSelfIlluminationArmed = true;
                }
            }
            else if (lbSelfIlluminationArmed)
            {
                const Vector4 lvZero(0.0f,0.0f,0.0f,0.0f);
                CgsGraphics::mShaderConstantTable.SetShaderConstantData(24, lvZero);
                lbSelfIlluminationArmed = false;
            }
            if (!mbInReplay && !RaceCarIsNormal(lPart))
                RaceCarMatrixAssert("Part matrix is not normal", lPart, 3627);
            if (!mbInReplay && !RaceCarIsOrthogonal(lPart, 1.1920928955078125e-7f))
                RaceCarMatrixAssert("Part matrix is not orthogonal", lPart, 3628);
            const u8 lu8Technique = (lbDamaged || DEBUG_mbOverrideDamage)
                ? (lbShadowPass ? 3u : 0u) : (lbShadowPass ? 2u : 1u);
            const CgsGraphics::Model::State leDrawLOD = lpRenderParams->GetLOD();
            const u8 luDamageFlags = lpRenderParams->GetRenderDamageFlag();
            const CgsGraphics::Renderable* lpRenderable = lpPartModel->GetRenderable(leDrawLOD);
            CGS_ASSERT(lpRenderable != 0, "Missing renderable in a model");
            CgsGraphics::DispatchList* lpList = lpDispatchFrame->GetList(liObjectList);
            CGS_ASSERT(lpList != 0, "lpDispatchList");
            CGS_ASSERT(!lpPartModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER),
                       "lbInstancing == GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER )");
            CgsGraphics::mShaderConstantTable.SetShaderConstantData(0, lPart);
            const bool lbFirst = (lpList->GetCount() & 0x7F) == 0;
            lpDispatchFrame->GetBin().BeginPacket();
            CgsGraphics::DrawRenderable::AddToBin(lpRenderable, lpDispatchFrame, lbFirst,
                static_cast<s8>(liOpaqueMeshList),
                static_cast<s8>(lbShadowPass ? liOpaqueMeshList : liTransparentMeshList),
                1, lu8Technique, lbShadowPass, 0xFFu, 0u, 0, luDamageFlags);
            lpList->Submit(0, lpDispatchFrame->GetBin().EndPacket());
        }
    }
    // ARTIST 0x822D0964..0x822D0F64: the glass pass follows the body-part pass.
    if (lbRenderAttachedGeometry)
    {
        for (u32 luGlass = 0; luGlass < lrCarGraphics->muShatteredGlassPartsCount; ++luGlass)
        {
            const BrnVehicle::ShatteredGlassPart* lpPart = &lrCarGraphics->GetShatteredGlassParts()[luGlass];
            CGS_ASSERT(lpPart != 0, "lpPart != NULL");
            const s32 liPane = static_cast<s32>(lpPart->muBodyPartType) - 16;
            const f32 lfFracture = lpRenderParams->GetCrackedGlassFractureAmountN(liPane);
            if ((lpRenderParams->GetRenderDamageFlag() & (1u << liPane)) == 0 || lfFracture > 1.0f)
                continue;
            const f32 lfEqualisation = lpRenderParams->GetCrackedGlassEqualisationFactorN(liPane);
            const Vector4 lUVOffsets = KAV4_GLASS_FRACTURE_UV_OFFSETS[luGlass];
            const rw::math::fpu::Vector2Template<float> lScale = lpRenderParams->GetCrackedGlassScaleFactorsN(liPane);
            SetGlassFractureConstants(lfFracture, lfEqualisation, lScale, lUVOffsets);

            const Matrix44Affine& lrLocator = lrCarGraphics->GetPartLocators()[lpPart->muBodyPartIndex];
            Matrix44Affine lGlass;
            __vector4 lvRow = lrLocator.xAxis.mV;
            __vector4 lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lGlass.xAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.yAxis.mV;
            lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lGlass.yAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.zAxis.mV;
            lvResult = __vmulfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lGlass.zAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);
            lvRow = lrLocator.wAxis.mV;
            lvResult = __vmaddfp(__vspltw(lvRow, 0), lBodyTransform.xAxis.mV, lBodyTransform.wAxis.mV);
            lvResult = __vmaddfp(__vspltw(lvRow, 1), lBodyTransform.yAxis.mV, lvResult);
            lGlass.wAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lBodyTransform.zAxis.mV, lvResult);

            const ActiveRaceCar::RenderParams::DetachedPartRenderQueue& lrQueue = lpRenderParams->GetDetachedPartQueue();
            for (s32 liEvent = 0; liEvent < lrQueue.GetLength(); ++liEvent)
            {
                const DetachedPartRenderEvent& lrEvent = lrQueue.GetEvent(liEvent);
                if (lrEvent.miPartIndex == static_cast<s32>(lpPart->muBodyPartIndex))
                {
                    lGlass = lrEvent.mTransform;
                    break;
                }
            }
            const u8 lu8Technique = (lbDamaged || DEBUG_mbOverrideDamage)
                ? (lbShadowPass ? 3u : 0u) : (lbShadowPass ? 2u : 1u);
            const CgsGraphics::Model* lpModel = lpPart->GetModel();
            const CgsGraphics::Renderable* lpRenderable = lpModel->GetRenderable(CgsGraphics::Model::E_STATE_LOD_0);
            CGS_ASSERT(lpRenderable != 0, "lpRenderable");
            CgsGraphics::DispatchList* lpList = lpDispatchFrame->GetList(liObjectList);
            CGS_ASSERT(lpList != 0, "lpDispatchList");
            CGS_ASSERT(!lpModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER),
                       "lbInstancing == GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER )");
            CgsGraphics::mShaderConstantTable.SetShaderConstantData(0, lGlass);
            const bool lbFirst = (lpList->GetCount() & 0x7F) == 0;
            lpDispatchFrame->GetBin().BeginPacket();
            CgsGraphics::DrawRenderable::AddToBin(lpRenderable, lpDispatchFrame, lbFirst,
                static_cast<s8>(liOpaqueMeshList),
                static_cast<s8>(lbShadowPass ? liOpaqueMeshList : liTransparentMeshList),
                1, lu8Technique, lbShadowPass, 0xFFu, 0u, 0, 0u);
            lpList->Submit(0, lpDispatchFrame->GetBin().EndPacket());
        }
    }
    // ARTIST 0x822D0F68..0x822D15BC.
    if (mbRenderWheels && lbRenderAttachedGeometry)
    {
        const BrnWheel::GraphicsSpec* lpWheelSpec = lrWheelGraphics.operator->();
        const CgsGraphics::Model* lpWheelModel = lpWheelSpec->GetWheelModel();
        CgsGraphics::Model::State leLOD = lpRenderParams->GetLOD();
        if (leLOD <= CgsGraphics::Model::E_STATE_LOD_1) leLOD = CgsGraphics::Model::E_STATE_LOD_1;
        if (lpWheelModel)
        {
            if (!lpWheelModel->DoesStateExist(leLOD)) leLOD = CgsGraphics::Model::E_STATE_LOD_1;
            if (lpWheelModel->DoesStateExist(leLOD))
            {
                // The four real constant vectors immediately precede the four real
                // matrices in ARTIST's stack frame. The provider reads a fifth vector,
                // which is the first matrix row, rather than a fifth constant entry.
                struct WheelInstanceStack
                {
                    Vector4 maConstants[4];
                    Matrix44Affine maMatrices[4];
                } lWheelStack;
                CGS_STATIC_ASSERT((offsetof(WheelInstanceStack, maMatrices) == 0x40), "ARTIST adjacent wheel vectors and matrices");
                const Matrix44Affine* lapMatrices[4] = {0, 0, 0, 0};
                s32 liCount = 0;
                u8 lu8Technique = 0;
                u32 luWheelCount = static_cast<u32>(giWheelsToRender);
                if (luWheelCount != 0)
                {
                    u8 luWheel = 0;
                    do
                    {
                        if (lpRenderParams->GetWheelExists(luWheel))
                        {
                            const Matrix44Affine& lrWheel = lpRenderParams->GetWheelTransform(luWheel);
                            const Matrix44Affine& lrScale = lpRenderParams->GetWheelScaleMatrix(luWheel);
                            Matrix44Affine lWheel;
                            __vector4 lvRow = lrScale.xAxis.mV;
                            __vector4 lv = __vmulfp(__vspltw(lvRow, 0), lrWheel.xAxis.mV);
                            lv = __vmaddfp(__vspltw(lvRow, 1), lrWheel.yAxis.mV, lv);
                            lWheel.xAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lrWheel.zAxis.mV, lv);
                            lvRow = lrScale.yAxis.mV;
                            lv = __vmulfp(__vspltw(lvRow, 0), lrWheel.xAxis.mV);
                            lv = __vmaddfp(__vspltw(lvRow, 1), lrWheel.yAxis.mV, lv);
                            lWheel.yAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lrWheel.zAxis.mV, lv);
                            lvRow = lrScale.zAxis.mV;
                            lv = __vmulfp(__vspltw(lvRow, 0), lrWheel.xAxis.mV);
                            lv = __vmaddfp(__vspltw(lvRow, 1), lrWheel.yAxis.mV, lv);
                            lWheel.zAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lrWheel.zAxis.mV, lv);
                            lvRow = lrScale.wAxis.mV;
                            lv = __vmaddfp(__vspltw(lvRow, 0), lrWheel.xAxis.mV, lrWheel.wAxis.mV);
                            lv = __vmaddfp(__vspltw(lvRow, 1), lrWheel.yAxis.mV, lv);
                            lWheel.wAxis.mV = __vmaddfp(__vspltw(lvRow, 2), lrWheel.zAxis.mV, lv);
                            const f32 lfAngularVelocity = lpRenderParams->GetWheelAngularVelocity(luWheel);
                            const f32 lfSpin = lfAngularVelocity / gvWheelBlurConstants.mV.x;
                            const __vector4 lvSpin = __vspltw(__lvlx(&lfSpin, 0), 0);
                            const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
                            Vector4 lWheelConstants;
                            lWheelConstants.mV = __vrlimi(__vspltisw(0), __vminfp(lvSpin, lvOne), 8, 0);
                            CgsGraphics::mShaderConstantTable.SetShaderConstantData(25, lWheelConstants);
                            const f32 lfTechniqueVelocity = lpRenderParams->GetWheelAngularVelocity(luWheel);
                            const __vector4 lvAngularVelocity = __vspltw(__lvlx(&lfTechniqueVelocity, 0), 0);
                            unsigned int luCompare;
                            __vcmpgtfpR(gvWheelBlurConstants.mV, lvAngularVelocity, &luCompare);
                            if (luCompare & 0x80u) lu8Technique = 1;
                            if (lbShadowPass) lu8Technique = 2;
                            const bool lbXValid = RaceCarWheelRowValid(lWheel.xAxis.mV);
                            const bool lbYValid = RaceCarWheelRowValid(lWheel.yAxis.mV);
                            const bool lbZValid = RaceCarWheelRowValid(lWheel.zAxis.mV);
                            const bool lbWValid = RaceCarWheelRowValid(lWheel.wAxis.mV);
                            if (!(lbXValid && lbYValid && lbZValid && lbWValid))
                            {
                                CgsDev::Assert::BeginAssert();
                                CgsDev::Assert::FireAssert("rw::math::IsValid(lWheelMatrix)",
                                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp", 3856);
                                CgsDev::Assert::EndAssert();
                            }
                            luWheelCount = static_cast<u32>(giWheelsToRender);
                            lWheelStack.maMatrices[liCount] = lWheel;
                            lWheelStack.maConstants[liCount] = lWheelConstants;
                            lapMatrices[liCount] = &lWheelStack.maMatrices[liCount];
                            ++liCount;
                        }
                        ++luWheel;
                    } while (static_cast<u32>(luWheel) < luWheelCount);
                }
                if (liCount > 0)
                {
                    CGS_ASSERT(lpWheelModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER),
                               "lpWheelModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER)");
                    const CgsGraphics::Renderable* lpRenderable = lpWheelModel->GetRenderable(leLOD);
                    CGS_ASSERT(lpRenderable != 0, "lpRenderable");
                    CgsGraphics::DispatchList* lpList = lpDispatchFrame->GetList(liObjectList);
                    CGS_ASSERT(lpList != 0, "lpDispatchList");
                    CGS_ASSERT(lpWheelModel->GetFlag(CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER),
                               "lbInstancing == GetFlag( CgsGraphics::Model::E_FLAG_MODEL_USES_INSTANCE_SHADER )");
                    CgsGraphics::Model::SetupShaderConstantsForInstancing(liCount, lapMatrices, lWheelStack.maConstants);
                    const bool lbFirst = (lpList->GetCount() & 0x7F) == 0;
                    lpDispatchFrame->GetBin().BeginPacket();
                    CgsGraphics::DrawRenderable::AddToBin(lpRenderable, lpDispatchFrame, lbFirst,
                        static_cast<s8>(liOpaqueMeshList), static_cast<s8>(liTransparentMeshList),
                        0, lu8Technique, lbShadowPass, 0xFFu, 0u, liCount, 0u);
                    lpList->Submit(0, lpDispatchFrame->GetBin().EndPacket());
                }
            }
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
        }
    }


}

// ARTIST 822E7B20 reads this writable byte, initialized to one.
bool gUseVisibleRaceCars = true;
// ARTIST CRT 82C4BCB8..BD1C constructs the three broadcast cull distances.
static Vector4 gavShadowCullDistances[3] = {
    Vector4(0.0f, 0.0f, 0.0f, 0.0f),
    Vector4(64.0f, 64.0f, 64.0f, 64.0f),
    Vector4(144.0f, 144.0f, 144.0f, 144.0f)
};

static rw::math::vpu::VecFloat RaceCarDistance(__vector4 lvCamera, __vector4 lvPosition,
                                               __vector4& lrSquared)
{
    const __vector4 lvDelta = __vsubfp(lvCamera, lvPosition);
    lrSquared = __vmsum3fp(lvDelta, lvDelta);
    const __vector4 lvZero = __vspltisw(0);
    const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
    const __vector4 lvHalf = __vcfsx(__vspltisw(1), 1);
    const __vector4 lvIsZero = __vcmpeqfp(lvZero, lrSquared);
    __vector4 lvReciprocal = __vrsqrtefp(lrSquared);
    for (s32 li = 0; li != 2; ++li)
    {
        const __vector4 lvReciprocalSquared = __vmulfp(lvReciprocal, lvReciprocal);
        const __vector4 lvHalfReciprocal = __vmulfp(lvReciprocal, lvHalf);
        lvReciprocal = __vmaddfp(lvHalfReciprocal,
                                 __vnmsubfp(lrSquared, lvReciprocalSquared, lvOne),
                                 lvReciprocal);
    }
    rw::math::vpu::VecFloat lDistance;
    lDistance.mV = __vsel(__vmulfp(lrSquared, lvReciprocal), lvZero, lvIsZero);
    return lDistance;
}

void RaceCarEntityModule::GenerateDispatchLists(
    RaceCarEntityModuleIO::InputBuffer_GenerateDispatchLists* lpInput,
    const Array<CgsSceneManager::EntityId, 32u>& lrVisibleEntities,
    s32 liObjectList, s32 liOpaqueMeshList, s32 liTransparentMeshList,
    bool lbEnvironmentMapPass, Vector4 lvFogScattering,
    Vector4 lvFogColourPlusWhiteLevel, Vector3 lvCameraPosition)
{
    CGS_ASSERT(lpInput != 0, "lpInput != NULL");
    lpInput->LockForRead();
    BrnBlobbyShadowManager::BrnBlobbyShadowBuffer* lpBlobby = lpInput->GetBlobbyShadowBuffer();
    CGS_ASSERT(lpBlobby != 0, "lpBlobbyShadowRenderer");
    BrnCoronaManager::BrnSubmissionInterface* lpCorona = lpInput->GetCoronaSubmissionInterface();
    CGS_ASSERT(lpCorona != 0, "lpCoronaSubmissionInterface");
    CgsGraphics::DispatchFrame* lpDispatchFrame = lpInput->GetDispatchFrame();
    ShadowMap* lpShadowMap = lpInput->GetShadowMap();
    CGS_ASSERT(lpShadowMap != 0, "lpShadowMap");

    if (gUseVisibleRaceCars)
    {
        for (s32 liVisible = 0; liVisible < lrVisibleEntities.GetCount(); ++liVisible)
        {
            const CgsSceneManager::EntityId lEntity = lrVisibleEntities.GetItem(liVisible);
            const s32 liCar = lEntity.GetEntityIndex();
            if (mbInReplay)
            {
                if (!mReplaySerialiser.IsPlaying() || lEntity.GetOwner() != 33)
                    continue;
                if (lpInput->GetCameraInput()->GetState().IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_HIDE_PLAYER)
                    && liCar == static_cast<u16>(mePlayerActiveRaceCarIndex))
                    continue;
                if (!mRaceCarStreamer.IsRaceCarActive(liCar) || !mRaceCarStreamer.IsRaceCarLoaded(liCar))
                    continue;
                const EActiveRaceCarIndex leIndex = static_cast<EActiveRaceCarIndex>(liCar);
                BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrActive =
                    mReplaySerialiser.GetStaticLayout()->mLiveFrame.mActiveRaceCars;
                if (!lrActive.IsRaceCarActive(leIndex))
                    continue;
                const BrnPhysics::Vehicle::RaceCarState* lpState = lrActive.GetRaceCarStateMutable(leIndex);
                __vector4 lvSquared;
                const rw::math::vpu::VecFloat lDistance = RaceCarDistance(lvCameraPosition.mV, lpState->mTransform.wAxis.mV, lvSquared);
                if (lbEnvironmentMapPass)
                {
                    const __vector4 lvCull = gavShadowCullDistances[lpShadowMap->GetCurrentShadowMap()].mV;
                    unsigned int luCompare;
                    __vcmpgtfpR(lvCull, lvSquared, &luCompare);
                    if ((luCompare & 0x80u) != 0) continue;
                }
                CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec> lCarGraphics(mRaceCarStreamer.GetGraphicsResource(liCar).GetResourceHandle());
                CgsResource::ResourcePtr<BrnWheel::GraphicsSpec> lWheelGraphics(mRaceCarStreamer.GetWheelGraphicsResource(liCar).GetResourceHandle());
                const ActiveRaceCar::RenderParams* lpParams = &mReplaySerialiser.GetStaticLayout()->mLiveFrame.maRenderParams[liCar];
                RenderRaceCar(lpDispatchFrame, lDistance, lvFogScattering, lvFogColourPlusWhiteLevel,
                              lpParams, lCarGraphics, lWheelGraphics, liObjectList, liOpaqueMeshList,
                              liTransparentMeshList, lpShadowMap, true);
                mabRaceCarsRendered[liCar] = true;
                continue;
            }

            ActiveRaceCar& lrCar = maActiveRaceCars[liCar];
            const Matrix44Affine lTransform = lrCar.GetTransform();
            __vector4 lvSquared;
            const rw::math::vpu::VecFloat lDistance = RaceCarDistance(lvCameraPosition.mV, lTransform.wAxis.mV, lvSquared);
            if (lbEnvironmentMapPass)
            {
                const __vector4 lvCull = gavShadowCullDistances[lpShadowMap->GetCurrentShadowMap()].mV;
                unsigned int luCompare;
                __vcmpgtfpR(lvCull, lvSquared, &luCompare);
                if ((luCompare & 0x80u) != 0) continue;
            }
            CGS_ASSERT(lEntity.GetOwner() == 1, "RaceCarEntityModule trying to render something that isn't a RaceCar");
            const bool lbAttached = !(lpInput->GetCameraInput()->GetState().IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_HIDE_PLAYER)
                && liCar == static_cast<u16>(mePlayerActiveRaceCarIndex));
            if (!lrCar.IsActive() || !lrCar.ShouldRenderThisFrame()) continue;
            const RaceCarStreamer::GraphicsResourcePtr& lrGraphics = mRaceCarStreamer.GetGraphicsResource(liCar);
            const RaceCarStreamer::WheelGraphicsResourcePtr& lrWheels = mRaceCarStreamer.GetWheelGraphicsResource(liCar);
            ActiveRaceCar::RenderParams* lpParams = lrCar.GetRenderParams();
            bool lbRender = true;
            if (lpShadowMap->IsRenderingShadowMap())
            {
                lpParams->SetLOD(lpShadowMap->CalcOptimisedLod(lpParams->GetLOD(), CgsGraphics::Model::E_STATE_LOD_4));
                if (!lrCar.IsPlayer())
                    lbRender = lpInput->GetCameraInput()->GetState().IsFlagSet(BrnDirector::Camera::CameraState::E_FLAG_SHOWING_ONLINE_INTRO);
            }
            if (lbRender)
                RenderRaceCar(lpDispatchFrame, lDistance, lvFogScattering, lvFogColourPlusWhiteLevel,
                              lpParams, lrGraphics, lrWheels, liObjectList, liOpaqueMeshList,
                              liTransparentMeshList, lpShadowMap, lbAttached);
            if (lpShadowMap->IsRenderingShadowMap() || !lbAttached || !lrCar.IsActive()
                || !lrCar.ShouldRenderThisFrame() || lpParams->IsRaceCarHidden()) continue;

            BrnPhysics::Vehicle::RaceCarState* lpPhysics = lrCar.GetPhysicsState();
            const RaceCarStreamer::PhysicsResourcePtr& lrPhysics = mRaceCarStreamer.GetPhysicsResource(liCar);
            // ARTIST's vperm masks 82CDADE0 / 82CDADF0 select bbox z and axle z lanes bitwise.
            static const __declspec(align(16)) unsigned char abAxleMask[16] = {8,9,10,11,24,25,26,27,0,1,2,3,0,1,2,3};
            static const __declspec(align(16)) unsigned char abBoxMask[16] = {0,1,2,3,0,1,2,3,8,9,10,11,24,25,26,27};
            const __vector4 lvBox = __vperm(lrCar.mDeformedBBox.mMax.mV, lrCar.mDeformedBBox.mMin.mV, __lvx(abBoxMask, 0));
            const __vector4 lvAxles = __vperm(lpParams->GetAxlePosition(3)->mV, lpParams->GetAxlePosition(0)->mV, __lvx(abAxleMask, 0));
            Vector4 lExtents;
            lExtents.mV = __vsldoi(lvBox, lvAxles, 8);
            const BrnPhysics::Deformation::StreamedDeformationSpec* lpSpec = lrPhysics.operator->();
            CGS_ASSERT(0.1f > lpSpec->mCollisionOffset.mV.x, "Vehicle has asymmetric bounding box round Z");
            const __vector4 lvNormal = lpPhysics->mAboveGroundTestResult.mIntersectionNormal.mV;
            const __vector4 lvNormalPerm = __vpermwi(lvNormal, 0x63);
            lpSpec = lrPhysics.operator->();
            const __vector4 lvBodyForward = lpParams->GetBodyTransform().zAxis.mV;
            const __vector4 lvRight = __vpermwi(__vnmsubfp(__vpermwi(lvBodyForward, 0x63),
                                             lvNormal, __vmulfp(lvBodyForward, lvNormalPerm)), 0x63);
            Matrix44Affine lShadowTransform;
            lShadowTransform.xAxis.mV = lvRight;
            lShadowTransform.yAxis.mV = lvNormal;
            lShadowTransform.zAxis.mV = __vpermwi(__vnmsubfp(lvNormalPerm, lvRight,
                __vmulfp(lvNormal, __vpermwi(lvRight, 0x63))), 0x63);
            lShadowTransform.wAxis.mV = lpPhysics->mAboveGroundTestResult.mIntersectionPosition.mV;
            rw::math::vpu::VecFloat lHeight;
            lHeight.mV = __vsubfp(__vspltw(__lvlx(&lpPhysics->mAboveGroundTestResult.mfVerticalDistance, 0), 0),
                                   __vspltw(lpSpec->mRigidBodyOffset.mV, 1));
            lpSpec = lrPhysics.operator->();
            rw::math::vpu::VecFloat lWidth;
            lWidth.mV = __vspltw(lpSpec->mHandlingBodyDimensions.mV, 0);
            lpBlobby->AddShadow(lShadowTransform, lExtents, lWidth, lHeight);
            if (mbRenderRaceCarCoronas && !(lpParams->GetDeformationSquared() >= KF_LIGHTS_OUT_DEFORMATION_SQUARED)
                && !lpPhysics->mbCrashing && !lpParams->IsEngineOff() && !lpParams->IsRaceCarHidden())
                SubmitCoronasForRaceCar(lpCorona, lrPhysics, lpParams,
                                       static_cast<u16>(mePlayerActiveRaceCarIndex) == liCar);
        }
    }
    else
    {
        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            ActiveRaceCar& lrCar = maActiveRaceCars[liCar];
            if (!lrCar.IsActive() || !lrCar.ShouldRenderThisFrame()) continue;
            const Matrix44Affine lTransform = lrCar.GetTransform();
            __vector4 lvSquared;
            const rw::math::vpu::VecFloat lDistance = RaceCarDistance(lvCameraPosition.mV, lTransform.wAxis.mV, lvSquared);
            CgsResource::ResourcePtr<BrnVehicle::GraphicsSpec> lCarGraphics(mRaceCarStreamer.GetGraphicsResource(liCar).GetResourceHandle());
            CgsResource::ResourcePtr<BrnWheel::GraphicsSpec> lWheelGraphics(mRaceCarStreamer.GetWheelGraphicsResource(liCar).GetResourceHandle());
            RenderRaceCar(lpDispatchFrame, lDistance, lvFogScattering, lvFogColourPlusWhiteLevel,
                          lrCar.GetRenderParams(), lCarGraphics, lWheelGraphics, liObjectList,
                          liOpaqueMeshList, liTransparentMeshList, lpShadowMap, true);
        }
    }
    lpInput->UnlockForRead();
}

}
