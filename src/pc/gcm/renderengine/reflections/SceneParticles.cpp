#include "pc/gcm/renderengine/reflections/SceneRender.h"
#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/EnvironmentMap.h"
#include "pc/gcm/renderengine/shadows/SceneSettings.h"
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"
#include "GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.h"
#include "rw/math/vpu/matrix44affine_operation.h"
#include <array>
#include <vector>

namespace CgsPC::Reflections
{
    namespace
    {
        // FLAG PC-platform leaf: bounded native buffers use the existing platform
        // low-memory allocator and real particle vertex writer. Each face has its own storage,
        // so a GPU draw never sees another face overwrite its CPU source span.
        struct ParticleBuffer
        {
            alignas(16) renderengine::VertexBufferHeader mHeader = {};
            EffectsVertexBuffer mWriter = {};
            void* mpStorage = nullptr;
            bool mbAllocationAttempted = false;
            ~ParticleBuffer() { if (mpStorage) CgsMemory::LowMemory::Release(mpStorage); }
            EffectsVertexBufferLocked* Begin(u32 luBytes)
            {
                if (!mbAllocationAttempted)
                {
                    mbAllocationAttempted = true;
                    mpStorage = CgsMemory::LowMemory::Reserve(luBytes);
                    if (!mpStorage || !CgsMemory::LowMemory::IsLowAddress(mpStorage))
                    {
                        if (mpStorage) CgsMemory::LowMemory::Release(mpStorage);
                        mpStorage = nullptr;
                        return nullptr;
                    }
                    // Native buffer image, using the same named fields as
                    // VertexBuffer::Initialize; separate ownership avoids its
                    // shared 4 MB immediate-mode arena.
                    mHeader.muReferenceCount = 1;
                    mHeader.muUnused14 = 0xffff0000u;
                    mHeader.muBaseAddress = static_cast<u32>(reinterpret_cast<uintptr_t>(mpStorage));
                    mHeader.muSize = luBytes;
                }
                if (!mpStorage) return nullptr;
                void* lpBytes = reinterpret_cast<void*>(static_cast<uintptr_t>(mHeader.muBaseAddress & ~3u));
                mWriter.Construct(lpBytes, mHeader.muSize);
                return &mWriter.Lock();
            }
            void End() { mWriter.UnLock(); }
            renderengine::VertexBuffer* GetBuffer() { return reinterpret_cast<renderengine::VertexBuffer*>(&mHeader); }
        };
        struct FaceBuffers { ParticleBuffer mSimple, mSparks, mLion; };
        std::array<FaceBuffers, 6> saBuffers;
        struct PublishedParticles
        {
            const BrnParticle::ParticleModule* mpOwner = nullptr;
            std::array<BrnParticle::Native::SparkArray, 4> maSparks;
            std::array<std::vector<BrnParticle::Native::SparkBucket>, 8> maSparkBuckets;
            BrnParticle::Native::SparkFrameDataSet mSparkFrames;
            std::array<BrnParticle::Native::BrnDebrisArray, 5> maDebris;
            std::array<std::vector<BrnParticle::Native::BrnDebrisArray::DebrisBucket>, 5> maDebrisBuckets;
        } sPublished;

        template<class Bucket>
        Bucket* CopyBuckets(const Bucket* lpSource, std::vector<Bucket>& lrStorage)
        {
            lrStorage.clear();
            for (const Bucket* lpBucket = lpSource; lpBucket;
                 lpBucket = static_cast<const Bucket*>(lpBucket->mpNextBucket))
                lrStorage.push_back(*lpBucket);
            for (size_t luBucket = 0; luBucket < lrStorage.size(); ++luBucket)
            {
                lrStorage[luBucket].mpPreviousBucket = luBucket ? &lrStorage[luBucket - 1] : nullptr;
                lrStorage[luBucket].mpNextBucket = luBucket + 1 < lrStorage.size() ? &lrStorage[luBucket + 1] : nullptr;
            }
            return lrStorage.empty() ? nullptr : lrStorage.data();
        }
        Matrix44Affine AffineView(const CgsGraphics::Camera& lrCamera)
        {
            return BrnParticle::RowCopyToAffine(lrCamera.mView);
        }
        Matrix44 PackedFrustum(const CgsGraphics::Camera& lrCamera)
        {
            CgsGraphics::CameraRwFrustum lFrustum;
            lrCamera.GetFrustum(lFrustum);
            const auto& lrLeft = lFrustum.maPlanes[2]; const auto& lrRight = lFrustum.maPlanes[3];
            const auto& lrTop = lFrustum.maPlanes[4]; const auto& lrBottom = lFrustum.maPlanes[5];
            return {{lrLeft.x, lrRight.x, lrTop.x, lrBottom.x}, {lrLeft.y, lrRight.y, lrTop.y, lrBottom.y},
                {lrLeft.z, lrRight.z, lrTop.z, lrBottom.z}, {lrLeft.w, lrRight.w, lrTop.w, lrBottom.w}};
        }
    }

