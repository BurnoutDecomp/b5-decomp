#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

namespace renderengine { class RenderTarget; }

class BrnPostFxBloom
{
public:
    void Construct(rw::IResourceAllocator* lpAllocator);
    void Render(renderengine::RenderTarget* lpBloomRenderTarget,
                renderengine::RenderTarget* lpIntermediateRenderTarget,
                renderengine::RenderTarget* lpSourceRenderTarget,
                f32 lfThreshold, f32 lfWhiteLevel);

private:
    renderengine::ProgramBufferData* CreateProgram(const void* lpMicrocode,
                                                   u32 luSize, bool lbPixelProgram);
    void PrepareDownSampleBuffer(renderengine::RenderTarget* lpDestRenderTarget,
                                 renderengine::RenderTarget* lpSourceRenderTarget);
    void Generate1PassBlurredBloomBuffer(renderengine::RenderTarget* lpDestRenderTarget,
                                         renderengine::RenderTarget* lpSourceRenderTarget);
    void Generate2PassBlurredBloomBuffer(renderengine::RenderTarget* lpDestRenderTarget,
                                         renderengine::RenderTarget* lpSourceRenderTarget);

    rw::IResourceAllocator* mpAllocator;
    rw::Resource mBloomVertexDescriptorResource;
    renderengine::VertexDescriptorData* mpBloomVertexDescriptor;
    bool mbUseNewBloom;
    bool mbUseHardwareGaussianInBloom;

    renderengine::ProgramBufferData* mpBloomDSVertexProgram;
    renderengine::ProgramBufferData* mpBloomDSPixelProgram;
    renderengine::ProgramVariableHandle mBloomDSVertexVariableHandleUvOffset_00_01;
    renderengine::ProgramVariableHandle mBloomDSVertexVariableHandleUvOffset_02_03;
    renderengine::ProgramVariableHandle mBloomDSPixelVariableHandleDot;
    renderengine::ProgramVariableHandle mBloomDSPixelVariableHandleThresholdScale;
    u8 muBloomDSPixelShaderRegisterCountOriginal;
    u8 muBloomDSPixelShaderRegisterCountModified;

    renderengine::ProgramBufferData* mpBloomBlurVertexProgram;
    renderengine::ProgramBufferData* mpBloomBlurPixelProgram;
    renderengine::ProgramVariableHandle mBloomBlurVertexVariableHandleUvOffset_00_01;
    renderengine::ProgramVariableHandle mBloomBlurVertexVariableHandleUvOffset_02_03;
    u8 muBloomBlurPixelShaderRegisterCountOriginal;
    u8 muBloomBlurPixelShaderRegisterCountModified;

    renderengine::ProgramBufferData* mpBloomBlurOldVertexProgram;
    renderengine::ProgramBufferData* mpBloomBlurOldPixelProgram;
    renderengine::ProgramVariableHandle mBloomBlurOldVertexVariableHandleUvOffset_00_01;
    renderengine::ProgramVariableHandle mBloomBlurOldVertexVariableHandleUvOffset_02_03;
    renderengine::ProgramVariableHandle mBloomBlurOldVertexVariableHandleUvOffset_04_05;
    renderengine::ProgramVariableHandle mBloomBlurOldPixelVariableHandleTapWeights_0_to_3;
    renderengine::ProgramVariableHandle mBloomBlurOldPixelVariableHandleTapWeights_4;
    u8 muBloomBlurOldPixelShaderRegisterCountOriginal;
    u8 muBloomBlurOldPixelShaderRegisterCountModified;

    f32 mfThreshold;
    f32 mfWhiteLevel;
};
