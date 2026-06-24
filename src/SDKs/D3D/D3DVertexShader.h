#pragma once

// ===========================================================================
// D3D::CVertexShader -- the D3D vertex-shader wrapper in
// BURNOUT_X360_ARTIST.XEX. This header is the canonical OWNING home for:
//
//     D3D::CVertexShader::GetDebuggerHint  @ 0x829436A8
//
// There is no reference source and no DWARF for this TU, so the SHAPE below
// is reconstructed from the X360 asm of the single method. `D3D` is the X360
// graphics-SDK boundary, so its identifiers (CVertexShader, GetDebuggerHint)
// are preserved verbatim per the naming convention.
//
// GetDebuggerHint @ 0x829436A8 (asm):
//   r11 = this + 0x368            ; addi r11, r31, 0x368   (anchor at +872)
//   r10 = lwz 0xC(r11)            ; load the offset at +884 (anchor + 12)
//   r31 = r10 + r11               ; (this+872) + offset
//   if (r10 == 0) r31 = 0
//   return r31
//   (The `mr r13,r13` / `mr r14,r14` self-moves are X360 instrumentation no-op
//    markers, not behaviour.)
//
// So +884 holds a BYTE offset, relative to the +872 anchor, to an in-object
// PIX/shader-debugger hint blob. A zero offset means "no hint" -> return null;
// otherwise return a pointer to (anchor + offset). The accessor is read by
// D3D::PIXMetaLoadVertexShaderImp.
//
// LAYOUT: only the +872 anchor and the +884 offset word are touched by this TU;
// everything below is opaque shader-object state, pinned by explicit padding so
// the two fields land at their attested offsets. The full CVertexShader layout
// lives with the other D3D shader TUs.
// ===========================================================================

#include "types.hpp"

namespace D3D
{

class CVertexShader
{
public:
    // @ 0x829436A8 -- return a pointer to the in-object shader-debugger hint
    // blob, or null when no hint is present (offset word == 0).
    const void* GetDebuggerHint();

private:
    u8  mPad0[872];          // +0    opaque shader-object state up to the anchor
    u8  mDebuggerHintAnchor; // +872  anchor the hint offset is measured from
    u8  mPad1[11];           // +873  rest of the anchor word region (to +884)
    u32 muDebuggerHintOffset;// +884  byte offset (from +872) to the hint blob;
                             //       0 == no hint
};

} // namespace D3D
