#pragma once

#include "types.hpp"
#include <xtl.h>

// ARTIST off_83271608. The native SDK owns the D3DDevice declaration and ABI.
extern D3DDevice* gpD3DDevice;

namespace renderengine
{
    class PixelBuffer;
    // ARTIST Device-owned native surfaces 83271100/83271140 and flag8327161C.
    extern PixelBuffer gOriginalColorBuffer;
    extern PixelBuffer gOriginalDepthStencilBuffer;
    extern u32 guTilingEnabled;

    class Device
    {
    public:
        // ARTIST 82B61F08: remove every texture binding of this resource.
        static void UnSetResource(D3DResource* lpResource);
    };
}
