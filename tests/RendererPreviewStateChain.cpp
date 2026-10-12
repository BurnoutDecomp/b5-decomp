// Executes extracted production preview, Basic2d, typed-state and shadow bodies.
// Only the native appliers/draw boundary and the narrow preview owner are fixtures.
// This is a CPU request/cache witness, not a GPU or whole-module layout witness.
#include <Windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <vector>
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsTypes.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm3dRenderBuffer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"
#include "pc/gcm/renderengine/device.h"
#include "pc/gcm/renderengine/texture.h"
#include "pc/gcm/renderengine/ShaderBindings.h"
#include "pc/gcm/renderengine/SamplerStateCache.h"
#include "pc/gcm/renderengine/GeometryBindings.h"
#include "rw/rwcore_structs.h"

static unsigned suChecks, suFailures, suAssertions;
static void Check(bool lbPassed, const char* lpcMessage)
{
    ++suChecks;
    if (!lbPassed) { ++suFailures; std::printf("FAIL %s\n", lpcMessage); }
}
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++suAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { void WriteToLog(const char*) {} }
}
namespace renderengine {
IDirect3DDevice9* gDevice = nullptr;
s32 gDisplayWidth = 1280, gDisplayHeight = 720;
u32 guPresentCount = 1;
}
IDirect3DDevice9* gpD3DDevice = nullptr;

enum EventKind { E_BLEND, E_DEPTH, E_RASTER, E_SAMPLER, E_TEXTURE, E_DRAW };
struct Event
{
    EventKind meKind;
    const void* mpValue;
    bool mbWasUnset;
    u32 muUnit, muFlags;
    CgsGraphics::ImRendererBase* mpOwner;
};
static std::vector<Event> saEvents;
static void Record(EventKind leKind, const void* lpValue, bool lbWasUnset = false,
                   u32 luUnit = 0, u32 luFlags = 0)
{
    saEvents.push_back({leKind, lpValue, lbWasUnset, luUnit, luFlags,
        CgsGraphics::ImRendererBase::mgpActiveRenderer});
}
static unsigned EventCount(EventKind leKind)
{
    unsigned luCount = 0;
    for (const Event& lrEvent : saEvents) luCount += lrEvent.meKind == leKind;
    return luCount;
}

// Full canonical objects keep the same pointer vocabulary as the real provider.
// Their native contents are outside these applier recording boundaries.
static renderengine::BlendMaterialState sStandardBlend = {};
static renderengine::DepthStencilState sDepthOff = {};
static renderengine::RasterizerState sCullNone = {};
namespace CgsGui {
const renderengine::BlendState* gpGuiBlendStateStandard =
    reinterpret_cast<const renderengine::BlendState*>(&sStandardBlend);
const renderengine::RasterizerState* gpGuiRasterizerStateCullNone = &sCullNone;
}
namespace CgsGraphics {
void PrepareIm3dStateLibraryPC() { Check(false, "null-device Begin must stop before native provider"); }
const renderengine::DepthStencilState* GetIm3dDepthStencilZBufferOffPC() { return &sDepthOff; }
}
namespace shadow {
void* Device::Xbox2SetStateLowLevelShadowed(void* lpState, bool lbWasUnset)
{ Record(E_BLEND, lpState, lbWasUnset); return lpState; }
void* Device::Xbox2SetDepthStencilStateLowLevelShadowed(void* lpState, bool lbWasUnset)
{ Record(E_DEPTH, lpState, lbWasUnset); return lpState; }
void* Device::Xbox2SetRasterizerStateLowLevelShadowed(void* lpState, bool lbWasUnset)
{ Record(E_RASTER, lpState, lbWasUnset); return lpState; }
}
extern "C" void* SetSamplerStateLowLevel(void* lpState, u32 luUnit, bool lbWasUnset)
{ Record(E_SAMPLER, lpState, lbWasUnset, luUnit); return lpState; }
extern "C" unsigned int D3DDevice_SetTexture(IDirect3DDevice9*, u32 luUnit,
                                             void* lpTexture, unsigned int luFlags)
{ Record(E_TEXTURE, lpTexture, false, luUnit, luFlags); return 0; }

