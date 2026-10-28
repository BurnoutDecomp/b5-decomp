#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"

namespace renderengine
{
// ARTIST 82B63738: five entries, then native state size/alignment.
ResourceDescriptor5* RasterizerState::GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters*)
{
    for (int li = 0; li < 5; ++li)
    {
        lpDescriptor->maEntries[li].muSize = 0;
        lpDescriptor->maEntries[li].muAlignment = 1;
    }
    lpDescriptor->maEntries[0].muSize = 0x34;
    lpDescriptor->maEntries[0].muAlignment = 4;
    return lpDescriptor;
}

RasterizerState* RasterizerState::Initialize(RasterizerState** ppState, const Parameters* lpParameters)
{
    RasterizerState* lpState = *ppState;
    lpState->muFillMode            = lpParameters->muFillMode;               // lwz 0x00 / stw 0x00
    lpState->muCullMode            = lpParameters->muCullMode;               // lwz 0x04 / stw 0x04
    lpState->muDepthBias           = lpParameters->muDepthBias;              // lwz 0x08 / stw 0x08
    lpState->muSlopeScaleDepthBias = lpParameters->muSlopeScaledDepthBias;   // lwz 0x0C / stw 0x0C
    lpState->muMultiSampleMask     = lpParameters->muMultisampleEnable;      // lwz 0x10 / stw 0x10
    lpState->muPrimitiveResetIndex = lpParameters->muAntialiasedLineEnable;  // lwz 0x14 / stw 0x2C
    lpState->muScissorTestEnable   = lpParameters->mu8ScissorEnable;         // lbz 0x18 / stw 0x14
    lpState->muMultiSampleAntiAlias= lpParameters->mu8DepthClipEnable;       // lbz 0x19 / stw 0x18
    lpState->muViewportEnable      = lpParameters->mu8FrontCounterClockwise; // lbz 0x1A / stw 0x20
    lpState->muHalfPixelOffset     = lpParameters->mu8ConservativeRaster;    // lbz 0x1B / stw 0x24
    lpState->muPrimitiveResetEnable= lpParameters->mu8PaddingMode;           // lbz 0x1C / stw 0x28
    lpState->muInitialised         = 1u;                                     // li 1     / stw 0x30
    return lpState;
}
}
