// CgsGraphics::ImRendererBase::ConstructBlendState @ 0x827ED118
//
// Parameterised blend-state builder: reads from this object (treated as the
// ResourceAllocator through vtable slot +0x10), computes the channel-0
// blend-factor word from the incoming mode/blend arguments, and builds the
// live BlendState via GetResourceDescriptor + ResourceAllocator::Create +
// BlendState::Initialize.
//
// This function is attributed to stateparams.h by the X360 DWARF (same TU).
// It is a private helper called by ImRendererBase::ConstructOnceOnly.

#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"

namespace CgsGraphics
{
namespace
{
    // The rw resource allocator's Create slot; vtable offset +0x10 on X360.
    // Declared again here (mirroring CgsImRenderer.cpp) for this compilation unit.
    class ResourceAllocator
    {
    public:
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void* lpStateHandlesOut,
            ResourceAllocator* /*lpAllocator*/,
            const void* lpDescriptor,
            int /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpStateHandlesOut, lpDescriptor);
        }
    };

    // The packed blend-factor word splated across channels 1..3 (same constant as
    // CgsImRenderer.cpp; 0x07060706 = the default per-channel blend config).
    const u32 KU_BLEND_FACTOR_WORD = 0x07060706u;
}

// ---------------------------------------------------------------------------
// CgsGraphics::ImRendererBase::ConstructBlendState  @ 0x827ED118  [EXECUTED]
//
// Builds a dynamic blend state whose channel-0 factor word is assembled from:
//   luMode  (a2, 5-bit): raw source-blend enum packed into bits [0..4].
//   liBlendA (a3, 32-bit): blend-op selector; bits [3..10] of (a3*8) supply
//            the mid-field bits after a left shift of 5 (→ bits [8..15]).
//   liBlendB (a4, 32-bit): destination factor; bits outside [3..10] are kept
//            (& 0xFFFFF807) and merged before the shift.
//   Upper 16 bits: fixed 0x07060000.
//
// The vtable call (*(*a1 + 16))(v6, a1, v7, 0) dispatches through
// ResourceAllocator::Create at vtable offset +0x10; 'this' is both the
// ImRendererBase and the ResourceAllocator (same vtable, same object).
// ---------------------------------------------------------------------------
const BlendState* ImRendererBase::ConstructBlendState(u8 luMode, int liBlendA, int liBlendB)
{
    renderengine::BlendStateParameters lParameters;

    // Channels 1–3: the standard packed factor word (unchanged from other builders).
    lParameters.maBlendFactor[1] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[2] = KU_BLEND_FACTOR_WORD;
    lParameters.maBlendFactor[3] = KU_BLEND_FACTOR_WORD;

    // Channel 0: mode-specific blend-factor word.
    // X360: v8[0] = a2 & 0x1F | (32 * ((8 * a3) & 0x7F8 | a4 & 0xFFFFF807)) & 0xFFE0 | 0x7060000
    lParameters.maBlendFactor[0] =
        (static_cast<u32>(luMode) & 0x1Fu)
        | (32u * (  (static_cast<u32>(liBlendA) * 8u) & 0x7F8u
                  | (static_cast<u32>(liBlendB) & 0xFFFFF807u))) & 0xFFE0u
        | 0x07060000u;

    lParameters.muState15 = 7u;
    lParameters.muState4  = 15u;
    lParameters.muState5  = 15u;
    lParameters.muState6  = 15u;
    lParameters.muState7  = 15u;
    lParameters.muState8  = 135u;         // 0x87
    lParameters.muState17 = 0u;
    lParameters.muState9  = 0xFFFFFFFFu;  // -1
    lParameters.mbHasCustomBlendFactors = 1u;
    lParameters.mbState10 = 0u;
    lParameters.mbState11 = 0u;
    lParameters.mbState12 = 0u;
    lParameters.mbState13 = 0u;
    lParameters.mbState14 = 0u;
    lParameters.mbState16 = 0u;

    void* lpDescriptor[12] = {};
    renderengine::BlendState::GetResourceDescriptor(lpDescriptor, &lParameters);

    // Dispatch through ResourceAllocator::Create at vtable +0x10.
    // X360: (*(*a1 + 16))(v6, a1, v7, 0) — 'this' is both ImRendererBase and allocator.
    renderengine::BlendMaterialState* lapStateHandles[5] = {};
    ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(this);
    lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, lpDescriptor, 0);

    return reinterpret_cast<const BlendState*>(
        renderengine::BlendState::Initialize(lapStateHandles, &lParameters));
}

} // namespace CgsGraphics
