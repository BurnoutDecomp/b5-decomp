#include "SDKs/RenderEngineClub/MAIN/components/src/vertexbufferhelper.h"

namespace renderengine
{
// ARTIST 823F8BF0. The native array is terminated by VERTEXFORMAT_NA.
void VertexBufferHelper::Parameters::CalculateBufferSize()
{
    numElements = 0;
    while (elementFormats[numElements] != VERTEXFORMAT_NA) ++numElements;
    stride = VertexBufferHelper::CalculateStride(elementFormats, numElements);
    bufferSize = numVertices * stride;
}

}
