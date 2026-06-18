#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsFont.h"   // SafeResourceHandle<Font> (SetDebugFont)

// CgsDev::Debug3DImmediateRender - the world-space (3D) debug immediate renderer (the 3D counterpart
// of Debug2DImmediateRender). INCREMENTAL: only the debug-font handoff is modelled here, so
// DebugManager::SetDebugFont can set the font on BOTH the 2D and 3D renderers exactly as the X360
// does; the 3D drawing path (world-space DrawLine/DrawBox/DrawText) is the render follow-on. X360
// SetDebugFont (0x823B1448) stores the 2-word handle at this+0x2C/+0x30.

namespace CgsDev
{
    struct Debug3DImmediateRender
    {
        // The debug-font handoff (X360 0x823B1448): store the loaded bitmap Font's handle. Mirrors
        // Debug2DImmediateRender::SetDebugFont; DebugManager::SetDebugFont drives both.
        void SetDebugFont(const CgsResource::SafeResourceHandle<CgsResource::Font>& lrFont);
        bool HasResourceFont() const { return !mpFont.IsNull(); }

        CgsResource::SafeResourceHandle<CgsResource::Font> mpFont;
    };
}
