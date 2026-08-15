// ⚠ WHOLE-FILE REPLACEMENT of the committed file (which is a misreconstruction -- see the banner).
#ifndef BRN_POST_FX_SHADER_H
#define BRN_POST_FX_SHADER_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                                    // rw::Resource / rw::IResourceAllocator
#include "rw/math/vpu/types.h"                                                    // Vector2 / Vector4 / Matrix44 / Matrix44Affine
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"        // ProgramBufferData / ProgramVariableHandle
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxvignette.h"   // Vignette::State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxdof.h"        // DepthOfField::State
#include "GameSource/Graphics/PostFx/BrnPostFxBloomData.h"                         // BrnPostFxBloomData

// ==================================================================================================
// BrnPostFxShader -- THE POST-FX COMPOSITE: one full-screen draw through a TWELVE-PERMUTATION
// uber-shader that tone-maps the resolved scene and folds bloom, the 2D tint, the vignette, the
// colour-cube (3D tint), depth of field and camera motion blur into a single pass.
//
// ⚠ THIS FILE REPLACES A MISRECONSTRUCTION. The committed version modelled the class as
//       u8 mPad0[720]; HeadElem mHeadElems[5]; LoopElem mLoopElems[2];
// "unrecovered members preceding the head elements". Every one of those three lines is now
// accounted for by a real member, and the old shape is not merely imprecise -- it is wrong:
//
//   * the 720 bytes are NOT padding. BrnPostFxShader::Construct @0x823FDD18 makes exactly twelve
//     Shader::Construct calls whose first argument is `this + {0x3C,0,0xB4,0x78,0x12C,0xF0,0x1A4,
//     0x168,0x21C,0x1E0,0x294,0x258}` -- i.e. guest dword indices 0,15,30,...,165. Twelve
//     sub-objects of 15 guest dwords each == 720 guest bytes, starting at the very FRONT of the
//     object with nothing before them.
//   * "HeadElem mHeadElems[5]" (stride 6, at guest dword 180) is the FIVE {rw::Resource, object
//     pointer} render-state pairs that follow the array: 5 words of resource + 1 pointer == the
//     stride-6 the old reconstruction saw. Construct writes them at guest dwords 180..209 and the
//     pointers at 185 / 191 / 197 / 203 / 209 (asm byte offsets 0x2E4 / 0x2FC / 0x314 / 0x32C /
//     0x344, all four attested again as READS in BrnPostFxShader::Render).
//   * "LoopElem mLoopElems[2]" (stride 5, at guest dword 210) is the two motion-blur SamplerState
//     rw::Resource blocks; their two object pointers live at guest dwords 220/221 (byte 0x370/0x374,
//     read by Render as `this[quality + 220]`).
//
//   ⚠ Those slots hold POINTERS, so the array WIDENS on the LLP64 host: the guest 60-byte Shader
//   stride is NOT the host stride and guest dword 191 is NOT a host offset. NOTHING below carries
//   720, 15, 191 or 197 as a number -- every member is reached by NAME and the compiler sizes the
//   object. (AGENTS.md rule: no X360 offsets/strides/sizeofs on the host.)
//
// SHAPE AUTHORITY: the DecFIGS DWARF (references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/
// BrnPostFxShader.h), which declares this class member-for-member -- `BrnPostFxShader::Shader[12]
// maShaders`, then mVertexDescriptorResource / mpVertexDescriptor / mBlendStateResource /
// mpBlendState / mDepthStencilStateResource / mpDepthStencilState / mSamplerStateResource_Point /
// mpSamplerState_Point / mSamplerStateResource_Linear / mpSamplerState_Linear /
// maSamplerStateResource_MotionBlur[2] / mapSamplerState_MotionBlur[2]. That is EXACTLY the
// 12 + 5x(resource+pointer) + 2x(resource) + 2x(pointer) the X360 asm walks, in that order.
// BEHAVIOUR AUTHORITY: the X360 ARTIST asm (Construct @0x823FDD18, Destruct @0x823FE3F0,
// Shader::Construct @0x823FD970, Shader::Destruct @0x823FDB08, Shader::SetProgram @0x823FDBB0,
// Render @0x82408F08).
// ==================================================================================================

