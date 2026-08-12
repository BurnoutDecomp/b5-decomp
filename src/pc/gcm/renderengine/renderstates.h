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

    // The 0x4A-byte depth/stencil parameter block the immediate-mode builder fills positionally
    // (X360 asm: 17 state words at +0x00..+0x40, then 6 flag bytes at +0x44..+0x49). Only the words
    // the builder writes are named; the rest are zeroed. muFunction is the leading word (the
    // requested comparison function).
    //
    // NOTE (2026-08-12): Initialize @0x82B62890 is now known to copy this block POSITIONALLY into
    // the object (Parameters word N -> object word N; Parameters byte 0x44+N -> object word
    // 0x44+4*N), which means every field below has a known destination and therefore a known
    // meaning -- see the per-field destination comments. The placeholder names (muState4,
    // muState8, ...) are kept ONLY because three consumers already spell them that way
    // (CgsDepthStencilStateFactory.cpp, CgsImRenderer.cpp, ImmediateModePCLeaf.cpp); renaming
    // them is a follow-up that has to land together with those files.
    struct Parameters
    {
        u32 muFunction;        // +0x00 -> muZFunc                 (the Function arg)
        u32 maState1[3];       // +0x04 -> muStencilFail / muStencilZFail / muStencilPass (zeroed)
        u32 muState4;          // +0x10 -> muStencilFunc           == 7 (E_FUNCTION_ALWAYS)
        u32 maState5[3];       // +0x14 -> muCcwStencilFail / ZFail / Pass (zeroed)
        u32 muState8;          // +0x20 -> muCcwStencilFunc        == 7 (E_FUNCTION_ALWAYS)
        u32 muState9;          // +0x24 -> muHiStencilFunc         (zeroed)
        u32 muState10;         // +0x28 -> muStencilRef            (zeroed)
        u32 muStencilReadMask; // +0x2C -> muStencilMask           == -1
        u32 muStencilWriteMask;// +0x30 -> muStencilWriteMask      == -1
        u32 muState13;         // +0x34 -> muCcwStencilRef         (zeroed)
        u32 muState14;         // +0x38 -> muCcwStencilMask        == -1
        u32 muState15;         // +0x3C -> muCcwStencilWriteMask   == -1
        u32 muState16;         // +0x40 -> muHiStencilRef          (zeroed)
        u8  mbDepthTestEnable; // +0x44 -> muZEnable               (the first bool arg)
        u8  mbDepthWriteEnable;// +0x45 -> muZWriteEnable          (the second bool arg)
        u8  mu8Flag2;          // +0x46 -> muStencilEnable         (zeroed)
        u8  mu8Flag3;          // +0x47 -> muTwoSidedStencilMode   (zeroed)
        u8  mu8Flag4;          // +0x48 -> muHiStencilEnable       (zeroed)
        u8  mu8Flag5;          // +0x49 -> muHiStencilWriteEnable  (zeroed)
    };

    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor);
    // X360 the immediate-mode builder passes the params alongside the out descriptor; the descriptor
    // build itself is a fixed { size, align } block and ignores the params. Additive overload.
    static ResourceDescriptor5* GetResourceDescriptor(ResourceDescriptor5* lpDescriptor, const Parameters* lpParameters);
    static DepthStencilState* Initialize(DepthStencilState** ppState, const Parameters* lpParameters);

    // The 0x60-byte object the X360 GetResourceDescriptor (0x82B636F8) sizes ({0x60, 4} entry-0
    // qword) and the serialised world MaterialState blob embeds: the 17 marshalled state words,
    // then the six Parameters flag bytes stw-widened to words, then the initialised word
    // Initialize sets to 1. All three groups are DIRECTLY attested by
    // renderengine::DepthStencilState::Initialize @0x82B62890 (17x lwz/stw, 6x lbz->stw at
    // 0x44/0x48/0x4C/0x50/0x54/0x58, li 1 -> stw 0x5C) -- recovered from the ARTIST .i64, which
    // names the function; it is merely absent from the JSON export subset.
    //
    // Every word's meaning is attested by the X360 depth/stencil half of the dispatch state
    // triple (@0x827E8150), which pushes each one through its own D3DDevice_SetRenderState_*
    // fast-set. Note that the comparison words carry the XENOS D3DCMPFUNC encoding, which is the
    // GPU's own 3-bit function field and is ZERO-based (NEVER=0 .. ALWAYS=7) -- attested by
    // D3DDevice_SetRenderState_ZFunc @0x82939870 packing `(16*Value) & 0x70` into RB_DEPTHCONTROL
    // and D3DDevice_SetRenderState_AlphaFunc @0x82939328 packing `Value & 7`. PC Direct3D 9's
    // D3DCMPFUNC is ONE-based, so a PC leaf that forwards one of these has to add 1.
    u32 muZFunc;                   // +0x00 -> ZFunc
    u32 muStencilFail;             // +0x04 -> StencilFail
    u32 muStencilZFail;            // +0x08 -> StencilZFail
    u32 muStencilPass;             // +0x0C -> StencilPass
    u32 muStencilFunc;             // +0x10 -> StencilFunc
    u32 muCcwStencilFail;          // +0x14 -> CCWStencilFail
    u32 muCcwStencilZFail;         // +0x18 -> CCWStencilZFail
    u32 muCcwStencilPass;          // +0x1C -> CCWStencilPass
    u32 muCcwStencilFunc;          // +0x20 -> CCWStencilFunc
    u32 muHiStencilFunc;           // +0x24 -> HiStencilFunc
    u32 muStencilRef;              // +0x28 -> StencilRef
    u32 muStencilMask;             // +0x2C -> StencilMask
    u32 muStencilWriteMask;        // +0x30 -> StencilWriteMask
    u32 muCcwStencilRef;           // +0x34 -> CCWStencilRef
    u32 muCcwStencilMask;          // +0x38 -> CCWStencilMask
    u32 muCcwStencilWriteMask;     // +0x3C -> CCWStencilWriteMask
    u32 muHiStencilRef;            // +0x40 -> HiStencilRef
    u32 muZEnable;                 // +0x44 -> ZEnable          (Parameters::mbDepthTestEnable)
    u32 muZWriteEnable;            // +0x48 -> ZWriteEnable      (Parameters::mbDepthWriteEnable)
    u32 muStencilEnable;           // +0x4C -> StencilEnable
    u32 muTwoSidedStencilMode;     // +0x50 -> TwoSidedStencilMode
    u32 muHiStencilEnable;         // +0x54 -> HiStencilEnable
    u32 muHiStencilWriteEnable;    // +0x58 -> HiStencilWriteEnable
    u32 muInitialised;             // +0x5C 1 once Initialize has run

    // Never called; exists only to pin the recovered layout. A member-function BODY is a
    // complete-class context (a bare static_assert in the class body would not be), so offsetof
    // on the members is legal here.
    //
    // These pins are POINTER-INVARIANT: every member is a u32, so the X360 offsets transfer to the
    // x64 host unchanged -- there is no 4->8 widening anywhere in this object. Double-attested:
    //   * the word INDEX each member is read at by the dispatch applier
    //     shadow::Device::Xbox2SetDepthStencilStateLowLevelShadowed @0x827E8150 (a1[0]..a1[22]);
    //   * the byte OFFSET each member is stored at by renderengine::DepthStencilState::Initialize
    //     @0x82B62890 (stw ..., 0x00(r3) .. 0x5C(r3)).
    static void _AssertLayout()
    {
        static_assert(sizeof(DepthStencilState) == 0x60, "X360 GetResourceDescriptor @0x82B636F8 sizes the object at 0x60");

        static_assert(offsetof(DepthStencilState, muZFunc)                == 0x00, "a1[0]  -> ZFunc");
        static_assert(offsetof(DepthStencilState, muStencilFail)          == 0x04, "a1[1]  -> StencilFail");
        static_assert(offsetof(DepthStencilState, muStencilZFail)         == 0x08, "a1[2]  -> StencilZFail");
        static_assert(offsetof(DepthStencilState, muStencilPass)          == 0x0C, "a1[3]  -> StencilPass");
        static_assert(offsetof(DepthStencilState, muStencilFunc)          == 0x10, "a1[4]  -> StencilFunc");
        static_assert(offsetof(DepthStencilState, muCcwStencilFail)       == 0x14, "a1[5]  -> CCWStencilFail");
        static_assert(offsetof(DepthStencilState, muCcwStencilZFail)      == 0x18, "a1[6]  -> CCWStencilZFail");
        static_assert(offsetof(DepthStencilState, muCcwStencilPass)       == 0x1C, "a1[7]  -> CCWStencilPass");
        static_assert(offsetof(DepthStencilState, muCcwStencilFunc)       == 0x20, "a1[8]  -> CCWStencilFunc");
        static_assert(offsetof(DepthStencilState, muHiStencilFunc)        == 0x24, "a1[9]  -> HiStencilFunc");
        static_assert(offsetof(DepthStencilState, muStencilRef)           == 0x28, "a1[10] -> StencilRef");
        static_assert(offsetof(DepthStencilState, muStencilMask)          == 0x2C, "a1[11] -> StencilMask");
        static_assert(offsetof(DepthStencilState, muStencilWriteMask)     == 0x30, "a1[12] -> StencilWriteMask");
        static_assert(offsetof(DepthStencilState, muCcwStencilRef)        == 0x34, "a1[13] -> CCWStencilRef");
        static_assert(offsetof(DepthStencilState, muCcwStencilMask)       == 0x38, "a1[14] -> CCWStencilMask");
        static_assert(offsetof(DepthStencilState, muCcwStencilWriteMask)  == 0x3C, "a1[15] -> CCWStencilWriteMask");
        static_assert(offsetof(DepthStencilState, muHiStencilRef)         == 0x40, "a1[16] -> HiStencilRef");
        static_assert(offsetof(DepthStencilState, muZEnable)              == 0x44, "a1[17] -> ZEnable");
        static_assert(offsetof(DepthStencilState, muZWriteEnable)         == 0x48, "a1[18] -> ZWriteEnable");
        static_assert(offsetof(DepthStencilState, muStencilEnable)        == 0x4C, "a1[19] -> StencilEnable");
        static_assert(offsetof(DepthStencilState, muTwoSidedStencilMode)  == 0x50, "a1[20] -> TwoSidedStencilMode");
        static_assert(offsetof(DepthStencilState, muHiStencilEnable)      == 0x54, "a1[21] -> HiStencilEnable");
        static_assert(offsetof(DepthStencilState, muHiStencilWriteEnable) == 0x58, "a1[22] -> HiStencilWriteEnable");
        static_assert(offsetof(DepthStencilState, muInitialised)          == 0x5C, "Initialize's li 1 / stw 0x5C");

        // Parameters: every offset the Initialize @0x82B62890 copy reads. Also pointer-invariant
        // (17 u32 + 6 u8). The six flag BYTES are what makes the block 0x4A, not 0x5C -- getting
        // this wrong would shear the whole widened tail.
        static_assert(offsetof(Parameters, muFunction)         == 0x00, "lwz 0x00(r4)");
        static_assert(offsetof(Parameters, maState1)           == 0x04, "lwz 0x04/0x08/0x0C(r4)");
        static_assert(offsetof(Parameters, muState4)           == 0x10, "lwz 0x10(r4)");
        static_assert(offsetof(Parameters, maState5)           == 0x14, "lwz 0x14/0x18/0x1C(r4)");
        static_assert(offsetof(Parameters, muState8)           == 0x20, "lwz 0x20(r4)");
        static_assert(offsetof(Parameters, muStencilReadMask)  == 0x2C, "lwz 0x2C(r4)");
        static_assert(offsetof(Parameters, muStencilWriteMask) == 0x30, "lwz 0x30(r4)");
        static_assert(offsetof(Parameters, muState16)          == 0x40, "lwz 0x40(r4) -- last word copy");
        static_assert(offsetof(Parameters, mbDepthTestEnable)  == 0x44, "lbz 0x44(r4)");
        static_assert(offsetof(Parameters, mbDepthWriteEnable) == 0x45, "lbz 0x45(r4)");
        static_assert(offsetof(Parameters, mu8Flag2)           == 0x46, "lbz 0x46(r4)");
        static_assert(offsetof(Parameters, mu8Flag3)           == 0x47, "lbz 0x47(r4)");
        static_assert(offsetof(Parameters, mu8Flag4)           == 0x48, "lbz 0x48(r4)");
        static_assert(offsetof(Parameters, mu8Flag5)           == 0x49, "lbz 0x49(r4)");
    }
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

