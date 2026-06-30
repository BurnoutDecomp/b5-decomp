// CgsGraphics::ImRenderer<Basic2dColouredTexturedVertex> -- the X360 immediate-mode 2D
// position+colour+UV (TEXTURED) renderer instantiation. This is the renderer behind
// CgsGraphics::Im2d (the loading-screen / GUI / HUD / Apt textured-2D path: CgsGraphics::Im2d::
// Construct builds it; the GUI / loading-screen / renderer-module dispatchers drive
// BeginRendering / SetTransform / Render / EndRendering).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<Basic2dColouredTexturedVertex> member
// bodies the committed CgsIm2d.cpp PC fold does NOT already body):
//   CgsGraphics::ImRenderer<Basic2dColouredTexturedVertex>::Construct     @ 0x827F8BA8
//   CgsGraphics::ImRenderer<Basic2dColouredTexturedVertex>::AddProgram    @ 0x827EFC48
//   CgsGraphics::ImRenderer<Basic2dColouredTexturedVertex>::SetTransform  @ 0x823AC048
//
// This mirrors CgsIm2dUntex.cpp (ImRenderer<Basic2dColouredVertex>) and CgsIm3d.cpp
// (ImRenderer<BasicColouredTexturedVertex>): the per-vertex-type member bodies are defined
// out-of-class then the members THIS TU owns are instantiated INDIVIDUALLY (per-member explicit
// instantiation on the committed ImRenderer<V>), NEVER a fresh whole-struct instantiation -- that
// would force fabricated bodies for the template members this TU does not attest (the wave-30
// BasicColouredVertex lesson). The committed CgsIm2d.cpp already PC-folds and whole-struct-
// instantiates this vertex type's BeginRendering / EndRendering / Render / RenderStart / RenderEnd;
// those are NOT re-homed here. The X360 BeginRendering (@0x823ABED8) / EndRendering (@0x823ABFD8) /
// Render (@0x823C7030) are attested for this TU but their committed ImRenderer<V> counterparts are
// the PC fold (CgsIm2d.cpp), so they are NOT re-bodied here.
//
// The eight ImRenderBuffer<V> command-buffer members the postmortem also lists for this symbol
// (Prepare / RenderFromStaticVer / SetBufferFullRewin / SetClear / SetProgram / SetState /
// SetTexture / Swap -- the ones whose asserts cite CgsImRenderBuffer.h / CgsImRenderBuffer.h) belong
// to a DIFFERENT template instantiation (ImRenderBuffer<Basic2dColouredTexturedVertex>) and are NOT
// this ledger key's to home (the brief: the key is ImRenderer<V>, not ImRenderBuffer<V>).
//
// The asm is authoritative for every constant: the three vertex-descriptor element words
// (0x2C23A5 / 0x14C86 / 0x2C23A5) and the "is pixel program" flag (1) come straight from the
// immediates -- none are fabricated. The 2D position element word (0x2C23A5, a FLOAT2) is the only
// descriptor difference from the 3D textured renderer (CgsIm3d.cpp uses 0x2A23B9, a FLOAT3, for its
// position element); colour (0x14C86) and texcoord (0x2C23A5) words match.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"

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
    // mirroring CgsIm3d.cpp / CgsIm2dUntex.cpp -- the immediate-mode layer only ever reaches the
    // allocator through this one virtual. The X360 call is (*(*a2 + 16))(handlesOut, a2, desc, 0).
    class ResourceAllocator
    {
    public:
        virtual void* Create(
            void* lpResourceHandlesOut,
            ResourceAllocator* lpAllocator,
            const void* lpDescriptor,
            int liFlags) = 0;
    };

    // The three vertex-descriptor element words the X360 Construct stores into the textured 2D
    // stream's descriptor elements (the inlined Basic2dColouredTexturedVertex::
    // FillVertexDescriptorParameters). They are the raw asm immediates written at each element's +4
    // dword (lis/ori pairs at 0x827F8Cxx):
    //   element 0 (position, FLOAT2) -> 0x2C23A5  (lis 0x2C / ori 0x23A5)
    //   element 1 (colour,   UBYTE4N) -> 0x14C86  (lis 1    / ori 0x4C86)
    //   element 2 (texcoord, FLOAT2)  -> 0x2C23A5 (lis 0x2C / ori 0x23A5)
    const u32 KU_POSITION_ELEMENT_WORD = 0x2C23A5u;
    const u32 KU_COLOUR_ELEMENT_WORD   = 0x14C86u;
    const u32 KU_TEXCOORD_ELEMENT_WORD = 0x2C23A5u;
}

} // namespace CgsGraphics

