#include "GameSource/Graphics/BrnCoronaManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"  // CgsGraphics::ResourceAllocatorCreate
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState
#include "pc/gcm/renderengine/texture.h"             // renderengine::Texture
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::operator-, Dot, MagnitudeSquared


// Reconstructed from BURNOUT_X360_ARTIST.XEX. Three ledger-tracked functions:
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaTypeParams&) @ 0x823FD270
//   BrnCoronaManager::BrnSubmissionInterface::AddPropCorona                              @ 0x823FD138
//   BrnCoronaManager::SetTextureAtlas                                                    @ 0x823FD000
//
// Everything else declared in BrnCoronaManager.h is NOT X360-attested for this TU (no ledger
// address under GameSource/Unity/../Graphics/BrnCoronaManager.cpp) and is deliberately left
// declaration-only -- see the header comment and AGENTS.md's DWARF/X360-ledger gating rule.
//
// ---------------------------------------------------------------------------------------------
// [FLAG BLOCKED -- CONSOLE DATA NOT RECOVERED] (carlights step 1, group `coronas`, 2026-08-17)
//
// This TU is STILL NOT MOUNTABLE-FOR-EFFECT, and the reason is DATA, not code. Four console blobs
// decide everything a corona looks like, and none of them is in any export we hold. Their exact
// addresses/sizes are pinned below so a single idat dump closes them:
//
//   1. BrnCoronaTypeParams::smParams  = X360 unk_82F24310, 25 records x 48 bytes = 1200 bytes.
//      PROOF: sub_823FD428 (the `const BrnCoronaType&` AddCorona overload, xrefs from
//      SubmitCoronasForRaceCar / SubmitCoronasForVehicle / RenderCoronasForInstance) is exactly
//      `AddCorona(pos, dir, scale, opacity, *(&unk_82F24310 + 48 * type))`; its asm is
//      `lwz r11,0(r6); slwi r9,r11,1; add r11,r11,r9; slwi r11,r11,4` == type*3*16 == type*48.
//      *** THE CONSOLE RECORD IS 48 BYTES; THIS HOST DECLARATION IS 64 (Vector2/Vector3 are
//      alignas(16) here, so mScaleCurve lands at +0x20, not the console's +0x18). The dump must be
//      transcribed FIELD BY FIELD into C++ initialisers -- never memcpy'd. ***
//   2. BrnCoronaManager::s_atlasUVs   = X360 unk_82FAFC10, 12 rows x 4 Vector2 x 16 B = 768 bytes.
//      PROOF: SetTextureAtlas @0x823FD11C-2C stores &unk_82FAFC10 into dword_82FAB6B8, and
//      CoronaRenderer::Dispatch<shadow::Device> @0x82404F30 reads the four quad UVs as
//      `*(textureID<<6 + dword_82FAB6B8 + {0,4 | 16,20 | 32,36 | 48,52})` -- a 64-byte row per
//      texture id, four 16-byte-strided (u,v) pairs per row. So dword_82FAB6B8 is the ATLAS UV
//      TABLE POINTER (it was previously modelled here as a "vtable" byte -- corrected below).
//   3. kfCoronaFadeDistance          = X360 flt_82F242CC (4 bytes) -- see the placeholder below.
//   4. kPropCoronaScaleCurveParams   = X360 flt_82F242FC / flt_82F24300 / flt_82F24304 (12 bytes),
//      and gPropCoronaColour's initial RGB = X360 dword_82F242D0 (4 bytes).
//
//   A single dump of 0x82F242C0..0x82F247E0 (1312 bytes) covers 1, 3 and 4; 0x82FAFC10..0x82FAFF10
//   (768 bytes) covers 2. Both live in the 0x82F2xxxx / 0x82FAxxxx WRITABLE segments, so the dump
//   must also state whether a runtime initialiser writes them (the way 0x82C4BC30 splats
//   unk_82FAD990) -- if it does, the image bytes are zeros and the initialiser is the ground truth.
// ---------------------------------------------------------------------------------------------

