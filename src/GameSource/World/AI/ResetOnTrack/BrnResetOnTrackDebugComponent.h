#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Containers/CgsRingBuffer.h"
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleResultInterface.h"

namespace CgsDev
{
    struct Debug2DImmediateRender;
    struct Debug3DImmediateRender;
}

namespace BrnAI
{
    struct ResetOnTrackManager;

    // X360 ARTIST ResetOnTrackManager::Construct @0x82791A48 initializes this embedded
    // component inline; its public menu body and history writers are recovered from the
    // corresponding ARTIST callers. DecFIGS BrnResetOnTrackDebugComponent.h supplies the
    // named member/type shape. Pointer-bearing ring-buffer headers widen naturally on PC.
    struct ResetOnTrackDebugComponent : public CgsDev::DebugComponent
    {
        struct CGS_ALIGN(16) ResetEntry
        {
            AIModuleIO::ResetOnTrackRequest mRequest;
            AIModuleIO::ResetOnTrackResult  mResult;
            f32                             mfAngle;
            f32                             mfDistanceToPlayer;
            s32                             miHngTestCount;
            bool                            mbHasDriver;
        };

        struct HngTestEntry
        {
            Vector3 mStartPosition;
            Vector3 mEndPosition;
            bool    mbHit;
        };

        void Construct(ResetOnTrackManager* lpResetOnTrackManager);
        void Destruct();

        virtual void Update();
        virtual void RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay);
        virtual void RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay);

        void PushResetInfo(const AIModuleIO::ResetOnTrackRequest* lpRequest,
                           const AIModuleIO::ResetOnTrackResult* lpResult);
        void PushHngTestInfo(Vector2 lStartPosition, Vector2 lEndPosition, bool lbHit);

    protected:
        virtual const char* GetName() const;
        virtual const char* GetPath() const;
        virtual void OnActivate();

    private:
        void DrawResetOnTrackAISection(CgsDev::Debug3DImmediateRender* lpDisplay) const;
        static void ResetPlayerCarOnTrackCallback(void* lpUserData);

        ResetOnTrackManager* mpResetOnTrackManager;
        CgsContainers::FixedRingBuffer<ResetEntry, 16>   mResetRingBuffer;
        CgsContainers::FixedRingBuffer<HngTestEntry, 16> mHngTestRingBuffer;
        s32  miLastHngTestCount;
        s32  miFrameCount;
        bool mbShowPlayerInfoOnly;
        bool mbShowResetInfo;
        bool mbShowCurrentInfo;
        bool mbShowHngTestInfo;
        bool mbContinousResetOnTrack;
        bool mbUseHardcodedResetPosition;
        bool mbIncrementResetSpeed;
        s32  miContinuousResetFrameSkip;
    };

    CGS_STATIC_ASSERT((sizeof(ResetOnTrackDebugComponent::ResetEntry) == 0x50),
                  "ResetEntry X360 stride");
    CGS_STATIC_ASSERT((sizeof(ResetOnTrackDebugComponent::HngTestEntry) == 0x30),
                  "HngTestEntry X360 stride");
}
