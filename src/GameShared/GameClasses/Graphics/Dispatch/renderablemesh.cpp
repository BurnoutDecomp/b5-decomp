// renderablemesh.cpp -- the global ::RenderableMesh dispatch primitive's out-of-line members.

#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"
#include "rw/rwcore_structs.h"   // rw::BaseResourceDescriptors<5> complete (ResourceDescriptor)

// Allocation requirement of one serialised mesh record: lane 0 is the record plus its
// buffer-pointer table, rounded up to 16 bytes at alignment 16; lanes 1..4 stay the identity
// {0, 1}. The vertex-descriptor count does not enter the size.
CgsResource::ResourceDescriptor RenderableMesh::GetResourceDescriptor(uint32_t luNumVertexBuffers,
                                                                      uint32_t /*luNumVertexDescriptors*/)
{
    const uint32_t luSize = (4u * luNumVertexBuffers + 63u) & 0xFFFFFFF0u;

    CgsResource::ResourceDescriptor lDescriptor;
    lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
    lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16u;
    return lDescriptor;
}