// renderengine's packed 19-word blend material block (the vendor home is
// SDKs/RenderEngineClub .../states/blendstate.h: BlendMaterialState { u32 maState[19] };
// BlendState::Initialize @0x82B627C8 is stw-only -- all-u32 -- and the last word +0x48 is the
// initialised flag). Forward-declared here so MaterialState can hold a typed pointer without
// coupling this header to the vendor tree.
struct BlendMaterialState;
class RasterizerState;   // defined below (13 u32 words)

// renderengine::MaterialState -- a material's render-state block: three self-pointers to the
// state objects serialised in the same resource, then those objects. The X360 assert strings in
// CgsMaterialTechniqueResourceType::PostFixUp (0x828A7EC8: "lpMaterial->mpMaterialState->
// mpBlendState", with *mpBlendState handed to renderengine::BlendState::GetParameters) name the
// first field; the second and third blocks match the DepthStencilState (24 words, function word
// first, the E_FUNCTION_ALWAYS=7 lanes at +0x10/+0x20) and RasterizerState (13 words; FixDown
// 0x828A8A80 marks its +0x28 word) objects respectively.
//
// Serialised form: X360 blob = 252 bytes { 3 u32 offsets {0xC, 0x5C, 0xBC} + blend 0x50 +
// depth-stencil 0x60 + rasterizer 0x40 }. The platform-4 (x64) blob widens ONLY the three
// pointer slots (u64 offsets {0x18, 0x68, 0xC8}) = 264 bytes == sizeof(MaterialState); the
// state objects are pointer-free u32 blocks and keep their X360 layout. Porter:
// tools/assets/bundles/world_type_transcode.py transcode_materialstate.
class MaterialState
{
public:
    BlendMaterialState* mpBlendState;         // +0  (X360 +0x00) -> +0x18 blend block
    DepthStencilState*  mpDepthStencilState;  // +8  (X360 +0x04) -> +0x68 depth-stencil block
    RasterizerState*    mpRasterizerState;    // +16 (X360 +0x08) -> +0xC8 rasterizer block
                                              //      (FixDown sets its +0x28 u32 = 1)
    u8    mauState[240];   // the three serialised state objects (X360 +0x0C..+0xFB)
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

