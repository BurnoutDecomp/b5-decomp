// WHOLE-FILE REPLACEMENT of the committed file (which bodied only the ctor, against a
// misreconstructed class -- see BrnPostFxShader.h's banner).
#include "GameSource/Graphics/PostFx/BrnPostFxShader.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"           // shadow::Device
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"        // BlendMaterialState / BlendStateParameters
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"         // ResourceAllocatorCreate
#include "rw/math/vpu/matrix44affine_operation.h"                               // vpu::Inverse(Matrix44Affine) / Mult(affine,affine)
#include "rw/math/vpu/matrix44_operation.h"                                     // vpu::Mult(Matrix44Affine, Matrix44)
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"        // fpu::Matrix44_64 + Inverse/Mult/Subtract

#include <cmath>   // powf -- the vignette gradient exponent (see KF_VIGNETTE_GRADIENT_POWER)

// ==================================================================================================
// BrnPostFxShader -- THE POST-FX COMPOSITE.
//
// SOURCE OF TRUTH: the X360 ARTIST asm is the spine (Construct @0x823FDD18, Destruct @0x823FE3F0,
// Shader::Construct @0x823FD970, Shader::Destruct @0x823FDB08, Shader::SetProgram @0x823FDBB0,
// Render @0x82408F08). The DecFIGS DWARF supplies every declaration shape, every parameter name and
// every local name (references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/BrnPostFxShader.cpp).
// The twenty-four microcode packages and their interned constant names are dumped byte-exact in
// scratch/postfx_wave1b_dossiers/DATA_DUMP.md.
//
// --------------------------------------------------------------------------------------------------
// WHAT IS LIVE AND WHAT IS GATED, AND WHY. Read this before flipping anything.
//
// The CONSTANT MATHS is unconditional: it is pure, it touches no device state, and every value in it
// comes from an assembly immediate or a caller argument. It is compiled and executed on every path.
//
// Three things this composite needs do not exist in this tree yet. Each gets its own gate, named for
// its blocker rather than for a wave, and each gate's OFF arm is a LOUD one-shot report, never a
// silent fallback:
//
//   BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE   -- Construct / Destruct / Shader::Construct /
//        Shader::Destruct. Needs (a) programs in a form the PC can bind -- the X360 embeds Xenos
//        MICROCODE, the PC needs a transcoded D3D9 pair per permutation. ALL TWELVE PAIRS NOW EXIST
//        (post-fx step 5): pc/gcm/renderengine/PostFxProgramsPC.cpp carries ONE shared vertex image
//        -- the twelve X360 vertex packages are 596 bytes each and BYTE-IDENTICAL, md5
//        a47e7e9943a3570c484e1724d6dff763 for all twelve, so there is one vertex program and not
//        twelve -- plus TWELVE pixel images, each decoded from its own Xenos package and each
//        reproducing that permutation's CTAB register/type/count surface name for name. All are
//        wrapped as platform-4 ShaderProgramBuffer images and adopted through
//        renderengine::ProgramBufferPC_Adopt. NO SLOT IS CONSTRUCTED EMPTY ANY MORE; the empty-slot
//        arm in Shader::Construct survives only as the guard against a null argument.
//        Still missing for this gate: (b) renderengine::SamplerState,
//        renderengine::DepthStencilState and renderengine::VertexDescriptor::Release, whose homes
//        (states/samplerstate.cpp, pc/gcm/renderengine/DepthStencilState.cpp,
//        pc/gcm/renderengine/VertexDescriptor.cpp) are not on the exe build list; and (c)
//        PostFxProgramsPC.cpp itself, which is not on that list either.
//
//   BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE -- ⭐ CLOSED 2026-08-15 (post-fx step-6 producers
//        wave), now 1. It gated the four out-of-line rw::math::fpu::Matrix44Template<double> calls
//        Render makes to build the camera-reprojection matrix, whose three outputs are BlurMatrixX /
//        BlurMatrixY / BlurMatrixW. Those four callees (Inverse @0x82405210, Mult(M,M) @0x82405690,
//        Subtract @0x82405780, Mult(M,scalar) @0x824058A0) had no home in this tree; the double
//        4x4 family now lives with the rest of the rw::math::fpu template vocabulary in
//        GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h (which already homed
//        Vector2/3/4Template and Matrix33Template -- see that file's ADDITIVE GROW banner for why
//        it is that file and not a new one).
//        WHAT THE GATE'S OFF ARM USED TO COST, kept here because it is the shape of the bug the
//        flip removes: with the block missing, slots 4..11 -- which DO intern BlurMatrixX/Y/W --
//        uploaded three ZERO float4s, the pixel program's screen velocity `(row.xy - row.z * uv)`
//        came out exactly (0,0), both tex2Dgrad gradients were zero, and motion blur was SILENTLY
//        ABSENT rather than wrong. Permutation 0 was never affected either way: its pixel package
//        (X360 0x8203F888) interns no BlurMatrix* at all (DATA_DUMP.md, "slot dword 0"), so those
//        handles carry register count 0 and SetProgram skips them.
//
//   BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE    -- the whole DEVICE half of Render: the state / texture /
//        sampler binds and the full-screen quad. Needs (a) BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE to be
//        1 as well, so that slot 0 actually holds the adopted PC pair -- without a bound program
//        SetProgram binds nothing and the quad would rasterise with whatever the previous draw left
//        installed, i.e. a WRONG frame; (b) shadow::Device::SetState(const renderengine::TextureState*,
//        u32) -- X360 0x8227D158, named by the DWARF's own recorded call list for this function --
//        which is neither declared nor defined in the tree; and (c) D3DDevice_BeginVertices /
//        D3DDevice_EndVertices, which are DECLARED in three TUs and DEFINED IN NONE.
//
//
// --------------------------------------------------------------------------------------------------
// WHY THE INDEX IS 0 ON THIS BUILD, FROM THE CODE RATHER THAN FROM HOPE.
//   leShader = 4*(meQuality+1 if lbEnableMotionBlur) | 2*lbEnableDof | lbEnableTint3d.
//   All three flags reach this function from BrnPostFx::Render @0x8240A468, which computes each as
//   `(m_enabledFx & <bit>) != 0 && lpOverrideSourceTexture == nullptr` (@0x8240A670-0x8240A6E8) or
//   from its own lbMotionBlurEnabled argument (@0x8240A650).
//   `m_enabledFx` is written in exactly ONE place in the whole tree -- `m_enabledFx = 0;` in
//   BrnPostFx::BrnPostFx (BrnPostFx.cpp:29). Its five mutators (SetDepthOfField / SetBloom /
//   SetVignette / SetTint / SetB4Blur, BrnPostFx.h:73-77) are DECLARATION-ONLY: no definition exists.
//   Run for yourself: `grep -rn "m_enabledFx" b5-decomp/src` returns 3 lines -- BrnPostFx.cpp:29 (the
//   assignment), BrnPostFx.h:41 (a comment) and BrnPostFx.h:114 (the declaration).
//   So every bit is permanently clear and leShader is 0 every frame. WHAT WOULD CHANGE IT: bodying
//   any of those five mutators and having something call it, or BrnRendererModule::Render passing
//   lbMotionBlurEnabled = true. SINCE STEP 5 THAT IS NO LONGER A WALL: every one of the twelve slots
//   holds a real PC program pair, so any permutation this function selects can be drawn. What the
//   early-out below still refuses is a slot whose ADOPT FAILED -- an empty slot binds no program at
//   all, and rasterising a quad with whatever program the previous draw left installed is the one
//   failure mode worth a hard return.
// ==================================================================================================

#define BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE            1
#define BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE   1
#define BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE             1

namespace renderengine
{
    // One shader-constant upload: luNumRegisters float4s at register luRegister, on the vertex or the
    // pixel bank. DEFINED in the mounted pc/gcm/renderengine/XenonD3D9Shims.cpp:778; declared here
    // exactly as GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp:139 declares it -- that
    // TU is the existing precedent for this seam, which has no header yet.
    void WorldShaderConstants_Set(bool lbPixel, u32 luRegister, const void* lpData, u32 luNumRegisters);

    extern const u32 guPostFxCompositeVertexProgramPCSize;
              // slot  0
    extern const u32 guPostFxCompositePixelProgramPCSize;
        // slot  1
    extern const u32 guPostFxCompositeTint3dPixelProgramPCSize;
           // slot  2
    extern const u32 guPostFxCompositeDofPixelProgramPCSize;
     // slot  3
    extern const u32 guPostFxCompositeDofTint3dPixelProgramPCSize;
          // slot  4
    extern const u32 guPostFxCompositeBlurPixelProgramPCSize;
    // slot  5
    extern const u32 guPostFxCompositeBlurTint3dPixelProgramPCSize;
       // slot  6
    extern const u32 guPostFxCompositeBlurDofPixelProgramPCSize;
 // slot  7
    extern const u32 guPostFxCompositeBlurDofTint3dPixelProgramPCSize;
        // slot  8
    extern const u32 guPostFxCompositeBlurHqPixelProgramPCSize;
  // slot  9
    extern const u32 guPostFxCompositeBlurHqTint3dPixelProgramPCSize;
     // slot 10
    extern const u32 guPostFxCompositeBlurHqDofPixelProgramPCSize;
       // slot 11
    extern const u32 guPostFxCompositeBlurHqDofTint3dPixelProgramPCSize;
}

#if BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                          u32 luVertexCount, u32 luStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);
// (The TextureState bind, X360 0x8227D158, is the real shadow::Device::SetState(const TextureState*,
// u32) overload in shadowingdevice.h since the gate-flip wave -- the free-function seam that stood
// in for it here is gone.)
#endif  // BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE

namespace
{
    // --- the assembly immediates this file uses, each traced to its X360 constant ------------------
    const f32 KF_ZERO      =  0.0f;   // flt_82001CC0
    const f32 KF_ONE       =  1.0f;   // flt_82001C98
    const f32 KF_MINUS_ONE = -1.0f;   // flt_820037C8
    const f32 KF_HALF      =  0.5f;   // flt_82001DA0

    // The vignette gradient curve. Both NAMES are the DWARF's own locals (BrnPostFxShader.cpp:712 /
    // :713); both VALUES are DATA_DUMP entries.
    const f32 KF_VIGNETTE_GRADIENT_POWER = 16.0f;    // dbl_82046258 == 0x4030000000000000
    const f32 KF_VIGNETTE_GRADIENT_LIMIT = 500.0f;   // flt_8200A034 == 0x43FA0000

    // The composite quad: `li r4, 6` (PrimitiveType) / `li r5, 4` (VertexCount) / `li r6, 0x14`
    // (stride) @0x82409960. 6 is the triangle-strip type the other 2D quads in this tree pass; 20 is
    // three position floats plus two UV floats, which is exactly the DWARF's vertex iterator for this
    // file -- `typedef VertexIterator2<renderengine::VertexTypeFloat3, renderengine::VertexTypeFloat2>
    // PostFxVertexIterator` (BrnPostFxShader.cpp:112).
    //
    // ⚠ 6 IS A XENOS ENUM VALUE AND MUST BE TRANSLATED, NOT PASSED THROUGH. On the Xenos
    // D3DPRIMITIVETYPE 6 is TRIANGLESTRIP (and Font/CgsFontRenderer.cpp:190-191 already names 6
    // KU_PRIMITIVE_TRIANGLE_STRIP in this tree); on PC D3D9, 6 is D3DPT_TRIANGLEFAN and TRIANGLESTRIP is 5. The four vertices below
    // are emitted in strip order (BL, BR, TL, TR) and still cover the quad read as a fan -- but the
    // fan's second triangle (BL, TL, TR) has the OPPOSITE winding to its first, so with backface
    // culling on, a diagonal half of the screen simply is not drawn. Keeping the console's 6 here is
    // translation, and this comment is where that requirement is written down.
    const u32 KU_COMPOSITE_PRIMITIVE_TYPE = 6;
    const u32 KU_COMPOSITE_VERTEX_COUNT   = 4;
    const u32 KU_COMPOSITE_VERTEX_STRIDE  = 20;

    // The D3D sampler units the composite binds, in the order the asm binds them. RECOVERED TWICE:
    // from the `li r4, N` immediates below, and independently from the sampler REGISTER assignments
    // in the microcode constant tables themselves (SamplerSource s0, SamplerBloom s1, SamplerDof s2,
    // Sampler3dTint s3, SamplerDepth s4 -- identical in every one of the twelve packages that
    // declares them; work/ctab_decode.py, work/ctab_register_map.txt).
    const u32 KU_SAMPLER_SOURCE  = 0;   // `li r4, 0` -> D3DDevice_SetTexture(dev, 0, ...) @0x824097E4
    const u32 KU_SAMPLER_BLOOM   = 1;   // `li r4, 1` @0x82409878
    const u32 KU_SAMPLER_DOF     = 2;   // `li r4, 2` @0x824098DC
    const u32 KU_SAMPLER_TINT_3D = 3;   // `li r4, 3` @0x82409924
    const u32 KU_SAMPLER_DEPTH   = 4;   // `li r4, 4` @0x82409930

