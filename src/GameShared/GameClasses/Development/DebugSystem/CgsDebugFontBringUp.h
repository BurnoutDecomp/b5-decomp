#pragma once

namespace CgsDev
{
    class DebugManager;

    // Bring up the resource (bitmap) debug font and hand it to the debug renderers. Mirrors the X360
    // GamePrepare handoff, using the reconstructed resource load-path:
    //   RegisterAllResourceTypes -> a Pool (InitPool) -> BundleLoader::LoadBundle(path, pool, resolver)
    //   -> find the loaded Font -> Font::CreateTextureState -> DebugManager::SetDebugFont.
    // A non-null Font handle flips Debug2D/3DImmediateRender::DrawText off the vector-font fallback
    // onto the bitmap TextRenderer path.
    //
    // Returns true if a font was loaded + set; false (bundle missing / unreadable / no font in it)
    // leaves the built-in vector font in place. Call ONCE after the graphics device + DebugManager are
    // up (the font's atlas rasters create D3D textures during fix-up). Needs the .font bundle DATA on
    // disk (the data pipeline is user-owned); without it this is a no-op that keeps the vector font.
    bool LoadAndSetDebugFont(const char* lpcBundlePath, DebugManager& lrManager);
}
