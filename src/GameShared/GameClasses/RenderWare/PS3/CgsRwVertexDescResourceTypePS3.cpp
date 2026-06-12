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
    struct VertexDescriptor
    {
        u8   mPad[10];          // [0x00] opaque
        u8   mbNeedsCreate;     // [0x0A] flag set before CreateD3DObject

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
