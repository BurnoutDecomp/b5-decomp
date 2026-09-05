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
#include "GameSource/Effects/Particles/Native/BrnLionBlendIm3d.h"   // BrnGraphics::Im3dBlend

#include <cstring>   // memcpy -- the staged shader-constant copies (console lvx128/stvx128)
#include <cstdio>    // [diag] snprintf (the eight-handle resolve line in Construct)

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "pc/gcm/renderengine/VertexDescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

// The four re-authored PC program images (pc/gcm/renderengine/LionBlendProgramsPC.cpp -- the PC
// stand-in for the four guest Xenos microcode blobs Im3dBlend::Construct hands to
// ImRenderer<LionBlendVertex>::Construct; see that file's recipe). Declared the same way
// BrnIm3dSkidsRenderer.cpp declares the skid pair.
namespace renderengine
{
    extern const u8  gauLionBlendVertexProgramPC[];
    extern const u32 guLionBlendVertexProgramPCSize;
    extern const u8  gauLionBlendPixelProgramPC[];
    extern const u32 guLionBlendPixelProgramPCSize;
    extern const u8  gauLionBlendZFadeVertexProgramPC[];
    extern const u32 guLionBlendZFadeVertexProgramPCSize;
    extern const u8  gauLionBlendZFadePixelProgramPC[];
    extern const u32 guLionBlendZFadePixelProgramPCSize;
}