    // ⚠ FOR WHOEVER WRITES THE PERMUTATION-0 HLSL: this is the shader's OWN register map, decoded
    // byte-exact out of the constant table interned in the X360 pixel package at 0x8203F888
    // (696 bytes, DATA_DUMP.md "slot dword 0"). It is what the PC transcode must reproduce, because
    // GetVariableHandleByName resolves these names against the PC program buffer and SetProgram
    // uploads to whatever register the buffer reports:
    //     GlobalParams c0 | BloomColour c1 | VignetteInnerRgbPlusMul c2 | VignetteOuterRgbPlusAdd c3
    //     Tint2dColour c4 | SamplerSource s0 | SamplerBloom s1
    // Permutation 0 declares NO Sampler3dTint, NO DofParamsA/B/SamplerDof, NO BlurMatrixX/Y/W and NO
    // SamplerDepth. Note that a name's register is NOT stable across permutations -- BloomColour is
    // c1 here and c3 in every DoF permutation -- which is exactly why the console looks each name up
    // instead of hard-coding a register, and why this file must never hard-code one either.

    // The interned constant names, in the order Shader::Construct walks the two static name tables.
    // The DWARF declares them (`extern const char*[2] gaacVertexVariableNames` /
    // `[10] gaacPixelVariableNames`, BrnPostFxShader.h:56/59); they are file-scope here because
    // nothing outside this TU reads them. Every string is ASCII interned in the microcode packages
    // themselves, so the SET is RECOVERED byte-exact (DATA_DUMP.md).
    //
    // ⚠ RECOVERED vs INFERRED, per BrnPostFxShader.h's enum banner -- read it before reordering
    // anything here. RECOVERED: both vertex entries, and pixel index 0 (IDA named each table after
    // the string its first element points at, and the vertex table has only two entries).
    // INFERRED: pixel indices 1..9. Two independent lines agree on this sequence -- which value the
    // asm stores in each of the ten stack slots, and the c0..c9 register order the full permutation's
    // CTAB assigns (work/ctab_decode.py) -- but the ten .rdata pointers themselves were never dumped.
    // BLOCKED item in REPORT.md; harmless in permutation 0, where five of these names are not
    // interned at all and their handles carry register count 0.
    const char* const gaacVertexVariableNames[BrnPostFxShader::E_VS_VAR_COUNT] =
    {
        "VignetteCentreXyScaleXy",
        "VignetteAngle",
    };

    const char* const gaacPixelVariableNames[BrnPostFxShader::E_PS_VAR_COUNT] =
    {
        "GlobalParams",
        "DofParamsA",
        "DofParamsB",
        "BloomColour",
        "VignetteInnerRgbPlusMul",
        "VignetteOuterRgbPlusAdd",
        "Tint2dColour",
        "BlurMatrixX",
        "BlurMatrixY",
        "BlurMatrixW",
    };

    struct PostFxVertex
    {
        f32 mfX, mfY, mfZ;   // renderengine::VertexTypeFloat3
        f32 mfU, mfV;        // renderengine::VertexTypeFloat2
    };

    // Multiply a Vector4 lane by lane. This tree's rw::math::vpu::Vector4 is the portable
    // reconstruction of the console's single 16-byte VectorIntrinsic and carries x/y/z/w plus
    // SetZero() and nothing else -- the SIMD operators live in the SDK's *_operation headers, which
    // that vendor header's own banner says are deliberately not reproduced. The console's `vmulfp128`
    // is one instruction; four named lanes is the same four floats in the same order.
    rw::math::vpu::Vector4 MultiplyPerLane(const rw::math::vpu::Vector4& lrA,
                                           const rw::math::vpu::Vector4& lrB)
    {
        rw::math::vpu::Vector4 lResult;
        lResult.x = lrA.x * lrB.x;
        lResult.y = lrA.y * lrB.y;
        lResult.z = lrA.z * lrB.z;
        lResult.w = lrA.w * lrB.w;
        return lResult;
    }

    // Project one depth-of-field focal plane into the [0,1] normalised device depth the pixel program
    // compares against, and clamp it. The X360 open-codes this four times over lDofState's four focal
    // planes (@0x82409030-0x824090D8) and the DWARF records four rw::math::vpu::Clamp calls in that
    // block. The zero test is the console's own (`fcmpu cr6, f13, f28` against 0.0f, taking the
    // constant-0 branch), not a defensive addition: an unset plane must project to 0, not divide by 0.
    // The two fsel pairs are the clamp, in this order: `fsel f13, -f13, 0.0f, f13` drives anything at
    // or below zero to zero, then `fsel out, 1.0f - f13, f13, 1.0f` caps at one.
    f32 ProjectAndClampFocalPlane(f32 lfPlane, f32 lfZProjScale, f32 lfWProjOffset)
    {
        if (lfPlane == KF_ZERO)
        {
            return KF_ZERO;
        }

        const f32 lfProjected  = ((lfPlane * lfZProjScale) + lfWProjOffset) / lfPlane;
        const f32 lfLowClamped = (lfProjected <= KF_ZERO) ? KF_ZERO : lfProjected;
        return (lfLowClamped >= KF_ONE) ? KF_ONE : lfLowClamped;
    }

    // Narrow one COLUMN of the double-precision reprojection matrix into the single-precision
    // shader constant. The console does exactly this, lane by lane: `lfd` the double, `frsp` it,
    // `stfs` it into the float4 (0x824095D0-0x82409684, twelve such triples for the three
    // constants). static_cast<f32> IS frsp -- round the double to single, once.
    rw::math::vpu::Vector4 NarrowToVector4(const rw::math::fpu::Vector4_64& lrColumn)
    {
        rw::math::vpu::Vector4 lResult;
        lResult.x = static_cast<f32>(lrColumn.X());
        lResult.y = static_cast<f32>(lrColumn.Y());
        lResult.z = static_cast<f32>(lrColumn.Z());
        lResult.w = static_cast<f32>(lrColumn.W());
        return lResult;
    }

    // Report a disclosed bring-up condition exactly once. A composite that silently does nothing, or
    // silently draws the wrong permutation, is the failure mode this whole file is written to avoid.
    void ReportOnce(bool& lrbAlreadyReported, const char* lpcText)
    {
        if (!lrbAlreadyReported)
        {
            lrbAlreadyReported = true;
            CgsDev::Log::WriteToLog(lpcText);
        }
    }

#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
    // ----------------------------------------------------------------------------------------------
    // THE TWENTY-FOUR CONSOLE PROGRAM PACKAGES -- A DOCUMENTATION TABLE, NOT DECLARATIONS.
    //
    // On the X360 these are Xenos MICROCODE packages embedded in the guest image at the addresses
    // below; each carries a CTAB-style variable table whose interned ASCII names are what
    // GetVariableHandleByName resolves against (all twenty-four dumped byte-exact in DATA_DUMP.md).
    // The names are the ORIGINAL permutation names, from the DecFIGS build's compiled-shader symbols
    // (dwarfdump/GameProjects/.../CompiledShaders/BrnPostFx*_{VS,PS}.h). Every vertex package is
    // 0x254 == 596 bytes (`li r6, 0x254`, identical at all twelve call sites); the per-permutation
    // pixel sizes are the `li r8` immediates.
    //
    // ⚠ THESE ARE COMMENTS ON PURPOSE. An earlier revision declared all twenty-four as
    // `extern "C" const u8 gacBrnPostFx*_{VS,PS}[]` -- twenty-four symbols that NO TU in this tree
    // defines, i.e. twenty-four guaranteed LNK2019s the per-TU `cl /c` gate cannot see. A PC build
    // cannot bind Xenos microcode anyway: each pair has to be transcoded to a D3D9 vertex/pixel
    // program, wrapped into a platform-4 ShaderProgramBuffer image and adopted through
    // renderengine::ProgramBufferPC_Adopt. ALL TWELVE now exist -- one shared vertex image plus
    // twelve pixel images, declared at the top of this file. The twelve VERTEX addresses below are
    // twelve addresses of the same 596 bytes (byte-identical, md5
    // a47e7e9943a3570c484e1724d6dff763), which is why the PC side has one vertex program.
    //
    //   slot  permutation (E_SHADER_*)                     X360 VS      X360 PS      PS bytes
    //    0    BLOOM_VIGNETTE_TINT2D                        0x8203F630   0x8203F888    696
    //    1    BLOOM_TINT3D_VIGNETTE_TINT2D                 0x8203F0B8   0x8203F310    796
    //    2    DOF_BLOOM_VIGNETTE_TINT2D                    0x82040180   0x820403D8    928
    //    3    DOF_BLOOM_TINT3D_VIGNETTE_TINT2D             0x8203FB40   0x8203FD98    996
    //    4    BLUR_BLOOM_VIGNETTE_TINT2D                   0x82040E48   0x820410A0   1076
    //    5    BLUR_BLOOM_TINT3D_VIGNETTE_TINT2D            0x82040778   0x820409D0   1140
    //    6    BLUR_DOF_BLOOM_VIGNETTE_TINT2D               0x82041C50   0x82041EA8   1240
    //    7    BLUR_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D        0x820414D8   0x82041730   1308
    //    8    BLURHQ_BLOOM_VIGNETTE_TINT2D                 0x82042A50   0x82042CA8   1076
    //    9    BLURHQ_BLOOM_TINT3D_VIGNETTE_TINT2D          0x82042380   0x820425D8   1140
    //   10    BLURHQ_DOF_BLOOM_VIGNETTE_TINT2D             0x82043858   0x82043AB0   1240
    //   11    BLURHQ_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D      0x820430E0   0x82043338   1308
    //
    // (The twelve Construct call sites below carry the same addresses and sizes per line, so the
    // console's emission order and its immediates stay on the page next to the code they came from.)
    // ----------------------------------------------------------------------------------------------

    const u32 KU_SAMPLER_ADDRESS_CLAMP = 2;   // `li r28, 2` -> the three address words
    const u32 KU_SAMPLER_FILTER_POINT  = 0;
    const u32 KU_SAMPLER_FILTER_LINEAR = 1;   // `li r26, 1`
    const u32 KU_SAMPLER_FILTER_ANISO  = 4;   // `li r22, 4` -> `stw r22, var_4D4 / var_4D0`
    const u32 KU_SAMPLER_MIP_FILTER    = 2;   // constant across all four samplers

    // The MAX ANISOTROPY Construct writes into mapSamplerState_MotionBlur[quality]'s muConvolution:
    // `luMaxAnisotropy = 4` before the loop and `luMaxAnisotropy += 12` at the end of every
    // iteration (the X360 `addi r27, r27, 0xC`), i.e. 4 for E_QUALITY_CHEAP and 16 for
    // E_QUALITY_EXPENSIVE. Written as that recurrence's closed form over the SAME quality index the
    // bind site uses to pick the sampler, so the value pushed to the device cannot drift from the
    // value inside the sampler object. It is also 1 / the pixel programs' sub-tap jitter constant
    // (0.25 for BLUR, 0.0625 for BLURHQ -- tools/assets/shaders/brn_postfx_composite.fx), which is
    // what keeps the CPU side and the shader side consistent by construction.
    u32 MotionBlurSamplerMaxAnisotropy(u32 luQuality)
    {
        return 4u + 12u * luQuality;
    }
#endif  // BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
}

#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
namespace renderengine
{
    // renderengine::SamplerState's parameter block and the two entry points Construct needs. Its
    // GetResourceDescriptor @0x82B63588 / Initialize @0x82B62630 are reconstructed in
    // SDKs/RenderEngineClub/MAIN/components/src/states/samplerstate.cpp, which has NO HEADER and is
    // NOT on the exe build list. Declared here the same way the two other mounted consumers already
    // declare their copies (GameSource/Graphics/ImmediateMode/BrnIm3d.cpp:81 and
    // GameSource/Graphics/BrnRendererMemory.cpp:61).
    //
    // ⚠ THIS BLOCK IS DELIBERATELY IN `namespace renderengine`, NOT IN AN ANONYMOUS ONE, AND THE TWO
    // SIGNATURES ARE COPIED VERBATIM FROM samplerstate.cpp. MSVC mangles the parameter type into the
    // function's decorated name, so a copy of this struct in an anonymous namespace -- or an
    // Initialize spelled over `SamplerState*` instead of `SamplerStateData*` -- produces a symbol NO
    // TU CAN EVER DEFINE. That is the exact defect ShadowPass.h exists to document; it was
    // caught here by dumping this object's undefined externals rather than by reading.
    //
    // ⚠ THREE OF THE FIELD NAMES BELOW ARE WRONG, and the evidence is in Construct. samplerstate.cpp
    // calls +0x0C/+0x10 `muBias`/`muMaxLevel` and +0x28 `muConvolution`, but Construct writes
    // (0,0) / (1,1) / (4,4) into the first pair for the POINT / LINEAR / ANISOTROPIC samplers and
    // 1 / 1 / (4 or 16) into the second -- i.e. they are MinFilter, MagFilter and MaxAnisotropy. The
    // DWARF agrees: Construct's recorded setters are SetAddressU/V/W, SetMinMipLevel, SetMaxMipLevel,
    // SetMinFilter, SetMagFilter, SetMipFilter, SetMaxAnisotropy. Renaming them is a mechanical pass
    // across samplerstate.cpp + texturestate.cpp + BrnIm3d.cpp + BrnRendererMemory.cpp and is NOT done
    // here; the assignments in Construct write the same BYTES through the tree's CURRENT names and
    // say, per line, which field each one really is.
    struct SamplerStateData;   // the 0x20-byte runtime block; opaque to this TU

