#include "types.hpp"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaRenderer::Initialize @ 0x822850F8
//
// CoronaRenderer::Initialize is the one-time corona render-pass setup the corona manager runs
// during BrnCoronaManager::Construct (rwgcoronabuffer.h). It builds three render resources from
// the supplied rw resource allocator and parks each created handle in a file-scope global the
// later corona draw path reads:
//   * a vertex ProgramBuffer  (shader function &gCoronaVertexShader, shaderType 1)  -> gpCoronaVertexProgram
//   * a pixel  ProgramBuffer  (shader function &gCoronaPixelShader,  shaderType 0)  -> gpCoronaPixelProgram,
//       then three named shader-constant handles are looked up out of it
//       (viewProjectionMatrix / cameraPositionPlusBrightness / viewXyScale)
//   * a VertexDescriptor describing the four-element corona vertex stream         -> gpCoronaVertexDecl
//
// For each resource the pattern is identical and follows the X360 asm exactly:
//   1. <Type>::GetResourceDescriptor(localDescriptor, params)   -> a rw::BaseResourceDescriptors<5>
//   2. allocator->Allocate(handleSlots, allocator, descriptor, 0) (vtable +0x10) -> fills a 5-pointer
//      handle array (the allocator hands back one handle per descriptor slot)
//   3. <Type>::Initialize(handleSlots, params)                  -> builds the object in place
//
// The ProgramBuffer / VertexDescriptor surfaces are committed elsewhere (states/programbuffer.cpp and
// pc/gcm/renderengine/VertexDescriptor.{h,cpp}); per the established vendor-TU convention this file
// re-declares only the minimal slice of each it calls, with the layout/signatures the X360 corona
// asm uses (asm overrides DWARF). The descriptor type is rw::BaseResourceDescriptors<5> (40 bytes,
// the X360 five-entry form) -- see rw/rwcore_structs.h.
//
// FLAG (committed-type inconsistency, NOT applied here): the committed renderengine::VertexDescriptor
// has two divergent reconstructions of its Parameters/Element record -- pc/gcm/renderengine's
// VertexDescriptor.cpp parses a 16-byte element record (matching this X360 corona TU), while
// renderstates.h's VertexDescriptor::Parameters models a 12-byte Element. The X360 ARTIST.XEX corona
// asm builds a 16-byte-stride Parameters block (elements at +0x00/+0x10/+0x20/+0x30, the RenderWare
// format word at element+0x04), so the local Parameters slice below matches the 16-byte form. The
// 12-byte renderstates.h Parameters should be reconciled to the 16-byte X360 layout by GROWING it,
// not retyped here.

namespace renderengine
{
    // ---- rw::BaseResourceDescriptors<5> is the descriptor table type (rw/rwcore_structs.h). The
    //      corona renderer only ever passes it through to the allocator, so it is named via a raw
    //      40-byte slot here to avoid pulling the rwcore template in; the dependency bodies that
    //      actually populate/consume it are committed (states/programbuffer.cpp, VertexDescriptor.cpp).
    struct ResourceDescriptorTable5
    {
        u32 mauWords[10];   // five {size, alignment} entries == rw::BaseResourceDescriptors<5>
    };

    // ---- The rw resource allocator the corona manager hands to Initialize. The X360 call is
    //      (*(*a1 + 0x10))(out, this, descriptor, 0): a virtual at byte offset 0x10 that allocates one
    //      resource handle per descriptor slot, writing a five-pointer handle array into `out`. Only
    //      that one entry point is modelled (the rest of the vtable is opaque to this TU).
    class ResourceAllocator
    {
    public:
        virtual void  Reserved00() = 0;          // vtable +0x00
        virtual void  Reserved04() = 0;          // vtable +0x04
        virtual void  Reserved08() = 0;          // vtable +0x08
        virtual void  Reserved0C() = 0;          // vtable +0x0C
        // vtable +0x10: allocate resource handles for `lpDescriptor`, writing the created handle(s)
        // into `lpHandlesOut`. `luFlags` is 0 at every corona call site.
        virtual void* Allocate(void* lpHandlesOut, ResourceAllocator* lpThis,
                               const ResourceDescriptorTable5* lpDescriptor, int luFlags) = 0;
    };

    // ---- renderengine::ProgramBuffer (committed home: states/programbuffer.cpp). Minimal slice.
    //      ProgramBufferParameters here is the corona-built unpack block: a compiled-shader function
    //      pointer at +0x00, a vertex/pixel shader-type flag at +0x04 and a size/blob word at +0x08
    //      (the X360 stores 228 / 788 there for the two shaders), the rest zero. GetResourceDescriptor
    //      takes the muFunction != 0 path (XGGetMicrocodeShaderParts), so only muFunction is read at
    //      sizing time; Initialize consumes the full block.
    struct ProgramBufferParameters
    {
        u32 muFunction;    // +0x00 compiled-shader function pointer
        u32 muShaderType;  // +0x04 1 -> pixel, 0 -> vertex (copied to the runtime object)
        u32 muBlobSize;    // +0x08 microcode/constant-table blob size for this shader
        u32 muReserved0C;  // +0x0C
        u32 muReserved10;  // +0x10
        u32 muReserved14;  // +0x14
        u32 muReserved18;  // +0x18
        u32 muReserved1C;  // +0x1C
        u32 muReserved20;  // +0x20
        u32 muReserved24;  // +0x24
    };