namespace renderengine
{
    class  Texture;                 // pc/gcm/renderengine/texture.h
    class  TextureState;            // pc/gcm/renderengine/renderstates.h
    class  DepthStencilState;       // pc/gcm/renderengine/renderstates.h
    struct SamplerStateData;        // SDKs/RenderEngineClub/.../states/samplerstate.cpp (no header yet)
    struct BlendMaterialState;      // SDKs/RenderEngineClub/.../states/blendstate.h
    struct VertexDescriptorData;    // pc/gcm/renderengine/VertexDescriptor.h
}

// --------------------------------------------------------------------------------------------------
// MotionBlurState -- the camera-motion-blur reprojection source.
//
// DWARF home: BrnPostFxShader.h:41-72 (this file), NOT BrnPostFx.h. Confirmed by the X360 asm:
// BrnPostFxShader::Render's 5th argument is `this + 0x450` and it reads a 4x4 float matrix at +0x00,
// a second at +0x40 and an int at +0x80 -- exactly Matrix44 (64 bytes) + Matrix44 (64 bytes) +
// EQuality. The shipped assert at Render+0x824096AC quotes the source expression
// "( lMotionBlurState.meQuality >= 0 ) && ( lMotionBlurState.meQuality < MotionBlurState::E_QUALITY_COUNT )"
// (BrnPostFxShader.cpp:798), which names both the variable and the enum.
//
// ⚠ THIS RETIRES `BrnPostFx::mau8MotionBlurStatePad[0x20]` (BrnPostFx.h:123). The real block is
// 0x84 bytes before maViewCache and 0x84 + 3 * sizeof(Matrix44Affine) with it -- 0x20 is far too
// small, so the committed BrnPostFx layout shears everything after it. Fixing that is the post-fx
// DRIVER's edit (BrnPostFx.h/.cpp), not this file's; it is listed in REPORT.md as a required
// companion change.
// --------------------------------------------------------------------------------------------------
struct MotionBlurState
{
    // DWARF BrnPostFxShader.h:44. E_QUALITY_COUNT == 2 is what the shipped assert bounds meQuality
    // against (`cmpwi cr6, r11, 2 / blt` @0x824096A4).
    enum EQuality
    {
        E_QUALITY_CHEAP     = 0,
        E_QUALITY_EXPENSIVE = 1,
        E_QUALITY_COUNT     = 2,
    };

    // DWARF BrnPostFxShader.h:53 / :61.
    //
    // Construct IS NOW BODIED, in this file's .cpp, where the DWARF puts it
    // (dwarfdump/GameSource/Graphics/PostFx/BrnPostFxShader.cpp:5, source line
    // "BrnPostFxShader.cpp:117"). ⚠ CORRECTION: an earlier revision of this comment said both
    // bodies "belong to the camera side". They do not -- both definitions are in THIS TU per the
    // DWARF; only MotionBlurState::Update's *caller* is on the camera side. Construct has no
    // standalone X360 symbol (the compiler inlined it into BrnPostFx::Construct @0x82409F80) and is
    // recovered from that caller's asm; see the banner over its definition.
    //
    // Update (@0x823F8490 -- a real X360 symbol, unlike Construct) is STILL declaration-only: it
    // rebuilds the current/previous world-view-projection pair from the view + projection and the
    // three-entry view cache, through the rw::math::fpu double-precision matrix family that does not
    // exist in this tree (the same blocker as BRN_POSTFX_MOTION_BLUR_REPROJECTION_AVAILABLE in the
    // .cpp). It is declared here because the type is this file's, and because Render's parameter
    // needs the complete layout.
    void Construct();
    void Update(const rw::math::vpu::Matrix44Affine& lView,
                const rw::math::vpu::Matrix44& lProjection,
                f32 lfTimeStep,
                EQuality leQuality);

    rw::math::vpu::Matrix44       mCurrentWVP;      // +0x00  DWARF :66  (Render reads 16 floats here)
    rw::math::vpu::Matrix44       mPreviousWVP;     // +0x40  DWARF :67  (Render reads 16 floats here)
    EQuality                      meQuality;        // +0x80  DWARF :68  (`lwz r11, 0x80(r30)`)
    rw::math::vpu::Matrix44Affine maViewCache[3];   // +0x84 on the CONSOLE (DWARF :72; Update only).
                                                    // HOST: Matrix44Affine is alignas(16) here, so it
                                                    // pads to +0x90 -- nothing in this build reads it
                                                    // by offset, and Update (still a stub) is its only
                                                    // user; do NOT carry 0x84 into any host reader.
};

