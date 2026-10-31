// CgsRenderTarget.cpp - Burnout's facade over an EATech post-fx render target.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   CgsRenderTarget::CgsRenderTarget              @ 0x823F47A0
//   CgsRenderTarget::Construct                    @ 0x827ECC28   (vtable slot 0)
//   CgsRenderTarget::Destruct                     @ 0x8284CB38   (vtable slot 1 - empty, ICF-folded)
//   CgsRenderTarget::Prepare                      @ 0x82C296C8   (vtable slot 2 - `return true`, ICF-folded)
//   CgsRenderTarget::GetTexture                   @ 0x827E74A8
//   CgsRenderTarget::GetDepthTexture              @ 0x827E7528
//   CgsRenderTarget::SetRenderTargetState         @ 0x827E7588
//   CgsRenderTarget::SetRenderTargetStateInvertDepth @ 0x827E7668
//   CgsRenderTarget::Begin                        @ 0x827E7748
//   CgsRenderTarget::End                          @ 0x827E7758
//
// The three build-side virtual addresses were previously parked as "not resolved". They are not
// reachable by name (only Construct carries a symbol, and its JSON export is one of the gaps in
// .ida-exports/) - they were recovered from the class's vtable, off_820460B4, which the ctor installs
// at +0x00. The vtable holds exactly three slots (slot 3 onward is the "liIndex >=0 && liIndex<4"
// string literal), matching the DWARF's three virtuals in order. Slots 1 and 2 point at bodies IDA
// labels with OTHER classes' names (BaseCollisionGenerator::Destruct / Playback::Content::DoOnPostLoad)
// because the linker ICF-folded them: a lone `blr` and a lone `li r3,1; blr`. So on the X360 build
// CgsRenderTarget::Destruct is EMPTY and CgsRenderTarget::Prepare just returns true (the PS3/DWARF
// build has real bodies at CgsRenderTarget.cpp:205/221 - an RSX teardown the X360 does not need).
//
// Behaviour + calling convention verified against the X360 asm (the compiler inlined the
// rw::graphics::postfx::RenderTarget accessors the C++ called; they are un-inlined back to named
// calls here, per the reconstruction rules).

#include "GameShared/GameClasses/Graphics/CgsRenderTarget.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/texturestate.h"

// One surface record's default state, as the X360 ctor (0x823F47A0) seeds it: "unused, no
// compression, no ZCull" - filter mode 1, not-in-use, no system memory, texture type 1, and all of
// the EDRAM-tile / ZCull / compression-base fields cleared to -1 (the X360 "unassigned" sentinel);
// the shared-target pointers start null. Fields the asm does NOT write (use-device, buffer/texture
// format, base EDRAM) are left for the serialise-side setters, so they are not touched here either.
// The X360 compiler inlined this constructor five times - once per record - into the class ctor.
CgsRenderTarget::CgsRenderTargetSurface::CgsRenderTargetSurface()
{
    mu32FilterMode       = 1;
    mbInUse              = false;
    mbUseSystemMemory    = false;
    miPS3CompressionBase = -1;
    ms8TileIndex         = -1;
    ms8ZCullIndex        = -1;
    miZCullAddress       = -1;
    mpTarget             = 0;
    mu32TextureType      = 1;
    mpSharedTarget       = 0;
    mpSharedAddress      = 0;
}

// Construct an empty render-target description: the vtable goes in at +0x00, the five surface records
// take their defaults above (four colour + the depth/stencil one), and the section count starts at 1.
//
// Stores reconstructed by name from the X360 asm (offsets there: maRenderTargets at +0x18, stride
// 0x30, mDepthTarget at +0xD8, mpRenderTarget at +0x108); this recon keeps the semantics, not the
// byte offsets. The X360 ctor does not write mpRenderTarget or mpAllocator - the built post-fx target
// and its allocator are installed later by Construct() - so they are not seeded here.
CgsRenderTarget::CgsRenderTarget()
{
    mu8NumSections = 1;
}

