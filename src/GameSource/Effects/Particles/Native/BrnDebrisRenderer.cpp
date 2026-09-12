#include "GameSource/Effects/Particles/Native/BrnDebrisRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"     // BrnDebrisArrayParams table

#include "GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.h" // the borrowed renderer
#include "GameSource/Effects/BrnEffectsUtils.h"                      // BrnEffects::Utils::SinCosCycles
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"          // [diag] CgsDev::Log::WriteToLog
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h" // shadow::Device
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h" // renderengine::BlendState*
#include "GameShared/GameClasses/Graphics/CgsResourceAllocatorCreate.h"
#include "pc/gcm/renderengine/renderstates.h"                       // renderengine::MeshHelper
#include "pc/gcm/renderengine/device.h"                             // renderengine::Device (the Dispatch<> tag)
#include "pc/gcm/renderengine/ShadowPassPCLeaf.h"                   // LionParticleSampler_ApplyState

#include <cstdio>   // [diag] snprintf (the first-draw witness)

// The two shared render states the debris pass binds (ImmediateModePCLeaf.cpp), declared
// `extern void*` exactly as BrnSparkRenderer_Render.cpp / BrnTrailRender.cpp declare theirs.
extern void* gpImDebrisRasterizerState;      // dword_83010F44  cull BACK
extern void* gpImDebrisDepthStencilState;    // dword_83010F48  Z test + Z write, LESSEQUAL
void ImDeviceSetDepthStencilState(void* lpState);
void ImDeviceSetRasterizerState(void* lpState);

// BrnParticle::Native::BrnDebrisRenderer::Construct @ 0x82281DA8
//
// Reconstructed store-for-store from the X360 ARTIST asm. The original built a
// renderengine::BlendState::Parameters via the setter API; the compiler inlined every
// setter into the raw parameter-word stores reproduced below. The parameter block is then
// turned into a resource descriptor, the backing resource is allocated through the passed
// rw::IResourceAllocator (virtual Create at vtable +0x10, matching
// CgsGraphics::ImRendererBase::ConstructBlendState @ 0x827ED118), and the blend state is
// Initialize()d in place. mpRenderer caches the borrowed Im3d renderer.
//
// Blend-state parameters (from the inlined v8[]/byte stores):
//   maBlendFactor = { 0x07060701, 0x07060706, 0x07060706, 0x07060706 }
//   muState15=4  muState4..7=15  muState8=0x87  muState17=1  muState9=0xFFFFFFFF
//   mbHasCustomBlendFactors=1  mbState10..14=0  mbState16=1

namespace
{
    // The rw resource allocator's Create slot; vtable offset +0x10 on X360 (same shape the
    // immediate-mode blend-state builders use). Declared file-locally so the virtual
    // dispatch compiles without pulling the full rwcore allocator surface.
    class ResourceAllocator
    {
    public:
        // NOT a vtable slot. Declaring this `virtual` put it at slot 0, which on the
        // rw::IResourceAllocator actually behind the reinterpret_cast is the VIRTUAL
        // DESTRUCTOR -- so the call allocated nothing and left the allocator's vptr
        // downgraded to the inert base for the rest of the run. Call the interface by
        // NAME instead; see CgsResourceAllocatorCreate.h.
        void* Create(
            void*                   lpResourceOut,
            ResourceAllocator*      /*lpAllocator*/,
            const void*             lpDescriptor,
            int                     /*liFlags*/)
        {
            return CgsGraphics::ResourceAllocatorCreate(this, lpResourceOut, lpDescriptor);
        }
    };
}

