#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.h"
#include "rw/rwcore_structs.h"                   // rw::BaseResourceDescriptors<5>
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"

// Native streamed vertex declaration resource lifecycle.
namespace CgsResource
{
    // ARTIST820A0CC0 vtable, GetTypeID828A80F0.
    u32 RwVertexDescResourceType::GetTypeID() const
    {
        return E_RESOURCETYPE_VERTEX_DESCRIPTOR;
    }

    EDebugResourceCategory RwVertexDescResourceType::GetDebugResourceCategory() const
    {
        return E_DEBUGRESOURCECATEGORY_MESH;
    }

    void RwVertexDescResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        // ARTIST828A80F8 marks initialized then creates the GPU declaration.
        renderengine::VertexDescriptorData* lpData = static_cast<renderengine::VertexDescriptorData*>(lpResource);
        lpData->muInitialised = 1;
        renderengine::VertexDescriptor::CreateD3DObject(lpData);
    }

    // X360 0x828A8A28: construct the parsed-element parameter block, fill it from the serialised
    // resource, forward to the renderengine sizer; the descriptor is returned by value (X360 sret).
    ResourceDescriptor RwVertexDescResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        renderengine::VertexDescriptor::Parameters lParameters;        // Parameters::Parameters(v6)
        renderengine::VertexDescriptor::GetParameters(lpResource, &lParameters);  // GetParameters(a3, v6)

        return renderengine::VertexDescriptor::GetResourceDescriptor(lParameters);
    }
}
