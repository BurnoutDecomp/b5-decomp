#include "GameShared/GameClasses/RenderWare/PS3/CgsRwVertexDescResourceTypePS3.h"
#include "rw/rwcore_structs.h"   // rw::Resource (unused here, kept for the base type)

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A80F8
//   (CgsResource::RwVertexDescResourceType::FixUp)
//
// FixUp marks the vertex descriptor for (re)create (flag at 0x0A) and builds the
// platform D3D object. It uses no relocation delta, so the rw::Resource argument is
// unused. CreateD3DObject is an external platform (render-engine) entry.

namespace renderengine
{
    // Member names/types per burnout.wiki (renderengine::VertexDescriptor); the flag
    // at 0x0A is set before CreateD3DObject (matches the X360 FixUp pseudocode).
    struct VertexDescriptor
    {
        s32 m_refCount;     // 0x00
        u32 m_typesFlags;   // 0x04
        u16 m_numElements;  // 0x08
        u8  mbNeedsCreate;  // 0x0A
        u8  m_pad0B;        // 0x0B
        u32 m_pad0C;        // 0x0C (Element[] begins at 0x10)

        int CreateD3DObject();
    };
}

namespace CgsResource
{
    void RwVertexDescResourceType::FixUp(void* lpResource, const rw::Resource&) const
    {
        renderengine::VertexDescriptor* lpDescriptor =
            static_cast<renderengine::VertexDescriptor*>(lpResource);
        lpDescriptor->mbNeedsCreate = 1;
        lpDescriptor->CreateD3DObject();
    }
}
