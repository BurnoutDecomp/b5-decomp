#include "GameSource/Effects/Particles/Native/BrnDebrisRenderer.h"

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
