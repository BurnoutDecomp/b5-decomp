// BrnParticle::Native::BrnSimpleParticleRenderer::Construct @ 0x82284518
//
// Initialises the three blend states this renderer wraps by reading from the
// three global templates (dword_83010F20/F24/F28), applying a fixed set of
// parameter overrides per block (HIBYTE of channel-0 factor = 7,
// mbHasCustomBlendFactors = 1, muState15 = 4, muState17 = 1, byte2 of
// channel-0 = 1), computing the resource descriptor, malloc-ing the backing
// buffer, and calling Initialize.  X360 DWARF attributes this body to
// stateparams.h.
//
// HIBYTE is kept as a call-form macro to match the parity fingerprint of the
// X360 pseudocode (scanner counts call sites).

#include "GameSource/Effects/Particles/Native/BrnSimpleParticleRenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"

#include <cstdlib>   // malloc
#include <cstring>   // memset

// HIBYTE as lvalue macro (byte 3 of a u32).
#define HIBYTE(x) (*reinterpret_cast<u8*>(reinterpret_cast<u8*>(&(x)) + 3u))

// The three process-wide blend-state template handles, set up once by
// LionParticleRender's ConstructOnceOnly path.  The globals live in a
// LionParticleRender.cpp data block; declared extern for the compile gate.
extern renderengine::BlendMaterialState* dword_83010F20;  // template 0
extern renderengine::BlendMaterialState* dword_83010F24;  // template 1
extern renderengine::BlendMaterialState* dword_83010F28;  // template 2