namespace BrnParticle
{
namespace Native
{
    // =====================================================================================
    // _gaDebrisArrayParams -- the five debris parameter presets, indexed by EDebrisArrayID.
    // This file is the table's home (both BrnDebrisArray::Construct asserts cite it), and
    // BrnDebrisArray::Construct binds mpParams to one of these entries.
    //
    // RECOVERED FROM THE CONSOLE IMAGE, not authored. The scalar half is plain initialised
    // data read straight out of the image; the two vector members (mColour, mvBounciness)
    // are SILENT ZEROES there -- Vector4/Vector3 are non-trivial, so the compiler emitted a
    // CRT dynamic initialiser that writes them at startup. That initialiser was located and
    // disassembled, and it is where the colours and the bounce vectors below come from:
    //
    //   * the shared bounce base is one namespace-scope const Vector3 (0.7, 0.6, 0.7). The
    //     initialiser stores it UNSCALED into entry 0 and stores base * s into entries 1..4,
    //     with s = 0.9 / 0.8 / 0.5 / 0.8 -- four broadcast-then-multiply pairs against a
    //     scalar. Written below as that multiply, not as its product, because the multiply
    //     is what the original source said.
    //   * every colour is white except the Dark preset, which is (0.5, 0.5, 0.5, 1.0).
    //
    // Corroboration that the offsets are right, independent of the initialiser: the entry
    // stride is 80 bytes (the value the array index is scaled by), the two vector stores land
    // at entry +0x20 and entry +0x30, and the particle counts below are exactly what
    // GetNewDebris compares its 96-per-bucket budget against.
    // =====================================================================================
    namespace
    {
        // The bounce-damping base every preset scales. Per-axis: the debris keeps 70% of its
        // horizontal speed and 60% of its vertical speed across a bounce.
        constexpr rw::math::vpu::Vector3 KV_DEBRIS_BOUNCINESS = { 0.7f, 0.6f, 0.7f, 0.0f };

        constexpr rw::math::vpu::Vector3 ScaledBounciness(f32 lfScale)
        {
            return rw::math::vpu::Vector3{ KV_DEBRIS_BOUNCINESS.x * lfScale,
                                           KV_DEBRIS_BOUNCINESS.y * lfScale,
                                           KV_DEBRIS_BOUNCINESS.z * lfScale,
                                           KV_DEBRIS_BOUNCINESS.w * lfScale };
        }

        constexpr rw::math::vpu::Vector4 KV_DEBRIS_WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
    }