    struct SamplerStateParameters
    {
        u32 muAddressU;
        u32 muAddressV;
        u32 muAddressW;
        u32 muBias;                   // really: MIN filter
        u32 muMaxLevel;               // really: MAG filter
        u32 muMinLevel;               // really: MIP filter
        u32 muMaxAnisotropy;
        u32 muMagFilter;
        u32 muMinFilter;
        u32 muMipFilter;
        u32 muConvolution;            // really: MAX ANISOTROPY
        u32 muColor;
        u32 muRemap;
        u32 muDepthTextureFunction;
        u32 muGamma;
        u32 muState17;
        u8  mbState15;
        u8  mbState22;
        u8  mbState16;
        u8  mbState11Zero;
        u8  mbState26;
    };

    class SamplerState
    {
    public:
        static ResourceDescriptorEntry* GetResourceDescriptor(ResourceDescriptorEntry* lpDescriptor);
        static SamplerStateData* Initialize(SamplerStateData** lppState,
                                            const SamplerStateParameters* lpParams);
    };
}
#endif

// ==================================================================================================
// MotionBlurState::Construct
//
// DWARF HOME: references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/BrnPostFxShader.cpp:5, whose
// own source-line comment reads "// BrnPostFxShader.cpp:117" -- i.e. THIS file, not a camera TU.
// (The step-2 driver report guessed "the camera side"; the DWARF puts the definition here, next to
// MotionBlurState::Update at BrnPostFxShader.cpp:130, and this file already owns the type's
// declaration.)
//
// NO STANDALONE X360 SYMBOL. A name scan of all 30,095 exports in
// .ida-exports/BURNOUT_X360_ARTIST.XEX finds MotionBlurState::Update @0x823F8490 and NO
// MotionBlurState::Construct -- the console compiler INLINED it into its one caller,
// BrnPostFx::Construct @0x82409F80. So the body is recovered from the CALLER's assembly, and
// AGENTS.md's inlining-reversal rule is what puts it back as a call (BrnPostFx.cpp:386 already
// makes that call; this is the definition it has been missing).
//
// THE ASM, instruction for instruction (asm 0x8240A3D0-0x8240A448; r29 = the BrnPostFx `this`,
// r11 = r29 + 0x450 == this MotionBlurState sub-object, r10 = r11 + 0x40, and the three index
// registers are constants set far earlier in the function: r24 = 0x10 @0x82409F90, r25 = 0x20
// @0x8240A218, r23 = 0x30 @0x8240A0B0, r30 = 1):
//
//   0x8240A3F8  lvx128  v0, r0, r9    ; r9 = rw::math::vpu::detail::gIVector  @0x82181500 {1,0,0,0}
//   0x8240A400  stvx128 v0, r0, r11   ;   -> mCurrentWVP.xAxis   (+0x00)
//   0x8240A408  lvx128  v0, r0, r8    ; r8 = unk_82181510                     {0,1,0,0}
//   0x8240A410  stvx128 v0, r11, r24  ;   -> mCurrentWVP.yAxis   (+0x10)
//   0x8240A418  lvx128  v0, r0, r7    ; r7 = unk_82181520                     {0,0,1,0}
//   0x8240A41C  stvx128 v0, r11, r25  ;   -> mCurrentWVP.zAxis   (+0x20)
//   0x8240A420  lvx128  v0, r0, r6    ; r6 = unk_82181530                     {0,0,0,1}
//   0x8240A424  stvx128 v0, r11, r23  ;   -> mCurrentWVP.wAxis   (+0x30)
//   0x8240A428..0x8240A444             ; the SAME four vectors again, base r10 == r11 + 0x40
//                                      ;   -> mPreviousWVP.{x,y,z,w}Axis      (+0x40..+0x70)
//   0x8240A448  stw     r30, 0x80(r11) ;   -> meQuality = 1
//
// THE FOUR CONSTANTS ARE THE IDENTITY BASIS, DUMPED RATHER THAN ASSUMED.
// scratch/postfx_wave1b_dossiers/DATA_DUMP.md:1678 gives 0x82181510 = {0, 1.0f, 0, 0} with the two
// following 16-byte rows {0,0,1.0f,0} and {0,0,0,1.0f}; gIVector @0x82181500 = {1,0,0,0} is pinned
// in this tree at CameraUtils.cpp:128 and used the same way by BrnSkyDomeManager.cpp:507 and
// ExternalPhysicsBody.cpp:655. Two runs of those four rows over two 64-byte blocks IS
// rw::math::vpu::Matrix44::SetIdentity() twice -- which is precisely what the DWARF hint listing
// for this function shows, two SetIdentity calls and nothing else. The de-inlined call is therefore
// the reconstruction, not a paraphrase of it.
//
// WHY meQuality BELONGS TO THIS FUNCTION AND NOT TO ITS CALLER. Inlining erases the call boundary
// in the asm, so the asm cannot settle it -- but the DWARF can: mCurrentWVP, mPreviousWVP and
// meQuality are all under a `private:` section of MotionBlurState (dwarfdump BrnPostFxShader.h:66 /
// :67 / :68), so BrnPostFx, which is not a friend, could not have written +0x80 itself. The store
// is inside Construct. (A plain store is not a call, so its absence from the DWARF's call-only hint
// listing is not evidence either way.)
//
// E_QUALITY_EXPENSIVE, not "1": r30 is 1, and the DWARF enum list makes 1 == E_QUALITY_EXPENSIVE
// (BrnPostFxShader.h:44). Named, per AGENTS.md's logical-type-restoration rule.
//
// WHAT IS *NOT* TOUCHED, reproduced as written: maViewCache[3] (+0x84). The caller's asm writes
// nothing past +0x80, and Update is the only reader/writer of the cache. Zeroing it here would be
// behaviour the binary does not have.
// ==================================================================================================
void MotionBlurState::Construct()
{
    mCurrentWVP.SetIdentity();
    mPreviousWVP.SetIdentity();
    meQuality = E_QUALITY_EXPENSIVE;
}

// ==================================================================================================
// X360 0x823F8490 -- MotionBlurState::Update  (DWARF BrnPostFxShader.cpp:130)
//
// Advance the world-view-projection pair the composite's reprojection block differences. Called
// once a frame from BrnRendererModule::Render @0x8240BFA8 line 1251 with the read-locked
// ParticleRenderData's camera view (+0x60), its projection (+0xA0), its time step (+0x0C, in f1)
// and the base effects frame's mbIsExpensiveMotionBlur as the quality.
//
// THE WHOLE FUNCTION IS SINGLE-PRECISION VMX AND CONTAINS NO CALL AT ALL -- the four affine
// inverses and the three matrix products below are inlined. Its shape was recovered by executing
// the instruction stream (0x823F8508-0x823F8ACC) over symbolic inputs in a numeric simulator and
// matching the two stored matrices against candidate formulas; every claim here is that
// simulation's output, not a reading of the register allocation. The DWARF supplies the LOCAL
// NAMES (BrnPostFxShader.cpp:145-161) and they line up one-for-one with what the simulation found:
//   lOriginalViewCurr :145   lOriginalViewPrev :146    lOriginalCamTransCurr :149
//   lOriginalCamTransPrev :150   lCamTrans :151        lConstructedCamTransPrev :154
//   lLinearVelocity (Vector3) :157                     lConstructedViewPrev :161
// and the DWARF's own call tally for this function is FOUR rw::math::vpu::Inverse, TWO
// rw::math::vpu::operator*, THREE rw::math::vpu::Mult, TWO rw::math::vpu::operator-, ONE
// Matrix44Affine::SetW, THREE Matrix44Affine::operator= and FOUR Matrix44::operator= -- which is
// exactly the count of each in the two arms written below (4 inverses; 2 affine products; 3
// affine*projection products = 2 in the zero-dt arm + 1 in the other; 2 vector subtractions; the
// wAxis write; the 3-deep cache shift; and 2 + 2 matrix assignments).
//
// ⚠ THE BRANCH IS ON A ZERO TIME STEP, AND IT IS THE *ZERO* CASE THAT DOES THE WORK.
//   `lfs f0, flt_82001CC0 / fcmpu cr6, f31, f0 / bne cr6, loc_823F899C` @0x823F84E4-0x823F8504.
//   flt_82001CC0 is 0.0f (the same constant this file's KF_ZERO already names, and the one
//   BrnDirector::Camera::DepthOfField::SetParams @0x821F1AC8 compares its blurriness against).
//   So a NON-zero step takes the ordinary arm and a ZERO step takes the reconstruction arm. That is
//   the right way round: a zero step means the simulation did not advance, so shifting the history
//   would fill it with identical views and kill the blur on every duplicated frame. Instead the
//   camera's LINEAR VELOCITY is recovered from the two cached views and applied backwards to the
//   current camera transform, which keeps the blur alive across a duplicate frame.
//
// THE ZERO-STEP ARM, as the simulator pinned it (host-order names on the left, the guest
// addresses the values are read from on the right):
//   lOriginalViewCurr        = maViewCache[1]                       (this + 0xD0)
//   lOriginalViewPrev        = maViewCache[2]                       (this + 0x110)
//   lOriginalCamTransCurr    = Inverse(lOriginalViewCurr)           (det @0x823F85B0)
//   lOriginalCamTransPrev    = Inverse(lOriginalViewPrev)           (det @0x823F85F4)
//   lCamTrans                = Inverse(lView)                       (det @0x823F8678)
//   lConstructedCamTransPrev = lOriginalCamTransPrev * lOriginalViewCurr * lCamTrans
//   lLinearVelocity          = lOriginalCamTransCurr.W - lOriginalCamTransPrev.W
//   lConstructedCamTransPrev.SetW(lCamTrans.W - lLinearVelocity)
//   lConstructedViewPrev     = Inverse(lConstructedCamTransPrev)    (det @0x823F8844)
//   mCurrentWVP  = lView                * lProjection
//   mPreviousWVP = lConstructedViewPrev * lProjection
// The ROTATION half reads as "take the rotation the camera turned through between the two cached
// frames and re-apply it, backwards, to where the camera is now"; note it uses lOriginalViewCurr
// directly where Inverse(lOriginalCamTransCurr) would be the same matrix -- which is why there are
// four inverses and not five.
//
// THE NON-ZERO-STEP ARM (asm 0x823F899C-0x823F8ACC) is a plain history shift: the three cache
// entries move down one, the newest view goes in at [0], the previous WVP becomes what the current
// one was, and the current one is rebuilt. Note the cache is written but never READ in this arm --
// it exists only to feed the zero-step arm above.
//
// SIMULATION EVIDENCE (scratch/postfx_step6_producers/mbstate/work/vmx_sim.py +
// validate_maths.py): running the real instruction stream over four randomly-generated
// NON-orthonormal affines and a perspective projection reproduces both stored matrices to 1e-5
// against the formulas above; with maViewCache[1] == maViewCache[2] the two outputs collapse to
// each other, which is the degenerate case the formula predicts.
// ==================================================================================================
void MotionBlurState::Update(const rw::math::vpu::Matrix44Affine& lView,
                             const rw::math::vpu::Matrix44& lProjection,
                             f32 lfTimeStep,
                             EQuality leQuality)
{
    // 0x823F84B4-0x823F84E0 -- the console's own bounds assert, at the console's source line, with
    // the console's (deliberately inclusive) upper bound: `cmpwi r28, 0 / blt` then
    // `cmpwi r28, 2 / ble`, i.e. it fires only for < 0 or > E_QUALITY_COUNT.
    CGS_ASSERT((leQuality >= 0) && (leQuality <= E_QUALITY_COUNT),
               "( leQuality >= 0 ) && ( leQuality <= E_QUALITY_COUNT )");   // BrnPostFxShader.cpp:132

    meQuality = leQuality;                                                  // `stw r28, 0x80(r31)`

    if (lfTimeStep == KF_ZERO)
    {
        const rw::math::vpu::Matrix44Affine lOriginalViewCurr = maViewCache[1];
        const rw::math::vpu::Matrix44Affine lOriginalViewPrev = maViewCache[2];

        const rw::math::vpu::Matrix44Affine lOriginalCamTransCurr =
            rw::math::vpu::Inverse(lOriginalViewCurr);
        const rw::math::vpu::Matrix44Affine lOriginalCamTransPrev =
            rw::math::vpu::Inverse(lOriginalViewPrev);
        const rw::math::vpu::Matrix44Affine lCamTrans = rw::math::vpu::Inverse(lView);

        // The rotation the camera turned through over the cached pair, re-applied to the current
        // camera transform. Two binary affine products, exactly the DWARF's two operator* calls.
        rw::math::vpu::Matrix44Affine lConstructedCamTransPrev =
            (lOriginalCamTransPrev * lOriginalViewCurr) * lCamTrans;

        // The camera's per-frame world-space displacement, and the position it therefore had.
        // (DWARF spells the write Matrix44Affine::SetW; this vendor type's translation row is the
        // public wAxis / Pos() lane, so the assignment IS that setter.)
        const rw::math::vpu::Vector3 lLinearVelocity =
            lOriginalCamTransCurr.wAxis - lOriginalCamTransPrev.wAxis;
        lConstructedCamTransPrev.wAxis = lCamTrans.wAxis - lLinearVelocity;

        const rw::math::vpu::Matrix44Affine lConstructedViewPrev =
            rw::math::vpu::Inverse(lConstructedCamTransPrev);

        mCurrentWVP  = rw::math::vpu::Mult(lView, lProjection);
        mPreviousWVP = rw::math::vpu::Mult(lConstructedViewPrev, lProjection);
    }
    else
    {
        maViewCache[2] = maViewCache[1];
        maViewCache[1] = maViewCache[0];
        maViewCache[0] = lView;

        mPreviousWVP = mCurrentWVP;
        mCurrentWVP  = rw::math::vpu::Mult(lView, lProjection);
    }
}