    // FLAG PC-platform leaf: called only at the existing joined command-frame
    // boundary. Capture bucket payloads and relink their copies; render never
    // borrows the next update's spark/debris rings.
    void ParticleCapture::Publish(BrnParticle::ParticleModule& lrParticles)
    {
        if (!Particles().mbEnabled && !Shadows::Debris().mbEnabled) return;
        for (u32 luArray = 0; luArray < sPublished.maSparks.size(); ++luArray)
        {
            auto& lrCopy = sPublished.maSparks[luArray];
            lrCopy = lrParticles.maSparks[luArray];
            lrCopy.mRegularBank.mpBuckets = CopyBuckets(lrCopy.mRegularBank.mpBuckets, sPublished.maSparkBuckets[luArray * 2]);
            lrCopy.mCrashBank.mpBuckets = CopyBuckets(lrCopy.mCrashBank.mpBuckets, sPublished.maSparkBuckets[luArray * 2 + 1]);
        }
        sPublished.mSparkFrames = lrParticles.mSparkFrameDataSetUpdate;
        for (u32 luArray = 0; luArray < sPublished.maDebris.size(); ++luArray)
        {
            auto& lrCopy = sPublished.maDebris[luArray];
            lrCopy = lrParticles.maDebris[luArray];
            lrCopy.mpBuckets = CopyBuckets(lrCopy.mpBuckets, sPublished.maDebrisBuckets[luArray]);
        }
        sPublished.mpOwner = &lrParticles;
    }

    bool ParticleCapture::Prepare(const BrnParticle::ParticleModule::ParticleRenderData* lpData)
    {
        if (!Particles().mbEnabled || !lpData || !lpData->mpParticleModule) return false;
        // This is the sole Lion lifecycle update for the presentation. Per-face
        // Render below evaluates existing particles at this same absolute time.
        lpData->mpParticleModule->BuildLionVertexBuffers(lpData);
        return true;
    }

    bool ParticleCapture::HasDebrisShadow(const BrnParticle::ParticleModule::ParticleRenderData& lrData, f32 lfDistance)
    {
        if (!lrData.mpParticleModule || sPublished.mpOwner != lrData.mpParticleModule || !(lfDistance > 0)) return false;
        const auto& lrEye = lrData.mCgsCamera.GetPosition();
        for (u32 luArray = 0; luArray < 4; ++luArray)
            for (const auto& lrBucket : sPublished.maDebrisBuckets[luArray])
                for (u32 lu = 0; lu < lrBucket.mu16NumberOfParticlesInBucket; ++lu)
                {
                    const auto& lrParticle = lrBucket.maParticleData[lu];
                    const auto& lrPosition = lrParticle.mPositionPlusRotVel;
                    const f32 lfX = lrPosition.x - lrEye.x, lfY = lrPosition.y - lrEye.y, lfZ = lrPosition.z - lrEye.z;
                    if (lrParticle.mVelocityPlusScale.w > 0 && lfX*lfX + lfY*lfY + lfZ*lfZ <= lfDistance*lfDistance)
                        return true;
                }
        return false;
    }

