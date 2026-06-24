#include "SDKs/D3D/D3DVertexShader.h"

// ===========================================================================
// D3D::CVertexShader::GetDebuggerHint @ 0x829436A8 -- reconstructed from
// BURNOUT_X360_ARTIST.XEX. See D3DVertexShader.h for the layout and asm notes.
// ===========================================================================

namespace D3D
{

const void* CVertexShader::GetDebuggerHint()
{
    // r11 = this + 0x368 (anchor); r10 = *(anchor + 0xC) (the offset word).
    const u8* pAnchor = &mDebuggerHintAnchor;
    u32 uOffset = muDebuggerHintOffset;

    if (uOffset == 0) // cmplwi r10,0 ; beq -> r31 = 0
        return nullptr;

    return pAnchor + uOffset; // add r31, r10, r11 (byte offset from the anchor)
}

} // namespace D3D
