#include "GameShared/GameClasses/RenderWare/CgsRwMaterialCRC32ResourceType.h"
#include "GameShared/GameClasses/Graphics/Dispatch/materialcrc32.h"   // MaterialCRC32
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5>
#include "types.hpp"

// CgsResource::RwMaterialCRC32ResourceType -- the RwMaterialCRC32 resource-type handler (id 0xB).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsResource::RwMaterialCRC32ResourceType::GetSerialisedResourceDescriptor @ 0x828A88B0
//
// Faithful to the X360 body: a thin forwarder. It returns the descriptor produced by the
// material's own MaterialCRC32::GetResourceDescriptor (X360 0x827F7878) -- the result buffer is
// the by-value return (X360 sret r3 saved across the call and returned unchanged). The handler
// adds nothing of its own; the sizing maths live in the MaterialCRC32 engine home.

namespace CgsResource
{
    ResourceDescriptor RwMaterialCRC32ResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // X360 0x828A88B0: mr r31,r3 (save sret); bl MaterialCRC32::GetResourceDescriptor; mr r3,r31.
        // The serialised resource IS the MaterialCRC32 object; forward to its descriptor builder.
        MaterialCRC32* lpMaterial = const_cast<MaterialCRC32*>(static_cast<const MaterialCRC32*>(lpResource));
        return lpMaterial->GetResourceDescriptor();
    }
}
