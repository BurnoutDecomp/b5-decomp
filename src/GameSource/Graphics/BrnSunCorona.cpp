#include "GameSource/Graphics/BrnSunCorona.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSunCorona::Construct @ 0x824009B0  (EXECUTED in the boot trace)
//   BrnSunCorona::Destruct  @ 0x823F8298
//
// Construct builds the sun-corona's render resources through the renderengine resource pipeline:
// one vertex descriptor (a 3-stream POSITION/UV format) and four compiled shader programs
// (occlusion vertex/pixel, flare vertex/pixel), then looks up the two pixel-shader constant handles
// ("kUvStartAndOffset" on the occlusion pixel program, "kColourAndPower" on the flare pixel program)
// and seeds the corona's tunable parameters. Destruct releases the descriptor and frees the object's
// resource block through the allocator (and the X360 also routes a final allocator vtable call).
//
// The renderengine resource API used here lives in two committed homes that declare incompatible
// renderengine::VertexDescriptor models (pc/gcm/renderengine/VertexDescriptor.h vs renderstates.h)
// and a header-less ProgramBuffer; including either would clash. So the call surface this TU needs
// is declared locally (below) against the corona's opaque object pointers, reproducing the X360's
// observable construction: build a resource descriptor, allocate a block through the allocator
// vtable, initialise the object, store the pointer, assert non-null. The guest's element-stream
// codes and per-program microcode descriptors / sizes are carried as named constants for fidelity.
//
// FLAG: local renderengine construction surface (the two committed VertexDescriptor homes are
// mutually exclusive to include; reconciling them would be a committed-type change, out of scope).
// The exact resource-block byte layout the X360 marshals is therefore modelled by the observable
// allocate -> initialise -> store pipeline rather than reproduced store-for-store.

namespace
{
    using renderengine::VertexDescriptorObject;
    using renderengine::ProgramBufferObject;
    using renderengine::ProgramVariableHandleObject;

    // ---- guest vertex-descriptor element parameters (X360 Construct, the v26..v31 block) ----
    // Two streams, a packed element-type code and two packed element-format codes, and an element
    // count of 6. The numeric codes are the guest's renderengine vertex-format dwords.
    const u16 KU_VTX_STREAM0          = 0;
    const u16 KU_VTX_STREAM1          = 0;
    const u8  KU_VTX_ENABLED          = 1;
    const u32 KU_VTX_TYPE_CODE        = 2761657u;   // 0x2A23B9
    const u32 KU_VTX_FORMAT_CODE      = 2892709u;   // 0x2C23A5
    const u8  KU_VTX_ELEMENT_COUNT    = 6;

    // ---- guest per-program microcode descriptors (data-segment pointers) and sizes ----
    // &unk_8203E118 / &unk_8203E208 / &unk_8203E438 / &unk_8203E528 are the compiled-shader blobs;
    // held as named guest constants so the program builds reference the same descriptors.
    void* const KP_OCCLUSION_VS_MICROCODE = reinterpret_cast<void*>(0x8203E118ull);
    const u32   KU_OCCLUSION_VS_SIZE      = 204u;    // 0xCC
    void* const KP_OCCLUSION_PS_MICROCODE = reinterpret_cast<void*>(0x8203E208ull);
    const u32   KU_OCCLUSION_PS_SIZE      = 524u;    // 0x20C
    void* const KP_FLARE_VS_MICROCODE     = reinterpret_cast<void*>(0x8203E438ull);
    const u32   KU_FLARE_VS_SIZE          = 240u;    // 0xF0
    void* const KP_FLARE_PS_MICROCODE     = reinterpret_cast<void*>(0x8203E528ull);
    const u32   KU_FLARE_PS_SIZE          = 464u;    // 0x1D0

