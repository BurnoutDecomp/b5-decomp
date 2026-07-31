// CgsGraphics::ImRenderer<BasicColouredVertex> -- the X360 immediate-mode untextured 3D
// (position+colour) renderer instantiation. This is the renderer behind CgsGraphics::Im3dUntex
// (the untextured-quad / progress-bar path: BrnGui::ProgressBarRenderer::RenderComponent /
// RenderQuadUntex drive it via the Im3dUntex hierarchy in CgsIm3d.h).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<BasicColouredVertex> member
// bodies; ledger key `class:CgsGraphics::BasicColouredVertex>`):
//   CgsGraphics::ImRenderer<BasicColouredVertex>::Construct     @ 0x827F8770  (EXECUTED in goal trace)
//   CgsGraphics::ImRenderer<BasicColouredVertex>::AddProgram    @ 0x827EF918  (EXECUTED in goal trace)
//   CgsGraphics::ImRenderer<BasicColouredVertex>::SetProgram    @ 0x827DBD78
//
// This mirrors CgsIm3dZOnly.cpp / CgsIm3d.cpp exactly: the per-vertex-type member bodies are
// defined out-of-class then the members this TU owns are instantiated INDIVIDUALLY (never
// `template struct ImRenderer<V>`, which would force fabricated bodies for the template members
// this TU does not attest -- the wave-30 BasicColouredVertex lesson).
//
// The OTHER `class:CgsGraphics::BasicColouredVertex>` ledger functions (BeginRendering,
// EndRendering, Prepare, Render, SetState, SetTransform, Swap,
// SetBufferFullRewindToLastEnd) are a DIFFERENT template instantiation --
// CgsGraphics::ImRenderBuffer<BasicColouredVertex> (the untextured 3D command/vertex buffer,
// asserts cite ".../ImRenderBuffer/CgsImRenderBuffer.h") -- and are reconstructed in
// CgsImRenderBufferTemplate.cpp, NOT here. Dispatch and HandleComman belong to yet another,
// unhomed type (an Im3dRenderBuffer subclass whose asserts cite CgsIm3dRenderBuffer.h) and are
// left declaration-only (see the header). The ledger's flat function-name grouping conflated
// all three under one TU id; this file only owns the ImRenderer<V> slice.
//
// The asm is authoritative for every constant: the two vertex-descriptor element words
// (0x2A23B9 position, 0x14C86 colour) and the "is pixel program" flag (1) come straight from
// the immediates -- none are fabricated.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasicColouredVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the program/descriptor resource handles
    // the matching renderengine *::Initialize then turns into the live object. Modelled by name,
    // mirroring CgsImRenderer.cpp / CgsIm3dZOnly.cpp / CgsIm3d.cpp -- the immediate-mode layer only
    // ever reaches the allocator through this one virtual. The X360 call is
    // (*(*a2 + 16))(handlesOut, a2, descriptor, 0).
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

    // The two vertex-descriptor element words the X360 Construct stores into the untextured
    // stream's descriptor elements (the inlined BasicColouredVertex::FillVertexDescriptorParameters).
    // They are the raw asm immediates written at each element's +4 dword:
    //   element 0 (position, FLOAT3) -> 0x2A23B9  (lis 0x2A / ori 0x23B9)
    //   element 1 (colour,   UBYTE4N) -> 0x14C86  (lis 1   / ori 0x4C86)
    const u32 KU_POSITION_ELEMENT_WORD = 0x2A23B9u;
    const u32 KU_COLOUR_ELEMENT_WORD   = 0x14C86u;
}

} // namespace CgsGraphics

// The shadow-device program binders SetProgram drives. Out-of-scope (the X360 shadow::Device home
// is only declaration-modelled in shadowingdevice.h: SetVertexProgramInternal takes no args there
// and the arg-passing form is unhomed). Declared here as the minimal external surface the body
// calls -- the X360 asm passes the bound program pointer to each. Shared declaration form with
// CgsIm3dZOnly.cpp / CgsIm3d.cpp / CgsIm2dUntex.cpp.
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<BasicColouredVertex>::AddProgram  @ 0x827EF918
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical body to the other ImRenderer<V> instantiations -- the per-program
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

    renderengine::ProgramResourceLayout lVertexLayout = {};
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

    renderengine::ProgramResourceLayout lPixelLayout = {};
    lpAllocatorIf->Create(&lPixelLayout, lpAllocatorIf, &lPixelDescriptor, 0);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<BasicColouredVertex>::Construct  @ 0x827F8770
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's position+colour vertex descriptor, clear the program tables,
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

    // Build the untextured position+colour vertex descriptor (the X360 inlines BasicColouredVertex::
    // FillVertexDescriptorParameters: two elements -- FLOAT3 position, UBYTE4N colour -- whose +4
    // words are the two KU_*_ELEMENT_WORD asm immediates).
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
// ImRenderer<BasicColouredVertex>::SetProgram  @ 0x827DBD78
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

// Emit the three X360-attested ImRenderer<BasicColouredVertex> member bodies. We instantiate the
// members INDIVIDUALLY rather than `template struct ImRenderer<BasicColouredVertex>` because the rest
// of the template's API (BeginRendering / EndRendering / Render / RenderStart / RenderEnd /
// SetTransform) is NOT attested by the X360 ARTIST for this TU -- a whole-struct explicit
// instantiation would force fabricated bodies for them (the wave-30 lesson; mirrors
// CgsIm3dZOnly.cpp / CgsIm3d.cpp / CgsIm2dUntex.cpp).
template void ImRenderer<BasicColouredVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BasicColouredVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template bool ImRenderer<BasicColouredVertex>::SetProgram(s8);

} // namespace CgsGraphics
