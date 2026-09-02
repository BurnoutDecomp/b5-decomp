// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.cpp
//
// BrnGraphics::Im3dSkidsRenderer -- the skid-decal immediate-mode renderer.
//
//   Construct @0x82295150 (X360 ARTIST; the ImRenderer<SkidVertex> base bodies
//   it drives live in BrnSkidVertex.cpp: Construct @0x8228E1D8, AddProgram
//   @0x822870B8, BeginRendering @0x8227C1E8, Render @0x8228E068).
//   SetTransform / SetBlendStartColour / SetBlendEndColour -- the inlined
//   BeginShaderStates + copy sequences in TrailRenderer::BeginRender
//   @0x82284468 and TrailRenderer::Render @0x82295930.
//
// THE PROGRAM PAIR. The X360 Construct hands ImRenderer<SkidVertex>::Construct
// two executable-embedded Xenos programs (unk_8200E9D0, 456 bytes vertex;
// unk_8200EB98, 228 bytes pixel -- `v5[0] = 456; v7 = 228` in the asm). They
// were disassembled with tools/assets/shaders/xenos.py:
//
//   vertex:  vfetch r2.xyz (position), r1.xyzw (uv, time, alpha)
//            oPos   = pos.x * c0 + pos.y * c1 + pos.z * c2 + c3     (gWorldViewProj, row-major)
//            colour = (c5 - c4) * r1.z + c4                          (lerp gStart->gEnd by TIME)
//            oCol   = { colour.xyz, colour.w * r1.w }                 (alpha * STRENGTH)
//            oTex0  = r1.xy
//   pixel:   oColour = tfetch(tf0, uv) * interpolated colour
//
// and re-authored as D3D9 vs_3_0 / ps_3_0 in tools/assets/shaders/brn_skid.fx,
// compiled + wrapped exactly like the sky dome's pair (SkyDomeProgramsPC.cpp)
// into SkidProgramsPC.cpp -- the PC platform leaf this TU binds.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnIm3dSkidsRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // memcpy (the raw shader-constant row copies)

// The two converted PC program images (pc/gcm/renderengine/SkidProgramsPC.cpp -- the PC stand-in
// for the two guest .data blobs; see that file's recipe).
namespace renderengine
{
    extern const u8  gauSkidVertexProgramPC[];
    extern const u32 guSkidVertexProgramPCSize;
    extern const u8  gauSkidPixelProgramPC[];
    extern const u32 guSkidPixelProgramPCSize;
}

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- the same minimal extern
// surface BrnIm3d.cpp / BrnSunCorona.cpp / BrnPostFxBloom.cpp declare (defined in
// ImmediateModePCLeaf.cpp). Returns the staged row the caller copies the constant into.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace BrnGraphics
{
    namespace
    {
        // One shader-constant write: open the row for the handle and copy the bytes into it --
        // the X360's `bl renderengine::Device::BeginShaderStates; stvx128 ...` pattern.
        void PushShaderConstant(renderengine::ProgramVariableHandle* lpHandle, const void* lpValue, u32 luBytes)
        {
            void* lpShaderState = 0;
            RenderEngineDeviceBeginShaderStates(lpHandle, &lpShaderState);
            if (lpShaderState != 0)
            {
                memcpy(lpShaderState, lpValue, luBytes);
            }
        }
    }

    // =========================================================================================
    // Im3dSkidsRenderer::Construct  @0x82295150
    //   Build the base renderer with the one skid program pair, then resolve gWorldViewProj /
    //   gStartColour / gEndColour against the VERTEX program (`lwz r3, 0x14(this)` before each
    //   lookup == mapVertexProgramBuffer[0]). The X360 re-asserts "mapVertexProgramBuffer[
    //   li8Program ] != NULL" (CgsImRenderer.h:570) before every lookup; hoisted to one check.
    // =========================================================================================
    void Im3dSkidsRenderer::Construct(rw::IResourceAllocator* lpAllocator)
    {
        // X360: the two guest .data microcode blobs, 456 / 228 bytes; PC: the converted images.
        const void* lapVertexProgramBinary[1] = { renderengine::gauSkidVertexProgramPC };
        const void* lapPixelProgramBinary[1]  = { renderengine::gauSkidPixelProgramPC };
        const u32   lauVertexProgramSize[1]   = { renderengine::guSkidVertexProgramPCSize };
        const u32   lauPixelProgramSize[1]    = { renderengine::guSkidPixelProgramPCSize };
        CgsGraphics::ImRenderer<SkidVertex>::Construct(
            lpAllocator,
            lapVertexProgramBinary, lauVertexProgramSize,
            lapPixelProgramBinary,  lauPixelProgramSize,
            1);

        renderengine::ProgramBufferData* const lpVertexProgram =
            reinterpret_cast<renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
        CGS_ASSERT(lpVertexProgram != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
        if (lpVertexProgram != 0)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gWorldViewProj"), &mWorldViewProjStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gStartColour"), &mStartColourStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gEndColour"), &mEndColourStateHandle);
        }
    }

    // =========================================================================================
    // The three constant setters (DWARF BrnIm3dSkidsRenderer.h:99/111/124), inlined on the X360:
    //   BeginShaderStates(this+0x58) + 4 x stvx128   (TrailRenderer::BeginRender @0x822959C8..)
    //   BeginShaderStates(this+0x5C) + stvx128       (TrailRenderer::Render @0x822959C8)
    //   BeginShaderStates(this+0x60) + stvx128       (TrailRenderer::Render @0x822959F4)
    // =========================================================================================
    void Im3dSkidsRenderer::SetTransform(Matrix44::InParam lTransform)
    {
        PushShaderConstant(&mWorldViewProjStateHandle, &lTransform, sizeof(Matrix44));
    }

    void Im3dSkidsRenderer::SetBlendStartColour(Vector4 lColour)
    {
        PushShaderConstant(&mStartColourStateHandle, &lColour, sizeof(Vector4));
    }

    void Im3dSkidsRenderer::SetBlendEndColour(Vector4 lColour)
    {
        PushShaderConstant(&mEndColourStateHandle, &lColour, sizeof(Vector4));
    }
}
