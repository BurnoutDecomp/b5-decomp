#include "GameSource/Graphics/BrnCoronaManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState
#include "pc/gcm/renderengine/texture.h"             // renderengine::Texture
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::operator-, Dot, MagnitudeSquared

#include <cstring>   // memcpy

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Three ledger-tracked functions:
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaTypeParams&) @ 0x823FD270
//   BrnCoronaManager::BrnSubmissionInterface::AddPropCorona                              @ 0x823FD138
//   BrnCoronaManager::SetTextureAtlas                                                    @ 0x823FD000
//
// Everything else declared in BrnCoronaManager.h is NOT X360-attested for this TU (no ledger
// address under GameSource/Unity/../Graphics/BrnCoronaManager.cpp) and is deliberately left
// declaration-only -- see the header comment and AGENTS.md's DWARF/X360-ledger gating rule.

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
// The X360 asm dispatches through lAllocator's own vtable at +16 (Allocate: out, this, descriptor,
// flags) and +20 (Free: this, block) -- the same resource-carving allocator interface the sibling
// corona vendor TU declares (SDKs/RenderEngineClub/MAIN/components/src/coronas/rwgcoronarenderer.cpp
// ResourceAllocator). This is a DIFFERENT vtable shape than the x64-PDB-generated rw::IResourceAllocator
// (whose Alloc/Free slots are a generic bump-allocator API, not this descriptor-carving one), so it is
// redeclared here at the minimal matching slice per that same established per-TU vendor convention
// rather than force-fit onto the generated header.
class ResourceCarvingAllocator
{
public:
    virtual void  Reserved00() = 0;   // vtable +0x00 (dtor slot)
    virtual void  Reserved04() = 0;   // vtable +0x04
    virtual void  Reserved08() = 0;   // vtable +0x08
    virtual void  Reserved0C() = 0;   // vtable +0x0C
    // vtable +0x10: allocate a resource for `lpDescriptor`, writing the created handle(s) into
    // `lpHandleOut`. `luFlags` is 0 at this call site.
    virtual void* Allocate(void* lpHandleOut, ResourceCarvingAllocator* lpThis,
                            const u32* lpDescriptor, int luFlags) = 0;
    // vtable +0x14: free a previously-allocated resource block.
    virtual void  Free(void* lpBlock) = 0;
};
}

namespace
{
// File-scope globals the asm writes after building the atlas texture state (X360
// dword_82FAB6A8/B8/BC). The SAME three addresses are also written by BrnCoronaManager::Construct
// (ledger TU SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronabuffer.h @ 0x823FCD90),
// whose already-committed reconstruction models the corona-render-state globals under different
// names (gpCoronaBuffer0/1/2, guCoronaFlag0/1, gpCoronaVTable) that this TU cannot trace back to
// these exact raw addresses without re-deriving that (separate, already-reviewed) function. Declared
// here as this TU's own copy so SetTextureAtlas's store side effects are faithfully reproduced; a
// true single shared definition is a follow-up reconciliation once BrnCoronaManager::Construct is
// re-verified against these same addresses (compile-only gate: no link-time collision at this stage).
u32 gsuCoronaCurrentTextureStateResult; // X360 dword_82FAB6BC -- last SetTextureAtlas/Construct result
u32 gsuCoronaTextureStateFlags;         // X360 dword_82FAB6A8 -- SetTextureAtlas writes 4 here
u8  gsuCoronaVTableTarget;               // X360 unk_82FAFC10 (this TU's own copy of the vtable-image byte)
void* gspCoronaVTable;                  // X360 dword_82FAB6B8 -- &unk_82FAFC10
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
    // See ResourceCarvingAllocator above: the X360 call site dispatches through a descriptor-carving
    // vtable shape that the generated rw::IResourceAllocator does not model, so the reference is
    // reinterpreted at the matching local slice (documented cast, not a raw offset poke -- the target
    // is a real vtable-dispatched object, just declared through this TU's minimal interface slice).
    ResourceCarvingAllocator* lpAllocator =
        const_cast<ResourceCarvingAllocator*>(reinterpret_cast<const ResourceCarvingAllocator*>(&lAllocator));

    if (m_textureStateAtlas)
        lpAllocator->Free(&m_textureStateAtlasResource);

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

    u32 laDescriptor[13] = {};
    renderengine::TextureState::GetResourceDescriptor(laDescriptor);

    u32 lauResourceHandle[5] = {};
    lpAllocator->Allocate(lauResourceHandle, lpAllocator, laDescriptor, 0);
    std::memcpy(&m_textureStateAtlasResource, lauResourceHandle, sizeof(lauResourceHandle));

    // NOTE: the asm never stores lpTextureAtlas into m_textureAtlas (this+4) -- only
    // m_textureStateAtlas (this+8) is written, via TextureState::Initialize's return. Left as-is
    // to match the binary exactly rather than adding an unattested store.
    m_textureStateAtlas = renderengine::TextureState::Initialize(&m_textureStateAtlasResource, &lTextureStateParams);

    gsuCoronaCurrentTextureStateResult = static_cast<u32>(reinterpret_cast<uintptr_t>(m_textureStateAtlas));
    gsuCoronaTextureStateFlags = 4;
    gspCoronaVTable = &gsuCoronaVTableTarget;
}
