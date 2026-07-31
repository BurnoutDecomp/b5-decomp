// CgsGraphics::ImRenderer<BrnGraphics::LionBlendVertex> -- the X360 immediate-mode Lion-blend particle
// renderer instantiation. This is the renderer behind BrnGraphics::Im3dBlend (Im3dBase<LionBlendVertex>
// : ImRenderer<LionBlendVertex>), which BrnGraphics::LionBlendRenderer wraps and BrnParticle::
// LionParticleRender drives. BrnGraphics::Im3dBlend::Construct builds it; Im3dBlend::BeginRendering
// drives BeginRendering(li8Program).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<LionBlendVertex> member bodies):
//   CgsGraphics::ImRenderer<LionBlendVertex>::AddProgram      @ 0x82287250
//   CgsGraphics::ImRenderer<LionBlendVertex>::BeginRendering  @ 0x8227C3A8  (the signed-char overload)
//   CgsGraphics::ImRenderer<LionBlendVertex>::Construct       @ 0x8228E890
//   CgsGraphics::ImRenderer<LionBlendVertex>::SetProgram      @ 0x827DC398
//
// This mirrors CgsIm3dZOnly.cpp / CgsIm3dSkyDome.cpp: the per-vertex-type member bodies are defined
// out-of-class then the template members are instantiated INDIVIDUALLY (not a whole-struct explicit
// instantiation, which would force fabricated PC-fold bodies for Render / RenderStart / RenderEnd --
// the wave-30 BasicColouredVertex lesson). The X360 program-table members (mpVertexDescriptor / the two
// program tables / current slot) live on the shared ImRenderer<V> template in CgsImRenderer.h.
//
// The asm is authoritative for every constant: the three vertex-descriptor element words
// (0x1A23A6 / 0x14C86 / 0x1A23A6), the element[1]/[2] in-stream byte offsets (16 / 20) and usage
// indices (1 / 4 / 6) all come straight from the immediates -- none are fabricated. The offsets
// 0/16/20 also match the DWARF-attested LionBlendVertex layout (position Vector4 @0, RGBA8 @16,
// uv Vector4 @20; GetStride()==36).

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

// The Lion-blend particle vertex format (BrnLionBlendVertex.h). Only its NAME is needed here -- the
// template bodies never take sizeof(V) (the vertex descriptor is built from the asm immediates and the
// program uploads are byte-based), so an incomplete type suffices for the explicit instantiation.
namespace BrnGraphics { struct LionBlendVertex; }

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the program/descriptor resource handles
    // the matching renderengine *::Initialize then turns into the live object. Modelled by name here,
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

    // The three vertex-descriptor element type-words the X360 Construct stores (the inlined
    // LionBlendVertex descriptor fill). element[0] and element[2] share the same type-word (the asm
    // reuses r11 = 0x1A23A6 for both, li/ori r11,0x1A,0x23A6); element[1] carries 0x14C86
    // (li/ori r11,1,0x4C86). None are fabricated.
    const u32 KU_ELEMENT_WORD_A = 0x1A23A6u;
    const u32 KU_ELEMENT_WORD_B = 0x14C86u;

    // The in-stream byte offsets of element[1] and element[2] (asm immediates 0x10 / 0x14 stored at
    // each element's +2 word): the second attribute starts 16 bytes in, the third 20 bytes in.
    const u16 KU_ELEMENT1_OFFSET = 16u;
    const u16 KU_ELEMENT2_OFFSET = 20u;

    // ---- per-module shadow caches (X360 .data block off_83010950) -----------------------------------
    // These mirror the live device bindings the immediate-mode renderer compares against; one set per
    // module (NOT per renderer), shared by BeginRendering and SetProgram in this TU. They correspond to
    // dword_8301095C (last vertex program), off_83010958 (last vertex descriptor) and byte_83010A34
    // (the vertex-program-state dirty flag).
    renderengine::ProgramBufferData*           spgLastVertexProgram   = nullptr;  // dword_8301095C
    const renderengine::VertexDescriptorData*  spVertexDescriptorLast = nullptr;  // off_83010958
    bool                                       sbVertexProgramDirty   = false;    // byte_83010A34
}

} // namespace CgsGraphics