// ==================================================================================================
// X360 0x82401538 -- BrnPostFxShader::BrnPostFxShader
//
// Zero the five {rw::Resource, state pointer} pairs and the two motion-blur sampler resource blocks.
// The committed reconstruction described these same stores as "five stride-6 head elements + two
// stride-5 loop elements"; that is exactly this list once the guest strides are read as what they
// are -- 5 resource words + 1 pointer word == 6, and a bare resource == 5. The coincidence is one of
// the strongest confirmations that the layout in the header is right.
//
// Note what is NOT zeroed: the two mapSamplerState_MotionBlur pointers (guest dwords 220/221) sit
// past the ctor's last store. Reproduced as written -- Construct fills them unconditionally, and
// clearing them here would be inventing behaviour the console does not have.
// ==================================================================================================
BrnPostFxShader::BrnPostFxShader()
{
    mVertexDescriptorResource    = rw::Resource();
    mpVertexDescriptor           = 0;
    mBlendStateResource          = rw::Resource();
    mpBlendState                 = 0;
    mDepthStencilStateResource   = rw::Resource();
    mpDepthStencilState          = 0;
    mSamplerStateResource_Point  = rw::Resource();
    mpSamplerState_Point         = 0;
    mSamplerStateResource_Linear = rw::Resource();
    mpSamplerState_Linear        = 0;

    for (u32 luQuality = 0; luQuality < MotionBlurState::E_QUALITY_COUNT; ++luQuality)
    {
        maSamplerStateResource_MotionBlur[luQuality] = rw::Resource();
    }
}

// ==================================================================================================
// X360 0x823FD970 -- BrnPostFxShader::Shader::Construct
//
// Compile this permutation's vertex + pixel program out of the allocator, then resolve every shader
// constant NAME against the compiled programs. The handle GetVariableHandleByName fills is four bytes
// -- register set, register index, program type, register COUNT -- and a name the permutation does
// not declare comes back with count 0. That zero is the mechanism the whole twelve-permutation design
// rests on: one shared 2 + 10 constant block feeds twelve shaders with twelve different constant
// sets, because SetProgram skips every zero-count handle.
//
// luPs3RegisterCount is the PS3 fragment-program register budget. All twelve X360 call sites pass 0
// (`li r9, 0` before every `bl`) and the X360 body never reads it -- the two
// muPixelShaderRegisterCount* members it feeds on PS3 (via ProgramBuffer::PS3Get/SetRegisterCount,
// per the DWARF) are left as the console leaves them.
// ==================================================================================================
void BrnPostFxShader::Shader::Construct(rw::IResourceAllocator* lpAllocator,
                                        const void* lpVertexProgram, u32 luVertexProgramSize,
                                        const void* lpPixelProgram,  u32 luPixelProgramSize,
                                        u8 luPs3RegisterCount)
{
#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
    mpVertexProgram = 0;
    mpPixelProgram  = 0;

    if (lpVertexProgram != 0 && lpPixelProgram != 0)
    {
        if (lpAdoptedVertexProgram != 0)
        {
            if (lpAdoptedPixelProgram != 0)
            {
                mpVertexProgram = lpAdoptedVertexProgram;
                mpPixelProgram  = lpAdoptedPixelProgram;
   // ownership: arena, NOT an allocator block -- see Destruct
            }
        }

        {
            {
                static bool sbReportedRefused = false;
            }
            mpVertexProgram = 0;
            mpPixelProgram  = 0;
            return;
        }

        if (false)   // the console route below is unreachable on the PC backend -- see the banner above
        {
            // ======== THE CONSOLE ROUTE (X360 0x823FD970), PRESERVED BYTE-FAITHFUL ==============
            // Kept, and kept compiled, so the console's structure stays on the page and so a future
            // backend that CAN build a program buffer has the real body to use; on this backend it
            // would crash for the reason above, so it sits behind `if (false)` rather than behind
            // "adopt refused" (which is what used to reach it).

            // ---- the vertex program (muShaderType 0) ---------------------------------------------
            // The X360 zeroes param words [1],[4],[6],[8],[9] and sets muFunction / muReserved8 -- the
            // same five-zero pattern every other program builder in this tree writes.
            renderengine::ProgramBufferParameters lVsParameters;
            lVsParameters.muShaderType        = 0;
            lVsParameters.muMicrocodePart1    = 0;
            lVsParameters.muMicrocodePart3    = 0;
            lVsParameters.muConstantTableSize = 0;
            lVsParameters.muNumVariables      = 0;
            lVsParameters.muFunction          = static_cast<u32>(reinterpret_cast<usize>(lpVertexProgram));
            lVsParameters.muReserved8         = luVertexProgramSize;

            rw::BaseResourceDescriptors<5> lVsResDesc;
            renderengine::ProgramBuffer::GetResourceDescriptor(&lVsResDesc, &lVsParameters);

            renderengine::ProgramResourceLayout lVsResource = {};
            CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lVsResource, &lVsResDesc);
            mpVertexProgram = renderengine::ProgramBuffer::Initialize(&lVsResource, &lVsParameters);

            CGS_ASSERT(mpVertexProgram != 0, "NULL != mpVertexProgram");   // BrnPostFxShader.cpp:196

            // ---- the pixel program (muShaderType 1) ----------------------------------------------
            renderengine::ProgramBufferParameters lPsParameters;
            lPsParameters.muMicrocodePart1    = 0;
            lPsParameters.muMicrocodePart3    = 0;
            lPsParameters.muConstantTableSize = 0;
            lPsParameters.muNumVariables      = 0;
            lPsParameters.muFunction          = static_cast<u32>(reinterpret_cast<usize>(lpPixelProgram));
            lPsParameters.muShaderType        = 1;
            lPsParameters.muReserved8         = luPixelProgramSize;

            rw::BaseResourceDescriptors<5> lPsResDesc;
            renderengine::ProgramBuffer::GetResourceDescriptor(&lPsResDesc, &lPsParameters);

            renderengine::ProgramResourceLayout lPsResource = {};
            CgsGraphics::ResourceAllocatorCreate(lpAllocator, &lPsResource, &lPsResDesc);
            mpPixelProgram = renderengine::ProgramBuffer::Initialize(&lPsResource, &lPsParameters);

            CGS_ASSERT(mpPixelProgram != 0, "NULL != mpPixelProgram");     // BrnPostFxShader.cpp:208
            // ======== end of the preserved console route ========================================
        }

        // ---- resolve the constant names --------------------------------------------------------
        // Two loops, `li r31, 2` over &VignetteCentreXyScaleXyPtr and `li r31, 0xA` over
        // &GlobalParamsPtr -- two static tables of interned name pointers, walked in order. Shared by
        // both routes: an adopted image resolves names through exactly the same descriptor table
        // walk, which is the whole point of the rebase Adopt performs.
        for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                mpVertexProgram,
                reinterpret_cast<const u8*>(gaacVertexVariableNames[luVsConstant]),
                &maVertexVariableHandle[luVsConstant]);
        }

        for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                mpPixelProgram,
                reinterpret_cast<const u8*>(gaacPixelVariableNames[luPsConstant]),
                &maPixelVariableHandle[luPsConstant]);
        }
    }
    else
    {

        for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
        {
            maVertexVariableHandle[luVsConstant] = renderengine::ProgramVariableHandle();
        }
        for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
        {
            maPixelVariableHandle[luPsConstant] = renderengine::ProgramVariableHandle();
        }
        muPixelShaderRegisterCountOriginal = 0;
        muPixelShaderRegisterCountModified = 0;
    }

    (void)luPs3RegisterCount;   // PS3-only; every X360 call site passes 0 and the X360 body ignores it
#else
    (void)lpAllocator;   (void)lpVertexProgram; (void)luVertexProgramSize;
    (void)lpPixelProgram; (void)luPixelProgramSize; (void)luPs3RegisterCount;

    // Leave the slot visibly EMPTY rather than half-built: SetProgram then binds nothing, and the
    // draw gate refuses to issue a quad, so nothing can present a wrong frame.
    mpVertexProgram = 0;
    mpPixelProgram  = 0;
    for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
    {
        maVertexVariableHandle[luVsConstant] = renderengine::ProgramVariableHandle();
    }
    for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
    {
        maPixelVariableHandle[luPsConstant] = renderengine::ProgramVariableHandle();
    }
    muPixelShaderRegisterCountOriginal = 0;
    muPixelShaderRegisterCountModified = 0;
#endif  // BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
}

