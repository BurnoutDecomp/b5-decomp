// Tiny embed/translation-unit gate for renderengine::VertexProgramState. Forces the header to
// compile standalone and ODR-uses both reconstructed members so the gate fails loudly on any
// offset/name/signature regression. Compile-only (cl /c).
#include "pc/gcm/renderengine/VertexProgramState.h"

void renderengine_vps_embed_check()
{
    using namespace renderengine;

    VertexProgramState lState{};
    VertexProgram      lProgram{};
    lState.mpVertexProgram = &lProgram;

    (void)lState.GetD3DVertexShader();

    VertexProgramState*       lpState  = &lState;
    VertexProgramState* const lpHolder[1] = { lpState };
    const void* const         lpParams[2] = { &lProgram, nullptr };
    (void)VertexProgramState::InitializeNoBindX360(lpHolder, lpParams);
}
