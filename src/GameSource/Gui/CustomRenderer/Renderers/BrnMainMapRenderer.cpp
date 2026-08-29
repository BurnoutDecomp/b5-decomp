// ============================================================================
// BrnMainMapRenderer.cpp -- BrnGui::MainMapRenderer, THE MAP WORLD.
//
// Manager slot 2 (E_MAINMAP): the component that actually draws the Paradise City map
// surface inside the CrashNav / pre-race map chrome. Everything upstream of it was already
// live before this wave -- MainMapComponent::Update publishes GuiEventRenderMainMap (id
// 223) every frame with a one-entry active-texture set (the low-res backdrop built from
// GuiCache resource 199) -- and the event was dropped because this class did not exist
// beyond its compiler-generated constructor.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (Jan-2008). Addresses on each body.
//
// ---------------------------------------------------------------------------
// THE DRAW TARGET
// ---------------------------------------------------------------------------
// The console reaches the buffer as `*lpRendererSet` (the AptIm2dRenderBuffer) and then
// `+ 4` (that object's mCommandBuffer), and it passes BOTH spellings around: r27 to
// Im2dRenderBuffer::SetTransform / PopMask / SetMaskRect, and r27+4 to the
// <Basic2dColouredTexturedVertex> state setters and the vertex submit. On this host both
// collapse to the one named member -- exactly as BrnCrashNavIconRenderer_wK_01.cpp's
// ResolveCrashNavBuffer and BrnBoostBarRenderer.cpp's ResolveBoostBarBuffer already do.
// Nothing here applies an X360 byte offset to a host object.
//
// SEMANTIC-LEVEL SIMD: RenderComponent's corner transform is hand-vectorised VMX128 on the
// console (lvx128 / vperm / vrlimi128 / vmulfp128 / vmaddfp128 / vsldoi -- it transforms
// the tile's world (left,top) and (right,bottom) in one pair of 3x3 applies and packs the
// two results into ONE vector whose lanes the four vertices then read as
// {x0,y0,x1,y1}). That has no portable PC equivalent and is reconstructed at the semantic
// level through the named MapTransform helpers, the policy rw/math/vpu/types.h states and
// BrnMapUtils.cpp / BrnSatNavRenderer.cpp / BrnCrashNavIconRenderer_wK_01.cpp follow.
//
// ASSERTS are lowered to CGS_ASSERT with the recovered expression; the console's own
// BrnMainMapRenderer.cpp line number rides in a trailing comment (CGS_ASSERT stamps
// __FILE__/__LINE__ of THIS file).
// ============================================================================

#include "GameSource/Gui/CustomRenderer/Renderers/BrnMainMapRenderer.h"

#include "GameSource/Gui/BrnGuiCache.h"                     // BrnGui::GuiCache (GetTime)
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"           // BrnGui::MapTransform
#include "GameSource/Gui/SatNav/BrnMapManager.h"            // MapManager::sActiveTextures / SatNavTile::sTexture
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h" // CgsGui::GuiEventLoadNotification
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptRenderHandler.h"  // CgsGui::AptIm2dRenderBuffer
#include "GameShared/GameClasses/Gui/View/ParticleSystem2d/CgsBillboardRenderer.h" // the shared GUI states + screen transform
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h" // CgsDev::Log::gpDebugPrint (the [map-world] diags)
#include "pc/gcm/renderengine/renderstates.h"               // renderengine::TextureState (+Parameters) / BlendState

namespace BrnGui
{
// ---------------------------------------------------------------------------
// ⚠️ PRE-EXISTING ODR FORK -- why SetMaskRect is DECLARED here instead of included.
// Its declaration's home is GameSource/Gui/BrnCustomRendererManager.h, but that header
// now embeds THIS class by value, so including it from this TU would need the manager's
// complete type while BrnMainMapRenderer.h's own include guard is still open. A FUNCTION
// declaration is not a type fork: the signature below is copied verbatim from
// BrnCustomRendererManager.h:218 and links against the one body in BrnCustomRenderer.cpp
// (X360 @0x82450BE0 -- the POINTER overload, which is the one @0x82460130 calls; the same
// call BrnCrashNavIconRenderer_wK_01.cpp reaches through its own local declaration).
// ---------------------------------------------------------------------------
void SetMaskRect(CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex>* lpRenderBuffer,
                 const renderengine::TextureState* lpTextureState,
                 const rw::math::vpu::Vector4& lrv4Rect,
                 const rw::math::vpu::Vector4& lrv4MaskUVs);

namespace
{
    typedef CgsGraphics::ImRenderBuffer<CgsGraphics::Basic2dColouredTexturedVertex> Im2dCommandBuffer;

    // ---- the .rdata floats the bodies read, from the raw image ----------------------
    // (file offset = VA - 0x82000000, big-endian)
    //   flt_82001C98 = 1.0f      flt_82001CC0 = 0.0f
    //   flt_82010C20 = 255.0f    flt_82035570 = -FLT_MAX  (the mfTimeNow sentinel)
    const f32 KF_ALPHA_SCALE   = 255.0f;   // flt_82010C20
    const f32 KF_TIME_SENTINEL = -3.402823466e+38f;   // flt_82035570 == -FLT_MAX

    // The X360's `fneg`/`fsel` pair on both fade arms: max(x,0) then min(x,1).
    f32 Clamp01(f32 lfValue)
    {
        if (lfValue < 0.0f) return 0.0f;
        if (lfValue > 1.0f) return 1.0f;
        return lfValue;
    }

    CgsGraphics::Vector2 MakeV2(f32 lfX, f32 lfY)
    {
        CgsGraphics::Vector2 lv2;
        lv2.x = lfX;
        lv2.y = lfY;
        return lv2;
    }

    Vector4 MakeV4(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        Vector4 lv4;
        lv4.x = lfX; lv4.y = lfY; lv4.z = lfZ; lv4.w = lfW;
        return lv4;
    }

    // The map-space point MapTransform::Transform consumes: the console feeds it a
    // {x, y, 1, ?} vector (the `vrlimi128 v13, v127, 2, 0` that writes 1.0 into lane 2).
    Vector2 MakeMapPoint(f32 lfX, f32 lfY)
    {
        Vector2 lv2;
        lv2.x = lfX;
        lv2.y = lfY;
        lv2.z = 1.0f;
        lv2.w = 0.0f;
        return lv2;
    }

    // RenderComponent's quad colour word, built as
    //   `(((((a << 24) | 0xFFFFFF) >> 16) | 0xFFFF00) << 8) | ((a << 24) >> 24)`
    // == 0xFFFFFF00 | alpha, i.e. big-endian memory order [FF][FF][FF][alpha]. On the host
    // that memory order IS struct RGBA8 (the BrnCrashNavIconRenderer_wK_01.cpp
    // UnpackConsoleColour convention), so it assigns by lane.
    CgsGraphics::RGBA8 MakeWhiteWithAlpha(u8 lu8Alpha)
    {
        CgsGraphics::RGBA8 lColour;
        lColour.r = 0xFFu;
        lColour.g = 0xFFu;
        lColour.b = 0xFFu;
        lColour.a = lu8Alpha;
        return lColour;
    }

