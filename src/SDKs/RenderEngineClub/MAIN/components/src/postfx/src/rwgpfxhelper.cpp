#include "types.hpp"

#include <cmath>    // cosf / sinf / fabsf
#include <cstring>  // memset
#include <cstdint>  // intptr_t

#include <new>      // placement new (the render-target debugger carve)
#include <cstdio>   // std::snprintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // CgsDev::Log::WriteToLog (the one-shot gate reports)

#include "rw/rwcore_structs.h"  // rw::IResourceAllocator / rw::ResourceDescriptor / rw::Resource
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertargetdebugger.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/renderstates.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h"  // RenderTarget (Blur9 / DownSample)
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"                // shadow::Device::SetState / SetPixelProgram

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- open a shader-constant row
// run for a handle and return the write cursor (X360 r3). The shared decl-only surface every
// immediate-mode TU declares (CgsIm2dColTex.cpp:91, BrnIm3d.cpp:118, BrnPostFxBloom.cpp:134);
// DEFINED in the mounted pc/gcm/renderengine/ImmediateModePCLeaf.cpp:543. No header owns it.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   rw::graphics::postfx::PfxHelper::CreateProgram                 @ 0x823FE480
//   rw::graphics::postfx::PfxHelper::CreateStates                  @ 0x82402D88
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16WithBilinear@ 0x823F8E78
//   rw::graphics::postfx::PfxHelper::InitWeights_Blur16            @ 0x823F8F50
//   rw::graphics::postfx::PfxHelper::InitWeights_DirBlur9Quadratic @ 0x823F8C70
//
// PfxHelper is the post-fx singleton: it owns the full-screen-quad geometry, the opaque-blend /
// depth-stencil render states, and the blur/copy shader programs every effect compiles against.
// CreateProgram is the shader-program factory the post-fx effects call directly; CreateStates builds
// the shared opaque blend state. Both follow the standard render-engine idiom: build a parameters
// block -> GetResourceDescriptor sizes it -> the allocator's DoAllocate carves the storage ->
// Initialize constructs the runtime object in place.
//
// The InitWeights_* generators are pure float math that fill a caller-owned vec4 weight table; every
// literal below is the immediate the asm loads. The flt_82001C98 / flt_82001CC0 constants are
// 1.0 / 0.0 (the same pair the sibling DepthOfField TU documents).
//
// ADDED THIS WAVE: the PfxHelper constructor (0x82408348), Release (0x82402E58),
// Parameters::Parameters, the SafeRelease<> teardown template (0x823FF480 / 0x823FF4F8) and the
// gpPfxHelper singleton definition. Two blocks of the constructor are gated -- see the gates below.

// ---- PC BRING-UP GATE: the embedded Xenos microcode ------------------------------------------
// Every post-fx program in this subsystem -- the helper's four, B4Blur's eight, the render-target
// debugger's two, DepthOfField's and Vignette's -- is built from a compiled-shader package embedded
// in the guest image and handed to renderengine::ProgramBuffer::Initialize, which calls
// XGGetMicrocodeShaderParts. The PC stub of that entry point returns 0 WITHOUT writing *lpParts
// (ImmediateModePCLeaf.cpp:626-646), so the console route then reads an uninitialised
// ProgramMicrocodeParts for the microcode size and feeds a truncated 64-bit function pointer to
// Xbox2CreateConstantTable -- a crash, not a cosmetic gap.
//
// The precedent this follows is BrnPostFxShader::Shader::Construct: try the PC image first
// (renderengine::ProgramBufferPC_Adopt), keep the console route behind the gate so the console
// structure stays on the page, and when there is NO PC image leave the slot HONESTLY EMPTY -- a
// null program and zero-count handles, so nothing can draw with a wrong program -- with one
// ReportOnce line naming the gate. The gate lives in PfxHelper::CreateProgram, the single funnel
// every one of those sites goes through, so every CALL SITE stays console-faithful.
//
// ⚠ THE GATE NOW GUARDS ONLY THE CONSOLE ROUTE -- THIS TU'S FOUR PROGRAMS ARE REAL AGAIN.
// The four Xenos packages this file compiles (0x82044240 / 0x820444F8 / 0x820447A8 / 0x82044360)
// were dumped byte-exact out of ARTIST_copy.i64 (scratch/postfx_step5_wave/PACKAGES_DUMP.md; every
// dumped size matches the byte count each call site below already recorded), disassembled with
// tools/assets/shaders/xenos.py, re-expressed as HLSL in tools/assets/shaders/brn_postfx_helper.fx
// and published as PC ShaderProgramBuffer images in
// pc/gcm/renderengine/PostFxHelperProgramsPC.cpp -- so every CreateProgram call site below now
// hands a REAL PC image to the adopt path (a) and none of them reaches the gate. The gate stays 0
// because the thing it guards is unchanged: renderengine::ProgramBuffer::Initialize (and
// GetResourceDescriptor before it) still cannot run on this backend. rwgpfxb4blur.cpp's eight
// packages and rwgpfxtint.cpp's are still not dumped and still take arm (b).
#define RW_GPFX_PROGRAM_MICROCODE_AVAILABLE   0

// ---- PC BRING-UP GATE: the full-screen-quad geometry -------------------------------------------
// The constructor's vertex-descriptor / vertex-buffer / mesh block uploads the quad's vertices from
// the guest data block at X360 unk_82FAFFF0. FOUR independent things are missing; only the SHAPE of
// the upload has been recovered since the last wave (see the BLOCKED note at the site):
//   1. THE VERTEX VALUES. The block is the DWARF's file-scope `rw::graphics::postfx::Vertex` --
//      `Vector3 Vertex[4]`, rwgpfxhelper.cpp:74 -- i.e. 4 x 16 = the 0x40 bytes the note records.
//      Its image copy is ALL ZERO because it is CONSTRUCTED AT RUNTIME by a CRT static initialiser
//      (not in the export set; found with idat, 2026-08-15: 0x82C4F910-0x82C4F9C0 lfs/stfs four
//      16-byte stack vectors from flt_820037C8 = -1.0 / flt_82001CC0 = 0.0 / flt_82001C98 = 1.0 and
//      lvx128/stvx128 into unk_82FAFFF0 +0x00/+0x10/+0x20/+0x30). THE VALUES ARE KNOWN:
//          Vertex[0] = (-1, -1, 0)   Vertex[1] = ( 1, -1, 0)   Vertex[2] = (-1,  1, 0)   Vertex[3] = ( 1,  1, 0)
//      -- the clip-space unit quad; the console RECTLIST draws Vertex[0..2] with uv (0,1),(1,1),(0,0);
//      a PC TRIANGLESTRIP needs Vertex[3] with uv (1,0). What is STILL missing is the PC DRAW itself:
//      Xenos RECTLIST(8) is refused by XenonD3D9Shims::MapPrimitive and D3DDevice_DrawVertices has no
//      PC definition, so RenderQuad must draw the 4-vertex strip through D3DDevice_BeginVertices/
//      EndVertices like BrnPostFxBloom::DrawFullScreenQuad -- the follow-up that flips this gate.
//   2. renderengine::MeshHelper::GetResourceDescriptor has no declaration in this tree (its X360 body
//      exists @0x82B64070 -- just not in the .ida-exports set; rung-6 verifier), and
//      MeshHelper::Initialize is declared non-static with one parameter while the console calls it
//      with (resource, parameters).
//   3. The draw needs renderengine::MeshHelper::Dispatch<renderengine::Device>, whose only home
//      (pc/gcm/renderengine/MeshHelper.cpp) is NOT on tools/build/build_game_exe.bat, and
//      D3DDevice_DrawVertices, which has no definition anywhere in the tree.
//   4. The primitive is Xenos D3DPT_RECTLIST (8, `li r4, 8` @0x823FE634), which
//      XenonD3D9Shims.cpp::MapPrimitive refuses by design (XenonD3D9Shims.cpp:2376-2378): D3D9 has
//      no RECTLIST and expanding one is a rendering decision, not a translation.
#define RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE 0