// --------------------------------------------------------------------------------------------------
// BrnPostFxBloomData -- the bloom half of the composite's constants -- IS NOT DEFINED HERE.
//
// Its DWARF home is its OWN header, references/DecFIGS/dwarfdump/GameSource/Graphics/PostFx/
// BrnPostFxBloomData.h:31-35, so it lives at the mirrored path b5-decomp/src/GameSource/Graphics/
// PostFx/BrnPostFxBloomData.h and is `#include`d above (AGENTS.md: "Mirror original paths" /
// "Reconstruct includes; don't fake them"). An earlier revision of this file defined it inline; that
// was a second definition of a type the post-fx DRIVER agent creates at its real home in the same
// wave, i.e. an ODR collision (C2011 in BrnPostFx.cpp, which includes both). Fixed by deferring to
// that home, contents unchanged.
//
// What this file still needs from it is only what Render reads: the X360 composite's 2nd argument is
// `this + 0x950` and it loads a Vector4 at +0x00 (`lvx128 v0, r0, r5` @0x82408FD8) and a float at
// +0x14 (`lfs f13, 0x14(r5)` @0x82408FCC) -- mColour and mfLuminance.
// --------------------------------------------------------------------------------------------------

class BrnPostFxShader
{
public:
    // ----------------------------------------------------------------------------------------------
    // THE TWELVE PERMUTATIONS. `E_SHADER_COUNT == 12` is CONFIRMED by the shipped assert
    // "leShader < E_Shader_Count" (BrnPostFxShader.cpp:804, fired at @0x82409710 with `cmplwi r29,
    // 0xC`), and the index arithmetic at @0x824096C8-0x8240970C is
    //
    //     leShader = (lbEnableMotionBlur ? 4 * (meQuality + 1) : 0)   // 0, 4 or 8
    //              | (lbEnableDof     ? 2 : 0)
    //              | (lbEnableTint3d  ? 1 : 0)
    //
    // and the slot base is `mulli r11, r29, 0x3C` -- 60 guest bytes == one Shader.
    //
    // The names are the ORIGINAL ones: the DecFIGS build compiles the same twelve permutations as
    // twelve named shader blobs (references/DecFIGS/dwarfdump/GameProjects/.../CompiledShaders/
    // BrnPostFx*_{VS,PS}.h), and each name's feature list matches, exactly, the constants interned
    // in the X360 microcode package Construct puts in that slot (scratch/postfx_wave1b_dossiers/
    // DATA_DUMP.md): slot 0 carries no Sampler3dTint / no DofParams* / no BlurMatrix*, slot 1 adds
    // Sampler3dTint, slot 2 adds DofParamsA+B, slot 4 adds BlurMatrixX/Y/W, slot 8 is the same
    // constant signature as slot 4 with a different (larger-quality) body. Twelve for twelve.
    // ----------------------------------------------------------------------------------------------
    enum EShader
    {
        E_SHADER_BLOOM_VIGNETTE_TINT2D                      = 0,
        E_SHADER_BLOOM_TINT3D_VIGNETTE_TINT2D               = 1,
        E_SHADER_DOF_BLOOM_VIGNETTE_TINT2D                  = 2,
        E_SHADER_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D           = 3,
        E_SHADER_BLUR_BLOOM_VIGNETTE_TINT2D                 = 4,
        E_SHADER_BLUR_BLOOM_TINT3D_VIGNETTE_TINT2D          = 5,
        E_SHADER_BLUR_DOF_BLOOM_VIGNETTE_TINT2D             = 6,
        E_SHADER_BLUR_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D      = 7,
        E_SHADER_BLURHQ_BLOOM_VIGNETTE_TINT2D               = 8,
        E_SHADER_BLURHQ_BLOOM_TINT3D_VIGNETTE_TINT2D        = 9,
        E_SHADER_BLURHQ_DOF_BLOOM_VIGNETTE_TINT2D           = 10,
        E_SHADER_BLURHQ_DOF_BLOOM_TINT3D_VIGNETTE_TINT2D    = 11,