// ==================================================================================================
// X360 0x823FDB08 -- BrnPostFxShader::Shader::Destruct
//
// Release each program, hand its block back to the allocator, and null the slot.
//
// THE RELEASE VCALL TAKES AN rw::Resource, NOT A RAW BLOCK POINTER -- one convention for both
// Destructs in this file, and it is the one the assembly supports. In BOTH functions the call is
// `lwz r11, 0(allocator); lwz r11, 0x14(r11); mtctr; bctrl` with r3 = the allocator and r4 = the
// address of a five-word rw::Resource:
//   * here @0x823FDB08, r4 is a STACK Resource (var_60 / var_40): five `stw r31` zero the lanes,
//     then `stw r10, var_60(r1)` puts the program pointer in LANE 0 and `addi r4, r1, var_60`
//     passes the block (0x823FDB24-0x823FDB5C, and again at 0x823FDB6C-0x823FDBA0);
//   * in BrnPostFxShader::Destruct @0x823FE3F0, r4 is `this + 0x2D0 / 0x2E8 / 0x300` -- three MEMBER
//     Resources.
// So the slot's signature is `DoFree(const rw::Resource&)`, which is exactly what the x64 rwcore
// vtable names there (IResourceAllocator_vtbl::DoFree, +56, overridden as
// ?DoFree@LinearResourceAllocator@rw@@MEAAXAEBVResource@2@@Z). An earlier revision passed the raw
// `mpVertexProgram` to the sibling `Free(void*, u64)` slot here while passing a Resource& in the
// other Destruct -- two conventions for one vcall, and only the second matched the asm.
//
// ⚠ WAVE DEPENDENCY: `rw::IResourceAllocator::DoFree` is added by the post-fx DRIVER agent's edit 11
// to vendor/renderware/include/rw/rwcore_structs.h (additive virtual, no storage, default no-op, so
// the RW_SIZE_ASSERT below it still holds). If that edit does not land, this arm does not compile --
// deliberately, rather than silently reverting to the wrong slot. The shipping (gates-off) arm does
// not reference it at all.
//
// --------------------------------------------------------------------------------------------------
// OWNERSHIP -- WHO OWNS THE PROGRAM IMAGE, AND WHY AN ADOPTED SLOT IS NOT HANDED BACK.
//
// A slot built by the CONSOLE route owns an allocator block: Construct ran
// CgsGraphics::ResourceAllocatorCreate, so the program pointer really is the base of a block the
// allocator carved, and reconstructing a Resource around it and calling DoFree is exactly what the
// console does.
//
// A slot built by ProgramBufferPC_Adopt owns NOTHING THE ALLOCATOR EVER ISSUED. The adopted image is
// allocator with no free function at all -- grep the leaf: ArenaAlloc and ArenaAddress exist, there is
// no ArenaFree, and `suArenaUsed` only ever advances. So:
//   * DoFree MUST NOT be called on an adopted slot. The default DoFree in rwcore_structs.h is an inline
//     no-op, but the concrete game allocator (rw::LinearResourceAllocator, whose vtable slot [7] is
//     ?DoFree@LinearResourceAllocator@rw@@MEAAXAEBVResource@2@@Z) overrides it and would be handed a
//     Resource whose lane 0 points into a foreign arena -- a free of a block it never issued. That is
//     heap corruption, and the only thing standing between us and it is this branch.
//   * The arena block is therefore LEAKED at Destruct. Stated plainly rather than papered over: it is
//     608 + 976 bytes for the one backed permutation, allocated once at boot in
//     BrnPostFxShader::Construct and reclaimed only when the process exits. BrnPostFx (and so this
//     object) is boot-lifetime, so this leak cannot repeat or grow. Inventing an arena free here --
//     the arena has no notion of one -- would be worse than disclosing it.
//   * ProgramBuffer::Release IS still called on an adopted buffer, and it is safe there: its body
//     (programbuffer.cpp:307) calls ProgramBuffer_ReleaseResource -- defined as a no-op PC stub in the
//     mounted XenonD3D9Shims.cpp:4515 -- and then writes -1 into the image's leading shader-type word,
//     which is a store into writable arena memory and marks the buffer dead exactly as on the console.
// --------------------------------------------------------------------------------------------------
// ==================================================================================================
void BrnPostFxShader::Shader::Destruct(rw::IResourceAllocator* lpAllocator)
{
#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
    // ⚠ PC CHOICE, and it is not optional: the two null tests. The console has none -- its
    // Shader::Construct asserts NULL != mpVertexProgram / mpPixelProgram, so on the X360 a slot always
    // holds a program by the time this runs. On this build eleven of the twelve slots are constructed
    // EMPTY, and ProgramBuffer::Release dereferences its argument (programbuffer.cpp:311 writes -1
    // through it), so releasing an empty slot would be a null store. The per-program ORDER is the
    // console's, unchanged: release, hand the block back, null the slot.
    if (mpVertexProgram != 0)
    {
        renderengine::ProgramBuffer::Release(mpVertexProgram);

        {
            // The console's zeroed stack Resource with the program pointer in lane 0. `= {}`
            // value-initialises every lane; no guest size, count or stride appears here (rw::Resource
            // carries its own extent).
            rw::Resource lVertexProgramResource = {};
            lVertexProgramResource.m_baseResources[0] = mpVertexProgram;
            lpAllocator->DoFree(lVertexProgramResource);
        }
        // else: adopted -- arena memory, deliberately not handed back. See the ownership banner.
    }
    mpVertexProgram = 0;

    if (mpPixelProgram != 0)
    {
        renderengine::ProgramBuffer::Release(mpPixelProgram);

        {
            rw::Resource lPixelProgramResource = {};
            lPixelProgramResource.m_baseResources[0] = mpPixelProgram;
            lpAllocator->DoFree(lPixelProgramResource);
        }
    }
    mpPixelProgram = 0;

#else
    (void)lpAllocator;
    mpVertexProgram = 0;
    mpPixelProgram  = 0;
#endif
}

// ==================================================================================================
// X360 0x823FDD18 -- BrnPostFxShader::Construct
//
// Build the twelve permutations, then the render states the composite quad draws with: a vertex
// descriptor (float3 position + float2 UV), an opaque blend state, a no-depth depth/stencil state and
// four sampler states (point, linear, and one anisotropic sampler per motion-blur quality).
// ==================================================================================================
void BrnPostFxShader::Construct(rw::IResourceAllocator* lpAllocator)
{
#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
    // ---- the twelve permutations ---------------------------------------------------------------
    // Written in the ASSEMBLY'S OWN EMISSION ORDER (slot 1, 0, 3, 2, 5, 4, ... -- the compiler
    // interleaved each Tint3d variant ahead of its sibling). Twelve calls, one per slot, exactly as
    // the console emits them; the X360 program addresses and the `li r6 / r8` size immediates that
    // belong to each call site are carried per line so the console's own arguments stay next to the
    // call they came from.
    //
    // ⚠ WHAT EACH CALL ACTUALLY PASSES, AND WHY IT IS NOT THE CONSOLE'S ARGUMENT.
    // The X360 passes a Xenos MICROCODE package address and its microcode size. A PC build cannot
    // bind Xenos microcode at all, so those twenty-four addresses do not exist on this target and
    // there is no symbol to name (declaring twenty-four `gacBrnPostFx*` externs no TU defines, which
    // an earlier revision did, buys nothing but twenty-four LNK2019s a per-TU `cl /c` gate cannot
    // see). Instead every slot gets the transcoded PC pair the shader step authored and published in
    // pc/gcm/renderengine/PostFxProgramsPC.cpp, WITH THAT ARRAY'S OWN PUBLISHED SIZE.
    //   * THE VERTEX IMAGE IS THE SAME ONE TWELVE TIMES, because the console's twelve vertex
    //     packages are byte-identical (md5 a47e7e9943a3570c484e1724d6dff763 for all twelve, 596
    //     bytes each). Handing one image to twelve slots is not a stand-in: it IS what the console
    //     ships, deduplicated. Each slot still adopts its own ProgramBufferData -- Adopt copies the
    //     image into the arena per call -- so the twelve slots stay independently releasable.
    //   * THE PIXEL IMAGE IS PER SLOT, decoded from that slot's own Xenos package, and each one
    //     reproduces that permutation's CTAB register/type/count surface exactly (the twelve-way
    //     comparison is in the wave report). No slot is given another slot's blob.
    //   ⚠ NEVER pair a PC image with a console size: the wrapped-image sizes (608 vertex,
    //     976..2016 pixel) and the X360 microcode sizes (596 vertex, 696..1308 pixel) are different
    //     numbers, and ProgramBufferPC_Adopt bounds-checks the image against the size it is given (a
    //     short size refuses a valid image, a long one reads off the end of the array). Every size
    //     below is read from the generated TU, never written out here.
    // The order is still the ASSEMBLY'S emission order; the trailing comment on each line carries
    // that call site's console operands.
                                          // X360 VS 0x8203F0B8 596 / PS 0x8203F310 796
                                          // X360 VS 0x8203F630 596 / PS 0x8203F888 696
                                          // X360 VS 0x8203FB40 596 / PS 0x8203FD98 996
                                          // X360 VS 0x82040180 596 / PS 0x820403D8 928
                                          // X360 VS 0x82040778 596 / PS 0x820409D0 1140
                                          // X360 VS 0x82040E48 596 / PS 0x820410A0 1076
                                          // X360 VS 0x820414D8 596 / PS 0x82041730 1308
                                          // X360 VS 0x82041C50 596 / PS 0x82041EA8 1240
                                          // X360 VS 0x82042380 596 / PS 0x820425D8 1140
                                          // X360 VS 0x82042A50 596 / PS 0x82042CA8 1076
                                          // X360 VS 0x820430E0 596 / PS 0x82043338 1308
                                          // X360 VS 0x82043858 596 / PS 0x82043AB0 1240

    // ---- the vertex descriptor ------------------------------------------------------------------
    // Two elements, both on stream 0: a FLOAT3 (element word 0x2A23B9, `lis 0x2A / ori 0x23B9`) and a
    // FLOAT2 (0x2C23A5 -- the same word CgsIm2dColTex.cpp already names KU_TEXCOORD_ELEMENT_WORD for
    // a FLOAT2). That is exactly the DWARF's PostFxVertexIterator (VertexTypeFloat3 +
    // VertexTypeFloat2) and exactly the 20-byte stride the quad is emitted at. The asm writes only
    // stream / element word / usage-index per element -- no in-stream offset -- matching the DWARF's
    // SetElementStream + SetElementFormat + SetElementType and nothing else.
    //
    // ⚠ FOR THE PC DECLARATION BUILDER: leaving the offset lane alone is FAITHFUL (unlike
    // CgsIm2dColTex.cpp, which writes 0 / 8 / 12), but it means the second element's in-stream offset
    // is NOT supplied here -- Parameters()'s constructor leaves it at 0xFFFF and the usage lane at
    // 0xFF. Whoever turns these two elements into a D3D9 vertex declaration must DERIVE the UV
    // element's 12-byte offset from the preceding FLOAT3, exactly as the console's descriptor
    // compiler does. Taking 0xFFFF at face value makes both elements fetch from offset 0, the UVs
    // read the position floats, and the composited picture goes flat -- with no error anywhere.
    renderengine::VertexDescriptor::Parameters lVertexDescriptorParams;
    lVertexDescriptorParams.maElements[0].mu16Stream    = 0;
    lVertexDescriptorParams.maElements[0].miOffset      = static_cast<s32>(0x2A23B9);
    lVertexDescriptorParams.maElements[0].mu8UsageIndex = 1;
    lVertexDescriptorParams.maElements[1].mu16Stream    = 0;
    lVertexDescriptorParams.maElements[1].miOffset      = static_cast<s32>(0x2C23A5);
    lVertexDescriptorParams.maElements[1].mu8UsageIndex = 6;

    u8 lauVertexDescriptorResDesc[144] = {};
    renderengine::VertexDescriptor::GetResourceDescriptor(lauVertexDescriptorResDesc,
                                                          &lVertexDescriptorParams);
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &mVertexDescriptorResource,
                                         lauVertexDescriptorResDesc);
    mpVertexDescriptor = renderengine::VertexDescriptor::Initialize(&mVertexDescriptorResource,
                                                                    &lVertexDescriptorParams);

    // ---- the blend state ------------------------------------------------------------------------
    // Every immediate is an asm store: the four per-target blend words 0x07060706 (`lis 0x706 / ori
    // 0x706`), AlphaFunc 7 (ALWAYS in the Xenos zero-based encoding), ColorWriteEnable 15 on all four
    // targets, AlphaToMaskOffsets 0x87, AlphaRef 0, BlendFactor -1, and all seven flag bytes zero --
    // including mbHasCustomBlendFactors, so Initialize substitutes its own ONE/ZERO default for the
    // four factor words. Opaque: nothing is blended, the composite overwrites the target.
    renderengine::BlendStateParameters lBlendStateParams;
    lBlendStateParams.maBlendFactor[0]        = 0x07060706u;
    lBlendStateParams.maBlendFactor[1]        = 0x07060706u;
    lBlendStateParams.maBlendFactor[2]        = 0x07060706u;
    lBlendStateParams.maBlendFactor[3]        = 0x07060706u;
    lBlendStateParams.muState15               = 7;            // AlphaFunc == ALWAYS
    lBlendStateParams.muState4                = 15;           // ColorWriteEnable
    lBlendStateParams.muState5                = 15;           // ColorWriteEnable1
    lBlendStateParams.muState6                = 15;           // ColorWriteEnable2
    lBlendStateParams.muState7                = 15;           // ColorWriteEnable3
    lBlendStateParams.muState8                = 0x87;         // AlphaToMaskOffsets
    lBlendStateParams.muState17               = 0;            // AlphaRef
    lBlendStateParams.muState9                = 0xFFFFFFFFu;  // BlendFactor (`li r28, -1`)
    lBlendStateParams.mbHasCustomBlendFactors = 0;
    lBlendStateParams.mbState10               = 0;
    lBlendStateParams.mbState11               = 0;
    lBlendStateParams.mbState12               = 0;
    lBlendStateParams.mbState13               = 0;
    lBlendStateParams.mbState14               = 0;
    lBlendStateParams.mbState16               = 0;

    u8 lauBlendStateResDesc[48] = {};
    renderengine::BlendState::GetResourceDescriptor(lauBlendStateResDesc, &lBlendStateParams);
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &mBlendStateResource, lauBlendStateResDesc);
    mpBlendState = reinterpret_cast<renderengine::BlendMaterialState*>(
        renderengine::BlendState::Initialize(
            reinterpret_cast<renderengine::BlendMaterialState**>(&mBlendStateResource),
            &lBlendStateParams));

    // ---- the depth/stencil state ----------------------------------------------------------------
    // ZFunc 3 (LESSEQUAL in the Xenos zero-based encoding), both stencil funcs 7 (ALWAYS), all four
    // stencil masks -1, every flag byte zero -- depth test OFF, depth write OFF, stencil OFF. A
    // full-screen composite must not be depth-rejected and must not disturb the depth buffer.
    renderengine::DepthStencilState::Parameters lDepthStencilParams;
    lDepthStencilParams.muFunction         = 3;
    lDepthStencilParams.maState1[0]        = 0;
    lDepthStencilParams.maState1[1]        = 0;
    lDepthStencilParams.maState1[2]        = 0;
    lDepthStencilParams.muState4           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;
    lDepthStencilParams.maState5[0]        = 0;
    lDepthStencilParams.maState5[1]        = 0;
    lDepthStencilParams.maState5[2]        = 0;
    lDepthStencilParams.muState8           = renderengine::DepthStencilState::E_FUNCTION_ALWAYS;
    lDepthStencilParams.muState9           = 0;
    lDepthStencilParams.muState10          = 0;
    lDepthStencilParams.muStencilReadMask  = 0xFFFFFFFFu;
    lDepthStencilParams.muStencilWriteMask = 0xFFFFFFFFu;
    lDepthStencilParams.muState13          = 0;
    lDepthStencilParams.muState14          = 0xFFFFFFFFu;
    lDepthStencilParams.muState15          = 0xFFFFFFFFu;
    lDepthStencilParams.muState16          = 0;
    lDepthStencilParams.mbDepthTestEnable  = 0;
    lDepthStencilParams.mbDepthWriteEnable = 0;
    lDepthStencilParams.mu8Flag2           = 0;
    lDepthStencilParams.mu8Flag3           = 0;
    lDepthStencilParams.mu8Flag4           = 0;
    lDepthStencilParams.mu8Flag5           = 0;

    renderengine::ResourceDescriptor5 lDepthStencilStateResDesc;
    renderengine::DepthStencilState::GetResourceDescriptor(&lDepthStencilStateResDesc,
                                                           &lDepthStencilParams);
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &mDepthStencilStateResource,
                                         &lDepthStencilStateResDesc);
    mpDepthStencilState = renderengine::DepthStencilState::Initialize(
        reinterpret_cast<renderengine::DepthStencilState**>(&mDepthStencilStateResource),
        &lDepthStencilParams);

    // ---- the POINT sampler ----------------------------------------------------------------------
    // Clamped on all three axes, unfiltered. This is what unit 0 gets when the source is the scene
    // target at 1:1 -- a linear fetch there would soften every pixel.
    renderengine::SamplerStateParameters lSamplerStateParams_Point;
    for (u32 luByte = 0; luByte < sizeof(lSamplerStateParams_Point); ++luByte)
    {
        reinterpret_cast<u8*>(&lSamplerStateParams_Point)[luByte] = 0;
    }
    lSamplerStateParams_Point.muAddressU    = KU_SAMPLER_ADDRESS_CLAMP;
    lSamplerStateParams_Point.muAddressV    = KU_SAMPLER_ADDRESS_CLAMP;
    lSamplerStateParams_Point.muAddressW    = KU_SAMPLER_ADDRESS_CLAMP;
    lSamplerStateParams_Point.muBias        = KU_SAMPLER_FILTER_POINT;   // MIN filter
    lSamplerStateParams_Point.muMaxLevel    = KU_SAMPLER_FILTER_POINT;   // MAG filter
    lSamplerStateParams_Point.muMinLevel    = KU_SAMPLER_MIP_FILTER;     // MIP filter
    lSamplerStateParams_Point.muConvolution = 1;                         // MAX anisotropy
    lSamplerStateParams_Point.mbState11Zero = 1;   // `stb r26, var_36D`
    lSamplerStateParams_Point.mbState26     = 1;   // `stb r26, var_36C`

    renderengine::ResourceDescriptorEntry laSamplerResDesc_Point[5] = {};
    renderengine::SamplerState::GetResourceDescriptor(laSamplerResDesc_Point);
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &mSamplerStateResource_Point,
                                         laSamplerResDesc_Point);
    mpSamplerState_Point = renderengine::SamplerState::Initialize(
        reinterpret_cast<renderengine::SamplerStateData**>(&mSamplerStateResource_Point),
        &lSamplerStateParams_Point);
    CGS_ASSERT(mpSamplerState_Point != 0, "NULL != mpSamplerState_Point");   // BrnPostFxShader.cpp:507

    // ---- the LINEAR sampler ---------------------------------------------------------------------
    // Identical but bilinear. Bound on units 1 and 2 always, and on unit 0 when the caller supplied an
    // override source texture (i.e. the source is not 1:1 with the target).
    renderengine::SamplerStateParameters lSamplerStateParams_Linear = lSamplerStateParams_Point;
    lSamplerStateParams_Linear.muBias     = KU_SAMPLER_FILTER_LINEAR;   // MIN filter
    lSamplerStateParams_Linear.muMaxLevel = KU_SAMPLER_FILTER_LINEAR;   // MAG filter

    renderengine::ResourceDescriptorEntry laSamplerResDesc_Linear[5] = {};
    renderengine::SamplerState::GetResourceDescriptor(laSamplerResDesc_Linear);
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &mSamplerStateResource_Linear,
                                         laSamplerResDesc_Linear);
    mpSamplerState_Linear = renderengine::SamplerState::Initialize(
        reinterpret_cast<renderengine::SamplerStateData**>(&mSamplerStateResource_Linear),
        &lSamplerStateParams_Linear);
    CGS_ASSERT(mpSamplerState_Linear != 0, "NULL != mpSamplerState_Linear");  // BrnPostFxShader.cpp:526

    // ---- the two MOTION-BLUR samplers -----------------------------------------------------------
    // One per MotionBlurState::EQuality: anisotropic, 4x for E_QUALITY_CHEAP and 16x for
    // E_QUALITY_EXPENSIVE. The X360 loop counts 4, 16 in steps of 12 and stops at 0x1C -- exactly two
    // iterations, which is E_QUALITY_COUNT and is why the arrays are sized by that enum. The
    // console's `if (luCount == 0 || luCount == 1)` guard around the anisotropic override is always
    // true for those two iterations; it is reproduced because it is a real branch.
    u32 luMaxAnisotropy = 4;
    for (u32 luCount = 0; luCount < MotionBlurState::E_QUALITY_COUNT; ++luCount)
    {
        renderengine::SamplerStateParameters lSamplerStateParams_MotionBlur = lSamplerStateParams_Point;
        lSamplerStateParams_MotionBlur.muBias        = KU_SAMPLER_FILTER_LINEAR;
        lSamplerStateParams_MotionBlur.muMaxLevel    = KU_SAMPLER_FILTER_LINEAR;
        lSamplerStateParams_MotionBlur.muConvolution = 1;

        if (luCount == 0 || luCount == 1)
        {
            lSamplerStateParams_MotionBlur.muBias        = KU_SAMPLER_FILTER_ANISO;
            lSamplerStateParams_MotionBlur.muMaxLevel    = KU_SAMPLER_FILTER_ANISO;
            lSamplerStateParams_MotionBlur.muConvolution = luMaxAnisotropy;
        }

        renderengine::ResourceDescriptorEntry laSamplerResDesc_MotionBlur[5] = {};
        renderengine::SamplerState::GetResourceDescriptor(laSamplerResDesc_MotionBlur);
        CgsGraphics::ResourceAllocatorCreate(lpAllocator, &maSamplerStateResource_MotionBlur[luCount],
                                             laSamplerResDesc_MotionBlur);
        mapSamplerState_MotionBlur[luCount] = renderengine::SamplerState::Initialize(
            reinterpret_cast<renderengine::SamplerStateData**>(&maSamplerStateResource_MotionBlur[luCount]),
            &lSamplerStateParams_MotionBlur);
        CGS_ASSERT(mapSamplerState_MotionBlur[luCount] != 0,
                   "NULL != mapSamplerState_MotionBlur[luCount]");            // BrnPostFxShader.cpp:565

        luMaxAnisotropy += 12;   // `addi r27, r27, 0xC` -> 4 then 16
    }
