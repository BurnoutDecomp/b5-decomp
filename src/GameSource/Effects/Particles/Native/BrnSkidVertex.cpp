// CgsGraphics::ImRenderer<BrnGraphics::SkidVertex> -- the X360 immediate-mode skid-trail renderer
// instantiation. This is the renderer behind the skid/tyre-mark decal path: BrnParticle::Native::
// TrailRenderer::BeginRender / Render drive BeginRendering / Render, and BrnGraphics::Im3dSkidsRenderer
// ::Construct drives Construct.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<SkidVertex> member bodies):
//   CgsGraphics::ImRenderer<SkidVertex>::AddProgram      @ 0x822870B8
//   CgsGraphics::ImRenderer<SkidVertex>::BeginRendering  @ 0x8227C1E8
//   CgsGraphics::ImRenderer<SkidVertex>::Construct       @ 0x8228E1D8
//   CgsGraphics::ImRenderer<SkidVertex>::Render          @ 0x8228E068
//   CgsGraphics::ImRenderer<SkidVertex>::SetProgram      @ 0x827DC2B8
//
// This mirrors CgsIm3dSkyDome.cpp / CgsIm2dUntex.cpp exactly: the per-vertex-type member bodies are
// defined out-of-class then the members this TU owns are instantiated INDIVIDUALLY (never
// `template struct ImRenderer<V>`, which would force fabricated bodies for the template members this
// TU does not attest -- the wave-30 lesson). EndRendering / SetTransform / RenderStart / RenderEnd are
// NOT attested by the X360 ARTIST for this TU, so they are neither reconstructed nor instantiated here.
//
// The asm is authoritative for every constant: the two vertex-descriptor element words (0x2A23B9 and
// 0x1A23A6), the element[1] byte-offset (12), the usage-index codes (1 / 6), the "is pixel program"
// flag (1), the 28-byte GPU stream stride and the 0x80000 fence threshold all come straight from the
// immediates -- none are fabricated.

#include "GameSource/Effects/Particles/Native/BrnSkidVertex.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the program/descriptor resource handles the
    // matching renderengine *::Initialize then turns into the live object. Modelled by name here,
    // mirroring CgsIm3dSkyDome.cpp -- the immediate-mode layer only ever reaches the allocator through
    // this one virtual. The X360 call is (*(*a2 + 16))(handlesOut, a2, descriptor, 0).
    class ResourceAllocator
    {
    public:
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void* lpResourceHandlesOut,
            ResourceAllocator* /*lpAllocator*/,
            const void* lpDescriptor,
            int /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpResourceHandlesOut, lpDescriptor);
        }
    };

    // The two vertex-descriptor element type-words the X360 Construct stores (the inlined
    // SkidVertex::FillVertexDescriptorParameters). element[0] is the 12-byte position attribute,
    // element[1] the packed UV/time/alpha attribute at byte-offset 12. The values are the raw asm
    // immediates (lis/ori r11,0x2A,0x23B9 == 0x2A23B9 and lis/ori r11,0x1A,0x23A6 == 0x1A23A6); none
    // are fabricated.
    const u32 KU_POSITION_ELEMENT_WORD    = 0x2A23B9u;
    const u32 KU_UVTIMEALPHA_ELEMENT_WORD = 0x1A23A6u;

    // The element[1] in-stream byte offset (the asm immediate 0xC stored at the element's +2 word):
    // the second attribute starts 12 bytes into the vertex (after the 12-byte position).
    const u16 KU_UVTIMEALPHA_ELEMENT_OFFSET = 12u;

    // ImRenderer<V>::Render submits this vertex with a 28-byte GPU stream stride (li r6, 0x1C) and
    // inserts a GPU fence whenever the batch byte-size (28 * count) exceeds this threshold
    // (lis r10,8 -> 0x80000). The in-memory SkidVertex is 32 bytes (Vector3 + Vector4, both 16-byte
    // vpu), so the per-vertex source advance is sizeof(V) == 32 (asm addi r11,r11,0x20).
    const u32 KU_SKID_VERTEX_STRIDE   = 28u;
    const u32 KU_FENCE_BYTE_THRESHOLD = 0x80000u;

    // ---- per-module shadow caches (X360 .data block off_83010950) -----------------------------------
    // One set per module (NOT per renderer), shared by BeginRendering and SetProgram in this TU.
    // dword_8301095C (last vertex program), off_83010958 (last vertex descriptor), byte_83010A34
    // (vertex-program-state dirty flag).
    renderengine::ProgramBufferData*           spgLastVertexProgram   = nullptr;  // dword_8301095C
    const renderengine::VertexDescriptorData*  spVertexDescriptorLast = nullptr;  // off_83010958
    bool                                       sbVertexProgramDirty   = false;    // byte_83010A34
}

} // namespace CgsGraphics