using BasicVertex = CgsGraphics::Basic2dColouredTexturedVertex;
static BasicVertex saCaptured[4];
static u32 suCapturedCount;
static renderengine::PrimitiveType seCapturedPrimitive;
struct RecordingIm2d : CgsGraphics::Im2d
{
    void Render(renderengine::PrimitiveType lePrimitive, const BasicVertex* lpVertices, u32 luCount)
    {
        // The real Render executes its owner assertion and null-device early-out.
        CgsGraphics::ImRenderer<BasicVertex>::Render(lePrimitive, lpVertices, luCount);
        Check(luCount == 4, "preview submits exactly four CPU vertices");
        suCapturedCount = luCount;
        seCapturedPrimitive = lePrimitive;
        std::memcpy(saCaptured, lpVertices, sizeof(saCaptured));
        Record(E_DRAW, shadow::Device::GetSamplerTextureShadow(0));
    }
};

// Unrelated renderer module members are excluded from this fixture owner. The
// runner separately checks the real header's canonical params/resource types.
struct BrnRendererModule
{
    RecordingIm2d mIm2dRenderer;
    renderengine::TextureState* mpTextureState = nullptr;
    renderengine::TextureState::Parameters mTextureStateParams = {};
    rw::Resource mTextureStateResource;
    void DebugDisplayRenderTarget(renderengine::Texture*, Vector2, Vector2, CgsDev::RGBA);
};

#include "renderer_preview_state_chain.inc"

using Immediate = CgsGraphics::Im2d;
using Owner = CgsGraphics::ImRendererBase;
using Shadow = shadow::Device;

static void UnlockStates()
{
    Shadow::mbBlendStateLocked = false;
    Shadow::mbDepthStencilStateLocked = false;
    Shadow::mbRasteriserStateLocked = false;
}

