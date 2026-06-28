#include "GameSource/Gui/Flapt/BrnFlaptRenderer.h"

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (full def) + ImRenderer<V>::SetProgram

// BrnFlapt::FlaptRenderer member functions, reconstructed from BURNOUT_X360_ARTIST.XEX.
// This TU bodies the shader-state accessor:
//
//   SetShader @ 0x82470718
//
// (The sibling FlaptRenderer methods - Construct / RenderMesh / RenderMask /
// RenderTextField / SetSpecialTextureShaderProgram / StartRenderingFrame /
// StartDrawingMask / PopMask - land in their own TUs in this same file.)

namespace BrnFlapt
{

// ---- SetShader @ 0x82470718 ----------------------------------------------
// Cache-then-set the immediate-mode render buffer's vertex/pixel program. The X360
// compares the requested program id against the cached miShaderProgram (a signed
// word, initialised to -1 by Construct); only on a change does it push the
// SetProgram command and update the cache - so a redundant SetShader is a no-op.
//
// X360 call chain for the SetProgram receiver: r11 = this->mpImRenderSet (lwz 0(this)),
// r11 = mpImRenderSet->mpIm2dRenderBuffer (lwz 0(r11)), r3 = r11 + 4. The "+4" is the
// X360 polymorphic ImRenderBuffer<V>'s command-API sub-object offset; on the PC fold
// (Im2dRenderBuffer == Im2d, which derives ImRenderer<Basic2dColouredTexturedVertex>)
// SetProgram is reached as an ordinary base-class method by name, so the offset folds
// away. SetProgram takes the program id as a byte (the X360 sign-extends it with
// extsb before the call); the cache is the full word the caller passed.
void FlaptRenderer::SetShader(s32 liProgramId)
{
    if (liProgramId != miShaderProgram)
    {
        mpImRenderSet->mpIm2dRenderBuffer->SetProgram(static_cast<s8>(liProgramId));
        miShaderProgram = liProgramId;
    }
}

}
