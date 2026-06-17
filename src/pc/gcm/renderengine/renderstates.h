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

class Texture;  // the imported raster's runtime type (texture.h)

// renderengine::TextureState -- a texture sampler state plus the raster it samples. The raster
// is an imported resource (RwRaster, type id 0); the pool's import resolution writes the resolved
// raster pointer into mpRaster, which the handler (CgsResource::RwTextureStateResourceType)
// reports via GetImportPointer at that slot. X360 object = 36 bytes: a 32-byte sampler block + a
// 4-byte raster pointer at +0x20 (the pointer widens to 8 bytes on the x64 target; the sampler
// block stays opaque until a renderer body needs its fields).
class TextureState
{
public:
    // The unresolved-import sentinel DebugValidate rejects (X360 stores -1 until resolved).
    enum { KU_INVALID_TEXTURE = 0xFFFFFFFFu };

    u8       mauSamplerState[32];  // +0x00 sampler params (filter / address / lod)
    Texture* mpRaster;             // +0x20 imported RwRaster
};

// renderengine::MaterialState (a.k.a. BlendState) -- a material's render-state block. FixUp /
// FixDown rebase three main-memory self-pointers at the front (serialised as offsets); the rest
// is opaque render state. X360 object = 252 bytes (three 4-byte pointers + 240 bytes); on x64
// the pointers widen, so the leading block is 24 bytes. FixDown also marks the sub-object the
// third pointer references (sets its +0x28 u32 to 1).
class MaterialState
{
public:
    void* mpField0;        // +0  (X360 +0x00) main-memory self-pointer
    void* mpField1;        // +8  (X360 +0x04) main-memory self-pointer
    void* mpField2;        // +16 (X360 +0x08) -> sub-object (FixDown sets its +0x28 u32 = 1)
    u8    mauState[240];   // remaining opaque render state (X360 +0x0C..+0xFB)
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