// The shadow-device program binders BeginRendering / SetProgram drive; the X360 asm passes the bound
// program pointer to each. Bodies live in the shadow-device TU.
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

// The D3D immediate-vertex ring API ImRenderer<V>::Render drives (X360 D3DDevice_* calls); minimal
// external surfaces matching the asm calling convention. Their bodies live in the platform D3D TU.
struct D3DDevice;
extern "C" u32   D3DDevice_InsertFence(D3DDevice* lpDevice);
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType, u32 luVertexCount, u32 luStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<SkidVertex>::AddProgram  @ 0x822870B8
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical shape to the Im3dSkyDome AddProgram -- vertex-type-agnostic.)
// ---------------------------------------------------------------------------------------------------
template <typename V>
s8 ImRenderer<V>::AddProgram(rw::IResourceAllocator* lpAllocator,
                             const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                             const void* lpPixelProgramBinary, u32 luPixelProgramSize)
{
    s8 li8ProgramIndex = 0;
    while (mapVertexProgramBuffer[li8ProgramIndex] != nullptr)
    {
        li8ProgramIndex = static_cast<s8>(li8ProgramIndex + 1);
        if (li8ProgramIndex >= KI8_MAX_PROGRAMS)
        {
            break;
        }
    }

    if (li8ProgramIndex < KI8_MAX_PROGRAMS)
    {
        CGS_ASSERT(mapPixelProgramBuffer[li8ProgramIndex] == nullptr,
                   "mapPixelProgramBuffer[ li8ProgramIndex ] == NULL");
    }

    CGS_ASSERT(li8ProgramIndex < KI8_MAX_PROGRAMS,
               "Adding too many shader programs to the immediate mode renderer");

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);

    // ---- vertex program (muFunction = binary, muReserved8 = size, muShaderType = 0 -> vertex) ----
    renderengine::ProgramBufferParameters lVertexParams;
    lVertexParams.muShaderType        = 0;
    lVertexParams.muMicrocodePart1    = 0;
    lVertexParams.muMicrocodePart3    = 0;
    lVertexParams.muConstantTableSize = 0;
    lVertexParams.muNumVariables      = 0;
    lVertexParams.muFunction          = static_cast<u32>(reinterpret_cast<uintptr_t>(lpVertexProgramBinary));
    lVertexParams.muReserved8         = luVertexProgramSize;

    rw::BaseResourceDescriptors<5> lVertexDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lVertexDescriptor, &lVertexParams);

    renderengine::ProgramResourceLayout lVertexLayout = {};
    lpAllocatorIf->Create(&lVertexLayout, lpAllocatorIf, &lVertexDescriptor, 0);
    mapVertexProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lVertexLayout, &lVertexParams));

    // ---- pixel program (muShaderType = 1 -> pixel) ----------------------------------------------
    renderengine::ProgramBufferParameters lPixelParams;
    lPixelParams.muMicrocodePart1    = 0;
    lPixelParams.muMicrocodePart3    = 0;
    lPixelParams.muConstantTableSize = 0;
    lPixelParams.muNumVariables      = 0;
    lPixelParams.muFunction          = static_cast<u32>(reinterpret_cast<uintptr_t>(lpPixelProgramBinary));
    lPixelParams.muReserved8         = luPixelProgramSize;
    lPixelParams.muShaderType        = 1;

    rw::BaseResourceDescriptors<5> lPixelDescriptor;
    renderengine::ProgramBuffer::GetResourceDescriptor(&lPixelDescriptor, &lPixelParams);

    renderengine::ProgramResourceLayout lPixelLayout = {};
    lpAllocatorIf->Create(&lPixelLayout, lpAllocatorIf, &lPixelDescriptor, 0);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<SkidVertex>::Construct  @ 0x8228E1D8
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's skid-vertex descriptor (TWO elements: a 12-byte position +
// a packed UV/time/alpha attribute at byte-offset 12), clear the program tables, then upload each of
// li8NumberPrograms vertex/pixel program pairs via AddProgram.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::Construct(rw::IResourceAllocator* lpAllocator,
                              const void* const* lapVertexProgramBinary,
                              const u32* lauVertexProgramSize,
                              const void* const* lapPixelProgramBinary,
                              const u32* lauPixelProgramSize,
                              s8 li8NumberPrograms)
{
    // Build the shared render-state library exactly once (module-static guard byte_83010F95).
    static bool sbStateLibraryConstructed = false;
    if (!sbStateLibraryConstructed)
    {
        ConstructOnceOnly(lpAllocator);
        sbStateLibraryConstructed = true;
    }

    // Build the skid-vertex descriptor (the X360 inlines SkidVertex::FillVertexDescriptorParameters).
    // element[0]: 12-byte position (type-word KU_POSITION_ELEMENT_WORD, usageIndex 1).
    // element[1]: packed UV/time/alpha at in-stream byte-offset 12 (type-word KU_UVTIMEALPHA_ELEMENT_WORD,
    // usageIndex 6). The asm stores stream / pad0 / miOffset / usageIndex (element +0/+2/+4/+0x0B); the
    // remaining Element fields are zeroed by Parameters(). usageIndex is the offset-11 byte (NOT the
    // offset-12 mu8Enabled).
    renderengine::VertexDescriptor::Parameters lParameters;
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = KU_UVTIMEALPHA_ELEMENT_OFFSET;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_UVTIMEALPHA_ELEMENT_WORD);
    lParameters.maElements[1].mu8UsageIndex = 6;

    u8 lauDescriptor[48] = {};
    renderengine::VertexDescriptor::GetResourceDescriptor(lauDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    rw::Resource lDescriptorResource = {};
    lpAllocatorIf->Create(&lDescriptorResource, lpAllocatorIf, lauDescriptor, 0);
    mpVertexDescriptor =
        reinterpret_cast<renderengine::VertexDescriptor*>(
            renderengine::VertexDescriptor::Initialize(&lDescriptorResource, &lParameters));

    CGS_ASSERT(li8NumberPrograms <= KI8_MAX_PROGRAMS, "li8NumberPrograms<=KI8_MAX_PROGRAMS");

    // Clear both program tables (the X360 walks the two parallel 8-entry tables in one strided loop).
    mi8CurrentProgram = 0;
    for (s32 liSlot = 0; liSlot < KI8_MAX_PROGRAMS; ++liSlot)
    {
        mapVertexProgramBuffer[liSlot] = nullptr;
        mapPixelProgramBuffer[liSlot]  = nullptr;
    }

    // Upload each supplied program pair.
    for (s32 liProgram = 0; liProgram < li8NumberPrograms; ++liProgram)
    {
        CGS_ASSERT(lapVertexProgramBinary[liProgram] != nullptr,
                   "lapVertexProgramBinary[ li8ProgramIndex ] != NULL");
        CGS_ASSERT(lapPixelProgramBinary[liProgram] != nullptr,
                   "lapPixelProgramBinary[ li8ProgramIndex ] != NULL");
        if (lauVertexProgramSize[liProgram] == 0)
        {
            CGS_ASSERT(lauVertexProgramSize[liProgram] > 0,
                       "lauVertexProgramSize[ li8ProgramIndex ] > 0");
            CGS_ASSERT(lauVertexProgramSize[liProgram] > 0,
                       "lauVertexProgramSize[ li8ProgramIndex ] > 0");
        }

        AddProgram(lpAllocator,
                   lapVertexProgramBinary[liProgram], lauVertexProgramSize[liProgram],
                   lapPixelProgramBinary[liProgram], lauPixelProgramSize[liProgram]);
    }
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<SkidVertex>::BeginRendering  @ 0x8227C1E8
// Begin a skid-trail render pass: assert slot 0's vertex+pixel programs are present and that no
// renderer is currently active, record this renderer as the active one, reset the device shadow cache,
// then bind slot 0's programs and this renderer's vertex descriptor (each through the module shadow
// cache, skipping the bind when unchanged). Structurally identical to the sky-dome BeginRendering.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::BeginRendering()
{
    CGS_ASSERT(mapVertexProgramBuffer[0] != nullptr, "mapVertexProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[0] != nullptr, "mapPixelProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mgpActiveRenderer == nullptr, "mgpActiveRenderer == NULL");

    // X360 stores `this + 4` (the ImRendererBase subobject past this renderer's vptr) == `this`.
    mgpActiveRenderer = static_cast<ImRendererBase*>(this);

    shadow::Device::ResetShadowing();

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[0];
    renderengine::ProgramBuffer* lpPixelProgram  = mapPixelProgramBuffer[0];

    mi8CurrentProgram = 0;

    if (spgLastVertexProgram != reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram))
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram);
    }

    shadow::DeviceSetPixelProgram(lpPixelProgram);

    const renderengine::VertexDescriptorData* lpVertexDescriptor =
        reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor);
    if (spVertexDescriptorLast != lpVertexDescriptor)
    {
        sbVertexProgramDirty   = true;
        spVertexDescriptorLast = lpVertexDescriptor;
    }
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<SkidVertex>::SetProgram  @ 0x827DC2B8
// Bind program slot li8Program's vertex + pixel programs on the device. The vertex program is
// shadow-cached (module-static spgLastVertexProgram == dword_8301095C): if unchanged the bind is
// skipped and false returned; otherwise the vertex program is set, the cache updated, the current slot
// recorded and the pixel program set, returning true.
// ---------------------------------------------------------------------------------------------------
template <typename V>
bool ImRenderer<V>::SetProgram(s8 li8Program)
{
    CGS_ASSERT(mapVertexProgramBuffer[li8Program] != nullptr,
               "mapVertexProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[li8Program] != nullptr,
               "mapPixelProgramBuffer[ li8Program ] != NULL");

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[li8Program];
    bool lbChanged;
    if (spgLastVertexProgram == reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram))
    {
        lbChanged = false;
    }
    else
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram);
        lbChanged = true;
    }

    if (lbChanged)
    {
        mi8CurrentProgram = li8Program;
        shadow::DeviceSetPixelProgram(mapPixelProgramBuffer[li8Program]);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<SkidVertex>::Render  @ 0x8228E068
// Submit luCount skid vertices as a 28-byte-stride immediate vertex run on the GPU ring. Asserts this
// is the active renderer and the vertex pointer is non-null, flushes the shadowed vertex-program
// state, inserts a GPU fence when the batch exceeds 0x80000 bytes, then copies each vertex's SEVEN
// meaningful floats into the ring between D3DDevice_BeginVertices / D3DDevice_EndVertices.
//
// The X360 packs the copy with VMX splats (two lvx128 per vertex + vspltw + stvewx): the first 16-byte
// load supplies pos.x/y/z (source words 0,1,2 -- the Vector3's w-pad word 3 is NOT copied); the second
// 16-byte load (source +16) supplies uv.x/y/z/w (source words 4,5,6,7). The source cursor advances
// sizeof(SkidVertex) == 32 bytes per vertex (addi r11,r11,0x20); the GPU dest stride is 28 bytes.
// Mirroring the committed ImRenderer<Basic2dColouredVertex>::Render de-optimisation, the tiled VMX
// scatter is reconstructed as one straight per-vertex linear copy of the SAME seven words the asm
// stores -- reading source words {0,1,2, 4,5,6,7} (skipping the Vector3 pad), writing seven contiguous
// dwords per 28-byte GPU vertex.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount)
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");

    const u32 luPrimitiveType = static_cast<u32>(lePrimitiveType);

    shadow::Device::FlushVertexProgramState();

    D3DDevice* lpDevice = reinterpret_cast<D3DDevice*>(mgpDevice);

    CGS_ASSERT(lpVertices != nullptr, "lpVertices");

    if ((KU_SKID_VERTEX_STRIDE * luCount) > KU_FENCE_BYTE_THRESHOLD)
    {
        D3DDevice_InsertFence(lpDevice);
    }

    void* lpRing = D3DDevice_BeginVertices(lpDevice, luPrimitiveType, luCount, KU_SKID_VERTEX_STRIDE);

    u32* lpDst = reinterpret_cast<u32*>(lpRing);
    if (lpDst != nullptr)
    {
        const u8* lpSrc = reinterpret_cast<const u8*>(lpVertices);
        for (u32 luVertex = 0; luVertex < luCount; ++luVertex)
        {
            const u32* lpVertexWords = reinterpret_cast<const u32*>(lpSrc + luVertex * sizeof(V));
            lpDst[0] = lpVertexWords[0];   // mv3Pos.x          (float bits, source word 0)
            lpDst[1] = lpVertexWords[1];   // mv3Pos.y          (float bits, source word 1)
            lpDst[2] = lpVertexWords[2];   // mv3Pos.z          (float bits, source word 2)
            lpDst[3] = lpVertexWords[4];   // mv4UvTimeAlpha.x  (float bits, source word 4 -- skips pad word 3)
            lpDst[4] = lpVertexWords[5];   // mv4UvTimeAlpha.y  (float bits, source word 5)
            lpDst[5] = lpVertexWords[6];   // mv4UvTimeAlpha.z  (float bits, source word 6)
            lpDst[6] = lpVertexWords[7];   // mv4UvTimeAlpha.w  (float bits, source word 7)
            lpDst += 7;
        }
    }

    D3DDevice_EndVertices(lpDevice);
}

// Emit the five X360-attested ImRenderer<SkidVertex> member bodies. We instantiate the members
// INDIVIDUALLY rather than `template struct ImRenderer<...>` because the rest of the template's API
// (EndRendering / SetTransform / RenderStart / RenderEnd) is not attested by the X360 ARTIST for this
// TU -- a whole-struct explicit instantiation would force fabricated bodies for them.
template void ImRenderer<BrnGraphics::SkidVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BrnGraphics::SkidVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<BrnGraphics::SkidVertex>::BeginRendering();
template bool ImRenderer<BrnGraphics::SkidVertex>::SetProgram(s8);
template void ImRenderer<BrnGraphics::SkidVertex>::Render(
    renderengine::PrimitiveType, const BrnGraphics::SkidVertex*, u32);

} // namespace CgsGraphics
