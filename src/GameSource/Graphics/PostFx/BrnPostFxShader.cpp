// WHOLE-FILE REPLACEMENT of the committed file (which bodied only the ctor, against a
// misreconstructed class -- see BrnPostFxShader.h's banner).
#include "GameSource/Graphics/PostFx/BrnPostFxShader.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                             // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                      // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"           // shadow::Device
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"        // BlendMaterialState / BlendStateParameters
#include "pc/gcm/renderengine/renderstates.h"                                   // DepthStencilState / ResourceDescriptor5
#include "pc/gcm/renderengine/VertexDescriptor.h"                               // VertexDescriptor / VertexDescriptorData
#include "pc/gcm/renderengine/texture.h"                                        // renderengine::Texture
#include "pc/gcm/renderengine/Xbox2SurfaceShims.h"                              // renderengine::gpD3DDevice
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"         // ResourceAllocatorCreate

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
//   BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE -- the four rw::math::fpu::Matrix44Template<double>
//        calls Render makes to build the camera-reprojection matrix, whose three outputs are
//        BlurMatrixX / BlurMatrixY / BlurMatrixW. NO rw::math::fpu matrix type and no
//        Inverse/Mult/Subtract exist anywhere in b5-decomp (vendor/renderware/include/rw/math/fpu/
//        holds matrix33_operation.h and scalar_operation.h and nothing else).
//        THIS CANNOT AFFECT PERMUTATION 0, AND THAT IS PROVEN RATHER THAN ASSUMED: permutation 0's
//        pixel package (X360 0x8203F888) interns exactly BloomColour, GlobalParams, SamplerBloom,
//        SamplerSource, Tint2dColour, VignetteInnerRgbPlusMul, VignetteOuterRgbPlusAdd -- no
//        BlurMatrix* at all (DATA_DUMP.md, "slot dword 0"). A name absent from a permutation's
//        constant table resolves to a handle with register COUNT 0, and SetProgram skips those. So
//        in permutation 0 those three constants are never uploaded and their value is unread. The
//        gate leaves them at the zero the console's own zero-fill puts there.
//        ⚠ WHAT CHANGED WHEN THE EIGHT BLUR PERMUTATIONS GOT PC PROGRAMS (step 5). Slots 4..11 DO
//        intern BlurMatrixX/Y/W, so selecting one now uploads three ZERO float4s to a shader that
//        reads them. The consequence is bounded and it is NOT a crash: the pixel program builds its
//        screen velocity as `(row.xy - row.z * uv) * mask`, which with a zero matrix is exactly
//        (0,0) -- so both gradients handed to tex2Dgrad are zero and the fetch degenerates to an
//        ordinary top-mip tap. Motion blur would be SILENTLY ABSENT, not wrong. That is the whole
//        reason this gate stays 0 and is reported once per boot instead of being quietly ignored.
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
// While BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE is 0, Render computes its constants, asserts exactly as
// the console does, reports once, and returns WITHOUT TOUCHING ANY DEVICE STATE. That is the safe
// failure: BrnRendererModule's [FLAG PC bring-up] PCBringUpBlitSceneTargetToBackBuffer is still what
// presents the frame, and half-binding this composite's blend / depth / sampler states would corrupt
// the state that blit relies on. DO NOT DELETE THAT BLIT UNTIL THIS GATE IS 1 AND A FRAME HAS BEEN
// VERIFIED.
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
#define BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE   0
#define BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE             1

namespace renderengine
{
    // One shader-constant upload: luNumRegisters float4s at register luRegister, on the vertex or the
    // pixel bank. DEFINED in the mounted pc/gcm/renderengine/XenonD3D9Shims.cpp:778; declared here
    // exactly as GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.cpp:139 declares it -- that
    // TU is the existing precedent for this seam, which has no header yet.
    void WorldShaderConstants_Set(bool lbPixel, u32 luRegister, const void* lpData, u32 luNumRegisters);

