// CgsGraphics::ImRenderer<BrnGraphics::Im3dSkyDomeVertex> -- the X360 immediate-mode 3D sky-dome
// renderer instantiation. This is the renderer behind the sky-dome draw path: BrnSkyDomeManager::
// Render / RenderToEnvironmentMap drive BeginRendering / EndRendering, and the sky-dome geometry
// submission (sub_823FF1D0) drives SetTransform.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ImRenderer<Im3dSkyDomeVertex> member bodies):
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::AddProgram      @ 0x823FF570
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::BeginRendering  @ 0x823F9938
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::Construct       @ 0x82404CF8
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::EndRendering    @ 0x823F9A38
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::SetProgram      @ 0x827DC0F8
//   CgsGraphics::ImRenderer<Im3dSkyDomeVertex>::SetTransform    @ 0x823FA968
//
// This mirrors CgsIm3dZOnly.cpp (the ImRenderer<PositionOnlyVertex> instantiation): the per-vertex-
// type member bodies are defined out-of-class then the template members are instantiated INDIVIDUALLY
// (not a whole-struct explicit instantiation, which would force fabricated PC-fold bodies -- the
// wave-30 BasicColouredVertex lesson). The X360 program-table members (mpVertexDescriptor / the two
// program tables / current slot / shader-state blocks / transform store) were grown additively onto
// the shared ImRenderer<V> template in CgsImRenderer.h (wave 35).
//
// The asm is authoritative for every constant: the two vertex-descriptor element words (0x2A23B9 and
// 0x2C23A5), the element[1] byte-offset (12) and format/usage codes (5 / 6), and the "is pixel
// program" flag (1) all come straight from the immediates -- none are fabricated.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsIm3dSkyDomeVertex.h"

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

    // The two vertex-descriptor element type-words the X360 Construct stores (the inlined
    // Im3dSkyDomeVertex descriptor fill). element[0] is the 12-byte position attribute, element[1] the
    // 4-byte packed attribute at byte-offset 12. The values are the raw asm immediates
    // (li/ori r11,0x2A,0x23B9 == 0x2A23B9 and li/ori r11,0x2C,0x23A5 == 0x2C23A5); none are fabricated.
    const u32 KU_POSITION_ELEMENT_WORD = 0x2A23B9u;
    const u32 KU_PACKED_ELEMENT_WORD   = 0x2C23A5u;

    // The element[1] in-stream byte offset (the asm immediate 0xC stored at the element's +2 word):
    // the second attribute starts 12 bytes into the vertex (after the position).
    const u16 KU_PACKED_ELEMENT_OFFSET = 12u;

    // ---- per-module shadow caches (X360 .data block off_83010950) -----------------------------------
    // These mirror the live device bindings the immediate-mode renderer compares against; they are one
    // set per module (NOT per renderer), shared by BeginRendering and SetProgram in this TU. They
    // correspond to dword_8301095C (last vertex program), off_83010958 (last vertex descriptor) and
    // byte_83010A34 (the vertex-program-state dirty flag).
    renderengine::ProgramBufferData*           spgLastVertexProgram   = nullptr;  // dword_8301095C
    const renderengine::VertexDescriptorData*  spVertexDescriptorLast = nullptr;  // off_83010958
    bool                                       sbVertexProgramDirty   = false;    // byte_83010A34
}

} // namespace CgsGraphics

// The shadow-device program binders BeginRendering / SetProgram drive. The committed shadow::Device
// models SetVertexProgramInternal() with NO args and SetPixelProgram(ProgramBufferData*), but the
// X360 asm here passes the bound program pointer to each. These minimal external surfaces match the
// asm calling convention (mirroring CgsIm3dZOnly.cpp); their bodies live in the shadow-device TU.
namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

// The renderengine shader-state entry point SetTransform writes the world matrix into. Out-of-scope
// (blocked class:renderengine::Device); declared here (external linkage, defined in its own
// renderengine TU) as the minimal surface so the body links. X360: r3 =
// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr).
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

namespace CgsGraphics
{

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Im3dSkyDomeVertex>::AddProgram  @ 0x823FF570
// Find the first empty vertex-program slot, upload the supplied vertex + pixel program binaries into
// it (sizing each via ProgramBuffer::GetResourceDescriptor -> allocator Create -> Initialize), and
// return the slot index. Asserts the chosen slot's pixel entry is empty and that we did not run past
// the program table. (Identical shape to the Im3dZOnly AddProgram -- same param-word stores.)
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

