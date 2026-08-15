#ifndef RW_GPFX_HELPER_H
#define RW_GPFX_HELPER_H

#include "types.hpp"
#include "rw/rwcore_structs.h"                                              // rw::IResourceAllocator
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // renderengine::ProgramBufferData / ProgramVariableHandle

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
namespace rw
{
namespace graphics
{
namespace postfx
{
    class RenderTargetDebugger;

    class PfxHelper
    {
    public:
        // The post-fx vertex (Position + UV) the full-screen-quad uses; the ctor uploads four of them
        // into the locked vertex buffer (DWARF rw::graphics::postfx::Vertex[4]).
        struct Vertex
        {
            f32 maPosition[3];  // +0x00
            f32 maUv[2];        // +0x0C  (only the first two words of the trailing vector are stored)
        };

        // Construct the helper. lppParameters[0] is the rw resource allocator the helper keeps and
        // hands to every renderengine resource it builds. X360 0x82408348.
        explicit PfxHelper(rw::IResourceAllocator** lppParameters);

        // Tear the helper down: release every owned renderengine resource and clear the singleton.
        // X360 0x82402E58.
        s32 Release();

        // Compile a shader program from embedded microcode and return its runtime buffer.
        //   leType     0 -> vertex program, 1 -> pixel program (the renderengine shader-kind flag)
        //   lpMicrocode  pointer to the embedded compiled-shader microcode blob (platform data)
        //   luSize       microcode byte size
        //   lpReserved   the allocator override; 0 at every X360 call site (-> the singleton's own).
        // X360 0x823FE480.
        static renderengine::ProgramBufferData* CreateProgram(s32 leType, const void* lpMicrocode,
                                                              u32 luSize, rw::IResourceAllocator* lpReserved);

        // Build the shared opaque blend state and store it at +0x4C. X360 0x82402D88.
        s32 CreateStates();

        // The allocator the helper was constructed with (X360 PfxHelper +0x00, read by
        // BrnPostFx::Destruct @0x824081C0 to free the depth-of-field program's resource).
        rw::IResourceAllocator* GetAllocator();

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
        // The owned renderengine resources are opaque runtime handles -- the constructor stores the
        // Initialize() return value (a vendor resource pointer) verbatim; their concrete layouts live
        // in their own vendor TUs and are not needed at this storage site.
        rw::IResourceAllocator*           mpAllocator;          // +0x00
        void*                             mpMesh;               // +0x04
        void*                             mpVertexDescriptor;   // +0x08
        void*                             mpVertexBuffer;       // +0x0C
        u32                               muPad10;              // +0x10
        void*                             mpDepthStencilState;  // +0x14
        renderengine::ProgramBufferData*  mpCopyProgram;        // +0x18 (uvOffset program)
        renderengine::ProgramVariableHandle mUvOffsetHandle;    // +0x1C
        renderengine::ProgramBufferData*  mpBlur16Program;      // +0x20
        renderengine::ProgramVariableHandle maBlur16Handles[3]; // +0x24 (sampleOffsets4x4_1/2, sampleOffsets4_1)
        renderengine::ProgramBufferData*  mpBlur9Program;       // +0x30
        renderengine::ProgramVariableHandle maBlur9Handles[4];  // +0x34 (sampleOffsets_1..4)
        renderengine::ProgramBufferData*  mpDirBlurProgram;     // +0x44
        renderengine::ProgramVariableHandle mDirBlurHandle;     // +0x48
        void*                             mpOpaqueBlendState;   // +0x4C
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
    // X360 off_82FAEE80 -- THE post-fx helper singleton. PfxHelper::PfxHelper @0x82408348 publishes
    // itself here; every post-fx effect and BrnPostFx::Destruct read it. Declaration only: the
    // definition belongs to the PfxHelper TU (rwgpfxhelper.cpp), which is not yet on the build list.
    extern PfxHelper* gpPfxHelper;
}
}
}

#endif // RW_GPFX_HELPER_H