// ---- THE FOUR PC PROGRAM IMAGES ----------------------------------------------------------------
// [FLAG PC-platform leaf: shader programs] The D3D9 counterparts of the four Xenos packages this TU
// compiles, wrapped as platform-4 ShaderProgramBuffer images by the generated leaf
// pc/gcm/renderengine/PostFxHelperProgramsPC.cpp (sibling of PostFxProgramsPC.cpp and
// PostFxBloomProgramsPC.cpp). Declared here as the minimal external surface -- the same convention
// BrnPostFxBloom.cpp:121-149 and BrnPostFxShader.cpp already use for their own generated leaves;
// there is no header for these.
//
// ⚠ THE SIZES ARE THE *PC IMAGE* SIZES, NOT THE X360 MICROCODE SIZES (288 / 688 / 948 / 408).
// ProgramBufferPC_Adopt bounds-checks the blob against the size it is handed and copies exactly
// that many bytes, so a console size here would either refuse a valid image ("microcode size is out
// of range") or read past the array. Nothing below hard-codes a number: each size is read from the
// generated TU's own published constant, whose static_assert ties it to sizeof() of the array.
namespace renderengine
{
    extern const u8  gauPostFxHelperQuadVertexProgramPC[];
    extern const u32 guPostFxHelperQuadVertexProgramPCSize;
    extern const u8  gauPostFxHelperBlur9PixelProgramPC[];
    extern const u32 guPostFxHelperBlur9PixelProgramPCSize;
    extern const u8  gauPostFxHelperBlur16PixelProgramPC[];
    extern const u32 guPostFxHelperBlur16PixelProgramPCSize;
    extern const u8  gauPostFxHelperBlur4PixelProgramPC[];
    extern const u32 guPostFxHelperBlur4PixelProgramPCSize;
}

namespace
{
    // One log line per gate, not one per frame (the BrnPostFxShader.cpp idiom).
    void ReportOnce(bool& lrbAlreadyReported, const char* lpcText)
    {
        if (!lrbAlreadyReported)
        {
            lrbAlreadyReported = true;
            CgsDev::Log::WriteToLog(lpcText);   // BrnGame.log, like every sibling post-fx report (verify pass 2026-08-15)
        }
    }

    // The renderengine allocator keeps its dispatch pointer as the first word; the helper stores that
    // word (m_allocator) and routes DoAllocate through it.
    rw::Resource AllocateResource(rw::IResourceAllocator* lpAllocator,
                                  const rw::BaseResourceDescriptors<5>& lrDescriptor)
    {
        return lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lrDescriptor), nullptr);
    }
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 0x823FE480.
    //
    // ⚠ THIS IS THE MICROCODE WALL, AND IT IS THE ONLY PLACE IT NEEDS TO BE. Every post-fx program
    // in the whole subsystem -- the helper's four, B4Blur's eight, the render-target debugger's two,
    // DepthOfField's and Vignette's -- is built through this one function, so gating it here keeps
    // every call site console-faithful. Below the gate the console route is UNCHANGED and still on
    // the page; above it are the two things the PC backend needs:
    //
    //   (a) TRY THE PC IMAGE FIRST. renderengine::ProgramBufferPC_Adopt takes a platform-4
    //       ShaderProgramBuffer image (what tools/assets/shaders/shader_transcode.py emits) and makes
    //       it a live ProgramBufferData. It returns null for anything else -- including raw Xenos
    //       microcode, which fails its D3D9-SM3 token gate -- so the console route below is a true
    //       fallthrough, not a replacement. Same shape as BrnPostFxShader::Shader::Construct.
    //   (b) REFUSE THE CONSOLE ROUTE WHEN THERE IS NO IMAGE. ProgramBuffer::Initialize (and
    //       GetResourceDescriptor before it) call XGGetMicrocodeShaderParts, whose PC stub returns 0
    //       WITHOUT writing *lpParts (ImmediateModePCLeaf.cpp:626-646); the body then reads that
    //       uninitialised block for the microcode size and hands a truncated 64-bit function pointer
    //       to Xbox2CreateConstantTable. That is a crash, so the honest answer is a null program and
    //       one report line -- never a program built from bytes that are not there.
    //
    // [FLAG PC bring-up: RW_GPFX_PROGRAM_MICROCODE_AVAILABLE]
    renderengine::ProgramBufferData* PfxHelper::CreateProgram(s32 leType, const void* lpMicrocode,
                                                              u32 luSize, rw::IResourceAllocator* lpReserved)
    {
        // (a) a pre-built PC program image, if this blob is one.
        if (lpMicrocode != nullptr)
        {
            renderengine::ProgramBufferData* const lpAdopted =
                renderengine::ProgramBufferPC_Adopt(lpMicrocode, luSize, static_cast<u32>(leType));
            if (lpAdopted != nullptr)
            {
                return lpAdopted;
            }
        }

#if !RW_GPFX_PROGRAM_MICROCODE_AVAILABLE
        // (b) no PC image and the console route cannot run here: HONESTLY EMPTY.
        {
            static bool sbReportedNoProgram = false;
            ReportOnce(sbReportedNoProgram,
                       "[PfxHelper] no PC program image and the X360 microcode route is unavailable"
                       " on this backend (XGGetMicrocodeShaderParts is a stub): the post-fx program"
                       " slot is left EMPTY. [FLAG PC bring-up:"
                       " RW_GPFX_PROGRAM_MICROCODE_AVAILABLE]\n");
        }
        return nullptr;
#else
        // When no allocator override is given the asm dereferences the singleton and reads its first
        // word -- the allocator the helper itself was built with.
        rw::IResourceAllocator* lpAllocator = lpReserved;
        if (lpAllocator == nullptr)
        {
            lpAllocator = gpPfxHelper->GetAllocator();
        }

        // Build the program parameters: function = microcode blob, shader-type = leType, the size in
        // the third word; the remaining words start zeroed.
        renderengine::ProgramBufferParameters lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.muFunction   = static_cast<u32>(reinterpret_cast<uintptr_t>(lpMicrocode));
        lParams.muShaderType = static_cast<u32>(leType);
        lParams.muReserved8  = luSize;

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lParams);

        rw::Resource lResource = AllocateResource(lpAllocator, lDescriptor);
        return renderengine::ProgramBuffer::Initialize(
            reinterpret_cast<renderengine::ProgramResourceLayout*>(&lResource), &lParams);
