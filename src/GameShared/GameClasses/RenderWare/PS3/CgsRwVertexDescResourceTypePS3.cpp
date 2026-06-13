#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A80F8
//   (CgsResource::RwVertexDescResourceType::FixUp)
//
// Behaviour-faithful to the X360 pseudocode:
//     *(descriptor + 10) = 1;                                  // mark for (re)create
//     return renderengine::VertexDescriptor::CreateD3DObject(descriptor);
//
// `this` (a1) is unused; the operand is the vertex descriptor being fixed up after
// load. A flag at offset 0xA is set before the platform D3D object is built.
// CreateD3DObject is an external platform (render-engine) entry; forward-declared.

namespace renderengine
{
    // Member names/types per burnout.wiki (Vertex Descriptor / Xbox 360 ->
    // renderengine::VertexDescriptor); the flag at 0x0A is unnamed there and kept
    // descriptive. Layout matches the X360 spine FixUp pseudocode (sets *+0x0A).
    struct VertexDescriptor
    {
        s32  m_refCount;        // 0x00
        u32  m_typesFlags;      // 0x04 type flags
        u16  m_numElements;     // 0x08 number of elements
        u8   mbNeedsCreate;     // 0x0A flag set before CreateD3DObject
        u8   m_pad0B;           // 0x0B padding
        u32  m_pad0C;           // 0x0C padding (Element[] begins at 0x10)

        int CreateD3DObject();
    };
}

namespace CgsResource
{
    class RwVertexDescResourceType
    {
    public:
        int FixUp(renderengine::VertexDescriptor* lpDescriptor);
    };

    int RwVertexDescResourceType::FixUp(renderengine::VertexDescriptor* lpDescriptor)
    {
        lpDescriptor->mbNeedsCreate = 1;
        return lpDescriptor->CreateD3DObject();
    }
}
