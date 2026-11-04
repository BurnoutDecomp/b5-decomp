#include "GameSource/Effects/Particles/Native/BrnDebrisRenderer.h"
#include "GameSource/Effects/Particles/Native/BrnDebrisArray.h"     // BrnDebrisArrayParams table

#include "GameSource/Effects/Particles/Native/BrnIm3dTexPlusLighting.h" // the borrowed renderer
#include "SharedClasses/Graphics/BrnVFXMeshCollectionResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h" // shadow::Device
#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h" // renderengine::BlendState*

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
        const rw::math::vpu::Vector3 KV_DEBRIS_BOUNCINESS(0.7f, 0.6f, 0.7f);

        rw::math::vpu::Vector3 ScaledBounciness(f32 lfScale)
        {
            return KV_DEBRIS_BOUNCINESS * lfScale;
        }

        const rw::math::vpu::Vector4 KV_DEBRIS_WHITE(1.0f, 1.0f, 1.0f, 1.0f);
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
          rw::math::vpu::Vector4(0.5f, 0.5f, 0.5f, 1.0f), ScaledBounciness( 0.8f ),    0.1f,   0.8f },
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

        const rw::ResourceDescriptor descriptor =
            renderengine::BlendState::GetResourceDescriptor(lParameters);
        const rw::Resource resource = lpAllocator->DoAllocate(descriptor, 0);
        mpBlendState = renderengine::BlendState::Initialize(resource, lParameters);

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
        mpRenderer->BeginRendering();
        mpRenderer->SetState(reinterpret_cast<const CgsGraphics::BlendState*>(mpBlendState));
        mpRenderer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpRasterizerState_CullCW);       // dword_83010F44
        mpRenderer->SetState(CgsGraphics::ImRendererBase::GetStateLibrary().mpDepthStencilState_ZBufferOn);   // dword_83010F48
        // ARTIST 82281F34..82281FA4 takes the sampler before asserting the active renderer.
        const CgsGraphics::SamplerState* const sampler =
            CgsGraphics::ImRendererBase::GetStateLibrary().mpSamplerState_Linear_MipNearest_ClampUV;
        CgsGraphics::ImRendererBase& renderer = *mpRenderer;
        if (CgsGraphics::ImRendererBase::mgpActiveRenderer != &renderer)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mgpActiveRenderer == this",
                "..\\..\\..\\GameShared\\GameClasses\\Graphics/ImmediateMode/CgsImRenderer.h", 839);
            CgsDev::Assert::EndAssert();
        }
        shadow::Device::SetState(sampler, 0);

        mpRenderer->SetViewProjection(lViewProjection);
        mpRenderer->SetEye(lEye);

        Vector3 lNegatedLightDirection = -lLightDirection;
        mpRenderer->SetLightDirection(lNegatedLightDirection);

        mpRenderer->SetLightColour(lLightColour);
    }

    namespace
    {
        const char kDebrisRendererSource[] =
            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Effects/Particles/Native/BrnDebrisRenderer.cpp";
        const char kBucketSource[] = "..\\..\\..\\GameSource\\Effects/Particles/Native/FXBuckets.h";

        CGS_STATIC_ASSERT((sizeof(BrnDebris) == 0x50), "ARTIST debris particle stride");
        CGS_STATIC_ASSERT((offsetof(BrnDebrisArray::DebrisBucket, maParticleBirthTimes) == 0x10),
            "ARTIST debris birth times");
        CGS_STATIC_ASSERT((offsetof(BrnDebrisArray::DebrisBucket, maParticleData) == 0x190),
            "ARTIST debris particle array");
        CGS_STATIC_ASSERT((BrnDebrisArray::DebrisBucket::KuMaxNumParticles == 96),
            "ARTIST debris bucket capacity");

        void DebrisAssert(bool condition, const char* expression, const char* file, s32 line)
        {
            if (!condition)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(expression, file, line);
                CgsDev::Assert::EndAssert();
            }
        }

        __vector4 SplatFloat(f32 value)
        {
            return __vspltw(__lvlx(&value, 0), 0);
        }

        // ARTIST 8228B5xx. Cos4_UnitCycles, shared by the cosine and
        // quarter-turn-shifted sine lanes of the same minimax polynomial.
        __vector4 DebrisCosine(__vector4 cycles)
        {
            const __vector4 fraction = __vsubfp(cycles, __vrfim(cycles));
            const __vector4 folded = __vaddfp(__vminfp(fraction, __vsubfp(SplatFloat(1.0f), fraction)),
                SplatFloat(-0.25f));
            const __vector4 squared = __vmulfp(folded, folded);
            const __vector4 inner =
                __vmaddfp(squared, SplatFloat(-71.17897033691406f), SplatFloat(40.897369384765625f));
            return __vmulfp(__vmaddfp(squared, inner, SplatFloat(-6.278042793273926f)), folded);
        }

        template<u32 Lane>
        void SetDebrisTransform(Matrix44& transform, const BrnDebris& particle, __vector4 axis,
            __vector4 sineLanes, __vector4 cosineLanes, __vector4 fadeInLanes, __vector4 fadeOutLanes,
            __vector4 visible)
        {
            const __vector4 sine = __vspltw(sineLanes, Lane);
            const __vector4 cosine = __vspltw(cosineLanes, Lane);
            const __vector4 fadeIn = __vspltw(fadeInLanes, Lane);
            const __vector4 fadeOut = __vspltw(fadeOutLanes, Lane);
            const __vector4 zero = __vspltisw(0);
            const __vector4 one = SplatFloat(1.0f);
            const __vector4 sign = __vslw(__vspltisw(-1), __vspltisw(-1));
            const __vector4 axisSine = __vmulfp(axis, sine);
            const __vector4 negativeSine = __vxor(axisSine, sign);
            const __vector4 axisComplement = __vmulfp(axis, __vsubfp(one, cosine));

            const Vector4 xTerm(cosine.x, axisSine.z, negativeSine.y, 0.0f);
            const Vector4 yTerm(negativeSine.z, cosine.x, axisSine.x, 0.0f);
            const Vector4 zTerm(axisSine.y, negativeSine.x, cosine.x, 0.0f);
            const __vector4 size = __vsel(zero, __vspltw(particle.mVelocityPlusScale.mV, 3), visible);
            const __vector4 scale = __vmulfp(fadeIn, size);
            const __vector4 color = particle.mDiffuseColour.mV;

            const __vector4 rowX = __vmaddfp(axis, __vspltw(axisComplement, 0), xTerm.mV);
            const __vector4 rowY = __vmaddfp(axis, __vspltw(axisComplement, 1), yTerm.mV);
            const __vector4 rowZ = __vmaddfp(axis, __vspltw(axisComplement, 2), zTerm.mV);
            transform.xAxis.mV = __vrlimi(__vmulfp(rowX, scale), __vspltw(color, 0), 1, 0);
            transform.yAxis.mV = __vrlimi(__vmulfp(rowY, scale), __vspltw(color, 1), 1, 0);
            transform.zAxis.mV = __vrlimi(__vmulfp(rowZ, scale), __vspltw(color, 2), 1, 0);
            const __vector4 alpha = __vmulfp(fadeOut, __vspltw(color, 3));
            transform.wAxis.mV = __vrlimi(particle.mPositionPlusRotVel.mV, alpha, 1, 0);
        }
    }

    // ARTIST 8228B078. The array ID is passed in r6 and unused.
    void BrnDebrisRenderer::RenderDebrisArray(f32 currentTime, const BrnDebrisArray* array,
        EDebrisArrayID, bool fullLifetime)
    {
        DebrisAssert(array != 0, "lpArray != NULL", kDebrisRendererSource, 524);
        if (!array->Buckets())
            return;

        const BrnDebrisArrayParams* const params = array->Params();
        DebrisAssert(params != 0, "lpParams != NULL", kDebrisRendererSource, 538);
        mpRenderer->SetShinyParams(0.0f, params->mfAmbientFactor, params->mfSpecularPower,
            params->mfSpecularIntensity);
        mpRenderer->SetTexture(const_cast<renderengine::Texture*>(array->Texture()), 0);

        const BrnVFXMeshCollection* const collection = array->MeshCollection();
        shadow::Device::SetState(collection->GetMeshHelper());
        shadow::Device::FlushVertexProgramState();
        const shadow::Device::DrawIndexedParameters draw = {
            D3DPT_TRIANGLELIST, 0, 0, collection->GetNumIndices()
        };

        const __vector4 time = SplatFloat(currentTime);
        const __vector4 inverseFadeIn = SplatFloat(1.0f / params->mfFadeInTime);
        const __vector4 fadeOutRate = SplatFloat(1.25f);
        const __vector4 radiansToCycles = SplatFloat(0.15915493667125702f);
        const __vector4 one = SplatFloat(1.0f);
        const __vector4 zero = __vspltisw(0);
        const Vector4 defaultAxis(0.0f, 1.0f, 0.0f, 0.0f);
        const Vector4 firstIndices(0.5f, 1.5f, 2.5f, 3.5f);
        const __vector4 step = __vcfsx(__vspltisw(4), 0);
        const Vector4 fullLifetimeLanes(7.0f, 9.000000953674316f, 7.999999046325684f, 10.0f);
        const __vector4 lifetime = fullLifetime ? fullLifetimeLanes.mV
            : __vmulfp(fullLifetimeLanes.mV, SplatFloat(0.2f));

        Matrix44 transforms[BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS];
        for (const BrnDebrisArray::DebrisBucket* bucket = array->Buckets(); bucket;
             bucket = static_cast<const BrnDebrisArray::DebrisBucket*>(bucket->mpNextBucket))
        {
            DebrisAssert((reinterpret_cast<uintptr_t>(bucket) & 15u) == 0,
                "(((uint32_t)lpCurrentBucket) & 0xf) == 0", kDebrisRendererSource, 587);
            const u32 count = bucket->mu16NumberOfParticlesInBucket;
            const u32 capacity = BrnDebrisArray::DebrisBucket::KuMaxNumParticles;
            DebrisAssert(count <= capacity,
                "mu16NumberOfParticlesInBucket <= KuMaxNumParticles", kBucketSource, 152);
            DebrisAssert((reinterpret_cast<uintptr_t>(bucket->maParticleBirthTimes) & 15u) == 0,
                "( ( (uint32_t)maParticleBirthTimes ) & 15 ) == 0", kBucketSource, 123);
            DebrisAssert(count != 0, "luNumParticles != 0", kDebrisRendererSource, 598);

            const __vector4 countLimit = SplatFloat(static_cast<f32>(count));
            __vector4 indices = firstIndices.mV;
            u32 first = 0;
            do
            {
                __vector4 batchVisible = zero;
                for (u32 slot = 0; slot < BrnGraphics::Im3dTexPlusLighting::KU_NUM_TRANSFORMS; slot += 4)
                {
                    const __vector4 born = __lvx(bucket->maParticleBirthTimes + first, 0);
                    const __vector4 age = __vsubfp(time, born);
                    const __vector4 remaining = __vsubfp(lifetime, age);
                    const __vector4 bornMask = __vcmpgefp(age, zero);
                    const __vector4 expired = __vcmpgefp(age, lifetime);
                    const __vector4 beyondBucket = __vcmpgefp(indices, countLimit);
                    const __vector4 visible = __vandc(bornMask, __vor(expired, beyondBucket));
                    batchVisible = __vor(batchVisible, visible);
                    const __vector4 fadeIn = __vminfp(one, __vmulfp(age, inverseFadeIn));
                    const __vector4 fadeOut = __vminfp(one, __vmulfp(remaining, fadeOutRate));

                    for (u32 lane = 0; lane < 4; ++lane)
                        DebrisAssert(first + lane < capacity, "luParticle < KuMaxNumParticles", kBucketSource, 133);

                    const BrnDebris* const particle = bucket->maParticleData + first;
                    const __vector4 mask0 = __vspltw(visible, 0);
                    const __vector4 mask1 = __vspltw(visible, 1);
                    const __vector4 mask2 = __vspltw(visible, 2);
                    const __vector4 mask3 = __vspltw(visible, 3);
                    const __vector4 axis0 = __vsel(defaultAxis.mV, particle[0].mAxisPlusAngle.mV, mask0);
                    const __vector4 axis1 = __vsel(defaultAxis.mV, particle[1].mAxisPlusAngle.mV, mask1);
                    const __vector4 axis2 = __vsel(defaultAxis.mV, particle[2].mAxisPlusAngle.mV, mask2);
                    const __vector4 axis3 = __vsel(defaultAxis.mV, particle[3].mAxisPlusAngle.mV, mask3);

                    __vector4 firstAngles;
                    firstAngles.u[0] = firstAngles.u[1] = 0x00010203u;
                    firstAngles.u[2] = 0x0C0D0E0Fu;
                    firstAngles.u[3] = 0x1C1D1E1Fu;
                    __vector4 secondAngles;
                    secondAngles.u[0] = 0x0C0D0E0Fu;
                    secondAngles.u[1] = 0x1C1D1E1Fu;
                    secondAngles.u[2] = secondAngles.u[3] = 0x00010203u;
                    const __vector4 angles = __vsldoi(__vperm(axis0, axis1, firstAngles),
                        __vperm(axis2, axis3, secondAngles), 8);
                    const __vector4 sine = DebrisCosine(__vmaddfp(angles, radiansToCycles, SplatFloat(-0.25f)));
                    const __vector4 cosine = DebrisCosine(__vmulfp(angles, radiansToCycles));
                    SetDebrisTransform<0>(transforms[slot], particle[0], axis0,
                        sine, cosine, fadeIn, fadeOut, mask0);
                    SetDebrisTransform<1>(transforms[slot + 1], particle[1], axis1,
                        sine, cosine, fadeIn, fadeOut, mask1);
                    SetDebrisTransform<2>(transforms[slot + 2], particle[2], axis2,
                        sine, cosine, fadeIn, fadeOut, mask2);
                    SetDebrisTransform<3>(transforms[slot + 3], particle[3], axis3,
                        sine, cosine, fadeIn, fadeOut, mask3);

                    first += 4;
                    indices = __vaddfp(indices, step);
                }

                // ARTIST 8327F110 packs each lane's high mask byte into a word.
                __vector4 permute;
                permute.u[0] = permute.u[1] = permute.u[2] = permute.u[3] = 0x0004080Cu;
                const __vector4 packed = __vperm(batchVisible, batchVisible, permute);
                const __vector4 empty = __vcmpequw(packed, zero);
                unsigned int comparison;
                __vcmpeqfpR(__vnor(empty, empty), zero, &comparison);
                if (!XMComparisonAllTrue(comparison))
                {
                    mpRenderer->SetTransformArray(transforms);
                    shadow::Device::DrawIndexedMultipleStreams_Custom(draw);
                }
            } while (first < count);
        }
    }
}
}