    // The 13-word marshalled block. Every word's meaning is attested by the X360 rasteriser
    // half of the dispatch state triple (@0x827E8690), which pushes each one through its own
    // D3DDevice_SetRenderState_* fast-set in this order -- so these are names for words that
    // were previously an opaque maState[13].
    u32 muFillMode;                // +0x00 -> D3DDevice_SetRenderState_FillMode
    u32 muCullMode;                // +0x04 -> CullMode
    u32 muDepthBias;               // +0x08 -> DepthBias
    u32 muSlopeScaleDepthBias;     // +0x0C -> SlopeScaleDepthBias
    u32 muMultiSampleMask;         // +0x10 -> MultiSampleMask
    u32 muScissorTestEnable;       // +0x14 -> ScissorTestEnable
    u32 muMultiSampleAntiAlias;    // +0x18 -> MultiSampleAntiAlias
    u32 muUnused1C;                // +0x1C  (word 7: the applier never reads it)
    u32 muViewportEnable;          // +0x20 -> ViewportEnable
    u32 muHalfPixelOffset;         // +0x24 -> HalfPixelOffset
    u32 muPrimitiveResetEnable;    // +0x28 -> PrimitiveResetEnable (the word MaterialState::
                                   //          FixDown @0x828A8A80 marks)
    u32 muPrimitiveResetIndex;     // +0x2C -> PrimitiveResetIndex (bound only when enabled)
    u32 muInitialised;             // +0x30  1 once Initialize has run
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

    // Bind a compiled MeshData to the device (X360 0x8227B530). The primary values are the
    // index buffers (bound one after another via TDevice's SetIndices) and the secondary values
    // are the vertex-stream buffers (bound to stream slots 0.. via SetStreamSource). TDevice is
    // the render-backend tag the X360 compile-time-selected and inlined to the D3D fast-path on
    // the renderengine device singleton; the only instantiation the build emits is
    // Dispatch<renderengine::Device>. Static: the X360 takes the MeshData directly (no MeshHelper
    // this). Body + explicit instantiation live in MeshHelper.cpp so the D3D fast-path externs
    // stay out of this shared header. Called by the debris + post-fx quad renderers.
    template< typename TDevice >
    static void Dispatch(const MeshData* lpData);

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