#error "RW_GPFX_PROGRAM_MICROCODE_AVAILABLE is on but the console microcode route truncates a 64-bit pointer into muFunction and reaches a stub XGGetMicrocodeShaderParts on this backend -- it cannot be enabled on PC (rung-6 verifier)"
#endif  // RW_GPFX_PROGRAM_MICROCODE_AVAILABLE
    }

    // X360 0x82402D88. DWARF rwgpfxhelper.h:112 -- void; the console leaves the new state in r3 but
    // its only caller (the constructor) discards it, and the store into m_opaqueBlendState below is
    // what publishes it.
    void PfxHelper::CreateStates()
    {
        // Opaque-blend material parameters. The four leading words are the per-channel blend factors
        // (0x07060706 each, from the asm's lis 0x706 / ori 0x706 build), then the channel-write masks
        // (0xF x4), the blend op (7), the flag word (0x87 == 135), and a -1 word; the boolean tail and
        // the remaining state words are zero.
        renderengine::BlendStateParameters lParams;
        std::memset(&lParams, 0, sizeof(lParams));
        lParams.maBlendFactor[0] = 0x07060706u;  // 117835526
        lParams.maBlendFactor[1] = 0x07060706u;
        lParams.maBlendFactor[2] = 0x07060706u;
        lParams.maBlendFactor[3] = 0x07060706u;
        // Set by which params WORD the asm writes (0x82402DC0..0x82402DE8), NOT by the
        // Hex-Rays v5[] index names: blendstate.h orders word4=muState15 .. word11=muState9.
        lParams.muState15        = 7;            // word4 = v5[4]
        lParams.muState4         = 15;           // word5 = v5[5]
        lParams.muState5         = 15;           // word6 = v5[6]
        lParams.muState6         = 15;           // word7 = v5[7]
        lParams.muState7         = 15;           // word8 = v5[8]
        lParams.muState8         = 135;          // word9 = v5[9] = 0x87
        lParams.muState17        = 0;            // word10 = v5[10]
        lParams.muState9         = static_cast<u32>(-1); // word11 = v5[11]

        rw::BaseResourceDescriptors<5> lDescriptor;
        renderengine::BlendState::GetResourceDescriptor(&lDescriptor);

        rw::Resource lResource = AllocateResource(m_allocator, lDescriptor);
        m_opaqueBlendState = static_cast<renderengine::BlendMaterialState*>(
            renderengine::BlendState::Initialize(
                reinterpret_cast<renderengine::BlendMaterialState**>(&lResource), &lParams));
    }

    // X360 0x823F8E78. 16-tap (4x4) box blur with bilinear sampling: for each of the 4x4 sample
    // positions emit (offsetX, offsetY, 0.25, 1.0). offsetX/offsetY step by 2 (bilinear pairs).
    void PfxHelper::InitWeights_Blur16WithBilinear(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32* lpOut = lpBlurWeights;
        for (s32 liY = 0; liY < 4; liY += 2)
        {
            const f32 lfOffsetY = static_cast<f32>(liY - 1) * lfInvHeight;
            for (s32 liX = 0; liX < 4; liX += 2)
            {
                const f32 lfOffsetX = static_cast<f32>(liX - 1) * lfInvWidth;
                lpOut[0] = lfOffsetX;
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.25f;
                lpOut[3] = 1.0f;
                lpOut += 4;
            }
        }
    }

    // X360 0x823F8F50. 16-tap (4x4) box blur. Each of the four rows emits four samples at x-offsets
    // {-1.5, -0.5, 0.5, 1.5}/width and a constant y-offset of (row+0.5)/height, with a flat 0.0625
    // weight in lane 2. A second pass normalises those 16 weights so they sum to 1.
    void PfxHelper::InitWeights_Blur16(f32* lpBlurWeights, s32 liWidth, s32 liHeight)
    {
        static const f32 KAF_OFFSETS_X[4] = { -1.5f, -0.5f, 0.5f, 1.5f };

        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liRow = 0; liRow < 4; ++liRow)
        {
            const f32 lfOffsetY = (static_cast<f32>(liRow) + 0.5f) * lfInvHeight;
            for (s32 liCol = 0; liCol < 4; ++liCol)
            {
                lpOut[0] = lfInvWidth * KAF_OFFSETS_X[liCol];
                lpOut[1] = lfOffsetY;
                lpOut[2] = 0.0625f;
                lfTotalWeight += 0.0625f;
                lpOut += 4;
            }
        }

        // Normalise the weight lane (lane 2 of each vec4) by the accumulated total.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 16; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] *= lfInvTotal;
        }
    }

    // X360 0x823F8C70. 9-tap directional blur with a quadratic falloff. The 9 taps step along
    // (cos angle, sin angle); each tap is centred about index 4 and quantised in 2/9 (0.22222222)
    // steps. The weight is (1 - d^2)/radius + 0.05 (d == normalised step). The weight lane is then
    // normalised by the accumulated total and scaled by weightFactor.
    void PfxHelper::InitWeights_DirBlur9Quadratic(f32* lpBlurWeights, s32 liWidth, s32 liHeight,
                                                  f32 lfAngle, f32 lfRadius, f32 lfWeightFactor)
    {
        const f32 lfInvWidth  = 1.0f / static_cast<f32>(liWidth);
        const f32 lfInvHeight = 1.0f / static_cast<f32>(liHeight);

        // XMVectorCos / XMVectorSin splat the scalar cos/sin of the angle.
        f32 lfCos = cosf(lfAngle);
        f32 lfSin = sinf(lfAngle);
        // Snap near-axis directions to the axis (asm: |cos| < 0.001 -> 0, |sin| > 0.999 -> 1).
        if (fabsf(lfCos) < 0.001f)
            lfCos = 0.0f;
        if (fabsf(lfSin) > 0.99900001f)
            lfSin = 1.0f;

        const f32 lfInvRadius = 1.0f / lfRadius;

        f32 lfTotalWeight = 0.0f;
        f32* lpOut = lpBlurWeights;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            const f32 lfStep = (static_cast<f32>(liSample) - 4.0f) * lfRadius * 0.22222222f;
            const f32 lfNorm = lfStep * lfInvRadius;
            const f32 lfWeight = -((lfNorm * lfNorm) - 1.0f);

            // asm 0x823F8D90-DA8: lane0 = step*cos*(1/width), lane1 = step*sin*(1/height).
            lpOut[0] = (lfStep * lfCos) * lfInvWidth;
            lpOut[1] = (lfStep * lfSin) * lfInvHeight;
            lpOut[2] = lfWeight * lfInvRadius + 0.050000001f;
            lfTotalWeight += lpOut[2];
            lpOut += 4;
        }

        // Normalise the weight lane and fold in weightFactor.
        const f32 lfInvTotal = 1.0f / lfTotalWeight;
        for (s32 liSample = 0; liSample < 9; ++liSample)
        {
            lpBlurWeights[liSample * 4 + 2] = lpBlurWeights[liSample * 4 + 2] * lfInvTotal * lfWeightFactor;
        }
    }
}
}
}