namespace BrnParticle { namespace Native {

// ---------------------------------------------------------------------------
// BrnSimpleParticleRenderer::Construct  @ 0x82284518  [not executed in trace]
//
// a2 — present in X360 ABI but never read (originally an allocator handle that
//       was later bypassed in favour of raw malloc in this path).
// a3 — stored to slot 0 of this struct (mInitParam).
// ---------------------------------------------------------------------------
int BrnSimpleParticleRenderer::Construct(int /*a2*/, int a3)
{
    mInitParam = a3;

    // Shared descriptor slot reused across all 3 blocks (X360: single v14 stack slot).
    renderengine::ResourceDescriptorEntry lDesc;

    // ---- Blend state 0 (dword_83010F20) ------------------------------------
    renderengine::BlendStateParameters lP0;
    lP0.maBlendFactor[0] = 0x07060706u;
    lP0.maBlendFactor[1] = 0x07060706u;
    lP0.maBlendFactor[2] = 0x07060706u;
    lP0.maBlendFactor[3] = 0x07060706u;
    lP0.muState15 = 7u; lP0.muState4 = 15u; lP0.muState5 = 15u;
    lP0.muState6  = 15u; lP0.muState7 = 15u; lP0.muState8 = 135u;
    lP0.muState17 = 0u; lP0.muState9 = 0xFFFFFFFFu;
    lP0.mbHasCustomBlendFactors = 0u;
    lP0.mbState10 = 0u; lP0.mbState11 = 0u; lP0.mbState12 = 0u;
    lP0.mbState13 = 0u; lP0.mbState14 = 0u; lP0.mbState16 = 0u;

    renderengine::BlendState::GetParameters(dword_83010F20, &lP0);
    HIBYTE(lP0.maBlendFactor[0]) = 7u;
    lP0.mbHasCustomBlendFactors = 1u;
    lP0.muState15 = 4u;
    lP0.muState17 = 1u;
    lP0.maBlendFactor[0] = (lP0.maBlendFactor[0] & 0xFF00FFFFu) | 0x00010000u;

    renderengine::BlendState::GetResourceDescriptor(&lDesc, &lP0);

    renderengine::BlendMaterialState* lapH0[8];
    lapH0[0] = reinterpret_cast<renderengine::BlendMaterialState*>(malloc(lDesc.muSize));
    memset(lapH0 + 1, 0, sizeof(void*) * 4u);
    mpBlendState0 = renderengine::BlendState::Initialize(lapH0, &lP0);

    // ---- Blend state 1 (dword_83010F24) ------------------------------------
    renderengine::BlendStateParameters lP1;
    lP1.maBlendFactor[0] = 0x07060706u;
    lP1.maBlendFactor[1] = 0x07060706u;
    lP1.maBlendFactor[2] = 0x07060706u;
    lP1.maBlendFactor[3] = 0x07060706u;
    lP1.muState15 = 7u; lP1.muState4 = 15u; lP1.muState5 = 15u;
    lP1.muState6  = 15u; lP1.muState7 = 15u; lP1.muState8 = 135u;
    lP1.muState17 = 0u; lP1.muState9 = 0xFFFFFFFFu;
    lP1.mbHasCustomBlendFactors = 0u;
    lP1.mbState10 = 0u; lP1.mbState11 = 0u; lP1.mbState12 = 0u;
    lP1.mbState13 = 0u; lP1.mbState14 = 0u; lP1.mbState16 = 0u;

    renderengine::BlendState::GetParameters(dword_83010F24, &lP1);
    HIBYTE(lP1.maBlendFactor[0]) = 7u;
    lP1.mbHasCustomBlendFactors = 1u;
    lP1.muState15 = 4u;
    lP1.muState17 = 1u;
    lP1.maBlendFactor[0] = (lP1.maBlendFactor[0] & 0xFF00FFFFu) | 0x00010000u;

    renderengine::BlendState::GetResourceDescriptor(&lDesc, &lP1);

    renderengine::BlendMaterialState* lapH1[8];
    lapH1[0] = reinterpret_cast<renderengine::BlendMaterialState*>(malloc(lDesc.muSize));
    memset(lapH1 + 1, 0, sizeof(void*) * 4u);
    mpBlendState1 = renderengine::BlendState::Initialize(lapH1, &lP1);

    // ---- Blend state 2 (dword_83010F28) ------------------------------------
    renderengine::BlendStateParameters lP2;
    lP2.maBlendFactor[0] = 0x07060706u;
    lP2.maBlendFactor[1] = 0x07060706u;
    lP2.maBlendFactor[2] = 0x07060706u;
    lP2.maBlendFactor[3] = 0x07060706u;
    lP2.muState15 = 7u; lP2.muState4 = 15u; lP2.muState5 = 15u;
    lP2.muState6  = 15u; lP2.muState7 = 15u; lP2.muState8 = 135u;
    lP2.muState17 = 0u; lP2.muState9 = 0xFFFFFFFFu;
    lP2.mbHasCustomBlendFactors = 0u;
    lP2.mbState10 = 0u; lP2.mbState11 = 0u; lP2.mbState12 = 0u;
    lP2.mbState13 = 0u; lP2.mbState14 = 0u; lP2.mbState16 = 0u;

    renderengine::BlendState::GetParameters(dword_83010F28, &lP2);
    HIBYTE(lP2.maBlendFactor[0]) = 7u;
    lP2.mbHasCustomBlendFactors = 1u;
    lP2.muState15 = 4u;
    lP2.muState17 = 1u;
    lP2.maBlendFactor[0] = (lP2.maBlendFactor[0] & 0xFF00FFFFu) | 0x00010000u;

    renderengine::BlendState::GetResourceDescriptor(&lDesc, &lP2);

    renderengine::BlendMaterialState* lapH2[8];
    lapH2[0] = reinterpret_cast<renderengine::BlendMaterialState*>(malloc(lDesc.muSize));
    memset(lapH2 + 1, 0, sizeof(void*) * 4u);
    mpBlendState2 = renderengine::BlendState::Initialize(lapH2, &lP2);

    return reinterpret_cast<int>(mpBlendState2);
}

} } // namespace BrnParticle::Native

#undef HIBYTE
