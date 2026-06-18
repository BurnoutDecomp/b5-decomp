#ifndef CGS_RESOURCE_TYPE_REGISTRATION_H
#define CGS_RESOURCE_TYPE_REGISTRATION_H

namespace CgsResource
{
    // Register every reconstructed resource-type handler with the registry (TypeRegistry), so the
    // bundle loader's FTypeResolver can resolve their ids. Call once during resource-system
    // bring-up, before loading any bundle. Grows as more handlers are brought up.
    //
    // A font (id 0x21) imports its atlas pages as rasters (id 0), so the raster + texture-state
    // handlers must be registered alongside the font handler for a font bundle to resolve fully.
    void RegisterAllResourceTypes();
}

#endif
