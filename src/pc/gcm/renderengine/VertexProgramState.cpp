#include "pc/gcm/renderengine/VertexProgramState.h"

// renderengine::VertexProgramState -- vertex-shader / vertex-program binding state.
//   GetD3DVertexShader    0x82B60B88
//   InitializeNoBindX360  0x82B629C0
// Both reconstructed store-for-store / load-for-load from the X360 ARTIST binary; members by name.

namespace renderengine
{
    // @0x82B60B88:
    //   lwz r11, 0x24(r3)   ; r11 = this->mpVertexProgram
    //   addi r3, r11, 0x14  ; r3  = mpVertexProgram + 0x14
    //   blr                 ; return &mpVertexProgram->mauD3DVertexShader  (the D3D vertex shader)
    D3DVertexShader* VertexProgramState::GetD3DVertexShader()
    {
        return reinterpret_cast<D3DVertexShader*>(&mpVertexProgram->mauD3DVertexShader);
    }

    // @0x82B629C0:
    //   lwz  r11, 0(r4)     ; r11 = lppParams[0]
    //   lwz  r3,  0(r3)     ; r3  = lppState[0]  (the VertexProgramState to fill; returned)
    //   stw  r11, 0x24(r3)  ; mpVertexProgram     = lppParams[0]
    //   lwz  r11, 4(r4)     ; r11 = lppParams[1]
    //   stw  r11, 0x20(r3)  ; mpVertexDeclaration = lppParams[1]
    //   blr                 ; return r3
    // Store order is +0x24 then +0x20, preserved below.
    VertexProgramState* VertexProgramState::InitializeNoBindX360(VertexProgramState* const* lppState,
                                                                 const void* const* lppParams)
    {
        VertexProgramState* lpState = *lppState;
        lpState->mpVertexProgram     = static_cast<VertexProgram*>(const_cast<void*>(lppParams[0]));
        lpState->mpVertexDeclaration = const_cast<void*>(lppParams[1]);
        return lpState;
    }
}