    // ---- THE TWELVE PERMUTATIONS' PC PROGRAMS ----------------------------------------------------
    // [FLAG PC-platform leaf: shader programs] The composite's vertex + pixel programs, wrapped as
    // platform-4 ShaderProgramBuffer images by the shader step
    // (pc/gcm/renderengine/PostFxProgramsPC.cpp -- a GENERATED PC leaf). Declared here as the minimal
    // external surface, byte for byte the convention BrnIm3d.cpp:51-57 already uses for the sky-dome
    // pair; there is no header for these generated leaves.
    //
    // ONE VERTEX IMAGE FOR ALL TWELVE SLOTS, AND THAT IS MEASURED. The twelve X360 vertex packages
    // (0x8203F0B8, 0x8203F630, 0x8203FB40, 0x82040180, 0x82040778, 0x82040E48, 0x820414D8,
    // 0x82041C50, 0x82042380, 0x82042A50, 0x820430E0, 0x82043858) are 596 bytes each and
    // BYTE-IDENTICAL -- md5 a47e7e9943a3570c484e1724d6dff763 for all twelve. The vignette coordinate
    // the vertex program builds does not depend on any of the three permutation axes, so the console
    // shipped the same program twelve times; this build declares it once.
    //
    // ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES (596 vertex, 696..1308
    // pixel). ProgramBufferPC_Adopt bounds-checks the blob against the size it is handed and copies
    // exactly that many bytes, so passing a console size here would either refuse a valid image
    // (596 < the 0x14 + 516 microcode extent -> "microcode size is out of range") or read past the
    // array. That is why nothing below hard-codes a number: each size is read from the generated
    // TU's own published constant, which its static_assert ties to sizeof() of the array.
    extern const u8  gauPostFxCompositeVertexProgramPC[];
    extern const u32 guPostFxCompositeVertexProgramPCSize;
    extern const u8  gauPostFxCompositePixelProgramPC[];              // slot  0
    extern const u32 guPostFxCompositePixelProgramPCSize;
    extern const u8  gauPostFxCompositeTint3dPixelProgramPC[];        // slot  1
    extern const u32 guPostFxCompositeTint3dPixelProgramPCSize;
    extern const u8  gauPostFxCompositeDofPixelProgramPC[];           // slot  2
    extern const u32 guPostFxCompositeDofPixelProgramPCSize;
    extern const u8  gauPostFxCompositeDofTint3dPixelProgramPC[];     // slot  3
    extern const u32 guPostFxCompositeDofTint3dPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurPixelProgramPC[];          // slot  4
    extern const u32 guPostFxCompositeBlurPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurTint3dPixelProgramPC[];    // slot  5
    extern const u32 guPostFxCompositeBlurTint3dPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurDofPixelProgramPC[];       // slot  6
    extern const u32 guPostFxCompositeBlurDofPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurDofTint3dPixelProgramPC[]; // slot  7
    extern const u32 guPostFxCompositeBlurDofTint3dPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurHqPixelProgramPC[];        // slot  8
    extern const u32 guPostFxCompositeBlurHqPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurHqTint3dPixelProgramPC[];  // slot  9
    extern const u32 guPostFxCompositeBlurHqTint3dPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurHqDofPixelProgramPC[];     // slot 10
    extern const u32 guPostFxCompositeBlurHqDofPixelProgramPCSize;
    extern const u8  gauPostFxCompositeBlurHqDofTint3dPixelProgramPC[];       // slot 11
    extern const u32 guPostFxCompositeBlurHqDofTint3dPixelProgramPCSize;
}