// ================================================================================================
// Construct  @ 0x827ECC28  (vtable slot 0)
// ================================================================================================
// Turn the serialised description this object has been seeded with into a live post-fx render target:
// flatten every surface record into a rw::graphics::postfx::RenderTarget::Parameters block, derive the
// colour / depth-stencil creation modes from the in-use flags, and hand the block to
// RenderTarget::Initialize. The returned object lands in mpRenderTarget.
//
// The colour mode is derived, not stored: NONE when no colour section is in use, otherwise
// USE_DEVICE_FOR_WRITE if ANY in-use colour section is device-written and CREATE if none is. (The
// X360 spells that as `if (anyInUse) mode = (anyUseDevice != 0) + 1; else mode = 0` over two bytes
// OR-accumulated across the loop - the enum values 1 and 2 are adjacent.) The depth mode is
// USE_PROVIDED when the depth record already carries a Target*, else CREATE/NONE from its in-use flag.
void CgsRenderTarget::Construct(rw::IResourceAllocator* lpAllocator)
{
    CGS_ASSERT(lpAllocator != 0, "Must have allocator to construct render target\n");

    mpAllocator = lpAllocator;

    rw::graphics::postfx::RenderTarget::Parameters lParameters;

    lParameters.SetAllocator(lpAllocator);
    lParameters.SetWidth(mu32MaxWidth);
    lParameters.SetHeight(mu32MaxHeight);
    // NOTE: the X360 hard-codes 1 here (li r11,1 -> +0x0C); it does NOT forward mu32MaxNumMipMaps.
    lParameters.SetNumMipMaps(1);
    lParameters.SetUseDepthStencilAsTexture(mbUseDepthStencilAsTexture);
    lParameters.SetNumSections(mu8NumSections);

    // --- the colour sections -----------------------------------------------------------------------
    bool lbAnyColourInUse     = false;
    bool lbAnyColourUseDevice = false;

    for (u32 luSection = 0; luSection < KU_NUM_COLOUR_SECTIONS; ++luSection)
    {
        const CgsRenderTargetSurface&             lrSurface = maRenderTargets[luSection];
        rw::graphics::postfx::Target::Parameters& lrColour  = lParameters.GetColorTargetParameters(luSection);

        lbAnyColourInUse     = lbAnyColourInUse     || lrSurface.mbInUse;
        lbAnyColourUseDevice = lbAnyColourUseDevice || lrSurface.mbUseDevice;

        lrColour.SetAllocator(lpAllocator);
        lrColour.SetWidth(mu32MaxWidth);
        lrColour.SetHeight(mu32MaxHeight);
        lrColour.SetBufferFormat(lrSurface.mu32BufferFormat);
        lrColour.SetTextureFormat(lrSurface.mu32TextureFormat);
        lrColour.SetTextureType(lrSurface.mu32TextureType);
        lrColour.SetMultisampleFormat(static_cast<u32>(mn32MultiSampleFormat));
        lrColour.SetFilterMode(lrSurface.mu32FilterMode);
        lrColour.SetBaseEDRAM(static_cast<s32>(lrSurface.mu32BaseEDRAM));
        lrColour.SetUseSystemMemory(lrSurface.mbUseSystemMemory);
        lrColour.SetNumSections(mu8NumSections);

        // A section that shares another target's memory is un-tiled; otherwise it keeps its tile slot.
        if (lrSurface.mpSharedTarget != 0)
        {
            lrColour.ShareMemoryWithTarget(lrSurface.mpSharedTarget, lrSurface.mpSharedAddress);
        }
        else
        {
            lrColour.SetTileIndex(lrSurface.ms8TileIndex);
        }
    }

    rw::graphics::postfx::RenderTarget::RenderTargetMode lColourMode =
        rw::graphics::postfx::RenderTarget::eRenderTarget_NONE;
    if (lbAnyColourInUse)
    {
        lColourMode = lbAnyColourUseDevice
                        ? rw::graphics::postfx::RenderTarget::eRenderTarget_USE_DEVICE_FOR_WRITE
                        : rw::graphics::postfx::RenderTarget::eRenderTarget_CREATE;
    }
    lParameters.SetColorMode(lColourMode);

    // --- the depth / stencil section ---------------------------------------------------------------
    if (mDepthTarget.mpTarget != 0)
    {
        lParameters.SetProvidedDepthStencilTarget(mDepthTarget.mpTarget);
        lParameters.SetDepthStencilMode(rw::graphics::postfx::RenderTarget::eRenderTarget_USE_PROVIDED);
    }
    else
    {
        lParameters.SetDepthStencilMode(mDepthTarget.mbInUse
                                          ? rw::graphics::postfx::RenderTarget::eRenderTarget_CREATE
                                          : rw::graphics::postfx::RenderTarget::eRenderTarget_NONE);
    }

    rw::graphics::postfx::Target::Parameters& lrDepth = lParameters.GetDepthStencilParameters();
    lrDepth.SetAllocator(lpAllocator);
    lrDepth.SetWidth(mu32MaxWidth);
    lrDepth.SetHeight(mu32MaxHeight);
    lrDepth.SetBufferFormat(mDepthTarget.mu32BufferFormat);
    lrDepth.SetTextureFormat(mDepthTarget.mu32TextureFormat);
    lrDepth.SetMultisampleFormat(static_cast<u32>(mn32MultiSampleFormat));
    lrDepth.SetBaseEDRAM(static_cast<s32>(mDepthTarget.mu32BaseEDRAM));
    lrDepth.SetUseSystemMemory(mDepthTarget.mbUseSystemMemory);
    lrDepth.SetZCullIndex(mDepthTarget.ms8ZCullIndex);
    lrDepth.SetZCullAddress(mDepthTarget.miZCullAddress);
    lrDepth.SetNumSections(mu8NumSections);
    // The depth record forwards NO texture type and NO filter mode - the X360 writes neither.
    if (mDepthTarget.mpSharedTarget != 0)
    {
        lrDepth.ShareMemoryWithTarget(mDepthTarget.mpSharedTarget, mDepthTarget.mpSharedAddress);
    }
    else
    {
        lrDepth.SetTileIndex(mDepthTarget.ms8TileIndex);
    }

    // (The X360 leaves -1 sitting in r4 at the call from the tile-index stores above; the callee is
    // the one-argument RenderTarget::Initialize(const Parameters&), so that register is dead.)
    mpRenderTarget = rw::graphics::postfx::RenderTarget::Initialize(lParameters);
}

