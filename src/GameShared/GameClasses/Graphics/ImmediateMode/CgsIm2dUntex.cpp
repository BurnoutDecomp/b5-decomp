// CgsGraphics::ImRenderer<Basic2dColouredVertex> -- the X360 immediate-mode 2D position+colour
// (UNtextured) renderer instantiation. This is the renderer behind CgsGraphics::Im2dUntex (the
// debug-overlay / solid-2D-quad path: BrnRendererModule::RenderThreeThreadMonitors drives
// BeginRendering / SetTransform / Render).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<Basic2dColouredVertex> member bodies):
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::Construct       @ 0x827F8E08
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::AddProgram      @ 0x827EFDE0
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::BeginRendering  @ 0x823F9640
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::SetProgram      @ 0x827DC018
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::SetTransform    @ 0x823F9740
//   CgsGraphics::ImRenderer<Basic2dColouredVertex>::Render          @ 0x82404858
//
// This mirrors CgsIm3dZOnly.cpp (the ImRenderer<PositionOnlyVertex> precedent) exactly: the per-
// vertex-type member bodies are defined out-of-class then the members this TU owns are instantiated
// INDIVIDUALLY (never `template struct ImRenderer<V>`, which would force fabricated bodies for the
// template members this TU does not attest -- the wave-30 BasicColouredVertex lesson). Unlike the
// position-only precedent, the X360 ARTIST attests BeginRendering and Render for THIS vertex type,
// so they are reconstructed and instantiated here as their X360 (shadow::Device / D3DDevice) bodies
// rather than left to the PC 2D fold in CgsIm2d.cpp.
//
// The asm is authoritative for every constant: the two vertex-descriptor element words (0x2C23A5 and
// 0x14C86), the "is pixel program" flag (1), the 12-byte vertex stride and the 0x80000 fence
// threshold all come straight from the immediates -- none are fabricated.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredVertex.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot the X360 immediate-mode builder dispatches through
    // (vtable +0x10): given the sized descriptor it carves the program/descriptor resource handles
    // the matching renderengine *::Initialize then turns into the live object. Modelled by name here,
    // mirroring CgsIm3dZOnly.cpp -- the immediate-mode layer only ever reaches the allocator through
    // this one virtual. The X360 call is (*(*a2 + 16))(handlesOut, a2, descriptor, 0).
    class ResourceAllocator
    {
    public:
        virtual void* Create(
            void* lpResourceHandlesOut,
            ResourceAllocator* lpAllocator,
            const void* lpDescriptor,
            int liFlags) = 0;
    };

    // The two vertex-descriptor element words the X360 Construct stores (the inlined
    // Basic2dColouredVertex::FillVertexDescriptorParameters): element[0]'s +4 dword is the raw asm
    // immediate 0x2C23A5 (lis r11,0x2C / ori r11,0x23A5) and element[1]'s +4 dword is 0x14C86
    // (lis r11,1 / ori r11,0x4C86). Both are the GPU vertex-format element descriptors -- neither is
    // fabricated; they are the literal immediates from 0x827F8E58 / 0x827F8EA0.
    const s32 KI_POSITION_ELEMENT_WORD = 0x2C23A5;  // element[0] (position stream)
    const s32 KI_COLOUR_ELEMENT_WORD   = 0x14C86;   // element[1] (packed colour stream)

    // ImRenderer<V>::Render submits this vertex with a 12-byte stride (li r6, 0xC) and inserts a GPU
    // fence whenever the batch byte-size (12 * count) exceeds this threshold (lis r10,8 -> 0x80000).
    const u32 KU_VERTEX_STRIDE      = 12u;
    const u32 KU_FENCE_BYTE_THRESHOLD = 0x80000u;
}
} // namespace CgsGraphics

// The renderengine shader-state entry point SetTransform writes the world matrix into. Out-of-scope
// (blocked class:renderengine::Device); declared here (external linkage, defined in its own
// renderengine TU) as the minimal surface so the body links. X360: r3 =
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

// The shadow-device program binders SetProgram / BeginRendering drive. Out-of-scope (the X360
// shadow::Device home only declaration-models SetVertexProgramInternal/SetPixelProgram). Declared
// here as the minimal external surface the bodies call -- the X360 asm passes the bound program
// pointer to each. DeviceSetVertexDescriptor models the inlined vertex-descriptor shadow store
// BeginRendering performs (X360 compares off_83010958 / sets the dirty byte_83010A34 in line).
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
    void DeviceSetVertexDescriptor(void* lpVertexDescriptor);
}

