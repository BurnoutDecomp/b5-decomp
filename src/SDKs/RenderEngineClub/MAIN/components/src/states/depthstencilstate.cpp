#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"

namespace renderengine
{
// ARTIST 82B636F8: five entries, then native state size/alignment.
ResourceDescriptor5* DepthStencilState::GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters*)
{
    for (int li = 0; li < 5; ++li)
    {
        lpDescriptor->maEntries[li].muSize = 0;
        lpDescriptor->maEntries[li].muAlignment = 1;
    }
    lpDescriptor->maEntries[0].muSize = 0x60;
    lpDescriptor->maEntries[0].muAlignment = 4;
    return lpDescriptor;
}

DepthStencilState* DepthStencilState::Initialize(DepthStencilState** ppState, const Parameters* lpParameters)
{
    DepthStencilState* lpState = *ppState;

    // The 17 marshalled state words (Parameters +0x00..+0x40 -> object +0x00..+0x40).
    lpState->muZFunc                = lpParameters->muFunction;          // +0x00 -> ZFunc
    lpState->muStencilFail          = lpParameters->maState1[0];         // +0x04 -> StencilFail
    lpState->muStencilZFail         = lpParameters->maState1[1];         // +0x08 -> StencilZFail
    lpState->muStencilPass          = lpParameters->maState1[2];         // +0x0C -> StencilPass
    lpState->muStencilFunc          = lpParameters->muState4;            // +0x10 -> StencilFunc
    lpState->muCcwStencilFail       = lpParameters->maState5[0];         // +0x14 -> CCWStencilFail
    lpState->muCcwStencilZFail      = lpParameters->maState5[1];         // +0x18 -> CCWStencilZFail
    lpState->muCcwStencilPass       = lpParameters->maState5[2];         // +0x1C -> CCWStencilPass
    lpState->muCcwStencilFunc       = lpParameters->muState8;            // +0x20 -> CCWStencilFunc
    lpState->muHiStencilFunc        = lpParameters->muState9;            // +0x24 -> HiStencilFunc
    lpState->muStencilRef           = lpParameters->muState10;           // +0x28 -> StencilRef
    lpState->muStencilMask          = lpParameters->muStencilReadMask;   // +0x2C -> StencilMask
    lpState->muStencilWriteMask     = lpParameters->muStencilWriteMask;  // +0x30 -> StencilWriteMask
    lpState->muCcwStencilRef        = lpParameters->muState13;           // +0x34 -> CCWStencilRef
    lpState->muCcwStencilMask       = lpParameters->muState14;           // +0x38 -> CCWStencilMask
    lpState->muCcwStencilWriteMask  = lpParameters->muState15;           // +0x3C -> CCWStencilWriteMask
    lpState->muHiStencilRef         = lpParameters->muState16;           // +0x40 -> HiStencilRef

    // The six Parameters flag BYTES (+0x44..+0x49), each lbz->stw widened into its own word.
    lpState->muZEnable              = lpParameters->mbDepthTestEnable;   // +0x44 -> ZEnable
    lpState->muZWriteEnable         = lpParameters->mbDepthWriteEnable;  // +0x48 -> ZWriteEnable
    lpState->muStencilEnable        = lpParameters->mu8Flag2;            // +0x4C -> StencilEnable
    lpState->muTwoSidedStencilMode  = lpParameters->mu8Flag3;            // +0x50 -> TwoSidedStencilMode
    lpState->muHiStencilEnable      = lpParameters->mu8Flag4;            // +0x54 -> HiStencilEnable
    lpState->muHiStencilWriteEnable = lpParameters->mu8Flag5;            // +0x58 -> HiStencilWriteEnable

    lpState->muInitialised = 1u;                                        // +0x5C  li 1 / stw
    return lpState;
}
}
