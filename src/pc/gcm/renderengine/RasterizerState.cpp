#include "renderstates.h"

namespace renderengine
{
RasterizerState* RasterizerState::Initialize(const Parameters* lpParameters)
{
    maState[0] = lpParameters->muFillMode;
    maState[1] = lpParameters->muCullMode;
    maState[2] = lpParameters->muDepthBias;
    maState[3] = lpParameters->muSlopeScaledDepthBias;
    maState[4] = lpParameters->muMultisampleEnable;
    maState[11] = lpParameters->muAntialiasedLineEnable;
    maState[5] = lpParameters->mu8ScissorEnable;
    maState[6] = lpParameters->mu8DepthClipEnable;
    maState[8] = lpParameters->mu8FrontCounterClockwise;
    maState[9] = lpParameters->mu8ConservativeRaster;
    maState[10] = lpParameters->mu8PaddingMode;
    maState[12] = 1;
    return this;
}
}
