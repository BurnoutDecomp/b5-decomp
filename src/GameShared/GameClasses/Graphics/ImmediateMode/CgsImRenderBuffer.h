#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (the PC immediate renderer)
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"

// CgsGraphics::Im2dRenderBuffer / Im3dRenderBuffer - the render buffers the TextRenderer batches into.
//
// The original producer writes commands and vertices into one bank while the
// renderer consumes the other. Keep that separation on PC: Im2d is the native
// immediate renderer, while Im2dRenderBuffer records the original command stream.
namespace CgsGraphics
{
    // The 3D text buffer is used only by the 3D text path; opaque here (a follow-on -- the debug-2D
    // path never touches it).
    class Im3dRenderBuffer;
}
