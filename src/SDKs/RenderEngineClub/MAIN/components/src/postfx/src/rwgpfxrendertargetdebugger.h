#pragma once

#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

namespace renderengine { struct VertexDescriptorData; }

namespace rw
{
namespace graphics
{
namespace postfx
{
    class RenderTargetDebugger
    {
    public:
        RenderTargetDebugger();

    private:
        renderengine::VertexDescriptorData* mpVertexDescriptor;
        renderengine::ProgramBufferData* mpVertexProgram;
        renderengine::ProgramBufferData* mpPixelProgram;
    };
}
}
}
