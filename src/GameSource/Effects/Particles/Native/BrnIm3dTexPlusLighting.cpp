// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.cpp
//
// BrnGraphics::Im3dTexPlusLighting::Construct -- the only out-of-line body the
// class has. It is the exact shape of the sibling Im3dSkidsRenderer::Construct:
// build the ImRenderer<BrnGraphics::WorldTexturedVertex> base over ONE
// {vertex, pixel} program pair, then resolve the named shader constants --
// gaWorldTransforms / gViewProjection / gEyeLocation against the VERTEX program
// and gLightDirection / gLightColour / gShinyParams against the PIXEL one. The
// original re-asserts "mapVertexProgramBuffer[ li8Program ] != NULL" /
// "mapPixelProgramBuffer[ li8Program ] != NULL" (CgsImRenderer.h:570 / :554)
// before every one of the six lookups; hoisted to one check per program.
//
// THE PROGRAM PAIR. The original hands ImRenderer<WorldTexturedVertex>::Construct
// two executable-embedded Xenos programs (728 bytes vertex, 720 bytes pixel).
// They were disassembled and re-authored as D3D9 vs_3_0 / ps_3_0, compiled and
// wrapped exactly like the skid pair into
// pc/gcm/renderengine/WorldTexturedProgramsPC.cpp -- the PC platform leaf this
// TU binds.
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] CgsDev::Log::WriteToLog

#include <cstdio>    // [diag] snprintf (the constant-resolve line)

// The two converted PC program images (pc/gcm/renderengine/WorldTexturedProgramsPC.cpp --
// the PC stand-in for the two guest .data blobs; see that file's recipe).
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr): open one shader-constant
// row for a named program constant and return the write cursor. Shared declaration with the
// sibling immediate-mode TUs (CgsIm3d.cpp / CgsIm3dSkyDome.cpp spell it identically).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace renderengine
{
    extern const u8  gauWorldTexturedVertexProgramPC[];
    extern const u32 guWorldTexturedVertexProgramPCSize;
    extern const u8  gauWorldTexturedPixelProgramPC[];
    extern const u32 guWorldTexturedPixelProgramPCSize;
}

namespace BrnGraphics
{
    void Im3dTexPlusLighting::Construct(rw::IResourceAllocator* lpAllocator)
    {
        const void* lapVertexProgramBinary[1] = { renderengine::gauWorldTexturedVertexProgramPC };
        const void* lapPixelProgramBinary[1]  = { renderengine::gauWorldTexturedPixelProgramPC };
        const u32   lauVertexProgramSize[1]   = { renderengine::guWorldTexturedVertexProgramPCSize };
        const u32   lauPixelProgramSize[1]    = { renderengine::guWorldTexturedPixelProgramPCSize };
        CgsGraphics::ImRenderer<WorldTexturedVertex>::Construct(
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
                lpVertexProgram, reinterpret_cast<const u8*>("gaWorldTransforms"),
                &mTransformArrayStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gViewProjection"),
                &mViewProjectionMatrixStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gEyeLocation"),
                &mEyeStateHandle);
        }

