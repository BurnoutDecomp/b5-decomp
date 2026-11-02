#pragma once

#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/depthstencilstate.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/rasterizerstate.h"

namespace CgsGraphics
{
// DecFIGS CgsMaterialState.h:40-45; ARTIST 828A8A80/828A8AB8 fix these slots.
// The serialized resource also contains its state allocations; they are not
// members of this three-pointer dispatch view.
struct MaterialState
{
    renderengine::BlendState* mpBlendState;
    renderengine::DepthStencilState* mpDepthStencilState;
    renderengine::RasterizerState* mpRasterizerState;
};

CGS_STATIC_ASSERT((sizeof(MaterialState) == 0x0C), "ARTIST material dispatch view");
CGS_STATIC_ASSERT((offsetof(MaterialState, mpRasterizerState) == 8), "ARTIST rasterizer slot");
}
