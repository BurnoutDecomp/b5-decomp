#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include <xgraphics.h>

namespace renderengine
{
// ARTIST 823C4CB8: descriptor lane 0 is the native header; lane 2 is
// separately allocated GPU-visible storage. Other lanes retain {0,1}.
Texture2D::ResourceDescriptor* Texture2D::GetResourceDescriptor(
    ResourceDescriptor* lpDescriptor, const Parameters* lpParameters)
{
    for (u32 i = 0; i != 5; ++i)
    {
        lpDescriptor->maEntries[i].muSize = 0;
        lpDescriptor->maEntries[i].muAlignment = 1;
    }
    lpDescriptor->maEntries[0].muSize = sizeof(Texture2D);
    lpDescriptor->maEntries[0].muAlignment = 4;
    if (!(lpParameters->muFlags & FLAG_DONTALLOCATE))
    {
        lpDescriptor->maEntries[2].muSize = Xbox2GetPhysicalMemorySize(lpParameters);
        lpDescriptor->maEntries[2].muAlignment = D3DTEXTURE_ALIGNMENT;
    }
    return lpDescriptor;
}

// ARTIST 82B610D8 passes the remaining four SDK parameters on the stack.
u32 Texture2D::Xbox2GetPhysicalMemorySize(const Parameters* lpParameters)
{
    return XGSetTextureHeader(lpParameters->muWidth, lpParameters->muHeight,
        lpParameters->muNumLevels, 0, static_cast<D3DFORMAT>(lpParameters->meFormat),
        D3DPOOL_DEFAULT, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, 0, 0, 0);
}

// ARTIST 82B62E78 expands the five-word 2D parameters into the seven-word
// generic native record before calling the shared initialization body.
Texture2D* Texture2D::Initialize(const rw::Resource* lpResource, const Parameters* lpParameters)
{
    Texture::Parameters lParameters;
    lParameters.meType = TYPE_2D;
    lParameters.muFlags = lpParameters->muFlags;
    lParameters.muWidth = lpParameters->muWidth;
    lParameters.muHeight = lpParameters->muHeight;
    lParameters.muDepth = 1;
    lParameters.muNumLevels = lpParameters->muNumLevels;
    lParameters.meFormat = lpParameters->meFormat;
    return static_cast<Texture2D*>(Texture::Initialize(lpResource, &lParameters));
}
}