namespace
{
// File-scope constants (DWARF BrnCoronaManager.cpp:32/33), rodata floats in the console image.
// kfNoFadeDistanceSqrd's VALUE is Hex-Rays-resolved directly in the AddCorona pseudocode (the
// literal `62500.0` in `(62500.0 - flt_82F242CC)`, X360 flt_8201BCC4) -- reproduced faithfully.
// kfCoronaFadeDistance (X360 flt_82F242CC) is read only symbolically (`if (v20 > flt_82F242CC)`);
// its rodata VALUE is not recoverable from this dossier, so it is declared as the named tunable the
// body reads with a neutral placeholder. The body's STRUCTURE (which threshold gates the fade-in,
// the linear ramp shape, the fsel clamp) is faithful to the asm regardless of the exact tuning value
// -- same pattern as BrnDirectorVehicleTracker.cpp's crash-band thresholds. FLAG: placeholder value,
// not the console's actual tuning -- do NOT trust it for exact fade-distance behaviour.
const f32 kfNoFadeDistanceSqrd = 62500.0f;   // BrnCoronaManager.cpp:33 (X360 flt_8201BCC4)
f32 kfCoronaFadeDistance       = 0.0f;       // BrnCoronaManager.cpp:32 (X360 flt_82F242CC) FLAG: rodata value not recovered

// BrnCoronaManager.cpp:36 (DWARF) -- the shared prop-corona colour; AddPropCorona rewrites only
// its alpha byte per call (X360 dword_82F242D0, adjacent to the kPropCoronaScale constants it is
// declared next to in the DWARF dump).
rw::RGBA gPropCoronaColour;

// Pack an 8-bit alpha into the top byte of an RGBA word (rw::RGBA::RGBA's byte order is
// (a<<24)|(b<<16)|(g<<8)|r -- see GameShared/GameClasses/RenderWare/RwRGBA.cpp), matching the
// X360 asm's `stb`-then-rotate-into-place sequence in both AddCorona and AddPropCorona.
inline void SetAlphaByte(rw::RGBA& lrColour, uint8_t lu8Alpha)
{
    lrColour.m_rgba = (lrColour.m_rgba & 0x00FFFFFFu) | (static_cast<uint32_t>(lu8Alpha) << 24);
}

// The 48-byte vector payload renderengine::CoronaBuffer::Iterator::Write stages from v1/v2/v3
// (record +0x00..+0x2F: position, direction, size-ish data). Vector4-shaped per lane so a plain
// memcpy of three of these reproduces the three 128-bit VMX stores exactly.
struct CoronaWritePayload
{
    Vector4 mvPosition;
    Vector4 mvDirection;
    Vector4 mvSize;
};
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::BrnSubmissionInterface::AddCorona (const BrnCoronaTypeParams&) @ 0x823FD270
//
// Back-face-culls the corona (skips it entirely when the camera-to-corona delta faces away from
// the corona's direction), then distance-fades both size and alpha:
//   delta   = lvPosition - mCameraPosition
//   visible = Dot3(delta, lvDirection) < 0.0f      (asm: 0 > Dot -> take the write path)
//   distSqr = MagnitudeSquared(delta)
//   scale   = lCoronaTypeParams.mScaleCurve.Evaluate(distSqr) * lfScale
//   alpha   = lfOpacity, clamped down to 0 once distSqr passes kfCoronaFadeDistance, hitting 0 at
//             kfNoFadeDistanceSqrd (a linear fade-out ramp over that window)
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::BrnSubmissionInterface::AddCorona(const Vector3& lvPosition, const Vector3& lvDirection,
                                                           f32 lfScale, f32 lfOpacity,
                                                           const BrnCoronaTypeParams& lCoronaTypeParams)
{
    const Vector3 lvDelta = rw::math::vpu::operator-(lvPosition, mCameraPosition);

    // Back-face cull: only submit the corona when it faces toward the camera.
    if (rw::math::vpu::Dot(lvDelta, lvDirection) < 0.0f)
    {
        const f32 lfDistSqr = rw::math::vpu::MagnitudeSquared(lvDelta);

        f32 lfAlphaScale = 1.0f;
        if (lfDistSqr > kfCoronaFadeDistance)
        {
            lfAlphaScale = 1.0f - ((lfDistSqr - kfCoronaFadeDistance) / (kfNoFadeDistanceSqrd - kfCoronaFadeDistance));
            if (lfAlphaScale < 0.0f)
                lfAlphaScale = 0.0f;
        }

        CGS_ASSERT(mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>(KI_MAX_CORONAS),
                    "mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>( KI_MAX_CORONAS )");

        const f32 lfSizeScale = lCoronaTypeParams.mScaleCurve.Evaluate(lfDistSqr) * lfScale;

        // A function-local static colour (X360's guarded lazy-init of dword_82FB0044/0x82FB0040):
        // only the alpha byte changes per call, the RGB channels stay at their initial 0xFF.
        static rw::RGBA lColour(0xFF, 0xFF, 0xFF, 0xFF);
        SetAlphaByte(lColour, static_cast<uint8_t>(lfAlphaScale * lfOpacity * 255.0f));

        // v3 = lCoronaTypeParams.mvSize (loaded as a full 128-bit VMX register, so its z/w lanes are
        // whatever the archetype table stores there -- Vector2's unused z/w padding lanes, not a
        // documented duplicate of x/y; zeroed here rather than guessing a value) * lfSizeScale.
        CoronaWritePayload lPayload;
        lPayload.mvPosition  = Vector4{ lvPosition.x, lvPosition.y, lvPosition.z, lvPosition.w };
        lPayload.mvDirection = Vector4{ lvDirection.x, lvDirection.y, lvDirection.z, lvDirection.w };
        lPayload.mvSize      = Vector4{ lCoronaTypeParams.mvSize.x * lfSizeScale,
                                         lCoronaTypeParams.mvSize.y * lfSizeScale,
                                         0.0f, 0.0f };

        mBufferIterator.Write(&lPayload, lCoronaTypeParams.mfBiasDistance, lColour.m_rgba,
                               lCoronaTypeParams.miTextureID);
    }
}

// kPropCoronaScale's curve-param thresholds (BrnCoronaManager.cpp:68, DWARF) -- X360
// flt_82F242FC/flt_82F24300/flt_82F24304, three rodata floats baked as immediates at the
// AddPropCorona call site (Hex-Rays keeps them symbolic, not resolved to a decimal literal like
// kfNoFadeDistanceSqrd was), so their VALUES are not recoverable from this dossier. Declared as
// the named tunable AddPropCorona reads, with a neutral placeholder -- same FLAG convention as
// kfCoronaFadeDistance above / BrnDirectorVehicleTracker.cpp's crash-band thresholds.
const Vector3 kPropCoronaScaleCurveParams = { 0.0f, 0.0f, 0.0f, 0.0f };  // FLAG: rodata values not recovered

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::BrnSubmissionInterface::AddPropCorona @ 0x823FD138
//
// Prop coronas (world-placed lights on scenery) have no back-face cull -- every call submits a
// record. The corona's on-screen size is NOT lvSize scaled by a distance factor; lvSize is itself
// the (near, far) size pair handed straight to the SmoothStep evaluator as its scale-factors
// argument -- i.e. the submitted size is a plain distance-based LERP between lvSize.x and lvSize.y
// (SmoothStep::Evaluate(kPropCoronaScaleCurveParams, lvSize, distSqr), asm: v18 <- lvSize is passed
// unmodified as Evaluate's second arg, and the scalar result alone is broadcast into the size
// lanes of the Write payload -- confirmed by the post-call `vspltw v3,v0,0` broadcasting *only*
// the Evaluate return value, not a lvSize-scaled vector). Alpha has no distance fade here: the
// caller-supplied lfAlpha is written straight through into the shared gPropCoronaColour.
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::BrnSubmissionInterface::AddPropCorona(const Vector3& lvPosition, const Vector3& lvDirection,
                                                               const Vector2& lvSize, f32 lfAlpha,
                                                               int32_t liTextureID)
{
    CGS_ASSERT(mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>(KI_MAX_CORONAS),
                "mBufferIterator.GetNumCoronasWritten() < static_cast<uint32_t>( KI_MAX_CORONAS )");

    const Vector3 lvDelta = rw::math::vpu::operator-(lvPosition, mCameraPosition);
    const f32 lfDistSqr = rw::math::vpu::MagnitudeSquared(lvDelta);

    const BrnEffects::Curves::SmoothStep lScaleCurve{};
    const f32 lfSize = lScaleCurve.Evaluate(kPropCoronaScaleCurveParams, lvSize, lfDistSqr);

    SetAlphaByte(gPropCoronaColour, static_cast<uint8_t>(lfAlpha * 255.0f));

    CoronaWritePayload lPayload;
    lPayload.mvPosition  = Vector4{ lvPosition.x, lvPosition.y, lvPosition.z, lvPosition.w };
    lPayload.mvDirection = Vector4{ lvDirection.x, lvDirection.y, lvDirection.z, lvDirection.w };
    lPayload.mvSize      = Vector4{ lfSize, lfSize, lfSize, lfSize };

    mBufferIterator.Write(&lPayload, 0.5f, gPropCoronaColour.m_rgba, liTextureID);
}

namespace
{
// File-scope globals the asm writes after building the atlas texture state (X360
// dword_82FAB6A8/B8/BC). The SAME three addresses are also written by BrnCoronaManager::Construct
// (ledger TU SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h @ 0x823FCD90),
// whose already-committed reconstruction models them under different names; a single shared
// definition is a follow-up reconciliation once that function is re-derived against the real class
// (compile-only gate: no link-time collision at this stage).
//
// TYPES CORRECTED 2026-08-17 (carlights step 1, group `coronas`) -- what each one actually holds is
// named by its CONSUMER, renderengine::CoronaRenderer::Begin<shadow::Device> @0x823FF2C0 and
// ::Dispatch<shadow::Device> @0x82404F30:
//
//   dword_82FAB6BC : Begin's first act is `sub_8227D158(dword_82FAB6BC, 0)` -- the built atlas
//                    TextureState bound on sampler 0. It is a POINTER, so on this x64 host it must
//                    be a pointer: the previous `u32` spelling truncated a 64-bit host pointer to
//                    32 bits (AGENTS.md rule 1, GUEST vs HOST), which would have handed the sampler
//                    a corrupt state the moment this TU was mounted.
//   dword_82FAB6A8 : Begin gates `shadow::Device::Xbox2SetStateLowLevelShadowed(dword_82FAB6C0,...)`
//                    on a separate word; this one is a plain state word -- Construct writes 1,
//                    SetTextureAtlas writes 4. Kept as an opaque u32 (its consumer is not in the
//                    corona call graph we have; no meaning is asserted).
//   dword_82FAB6B8 : NOT a vtable. Dispatch reads it as
//                    `*(textureID<<6 + dword_82FAB6B8 + {0,4|16,20|32,36|48,52})` -- the four quad
//                    UV pairs of one atlas page -- so it is `&BrnCoronaManager::s_atlasUVs[0]`,
//                    i.e. a `const Vector2 (*)[4]` over the 12x4 table at X360 unk_82FAFC10.
//                    The previous model (a one-byte `gsuCoronaVTableTarget` global taken as "the
//                    vtable image") would have let Dispatch read 768 bytes past a 1-byte object.
renderengine::TextureState* gspCoronaAtlasTextureState;  // X360 dword_82FAB6BC
u32                          gsuCoronaRenderStateWord;    // X360 dword_82FAB6A8 (Construct: 1, here: 4)
const Vector2              (*gspaCoronaAtlasUVs)[4];      // X360 dword_82FAB6B8 == &s_atlasUVs[0]
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::SetTextureAtlas @ 0x823FD000
//
// Frees the previous atlas texture-state resource (if one exists), builds a fresh TextureState
// from a fixed sampler-parameter block (2/2/2 address mode, linear mag/min filter, 13x aniso,
// point mip filter, no LOD bias) over the new atlas texture, and records it as m_textureStateAtlas
// / m_textureStateAtlasResource, then updates the shared corona-render-state globals (see above).
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::SetTextureAtlas(const rw::IResourceAllocator& lAllocator, renderengine::Texture* lpTextureAtlas)
{
    // THE ALLOCATOR IS CALLED BY NAME, NOT BY GUESSED SLOT (fixed 2026-08-17, carlights step 1).
    // The X360 reaches the allocator through two indirect calls, `(*(*a2 + 16))(out, a2, desc, 0)`
    // and `(*(*a2 + 20))(a2, &m_textureStateAtlasResource)`. This TU used to model that with a local
    // `class ResourceCarvingAllocator { virtual Reserved00..0C; virtual Allocate; virtual Free; }`
    // reinterpret_cast over the real rw::IResourceAllocator -- the EXACT shim that
    // GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h documents as destructive: the
    // guessed slot numbers are console 4-byte slots, the host vtable has 8-byte slots, and
    // rw::IResourceAllocator's slot 0 is its VIRTUAL DESTRUCTOR. Calling through it ran the
    // allocator's deleting destructor, returned an unwritten out-parameter, and left the allocator
    // permanently downgraded to the inert base vtable (this is what broke the sky dome; see that
    // header's banner). The named calls below are the same two operations on the committed
    // interface: DoAllocate (via the shared helper) and DoFree(const Resource&).
    rw::IResourceAllocator* lpAllocator = const_cast<rw::IResourceAllocator*>(&lAllocator);

    if (m_textureStateAtlas)
        lpAllocator->DoFree(m_textureStateAtlasResource);

    renderengine::TextureState::Parameters lTextureStateParams = {};
    lTextureStateParams.muAddressU      = 2;
    lTextureStateParams.muAddressV      = 2;
    lTextureStateParams.muAddressW      = 2;
    lTextureStateParams.muMagFilter     = 1;
    lTextureStateParams.muMinFilter     = 1;
    lTextureStateParams.muMipFilter     = 0;
    lTextureStateParams.muField6        = 0;
    lTextureStateParams.muField7        = 0;
    lTextureStateParams.muMaxAnisotropy = 13;
    lTextureStateParams.muField9        = 0;
    lTextureStateParams.muField10       = 1;
    lTextureStateParams.mfMipLodBias    = 0.0f;
    lTextureStateParams.mfField12       = 0.0f;
    lTextureStateParams.muField13       = 0;
    lTextureStateParams.muField14       = 0;
    lTextureStateParams.muField15       = 0;
    lTextureStateParams.mu8Field40      = 0;
    lTextureStateParams.mu8Field41      = 0;
    lTextureStateParams.mu8Field42      = 0;
    lTextureStateParams.mu8Field43      = 1;
    lTextureStateParams.mu8Field44      = 1;
    lTextureStateParams.mpTexture       = lpTextureAtlas;

    // renderengine::TextureState::GetResourceDescriptor writes ten words (texturestate.cpp:17-25);
    // the console's stack block is larger but only those ten are consumed. Sized to match the
    // sibling call sites (CgsFont.cpp:323, CgsImRenderer.cpp:259).
    u32 laDescriptor[10] = {};
    renderengine::TextureState::GetResourceDescriptor(laDescriptor);

    // The console's `(*(*a2 + 16))(handlesOut, a2, descriptor, 0)`, carved straight into the member
    // rw::Resource. The old form allocated into a `u32 lauResourceHandle[5]` and memcpy'd 20 bytes
    // over an rw::Resource -- five GUEST words written across four HOST pointers (AGENTS.md rule 1).
    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &m_textureStateAtlasResource, laDescriptor);

    // NOTE: the asm never stores lpTextureAtlas into m_textureAtlas (this+4) -- only
    // m_textureStateAtlas (this+8) is written, via TextureState::Initialize's return. Left as-is
    // to match the binary exactly rather than adding an unattested store.
    m_textureStateAtlas = renderengine::TextureState::Initialize(&m_textureStateAtlasResource, &lTextureStateParams);

    // @0x823FD108-2C, in the asm's order.
    gspCoronaAtlasTextureState = m_textureStateAtlas;
    gsuCoronaRenderStateWord   = 4;
    // [FLAG BLOCKED] The console stores `&unk_82FAFC10` == &s_atlasUVs[0] here. s_atlasUVs is
    // DECLARED (BrnCoronaManager.h:230) but has no definition in this tree, because its 768 bytes
    // of atlas UVs are not in any export we hold (see the BLOCKED DATA block at the top of this
    // file). Left null rather than pointed at invented UVs: Dispatch is not live, and a zeroed
    // table would draw degenerate quads that LOOK like a working corona pass. Restore this to
    // `&s_atlasUVs[0]` in the same commit that lands the dumped table.
    gspaCoronaAtlasUVs = 0;
}