    u32 ParticleCapture::RenderDebrisShadow(const BrnParticle::ParticleModule::ParticleRenderData& lrData,
        const CgsGraphics::Camera& lrCamera, f32 lfDistance)
    {
        using namespace BrnParticle::Native;
        if (!lrData.mpParticleModule || sPublished.mpOwner != lrData.mpParticleModule || !(lfDistance > 0)) return 0;
        auto& lrParticles = *lrData.mpParticleModule;
        lrParticles.mDebrisRenderer.BeginRender(lrCamera.GetViewProjectionMatrix(), lrData.mvSunDirection,
            lrData.mvSunColour, lrData.mvAmbientColour, lrData.mCgsCamera.GetPosition());
        static std::array<std::vector<BrnDebrisArray::DebrisBucket>, 4> saShadowBuckets;
        u32 luCount = 0;
        for (u32 luArray = 0; luArray < 4; ++luArray)
        {
            auto lCopy = sPublished.maDebris[luArray];
            lCopy.mpBuckets = CopyBuckets(lCopy.mpBuckets, saShadowBuckets[luArray]);
            for (auto& lrBucket : saShadowBuckets[luArray])
                for (u32 lu = 0; lu < lrBucket.mu16NumberOfParticlesInBucket; ++lu)
                {
                    auto& lrParticle = lrBucket.maParticleData[lu];
                    const auto& lrEye = lrData.mCgsCamera.GetPosition();
                    const auto& lrPosition = lrParticle.mPositionPlusRotVel;
                    const f32 lfX = lrPosition.x - lrEye.x, lfY = lrPosition.y - lrEye.y, lfZ = lrPosition.z - lrEye.z;
                    if (lfX*lfX + lfY*lfY + lfZ*lfZ > lfDistance*lfDistance)
                        lrParticle.mVelocityPlusScale.w = 0;
                    else if (lrParticle.mVelocityPlusScale.w > 0) ++luCount;
                }
            lrParticles.mDebrisRenderer.RenderDebrisArray(lrData.mfCurrentTime, &lCopy,
                static_cast<EDebrisArrayID>(luArray),
                (lrData.muFlags & BrnParticle::ParticleModule::ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0);
        }
        CgsGraphics::ImRendererBase::mgpActiveRenderer = nullptr;
        return luCount;
    }

    u32 ParticleCapture::Render(u32 luFace, BrnParticle::ParticleModule& lrParticles,
        const BrnParticle::ParticleModule::ParticleRenderData& lrData, const CgsGraphics::Camera& lrCamera)
    {
        using namespace BrnParticle;
        using namespace BrnParticle::Native;
        if (sPublished.mpOwner != &lrParticles || lrData.mbPlayingEffectsSuspendedPC) return 0;
        // Keep the geometry pass's exact depth projection. The native clip plane
        // applies the independent cutoff without changing particle occlusion.
        const CgsGraphics::Camera& lCamera = lrCamera;
        u32 luBytes = 0;
        FaceBuffers& lrBuffers = saBuffers[luFace];
        const bool lbCrashBanks = (lrData.muFlags & ParticleModule::ParticleRenderData::eRenderDataFlagReducedFrameRate) != 0;
        SimpleParticleBatchArray lSimpleBatches;
        lSimpleBatches.Construct(); lSimpleBatches.Clear();
        u32 luPreSimpleCount = 0;

        if ((lrData.muFlags & ParticleModule::ParticleRenderData::eRenderDataFlagRenderDebris) != 0)
        {
            lrParticles.mDebrisRenderer.BeginRender(lCamera.GetViewProjectionMatrix(), lrData.mvSunDirection,
                lrData.mvSunColour, lrData.mvAmbientColour, lCamera.GetPosition());
            for (u32 luArray = 0; luArray < sPublished.maDebris.size(); ++luArray)
                lrParticles.mDebrisRenderer.RenderDebrisArray(lrData.mfCurrentTime, &sPublished.maDebris[luArray],
                    static_cast<EDebrisArrayID>(luArray), lbCrashBanks);
            CgsGraphics::ImRendererBase::mgpActiveRenderer = nullptr;
        }
        if ((lrData.muFlags & ParticleModule::ParticleRenderData::eRenderDataFlagRenderSimple) != 0)
        {
            if (auto* lpWriter = lrBuffers.mSimple.Begin(163840u))
            {
                const u32 lauPreTypes[] = {3,4,5,6,7,8,9,10,11,12};
                SimpleParticleVertexBufferBuilder::BuildDispatchData(lpWriter, lSimpleBatches,
                    lrParticles.mSimpleParticleFramePC.GetArrays(), lauPreTypes, 10,
                    lrData.mfCurrentTime, lCamera, lrData.mfWhiteLevel, lbCrashBanks);
                luPreSimpleCount = static_cast<u32>(lSimpleBatches.GetCount());
                const u32 lauPostTypes[] = {1,2};
                SimpleParticleVertexBufferBuilder::BuildDispatchData(lpWriter, lSimpleBatches,
                    lrParticles.mSimpleParticleFramePC.GetArrays(), lauPostTypes, 2,
                    lrData.mfCurrentTime, lCamera, lrData.mfWhiteLevel, lbCrashBanks);
                luBytes += lpWriter->GetBytesUsed();
                lrBuffers.mSimple.End();
                lrParticles.mSimpleParticleRenderer.Dispatch(lrBuffers.mSimple.GetBuffer(), lSimpleBatches,
                    0, luPreSimpleCount, nullptr,
                    lCamera.maProjectionScalars[7], lCamera.maProjectionScalars[8], false);
            }
        }
        if ((lrData.muFlags & ParticleModule::ParticleRenderData::eRenderDataFlagRenderLion) != 0)
        {
            if (auto* lpWriter = lrBuffers.mLion.Begin(196608u))
            {
                auto& lrLion = lrParticles.mLionRenderer;
                const auto lBack = lrLion.mBackMat, lView = lrLion.mViewMat;
                const auto lProjection = lrLion.mViewProjection, lFrustum = lrLion.mPackedFrustumLrtb;
                const Matrix44Affine lFaceView = AffineView(lCamera);
                lrLion.SetCameraData(rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lFaceView),
                    lFaceView, lCamera.GetViewProjectionMatrix(), PackedFrustum(lCamera));
                LionBatchArray lBatches; lBatches.Construct(); lBatches.Clear();
                cLionFX::Render(*lpWriter, lBatches, LionTimeFromSeconds(lrData.mfCurrentTime));
                luBytes += lpWriter->GetBytesUsed();
                lrBuffers.mLion.End();
                cLionFX::Dispatch(lrBuffers.mLion.GetBuffer(), lBatches, lrData.mfWhiteLevel, false,
                    lCamera.maProjectionScalars[7], lCamera.maProjectionScalars[8], 0.6f, 0, 0, nullptr);
                lrLion.SetCameraData(lBack, lView, lProjection, lFrustum);
            }
        }
        if ((lrData.muFlags & ParticleModule::ParticleRenderData::eRenderDataFlagRenderSparks) != 0)
        {
            if (auto* lpWriter = lrBuffers.mSparks.Begin(0x80000u))
            {
                SparkBatchArray lBatches; lBatches.Construct(); lBatches.Clear();
                SparkFrameDataSet lFrames = sPublished.mSparkFrames;
                const Matrix44Affine lView = AffineView(lCamera);
                for (auto& lrFrame : lFrames.maFrames)
                    lrFrame.Set(lView, lCamera.mProjection, lrFrame.mfTimeStamp);
                SparkVertexBufferBuilder::BuildDispatchData(lpWriter, lBatches, sPublished.maSparks.data(), 4,
                    lrData.mfTimeStepMultiplier, lFrames, lrData.mfWhiteLevel, lbCrashBanks);
                luBytes += lpWriter->GetBytesUsed();
                lrBuffers.mSparks.End();
                lrParticles.mSparkRenderer.Dispatch(lCamera.mProjection, lrBuffers.mSparks.GetBuffer(), lBatches);
            }
        }
        if (static_cast<u32>(lSimpleBatches.GetCount()) > luPreSimpleCount)
            lrParticles.mSimpleParticleRenderer.Dispatch(lrBuffers.mSimple.GetBuffer(), lSimpleBatches,
                luPreSimpleCount, static_cast<u32>(lSimpleBatches.GetCount()) - luPreSimpleCount, nullptr,
                lCamera.maProjectionScalars[7], lCamera.maProjectionScalars[8], false);
        return luBytes;
    }

    // FLAG PC-platform leaf: the published tyre/skid strips are dynamic road
    // decals. Their capture policy is independent of the other particle types.
    u32 DecalCapture::Render(const BrnParticle::ParticleModule::ParticleRenderData& lrData,
        const CgsGraphics::Camera& lrCamera)
    {
        if (!lrData.mpParticleModule || lrData.mbPlayingEffectsSuspendedPC
            || !(lrData.muFlags & BrnParticle::ParticleModule::ParticleRenderData::eRenderDataFlagRenderTrails)) return 0;
        auto& lrTrails = lrData.mpParticleModule->mTrailFramePC;
        const u32 luBefore = BrnParticle::Native::TrailRenderer::guProbeDraws;
        lrTrails.Update(lrData.mfCurrentTime, lrCamera.GetViewProjectionMatrix());
        lrTrails.Render(lrData.mfWhiteLevel);
        lrTrails.Update(lrData.mfCurrentTime, lrData.mCgsCamera.GetViewProjectionMatrix());
        return BrnParticle::Native::TrailRenderer::guProbeDraws - luBefore;
    }
}
