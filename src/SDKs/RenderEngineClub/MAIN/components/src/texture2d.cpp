#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include <xgraphics.h>
#include "rw/rwcore_structs.h"

namespace renderengine
{
// ARTIST 82B610D8 passes the remaining four SDK parameters on the stack.
u32 Texture2D::Xbox2GetPhysicalMemorySize(const Parameters& parameters)
{
    UINT luBaseSize;
    return XGSetTextureHeader(parameters.muWidth, parameters.muHeight,
        parameters.muNumLevels, 0, static_cast<D3DFORMAT>(parameters.meFormat),
        D3DPOOL_DEFAULT, 0, XGHEADER_CONTIGUOUS_MIP_OFFSET, 0, NULL, &luBaseSize, NULL);
}

// ARTIST 82B62E78 expands the five-word 2D parameters into the seven-word
// generic native record before calling the shared initialization body.
Texture2D* Texture2D::Initialize(const rw::Resource& resource, const Parameters& parameters)
{
    const Parameters* lpParameters = &parameters;
    Texture::Parameters lParameters;
    lParameters.meType = TYPE_2D;
    lParameters.muFlags = lpParameters->muFlags;
    lParameters.muWidth = lpParameters->muWidth;
    lParameters.muHeight = lpParameters->muHeight;
    lParameters.muDepth = 1;
    lParameters.muNumLevels = lpParameters->muNumLevels;
    lParameters.meFormat = lpParameters->meFormat;
    return static_cast<Texture2D*>(Texture::Initialize(resource, lParameters));
}
}
