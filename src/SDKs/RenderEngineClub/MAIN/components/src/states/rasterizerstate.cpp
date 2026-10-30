#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"
#include <cstring>

namespace renderengine
{
// ARTIST 82B63738: five entries, then native state size/alignment.
rw::ResourceDescriptor RasterizerState::GetResourceDescriptor(const Parameters&)
{
    rw::ResourceDescriptor descriptor;
    descriptor.m_baseResourceDescriptors[0].m_size = 0x34;
    descriptor.m_baseResourceDescriptors[0].m_alignment = 4;
    return descriptor;
}

RasterizerState* RasterizerState::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    RasterizerState* const lpState = static_cast<RasterizerState*>(resource.m_baseResources[0]);
    const Parameters* const lpParameters = &parameters;
    lpState->muFillMode            = lpParameters->muFillMode;               // lwz 0x00 / stw 0x00
    lpState->muCullMode            = lpParameters->muCullMode;               // lwz 0x04 / stw 0x04
    std::memcpy(&lpState->muDepthBias, &lpParameters->mfDepthBias, sizeof(u32));
    std::memcpy(&lpState->muSlopeScaleDepthBias, &lpParameters->mfSlopeScaleDepthBias, sizeof(u32));
    lpState->muMultiSampleMask     = lpParameters->muMultiSampleMask;      // lwz 0x10 / stw 0x10
    lpState->muPrimitiveResetIndex = lpParameters->muPrimitiveResetIndex;  // lwz 0x14 / stw 0x2C
    lpState->muScissorTestEnable   = lpParameters->mbScissorTestEnable;         // lbz 0x18 / stw 0x14
    lpState->muMultiSampleAntiAlias= lpParameters->mbMultiSampleAntiAlias;       // lbz 0x19 / stw 0x18
    lpState->muViewportEnable      = lpParameters->mbViewportEnable; // lbz 0x1A / stw 0x20
    lpState->muHalfPixelOffset     = lpParameters->mbHalfPixelOffset;    // lbz 0x1B / stw 0x24
    lpState->muPrimitiveResetEnable= lpParameters->mbPrimitiveResetEnable;           // lbz 0x1C / stw 0x28
    lpState->muInitialised         = 1u;                                     // li 1     / stw 0x30
    return lpState;
}
}
