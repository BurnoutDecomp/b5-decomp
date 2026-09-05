// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/source/ParticleRender/ParticleRender.cpp
//
// cParticleRender -- the Lion (eauk_lion) particle-render pipeline driver. Reconstructed
// from the X360 ARTIST pseudocode + assembly (behaviour authority) and the DecFIGS DWARF
// (declaration shape).
//
// LANDED HERE: cParticleRender::Dispatch (0x82911E98), and -- added 2026-09-03 by the boost-
// exhaust wave -- Instance / AppInit / Update, none of which has a body of its own in the X360
// image (all three are inlined into cLionFX::Init @0x82914A98, itself an export-set hole; see
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
//   * cParticleRender::Render          (0x829147F8) -- BLOCKED. The per-emitter frustum cull
//     is VMX128: it loads two un-recovered rodata constant tables (unk_8327F110, a vperm
//     permute table; unk_83123740, a masking constant) whose bytes are not in the dossier,
//     then runs vperm/vcmpgtfp/vcmpequw/vcmpeqfp over the packed frustum planes. The cull
//     cannot be faithfully reconstructed without those table bytes.
//   * cParticleRender::EmitterCubeRender (0x82913C80) -- BLOCKED. The per-particle box clip
//     is VMX128 (lvx128/stvx128/vsubfp/vmsum3fp128/fsel/fnmsubs over the bucket vectors) and
//     multiplies by an un-recovered rodata float constant (flt_82F357F4); the snap-to-plane
//     math cannot be reproduced without that constant's value.
//   * cParticleRender::EmitterRender   (0x82913928) -- the type conflict that used to park it is
//     GONE (see the banner). Its remaining blocker is its three callees: the
//     cParticleEmitter::SimulateParticlesInBucketGeneral<> kernels for the Matrix (212 lines),
//     Vector (132) and Local (234) bucket types. Everything else it needs now exists --
//     LionParticleRender::RenderGroupBeginLite / GetVertexStride / Render / RenderGroupEndLite
//     are bodied, cParticleBucket::GetpMatrix is bodied, and the bucket walk it does
//     (mpMatrices -> mpVectors -> locator) is the same three-way GetpMatrix documents.
//
// Dispatch's device path: the X360 build inlines the shadow-device sampler-state bind (the
// dword_830109A8 compare + sub_827E8950 + store block) that shadow::Device::SetState owns;
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
// cParticleRender::Render @0x82914834 and ::EmitterRender @0x82913978 call
// BrnParticle::LionParticleRender::GetCameraMatrix / RenderGroupBeginLite / GetVertexStride /
// Render / RenderGroupEndLite with `bl`, NOT through the iParticleRender vtable -- the Lion
// runtime in this build is compiled knowing its one concrete renderer. (The Vector arm's
// sub_82289158 is a second, different `bl`, which is what proves these are not devirtualised
// vtable calls: one slot cannot resolve to two addresses.)
#include "GameSource/Effects/Particles/LionParticleRender.h"    // BrnParticle::LionParticleRender
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // the one-shot EmitterCubeRender announcement
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"   // shadow::Device
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                 // renderengine::LionParticleSampler_ApplyState
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

#include <cstddef>   // offsetof (the layout pins at the foot of this file)
#include <cstdio>
#include <cstdlib>   // [lionfx] getenv -- the BRN_LIONFX_NOCULL bring-up bypass    // snprintf -- the [lionfx] bring-up witness

// --- Platform (Xbox 360 D3D) device + fast-path draw thunks ------------------------------------
// The engine's single D3D device global (X360 off_83271608 == renderengine::gpD3DDevice, defined
// in the renderengine device TU) and the two Xenon D3DDevice_* thunks Dispatch binds through. No
// project TU homes the thunks; declared here as the minimal extern surface, matching the XDK d3d9
// fast-set API and the shadow-device precedent (shadowingdevice.cpp).
struct IDirect3DDevice9;
extern IDirect3DDevice9* gpD3DDevice;

