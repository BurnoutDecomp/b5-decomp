#pragma once

#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

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
