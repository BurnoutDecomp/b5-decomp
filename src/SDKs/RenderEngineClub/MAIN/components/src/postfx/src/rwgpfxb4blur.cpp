#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.h"   // B4Blur::State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"   // PfxHelper::CreateProgram

#include <cmath>   // std::pow

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::B4Blur::State::State             @ 0x823F52E0
//   rw::graphics::postfx::B4Blur::State::SetBlendSharpness @ 0x823F53A8
//   rw::graphics::postfx::B4Blur::B4Blur                   @ 0x823FE9C8
//   rw::graphics::postfx::B4Blur::Parameters::Parameters   -- INLINED at BrnPostFx::Construct
//
// THAT IS THE WHOLE CLASS IN THIS BUILD, AND IT IS A FINDING. The DWARF also declares
// B4Blur::{GetResourceDescriptor, Initialize, Release, GetAllocator, CommitRenderTargetToBackBuffer,
// DownSample, Apply, RenderBlurQuad, RenderRadialQuad, RenderScatterQuad, CalculateDownSampleTaps,
// Min, Max} (references/DecFIGS/dwarfdump/.../include/postfx/rwgpfxb4blur.h:173-219), but NONE of
// them exists in the X360 image: grepping the "name" field of every .ida-exports JSON for B4Blur
// returns exactly the THREE standalone functions above (Parameters::Parameters is inlined, so it has
// no export of its own), and progress/status.json holds the same three. So on ARTIST
// the effect is CONSTRUCTED and never DRIVEN -- there is no pass to reconstruct, no pass order to
// confirm, and adopting the eight programs below cannot change a pixel on any build. What it does
// change is that the eight slots stop being empty and the class's programs become real for whoever
// lands the passes (they would have to come from the PS3 build, which does have the bodies).

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x823F52E0.
    // The ctor builds four {x, y, 0, 0} vectors on the stack and writes them with stvx128 at
    // +0x00/+0x10/+0x20/+0x30, then writes the six trailing scalars with stfs. The .rdata floats
    // it loads are flt_82001C98 == 1.0, flt_82001DA0 == 0.5, flt_82001CC0 == 0.0 and
    // flt_8200CE04 == 0.005 (the decompiler shows the last as 0.0049999999, the single-precision
    // image of 0.005). The vector lanes z/w are the 0.0 the SIMD slot carries.
    B4Blur::State::State()
    {
        m_blendAmount.x = 1.0f;  m_blendAmount.y = 1.0f;  m_blendAmount.z = 0.0f;  m_blendAmount.w = 0.0f;
        m_blurAmount.x  = 1.0f;  m_blurAmount.y  = 1.0f;  m_blurAmount.z  = 0.0f;  m_blurAmount.w  = 0.0f;
        m_blendCenter.x = 0.5f;  m_blendCenter.y = 0.5f;  m_blendCenter.z = 0.0f;  m_blendCenter.w = 0.0f;
        m_blurCenter.x  = 0.5f;  m_blurCenter.y  = 0.5f;  m_blurCenter.z  = 0.0f;  m_blurCenter.w  = 0.0f;

        m_blurOpacity   = 1.0f;   // +0x40
        m_blurVelocity  = 1.0f;   // +0x44
        m_blendSharpMUL = 0.0f;   // +0x48
        m_blendSharpADD = 0.0f;   // +0x4C
        m_blendNoise    = 0.005f; // +0x50
        m_blendAngle    = 0.0f;   // +0x54
    }

    // X360 0x823F53A8.
    // f13 = (lfSharpness + 1.0) * 0.5            ; remap [-1,1] -> [0,1] (computed in double)
    // f1  = pow(f13, 16.0)                       ; bl sub_82C09970 (the CRT pow(double,double))
    // f13 = frsp(f1)                             ; round the pow result down to single
    // f0  = f13 * 500.0                          ; single-precision scale (flt_8200A034 == 500.0)
    // m_blendSharpMUL = f0 + 1.0                 ; stfs 0x48  (flt_82001C98 == 1.0)
    // m_blendSharpADD = -f0                      ; stfs 0x4C
    // The eight phantom integer "args" Hex-Rays shows on the call are leftover GPRs from the merged
    // pow signature, not real parameters; pow consumes only the two doubles (the value and 16.0).
    void B4Blur::State::SetBlendSharpness(f32 lfSharpness)
    {
        const f32 lfPower = static_cast<f32>(
            std::pow((static_cast<double>(lfSharpness) + 1.0) * 0.5, 16.0));   // frsp of pow(...)
        const f32 lfScaled = lfPower * 500.0f;

        m_blendSharpMUL = lfScaled + 1.0f;   // +0x48
        m_blendSharpADD = -lfScaled;         // +0x4C
    }
}
}
}