    // ClearBackgroundFadeToMapEdges' colour word: `ROR4(mu8Alpha, 8)` == alpha << 24, then
    // re-shuffled to a memory order of [00][00][00][alpha] -- BLACK at the fade alpha.
    // (The inactive arm rewrites the top byte to 0x80 == KU8_INACTIVE_ALPHA.)
    CgsGraphics::RGBA8 MakeBlackWithAlpha(u8 lu8Alpha)
    {
        CgsGraphics::RGBA8 lColour;
        lColour.r = 0u;
        lColour.g = 0u;
        lColour.b = 0u;
        lColour.a = lu8Alpha;
        return lColour;
    }

    CgsGraphics::RGBA8 MakeTransparent()
    {
        CgsGraphics::RGBA8 lColour;
        lColour.r = 0u; lColour.g = 0u; lColour.b = 0u; lColour.a = 0u;
        return lColour;
    }

    // Resolve the 2D command buffer out of the renderer set. Identical to
    // BrnCrashNavIconRenderer_wK_01.cpp's ResolveCrashNavBuffer; see the DRAW TARGET note.
    Im2dCommandBuffer* ResolveMainMapBuffer(CgsGui::ImRendererSet* lpRendererSet)
    {
        if (lpRendererSet == 0)
            return 0;
        CgsGui::AptIm2dRenderBuffer* lpAptBuffer =
            *reinterpret_cast<CgsGui::AptIm2dRenderBuffer* const*>(lpRendererSet);
        return (lpAptBuffer != 0) ? &lpAptBuffer->mCommandBuffer : 0;
    }

    // Event-type ids RecvEvent @0x82449E98 drains (its three switch arms).
    enum
    {
        KI_EVENT_LOAD_NOTIFICATION = 14,   // 0x00E  a loaded TEXTURE -> one of the two masks
        KI_EVENT_SET_CACHE         = 64,   // 0x040  latch the GuiCache pointer
        KI_EVENT_RENDER_MAIN_MAP   = 223   // 0x0DF  the per-frame map view record
    };

    // The request type the case-14 arm accepts (`lwz r11,8(r22); cmpwi 0xB`) and the two
    // load-request ids it dispatches on (`lwz r11,0xC(r22)`; 0xCA / 0xCB). The same two ids
    // BrnCrashNavIconRenderer.cpp names -- 203 MainMapBackgroundMask, 202 PreRaceBackgroundMask
    // -- which is also what REQUESTS them: CrashNavIconRenderer's Prepare is the only
    // EnsureResourcesAreLoaded call site for the pair, so slot 3 going live is what makes
    // these notifications happen at all.
    const s32 KI_REQUESTTYPE_TEXTURE          = 11;
    const u32 KU_LOADID_BACKGROUND_MASK       = 203u;   // 0xCB
    const u32 KU_LOADID_PRERACE_MASK          = 202u;   // 0xCA

    // Build one renderengine texture state over the given backing resource. The PC fold of
    // the X360's `TextureState::GetResourceDescriptor -> allocator vtable +0x10 -> copy 5
    // dwords -> TextureState::Initialize` sequence; the sampler words are the console's own
    // (RecvEvent's stack descriptor @0x8244A048..0x8244A110: 2/2/0, 1/1/2, 0,0, aniso 13,
    // 0, 1, lod 0.0/0.0, 0,0,0, then the byte flags 0,0,0,1,1). Byte-identical to
    // BrnCrashNavIconRenderer.cpp's CreateIconTextureState and BrnSatNavRenderer.cpp's
    // CreatePayloadTextureState -- the same shipped helper, inlined into three call sites.
    renderengine::TextureState* CreateMaskTextureState(rw::Resource* lpBacking, const void* lpTexture)
    {
        renderengine::TextureState::Parameters lParams;
        lParams.muAddressU      = 2u;
        lParams.muAddressV      = 2u;
        lParams.muAddressW      = 0u;
        lParams.muMagFilter     = 1u;
        lParams.muMinFilter     = 1u;
        lParams.muMipFilter     = 2u;
        lParams.muField6        = 0u;
        lParams.muField7        = 0u;
        lParams.muMaxAnisotropy = 13u;
        lParams.muField9        = 0u;
        lParams.muField10       = 1u;
        lParams.mfMipLodBias    = 0.0f;
        lParams.mfField12       = 0.0f;
        lParams.muField13       = 0u;
        lParams.muField14       = 0u;
        lParams.muField15       = 0u;
        lParams.mu8Field40      = 0u;
        lParams.mu8Field41      = 0u;
        lParams.mu8Field42      = 0u;
        lParams.mu8Field43      = 1u;
        lParams.mu8Field44      = 1u;
        lParams.mpTexture       = reinterpret_cast<renderengine::Texture*>(const_cast<void*>(lpTexture));
        return renderengine::TextureState::Initialize(lpBacking, &lParams);
    }
}

// ---------------------------------------------------------------------------
// @0x827DF3E8 -- the C++ constructor. Compiler-generated on the console: it installs the
// class vtable (off_820CF868), zero-fills the SIX five-dword renderengine Resource
// descriptors at guest +0x48/+0x60/+0x78/+0x90/+0xA8/+0xC0 (each written as five stores
// with the compiler's duplicate-first-write unroll artifact; the "gap" dword after each
// 24-byte stride is the state POINTER that follows it, which Construct() seeds instead),
// then runs the four embedded ParticleSystem2d constructors in a count-down loop
// (r30 = 3..0, base +0xF0, stride 0x21A0).
//
// On the host the vtable and the four particle-system sub-object constructions are emitted
// by the compiler, so the body only has to reproduce the POD zero-seeds.
// ---------------------------------------------------------------------------
MainMapRenderer::MainMapRenderer()
{
    for (s32 li = 0; li < 5; ++li)
    {
        mBackgroundMaskTextureStateResource[li] = 0;   // guest +0x48
        mPreRaceMaskTextureStateResource[li]    = 0;   // guest +0x60
        mMapTileBlendStateResource[li]          = 0;   // guest +0x78
        mRouteSegmentTextureStateResource[li]   = 0;   // guest +0x90
        mRouteSegmentBlendStateResource[li]     = 0;   // guest +0xA8
        mPulseTextureStateResource[li]          = 0;   // guest +0xC0
    }
}

// ---------------------------------------------------------------------------
// @0x82446238 -- Construct. Chains the base, parks the stage machine (release starts DONE
// -- nothing to release yet, the same seed BrnCrashNavIconRenderer.cpp:215 documents),
// then clears the bindings, the fade state and the pulse bookkeeping.
//
// STORE-FOR-STORE, in the console's order:
//   *(a1+12)=1 *(a1+8)=0 *(a1+48)=0 *(a1+68)=0 *(a1+92)=0 *(a1+140)=0 *(a1+164)=0
//   *(a1+188)=0 *(a1+212)=0 *(a1+64)=0 *(a1+34672..34684)=0 *(a1+220)=0.0f
//   *(a1+34688)=0 *(a1+224)=0.0f *(a1+216)=0 *(a1+228)=0
// ⚠️ +0x74 (mpPreRaceMaskTextureState) is DELIBERATELY ABSENT: the guest does not re-store
// it here. Its slot was already cleared by the ctor's descriptor run, so this is a guest
// omission that is harmless, not a transcription gap -- do not "fix" it by adding a store.
// ---------------------------------------------------------------------------
void MainMapRenderer::Construct()
{
    CgsGui::CustomRenderComponentInterface::Construct();

    meReleaseStage = E_RELEASESTAGE_DONE;    // stw r9(1), 0xC
    mePrepareStage = E_PREPARESTAGE_START;   // stw r11(0), 8

    // `stw r11, 0x30` -- the one word of the 48-byte mRenderMainMapEvent the console seeds.
    // guest +0x30 == the record's +0x20 == mpActiveTextures, which is the first thing
    // RenderComponent's four-way guard tests. The rest is overwritten wholesale by the
    // first event 223.
    mRenderMainMapEvent.mpActiveTextures = 0;

    mpHeapAllocator              = 0;   // stw r11, 0x44
    mpBackgroundMaskTextureState = 0;   // stw r11, 0x5C
    mpMapTileBlendState          = 0;   // stw r11, 0x8C
    mpRouteSegmentTextureState   = 0;   // stw r11, 0xA4
    mpRouteSegmentBlendState     = 0;   // stw r11, 0xBC
    mpPulseTextureState          = 0;   // stw r11, 0xD4
    mpGuiCache                   = 0;   // stw r11, 0x40

    for (s32 li = 0; li < KI_MAX_PULSES; ++li)
        mfLastProp[li] = 0.0f;          // stw r11, 0(r10)..0xC(r10)  (r10 == this+0x8770)

    mfFadeStartTime = 0.0f;             // stfs f0(flt_82001CC0 == 0.0f), 0xDC
    mbDrawRoute     = false;            // stbx r11, r31, r9(0x8780)
    mfFadeDuration  = 0.0f;             // stfs f0, 0xE0
    meFadeState     = E_FADESTATE_NONE; // stw r11, 0xD8
    mu8Alpha        = 0;                // stb r11, 0xE4
}

// ---------------------------------------------------------------------------
// @0x824462D0 -- Prepare. A two-state machine with NO resource-loading stage: the masks
// arrive later, through RecvEvent's load notifications. The START arm latches the heap
// allocator and falls straight through into DONE, so the component reports prepared on its
// very first pump.
//
// The console's START arm stores, in this order: meReleaseStage = 1, mpHeapAllocator = r5,
// mePrepareStage = 0 -- then the shared tail stores mePrepareStage = 1 and returns true.
// The `mePrepareStage = START` immediately before the tail's `= DONE` is the guest's own
// redundant store (the same shape CustomRendererManager::Prepare @0x82444140 has); it is
// transcribed rather than folded away.
// ---------------------------------------------------------------------------
bool MainMapRenderer::Prepare(CgsGui::GuiEventQueueSmall* lpEventQueue,
                              rw::IResourceAllocator* lpHeapAllocator,
                              rw::IResourceAllocator* lpTextureAllocator)
{
    (void)lpEventQueue;        // r4 -- the console never latches the output queue here
    (void)lpTextureAllocator;  // r6 -- nor the texture allocator

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        meReleaseStage  = E_RELEASESTAGE_DONE;
        mpHeapAllocator = lpHeapAllocator;      // stw r5, 0x44
        mePrepareStage  = E_PREPARESTAGE_START;
        break;

    case E_PREPARESTAGE_DONE:
        break;

    default:
        // The console streams " unknown prepare stage in MainMapRenderer " through
        // StrStream/gpcMessageBuffer; lowered to the static message (console line 130).
        CGS_ASSERT(false, " unknown prepare stage in MainMapRenderer ");
        return false;
    }