static void CheckBracket()
{
    Immediate lFirst, lSecond;
    Owner::mgpActiveRenderer = nullptr;
    Shadow::mpStreamArray = &lFirst;
    Shadow::mpIndexStreamArray = &lSecond;
    Shadow::mpVertexDescriptor = reinterpret_cast<renderengine::VertexDescriptorData*>(&lFirst);
    Shadow::mpVertexProgramShadow = reinterpret_cast<renderengine::ProgramBufferData*>(&lFirst);
    Shadow::mpPixelProgramShadow = reinterpret_cast<renderengine::ProgramBufferData*>(&lSecond);
    Shadow::mpBlendState = &sStandardBlend;
    Shadow::mpDepthStencilState = &sDepthOff;
    Shadow::mpRasterizerState = &sCullNone;
    Shadow::muMisc30 = 0xA5A5;
    Shadow::mbVertexProgramStateDirty = false;
    Shadow::mbBlendStateLocked = Shadow::mbDepthStencilStateLocked = Shadow::mbRasteriserStateLocked = true;
    for (u32 luUnit = 0; luUnit < Shadow::KU_MAX_TEXTURE_STATES; ++luUnit)
    {
        Shadow::mapTextureState[luUnit] = reinterpret_cast<renderengine::TextureState*>(&lFirst);
        Shadow::mapSamplerState[luUnit] = &lFirst;
        Shadow::mapSamplerTexture[luUnit] = &lSecond;
    }
    lFirst.mi8CurrentProgram = 7;
    lFirst.mCurrentTransform.mOriginXYZ.x = 1000.0f;
    saEvents.clear();
    const unsigned luBefore = suAssertions;
    lFirst.BeginRendering();
    Check(Owner::mgpActiveRenderer == static_cast<Owner*>(&lFirst), "Begin publishes the real base subobject before native early-out");
    Check(lFirst.mi8CurrentProgram == 0, "Begin selects original program slot zero");
    Check(Shadow::mpStreamArray == nullptr && Shadow::mpIndexStreamArray == nullptr
        && Shadow::mpVertexDescriptor == nullptr && Shadow::mpVertexProgramShadow == nullptr,
        "Begin resets geometry and vertex-program keys");
    Check(Shadow::mpPixelProgramShadow == reinterpret_cast<const renderengine::ProgramBufferData*>(uintptr_t(~0u)),
        "Begin writes original pixel-program sentinel");
    Check(!Shadow::mpBlendState && !Shadow::mpDepthStencilState && !Shadow::mpRasterizerState
        && Shadow::muMisc30 == 0 && Shadow::mbVertexProgramStateDirty,
        "Begin resets the canonical state triple and marks vertex state dirty");
    for (u32 luUnit = 0; luUnit < Shadow::KU_MAX_TEXTURE_STATES; ++luUnit)
        Check(!Shadow::mapTextureState[luUnit] && !Shadow::mapSamplerState[luUnit]
            && !Shadow::mapSamplerTexture[luUnit], "Begin resets all three keys at every texture unit");
    Check(Shadow::mbBlendStateLocked && Shadow::mbDepthStencilStateLocked && Shadow::mbRasteriserStateLocked,
        "shadow reset preserves original lock windows");
    Check(lFirst.mCurrentTransform.mOriginXYZ.x == -1.0f
        && lFirst.mCurrentTransform.mOriginXYZ.y == 1.0f
        && lFirst.mCurrentTransform.mRightUp.x == 1.0f / 640.0f
        && lFirst.mCurrentTransform.mRightUp.w == -1.0f / 360.0f
        && lFirst.mCurrentTransform.mColourScale.w == 1.0f,
        "Im2d Begin publishes logical-screen transform before claiming the owner");
    Check(saEvents.empty(), "null native device produces no native state or draw requests");
    lFirst.CgsGraphics::ImRenderer<BasicVertex>::Render(static_cast<renderengine::PrimitiveType>(6), nullptr, 0);
    Check(suAssertions == luBefore, "paired Begin and Render satisfy original ownership assertions");
    lFirst.EndRendering();
    Check(Owner::mgpActiveRenderer == nullptr, "paired End releases the active owner");
    Owner::mgpActiveRenderer = &lFirst;
    lSecond.EndRendering();
    Check(suAssertions == luBefore + 1 && Owner::mgpActiveRenderer == nullptr,
        "mismatched End asserts and still unconditionally releases ownership");
    Owner::mgpActiveRenderer = nullptr;
    lFirst.BeginRendering();
    lSecond.mi8CurrentProgram = 6;
    lSecond.BeginRendering();
    Check(suAssertions == luBefore + 2 && Owner::mgpActiveRenderer == &lSecond
        && lSecond.mi8CurrentProgram == 0, "nested Begin asserts then performs original owner and slot stores");
    lSecond.EndRendering();
    Owner::mgpActiveRenderer = nullptr;
    UnlockStates();
}

template<class Canonical, class Opaque>
static void CheckStateCache(Immediate& lrOwner, Canonical& lrA, Canonical& lrB,
                            bool& lrbLocked, const Canonical*& lrpCached, EventKind leKind)
{
    const Opaque* lpA = reinterpret_cast<const Opaque*>(&lrA);
    const Opaque* lpB = reinterpret_cast<const Opaque*>(&lrB);
    saEvents.clear();
    lrpCached = nullptr;
    lrOwner.SetState(lpA);
    Check(EventCount(leKind) == 1 && lrpCached == &lrA && saEvents.back().mbWasUnset,
        "first typed state bind applies and publishes the canonical cache");
    lrOwner.SetState(lpA);
    Check(EventCount(leKind) == 1, "unchanged typed state skips the native applier");
    lrbLocked = true;
    lrOwner.SetState(lpB);
    Check(EventCount(leKind) == 1 && lrpCached == &lrA, "locked typed state preserves cache and native request count");
    lrbLocked = false;
    lrOwner.SetState(lpB);
    Check(EventCount(leKind) == 2 && lrpCached == &lrB && !saEvents.back().mbWasUnset,
        "unlocked changed state applies with a nonempty previous key");
    Shadow::SetState(&lrA);
    lrOwner.SetState(lpB);
    Check(EventCount(leKind) == 4 && lrpCached == &lrB,
        "dispatch and immediate setters observe one shared cache");
    lrOwner.SetState(static_cast<const Opaque*>(nullptr));
    Check(EventCount(leKind) == 5 && lrpCached == nullptr, "null state invalidates the original cache after applier request");
    lrOwner.SetState(lpA);
    Check(EventCount(leKind) == 6 && saEvents.back().mbWasUnset && lrpCached == &lrA,
        "post-invalidation bind carries the original was-unset flag");
    Check(saEvents.back().mpOwner == static_cast<Owner*>(&lrOwner), "typed bind executes with the actual active renderer owner");
}

