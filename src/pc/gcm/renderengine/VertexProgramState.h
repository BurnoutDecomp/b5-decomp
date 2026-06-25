#pragma once

#include "types.hpp"

namespace renderengine
{
    struct D3DVertexShader;

    struct VertexProgram
    {
        u8 mauOpaque00[0x14];
        u8 mauD3DVertexShader;
    };

    class VertexProgramState
    {
    public:
        D3DVertexShader* GetD3DVertexShader();

        static VertexProgramState* InitializeNoBindX360(
            VertexProgramState* const* lppState,
            const void* const* lppParams);

        u8 mauOpaque00[0x20];
        void* mpVertexDeclaration;
        VertexProgram* mpVertexProgram;
    };
}
