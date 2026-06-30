// CgsGraphics::ImRenderer<BasicColouredTexturedVertex> -- the X360 immediate-mode textured 3D
// renderer instantiation. This is the renderer behind CgsGraphics::Im3d (the smoke / spark /
// blobby-shadow / above-car / debug-3D textured paths: Im3dSmokeRenderer::Construct and
// CgsGraphics::Im3d::Construct build it; the particle / shadow / GUI dispatchers drive
// BeginRendering / SetTransform / EndRendering).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<BasicColouredTexturedVertex> member
// bodies):
//   CgsGraphics::ImRenderer<BasicColouredTexturedVertex>::Construct     @ 0x8228D718
//   CgsGraphics::ImRenderer<BasicColouredTexturedVertex>::AddProgram    @ 0x82286A10
//   CgsGraphics::ImRenderer<BasicColouredTexturedVertex>::SetProgram    @ 0x827DBC98
//   CgsGraphics::ImRenderer<BasicColouredTexturedVertex>::EndRendering  @ 0x8227B830
//   CgsGraphics::ImRenderer<BasicColouredTexturedVertex>::SetTransform  @ 0x8227B8A0
//
// This mirrors CgsIm3dZOnly.cpp (ImRenderer<PositionOnlyVertex>): the per-vertex-type member
// bodies are defined out-of-class then the template is instantiated PER MEMBER (NOT a whole-struct
// `template struct ImRenderer<...>`), because the rest of the template's API (BeginRendering /
// Render / RenderStart / RenderEnd) is the PC 2D fold whose generic bodies live in CgsIm2d.cpp and
// is NOT attested by the X360 ARTIST for this TU -- a whole-struct explicit instantiation would
// force fabricated bodies for them (the wave-30 BasicColouredVertex lesson).
//
// The X360 BeginRendering (@0x8227B5E8, takes an s8 program index + drives the shadow-device
// program binders) and Render (@0x824041B8, VMX vertex-pack + Xbox360 D3DDevice_BeginVertices /
// D3DDevice_EndVertices extensions) are attested for this TU but are platform-D3D/VMX bodies whose
// committed ImRenderer<V> template counterparts are the PC fold; their faithful reconstruction
// needs unhomed Xbox360 D3D-extension intrinsics, so they are NOT bodied here (declaration-only,
// not instantiated) rather than fabricated. The ImRenderBuffer<V> command-buffer members
// (Dispatch / Prepare / RenderEnd / SetBufferFullRewindT / SetState / Swap / Hand -- the ones whose
// asserts cite CgsImRenderBuffer.h / CgsIm3dRenderBuffer.h) belong to a DIFFERENT template
// instantiation and are not this ledger key's to home.
//
// The asm is authoritative for every constant: the three vertex-descriptor element words
// (0x2A23B9 / 0x14C86 / 0x2C23A5) and the "is pixel program" flag (1) come straight from the
// immediates -- none are fabricated.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredTexturedVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the program/descriptor resource handles
    // the matching renderengine *::Initialize then turns into the live object. Modelled by name,
    // mirroring CgsImRenderer.cpp / CgsIm3dZOnly.cpp -- the immediate-mode layer only ever reaches
    // the allocator through this one virtual. The X360 call is (*(*a2 + 16))(handlesOut, a2, desc, 0).
    class ResourceAllocator
    {
    public:
        virtual void* Create(
            void* lpResourceHandlesOut,
            ResourceAllocator* lpAllocator,
            const void* lpDescriptor,
            int liFlags) = 0;
    };

    // The three vertex-descriptor element words the X360 Construct stores into the textured stream's
    // descriptor elements (the inlined BasicColouredTexturedVertex::FillVertexDescriptorParameters).
    // They are the raw asm immediates written at each element's +4 dword:
    //   element 0 (position, FLOAT3) -> 0x2A23B9  (lis 0x2A / ori 0x23B9)
    //   element 1 (colour,   UBYTE4N) -> 0x14C86  (lis 1   / ori 0x4C86)
    //   element 2 (texcoord, FLOAT2)  -> 0x2C23A5 (lis 0x2C / ori 0x23A5)
    const u32 KU_POSITION_ELEMENT_WORD = 0x2A23B9u;
    const u32 KU_COLOUR_ELEMENT_WORD   = 0x14C86u;
    const u32 KU_TEXCOORD_ELEMENT_WORD = 0x2C23A5u;
}

} // namespace CgsGraphics

