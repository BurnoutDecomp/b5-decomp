#pragma once

#include "types.hpp"

namespace renderengine
{
struct ResourceDescriptor5
{
    struct Entry
    {
        u32 muSize;
        u32 muAlignment;
    };

    Entry maEntries[5];
};

class DepthStencilState
{
public:
    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor);
};

class RasterizerState
{
public:
    struct Parameters
    {
        u32 muFillMode;
        u32 muCullMode;
        u32 muDepthBias;
        u32 muSlopeScaledDepthBias;
        u32 muMultisampleEnable;
        u32 muAntialiasedLineEnable;
        u8  mu8ScissorEnable;
        u8  mu8DepthClipEnable;
        u8  mu8FrontCounterClockwise;
        u8  mu8ConservativeRaster;
        u8  mu8PaddingMode;
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters* lpParameters);
    static RasterizerState* Initialize(RasterizerState** ppState, const Parameters* lpParameters);
    RasterizerState* Initialize(const Parameters* lpParameters);

private:
    u32 maState[13];
};

class MeshHelper
{
public:
    struct MeshData
    {
        u32 muPrimaryCount;
        u32 muSecondaryCount;
        u32 maValues[17];
    };

    MeshData* Initialize(const u32* lpaParameters);

private:
    MeshData* mpData;
};

class VertexDescriptor
{
public:
    class Parameters
    {
    public:
        Parameters();

    private:
        struct Element
        {
            u16 mu16Stream;
            u16 mu16Pad0;
            s32 miOffset;
            u8  mu8Type;
            u8  mu8Pad1;
            u8  mu8Usage;
            u8  mu8UsageIndex;
            u8  mu8Enabled;
            u8  maPad2[3];
        };

        Element maElements[16];
        u8      maElementFlags[16];
    };
};
}
