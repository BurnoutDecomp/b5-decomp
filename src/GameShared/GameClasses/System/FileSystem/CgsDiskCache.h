#pragma once

namespace CgsFileSystem
{
    // X360 retains these three debug controls. The PS3 cache engine/storage is
    // not imported: ARTIST's next PatchManager starts immediately after them.
    struct DiskCache
    {
    public:
        // Inlined ARTIST82903864..74; names/shape from DecFIGS CgsDiskCache.h.
        void Construct()
        {
            mbDEBUG_EnableCache = true;
            mbDEBUG_ErrorTest = false;
            mbDEBUG_EnableDumping = true;
        }

    private:
        bool mbDEBUG_EnableCache;
        bool mbDEBUG_ErrorTest;
        bool mbDEBUG_EnableDumping;
    };
}