// renderengine::Device::BeginShaderStates(shaderStateBlock, &outPtr) -- the same minimal extern
// surface BrnIm3dSkidsRenderer.cpp / BrnIm3d.cpp / BrnPostFxBloom.cpp declare (defined in
// ImmediateModePCLeaf.cpp). Returns the staged row the caller copies the constant into.
void* RenderEngineDeviceBeginShaderStates(void* lpShaderStateBlock, void** lppShaderStateOut);

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

    // ---- [PC-platform leaf] adopt a pre-built PC ShaderProgramBuffer image ----------------------
    // ⭐⭐ THE SAME MISSING ARM THE SKID PATH HAD, AND MEASURED THE SAME WAY. The console route
    // below (GetResourceDescriptor -> allocator Create -> Initialize) cannot run on the PC
    // backend: both renderengine::ProgramBuffer bodies call XGGetMicrocodeShaderParts, whose PC
    // stub returns 0 WITHOUT writing *lpParts, then read that uninitialised
    // ProgramMicrocodeParts for the microcode size and hand a 64-bit function pointer truncated
    // into the u32 muFunction to Xbox2CreateConstantTable. CgsIm3dSkyDome.cpp and
    // BrnSkidVertex.cpp both carry this adopt path; this TU was left on the console route.
    //
    // MEASURED, on the first run that reached the new Im3dBlend::Construct: the descriptor built
    // fine ("[ImLeaf] vertex descriptor built: elements=3 stride0=36 decl=ok") but NO
    // "[ImLeaf] adopted PC program buffer" line followed, and Construct reported
    //     [lionblend] p0{wvp=0 cs=0} p1{wvp=0 cs=0 off=0 scl=0 dconv=0 dfade=0}
    // -- all eight handles with mu8RegisterCount == 0. The PC leaf's BeginShaderStates routes a
    // zero-count handle to a DISCARD row, so worldViewProj and colourScale would never have been
    // uploaded and every particle would have been transformed by whatever c0..c3 the previous
    // draw left behind. LionBlendProgramsPC.cpp's four images are not at fault -- their
    // tables declare 2 / 1 / 4 / 4 variables. Nothing was adopting them.
    //
    // A non-PC binary returns null here and falls through to the console path unchanged.
    if (renderengine::ProgramBufferData* lpAdoptedVertex =
            renderengine::ProgramBufferPC_Adopt(lpVertexProgramBinary, luVertexProgramSize, 0u))
    {
        renderengine::ProgramBufferData* const lpAdoptedPixel =
            renderengine::ProgramBufferPC_Adopt(lpPixelProgramBinary, luPixelProgramSize, 1u);
        if (lpAdoptedPixel != nullptr)
        {
            mapVertexProgramBuffer[li8ProgramIndex] =
                reinterpret_cast<renderengine::ProgramBuffer*>(lpAdoptedVertex);
            mapPixelProgramBuffer[li8ProgramIndex] =
                reinterpret_cast<renderengine::ProgramBuffer*>(lpAdoptedPixel);
            return li8ProgramIndex;
        }
    }

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
// element[2] share type-word 0x1A23A6 at in-stream offsets 0 / 20 with ELEMENT TYPES 1 / 6; element[1]
// is type-word 0x14C86 at offset 16 with element type 4), clear the program tables, then upload each of
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

    // Build the Lion-blend vertex descriptor (the X360 inlines the descriptor fill).
    //
    // ⭐⭐ THE ASM STORES EXACTLY FOUR LANES PER ELEMENT, AND THE THREE IT LEAVES ALONE ARE THE
    // POINT. Walk 0x8228E8E0..0x8228E93C: `sth` to element+0 (stream) and element+2 (in-stream
    // offset), `stw` to element+4 (the format word), `stb` to element+0x0B (the element TYPE) --
    // and nothing at all to +0x08/+0x09/+0x0A. Those three are the METHOD / USAGE / USAGE-INDEX
    // lanes, and VertexDescriptor::Parameters::Parameters @0x82276870 seeds them 0 / 0xFF / 0xFF.
    // 0xFF is the "take the default for this element type" sentinel VertexDescriptor::Initialize
    // reads (`lbz r11, 2(r7)` -> gauVertexFormatDefaults[type*2 (+1)]), which is what turns
    // element types 1 / 4 / 6 into POSITION0 / COLOR0 / TEXCOORD0.
    //
    // ⛔ THIS FILE USED TO WRITE 0 INTO ALL THREE. That is not a no-op: it replaces both
    // sentinels with a real value, so every one of the three elements would have declared
    // D3DDECLUSAGE_POSITION index 0 -- three positions, no colour, no uv. It is the same defect
    // ImmediateModePCLeaf.cpp records for the post-fx composite quad ("drew NOTHING with
    // hr == S_OK"), and the working sibling (BrnSkidVertex.cpp's ImRenderer<SkidVertex>::
    // Construct) writes only the four attested lanes. Do not re-add them.
    renderengine::VertexDescriptor::Parameters lParameters;
    lParameters.maElements[0].mu16Stream    = 0;
    lParameters.maElements[0].mu16Pad0      = 0;                                  // in-stream offset 0
    lParameters.maElements[0].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_A);
    lParameters.maElements[0].mu8UsageIndex = 1;                                  // element type 1 -> POSITION0

    lParameters.maElements[1].mu16Stream    = 0;
    lParameters.maElements[1].mu16Pad0      = KU_ELEMENT1_OFFSET;
    lParameters.maElements[1].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_B);
    lParameters.maElements[1].mu8UsageIndex = 4;                                  // element type 4 -> COLOR0

    lParameters.maElements[2].mu16Stream    = 0;
    lParameters.maElements[2].mu16Pad0      = KU_ELEMENT2_OFFSET;
    lParameters.maElements[2].miOffset      = static_cast<s32>(KU_ELEMENT_WORD_A);
    lParameters.maElements[2].mu8UsageIndex = 6;                                  // element type 6 -> TEXCOORD0

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