        renderengine::ProgramBufferData* const lpPixelProgram =
            reinterpret_cast<renderengine::ProgramBufferData*>(mapPixelProgramBuffer[0]);
        CGS_ASSERT(lpPixelProgram != 0, "mapPixelProgramBuffer[ li8Program ] != NULL");
        if (lpPixelProgram != 0)
        {
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpPixelProgram, reinterpret_cast<const u8*>("gLightDirection"),
                &mWorldLightDirectionStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpPixelProgram, reinterpret_cast<const u8*>("gLightColour"),
                &mLightColourStateHandle);
            renderengine::ProgramBuffer::GetVariableHandleByName(
                lpPixelProgram, reinterpret_cast<const u8*>("gShinyParams"),
                &mShinyParamsStateHandle);
        }

        // [DIAG] DID THE SIX CONSTANTS RESOLVE? mu8RegisterCount == 0 is
        // GetVariableHandleByName's "not found" answer, and a zero-count handle sends its
        // shader-state row to a DISCARD bin -- so an unresolved gaWorldTransforms would let
        // every debris instance transform by whatever c0..c127 happen to hold, which is
        // invisible both in the log and (until something draws) in the picture.
        // DELETE-WHEN-STABLE.
        {
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[debrispass] worldtex constants: transforms{idx=%u cnt=%u} vp{idx=%u cnt=%u} "
                "eye{idx=%u cnt=%u} lightdir{idx=%u cnt=%u} lightcol{idx=%u cnt=%u} "
                "shiny{idx=%u cnt=%u}\n",
                mTransformArrayStateHandle.mu8RegisterSet,      mTransformArrayStateHandle.mu8RegisterCount,
                mViewProjectionMatrixStateHandle.mu8RegisterSet, mViewProjectionMatrixStateHandle.mu8RegisterCount,
                mEyeStateHandle.mu8RegisterSet,                 mEyeStateHandle.mu8RegisterCount,
                mWorldLightDirectionStateHandle.mu8RegisterSet, mWorldLightDirectionStateHandle.mu8RegisterCount,
                mLightColourStateHandle.mu8RegisterSet,         mLightColourStateHandle.mu8RegisterCount,
                mShinyParamsStateHandle.mu8RegisterSet,         mShinyParamsStateHandle.mu8RegisterCount);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // =====================================================================================
    // THE SIX CONSTANT SETTERS.
    //
    // Inline on the console, so each one is recovered from the stores its call sites make.
    // Every one has the same three-step shape the console emits at each site:
    //   BeginShaderStates(&<the named constant's handle>, &cursor); build the value; store it.
    // The shader-state row is opened by HANDLE, so an unresolved constant (register count 0)
    // routes its write to the discard row instead of corrupting the constant file.
    //
    // WHERE EACH ONE IS ATTESTED (the store, not the name):
    //   SetViewProjection / SetEye / SetLightDirection / SetLightColour -- the four
    //     BeginShaderStates calls in BrnDebrisRenderer::BeginRender, in that order.
    //   SetShinyParams   -- the first BeginShaderStates call in ::RenderDebrisArray.
    //   SetTransformArray-- the last one, whose 2048-byte block copy is the 32-matrix batch.
    //
    // THE W LANE IS PART OF THE STORE, not padding: the console folds a literal 1.0f into
    // lane 3 of the eye / light-direction / light-colour rows (a `vrlimi` of the float-1.0
    // built by `vcsxwfp` of a splatted integer 1). The pixel program's dot products read
    // xyz only, so the 1.0 is the homogeneous lane -- reproduced because it is what the
    // store writes.
    // =====================================================================================
    namespace
    {
        // Write one float4 constant row through the cursor BeginShaderStates handed back.
        inline void WriteShaderRow(void* lpShaderState, f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            if (lpShaderState == 0)
            {
                return;
            }
            f32* const lpfRow = reinterpret_cast<f32*>(lpShaderState);
            lpfRow[0] = lfX;
            lpfRow[1] = lfY;
            lpfRow[2] = lfZ;
            lpfRow[3] = lfW;
        }
    }

    void Im3dTexPlusLighting::SetTransformArray(const Matrix44* lpTransforms)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mTransformArrayStateHandle, &lpShaderState);
        if (lpShaderState == 0 || lpTransforms == 0)
        {
            return;
        }

        // The console copies the whole 32-matrix batch in one block move (0x800 bytes).
        const u8* const lpSrc = reinterpret_cast<const u8*>(lpTransforms);
        u8* const       lpDst = reinterpret_cast<u8*>(lpShaderState);
        for (u32 luByte = 0; luByte < KU_NUM_TRANSFORMS * 64u; ++luByte)
        {
            lpDst[luByte] = lpSrc[luByte];
        }
    }

    void Im3dTexPlusLighting::SetViewProjection(Matrix44::InParam lViewProjection)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mViewProjectionMatrixStateHandle, &lpShaderState);
        if (lpShaderState == 0)
        {
            return;
        }

        // Four 16-byte rows, copied straight through (the console's four lvx/stvx pairs).
        const u8* const lpSrc = reinterpret_cast<const u8*>(&lViewProjection);
        u8* const       lpDst = reinterpret_cast<u8*>(lpShaderState);
        for (u32 luByte = 0; luByte < 64u; ++luByte)
        {
            lpDst[luByte] = lpSrc[luByte];
        }
    }

    void Im3dTexPlusLighting::SetEye(Vector3 lEye)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mEyeStateHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lEye.x, lEye.y, lEye.z, 1.0f);
    }

    void Im3dTexPlusLighting::SetLightDirection(Vector3 lDirection)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mWorldLightDirectionStateHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lDirection.x, lDirection.y, lDirection.z, 1.0f);
    }

    void Im3dTexPlusLighting::SetLightColour(Vector3 lColour)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mLightColourStateHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lColour.x, lColour.y, lColour.z, 1.0f);
    }

    void Im3dTexPlusLighting::SetShinyParams(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mShinyParamsStateHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lfX, lfY, lfZ, lfW);
    }
}
