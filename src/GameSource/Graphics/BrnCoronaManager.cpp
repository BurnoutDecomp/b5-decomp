#include "GameSource/Graphics/BrnCoronaManager.h"

#include <cstdio>   // snprintf (the one-shot bring-up lines)

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/CgsBlendStateFactory.h"        // saBlendStates[2]
#include "GameShared/GameClasses/Graphics/CgsDepthStencilStateFactory.h" // saDepthStencilStates[1]/[2]
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"  // CgsGraphics::ResourceAllocatorCreate
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"
#include "pc/gcm/renderengine/renderstates.h"        // renderengine::TextureState
#include "pc/gcm/renderengine/texture.h"             // renderengine::Texture
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::operator-, Dot, MagnitudeSquared


// Reconstructed from BURNOUT_X360_ARTIST.XEX. Ledger-tracked, with an address:
//   BrnCoronaManager::Construct                                                          @ 0x823FCD90
//   BrnCoronaManager::Render                                                             @ 0x824075D8
//   BrnCoronaManager::SetTextureAtlas                                                    @ 0x823FD000
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaTypeParams&)  @ 0x823FD270
//   BrnCoronaManager::BrnSubmissionInterface::AddCorona(..., const BrnCoronaType&)        @ sub_823FD428
//   BrnCoronaManager::BrnSubmissionInterface::AddPropCorona                               @ 0x823FD138
// INLINED on the console (no standalone address) and recovered from their inliners:
//   BrnCoronaManager::Clear                    <- BrnRendererModule::StartOfFrame @0x823FC160 (tail)
//   BrnCoronaManager::Swap                     <- BrnRendererModule::SwapBuffers  @0x823FC678 (tail)
//   BrnCoronaManager::GetSubmissionInterface   <- BrnRendererModule::Update       @0x824060F0-108
//   BrnSubmissionInterface::SetCameraInfo      <- BrnRendererModule::Update       @0x82405EC4-0x82405FA0
// Declared only, with the reason on each: Destruct, Prepare, Release, GetTypeParams.
//
// -------------------------------------------------------------------------------------------------
// WHAT LANDED THIS WAVE (coronas step 1, group `coronarender`, 2026-08-17)
//
// The subsystem is MOUNTED. The four data blockers the previous wave recorded are closed by the
// other three groups of this wave, and this file consumes them through two named seams:
//   * SEAM S1 -> `coronaprogs`   : the authored PC vertex/pixel program pair (consumed by
//                                  renderengine::CoronaRenderer::Initialize, not by this file).
//   * SEAM S2 -> `coronadata`    : BrnCoronaTypeParams::GetCoronaTypeParams + the archetype table,
//                                  and KA_CORONA_ATLAS_UVS (the 12 x 4 atlas UV table, declared
//                                  extern below and DEFINED by that group).
// The three file-scope tunables this file owns (kfCoronaFadeDistance, gPropCoronaColour,
// kPropCoronaScaleCurveParams) carry `coronadata`'s recovered values (conductor fold-in, coronas
// step 1: the whole-file rewrite here and coronadata's five OLD/NEW blocks against the same file
// could not both apply, so the VALUES were folded into this text -- see each definition's cite).
// -------------------------------------------------------------------------------------------------

// =================================================================================================
// SEAM S2 -- THE ATLAS UV TABLE (group `coronadata`).
//
// `BrnCoronaManager::s_atlasUVs` (DWARF BrnCoronaManager.h:229) is a 12 x 4 table of Vector2 whose
// 768 bytes are ZERO IN THE SHIPPED IMAGE: the console fills it at run time from
// sub_82C4EDD8 (0x82C4EDD8-0x82C4F414), which builds each 16-byte lane on the stack and stores it
// into unk_82FAFC10 -- so the DWARF's own `Vector2[12][4] s_atlasUVs` is NOT const, and neither is
// the definition below. The initialiser is what the runtime sees, and the initialiser is the ground
// truth (DATA_DUMP.md says exactly this).
//
// THE CONTRACT (SEAM S2, reconciled by the conductor on the Vector2 shape both verifiers agreed
// on): `coronadata` defines, with EXTERNAL linkage, in GameSource/Graphics/BrnCoronaTypeParams.cpp,
//        extern const Vector2 KA_CORONA_ATLAS_UVS[12][4];   // = { #include "...rwgcoronaatlasuvs.inl" }
// -- 12 atlas pages x 4 quad corners, one 16-byte lane each (x=u, y=v, z=w=0), transcribed from
// sub_82C4EDD8's 48 stvx128 stores. This TU only DECLARES it: a zero-filled local fallback would
// draw a full page of one atlas texel and LOOK like a working corona pass (AGENTS.md rule 9), so a
// missing definition must fail at LINK time, loudly, with exactly one unresolved symbol.
// =================================================================================================
extern const Vector2 KA_CORONA_ATLAS_UVS[12][4];