// The renderengine shader-state entry point SetTransform writes the world matrix into. Out-of-scope
// (blocked class:renderengine::Device); declared here (external linkage, defined in its own
// renderengine TU) as the minimal surface so the body links. X360: r3 =
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr). Shared declaration with the
// CgsIm3d.cpp / CgsIm2dUntex.cpp renderers.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Basic2dColouredTexturedVertex>::AddProgram  @ 0x827EFC48
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical body to the other immediate-mode renderers' AddProgram -- the
// per-program upload is vertex-type-agnostic.)
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
// ImRenderer<Basic2dColouredTexturedVertex>::Construct  @ 0x827F8BA8
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's THREE-element (position + packed colour + UV) vertex descriptor,
// clear the program tables, then upload each of li8NumberPrograms vertex/pixel program pairs via
// AddProgram.
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

    // Build the position+colour+UV vertex descriptor (the X360 inlines Basic2dColouredTexturedVertex::
    // FillVertexDescriptorParameters: three elements -- FLOAT2 position, UBYTE4N colour, FLOAT2 UV --
    // whose +4 words are the three KU_*_ELEMENT_WORD asm immediates and whose mu16Pad0 byte offsets
    // (0 / 8 / 12) track the 2D vertex's tighter layout: pos is 8 bytes, colour at +8, UV at +12).
    renderengine::VertexDescriptor::Parameters lParameters;

    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 8;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_COLOUR_ELEMENT_WORD);
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 10;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = 12;
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
// ImRenderer<Basic2dColouredTexturedVertex>::SetTransform  @ 0x823AC048
// Copy the supplied 4x4 (4x 16-byte rows) world matrix into this renderer's transform store, then for
// each of the four matrix rows fetch the current program slot's device shader-state block
// (renderengine::Device::BeginShaderStates) and write the row into it, advancing the shader-state
// write cursor by 16 bytes per row. Returns the last shader-state result pointer (X360 r3).
//
// The X360 stores the matrix to `this + 0xE0` (mauTransform) via four stvx128 and indexes the
// shader-state blocks at `this + 4*(mi8CurrentProgram + 22)` for the first row, then +30/+38/+46 for
// the next three rows. Here the store target is mauTransform and the blocks are reached by name via
// maShaderStateBlocks (the LLP64 non-invariant-offset convention). Mirrors CgsIm2dUntex.cpp exactly.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void* ImRenderer<V>::SetTransform(const void* lpTransform)
{
    // Copy the 4x4 world matrix into this renderer's PERSISTENT transform store (X360 stvx128 of the
    // four rows into the object member, NOT a stack local), so the side-effect is preserved.
    const u8* lpSrc = reinterpret_cast<const u8*>(lpTransform);
    for (s32 liByte = 0; liByte < 64; ++liByte)
    {
        this->mauTransform[liByte] = lpSrc[liByte];
    }

    // The X360 walks one shader-state block per matrix row: BeginShaderStates returns a write cursor
    // (v24[0]); the row is written there, then the cursor is advanced 16 bytes for the next row.
    // FLAG (renderengine::Device out-of-scope/blocked; BeginShaderStates is a decl-only stub here):
    // the asm dispatches the FOUR rows to four STRIDED blocks at 4*(slot+22/30/38/46) (8 words apart)
    // threading ONE accumulating write cursor (v24[0], +=16/row). This reconstruction reaches the block
    // BY NAME via &maShaderStateBlocks[slot] per row and re-fetches the cursor per row -- the persistent
    // mauTransform store (the real side-effect) is faithful; the per-row block stride + cursor
    // accumulation re-derive when the Device block layout lands.
    void* lpResult = nullptr;
    for (s32 liRow = 0; liRow < 4; ++liRow)
    {
        void* lpShaderState = nullptr;
        lpResult = RenderEngineDeviceBeginShaderStates(
            &this->maShaderStateBlocks[this->mi8CurrentProgram], &lpShaderState);

        u8* lpDst = reinterpret_cast<u8*>(lpShaderState);
        if (lpDst != nullptr)
        {
            const u8* lpRow = &this->mauTransform[liRow * 16];
            for (s32 liByte = 0; liByte < 16; ++liByte)
            {
                lpDst[liByte] = lpRow[liByte];
            }
        }
    }

    return lpResult;
}

// Emit the three X360-attested ImRenderer<Basic2dColouredTexturedVertex> member bodies the committed
// CgsIm2d.cpp PC fold does not already body (Construct / AddProgram / SetTransform). We instantiate
// the members INDIVIDUALLY rather than `template struct ImRenderer<Basic2dColouredTexturedVertex>`
// because the committed CgsIm2d.cpp already PC-folds and whole-struct-instantiates this type's
// BeginRendering / EndRendering / Render / RenderStart / RenderEnd -- a fresh whole-struct
// instantiation here would re-home those committed bodies (and force a fabricated SetProgram, which
// for this vertex type is the ImRenderBuffer command-buffer member, not an ImRenderer one). This is
// the wave-30 / wave-37 lesson: instantiate only the members this TU owns.
template void ImRenderer<Basic2dColouredTexturedVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<Basic2dColouredTexturedVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void* ImRenderer<Basic2dColouredTexturedVertex>::SetTransform(const void*);

} // namespace CgsGraphics
