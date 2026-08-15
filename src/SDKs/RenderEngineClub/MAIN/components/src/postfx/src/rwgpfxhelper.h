#ifndef RW_GPFX_HELPER_H
#define RW_GPFX_HELPER_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                              // rw::IResourceAllocator
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBufferData / ProgramVariableHandle
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"     // renderengine::BlendMaterialState
#include "pc/gcm/renderengine/VertexDescriptor.h"                            // renderengine::VertexDescriptorData
#include "pc/gcm/renderengine/VertexBuffer.h"                                // renderengine::VertexBufferHeader
#include "pc/gcm/renderengine/renderstates.h"                                // renderengine::MeshHelper / DepthStencilState

// rw::graphics::postfx::PfxHelper -- the post-fx singleton helper. It owns the shared post-fx
// render state (the full-screen-quad geometry + descriptor, the opaque-blend / depth-stencil states)
// and the small library of blur/copy shader programs every effect (DepthOfField, Tint,
// RenderTargetDebugger, ...) compiles against via CreateProgram.
//
// SHAPE: the member offsets are X360-asm authoritative (PfxHelper @0x82408348 ctor stores into r31
// at the offsets noted; CreateStates @0x82402D88 stores the blend state at this+0x4C; Release
// @0x82402E58 releases each member). No DWARF/leak source models the class body, so the layout is
// the constructor's store map. The handle gaps (gap24 / gap34) hold ProgramVariableHandle words the
// constructor binds by name immediately after each CreateProgram.

namespace renderengine
{
    // The packed 19-word blend block BlendState::Initialize returns and shadow::Device::SetState
    // takes; its home is .../states/blendstate.h:35. Forward-declared (pointer-only) so this header
    // does not pull the whole state surface into every post-fx includer -- the same treatment
    // pc/gcm/renderengine/renderstates.h gives it.
    struct BlendMaterialState;
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    class RenderTargetDebugger;
    class RenderTarget;   // pointer-only here; rwgpfxrendertarget.h is the home

    class PfxHelper
    {
    public:
        // DWARF rwgpfxhelper.h:31 -- the down-sample tap pattern PfxHelper::DownSample selects
        // between. Recovered from the DecFIGS DWARF; neither enumerator is referenced by a body
        // this TU owns yet, so both are declaration-only here.
        enum Method
        {
            METHOD_16TAP               = 0,
            METHOD_16TAP_WITH_BILINEAR = 1
        };

        // DWARF rwgpfxhelper.h:39-43 -- the construction parameter block. ONE member, the rw
        // resource allocator the helper keeps and hands to every renderengine resource it builds.
        //
        // ⚠ SIGNATURE CORRECTION (was `PfxHelper(rw::IResourceAllocator**)`). The X360 call site
        // BrnPostFx::Construct @0x8240A034 passes `addi r4, r1, 0x300+var_278`, and var_278 is the
        // stack word the allocator was written into at 0x82409FC4 -- i.e. the ADDRESS OF THE
        // PARAMETER BLOCK, whose first member happens to be the allocator. The DWARF names that
        // block (`PfxHelper(const Parameters&)`, rwgpfxhelper.h:110). `IResourceAllocator**` and
        // `const Parameters&` spell the same address, but only one of them is the declaration the
        // original source had, and only the typed one survives the day Parameters grows a member.
        struct Parameters
        {
            rw::IResourceAllocator* allocator;   // rwgpfxhelper.h:41

            // rwgpfxhelper.h:43. INLINED at the X360 call site (BrnPostFx::Construct writes the
            // allocator word directly and nothing else), so the body is empty -- `allocator` is
            // left for the caller to assign, exactly as the console leaves it.
            Parameters();
        };

        // The post-fx vertex (Position + UV) the full-screen-quad uses; the ctor uploads the quad
        // into the locked vertex buffer (DWARF rw::graphics::postfx::Vertex[4]).
        struct Vertex
        {
            f32 maPosition[3];  // +0x00
            f32 maUv[2];        // +0x0C  (only the first two words of the trailing vector are stored)
        };

        // Construct the helper from its parameter block. X360 0x82408348.
        explicit PfxHelper(const Parameters& lrParameters);

