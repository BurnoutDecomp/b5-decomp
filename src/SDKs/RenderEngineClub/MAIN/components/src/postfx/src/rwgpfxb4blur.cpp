#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxb4blur.h"   // B4Blur::State
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"   // PfxHelper::CreateProgram

#include <cmath>   // std::pow

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::B4Blur::State::State             @ 0x823F52E0
//   rw::graphics::postfx::B4Blur::State::SetBlendSharpness @ 0x823F53A8
//   rw::graphics::postfx::B4Blur::B4Blur                   @ 0x823FE9C8
//   rw::graphics::postfx::B4Blur::Parameters::Parameters   -- INLINED at BrnPostFx::Construct

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

namespace
{
    // ---- PC BRING-UP GATE: the embedded Xenos microcode (same wall as rwgpfxhelper.cpp) ---------
    // See the long note in rwgpfxhelper.cpp: the console route through
    // renderengine::ProgramBuffer::Initialize calls XGGetMicrocodeShaderParts, whose PC stub returns
    // 0 without writing *lpParts (ImmediateModePCLeaf.cpp:626-646), and then reads it. The gate lives
    // in PfxHelper::CreateProgram, which is the single funnel every one of the eight sites below goes
    // through, so this TU keeps the console's call structure verbatim and inherits the honest-empty
    // behaviour: a null image in, a null program out, and zero-count handles.
    //
    // [FLAG BLOCKED: EIGHT Xenos microcode packages, all absent from the function-only IDA export and
    //  from scratch/postfx_wave1b_dossiers/DATA_DUMP.md. Extractable from the ARTIST .i64 exactly the
    //  way rwgpfxrendertargetdebugger.cpp already extracted its own pair. Address, size, stage and
    //  the named constants each one must expose:
    //    0x82045148  348  vertex  (quad vertex program -- no named constant bound)
    //    0x82045600  324  vertex  (scatter vertex program -- "uvOffset")
    //    0x82045748  620  pixel   (scatter pixel program  -- "scatterScalars1", "scatterScalars2")
    //    0x82045AC0  324  vertex  (radial vertex program  -- "uvOffset")
    //    0x82045C08  716  pixel   (radial pixel program   -- "scatterScalars1", "scatterScalars2")
    //    0x820459B8  260  pixel   (texture/blend program  -- "blendFactor")
    //    0x820454E0  288  pixel   (down-sample program    -- "sampleOffsets")
    //    0x820452A8  564  pixel   (blur program           -- "controlScalars")
    //  A PC replacement additionally needs an HLSL pair per program; none is authored in this wave.]
    const void* const KP_QUAD_VERTEX_PROGRAM      = nullptr;  // X360 &unk_82045148
    const u32         KU_QUAD_VERTEX_PROGRAM_SIZE      = 348u;
    const void* const KP_SCATTER_VERTEX_PROGRAM   = nullptr;  // X360 &unk_82045600
    const u32         KU_SCATTER_VERTEX_PROGRAM_SIZE   = 324u;
    const void* const KP_SCATTER_PIXEL_PROGRAM    = nullptr;  // X360 &unk_82045748
    const u32         KU_SCATTER_PIXEL_PROGRAM_SIZE    = 620u;
    const void* const KP_RADIAL_VERTEX_PROGRAM    = nullptr;  // X360 &unk_82045AC0
    const u32         KU_RADIAL_VERTEX_PROGRAM_SIZE    = 324u;
    const void* const KP_RADIAL_PIXEL_PROGRAM     = nullptr;  // X360 &unk_82045C08
    const u32         KU_RADIAL_PIXEL_PROGRAM_SIZE     = 716u;
    const void* const KP_TEXTURE_PIXEL_PROGRAM    = nullptr;  // X360 &unk_820459B8
    const u32         KU_TEXTURE_PIXEL_PROGRAM_SIZE    = 260u;
    const void* const KP_DOWNSAMPLE_PIXEL_PROGRAM = nullptr;  // X360 &unk_820454E0
    const u32         KU_DOWNSAMPLE_PIXEL_PROGRAM_SIZE = 288u;
    const void* const KP_BLUR_PIXEL_PROGRAM       = nullptr;  // X360 &unk_820452A8
    const u32         KU_BLUR_PIXEL_PROGRAM_SIZE       = 564u;

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
    // programbuffer.cpp:263). Not a console behaviour.
    // [FLAG PC bring-up: the eight microcode packages above are absent]
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
        m_quadVertexProgram = PfxHelper::CreateProgram(0, KP_QUAD_VERTEX_PROGRAM,
                                                       KU_QUAD_VERTEX_PROGRAM_SIZE, m_allocator);

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
        m_scatterVertexProgram = PfxHelper::CreateProgram(0, KP_SCATTER_VERTEX_PROGRAM,
                                                          KU_SCATTER_VERTEX_PROGRAM_SIZE, m_allocator);
        BindProgramVariable(m_scatterVertexProgram, "uvOffset", m_scatterUVOffsetHandle);