    renderengine::ProgramResourceLayout lPixelLayout;
    lpAllocatorIf->Create(&lPixelLayout, lpAllocatorIf, &lPixelDescriptor, 0);
    mapPixelProgramBuffer[li8ProgramIndex] =
        reinterpret_cast<renderengine::ProgramBuffer*>(
            renderengine::ProgramBuffer::Initialize(&lPixelLayout, &lPixelParams));

    return li8ProgramIndex;
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Im3dSkyDomeVertex>::Construct  @ 0x82404CF8
// One-time-construct the shared render-state library (ImRendererBase::ConstructOnceOnly, guarded by a
// module flag), build this renderer's sky-dome vertex descriptor (TWO elements: a 12-byte position +
// a packed attribute at byte-offset 12), clear the program tables, then upload each of
// li8NumberPrograms vertex/pixel program pairs via AddProgram.
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

    // Build the sky-dome vertex descriptor (the X360 inlines the descriptor fill). element[0] is the
    // 12-byte position (type-word KU_POSITION_ELEMENT_WORD, usageIndex 1); element[1] is a packed
    // attribute at in-stream byte-offset 12 (type-word KU_PACKED_ELEMENT_WORD, format/usage 5 / 6).
    renderengine::VertexDescriptor::Parameters lParameters;
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8Type       = 0;
    lParameters.maElements[0].mu8Pad1       = 0;
    lParameters.maElements[0].mu8Usage      = 0;
    lParameters.maElements[0].mu8UsageIndex = 1;

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = KU_PACKED_ELEMENT_OFFSET;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_PACKED_ELEMENT_WORD);
    lParameters.maElements[1].mu8Type       = 0;
    lParameters.maElements[1].mu8Pad1       = 5;
    lParameters.maElements[1].mu8Usage      = 0;
    lParameters.maElements[1].mu8UsageIndex = 6;

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
// ImRenderer<Im3dSkyDomeVertex>::BeginRendering  @ 0x823F9938
// Begin a sky-dome render pass: assert slot 0's vertex+pixel programs are present and that no renderer
// is currently active, record this renderer as the active one, reset the device shadow cache, then
// bind slot 0's programs and this renderer's vertex descriptor (each through the module shadow cache,
// skipping the bind when unchanged). Returns the SetPixelProgram result (X360 r3).
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

    // Shadow this renderer's vertex descriptor; mark the vertex-program state dirty when it changed.
    const renderengine::VertexDescriptorData* lpVertexDescriptor =
        reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor);
    if (spVertexDescriptorLast != lpVertexDescriptor)
    {
        sbVertexProgramDirty   = true;
        spVertexDescriptorLast = lpVertexDescriptor;
    }
}

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Im3dSkyDomeVertex>::SetProgram  @ 0x827DC0F8
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

// ---------------------------------------------------------------------------------------------------
// ImRenderer<Im3dSkyDomeVertex>::EndRendering  @ 0x823F9A38
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
// ImRenderer<Im3dSkyDomeVertex>::SetTransform  @ 0x823FA968
// Copy the supplied 4x4 (4x 16-byte rows) world matrix into this renderer's transform store, then
// fetch the current program slot's device shader-state block (renderengine::Device::BeginShaderStates)
// and write the same matrix into it. Returns the shader-state result pointer (X360 r3).
//
// The X360 stores the matrix to `this + 0x80` (the renderer's transform store) and indexes the
// shader-state block at `this + 4*(mi8CurrentProgram + 22)` == &maShaderStateBlocks[slot]; here both
// are reached by name via the grown-on transform store and the per-slot shader-state accessor.
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
    void* lpResult = RenderEngineDeviceBeginShaderStates(&this->maShaderStateBlocks[this->mi8CurrentProgram], &lpShaderState);

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

// Emit the six X360-attested ImRenderer<Im3dSkyDomeVertex> member bodies (the sky-dome path). We
// instantiate the members INDIVIDUALLY rather than `template struct ImRenderer<...>` because the rest
// of the template's API (Render / RenderStart / RenderEnd) is the PC 2D fold whose bodies live in
// CgsIm2d.cpp and is NOT attested by the X360 ARTIST for this TU -- a whole-struct explicit
// instantiation would force fabricated bodies for them. (The wave-30 BasicColouredVertex lesson:
// instantiate only what this TU owns.)
template void ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template s8 ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::BeginRendering();
template bool ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::SetProgram(s8);
template void ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::EndRendering();
template void* ImRenderer<BrnGraphics::Im3dSkyDomeVertex>::SetTransform(const void*);

} // namespace CgsGraphics