// ================================================================================================
// rw::graphics::postfx::PfxHelper -- constructor @0x82408348, Release @0x82402E58,
// Parameters::Parameters (inlined), the gpPfxHelper singleton (X360 off_82FAEE80) and the
// SafeRelease<> teardown template (X360 0x823FF480 / 0x823FF4F8).
//
// Goes into b5-decomp/src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxhelper.cpp.
// ================================================================================================

// ---- THE SINGLETON ------------------------------------------------------------------------------
// X360 off_82FAEE80. Written by the constructor (`stw r3, off_82FAEE80@l(r11)` @0x8240835C-ish, the
// second store the ctor makes) and cleared by Release (`stw r31, off_82FAEE80@l(r11)` @0x824030A4).
// The DWARF spells it `static PfxHelper* PfxHelper::m_instance` (rwgpfxhelper.h:150); this tree
// already committed the file-scope declaration in rwgpfxhelper.h and BrnPostFx::Destruct reads it by
// that name, so this TU is its DEFINITION and there is exactly one home.
//
// ⚠ SPLIT BRAIN REMOVED. The committed rwgpfxhelper.cpp carried a SECOND, TU-local copy of the same
// pointer (`namespace { PfxHelper* gpPfxHelperSingleton = nullptr; }`) that CreateProgram read while
// nothing ever wrote it -- so on this build CreateProgram's no-override path dereferenced a null
// pointer while the real singleton sat in the header's extern. One pointer now.
namespace rw
{
namespace graphics
{
namespace postfx
{
    PfxHelper* gpPfxHelper = nullptr;
}
}
}

namespace
{
    // X360 dword_82FAEE84 -- the post-fx render-target debugger singleton the constructor builds
    // once. TU-local: the only two instructions in the whole export set that touch 82FAEE84 are the
    // load and the store inside PfxHelper::PfxHelper (grep pasted in the report), so no other TU can
    // name it and it stays file scope.
    rw::graphics::postfx::RenderTargetDebugger* gpRenderTargetDebugger = nullptr;

    // ---- the per-T half of SafeRelease -----------------------------------------------------------
    // On the console SafeRelease<T> calls `T::Release(ptr)` before handing the block back. This tree
    // splits the renderengine vocabulary differently from the vendor original -- the OBJECT is
    // `VertexDescriptorData` / `VertexBufferHeader` / `ProgramBufferData` / `MeshHelper::MeshData`
    // while the static `Release` lives on the API class (`VertexDescriptor`, `VertexBuffer`,
    // `ProgramBuffer`) -- so the per-T step is spelled as this overload set instead of `T::Release`.
    // The two out-of-line X360 instantiations prove the two EMPTY ones:
    //   SafeRelease<BlendState>        @0x823FF480 -- DoFree + null only, no Release call
    //   SafeRelease<DepthStencilState> @0x823FF4F8 -- DoFree + null only, no Release call
    void ReleaseObject(renderengine::VertexDescriptorData* lpObject)
    {
        renderengine::VertexDescriptor::Release(lpObject);
    }
    void ReleaseObject(renderengine::VertexBufferHeader* lpObject)
    {
        renderengine::VertexBuffer::Release(lpObject);
    }
    void ReleaseObject(renderengine::ProgramBufferData* lpObject)
    {
        renderengine::ProgramBuffer::Release(lpObject);
    }
    void ReleaseObject(renderengine::MeshHelper::MeshData*)
    {
        // X360 `bl STUB` @0x82402F20 -- the mesh's own Release. THE CALLEE IS NAMED NOW, from the
        // DecFIGS DWARF: `void renderengine::MeshHelper::Release()` (meshhelper.h:86), sitting in
        // PfxHelper::Release exactly where SafeRelease<renderengine::MeshHelper>
        // (rwgpfxhelper.h:176) puts it -- between the VertexBuffer release at +0x0C (0x82402ED4) and
        // the DepthStencilState release at +0x14 (0x82402F68), operating on this->m_quadMeshState
        // (`lwz r3, 4(r30)` @0x82402F14).
        // Its HOME in this tree is pc/gcm/renderengine/renderstates.h (the MeshHelper declaration)
        // plus pc/gcm/renderengine/MeshHelper.cpp (the body) -- NOT this file; a rival definition
        // under the RenderEngineClub tree would be a split brain against the class's real home.
        // [FLAG BLOCKED: the renderengine::MeshHelper::Release BODY. IDA resolves the branch target
        //  at 0x82402F20 to "STUB" and no MeshHelper::Release appears anywhere in the 30,095-function
        //  export set (grep pasted in the wave report -- only Dispatch<Device> @0x8227B530 and
        //  Initialize @0x82B63EB8 survive under that class name), so there is no body to reconstruct.
        //  The resource is still handed back below, so the only thing missing is the vendor object's
        //  own teardown -- and the two out-of-line SafeRelease instantiations that DO exist
        //  (BlendState @0x823FF480, DepthStencilState @0x823FF4F8) emit no Release call at all, so a
        //  no-op is a plausible but UNVERIFIED shape for this one; it is left empty and flagged
        //  rather than asserted.]
    }
    void ReleaseObject(renderengine::DepthStencilState*)
    {
        // Empty on the console: SafeRelease<DepthStencilState> @0x823FF4F8 emits no Release call.
    }
    void ReleaseObject(renderengine::BlendMaterialState*)
    {
        // Empty on the console: SafeRelease<BlendState> @0x823FF480 emits no Release call.
    }

    // (The four KP_*/KU_* null microcode placeholders that stood here are gone. The real PC images
    // are declared at file scope above and passed straight into CreateProgram at the call sites,
    // where the X360 address and byte size they were decoded from stay in the comment.)

    // A handle whose register count is 0 is "not found" -- exactly what
    // renderengine::ProgramBuffer::GetVariableHandleByName writes when the name misses
    // (programbuffer.cpp:299). Used to leave every handle honestly empty when its program is null.
    void ClearHandle(renderengine::ProgramVariableHandle& lrHandle)
    {
        lrHandle.mu8RegisterSet   = 0u;
        lrHandle.mu8RegisterIndex = 0u;
        lrHandle.mu8ShaderType    = 0u;
        lrHandle.mu8RegisterCount = 0u;
    }

    // The console's `ProgramBuffer::GetVariableHandleByName(program, name, &handle)` plus the ONE
    // guard the PC bring-up needs: the committed lookup dereferences lpData unconditionally
    // (programbuffer.cpp:263), so with a gated-off program it would fault. The guard exists only
    // because the program can be null on this build; it is not a console behaviour.
    // [FLAG PC bring-up: RW_GPFX_PROGRAM_MICROCODE_AVAILABLE]
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
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 rwgpfxhelper.h:176, instantiations @0x823FF480 / @0x823FF4F8 and seven inlined copies
    // inside Release. The rw::Resource handed to DoFree is REBUILT FROM THE OBJECT POINTER: the
    // console zeroes a five-word block and stores the pointer in word 0
    // (`stw r31, 0..0x10(r11)` then `stw r10, 0(r11)`), which is the same allocator convention
    // rwcore_structs.h pins (DoFree is the vtbl+0x14 slot). The host descriptor has FOUR entries,
    // not five -- rw::Resource is BaseResources<4> on the PC (rwcore.pdb, x64) against the X360's
    // serialised <5>; the documented cross-build delta, same one BrnPostFx::Construct's carve applies.
    template< class T >
    void PfxHelper::SafeRelease(T*& lprObject)
    {
        if (lprObject != nullptr)
        {
            ReleaseObject(lprObject);

            rw::Resource lResource;
            for (u32 luSlot = 0u; luSlot < 4u; ++luSlot)
            {
                lResource.m_baseResources[luSlot] = nullptr;
            }
            lResource.m_baseResources[0] = lprObject;
            m_allocator->DoFree(lResource);

            lprObject = nullptr;
        }
    }