// The definition of the class's own static. Filled once, from the table above, by
// BrnCoronaFillAtlasUVs below -- the host stand-in for the console's static initialiser.
Vector2 BrnCoronaManager::s_atlasUVs[12][4];

namespace
{
// File-scope constants (DWARF BrnCoronaManager.cpp:32/33), rodata floats in the console image.
// kfNoFadeDistanceSqrd's VALUE is Hex-Rays-resolved directly in the AddCorona pseudocode (the
// literal `62500.0` in `(62500.0 - flt_82F242CC)`, X360 flt_8201BCC4) -- reproduced faithfully.
//
// kfCoronaFadeDistance = X360 flt_82F242CC (the tuning block at 0x82F242C0 +0x0C), RECOVERED by
// `coronadata` from the shipped image: word 473D1000 == 48400.0f == 220^2 -- a SQUARED distance
// (do not square it again). The fade ramp therefore runs from 220 m to kfNoFadeDistanceSqrd's
// 250 m, exactly the console's. (Its runtime initialiser sub_82C4F418 does not store this word;
// the image value is the console-visible one.) With the old 0.0f placeholder every corona was on
// the ramp from distance zero -- dimmer everywhere, gone at 250 m.
const f32 kfNoFadeDistanceSqrd = 62500.0f;   // BrnCoronaManager.cpp:33 (X360 flt_8201BCC4)
f32 kfCoronaFadeDistance       = 48400.0f;   // BrnCoronaManager.cpp:32 (X360 flt_82F242CC @0x82F242CC, image word 473D1000) = 220^2

// BrnCoronaManager.cpp:36 (DWARF) -- the shared prop-corona colour; AddPropCorona rewrites only
// its alpha byte per call (X360 dword_82F242D0 ships FFFFFFFF in the image; AddPropCorona
// @0x823FD228/D244/D24C rewrites ONLY the top byte, so RGB stay 0xFF -- an uninitialised RGB of 0
// would make every prop corona an invisible additive-black quad).
rw::RGBA gPropCoronaColour(0xFF, 0xFF, 0xFF, 0xFF);

// Pack an 8-bit alpha into the top byte of an RGBA word (rw::RGBA's byte order is
// (a<<24)|(b<<16)|(g<<8)|r -- see GameShared/GameClasses/RenderWare/RwRGBA.cpp), matching the
// X360 asm's `stb`-then-rotate-into-place sequence in both AddCorona and AddPropCorona.
inline void SetAlphaByte(rw::RGBA& lrColour, uint8_t lu8Alpha)
{
    lrColour.m_rgba = (lrColour.m_rgba & 0x00FFFFFFu) | (static_cast<uint32_t>(lu8Alpha) << 24);
}

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

        // v3 = lCoronaTypeParams.mvSize * lfSizeScale -- a FULL 128-bit multiply, so all four lanes
        // scale. (The previous body staged a 48-byte blob and zeroed the z/w lanes; with the writer
        // retyped to the DWARF's by-value Vector2 the console's own arithmetic is what happens, and
        // the two unread lanes carry whatever the archetype table holds there, exactly as on the
        // console. Dispatch reads only x/y.)
        Vector2 lvSize;
        lvSize.x = lCoronaTypeParams.mvSize.x * lfSizeScale;
        lvSize.y = lCoronaTypeParams.mvSize.y * lfSizeScale;
        lvSize.z = lCoronaTypeParams.mvSize.z * lfSizeScale;
        lvSize.w = lCoronaTypeParams.mvSize.w * lfSizeScale;

        mBufferIterator.Write(lvPosition, lvDirection, lvSize, lCoronaTypeParams.mfBiasDistance,
                               lColour, lCoronaTypeParams.miTextureID);
    }
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::BrnSubmissionInterface::AddCorona (const BrnCoronaType&) @ sub_823FD428
//
// A one-line forwarder: index the archetype table at the console's 48-byte stride and tail-call
// the BrnCoronaTypeParams overload.
//     0x823FD434  lwz   r11, 0(r6)            ; the BrnCoronaType, by reference
//     0x823FD438  lis   r10, unk_82F24310@ha  ; &BrnCoronaTypeParams::smParams[0]
//     0x823FD43C  slwi  r9, r11, 1
//     0x823FD444  add   r11, r11, r9          ; t * 3
//     0x823FD448  slwi  r11, r11, 4           ; * 16   -> t * 48 (the CONSOLE record size)
//     0x823FD44C  add   r6, r11, r10
//     0x823FD450  bl    BrnCoronaManager::BrnSubmissionInterface::AddCorona
// The 48 is a GUEST stride and must NOT survive to the host (the host record is 64 bytes because
// Vector2/Vector3 are alignas(16) here) -- which is exactly why the indexing is delegated to
// BrnCoronaTypeParams::GetCoronaTypeParams (SEAM S2, group `coronadata`) rather than reproduced.
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::BrnSubmissionInterface::AddCorona(const Vector3& lvPosition, const Vector3& lvDirection,
                                                          f32 lfScale, f32 lfOpacity,
                                                          const BrnCoronaType& leCoronaType)
{
    AddCorona(lvPosition, lvDirection, lfScale, lfOpacity,
              BrnCoronaTypeParams::GetCoronaTypeParams(leCoronaType));
}

