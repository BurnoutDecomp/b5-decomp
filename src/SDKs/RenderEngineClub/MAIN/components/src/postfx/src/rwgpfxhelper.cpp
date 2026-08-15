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
// ⚠ THE MICROCODE BYTES FOR EVERY SITE IN THIS TU AND IN rwgpfxb4blur.cpp / rwgpfxtint.cpp ARE
// ABSENT from the function-only IDA export and from scratch/postfx_wave1b_dossiers/DATA_DUMP.md
// (which carries only the first 32 bytes of one of them). They ARE recoverable the same way
// rwgpfxrendertargetdebugger.cpp already recovered its own pair -- by dumping the .rdata range out
// of the ARTIST .i64 -- and the exact addresses/sizes are listed at each site and in the wave
// report. Nothing is invented in the meantime.
#define RW_GPFX_PROGRAM_MICROCODE_AVAILABLE   0

// ---- PC BRING-UP GATE: the full-screen-quad geometry -------------------------------------------
// The constructor's vertex-descriptor / vertex-buffer / mesh block uploads the quad's vertices from
// the guest data block at X360 unk_82FAFFF0. Those bytes are not in the export set (function-only)
// and not in DATA_DUMP.md, and renderengine::MeshHelper::GetResourceDescriptor -- which the console
// calls -- has no declaration anywhere in this tree. See the BLOCKED note at the site.
#define RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE 0

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
        // X360 `bl STUB` @0x82402F20 -- the mesh's own Release, which IDA could not name. This tree
        // has no renderengine::MeshHelper::Release declaration to call.
        // [FLAG BLOCKED: renderengine::MeshHelper::Release -- X360 callee at 0x82402F20 unnamed in
        //  the export ("STUB"), no declaration in the tree. The resource is still handed back below,
        //  so the only thing missing is the vendor object's own teardown.]
    }
    void ReleaseObject(renderengine::DepthStencilState*)
    {
        // Empty on the console: SafeRelease<DepthStencilState> @0x823FF4F8 emits no Release call.
    }
    void ReleaseObject(renderengine::BlendMaterialState*)
    {
        // Empty on the console: SafeRelease<BlendState> @0x823FF480 emits no Release call.
    }

    // ---- the four embedded microcode packages the constructor compiles --------------------------
    // The pointers are null because the BYTES ARE ABSENT (see the gate). The X360 address and the
    // byte size are the asm's own immediates and are carried so a later shader wave can fill them in
    // without re-deriving anything. A null image makes CreateProgram return null, which is the
    // honest-empty slot; it is NOT a one-byte placeholder standing in for a 288-byte blob (that is
    // the shape that lets a console route read off the end of a stand-in).
    const void* const KP_QUAD_VERTEX_PROGRAM  = nullptr;  // X360 &unk_82044240
    const u32         KU_QUAD_VERTEX_PROGRAM_SIZE  = 288u;
    const void* const KP_9TAP_PIXEL_PROGRAM   = nullptr;  // X360 &unk_820444F8
    const u32         KU_9TAP_PIXEL_PROGRAM_SIZE   = 688u;
    const void* const KP_16TAP_PIXEL_PROGRAM  = nullptr;  // X360 &unk_820447A8
    const u32         KU_16TAP_PIXEL_PROGRAM_SIZE  = 948u;
    const void* const KP_4TAP_PIXEL_PROGRAM   = nullptr;  // X360 &unk_82044360
    const u32         KU_4TAP_PIXEL_PROGRAM_SIZE   = 408u;

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
        // [FLAG BLOCKED: TWO missing items, neither guessable.
        //   (a) THE VERTEX DATA. The lvx128/vspltw/stvewx sequence at 0x824083E0-0x824084A0 sources
        //       every position lane from the guest data block at X360 unk_82FAFFF0 (0x40 bytes,
        //       .data). The IDA export is function-only and DATA_DUMP.md does not carry it, so the
        //       quad's corner positions are unknown. They are NOT "obviously the unit quad": the
        //       vertex count in the buffer parameters is 3, not 4, so this is a full-screen TRIANGLE
        //       and its two out-of-range corners cannot be inferred from the four-vertex Vertex[4]
        //       the DWARF names.
        //   (b) renderengine::MeshHelper::GetResourceDescriptor has NO declaration in this tree, and
        //       MeshHelper::Initialize is declared non-static with one parameter while the console
        //       calls it with (resource, parameters). Both would have to be minted.
        //  CONSEQUENCE: the helper has no quad, so PfxHelper::RenderQuad / Blur9 / Blur16 /
        //  DownSample cannot draw. None of them is reachable on this build -- BrnPostFx runs
        //  permutation 0 with every effect bit clear -- and the null members make a premature call
        //  crash rather than draw garbage.]
        static bool sbReportedNoQuad = false;
        ReportOnce(sbReportedNoQuad,
                   "[PfxHelper] full-screen-quad geometry not built: the quad's vertex data"
                   " (X360 unk_82FAFFF0, 0x40 bytes) is absent from the export set and"
                   " MeshHelper::GetResourceDescriptor has no declaration."
                   " [FLAG BLOCKED: RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE]\n");
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
        // The console builds, in order:
        //   CreateProgram(0, &unk_82044240, 288) -> m_quadVertexProgram   , handle "uvOffset"
        //   CreateProgram(1, &unk_820444F8, 688) -> m_9tapPixelProgram    , handles
        //                                            "sampleOffsets4x4_1" / "_2" / "sampleOffsets4_1"
        //   CreateProgram(1, &unk_820447A8, 948) -> m_16tapPixelProgram   , handles
        //                                            "sampleOffsets_1" .. "_4"
        //   CreateProgram(1, &unk_82044360, 408) -> m_4tapPixelProgram    , handle "sampleOffsets"
        // (The DWARF's 9tap/16tap member names and the shader-variable names disagree about which
        // slot is which -- the +0x20 program binds the "4x4" names and the +0x30 program the plain
        // ones. The MEMBER names are the DWARF's and the OFFSETS are the asm's; both are reproduced
        // as found and neither is "corrected", because only the microcode could settle it.)
        //
        // Each site passes the microcode pointer as null with its X360 address and byte size
        // recorded, because the bytes are absent (see the gate). CreateProgram returns null for a
        // null image, so every slot above stays null and every handle stays zero-count.
        //
        // [FLAG BLOCKED: four Xenos microcode packages, extractable from the ARTIST .i64 exactly the
        //  way rwgpfxrendertargetdebugger.cpp already extracted its own pair:
        //    0x82044240  288 bytes  vertex  ("uvOffset")
        //    0x820444F8  688 bytes  pixel   ("sampleOffsets4x4_1", "sampleOffsets4x4_2",
        //                                    "sampleOffsets4_1")
        //    0x820447A8  948 bytes  pixel   ("sampleOffsets_1".."sampleOffsets_4")
        //    0x82044360  408 bytes  pixel   ("sampleOffsets")
        //  A PC replacement additionally needs an HLSL pair per program; none is authored in this
        //  wave by design.]
        m_quadVertexProgram = CreateProgram(0, KP_QUAD_VERTEX_PROGRAM, KU_QUAD_VERTEX_PROGRAM_SIZE,
                                            m_allocator);
        BindProgramVariable(m_quadVertexProgram, "uvOffset", m_uvOffsetHandle);

        m_9tapPixelProgram = CreateProgram(1, KP_9TAP_PIXEL_PROGRAM, KU_9TAP_PIXEL_PROGRAM_SIZE,
                                           m_allocator);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_1", m_blur9SamplesHandle[0]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4x4_2", m_blur9SamplesHandle[1]);
        BindProgramVariable(m_9tapPixelProgram, "sampleOffsets4_1",   m_blur9SamplesHandle[2]);

        m_16tapPixelProgram = CreateProgram(1, KP_16TAP_PIXEL_PROGRAM, KU_16TAP_PIXEL_PROGRAM_SIZE,
                                            m_allocator);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_1", m_blur16SamplesHandle[0]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_2", m_blur16SamplesHandle[1]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_3", m_blur16SamplesHandle[2]);
        BindProgramVariable(m_16tapPixelProgram, "sampleOffsets_4", m_blur16SamplesHandle[3]);

        m_4tapPixelProgram = CreateProgram(1, KP_4TAP_PIXEL_PROGRAM, KU_4TAP_PIXEL_PROGRAM_SIZE,
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
    // On this build every helper program is honestly empty (RW_GPFX_PROGRAM_MICROCODE_AVAILABLE 0)
    // -- zero-count handles route the rows to the leaf's discard row, SetPixelProgram binds null --
    // and the quad has no geometry (RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE 0), so RenderQuad reports
    // once and draws nothing. Neither pass is reachable while every effect bit is clear.
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
    void PfxHelper::RenderQuad(const f32* lpafUvOffset)
    {
        (void)lpafUvOffset;
        // [FLAG BLOCKED: RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE] The console pushes lpafUvOffset
        // through m_uvOffsetHandle, binds m_quadVertexProgram / m_quadVertexDescriptor /
        // m_quadDepthStencilState and issues the mesh draw. This build's helper owns no quad (the
        // guest vertex block unk_82FAFFF0 is not dumped -- see the constructor), so there is nothing
        // to draw and drawing a fabricated triangle would read as a rendering choice.
        static bool sbReported = false;
        ReportOnce(sbReported,
                   "[rwgpfx] PfxHelper::RenderQuad: NO quad geometry on this build -- the pass drew"
                   " nothing (destination left as it was). [FLAG BLOCKED:"
                   " RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE]\n");
    }
}
}
}