// ================================================================================================
// rw::graphics::postfx::B4Blur::B4Blur @0x823FE9C8 and B4Blur::Parameters::Parameters (inlined).
// Goes into b5-decomp/src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.cpp.
// ================================================================================================

// ---- THE SEVEN PC PROGRAM IMAGES ---------------------------------------------------------------
// [FLAG PC-platform leaf: shader programs] The D3D9 counterparts of the EIGHT Xenos packages this
// TU compiles, wrapped as platform-4 ShaderProgramBuffer images by the generated leaf
// pc/gcm/renderengine/PostFxB4BlurProgramsPC.cpp (sibling of PostFxBloomProgramsPC.cpp and
// PostFxHelperProgramsPC.cpp). Declared here as the minimal external surface -- the same convention
// rwgpfxhelper.cpp:120-131 and BrnPostFxBloom.cpp already use for their own generated leaves; there
// is no header for these.
//
// The eight console packages (scratch/postfx_step6_producers/PACKAGES_DUMP_B4BLUR.md, dumped from
// ARTIST_copy.i64 -- every dumped size matches the byte count each call site below already records)
// were disassembled with tools/assets/shaders/xenos.py, re-expressed as HLSL in
// tools/assets/shaders/brn_postfx_b4blur.fx against the CTAB interned in each package, and compiled
// with fxc. The PC variable table was compared against the console CTAB entry by entry (name,
// register set, register index, register count, class, type): IDENTICAL for all eight.
//
// ⚠ SEVEN IMAGES, EIGHT CALL SITES. 0x82045600 (scatter vertex) and 0x82045AC0 (radial vertex) are
// BYTE-IDENTICAL -- 324 bytes, sha1 9db13387ba196d6f185017f5b3053bda4d2835c0 both -- so one array
// serves both sites. That still builds TWO ProgramBuffer objects, exactly as the console does with
// its two copies of the same package; nothing is shared but the source image.
//
// ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES. ProgramBufferPC_Adopt
// bounds-checks the blob against the size it is handed and copies exactly that many bytes, so a
// console size here would either refuse a valid image or read past the array. Nothing below
// hard-codes a number: each size is read from the generated TU's own published constant, whose
// static_assert ties it to sizeof() of the array.
namespace renderengine
{
    extern const u8  gauPostFxB4BlurQuadVertexProgramPC[];
    extern const u32 guPostFxB4BlurQuadVertexProgramPCSize;
    extern const u8  gauPostFxB4BlurScatterVertexProgramPC[];
    extern const u32 guPostFxB4BlurScatterVertexProgramPCSize;
    extern const u8  gauPostFxB4BlurScatterPixelProgramPC[];
    extern const u32 guPostFxB4BlurScatterPixelProgramPCSize;
    extern const u8  gauPostFxB4BlurRadialPixelProgramPC[];
    extern const u32 guPostFxB4BlurRadialPixelProgramPCSize;
    extern const u8  gauPostFxB4BlurTexturePixelProgramPC[];
    extern const u32 guPostFxB4BlurTexturePixelProgramPCSize;
    extern const u8  gauPostFxB4BlurDownSamplePixelProgramPC[];
    extern const u32 guPostFxB4BlurDownSamplePixelProgramPCSize;
    extern const u8  gauPostFxB4BlurBlurPixelProgramPC[];
    extern const u32 guPostFxB4BlurBlurPixelProgramPCSize;
}

