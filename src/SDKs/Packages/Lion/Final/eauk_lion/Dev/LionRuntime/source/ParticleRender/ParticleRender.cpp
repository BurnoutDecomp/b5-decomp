// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/source/ParticleRender/ParticleRender.cpp
//
// cParticleRender -- the Lion (eauk_lion) particle-render pipeline driver. Reconstructed
// behaviour-faithful against the shipped console build, with the declaration shape taken
// from the original headers.
//
// LANDED HERE: cParticleRender::Dispatch, and -- added 2026-09-03 by the boost-
// exhaust wave -- Instance / AppInit / Update, none of which has a body of its own in the
// console image (all three are inlined into cLionFX::Init; see
// the block above their definitions).
//
// The other three LEDGER functions in this TU are still declared-only, and the reasons below are
// RESTATED as of 2026-09-03 rather than left to describe a tree that no longer exists -- the
// second time in two days this banner has needed that, which is the warning.
// ⭐⭐ THE TYPE CONFLICT IS GONE. This banner used to say "the cMatrix half of that conflict is
// REAL and remains (this header typedefs cMatrix to rw::math::vpu::Matrix44; ParticleBucket.h
// still carries its own `struct cMatrix`)". Both halves are retired: cVector and cMatrix each
// have ONE home now (eauk_common/Maths/Vector.h and .../Matrix.h), and this header includes the
// matrix home instead of typedef'ing over it. cParticleRender::EmitterRender is NO LONGER BLOCKED
// ON A TYPE -- it is blocked on the three cParticleEmitter::SimulateParticlesInBucketGeneral<>
// kernels it calls (578 pseudocode lines between them), which have no bodies.
//
//   * cParticleRender::Render          -- BLOCKED. The per-emitter frustum cull
//     is pure vector-unit code: it loads two un-recovered constant tables (a lane-permute
//     table and a masking constant) whose bytes have not been recovered,
//     then runs permute/compare sequences over the packed frustum planes. The cull
//     cannot be faithfully reconstructed without those table bytes.
//   * cParticleRender::EmitterCubeRender -- BLOCKED. The per-particle box clip
//     is pure vector-unit code (lane loads/stores, subtracts, 3-component dot products and
//     selects over the bucket vectors) and multiplies by an un-recovered float constant; the
//     snap-to-plane math cannot be reproduced without that constant's value.
//   * cParticleRender::EmitterRender   -- the type conflict that used to park it is
//     GONE (see the banner). Its remaining blocker is its three callees: the
//     cParticleEmitter::SimulateParticlesInBucketGeneral<> kernels for the Matrix (212 lines),
//     Vector (132) and Local (234) bucket types. Everything else it needs now exists --
//     LionParticleRender::RenderGroupBeginLite / GetVertexStride / Render / RenderGroupEndLite
//     are bodied, cParticleBucket::GetpMatrix is bodied, and the bucket walk it does
//     (mpMatrices -> mpVectors -> locator) is the same three-way GetpMatrix documents.
//
// Dispatch's device path: the console build inlines the shadow-device sampler-state bind (the
// cached-state compare plus the setter-and-store block) that shadow::Device::SetState owns;
// it is de-inlined back to that call here (semantic parity, one owning body -- AGENTS.md).
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"          // the emitter + the three simulation helpers
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"   // the live-emitter list Render walks
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"       // CELL_RENDER_FLAG / Material()
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"          // cParticleLocator::GetMat
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBindings.h"             // the emitter binding block
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"   // the locked buffer + its Begin/EndBatch window
// ⚠ THE LION SDK REACHES INTO THE GAME HERE, AND THAT IS THE CONSOLE'S OWN SHAPE, not a shortcut:
// cParticleRender::Render and ::EmitterRender call
// BrnParticle::LionParticleRender::GetCameraMatrix / RenderGroupBeginLite / GetVertexStride /
// Render / RenderGroupEndLite as DIRECT calls, NOT through the iParticleRender vtable -- the Lion
// runtime in this build is compiled knowing its one concrete renderer. (The Vector arm calls a
// second, different helper directly, which is what proves these are not devirtualised
// vtable calls: one vtable slot cannot resolve to two different targets.)
#include "GameSource/Effects/Particles/LionParticleRender.h"    // BrnParticle::LionParticleRender
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // the one-shot EmitterCubeRender announcement
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"   // shadow::Device
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

#include <cstddef>   // offsetof (the layout pins at the foot of this file)

// Native XDK APIs carry the real D3D types and the 64-bit PendingMask3 argument.
#include <d3d9.h>
extern D3DDevice* gpD3DDevice;

static const D3DPRIMITIVETYPE KU_PARTICLE_PRIMITIVE_TYPE = D3DPT_QUADLIST;

