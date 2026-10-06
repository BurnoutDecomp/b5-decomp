#pragma once

#include "GameSource/Effects/Particles/Native/BrnTrailSystem.h"
#include "pc/gcm/renderengine/TrailPixelDiagPC.h"

namespace BrnParticle { namespace Native {

// FLAG PC-platform leaf: update lays and reuses trails while dispatch consumes
// the preceding frame. Copy the draw inputs only after dispatch has joined;
// retain the original strip builder, colours, timestamps and camera arithmetic.
class TrailFramePC
{
public:
    void Publish(TrailSystem& lrSource)
    {
        mbReady = lrSource.mbIsReady;
        if (!mbReady) return;
        mTexture = lrSource.mTrailTexture;
        mRenderer = lrSource.mRenderer;
        for (s32 liEmitter = 0; liEmitter < KN_TRAIL_EMITTER_POOL_SIZE; ++liEmitter)
        {
            maEmitters[liEmitter] = lrSource.maEmitterPool[liEmitter];
            if (lrSource.maEmitterPool[liEmitter].mpCurrentSegments != nullptr)
                maSegments[liEmitter] = *lrSource.maEmitterPool[liEmitter].mpCurrentSegments;
            maEmitters[liEmitter].mpCurrentSegments = &maSegments[liEmitter];
            maEmitters[liEmitter].mpOldSegments = &maSegments[liEmitter];
            maEmitters[liEmitter].mpOwner = nullptr;
        }
        for (s32 liType = 0; liType < KI_MAX_NUM_TRAIL_TYPES; ++liType)
        {
            maParams[liType] = TrailSystem::mgaDefaultParams[liType];
            EmitterArray& lrActive = lrSource.maActiveEmitters[liType];
            maCounts[liType] = lrActive.GetSize();
            for (s32 liActive = 0; liActive < maCounts[liType]; ++liActive)
            {
                const s32 liEmitter = static_cast<s32>(lrActive[liActive] - lrSource.maEmitterPool);
                maActive[liType][liActive] = &maEmitters[liEmitter];
            }
        }
    }

    void Update(f32 lfCurrentTime, Matrix44::InParam lViewProjection)
    {
        mRenderer.Update(lfCurrentTime, lViewProjection);
    }

    void Render(f32 lfWhiteLevel)
    {
        if (!mbReady) return;
        // Default-off readbacks bracket exactly this original pass, including
        // an empty pass after expiry. They never bind, add or suppress a draw.
        bool lbPixelPair = false;
        if (renderengine::TrailPixelDiag_EnabledPC())
        {
            renderengine::TrailPixelBatchPC laBatches[KI_MAX_NUM_TRAIL_TYPES];
            for (s32 liType = 0; liType < KI_MAX_NUM_TRAIL_TYPES; ++liType)
                laBatches[liType] = {maActive[liType], maCounts[liType]};
            lbPixelPair = renderengine::TrailPixelDiag_BeginPC(mRenderer.mfCurrentTime,
                mRenderer.mViewProjectionMatrix, laBatches);
        }
        mRenderer.BeginRender(mTexture);
        for (s32 liType = 0; liType < KI_MAX_NUM_TRAIL_TYPES; ++liType)
            if (maCounts[liType] > 0)
                mRenderer.Render(maActive[liType], maCounts[liType], &maParams[liType],
                                 static_cast<s8>(liType), lfWhiteLevel);
        mRenderer.EndRender();
        if (lbPixelPair) renderengine::TrailPixelDiag_EndPC();
    }

private:
    bool mbReady = false;
    CgsResource::SafeResourceHandle<renderengine::Texture> mTexture;
    TrailRenderer mRenderer = {};
    TrailEmitter maEmitters[KN_TRAIL_EMITTER_POOL_SIZE] = {};
    TrailSegmentCollection maSegments[KN_TRAIL_EMITTER_POOL_SIZE] = {};
    TrailEmitter* maActive[KI_MAX_NUM_TRAIL_TYPES][KN_TRAIL_EMITTER_POOL_SIZE] = {};
    s32 maCounts[KI_MAX_NUM_TRAIL_TYPES] = {};
    TrailParams maParams[KI_MAX_NUM_TRAIL_TYPES] = {};
};

} }