#else
    (void)lpAllocator;

    // Nothing is built. Every maShaders slot keeps the ctor's null programs, every state pointer stays
    // null, and Render's draw gate refuses to issue a quad -- so this is inert, not partial.
    static bool sbReportedNoPrograms = false;
#endif  // BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
}

// ==================================================================================================
// X360 0x823FE3F0 -- BrnPostFxShader::Destruct
//
// Destruct all twelve permutations, release the vertex descriptor, then hand the vertex-descriptor,
// blend and depth/stencil resource blocks back to the allocator.
//
// Note what the console does NOT free: the four sampler-state resource blocks. Only three release
// vcalls follow the VertexDescriptor::Release, at this+0x2D0 / +0x2E8 / +0x300 -- the descriptor,
// blend and depth/stencil blocks. Reproduced as written; adding the sampler frees would be inventing
// behaviour.
//
// The release vcall is the same vtbl+0x14 slot Shader::Destruct uses and takes the same argument
// shape -- an rw::Resource by reference (`addi r4, r28, 0x2D0` etc.). See the banner on
// Shader::Destruct for the recovery and for the rwcore_structs.h dependency.
// ==================================================================================================
void BrnPostFxShader::Destruct(rw::IResourceAllocator* lpAllocator)
{
    for (u32 luShader = 0; luShader < E_SHADER_COUNT; ++luShader)
    {
        maShaders[luShader].Destruct(lpAllocator);
    }

#if BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE
    renderengine::VertexDescriptor::Release(mpVertexDescriptor);

    lpAllocator->DoFree(mVertexDescriptorResource);   // `addi r4, r28, 0x2D0` @0x823FE434
    lpAllocator->DoFree(mBlendStateResource);         // `addi r4, r28, 0x2E8` @0x823FE44C
    lpAllocator->DoFree(mDepthStencilStateResource);  // `addi r4, r28, 0x300` @0x823FE464
#else
    (void)lpAllocator;
#endif
}

