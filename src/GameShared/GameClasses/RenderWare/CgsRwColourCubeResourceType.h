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
        // The registry id. 43 (0x2B) is measured off the shipped data, not guessed: every
        // ColourCube entry in ENVIRONMENTSETTINGS/COLOURCUBES/PARADISE_INGAME_JUNK.BUNDLE and in
        // POSTFX/COLOURCUBEDICTIONARY.BIN carries type id 43 in its bundle-entry word, their debug
        // string table names those resources type="ColourCube", and the boot log names the same
        // number when the handler is missing:
        //   [bundle] UNREGISTERED resource type id 43 in 'PostFx/colourcubedictionary.bin'
        //     -- scratch/postfx_step9_final/boot1_BrnGame.log:387
        virtual uint32_t           GetTypeID() const;
        virtual ResourceDescriptor GetSerialisedResourceDescriptor(const void* lpResource) const;
    };
}

#endif // CGS_RW_COLOUR_CUBE_RESOURCE_TYPE_H
