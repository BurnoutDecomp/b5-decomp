#pragma once

#include "types.hpp"

namespace rw { struct Resource; }   // TextureState::Initialize backing memory (rwcore_structs.h)

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

    // Sampler parameters CgsResource::Font::CreateTextureState builds (X360 0x82835658 values).
    // The exact field meanings are partly inferred from the values; the texture is the atlas page
    // being sampled. On X360 these are marshalled into a Xenos GPU sampler descriptor (via
    // renderengine::SamplerState::Initialize); on PC they configure a D3D sampler at draw time.
    struct Parameters
    {
        u32      muAddressU;      // 2
        u32      muAddressV;      // 2
        u32      muAddressW;      // 0
        u32      muMagFilter;     // 1
        u32      muMinFilter;     // 1
        u32      muMipFilter;     // 1
        u32      muField6;        // 0
        u32      muField7;        // 0
        u32      muMaxAnisotropy; // 13
        u32      muField9;        // 0
        u32      muField10;       // 1
        f32      mfMipLodBias;    // -0.5
        f32      mfField12;       // 0.0
        u32      muField13;       // 0
        u32      muField14;       // 0
        u32      muField15;       // 0
        // Five trailing state-flag bytes the post-fx Tint texture-state build sets at +0x40..+0x44
        // (0,0,0,1,1); the X360 marshals them into the sampler/state descriptor alongside the words
        // above. Modelled explicitly so they can be set by name (the font path leaves them default).
        // Their presence pushes mpTexture to +0x48 (X360 stores the raster there, not at +0x40).
        u8       mu8Field40;      // +0x40 (0)
        u8       mu8Field41;      // +0x41 (0)
        u8       mu8Field42;      // +0x42 (0)
        u8       mu8Field43;      // +0x43 (1)
        u8       mu8Field44;      // +0x44 (1)
        u8       mau8Pad45[3];    // +0x45 align to +0x48
        Texture* mpTexture;       // +0x48 the raster to sample (atlas page 0 / the tint lookup map)
    };

    // Size the texture-state resource (X360 0x82B635C8 builds the rw::BaseResourceDescriptors<5>).
    static void GetResourceDescriptor(u32* lpDescriptorOut);
    // Create a texture state from the sampler parameters (X360 0x82B62720). [PC DIVERGENCE: stores
    // the config + raster for draw-time application instead of marshalling a Xenos GPU descriptor.]
    static TextureState* Initialize(rw::Resource* lpResourceMemory, const Parameters* lpParams);

    u8       mauSamplerState[32];  // +0x00 sampler params (filter / address / lod)
    Texture* mpRaster;             // +0x20 imported / bound RwRaster
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
    // The parsed-element parameter block the resource-type handlers marshal: 16 element records
    // (16 bytes each) + 16 trailing per-element flag bytes. A freshly constructed block seeds every
    // record's offset to -1 (the "no element" sentinel GetSerialisedResourceDescriptor counts).
    class Parameters
    {
    public:
        Parameters();

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

    // 0x82B61260 -- read the serialised descriptor's element table out into a Parameters block,
    // padding the unused tail records with the "no element" sentinel. lpData is the serialised
    // RenderWare vertex-descriptor object (its u16 element-count is at +0x08, its element table at
    // +0x10). [Companion data-model home pc/gcm/renderengine/VertexDescriptor.cpp owns the
    // VertexDescriptorData-typed overload; this Parameters-typed overload is the resource-type
    // handler's call surface.]
    static void GetParameters(const void* lpData, Parameters* lpParamsOut);

    // 0x82B637D0 -- size the rw resource from a filled Parameters block: count the records whose
    // offset is not the sentinel (-1), then write the five-entry serialised descriptor (slot0 =
    // {size = 0x11*validElements + 0x10, alignment = 4}; the other four entries are {size = 0,
    // alignment = 1}). lpDescriptorOut is rw::BaseResourceDescriptors<5> (10 dwords).
    static void GetResourceDescriptor(void* lpDescriptorOut, const Parameters* lpParams);
};
}