    mePrepareStage = E_PREPARESTAGE_DONE;
    return true;
}

// ---------------------------------------------------------------------------
// @0x82446380 -- Release. On the START stage: release the four particle systems, then drop
// each owned render state that is still bound. Both stages fall onto the same tail
// (meReleaseStage = DONE, return true).
//
// ⚠️ mpMapTileBlendState (+0x8C) is NOT in the release list on the console -- five of the
// six state pointers are freed, that one is not. Transcribed as written.
//
// The console frees through the allocator's destroy slot,
// `(*(*mpHeapAllocator + 0x14))(mpHeapAllocator, &resource)`. The allocator type is
// uncommitted in this slice, so the dispatch is OMITTED and the null-out side effects are
// reproduced -- the same call the sibling BrnSatNavRenderer::Release documents rather than
// invents.
// ---------------------------------------------------------------------------
bool MainMapRenderer::Release()
{
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
        meReleaseStage = E_RELEASESTAGE_START;   // the guest's redundant re-store

        // `v4 = a1 + 240; v5 = 4; do { ParticleSystem2d::Release(v4); v4 += 8608; } while (--v5);`
        for (s32 li = 0; li < KI_MAX_PULSES; ++li)
            mParticleSystem[li].Release();

        if (mpBackgroundMaskTextureState) mpBackgroundMaskTextureState = 0;   // +0x5C / res +0x48
        if (mpPreRaceMaskTextureState)    mpPreRaceMaskTextureState    = 0;   // +0x74 / res +0x60
        if (mpRouteSegmentTextureState)   mpRouteSegmentTextureState   = 0;   // +0xA4 / res +0x90
        if (mpRouteSegmentBlendState)     mpRouteSegmentBlendState     = 0;   // +0xBC / res +0xA8
        if (mpPulseTextureState)          mpPulseTextureState          = 0;   // +0xD4 / res +0xC0
        break;

    case E_RELEASESTAGE_DONE:
        break;

    default:
        // Console line 210, streamed through StrStream.
        CGS_ASSERT(false, " unknown release stage in MainMapRenderer ");
        return false;
    }

    meReleaseStage = E_RELEASESTAGE_DONE;
    return true;
}

// ---------------------------------------------------------------------------
// Destruct -- vtable slot 3 (+0x0C) -> 0x822A9750, whose four bytes are `48 5A 33 E8`, an
// unconditional branch to 0x8284CB38 == a bare `blr`. An EMPTY body, ICF-folded with the
// same do-nothing leaf half the GUI renderers share. Reproduced as written.
// ---------------------------------------------------------------------------
void MainMapRenderer::Destruct()
{
}

// ---------------------------------------------------------------------------
// Update -- vtable slot 6 (+0x18) -> 0x8284CB38 == `4E 80 00 20`, a bare `blr`. Empty, and
// folded with the same leaf BrnCrashNavIconRenderer.cpp:437 documents. The fade this class
// owns is advanced inside RenderComponent (through UpdateAlphaForFadeState), not from here
// -- so on the console too the fade is frame-driven by DRAWING.
// ---------------------------------------------------------------------------
void MainMapRenderer::Update()
{
}