    // A built program object handle; the X360 keeps a 5-pointer (20-byte) handle array per resource.
    struct ProgramBufferData;

    // The named-constant lookup result (states/programbuffer.cpp ProgramVariableHandle, 4 bytes).
    struct ProgramVariableHandle
    {
        u8 mu8RegisterSet;
        u8 mu8RegisterIndex;
        u8 mu8ShaderType;
        u8 mu8RegisterCount;
    };

    class ProgramBuffer
    {
    public:
        static ResourceDescriptorTable5* GetResourceDescriptor(ResourceDescriptorTable5* lpDescriptor,
                                                               const ProgramBufferParameters* lpParams);
        static ProgramBufferData* Initialize(ProgramBufferData** lpHandles,
                                             const ProgramBufferParameters* lpParams);
        static u32 GetVariableHandleByName(ProgramBufferData* lpData, const u8* lpName,
                                           ProgramVariableHandle* lpHandle);
    };

    // ---- renderengine::VertexDescriptor (committed home: pc/gcm/renderengine/VertexDescriptor.{h,cpp}).
    //      Minimal slice matching the X360 corona usage (16-byte element record, stride 0x10).
    class VertexDescriptor
    {
    public:
        // The format-parameter block the corona renderer fills: 16 element records, 16 bytes each.
        // GetResourceDescriptor/Initialize read each record's RenderWare format word at +0x04 and treat
        // -1 there as an empty slot (see VertexDescriptor.cpp). Defaulted to all-empty by the ctor.
        class Parameters
        {
        public:
            // X360 0x82B635xx-region ctor: clears the 16-record table to the empty sentinel.
            Parameters();

            struct Element
            {
                u16 mu16Stream;      // +0x00 stream index
                u16 mu16Pad02;       // +0x02
                u32 muFormat;        // +0x04 RenderWare element format/type word (-1 == empty)
                u8  mu8Method;       // +0x08
                u8  mu8Usage;        // +0x09
                u8  mu8UsageIndex;   // +0x0A
                u8  mu8Enabled;      // +0x0B (the byte the corona asm stb's at element+0x0B)
                u32 muOffset;        // +0x0C byte offset within the stream
            };

            Element maElements[16];
        };

        static ResourceDescriptorTable5* GetResourceDescriptor(ResourceDescriptorTable5* lpDescriptorOut,
                                                               const Parameters* lpParameters);
        static ProgramBufferData* Initialize(ProgramBufferData** lpHandles, const Parameters* lpParameters);
    };

    static_assert(sizeof(VertexDescriptor::Parameters::Element) == 16,
                  "corona VertexDescriptor::Parameters::Element is the 16-byte X360 record");
    static_assert(sizeof(ProgramBufferParameters) == 40, "ProgramBufferParameters is 10 words on X360");
    static_assert(sizeof(ResourceDescriptorTable5) == 40, "rw::BaseResourceDescriptors<5> is 40 bytes");

    // The one-time corona render-pass setup (committed declaration: coronas/rwgcoronabuffer.h).
    class CoronaRenderer
    {
    public:
        static void Initialize(void* pResourceAllocator);
    };
}

namespace
{
    // The corona shader function blobs in the X360 image (&unk_8200F1B8 = vertex, &unk_8200F2A0 =
    // pixel). HONEST PLACEHOLDER: compiled-microcode data the renderengine links against; declared as
    // opaque externs so the parameter blocks can name them. Their addresses are the muFunction values.
    extern const u8 gCoronaVertexShader;   // X360 &unk_8200F1B8
    extern const u8 gCoronaPixelShader;    // X360 &unk_8200F2A0
    const u8 gCoronaVertexShader = 0;
    const u8 gCoronaPixelShader  = 0;

    // The named shader constants the pixel program exposes (X360 string literals at the call sites).
    const u8 gszViewProjectionMatrix[]         = "viewProjectionMatrix";
    const u8 gszCameraPositionPlusBrightness[] = "cameraPositionPlusBrightness";
    const u8 gszViewXyScale[]                  = "viewXyScale";

    // File-scope globals the later corona draw path reads (X360 dword_82FAB6B0/B4/AC and the handle
    // arrays at dword_82FAB850 / dword_82FAC1CC / dword_82FAC1B8). The built objects live in these
    // five-pointer handle slots; the *Program / *Decl pointers are the Initialize return values.
    renderengine::ProgramBufferData* gapCoronaVertexProgramHandles[5];  // X360 dword_82FAB850
    renderengine::ProgramBufferData* gapCoronaPixelProgramHandles[5];   // X360 dword_82FAC1CC
    renderengine::ProgramBufferData* gapCoronaVertexDeclHandles[5];     // X360 dword_82FAC1B8

