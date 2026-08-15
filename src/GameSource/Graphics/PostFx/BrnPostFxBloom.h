#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

// The post-fx render target the bloom chain draws through. Pointer-only here (the documented
// cascade-avoidance forward declaration); defined in
// SDKs/RenderEngineClub/MAIN/components/include/postfx/rwgpfxrendertarget.h.
namespace rw { namespace graphics { namespace postfx { class RenderTarget; } } }

class BrnPostFxBloom
{
public:
    void Construct(rw::IResourceAllocator* lpAllocator);

    // X360: INLINED into BrnPostFx::Destruct @0x82408214-0x82408260, whose assert names this file
    // and line 265. Assert the allocator, release the bloom vertex descriptor, free its resource.
    void Destruct();

    void Render(rw::graphics::postfx::RenderTarget* lpBloomRenderTarget,
                rw::graphics::postfx::RenderTarget* lpIntermediateRenderTarget,
                rw::graphics::postfx::RenderTarget* lpSourceRenderTarget,
                f32 lfThreshold, f32 lfWhiteLevel);

private:
    renderengine::ProgramBufferData* CreateProgram(const void* lpMicrocode,
                                                   u32 luSize, bool lbPixelProgram);
    // Parameter names are the DecFIGS DWARF's own (BrnPostFxBloom.cpp:285 / :310 / :446), and they
    // are not cosmetic. In the two blur passes NEITHER argument is purely a destination: the two-pass
    // blur PING-PONGS -- it samples lpBloomRt into lpWorkRt, resolves, then samples lpWorkRt back into
    // lpBloomRt -- so the old "lpDestRenderTarget / lpSourceRenderTarget" spelling named the second
    // pass exactly backwards. The console's own assert strings inside
    // Generate2PassBlurredBloomBuffer @0x82402308 spell both names out.
    //   X360 0x82401AE8 / 0x82401F50 / 0x82402308; DWARF BrnPostFxBloom.h:116 / :122 / :128.
    void PrepareDownSampleBuffer(rw::graphics::postfx::RenderTarget* lpDestRt,
                                 rw::graphics::postfx::RenderTarget* lpSourceRt);
    void Generate1PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                         rw::graphics::postfx::RenderTarget* lpWorkRt);
    void Generate2PassBlurredBloomBuffer(rw::graphics::postfx::RenderTarget* lpBloomRt,
                                         rw::graphics::postfx::RenderTarget* lpWorkRt);

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
