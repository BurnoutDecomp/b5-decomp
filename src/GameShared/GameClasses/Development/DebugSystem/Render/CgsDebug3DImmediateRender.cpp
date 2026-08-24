#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"

// CgsDev::Debug3DImmediateRender -- the font-handoff slice (the world-space drawing is a follow-on).

namespace CgsDev
{
    // X360 Construct @0x8281A488 (the modelled slice). The full body clears the render-buffer slot,
    // nulls the font handle, constructs the renderer's vector font (size {10,10}, glyph-cell scale
    // 0.125), creates the debug render states, latches the virtual screen size, constructs the text
    // renderer, and builds the sphere index table. Only the members this incremental type carries
    // are initialised here; the drawing-path members (vector font / render states / text renderer /
    // sphere indices) land with the Debug3D render follow-on, and the allocator is threaded through
    // for them exactly as the X360 call does.
    void Debug3DImmediateRender::Construct(rw::IResourceAllocator* /*lpAllocator*/,
                                           f32 lfVirtualScreenWidth, f32 lfVirtualScreenHeight)
    {
        mpFont.Clear();                                  // @0x8281A488: font handle <- NULL handle pair
        mfVirtualScreenWidth  = lfVirtualScreenWidth;    // _R31[8]
        mfVirtualScreenHeight = lfVirtualScreenHeight;   // _R31[9]
    }

    // Faithful port of X360 0x823B1448: store the loaded font handle (the 3D renderer's copy; the 2D
    // renderer holds the equivalent). (X360 asserts lrFont != CgsResource::NULLResourceHandle.)
    void Debug3DImmediateRender::SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont)
    {
        mpFont = lrFont;
    }
}
