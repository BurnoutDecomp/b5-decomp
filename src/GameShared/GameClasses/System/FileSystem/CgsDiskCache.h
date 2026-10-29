#pragma once
#include "types.hpp"
namespace CgsFileSystem
{
    // The native ARTIST build retains these three debug options. The full PS3
    // cache engine is absent here; PatchManager follows atfilesystem+39240.
    // ARTIST82903864..74 writes1,0,1; DecFIGS names the option bytes.
    struct DiskCache
    {
        bool mbDEBUG_EnableCache;
        bool mbDEBUG_ErrorTest;
        bool mbDEBUG_EnableDumping;
        void Construct()
        {
            mbDEBUG_EnableCache = true;
            mbDEBUG_ErrorTest = false;
            mbDEBUG_EnableDumping = true;
        }
    };
}