    extern const BrnDebrisArrayParams _gaDebrisArrayParams[eDebrisArray_Max] =
    {
        // eDebrisArray_Coloured
        { "lowres_debris.rf3",     400, 10.0f,  0.2f,  1.0f, 0.2f,
          KV_DEBRIS_WHITE,                  KV_DEBRIS_BOUNCINESS,        0.1f,   0.8f },
        // eDebrisArray_Shiny
        { "lowres_debris.rf3",     300, 10.0f,  0.2f,  0.7f, 0.3f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.9f ),    0.1f,   0.8f },
        // eDebrisArray_Dark
        { "lowres_debris.rf3",     400,  1.0f,  0.01f, 0.5f, 0.2f,
          { 0.5f, 0.5f, 0.5f, 1.0f },       ScaledBounciness( 0.8f ),    0.1f,   0.8f },
        // eDebrisArray_HighDetail
        { "highres_debris_02.rf3", 100, 10.0f,  0.2f,  1.0f, 0.2f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.5f ),    0.1f,   0.8f },
        // eDebrisArray_Glass
        { "Glass_debris.rf3",      400, 50.0f, 50.0f,  1.0f, 0.6f,
          KV_DEBRIS_WHITE,                  ScaledBounciness( 0.8f ),    0.001f, 0.8f },
    };

    void BrnDebrisRenderer::Construct(rw::IResourceAllocator* lpAllocator,
                                      BrnGraphics::Im3dTexPlusLighting* lpRenderer)
    {
        renderengine::BlendStateParameters lParameters;
        lParameters.maBlendFactor[0] = 0x07060701u;
        lParameters.maBlendFactor[1] = 0x07060706u;
        lParameters.maBlendFactor[2] = 0x07060706u;
        lParameters.maBlendFactor[3] = 0x07060706u;
        lParameters.muState15 = 4u;
        lParameters.muState4  = 15u;
        lParameters.muState5  = 15u;
        lParameters.muState6  = 15u;
        lParameters.muState7  = 15u;
        lParameters.muState8  = 135u;        // 0x87
        lParameters.muState17 = 1u;
        lParameters.muState9  = 0xFFFFFFFFu;  // -1
        lParameters.mbHasCustomBlendFactors = 1u;
        lParameters.mbState10 = 0u;
        lParameters.mbState11 = 0u;
        lParameters.mbState12 = 0u;
        lParameters.mbState13 = 0u;
        lParameters.mbState14 = 0u;
        lParameters.mbState16 = 1u;

        // Cache the borrowed Im3d renderer (this[0]).
        mpRenderer = lpRenderer;

        // Build the resource descriptor for the parameter block (X360: 48-byte scratch).
        void* lpDescriptor[12] = {};
        renderengine::BlendState::GetResourceDescriptor(lpDescriptor, &lParameters);

        // Allocate the backing resource through the allocator's Create slot (vtable +0x10).
        // X360: (*(*a2 + 16))(v6, a2, v7, 0) -- v6 is a 32-byte state-handle scratch.
        renderengine::BlendMaterialState* lapStateHandles[8] = {};
        ResourceAllocator* lpAllocatorIf = reinterpret_cast<ResourceAllocator*>(lpAllocator);
        lpAllocatorIf->Create(lapStateHandles, lpAllocatorIf, lpDescriptor, 0);

        // Initialize the blend state in place and latch it (this[1]).
        mpBlendState = reinterpret_cast<renderengine::BlendState*>(
            renderengine::BlendState::Initialize(lapStateHandles, &lParameters) );

        CGS_ASSERT( mpBlendState, "mpBlendState" );
    }

    // =====================================================================================
    // BrnParticle::Native::BrnDebrisRenderer::BeginRender
    //
    // Open the debris pass. Instruction for instruction the original does:
    //   1. mpRenderer->BeginRendering()          -- bind the world-textured program pair and
    //                                               this renderer's vertex declaration.
    //   2. SetState(mpBlendState)                -- the alpha-tested blend Construct built.
    //   3. the rasteriser out of the shared state library (cull BACK).
    //   4. the depth-stencil out of the same library (Z test on, Z WRITE ON, LESSEQUAL) --
    //      debris is solid geometry, unlike the sparks and the sky.
    //   5. the library's linear/clamp sampler on unit 0, through the shadow cache.
    //   6. four constant pushes, in this order: the view-projection, the eye, the NEGATED
    //      light direction, the light colour.
    //
    // THE NEGATION IS A STORE, NOT A CHOICE: the original exclusive-ORs the incoming light
    // direction with a broadcast sign mask before writing the row, so what reaches
    // gLightDirection is -direction. The pixel program uses that vector as the "towards the
    // light" L of its N.L, so a sign lost here lights every piece of debris from behind. It is
    // spelled at the call site rather than inside SetLightDirection because every one of the
    // six setters is otherwise a plain store, and nothing names which side of the inline
    // boundary the negation sat on.
    //
    // THE AMBIENT COLOUR IS PASSED AND NOT READ. The caller loads renderData->mvAmbientColour
    // into the third vector-argument register for this call and the body never touches it; the
    // ambient term the pixel program applies comes from the per-ARRAY gShinyParams.y instead
    // (RenderDebrisArray). Kept in the signature so the call site stays the original's.
    // =====================================================================================
    void BrnDebrisRenderer::BeginRender(Matrix44::InParam lViewProjection,
                                        Vector3           lLightDirection,
                                        Vector3           lLightColour,
                                        Vector3           /*lAmbientColour*/,
                                        Vector3           lEye)
    {
        if (mpRenderer == 0)
        {
            // A null renderer is a crash, not a missing effect: BeginRendering binds the
            // program pair. Same guard, same reason, as SparkRenderer::Dispatch's.
            static bool sbLogged = false;
            if (!sbLogged)
            {
                sbLogged = true;
                CgsDev::Log::WriteToLog("[debrispass] BeginRender SKIPPED: mpRenderer is null\n");
            }
            return;
        }

        mpRenderer->BeginRendering();
        mpRenderer->SetState(reinterpret_cast<const CgsGraphics::BlendState*>(mpBlendState));
        ImDeviceSetRasterizerState(gpImDebrisRasterizerState);       // dword_83010F44
        ImDeviceSetDepthStencilState(gpImDebrisDepthStencilState);   // dword_83010F48
        renderengine::LionParticleSampler_ApplyState(0);             // dword_83010F60 on unit 0

        mpRenderer->SetViewProjection(lViewProjection);
        mpRenderer->SetEye(lEye);

        Vector3 lNegatedLightDirection;
        lNegatedLightDirection.x = -lLightDirection.x;
        lNegatedLightDirection.y = -lLightDirection.y;
        lNegatedLightDirection.z = -lLightDirection.z;
        lNegatedLightDirection.w = -lLightDirection.w;
        mpRenderer->SetLightDirection(lNegatedLightDirection);

        mpRenderer->SetLightColour(lLightColour);
    }

    // =====================================================================================
    // BrnParticle::Native::BrnDebrisRenderer::RenderDebrisArray
    //
    // One debris array, start to finish. The original is a hand-vectorised loop that works
    // four particles at a time in structure-of-arrays lanes; it is reproduced here as the
    // per-particle scalar loop it is the four-wide form of. Every constant, every store and
    // every comparison below is recovered -- nothing is fitted.
    //
    // SHAPE:
    //   assert(lpArray);  bail when the array has no live buckets
    //   assert(lpParams); push gShinyParams = (0, ambient, specularPower, specularIntensity)
    //   bind the array's texture on unit 0 and dispatch its mesh; flush the vertex-program state
    //   for each live bucket, for each group of KU_NUM_TRANSFORMS(32) particles:
    //       build one world matrix per particle into the batch
    //       if any particle in the group was visible: stage the batch and issue ONE indexed draw
    //
    // THE BATCH IS ALWAYS 32 MATRICES WIDE, even at the end of a short bucket. The original
    // runs its inner block a fixed eight times (eight x four lanes) and lets the per-lane
    // validity test blank the overrun, rather than trimming the draw -- the instance count is
    // baked into the mesh, so a short batch would still draw 32 copies. The overrun slots read
    // particle records that are inside the bucket and are made degenerate by a zero scale.
    //
    // THE PER-INSTANCE COLOUR RIDES THE MATRIX'S FOURTH COLUMN. Rows 0..2 carry the scaled
    // rotation in xyz and the diffuse R / G / B in w; row 3 carries the position in xyz and the
    // faded alpha in w. The vertex program exports exactly {m0.w, m1.w, m2.w, m3.w} as the
    // vertex colour, which is why the rotation rows can be three-wide and still fill a float4x4.
    //
    // THE FOUR LIFETIMES ARE NOT ONE NUMBER. The lifetime the age is tested against is a
    // four-lane constant, {7, 9, 8, 10} seconds, indexed by the particle's own position in its
    // lane group -- so a burst of debris does not evaporate all at once. (The 9 and the 8 are
    // stored as 9.00000095 and 7.99999905; they are reproduced as stored.) With the caller's
    // reduced-frame-rate flag clear the whole vector is scaled by 0.2.
    //
    // THAT SCALE IS CORROBORATED BY THE OTHER END OF THE SYSTEM, independently of this body:
    // BrnDebrisArray::FreeExpiredBuckets recycles a bucket once its last particle is older than
    // TEN seconds at 30 Hz and TWO seconds otherwise. Ten is this vector's largest lane, and two
    // is that lane times 0.2 -- so the producer's recycle window and the renderer's fade-out
    // window are the same window, read from two unrelated functions. A misread lifetime or a
    // misread scale would have made them disagree.
    // =====================================================================================
    namespace
    {
        // The per-lane debris lifetimes in seconds, indexed by (particle index & 3).
        const f32 KAF_DEBRIS_LIFETIME[4] = { 7.0f, 9.00000095f, 7.99999905f, 10.0f };

        // The lifetime scale applied when the caller's reduced-frame-rate flag is CLEAR.
        const f32 KF_DEBRIS_SHORT_LIFETIME_SCALE = 0.2f;

        // Fade-out rate: the last 1/1.25 == 0.8 s of a particle's life fade it out.
        const f32 KF_DEBRIS_FADE_OUT_RATE = 1.25f;

        // The half-offset the per-lane particle-index ramp carries ({0.5, 1.5, 2.5, 3.5} + 4n),
        // compared against the bucket's particle count to blank the tail of the last group.
        const f32 KF_DEBRIS_INDEX_BIAS = 0.5f;

        // The rotation axis a blanked lane falls back to (+Y), with a zero angle.
        const f32 KAF_DEBRIS_DEFAULT_AXIS[3] = { 0.0f, 1.0f, 0.0f };

        // The primitive topology the draw carries (TRIANGLELIST).
        const u32 KU_DEBRIS_PRIMITIVE_TYPE = 4u;

        // The serialised BrnVFXMeshCollection is reached by dword offset -- the same
        // external-serialised-data exception, and the same two indices, that
        // BrnVFXMeshCollectionResourceType.cpp's FixUp uses.
        enum EVFXMeshCollectionDword
        {
            E_MESHCOLLECTION_MESHHELPER  = 33,   // 0x84 -- MeshHelper* (rebased at FixUp)
            E_MESHCOLLECTION_NUM_INDICES = 34    // 0x88 -- muNumIndices
        };

        inline f32 MinF(f32 lfA, f32 lfB) { return (lfA < lfB) ? lfA : lfB; }
    }

    void BrnDebrisRenderer::RenderDebrisArray(f32                   lfCurrentTime,
                                              const BrnDebrisArray* lpArray,
                                              EDebrisArrayID        /*leArrayId*/,
                                              bool                  lbFullLifetime)
    {
        CGS_ASSERT(lpArray != 0, "lpArray != NULL");
        if (lpArray == 0 || mpRenderer == 0)
        {
            return;
        }

        // No live buckets -> nothing to draw. This test comes BEFORE the parameter assert.
        if (lpArray->Buckets() == 0)
        {
            return;
        }

        const BrnDebrisArrayParams* const lpParams = lpArray->Params();
        CGS_ASSERT(lpParams != 0, "lpParams != NULL");
        if (lpParams == 0)
        {
            return;
        }

        // gShinyParams = (0, ambient, specular power, specular intensity). Lane 0 is written as a
        // literal zero and is read by neither program; lanes 1..3 are the array's own parameters,
        // and they land exactly where the pixel program reads its ambient add, its specular
        // exponent and its specular scale.
        mpRenderer->SetShinyParams(0.0f,
                                   lpParams->mfAmbientFactor,
                                   lpParams->mfSpecularPower,
                                   lpParams->mfSpecularIntensity);

        renderengine::Texture* const lpTexture = lpArray->Texture();
        mpRenderer->SetTexture(lpTexture);

        // The mesh collection carries the instanced debris mesh and its index count.
        const BrnVFXMeshCollection* const lpCollection = lpArray->MeshCollection();
        if (lpCollection == 0)
        {
            // The debris mesh resource is not bound on this build, so there is no geometry to
            // instance. Say so once rather than issuing a draw with no streams.
            static bool sbLoggedNoMesh = false;
            if (!sbLoggedNoMesh)
            {
                sbLoggedNoMesh = true;
                CgsDev::Log::WriteToLog(
                    "[debrispass] array SKIPPED: mMeshCollection is null (FX bundle not bound)\n");
            }
            return;
        }

        const u32* const lpauCollection = reinterpret_cast<const u32*>(lpCollection);
        const renderengine::MeshHelper::MeshData* const lpMeshData =
            reinterpret_cast<const renderengine::MeshHelper::MeshData*>(
                static_cast<uintptr_t>(lpauCollection[E_MESHCOLLECTION_MESHHELPER]));
        const u32 luNumIndices = lpauCollection[E_MESHCOLLECTION_NUM_INDICES];

        if (lpMeshData == 0)
        {
            return;
        }

        // Bind the mesh's index buffer and vertex streams. The original guards this with the
        // device's own bound-mesh shadow; the bind is issued unconditionally here instead,
        // because BeginRender's BeginRendering has just reset that shadow and nothing between
        // there and here binds a stream -- so the guard can only ever miss. Re-binding the same
        // buffers is what the guarded path does on a miss anyway, and keeping a private copy of
        // the device's shadow word is the defect the immediate-mode renderers were corrected for.
        renderengine::MeshHelper::Dispatch<renderengine::Device>(lpMeshData);
        shadow::Device::FlushVertexProgramState();

        shadow::Device::DrawIndexedParameters lDrawParameters;
        lDrawParameters.mePrimitiveType   = KU_DEBRIS_PRIMITIVE_TYPE;
        lDrawParameters.muBaseVertexIndex = 0u;
        lDrawParameters.muMinVertexIndex  = 0u;
        lDrawParameters.muNumVertices     = luNumIndices;

        // 1 / fade-in time, formed once per array (the original divides a literal 1.0 by the
        // parameter and broadcasts the quotient).
        const f32 lfInverseFadeInTime = 1.0f / lpParams->mfFadeInTime;

        Matrix44 laTransforms[BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS];

        u32 luDrawnBatches   = 0;
        u32 luDrawnInstances = 0;

        for (const BrnDebrisArray::DebrisBucket* lpBucket = lpArray->Buckets();
             lpBucket != 0;
             lpBucket = static_cast<const BrnDebrisArray::DebrisBucket*>(lpBucket->mpNextBucket))
        {
            CGS_ASSERT((reinterpret_cast<uintptr_t>(lpBucket) & 0xFu) == 0,
                       "(((uint32_t)lpCurrentBucket) & 0xf) == 0");
            CGS_ASSERT(lpBucket->mu16NumberOfParticlesInBucket
                           <= BrnDebrisArray::DebrisBucket::KuMaxNumParticles,
                       "mu16NumberOfParticlesInBucket <= KuMaxNumParticles");

            const u32 luNumParticles = lpBucket->mu16NumberOfParticlesInBucket;
            const f32 lfNumParticles = static_cast<f32>(luNumParticles);
            CGS_ASSERT(luNumParticles != 0, "luNumParticles != 0");

            u32 luParticle = 0;
            do
            {
                bool lbAnyVisible = false;

                for (u32 luSlot = 0;
                     luSlot < BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS;
                     ++luSlot)
                {
                    const u32 luIndex = luParticle + luSlot;
                    CGS_ASSERT(luIndex < BrnDebrisArray::DebrisBucket::KuMaxNumParticles,
                               "luParticle < KuMaxNumParticles");

                    const BrnDebris& lrParticle = lpBucket->maParticleData[luIndex];

                    // --- the three visibility tests, in the original's order ------------------
                    const f32 lfAge = lfCurrentTime - lpBucket->maParticleBirthTimes[luIndex];

                    f32 lfLifetime = KAF_DEBRIS_LIFETIME[luIndex & 3u];
                    if (!lbFullLifetime)
                    {
                        lfLifetime *= KF_DEBRIS_SHORT_LIFETIME_SCALE;
                    }

                    // The index ramp carries a half-lane bias, so the test is index + 0.5 vs the
                    // count -- i.e. "this slot is past the end of the bucket".
                    const bool lbInRange = !((static_cast<f32>(luIndex) + KF_DEBRIS_INDEX_BIAS)
                                                 >= lfNumParticles);
                    // NaN polarity follows the original's compares: an unordered age is NOT born
                    // and IS expired.
                    const bool lbBorn    = (lfAge >= 0.0f);
                    const bool lbExpired = (lfAge >= lfLifetime);
                    const bool lbVisible = lbInRange && lbBorn && !lbExpired;
                    lbAnyVisible = lbAnyVisible || lbVisible;

                    // --- fade in and fade out, both clamped at 1 ------------------------------
                    const f32 lfFadeIn  = MinF(1.0f, lfAge * lfInverseFadeInTime);
                    const f32 lfFadeOut = MinF(1.0f, (lfLifetime - lfAge) * KF_DEBRIS_FADE_OUT_RATE);

                    // --- axis + angle, blanked to (+Y, 0) when the slot is not visible --------
                    f32 lfAxisX = KAF_DEBRIS_DEFAULT_AXIS[0];
                    f32 lfAxisY = KAF_DEBRIS_DEFAULT_AXIS[1];
                    f32 lfAxisZ = KAF_DEBRIS_DEFAULT_AXIS[2];
                    f32 lfAngle = 0.0f;
                    f32 lfScale = 0.0f;
                    if (lbVisible)
                    {
                        lfAxisX = lrParticle.mAxisPlusAngle.x;
                        lfAxisY = lrParticle.mAxisPlusAngle.y;
                        lfAxisZ = lrParticle.mAxisPlusAngle.z;
                        lfAngle = lrParticle.mAxisPlusAngle.w;
                        lfScale = lrParticle.mVelocityPlusScale.w;
                    }
                    lfScale *= lfFadeIn;

                    // --- the rotation about that axis by that angle --------------------------
                    // The original evaluates the same folded minimax sin/cos the effects utils
                    // own, then assembles the rows as axis * (axis.<lane> * (1 - cos)) plus the
                    // cross-product triple -- the textbook axis-angle matrix for row vectors.
                    f32 lfSin = 0.0f;
                    f32 lfCos = 0.0f;
                    BrnEffects::Utils::SinCosCycles(lfAngle, lfSin, lfCos);
                    const f32 lfOneMinusCos = 1.0f - lfCos;

                    const f32 lfXK = lfAxisX * lfOneMinusCos;
                    const f32 lfYK = lfAxisY * lfOneMinusCos;
                    const f32 lfZK = lfAxisZ * lfOneMinusCos;
                    const f32 lfXS = lfAxisX * lfSin;
                    const f32 lfYS = lfAxisY * lfSin;
                    const f32 lfZS = lfAxisZ * lfSin;

                    Matrix44& lrTransform = laTransforms[luSlot];

                    lrTransform.xAxis.x = (lfAxisX * lfXK + lfCos) * lfScale;
                    lrTransform.xAxis.y = (lfAxisY * lfXK + lfZS)  * lfScale;
                    lrTransform.xAxis.z = (lfAxisZ * lfXK - lfYS)  * lfScale;
                    lrTransform.xAxis.w = lrParticle.mDiffuseColour.x;

                    lrTransform.yAxis.x = (lfAxisX * lfYK - lfZS)  * lfScale;
                    lrTransform.yAxis.y = (lfAxisY * lfYK + lfCos) * lfScale;
                    lrTransform.yAxis.z = (lfAxisZ * lfYK + lfXS)  * lfScale;
                    lrTransform.yAxis.w = lrParticle.mDiffuseColour.y;

                    lrTransform.zAxis.x = (lfAxisX * lfZK + lfYS)  * lfScale;
                    lrTransform.zAxis.y = (lfAxisY * lfZK - lfXS)  * lfScale;
                    lrTransform.zAxis.z = (lfAxisZ * lfZK + lfCos) * lfScale;
                    lrTransform.zAxis.w = lrParticle.mDiffuseColour.z;

                    // Row 3 is the position VERBATIM (it is not scaled), with the alpha faded.
                    lrTransform.wAxis.x = lrParticle.mPositionPlusRotVel.x;
                    lrTransform.wAxis.y = lrParticle.mPositionPlusRotVel.y;
                    lrTransform.wAxis.z = lrParticle.mPositionPlusRotVel.z;
                    lrTransform.wAxis.w = lrParticle.mDiffuseColour.w * lfFadeOut;
                }

                // The original reduces the accumulated lane masks with the compiler's all-lanes
                // idiom and draws when ANY lane of ANY of the eight sub-groups was visible --
                // one draw per 32 particles, skipped when the whole batch is dead.
                if (lbAnyVisible)
                {
                    mpRenderer->SetTransformArray(laTransforms);
                    shadow::Device::DrawIndexedMultipleStreams_Custom(lDrawParameters);
                    ++luDrawnBatches;
                    luDrawnInstances += BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS;
                }

                luParticle += BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS;
            }
            while (luParticle < luNumParticles);
        }

        // [DIAG] NOT IN THE ORIGINAL. THE FIRST-DRAW WITNESS, once per run.
        // Every other measurement this pass offers stays perfect when the draw produces no
        // pixels: the constants resolve, the mesh dispatches, the draw returns S_OK. This is the
        // one line that says the pass reached a draw at all, and with how much in it.
        // DELETE-WHEN-STABLE.
        if (luDrawnBatches != 0)
        {
            static bool sbLoggedFirstDraw = false;
            if (!sbLoggedFirstDraw)
            {
                sbLoggedFirstDraw = true;
                char lacMsg[128];
                std::snprintf(lacMsg, sizeof(lacMsg),
                              "[debrispass] first draw: arrays=%u instances=%u\n",
                              static_cast<unsigned>(luDrawnBatches),
                              static_cast<unsigned>(luDrawnInstances));
                CgsDev::Log::WriteToLog(lacMsg);
            }
        }
    }

}
}