        // Tear the helper down: release every owned renderengine resource and clear the singleton.
        // X360 0x82402E58. DWARF rwgpfxhelper.h:51 -- returns void (the X360 `blr` leaves whatever
        // the last DoFree returned in r3; BrnPostFx::Destruct ignores it, and the DWARF is the
        // authority for the declaration shape).
        void Release();

        // Compile a shader program from embedded microcode and return its runtime buffer.
        //   leType     0 -> vertex program, 1 -> pixel program (the renderengine shader-kind flag)
        //   lpMicrocode  pointer to the embedded compiled-shader microcode blob (platform data)
        //   luSize       microcode byte size
        //   lpReserved   the allocator override; 0 at every X360 call site (-> the singleton's own).
        // X360 0x823FE480.
        static renderengine::ProgramBufferData* CreateProgram(s32 leType, const void* lpMicrocode,
                                                              u32 luSize, rw::IResourceAllocator* lpReserved);

        // Build the shared opaque blend state and store it at +0x4C. X360 0x82402D88.
        // DWARF rwgpfxhelper.h:112 -- void (the console's r3 on return is the state pointer the
        // store already published; no caller reads it).
        void CreateStates();

        // The allocator the helper was constructed with (X360 PfxHelper +0x00, read by
        // BrnPostFx::Destruct @0x824081C0 to free the depth-of-field program's resource).
        // DWARF rwgpfxhelper.h:81. Inline: the X360 read is a single `lwz` and an out-of-line body
        // would cost the link a symbol for a member read.
        rw::IResourceAllocator* GetAllocator() { return m_allocator; }

        // The shared opaque blend state CreateStates builds at +0x4C (DWARF rwgpfxhelper.h:76).
        // Inline for the same reason as GetAllocator: no standalone X360 export exists; every reader
        // is a `lwz rN, 0x4C(helper)` (DepthOfField::DownSampleAndGaussianBlur's `lwz r29, 0x4C(r31)`
        // @0x824082A0, which pushes it through shadow::Device::SetState(const BlendMaterialState*)).
        renderengine::BlendMaterialState* GetOpaqueBlendState() const { return m_opaqueBlendState; }

        // --- The blur / down-sample passes the effects drive the shared quad with ------------------
        // Both are REAL standalone X360 functions (bodies in rwgpfxhelper.cpp).
        //
        // Blur9 @0x824030C0: r3 this, r4/r5 the two render targets, f1/f2/f3 the three floats
        // (`fmr f31, f1` / `fmr f30, f2` / `fmr f29, f3` @0x824030DC-EC). Begin()s the destination,
        // builds a 9-tap directional weight table through InitWeights_DirBlur9Quadratic(angle, radius,
        // weightFactor) and draws the shared quad. DWARF rwgpfxhelper.h:59.
        void Blur9(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                   f32 lfAngle, f32 lfRadius, f32 lfWeightFactor);

        // DownSample @0x824032C8: Begin() the destination, build a 16-tap box weight table (bilinear
        // or not, per leMethod) and draw the shared quad. DWARF rwgpfxhelper.h:65.
        void DownSample(RenderTarget* lpDestRenderTarget, RenderTarget* lpSourceRenderTarget,
                        Method leMethod);

        // --- Blur-weight table generators (pure float math; output buffers are caller-owned) ------
        // 16-tap 4x4 box blur with bilinear sampling: 4x4 grid of (offsetX, offsetY, 0.25, 1.0).
        // X360 0x823F8E78.
        static void InitWeights_Blur16WithBilinear(f32* lpBlurWeights, s32 liWidth, s32 liHeight);

        // 16-tap 4x4 box blur, normalised so the 16 equal 0.0625 weights sum to 1. X360 0x823F8F50.
        static void InitWeights_Blur16(f32* lpBlurWeights, s32 liWidth, s32 liHeight);

        // 9-tap directional blur with a quadratic falloff along (cos angle, sin angle), normalised by
        // the accumulated weight and scaled by weightFactor. X360 0x823F8C70.
        static void InitWeights_DirBlur9Quadratic(f32* lpBlurWeights, s32 liWidth, s32 liHeight,
                                                  f32 lfAngle, f32 lfRadius, f32 lfWeightFactor);

