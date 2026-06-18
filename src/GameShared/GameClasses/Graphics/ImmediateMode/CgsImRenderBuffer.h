#pragma once

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"   // CgsGraphics::Im2d (the PC immediate renderer)

// CgsGraphics::Im2dRenderBuffer / Im3dRenderBuffer - the render buffers the TextRenderer batches into.
//
// On the X360/PS3 these are distinct double-buffered vertex buffers (ImRenderBuffer<V>) the renderers
// feed; the text path reserves a run with RenderStart, fills it, binds the atlas with SetState, and
// submits it with RenderEnd. On the PC target the immediate-mode renderer (CgsGraphics::Im2d) IS the
// buffer - it draws each batch straight through D3D9 (see CgsIm2d.cpp) - so the buffer/renderer split
// folds onto the one Im2d type here (the same object the loading screen + debug overlay render through).
// The RenderStart/SetState/RenderEnd buffer API lives on the Im2d render hierarchy (CgsImRenderer.h).
namespace CgsGraphics
{
    typedef Im2d Im2dRenderBuffer;

    // The 3D text buffer is used only by the 3D text path; opaque here (a follow-on -- the debug-2D
    // path never touches it).
    class Im3dRenderBuffer;
}