// =================================================================================================
// BrnGraphics::Im3dBlend::BeginRendering  @ 0x82282060   (DWARF BrnLionBlendIm3d.h:71)
//
// Start a Lion particle batch. Two paths, chosen by the bool8_t in r6:
//   * abZFadeEnable == false -> program slot 0, the plain LionBlended pair. Push only
//     worldViewProj (+0xC0) and colourScale (+0xC4).
//   * abZFadeEnable == true  -> program slot 1, the LionBlendedZFade pair. Bind the scene depth
//     texture on sampler unit 1, then push worldViewProj / colourScale / gOffset / gScale /
//     gDepthConversion / gDepthFadeConstants (+0xC8 .. +0xDC).
// Both paths finish through the SAME tail (loc_8228230C): one more 16-byte constant copy into the
// cursor the last BeginShaderStates handed back. The compiler merged the two tails, which is why
// the pseudocode looks like the colourScale write escapes the else branch -- it does not; each
// path stages its own value into the same stack slot first.
//
// The eight handle offsets this body indexes (+0xC0..+0xDC) are the SECOND independent witness
// for the Im3dBlend layout; the resolve site in Construct @0x8229B260 is the first and
// LionBlendRenderer::SetCameraData @0x822824F8 (which starts writing at +0xE0) is the third.
// See BrnLionBlendIm3d.h.
//
// SIGNATURE RECOVERED FROM THE CALL SITE. Hex-Rays types this `int(int a1 .. int a31)`.
// LionParticleRender::BeginRendering @0x82289568 passes: r3 = &mLionImmediateModeRenderer,
// r4 = &mViewProjection, f1..f6 = the six floats, r6 = the bool8_t, and the TextureState* in the
// stack parameter slot this body reads at `arg_5C`. That is slot 9 of an 8-byte-slot parameter
// area starting at +0x14 (0x14 + 8*9 == 0x5C, low word) -- each f32 argument consumes its GPR
// slot without occupying the register, which is what pushes the ninth argument onto the stack
// while r7..r10 sit unused.
//
// ⭐⭐ AND THE SIX FLOATS NOW HAVE DWARF NAMES, WHICH CORRECTED TWO OF THEM (2026-09-05). The
// same six values are passed straight through, unchanged and in order, by cLionFX::Dispatch
// @0x82912BA8 -> cParticleRender::Dispatch @0x82911E98 -> LionParticleRender::BeginRendering
// @0x82289568 -> here, and the DWARF declares cLionFX::Dispatch (LionFX.h:79) as
//     (vertexBuffer, batchArray, afWhiteLevel, abEnableZFade, afNearPlane, afFarPlane,
//      afDepthFadeDistance, afDepthSamplerOffsetU, afDepthSamplerOffsetV, apDepthTextureState)
// so f5/f6 are the depth sampler's HALF-TEXEL OFFSETS, not viewport half-extents as this file
// called them until now. That matters because it decides what the shader computes: with
// gScale.xy == (0.5, -0.5) and gOffset.xy == (f5 + 0.5, f6 + 0.5), `ndc * gScale + gOffset` is
// the standard D3D ndc -> NORMALISED screen-uv map with the half-texel bias -- a texture
// coordinate, not a pixel coordinate. Under the old names the same expression would have spanned
// one single pixel and read as a shader bug.
//
// It also corroborates the z-fade derivation independently: f2/f3 are the projection's REAL near
// and far planes and f4 is a fade DISTANCE, which is exactly what makes the constant term of
// zFar + (zNear - zFar) * z vanish and leaves (zScene - zParticle) / afDepthFadeDistance. See the
// head of tools/assets/shaders/brn_lionblend.fx for that derivation in full.
//
// EVERY CONSTANT IS READ OUT OF THE IMAGE, none is fabricated:
//     flt_82001CC0 == 00000000 == 0.0      flt_82001C98 == 3F800000 == 1.0
//     flt_82001DA0 == 3F000000 == 0.5      flt_82004C78 == BF000000 == -0.5
//     flt_82011668 == 3F7F0001 == 0.99609381      (255.0/256.0, rounded up in the last bit)
//     flt_82011664 == 3B7F0001 == 0.0038909914    (== the above / 256)
//     flt_82011660 == 377F0001 == 1.5199185e-05   (== the above / 65536)
// The last three are the classic pack-depth-into-RGB triple K * (1, 1/256, 1/65536); the shader
// variable they feed is literally named "gDepthConversion".
// =================================================================================================