// kPropCoronaScale's curve-param thresholds (BrnCoronaManager.cpp:68, DWARF) -- X360
// flt_82F242FC / flt_82F24300 / flt_82F24304, RECOVERED by `coronadata` from the image: words
// 43C80000 / 44AF0000 / 46629000 = 400 / 1400 / 14500 (read by AddPropCorona @0x823FD1C8/D1D0/D1D8
// into a packed stack Vector3). The console object at 0x82F242FC is FIVE floats
// (400, 1400, 14500, 0.75, 2.25) -- a whole SmoothStep -- but AddPropCorona reads only the first
// three (its scale-factors argument is the caller's lvSize, see the banner below); the trailing
// (0.75, 2.25) are recorded here so nobody has to re-dump them. Prop coronas have no producer on
// this build yet (BrnWorld::PropEntityModule::RenderPropAndCoronas @0x822DC010 is ledger `todo`).
const Vector3 kPropCoronaScaleCurveParams = { 400.0f, 1400.0f, 14500.0f, 0.0f };

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::BrnSubmissionInterface::AddPropCorona @ 0x823FD138
//
// Prop coronas (world-placed lights on scenery) have no back-face cull -- every call submits a
// record. The corona's on-screen size is NOT lvSize scaled by a distance factor; lvSize is itself
// the (near, far) size pair handed straight to the SmoothStep evaluator as its scale-factors
// argument -- i.e. the submitted size is a plain distance-based LERP between lvSize.x and lvSize.y
// (SmoothStep::Evaluate(kPropCoronaScaleCurveParams, lvSize, distSqr), asm: v18 <- lvSize is passed
// unmodified as Evaluate's second arg, and the scalar result alone is broadcast into the size
// lanes of the record -- confirmed by the post-call `vspltw v3,v0,0` broadcasting *only* the
// Evaluate return value, not a lvSize-scaled vector). Alpha has no distance fade here: the
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

    // `vspltw v3, v0, 0` -- the scalar broadcast into all four size lanes.
    Vector2 lvBroadcastSize;
    lvBroadcastSize.x = lfSize;
    lvBroadcastSize.y = lfSize;
    lvBroadcastSize.z = lfSize;
    lvBroadcastSize.w = lfSize;

    mBufferIterator.Write(lvPosition, lvDirection, lvBroadcastSize, 0.5f, gPropCoronaColour,
                           liTextureID);
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::BrnSubmissionInterface::SetCameraInfo -- INLINED on the console.
//
// Recovered from its one inliner, BrnRendererModule::Update @0x82405EC4-0x82405FA0. THAT function
// picks WHICH interface (the write side, `lbz r10, 0x130(r11)` with NO xori @0x82405F60) and
// computes the three values; this method is the three stores it then performs:
//     iface.mViewProj       = <the CgsCamera's four rows>   (interface +0x10, four 16-byte lanes)
//     iface.mCameraPosition = camera[+0x30]                 (interface +0x50)
//     iface.mViewXyScale    = <the fsel-clamped xy scale>   (interface +0x60)
// The camera gate (`if (camera[+0x58] > flt_82004014)`, flt_82004014 == 0.1f) and the viewXyScale
// arithmetic are the CALLER'S -- they live in BrnRendererModule::Update, not here, and that is
// where this wave lands them.
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::BrnSubmissionInterface::SetCameraInfo(const Matrix44& lViewProj,
                                                             const Vector3& lCameraPosition,
                                                             const Vector4& lViewXyScale)
{
    mViewProj       = lViewProj;
    mCameraPosition = lCameraPosition;
    mViewXyScale    = lViewXyScale;
}