// ---------------------------------------------------------------------------
// @0x82C290D8 -- SetRenderEnabled. `stb r4, 4(r3); blr`: the base flag and nothing else.
//
// ⚠️ THIS BODY MOVED HERE from GameShared/.../CustomRenderer/CgsCustomRenderer.cpp, where
// it had been parked with a NON-VIRTUAL `MainMapRenderer* SetRenderEnabled(bool)`
// signature because this class had no base to override. That signature did not match the
// DWARF (`virtual void SetRenderEnabled(bool)`, BrnMainMapRenderer.cpp:377) and so would
// have SHADOWED the base slot rather than overridden it -- the exact defect class the
// H3b wave found on SatNavRenderer::Prepare/RecvEvent/GetID. DecFIGS attributed the leaf
// to CgsCustomRenderer.h only because the leaf is ICF-folded; its identity is this class's.
// ---------------------------------------------------------------------------
void MainMapRenderer::SetRenderEnabled(bool lbRenderEnabled)
{
    mbRenderEnabled = lbRenderEnabled;
}

// ---------------------------------------------------------------------------
// GetRenderLayer -- vtable slot 8 (+0x20) -> 0x82C296C8, whose eight bytes are
// `38 60 00 01 / 4E 80 00 20` == `li r3,1 ; blr`. The map world draws in LAYER 1, the same
// layer BrnCrashNavIconRenderer.cpp:451 recovered from the same ICF'd leaf -- which is what
// puts the icons in the same pass as the world they sit on.
//
// FOR THE RECORD: vtable slot 10 (+0x28, GetNumTextures) is the SAME address, i.e. the
// console's MainMapRenderer::GetNumTextures also returns 1. It is left undeclared here, on
// the sibling's precedent (CrashNavIconRenderer's slot 10 is the identical fold and is
// likewise not declared): with two distinct methods ICF'd onto one `return 1` leaf the
// source-level intent is not recoverable from the vtable alone, and nothing in this build
// asks a map component for a render-to-texture output.
// ---------------------------------------------------------------------------
CgsGui::eCustomRenderLayer MainMapRenderer::GetRenderLayer() const
{
    return CgsGui::E_CUSTOMRENDERLAYER_1;
}

// ---------------------------------------------------------------------------
// @0x824464F8 -- GetID. Constant-folded in the asm:
//   lis r3,-0x6C83 / ori r3,0x607E / sldi r3,32 / oris r3,0x4882 / ori r3,0x4000
// ---------------------------------------------------------------------------
CgsID MainMapRenderer::GetID() const
{
    return 0x937D607E48824000ull;
}

// ---------------------------------------------------------------------------
// @0x82446510 -- StartFade. The manager's event-213 map toggle (sub-mode 0, animated arm)
// calls this through component vtable +0x2C.
// ---------------------------------------------------------------------------
void MainMapRenderer::StartFade(bool lbFadeIn, f32 lfDuration)
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // console line 1160
    if (mpGuiCache == 0)
        return;

    // `v6 = 1; if (!a2) v6 = 2;`
    meFadeState = lbFadeIn ? E_FADESTATE_IN : E_FADESTATE_OUT;

    // The shared GuiCache clock assert (CgsGuiEventTypeDefs.h:250 on the console).
    CGS_ASSERT(mpGuiCache->GetTime() != KF_TIME_SENTINEL, "mfTimeNow!=-FLT_MAX");

    mfFadeStartTime = mpGuiCache->GetTime();
    mfFadeDuration  = lfDuration;
}

// ---------------------------------------------------------------------------
// @0x824465D8 -- ClearFadeState. One store (component vtable +0x30).
// ---------------------------------------------------------------------------
void MainMapRenderer::ClearFadeState()
{
    meFadeState = E_FADESTATE_NONE;
}

// ---------------------------------------------------------------------------
// @0x824465E8 -- UpdateAlphaForFadeState. Roll mu8Alpha from the fade state and the GuiCache
// clock. The FADE-OUT arm is the one that switches the component off when it finishes
// (`(*(*v1 + 28))(v1, 0)` == SetRenderEnabled(false) through vtable +0x1C).
//
// The console's assert text names the function GetAlphaForFadeState (a rename that did not
// reach the message); reproduced verbatim, console line 1258.
// ---------------------------------------------------------------------------
void MainMapRenderer::UpdateAlphaForFadeState()
{
    CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");   // console line 1206
    if (mpGuiCache == 0)
        return;

    CGS_ASSERT(mpGuiCache->GetTime() != KF_TIME_SENTINEL, "mfTimeNow!=-FLT_MAX");
    const f32 lfTimeNow = mpGuiCache->GetTime();

    switch (meFadeState)
    {
    case E_FADESTATE_NONE:
        mu8Alpha = KU8_MAX_ALPHA;   // li r11,0xFF ; stb r11,0xE4
        break;

    case E_FADESTATE_IN:
        if (lfTimeNow < mfFadeStartTime + mfFadeDuration)
        {
            const f32 lfProp = Clamp01((lfTimeNow - mfFadeStartTime) / mfFadeDuration);
            mu8Alpha = static_cast<u8>(static_cast<s32>(lfProp * KF_ALPHA_SCALE));
        }
        else
        {
            meFadeState = E_FADESTATE_NONE;
            mu8Alpha    = KU8_MAX_ALPHA;
        }
        break;

    case E_FADESTATE_OUT:
        if (lfTimeNow < mfFadeStartTime + mfFadeDuration)
        {
            const f32 lfProp = Clamp01(1.0f - (lfTimeNow - mfFadeStartTime) / mfFadeDuration);
            mu8Alpha = static_cast<u8>(static_cast<s32>(lfProp * KF_ALPHA_SCALE));
        }
        else
        {
            SetRenderEnabled(false);
            meFadeState = E_FADESTATE_NONE;
            mu8Alpha    = 0;
        }
        break;

    default:
        CGS_ASSERT(false, "Unhandled fade state in MainMapRenderer::GetAlphaForFadeState");
        mu8Alpha = KU8_MAX_ALPHA;   // the assert arm falls into the FADESTATE_NONE store
        break;
    }
}