        E_SHADER_COUNT                                      = 12,
    };

    // The two shader-constant tables every permutation shares. Their SIZES are RECOVERED: they are
    // the X360 loop counts in Shader::Construct (`li r31, 2` @0x823FDAA0 walking
    // &VignetteCentreXyScaleXyPtr, `li r31, 0xA` @0x823FDAD4 walking &GlobalParamsPtr), and the DWARF
    // spells the arrays `ProgramVariableHandle[2] maVertexVariableHandle` /
    // `[10] maPixelVariableHandle` and the name tables `const char*[2] gaacVertexVariableNames` /
    // `const char*[10] gaacPixelVariableNames` (BrnPostFxShader.h:56/59). Every NAME below is an
    // ASCII string interned in the microcode packages themselves (DATA_DUMP.md) -- the SET is
    // recovered byte-exact.
    //
    // ⚠ HOW MUCH OF THE *ORDER* IS RECOVERED, STATED HONESTLY (this used to be overclaimed):
    //
    //   VERTEX table -- RECOVERED, both entries. IDA named the table after the string its FIRST
    //   element points at (VignetteCentreXyScaleXyPtr), which pins index 0; the table has exactly two
    //   entries and the packages intern exactly two vertex names, so index 1 is forced.
    //
    //   PIXEL table -- index 0 RECOVERED (same argument: the table symbol is GlobalParamsPtr, and
    //   Render's slot-0 value is 1/lfWhiteLevel, which only GlobalParams can be).
    //   INDICES 1..9 ARE INFERRED, not read out of .rdata. Two independent lines agree on the order
    //   below, which is why it is used, but neither is a dump of the pointer array:
    //     (a) SEMANTIC -- the asm pins which computed value lands in which of the ten contiguous
    //         stack Vector4s (laPixelShaderConstants, DWARF BrnPostFxShader.cpp:655), and each value
    //         admits exactly one of the ten interned names: the four projected focal planes -> a DoF
    //         params vector, {amount, 1/(A.y-A.x), 1/(A.w-A.z), 0} -> the second one, bloom colour x
    //         luminance -> BloomColour, the vignette pair whose .w lanes are the gradient MUL and ADD
    //         -> ...RgbPlusMul / ...RgbPlusAdd in that order (the names say which), the tint plus the
    //         brightness bias -> Tint2dColour, three rows of the reprojection matrix -> BlurMatrix*.
    //     (b) MICROCODE -- decoding the CTAB of the FULL permutation (index 7 / 11, the only ones
    //         that declare all ten float4s) gives register assignments c0..c9 in exactly this
    //         sequence: GlobalParams c0, DofParamsA c1, DofParamsB c2, BloomColour c3,
    //         VignetteInnerRgbPlusMul c4, VignetteOuterRgbPlusAdd c5, Tint2dColour c6, BlurMatrixX
    //         c7, BlurMatrixY c8, BlurMatrixW c9. (work/ctab_decode.py re-derives all 24 tables from
    //         DATA_DUMP.md; the full map is in REPORT.md.) fxc allocates in source-declaration order,
    //         so this is the order the constants were DECLARED in, not proof of the pointer array's
    //         order -- but the two orders coinciding is what the original source would look like.
    //   TO PROMOTE 1..9 TO RECOVERED: dump the ten dwords at `gaacPixelVariableNames`, the .rdata
    //   symbol IDA calls `GlobalParamsPtr` (its address is the @ha/@l pair at 0x823FDAC8 +
    //   0x823FDAD0), and follow each to its string. Listed as BLOCKED in REPORT.md.
    //   WHAT A TRANSPOSITION WOULD COST: nothing in permutation 0, which interns none of
    //   DofParamsA/B or BlurMatrixX/Y/W -- those five handles carry register count 0 and SetProgram
    //   skips them however they are ordered. It would matter the first time a DoF or blur
    //   permutation is selected.
    enum EVertexVariable
    {
        E_VS_VAR_VIGNETTE_CENTRE_XY_SCALE_XY = 0,
        E_VS_VAR_VIGNETTE_ANGLE              = 1,
        E_VS_VAR_COUNT                       = 2,
    };

