#include "pc/gcm/renderengine/reflections/Resolution.h"
#include "pc/gcm/renderengine/device.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <d3d9.h>
#include <algorithm>
#include <cstdio>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: cap the optional face size to the native device's
    // cube, colour and depth extents before the engine describes all its targets.
    u32 CreationResolution()
    {
        D3DCAPS9 lCaps = {};
        if (!renderengine::gDevice || FAILED(renderengine::gDevice->GetDeviceCaps(&lCaps))) return 128;
        const u32 luMaximum = (std::min)(lCaps.MaxTextureWidth, lCaps.MaxTextureHeight);
        const u32 luPixels = LimitResolution(RequestedResolution(), luMaximum);
        char lacMessage[160];
        std::snprintf(lacMessage, sizeof(lacMessage), "[envmap] resolution requested=%d actual=%u per face (restart to change)\n",
            RequestedResolution(), luPixels);
        CgsDev::Log::WriteToLog(lacMessage);
        return luPixels;
    }
}
