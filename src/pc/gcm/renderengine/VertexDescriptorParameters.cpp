#include "renderstates.h"

namespace renderengine
{
VertexDescriptor::Parameters::Parameters()
{
    for (u32 luIndex = 0; luIndex < 16; ++luIndex)
    {
        Element& lElement = maElements[luIndex];
        lElement.mu16Stream = 0xffff;
        lElement.miOffset = -1;
        lElement.mu8Type = 255;
        lElement.mu8Usage = 255;
        lElement.mu8UsageIndex = 0;
        lElement.mu8Enabled = 1;
        maElementFlags[luIndex] = 0;
    }
}
}