// ---------------------------------------------------------------------------
// @0x82449E98 -- RecvEvent. The renderer's whole input surface: the GuiCache bind, the two
// mask textures as they finish loading, and the per-frame map view record.
//
// ⭐ THE id-223 LATCH. `memcpy(this + 16, event, 48)` on the console; on this host the
// record carries a native-width mpActiveTextures pointer, so a literal 48-byte copy would
// TRUNCATE it (the SatNav-212 / PlayAptMovie width precedent) -- typed assignment instead.
// ---------------------------------------------------------------------------
void MainMapRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent != 0, " null event passed ");   // console line 270
    if (lpEvent == 0)
        return;

    switch (liEventType)
    {
    case KI_EVENT_SET_CACHE:
    {
        // The payload carries a NATIVE-WIDTH cache pointer on this host; a word-0 read
        // would truncate it to 32 bits (the SatNavRenderer H3b boot-AV lesson). Typed read.
        GuiCache* const* lppCache = reinterpret_cast<GuiCache* const*>(lpEvent);
        CGS_ASSERT(*lppCache != 0, "lpCacheEvent->mpCachePointer");   // console line 284
        mpGuiCache = *lppCache;   // stw r11, 0x40
        break;
    }

    case KI_EVENT_LOAD_NOTIFICATION:
    {
        const CgsGui::GuiEventLoadNotification* lpcNotification =
            reinterpret_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);

        // `lwz r11,0(r22); lwz r11,0(r11)` -- the resource memory's first word IS the
        // loaded object pointer (the identical indirection BrnGuiCache.cpp:362 performs on
        // the same notification before storing maResources[id].mpResource).
        const void* lpTexture = 0;
        if (lpcNotification->mResourceHandle.mpResourceMemory != 0)
        {
            lpTexture = *reinterpret_cast<void* const*>(
                lpcNotification->mResourceHandle.mpResourceMemory);
        }
        CGS_ASSERT(lpTexture != 0,
                   "Invalid resource data sent MainMapRenderer::RecvEvent");   // console line 294

        if (static_cast<s32>(lpcNotification->meRequestType) != KI_REQUESTTYPE_TEXTURE)
            break;   // `lwz r11,8(r22); cmpwi 0xB; bne default`

        if (lpcNotification->muLoadRequestId == KU_LOADID_BACKGROUND_MASK)
        {
            CGS_ASSERT(lpTexture != 0, "lpTexture!=NULL");   // console line 305
            if (lpTexture == 0)
                break;
            mBackgroundMaskTextureStateResource[0] = 0;   // the console's descriptor slot
            mpBackgroundMaskTextureState =
                CreateMaskTextureState(&mBackgroundMaskTextureStateBacking, lpTexture);
        }
        else if (lpcNotification->muLoadRequestId == KU_LOADID_PRERACE_MASK)
        {
            CGS_ASSERT(lpTexture != 0, "lpTexture!=NULL");   // console line 323
            if (lpTexture == 0)
                break;
            mPreRaceMaskTextureStateResource[0] = 0;
            mpPreRaceMaskTextureState =
                CreateMaskTextureState(&mPreRaceMaskTextureStateBacking, lpTexture);
        }
        break;
    }

    case KI_EVENT_RENDER_MAIN_MAP:
        mRenderMainMapEvent = *reinterpret_cast<const GuiEventRenderMainMap*>(lpEvent);
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// @0x8245A5F0 -- ClearBackgroundFadeToMapEdges. The map's background, drawn UNDER the
// tiles: three full-height quads across the published view rect.
//
//   * first it walks the active-texture set and folds the DRAWN horizontal extent:
//         lfMapLeft  = max(0, all transformed mBBWorld left edges)
//         lfMapRight = min(1, all transformed mBBWorld right edges)
//     (`fsel f31, f31-LTx, f31, LTx` == max; `fsel f30, f30-RBx, RBx, f30` == min; the
//      seeds are 0.0 and 1.0, so the band is clamped into the unit view.)
//   * then three quads at the SAME per-frame colour:
//         [viewRect.x .. lfMapLeft ]  transparent -> black    (the left fade bar)
//         [lfMapLeft  .. lfMapRight]  solid black             (the map's own backdrop)
//         [lfMapRight .. viewRect.z]  black -> transparent    (the right fade bar)
//     all spanning viewRect.y..viewRect.w, UVs (0,0)(0,1)(1,0)(1,1).
//
// The colour is `ROR4(mu8Alpha, 8)` re-shuffled to memory order [00][00][00][alpha] --
// BLACK at the fade alpha -- with the inactive arm overwriting the alpha byte with 0x80.
//
// ⚠️ THE TWO Matrix44 PARAMETERS THE DWARF DECLARES ARE DEAD. @0x8245A5F0 never touches r7
// or r8, and RenderComponent passes the SAME stack slot (sp+0xC0) for both -- a slot that
// by then holds MakeCoordSpaceFromRect(mv4MapRect), because the compiler reused it for the
// GetNormalisedToRendererTransform result AND the GetGuiCamera block AND that Matrix33 in
// turn. Both of those two calls are therefore DEAD CODE in this build (their results are
// overwritten before any read). Carrying two ignored Matrix44 arguments would only make
// that fiction look load-bearing, so the parameters are dropped and the fact recorded here.
// ---------------------------------------------------------------------------
void MainMapRenderer::ClearBackgroundFadeToMapEdges(Im2dCommandBuffer* lpRenderBuffer,
                                                    const Vector4& lrv4ViewRect,
                                                    u32 luTextureCount,
                                                    const Matrix33& lrm33MapToView)
{
    if (lpRenderBuffer == 0)
        return;

    // The per-frame background alpha: the fade alpha while the map is live or fading,
    // otherwise the flat inactive value.
    u8 lu8Alpha = mu8Alpha;
    if (!mRenderMainMapEvent.mbIsActive && meFadeState == E_FADESTATE_NONE)
        lu8Alpha = KU8_INACTIVE_ALPHA;

    // ---- fold the drawn horizontal extent -------------------------------------------
    f32 lfMapLeft  = 0.0f;   // _FP31, seeded 0.0
    f32 lfMapRight = 1.0f;   // _FP30, seeded 1.0

    const MapManager::sActiveTextures* lpcActive = mRenderMainMapEvent.mpActiveTextures;
    if (luTextureCount != 0 && lpcActive != 0)
    {
        for (u32 luIndex = 0; luIndex < luTextureCount; ++luIndex)
        {
            const SatNavTile::sTexture& lrTexture = lpcActive->maTextures[luIndex];
            if (lrTexture.mpTextureState == 0)
                continue;

            const Vector2 lv2TopLeft = MapTransform::Transform(
                MakeMapPoint(lrTexture.mBBWorld.mfLeft, lrTexture.mBBWorld.mfTop),
                lrm33MapToView);
            const Vector2 lv2BottomRight = MapTransform::Transform(
                MakeMapPoint(lrTexture.mBBWorld.mfRight, lrTexture.mBBWorld.mfBottom),
                lrm33MapToView);

            if (lv2TopLeft.x     > lfMapLeft)  lfMapLeft  = lv2TopLeft.x;
            if (lv2BottomRight.x < lfMapRight) lfMapRight = lv2BottomRight.x;
        }
    }

    // ---- the three quads --------------------------------------------------------------
    // `SetTexture(a2+4, dword_83010F58)` -- an entry of the shared GUI state library. FLAG
    // (state-library global, not modelled in this slice -- the CgsGuiViewModule
    // RenderBlackScreen precedent): these three quads are flat vertex-coloured gradients,
    // so the untextured bind is the correct visual and is what is emitted here.
    lpRenderBuffer->SetTexture(0);
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateStandard);   // sub_82458EC0(a2+4, dword_83010F20)

    // `SetTransform(a2, &flt_830112D0)` -- the CONSTANT unit-to-screen transform (the same
    // data global CgsGuiViewModule::RenderBlackScreen binds: origin (0,0), right (1280,0),
    // up (0,720), identity 0..255 colour transform). Built here rather than referenced,
    // because the global has no committed home; see that function's note for the pinning.
    CgsGraphics::Im2dTransform lUnitToScreen;
    lUnitToScreen.mOriginXYZ.SetZero();
    lUnitToScreen.mRightUp.x = 1280.0f;
    lUnitToScreen.mRightUp.y = 0.0f;
    lUnitToScreen.mRightUp.z = 0.0f;
    lUnitToScreen.mRightUp.w = 720.0f;
    lUnitToScreen.mColourShift.SetZero();
    lUnitToScreen.mColourScale.x = 255.0f;
    lUnitToScreen.mColourScale.y = 255.0f;
    lUnitToScreen.mColourScale.z = 255.0f;
    lUnitToScreen.mColourScale.w = 255.0f;
    lpRenderBuffer->SetTransform(lUnitToScreen);

    const CgsGraphics::RGBA8 lBlack       = MakeBlackWithAlpha(lu8Alpha);
    const CgsGraphics::RGBA8 lTransparent = MakeTransparent();

    const f32 lfTop    = lrv4ViewRect.y;
    const f32 lfBottom = lrv4ViewRect.w;

    CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];
    laVertices[0].mv2Tex0UV = MakeV2(0.0f, 0.0f);
    laVertices[1].mv2Tex0UV = MakeV2(0.0f, 1.0f);
    laVertices[2].mv2Tex0UV = MakeV2(1.0f, 0.0f);
    laVertices[3].mv2Tex0UV = MakeV2(1.0f, 1.0f);

    // quad 1 -- the left bar: transparent at the view edge, black at the map's left edge.
    laVertices[0].mv2Pos = MakeV2(lrv4ViewRect.x, lfTop);    laVertices[0].mv4Colour = lTransparent;
    laVertices[1].mv2Pos = MakeV2(lrv4ViewRect.x, lfBottom); laVertices[1].mv4Colour = lTransparent;
    laVertices[2].mv2Pos = MakeV2(lfMapLeft,      lfTop);    laVertices[2].mv4Colour = lBlack;
    laVertices[3].mv2Pos = MakeV2(lfMapLeft,      lfBottom); laVertices[3].mv4Colour = lBlack;
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

    // quad 2 -- the map's own backdrop, solid.
    laVertices[0].mv2Pos = MakeV2(lfMapLeft,  lfTop);    laVertices[0].mv4Colour = lBlack;
    laVertices[1].mv2Pos = MakeV2(lfMapLeft,  lfBottom); laVertices[1].mv4Colour = lBlack;
    laVertices[2].mv2Pos = MakeV2(lfMapRight, lfTop);    laVertices[2].mv4Colour = lBlack;
    laVertices[3].mv2Pos = MakeV2(lfMapRight, lfBottom); laVertices[3].mv4Colour = lBlack;
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);

    // quad 3 -- the right bar: black at the map's right edge, transparent at the view edge.
    laVertices[0].mv2Pos = MakeV2(lfMapRight,     lfTop);    laVertices[0].mv4Colour = lBlack;
    laVertices[1].mv2Pos = MakeV2(lfMapRight,     lfBottom); laVertices[1].mv4Colour = lBlack;
    laVertices[2].mv2Pos = MakeV2(lrv4ViewRect.z, lfTop);    laVertices[2].mv4Colour = lTransparent;
    laVertices[3].mv2Pos = MakeV2(lrv4ViewRect.z, lfBottom); laVertices[3].mv4Colour = lTransparent;
    lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
}

