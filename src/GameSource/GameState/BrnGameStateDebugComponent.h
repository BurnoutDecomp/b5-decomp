#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                        // CgsID
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // real debug component base

namespace CgsDev
{
    struct Debug2DImmediateRender;
    class DebugRender;
    namespace DebugUI
    {
        struct Metrics;
    }
}

namespace BrnGameState
{
    class GameStateModule;

    // GameStateModule's X360-owned debug page. The declaration shape is from DecFIGS;
    // ARTIST registration, consumer and lifecycle accesses gate the X360-attested members.
    class GameStateDebugComponent : public CgsDev::DebugComponent
    {
    public:
        GameStateDebugComponent() : CgsDev::DebugComponent(CgsDev::DebugComponent::GameStateConstructorTag()) {}

    public:
        void Construct(GameStateModule* lpGameStateModule);
        void Destruct();
        void RenderHUD(CgsDev::Debug2DImmediateRender* lpRender) ;
        void RenderLandmarkDebug() const;
        void DrawRankNumber(s32 liRank);

    protected:
        const char* GetName() const ;
        void OnActivate() ;

    private:
        void RenderCurrentLandmarkInfo(CgsDev::DebugRender& lrRender,
                                       const CgsDev::DebugUI::Metrics& lrMetrics) const;
        void RenderNextLandmarkInfo(CgsDev::DebugRender& lrRender,
                                    const CgsDev::DebugUI::Metrics& lrMetrics) const;
        void DrawUpcomingRoads(CgsDev::Debug2DImmediateRender* lpRender);
        void DrawStuntBreakdown(CgsDev::Debug2DImmediateRender* lpRender);
        s32 FindRoadNameIndexFromId(CgsID lRoadId);

        static void ClearTrainingFlagsCallback(void* lpData);
        static void ToggleShowtimeCallback(void* lpData);
        static void PlayTrainingMessageCallback(void* lpData);

        static const s32 KI_SCORE_TEXT_LENGTH = 128;
        static const s32 KI_LANDMARK_TEXT_LENGTH = 128;

        GameStateModule* mpGameStateModule;        // +0x0C
        bool mbShowUpComingRoadRules;             // +0x10
        bool mbDebugThisLandmark;                 // +0x11
        bool mbDebugNextLandmark;                 // +0x12
        bool mbDebugRenderFinishLines;            // +0x13
        bool mbDebugCheckpointTimes;              // +0x14
        bool mbDrawStuntBreakdown;                // +0x15
        bool mbDrawMiscInfo;                      // +0x16
        s32 meTrainingMessage;                    // +0x18
        char macScoreText[KI_SCORE_TEXT_LENGTH];   // +0x1C
    };
}