#if BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE
// [FLAG PC bring-up] The two ends of the composite draw that have no home in this tree. Declared at
// file scope (the same pattern CgsIm2dUntex.cpp uses for D3DDevice_BeginVertices and
// shadow::DeviceSetVertexProgramInternal) so that the gated block below is TYPE-CHECKED even while
// the gate is off -- code behind a dead #if is not compiled, and this campaign has already shipped a
// batch of C2039s that hid that way for weeks.
struct D3DDevice;
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
    // faithful; the PC leaf that eventually defines D3DDevice_BeginVertices owns the 6 -> 5
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
    // TU CAN EVER DEFINE. That is the exact defect ShadowPassPCLeaf.h exists to document; it was
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
    mbAdoptedPC     = false;

    if (lpVertexProgram != 0 && lpPixelProgram != 0)
    {
        // ---- [FLAG PC-platform leaf: program buffers] ADOPT A PRE-BUILT PC IMAGE ---------------
        // THE CONSOLE ROUTE BELOW CANNOT RUN ON THE PC BACKEND -- it is a crash, not a cosmetic gap.
        // renderengine::ProgramBuffer::GetResourceDescriptor AND ::Initialize both call
        // XGGetMicrocodeShaderParts, whose PC stub returns 0 WITHOUT writing *lpParts; both then read
        // that uninitialised ProgramMicrocodeParts for the microcode size and hand a 64-bit function
        // pointer truncated into the u32 muFunction to Xbox2CreateConstantTable
        // (ImmediateModePCLeaf.cpp:626-646 states this outright).
        //
        // When the supplied binary already IS a converted platform-4 ShaderProgramBuffer -- which is
        // exactly what the sibling shader step's permutation-0 pair is -- there is nothing for
        // Initialize to build: the image carries the shader type at +0, the variable count at +4, the
        // microcode size at +8, the D3D9 SM3 bytecode at +0x14 and the ProgramVariableDescriptor table
        // after it. ProgramBufferPC_Adopt (programbuffer.h:123, body ImmediateModePCLeaf.cpp:647)
        // validates that shape, copies the image into the low-4GB arena and rebases the descriptors'
        // name offsets from FILE offsets to absolute addresses, so GetVariableHandleByName below works
        // unchanged. A non-PC binary returns null and falls through to the console path untouched.
        //
        // THIS IS THE SkyDome PRECEDENT, SAME SHAPE: CgsIm3dSkyDome.cpp:149-171 (VS shader type 0, PS
        // type 1, both-or-neither, null falls through to the preserved console route).
        renderengine::ProgramBufferData* const lpAdoptedVertexProgram =
            renderengine::ProgramBufferPC_Adopt(lpVertexProgram, luVertexProgramSize, 0u);
        if (lpAdoptedVertexProgram != 0)
        {
            renderengine::ProgramBufferData* const lpAdoptedPixelProgram =
                renderengine::ProgramBufferPC_Adopt(lpPixelProgram, luPixelProgramSize, 1u);
            if (lpAdoptedPixelProgram != 0)
            {
                mpVertexProgram = lpAdoptedVertexProgram;
                mpPixelProgram  = lpAdoptedPixelProgram;
                mbAdoptedPC     = true;   // ownership: arena, NOT an allocator block -- see Destruct
            }
        }

        if (!mbAdoptedPC)
        {
            // ======== THE CONSOLE ROUTE (X360 0x823FD970), PRESERVED BYTE-FAITHFUL ==============
            // Reached only for a NON-NULL binary that is not a PC ShaderProgramBuffer image. No call
            // site in this file can reach it -- BrnPostFxShader::Construct passes either the two
            // adopted PC images or (nullptr, 0) -- and on this backend it would crash for the reason
            // above. It is kept, and kept compiled, so the console's structure stays on the page and
            // so a future backend that CAN build a program buffer has the real body to use.

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
        // ---- [FLAG PC bring-up] AN HONESTLY EMPTY SLOT -----------------------------------------
        // NO CALL SITE IN THIS FILE REACHES THIS ARM ANY MORE. Since post-fx step 5 all twelve
        // Construct calls below pass a real PC pair, so this branch survives only as the guard
        // against a null argument -- which is what it always was. Kept rather than deleted because
        // an empty slot is still the only correct answer to a null pair: both program pointers stay
        // null and every handle keeps register count 0, so SetProgram binds nothing and uploads
        // nothing, and Render's own guard refuses to draw a slot with no program. Nothing
        // half-built, nothing invented.
        //
        // DEVIATION FROM THE CONSOLE, DELIBERATE: the console asserts NULL != mpVertexProgram /
        // mpPixelProgram (BrnPostFxShader.cpp:196/208) on every slot. Reporting once instead of
        // firing a per-slot assert storm for a condition this file discloses is the PC choice; with
        // every slot backed it should now never be reached at all, and the log line saying it WAS
        // reached is the signal that a program array went missing from the leaf.
        static bool sbReportedEmptySlot = false;
        ReportOnce(sbReportedEmptySlot,
                   "[postfx-composite] a permutation slot was constructed EMPTY -- Shader::Construct"
                   " was handed a null program pair. Since step 5 every slot has a PC pair"
                   " (renderengine::gauPostFxComposite*ProgramPC), so this line means a program"
                   " array is missing from pc/gcm/renderengine/PostFxProgramsPC.cpp. Empty slots"
                   " bind nothing and Render refuses to draw them."
                   " [FLAG PC bring-up: BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE]\n");

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
    mbAdoptedPC     = false;
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
// carved from the PC leaf's own low-4GB arena (ImmediateModePCLeaf.cpp:148 ArenaAlloc), a BUMP
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

        if (!mbAdoptedPC)
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

        if (!mbAdoptedPC)
        {
            rw::Resource lPixelProgramResource = {};
            lPixelProgramResource.m_baseResources[0] = mpPixelProgram;
            lpAllocator->DoFree(lPixelProgramResource);
        }
    }
    mpPixelProgram = 0;

    mbAdoptedPC = false;
#else
    (void)lpAllocator;
    mpVertexProgram = 0;
    mpPixelProgram  = 0;
    mbAdoptedPC     = false;
#endif
}