// ---------------------------------------------------------------------------
// @0x82460130 -- RenderComponent. THE DRAWER.
//
// The console's shape, in order:
//   1. four-way guard: mpActiveTextures, mpGuiCache, mpBackgroundMaskTextureState and
//      mpPreRaceMaskTextureState must ALL be bound, or the whole body is skipped;
//   2. UpdateAlphaForFadeState(), then pick the quad alpha (the fade alpha while the map is
//      active or fading, otherwise KU8_INACTIVE_ALPHA);
//   3. CgsGui::GetNormalisedToRendererTransform() and CgsGui::GetGuiCamera() -- BOTH DEAD
//      (see the ClearBackgroundFadeToMapEdges note: their results are written into the same
//      stack slot MakeCoordSpaceFromRect then overwrites, and nothing reads them in
//      between). They are not called here; the fact is recorded rather than the call faked;
//   4. the map->view transform:
//         MakeTransform(MakeCoordSpaceFromRect(mv4MapRect), MakeCoordSpaceFromRect(mv4ViewRect))
//      -- the same pair BrnCrashNavIconRenderer_wK_01.cpp's RenderEventIcon feeds to
//      MapTransform::Transform, which is why the icons land on the world this draws;
//   5. open the block, pick the mask texture by map type (and, for LOADING_MAP, push a
//      black SetClear first), push the map's clip mask over the view rect at UV {0,0,1,1};
//   6. ClearBackgroundFadeToMapEdges;
//   7. bind the ADDITIVE blend (dword_83010F24) and the shared billboard screen transform;
//   8. one 4-vertex quad per active texture with a non-null mpTextureState: transform the
//      tile's world (left,top) and (right,bottom) through the map->view transform and emit
//      TL/BL/TR/BR at UV (0,0)/(0,1)/(1,0)/(1,1), coloured white at the frame alpha;
//   9. pop the mask, close the block.
//
// THE STRIDE-36 LOOP: `v25 = *(mpActiveTextures + 684)` is
// MapManager::sActiveTextures::muTextureCount, and the 0x24 step is SatNavTile::sTexture
// (mpTextureState +0x00, mBB +0x04, mBBWorld +0x14). The reads at +0x14/+0x18/+0x1C/+0x20
// are mBBWorld's left/top/right/bottom. All by name here.
// ---------------------------------------------------------------------------
void MainMapRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    // [DIAG] NOT IN THE X360 BINARY -- the wave witness, one line every 120 draws: which of
    // the four guard bindings this component actually has, and how many tiles the published
    // set carries. It is what proved the two mask states were never arriving (the third
    // notification seat, now added in BrnGuiModule.cpp) and it is the first thing to read if
    // the map screen ever goes blank again. Same shape/cadence policy as the [satnav-diag]
    // line in BrnSatNavRenderer.cpp.
    {
        static s32 siCall = 0;
        if ((siCall++ % 120) == 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[map-world] render: tex=" << static_cast<s32>(mRenderMainMapEvent.mpActiveTextures != 0)
                << " cache=" << static_cast<s32>(mpGuiCache != 0)
                << " bgmask=" << static_cast<s32>(mpBackgroundMaskTextureState != 0)
                << " prmask=" << static_cast<s32>(mpPreRaceMaskTextureState != 0)
                << " count=" << (mRenderMainMapEvent.mpActiveTextures != 0
                                     ? static_cast<s32>(mRenderMainMapEvent.mpActiveTextures->muTextureCount) : -1)
                << "\n";
        }
    }

    // (1) the console's four-way guard, verbatim (+0x30 / +0x40 / +0x5C / +0x74).
    if (mRenderMainMapEvent.mpActiveTextures == 0 ||
        mpGuiCache                           == 0 ||
        mpBackgroundMaskTextureState         == 0 ||
        mpPreRaceMaskTextureState            == 0)
    {
        return;
    }

    Im2dCommandBuffer* lpRenderBuffer = ResolveMainMapBuffer(lpRendererSet);
    if (lpRenderBuffer == 0)
        return;   // [FLAG PC bring-up] the console derefs the set straight through.

    // (2) roll the fade, then pick this frame's alpha.
    UpdateAlphaForFadeState();

    u8 lu8Alpha = KU8_INACTIVE_ALPHA;                       // li r28, 0x80
    if (mRenderMainMapEvent.mbIsActive || meFadeState != E_FADESTATE_NONE)
        lu8Alpha = mu8Alpha;                                // lbz r28, 0xE4

    // (4) the map->view transform.
    const Matrix33 lm33MapToView = MapTransform::MakeTransform(
        MapTransform::MakeCoordSpaceFromRect(mRenderMainMapEvent.mv4MapRect),
        MapTransform::MakeCoordSpaceFromRect(mRenderMainMapEvent.mv4ViewRect));

    const u32 luTextureCount = mRenderMainMapEvent.mpActiveTextures->muTextureCount;

    // (5) open the block and pick the mask.
    lpRenderBuffer->BeginRendering();                        // sub_824587B0(v3 + 4)

    const renderengine::TextureState* lpMaskTextureState = mpBackgroundMaskTextureState;
    if (mRenderMainMapEvent.meMapType == GuiEventRenderMainMap::E_MAPTYPE_PRERACE)
    {
        lpMaskTextureState = mpPreRaceMaskTextureState;      // lwz r4, 0x74
    }
    else if (mRenderMainMapEvent.meMapType == GuiEventRenderMainMap::E_MAPTYPE_LOADING_MAP)
    {
        // The console pushes an opaque-black clear first (the guarded function-local static
        // at qword_82FB3790 is {0,0,0,1}), then falls onto the BACKGROUND mask.
        // FLAG: ImRenderBuffer<V>::SetClear (IM_CMD_SET_CLEAR, opcode 13) has a command
        // record in CgsImRenderBufferTemplate.h but no committed setter in this slice, so
        // the call is DOCUMENTED, not stubbed with a substitute -- the same policy
        // CgsCustomRenderer.cpp's Render() states for the five missing Im2d state entry
        // points. This arm is unreachable on the main map (meMapType == E_MAPTYPE_MAINMAP).
    }
    else if (mRenderMainMapEvent.meMapType != GuiEventRenderMainMap::E_MAPTYPE_MAINMAP)
    {
        // Console line 492, streamed as "Unhandled map type <n> in
        // MainMapRenderer::RenderComponent()"; the arm still falls onto the background mask.
        CGS_ASSERT(false, "Unhandled map type in MainMapRenderer::RenderComponent()");
    }

    // The mask covers the whole published view rect at the mask texture's full UV range
    // (the `vspltisw 1 / vcsxwfp128 / vperm / vsldoi` block builds exactly {0,0,1,1}).
    //
    // ✅ CLOSED 2026-08-29 -- THE CROP WAS A TEXTURE-CONVERTER DEFECT, NOT A RENDER-PATH ONE.
    // Nothing in this TU had to change. The two sampler/address-mode fixes recorded below
    // STAND (they were real divergences); the third candidate -- "PushMask is really a
    // SCISSOR and the texture fold is a PC embellishment" -- is REFUTED: the console does
    // fold the mask texture, and the shipped mask is a full-pass one.
    //
    // ROOT CAUSE (parent repo, tools/assets/bundles/x360_tex.py): the Xenos packed-mip-tail
    // slot table was being applied to level 0 of a chain whose packed_mip_base is 0. A
    // base == 0 level 0 is the BASE image and sits at the tile ORIGIN; the table's first slot
    // is (0,4) blocks for a wide texture / (4,0) for a square-or-tall one, so the converter
    // read a region of the 32x32-block tile that is entirely zero. GUI texture 203
    // (0xB3E5FAA5 MainMapBackgroundMask, 32x8) and 202 (0xBFF04731 PreRace mask, 256x8) are
    // the only base == 0 entries in GUITEXTURES.BIN, which is why only the map masks broke.
    // Untiling the retail X360 payload proves the real asset: 16/16 (203) and 128/128 (202)
    // DXT5 blocks non-zero, colour PURE WHITE over the full extent, with an alpha ramp along
    // the short edge -- i.e. exactly the full-pass mask the colour-modulate convention wants.
    // The "2 surviving blocks = 25% x 50%" measurement below was of the SHIPPED build/game
    // bundle, which an older converter path had broken the same way; build/game/GUITEXTURES.BIN
    // has been reconverted (old copy kept as GUITEXTURES.BIN.pre-maskfix) and the map world now
    // fills the view rect (boot-verified, scratch/mainmenu_wave/rc_run1..2).
    // Do NOT re-litigate the sampler: it is measured correct.
    //
    // (the record of how the seam was located, kept because the reasoning is still useful)
    //
    // ⭐ MEASURED 2026-08-29 (runs scratch/mainmenu_wave/maskprobe1..3, probes since removed;
    // the extracted asset is scratch/mainmenu_wave/mask203/tex203.dxt5):
    //   1. The hardcoded stage-1 D3DTADDRESS_BORDER WAS a divergence and IS FIXED: the Im2d
    //      dispatch now honours the address modes the bound TextureState carries
    //      (CgsImRenderBufferTemplate.cpp, IM_CMD_PUSH_MASK, the ⭐ [map-world 2026-08-29]
    //      block). Verified LIVE -- the map mask push now reports addr=3/3 (D3DTADDRESS_CLAMP,
    //      renderengine mode 2) where it used to report 4/4. It also un-erases
    //      BoostBarRenderer::SetChainedInactiveMask, whose 0..20*width U window was being
    //      cut to the first repeat by the zero border.
    //   2. THE CROP SURVIVED THAT FIX, because the premise under it is false for THIS asset:
    //      clamping does not "stretch edge pixels across the rect" when the edge texels are
    //      black. The mask push is measured as rect=(0,122.2)-(1280,604) uv=(0,0)-(1,1),
    //      raster 32x8, D3D texture 32x8, 1 mip -- no extent mismatch, no UV mismatch.
    //   3. THE MASK ASSET ITSELF IS ~EMPTY. GUITEXTURES.BIN entry [12], resource id
    //      0xB3E5FAA5 (GUI texture 203, MainMapBackgroundMask): header 32x8 DXT5, mips=1,
    //      gfx payload 16384 bytes -- of whose 1024 DXT5 blocks exactly TWO are non-zero
    //      (blocks 0 and 1 = texels x0..7, y0..3: colour white, alpha 5 and a 25..105 ramp).
    //      Every other block is all-zero = black, alpha 0. The X360 source bundle carries the
    //      same thing (that entry's compressed payload is 128 bytes for a 16384-byte
    //      resource), so this is NOT the PC converter dropping data on the floor.
    //      ^^ THAT LAST SENTENCE WAS WRONG, and it is the reason this took two waves: a
    //      128-byte deflate of a 16 KB X360 GPU tile is what a MOSTLY-ZERO TILE looks like,
    //      and a base == 0 tile IS mostly zero -- an 8x2-block image inside a 32x32-block
    //      tile is 1.5% occupancy. "The source is sparse too" is not evidence that the
    //      converter read the right 1.5%. Untiling the tile and printing its non-zero block
    //      map settled it in one command; inferring from compressed sizes did not.
    //      8/32 = 25% and 4/8 = 50% -- and the crop measures 25.4% x 53.5% of the view rect
    //      (map content ends at x~335 of 1280, y~380 of the 122..604 band). The crop IS the
    //      mask's colour content, folded in by the opcode-17 COLOUR-modulate convention.
    //   ADJUDICATED: the question "what does the console do with an all-but-empty mask raster"
    //   was the wrong question -- the console's raster is NOT empty. Of the three candidates,
    //   (a) SCISSOR is REFUTED, (b) runtime-filled surface is REFUTED, and (c) "the shipped
    //   32x8 header is the wrong level of a larger chain" was the near miss: the header is
    //   right, the converter was reading the wrong PLACE inside the right tile.
    //
    // (was, and still the record of how the seam was located)
    // ✅ (CLOSED -- see the banner above) the wave-1 measurement, verbatim (2026-08-29, runs
    // scratch/mainmenu_wave/mapworld_geo, _nomask, _mask, _red): the map world DRAWS and its
    // geometry is right, but this mask crops it to roughly the top-left 28% x 53% of the view
    // rect. Three probes pin it:
    //   * quad UNTEXTURED, mask on   -> a hard-edged solid box at that same 28% x 53%;
    //   * mask push REMOVED          -> the quad covers the whole screen (so the transform
    //                                   chain, the corner pack and the vertex order are all
    //                                   correct -- the geometry is not the bug);
    //   * mask texture measured      -> GUI texture 203 (MainMapBackgroundMask) arrives 32x8,
    //                                   while every other type-11 texture in the same boot is
    //                                   sane (206/204/235 = 256x256, 5/10 = 512x256, ...).
    // So the crop is the mask ASSET/sampling path, shared substrate: the console's mask
    // texture state (built in RecvEvent above) asks for address mode 2 on both axes, while
    // the PC Im2d dispatch hardcodes stage-1 D3DTADDRESS_BORDER with a zero border
    // (CgsImRenderBufferTemplate.cpp, IM_CMD_PUSH_MASK). BrnCrashNavIconRenderer's
    // RenderComponent pushes the SAME mask over the SAME rect, so whatever fixes it fixes
    // both. NEXT STEP: dump texture 203 out of GUITEXTURES.BIN and compare its converted
    // extent/mip against the console's, before touching the dispatcher's sampler states --
    // those were calibrated against the boost-bar and sat-nav masks (see the hud-H3b note on
    // the two mask-asset conventions at that opcode) and must not be changed blind.
    // (That NEXT STEP is what finally closed it -- the dump, not the reasoning.)
    SetMaskRect(lpRenderBuffer, lpMaskTextureState,
                mRenderMainMapEvent.mv4ViewRect, MakeV4(0.0f, 0.0f, 1.0f, 1.0f));

    // (6) the background, under everything.
    ClearBackgroundFadeToMapEdges(lpRenderBuffer, mRenderMainMapEvent.mv4ViewRect,
                                  luTextureCount, lm33MapToView);

    // (7) the tile pass state.
    lpRenderBuffer->SetState(CgsGui::gpGuiBlendStateAdditive);      // sub_82458EC0(v3+4, dword_83010F24)
    lpRenderBuffer->SetTransform(CgsGui::gBillboardScreenTransform); // SetTransform(v3, unk_83011090)

    // (8) one quad per active texture.
    const CgsGraphics::RGBA8 lColour = MakeWhiteWithAlpha(lu8Alpha);
    const MapManager::sActiveTextures* lpcActive = mRenderMainMapEvent.mpActiveTextures;

    for (u32 luIndex = 0; luIndex < luTextureCount; ++luIndex)
    {
        const SatNavTile::sTexture& lrTexture = lpcActive->maTextures[luIndex];
        if (lrTexture.mpTextureState == 0)
            continue;   // `lwz r4, 0(r11); beq LABEL` -- skip, do not stop

        // The console transforms BOTH corners in one vectorised pass and packs the result
        // as {x0,y0,x1,y1}; lanes 0/1 are the (left,top) corner and 2/3 the
        // (right,bottom) one (the vsldoi takes the LT half first). See the SIMD note.
        const Vector2 lv2TopLeft = MapTransform::Transform(
            MakeMapPoint(lrTexture.mBBWorld.mfLeft, lrTexture.mBBWorld.mfTop),
            lm33MapToView);
        const Vector2 lv2BottomRight = MapTransform::Transform(
            MakeMapPoint(lrTexture.mBBWorld.mfRight, lrTexture.mBBWorld.mfBottom),
            lm33MapToView);

        CgsGraphics::Basic2dColouredTexturedVertex laVertices[4];
        laVertices[0].mv2Pos = MakeV2(lv2TopLeft.x,     lv2TopLeft.y);
        laVertices[1].mv2Pos = MakeV2(lv2TopLeft.x,     lv2BottomRight.y);
        laVertices[2].mv2Pos = MakeV2(lv2BottomRight.x, lv2TopLeft.y);
        laVertices[3].mv2Pos = MakeV2(lv2BottomRight.x, lv2BottomRight.y);
        laVertices[0].mv2Tex0UV = MakeV2(0.0f, 0.0f);
        laVertices[1].mv2Tex0UV = MakeV2(0.0f, 1.0f);
        laVertices[2].mv2Tex0UV = MakeV2(1.0f, 0.0f);
        laVertices[3].mv2Tex0UV = MakeV2(1.0f, 1.0f);
        for (s32 li = 0; li < 4; ++li)
            laVertices[li].mv4Colour = lColour;

        // The tile's own texture state (SatNavTile::sTexture::mpTextureState is typed
        // CgsGraphics::TextureState* in BrnSatNavTile.h; the buffer's SetState overload
        // takes the renderengine state the dispatcher unwraps -- the same handle).
        lpRenderBuffer->SetState(
            reinterpret_cast<const renderengine::TextureState*>(lrTexture.mpTextureState));
        lpRenderBuffer->Render(static_cast<renderengine::PrimitiveType>(6), laVertices, 4);
    }

    // (9)
    lpRenderBuffer->PopMask();        // Im2dRenderBuffer::PopMask(v3)
    lpRenderBuffer->EndRendering();   // sub_82458898(v3 + 4)
}

}
