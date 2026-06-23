#include "GameShared/GameClasses/RenderWare/CgsRwColourCubeResourceType.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxcolourcube.h"  // rw::graphics::postfx::ColourCube
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5>
#include "types.hpp"

// CgsResource::RwColourCubeResourceType -- the post-fx colour-cube resource-type handler.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwColourCubeResourceType::GetSerialisedResourceDescriptor @ 0x828A8678
//
// Faithful to the X360 body: the handler reads the cube edge length (the first u32 of the
// serialised colour-cube resource), copies it into a local ColourCube::Parameters block, then
// forwards to rw::graphics::postfx::ColourCube::GetResourceDescriptor, which writes the five-entry
// descriptor (slot0 = {3*N^3 + 16, align 16}, the packed RGB volume + 16-byte header). The result
// descriptor is returned by value (X360 sret). See rwgpfxcolourcube.cpp for the sizing maths.

namespace CgsResource
{
    ResourceDescriptor RwColourCubeResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // X360 0x828A8678: v5[0] = *a3 (the edge length); ColourCube::GetResourceDescriptor(out, v5).
        rw::graphics::postfx::ColourCube::Parameters lParameters;
        lParameters.muEdgeLength = *reinterpret_cast<const u32*>(lpResource);

        ResourceDescriptor lDescriptor;
        rw::graphics::postfx::ColourCube::GetResourceDescriptor(&lDescriptor, &lParameters);
        return lDescriptor;
    }
}