    // X360 rwgpfxhelper.h:43. INLINED at BrnPostFx::Construct -- the caller writes the allocator
    // word itself (`stw r28, 0x300+var_278(r1)` @0x82409FC4) and the block has no other member, so
    // the constructor emits nothing. Reproduced as the empty body it is; `allocator` is left
    // uninitialised exactly as the console leaves it, and every call site assigns it immediately.
    PfxHelper::Parameters::Parameters()
    {
    }

    // ============================================================================================
    // PfxHelper::PfxHelper @0x82408348
    //
    // The post-fx singleton's one-time build. In console order:
    //   1. m_allocator = parameters.allocator          (`*&this->char0 = *parameters`)
    //   2. gpPfxHelper = this                          (`off_82FAEE80 = this`)   <- publishes itself
    //   3. the full-screen-quad vertex descriptor + vertex buffer + its four vertices + the mesh
    //   4. the quad depth/stencil state
    //   5. four shader programs + their named-constant handles
    //   6. CreateStates()                              (the shared opaque blend state, +0x4C)
    //   7. the render-target debugger singleton, built once
    //
    // Steps 3 and 5 are gated -- see the two gates and their BLOCKED notes above. Steps 1, 2, 4, 6
    // and 7 are reproduced in full from the asm; every immediate below is the one the asm loads.
    // ============================================================================================
    PfxHelper::PfxHelper(const Parameters& lrParameters)
    {
        // asm: `*&this->char0 = *parameters` then `off_82FAEE80 = this`. The publish happens BEFORE
        // anything is built, which is why CreateProgram (step 5) can reach the singleton for its
        // default allocator while the object is still half-constructed. Faithful, order included.
        m_allocator  = lrParameters.allocator;
        gpPfxHelper  = this;

        // Slots the gated blocks would fill. The console leaves nothing uninitialised here (every
        // one of these is written by a step below), so zeroing them is a PC bring-up consequence of
        // the gates, not a console behaviour -- but it is the one that makes Release safe and makes
        // "no program" visible instead of undefined.
        // [FLAG PC bring-up: RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE / RW_GPFX_PROGRAM_MICROCODE_AVAILABLE]
        m_quadMeshState          = nullptr;
        m_quadVertexDescriptor   = nullptr;
        m_quadVertexBuffer       = nullptr;
        m_quadDrawParams         = nullptr;   // +0x10: never written by the console ctor either
        m_quadDepthStencilState  = nullptr;
        m_quadVertexProgram      = nullptr;
        m_9tapPixelProgram       = nullptr;
        m_16tapPixelProgram      = nullptr;
        m_4tapPixelProgram       = nullptr;
        m_opaqueBlendState       = nullptr;
        ClearHandle(m_uvOffsetHandle);
        for (u32 luHandle = 0u; luHandle < 3u; ++luHandle)
        {
            ClearHandle(m_blur9SamplesHandle[luHandle]);
        }
        for (u32 luHandle = 0u; luHandle < 4u; ++luHandle)
        {
            ClearHandle(m_blur16SamplesHandle[luHandle]);
        }
        ClearHandle(m_blur4SamplesHandle);

#if RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE
#error "RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE is on but the quad block is not reconstructed -- see the BLOCKED note"
#else
        // ---- 3. THE FULL-SCREEN QUAD (asm 0x82408380-0x8240855C) -- BLOCKED -----------------------
        // What the console does, in full, so the shape is on the page:
        //     renderengine::VertexDescriptor::Parameters  {element0: stream 0, format 0x002A23B9,
        //                                                  usageIndex 1;
        //                                                  element1: stream 0, format 0x002C23A5,
        //                                                  usageIndex 6}
        //       -> GetResourceDescriptor -> m_allocator->DoAllocate -> Initialize -> m_quadVertexDescriptor
        //     renderengine::VertexBufferHelper::Parameters {vertexCount 3, formatCodes {0x002A23B9,
        //                                                   0x002C23A5, -1...}}
        //       -> CalculateBufferSize -> VertexBuffer::GetResourceDescriptor -> DoAllocate
        //       -> CalculateBufferSize -> VertexBuffer::Initialize -> m_quadVertexBuffer
        //     VertexBufferHelper::Lock -> write the quad's vertices -> Unlock
        //     MeshHelper params {…, [1] = m_quadVertexBuffer}
        //       -> MeshHelper::GetResourceDescriptor -> DoAllocate -> Initialize -> m_quadMeshState
        //
        // WHAT THE VERTEX UPLOAD WRITES, RECOVERED IN FULL from the lvx128/vspltw/stvewx sequence at
        // 0x82408494-0x824085B0. VertexBufferHelper::Lock hands back a base pointer (var_3D0); the
        // sequence keeps it in r11 biased by -4 and stores through `stvewx vX, r11, 4` / `, r11, 8`
        // / `, 0, r8`, which lands three 20-byte vertices (FLOAT3 position + FLOAT2 uv == the
        // declaration built at 0x82408370-A8) at base+0, base+20, base+40:
        //     vertex 0  position = Vertex[0]   (lvx128 at unk_82FAFFF0 + 0x00, words 0/1/2)
        //               uv       = (0.0, 1.0)  (staged in var_440 from flt_82001CC0 / flt_82001C98)
        //     vertex 1  position = Vertex[1]   (lvx128 at unk_82FAFFF0 + 0x10)
        //               uv       = (1.0, 1.0)
        //     vertex 2  position = Vertex[2]   (lvx128 at unk_82FAFFF0 + 0x20)
        //               uv       = (0.0, 0.0)
        // The fourth Vector3 of the array is never read, and the vertex count in the buffer
        // parameters is 3 (`li r26, 3` @0x82408404), because the draw is Xenos D3DPT_RECTLIST (8):
        // three corners of a rectangle, the GPU infers the fourth. That corrects the previous note's
        // reading of "a full-screen TRIANGLE with two out-of-range corners" -- and the three UVs are
        // the SAME three the sibling BrnPostFxBloom::DrawFullScreenQuad writes for its first three
        // strip vertices, which is a useful cross-check on the decode but is NOT evidence for the
        // positions, so none is written here.
        //
        // [FLAG BLOCKED: FOUR missing items, none guessable -- enumerated at
        //  RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE above. In short: (1) the three corner POSITIONS,
        //  which live in the runtime-constructed `Vector3 Vertex[4]` at X360 unk_82FAFFF0 and are
        //  zero in the shipped image with no writer anywhere in the export set; (2)
        //  MeshHelper::GetResourceDescriptor has no declaration here; (3) MeshHelper.cpp is not
        //  mounted and D3DDevice_DrawVertices has no definition in the tree; (4) RECTLIST is refused
        //  by the PC primitive mapper on purpose.
        //  CONSEQUENCE: the helper has no quad, so PfxHelper::RenderQuad -- and therefore Blur9 and
        //  DownSample -- cannot draw. None of them is reachable on this build (BrnPostFx runs
        //  permutation 0 with every effect bit clear), and the null members make a premature call
        //  report-and-return rather than draw garbage. The four PROGRAMS are unaffected: they are
        //  real, adopted, and their constants upload.]
        static bool sbReportedNoQuad = false;
        ReportOnce(sbReportedNoQuad,
                   "[PfxHelper] full-screen-quad geometry not built: the corner positions are known"
                   " (CRT static initialiser @0x82C4F910: the clip-space unit quad) but the PC draw of"
                   " the RECTLIST (D3DDevice_DrawVertices) is not reconstructed yet -- RenderQuad/Blur9/"
                   "DownSample report and return. [FLAG PC bring-up: RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE]\n");
#endif

        // ---- 4. the quad depth/stencil state (asm 0x8240857C-0x82408600) --------------------------
        // Every word is an asm immediate: function 3, both stencil functions 7 (ALWAYS), all four
        // stencil masks -1, everything else 0, and all six trailing flag bytes 0 -- i.e. depth test
        // OFF, depth write OFF, stencil OFF, which is what a full-screen post-fx pass wants.
        {
            renderengine::DepthStencilState::Parameters lParameters = {};
            lParameters.muFunction        = 3u;              // v40[0]
            lParameters.maState1[0]       = 0u;              // v40[1..3]
            lParameters.maState1[1]       = 0u;
            lParameters.maState1[2]       = 0u;
            lParameters.muState4          = 7u;              // v40[4]  == E_FUNCTION_ALWAYS
            lParameters.maState5[0]       = 0u;              // v40[5..7]  (the memset)
            lParameters.maState5[1]       = 0u;
            lParameters.maState5[2]       = 0u;
            lParameters.muState8          = 7u;              // v40[8]  == E_FUNCTION_ALWAYS
            lParameters.muState9          = 0u;              // v40[9]
            lParameters.muState10         = 0u;              // v40[10]
            lParameters.muStencilReadMask  = static_cast<u32>(-1);  // v40[11]
            lParameters.muStencilWriteMask = static_cast<u32>(-1);  // v40[12]
            lParameters.muState13         = 0u;              // v40[13]
            lParameters.muState14         = static_cast<u32>(-1);   // v40[14]
            lParameters.muState15         = static_cast<u32>(-1);   // v40[15]
            lParameters.muState16         = 0u;              // v40[16]
            lParameters.mbDepthTestEnable  = 0u;             // v41
            lParameters.mbDepthWriteEnable = 0u;             // v42
            lParameters.mu8Flag2 = 0u;                       // v43
            lParameters.mu8Flag3 = 0u;                       // v44
            lParameters.mu8Flag4 = 0u;                       // v45
            lParameters.mu8Flag5 = 0u;                       // v46

            renderengine::ResourceDescriptor5 lDescriptor;
            renderengine::DepthStencilState::GetResourceDescriptor(&lDescriptor, &lParameters);

            rw::Resource lResource = m_allocator->DoAllocate(
                reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), nullptr);
            m_quadDepthStencilState = renderengine::DepthStencilState::Initialize(
                reinterpret_cast<renderengine::DepthStencilState**>(&lResource), &lParameters);
        }