// ----------------------------------------------------------------------------
// cParticleRender::Dispatch
//
// Replay the frame's accumulated batch list to the device. Called by cLionFX::Dispatch.
// ----------------------------------------------------------------------------
void cParticleRender::Dispatch(renderengine::VertexBuffer* apVertexBuffer,
                               const LionBatchArray& arBatchArray,
                               float32_t afWhiteLevel,
                               bool8_t abEnableZFade,
                               float32_t afNearPlane,
                               float32_t afFarPlane,
                               float32_t afDepthFadeDistance,
                               float32_t afDepthSamplerOffsetU,
                               float32_t afDepthSamplerOffsetV,
                               renderengine::TextureState* apDepthTextureState)
{
    // Nothing to replay if the batch list is empty. GetLength() fires the "Array used before
    // Construct/Clear was called" assert on the -1 sentinel, matching the console length read.
    if (arBatchArray.GetLength() == 0)
    {
        return;
    }

    // Reset the shadow device and bind the empty stream / particle sampler state for the pass.
    shadow::Device::ResetShadowing();
    D3DDevice_SetStreamSource(gpD3DDevice, 0, reinterpret_cast<D3DVertexBuffer*>(apVertexBuffer), 0, 0, 1);
    shadow::Device::SetState(const_cast<CgsGraphics::SamplerState*>(
        CgsGraphics::ImRendererBase::mgStateLibrary.mpSamplerState_Linear_MipNearest_ClampUV), 0);

    mpRenderer->BeginRendering(afWhiteLevel, abEnableZFade, afNearPlane, afFarPlane,
                               afDepthFadeDistance, afDepthSamplerOffsetU, afDepthSamplerOffsetV,
                               apDepthTextureState);

    // Walk every batch, opening a render group each time the material changes and issuing one
    // DrawVertices per batch. The length is re-read each iteration (its assert re-fires) to match
    // the console loop.
    const cParticleMaterial* lpCurrentMaterial = 0;
    for (u32 luIndex = 0; luIndex < arBatchArray.GetLength(); ++luIndex)
    {
        const LionBatch& lrBatch = arBatchArray.GetItem(luIndex);
        CGS_ASSERT(lrBatch.GetVertexCount() > 0, "lBatch.GetVertexCount() > 0");

        if (lrBatch.GetMaterial() != lpCurrentMaterial)
        {
            if (lpCurrentMaterial != 0)
            {
                mpRenderer->RenderGroupEnd();
            }
            lpCurrentMaterial = lrBatch.GetMaterial();
            mpRenderer->RenderGroupBegin(*lpCurrentMaterial);
        }

        const u32 luVertexStride = mpRenderer->GetVertexStride(*lpCurrentMaterial);
        D3DDevice_SetStreamSource(gpD3DDevice, 0, reinterpret_cast<D3DVertexBuffer*>(apVertexBuffer), 0, luVertexStride, 1);

        const u32 luStartVertex = lrBatch.GetStartVertex();
        const u32 luVertexCount = lrBatch.GetVertexCount();
        shadow::Device::FlushVertexProgramState();
        D3DDevice_DrawVertices(gpD3DDevice, KU_PARTICLE_PRIMITIVE_TYPE, luStartVertex, luVertexCount);


    }

    mpRenderer->RenderGroupEnd();
    mpRenderer->EndRendering();
}

// ================================================================================================
// cParticleRender::Instance / ::AppInit / ::Update
//
// ⭐ NONE OF THE THREE HAS A BODY OF ITS OWN IN THE CONSOLE IMAGE. All three are inlined into
// cLionFX::Init and cLionFX::Update -- and cLionFX::Init had no dossier of its own, being named
// only from cParticleSystem::AppInit's cross-references. Its behaviour was recovered directly
// from the shipped image and cross-checked against the original headers, which declare all
// three.
// ================================================================================================