    renderengine::ProgramBufferData* gpCoronaVertexProgram;             // X360 dword_82FAB6B0
    renderengine::ProgramBufferData* gpCoronaPixelProgram;              // X360 dword_82FAB6B4
    renderengine::ProgramBufferData* gpCoronaVertexDecl;                // X360 dword_82FAB6AC

    renderengine::ProgramVariableHandle gCoronaViewProjectionHandle;            // X360 byte_82FAB61C
    renderengine::ProgramVariableHandle gCoronaCameraPositionPlusBrightness;    // X360 byte_82FAB620
    renderengine::ProgramVariableHandle gCoronaViewXyScaleHandle;               // X360 byte_82FAB618
}

namespace renderengine
{
    // X360 0x822850F8.
    void CoronaRenderer::Initialize(void* pResourceAllocator)
    {
        ResourceAllocator* lpAllocator = static_cast<ResourceAllocator*>(pResourceAllocator);
        ResourceDescriptorTable5 lDescriptor;
        ProgramBufferData* lapHandles[5];

        // --- vertex program -------------------------------------------------------------------------
        ProgramBufferParameters lVertexParams = {};
        lVertexParams.muFunction   = static_cast<u32>(reinterpret_cast<usize>(&gCoronaVertexShader));  // &unk_8200F1B8
        lVertexParams.muShaderType = 1;                                            // var_21C = 1
        lVertexParams.muBlobSize   = 228;                                          // var_218 = 0xE4

        ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lVertexParams);
        lpAllocator->Allocate(lapHandles, lpAllocator, &lDescriptor, 0);
        std::memcpy(gapCoronaVertexProgramHandles, lapHandles, sizeof(lapHandles));  // copy 5 words
        gpCoronaVertexProgram = ProgramBuffer::Initialize(gapCoronaVertexProgramHandles, &lVertexParams);

        // --- pixel program --------------------------------------------------------------------------
        ProgramBufferParameters lPixelParams = {};
        lPixelParams.muFunction   = static_cast<u32>(reinterpret_cast<usize>(&gCoronaPixelShader));   // &unk_8200F2A0
        lPixelParams.muShaderType = 0;                                            // var_21C = 0
        lPixelParams.muBlobSize   = 788;                                          // var_218 = 0x314

        ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lPixelParams);
        lpAllocator->Allocate(lapHandles, lpAllocator, &lDescriptor, 0);
        std::memcpy(gapCoronaPixelProgramHandles, lapHandles, sizeof(lapHandles));  // copy 5 words
        gpCoronaPixelProgram = ProgramBuffer::Initialize(gapCoronaPixelProgramHandles, &lPixelParams);

        // Pull the three named shader-constant handles out of the pixel program.
        ProgramBuffer::GetVariableHandleByName(gpCoronaPixelProgram, gszViewProjectionMatrix,
                                               &gCoronaViewProjectionHandle);
        ProgramBuffer::GetVariableHandleByName(gpCoronaPixelProgram, gszCameraPositionPlusBrightness,
                                               &gCoronaCameraPositionPlusBrightness);
        ProgramBuffer::GetVariableHandleByName(gpCoronaPixelProgram, gszViewXyScale,
                                               &gCoronaViewXyScaleHandle);

        // --- vertex declaration ---------------------------------------------------------------------
        VertexDescriptor::Parameters lVertexDecl;  // ctor seeds the 16-record table

        // The corona vertex stream: four elements on stream 0 (X360 stores at var_160 +0x00/+0x10/
        // +0x20/+0x30). The RenderWare format word lands at element+0x04; the enable byte is the value
        // the asm stb's at element+0x0B-from-the-record-base on X360 (mu8Enabled now sits at +0x0B).
        lVertexDecl.maElements[0].mu16Stream  = 0;
        lVertexDecl.maElements[0].muFormat    = 0x1A23A6;   // 0x1A<<16 | 0x23A6
        lVertexDecl.maElements[0].mu8Enabled  = 1;

        lVertexDecl.maElements[1].mu16Stream  = 0;
        lVertexDecl.maElements[1].muFormat    = 0x2A23B9;   // 0x2A<<16 | 0x23B9
        lVertexDecl.maElements[1].mu8Enabled  = 3;

        lVertexDecl.maElements[2].mu16Stream  = 0;
        lVertexDecl.maElements[2].muFormat    = 0x1A23A6;
        lVertexDecl.maElements[2].mu8Enabled  = 6;

        lVertexDecl.maElements[3].mu16Stream  = 0;
        lVertexDecl.maElements[3].muFormat    = 0x14C86;    // 0x1<<16 | 0x4C86
        lVertexDecl.maElements[3].mu8Enabled  = 4;

        VertexDescriptor::GetResourceDescriptor(&lDescriptor, &lVertexDecl);
        lpAllocator->Allocate(lapHandles, lpAllocator, &lDescriptor, 0);
        std::memcpy(gapCoronaVertexDeclHandles, lapHandles, sizeof(lapHandles));  // copy 5 words
        gpCoronaVertexDecl = VertexDescriptor::Initialize(gapCoronaVertexDeclHandles, &lVertexDecl);
    }
}