        // ---- 5. the four shader programs and their named constants (asm 0x82408604-0x824086F0) ----
        // The console builds, in order (every immediate is the asm's own):
        //   CreateProgram(0, &unk_82044240, 0x120=288) -> m_quadVertexProgram , handle "uvOffset"
        //   CreateProgram(1, &unk_820444F8, 0x2B0=688) -> m_9tapPixelProgram  , handles
        //                                            "sampleOffsets4x4_1" / "_2" / "sampleOffsets4_1"
        //   CreateProgram(1, &unk_820447A8, 0x3B4=948) -> m_16tapPixelProgram , handles
        //                                            "sampleOffsets_1" .. "_4"
        //   CreateProgram(1, &unk_82044360, 0x198=408) -> m_4tapPixelProgram  , handle "sampleOffsets"
        //
        // THE MEMBER NAMES AND THE SHADER-VARIABLE NAMES DO NOT DISAGREE -- this retires the note the
        // previous revision left here. The packages are decoded now, and the tap counts settle it
        // from two independent readings of each program (its tfetch count and its CTAB register
        // count):
        //   +0x20 (0x820444F8): 9 tfetch; CTAB sampleOffsets4x4_1 c0 x4, sampleOffsets4x4_2 c4 x4,
        //                       sampleOffsets4_1 c8 x1 == 9 registers -> a NINE-tap program, which is
        //                       exactly the 4 + 4 + 1 rows Blur9 pushes through its three handles.
        //   +0x30 (0x820447A8): 16 tfetch; sampleOffsets_1..4, four registers each == 16 -> the
        //                       SIXTEEN-tap program DownSample(METHOD_16TAP) fills.
        //   +0x44 (0x82044360): 4 tfetch; sampleOffsets c0 x4 -> the FOUR-tap program
        //                       METHOD_16TAP_WITH_BILINEAR uses (InitWeights_Blur16WithBilinear emits
        //                       exactly four bilinear taps over the 4x4 box).
        // So m_9tapPixelProgram / m_16tapPixelProgram / m_4tapPixelProgram are correct as the DWARF
        // named them, and the "4x4" in the 9-tap's variable names is the author's name for the
        // constant block, not a tap count.
        //
        // Each site now passes the PC image published by PostFxHelperProgramsPC.cpp; CreateProgram
        // adopts it (arm (a)) and never reaches the gate. The X360 address and byte size stay in the
        // comment above as the provenance of what each image was decoded from.
        m_quadVertexProgram = CreateProgram(0, renderengine::gauPostFxHelperQuadVertexProgramPC,
                                            renderengine::guPostFxHelperQuadVertexProgramPCSize,
                                            m_allocator);
        BindProgramVariable(m_quadVertexProgram, "uvOffset", m_uvOffsetHandle);

        m_9tapPixelProgram = CreateProgram(1, renderengine::gauPostFxHelperBlur9PixelProgramPC,
                                           renderengine::guPostFxHelperBlur9PixelProgramPCSize,
                                           m_allocator);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_1", m_blur9SamplesHandle[0]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_2", m_blur9SamplesHandle[1]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4_1",   m_blur9SamplesHandle[2]);