    enum EPixelVariable
    {
        E_PS_VAR_GLOBAL_PARAMS                  = 0,
        E_PS_VAR_DOF_PARAMS_A                   = 1,
        E_PS_VAR_DOF_PARAMS_B                   = 2,
        E_PS_VAR_BLOOM_COLOUR                   = 3,
        E_PS_VAR_VIGNETTE_INNER_RGB_PLUS_MUL    = 4,
        E_PS_VAR_VIGNETTE_OUTER_RGB_PLUS_ADD    = 5,
        E_PS_VAR_TINT_2D_COLOUR                 = 6,
        E_PS_VAR_BLUR_MATRIX_X                  = 7,
        E_PS_VAR_BLUR_MATRIX_Y                  = 8,
        E_PS_VAR_BLUR_MATRIX_W                  = 9,
        E_PS_VAR_COUNT                          = 10,
    };

    // ----------------------------------------------------------------------------------------------
    // One permutation: its compiled vertex + pixel program and the shader-constant handles the
    // constant names resolved to. DWARF BrnPostFxShader.h:125-183.
    //
    // The GUEST object is 15 dwords (mpVertexProgram, mpPixelProgram, 2 + 10 four-byte handles, and
    // one dword holding the two PS3 register-count bytes). On the host the two pointers widen, so
    // the object is NOT 60 bytes and MUST NOT be indexed arithmetically -- maShaders[i] only.
    // ----------------------------------------------------------------------------------------------
    struct Shader
    {
        // X360 0x823FD970. Compile one permutation's vertex + pixel program pair out of `lpAllocator`
        // and resolve every shader-constant name against them.
        // luPs3RegisterCount is the PS3-only fragment-program register budget; ALL TWELVE X360 call
        // sites pass 0 (`li r9, 0` before each `bl`), and the X360 body never reads it.
        void Construct(rw::IResourceAllocator* lpAllocator,
                       const void* lpVertexProgram, u32 luVertexProgramSize,
                       const void* lpPixelProgram,  u32 luPixelProgramSize,
                       u8 luPs3RegisterCount);

        // X360 0x823FDB08. Release both programs and hand their resource blocks back.
        void Destruct(rw::IResourceAllocator* lpAllocator);

        // X360 0x823FDBB0. Bind this permutation and upload the constants whose names it actually
        // declares (a handle whose register COUNT is zero was not found in that permutation's
        // constant table and is skipped -- that is how the one shared 2 + 10 constant block feeds
        // twelve shaders with different constant sets).
        void SetProgram(const rw::math::vpu::Vector4* lpaVertexShaderConstants,
                        const rw::math::vpu::Vector4* lpaPixelShaderConstants);

        renderengine::ProgramBufferData*    mpVertexProgram;                        // DWARF :175
        renderengine::ProgramBufferData*    mpPixelProgram;                         // DWARF :176
        renderengine::ProgramVariableHandle maVertexVariableHandle[E_VS_VAR_COUNT]; // DWARF :177
        renderengine::ProgramVariableHandle maPixelVariableHandle[E_PS_VAR_COUNT];  // DWARF :178
        u8                                  muPixelShaderRegisterCountOriginal;     // DWARF :179
        u8                                  muPixelShaderRegisterCountModified;     // DWARF :180

        // [FLAG PC-platform leaf: program-buffer ownership] NOT A CONSOLE MEMBER, and it is not
        // pretending to be one -- the DWARF ends the struct at :180 and the guest object is 15 dwords.
        // It records WHICH OF THE TWO ROUTES built this slot's programs, because the two have
        // incompatible ownership and Destruct has no other way to tell them apart:
        //   false -> the console route (ProgramBuffer::GetResourceDescriptor -> allocator Create ->
        //            Initialize). The program pointer IS an allocator block and Destruct hands it back
        //            through IResourceAllocator::DoFree, exactly as the X360 does.
        //   true  -> renderengine::ProgramBufferPC_Adopt. The image lives in the PC leaf's bump arena
        //            (ImmediateModePCLeaf.cpp ArenaAlloc, which has no free), so the allocator never
        //            issued it and DoFree MUST NOT see it -- a concrete rw::LinearResourceAllocator
        //            would free a block it never carved. Documented leak-on-destruct of a boot-lifetime
        //            object; the full ownership story is the banner on Shader::Destruct.
        // The host layout of this struct already differs from the guest one (both program pointers
        // widen to 8 bytes), and nothing anywhere indexes a Shader by a guest stride, so adding a byte
        // here changes nothing that was ever console-shaped.
        bool                                mbAdoptedPC;
    };

