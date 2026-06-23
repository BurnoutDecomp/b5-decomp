#ifndef CGS_RW_COLOUR_CUBE_RESOURCE_TYPE_H
#define CGS_RW_COLOUR_CUBE_RESOURCE_TYPE_H

#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"

namespace CgsResource
{
    // The post-fx colour-cube resource-type handler. The serialised resource is a
    // rw::graphics::postfx::ColourCube build-parameters block whose leading word is the cube
    // edge length N; the descriptor it produces sizes the N x N x N packed-RGB volume (3 bytes
    // per cell) plus a 16-byte header, 16-byte aligned. The handler's
    // GetSerialisedResourceDescriptor (X360 0x828A8678) forwards to
    // rw::graphics::postfx::ColourCube::GetResourceDescriptor. Base recovered from the sibling
    // RenderWare resource-type handlers; GetSerialisedResourceDescriptor is owned here.
    class RwColourCubeResourceType : public Type
    {
    public:
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
    };
}

#endif // CGS_RW_COLOUR_CUBE_RESOURCE_TYPE_H
