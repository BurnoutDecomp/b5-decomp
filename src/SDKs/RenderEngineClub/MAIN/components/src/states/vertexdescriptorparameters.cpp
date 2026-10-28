#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"

namespace renderengine
{
// ARTIST82276870: the class at+0x0C is stw1, not a little-endian byte flag.
VertexDescriptor::Parameters::Parameters()
{
    for (u32 luIndex = 0; luIndex < 16; ++luIndex)
    {
        Element& lrElement = maElements[luIndex];
        lrElement.mu16Stream = 0;
        lrElement.mu16Pad0 = 0xFFFF;
        lrElement.miOffset = -1;
        lrElement.mu8Type = 0;
        lrElement.mu8Pad1 = 0xFF;
        lrElement.mu8Usage = 0xFF;
        lrElement.mu8UsageIndex = 0;
        lrElement.muElementClass = 1;
        maElementFlags[luIndex] = 0;
    }
}
}