// ================================================================================================
// Destruct  @ 0x8284CB38  (vtable slot 1)
// ================================================================================================
// EMPTY on the X360 build - the body is a lone `blr`, which the linker ICF-folded with every other
// empty function in the image (hence IDA's BaseCollisionGenerator::Destruct label on it). The PS3
// build has a real teardown at CgsRenderTarget.cpp:205; the X360 one does not.
void CgsRenderTarget::Destruct()
{
}

// ================================================================================================
// Prepare  @ 0x82C296C8  (vtable slot 2)
// ================================================================================================
// `li r3, 1; blr` - unconditionally reports the target as ready. Same ICF fold as Destruct (IDA
// labels the shared body CgsSound::Playback::Content::DoOnPostLoad). All of the real work happens in
// Construct on this build.
bool CgsRenderTarget::Prepare()
{
    return true;
}

// Begin a draw pass into the post-fx render target (resolve face/slice 0).
void CgsRenderTarget::Begin()
{
    mpRenderTarget->Begin(0);
}

// End the draw pass, resolving the colour surface back to its sampleable texture.
void CgsRenderTarget::End()
{
    mpRenderTarget->End(true);
}

// The resolved colour texture for colour section luIndex.
renderengine::Texture* CgsRenderTarget::GetTexture(u32 luIndex)
{
    CGS_ASSERT(mpRenderTarget != 0, "mpRenderTarget");
    return mpRenderTarget->maColourTargets[luIndex].mpTextureState->mpRaster;
}

// The resolved depth/stencil texture.
renderengine::Texture* CgsRenderTarget::GetDepthTexture()
{
    CGS_ASSERT(mpRenderTarget != 0, "mpRenderTarget");
    return mpRenderTarget->mDepthTarget.mpTexture;
}