// ==================================================================================================
// X360 0x82408F08 -- BrnPostFxShader::Render -- THE COMPOSITE.
// ==================================================================================================
void BrnPostFxShader::Render(f32 lfWhiteLevel,
                             const BrnPostFxBloomData& lBloomData,
                             const rw::graphics::postfx::Vignette::State& lVignetteState,
                             const rw::graphics::postfx::DepthOfField::State& lDofState,
                             const MotionBlurState& lMotionBlurState,
                             rw::math::vpu::Vector4 lTint2dColour,
                             f32 lfBrightness,
                             f32 lfContrast,
                             bool lbEnableDof,
                             bool lbEnableBloom,
                             bool lbEnableTint3d,
                             bool lbEnableVignette,
                             bool lbEnableMotionBlur,
                             renderengine::Texture* lpSourceTexture,
                             renderengine::Texture* lpBloomTexture,
                             renderengine::Texture* lpDofTexture,
                             const renderengine::TextureState* lp3dTintTexture,
                             const renderengine::TextureState* lpDepthTexture,
                             rw::math::vpu::Vector2 lHalfPixelOffset,
                             f32 lfAspectCorrection,
                             bool lbBilinearSource)
{
    // The two constant blocks, zeroed first. The X360 clears the vertex pair with two 16-byte stores
    // and the pixel block with a twenty-iteration 8-byte store loop (20 doublewords == 10 float4s):
    // EVERY constant starts at zero and only the enabled effects overwrite theirs. That is what makes
    // "all effects off" a legal state for a shader that always declares bloom, tint and vignette.
    rw::math::vpu::Vector4 laVertexShaderConstants[E_VS_VAR_COUNT];
    rw::math::vpu::Vector4 laPixelShaderConstants[E_PS_VAR_COUNT];
    for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
    {
        laVertexShaderConstants[luVsConstant].SetZero();
    }
    for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
    {
        laPixelShaderConstants[luPsConstant].SetZero();
    }

    // ---- GlobalParams ---------------------------------------------------------------------------
    // CORRECTION TO ANALYSIS.md section 4, with the evidence. GlobalParams does NOT carry brightness /
    // contrast / white level / aspect correction. The X360 writes it ONCE (`fdivs f0, f29, f1` ->
    // `stfs f0, var_400`, three 0.0f stores, one 16-byte store into the first pixel-constant slot
    // @0x82408FBC-0x82408FC0) and never touches that slot again -- grep the listing for var_230 and
    // it returns three hits: the zero-fill base, that store, and the SetProgram argument. So
    // GlobalParams == { 1 / lfWhiteLevel, 0, 0, 0 }.
    // The other three scalars ride elsewhere, and this matters for the options menu:
    //   lfBrightness       -> the ADDITIVE term of Tint2dColour (below)
    //   lfContrast         -> the RGB multiplier of both vignette constants (lContrastScale, below)
    //   lfAspectCorrection -> the quad's own vertical extent (lfLetterboxY, below), not a constant.
    const f32 lfInverseWhiteLevel = KF_ONE / lfWhiteLevel;
    laPixelShaderConstants[E_PS_VAR_GLOBAL_PARAMS].x = lfInverseWhiteLevel;
    laPixelShaderConstants[E_PS_VAR_GLOBAL_PARAMS].y = KF_ZERO;
    laPixelShaderConstants[E_PS_VAR_GLOBAL_PARAMS].z = KF_ZERO;
    laPixelShaderConstants[E_PS_VAR_GLOBAL_PARAMS].w = KF_ZERO;

    // ---- BloomColour ----------------------------------------------------------------------------
    // The bloom colour scaled by its luminance over the white level, alpha forced to zero (the
    // console's `vrlimi128 v0, v126, 1, 0` inserts the zero vector's w lane). When bloom is off the
    // console stores the zero vector explicitly; the zero-fill above already did it, but the store is
    // reproduced so the two paths read the same.
    if (lbEnableBloom)
    {
        const f32 lfBloomScale = lBloomData.mfLuminance * lfInverseWhiteLevel;
        rw::math::vpu::Vector4 lBloomColour;
        lBloomColour.x = lBloomData.mColour.x * lfBloomScale;
        lBloomColour.y = lBloomData.mColour.y * lfBloomScale;
        lBloomColour.z = lBloomData.mColour.z * lfBloomScale;
        lBloomColour.w = KF_ZERO;
        laPixelShaderConstants[E_PS_VAR_BLOOM_COLOUR] = lBloomColour;
    }
    else
    {
        laPixelShaderConstants[E_PS_VAR_BLOOM_COLOUR].SetZero();
    }

    // ---- DofParamsA / DofParamsB ----------------------------------------------------------------
    // UNCONDITIONAL on the console -- there is no lbEnableDof test around this block. It is cheap, and
    // in a permutation without DofParams* in its constant table the two handles come back with
    // register count 0 and SetProgram drops them.
    //
    // zProjScale / wProjOffset are the two halves of the projection's depth transform, rebuilt from
    // the near/far planes the DoF state carries; each focal plane is pushed through it and clamped
    // into [0,1]. The names are the DWARF's (BrnPostFxShader.cpp:682/683/689/695/696/700).
    {
        const f32 zProjScale  = lDofState.m_projFarPlane
                              / (lDofState.m_projFarPlane - lDofState.m_projNearPlane);
        const f32 wProjOffset = -(lDofState.m_projNearPlane * zProjScale);

        rw::math::vpu::Vector4 lDofParams1;
        lDofParams1.x = ProjectAndClampFocalPlane(lDofState.m_focalPlanes[0], zProjScale, wProjOffset);
        lDofParams1.y = ProjectAndClampFocalPlane(lDofState.m_focalPlanes[1], zProjScale, wProjOffset);
        lDofParams1.z = ProjectAndClampFocalPlane(lDofState.m_focalPlanes[2], zProjScale, wProjOffset);
        lDofParams1.w = ProjectAndClampFocalPlane(lDofState.m_focalPlanes[3], zProjScale, wProjOffset);
        laPixelShaderConstants[E_PS_VAR_DOF_PARAMS_A] = lDofParams1;

        // The two blur ramps, as reciprocals so the pixel program multiplies instead of divides. The
        // zero guards are the console's (`fcmpu` against 0.0f, constant-0 branch), not additions.
        const f32 lfNearDelta = lDofParams1.y - lDofParams1.x;
        const f32 lfFarDelta  = lDofParams1.w - lDofParams1.z;

        rw::math::vpu::Vector4 lDofParams2;
        lDofParams2.x = lDofState.m_dofAmount;
        lDofParams2.y = (lfNearDelta == KF_ZERO) ? KF_ZERO : (KF_ONE / lfNearDelta);
        lDofParams2.z = (lfFarDelta  == KF_ZERO) ? KF_ZERO : (KF_ONE / lfFarDelta);
        lDofParams2.w = KF_ZERO;
        laPixelShaderConstants[E_PS_VAR_DOF_PARAMS_B] = lDofParams2;
    }

    // ---- the vignette pair, and where CONTRAST lives --------------------------------------------
    // lContrastScale multiplies the RGB of BOTH vignette constants; its w is 1. With the vignette OFF
    // the console writes lContrastScale itself into both -- so on this build (every effect bit clear)
    // the composite's only colour operations are a flat multiply by lfContrast and the Tint2dColour
    // offset below. That is the whole visible payload of this step.
    const rw::math::vpu::Vector4 lContrastScale = { lfContrast, lfContrast, lfContrast, KF_ONE };

    if (lbEnableVignette)
    {
        // THE MEMBER-NAME SHIFT THIS BLOCK USED TO DOCUMENT IS FIXED (2026-08-15, step-3 wave).
        // The composite reads its vignette state as four Vector4s: +0x00 a control vector whose .x is
        // the gradient sharpness and whose .y is the rotation angle, +0x10 the INNER colour, +0x20 the
        // OUTER colour, +0x30 centre.xy / scale.xy. rw::graphics::postfx::Vignette::State used to name
        // those mInnerColour / mOuterColour / mControlA / mControlB (plus a phantom trailing
        // mfSharpness at +0x40) -- shifted one Vector4 late -- so the lines below wrote the correct
        // bytes through the wrong names on purpose. rwgpfxvignette.h now carries the DecFIGS member
        // set (m_gradientScalars / m_innerColour / m_outerColour / m_centerScale, and the State is
        // exactly 0x40 bytes), pinned offset-by-offset against Vignette::SetState @0x823FE8B0 and the
        // constructor @0x82403DB8; the reads below are simply renamed to match, with no change of
        // meaning and no change of byte. (This branch still cannot run on this build -- see the
        // "why the index is 0" banner.)
        const f32 lfSharpParam        = (lVignetteState.m_gradientScalars.x * KF_HALF) + KF_HALF;  // +0x00 .x = sharpness
        const f32 lfGradientSharpness = powf(lfSharpParam, KF_VIGNETTE_GRADIENT_POWER);
        const f32 lfGradientMul       = (lfGradientSharpness * KF_VIGNETTE_GRADIENT_LIMIT) + KF_ONE;
        const f32 lfGradientAdd       = -(lfGradientSharpness * KF_VIGNETTE_GRADIENT_LIMIT);

        // VignetteAngle: lane x only, taken from the control vector's y. The console does it with one
        // vrlimi128 (rotate left one word, insert lane x); the DWARF names it SetX<VectorAxisY>.
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_ANGLE].x = lVignetteState.m_gradientScalars.y;

        // VignetteCentreXyScaleXy: centre.xy passes through, scale.xy is multiplied by the gradient.
        const rw::math::vpu::Vector4 lVignetteScaleMult = { KF_ONE, KF_ONE, lfGradientMul, lfGradientMul };
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY] =
            MultiplyPerLane(lVignetteState.m_centerScale, lVignetteScaleMult);   // +0x30 = centre / scale

        rw::math::vpu::Vector4 lInnerRgbPlusMul =
            MultiplyPerLane(lVignetteState.m_innerColour, lContrastScale);       // +0x10 = INNER colour
        lInnerRgbPlusMul.w = lfGradientMul;
        laPixelShaderConstants[E_PS_VAR_VIGNETTE_INNER_RGB_PLUS_MUL] = lInnerRgbPlusMul;

        rw::math::vpu::Vector4 lOuterRgbPlusAdd =
            MultiplyPerLane(lVignetteState.m_outerColour, lContrastScale);       // +0x20 = OUTER colour
        lOuterRgbPlusAdd.w = lfGradientAdd;
        laPixelShaderConstants[E_PS_VAR_VIGNETTE_OUTER_RGB_PLUS_ADD] = lOuterRgbPlusAdd;
    }
    else
    {
        // Vignette off: angle zero, centre/scale all-ones, both colour constants the flat contrast
        // scale. Inner == outer means the gradient interpolates between two identical colours, which
        // is the identity whatever the gradient does.
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_ANGLE].SetZero();
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY].x = KF_ONE;
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY].y = KF_ONE;
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY].z = KF_ONE;
        laVertexShaderConstants[E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY].w = KF_ONE;
        laPixelShaderConstants[E_PS_VAR_VIGNETTE_INNER_RGB_PLUS_MUL]    = lContrastScale;
        laPixelShaderConstants[E_PS_VAR_VIGNETTE_OUTER_RGB_PLUS_ADD]    = lContrastScale;
    }

    // ---- Tint2dColour, and where BRIGHTNESS lives -----------------------------------------------
    // The 2D tint colour premultiplied by its own alpha, plus a scalar bias added to all three
    // channels; alpha forced to zero. The bias is one fused negative-multiply-subtract in the asm
    // (`fadds f8, f31, f30` then `fnmsubs f9, f27, f30, f8`), i.e.
    // -(contrast*0.5 - (brightness + 0.5)). THIS is the options-menu brightness slider: a pure
    // additive offset on the composite's output.
    {
        const f32 lfTintBias = -((lfContrast * KF_HALF) - (lfBrightness + KF_HALF));
        rw::math::vpu::Vector4 lTint;
        lTint.x = (lTint2dColour.x * lTint2dColour.w) + lfTintBias;
        lTint.y = (lTint2dColour.y * lTint2dColour.w) + lfTintBias;
        lTint.z = (lTint2dColour.z * lTint2dColour.w) + lfTintBias;
        lTint.w = KF_ZERO;
        laPixelShaderConstants[E_PS_VAR_TINT_2D_COLOUR] = lTint;
    }

    // ---- BlurMatrixX / BlurMatrixY / BlurMatrixW ------------------------------------------------
    // The camera-motion-blur REPROJECTION matrix: screen -> world through the inverse of the current
    // world-view-projection, world -> screen through the previous one, and the difference of the two
    // taken as a per-pixel screen velocity. The console builds it in DOUBLE precision through four
    // rw::math::fpu::Matrix44Template<double> calls (Inverse @0x82405210, Mult @0x82405690, Subtract
    // @0x82405780, Mult-by-scalar @0x824058A0) over the DWARF's named intermediates
    // lProjectionToScreen / lScreenToProjection / lInverseCurrentWVP / lScreenToWorld_Curr /
    // lWorldToScreen_Prev / lScreenCurrentToPrevious / lScreenToVelocity / lScreenToVelocityRefined
    // (BrnPostFxShader.cpp:758-775), then narrows rows 0, 1 and 3 back to float4s.
    //
    // ⚠ THE CONSOLE COMPUTES THIS BLOCK UNCONDITIONALLY -- there is no test of lbEnableMotionBlur
    // around it. The sixteen lfs/stfd reads off lMotionBlurState.mCurrentWVP begin at 0x824092FC,
    // immediately after the vignette merge, and the four rw::math::fpu calls at 0x82409468 /
    // 0x82409478 / 0x824095BC / 0x824095CC are equally unguarded; arg_87 (lbEnableMotionBlur) is not
    // read until 0x824096C8, and then only to build the permutation index and pick the source
    // sampler. Reproduced unconditional.
    //
    // ---- THE CHAIN, CALL FOR CALL (asm 0x824092EC-0x824095CC) ------------------------------------
    // Every step below is one of the five `bl`s in that range, with the DWARF's own local name
    // (BrnPostFxShader.cpp:758-775) and the stack slot the console builds it in:
    //   var_3D0  lProjectionToScreen       -- built from GetMatrix44_64_Identity with three rows
    //                                        overwritten: m00 = dbl_82047D60, m11 = dbl_82047D58,
    //                                        m30 = m31 = dbl_82047D60 (stores @0x82409314 /
    //                                        0x82409344 / 0x82409318 / 0x8240931C).
    //   var_350  lScreenToProjection       -- same shape: m00 = dbl_82047D50, m11 = dbl_82047D48,
    //                                        m30 = dbl_82047D40, m31 = the identity's 1.0.
    //   var_110  lInverseCurrentWVP        = Inverse((f64)mCurrentWVP, det)          @0x82409468
    //   var_2B0  lScreenToWorld_Curr       = Mult(lScreenToProjection, lInverseCurrentWVP)
    //                                                                                @0x82409478
    //   var_110  lWorldToScreen_Prev       = Mult((f64)mPreviousWVP, lProjectionToScreen)
    //                                                                                @0x8240950C
    //   var_190  lScreenCurrentToPrevious  = Mult(lScreenToWorld_Curr, lWorldToScreen_Prev)
    //                                                                                @0x8240951C
    //   var_350  lScreenToVelocity         = Subtract(lScreenCurrentToPrevious, Identity)
    //                                                                                @0x8240956C
    //   var_190                            = Mult(Identity, lScreenToVelocity[3][3]) @0x824095BC
    //                                        (the scalar overload; f1 is loaded from var_2D8, which
    //                                        is element [3][3] of the matrix Subtract just wrote)
    //   var_3D0  lScreenToVelocityRefined  = Subtract(lScreenToVelocity, that)       @0x824095CC
    //
    // ---- THE FIVE CONSTANTS -- RECOVERED (idat dump 2026-08-16: dbl_82047D40 = -1.0, D48 = -2.0,
    // D50 = 2.0, D58 = -0.5, D60 = 0.5; dbl_82001CA0 = 1.0, dbl_82001CA8 = 0.0), and the argument
    // that had already pinned them before the dump, kept because it explains WHY: (a) Both matrices
    // are seeded from GetMatrix44_64_Identity (the DWARF
    // names that function twice for this function) and only the four listed elements are
    // overwritten, so every other element is the identity's 0 or 1 -- that is what pins
    // dbl_82001CA0 == 1.0 and dbl_82001CA8 == 0.0, which are then observed as m22/m33 in both.
    // (b) The two matrices are NAMED as an inverse pair. The only pair of that shape that IS an
    // inverse pair is the standard NDC<->uv mapping: u = 0.5x + 0.5, v = -0.5y + 0.5 one way and
    // x = 2u - 1, y = 1 - 2v the other. That forces D60 = 0.5, D58 = -0.5, D50 = 2.0, D48 = -2.0,
    // D40 = -1.0, and it forces lScreenToProjection's m31 to be exactly the identity's +1.0 --
    // which is what the asm stores there (`stfd f30, var_2E8`, f30 being the same register the
    // identity's diagonal comes from). Any other assignment breaks the inverse relation.
    // (The dump confirmed every one of the five, and the two identity doubles.)
    //
    // ---- WHY THE SHADER GETS *COLUMNS* -----------------------------------------------------------
    // The console narrows twelve elements back to float (`lfd` + `frsp` + `stfs`, 0x824095D0-
    // 0x82409684) in exactly this pattern: {m00,m10,m20,m30} -> BlurMatrixX, {m01,m11,m21,m31} ->
    // BlurMatrixY, {m03,m13,m23,m33} -> BlurMatrixW. Those are COLUMNS 0, 1 and 3 of the refined
    // matrix, because the pixel program's per-pixel expression is
    // `dot((u, v, depth, 1), constant)` -- a row-vector times a column. It also explains the one
    // asymmetry the permutation decode found: BlurMatrixW carries no `.w` term because element
    // [3][3] of the refined matrix is identically ZERO (the last Subtract removes
    // lScreenToVelocity[3][3] from the whole diagonal, so [3][3] cancels itself).
