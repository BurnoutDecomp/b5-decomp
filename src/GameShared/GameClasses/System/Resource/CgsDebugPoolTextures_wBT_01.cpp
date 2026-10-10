#include "GameShared/GameClasses/System/Resource/CgsDebugPoolTextures.h"

// CgsResource debug texture-browser option names. Both tables are read from the image's data
// section (seven and five string pointers, in EDebugSortMode / EDebugTextureRenderMode order); the
// resource debug component copies them into its sort-mode and render-mode option lists.
namespace CgsResource
{
    const char* KAPC_DEBUGSORTMODENAMES[E_DEBUGSORTMODE_COUNT] =
    {
        "Nothing",
        "Area",
        "Width",
        "Height",
        "Data",
        "Model Usage",
        "Instance Usage",
    };

    const char* KAPC_DEBUGTEXTURERENDERMODE[E_DEBUGTEXTURERENDERMODE_COUNT] =
    {
        "Actual Size",
        "Stretched",
        "Thumbnails 8x8",
        "Thumbnails 16x16",
        "Thumbnails 32x32",
    };
}