// Bind this render target's surface state on the device and set the viewport + scissor to its full
// extent. The X360 body reads section-0's render-target state regardless of luSection (the parameter
// selects the conceptual section but the bound state is always slot 0).
void CgsRenderTarget::SetRenderTargetState(u32 luSection)
{
    (void)luSection;
    const renderengine::RenderTargetState* state = mpRenderTarget->mpSection0State;
    if (!state) state = renderengine::Device::GetDefaultRenderTargetState();
    shadow::Device::SetState(state);
    D3DVIEWPORTF9 viewport;
    viewport.X = 0.0f; viewport.Y = 0.0f;
    viewport.Width = static_cast<f32>(mu32MaxWidth);
    viewport.Height = static_cast<f32>(mu32MaxHeight);
    viewport.MinZ = 0.0f; viewport.MaxZ = 1.0f; viewport.Flags = 0;
    D3DDevice_SetViewportF(gpD3DDevice, &viewport);
    RECT scissor;
    scissor.left = 0; scissor.top = 0;
    scissor.right = mu32MaxWidth; scissor.bottom = mu32MaxHeight;
    D3DDevice_SetScissorRect(gpD3DDevice, &scissor);
}

// ARTIST 827E7668 uses the same state and rectangle with inverted depth.
void CgsRenderTarget::SetRenderTargetStateInvertDepth(u32 luSection)
{
    (void)luSection;
    const renderengine::RenderTargetState* state = mpRenderTarget->mpSection0State;
    if (!state) state = renderengine::Device::GetDefaultRenderTargetState();
    shadow::Device::SetState(state);
    D3DVIEWPORTF9 viewport;
    viewport.X = 0.0f; viewport.Y = 0.0f;
    viewport.Width = static_cast<f32>(mu32MaxWidth);
    viewport.Height = static_cast<f32>(mu32MaxHeight);
    viewport.MinZ = 1.0f; viewport.MaxZ = 0.0f; viewport.Flags = 0;
    D3DDevice_SetViewportF(gpD3DDevice, &viewport);
    RECT scissor;
    scissor.left = 0; scissor.top = 0;
    scissor.right = mu32MaxWidth; scissor.bottom = mu32MaxHeight;
    D3DDevice_SetScissorRect(gpD3DDevice, &scissor);
}

// Pointer-invariant layout facts (uncalled; a member so it can reach the private data). The X360 byte
// offsets (surfaces at +0x18 stride 0x30, depth at +0xD8, mpRenderTarget at +0x108, sizeof 0x110) do
// NOT survive the LLP64 host - the surface records widen with their pointers - so nothing here pins a
// byte offset. What IS invariant is the SHAPE: the class is polymorphic (the ctor installs the
// three-slot vptr off_820460B4), there are exactly four colour records plus one depth record, and the
// sentinels the ctor writes are single signed bytes.
void CgsRenderTarget::_AssertLayout()
{
    CGS_STATIC_ASSERT((sizeof(CgsRenderTargetSurface) == 0x30), "ARTIST surface size");
    CGS_STATIC_ASSERT((sizeof(CgsRenderTarget) == 0x110), "ARTIST render target size");
    CGS_STATIC_ASSERT((offsetof(CgsRenderTarget, maRenderTargets) == 0x18), "ARTIST colour records");
    CGS_STATIC_ASSERT((offsetof(CgsRenderTarget, mDepthTarget) == 0xD8), "ARTIST depth record");
    CGS_STATIC_ASSERT((offsetof(CgsRenderTarget, mpRenderTarget) == 0x108), "ARTIST target pointer");

    // The X360 record count: five 0x30-byte surfaces in total, of which four are colour. Derived in
    // the header's COLOUR-SECTION COUNT note from sizeof 0x110 / mpRenderTarget @ +0x108 / the ctor's
    // five seeded records / the four-iteration clear loops. This is a COUNT, not an offset, so it is
    // pointer-invariant and safe to pin.
    CGS_STATIC_ASSERT((KU_NUM_COLOUR_SECTIONS == 4),
                  "the X360 build carries four colour sections (the PS3 DWARF's five is a merge delta)");
    CGS_STATIC_ASSERT((sizeof(((CgsRenderTarget*)0)->maRenderTargets) / sizeof(CgsRenderTargetSurface)
                      + 1 == 5),
                  "four colour records + one depth record = the five the X360 ctor seeds");

    // The tile / ZCull indices the ctor seeds to -1 are single signed-byte sentinels; the target slot
    // it clears to null is a pointer.
    CGS_STATIC_ASSERT((sizeof(((CgsRenderTargetSurface*)0)->ms8TileIndex) == 1),
                  "tile index is a single byte sentinel");
    CGS_STATIC_ASSERT((sizeof(((CgsRenderTargetSurface*)0)->mpTarget) == sizeof(void*)),
                  "surface target slot is a pointer");
}