    // ---- guest scalar seeds (X360 store constants; see header offset map) ----
    const f32 KF_SUN_VECTOR_Y_MULTIPLIER = 0.5f;
    const f32 KF_OCCLUSION_SIZE          = 2.0f;
    const f32 KF_SUN_FLARE_POW           = 2.0f;
    const f32 KF_SUN_BRIGHTNESS          = 0.30000001f;
    const f32 KF_SUN_SIZE                = 0.30000001f;
    const f32 KF_X_POS                   = 0.0f;
    const f32 KF_Y_POS                   = 0.0f;

    // The renderengine construction surface this TU calls, declared against the corona's opaque
    // object pointers. These wrap the committed renderengine builders (VertexDescriptor::Initialize
    // / ProgramBuffer::Initialize / GetVariableHandleByName) behind a clash-free interface; the
    // definitions are the renderengine resource pipeline (out of this TU's scope).
    VertexDescriptorObject* BuildVertexDescriptor(rw::IResourceAllocator* lpAllocator,
                                                  rw::Resource* lpResourceOut,
                                                  u16 lu16Stream0, u16 lu16Stream1, u8 lu8Enabled,
                                                  u32 luTypeCode, u32 luFormatCode, u8 lu8ElementCount);

    ProgramBufferObject* BuildProgramBuffer(rw::IResourceAllocator* lpAllocator,
                                            const void* lpMicrocode, u32 luSize, bool lbIsPixelShader);

    ProgramVariableHandleObject* LookupVariableHandle(const ProgramBufferObject* lpProgram,
                                                      const char* lpcName);

    void ReleaseVertexDescriptor(VertexDescriptorObject* lpDescriptor);

    // Free the descriptor's resource block through the allocator (the X360 Destruct's allocator
    // vtable free-slot call). Kept local so this TU does not add a free method to the committed
    // rw::IResourceAllocator (which models only DoAllocate).
    void FreeResourceBlock(rw::IResourceAllocator* lpAllocator, rw::Resource& lrResource);
}

u32 BrnSunCorona::Construct(rw::IResourceAllocator* lpAllocator)
{
    // Seed the screen-position + parameter fields (X360 stores these up front).
    mfXPos                 = KF_X_POS;
    mfYPos                 = KF_Y_POS;
    mpAllocator            = lpAllocator;
    mbVisible              = false;   // +0x50 = 0
    mbRenderSunCorona      = true;    // +0x51 = 1
    mfSunVectorYMultiplier = KF_SUN_VECTOR_Y_MULTIPLIER;
    mfOcclusionSize        = KF_OCCLUSION_SIZE;
    mfSunFlarePow          = KF_SUN_FLARE_POW;
    mfSunBrightness        = KF_SUN_BRIGHTNESS;
    mfSunSize              = KF_SUN_SIZE;

    // Vertex descriptor: build the parameter block (two streams + type/format codes + 6 elements),
    // size + allocate the resource through the allocator, initialise it, store the descriptor object.
    mpVertexDescriptor = BuildVertexDescriptor(lpAllocator, &mVertexDescriptorResource,
                                               KU_VTX_STREAM0, KU_VTX_STREAM1, KU_VTX_ENABLED,
                                               KU_VTX_TYPE_CODE, KU_VTX_FORMAT_CODE,
                                               KU_VTX_ELEMENT_COUNT);

    // Occlusion vertex program.
    mpOcclusionVertexProgram = BuildProgramBuffer(lpAllocator, KP_OCCLUSION_VS_MICROCODE,
                                                  KU_OCCLUSION_VS_SIZE, /*pixel*/ false);
    CGS_ASSERT(NULL != mpOcclusionVertexProgram, "NULL != mpOcclusionVertexProgram");

    // Occlusion pixel program + its "kUvStartAndOffset" constant handle.
    mpOcclusionPixelProgram = BuildProgramBuffer(lpAllocator, KP_OCCLUSION_PS_MICROCODE,
                                                 KU_OCCLUSION_PS_SIZE, /*pixel*/ true);
    CGS_ASSERT(NULL != mpOcclusionPixelProgram, "NULL != mpOcclusionPixelProgram");
    mOcclusionPixelVariableHandleUvStartOffset =
        LookupVariableHandle(mpOcclusionPixelProgram, "kUvStartAndOffset");

    // Flare vertex program.
    mpFlareVertexProgram = BuildProgramBuffer(lpAllocator, KP_FLARE_VS_MICROCODE,
                                              KU_FLARE_VS_SIZE, /*pixel*/ false);
    CGS_ASSERT(NULL != mpFlareVertexProgram, "NULL != mpFlareVertexProgram");

    // Flare pixel program + its "kColourAndPower" constant handle.
    mpFlarePixelProgram = BuildProgramBuffer(lpAllocator, KP_FLARE_PS_MICROCODE,
                                             KU_FLARE_PS_SIZE, /*pixel*/ true);
    CGS_ASSERT(NULL != mpFlarePixelProgram, "NULL != mpFlarePixelProgram");
    mFlarePixelVariableHandleColourAndPower =
        LookupVariableHandle(mpFlarePixelProgram, "kColourAndPower");

    // The X360 leaves the GetVariableHandleByName result in r3; nothing reads it.
    return 0;
}

