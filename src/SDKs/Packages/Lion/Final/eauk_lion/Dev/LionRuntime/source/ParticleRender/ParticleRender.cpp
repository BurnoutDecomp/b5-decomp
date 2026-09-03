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
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"   // shadow::Device
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT

#include <cstddef>   // offsetof (the layout pins at the foot of this file)

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

// ------------------------------------------------------------------------------------------------
// cParticleRender::Render @0x829147F8 -- LOUD TRAP, not a body.
//
// Its blockers are recorded at the head of this file (the VMX128 frustum cull needs two
// un-recovered rodata tables; EmitterRender needs the cParticleBucket layout, whose placeholder
// cMatrix still collides with this header's Matrix44 typedef). It is DEFINED here, as a trap,
// because the Lion install path put cLionFX::Render on the link: cLionFX::Render forwards to it,
// and cLionFX::Render itself is called only from the parked LION half of
// ParticleModule::BuildLionVertexBuffers, which announces itself every run. If this trap ever
// fires, an arm was unparked without its callee.
// ------------------------------------------------------------------------------------------------
void cParticleRender::Render(EffectsVertexBufferLocked& /*arVertexBuffer*/,
                             LionBatchArray& /*arBatchArray*/,
                             cParticleEmitterManager& /*arEmitterManager*/,
                             const cTime& /*arTime*/)
{
    CGS_ASSERT(false, "cParticleRender::Render @0x829147F8 -- NOT RECONSTRUCTED (VMX128 frustum cull)");
}