// ------------------------------------------------------------------------------------------------
// cParticleRender::Instance  (declared in the original header)
//
// A function-local static: that is the exact construct MSVC compiles into the guard-word +
// atexit pair every inlining call site shows -- test the guard's low bit, set it, then register
// a dynamic atexit destructor for the object. The mangled destructor symbol names both the
// accessor (Instance) and the object (m_instance). The object sits exactly 0x460 bytes before
// its own guard word, which is sizeof(cParticleRender): see the layout note in the header.
// ------------------------------------------------------------------------------------------------
cParticleRender& cParticleRender::Instance()
{
    static cParticleRender m_instance;
    return m_instance;
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::AppInit  (declared in the original header)
//
// Recovered from the inlined copy inside cLionFX::Init, store for store. The eight lod distances
// come from eight separate read-only float constants in the shipped image:
//     +0x40 <- 100.0     +0x50 <-  60.0
//     +0x44 <-  90.0     +0x54 <-  50.0
//     +0x48 <-  80.0     +0x58 <-  40.0
//     +0x4C <-  70.0     +0x5C <-  30.0
// Eight distinct constant slots rather than one table is what makes these eight separate literal
// statements in the source rather than an initialised array -- so they are written as eight
// SetLodDistance calls, which is also the accessor the original header gives them.
//
// ⛔ NOT TUNING. These are the console's own numbers, read out of the console's own image. If a
// particle LOD later looks wrong, the fix is a transcription defect somewhere else, not a nudge
// to one of these.
//
// ⚠ WHAT IS *NOT* HERE. The console body leaves mParticlesRenderedCount, mFogEnabledFlag,
// mFogNear, mFogFar, mCamPos, mCamDir and mFogAlphas[256] UNWRITTEN by AppInit -- there is no
// store to +0x08..+0x1C, +0x20..+0x3F or +0x60..+0x45F anywhere in this body. They are zero
// because m_instance is a static, and the fog/camera lanes are written by the per-frame path.
// Zeroing them here would be an invented arm.
// ------------------------------------------------------------------------------------------------
void cParticleRender::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                              iParticleRender* apRenderer)
{
    mpAllocator = apAllocator;   // the store at record +0x00
    mpRenderer  = apRenderer;    // the store at record +0x04

    SetLodDistance(0, 100.0f);
    SetLodDistance(1,  90.0f);
    SetLodDistance(2,  80.0f);
    SetLodDistance(3,  70.0f);
    SetLodDistance(4,  60.0f);
    SetLodDistance(5,  50.0f);
    SetLodDistance(6,  40.0f);
    SetLodDistance(7,  30.0f);
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::Update  (declared in the original header)
//
// ⚠ ATTESTED EMPTY -- this is a transcription, not a stub, and the distinction is the whole point
// of the trap-stub rule. The original cLionFX::Update makes exactly two calls:
// cParticleRender::Instance() and cParticleRender::Update(). Its console body is nine
// instructions long: the magic-static guard, the atexit registration, and a tail call to
// cParticleEmitterManager::Update. There is no third call, no store, and nothing else
// between the guard and the tail -- so Update() had nothing in it on this build. A __debugbreak
// here would fire every frame for a function the console runs every frame and that does nothing.
// ------------------------------------------------------------------------------------------------
void cParticleRender::Update(const cTime& /*arTime*/)
{
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::_AssertLayout -- layout pins. Never executed (offsetof folds at compile time in
// an uncalled function); it is a private static member so it can see the private members it pins.
//
// ⭐⭐ THESE ARE ABSOLUTE CONSOLE OFFSETS, AND THAT IS NOT AN OVERSIGHT. Almost every other layout
// pin in this tree asserts DELTAS, because a record with pointers in it is wider on the host. This
// record is the exception and the assertions prove it: the console's two 4-byte pointers plus four
// 4-byte scalars come to 0x18, which the 16-byte-aligned mCamPos pads to 0x20; on the host the same
// two pointers are 8 bytes each, giving 0x20 with NO padding. The widening is absorbed by padding
// the console already had, so every member from mCamPos onward -- including mLodDistances at +0x40,
// which is where cLionFX::Init's eight stores land -- sits at the SAME byte offset on both ABIs,
// and sizeof is 0x460 on both.
//
// ⭐ SEEN TO FAIL, and it taught me the paragraph above. The first version of the size pin asserted
// `0x460 + 2 * (sizeof(void*) - 4)` -- the usual "console size plus the widened pointers" formula --
// and MSVC rejected it (C2338) on the very first gate run. That formula is wrong twice over here:
// the widening is absorbed by padding, AND 0x468 is not a multiple of the record's own 16-byte
// alignment, so it could not have been a size at all. The member-offset pins below were what
// localised it. The alignment pin was then deliberately broken (cVector's alignas dropped to 4) to
// confirm it fires on its own message, and restored.
// ------------------------------------------------------------------------------------------------
void cParticleRender::_AssertLayout()
{
    // The two cVectors must be 16-byte aligned: that alignment is what puts mLodDistances on 0x40.
    CGS_STATIC_ASSERT((__alignof(cVector) == 16), "cVector must be 16-byte aligned (vector stride/align)");
    CGS_STATIC_ASSERT((sizeof(cVector) == 16),  "cVector must be 16 bytes");

    CGS_STATIC_ASSERT((offsetof(cParticleRender, mpAllocator) == 0x00),
                  "mpAllocator at +0x00 -- cLionFX::Init's first store");
    CGS_STATIC_ASSERT((offsetof(cParticleRender, mpRenderer) == sizeof(void*)),
                  "mpRenderer follows mpAllocator -- cLionFX::Init's second store");
    CGS_STATIC_ASSERT((offsetof(cParticleRender, mCamPos) == 0x20),
                  "mCamPos at +0x20 (declared in the original header) -- 16-byte aligned after the "
                  "two pointers + four scalars");
    CGS_STATIC_ASSERT((offsetof(cParticleRender, mCamDir) == 0x30),
                  "mCamDir at +0x30 (declared in the original header)");
    CGS_STATIC_ASSERT((offsetof(cParticleRender, mLodDistances) == 0x40),
                  "mLodDistances at +0x40 -- the eight float stores in cLionFX::Init");
    CGS_STATIC_ASSERT((offsetof(cParticleRender, mFogAlphas) == 0x60),
                  "mFogAlphas at +0x60, right after the 8-entry lod table");
    CGS_STATIC_ASSERT((sizeof(cParticleRender) == 0x460),
                  "sizeof(cParticleRender) == 0x460 -- the object ends exactly at "
                  "its magic-static guard word");
}

// =================================================================================================
// THE PER-FRAME RENDER DRIVER (landed 2026-09-05, the boost-exhaust wave).
//
// cParticleRender::Render walks the manager's live-emitter list once per frame, culls each emitter
// against the camera, and hands the survivors to EmitterRender (or, for a CELL_RENDER descriptor,
// EmitterCubeRender). EmitterRender walks that emitter's bucket list, drives the three
// SimulateParticlesInBucketGeneral<> kernels, and streams the surviving particles into the frame's
// vertex buffer as one LionBatch per material run. Everything below this is already landed:
// LionParticleRender::Render -> LionBlendRenderer::RenderSprites / RenderQuads / RenderTilts ->
// QuadDraw -> the LionBlendVertex writer.
//
// ⚠ THE PERFMON BRACKETS ARE NOT REPRODUCED, for this project's standing reason (the same
// paragraph ParticleModule.cpp carries): nothing on this build calls LionPerfMon::Construct, so
// every one of the six perfmon id words is 0 and a bracket here would time one
// shared id -- a diagnostic that reports something other than its name. They are timing only; no
// behaviour rides on them.
// =================================================================================================

// The Lion runtime's single fog descriptor (cLionFog::mSingleton, declared in the original
// header). Both EmitterRender and EmitterCubeRender pass its address as the `lpFog` argument
// of every iParticleRender::Render call. cLionFog has no reconstructed body in this tree and
// nothing on the landed draw path reads the pointer (LionParticleRender::Render's apFog parameter
// is unused), so it is carried as a null here rather than pointing at a fabricated object --
// stated, not hidden. DELETE-WHEN cLionFog lands: this becomes &cLionFog::mSingleton.
static const cLionFog* const gpLionFogSingleton = 0;   // stands in for &cLionFog::mSingleton

// The cull constants, all three read out of the shipped image rather than chosen:
//   10000.0  -- the RANGE test is on the SQUARED distance, so this is 100 m.
//   8.0      -- the cull radius, splatted across all four lanes by a startup initialiser.
//   -8.0     -- the same 8 metres, behind the eye.
static const f32 KF_EMITTER_CULL_RANGE_SQ = 10000.0f;
static const f32 KF_EMITTER_CULL_RADIUS   = 8.0f;
static const f32 KF_EMITTER_CULL_BEHIND   = -8.0f;

// The simulation run EmitterRender streams through: 32 RenderedParticle and 32 side-array
// elements (the original locals are `RenderedParticle[32] lParticle` and
// `cMatrix[32] lParticleMatrices`), which is also what the console stack frame measures: 0x800
// bytes of cMatrix and 0xE00 bytes of RenderedParticle.
static const u32 KU_SIMULATION_RUN = 32;

// ------------------------------------------------------------------------------------------------
// cParticleRender::Render   (the original locals are lMat / lpEmitter)
//
// ⭐⭐ THE TWO CONSTANT TABLES THE OLD TRAP CALLED "UN-RECOVERED" ARE BOTH READ, and neither is
// exotic -- they are dynamically initialised at startup, so they read as zero in the image:
//     a lane-permute table holding the word 0x0004080C four times
//                     => permuting a vector through it gathers the TOP BYTE of each of its four
//                        words into every byte of every output word: the classic "reduce four
//                        lane masks to one".
//     a constant vector holding 8.0 splatted across all four lanes
//                     => an 8-metre cull radius, and the same 8 the near test's -8.0 uses.
//
// WHAT IT DOES, in the console's order:
//   1. Take the camera basis from the RENDERER, not from this object:
//      LionParticleRender::GetCameraMatrix returns mCameraTransform by value, and this function
//      keeps its TRANSLATION row as mCamPos (stored at +0x20) and its Z row as mCamDir
//      (stored at +0x30). ⚠ Row 3 is the position and row 2 is the
//      direction -- taking row 3 for both (or transposing them) silently culls the whole world.
//   2. mParticlesRenderedCount = 0 (the word at +0x08), the per-frame vertex tally EmitterRender
//      accumulates into.
//   3. For every emitter on the manager's USED list (mpUsed @+0x18, walked by mpNext @+0x204):
//        * descriptor CELL_RENDER_FLAG (0x8) -> EmitterCubeRender, unconditionally. No cull: a
//          cell emitter is anchored to the camera, so it is always on screen.
//        * else, only if the emitter is ACTIVE (mFlags bit 0):
//            - RANGE: |locatorPos - camPos|^2 < 10000, i.e. 100 m.
//            - FRUSTUM: the renderer's packed LRTB planes at +0x120..+0x150, four planes at a time.
//              row0*p.x + row1*p.y + row2*p.z + 8 must be > the plane distances on ALL FOUR lanes.
//              ⭐ THE PLANE CONVENTION IS THE CAMERA'S OWN and the two halves agree exactly:
//              CgsGraphics::Camera stores each plane as (N, D) with dot3(N,p) == D and N pointing
//              INTO the volume (see CgsCamera.h), so `dot + 8 > D` is "inside, with an 8 m slack" --
//              the same 8 the splatted constant above carries. The four-lane AND is the
//              permute-and-compare reduction: gather the four masks' top bytes, test the word
//              against 0xFFFFFFFF (== all four inside), then read the all-true condition bit.
//              ⚠ THE PACKED ROWS ARE SoA, NOT FOUR PLANES. ParticleModule::BuildLionVertexBuffers
//              builds them by transposing GetFrustum's planes 2..5 (left/right/top/bottom) with
//              a permute/shift sequence, so row 0 is (Lx,Rx,Tx,Bx) and row 3 is (Ld,Rd,Td,Bd).
//              Reading a row as one plane is the mistake that would make this cull nonsense.
//            - NEAR: dot(mCamDir, locatorPos - camPos) > -8.0. The same 8 metres
//              again, this time behind the eye, which is why an emitter just behind the camera
//              plane still draws its trailing particles.
//          Survivors go to EmitterRender.
//
// ⚠ THE RANGE AND NEAR TESTS MEASURE FROM THE LOCATOR and there is no per-emitter bounds volume
// anywhere in this function. The 8 m slack IS the emitter's assumed radius; an effect wider than
// that pops at the screen edge on the console too.
// ------------------------------------------------------------------------------------------------
void cParticleRender::Render(EffectsVertexBufferLocked& arVertexBuffer,
                             LionBatchArray& arBatchArray,
                             cParticleEmitterManager& arEmitterManager,
                             const cTime& arTime)
{
    BrnParticle::LionParticleRender* const lpRenderer =
        static_cast<BrnParticle::LionParticleRender*>(mpRenderer);

    // --- the camera basis, out of the concrete renderer ---------------------------------------
    const cMatrix lCameraTransform = lpRenderer->GetCameraMatrix();

    mCamPos = lCameraTransform.wa;   // stored at +0x20
    mCamDir = lCameraTransform.za;   // stored at +0x30
    mParticlesRenderedCount = 0;

    const rw::math::vpu::Matrix44& lrFrustum = lpRenderer->GetPackedFrustumLrtb();

    for (cParticleEmitter* lpEmitter = arEmitterManager.GetpUsed();
         lpEmitter != 0;
         lpEmitter = lpEmitter->GetNextEmitter())
    {

        if ((lpEmitter->GetDescriptor()->Flags() & cParticleDescriptor::E_FLAG_CELL_RENDER) != 0)
        {
            EmitterCubeRender(arVertexBuffer, arBatchArray, lpEmitter, arTime);
            continue;
        }

        if (!lpEmitter->IsActive())
        {
            continue;
        }

        const cMatrix& lMat = lpEmitter->GetBindings().GetpLocator()->GetMat(arTime);

        // ---- range ---------------------------------------------------------------------------
        const f32 lfDx = lMat.wa.x - mCamPos.x;
        const f32 lfDy = lMat.wa.y - mCamPos.y;
        const f32 lfDz = lMat.wa.z - mCamPos.z;
        const f32 lfRangeSq = lfDx * lfDx + (lfDy * lfDy + lfDz * lfDz);
        if (lfRangeSq >= KF_EMITTER_CULL_RANGE_SQ)
        {
            continue;
        }

        // ---- frustum (the four packed LRTB planes, all four lanes) -----------------------------
        const f32 lafDistance[4] =
        {
            lrFrustum.xAxis.x * lMat.wa.x + lrFrustum.yAxis.x * lMat.wa.y
                + lrFrustum.zAxis.x * lMat.wa.z + KF_EMITTER_CULL_RADIUS,
            lrFrustum.xAxis.y * lMat.wa.x + lrFrustum.yAxis.y * lMat.wa.y
                + lrFrustum.zAxis.y * lMat.wa.z + KF_EMITTER_CULL_RADIUS,
            lrFrustum.xAxis.z * lMat.wa.x + lrFrustum.yAxis.z * lMat.wa.y
                + lrFrustum.zAxis.z * lMat.wa.z + KF_EMITTER_CULL_RADIUS,
            lrFrustum.xAxis.w * lMat.wa.x + lrFrustum.yAxis.w * lMat.wa.y
                + lrFrustum.zAxis.w * lMat.wa.z + KF_EMITTER_CULL_RADIUS,
        };
        const bool lbInsideAllFour = (lafDistance[0] > lrFrustum.wAxis.x)
                                  && (lafDistance[1] > lrFrustum.wAxis.y)
                                  && (lafDistance[2] > lrFrustum.wAxis.z)
                                  && (lafDistance[3] > lrFrustum.wAxis.w);
        if (!lbInsideAllFour)
        {
            continue;
        }

        // ---- near ------------------------------------------------------------------------------
        const f32 lfAlongView = mCamDir.x * lfDx + (mCamDir.y * lfDy + mCamDir.z * lfDz);
        if (lfAlongView <= KF_EMITTER_CULL_BEHIND)
        {
            continue;
        }

        EmitterRender(arVertexBuffer, arBatchArray, lpEmitter, arTime);
    }

}

// ------------------------------------------------------------------------------------------------
// cParticleRender::EmitterRender
//
// ONE emitter, one material, one batch. The local names below are the original source's own:
// lMaterial / lCurrentLocatorTime / lBindingsLocatorMat / lpFog /
// lpBucket / luVertexStride / lVertexIterator / lBatch / lParticle[32] / lParticleMatrices[32] /
// lParticleVectors[32] / lTotalNumParticlesSimulated.
//
// THE SHAPE:
//   RenderGroupBeginLite(material)                       -- bind the material for the whole run
//   lBindingsLocatorMat = locator->GetMat(aTime)
//   if the emitter has any buckets:
//       luVertexStride = GetVertexStride(material)
//       BeginBatch(lVertexIterator, lBatch, luVertexStride)
//       for each bucket in the emitter's list:
//           simulate it into lParticle[] + the side array, appending to whatever is already there
//           if the run is nearly full (n + 16 >= 32) OR this was the last bucket:
//               draw the run and reset the count
//       EndBatch(lVertexIterator, lBatch, luVertexStride)
//       if the batch emitted anything: tag it with the material and Append it
//   RenderGroupEndLite()
//
// ⭐ THE `n + 16 >= 32` FLUSH IS A HEADROOM TEST, NOT A FULLNESS TEST: the console adds 16 to the
// running count and branches when the sum reaches 32. 16 is cParticleBucket::KU_MAX_PARTICLES -- the most the NEXT
// bucket could add -- and 32 is the arrays' capacity. So it flushes when one more bucket could
// overflow, which is why the arrays are 32 and not 16: two buckets' worth minus one.
//
// ⭐ THE THREE ARMS PICK THEIR KERNEL FROM THE **FIRST** BUCKET AND NEVER RE-TEST IT
// (it reads the kind word at emitter +0xE60 while still on the list head). All of an emitter's buckets come
// from the same descriptor and therefore the same pool, so the kind cannot change mid-list; but
// the loop really is three separate loops in the binary, each with its own arrays, which is why
// this reads as duplication rather than one loop with a switch inside.
//
// ⚠ THE DRAW IS GATED ON THE BINDING'S WORLD INDEX (the binding pointer at emitter +0x1FC, then
// its own +0x04 -- i.e. cLionBindings::GetWorldIndex() == 0). The simulation still runs for a non-zero world -- the
// particles advance and the count is still added to mParticlesRenderedCount -- only the draw call
// is skipped. That is how the player's own effects leave the outside view while the bumper camera
// is up without their state drifting.
//
// ⚠ AND THE VECTOR ARM CALLS A DIFFERENT RENDER OVERLOAD (the `const cVector*` Render the
// original header declares alongside the cMatrix one), not the cMatrix one. It is the same draw
// with an adapter in front of it; see LionParticleRender.cpp.
// ------------------------------------------------------------------------------------------------
void cParticleRender::EmitterRender(const EffectsVertexBufferLocked& arVertexBuffer,
                                    const LionBatchArray& arBatchArray,
                                    cParticleEmitter* apEmitter,
                                    const cTime& arTime)
{
    const cParticleMaterial& lMaterial = *apEmitter->GetDescriptor()->Material();
    BrnParticle::LionParticleRender* const lpRenderer =
        static_cast<BrnParticle::LionParticleRender*>(mpRenderer);

    lpRenderer->RenderGroupBeginLite(lMaterial);

    // The console passes aTime for BOTH time parameters of the kernels (r7 and r8 are both r30).
    const cTime& lCurrentLocatorTime = arTime;
    const cMatrix& lBindingsLocatorMat =
        apEmitter->GetBindings().GetpLocator()->GetMat(arTime);

    const cLionFog* const lpFog = gpLionFogSingleton;

    cParticleBucket* lpBucket = apEmitter->GetBucket();
    if (lpBucket != 0)
    {
        const u32 luVertexStride = lpRenderer->GetVertexStride(lMaterial);

        EffectsVertexBufferIterator lVertexIterator;
        LionBatch lBatch;
        const_cast<EffectsVertexBufferLocked&>(arVertexBuffer)
            .BeginBatch(lVertexIterator, lBatch, luVertexStride);

        if (lpBucket->HasMatrices())
        {
            RenderedParticle lParticle[KU_SIMULATION_RUN];
            cMatrix          lParticleMatrices[KU_SIMULATION_RUN];
            u32              lTotalNumParticlesSimulated = 0;

            do
            {
                lTotalNumParticlesSimulated += apEmitter->SimulateParticlesInBucketGeneral(
                    MatrixSimulationHelper(&lParticleMatrices[lTotalNumParticlesSimulated]),
                    &lParticle[lTotalNumParticlesSimulated],
                    lpBucket, arTime, lCurrentLocatorTime, lBindingsLocatorMat);

                if ((lTotalNumParticlesSimulated + cParticleBucket::KU_MAX_PARTICLES
                        >= KU_SIMULATION_RUN
                     || lpBucket->GetEmitterNext() == 0)
                    && lTotalNumParticlesSimulated != 0)
                {
                    if (apEmitter->GetBindings().GetWorldIndex() == 0)
                    {
                        lpRenderer->Render(lVertexIterator, lParticle, lParticleMatrices,
                                           lTotalNumParticlesSimulated, 0, apEmitter, lpFog,
                                           arTime);
                    }
                    mParticlesRenderedCount += lTotalNumParticlesSimulated;
                    lTotalNumParticlesSimulated = 0;
                }

                lpBucket = lpBucket->GetEmitterNext();
            }
            while (lpBucket != 0);
        }
        else if (lpBucket->HasVectors())
        {
            RenderedParticle lParticle[KU_SIMULATION_RUN];
            cVector          lParticleVectors[KU_SIMULATION_RUN];
            u32              lTotalNumParticlesSimulated = 0;

            do
            {
                lTotalNumParticlesSimulated += apEmitter->SimulateParticlesInBucketGeneral(
                    VectorSimulationHelper(&lParticleVectors[lTotalNumParticlesSimulated]),
                    &lParticle[lTotalNumParticlesSimulated],
                    lpBucket, arTime, lCurrentLocatorTime, lBindingsLocatorMat);

                if ((lTotalNumParticlesSimulated + cParticleBucket::KU_MAX_PARTICLES
                        >= KU_SIMULATION_RUN
                     || lpBucket->GetEmitterNext() == 0)
                    && lTotalNumParticlesSimulated != 0)
                {
                    if (apEmitter->GetBindings().GetWorldIndex() == 0)
                    {
                        lpRenderer->Render(lVertexIterator, lParticle, lParticleVectors,
                                           lTotalNumParticlesSimulated, 0, apEmitter, lpFog,
                                           arTime);
                    }
                    mParticlesRenderedCount += lTotalNumParticlesSimulated;
                    lTotalNumParticlesSimulated = 0;
                }

                lpBucket = lpBucket->GetEmitterNext();
            }
            while (lpBucket != 0);
        }
        else
        {
            RenderedParticle lParticle[KU_SIMULATION_RUN];
            cMatrix          lParticleMatrices[KU_SIMULATION_RUN];
            u32              lTotalNumParticlesSimulated = 0;

            do
            {
                lTotalNumParticlesSimulated += apEmitter->SimulateParticlesInBucketGeneral(
                    LocalSimulationHelper(&lParticleMatrices[lTotalNumParticlesSimulated]),
                    &lParticle[lTotalNumParticlesSimulated],
                    lpBucket, arTime, lCurrentLocatorTime, lBindingsLocatorMat);

                if ((lTotalNumParticlesSimulated + cParticleBucket::KU_MAX_PARTICLES
                        >= KU_SIMULATION_RUN
                     || lpBucket->GetEmitterNext() == 0)
                    && lTotalNumParticlesSimulated != 0)
                {
                    if (apEmitter->GetBindings().GetWorldIndex() == 0)
                    {
                        lpRenderer->Render(lVertexIterator, lParticle, lParticleMatrices,
                                           lTotalNumParticlesSimulated, 0, apEmitter, lpFog,
                                           arTime);
                    }
                    mParticlesRenderedCount += lTotalNumParticlesSimulated;
                    lTotalNumParticlesSimulated = 0;
                }

                lpBucket = lpBucket->GetEmitterNext();
            }
            while (lpBucket != 0);
        }

        const_cast<EffectsVertexBufferLocked&>(arVertexBuffer)
            .EndBatch(lVertexIterator, lBatch, luVertexStride);

        if (lBatch.GetVertexCount() != 0)
        {
            lBatch.mpMaterial = &lMaterial;
            const_cast<LionBatchArray&>(arBatchArray).Append(lBatch);
        }
    }

    lpRenderer->RenderGroupEndLite();
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::EmitterCubeRender  (448 instructions on the console) -- NOT RECONSTRUCTED, and
// announced ONCE rather than asserted or passed over without a word.
// (The two-word phrase for that last failure mode is what the faithfulness lint flags as
// invented-format vocabulary, so it is spelled out longhand -- same reason BrnLionBlendRenderer.cpp
// spells it out in its own SetState note.)
//
// WHY A NAMED LOG AND NOT A TRAP: it IS reachable now. cParticleRender::Render routes every
// emitter whose descriptor carries CELL_RENDER_FLAG (0x8) here, before the active test and before
// any cull, so a single cell effect in the loaded bundle would turn a CGS_ASSERT into a per-frame
// assert storm -- the failure mode this project has measured at 839,983 lines in one run. Same
// call LionParticleRender::CreateInternalMaterial's banner makes, for the same reason.
//
// WHAT IT IS, from the pseudocode, so the next wave does not start cold. A CELL emitter is a
// camera-anchored volume (rain / dust / snow): its particles are wrapped into an axis-aligned box
// that follows the camera, and faded by distance from the box centre.
//   * The box is built from the descriptor's mpBehaviour +0x280 half-extent and
//     the camera position, one axis at a time: lo = camPos.a * e + locator.a - e, hi = ... + e,
//     with the wrap span 2*e.
//   * BeginBatch / the three simulation kernels / EndBatch / Append are the SAME shape as
//     EmitterRender above -- one batch, one material, the same three-way bucket-kind selection,
//     and the same cVector-overload vs cMatrix-overload split on the vector arm. The
//     only structural difference is that it uses ONE pair of arrays for all three kinds
//     and draws after every bucket rather than on a headroom test.
//   * Between simulate and draw it runs TWO extra passes the other renderer does not have:
//       - a DISTANCE FADE, unrolled x4: d2 = |particle.mPos - camPos|^2, then
//         alpha' = alpha - alpha * (1 - d2/(2e)^2) * K, with a branchless select for the
//         clamp. K is NOT yet read out of the image.
//       - a WRAP: each of the three position axes is folded back into [lo, hi] with a
//         `(p - hi) / span` truncate-and-subtract, which is what makes the volume infinite.
//
// WHAT IS NEEDED TO FINISH IT: the value of that fade constant K, the exact lane order of the four-wide
// fade (the unrolled block indexes lParticle at +3/+31/+59/+87 floats, i.e. the .w of mPos across
// four 112-byte records), and the three-axis wrap's sign conventions. All three are ordinary
// reads; none is blocked.
//
// WHAT SKIPPING IT COSTS TODAY: a CELL_RENDER effect simulates not at all and draws nothing. No
// other emitter kind is affected -- Render's test is exclusive.
// ------------------------------------------------------------------------------------------------
void cParticleRender::EmitterCubeRender(const EffectsVertexBufferLocked& /*arVertexBuffer*/,
                                        const LionBatchArray& /*arBatchArray*/,
                                        cParticleEmitter* /*apEmitter*/,
                                        const cTime& /*arTime*/)
{
    static bool sbLogged = false;
    if (!sbLogged)
    {
        sbLogged = true;
        CgsDev::Log::WriteToLog(
            "[effects] NOT RECONSTRUCTED: cParticleRender::EmitterCubeRender (the "
            "CELL_RENDER camera-anchored volume: the per-particle distance fade and the "
            "three-axis wrap). Emitters with CELL_RENDER_FLAG neither simulate nor draw; every "
            "other emitter kind is unaffected.\n");
    }
}