static void CheckTypedStates()
{
    Immediate lOwner, lOther;
    Owner::mgpActiveRenderer = nullptr;
    UnlockStates();
    lOwner.BeginRendering();
    renderengine::BlendMaterialState lBlendA = {}, lBlendB = {};
    renderengine::DepthStencilState lDepthA = {}, lDepthB = {};
    renderengine::RasterizerState lRasterA = {}, lRasterB = {};
    CheckStateCache<renderengine::BlendMaterialState, CgsGraphics::BlendState>(lOwner,
        lBlendA, lBlendB, Shadow::mbBlendStateLocked, Shadow::mpBlendState, E_BLEND);
    CheckStateCache<renderengine::DepthStencilState, CgsGraphics::DepthStencilState>(lOwner,
        lDepthA, lDepthB, Shadow::mbDepthStencilStateLocked, Shadow::mpDepthStencilState, E_DEPTH);
    CheckStateCache<renderengine::RasterizerState, CgsGraphics::RasterizerState>(lOwner,
        lRasterA, lRasterB, Shadow::mbRasteriserStateLocked, Shadow::mpRasterizerState, E_RASTER);
    const unsigned luBefore = suAssertions;
    lOther.SetState(reinterpret_cast<const CgsGraphics::RasterizerState*>(&lRasterB));
    Check(suAssertions == luBefore + 1 && Shadow::mpRasterizerState == &lRasterB,
        "wrong typed owner asserts while preserving original non-gating state side effect");
    lOwner.EndRendering();
    Owner::mgpActiveRenderer = nullptr;
}