    // X360 0x82401538 -- zero the five {resource, state pointer} pairs and the two motion-blur
    // sampler resource blocks. (The committed ctor already did exactly this; it spelled the same
    // stores "five stride-6 head elements + two stride-5 loop elements".)
    BrnPostFxShader();

    // X360 0x823FDD18 / 0x823FE3F0.
    void Construct(rw::IResourceAllocator* lpAllocator);
    void Destruct(rw::IResourceAllocator* lpAllocator);

    // ----------------------------------------------------------------------------------------------
    // X360 0x82408F08 -- THE COMPOSITE. Pick the permutation, build its 2 + 10 shader constants,
    // bind the render states / textures / samplers, and issue ONE full-screen quad.
    //
    // The parameter list and every parameter NAME are the DecFIGS DWARF's
    // (BrnPostFxShader.cpp:652); the X360 ARTIST asm confirms each one's ROLE and register:
    //   lfWhiteLevel        f1   1/lfWhiteLevel is GlobalParams.x; the caller passes 1.0f instead
    //                            when an override source texture is supplied.
    //   lBloomData          r5   `this+0x950` at the call site.
    //   lVignetteState      r6   `this+0x390`.
    //   lDofState           r7   `this+0x3D0`.
    //   lMotionBlurState    r8   `this+0x450`.
    //   lTint2dColour       v1   (`vmr128 v127, v2` / the v1 spill at arg_40 that the vignette-off
    //                            path reloads and splats .w from).
    //   lfBrightness        f2 ; lfContrast f3 ; lfAspectCorrection f4.
    //   the five bool8_t    stack, big-endian byte of each 8-byte slot: arg_67 Dof, arg_6F Bloom,
    //                            arg_77 Tint3d, arg_7F Vignette, arg_87 MotionBlur.
    //   the three Texture*  stack arg_8C / arg_94 / arg_9C -> D3D sampler units 0 / 1 / 2.
    //   the two TextureState* stack arg_A4 / arg_AC -> sampler units 3 / 4.
    //   lHalfPixelOffset    v2   added to every emitted UV.
    //   lbBilinearSource    stack arg_CF -- selects the LINEAR sampler for unit 0 instead of POINT.
    // ----------------------------------------------------------------------------------------------
    void Render(f32 lfWhiteLevel,
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
                bool lbBilinearSource);

private:
    Shader                         maShaders[E_SHADER_COUNT];              // DWARF :206

    rw::Resource                   mVertexDescriptorResource;              // DWARF :208
    renderengine::VertexDescriptorData* mpVertexDescriptor;                 // DWARF :209 (guest dword 185)

    rw::Resource                   mBlendStateResource;                    // DWARF :211
    renderengine::BlendMaterialState*   mpBlendState;                       // DWARF :212 (guest dword 191)

    rw::Resource                   mDepthStencilStateResource;             // DWARF :214
    renderengine::DepthStencilState*    mpDepthStencilState;                // DWARF :215 (guest dword 197)

    rw::Resource                   mSamplerStateResource_Point;            // DWARF :217
    renderengine::SamplerStateData*     mpSamplerState_Point;               // DWARF :218 (guest dword 203)

    rw::Resource                   mSamplerStateResource_Linear;           // DWARF :220
    renderengine::SamplerStateData*     mpSamplerState_Linear;              // DWARF :221 (guest dword 209)

    rw::Resource                   maSamplerStateResource_MotionBlur[MotionBlurState::E_QUALITY_COUNT]; // DWARF :223
    renderengine::SamplerStateData*     mapSamplerState_MotionBlur[MotionBlurState::E_QUALITY_COUNT];   // DWARF :224
};

#endif // BRN_POST_FX_SHADER_H
