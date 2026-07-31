// CgsGraphics::ImRenderer<BrnGraphics::WorldTexturedVertex> -- the X360 immediate-mode textured +
// lit 3D renderer instantiation (behind BrnGraphics::Im3dTexPlusLighting;
// BrnParticle::Native::BrnDebrisRenderer::BeginRender drives BeginRendering).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AddProgram @ 0x82286F20   Construct @ 0x8228DE30   BeginRendering @ 0x8227C090   SetProgram @ 0x827DC1D8
// Mirrors CgsIm3dSkyDome.cpp / CgsIm3dZOnly.cpp: per-member out-of-class defs + per-member explicit
// instantiation (NOT whole-struct). Element words 0x1A23A6/0x2A23B9/0x2C23A5 and the pixel flag 1 are
// raw asm immediates.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsWorldTexturedVertex.h"

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

    // Raw asm immediates written at each descriptor element's +4 dword.
    const u32 KU_POSITION_ELEMENT_WORD = 0x1A23A6u; // element 0 (pos + transform index) @ off 0
    const u32 KU_NORMAL_ELEMENT_WORD   = 0x2A23B9u; // element 1 (normal)                @ off 16
    const u32 KU_TEXCOORD_ELEMENT_WORD = 0x2C23A5u; // element 2 (UVs)                   @ off 28

    // Per-module shadow caches (X360 .data block off_83010950), shared by BeginRendering + SetProgram.
    renderengine::ProgramBuffer* spgLastVertexProgram      = nullptr; // dword_8301095C
    const void*                  spgLastVertexDescriptor   = nullptr; // off_83010958
    bool                         sbVertexProgramStateDirty = false;   // byte_83010A34
}
} // namespace CgsGraphics

namespace shadow
{
    void DeviceSetVertexProgramInternal(void* lpVertexProgram);
    void DeviceSetPixelProgram(void* lpPixelProgram);
}

namespace CgsGraphics
{

// AddProgram @ 0x82286F20 -- vertex-type-agnostic (identical to committed siblings).
template <typename V>
s8 ImRenderer<V>::AddProgram(rw::IResourceAllocator* lpAllocator,
                             const void* lpVertexProgramBinary, u32 luVertexProgramSize,
                             const void* lpPixelProgramBinary, u32 luPixelProgramSize)
{
    s8 li8ProgramIndex = 0;
    while (mapVertexProgramBuffer[li8ProgramIndex] != nullptr)
    {
        li8ProgramIndex = static_cast<s8>(li8ProgramIndex + 1);
        if (li8ProgramIndex >= KI8_MAX_PROGRAMS) { break; }
    }

    if (li8ProgramIndex < KI8_MAX_PROGRAMS)
    {
        CGS_ASSERT(mapPixelProgramBuffer[li8ProgramIndex] == nullptr,
                   "mapPixelProgramBuffer[ li8ProgramIndex ] == NULL");
    }
    CGS_ASSERT(li8ProgramIndex < KI8_MAX_PROGRAMS,
               "Adding too many shader programs to the immediate mode renderer");

    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);

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

// Construct @ 0x8228DE30
template <typename V>
void ImRenderer<V>::Construct(rw::IResourceAllocator* lpAllocator,
                              const void* const* lapVertexProgramBinary,
                              const u32* lauVertexProgramSize,
                              const void* const* lapPixelProgramBinary,
                              const u32* lauPixelProgramSize,
                              s8 li8NumberPrograms)
{
    static bool sbStateLibraryConstructed = false;
    if (!sbStateLibraryConstructed)
    {
        ConstructOnceOnly(lpAllocator);
        sbStateLibraryConstructed = true;
    }

    renderengine::VertexDescriptor::Parameters lParameters;
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_POSITION_ELEMENT_WORD);
    lParameters.maElements[0].mu8UsageIndex = 1;
    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = 16;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_NORMAL_ELEMENT_WORD);
    lParameters.maElements[1].mu8UsageIndex = 3;
    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = 28;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_TEXCOORD_ELEMENT_WORD);
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

    mi8CurrentProgram = 0;
    for (s32 liSlot = 0; liSlot < KI8_MAX_PROGRAMS; ++liSlot)
    {
        mapVertexProgramBuffer[liSlot] = nullptr;
        mapPixelProgramBuffer[liSlot]  = nullptr;
    }

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

// BeginRendering @ 0x8227C090 (parameterless; mirrors CgsIm3dSkyDome.cpp::BeginRendering)
template <typename V>
void ImRenderer<V>::BeginRendering()
{
    CGS_ASSERT(mapVertexProgramBuffer[0] != nullptr, "mapVertexProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[0] != nullptr, "mapPixelProgramBuffer[ 0 ] != NULL");
    CGS_ASSERT(mgpActiveRenderer == nullptr, "mgpActiveRenderer == NULL");

    mgpActiveRenderer = static_cast<ImRendererBase*>(this);
    shadow::Device::ResetShadowing();

    renderengine::ProgramBuffer* lpVertexProgram = mapVertexProgramBuffer[0];
    renderengine::ProgramBuffer* lpPixelProgram  = mapPixelProgramBuffer[0];
    mi8CurrentProgram = 0;

    if (spgLastVertexProgram != lpVertexProgram)
    {
        shadow::DeviceSetVertexProgramInternal(lpVertexProgram);
        spgLastVertexProgram = lpVertexProgram;
    }
    shadow::DeviceSetPixelProgram(lpPixelProgram);

    const void* lpVertexDescriptor = mpVertexDescriptor;
    if (spgLastVertexDescriptor != lpVertexDescriptor)
    {
        sbVertexProgramStateDirty = true;
        spgLastVertexDescriptor   = lpVertexDescriptor;
    }
}

// SetProgram @ 0x827DC1D8
template <typename V>
bool ImRenderer<V>::SetProgram(s8 li8Program)
{
    CGS_ASSERT(mapVertexProgramBuffer[li8Program] != nullptr,
               "mapVertexProgramBuffer[ li8Program ] != NULL");
    CGS_ASSERT(mapPixelProgramBuffer[li8Program] != nullptr,
               "mapPixelProgramBuffer[ li8Program ] != NULL");

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

template s8 ImRenderer<BrnGraphics::WorldTexturedVertex>::AddProgram(
    rw::IResourceAllocator*, const void*, u32, const void*, u32);
template void ImRenderer<BrnGraphics::WorldTexturedVertex>::Construct(
    rw::IResourceAllocator*, const void* const*, const u32*, const void* const*, const u32*, s8);
template void ImRenderer<BrnGraphics::WorldTexturedVertex>::BeginRendering();
template bool ImRenderer<BrnGraphics::WorldTexturedVertex>::SetProgram(s8);

} // namespace CgsGraphics