        m_16tapPixelProgram = CreateProgram(1, renderengine::gauPostFxHelperBlur16PixelProgramPC,
                                            renderengine::guPostFxHelperBlur16PixelProgramPCSize,
                                            m_allocator);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_1", m_blur16SamplesHandle[0]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_2", m_blur16SamplesHandle[1]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_3", m_blur16SamplesHandle[2]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_4", m_blur16SamplesHandle[3]);

        m_4tapPixelProgram = CreateProgram(1, renderengine::gauPostFxHelperBlur4PixelProgramPC,
                                           renderengine::guPostFxHelperBlur4PixelProgramPCSize,
                                           m_allocator);
        BindProgramVariable(m_4tapPixelProgram, "sampleOffsets", m_blur4SamplesHandle);

        // ---- 6. the shared opaque blend state (asm 0x824086F4) -------------------------------------
        CreateStates();

        // ---- 7. the render-target debugger singleton (asm 0x824086F8-0x824087B0) -------------------
        // Built ONCE, guarded by its own file-scope pointer. The carve is the console's:
        // entry 0 = {0xC, 0x10} (`v37[0] = 0xC00000010LL` -- size 12, alignment 16), the remaining
        // entries the {0, 1} identity. 12 is the GUEST size of the three-pointer debugger, so the
        // host sizeof is used and the console immediate documented -- the same rule (and the same
        // failure it prevents) as BrnPostFx::Construct's five carves.
        if (gpRenderTargetDebugger == nullptr)
        {
            rw::ResourceDescriptor lDescriptor;
            lDescriptor.m_baseResourceDescriptors[0].m_size =
                static_cast<u32>(sizeof(rw::graphics::postfx::RenderTargetDebugger));   // X360: 0xC
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10u;
            for (u32 luEntry = 1u; luEntry < 4u; ++luEntry)
            {
                lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
                lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
            }

            const rw::Resource lResource = m_allocator->DoAllocate(lDescriptor, nullptr);
            gpRenderTargetDebugger = (lResource.m_baseResources[0] != nullptr)
                ? new (lResource.m_baseResources[0]) rw::graphics::postfx::RenderTargetDebugger()
                : nullptr;
        }
    }

    // ============================================================================================
    // PfxHelper::Release @0x82402E58
    //
    // Nine SafeRelease<> calls in the console's own order -- which is NOT declaration order, and is
    // reproduced as found (blend state, vertex descriptor, vertex buffer, mesh, depth/stencil, then
    // the four programs quad/4tap/16tap/9tap) -- followed by clearing the allocator and the
    // singleton. m_quadDrawParams (+0x10) is deliberately NOT released: the console never touches it.
    // ============================================================================================
    void PfxHelper::Release()
    {
        SafeRelease(m_opaqueBlendState);       // 0x82402E70  +0x4C  (out-of-line SafeRelease<BlendState>)
        SafeRelease(m_quadVertexDescriptor);   // 0x82402E78  +0x08
        SafeRelease(m_quadVertexBuffer);       // 0x82402EC8  +0x0C
        SafeRelease(m_quadMeshState);          // 0x82402F14  +0x04
        SafeRelease(m_quadDepthStencilState);  // 0x82402F60  +0x14  (out-of-line SafeRelease<DepthStencilState>)
        SafeRelease(m_quadVertexProgram);      // 0x82402F6C  +0x18
        SafeRelease(m_4tapPixelProgram);       // 0x82402FB8  +0x44
        SafeRelease(m_16tapPixelProgram);      // 0x82403004  +0x30
        SafeRelease(m_9tapPixelProgram);       // 0x82403050  +0x20

        // asm 0x82403098-0x824030A4: `stw r31, 0(r30)` then `stw r31, off_82FAEE80@l(r11)`.
        m_allocator = nullptr;
        gpPfxHelper = nullptr;
    }

    // ============================================================================================
    // The blur / down-sample passes -- Blur9 @0x824030C0, DownSample @0x824032C8, RenderQuad
    // @0x823FE530. (Gate-flip wave, 2026-08-15; DepthOfField::DownSampleAndGaussianBlur is the
    // consumer.) Each pass is: Begin the destination, build a weight table into the console's
    // file-scope table (X360 unk_82FAED00 for the 9-tap, unk_82FAEC00 for the 16-tap, unk_82FAED90
    // for the bilinear 4), push it through the program's constant handles, bind the SOURCE target's
    // colour TextureState at unit 0 (`lwz r3, 0x2C(source)` = maColourTargets[0].mpTextureState,
    // through shadow::Device::SetState(const TextureState*, u32) @0x8227D158), bind the pixel
    // program, draw the shared quad, Resolve the destination.
    //
    // ⚠ PC constant rows: renderengine::Device::BeginShaderStates opens ONE run of rows for ONE
    // handle and the PC leaf caps a run at four rows (KU_ROW_BYTES). The console's DownSample writes
    // its sixteen rows after ONE BeginShaderStates(+0x34) -- the four sampleOffsets_1..4 constants
    // are consecutive registers, so one cursor walks all four -- which on PC would overrun the row
    // buffer. Pushed here through the four handles, four rows each: identical registers, identical
    // stores, one bound-safe run per handle. Blur9's three runs (4 + 4 + 1) already go through three
    // separate handles on the console (`+0x24 / +0x28 / +0x2C`), so it is one-to-one.
    //
    // On this build all four helper programs are REAL: the PC images published by
    // PostFxHelperProgramsPC.cpp are adopted in the constructor, so the weight rows below upload
    // through live handles and SetPixelProgram binds a live program. What is still missing is the
    // QUAD (RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE 0), so RenderQuad reports once and draws nothing
    // and neither pass produces pixels. Neither pass is reachable while every effect bit is clear.
    // ============================================================================================
    namespace
    {
        // The console's file-scope weight tables (float4 rows): unk_82FAED00 (9), unk_82FAEC00
        // (16), unk_82FAED90 (4). Sized as the generators fill them.
        f32 safBlur9Weights [ 9 * 4];
        f32 safBlur16Weights[16 * 4];
        f32 safBlur4Weights [ 4 * 4];

        // One BeginShaderStates run for lrHandle, then luRows float4 rows out of lpafTable in
        // order -- the console's `do { *cursor++ = *table++ x4 } while (--n)` loop.
        void PushWeightRows(renderengine::ProgramVariableHandle& lrHandle,
                            const f32* lpafTable, u32 luRows)
        {
            void* lpCursor = nullptr;
            RenderEngineDeviceBeginShaderStates(&lrHandle, &lpCursor);
            if (lpCursor == nullptr)
                return;
            f32* lpfRow = static_cast<f32*>(lpCursor);
            for (u32 luRow = 0; luRow < luRows; ++luRow)
            {
                lpfRow[0] = lpafTable[0];
                lpfRow[1] = lpafTable[1];
                lpfRow[2] = lpafTable[2];
                lpfRow[3] = lpafTable[3];
                lpfRow    += 4;
                lpafTable += 4;
            }
        }

        const u32 KU_HELPER_SAMPLER_SOURCE = 0u;   // `li r4, 0` at every sub_8227D158 call
    }

    // X360 0x824030C0.
    void PfxHelper::Blur9(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                          f32 lfAngle, f32 lfRadius, f32 lfWeightFactor)
    {
        lpDestRenderTarget->Begin(0u);                                                // bl @0x824030F8 (li r4,0)

        // The 9-tap directional table from the SOURCE's extent (`lwz r4, 4(r5)` / `lwz r5, 8(r5)`).
        InitWeights_DirBlur9Quadratic(safBlur9Weights,
                                      static_cast<s32>(lpSourceRenderTarget->muWidth),
                                      static_cast<s32>(lpSourceRenderTarget->muHeight),
                                      lfAngle, lfRadius, lfWeightFactor);            // bl @0x8240311C

        // Three runs through the 9-tap program's three handles: 4 + 4 + 1 rows (`addi r3, r30, 0x24 /
        PushWeightRows(m_blur9SamplesHandle[0], &safBlur9Weights[0 * 4], 4u);
        PushWeightRows(m_blur9SamplesHandle[1], &safBlur9Weights[4 * 4], 4u);
        PushWeightRows(m_blur9SamplesHandle[2], &safBlur9Weights[8 * 4], 1u);

        shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                 KU_HELPER_SAMPLER_SOURCE);                            // lwz r3,0x2C(r28) / bl @0x824031F8
        shadow::Device::SetPixelProgram(m_9tapPixelProgram);                           // bl @0x82403200

        // The DESTINATION's half texel (`lwz 4(r4)`, `lwz 8(r4)` -> 1/w, 1/h -> * 0.5 splat), the
        // uvOffset the quad's vertex program applies (0x82403208-0x82403298).
        f32 lafUvOffset[4];
        lafUvOffset[0] = 0.5f * (1.0f / static_cast<f32>(lpDestRenderTarget->muWidth));
        lafUvOffset[1] = 0.5f * (1.0f / static_cast<f32>(lpDestRenderTarget->muHeight));
        lafUvOffset[2] = 0.0f;
        lafUvOffset[3] = 0.0f;
        RenderQuad(lafUvOffset);                                                       // bl @0x8240329C

        lpDestRenderTarget->Resolve(true, true);                                       // bl @0x824032AC
    }

    // X360 0x824032C8.
    void PfxHelper::DownSample(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                               Method leMethod)
    {
        lpDestRenderTarget->Begin(0u);                                                // bl @0x824032EC (li r4,0)

        if (leMethod == METHOD_16TAP)                                                  // cmpwi cr6, r31, 0 @0x824032F0
        {
            InitWeights_Blur16(safBlur16Weights,
                               static_cast<s32>(lpSourceRenderTarget->muWidth),
                               static_cast<s32>(lpSourceRenderTarget->muHeight));     // bl @0x8240330C
            // Sixteen rows: the console walks one cursor from +0x34 across all four consecutive
            // sampleOffsets_1..4 registers; PC pushes the same rows through the four handles.
            PushWeightRows(m_blur16SamplesHandle[0], &safBlur16Weights[ 0 * 4], 4u);
            PushWeightRows(m_blur16SamplesHandle[1], &safBlur16Weights[ 4 * 4], 4u);
            PushWeightRows(m_blur16SamplesHandle[2], &safBlur16Weights[ 8 * 4], 4u);
            PushWeightRows(m_blur16SamplesHandle[3], &safBlur16Weights[12 * 4], 4u);
            shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                     KU_HELPER_SAMPLER_SOURCE);                        // lwz r3,0x2C(r30) / bl @0x82403364
            shadow::Device::SetPixelProgram(m_16tapPixelProgram);                      // +0x30, bl @0x8240336C
            RenderQuad(nullptr);                                                       // li r4, 0
        }
        else if (leMethod == METHOD_16TAP_WITH_BILINEAR)                               // cmpwi cr6, r31, 1 @0x82403374
        {
            InitWeights_Blur16WithBilinear(safBlur4Weights,
                                           static_cast<s32>(lpSourceRenderTarget->muWidth),
                                           static_cast<s32>(lpSourceRenderTarget->muHeight));
            // The console binds the program BEFORE the constant run on this arm (SetPixelProgram
            // @0x82403398, then BeginShaderStates @0x824033A4) -- order preserved.
            shadow::Device::SetPixelProgram(m_4tapPixelProgram);                       // +0x44
            PushWeightRows(m_blur4SamplesHandle, safBlur4Weights, 4u);                 // +0x48
            shadow::Device::SetState(lpSourceRenderTarget->maColourTargets[0].mpTextureState,
                                     KU_HELPER_SAMPLER_SOURCE);
            RenderQuad(nullptr);
        }
        // Any other method: no table, no draw -- the console falls straight to the Resolve.

        lpDestRenderTarget->Resolve(true, true);                                       // bl @0x82403410
    }

    // X360 0x823FE530.
    //
    // THE CONSOLE BODY, RECOVERED IN FULL (it was only sketched before). Every shadow-cache
    // compare-and-store in the asm is one of the shadow::Device binders inlined; AGENTS.md's
    // inlining-reversal rule says restore the call, and restoring it is also what keeps this TU off
    // four private statics of shadow::Device (dword_83010A28 / off_83010954 / dword_8301095C /
    // off_83010958, all already homed there):
    //     0x823FE53C-80  shadow::Device::SetState(m_quadDepthStencilState)              (+0x14)
    //     0x823FE584-A8  renderengine::MeshHelper::Dispatch<renderengine::Device>(
    //                        m_quadMeshState)                                           (+0x04)
    //     0x823FE5AC-C4  shadow::Device::SetVertexProgram(m_quadVertexProgram)           (+0x18)
    //     0x823FE5C8-E0  shadow::Device::SetVertexDescriptor(m_quadVertexDescriptor)     (+0x08)
    //     0x823FE5E4-EC  renderengine::Device::BeginShaderStates(&m_uvOffsetHandle, &cursor)
    //                        -- `addi r3, r29, 0x1C` IS m_uvOffsetHandle: one run, one constant
    //     0x823FE5F0-620 cursor[0..3] = lpafUvOffset ? the caller's float4 (`lvx128`/`stvx128`)
    //                        : four copies of flt_82001CC0 == 0.0f
    //     0x823FE624     shadow::Device::FlushVertexProgramState()
    //     0x823FE628-3C  D3DDevice_DrawVertices(gpD3DDevice, 8, 0, 3)
    // PrimitiveType 8 is Xenos D3DPT_RECTLIST and VertexCount 3 is the rect's three corners -- the
    // same three the constructor uploads.
    //
    // [FLAG BLOCKED: RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE] The sequence above is documented rather
    // than written, for the four reasons enumerated at the gate -- and NOT merely because the corner
    // values are missing. Even with the values: m_quadMeshState / m_quadVertexDescriptor are null on
    // this build so Dispatch would dereference null on its first read; MeshHelper.cpp is not on
    // tools/build/build_game_exe.bat; D3DDevice_DrawVertices has no definition in the tree; and
    // RECTLIST is refused by XenonD3D9Shims.cpp::MapPrimitive. Writing the calls now would add two
    // unresolvable externals to the link for a path that cannot execute, and drawing a fabricated
    // triangle instead would read as a rendering choice. The four PROGRAMS this function would bind
    // are real and adopted; only the geometry and the draw are missing.
    void PfxHelper::RenderQuad(const f32* lpafUvOffset)
    {
#if RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE
#error "RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE is on but the quad draw is not reconstructed -- see the BLOCKED note"
#else
        (void)lpafUvOffset;
        static bool sbReported = false;
        ReportOnce(sbReported,
                   "[rwgpfx] PfxHelper::RenderQuad: NO quad geometry on this build -- the pass drew"
                   " nothing (destination left as it was). [FLAG BLOCKED:"
                   " RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE]\n");
#endif
    }
}
}
}
