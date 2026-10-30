#pragma once

#include <xtl.h>

namespace renderengine
{
class BlendState;
class DepthStencilState;
class RasterizerState;

namespace StateHelper
{
// ARTIST82B64108 has no input arguments and returns status0/1.
int Initialize();
// ARTIST82B63D30 retains the viewport-copy destination in its return register.
int ResizeDefaultScissorRectAndViewportParameters();

// DecFIGS statehelper.h83..89; native storage is published by Initialize.
extern BlendState* m_blendStates[4];
extern DepthStencilState* m_defaultDepthStencilState;
extern RasterizerState* m_defaultRasterizerState;
// The X360 viewport is the genuine seven-word floating SDK form.
extern D3DVIEWPORTF9 m_defaultViewportParams;
extern D3DRECT m_defaultScissorRectParams;
}
}