namespace
{
    // The vertex-format element codes the two vertex descriptors are built from. These are the
    // asm's own immediates (`li`/`lis`+`ori` into the Parameters element words); they are opaque
    // renderengine format codes, carried verbatim.
    // (Verify pass 2026-08-15: the first two HEX values were mis-typed against their own decimal
    // comments -- 2761657 == 0x2A23B9 and 2892709 == 0x2C23A5, the same FLOAT3 / FLOAT2 codes every
    // other post-fx quad uses; the boot log showed the blur/scatter descriptors dropping elements.)
    const s32 KI_VERTEX_FORMAT_POSITION = 0x002A23B9;   // 2761657  FLOAT3
    const s32 KI_VERTEX_FORMAT_UV       = 0x002C23A5;   // 2892709  FLOAT2
    const s32 KI_VERTEX_FORMAT_EXTRA    = 0x001A23A6;   // 1713062  FLOAT4

    void ClearHandle(renderengine::ProgramVariableHandle& lrHandle)
    {
        lrHandle.mu8RegisterSet   = 0u;
        lrHandle.mu8RegisterIndex = 0u;
        lrHandle.mu8ShaderType    = 0u;
        lrHandle.mu8RegisterCount = 0u;
    }

    // The console's `ProgramBuffer::GetVariableHandleByName(program, name, &handle)` plus the ONE
    // guard the PC bring-up needs (the committed lookup dereferences lpData unconditionally,
    // programbuffer.cpp:263). Not a console behaviour -- it only matters if an adopt is refused.
    //
    // A MISS IS NOT AN ERROR AND ONE OF THE EIGHT BINDS BELOW LEGITIMATELY MISSES: the blur pixel
    // program has no "controlScalars" constant on either side (see the note at that call site).
    // GetVariableHandleByName writes a zero-count handle when the name is not in the table
    // (programbuffer.cpp:299), which is exactly what the console gets.
    void BindProgramVariable(const renderengine::ProgramBufferData* lpProgram, const char* lpcName,
                             renderengine::ProgramVariableHandle& lrHandle)
    {
        if (lpProgram == nullptr)
        {
            ClearHandle(lrHandle);
            return;
        }
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpProgram, reinterpret_cast<const u8*>(lpcName), &lrHandle);
    }

    // rw::Resource has no constructor in this tree (it is BaseResources<4>, a POD); the console
    // spells the same thing as rw::Resource::Resource(), which the DWARF shows SafeRelease<> calling
    // and which the B4Blur constructor runs five times up front (the five `*(a1+N) = 0` runs at
    // 0x823FEA00-0x823FEA6C, each writing five words then re-writing word 0).
    void ClearResource(rw::Resource& lrResource)
    {
        for (u32 luSlot = 0u; luSlot < 4u; ++luSlot)
        {
            lrResource.m_baseResources[luSlot] = nullptr;
        }
    }

    // The depth/stencil parameter block BOTH of B4Blur's states use -- byte-identical between the
    // two sites (asm v44[] and v37[]) and byte-identical to PfxHelper's: function 3, both stencil
    // functions 7 (ALWAYS), all four stencil masks -1, everything else 0, all six flag bytes 0
    // (depth test off, depth write off, stencil off). Written once instead of twice
    // (AGENTS.md inlining/duplication reversal); every value is an asm immediate.
    void BuildPostFxDepthStencilParameters(renderengine::DepthStencilState::Parameters& lrParameters)
    {
        lrParameters = renderengine::DepthStencilState::Parameters();
        lrParameters.muFunction         = 3u;                     // [0]
        lrParameters.maState1[0]        = 0u;                     // [1..3]
        lrParameters.maState1[1]        = 0u;
        lrParameters.maState1[2]        = 0u;
        lrParameters.muState4           = 7u;                     // [4] E_FUNCTION_ALWAYS
        lrParameters.maState5[0]        = 0u;                     // [5..7]
        lrParameters.maState5[1]        = 0u;
        lrParameters.maState5[2]        = 0u;
        lrParameters.muState8           = 7u;                     // [8] E_FUNCTION_ALWAYS
        lrParameters.muState9           = 0u;                     // [9]
        lrParameters.muState10          = 0u;                     // [10]
        lrParameters.muStencilReadMask  = static_cast<u32>(-1);   // [11]
        lrParameters.muStencilWriteMask = static_cast<u32>(-1);   // [12]
        lrParameters.muState13          = 0u;                     // [13]
        lrParameters.muState14          = static_cast<u32>(-1);   // [14]
        lrParameters.muState15          = static_cast<u32>(-1);   // [15]
        lrParameters.muState16          = 0u;                     // [16]
        lrParameters.mbDepthTestEnable  = 0u;
        lrParameters.mbDepthWriteEnable = 0u;
        lrParameters.mu8Flag2           = 0u;
        lrParameters.mu8Flag3           = 0u;
        lrParameters.mu8Flag4           = 0u;
        lrParameters.mu8Flag5           = 0u;
    }
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // rwgpfxb4blur.h:132. INLINED at the X360 call site: BrnPostFx::Construct calls
    // B4Blur::State::State on the block's m_state (`bl ...B4Blur__State__State` @0x8240A2F4 with
    // r3 = the parameter block's +0x10) and emits NOTHING else -- m_allocator and
    // m_scatterBlendState are written by the caller afterwards (0x8240A330 / 0x8240A36C). So the
    // constructor's whole body is the member's own default construction, and the two pointers are
    // left uninitialised exactly as the console leaves them.
    B4Blur::Parameters::Parameters()
    {
    }

    // ============================================================================================
    // B4Blur::B4Blur @0x823FE9C8
    //
    // ⚠ SIGNATURE CORRECTION (was `B4Blur(rw::IResourceAllocator**)`). BrnPostFx::Construct passes
    // `addi r4, r1, 0x300+var_170` @0x8240A3C0, and var_170 is the base of the PARAMETER BLOCK the
    // caller filled at 0x8240A330 (m_allocator), 0x8240A36C (m_scatterBlendState) and 0x8240A314
    // (the twelve-doubleword m_state copy). The callee then reads `*a2` (allocator), `a2[1]`
    // (scatter blend state) and `a2 + 4` (the 96-byte state) -- three DIFFERENT members of one
    // block, which an `IResourceAllocator**` cannot name. The DWARF agrees
    // (rwgpfxb4blur.h:196, `B4Blur(const Parameters&)`), so the declaration is corrected here and
    // the call site in BrnPostFx.cpp with it.
    //
    // THE LAYOUT THIS BODY WRITES IS PINNED BY THE ASM, MEMBER BY MEMBER, and it agrees with the
    // DWARF's declaration order exactly (rwgpfxb4blur.h:254-308):
    //   +0x00 m_state (0x60 on the guest -- the memcpy at the tail moves 96 bytes)
    //   +0x60 m_blurProgram      +0x64 m_downsampleProgram
    //   +0x68 m_rasterizerStateResource (5 words)   +0x7C m_rasterizerState
    //   +0x80 m_textureProgram   +0x84 m_blendFactorTextureHandle
    //   +0x88 m_sampleOffsetHandle  +0x8C m_controlScalarsHandle
    //   +0x90 m_quadVertexDescriptorResource        +0xA4 m_quadVertexDescriptor
    //   +0xA8 m_quadDepthStencilStateResource       +0xBC m_quadDepthStencilState
    //   +0xC0 m_quadVertexProgram
    //   +0xC4 m_scatterVertexDescriptorResource     +0xD8 m_scatterVertexDescriptor
    //   +0xDC m_scatterDepthStencilStateResource    +0xF0 m_scatterDepthStencilState
    //   +0xF4 m_scatterVertexProgram   +0xF8 m_scatterUVOffsetHandle
    //   +0xFC m_scatterPixelProgram    +0x100/+0x104 m_scatterScalars1/2Handle
    //   +0x108 m_scatterBlendState
    //   +0x10C m_radialVertexProgram   +0x110 m_radialUVOffsetHandle
    //   +0x114 m_radialPixelProgram    +0x118/+0x11C m_radialScalars1/2Handle
    //   +0x120 m_allocator
    // -- 0x124 rounded to the class's 16-byte alignment = 0x130, which is exactly the carve size
    // BrnPostFx::Construct requests (`li r11, 0x130` @0x8240A328). That is the independent check
    // that no member is missing and none is invented.
    //
    // ⚠ AND IT IS WHY THE COMMITTED CARVE WAS UNDER-SIZED. Until this header modelled the members,
    // `sizeof(B4Blur)` was 1 (an empty class), so BrnPostFx::Construct asked the allocator for ONE
    // BYTE and placement-new'd a 0x130-guest / ~0x1F0-host object into it. The carve reads
    // `sizeof(rw::graphics::postfx::B4Blur)` and therefore fixes itself with this header, exactly as
    // the driver's banner predicted.
    //
    // Every member is reached BY NAME; no guest offset survives to the host, and the four leading
    // `rw::Resource` members are host-width (4 slots, rwcore.pdb x64) against the console's 5 --
    // the documented cross-build delta.
    // ============================================================================================
    void B4Blur::SetState(const State& lrState)
    {
        m_state = lrState;
    }

    B4Blur::B4Blur(const Parameters& lrParameters)
    {
        // asm 0x823FE9E0: `B4Blur::State::State(a1)` -- m_state is default-constructed FIRST and
        // then assigned from the parameters at the very end (the tail memcpy). Reproduced in that
        // order because the intermediate value is observable to nothing but is what the source said.
        // (m_state's own constructor runs implicitly; the explicit re-seed is the tail assignment.)

        // asm 0x823FEA00-0x823FEA6C: the five resource handles are cleared before anything is built.
        ClearResource(m_rasterizerStateResource);          // +0x68
        ClearResource(m_quadVertexDescriptorResource);     // +0x90
        ClearResource(m_quadDepthStencilStateResource);    // +0xA8
        ClearResource(m_scatterVertexDescriptorResource);  // +0xC4
        ClearResource(m_scatterDepthStencilStateResource); // +0xDC

        // asm 0x823FEA70 `*(a1 + 288) = *a2` -- the allocator, before any allocation.
        m_allocator = lrParameters.m_allocator;

        // ---- the BLUR quad's vertex descriptor (asm 0x823FEA78-0x823FEB40) -------------------------
        // Five elements: one position + four extra streams (usage indices 1, 6, 7, 8, 9).
        {
            renderengine::VertexDescriptor::Parameters lParameters;
            lParameters.maElements[0].mu16Stream    = 0;
            lParameters.maElements[0].miOffset      = KI_VERTEX_FORMAT_POSITION;
            lParameters.maElements[0].mu8UsageIndex = 1;
            lParameters.maElements[1].mu16Stream    = 0;
            lParameters.maElements[1].miOffset      = KI_VERTEX_FORMAT_EXTRA;
            lParameters.maElements[1].mu8UsageIndex = 6;
            lParameters.maElements[2].mu16Stream    = 0;
            lParameters.maElements[2].miOffset      = KI_VERTEX_FORMAT_EXTRA;
            lParameters.maElements[2].mu8UsageIndex = 7;
            lParameters.maElements[3].mu16Stream    = 0;
            lParameters.maElements[3].miOffset      = KI_VERTEX_FORMAT_EXTRA;
            lParameters.maElements[3].mu8UsageIndex = 8;
            lParameters.maElements[4].mu16Stream    = 0;
            lParameters.maElements[4].miOffset      = KI_VERTEX_FORMAT_EXTRA;
            lParameters.maElements[4].mu8UsageIndex = 9;

            rw::BaseResourceDescriptors<5> lDescriptor;
            renderengine::VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lParameters);
            m_quadVertexDescriptorResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_quadVertexDescriptor = renderengine::VertexDescriptor::Initialize(
                &m_quadVertexDescriptorResource, &lParameters);
        }

        // ---- the BLUR quad's depth/stencil state (asm 0x823FEB44-0x823FEBF8) ------------------------
        {
            renderengine::DepthStencilState::Parameters lParameters;
            BuildPostFxDepthStencilParameters(lParameters);

            renderengine::ResourceDescriptor5 lDescriptor;
            renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParameters);
            m_quadDepthStencilStateResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_quadDepthStencilState = renderengine::DepthStencilState::Initialize(
                reinterpret_cast<renderengine::DepthStencilState**>(&m_quadDepthStencilStateResource),
                &lParameters);
        }

        // asm 0x823FEC0C: the blur quad's vertex program.
        //   CreateProgram(0, &unk_82045148, 0x15C=348) -- no named constant is bound to it, and the
        //   package's CTAB really does declare NONE. It is a pure pass-through: POSITION with w
        //   forced to 1 into oPos, and the four FLOAT4 streams (usage indices 6/7/8/9, the four
        //   KI_VERTEX_FORMAT_EXTRA elements declared just above) into TEXCOORD0..3. Those four
        //   float4s carry the blur pixel program's SEVEN tap UVs -- which is why the blur program
        //   needs no offset uniform. The DWARF corroborates: B4Blur::RenderBlurQuad builds a
        //   VertexIterator5<VertexTypeFloat3, VertexTypeFloat4 x4> (dwarfdump rwgpfxb4blur.cpp:270).
        m_quadVertexProgram = PfxHelper::CreateProgram(
            0, renderengine::gauPostFxB4BlurQuadVertexProgramPC,
            renderengine::guPostFxB4BlurQuadVertexProgramPCSize, m_allocator);

        // ---- the SCATTER quad's vertex descriptor (asm 0x823FEC10-0x823FECB4) ----------------------
        // Three elements: position, uv, one extra (usage indices 1, 6, 7).
        {
            renderengine::VertexDescriptor::Parameters lParameters;
            lParameters.maElements[0].mu16Stream    = 0;
            lParameters.maElements[0].miOffset      = KI_VERTEX_FORMAT_POSITION;
            lParameters.maElements[0].mu8UsageIndex = 1;
            lParameters.maElements[1].mu16Stream    = 0;
            lParameters.maElements[1].miOffset      = KI_VERTEX_FORMAT_UV;
            lParameters.maElements[1].mu8UsageIndex = 6;
            lParameters.maElements[2].mu16Stream    = 0;
            lParameters.maElements[2].miOffset      = KI_VERTEX_FORMAT_EXTRA;
            lParameters.maElements[2].mu8UsageIndex = 7;

            rw::BaseResourceDescriptors<5> lDescriptor;
            renderengine::VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lParameters);
            m_scatterVertexDescriptorResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_scatterVertexDescriptor = renderengine::VertexDescriptor::Initialize(
                &m_scatterVertexDescriptorResource, &lParameters);
        }

        // ---- the SCATTER quad's depth/stencil state (asm 0x823FECB8-0x823FED70) --------------------
        {
            renderengine::DepthStencilState::Parameters lParameters;
            BuildPostFxDepthStencilParameters(lParameters);

            renderengine::ResourceDescriptor5 lDescriptor;
            renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParameters);
            m_scatterDepthStencilStateResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_scatterDepthStencilState = renderengine::DepthStencilState::Initialize(
                reinterpret_cast<renderengine::DepthStencilState**>(&m_scatterDepthStencilStateResource),
                &lParameters);
        }

        // asm 0x823FED7C `*(a1 + 264) = a2[1]` -- the scatter blend state, taken straight from the
        // parameter block. On the console BrnPostFx::Construct fills that word from
        // dword_83010F74 == CgsBlendStateFactory::saBlendStates[1] ==
        // E_FACTORY_BLEND_STATE_TRANSPARENT_MODULATE_NO_ALPHA_TEST_DEST_RGBA
        // (CgsBlendStateFactory.h:151). The caller cannot name that slot yet (private static behind a
        // non-static accessor -- driver REPORT.md section 6), so on this build the member carries
        // whatever Parameters::Parameters() left. IDENTIFIED, not unattested; the plumbing is in the
        // factory TU, and NO parallel global is minted here.
        m_scatterBlendState = lrParameters.m_scatterBlendState;

        // ---- the scatter programs (asm 0x823FED80-0x823FEE18) ---------------------------------------
        //   CreateProgram(0, &unk_82045600, 0x144=324) -- POSITION pass-through, TEXCOORD0 = uv +
        //   uvOffset.xy, TEXCOORD1 = the per-vertex FLOAT4 the scatter pixel program reads.
        m_scatterVertexProgram = PfxHelper::CreateProgram(
            0, renderengine::gauPostFxB4BlurScatterVertexProgramPC,
            renderengine::guPostFxB4BlurScatterVertexProgramPCSize, m_allocator);
        BindProgramVariable(m_scatterVertexProgram, "uvOffset", m_scatterUVOffsetHandle);

        //   CreateProgram(1, &unk_82045748, 0x26C=620) -- ONE tap of low_texture displaced radially
        //   away from scatterScalars1.xy by a value-noise amount scaled by scatterScalars1.w and a
        //   [0,1] falloff built from |TEXCOORD1.xy|; alpha is a second saturate built from
        //   |TEXCOORD1.zw|. Recovered instruction by instruction in brn_postfx_b4blur.fx.
        m_scatterPixelProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxB4BlurScatterPixelProgramPC,
            renderengine::guPostFxB4BlurScatterPixelProgramPCSize, m_allocator);
        BindProgramVariable(m_scatterPixelProgram, "scatterScalars1", m_scatterScalars1Handle);
        BindProgramVariable(m_scatterPixelProgram, "scatterScalars2", m_scatterScalars2Handle);

        // ---- the radial programs (asm 0x823FEE1C-0x823FEEB4) ----------------------------------------
        //   CreateProgram(0, &unk_82045AC0, 0x144=324). ⚠ THE SAME 324 BYTES AS 0x82045600 -- the
        //   two packages are byte-identical, so the same PC array is adopted here. Two separate
        //   ProgramBuffer objects still result, exactly as on the console.
        m_radialVertexProgram = PfxHelper::CreateProgram(
            0, renderengine::gauPostFxB4BlurScatterVertexProgramPC,
            renderengine::guPostFxB4BlurScatterVertexProgramPCSize, m_allocator);
        BindProgramVariable(m_radialVertexProgram, "uvOffset", m_radialUVOffsetHandle);

        //   CreateProgram(1, &unk_82045C08, 0x2CC=716) -- EIGHT taps of low_texture marched from the
        //   pixel toward scatterScalars1.xy at scatterScalars2.x + i*scatterScalars2.y (i = 0..7),
        //   averaged with 1/8: the radial "speed streak" blur.
        m_radialPixelProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxB4BlurRadialPixelProgramPC,
            renderengine::guPostFxB4BlurRadialPixelProgramPCSize, m_allocator);
        BindProgramVariable(m_radialPixelProgram, "scatterScalars1", m_radialScalars1Handle);
        BindProgramVariable(m_radialPixelProgram, "scatterScalars2", m_radialScalars2Handle);

        // ---- the texture / down-sample / blur programs (asm 0x823FEEB8-0x823FEF40) -------------------
        // ⚠ THE TEXTURE PROGRAM'S ALLOCATOR OVERRIDE IS 0, and that is not a transcription slip: the
        // asm is `li r6, 0` at this site alone (`CreateProgram(1, &unk_820459B8, 260, 0)`), while the
        // other seven pass `*(a1 + 288)`. A null override makes CreateProgram fall back to the
        // PfxHelper singleton's own allocator, which on this build is the same object -- but the
        // console distinction is preserved rather than normalised away.
        //   CreateProgram(1, &unk_820459B8, 0x104=260) -- a modulated copy:
        //   oC0 = float4(tex2D(tex, uv).rgb * blendFactor.x, blendFactor.x). The sampler's CTAB name
        //   really is "tex" here, not "low_texture"/"scene_texture"; samplers are bound by UNIT, so
        //   the name only has to match so the two descriptor tables compare equal.
        m_textureProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxB4BlurTexturePixelProgramPC,
            renderengine::guPostFxB4BlurTexturePixelProgramPCSize, nullptr);
        BindProgramVariable(m_textureProgram, "blendFactor", m_blendFactorTextureHandle);

        //   CreateProgram(1, &unk_820454E0, 0x120=288) -- ⚠ ONE TAP, NOT SIXTEEN, on X360: the
        //   package's CTAB declares sampleOffsets as `matrix_rows float[4x4]` but with
        //   RegisterCount 1, and the microcode is a single `add uv, uv, c0.xy` plus a single fetch.
        //   The PS3 side of the same source fills sixteen floats (DecFIGS B4Blur::DownSample's
        //   `float[16] blur4SampleOffsets`, dwarfdump rwgpfxb4blur.cpp:849), so the X360 build
        //   compiled a narrower variant of the program. The PC image matches X360, register count
        //   included, so this handle stays a ONE-row handle on both sides.
        m_downsampleProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxB4BlurDownSamplePixelProgramPC,
            renderengine::guPostFxB4BlurDownSamplePixelProgramPCSize, m_allocator);
        BindProgramVariable(m_downsampleProgram, "sampleOffsets", m_sampleOffsetHandle);

        //   CreateProgram(1, &unk_820452A8, 0x234=564) -- a SEVEN-tap combine of per-vertex UVs
        //   (TEXCOORD0..3 carry eight uv pairs; the program fetches seven of them).
        //
        //   ⚠ THIS BIND MISSES, ON THE CONSOLE TOO, AND THAT IS THE CORRECT BEHAVIOUR. The asm asks
        //   for "controlScalars" by name (`aControlscalars` @0x823FEF0C-0x823FEF1C) and this line is
        //   a faithful transcription of that call -- but the compiled package's CTAB declares
        //   exactly ONE entry, the `low_texture` sampler. The three blur weights the name refers to
        //   were folded into the program's LITERAL block by the shader compiler
        //   (c255 = {0.75, 0.5, 0.66, 0}), so GetVariableHandleByName finds nothing and
        //   m_controlScalarsHandle is left a zero-count "not found" handle. The PC image reproduces
        //   that exactly -- the weights are literals there too and no `controlScalars` uniform is
        //   declared -- because giving the PC program a uniform the console program does not have
        //   would make a push that is a no-op on the console start moving pixels here.
        m_blurProgram = PfxHelper::CreateProgram(
            1, renderengine::gauPostFxB4BlurBlurPixelProgramPC,
            renderengine::guPostFxB4BlurBlurPixelProgramPCSize, m_allocator);
        BindProgramVariable(m_blurProgram, "controlScalars", m_controlScalarsHandle);

        // ---- the scatter rasterizer state (asm 0x823FEF44-0x823FEFF8) --------------------------------
        // Every word is an asm immediate: fill 0, cull 0, both depth-bias floats 0.0, multisample -1,
        // antialiased-line 0xFFFF, scissor off, depth-clip on, front-face CCW, conservative off,
        // padding-mode 1.
        {
            renderengine::RasterizerState::Parameters lParameters = {};
            lParameters.muFillMode              = 0u;                    // v31[0]
            lParameters.muCullMode              = 0u;                    // v31[1]
            lParameters.muDepthBias             = 0u;                    // v31[2] == 0.0f
            lParameters.muSlopeScaledDepthBias  = 0u;                    // v31[3] == 0.0f
            lParameters.muMultisampleEnable     = static_cast<u32>(-1);  // v31[4]
            lParameters.muAntialiasedLineEnable = 0xFFFFu;               // v31[5]
            lParameters.mu8ScissorEnable        = 0u;                    // v32
            lParameters.mu8DepthClipEnable      = 1u;                    // v33
            lParameters.mu8FrontCounterClockwise= 1u;                    // v34
            lParameters.mu8ConservativeRaster   = 0u;                    // v35
            lParameters.mu8PaddingMode          = 1u;                    // v36

            renderengine::ResourceDescriptor5 lDescriptor;
            renderengine::RasterizerState::GetResourceDescriptor(&lDescriptor, &lParameters);
            m_rasterizerStateResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_rasterizerState = renderengine::RasterizerState::Initialize(
                reinterpret_cast<renderengine::RasterizerState**>(&m_rasterizerStateResource),
                &lParameters);
        }

        // asm 0x823FEFFC `memcpy(a1, a2 + 4, 96)` -- 96 == the guest sizeof(State) (four 16-byte
        // Vector2 + six floats, padded to the 16-byte alignment). Copy-assignment reproduces it
        // without carrying a guest byte count to the host.
        m_state = lrParameters.m_state;
    }
}
}
}
