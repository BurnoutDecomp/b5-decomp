// Tiny embed check: include the renderengine::VertexDescriptor home and force ODR use of every
// reconstructed function so the compile gate exercises their signatures end to end. Compile-only
// (cl /c); the platform D3D externals + the pre-release helper resolve at link.
#include "pc/gcm/renderengine/VertexDescriptor.h"

void renderengine_vd_embed_check()
{
    using namespace renderengine;

    VertexDescriptorData lData{};
    VertexDescriptorWrapper lWrapper{};
    lWrapper.mpData = &lData;

    (void)VertexDescriptor::Initialize(&lWrapper, 0);
    (void)VertexDescriptor::CreateD3DObject(&lData);

    u32 lauParams[16 * 4] = {0u};
    (void)VertexDescriptor::GetParameters(&lData, lauParams);

    u64 lauDesc[5] = {0, 0, 0, 0, 0};
    (void)VertexDescriptor::GetResourceDescriptor(lauDesc, 0);

    (void)VertexDescriptor::Release(&lData);
}