// ==================================================================================================
// X360 0x823FDBB0 -- BrnPostFxShader::Shader::SetProgram
//
// Bind this permutation's program pair and upload the constants it declares.
//
// THE ZERO-COUNT SKIP IS THE WHOLE MECHANISM. Both loops test the handle's register COUNT (`lbz r11,
// 3(r30)` and `lbz r6, 0(r7)` where r7 walks the handle array offset by 3) and do nothing when it is
// zero. Permutation 0 declares five of the ten pixel constants -- GlobalParams, BloomColour,
// Tint2dColour, VignetteInnerRgbPlusMul, VignetteOuterRgbPlusAdd -- so DofParamsA/B and
// BlurMatrixX/Y/W are skipped, uploaded nowhere, and unread.
//
// ⚠ THE REGISTER NUMBER IS HANDLE BYTE 0 -- `mu8RegisterSet`, NOT `mu8RegisterIndex`.
// An earlier revision of this function uploaded at `mu8RegisterIndex` (byte 1). That is silent, total
// corruption: every constant would land on a register the shader does not read, and the registers it
// does read would keep whatever the previous draw left in them. Byte 0 is RECOVERED, four ways:
//   * THE X360 ASM, this function. The pixel loop walks `r7 = this + 0x13` == &maPixelVariableHandle[0]
//     + 3 and reads `lbz r6, 0(r7)` (byte 3 = register COUNT, the zero test), `lbz r11, -1(r7)`
//     (byte 2 = program type, the bank select) and `lbz r11, -3(r7)` -- BYTE 0 -- which is the value
//     that goes through `addi r11, r11, 0x78|0x178` then `slwi r11, r11, 4`. Byte 1 is never read.
//   * shadowingdevice.cpp:1035 does `WorldShaderConstants_Set(lbPixel, lpHandle[0], lpSource,
//     lpHandle[3])` over the same 4-byte handle, with the same lane note above it.
//   * ImmediateModePCLeaf.cpp:564 stages `lpHandle->mu8RegisterSet` as the register index, and
//     BrnIm3d.cpp:349 reads `maStateHandles[9].mu8RegisterSet` as a SAMPLER UNIT.
//   * ImmediateModePCLeaf.cpp:51-56 says why the PC agrees with the console here even though the
//     X360 field NAMES read backwards: the transcoder writes each descriptor as
//     `<I B B B B  nameOffset, regIdx, typeByte, regCount, 0>` and GetVariableHandleByName copies
//     +4 -> handle byte 0 and +5 -> handle byte 1, so on PC byte 1 is the DATA TYPE.
// Both lanes hold small numbers (registers are c0..c9 here, and a D3DXPT type code is also single
// digit), so the wrong lane never traps, never asserts and never produces a non-finite value -- it
// just draws the wrong picture. Do not "simplify" this back.
//
// [FLAG PC-platform leaf: shader-constant upload] The console writes the pixel constants BY HAND into
// the Xenos command-buffer constant shadow (address `16 * (register + 120 | 376)`, plus a 64-bit
// dirty mask OR-ed into the head of that block) and stages the vertex constants through
// renderengine::Device::BeginShaderStates. Both are the same operation -- "put this float4 at this
// constant register" -- and both are one Set{Vertex,Pixel}ShaderConstantF on D3D9, which is what
// renderengine::WorldShaderConstants_Set does. The 120 / 376 are the two bank BASES inside the Xenos
// shadow block, NOT part of the register number, so they do not appear here.
// ==================================================================================================
void BrnPostFxShader::Shader::SetProgram(const rw::math::vpu::Vector4* lpaVertexShaderConstants,
                                         const rw::math::vpu::Vector4* lpaPixelShaderConstants)
{
    shadow::Device::SetVertexProgram(mpVertexProgram);
    shadow::Device::SetPixelProgram(mpPixelProgram);

    // THE BANK COMES FROM THE HANDLE IN BOTH LOOPS, not from which table the handle sits in. The X360
    // pixel loop branches on the handle's program-type byte (`lbz r11, -1(r7)`, byte 2) to pick the
    // vertex constant base (0x78) or the pixel base (0x178); the vertex loop hands the whole handle to
    // renderengine::Device::BeginShaderStates, whose PC leaf reads the same byte
    // (ImmediateModePCLeaf.cpp:566, `mbPixel = (lpHandle->mu8ShaderType != 0)`). A name resolved out
    // of the VERTEX program buffer carries type 0 and a PIXEL one carries non-zero, so in practice
    // the vertex loop always takes the vertex bank -- deriving it rather than hard-coding `false`
    // costs nothing and keeps the two loops honest about where the bank actually comes from.
    for (u32 luVsConstant = 0; luVsConstant < E_VS_VAR_COUNT; ++luVsConstant)
    {
        const renderengine::ProgramVariableHandle& lrHandle = maVertexVariableHandle[luVsConstant];
        if (lrHandle.mu8RegisterCount != 0)
        {
            const bool lbPixelBank = (lrHandle.mu8ShaderType != 0);
            renderengine::WorldShaderConstants_Set(lbPixelBank, lrHandle.mu8RegisterSet,
                                                   &lpaVertexShaderConstants[luVsConstant], 1);
        }
    }

    for (u32 luPsConstant = 0; luPsConstant < E_PS_VAR_COUNT; ++luPsConstant)
    {
        const renderengine::ProgramVariableHandle& lrHandle = maPixelVariableHandle[luPsConstant];
        if (lrHandle.mu8RegisterCount != 0)
        {
            const bool lbPixelBank = (lrHandle.mu8ShaderType != 0);
            renderengine::WorldShaderConstants_Set(lbPixelBank, lrHandle.mu8RegisterSet,
                                                   &lpaPixelShaderConstants[luPsConstant], 1);
        }
    }
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
    maShaders[E_SHADER_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeTint3dPixelProgramPC,
        renderengine::guPostFxCompositeTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x8203F0B8 596 / PS 0x8203F310 796
    maShaders[E_SHADER_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositePixelProgramPC,
        renderengine::guPostFxCompositePixelProgramPCSize,
        0);                                          // X360 VS 0x8203F630 596 / PS 0x8203F888 696
    maShaders[E_SHADER_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeDofTint3dPixelProgramPC,
        renderengine::guPostFxCompositeDofTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x8203FB40 596 / PS 0x8203FD98 996
    maShaders[E_SHADER_DOF_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeDofPixelProgramPC,
        renderengine::guPostFxCompositeDofPixelProgramPCSize,
        0);                                          // X360 VS 0x82040180 596 / PS 0x820403D8 928
    maShaders[E_SHADER_BLUR_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurTint3dPixelProgramPC,
        renderengine::guPostFxCompositeBlurTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x82040778 596 / PS 0x820409D0 1140
    maShaders[E_SHADER_BLUR_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurPixelProgramPC,
        renderengine::guPostFxCompositeBlurPixelProgramPCSize,
        0);                                          // X360 VS 0x82040E48 596 / PS 0x820410A0 1076
    maShaders[E_SHADER_BLUR_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurDofTint3dPixelProgramPC,
        renderengine::guPostFxCompositeBlurDofTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x820414D8 596 / PS 0x82041730 1308
    maShaders[E_SHADER_BLUR_DOF_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurDofPixelProgramPC,
        renderengine::guPostFxCompositeBlurDofPixelProgramPCSize,
        0);                                          // X360 VS 0x82041C50 596 / PS 0x82041EA8 1240
    maShaders[E_SHADER_BLURHQ_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurHqTint3dPixelProgramPC,
        renderengine::guPostFxCompositeBlurHqTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x82042380 596 / PS 0x820425D8 1140
    maShaders[E_SHADER_BLURHQ_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurHqPixelProgramPC,
        renderengine::guPostFxCompositeBlurHqPixelProgramPCSize,
        0);                                          // X360 VS 0x82042A50 596 / PS 0x82042CA8 1076
    maShaders[E_SHADER_BLURHQ_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurHqDofTint3dPixelProgramPC,
        renderengine::guPostFxCompositeBlurHqDofTint3dPixelProgramPCSize,
        0);                                          // X360 VS 0x820430E0 596 / PS 0x82043338 1308
    maShaders[E_SHADER_BLURHQ_DOF_BLOOM_VIGNETTE_TINT2D].Construct(
        lpAllocator,
        renderengine::gauPostFxCompositeVertexProgramPC,
        renderengine::guPostFxCompositeVertexProgramPCSize,
        renderengine::gauPostFxCompositeBlurHqDofPixelProgramPC,
        renderengine::guPostFxCompositeBlurHqDofPixelProgramPCSize,
        0);                                          // X360 VS 0x82043858 596 / PS 0x82043AB0 1240

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
    ReportOnce(sbReportedNoPrograms,
               "[postfx-composite] BrnPostFxShader::Construct SKIPPED -- ALL TWELVE permutations now"
               " have PC program images (renderengine::gauPostFxComposite*ProgramPC), but"
               " renderengine::SamplerState, renderengine::DepthStencilState and"
               " renderengine::VertexDescriptor::Release still have no mounted home. The composite"
               " cannot draw; the bring-up scene blit still presents the frame."
               " [FLAG PC bring-up: BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE]\n");
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
    // sampler. So the DISCLOSURE below is unconditional too: the block is missing whether or not
    // motion blur was asked for, and gating the report on the flag would have described the
    // reconstruction as more faithful than it is.
    //
    // NOT RECONSTRUCTED THIS WAVE, AND THE THREE CONSTANTS ARE LEFT AT ZERO. There is no rw::math::fpu
    // 4x4 type anywhere in b5-decomp, so the four callees have no home. See the gate's banner for why
    // this cannot affect permutation 0: that permutation's pixel package does not intern
    // BlurMatrixX/Y/W at all, so their handles carry register count 0 and SetProgram never uploads
    // them.
#if BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE
#error "BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE is on but the reprojection block is not reconstructed -- see REPORT.md"
#else
    {
        static bool sbReportedNoReprojection = false;
        ReportOnce(sbReportedNoReprojection,
                   "[postfx-composite] the camera-motion reprojection block is not reconstructed (no"
                   " rw::math::fpu::Matrix44Template<double> in this tree); BlurMatrixX/Y/W stay zero."
                   " The console computes this block on every composite, motion blur or not; it is"
                   " unread in permutation 0, which does not intern those three constants."
                   " [FLAG PC bring-up: BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE]\n");
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

    // ---- the disclosed bring-up condition --------------------------------------------------------
    // THE "ONLY PERMUTATION 0" REFUSAL IS RETIRED (post-fx step 5). All twelve slots are constructed
    // from real PC program images, so every index this function can compute is drawable and there is
    // nothing left to refuse on the grounds of a missing program.
    //
    // WHAT REPLACES IT IS NARROWER AND STILL HARD: a slot whose programs are NULL is not drawn. That
    // is not a hypothetical -- Shader::Construct leaves a slot empty when it is handed a null pair,
    // and renderengine::ProgramBufferPC_Adopt returns null for any image that fails its shape check
    // (ImmediateModePCLeaf.cpp:671-690), so a truncated or mis-sized array in the generated leaf
    // lands here rather than in the rasteriser. SetProgram on an empty slot binds NO program at all,
    // which on D3D9 leaves whatever the previous draw installed -- the quad would then rasterise
    // through a foreign shader and write a garbage frame with no error anywhere. Reporting once and
    // returning leaves the device exactly as it was found, so the [FLAG PC bring-up] scene blit in
    // BrnRendererModule still presents a correct (un-composited) frame.
    //
    // The test is on BOTH pointers because SetProgram binds both and Construct only ever sets them
    // together. Naming the slot in the log line is the point: the number IS the permutation, and
    // 4|8 vs 2 vs 1 says immediately which axis brought an unbacked slot in.
    if (maShaders[leShader].mpVertexProgram == 0 || maShaders[leShader].mpPixelProgram == 0)
    {
        static bool sbReportedUnbackedPermutation = false;
        ReportOnce(sbReportedUnbackedPermutation,
                   "[postfx-composite] the selected permutation has NO PC program pair (an empty slot,"
                   " or ProgramBufferPC_Adopt refused its image). NOT DRAWING -- the frame is"
                   " presented un-composited rather than through whatever program the previous draw"
                   " left bound.\n");
        return;
    }

#if !BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE
    {
        static bool sbReportedNoDraw = false;
        ReportOnce(sbReportedNoDraw,
                   "[postfx-composite] constants computed, draw SKIPPED --"
                   " shadow::Device::SetState(const TextureState*, u32) (X360 0x8227D158) has no home,"
                   " and D3DDevice_BeginVertices/EndVertices are declared in three TUs and defined in"
                   " none. (Permutation 0's PC program pair EXISTS and is adopted whenever"
                   " BRN_POSTFX_SHADER_PROGRAMS_AVAILABLE is 1; the programs are no longer the"
                   " blocker here.)"
                   " [FLAG PC bring-up: BRN_POSTFX_COMPOSITE_DRAW_AVAILABLE]\n");
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
    if (lbEnableMotionBlur)
    {
        lpSourceSampler = mapSamplerState_MotionBlur[lMotionBlurState.meQuality];
    }
    else if (lbBilinearSource)
    {
        lpSourceSampler = mpSamplerState_Linear;
    }
    else
    {
        lpSourceSampler = mpSamplerState_Point;
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
    shadow::Device::SetState(lpDepthTexture,  KU_SAMPLER_DEPTH);

    shadow::Device::SetVertexDescriptor(mpVertexDescriptor);
    shadow::Device::FlushVertexProgramState();

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
