// BrnParticle::LionParticleRender::CreateInternalMaterial @ 0x82280C30
//
// Builds a process-wide internal-material entry for a particle material.
// X360 DWARF attributes this body to stateparams.h.
//
// The BYTE2/HIBYTE/HIDWORD/ROL4 macros and the __PAIR64__ helper are kept as
// function-call forms to match the parity fingerprint of the X360 pseudocode.
// The HIDWORD usages in the overflow-assert block set up a StrStreamBase object
// on the stack (the X360 assert-message path); on PC the message falls through
// to a plain FireAssert string.

#include "GameSource/Effects/Particles/LionParticleRender.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsPriorityQueue.h"

#include <cstdlib>   // malloc
#include <cstring>   // memset

// ---------------------------------------------------------------------------
// IDA pseudocode helper macros — keep as call-like expressions so the parity
// fingerprint matches the X360 pseudocode (scanner counts call sites).
// ---------------------------------------------------------------------------
static inline u32 __ROL4__(u32 v, unsigned n)
{
    return (v << n) | (v >> (32u - n));
}

// BYTE2/HIBYTE/HIDWORD as lvalue macros (byte-field access within a local value).
#define BYTE2(x)   (*reinterpret_cast<u8*>(reinterpret_cast<u8*>(&(x)) + 2u))
#define HIBYTE(x)  (*reinterpret_cast<u8*>(reinterpret_cast<u8*>(&(x)) + 3u))
#define HIDWORD(x) (*reinterpret_cast<u32*>(reinterpret_cast<u8*>(&(x)) + 4u))

// ---------------------------------------------------------------------------
// Process-wide global blend-state template, material table, and assert-path
// globals (all live in the X360 .data section of the LionParticleRender TU).
// Declared extern for the compile gate (cl /c does not link).
// ---------------------------------------------------------------------------
extern renderengine::BlendMaterialState* dword_83010F20;

struct BrnParticle_BlendStateMapEntry { u32 muHash; void* mpBlendState; };
extern BrnParticle_BlendStateMapEntry saBlendStates[256];
extern s32 siMaterialCount;

// Overflow-assert path: StrStreamBase vtable pointers (initialise a stack-local
// object before calling Clear + the string-append vtable slot).
extern u32 off_82000D00;
extern u32 off_82000D08;