// The shadow-device program binders BeginRendering / SetProgram drive. The committed shadow::Device
// models SetVertexProgramInternal() with NO args and SetPixelProgram(ProgramBufferData*), but the X360
// asm here passes the bound program pointer to each. These minimal external surfaces match the asm
// calling convention (mirroring CgsIm3dSkyDome.cpp); their bodies live in the shadow-device TU.
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<LionBlendVertex>::AddProgram  @ 0x82287250
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical shape to the Im3dZOnly / SkyDome AddProgram -- same param-word stores.)
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
    // The X360 zeroes exactly muShaderType / muMicrocodePart1 / muMicrocodePart3 / muConstantTableSize
    // / muNumVariables (param words [1],[4],[6],[8],[9]); the inline-microcode words [3],[5],[7] are
    // only consumed on the muFunction==0 path (not taken here) and are left untouched by the asm.
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
    // Same zeroed param words as the vertex case ([1],[4],[6],[8],[9]); muShaderType is set to 1
    // (the X360 stores li 1 into v17[1] after the GetResourceDescriptor stores -- the asm order).
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
// ImRenderer<LionBlendVertex>::BeginRendering  @ 0x8227C3A8   (the signed-char overload)
// Begin a Lion-blend render pass on the supplied program slot: assert li8Program is in range and that
// its vertex+pixel programs are present and that no renderer is currently active, record this renderer
// as the active one, reset the device shadow cache, store the slot as current, then bind the slot's
// vertex program (shadow-cached: skip if unchanged) and its pixel program (unconditional), and shadow
// this renderer's vertex descriptor (marking the vertex-program state dirty when it changed).
//
// DWARF types this overload as returning void; the X360 tail returns the SetPixelProgram result in r3,
// but that value is not consumed by the void signature.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::BeginRendering(s8 li8Program)
{
    CGS_ASSERT((li8Program >= 0) && (li8Program < KI8_MAX_PROGRAMS),
               "( li8Program >= 0 ) && ( li8Program < KI8_MAX_PROGRAMS )");
    CGS_ASSERT(mapVertexProgramBuffer[li8Program] != nullptr,
               "mapVertexProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[li8Program] != nullptr,
               "mapPixelProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mgpActiveRenderer == nullptr, "mgpActiveRenderer == NULL");

    // X360 stores `this + 4` (the ImRendererBase subobject past this renderer's vptr) == `this`, or 0
    // when `this` is null.
    mgpActiveRenderer = (this != nullptr) ? static_cast<ImRendererBase*>(this) : nullptr;

    shadow::Device::ResetShadowing();

    // Record the bound slot as current (X360 stb a2 -> this+0x54).
    mi8CurrentProgram = li8Program;

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[li8Program];
    renderengine::ProgramBuffer* lpPixelProgram  = mapPixelProgramBuffer[li8Program];

    if (spgLastVertexProgram != reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram))
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = reinterpret_cast<renderengine::ProgramBufferData*>(lpVertexProgram);
    }

    shadow::DeviceSetPixelProgram(lpPixelProgram);

    // Shadow this renderer's vertex descriptor (X360 reads this+0x10 = mpVertexDescriptor); mark the
    // vertex-program state dirty when it changed.
    const renderengine::VertexDescriptorData* lpVertexDescriptor =
        reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor);
    if (spVertexDescriptorLast != lpVertexDescriptor)
    {
        sbVertexProgramDirty   = true;
        spVertexDescriptorLast = lpVertexDescriptor;
    }
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<LionBlendVertex>::Construct  @ 0x8228E890
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's Lion-blend vertex descriptor (THREE elements: element[0] and
// element[2] share type-word 0x1A23A6 at in-stream offsets 0 / 20 with usage indices 1 / 6; element[1]
// is type-word 0x14C86 at offset 16 with usage index 4), clear the program tables, then upload each of
// li8NumberPrograms vertex/pixel program pairs via AddProgram.
//
// The three element offsets (0 / 16 / 20) match the DWARF-attested LionBlendVertex layout (position
// Vector4 @0, packed RGBA8 @16, uv Vector4 @20; GetStride()==36).
//
// The X360 passes the program binaries / sizes as four parallel arrays addressed off one base (a4);
// here they are the four explicit pointer arrays.
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

    // Build the Lion-blend vertex descriptor (the X360 inlines the descriptor fill). The asm only
    // stores stream/pad0/miOffset/usageIndex per element; type/pad1/usage stay zero from the ctor.
    renderengine::VertexDescriptor::Parameters lParameters;
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_A);
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = KU_ELEMENT1_OFFSET;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_B);
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 0;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = KU_ELEMENT2_OFFSET;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_A);
    lParameters.maElements[2].mu8Type       = 0;
    lParameters.maElements[2].mu8Pad1       = 0;
    lParameters.maElements[2].mu8Usage      = 0;
    lParameters.maElements[2].mu8UsageIndex = 6;

    u8 lauDescriptor[144] = {};
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
// ImRenderer<LionBlendVertex>::SetProgram  @ 0x827DC398
// Bind program slot li8Program's vertex + pixel programs on the device. The vertex program is
// shadow-cached (module-static spgLastVertexProgram == dword_8301095C): if it is unchanged the bind is
// skipped and false is returned; otherwise the vertex program is set, the cache updated, the current
// slot recorded and the pixel program set, returning true.
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

// Emit the four X360-attested ImRenderer<BrnGraphics::LionBlendVertex> member bodies. We instantiate
// the members INDIVIDUALLY rather than `template struct ImRenderer<...>` because the rest of the
// template's API (BeginRendering() no-arg / Render / RenderStart / RenderEnd) is the PC 2D fold whose
// bodies live in CgsIm2d.cpp and is NOT attested by the X360 ARTIST for this TU (the wave-30 lesson).
template s8 ImRenderer<BrnGraphics::LionBlendVertex>::AddProgram(rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<BrnGraphics::LionBlendVertex>::BeginRendering(s8);
template void ImRenderer<BrnGraphics::LionBlendVertex>::Construct(rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template bool ImRenderer<BrnGraphics::LionBlendVertex>::SetProgram(s8);

} // namespace CgsGraphics
