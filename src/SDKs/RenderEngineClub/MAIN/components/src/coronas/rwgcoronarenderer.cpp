#include "types.hpp"

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::CoronaRenderer::Initialize @ 0x822850F8
//
// CoronaRenderer::Initialize is the one-time corona render-pass setup the corona manager runs
// during BrnCoronaManager::Construct (rwgcoronabuffer.h). It builds three render resources from
// the supplied rw resource allocator and parks each created handle in a file-scope global the
// later corona draw path reads:
//   * a PIXEL  ProgramBuffer  (shader function &gCoronaPixelShader,  shaderType 1)  -> gpCoronaPixelProgram
//   * a VERTEX ProgramBuffer  (shader function &gCoronaVertexShader, shaderType 0)  -> gpCoronaVertexProgram,
//       then three named shader-constant handles are looked up out of THE VERTEX PROGRAM
//       (viewProjectionMatrix / cameraPositionPlusBrightness / viewXyScale)
//   * a VertexDescriptor describing the four-element corona vertex stream         -> gpCoronaVertexDecl
//
// *** VERTEX/PIXEL WERE INVERTED IN THE FIRST RECONSTRUCTION -- CORRECTED 2026-08-17 (carlights
// step 1, group `coronas`). Three independent proofs, all pointing the same way:
//   (a) programbuffer.h:73 `u32 muShaderType;  // +0x00 != 0 -> pixel, == 0 -> vertex` (the real
//       committed ProgramBufferData home), and pc/gcm ProgramBufferPC_Adopt's third argument uses
//       the same encoding (CgsIm3dSkyDome.cpp:159/162 pass 0u for the vertex program, 1u for the
//       pixel one). So the FIRST program built here (shaderType 1) is the PIXEL program.
//   (b) renderengine::CoronaRenderer::Begin<shadow::Device> @0x823FF2C0 -- the only consumer of
//       these globals -- does `shadow::Device::SetPixelProgram(dword_82FAB6B0)` and
//       `shadow::Device::SetVertexProgramInternal(dword_82FAB6B4)`. dword_82FAB6B0 is the FIRST
//       program's Initialize result, dword_82FAB6B4 the SECOND's.
//   (c) all three GetVariableHandleByName calls @0x822851xx take dword_82FAB6B4 (the SECOND,
//       vertex program) as their program -- which is where viewProjectionMatrix /
//       cameraPositionPlusBrightness / viewXyScale belong: Begin publishes them from a
//       RenderParameters block (view-projection matrix, camera position + brightness, view xy
//       scale), i.e. the billboard-expansion inputs of a VERTEX shader.
// A reader of the previous naming would have bound the 228-byte pixel program as the vertex
// program. Names/handle-array names/comments corrected; the STORES are unchanged (the asm's own
// order is preserved) and no behaviour moved. ***
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
//
// =============================================================================================
// [FLAG BLOCKED -- THIS TU CANNOT BE MOUNTED] (carlights step 1, group `coronas`, 2026-08-17)
// Recorded here so the next wave does not discover it at link time. THREE separate blockers:
//
// 1. SIX PHANTOM EXTERNALS. The local ProgramBuffer / VertexDescriptor slices below do NOT have
//    the mangled names of the committed homes, so mounting this file is six guaranteed LNK2019.
//    Measured (cl /c + dumpbin /SYMBOLS on the unmodified file, 2026-08-17) -- local slice vs the
//    real committed declaration:
//      ProgramBuffer::GetResourceDescriptor(ResourceDescriptorTable5*, const ProgramBufferParameters*)
//        REAL: states/programbuffer.h:107  (rw::BaseResourceDescriptors<5>*, const ProgramBufferParameters*)
//      ProgramBuffer::Initialize(ProgramBufferData**, const ProgramBufferParameters*)
//        REAL: states/programbuffer.h:111  (ProgramResourceLayout*, const ProgramBufferParameters*)
//      ProgramBuffer::GetVariableHandleByName(ProgramBufferData*, const u8*, ProgramVariableHandle*)
//        REAL: states/programbuffer.h:109  (const ProgramBufferData*, const u8*, ProgramVariableHandle*)
//      VertexDescriptor::Parameters::Parameters()
//        REAL: pc/gcm/renderengine/VertexDescriptor.h:78 (same spelling -- resolves once the local
//              class is deleted and the real header is included)
//      VertexDescriptor::GetResourceDescriptor(ResourceDescriptorTable5*, const Parameters*)
//        REAL: VertexDescriptor.h:81  static void GetResourceDescriptor(void*, const Parameters*)
//      VertexDescriptor::Initialize(ProgramBufferData**, const Parameters*)
//        REAL: VertexDescriptor.h:83  static VertexDescriptorData* Initialize(rw::Resource*, const Parameters*)
//    states/programbuffer.cpp IS on tools/build/build_game_exe.bat (line 629) and
//    pc/gcm/renderengine/VertexDescriptor.cpp is on it too, so the fix is to delete the local
//    slices and include the real headers -- deliberately NOT done in this pass, because it would
//    change which overloads the console argument shapes bind to and this TU is dead until (2)/(3)
//    are closed anyway.
//
// 2. THE MICROCODE WALL. gCoronaPixelShader / gCoronaVertexShader stand in for the two Xenos
//    microcode blobs embedded in the guest image: X360 &unk_8200F1B8 (the PIXEL program, 228 bytes
//    -- the value the console writes to ProgramBufferParameters +0x08) and &unk_8200F2A0 (the
//    VERTEX program, 788 bytes). They are NOT in SHADERS.BNDL (the corona pair is executable-
//    embedded, exactly like the sky dome's -- see pc/gcm/renderengine/SkyDomeProgramsPC.cpp), and
//    the placeholders here are ONE BYTE each: any real GetResourceDescriptor would run
//    XGGetMicrocodeShaderParts off the end of them. A PC pair has to be authored/recovered the way
//    the sky dome's and Godray's were (see the report for the two candidate sources).
//
// 3. muFunction IS A GUEST POINTER WORD. The X360 stores the blob's guest address in a u32; the
//    casts below truncate a HOST pointer into it (AGENTS.md rule 1). Harmless only because the TU
//    is dead. On PC the whole GetResourceDescriptor/Initialize route is replaced by
//    renderengine::ProgramBufferPC_Adopt(blob, size, shaderType) -- programbuffer.h:123.
// =============================================================================================

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
    //      pointer at +0x00, a vertex/pixel shader-type flag at +0x04 and the blob's byte size at
    //      +0x08 (the X360 stores 228 for the pixel shader / 788 for the vertex shader), the rest
    //      zero. GetResourceDescriptor takes the muFunction != 0 path (XGGetMicrocodeShaderParts),
    //      so only muFunction is read at sizing time; Initialize consumes the full block.
    //      NOTE (2026-08-17): the committed real home spells +0x08 `muReserved8` and NEITHER
    //      GetResourceDescriptor NOR Initialize reads it (states/programbuffer.cpp:137/182 -- the
    //      microcode size comes from XGGetMicrocodeShaderParts, not from this word). It is kept
    //      named here only because it is this TU's record of the two blob sizes, which is what a
    //      dump request needs.
    struct ProgramBufferParameters
    {
        u32 muFunction;    // +0x00 compiled-shader function pointer
        u32 muShaderType;  // +0x04 1 -> pixel, 0 -> vertex (copied to the runtime object)
        u32 muBlobSize;    // +0x08 the microcode blob's byte size (real home: muReserved8, unread)
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
    // The corona shader function blobs in the X360 image (&unk_8200F1B8 = the PIXEL program,
    // 228 bytes; &unk_8200F2A0 = the VERTEX program, 788 bytes -- see blocker (2) in the banner).
    // HONEST PLACEHOLDER: compiled Xenos microcode the renderengine links against; declared as
    // opaque externs so the parameter blocks can name them. Their addresses are the muFunction
    // values. ONE BYTE EACH -- this TU must not be mounted while that is true.
    extern const u8 gCoronaPixelShader;    // X360 &unk_8200F1B8, 228 bytes
    extern const u8 gCoronaVertexShader;   // X360 &unk_8200F2A0, 788 bytes
    const u8 gCoronaPixelShader  = 0;
    const u8 gCoronaVertexShader = 0;

    // The named shader constants the VERTEX program exposes (X360 string literals at the call
    // sites; all three GetVariableHandleByName calls take the vertex program -- see the banner).
    const u8 gszViewProjectionMatrix[]         = "viewProjectionMatrix";
    const u8 gszCameraPositionPlusBrightness[] = "cameraPositionPlusBrightness";
    const u8 gszViewXyScale[]                  = "viewXyScale";

    // File-scope globals the later corona draw path reads (X360 dword_82FAB6B0/B4/AC and the handle
    // arrays at dword_82FAB850 / dword_82FAC1CC / dword_82FAC1B8). The built objects live in these
    // five-pointer handle slots; the *Program / *Decl pointers are the Initialize return values.
    // Which is which is fixed by CoronaRenderer::Begin @0x823FF2C0: SetPixelProgram(dword_82FAB6B0),
    // SetVertexProgramInternal(dword_82FAB6B4).
    renderengine::ProgramBufferData* gapCoronaPixelProgramHandles[5];   // X360 dword_82FAB850
    renderengine::ProgramBufferData* gapCoronaVertexProgramHandles[5];  // X360 dword_82FAC1CC
    renderengine::ProgramBufferData* gapCoronaVertexDeclHandles[5];     // X360 dword_82FAC1B8

    renderengine::ProgramBufferData* gpCoronaPixelProgram;              // X360 dword_82FAB6B0
    renderengine::ProgramBufferData* gpCoronaVertexProgram;             // X360 dword_82FAB6B4
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

        // --- pixel program (built FIRST -- shaderType 1) ----------------------------------------------
        ProgramBufferParameters lPixelParams = {};
        lPixelParams.muFunction   = static_cast<u32>(reinterpret_cast<usize>(&gCoronaPixelShader));  // &unk_8200F1B8
        lPixelParams.muShaderType = 1;                                            // var_21C = 1 -> pixel
        lPixelParams.muBlobSize   = 228;                                          // var_218 = 0xE4

        ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lPixelParams);
        lpAllocator->Allocate(lapHandles, lpAllocator, &lDescriptor, 0);
        std::memcpy(gapCoronaPixelProgramHandles, lapHandles, sizeof(lapHandles));  // copy 5 words
        gpCoronaPixelProgram = ProgramBuffer::Initialize(gapCoronaPixelProgramHandles, &lPixelParams);

        // --- vertex program (built SECOND -- shaderType 0) --------------------------------------------
        ProgramBufferParameters lVertexParams = {};
        lVertexParams.muFunction   = static_cast<u32>(reinterpret_cast<usize>(&gCoronaVertexShader)); // &unk_8200F2A0
        lVertexParams.muShaderType = 0;                                           // var_21C = 0 -> vertex
        lVertexParams.muBlobSize   = 788;                                         // var_218 = 0x314

        ProgramBuffer::GetResourceDescriptor(&lDescriptor, &lVertexParams);
        lpAllocator->Allocate(lapHandles, lpAllocator, &lDescriptor, 0);
        std::memcpy(gapCoronaVertexProgramHandles, lapHandles, sizeof(lapHandles));  // copy 5 words
        gpCoronaVertexProgram = ProgramBuffer::Initialize(gapCoronaVertexProgramHandles, &lVertexParams);

        // Pull the three named shader-constant handles out of the VERTEX program (the X360 calls
        // @0x82285250 / 0x82285268 / 0x82285280 all take dword_82FAB6B4, the SECOND Initialize's
        // result -- `stw r3, dword_82FAB6B4(r30)` @0x8228524C, reloaded before calls 2 and 3).
        ProgramBuffer::GetVariableHandleByName(gpCoronaVertexProgram, gszViewProjectionMatrix,
                                               &gCoronaViewProjectionHandle);
        ProgramBuffer::GetVariableHandleByName(gpCoronaVertexProgram, gszCameraPositionPlusBrightness,
                                               &gCoronaCameraPositionPlusBrightness);
        ProgramBuffer::GetVariableHandleByName(gpCoronaVertexProgram, gszViewXyScale,
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