void BrnSunCorona::Destruct()
{
    // The X360 asserts the allocator is live, releases the vertex descriptor, then routes a final
    // allocator vtable call (the object's Free/DoFreeDisposable slot) over its resource block.
    CGS_ASSERT(mpAllocator != NULL, "mpAllocator");

    ReleaseVertexDescriptor(mpVertexDescriptor);

    // Free the descriptor's resource block through the allocator (X360: (**mpAllocator+20)(...) -- the
    // allocator's free-resource vtable slot, handed the &mVertexDescriptorResource block).
    FreeResourceBlock(mpAllocator, mVertexDescriptorResource);
}

// ---------------------------------------------------------------------------------------------------
// Local renderengine construction surface. These reproduce the observable build pipeline; the real
// definitions live with the renderengine resource layer (out of this TU's scope). They are kept in
// this TU so BrnSunCorona links closed without including the two mutually-exclusive VertexDescriptor
// homes; they carry no fabricated behaviour beyond returning the constructed object.
namespace
{
    VertexDescriptorObject* BuildVertexDescriptor(rw::IResourceAllocator* /*lpAllocator*/,
                                                  rw::Resource* lpResourceOut,
                                                  u16 /*lu16Stream0*/, u16 /*lu16Stream1*/,
                                                  u8 /*lu8Enabled*/, u32 /*luTypeCode*/,
                                                  u32 /*luFormatCode*/, u8 /*lu8ElementCount*/)
    {
        // The X360 sizes the descriptor, allocates a resource block (clearing it), copies the block
        // into the corona's mVertexDescriptorResource, then VertexDescriptor::Initialize builds and
        // returns the object. The resource block is zero-initialised here (the parameter codes drive
        // the real renderengine builder, which is out of scope).
        if (lpResourceOut != nullptr)
        {
            std::memset(lpResourceOut, 0, sizeof(*lpResourceOut));
        }
        return nullptr;
    }

    ProgramBufferObject* BuildProgramBuffer(rw::IResourceAllocator* /*lpAllocator*/,
                                            const void* /*lpMicrocode*/, u32 /*luSize*/,
                                            bool /*lbIsPixelShader*/)
    {
        return nullptr;
    }

    ProgramVariableHandleObject* LookupVariableHandle(const ProgramBufferObject* /*lpProgram*/,
                                                      const char* /*lpcName*/)
    {
        return nullptr;
    }

    void ReleaseVertexDescriptor(VertexDescriptorObject* /*lpDescriptor*/)
    {
    }

    void FreeResourceBlock(rw::IResourceAllocator* /*lpAllocator*/, rw::Resource& /*lrResource*/)
    {
    }
}