#if !BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE
    {
        static bool sbReportedNoReprojection = false;
    }
#else
    {
        typedef rw::math::fpu::Matrix44_64 Matrix44_64;

        // clip -> uv (BrnPostFxShader.cpp:758).
        Matrix44_64 lProjectionToScreen = rw::math::fpu::GetMatrix44_64_Identity();
        lProjectionToScreen.xAxis.Set( 0.5,  0.0, 0.0, 0.0);
        lProjectionToScreen.yAxis.Set( 0.0, -0.5, 0.0, 0.0);
        lProjectionToScreen.wAxis.Set( 0.5,  0.5, 0.0, 1.0);

        // uv -> clip (BrnPostFxShader.cpp:763) -- the exact inverse of the above.
        Matrix44_64 lScreenToProjection = rw::math::fpu::GetMatrix44_64_Identity();
        lScreenToProjection.xAxis.Set( 2.0,  0.0, 0.0, 0.0);
        lScreenToProjection.yAxis.Set( 0.0, -2.0, 0.0, 0.0);
        lScreenToProjection.wAxis.Set(-1.0,  1.0, 0.0, 1.0);

        f64 lfDeterminant = 0.0;
        const Matrix44_64 lInverseCurrentWVP = rw::math::fpu::Inverse(
            Matrix44_64(lMotionBlurState.mCurrentWVP), lfDeterminant);

        const Matrix44_64 lScreenToWorld_Curr =
            rw::math::fpu::Mult(lScreenToProjection, lInverseCurrentWVP);
        const Matrix44_64 lWorldToScreen_Prev =
            rw::math::fpu::Mult(Matrix44_64(lMotionBlurState.mPreviousWVP), lProjectionToScreen);
        const Matrix44_64 lScreenCurrentToPrevious =
            rw::math::fpu::Mult(lScreenToWorld_Curr, lWorldToScreen_Prev);

        const Matrix44_64 lScreenToVelocity = rw::math::fpu::Subtract(
            lScreenCurrentToPrevious, rw::math::fpu::GetMatrix44_64_Identity());

        const Matrix44_64 lScreenToVelocityRefined = rw::math::fpu::Subtract(
            lScreenToVelocity,
            rw::math::fpu::Mult(rw::math::fpu::GetMatrix44_64_Identity(),
                                lScreenToVelocity.wAxis.W()));

        laPixelShaderConstants[E_PS_VAR_BLUR_MATRIX_X] =
            NarrowToVector4(lScreenToVelocityRefined.GetXColumn());
        laPixelShaderConstants[E_PS_VAR_BLUR_MATRIX_Y] =
            NarrowToVector4(lScreenToVelocityRefined.GetYColumn());
        laPixelShaderConstants[E_PS_VAR_BLUR_MATRIX_W] =
            NarrowToVector4(lScreenToVelocityRefined.GetWColumn());
    }
#endif

    // ---- the permutation ------------------------------------------------------------------------
    // Both asserts are the console's, at the console's source lines.
    CGS_ASSERT((lMotionBlurState.meQuality >= 0) &&
               (lMotionBlurState.meQuality < MotionBlurState::E_QUALITY_COUNT),
               "( lMotionBlurState.meQuality >= 0 ) && ( lMotionBlurState.meQuality <"
               " MotionBlurState::E_QUALITY_COUNT )");                    // BrnPostFxShader.cpp:798

    const u32 lxBlurBit   = lbEnableMotionBlur
                          ? (4u * (static_cast<u32>(lMotionBlurState.meQuality) + 1u))
                          : 0u;                                           // BrnPostFxShader.cpp:799
    const u32 lxDofBit    = lbEnableDof    ? 2u : 0u;                      // BrnPostFxShader.cpp:795
    const u32 lxTint3dBit = lbEnableTint3d ? 1u : 0u;                      // BrnPostFxShader.cpp:796
    const u32 leShader    = lxBlurBit | lxDofBit | lxTint3dBit;            // BrnPostFxShader.cpp:800

    CGS_ASSERT(leShader < static_cast<u32>(E_SHADER_COUNT), "leShader < E_Shader_Count");  // :804

#if !BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE
    {
        static bool sbReportedNoDraw = false;
        (void)lpSourceTexture; (void)lpBloomTexture;   (void)lpDofTexture;
        (void)lp3dTintTexture; (void)lpDepthTexture;
        (void)lHalfPixelOffset; (void)lfAspectCorrection; (void)lbBilinearSource;
        return;
    }
#else
    // ---- bind -----------------------------------------------------------------------------------
    // Order is the asm's, and it is load-bearing: the programs and their constants first, then the two
    // render states, then texture+sampler per unit, then the vertex descriptor, then the flush.
    maShaders[leShader].SetProgram(laVertexShaderConstants, laPixelShaderConstants);

    // The console open-codes the lock-gate / compare / apply / cache sequence for each of these; that
    // sequence IS shadow::Device::SetState, so this is inlining reversal, not a paraphrase. The blend
    // third gates on the blend lock byte and the depth/stencil third on its own -- both inside
    // SetState.
    shadow::Device::SetState(mpBlendState);
    shadow::Device::SetState(mpDepthStencilState);

    // Unit 0: the resolved scene colour. Its sampler is the motion-blur anisotropic one while blur is
    // on, the LINEAR one when the caller overrode the source texture (so the source is not 1:1 with
    // the target), and otherwise POINT -- a 1:1 blit must not be filtered.
    shadow::Device::SetResource(lpSourceTexture, KU_SAMPLER_SOURCE);
    renderengine::SamplerStateData* lpSourceSampler = 0;
    // The min/mag filter word and the MAX ANISOTROPY of whichever sampler this arm picks, carried
    // alongside the object because the object's words never reach D3D9 (see the seam call below).
    // Every value is the one Construct wrote into that very sampler: POINT/LINEAR at
    // BrnPostFxShader.cpp:1293-1294 / :1313-1314, ANISO + 4|16 at :1341-1343 / :1356. The point and
    // linear blocks carry muConvolution = 1 (:1296, inherited by the linear copy at :1312).
    u32 luSourceFilter        = KU_SAMPLER_FILTER_POINT;
    u32 luSourceMaxAnisotropy = 1;
    if (lbEnableMotionBlur)
    {
        lpSourceSampler       = mapSamplerState_MotionBlur[lMotionBlurState.meQuality];
        luSourceFilter        = KU_SAMPLER_FILTER_ANISO;
        luSourceMaxAnisotropy =
            MotionBlurSamplerMaxAnisotropy(static_cast<u32>(lMotionBlurState.meQuality));
    }
    else if (lbBilinearSource)
    {
        lpSourceSampler = mpSamplerState_Linear;
        luSourceFilter  = KU_SAMPLER_FILTER_LINEAR;
    }
    else
    {
        lpSourceSampler = mpSamplerState_Point;
        luSourceFilter  = KU_SAMPLER_FILTER_POINT;
    }
    shadow::Device::SetState(static_cast<void*>(lpSourceSampler), KU_SAMPLER_SOURCE);

    // Units 1 and 2: the bloom buffer and the depth-of-field buffer, both bilinear (they are
    // down-sampled, so filtering them is the point).
    shadow::Device::SetResource(lpBloomTexture, KU_SAMPLER_BLOOM);
    shadow::Device::SetState(static_cast<void*>(mpSamplerState_Linear), KU_SAMPLER_BLOOM);
    shadow::Device::SetResource(lpDofTexture, KU_SAMPLER_DOF);
    shadow::Device::SetState(static_cast<void*>(mpSamplerState_Linear), KU_SAMPLER_DOF);

    // Units 3 and 4: the colour-cube volume and the scene depth, each bound as a whole TextureState
    // (texture + its own sampler) rather than as a texture/sampler pair.
    shadow::Device::SetState(lp3dTintTexture, KU_SAMPLER_TINT_3D);

    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

    // Default-off observation at this exact composite bind. An auxiliary
    // overlay sampler can leave the generic EndVertices probe latch raised,
    // so that heuristic does not establish which input texture was sampled.

    // ---- the quad -------------------------------------------------------------------------------
    // Four vertices in CLIP space, triangle-strip order (bottom-left, bottom-right, top-left,
    // top-right with y up), spanning x in [-1, 1] and y in [lfLetterboxY - 1, 1 - lfLetterboxY] --
    // lfAspectCorrection is what letterboxes the composite. The expression is the asm's own
    // (`fsubs f0, f29, f26` then `fsubs f12, f0, f29` and `fsubs f0, f29, f0`), kept as written
    // because the intermediate is the DWARF's named local.
    //
    // lHalfPixelOffset is added to every UV, and it is not optional: D3D9 samples at integer screen
    // coordinates while texel i's centre sits at i + 0.5, so a 0..1 UV quad without it lands on texel
    // BOUNDARIES and a linear fetch averages neighbours everywhere. The console computes the same
    // offset in BrnPostFx::Render (1/width, 1/height scaled by 0.5) and passes it in.
    const f32 lfLetterboxY = KF_ONE - lfAspectCorrection;                   // BrnPostFxShader.cpp:854
    const f32 lfBottomY    = lfLetterboxY - KF_ONE;
    const f32 lfTopY       = KF_ONE - lfLetterboxY;

    PostFxVertex laVertices[KU_COMPOSITE_VERTEX_COUNT];
    laVertices[0].mfX = KF_MINUS_ONE; laVertices[0].mfY = lfBottomY; laVertices[0].mfZ = KF_ZERO;
    laVertices[0].mfU = KF_ZERO + lHalfPixelOffset.x; laVertices[0].mfV = KF_ONE  + lHalfPixelOffset.y;
    laVertices[1].mfX = KF_ONE;       laVertices[1].mfY = lfBottomY; laVertices[1].mfZ = KF_ZERO;
    laVertices[1].mfU = KF_ONE  + lHalfPixelOffset.x; laVertices[1].mfV = KF_ONE  + lHalfPixelOffset.y;
    laVertices[2].mfX = KF_MINUS_ONE; laVertices[2].mfY = lfTopY;    laVertices[2].mfZ = KF_ZERO;
    laVertices[2].mfU = KF_ZERO + lHalfPixelOffset.x; laVertices[2].mfV = KF_ZERO + lHalfPixelOffset.y;
    laVertices[3].mfX = KF_ONE;       laVertices[3].mfY = lfTopY;    laVertices[3].mfZ = KF_ZERO;
    laVertices[3].mfU = KF_ONE  + lHalfPixelOffset.x; laVertices[3].mfV = KF_ZERO + lHalfPixelOffset.y;

    D3DDevice* const lpDevice = reinterpret_cast<D3DDevice*>(renderengine::gpD3DDevice);
    void* const lpRing = D3DDevice_BeginVertices(lpDevice, KU_COMPOSITE_PRIMITIVE_TYPE,
                                                 KU_COMPOSITE_VERTEX_COUNT,
                                                 KU_COMPOSITE_VERTEX_STRIDE);
    if (lpRing != 0)
    {
        PostFxVertex* const lpDst = reinterpret_cast<PostFxVertex*>(lpRing);
        for (u32 luVertex = 0; luVertex < KU_COMPOSITE_VERTEX_COUNT; ++luVertex)
        {
            lpDst[luVertex] = laVertices[luVertex];
        }
    }
    D3DDevice_EndVertices(lpDevice);
#endif  // BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE
}