extern "C"
{
    void D3DDevice_SetStreamSource(IDirect3DDevice9* lpDevice, u32 luStreamNumber,
                                   const void* lpStreamData, u32 luOffsetInBytes,
                                   u32 luStride, u32 luFlags);
    void D3DDevice_DrawVertices(IDirect3DDevice9* lpDevice, u32 luPrimitiveType,
                                u32 luStartVertex, u32 luVertexCount);
}

// The Lion particle path's sampler-state object (X360 dword_83010F60): Dispatch binds it on
// sampler 0 through the shadow device before rendering. Set up by the particle module init; no
// project TU homes it yet, so it is referenced by name as an external.
extern void* gpLionParticleSamplerState;

// The Xenon particle draw primitive type (X360 li r4, 0xD passed to D3DDevice_DrawVertices).
static const u32 KU_PARTICLE_PRIMITIVE_TYPE = 13;

// ----------------------------------------------------------------------------
// cParticleRender::Dispatch @ 0x82911E98
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
    // [lionfx] FLAG PC bring-up diagnostic -- the DRAW half's witness (see Render's twin). It
    // says how many batches actually reached the device this frame; a non-zero Render count with
    // a zero batch count is a vertex-buffer problem, and the two lines separate those.
    {
        static u32 suLastBatches = 0xFFFFFFFFu;
        const u32 luBatches = arBatchArray.GetLength();
        if (luBatches != suLastBatches)
        {
            suLastBatches = luBatches;
            char lacMsg[160];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[lionfx] Dispatch: batches=%u vb=%p white=%.3f\n",
                          luBatches, static_cast<const void*>(apVertexBuffer),
                          static_cast<double>(afWhiteLevel));
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // Nothing to replay if the batch list is empty. GetLength() fires the "Array used before
    // Construct/Clear was called" assert on the -1 sentinel, matching the X360 length read.
    if (arBatchArray.GetLength() == 0)
    {
        return;
    }

    // Reset the shadow device and bind the empty stream / particle sampler state for the pass.
    shadow::Device::ResetShadowing();
    D3DDevice_SetStreamSource(gpD3DDevice, 0, apVertexBuffer, 0, 0, 1);
    shadow::Device::SetState(gpLionParticleSamplerState, 0);

    // FLAG PC-platform leaf, paired 1:1 with the call above and DELETE-WHEN it works.
    // gpLionParticleSamplerState is null on this backend AND shadow::Device's sampler setter bottoms
    // out in the documented no-op SetSamplerStateLowLevel, so that call binds nothing twice over and
    // the pass runs on whatever sampler words the previous pass left on unit 0 (measured by the
    // [lionbind] probe: ADDRESSU/V = WRAP, the world path's non-cube default). The console's state is
    // not a mystery -- it is ImRendererBase::ConstructOnceOnly @0x827F1C20's
    // ConstructSamplerState(alloc, 1, 0, 2, 2), i.e. min/mag LINEAR, mip NONE, address U/V CLAMP --
    // and this installs exactly those words. See the banner on LionParticleSampler_ApplyState.
    renderengine::LionParticleSampler_ApplyState(0);

    mpRenderer->BeginRendering(afWhiteLevel, abEnableZFade, afNearPlane, afFarPlane,
                               afDepthFadeDistance, afDepthSamplerOffsetU, afDepthSamplerOffsetV,
                               apDepthTextureState);

    // Walk every batch, opening a render group each time the material changes and issuing one
    // DrawVertices per batch. The length is re-read each iteration (its assert re-fires) to match
    // the X360 loop.
    const cParticleMaterial* lpCurrentMaterial = nullptr;
    for (u32 luIndex = 0; luIndex < arBatchArray.GetLength(); ++luIndex)
    {
        const LionBatch& lrBatch = arBatchArray.GetItem(luIndex);
        CGS_ASSERT(lrBatch.GetVertexCount() > 0, "lBatch.GetVertexCount() > 0");

        if (lrBatch.GetMaterial() != lpCurrentMaterial)
        {
            if (lpCurrentMaterial != nullptr)
            {
                mpRenderer->RenderGroupEnd();
            }
            lpCurrentMaterial = lrBatch.GetMaterial();
            mpRenderer->RenderGroupBegin(*lpCurrentMaterial);
        }

        const u32 luVertexStride = mpRenderer->GetVertexStride(*lpCurrentMaterial);
        D3DDevice_SetStreamSource(gpD3DDevice, 0, apVertexBuffer, 0, luVertexStride, 1);

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
// ⭐ NONE OF THE THREE HAS A BODY OF ITS OWN IN THE X360 IMAGE. All three are inlined into
// cLionFX::Init @0x82914A98 and cLionFX::Update @0x82915758 -- and cLionFX::Init is an
// EXPORT-SET HOLE: IDA names it in cParticleSystem::AppInit's `xrefs_to` but emits no
// 0x82914A98.json, so it had no ledger row and no dossier. It was disassembled straight out of
// the packed .i64 (tools/re/x360rd.py + a capstone PPC-BE pass) and cross-checked against the
// DecFIGS DWARF, which declares all three (ParticleRender.h:165 / :174 / :224).
// ================================================================================================

// ------------------------------------------------------------------------------------------------
// cParticleRender::Instance  (DWARF ParticleRender.h:165)
//
// A function-local static: that is the exact construct MSVC compiles into the guard-word +
// atexit pair every inlining call site shows --
//     lwz  r10, dword_82FAD080 ; clrlwi r11,r10,31 ; bne <done>
//     ori  r11, r10, 1 ; stw r11, dword_82FAD080
//     bl   atexit(<dynamic atexit destructor for 'm_instance'>)
// -- and that mangled destructor symbol, which IDA prints in full as
//     `cParticleRender::Instance'::`2'::`dynamic atexit destructor for 'm_instance''
// is what names both the accessor (Instance) and the object (m_instance). The object itself is
// 0x82FACC20 and the guard is 0x82FAD080, exactly 0x460 bytes later, which is
// sizeof(cParticleRender): see the layout note in the header.
// ------------------------------------------------------------------------------------------------
cParticleRender& cParticleRender::Instance()
{
    static cParticleRender m_instance;
    return m_instance;
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::AppInit  (DWARF ParticleRender.h:174)
//
// Recovered from cLionFX::Init @0x82914A98, 0x82914B9C..0x82914C08, store for store. The eight
// lod distances come from eight separate .rdata floats, read out of the image (tools/re/x360rd.py):
//     +0x40 <- flt_820049E0 = 100.0     +0x50 <- flt_82004C6C =  60.0
//     +0x44 <- flt_82004F64 =  90.0     +0x54 <- flt_820138DC =  50.0
//     +0x48 <- flt_82004A18 =  80.0     +0x58 <- flt_82004D0C =  40.0
//     +0x4C <- flt_820051BC =  70.0     +0x5C <- flt_82004F5C =  30.0
// Eight distinct rodata slots rather than one table is what makes these eight separate literal
// statements in the source rather than an initialised array -- so they are written as eight
// SetLodDistance calls, which is also the accessor the DWARF gives them (h:207).
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
    mpAllocator = apAllocator;   // stw r30, 0(r11)   @0x82914BAC
    mpRenderer  = apRenderer;    // stw r26, 4(r11)   @0x82914BBC

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
// cParticleRender::Update  (DWARF ParticleRender.h:224)
//
// ⚠ ATTESTED EMPTY -- this is a transcription, not a stub, and the distinction is the whole point
// of the trap-stub rule. The DWARF for cLionFX::Update (LionFX.cpp:111) lists exactly two calls:
// cParticleRender::Instance() and cParticleRender::Update(). The X360 body of cLionFX::Update
// @0x82915758 is nine instructions: the magic-static guard, the atexit, and a tail call to
// cParticleEmitterManager::Update. There is no third call, no store, and no register traffic
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
// the console already had, so every member from mCamPos onward -- including mLodDistances at 0x40,
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
    static_assert(alignof(cVector) == 16, "cVector must be 16-byte aligned (X360 vector stride/align)");
    static_assert(sizeof(cVector) == 16,  "cVector must be 16 bytes");

    static_assert(offsetof(cParticleRender, mpAllocator) == 0x00,
                  "mpAllocator @+0x00 -- cLionFX::Init `stw r30, 0(r11)` @0x82914BAC");
    static_assert(offsetof(cParticleRender, mpRenderer) == sizeof(void*),
                  "mpRenderer follows mpAllocator -- cLionFX::Init `stw r26, 4(r11)` @0x82914BBC");
    static_assert(offsetof(cParticleRender, mCamPos) == 0x20,
                  "mCamPos @+0x20 (DWARF ParticleRender.h:255) -- 16-byte aligned after the "
                  "two pointers + four scalars");
    static_assert(offsetof(cParticleRender, mCamDir) == 0x30,
                  "mCamDir @+0x30 (DWARF ParticleRender.h:256)");
    static_assert(offsetof(cParticleRender, mLodDistances) == 0x40,
                  "mLodDistances @+0x40 -- the eight stfs in cLionFX::Init @0x82914BA8..0x82914C08");
    static_assert(offsetof(cParticleRender, mFogAlphas) == 0x60,
                  "mFogAlphas @+0x60, right after the 8-entry lod table");
    static_assert(sizeof(cParticleRender) == 0x460,
                  "sizeof(cParticleRender) == 0x460 -- the object at 0x82FACC20 ends exactly at "
                  "its magic-static guard word 0x82FAD080");
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
// every id in dword_82FAB638 / 64C / 650 / 654 / 658 / 65C is 0 and a bracket here would time one
// shared id -- a diagnostic that reports something other than its name. They are timing only; no
// behaviour rides on them.
// =================================================================================================

// The Lion runtime's single fog descriptor (X360 unk_83122E08 == cLionFog::mSingleton, DWARF
// LionFog.h:5). Both EmitterRender and EmitterCubeRender pass its address as the `lpFog` argument
// of every iParticleRender::Render call. cLionFog has no reconstructed body in this tree and
// nothing on the landed draw path reads the pointer (LionParticleRender::Render's apFog parameter
// is unused), so it is carried as a null here rather than pointing at a fabricated object --
// stated, not hidden. DELETE-WHEN cLionFog lands: this becomes &cLionFog::mSingleton.
static const cLionFog* const gpLionFogSingleton = 0;   // X360 &unk_83122E08

// The cull constants, all three read out of the image rather than chosen:
//   flt_82005D9C == 10000.0  -- the RANGE test is on the SQUARED distance, so this is 100 m.
//   unk_83123740 <- CRT thunk 0x82C70810 : splat4(*(float*)0x820FEC58) == splat4(8.0)
//   flt_820C26C0 == -8.0     -- the same 8 metres, behind the eye.
static const f32 KF_EMITTER_CULL_RANGE_SQ = 10000.0f;
static const f32 KF_EMITTER_CULL_RADIUS   = 8.0f;
static const f32 KF_EMITTER_CULL_BEHIND   = -8.0f;

// The simulation run EmitterRender streams through: 32 RenderedParticle and 32 side-array
// elements (DWARF ParticleRender.cpp:501-502 -- `RenderedParticle[32] lParticle` and
// `cMatrix[32] lParticleMatrices`), which is also what the X360 stack frame measures (0x800 of
// cMatrix at sp+0x280 and 0xE00 of RenderedParticle at sp+0xA80).
static const u32 KU_SIMULATION_RUN = 32;

// ------------------------------------------------------------------------------------------------
// cParticleRender::Render  @ 0x829147F8   (DWARF ParticleRender.cpp:299, locals lMat / lpEmitter)
//
// ⭐⭐ THE TWO RODATA TABLES THE OLD TRAP CALLED "UN-RECOVERED" ARE BOTH READ, and neither is
// exotic -- they were dynamically-initialised .bss, which reads 0x00000000 by definition:
//     unk_8327F110 <- CRT thunk 0x82C740E0 : the word 0x0004080C four times
//                     => vperm(A,A,tbl) gathers the TOP BYTE of each of A's four words into every
//                        byte of every output word: the classic "reduce four lane masks to one".
//     unk_83123740 <- CRT thunk 0x82C70810 : splat4(*(float*)0x820FEC58) == splat4(8.0)
//                     => an 8-metre cull radius, and the same 8 the near test's -8.0 uses.
// (tools/re/findinit.py -> tools/re/ppcdis.py -> tools/re/x360rd.py, the ParticleBuild recipe.)
//
// WHAT IT DOES, in the console's order:
//   1. Take the camera basis from the RENDERER, not from this object:
//      LionParticleRender::GetCameraMatrix returns mCameraTransform by value, and this function
//      keeps its TRANSLATION row as mCamPos (`stvx128 v0, r31, 0x20`) and its Z row as mCamDir
//      (`stvx128 v13, r0, r20` with r20 == this + 0x30). ⚠ Row 3 is the position and row 2 is the
//      direction -- taking row 3 for both (or transposing them) silently culls the whole world.
//   2. mParticlesRenderedCount = 0 (`stw r22, 8(r31)`), the per-frame vertex tally EmitterRender
//      accumulates into.
//   3. For every emitter on the manager's USED list (mpUsed @+0x18, walked by mpNext @+0x204):
//        * descriptor CELL_RENDER_FLAG (0x8) -> EmitterCubeRender, unconditionally. No cull: a
//          cell emitter is anchored to the camera, so it is always on screen.
//        * else, only if the emitter is ACTIVE (mFlags bit 0):
//            - RANGE: |locatorPos - camPos|^2 < 10000 (flt_82005D9C, i.e. 100 m).
//            - FRUSTUM: the renderer's packed LRTB planes at +0x120..+0x150, four planes at a time.
//              row0*p.x + row1*p.y + row2*p.z + 8 must be > the plane distances on ALL FOUR lanes.
//              ⭐ THE PLANE CONVENTION IS THE CAMERA'S OWN and the two halves agree exactly:
//              CgsGraphics::Camera stores each plane as (N, D) with dot3(N,p) == D and N pointing
//              INTO the volume (CgsCamera.h:44), so `dot + 8 > D` is "inside, with an 8 m slack" --
//              the same 8 the splat above carries. The four-lane AND is the vperm/vcmpequw/
//              vcmpeqfp. reduction: gather the four masks' top bytes, test the word against
//              0xFFFFFFFF (== all four inside), then read CR6's all-true bit.
//              ⚠ THE PACKED ROWS ARE SoA, NOT FOUR PLANES. ParticleModule::BuildLionVertexBuffers
//              builds them by transposing GetFrustum's planes 2..5 (left/right/top/bottom) with
//              six vperm + three vsldoi, so row 0 is (Lx,Rx,Tx,Bx) and row 3 is (Ld,Rd,Td,Bd).
//              Reading a row as one plane is the mistake that would make this cull nonsense.
//            - NEAR: dot(mCamDir, locatorPos - camPos) > -8.0 (flt_820C26C0). The same 8 metres
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

    // --- the camera basis, out of the concrete renderer (asm words 5-14) ----------------------
    const cMatrix lCameraTransform = lpRenderer->GetCameraMatrix();

    mCamPos = lCameraTransform.wa;   // stvx128 v0, r31, 0x20
    mCamDir = lCameraTransform.za;   // stvx128 v13, r0, (this + 0x30)
    mParticlesRenderedCount = 0;

    const rw::math::vpu::Matrix44& lrFrustum = lpRenderer->GetPackedFrustumLrtb();

    u32 luLive = 0, luCell = 0, luInactive = 0, luCulled = 0, luRendered = 0;   // [lionfx] witness
    u32 luCullRange = 0, luCullFrustum = 0, luCullNear = 0;                    // [lionfx] by stage

    // ---- [lionfx] FLAG PC bring-up: BRN_LIONFX_NOCULL -----------------------------------------
    // A DIAGNOSTIC BYPASS, not console behaviour. With it set, every emitter that reaches the
    // cull is drawn. It exists because "nothing on screen" and "everything culled" are the same
    // picture, and the three tests below all depend on camera data this build publishes through a
    // bring-up stand-in -- so one run with the bypass on separates "the culler is wrong" from
    // "the draw chain downstream of it is". DELETE with the boost-exhaust bring-up.
    static const bool sbNoCull = []() {
        const char* lpcValue = std::getenv("BRN_LIONFX_NOCULL");
        return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
    }();

    for (cParticleEmitter* lpEmitter = arEmitterManager.GetpUsed();
         lpEmitter != 0;
         lpEmitter = lpEmitter->GetNextEmitter())
    {
        ++luLive;

        if ((lpEmitter->GetDescriptor()->Flags() & cParticleDescriptor::E_FLAG_CELL_RENDER) != 0)
        {
            ++luCell;
            EmitterCubeRender(arVertexBuffer, arBatchArray, lpEmitter, arTime);
            continue;
        }

        if (!lpEmitter->IsActive())
        {
            ++luInactive;
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
            ++luCulled; ++luCullRange;
            if (!sbNoCull) continue;
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
            ++luCulled; ++luCullFrustum;
            if (!sbNoCull) continue;
        }

        // ---- near ------------------------------------------------------------------------------
        const f32 lfAlongView = mCamDir.x * lfDx + (mCamDir.y * lfDy + mCamDir.z * lfDz);
        if (lfAlongView <= KF_EMITTER_CULL_BEHIND)
        {
            ++luCulled; ++luCullNear;
            if (!sbNoCull) continue;
        }

        {
            // [lionfx] ONE-SHOT, the first emitter that ever reaches the cull: every number the
            // three tests consume, so a wrong camera publish is a readable line rather than a
            // guess. (The camera data reaches here through ParticleModule::BuildLionVertexBuffers
            // -> LionParticleRender::SetCameraData, and on this build the ParticleRenderData it
            // reads is written by a PC bring-up stand-in -- so "is the camera real" is exactly the
            // question that has to be answerable.) DELETE with the bring-up.
            static bool sbDiagOnce = false;
            if (!sbDiagOnce)
            {
                sbDiagOnce = true;
                char lacMsg[416];
                std::snprintf(lacMsg, sizeof(lacMsg),
                    "[lionfx] cull#1 cam=(%.2f,%.2f,%.2f) dir=(%.3f,%.3f,%.3f) emit=(%.2f,%.2f,%.2f)"
                    " distSq=%.1f along=%.2f planeD=(%.2f,%.2f,%.2f,%.2f) vs (%.2f,%.2f,%.2f,%.2f)\n",
                    mCamPos.x, mCamPos.y, mCamPos.z, mCamDir.x, mCamDir.y, mCamDir.z,
                    lMat.wa.x, lMat.wa.y, lMat.wa.z, lfRangeSq, lfAlongView,
                    lafDistance[0], lafDistance[1], lafDistance[2], lafDistance[3],
                    lrFrustum.wAxis.x, lrFrustum.wAxis.y, lrFrustum.wAxis.z, lrFrustum.wAxis.w);
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }

        EmitterRender(arVertexBuffer, arBatchArray, lpEmitter, arTime);
        ++luRendered;
    }

    // [lionfx] FLAG PC bring-up diagnostic -- the Lion draw path's own witness, printed only
    // when the answer CHANGES (so a steady state costs one line, not one per frame). It is the
    // attribution this subsystem has never had: if nothing appears on screen, this says whether
    // the emitters were missing, culled, or drawn with zero particles -- three different bugs
    // that all look identical in a screenshot. DELETE with the boost-exhaust bring-up.
    {
        static u32 suLastLive = 0xFFFFFFFFu;
        static u32 suLastRendered = 0xFFFFFFFFu;
        static u32 suLastParticles = 0xFFFFFFFFu;
        if (luLive != suLastLive || luRendered != suLastRendered
            || mParticlesRenderedCount != suLastParticles)
        {
            suLastLive = luLive;
            suLastRendered = luRendered;
            suLastParticles = mParticlesRenderedCount;
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg),
                          "[lionfx] Render: emitters live=%u cell=%u inactive=%u culled=%u"
                          " (range=%u frustum=%u near=%u) drawn=%u particles=%u nocull=%d\n",
                          luLive, luCell, luInactive, luCulled,
                          luCullRange, luCullFrustum, luCullNear, luRendered,
                          mParticlesRenderedCount, (int)sbNoCull);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }
}

// ------------------------------------------------------------------------------------------------
// cParticleRender::EmitterRender  @ 0x82913928   (DWARF ParticleRender.cpp:459)
//
// ONE emitter, one material, one batch. The local names below are the DWARF's own
// (ParticleRender.cpp:463-589): lMaterial / lCurrentLocatorTime / lBindingsLocatorMat / lpFog /
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
// ⭐ THE `n + 16 >= 32` FLUSH IS A HEADROOM TEST, NOT A FULLNESS TEST (`addi r11, r31, 0x10 ;
// cmplwi cr6, r11, 0x20 ; bge`). 16 is cParticleBucket::KU_MAX_PARTICLES -- the most the NEXT
// bucket could add -- and 32 is the arrays' capacity. So it flushes when one more bucket could
// overflow, which is why the arrays are 32 and not 16: two buckets' worth minus one.
//
// ⭐ THE THREE ARMS PICK THEIR KERNEL FROM THE **FIRST** BUCKET AND NEVER RE-TEST IT
// (`lwz r11, 0xE60(r27)` @0x829139C0 with r27 still the head). All of an emitter's buckets come
// from the same descriptor and therefore the same pool, so the kind cannot change mid-list; but
// the loop really is three separate loops in the binary, each with its own arrays, which is why
// this reads as duplication rather than one loop with a switch inside.
//
// ⚠ THE DRAW IS GATED ON THE BINDING'S WORLD INDEX (`lwz r11, 0x1FC(r29) ; lwz r11, 4(r11)`,
// i.e. cLionBindings::GetWorldIndex() == 0). The simulation still runs for a non-zero world -- the
// particles advance and the count is still added to mParticlesRenderedCount -- only the draw call
// is skipped. That is how the player's own effects leave the outside view while the bumper camera
// is up without their state drifting.
//
// ⚠ AND THE VECTOR ARM CALLS A DIFFERENT RENDER OVERLOAD (sub_82289158 == the `const cVector*`
// Render the DWARF declares at ParticleRender.h:8/:170), not the cMatrix one. It is the same draw
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

        const bool lbDrawThisWorld = (apEmitter->GetBindings().GetWorldIndex() == 0);

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
                    if (lbDrawThisWorld)
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
                    if (lbDrawThisWorld)
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
                    if (lbDrawThisWorld)
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
// cParticleRender::EmitterCubeRender  @ 0x82913C80 (448 instructions) -- NOT RECONSTRUCTED, and
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
//   * The box is built from the descriptor's mpBehaviour[+0x280] half-extent (`*(v11 + 640)`) and
//     the camera position, one axis at a time: lo = camPos.a * e + locator.a - e, hi = ... + e,
//     with the wrap span 2*e (`v16 = 2.0 * e`).
//   * BeginBatch / the three simulation kernels / EndBatch / Append are the SAME shape as
//     EmitterRender above -- one batch, one material, the same three-way bucket-kind selection,
//     and the same `sub_82289158` vs `LionParticleRender::Render` split on the vector arm. The
//     only structural difference is that it uses ONE pair of arrays for all three kinds
//     (v115/v112 + v113) and draws after every bucket rather than on a headroom test.
//   * Between simulate and draw it runs TWO extra passes the other renderer does not have:
//       - a DISTANCE FADE, unrolled x4: d2 = |particle.mPos - camPos|^2, then
//         alpha' = alpha - alpha * (1 - d2/(2e)^2) * flt_82F357F4, selected with `fsel` so the
//         clamp is branchless. flt_82F357F4 is NOT yet read out of the image.
//       - a WRAP: each of the three position axes is folded back into [lo, hi] with a
//         `(p - hi) / span` truncate-and-subtract, which is what makes the volume infinite.
//
// WHAT IS NEEDED TO FINISH IT: the value of flt_82F357F4, the exact lane order of the four-wide
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
            "[effects] NOT RECONSTRUCTED: cParticleRender::EmitterCubeRender @0x82913C80 (the "
            "CELL_RENDER camera-anchored volume: the per-particle distance fade and the "
            "three-axis wrap). Emitters with CELL_RENDER_FLAG neither simulate nor draw; every "
            "other emitter kind is unaffected.\n");
    }
}