// =================================================================================================
// BrnCoronaManager::Construct @ 0x823FCD90
//
// The asm, in order (every store cited):
//   0x823FCDAC  stb r11(=1), 0(r30)                     mbActive = true      (a BYTE -> bool)
//   0x823FCDB0  bl  renderengine::CoronaRenderer::Initialize(allocator)
//   0x823FCDD8  lwz dword_83010F78 -> stw dword_82FAB6C0     CoronaRenderer::SetBlendState
//   0x823FCDE8  lwz dword_83010914 -> stw dword_82FAB6C4  \  CoronaRenderer::SetDepthStencilStates
//   0x823FCDEC  lwz dword_83010910 -> stw dword_82FAB6C8  /
//   0x823FCE04  stw 0,               dword_82FAB6BC      \
//   0x823FCE10  stw 1,               dword_82FAB6A8       > CoronaRenderer::SetTextureAtlas(0, 1, &s_atlasUVs[0][0])
//   0x823FCE18  stw &unk_82FAFC10,   dword_82FAB6B8      /
//   0x823FCE1C  li  r11, 0x200 -> var_110                 Parameters::SetNumCoronas(512)
//   0x823FCE24  bl  CoronaBuffer::GetResourceDescriptor
//   0x823FCE44  bctrl  (allocator vtable +0x10) -> a1+0x28 (m_coronaBuffer0Resource, 5 guest words)
//   0x823FCE8C  bctrl  (same descriptor)        -> a1+0x3C (m_coronaBuffer1Resource)
//   0x823FCEC0  bl  CoronaBuffer::Initialize(&m_coronaBuffer0Resource, &params) -> stw 0xC(r30)
//   0x823FCED4  bl  CoronaBuffer::Initialize(&m_coronaBuffer1Resource, &params) -> stw 0x10(r30)
//   0x823FCED8-F78  the 64-byte default record staged on the stack (values below)
//   0x823FCF7C-FDC  the 512-iteration fill of BOTH buffers, 8 doublewords each
//   0x823FCFE8  stb 0,  0x130(r30)      mu8SubmissionSwapIndex = 0
//   0x823FCFEC  stw 0,  8(r30)          m_textureStateAtlas    = 0
//   0x823FCFF0  stw m_coronaBuffer0, 0x50(r30)     mSubmissionInterface[0].mpBuffer
//   0x823FCFF4  stw m_coronaBuffer1, 0xC0(r30)     mSubmissionInterface[1].mpBuffer
//
// THE DEFAULT RECORD (staged at var_D0, 64 bytes, copied into all 2 x 512 slots):
//   position  {0,0,0,0}    <- flt_82001CC0 == 0.0f x3 + a zero word
//   direction {0,0,1,0}    <- flt_82001C98 == 1.0f in z          (both floats are in DATA_DUMP.md;
//   size      {1,1,0,0}    <- 1.0f, 1.0f, then `std r28` zeroing   flt_82001C98 is the dump's own
//   distance  1.0f         <- 0x823FCF38 stfs f0, var_A0          CONTROL value)
//   colour    0xFFFFFFFF   <- 0x823FCF28 li r11,-1 ; stw var_9C
//   textureID 0            <- 0x823FCEDC stw r28, var_98
//   +0x3C     never written by the console either -- zeroed here, and said so rather than passed off
//             as attested.
//
// THE THREE STATE CAPTURES ARE NOW REAL (the previous reconstruction left them null with a
// [FLAG BLOCKED] saying CgsBlendStateFactory::GetState was non-static; the 1a verifier's F2 showed
// that paragraph was superseded -- CgsBlendStateFactory.h:194 declares it `static`).
//
// ⚠ PC ORDERING GATE, and why it is a gate and not a guess: on the console Construct runs from
// BrnRendererModule::Prepare, long after CgsBlendStateFactory::Prepare / CgsDepthStencilStateFactory
// ::Prepare filled their tables. On PC there is no BrnRendererModule::Prepare (it is not
// reconstructed), so Construct runs from a lazy bring-up gate -- and if it ran before the factories
// were Prepared it would capture three nulls and the pass would draw with whatever blend state the
// previous draw left. Construct therefore REFUSES to come up with a null state and says so; the
// caller retries next frame. DELETE-WHEN BrnRendererModule::Prepare is reconstructed.
// =================================================================================================
void BrnCoronaManager::Construct(rw::IResourceAllocator& lrAllocator)
{
    renderengine::BlendMaterialState* const lpBlendState =
        CgsBlendStateFactory::GetState(E_FACTORY_BLEND_STATE_TRANSPARENT_ADDITIVE_NO_ALPHA_TEST_DEST_RGBA);
    renderengine::DepthStencilState* const lpZTestState =
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZON_ZLEQ_ZWRITEOFF);
    renderengine::DepthStencilState* const lpNoZTestState =
        CgsDepthStencilStateFactory::GetState(E_FACTORY_DEPTH_STENCIL_STATE_ZOFF_ZALL_ZWRITEOFF);

    if (lpBlendState == 0 || lpZTestState == 0 || lpNoZTestState == 0)
    {
        static bool sbReportedStates = false;
        if (!sbReportedStates)
        {
            sbReportedStates = true;
            CgsDev::Log::WriteToLog("[corona] Construct DEFERRED: the blend / depth-stencil state"
                                    " factories are not Prepared yet\n");
        }
        return;   // IsConstructed() stays false; the bring-up gate retries next frame
    }

    mbActive = true;                                              // stb 1, 0(this)

    renderengine::CoronaRenderer::Initialize(lrAllocator);

    // The two inlined CoronaRenderer setters (@0x823FCDD8 / @0x823FCDE8-F8).
    renderengine::CoronaRenderer::SetBlendState(lpBlendState);
    renderengine::CoronaRenderer::SetDepthStencilStates(lpZTestState, lpNoZTestState);

    // -----------------------------------------------------------------------------------------
    // The host stand-in for the console's static initialiser sub_82C4EDD8 (0x82C4EDD8-0x82C4F414),
    // which fills unk_82FAFC10 == s_atlasUVs before main() runs. Reproduced as a one-shot fill HERE
    // rather than as a C++ dynamic initialiser, because the order of dynamic initialisers ACROSS
    // TUs is unspecified and this table is read from another TU (CoronaRenderer::Dispatch) -- the
    // exact class of bug the campaign's "placeholder constant" note warns about. Construct is the
    // earliest point at which the table is guaranteed to be filled before any reader runs.
    //
    // Each console lane is a 16-byte VMX Vector2 whose z/w are stored as an 8-byte zero
    // (`std r10, 0(rN)` throughout sub_82C4EDD8), so z/w are 0 here too.
    // (The loop is a member's, not a free function's: s_atlasUVs is `protected`.)
    // -----------------------------------------------------------------------------------------
    for (u32 luPage = 0; luPage < 12u; ++luPage)
    {
        for (u32 luCorner = 0; luCorner < 4u; ++luCorner)
        {
            s_atlasUVs[luPage][luCorner] = KA_CORONA_ATLAS_UVS[luPage][luCorner];   // z/w are 0 in the table
        }
    }

    // The third inlined setter (@0x823FCE04-18): no atlas yet, ONE sub-image, and the UV table
    // pointer. SetTextureAtlas raises the sub-image count to 4 when the real atlas arrives.
    renderengine::CoronaRenderer::SetTextureAtlas(0, 1u, &s_atlasUVs[0][0]);

    renderengine::CoronaBuffer::Parameters lParameters;
    lParameters.SetNumCoronas(512);                               // li r11, 0x200

    renderengine::CoronaBuffer::ResourceDescriptor5 lDescriptor;
    renderengine::CoronaBuffer::GetResourceDescriptor(&lDescriptor, &lParameters);

    // The two indirect allocator calls (`lwz r11,0(r31); lwz r11,0x10(r11); bctrl` @0x823FCE28-44
    // and @0x823FCE70-8C) -- rw::IResourceAllocator::DoAllocate, reached BY NAME through the shared
    // helper, never through a guessed vtable slot (CgsResourceAllocatorCreate.h's banner records
    // what the guessed-slot shim cost the sky dome).
    CgsGraphics::ResourceAllocatorCreate(&lrAllocator, &m_coronaBuffer0Resource, &lDescriptor);
    CgsGraphics::ResourceAllocatorCreate(&lrAllocator, &m_coronaBuffer1Resource, &lDescriptor);

    if (m_coronaBuffer0Resource.m_baseResources[0] == 0 || m_coronaBuffer1Resource.m_baseResources[0] == 0)
    {
        // [PC guard] The allocator's lane 0 came back empty (a known PC LinearResourceAllocator
        // starvation mode; console-impossible). Refuse BEFORE CoronaBuffer::Initialize writes the
        // header through it.
        m_coronaBuffer0 = 0;
        m_coronaBuffer1 = 0;
        mbActive        = false;
        CgsDev::Log::WriteToLog("[corona] Construct FAILED: the resource allocator returned no"
                                " corona buffer block (lane 0 null) - the corona pass stays off\n");
        return;
    }

    m_coronaBuffer0 = renderengine::CoronaBuffer::Initialize(
        reinterpret_cast<renderengine::CoronaBuffer**>(&m_coronaBuffer0Resource.m_baseResources[0]),
        &lParameters);
    m_coronaBuffer1 = renderengine::CoronaBuffer::Initialize(
        reinterpret_cast<renderengine::CoronaBuffer**>(&m_coronaBuffer1Resource.m_baseResources[0]),
        &lParameters);

    if (m_coronaBuffer0 == 0 || m_coronaBuffer1 == 0
        || m_coronaBuffer0->mpData == 0 || m_coronaBuffer1->mpData == 0)
    {
        // The allocator handed back nothing. Refuse rather than fill through a null pointer -- the
        // console cannot reach this state (its graphics heap is carved up front), so there is no
        // console behaviour to reproduce and the honest PC answer is a loud no-op.
        m_coronaBuffer0 = 0;
        m_coronaBuffer1 = 0;
        mbActive        = false;
        CgsDev::Log::WriteToLog("[corona] Construct FAILED: the resource allocator returned no"
                                " corona buffer block - the corona pass stays off\n");
        return;
    }

    // The 64-byte default record, stamped into every slot of both buffers.
    renderengine::Corona lDefault = {};   // +0x3C: the console copies whatever the stack held; a
                                          // defined zero is the safe host reproduction (Dispatch
                                          // never reads it) and is called out rather than claimed.
    lDefault.mvPosition.x  = 0.0f; lDefault.mvPosition.y  = 0.0f;
    lDefault.mvPosition.z  = 0.0f; lDefault.mvPosition.w  = 0.0f;
    lDefault.mvDirection.x = 0.0f; lDefault.mvDirection.y = 0.0f;
    lDefault.mvDirection.z = 1.0f; lDefault.mvDirection.w = 0.0f;
    lDefault.mvSize.x      = 1.0f; lDefault.mvSize.y      = 1.0f;
    lDefault.mvSize.z      = 0.0f; lDefault.mvSize.w      = 0.0f;
    lDefault.mfDistance    = 1.0f;
    lDefault.muColour      = 0xFFFFFFFFu;
    lDefault.miTextureID   = 0;

    const u32 luNumCoronas = m_coronaBuffer0->GetNumCoronas();
    for (u32 luIndex = 0; luIndex < luNumCoronas; ++luIndex)
    {
        m_coronaBuffer0->mpData[luIndex] = lDefault;
        m_coronaBuffer1->mpData[luIndex] = lDefault;
    }

    mu8SubmissionSwapIndex          = 0;                 // stb 0, 0x130(this)
    m_textureStateAtlas             = 0;                 // stw 0, 8(this)
    m_textureAtlas                  = 0;
    mSubmissionInterface[0].mpBuffer = m_coronaBuffer0;   // stw, 0x50(this)
    mSubmissionInterface[1].mpBuffer = m_coronaBuffer1;   // stw, 0xC0(this)

    // Neither interface has been Cleared yet: the console's first StartOfFrame does that before any
    // producer runs. Publish the cursors here too so a producer that beats the first StartOfFrame
    // (possible on PC, where Construct is lazy) writes into the buffer rather than through null.
    m_coronaBuffer0->Lock(mSubmissionInterface[0].mBufferIterator);
    m_coronaBuffer1->Lock(mSubmissionInterface[1].mBufferIterator);

    {
        char lacMessage[224];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[corona] manager constructed: buffers=2 x %u slots (%u bytes each),"
                      " atlasUV[0] = (%.4f,%.4f)..(%.4f,%.4f), fadeDistanceSqrd=%.1f\n",
                      (unsigned)luNumCoronas,
                      (unsigned)((luNumCoronas << 6) + 16u),
                      s_atlasUVs[0][0].x, s_atlasUVs[0][0].y,
                      s_atlasUVs[0][3].x, s_atlasUVs[0][3].y,
                      kfCoronaFadeDistance);
        CgsDev::Log::WriteToLog(lacMessage);
    }
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::Clear -- INLINED on the console; recovered from BrnRendererModule::StartOfFrame
// @0x823FC160's tail, which is the whole of it:
//     v11 = (112 * *(this + 14640) + this + 14336 + 80);   // &mSubmissionInterface[swapIndex]
//     v12 = *v11;                                          // .mpBuffer
//     v13 = *(*v11 + 4);                                   // mpBuffer->mpData
//     v11[2] = 0;                                          // mBufferIterator.muIndex      = 0
//     v11[1] = v13;                                        // mBufferIterator.mpData       = ...
//     v11[3] = *v12;                                       // mBufferIterator.muNumCoronas = ...
// -- i.e. exactly renderengine::CoronaBuffer::Lock(Iterator&) on the CURRENT (write) interface.
// (14336 == 0x3800 == BrnRendererModule +0x3800 == mCoronaManager; 14640 == 0x3800 + 0x130 ==
// mu8SubmissionSwapIndex; 112 == 0x70 == sizeof(BrnSubmissionInterface); 80 == 0x50 == the base of
// mSubmissionInterface.)
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::Clear()
{
    BrnSubmissionInterface& lrInterface = mSubmissionInterface[mu8SubmissionSwapIndex];
    if (lrInterface.mpBuffer != 0)
    {
        lrInterface.mpBuffer->Lock(lrInterface.mBufferIterator);
    }
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::Swap -- INLINED; recovered from BrnRendererModule::SwapBuffers @0x823FC678's
// last statement, `*(a1 + 14640) ^= 1u;` -- the whole function.
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::Swap()
{
    mu8SubmissionSwapIndex = static_cast<uint8_t>(mu8SubmissionSwapIndex ^ 1u);
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::GetSubmissionInterface -- INLINED; recovered from BrnRendererModule::Update
// @0x824060F0-108, the expression it hands to RendererIO::OutputBuffer::
// SetCoronaSubmissionInterface: `C + C[0x130] * 0x70 + 0x50`, i.e. the CURRENT (write) interface,
// with NO xor -- the read side (Render) is the one that takes the other slot.
// ---------------------------------------------------------------------------------------------
BrnCoronaManager::BrnSubmissionInterface* BrnCoronaManager::GetSubmissionInterface()
{
    return &mSubmissionInterface[mu8SubmissionSwapIndex];
}

// [FLAG PC bring-up] -- see the header for the whole reasoning. The console's own SetCameraInfo,
// aimed at the interface Render reads (mu8SubmissionSwapIndex ^ 1) instead of the one Update writes.
void BrnCoronaManager::PCBringUpSetRenderCamera(const Matrix44& lViewProj,
                                                 const Vector3& lCameraPosition,
                                                 const Vector4& lViewXyScale)
{
    mSubmissionInterface[mu8SubmissionSwapIndex ^ 1u].SetCameraInfo(lViewProj, lCameraPosition,
                                                                     lViewXyScale);
}

// =================================================================================================
// BrnCoronaManager::Render @ 0x824075D8
//
//   0x824075E8  lbz  r11, 0(r3)        -> mbActive               (a BYTE: `bool`, not a dword)
//   0x824075F4  lwz  r11, 8(r3)        -> m_textureStateAtlas    (null until SetTextureAtlas ran)
//   0x82407600  lbz  r11, 0x130(r3)    -> mu8SubmissionSwapIndex
//   0x8240760C  xori r11, r11, 1       -> THE OTHER interface: the one the world wrote into last
//                                         frame, not the one Update is currently publishing
//   0x82407614  mulli r11, r11, 0x70   -> sizeof(BrnSubmissionInterface) == 112
//   0x82407624  addi r31, r11, 0x50    -> &mSubmissionInterface[i]
//
// It then stages a RenderParameters block at var_80 and fills exactly three of its four members:
//   +0x10 <- {interface.mCameraPosition.xyz, lfWhiteLevel}   (`stfs f1, var_84` @0x82407644 drops
//            the float ARGUMENT into the w lane of the camera-position vector staged at var_90 --
//            which is the whole reason the shader constant is called "PlusBrightness")
//   +0x20 <- interface.mViewProj, four 16-byte rows (from interface+0x10 .. +0x40)
//   +0x60 <- interface.mViewXyScale (from interface+0x60)
// and NEVER writes +0x00 (m_flags). Begin does not read it. It is zeroed here rather than left
// indeterminate -- a defined zero is the safe host reproduction of a stack slot the console leaves
// alone, and it is called out instead of being passed off as attested.
//
//   0x8240768C  bl  Begin<shadow::Device>(params)
//   0x82407690  lwz r11, 8(r31)        -> mBufferIterator.muIndex; skip the batch when zero
//   0x8240769C  li  r10, 1             -> BatchParameters.m_flags = 1 == FLAG_OCCLUSION_ZTEST,
//                                         which is what selects saDepthStencilStates[2]
//                                         (ZON_ZLEQ_ZWRITEOFF) inside Dispatch. LOAD-BEARING.
//   0x824076A8  stw r11, var_90        -> BatchParameters.m_numCoronas
//   0x824076B0  bl  Dispatch<shadow::Device>(batch, interface.mpBuffer)
//   0x824076B4  stb 0, byte_82FAB695   -> CoronaRenderer::End()
//
// CALLER / PASS POSITION (BrnRendererModule::Render @0x8240BFA8): after the transparent car pass,
// before the post-fx composite, bracketed by the CgsDev::PerfMon* pair whose monitor is named
// "DT: Render coronas". This wave places it at the tail of BrnRendererModule::RenderWorldPasses,
// which is that position on the PC frame (RenderWorldPasses runs inside the anti-alias bracket, and
// ResolveMSAA + the composite follow it).
// =================================================================================================
void BrnCoronaManager::Render(f32 lfWhiteLevel)
{
    if (!mbActive || m_textureStateAtlas == 0)
        return;

    // The interface the WORLD filled last frame is the one Update is not publishing now.
    BrnSubmissionInterface& lrInterface = mSubmissionInterface[mu8SubmissionSwapIndex ^ 1u];

    renderengine::CoronaRenderer::RenderParameters lParams;
    lParams.muFlags = 0;                                        // not written by the console; see banner
    lParams.mvCameraPositionPlusWhiteLevel.x = lrInterface.mCameraPosition.x;
    lParams.mvCameraPositionPlusWhiteLevel.y = lrInterface.mCameraPosition.y;
    lParams.mvCameraPositionPlusWhiteLevel.z = lrInterface.mCameraPosition.z;
    lParams.mvCameraPositionPlusWhiteLevel.w = lfWhiteLevel;    // stfs f1, var_84
    lParams.mViewProjectionMatrix            = lrInterface.mViewProj;
    lParams.mvViewXyScale                    = lrInterface.mViewXyScale;

    // [FLAG PC-platform leaf] The console issues Begin unconditionally (@0x8240768C, before the
    // `lwz r11,8(r31)` count test). On this backend RenderEngineDeviceBeginShaderStates STAGES rows
    // that only Dispatch's FlushVertexProgramState drains, so an unconditional Begin on an EMPTY
    // frame would leak three constant rows into the next subsystem's flush. Begin is therefore
    // gated on the same count the console gates the batch on. DELETE-WHEN the leaf gains a
    // per-pass stage.
    const u32 luNumCoronas = lrInterface.mBufferIterator.GetNumCoronasWritten();
    if (luNumCoronas != 0u)
    {
        renderengine::CoronaRenderer::Begin(lParams);

        renderengine::CoronaRenderer::BatchParameters lBatch;
        lBatch.muNumCoronas = luNumCoronas;
        lBatch.muFlags      = renderengine::CoronaRenderer::BatchParameters::FLAG_OCCLUSION_ZTEST;
        renderengine::CoronaRenderer::Dispatch(lBatch, lrInterface.mpBuffer);
    }

    renderengine::CoronaRenderer::End();
}

// ---------------------------------------------------------------------------------------------
// BrnCoronaManager::SetTextureAtlas @ 0x823FD000
//
// Frees the previous atlas texture-state resource (if one exists), builds a fresh TextureState
// from a fixed sampler-parameter block (2/2/2 address mode, linear mag/min filter, 13x aniso,
// point mip filter, no LOD bias) over the new atlas texture, records it as m_textureStateAtlas /
// m_textureStateAtlasResource, and publishes the trio to the renderer -- which is the inlined
// CoronaRenderer::SetTextureAtlas(state, 4 sub-images, &s_atlasUVs[0][0]) at @0x823FD108-2C.
// ---------------------------------------------------------------------------------------------
void BrnCoronaManager::SetTextureAtlas(const rw::IResourceAllocator& lAllocator, renderengine::Texture* lpTextureAtlas)
{
    // THE ALLOCATOR IS CALLED BY NAME, NOT BY GUESSED SLOT. The X360 reaches the allocator through
    // two indirect calls, `(*(*a2 + 16))(out, a2, desc, 0)` and `(*(*a2 + 20))(a2, &resource)`.
    // This TU used to model that with a local `class ResourceCarvingAllocator` reinterpret_cast over
    // the real rw::IResourceAllocator -- the EXACT shim CgsResourceAllocatorCreate.h documents as
    // destructive (guessed console 4-byte slots over an 8-byte host vtable whose slot 0 is the
    // VIRTUAL DESTRUCTOR). The named calls below are the same two operations on the committed
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
    // sibling call sites (CgsFont.cpp:323, CgsAptRenderHandler.cpp:326,
    // BrnGuiColourCalibrationScreen.cpp:221).
    u32 laDescriptor[10] = {};
    renderengine::TextureState::GetResourceDescriptor(laDescriptor);

    CgsGraphics::ResourceAllocatorCreate(lpAllocator, &m_textureStateAtlasResource, laDescriptor);

    // NOTE: the asm never stores lpTextureAtlas into m_textureAtlas (this+4) -- only
    // m_textureStateAtlas (this+8) is written, via TextureState::Initialize's return. Left as-is
    // to match the binary exactly rather than adding an unattested store.
    m_textureStateAtlas = renderengine::TextureState::Initialize(&m_textureStateAtlasResource, &lTextureStateParams);

    // @0x823FD108-2C, in the asm's order -- the inlined CoronaRenderer::SetTextureAtlas.
    renderengine::CoronaRenderer::SetTextureAtlas(m_textureStateAtlas, 4u, &s_atlasUVs[0][0]);

    {
        char lacMessage[192];
        std::snprintf(lacMessage, sizeof(lacMessage),
                      "[corona] atlas bound: texture=%d textureState=%d subImages=4 -> pass %s\n",
                      (int)(lpTextureAtlas != 0), (int)(m_textureStateAtlas != 0),
                      renderengine::CoronaRenderer::IsReady() ? "READY" : "NOT READY");
        CgsDev::Log::WriteToLog(lacMessage);
    }
}