namespace BrnGraphics
{
namespace
{
    // The console stages each constant with lvx128/stvx128 straight into the row
    // BeginShaderStates handed back. On the host the staged row is an untyped byte cursor, so
    // the two copies are spelled by size -- 64 bytes for the matrix (four rows), 16 for a
    // Vector4 -- exactly as the asm does.
    void StageShaderConstant(void* lpCursor, const void* lpSource, u32 luBytes)
    {
        if (lpCursor != nullptr)
        {
            memcpy(lpCursor, lpSource, luBytes);
        }
    }
}

// =================================================================================================
// BrnGraphics::Im3dBlend::Construct  @ 0x8229B260   (DWARF BrnLionBlendIm3d.h:58)
//
// Build the two-program Lion-blend immediate-mode renderer and resolve the eight named shader
// constants against it. This is the function that had never run: with it absent, no Lion program
// pair was ever uploaded, so the three draw halves' vertices had nothing to be drawn WITH.
//
// THE SHAPE, straight off the asm:
//   words  3..37  store four 16-byte rows at `this + 0x80` == mCurrentTransform. The four source
//                 addresses are w::math::vpu::detail::gIVector (0x82181500) and the three that
//                 follow it at 0x82181510 / 0x82181520 / 0x82181530; read out of the image they
//                 are (1,0,0,0) / (0,1,0,0) / (0,0,1,0) / (0,0,0,1) -- the identity. Nothing is
//                 inferred: `lis r10, gIVector@ha` + `lvx128 v0, r0, r10` + `stvx128 v0, r0, r11`
//                 with r11 = r31 + 0x80, then the same triple at +0x10 / +0x20 / +0x30.
//   words 13..44  fill FOUR parallel two-entry stack arrays and call
//                 ImRenderer<LionBlendVertex>::Construct(allocator, r5, r6, r7, r8, r9=2):
//                     r5 -> var_C0 { unk_8200DD58, unk_8200E010 }   vertex binaries
//                     r6 -> var_A0 { 0x1A4,        0x220        }   vertex sizes
//                     r7 -> var_80 { unk_8200DF00, unk_8200E230 }   pixel binaries
//                     r8 -> var_60 { 0x10C,        0x1F8        }   pixel sizes
//                 (the three `addi r27/r26/r25, r10, unk_8200XXXX - unk_8200E230` are pointer
//                 arithmetic off the ONE `lis/addi` of unk_8200E230 the compiler materialised.)
//                 Slot 0 is "LionBlended", slot 1 "LionBlendedZFade" -- which is which is fixed
//                 by BeginRendering @0x82282060: its ZFade arm binds slot 1 and pushes gOffset /
//                 gScale / gDepthConversion / gDepthFadeConstants, and only the SECOND pair's
//                 constant tables carry those four names.
//   words 45..112 the eight GetVariableHandleByName resolves, each preceded by the console's own
//                 CgsImRenderer.h:570 / :554 null assert on the program buffer it is about to
//                 read. The X360 re-tests the pointer before every single lookup; hoisted to one
//                 test per program buffer here, exactly as the skid sibling does.
//
// The four PC program images are pc/gcm/renderengine/LionBlendProgramsPC.cpp -- the four guest
// Xenos packages re-authored as D3D9 from their own disassembly. Their constant tables pin the
// same registers the console's do, which is what makes these eight lookups resolve.
// =================================================================================================

void Im3dBlend::Construct(rw::IResourceAllocator* lpAllocator)
{
    // asm words 3..37 -- mCurrentTransform = identity. (mauTransform is the committed
    // ImRenderer<V> alias for Im3dBase<V>::mCurrentTransform at the console's +0x80; see the
    // "ONE CONSOLE WORD, TWO HOST HOMES" flag at the head of BrnLionBlendIm3d.h.)
    static const f32 KAF_IDENTITY[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,   // w::math::vpu::detail::gIVector @0x82181500
        0.0f, 1.0f, 0.0f, 0.0f,   // 0x82181510
        0.0f, 0.0f, 1.0f, 0.0f,   // 0x82181520
        0.0f, 0.0f, 0.0f, 1.0f,   // 0x82181530
    };
    memcpy(mauTransform, KAF_IDENTITY, sizeof(mauTransform));

    // asm words 13..44 -- the two program pairs, in slot order.
    const void* lapVertexProgramBinary[2] =
    {
        renderengine::gauLionBlendVertexProgramPC,       // slot 0  <- unk_8200DD58
        renderengine::gauLionBlendZFadeVertexProgramPC,  // slot 1  <- unk_8200E010
    };
    const u32 lauVertexProgramSize[2] =
    {
        renderengine::guLionBlendVertexProgramPCSize,       // console 0x1A4
        renderengine::guLionBlendZFadeVertexProgramPCSize,  // console 0x220
    };
    const void* lapPixelProgramBinary[2] =
    {
        renderengine::gauLionBlendPixelProgramPC,        // slot 0  <- unk_8200DF00
        renderengine::gauLionBlendZFadePixelProgramPC,   // slot 1  <- unk_8200E230
    };
    const u32 lauPixelProgramSize[2] =
    {
        renderengine::guLionBlendPixelProgramPCSize,        // console 0x10C
        renderengine::guLionBlendZFadePixelProgramPCSize,   // console 0x1F8
    };

    CgsGraphics::ImRenderer<LionBlendVertex>::Construct(
        lpAllocator,
        lapVertexProgramBinary, lauVertexProgramSize,
        lapPixelProgramBinary,  lauPixelProgramSize,
        static_cast<s8>(2));

    // ---- asm words 45..112: the eight named constants ------------------------------------------
    // Program 0's pair is read through mapVertexProgramBuffer[0] (`lwz r11, 0x14(r31)`), program
    // 1's through mapVertexProgramBuffer[1] (`0x18`) and mapPixelProgramBuffer[1] (`0x38`, the
    // pixel table base 0x34 plus one slot). Each handle's member is the `a3` out-parameter of its
    // own call, so name and offset are witnessed by the same instruction pair -- the table at the
    // head of BrnLionBlendIm3d.h.
    const renderengine::ProgramBufferData* const lpVertexProgram0 =
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[0]);
    CGS_ASSERT(lpVertexProgram0 != nullptr, "mapVertexProgramBuffer[ li8Program ] != NULL");
    if (lpVertexProgram0 != nullptr)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram0, reinterpret_cast<const u8*>("worldViewProj"),
            &mViewProjectionMatrixStateHandle_Normal);                        // +0xC0
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram0, reinterpret_cast<const u8*>("colourScale"),
            &mColourScaleStateHandle_Normal);                                 // +0xC4
    }

    const renderengine::ProgramBufferData* const lpVertexProgram1 =
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapVertexProgramBuffer[1]);
    CGS_ASSERT(lpVertexProgram1 != nullptr, "mapVertexProgramBuffer[ li8Program ] != NULL");
    if (lpVertexProgram1 != nullptr)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram1, reinterpret_cast<const u8*>("worldViewProj"),
            &mViewProjectionMatrixStateHandle_ZFade);                         // +0xC8
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram1, reinterpret_cast<const u8*>("colourScale"),
            &mColourScaleStateHandle_ZFade);                                  // +0xCC
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram1, reinterpret_cast<const u8*>("gOffset"),
            &mOffsetStateHandle_ZFade);                                       // +0xD0
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpVertexProgram1, reinterpret_cast<const u8*>("gScale"),
            &mScaleStateHandle_ZFade);                                        // +0xD4
    }

    const renderengine::ProgramBufferData* const lpPixelProgram1 =
        reinterpret_cast<const renderengine::ProgramBufferData*>(mapPixelProgramBuffer[1]);
    CGS_ASSERT(lpPixelProgram1 != nullptr, "mapPixelProgramBuffer[ li8Program ] != NULL");
    if (lpPixelProgram1 != nullptr)
    {
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram1, reinterpret_cast<const u8*>("gDepthConversion"),
            &mDepthConversionStateHandle_ZFade);                              // +0xD8
        renderengine::ProgramBuffer::GetVariableHandleByName(
            lpPixelProgram1, reinterpret_cast<const u8*>("gDepthFadeConstants"),
            &mDepthFadeStateHandle_ZFade);                                    // +0xDC
    }

    // [DIAG] DID THE EIGHT CONSTANTS RESOLVE? mu8RegisterCount == 0 is GetVariableHandleByName's
    // "not found" answer, and the PC leaf's BeginShaderStates routes a zero-count handle to a
    // DISCARD row -- so an unresolved worldViewProj means BeginRendering writes the matrix into a
    // bin and every particle transforms by whatever c0..c3 happen to hold. That is invisible both
    // in the log and in the picture, which is why it is worth one line. Same shape, and the same
    // reason, as the skid renderer's (BrnIm3dSkidsRenderer.cpp). DELETE-WHEN-STABLE.
    {
        char lacMsg[320];
        std::snprintf(lacMsg, sizeof(lacMsg),
            "[lionblend] Im3dBlend::Construct: p0{wvp=%u cs=%u} "
            "p1{wvp=%u cs=%u off=%u scl=%u dconv=%u dfade=%u} (register counts; 0 == UNRESOLVED)\n",
            mViewProjectionMatrixStateHandle_Normal.mu8RegisterCount,
            mColourScaleStateHandle_Normal.mu8RegisterCount,
            mViewProjectionMatrixStateHandle_ZFade.mu8RegisterCount,
            mColourScaleStateHandle_ZFade.mu8RegisterCount,
            mOffsetStateHandle_ZFade.mu8RegisterCount,
            mScaleStateHandle_ZFade.mu8RegisterCount,
            mDepthConversionStateHandle_ZFade.mu8RegisterCount,
            mDepthFadeStateHandle_ZFade.mu8RegisterCount);
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

void Im3dBlend::BeginRendering(const Matrix44& arViewProjection,
                               float32_t afColourScale, bool8_t abZFadeEnable,
                               float32_t afZFadeNear, float32_t afZFadeFar,
                               float32_t afDepthRange,
                               float32_t afDepthSamplerOffsetU, float32_t afDepthSamplerOffsetV,
                               renderengine::TextureState* apDepthTextureState)
{
    // asm words 11-19: drop the device's shadowed state, then shadow this renderer's vertex
    // descriptor (marking the vertex-program state dirty when it changed). The
    // ImRenderer<V>::BeginRendering call below repeats both -- so does the console; not folded.
    shadow::Device::ResetShadowing();
    {
        const renderengine::VertexDescriptorData* lpVertexDescriptor =
            reinterpret_cast<const renderengine::VertexDescriptorData*>(mpVertexDescriptor);
        if (CgsGraphics::spVertexDescriptorLast != lpVertexDescriptor)
        {
            CgsGraphics::sbVertexProgramDirty   = true;
            CgsGraphics::spVertexDescriptorLast = lpVertexDescriptor;
        }
    }

    // The staged shader-constant cursor renderengine::Device::BeginShaderStates advances
    // (X360 var_B0; every call returns the next row through it).
    void* lpConstantCursor = nullptr;

    // The tail constant both paths share (X360 var_60..var_54, copied at loc_8228230C).
    Vector4 lvTailConstant = { 0.0f, 0.0f, 0.0f, 0.0f };

    if (abZFadeEnable != 0)
    {
        // ---- program slot 1: the depth-fading pair -------------------------------------------
        CgsGraphics::ImRenderer<LionBlendVertex>::BeginRendering(static_cast<s8>(1));

        CGS_ASSERT(apDepthTextureState != nullptr, "lpDepthTextureState != NULL");
        // asm words 27-29: bind the scene depth texture on sampler unit 1.
        shadow::Device::SetState(apDepthTextureState, 1);

        // asm words 30-63 -- the four staged constants, in the order the stores appear.
        const f32 lfFadeRange = afZFadeNear - afZFadeFar;                        // fsubs f13

        // "gOffset": the viewport half-extents biased by a half pixel, plus the far plane.
        const Vector4 lvOffset = { afDepthSamplerOffsetU  + 0.5f,                  // flt_82001DA0
                                   afDepthSamplerOffsetV + 0.5f,
                                   afZFadeFar,
                                   0.0f };                                       // flt_82001CC0
        // "gScale": the half-viewport scale (y flipped) and the fade range.
        const Vector4 lvScale = { 0.5f, -0.5f, lfFadeRange, 0.0f };              // 82001DA0/82004C78
        // "gDepthConversion": the pack-depth-into-RGB triple scaled by the fade range, with the
        // far plane carried in w.
        const Vector4 lvDepthConversion = { lfFadeRange * 0.99609381f,
                                            lfFadeRange * 0.0038909914f,
                                            lfFadeRange * 1.5199185e-05f,
                                            afZFadeFar };
        // "gDepthFadeConstants": (near * far) / depthRange in x, zero elsewhere.
        lvTailConstant.x = (afZFadeNear * afZFadeFar) / afDepthRange;
        lvTailConstant.y = 0.0f;
        lvTailConstant.z = 0.0f;
        lvTailConstant.w = 0.0f;

        const Vector4 lvColourScale = { afColourScale, afColourScale, afColourScale, 1.0f };

        // +0xC8 "worldViewProj" -- the whole 64-byte matrix (four lvx128/stvx128 pairs).
        RenderEngineDeviceBeginShaderStates(&mViewProjectionMatrixStateHandle_ZFade,
                                            &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &arViewProjection, sizeof(Matrix44));

        // +0xCC "colourScale".
        RenderEngineDeviceBeginShaderStates(&mColourScaleStateHandle_ZFade, &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &lvColourScale, sizeof(Vector4));

        // +0xD0 "gOffset".
        RenderEngineDeviceBeginShaderStates(&mOffsetStateHandle_ZFade, &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &lvOffset, sizeof(Vector4));

        // +0xD4 "gScale".
        RenderEngineDeviceBeginShaderStates(&mScaleStateHandle_ZFade, &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &lvScale, sizeof(Vector4));

        // +0xD8 "gDepthConversion".
        RenderEngineDeviceBeginShaderStates(&mDepthConversionStateHandle_ZFade, &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &lvDepthConversion, sizeof(Vector4));

        // +0xDC "gDepthFadeConstants" -- the shared tail below copies it.
        RenderEngineDeviceBeginShaderStates(&mDepthFadeStateHandle_ZFade, &lpConstantCursor);
    }
    else
    {
        // ---- program slot 0: the plain pair ---------------------------------------------------
        CgsGraphics::ImRenderer<LionBlendVertex>::BeginRendering(static_cast<s8>(0));

        // +0xC0 "worldViewProj".
        RenderEngineDeviceBeginShaderStates(&mViewProjectionMatrixStateHandle_Normal,
                                            &lpConstantCursor);
        StageShaderConstant(lpConstantCursor, &arViewProjection, sizeof(Matrix44));

        // +0xC4 "colourScale" -- the shared tail below copies it.
        RenderEngineDeviceBeginShaderStates(&mColourScaleStateHandle_Normal, &lpConstantCursor);
        lvTailConstant.x = afColourScale;
        lvTailConstant.y = afColourScale;
        lvTailConstant.z = afColourScale;
        lvTailConstant.w = 1.0f;                                                 // flt_82001C98
    }

    // loc_8228230C -- the merged tail: one 16-byte copy into the row the last BeginShaderStates
    // handed back.
    StageShaderConstant(lpConstantCursor, &lvTailConstant, sizeof(Vector4));
}

}  // namespace BrnGraphics
