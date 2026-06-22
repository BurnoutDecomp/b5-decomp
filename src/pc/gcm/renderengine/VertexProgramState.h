#pragma once

#include "types.hpp"

// renderengine::VertexProgramState -- the platform vertex-shader / vertex-program binding state.
// Sibling to renderengine::VertexBuffer / renderengine::TextureState in this directory. On X360 a
// VertexProgramState ties together the vertex program (compiled D3D vertex shader) and the vertex
// declaration the device should bind before a draw; shadow::Device::GetVertexProgramState seeds it
// and shadow::Device::FlushVertexProgramState reads the D3D vertex shader back out of it to call
// D3DDevice_SetVertexShader.
//
//   GetD3DVertexShader    0x82B60B88
//   InitializeNoBindX360  0x82B629C0
//
// LAYOUT (X360-authoritative). Only the two dwords the attested paths touch are named; the leading
// bytes (+0x00..+0x1F) are an opaque base region preserved verbatim so the object width and the
// named-field offsets match the binary:
//   +0x20 mpVertexDeclaration  (InitializeNoBindX360 stores a2[1] here)
//   +0x24 mpVertexProgram      (InitializeNoBindX360 stores a2[0] here; GetD3DVertexShader reads it)
// GetD3DVertexShader returns mpVertexProgram + 0x14, i.e. &VertexProgram::mD3DVertexShader -- the
// address of the D3D vertex-shader header embedded at +0x14 inside the vertex program.

namespace renderengine
{
    // The compiled vertex-program object. Neither the full VertexProgram nor the D3DVertexShader
    // header type is committed under b5-decomp/src; the only attested fact is that the D3D vertex
    // shader lives at +0x14 inside it (GetD3DVertexShader returns mpVertexProgram + 0x14, handed
    // straight to D3DDevice_SetVertexShader). Modeled as a minimal placeholder preserving that one
    // offset -- replace with the real renderengine VertexProgram layout if/when it lands.
    // [HONEST PLACEHOLDER: only the +0x14 D3D-shader offset is X360-attested.]
    struct D3DVertexShader;   // opaque D3D9 vertex-shader header (incomplete; only its address is used)

    struct VertexProgram
    {
        u8 mauOpaque00[0x14];   // +0x00  opaque (program header / bytecode handle)
        u8 mauD3DVertexShader;  // +0x14  start of the embedded D3D vertex-shader header (opaque bytes)
    };

    // The vertex-program binding state. The leading 0x20 bytes are an opaque base region the
    // attested funcs never read by name; only mpVertexDeclaration / mpVertexProgram are named.
    class VertexProgramState
    {
    public:
        // X360 @0x82B60B88: `return *(this+0x24) + 0x14;` -- read mpVertexProgram, return the address
        // of its embedded D3D vertex shader (&mpVertexProgram->mD3DVertexShader).
        D3DVertexShader* GetD3DVertexShader();

        // X360 @0x82B629C0: handed (lppState, lpParams). lppState[0] is the VertexProgramState to fill;
        // lpParams[0] -> mpVertexProgram (+0x24), lpParams[1] -> mpVertexDeclaration (+0x20). Stores in
        // asm order (+0x24 then +0x20). Returns the filled VertexProgramState*.
        static VertexProgramState* InitializeNoBindX360(VertexProgramState* const* lppState,
                                                        const void* const* lppParams);

        u8             mauOpaque00[0x20];   // +0x00  opaque base region (preserved verbatim)
        void*          mpVertexDeclaration; // +0x20  vertex declaration (a2[1])
        VertexProgram* mpVertexProgram;     // +0x24  bound vertex program (a2[0])
    };
}
