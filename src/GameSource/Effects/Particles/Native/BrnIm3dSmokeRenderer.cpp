// ============================================================================
// GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.cpp
//
// BrnGraphics::Im3dSmokeRenderer::Construct     @0x82295260
// BrnGraphics::Im3dSmokeRenderer::SetConstants  @0x822839C8
//
// ⭐ 2026-09-24 (FX-CRASHVFX): NEW TU. ParticleModule carried this renderer as a 12-byte
// ContainedInterface placeholder, so ParticleModule::Prepare announced its Construct and every
// simple particle (impact smoke, crash impact dust, skid smoke) had nothing to draw through.
//
// Construct is the sibling Im3dTexPlusLighting / Im3dSkidsRenderer shape with TWO program pairs:
// the four stack words at 0x8229526C..0x822952C4 are
//     vertex programs { unk_8200EC80, unk_8200EFB8 }  sizes { 0x170, 0x114 }
//     pixel  programs { unk_8200EDF0, unk_8200F0D0 }  sizes { 0x1C8, 0xE4 }    count 2 (`li r9, 2`)
// then the four handles, each lookup behind its own program-present assert (CgsImRenderer.h:570 /
// :554, "mapVertexProgramBuffer[ li8Program ] != NULL") and its own found-assert
// (BrnIm3dSmokeRenderer.cpp:100 / :101 / :104 / :105, the four strings at 0x820134A0 / 0x82013450 /
// 0x820133E0 / 0x82013368).
// ============================================================================

#include "GameSource/Effects/Particles/Native/BrnIm3dSmokeRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] CgsDev::Log::WriteToLog

#include <cstdio>    // [diag] snprintf (the constant-resolve line)

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr): open one shader-constant row for
// a named program constant and return the write cursor. Shared declaration with the sibling
// immediate-mode TUs (BrnIm3dTexPlusLighting.cpp spells it identically).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace renderengine
{
    // BrnIm3dSmokeRendererProgramsPC.cpp -- the PC stand-ins for the four guest blobs.
    extern const u8  gauSmokeZFadeVertexProgramPC[];
    extern const u32 guSmokeZFadeVertexProgramPCSize;
    extern const u8  gauSmokeZFadePixelProgramPC[];
    extern const u32 guSmokeZFadePixelProgramPCSize;
    extern const u8  gauSmokeSansZFadeVertexProgramPC[];
    extern const u32 guSmokeSansZFadeVertexProgramPCSize;
    extern const u8  gauSmokeSansZFadePixelProgramPC[];
    extern const u32 guSmokeSansZFadePixelProgramPCSize;
}

namespace BrnGraphics
{
    namespace
    {
        // SetConstants' literals (all rodata, read directly):
        const f32 KF_HALF                 = 0.5f;                     // vcfsx(splat 1, 1)
        const f32 KF_SCALE_X              = 0.5f;                     // flt_82001DA0
        const f32 KF_SCALE_Y              = -0.5f;                    // flt_82004C78
        const f32 KF_ZERO                 = 0.0f;                     // flt_82001CC0
        // The 24-bit depth rebuilt from three 8-bit channels: 255/256, 255/65536, 255/16777216.
        const f32 KF_DEPTH_BYTE_HIGH      = 0.99609380960464478f;     // flt_82011668 (0x3F7F0001)
        const f32 KF_DEPTH_BYTE_MID       = 0.0038909914437681437f;   // flt_82011664 (0x3B7F0001)
        const f32 KF_DEPTH_BYTE_LOW       = 1.5199185327219311e-05f;  // flt_82011660 (0x377F0001)

        // Write one float4 constant row through the cursor BeginShaderStates handed back.
        inline void WriteShaderRow(void* lpShaderState, f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
        {
            if (lpShaderState == 0)
                return;
            f32* const lpfRow = reinterpret_cast<f32*>(lpShaderState);
            lpfRow[0] = lfX;
            lpfRow[1] = lfY;
            lpfRow[2] = lfZ;
            lpfRow[3] = lfW;
        }
    }

    void Im3dSmokeRenderer::Construct(rw::IResourceAllocator* lpAllocator)
    {
        const void* lapVertexProgramBinary[2] = { renderengine::gauSmokeZFadeVertexProgramPC,
                                                  renderengine::gauSmokeSansZFadeVertexProgramPC };
        const u32   lauVertexProgramSize[2]   = { renderengine::guSmokeZFadeVertexProgramPCSize,
                                                  renderengine::guSmokeSansZFadeVertexProgramPCSize };
        const void* lapPixelProgramBinary[2]  = { renderengine::gauSmokeZFadePixelProgramPC,
                                                  renderengine::gauSmokeSansZFadePixelProgramPC };
        const u32   lauPixelProgramSize[2]    = { renderengine::guSmokeZFadePixelProgramPCSize,
                                                  renderengine::guSmokeSansZFadePixelProgramPCSize };
        CgsGraphics::ImRenderer<CgsGraphics::BasicColouredTexturedVertex>::Construct(
            lpAllocator,
            lapVertexProgramBinary, lauVertexProgramSize,
            lapPixelProgramBinary,  lauPixelProgramSize,
            2);

        const renderengine::ProgramBufferData* lpVertexProgram =
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[KI8_PROGRAM_ZFADE]);
        const renderengine::ProgramBufferData* lpPixelProgram =
            reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[KI8_PROGRAM_ZFADE]);