struct StateStorage
{
    u8 maBefore[16];
    alignas(renderengine::TextureState) u8 maState[sizeof(renderengine::TextureState)];
    u8 maAfter[16];
};
static void InitializeOwner(BrnRendererModule& lrModule, StateStorage& lrStorage)
{
    std::memset(&lrStorage, 0xA5, sizeof(lrStorage));
    lrModule.mTextureStateResource.m_baseResources[0] = lrStorage.maState;
    for (u32 luLane = 1; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
        lrModule.mTextureStateResource.m_baseResources[luLane] =
            reinterpret_cast<void*>(UINT64_C(0x1234567800000000) + luLane);
    auto& lrParams = lrModule.mTextureStateParams;
    lrParams.muAddressU = lrParams.muAddressV = 2;
    lrParams.muMipFilter = 2;
    lrParams.muMaxAnisotropy = 13;
    lrParams.muField10 = 1;
    lrParams.mu8Field43 = lrParams.mu8Field44 = 1;
}
static bool StorageIntact(const StateStorage& lrStorage)
{
    for (u8 luByte : lrStorage.maBefore) if (luByte != 0xA5) return false;
    for (u8 luByte : lrStorage.maAfter) if (luByte != 0xA5) return false;
    return true;
}

static void CheckWholeTexture()
{
    Immediate lOwner;
    Owner::mgpActiveRenderer = nullptr;
    lOwner.BeginRendering();
    renderengine::Texture lRasterA = {}, lRasterB = {}, lRasterC = {};
    renderengine::TextureState lStateA = {}, lStateB = {};
    lStateA.mpRaster = &lRasterA; lStateB.mpRaster = &lRasterB;
    saEvents.clear();
    lOwner.SetState(&lStateA);
    Check(saEvents.size() == 2 && saEvents[0].meKind == E_SAMPLER
        && saEvents[0].mbWasUnset && saEvents[1].meKind == E_TEXTURE
        && saEvents[1].muFlags == 0x80000000u, "whole TextureState applies sampler then stage-zero raster with original mask");
    Check(Shadow::mapTextureState[0] == &lStateA && Shadow::mapSamplerState[0] == &lStateA
        && Shadow::mapSamplerTexture[0] == &lRasterA, "whole TextureState publishes all three native-width keys");
    lOwner.SetState(&lStateA);
    Check(saEvents.size() == 2, "same whole texture object produces no repeated binds");
    lOwner.SetState(&lStateB);
    Check(saEvents.size() == 4 && !saEvents[2].mbWasUnset
        && Shadow::GetSamplerTextureShadow(0) == &lRasterB, "changed whole texture applies the new raster and nonempty flag");
    lOwner.SetTexture(&lRasterC);
    Check(Shadow::mapTextureState[0] == nullptr && Shadow::mapSamplerState[0] == &lStateB
        && Shadow::mapSamplerTexture[0] == &lRasterC, "bare resource bind invalidates only the whole-unit key");
    const unsigned luBeforeRebind = static_cast<unsigned>(saEvents.size());
    lOwner.SetState(&lStateA);
    Check(saEvents.size() == luBeforeRebind + 2 && saEvents[luBeforeRebind].mbWasUnset,
        "whole texture after bare resource rebinds using whole-key was-unset");
    Shadow::SetState(static_cast<void*>(&lStateB), 0);
    Check(Shadow::mapTextureState[0] == nullptr && Shadow::mapSamplerTexture[0] == &lRasterA,
        "bare sampler bind invalidates whole key while preserving texture key");
    lOwner.SetState(&lStateA);
    Shadow::SetState(&lStateB, 3);
    Check(saEvents.back().muUnit == 3 && saEvents.back().muFlags == 0x10000000u
        && Shadow::mapTextureState[0] == &lStateA && Shadow::mapTextureState[3] == &lStateB,
        "indexed texture unit has original shifted bind mask and independent cache");
    const unsigned luBeforeMismatch = suAssertions;
    const unsigned luBeforeMismatchEvents = static_cast<unsigned>(saEvents.size());
    lStateA.mpRaster = &lRasterC;
    lOwner.SetState(&lStateA);
    Check(suAssertions == luBeforeMismatch + 1 && saEvents.size() == luBeforeMismatchEvents
        && Shadow::mapSamplerTexture[0] == &lRasterA,
        "mutated same texture object without reset asserts instead of silently rebinding");
    lOwner.EndRendering();
    Owner::mgpActiveRenderer = nullptr;
}

static void CheckPreview()
{
    BrnRendererModule lModule;
    StateStorage lStorage;
    InitializeOwner(lModule, lStorage);
    renderengine::Texture lRasterA = {}, lRasterB = {};
    Owner::mgpActiveRenderer = nullptr;
    UnlockStates();
    const CgsDev::RGBA kauColours[] = {0x80402010u, 0x01020304u, 0x000000FFu, 0xFFFFFFFFu};
    for (unsigned luPass = 0; luPass < 4; ++luPass)
    {
        renderengine::Texture* lpRaster = luPass & 1 ? &lRasterB : &lRasterA;
        const unsigned luAssertionsBefore = suAssertions;
        saEvents.clear();
        lModule.mIm2dRenderer.mi8CurrentProgram = 5;
        lModule.DebugDisplayRenderTarget(lpRaster, Vector2{0.25f, 0.125f},
            Vector2{0.5f, 0.25f}, kauColours[luPass]);
        Check(Owner::mgpActiveRenderer == nullptr && suAssertions == luAssertionsBefore,
            "preview opens and closes real ownership without assertion");
        Check(lModule.mIm2dRenderer.mi8CurrentProgram == 0, "preview enters through real slot-zero Begin");
        Check(lModule.mpTextureState == reinterpret_cast<renderengine::TextureState*>(lStorage.maState)
            && lModule.mpTextureState->mpRaster == lpRaster && StorageIntact(lStorage),
            "preview reinitializes the actual persistent resource with the new raster");
        Check(lModule.mTextureStateParams.mpTexture == lpRaster
            && std::memcmp(lModule.mpTextureState->mauSamplerState, &lModule.mTextureStateParams, 32) == 0,
            "preview publishes complete canonical sampler configuration");
        bool lbLanesIntact = true;
        for (u32 luLane = 1; luLane < rw::KU_RESOURCE_LANE_COUNT; ++luLane)
            lbLanesIntact &= lModule.mTextureStateResource.m_baseResources[luLane] ==
                reinterpret_cast<void*>(UINT64_C(0x1234567800000000) + luLane);
        Check(lbLanesIntact, "initializer leaves all other resource lanes untouched");
        Check(saEvents.size() == 5 && saEvents[0].meKind == E_SAMPLER
            && saEvents[0].mbWasUnset && saEvents[1].meKind == E_TEXTURE
            && saEvents[1].mpValue == lpRaster && saEvents[2].meKind == E_BLEND
            && saEvents[2].mpValue == &sStandardBlend && saEvents[3].meKind == E_RASTER
            && saEvents[3].mpValue == &sCullNone && saEvents[4].meKind == E_DRAW,
            "preview orders full texture, standard blend, cull-none and primitive submission");
        bool lbOwnerValid = true;
        for (const Event& lrEvent : saEvents)
            lbOwnerValid &= lrEvent.mpOwner == static_cast<Owner*>(&lModule.mIm2dRenderer);
        Check(lbOwnerValid, "every preview bind and draw occurs inside the real active bracket");
        Check(Shadow::mapTextureState[0] == lModule.mpTextureState
            && Shadow::GetSamplerTextureShadow(0) == lpRaster
            && Shadow::mpBlendState == &sStandardBlend && Shadow::mpRasterizerState == &sCullNone,
            "preview publishes the same cache consumed by subsequent dispatch");
        Check(suCapturedCount == 4 && static_cast<s32>(seCapturedPrimitive) == 6,
            "preview preserves original primitive-six four-vertex strip");
        const f32 kafX[] = {320.0f, 320.0f, 960.0f, 960.0f};
        const f32 kafY[] = {90.0f, 270.0f, 90.0f, 270.0f};
        for (unsigned luVertex = 0; luVertex < 4; ++luVertex)
        {
            const BasicVertex& lrVertex = saCaptured[luVertex];
            const u32 luColour = kauColours[luPass];
            Check(lrVertex.mv4Colour.r == (luColour & 0xFFu)
                && lrVertex.mv4Colour.g == ((luColour >> 8) & 0xFFu)
                && lrVertex.mv4Colour.b == ((luColour >> 16) & 0xFFu)
                && lrVertex.mv4Colour.a == ((luColour >> 24) & 0xFFu),
                "packed preview color reaches named host RGBA lanes low-to-high");
            Check(lrVertex.mv2Pos.x == kafX[luVertex] && lrVertex.mv2Pos.y == kafY[luVertex]
                && lrVertex.mv2Tex0UV.x == (luVertex >= 2 ? 1.0f : 0.0f)
                && lrVertex.mv2Tex0UV.y == (luVertex & 1 ? 1.0f : 0.0f),
                "preview preserves logical coordinates, corner order and complete UV rectangle");
        }
    }
    Shadow::mbBlendStateLocked = Shadow::mbRasteriserStateLocked = true;
    saEvents.clear();
    lModule.DebugDisplayRenderTarget(&lRasterA, Vector2{0.0f, 0.0f}, Vector2{1.0f, 1.0f}, 0xFFFFFFFFu);
    Check(EventCount(E_BLEND) == 0 && EventCount(E_RASTER) == 0
        && Shadow::mpBlendState == nullptr && Shadow::mpRasterizerState == nullptr,
        "preview respects locked blend/raster states after Begin shadow reset");
    Check(EventCount(E_TEXTURE) == 1 && EventCount(E_DRAW) == 1 && Owner::mgpActiveRenderer == nullptr,
        "state locks retain original texture and draw behavior through End");
    UnlockStates();
    Owner::mgpActiveRenderer = nullptr;
}

int main()
{
    CheckBracket();
    CheckTypedStates();
    CheckWholeTexture();
    CheckPreview();
    std::printf("RendererPreviewStateChain: %u checks, %u failures\n", suChecks, suFailures);
    return suFailures ? 1 : 0;
}
