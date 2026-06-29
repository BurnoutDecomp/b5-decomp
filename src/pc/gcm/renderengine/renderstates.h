#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/VertexDescriptor.h"

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
    // Depth-test comparison function the immediate-mode library selects per state
    // (CgsGraphics::ImRendererBase::ConstructDepthStencilState passes one of these as the
    // leading parameter word). Only the values the X360 state-library builder uses are attested;
    // E_FUNCTION_ALWAYS == 7 is the value the asm stores for the depth/stencil func words
    // (the "always pass" function), so it is named; the rest are left unenumerated until attested.
    enum Function : u32
    {
        E_FUNCTION_ALWAYS = 7,
    };

    // The 0x44-byte depth/stencil parameter block the immediate-mode builder fills positionally
    // (X360 asm: 17 state words + 6 flag bytes). Only the words the builder writes are named; the
    // rest are zeroed. muFunction is the leading word (the requested comparison function).
    struct Parameters
    {
        u32 muFunction;        // +0x00 maState[0]  (the Function arg)
        u32 maState1[3];       // +0x04 maState[1..3] (zeroed)
        u32 muState4;          // +0x10 maState[4]  == 7 (E_FUNCTION_ALWAYS)
        u32 maState5[3];       // +0x14 maState[5..7] (zeroed)
        u32 muState8;          // +0x20 maState[8]  == 7 (E_FUNCTION_ALWAYS)
        u32 muState9;          // +0x24 maState[9]  (zeroed)
        u32 muState10;         // +0x28 maState[10] (zeroed)
        u32 muStencilReadMask; // +0x2C maState[11] == -1
        u32 muStencilWriteMask;// +0x30 maState[12] == -1
        u32 muState13;         // +0x34 maState[13] (zeroed)
        u32 muState14;         // +0x38 maState[14] == -1
        u32 muState15;         // +0x3C maState[15] == -1
        u32 muState16;         // +0x40 maState[16] (zeroed)
        u8  mbDepthTestEnable; // +0x44 (the first bool arg)
        u8  mbDepthWriteEnable;// +0x45 (the second bool arg)
        u8  mu8Flag2;          // +0x46 (zeroed)
        u8  mu8Flag3;          // +0x47 (zeroed)
        u8  mu8Flag4;          // +0x48 (zeroed)
        u8  mu8Flag5;          // +0x49 (zeroed)
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor);
    // X360 the immediate-mode builder passes the params alongside the out descriptor; the descriptor
    // build itself is a fixed { size, align } block and ignores the params. Additive overload.
    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters* lpParameters);
    static DepthStencilState* Initialize(DepthStencilState** ppState, const Parameters* lpParameters);

private:
    u32 maState[17];
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
    // Face-cull selector the immediate-mode library passes per rasterizer state
    // (CgsGraphics::ImRendererBase::ConstructRasteriserState's second parameter, stored verbatim
    // into Parameters::muCullMode). The concrete enumerator values are chosen by the (out-of-scope)
    // caller ConstructOnceOnly, so none are attested here; modelled as a u32-backed enum so the
    // value flows through by name without inventing enumerators.
    enum CullMode : u32 {};

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

// renderengine::StateHelper -- the render-engine's default render-state table. Initialize (X360
// 0x82B64108, called from BrnRendererModule::Construct) builds the canonical set of default state
// objects at boot -- four blend states, one depth/stencil state, and one rasterizer state -- through
// the RenderWare default resource allocator, and records the resulting handles in the file-scope
// default-state slots (kept as class statics here so they are accessed by name, not by raw address).
// ResizeDefaultScissorRectAndViewportParameters (X360 0x82B63D30) rebuilds the default full-screen
// viewport float block and the default scissor rectangle from the current display resolution.
class StateHelper
{
public:
    // Build the default state table. Returns 1 if the device was ready (the default scissor/viewport
    // was also (re)built), 0 otherwise. X360 @0x82B64108.
    static int Initialize();

    // Rebuild the default full-screen viewport parameter block and the default scissor rectangle
    // from the current display resolution. X360 @0x82B63D30.
    static int ResizeDefaultScissorRectAndViewportParameters();
};

}