// The renderengine shader-state entry point SetTransform writes the world matrix into. Out-of-scope
// (blocked class:renderengine::Device); declared here (external linkage, defined in its own
// renderengine TU) as the minimal surface so the body links. X360: r3 =
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr). Shared declaration with the
// CgsIm3dZOnly.cpp position-only renderer.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The shadow-device program binders SetProgram drives. Out-of-scope (the X360 shadow::Device home
// is only declaration-modelled in shadowingdevice.h: SetVertexProgramInternal takes no args there
// and the arg-passing form is unhomed). Declared here as the minimal external surface the body
// calls -- the X360 asm passes the bound program pointer to each.
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<BasicColouredTexturedVertex>::AddProgram  @ 0x82286A10
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical body to the position-only renderer's AddProgram -- the per-program
// upload is vertex-type-agnostic.)
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

    renderengine::ProgramResourceLayout lVertexLayout;
    lpAllocatorIf->Create(&lVertexLayout, lpAllocatorIf, &lVertexDescriptor, 0);
    mapVertexProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lVertexLayout, &lVertexParams));

    // ---- pixel program (muShaderType = 1 -> pixel) ----------------------------------------------
    // Same zeroed param words as the vertex case ([1],[4],[6],[8],[9]); muShaderType is set to 1
    // (the X360 stores li 1 into the param block after the GetResourceDescriptor stores).
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

    renderengine::ProgramResourceLayout lPixelLayout;
    lpAllocatorIf->Create(&lPixelLayout, lpAllocatorIf, &lPixelDescriptor, 0);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<BasicColouredTexturedVertex>::Construct  @ 0x8228D718
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's position+colour+UV vertex descriptor, clear the program tables,
// then upload each of li8NumberPrograms vertex/pixel program pairs via AddProgram.
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

    // Build the textured vertex descriptor (the X360 inlines BasicColouredTexturedVertex::
    // FillVertexDescriptorParameters: three elements -- FLOAT3 position, UBYTE4N colour, FLOAT2 UV --
    // whose +4 words are the three KU_*_ELEMENT_WORD asm immediates).
    renderengine::VertexDescriptor::Parameters lParameters;

    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 12;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_COLOUR_ELEMENT_WORD);
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 10;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = 16;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_TEXCOORD_ELEMENT_WORD);
    lParameters.maElements[2].mu8Type       = 0;
    lParameters.maElements[2].mu8Pad1       = 5;
    lParameters.maElements[2].mu8Usage      = 0;
    lParameters.maElements[2].mu8UsageIndex = 6;

    u8 lauDescriptor[144] = {};
    renderengine::VertexDescriptor::GetResourceDescriptor(lauDescriptor, &lParameters);

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
    rw::Resource lDescriptorResource;
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
// ImRenderer<BasicColouredTexturedVertex>::SetProgram  @ 0x827DBC98
// Bind program slot li8Program's vertex + pixel programs on the device. The vertex program is
// shadow-cached (module-static dword_8301095C): if it is unchanged the bind is skipped and false is
// returned; otherwise the vertex program is set, the cache updated, the current slot recorded and the
// pixel program set, returning true.
// ---------------------------------------------------------------------------------------------------
template <typename V>
bool ImRenderer<V>::SetProgram(s8 li8Program)
{
    CGS_ASSERT(mapVertexProgramBuffer[li8Program] != nullptr,
               "mapVertexProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[li8Program] != nullptr,
               "mapPixelProgramBuffer[ li8Program ] != NULL");

    // The live vertex-program shadow cache (X360 dword_8301095C). One per module, not per renderer.
    static renderengine::ProgramBuffer* spgLastVertexProgram = nullptr;

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[li8Program];
    bool lbChanged;
    if (spgLastVertexProgram == lpVertexProgram)
    {
        lbChanged = false;
    }
    else
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = lpVertexProgram;
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
// ImRenderer<BasicColouredTexturedVertex>::EndRendering  @ 0x8227B830
// Assert this renderer is the active one, then clear the active-renderer module static. (The X360
// compares mgpActiveRenderer against `this + 4` -- the ImRendererBase subobject pointer past this
// renderer's vptr; `this` when null stays null.)
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::EndRendering()
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");
    mgpActiveRenderer = nullptr;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<BasicColouredTexturedVertex>::SetTransform  @ 0x8227B8A0
// Copy the supplied 4x4 (4x 16-byte rows) world matrix into this renderer's transform store, then
// fetch the current program slot's device shader-state block (renderengine::Device::BeginShaderStates)
// and write the same matrix into it. Returns the shader-state result pointer (X360 r3).
//
// The X360 stores the matrix to `this + 0x80` (the transform store) and indexes the shader-state
// block at `this + 4*(mi8CurrentProgram + 22)`; here both are reached by name via the transform store
// grown onto the renderer and the per-slot shader-state accessor (the wave-35 fix).
// ---------------------------------------------------------------------------------------------------
template <typename V>
void* ImRenderer<V>::SetTransform(const void* lpTransform)
{
    // Copy the 4x4 world matrix into this renderer's PERSISTENT transform store (X360 stvx128 into
    // `this + 0x80` -- the object member, NOT a stack local), so the side-effect is preserved.
    const u8* lpSrc = reinterpret_cast<const u8*>(lpTransform);
    for (s32 liByte = 0; liByte < 64; ++liByte)
    {
        this->mauTransform[liByte] = lpSrc[liByte];
    }

    // X360 passes `this + 4*(mi8CurrentProgram + 22)` == &maShaderStateBlocks[slot] (the current
    // program's device shader-state block), not plain `this`.
    void* lpShaderState = nullptr;
    void* lpResult =
        RenderEngineDeviceBeginShaderStates(&this->maShaderStateBlocks[this->mi8CurrentProgram], &lpShaderState);

    // Write the matrix into the fetched shader-state block.
    u8* lpDst = reinterpret_cast<u8*>(lpShaderState);
    if (lpDst != nullptr)
    {
        for (s32 liByte = 0; liByte < 64; ++liByte)
        {
            lpDst[liByte] = this->mauTransform[liByte];
        }
    }

    return lpResult;
}

// Emit the five X360-attested ImRenderer<BasicColouredTexturedVertex> member bodies. We instantiate
// the members INDIVIDUALLY rather than `template struct ImRenderer<BasicColouredTexturedVertex>`
// because the rest of the template's API (BeginRendering / Render / RenderStart / RenderEnd) is the
// PC 2D fold whose bodies live in CgsIm2d.cpp and is NOT attested by the X360 ARTIST for this TU --
// a whole-struct explicit instantiation would force fabricated bodies for them (the wave-30 lesson;
// mirrors CgsIm3dZOnly.cpp).
template void ImRenderer<BasicColouredTexturedVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BasicColouredTexturedVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template bool ImRenderer<BasicColouredTexturedVertex>::SetProgram(s8);
template void ImRenderer<BasicColouredTexturedVertex>::EndRendering();
template void* ImRenderer<BasicColouredTexturedVertex>::SetTransform(const void*);

} // namespace CgsGraphics