// The X360 D3D immediate-vertex submission intrinsics Render drives (the GPU-DMA ring "begin/end
// vertices" path). Out-of-scope platform intrinsics with no PC equivalent; declared here as the
// minimal external surface so the body links. The device pointer is ImRendererBase::mgpDevice
// (X360 off_83271608).
struct D3DDevice;
extern "C" u32   D3DDevice_InsertFence(D3DDevice* lpDevice);
extern "C" void* D3DDevice_BeginVertices(D3DDevice* lpDevice, u32 luPrimitiveType,
                                         u32 luVertexCount, u32 luVertexStreamZeroStride);
extern "C" void  D3DDevice_EndVertices(D3DDevice* lpDevice);

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Basic2dColouredVertex>::AddProgram  @ 0x827EFDE0
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical shape to the position-only precedent's AddProgram.)
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
    // / muNumVariables (param words [1],[4],[6],[8],[9]); the inline-microcode words are left untouched.
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
// ImRenderer<Basic2dColouredVertex>::Construct  @ 0x827F8E08
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's TWO-element (position + packed colour) vertex descriptor, clear
// the program tables, then upload each of li8NumberPrograms vertex/pixel program pairs via AddProgram.
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

    // Build the position+colour vertex descriptor. The X360 inlines Basic2dColouredVertex::
    // FillVertexDescriptorParameters: two elements whose +4 words are the literal asm immediates.
    renderengine::VertexDescriptor::Parameters lParameters;
    // element[0] -- position stream (var_1A0..var_195).
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = KI_POSITION_ELEMENT_WORD;
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;
    // element[1] -- packed-colour stream (var_190..var_185).
    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 8;
    lParameters.maElements[1].miOffset      = KI_COLOUR_ELEMENT_WORD;
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 0xA;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 4;

    u8 lauDescriptor[48] = {};
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
// ImRenderer<Basic2dColouredVertex>::BeginRendering  @ 0x823F9640
// Mark this the active renderer, reset the shadow cache, then bind program slot 0's vertex+pixel
// programs (vertex program shadow-cached) and the renderer's vertex descriptor (descriptor shadow-
// cached). Asserts slot 0's programs are present and that no renderer was already active.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::BeginRendering()
{
    CGS_ASSERT(mapVertexProgramBuffer[0] != nullptr, "mapVertexProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[0]  != nullptr, "mapPixelProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mgpActiveRenderer == nullptr, "mgpActiveRenderer == NULL");

    // X360 stores `this + 4` (the ImRendererBase subobject pointer past this renderer's vptr).
    mgpActiveRenderer = static_cast<ImRendererBase*>(this);

    shadow::Device::ResetShadowing();

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[0];
    renderengine::ProgramBuffer* lpPixelProgram  = mapPixelProgramBuffer[0];

    // Reset the current program slot (X360 stb 0 into +0x54).
    mi8CurrentProgram = 0;

    // The live vertex-program shadow cache (X360 dword_8301095C). One per module, not per renderer.
    static renderengine::ProgramBuffer* spgLastVertexProgram = nullptr;
    if (spgLastVertexProgram != lpVertexProgram)
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = lpVertexProgram;
    }

    shadow::DeviceSetPixelProgram(lpPixelProgram);

    // Shadow-cache the renderer's vertex descriptor (X360 off_83010958): when it differs from the
    // bound one, mark the vertex-program state dirty (byte_83010A34 = 1) and store the new descriptor.
    static renderengine::VertexDescriptor* spgLastVertexDescriptor = nullptr;
    if (spgLastVertexDescriptor != mpVertexDescriptor)
    {
        shadow::DeviceSetVertexDescriptor(mpVertexDescriptor);
        spgLastVertexDescriptor = mpVertexDescriptor;
    }
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Basic2dColouredVertex>::SetProgram  @ 0x827DC018
// Bind program slot li8Program's vertex + pixel programs on the device. The vertex program is
// shadow-cached (module-static dword_8301095C): if it is unchanged the bind is skipped and false is
// returned; otherwise the vertex program is set, the cache updated, the current slot recorded and the
// pixel program set, returning true. (Identical shape to the position-only precedent's SetProgram.)
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
// ImRenderer<Basic2dColouredVertex>::SetTransform  @ 0x823F9740
// Copy the supplied 4x4 (4x 16-byte rows) world matrix into this renderer's transform store, then for
// each of the four matrix rows fetch the current program slot's device shader-state block
// (renderengine::Device::BeginShaderStates) and write the row into it, advancing the shader-state
// write cursor by 16 bytes per row. Returns the last shader-state result pointer (X360 r3).
//
// The X360 stores the matrix to `this + 0xE0` (mauTransform) and indexes the shader-state blocks at
// `this + 4*(mi8CurrentProgram + 22)` for the first row, then +30/+38/+46 for the next three rows.
// Here the store target is mauTransform and the blocks are reached by name via maShaderStateBlocks.
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
    // threading ONE accumulating write cursor (var_40, +=16/row). This reconstruction reaches the block
    // BY NAME via &maShaderStateBlocks[slot] per row (the LLP64 non-invariant-offset convention) and
    // re-fetches the cursor per row -- the persistent mauTransform store (the real side-effect) is
    // faithful; the per-row block stride + cursor accumulation re-derive when the Device block layout lands.
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

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Basic2dColouredVertex>::Render  @ 0x82404858
// Submit luCount vertices as a 12-byte-stride immediate vertex run on the GPU ring. Asserts this is
// the active renderer and the vertex pointer is non-null, inserts a GPU fence when the batch exceeds
// 0x80000 bytes, then copies each vertex (two position floats + one packed colour dword) into the
// ring between D3DDevice_BeginVertices / D3DDevice_EndVertices. The X360 unrolls the copy four
// vertices at a time then mops up the remainder; the de-optimised form is one straight per-vertex copy.
// ---------------------------------------------------------------------------------------------------
template <typename V>
void ImRenderer<V>::Render(renderengine::PrimitiveType lePrimitiveType, const V* lpVertices, u32 luCount)
{
    CGS_ASSERT(mgpActiveRenderer == static_cast<ImRendererBase*>(this), "mgpActiveRenderer == this");

    // The X360 stows the primitive type into a renderer scratch dword (+0x04) and reads it straight
    // back as the BeginVertices PrimitiveType; the round-trip is a no-op here, so it is passed directly.
    const u32 luPrimitiveType = static_cast<u32>(lePrimitiveType);

    shadow::Device::FlushVertexProgramState();

    D3DDevice* lpDevice = reinterpret_cast<D3DDevice*>(mgpDevice);

    CGS_ASSERT(lpVertices != nullptr, "lpVertices");

    if ((KU_VERTEX_STRIDE * luCount) > KU_FENCE_BYTE_THRESHOLD)
    {
        D3DDevice_InsertFence(lpDevice);
    }

    // BeginVertices hands back the ring write cursor; each vertex is three dwords: the two position
    // floats (copied verbatim) then the packed colour word.
    void* lpRing = D3DDevice_BeginVertices(lpDevice, luPrimitiveType, luCount, KU_VERTEX_STRIDE);

    u32* lpDst = reinterpret_cast<u32*>(lpRing);
    if (lpDst != nullptr)
    {
        const u8* lpSrc = reinterpret_cast<const u8*>(lpVertices);
        for (u32 luVertex = 0; luVertex < luCount; ++luVertex)
        {
            const u32* lpVertexWords = reinterpret_cast<const u32*>(lpSrc + luVertex * KU_VERTEX_STRIDE);
            lpDst[0] = lpVertexWords[0];   // mv2Pos.x (float bits)
            lpDst[1] = lpVertexWords[1];   // mv2Pos.y (float bits)
            lpDst[2] = lpVertexWords[2];   // mv4Colour (packed dword)
            lpDst += 3;
        }
    }

    D3DDevice_EndVertices(lpDevice);
}

// Emit the six X360-attested ImRenderer<Basic2dColouredVertex> member bodies (the Im2dUntex path).
// We instantiate the members INDIVIDUALLY rather than `template struct ImRenderer<Basic2dColouredVertex>`
// because RenderStart / RenderEnd / EndRendering are not attested by the X360 ARTIST for this TU (a
// whole-struct explicit instantiation would force fabricated bodies for them). This is the wave-30
// BasicColouredVertex lesson: instantiate only what this TU owns.
template void ImRenderer<Basic2dColouredVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<Basic2dColouredVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<Basic2dColouredVertex>::BeginRendering();
template bool ImRenderer<Basic2dColouredVertex>::SetProgram(s8);
template void* ImRenderer<Basic2dColouredVertex>::SetTransform(const void*);
template void ImRenderer<Basic2dColouredVertex>::Render(renderengine::PrimitiveType, const Basic2dColouredVertex*, u32);

} // namespace CgsGraphics