        m_scatterPixelProgram = PfxHelper::CreateProgram(1, KP_SCATTER_PIXEL_PROGRAM,
                                                         KU_SCATTER_PIXEL_PROGRAM_SIZE, m_allocator);
        BindProgramVariable(m_scatterPixelProgram, "scatterScalars1", m_scatterScalars1Handle);
        BindProgramVariable(m_scatterPixelProgram, "scatterScalars2", m_scatterScalars2Handle);

        // ---- the radial programs (asm 0x823FEE1C-0x823FEEB4) ----------------------------------------
        m_radialVertexProgram = PfxHelper::CreateProgram(0, KP_RADIAL_VERTEX_PROGRAM,
                                                         KU_RADIAL_VERTEX_PROGRAM_SIZE, m_allocator);
        BindProgramVariable(m_radialVertexProgram, "uvOffset", m_radialUVOffsetHandle);

        m_radialPixelProgram = PfxHelper::CreateProgram(1, KP_RADIAL_PIXEL_PROGRAM,
                                                        KU_RADIAL_PIXEL_PROGRAM_SIZE, m_allocator);
        BindProgramVariable(m_radialPixelProgram, "scatterScalars1", m_radialScalars1Handle);
        BindProgramVariable(m_radialPixelProgram, "scatterScalars2", m_radialScalars2Handle);

        // ---- the texture / down-sample / blur programs (asm 0x823FEEB8-0x823FEF40) -------------------
        // ⚠ THE TEXTURE PROGRAM'S ALLOCATOR OVERRIDE IS 0, and that is not a transcription slip: the
        // asm is `li r6, 0` at this site alone (`CreateProgram(1, &unk_820459B8, 260, 0)`), while the
        // other seven pass `*(a1 + 288)`. A null override makes CreateProgram fall back to the
        // PfxHelper singleton's own allocator, which on this build is the same object -- but the
        // console distinction is preserved rather than normalised away.
        m_textureProgram = PfxHelper::CreateProgram(1, KP_TEXTURE_PIXEL_PROGRAM,
                                                    KU_TEXTURE_PIXEL_PROGRAM_SIZE, nullptr);
        BindProgramVariable(m_textureProgram, "blendFactor", m_blendFactorTextureHandle);

        m_downsampleProgram = PfxHelper::CreateProgram(1, KP_DOWNSAMPLE_PIXEL_PROGRAM,
                                                       KU_DOWNSAMPLE_PIXEL_PROGRAM_SIZE, m_allocator);
        BindProgramVariable(m_downsampleProgram, "sampleOffsets", m_sampleOffsetHandle);

        m_blurProgram = PfxHelper::CreateProgram(1, KP_BLUR_PIXEL_PROGRAM,
                                                 KU_BLUR_PIXEL_PROGRAM_SIZE, m_allocator);
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