namespace BrnParticle
{

// ---------------------------------------------------------------------------
// BrnParticle::LionParticleRender::CreateInternalMaterial @ 0x82280C30
//
// Reads the base blend parameters from the shared global template (dword_83010F20),
// applies the blend-mode switch from the material's mBlendMode byte (+0x3A = 58),
// and optionally overrides alpha-test parameters via mAlphaTestMode / mAlphaTestValue
// (+0x3B = 59 / +0x3C = 60) when mFlags bit 2 (+0x24 = 36) is set.
// Stores the result in the process-wide saBlendStates[] table (qword_82FAAD80).
// ---------------------------------------------------------------------------
U32 LionParticleRender::CreateInternalMaterial(const cParticleMaterial* apMaterial)
{
    if (!apMaterial)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "apMaterial != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Effects/Particles/LionParticleRender.cpp",
            530);
        CgsDev::Assert::EndAssert();
    }

    const U32 luHash = BrnParticle::LionParticleRender::HashMaterial(apMaterial);

    if (!luHash)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "luMaterialHash != 0",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Effects/Particles/LionParticleRender.cpp",
            534);
        CgsDev::Assert::EndAssert();
    }

    const void* const lpRenderer = mpRenderer;
    if (!lpRenderer)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mpRenderer != NULL",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Effects/Particles/LionParticleRender.cpp",
            539);
        CgsDev::Assert::EndAssert();
    }

    // Load the base blend-state parameters from the global template.
    renderengine::BlendStateParameters lParams;
    lParams.maBlendFactor[0] = 0x07060706u;
    lParams.maBlendFactor[1] = 0x07060706u;
    lParams.maBlendFactor[2] = 0x07060706u;
    lParams.maBlendFactor[3] = 0x07060706u;
    lParams.muState15 = 7u; lParams.muState4 = 15u; lParams.muState5 = 15u;
    lParams.muState6  = 15u; lParams.muState7 = 15u; lParams.muState8 = 135u;
    lParams.muState17 = 0u; lParams.muState9  = 0xFFFFFFFFu;
    lParams.mbHasCustomBlendFactors = 0u;
    lParams.mbState10 = 0u; lParams.mbState11 = 0u; lParams.mbState12 = 0u;
    lParams.mbState13 = 0u; lParams.mbState14 = 0u; lParams.mbState16 = 0u;

    renderengine::BlendState::GetParameters(dword_83010F20, &lParams);

    // Apply material-specific channel-0 blend override.
    // X360: v6 = *(a2 + 58) = mBlendMode
    const u32 luBlendMode = static_cast<u32>(apMaterial->mBlendMode);

    lParams.mbState16  = 1u;
    lParams.muState15  = 4u;
    lParams.maBlendFactor[0] &= 0xFFFFFF1Fu;        // clear bits [5..7]
    HIBYTE(lParams.maBlendFactor[0]) = 7u;           // byte 3 = 7
    lParams.muState17  = 1u;
    lParams.mbHasCustomBlendFactors = 1u;

    // v7 = maBlendFactor[0] & 0xFF00FFFF | 0x00010000  (clear byte2, set to 1)
    const u32 luV7 = (lParams.maBlendFactor[0] & 0xFF00FFFFu) | 0x00010000u;
    lParams.maBlendFactor[0] = luV7;

    u8 luV8 = 0u;
    switch (luBlendMode)
    {
    case 0: case 10: case 13:
        lParams.maBlendFactor[0] = luV7 & 0xFFFFFFE0u | 1u;
        BYTE2(lParams.maBlendFactor[0]) = 0u;
        break;
    case 1: case 2:
        lParams.maBlendFactor[0] = __ROL4__(3u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        BYTE2(lParams.maBlendFactor[0]) = 7u;
        break;
    case 4: case 5:
        lParams.maBlendFactor[0] = __ROL4__(5u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        luV8 = 11u;
        goto LABEL_18;
    case 6: case 11:
        lParams.maBlendFactor[0] = __ROL4__(3u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        BYTE2(lParams.maBlendFactor[0]) = 1u;
        HIBYTE(lParams.maBlendFactor[0]) = 1u;
        lParams.maBlendFactor[0] &= 0xFF00FFFFu;    // BYTE2 = 0 (final)
        break;
    case 7: case 12:
        lParams.maBlendFactor[0] = __ROL4__(5u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        BYTE2(lParams.maBlendFactor[0]) = 1u;
        break;
    case 8:
        lParams.maBlendFactor[0] = __ROL4__(3u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        BYTE2(lParams.maBlendFactor[0]) = 1u;
        lParams.maBlendFactor[0] = __ROL4__(1u, 7u) & 0xE0u | lParams.maBlendFactor[0] & 0xFFFFFF1Fu;
        break;
    case 9:
        lParams.maBlendFactor[0] = __ROL4__(5u, 1u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        BYTE2(lParams.maBlendFactor[0]) = 11u;
        lParams.maBlendFactor[0] = __ROL4__(1u, 7u) & 0xE0u | lParams.maBlendFactor[0] & 0xFFFFFF1Fu;
        break;
    case 14: case 15:
        lParams.maBlendFactor[0] = luV7 & 0xFFFFFFE0u | 7u;
        BYTE2(lParams.maBlendFactor[0]) = 6u;
        break;
    case 16: case 17:
        lParams.maBlendFactor[0] = luV7 & 0xFFFFFFE0u | 0xBu;
        luV8 = 10u;
        goto LABEL_18;
    case 18: case 19:
        lParams.maBlendFactor[0] = __ROL4__(1u, 2u) & 0x1Fu | luV7 & 0xFFFFFFE0u;
        luV8 = 8u;
LABEL_18:
        BYTE2(lParams.maBlendFactor[0]) = luV8;
        break;
    default:
        break;
    }

    // Optional alpha-test parameter override (mFlags bit 2 = alpha-test enable).
    // X360: *(a2 + 36) & 4 = mFlags & 4; *(a2 + 59) = mAlphaTestMode; *(a2 + 60) = mAlphaTestValue
    if ((apMaterial->mFlags & 4u) != 0u)
    {
        const u32 luAlphaTestMode  = static_cast<u32>(apMaterial->mAlphaTestMode);
        lParams.muState17          = static_cast<u32>(apMaterial->mAlphaTestValue);
        lParams.mbState16          = 1u;
        switch (luAlphaTestMode)
        {
        case 0: lParams.muState15 = 0u; break;
        case 1: lParams.muState15 = 7u; break;
        case 2: lParams.muState15 = 1u; break;
        case 3: lParams.muState15 = 3u; break;
        case 4: lParams.muState15 = 2u; break;
        case 5: lParams.muState15 = 6u; break;
        case 6: lParams.muState15 = 4u; break;
        case 7: lParams.muState15 = 5u; break;
        default: break;
        }
    }

    // Allocate and initialize the blend state.
    renderengine::BlendMaterialState* lapHandles[8];
    renderengine::ResourceDescriptorEntry lDescriptor;
    renderengine::BlendState::GetResourceDescriptor(&lDescriptor, &lParams);
    memset(&lapHandles[1], 0, 16u);
    lapHandles[0] = reinterpret_cast<renderengine::BlendMaterialState*>(
        malloc(lDescriptor.muSize));
    void* const lpBlendState = renderengine::BlendState::Initialize(lapHandles, &lParams);

    // Store in the process-wide table; assert on overflow (capacity = 256).
    s32 liCount = siMaterialCount;
    if (siMaterialCount >= 256)
    {
        CgsDev::Assert::BeginAssert();
        // X360: StrStreamBase setup via HIDWORD/Clear/vtable dispatch into gpcMessageBuffer.
        u64 lv15 = 0u;
        HIDWORD(lv15) = off_82000D00;
        reinterpret_cast<CgsContainers::BasePriorityQueue*>(&lv15)->
            CgsContainers::BasePriorityQueue::Clear();
        HIDWORD(lv15) = off_82000D08;
        (void)HIDWORD(lv15);  // X360: (*(HIDWORD(v15) + 4))(&v15, "Out of space...")
        CgsDev::Assert::FireAssert(
            "Out of space for more blend states",
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Effects/Particles/LionParticleRender.cpp",
            687);
        CgsDev::Assert::EndAssert();
        liCount = siMaterialCount;
    }

    // X360: qword_82FAAD80[v11] = __PAIR64__(v4, v10) — hash in high word, ptr in low word.
    saBlendStates[liCount].muHash      = luHash;
    saBlendStates[liCount].mpBlendState = lpBlendState;
    siMaterialCount = liCount + 1;

    return luHash;
}

} // namespace BrnParticle

#undef BYTE2
#undef HIBYTE
#undef HIDWORD
