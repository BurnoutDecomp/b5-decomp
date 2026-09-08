#include "GameSource/Effects/Particles/Native/BrnDebrisRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"     // BrnDebrisArrayParams table

#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h" // renderengine::BlendState*
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

// BrnParticle::Native::BrnDebrisRenderer::Construct @ 0x82281DA8
//
// Reconstructed store-for-store from the X360 ARTIST asm. The original built a
// renderengine::BlendState::Parameters via the setter API; the compiler inlined every
// setter into the raw parameter-word stores reproduced below. The parameter block is then
// turned into a resource descriptor, the backing resource is allocated through the passed
// rw::IResourceAllocator (virtual Create at vtable +0x10, matching
// CgsGraphics::ImRendererBase::ConstructBlendState @ 0x827ED118), and the blend state is
// Initialize()d in place. mpRenderer caches the borrowed Im3d renderer.
//
// Blend-state parameters (from the inlined v8[]/byte stores):
//   maBlendFactor = { 0x07060701, 0x07060706, 0x07060706, 0x07060706 }
//   muState15=4  muState4..7=15  muState8=0x87  muState17=1  muState9=0xFFFFFFFF
//   mbHasCustomBlendFactors=1  mbState10..14=0  mbState16=1

namespace
{
    // The rw resource allocator's Create slot; vtable offset +0x10 on X360 (same shape the
    // immediate-mode blend-state builders use). Declared file-locally so the virtual
    // dispatch compiles without pulling the full rwcore allocator surface.
    class ResourceAllocator
    {
    public:
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void*                   lpResourceOut,
            ResourceAllocator*      /*lpAllocator*/,
            const void*             lpDescriptor,
            int                     /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpResourceOut, lpDescriptor);
        }
    };
}

namespace BrnParticle
{
namespace Native
{
    // =====================================================================================
    // _gaDebrisArrayParams -- the five debris parameter presets, indexed by EDebrisArrayID.
    // This file is the table's home (both BrnDebrisArray::Construct asserts cite it), and
    // BrnDebrisArray::Construct binds mpParams to one of these entries.
    //
    // RECOVERED FROM THE CONSOLE IMAGE, not authored. The scalar half is plain initialised
    // data read straight out of the image; the two vector members (mColour, mvBounciness)
    // are SILENT ZEROES there -- Vector4/Vector3 are non-trivial, so the compiler emitted a
    // CRT dynamic initialiser that writes them at startup. That initialiser was located and
    // disassembled, and it is where the colours and the bounce vectors below come from:
    //
    //   * the shared bounce base is one namespace-scope const Vector3 (0.7, 0.6, 0.7). The
    //     initialiser stores it UNSCALED into entry 0 and stores base * s into entries 1..4,
    //     with s = 0.9 / 0.8 / 0.5 / 0.8 -- four broadcast-then-multiply pairs against a
    //     scalar. Written below as that multiply, not as its product, because the multiply
    //     is what the original source said.
    //   * every colour is white except the Dark preset, which is (0.5, 0.5, 0.5, 1.0).
    //
    // Corroboration that the offsets are right, independent of the initialiser: the entry
    // stride is 80 bytes (the value the array index is scaled by), the two vector stores land
    // at entry +0x20 and entry +0x30, and the particle counts below are exactly what
    // GetNewDebris compares its 96-per-bucket budget against.
    // =====================================================================================
    namespace
    {
        // The bounce-damping base every preset scales. Per-axis: the debris keeps 70% of its
        // horizontal speed and 60% of its vertical speed across a bounce.
        constexpr rw::math::vpu::Vector3 KV_DEBRIS_BOUNCINESS = { 0.7f, 0.6f, 0.7f, 0.0f };

        constexpr rw::math::vpu::Vector3 ScaledBounciness(f32 lfScale)
        {
            return rw::math::vpu::Vector3{ KV_DEBRIS_BOUNCINESS.x * lfScale,
                                           KV_DEBRIS_BOUNCINESS.y * lfScale,
                                           KV_DEBRIS_BOUNCINESS.z * lfScale,
                                           KV_DEBRIS_BOUNCINESS.w * lfScale };
        }

        constexpr rw::math::vpu::Vector4 KV_DEBRIS_WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    extern const BrnDebrisArrayParams _gaDebrisArrayParams[eDebrisArray_Max] =
    {
        // eDebrisArray_Coloured
        { "lowres_debris.rf3",     400, 10.0f,  0.2f,  1.0f, 0.2f,
          KV_DEBRIS_WHITE,                  KV_DEBRIS_BOUNCINESS,        0.1f,   0.8f },
        // eDebrisArray_Shiny
        { "lowres_debris.rf3",     300, 10.0f,  0.2f,  0.7f, 0.3f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.9f ),    0.1f,   0.8f },
        // eDebrisArray_Dark
        { "lowres_debris.rf3",     400,  1.0f,  0.01f, 0.5f, 0.2f,
          { 0.5f, 0.5f, 0.5f, 1.0f },       ScaledBounciness( 0.8f ),    0.1f,   0.8f },
        // eDebrisArray_HighDetail
        { "highres_debris_02.rf3", 100, 10.0f,  0.2f,  1.0f, 0.2f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.5f ),    0.1f,   0.8f },
        // eDebrisArray_Glass
        { "Glass_debris.rf3",      400, 50.0f, 50.0f,  1.0f, 0.6f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.8f ),    0.001f, 0.8f },
    };

    void BrnDebrisRenderer::Construct(rw::IResourceAllocator* lpAllocator,
                                      BrnGraphics::Im3dTexPlusLighting* lpRenderer)
    {
        renderengine::BlendStateParameters lParameters;
        lParameters.maBlendFactor[0] = 0x07060701u;
        lParameters.maBlendFactor[1] = 0x07060706u;
        lParameters.maBlendFactor[2] = 0x07060706u;
        lParameters.maBlendFactor[3] = 0x07060706u;
        lParameters.muState15 = 4u;
        lParameters.muState4  = 15u;
        lParameters.muState5  = 15u;
        lParameters.muState6  = 15u;
        lParameters.muState7  = 15u;
        lParameters.muState8  = 135u;        // 0x87
        lParameters.muState17 = 1u;
        lParameters.muState9  = 0xFFFFFFFFu;  // -1
        lParameters.mbHasCustomBlendFactors = 1u;
        lParameters.mbState10 = 0u;
        lParameters.mbState11 = 0u;
        lParameters.mbState12 = 0u;
        lParameters.mbState13 = 0u;
        lParameters.mbState14 = 0u;
        lParameters.mbState16 = 1u;

        // Cache the borrowed Im3d renderer (this[0]).
        mpRenderer = lpRenderer;

        // Build the resource descriptor for the parameter block (X360: 48-byte scratch).
        void* lpDescriptor[12] = {};
        renderengine::BlendState::GetResourceDescriptor(lpDescriptor, &lParameters);

        // Allocate the backing resource through the allocator's Create slot (vtable +0x10).
        // X360: (*(*a2 + 16))(v6, a2, v7, 0) -- v6 is a 32-byte state-handle scratch.
        renderengine::BlendMaterialState* lapStateHandles[8] = {};
        ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
        lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, lpDescriptor, 0);

        // Initialize the blend state in place and latch it (this[1]).
        mpBlendState = reinterpret_cast<renderengine::BlendState*>(
            renderengine::BlendState::Initialize(lapStateHandles, &lParameters) );

        CGS_ASSERT( mpBlendState, "mpBlendState" );
    }
}
}