    private:
        // RenderQuad @0x823FE530 -- draw the shared full-screen quad with the given UV offset
        // (Blur9 passes the destination's half texel; DownSample passes null). DWARF rwgpfxhelper.h:104.
        // On this build the quad has no geometry (RW_GPFX_HELPER_QUAD_GEOMETRY_AVAILABLE 0 -- see the
        // constructor's BLOCKED note), so the body reports once and draws nothing.
        void RenderQuad(const f32* lpafUvOffset);

        // rwgpfxhelper.h:176 -- the teardown helper Release calls nine times. TWO instantiations
        // survived out-of-line in the X360 image and pin the body exactly:
        //   SafeRelease<renderengine::BlendState>        @0x823FF480
        //   SafeRelease<renderengine::DepthStencilState> @0x823FF4F8
        // Both are: `if (ptr) { <T::Release>; rw::Resource r = {ptr,0,0,0,0};
        //            m_allocator->DoFree(r); ptr = 0; }`
        // -- and in BOTH of those two the `T::Release` step is ABSENT, because a BlendState / a
        // DepthStencilState is a plain marshalled state block with a trivial (inlined-away) Release,
        // while the seven inlined instantiations (VertexDescriptor / VertexBuffer / MeshHelper /
        // ProgramBuffer) DO emit their vendor Release first. See the ReleaseObject() overload set in
        // rwgpfxhelper.cpp for how that per-T step is spelled against this tree's PC type vocabulary.
        template< class T > void SafeRelease(T*& lprObject);

        // --- SHAPE (DWARF rwgpfxhelper.h:116-150; every offset below is CONFIRMED by the X360
        // constructor's store map and by PfxHelper::Release's nine release sites) ------------------
        rw::IResourceAllocator*             m_allocator;               // +0x00  :116
        renderengine::MeshHelper::MeshData* m_quadMeshState;           // +0x04  :122
        renderengine::VertexDescriptorData* m_quadVertexDescriptor;    // +0x08  :125
        renderengine::VertexBufferHeader*   m_quadVertexBuffer;        // +0x0C  :126
        // :127 DrawParameters* -- NEVER written by the constructor and NEVER released. No tree type
        // models the renderengine draw-parameter block, so it is carried as an opaque pointer (a
        // reference-only member; the forward-declaration exception applies, and there is nothing to
        // point it at yet).
        void*                               m_quadDrawParams;          // +0x10  :127
        renderengine::DepthStencilState*    m_quadDepthStencilState;   // +0x14  :128
        renderengine::ProgramBufferData*    m_quadVertexProgram;       // +0x18  :129 ("uvOffset")
        renderengine::ProgramVariableHandle m_uvOffsetHandle;          // +0x1C  :134
        renderengine::ProgramBufferData*    m_9tapPixelProgram;        // +0x20  :139
        renderengine::ProgramVariableHandle m_blur9SamplesHandle[3];   // +0x24  :140
        renderengine::ProgramBufferData*    m_16tapPixelProgram;       // +0x30  :142
        renderengine::ProgramVariableHandle m_blur16SamplesHandle[4];  // +0x34  :143
        renderengine::ProgramBufferData*    m_4tapPixelProgram;        // +0x44  :145
        renderengine::ProgramVariableHandle m_blur4SamplesHandle;      // +0x48  :146
        renderengine::BlendMaterialState*   m_opaqueBlendState;        // +0x4C  :148
        // :150 `static PfxHelper* m_instance` is the DWARF's spelling of the singleton. The X360
        // spells it as the file-scope pointer off_82FAEE80 and this tree already publishes it under
        // that shape (gpPfxHelper, below) -- BrnPostFx.cpp reaches it by that name. Not duplicated
        // here: one home only (no split brain).
    };
}
}
}

namespace rw
{
namespace graphics
{
namespace postfx
{
    // X360 off_82FAEE80 -- THE post-fx helper singleton, the DWARF's
    // `static PfxHelper* PfxHelper::m_instance` (rwgpfxhelper.h:150) in its X360 file-scope spelling.
    // PfxHelper::PfxHelper @0x82408348 publishes itself here as its SECOND act; PfxHelper::Release
    // @0x824030A4 clears it; every post-fx effect and BrnPostFx::Destruct read it. DEFINED in
    // rwgpfxhelper.cpp -- there is exactly one home (the TU-local second copy that body used to
    // carry is gone).
    extern PfxHelper* gpPfxHelper;
}
}
}

#endif // RW_GPFX_HELPER_H
