#include "GameSource/Effects/Particles/Native/BrnParticlePrograms.h"
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

#include "GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT


#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"

namespace BrnGraphics
{
    void Im3dTexPlusLighting::Construct(rw::IResourceAllocator* lpAllocator)
    {
        const void* const lapVertexProgramBinary[1] = { BrnParticlePrograms::gaWorldTexturedVertex };
        const void* const lapPixelProgramBinary[1] = { BrnParticlePrograms::gaWorldTexturedPixel };
        const u32   lauVertexProgramSize[1]   = { sizeof(BrnParticlePrograms::gaWorldTexturedVertex) };
        const u32   lauPixelProgramSize[1]    = { sizeof(BrnParticlePrograms::gaWorldTexturedPixel) };
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

    }

    // =====================================================================================
    // THE SIX CONSTANT SETTERS.
    //
    // Inline on the console, so each one is recovered from the stores its call sites make.
    // Every one has the same three-step shape the console emits at each site:
    //   BeginShaderStates(&<the named constant's handle>, &cursor); build the value; store it.
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
    void Im3dTexPlusLighting::SetTransformArray(const Matrix44* lpTransforms)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mTransformArrayStateHandle, lIterator);
        if (lpTransforms == 0)
        {
            return;
        }

        // The console copies the whole 32-matrix batch in one block move (0x800 bytes).
        const u8* const lpSrc = reinterpret_cast<const u8*>(lpTransforms);
        u8* const       lpDst = reinterpret_cast<u8*>(lIterator.m_dest);
        for (u32 luByte = 0; luByte < KU_NUM_TRANSFORMS * 64u; ++luByte)
        {
            lpDst[luByte] = lpSrc[luByte];
        }
    }

    void Im3dTexPlusLighting::SetViewProjection(Matrix44::InParam lViewProjection)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mViewProjectionMatrixStateHandle, lIterator);

        // Four 16-byte rows, copied straight through (the console's four lvx/stvx pairs).
        const u8* const lpSrc = reinterpret_cast<const u8*>(&lViewProjection);
        u8* const       lpDst = reinterpret_cast<u8*>(lIterator.m_dest);
        for (u32 luByte = 0; luByte < 64u; ++luByte)
        {
            lpDst[luByte] = lpSrc[luByte];
        }
    }

    void Im3dTexPlusLighting::SetEye(Vector3 lEye)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mEyeStateHandle, lIterator);
        *lIterator.m_dest = rw::math::vpu::Vector4(lEye.GetX().GetFloat(), lEye.GetY().GetFloat(), lEye.GetZ().GetFloat(), 1.0f);
    }

    void Im3dTexPlusLighting::SetLightDirection(Vector3 lDirection)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mWorldLightDirectionStateHandle, lIterator);
        *lIterator.m_dest = rw::math::vpu::Vector4(lDirection.GetX().GetFloat(), lDirection.GetY().GetFloat(), lDirection.GetZ().GetFloat(), 1.0f);
    }

    void Im3dTexPlusLighting::SetLightColour(Vector3 lColour)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mLightColourStateHandle, lIterator);
        *lIterator.m_dest = rw::math::vpu::Vector4(lColour.GetX().GetFloat(), lColour.GetY().GetFloat(), lColour.GetZ().GetFloat(), 1.0f);
    }

    void Im3dTexPlusLighting::SetShinyParams(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        renderengine::FloatShaderStateIterator lIterator;
        renderengine::Device::BeginShaderStates(mShinyParamsStateHandle, lIterator);
        *lIterator.m_dest = rw::math::vpu::Vector4(lfX, lfY, lfZ, lfW);
    }
}
