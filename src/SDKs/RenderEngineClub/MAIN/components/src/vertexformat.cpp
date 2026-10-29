#include "SDKs/RenderEngineClub/MAIN/components/src/vertexformat.h"

namespace renderengine
{
// ARTIST 82B61310. Unsupported declaration words return zero; the recognised
// constants include endian/sign/swizzle variants, not only the public aliases.
u32 VertexFormatGetStride(VertexFormat format)
{
    switch (static_cast<s32>(format))
    {
    case 85126: case 1583238:
    case 1712262: case 1712263: case 1712518: case 1712519:
    case 1712774: case 1712775: case 1713030: case 1713031:
    case 2760839: case 2760848: case 2760849: case 2761095:
    case 2761104: case 2761105: case 2761351: case 2761360:
    case 2761361: case 2761607: case 2761616: case 2761617:
    case 2891865: case 2892121: case 2892377: case 2892633: case 2892639:
    case 2916513: case 2916769: case 2917025: case 2917281: case 2917284:
        return 4;
    case 1712218: case 1712474: case 1712730: case 1712986: case 1712992:
    case 2891938: case 2892194: case 2892450: case 2892706: case 2892709:
        return 8;
    case 2761657:
        return 12;
    case 1712291: case 1712547: case 1712803: case 1713059: case 1713062:
        return 16;
    default:
        return 0;
    }
}
}
