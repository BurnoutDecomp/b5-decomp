#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"

// CgsDev::Debug3DImmediateRender -- the font-handoff slice (the world-space drawing is a follow-on).

namespace CgsDev
{
    // Faithful port of X360 0x823B1448: store the loaded font handle (the 3D renderer's copy; the 2D
    // renderer holds the equivalent). (X360 asserts lrFont != CgsResource::NULLResourceHandle.)
    void Debug3DImmediateRender::SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        mpFont = lrFont;
    }
}
