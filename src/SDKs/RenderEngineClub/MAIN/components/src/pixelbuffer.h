#pragma once

#include "types.hpp"
#include <xtl.h>
#include "SDKs/RenderEngineClub/MAIN/components/src/pixelformat.h"

namespace rw { struct Resource; struct ResourceDescriptor; }

namespace renderengine
{
class Texture;

class PixelBuffer : public D3DSurface
{
public:
    enum Type { TYPE_NA = -1, TYPE_RENDERTARGET = 0, TYPE_DEPTHSTENCIL = 1 };
    enum { FLAG_DONTALLOCATE = 1 };
    struct Parameters
    {
        Type type;
        u32 flags;
        u32 width;
        u32 height;
        PixelFormat format;
        u32 multiSampleType;
        s32 colorExpBias;
    };

    static rw::ResourceDescriptor GetResourceDescriptor(const Parameters&);
    static PixelBuffer* Initialize(const rw::Resource&, const Parameters&);
    void Xbox2SetBaseEDRAM(s16 base);
    void Xbox2SetBaseHierarchicalZ(u32 base);
    bool Xbox2ResolveTo(Texture*, u32 flags, u32 x, u32 y, u32 destLevel,
        u32 destSlice, const D3DVECTOR4* clearColor, float clearZ, u32 clearStencil);

protected:
    Type m_type;
};

CGS_STATIC_ASSERT((sizeof(D3DSurface) == 0x30), "ARTIST SDK surface");
CGS_STATIC_ASSERT((sizeof(PixelBuffer) == 0x34), "ARTIST pixel buffer");
CGS_STATIC_ASSERT((sizeof(PixelBuffer::Parameters) == 0x1C), "ARTIST pixel parameters");
}