        CGS_ASSERT(lpVertexProgram != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
        if (lpVertexProgram != 0)
        {
            const u32 luFound = renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gOffset"), &mOffsetHandle);
            CGS_ASSERT(luFound != 0,
                       "GetVertexProgram(0)->GetVariableHandleByName( \"gOffset\", mOffsetHandle )");
        }
        CGS_ASSERT(lpVertexProgram != 0, "mapVertexProgramBuffer[ li8Program ] != NULL");
        if (lpVertexProgram != 0)
        {
            const u32 luFound = renderengine::ProgramBuffer::GetVariableHandleByName(
                lpVertexProgram, reinterpret_cast<const u8*>("gScale"), &mScaleHandle);
            CGS_ASSERT(luFound != 0,
                       "GetVertexProgram(0)->GetVariableHandleByName( \"gScale\", mScaleHandle )");
        }
        CGS_ASSERT(lpPixelProgram != 0, "mapPixelProgramBuffer[ li8Program ] != NULL");
        if (lpPixelProgram != 0)
        {
            const u32 luFound = renderengine::ProgramBuffer::GetVariableHandleByName(
                lpPixelProgram, reinterpret_cast<const u8*>("gDepthConversion"), &mDepthConversionHandle);
            CGS_ASSERT(luFound != 0,
                       "GetPixelProgram(0)->GetVariableHandleByName( \"gDepthConversion\", mDepthConversionHandle )");
        }
        CGS_ASSERT(lpPixelProgram != 0, "mapPixelProgramBuffer[ li8Program ] != NULL");
        if (lpPixelProgram != 0)
        {
            const u32 luFound = renderengine::ProgramBuffer::GetVariableHandleByName(
                lpPixelProgram, reinterpret_cast<const u8*>("gDepthFadeConstants"), &mDepthFadeConstantsHandle);
            CGS_ASSERT(luFound != 0,
                       "GetPixelProgram(0)->GetVariableHandleByName( \"gDepthFadeConstants\", mDepthFadeConstantsHandle )");
        }

        // [DIAG] NOT IN THE X360 BINARY. DID THE FOUR CONSTANTS RESOLVE, AND ARE BOTH PAIRS UP?
        // A zero register count is GetVariableHandleByName's "not found", and an unresolved handle
        // routes its row to the discard bin -- invisible until something draws. DELETE-WHEN-STABLE.
        {
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[simplefx] smoke renderer: programs vs0=%d ps0=%d vs1=%d ps1=%d constants "
                "offset{idx=%u cnt=%u} scale{idx=%u cnt=%u} depthconv{idx=%u cnt=%u} fade{idx=%u cnt=%u}\n",
                mapVertexProgramBuffer[0] != 0, mapPixelProgramBuffer[0] != 0,
                mapVertexProgramBuffer[1] != 0, mapPixelProgramBuffer[1] != 0,
                mOffsetHandle.mu8RegisterSet, mOffsetHandle.mu8RegisterCount,
                mScaleHandle.mu8RegisterSet, mScaleHandle.mu8RegisterCount,
                mDepthConversionHandle.mu8RegisterSet, mDepthConversionHandle.mu8RegisterCount,
                mDepthFadeConstantsHandle.mu8RegisterSet, mDepthFadeConstantsHandle.mu8RegisterCount);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // =============================================================================================
    // SetConstants @0x822839C8 -- the soft-particle constants. Store for store:
    //   gOffset             = (offset.x + 0.5, offset.y + 0.5, far, 0)
    //                         (the vperm128 pair through 0x82CDA3C0 (A.x,A.x,A.x,B.y) and 0x82CDA400
    //                         (A.z,B.w,A.x,A.x), joined by `vsldoi128 v0, v126, v127, 8`)
    //   gScale              = (0.5, -0.5, near - far, 0)
    //   gDepthConversion    = ((near-far) * 255/256, (near-far) * 255/65536, (near-far) * 255/16777216, far)
    //   gDepthFadeConstants = (near * far / fadeDistance, 0, 0, 0)
    // i.e. the vertex program maps NDC to the depth texture's UV (with the half-pixel offset) and the
    // particle's depth to far + z*(near - far); the pixel program rebuilds the scene depth in the same
    // space from the three depth bytes and fades by (particle - scene) * near*far/fade / (scene*particle).
    // =============================================================================================
    void Im3dSmokeRenderer::SetConstants(rw::math::vpu::Vector2 lvDepthUvOffset,
                                         f32 lfNearPlane,
                                         f32 lfFarPlane,
                                         f32 lfFadeDistance)
    {
        const f32 lfNearMinusFar = lfNearPlane - lfFarPlane;
        const f32 lfFadeConstant = (lfNearPlane * lfFarPlane) / lfFadeDistance;

        void* lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mOffsetHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lvDepthUvOffset.x + KF_HALF, lvDepthUvOffset.y + KF_HALF,
                       lfFarPlane, KF_ZERO);

        lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mScaleHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, KF_SCALE_X, KF_SCALE_Y, lfNearMinusFar, KF_ZERO);

        lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mDepthConversionHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lfNearMinusFar * KF_DEPTH_BYTE_HIGH, lfNearMinusFar * KF_DEPTH_BYTE_MID,
                       lfNearMinusFar * KF_DEPTH_BYTE_LOW, lfFarPlane);

        lpShaderState = 0;
        RenderEngineDeviceBeginShaderStates(&mDepthFadeConstantsHandle, &lpShaderState);
        WriteShaderRow(lpShaderState, lfFadeConstant, KF_ZERO, KF_ZERO, KF_ZERO);
    }
}
