// b5-decomp/src/GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.cpp
//
// BrnGameState::RoadRulesDebugComponent: the road-rules debug menu (two render toggles and three
// cheats) and its HUD overlay (the current road + time-rule clock, and a Par / Player / Net
// score table over every challenge road).
//
// Console bodies: DecreaseCurrentTimeCallback, DecreaseCurrentStuntTimeCallback,
// AddCrashScoreCallback, OnActivate, RenderHUD. The rest has no body of its own on the console:
//   Construct                  inlined into RoadRulesManager::Construct
//   DecreaseCurrentTime & co.  inlined into their static callbacks
//   GetName / GetPath          vtable slots that point at shared identical bodies
//   RenderWorld                vtable slot that points at the shared empty body

#include "GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.h"

#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"                              // RoadRulesManager (friend access)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"                       // StreetManager, KI_MAX_CHALLENGES
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"                      // ChallengeHighScoreEntry (Net column)
#include "SharedClasses/StreetData/BrnStreetData.h"                                          // StreetData / Road
#include "SharedClasses/StreetData/BrnChallengeData.h"                                       // ChallengeData / ChallengeParScoresEntry
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender::DrawText
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                 // StrStream / SimpleStrStream

namespace BrnGameState
{
    // ---- HUD layout constants (read-only data, values dumped from the image) --------------
    const f32 RoadRulesDebugComponent::KF_PAR_SCORES_X    = 250.0f;
    const f32 RoadRulesDebugComponent::KF_PLAYER_SCORES_X = 300.0f;
    const f32 RoadRulesDebugComponent::KF_NET_SCORES_X    = 350.0f;
    const f32 RoadRulesDebugComponent::KF_ROAD_NAME_X     = 100.0f;
    const f32 RoadRulesDebugComponent::KF_SCORES_Y        =  50.0f;

    // Inlined into RoadRulesManager::Construct: the back-pointer store to +0x0C, both toggle
    // bytes cleared, then the base Register(). The base Construct() is not called.
    void RoadRulesDebugComponent::Construct(RoadRulesManager* lpRoadRulesManager)
    {
        mpRoadRulesManager = lpRoadRulesManager;
        mbRenderInfo       = false;
        mbRenderTimes      = false;

        Register();
    }

    // The vtable's GetName slot is a body shared with the network road-rules debug component.
    const char* RoadRulesDebugComponent::GetName() const
    {
        return "Road Rules";
    }

    // The vtable's GetPath slot is the shared "Gameplay" body.
    const char* RoadRulesDebugComponent::GetPath() const
    {
        return "Gameplay";
    }

    // The vtable's RenderWorld slot is the shared empty body.
    void RoadRulesDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        (void)lpRender;
    }

    // Knock 10 s off the time-rule clock, only while it reads more than 10 s. `fcmpu` + `blelr`
    // returns when the clock is <= 10 or unordered, i.e. it subtracts only when mfTime > 10.
    void RoadRulesDebugComponent::DecreaseCurrentTime()
    {
        if (mpRoadRulesManager->mfTime > 10.0f)
        {
            mpRoadRulesManager->mfTime -= 10.0f;
        }
    }

    // The same for the stunt clock.
    void RoadRulesDebugComponent::DecreaseCurrentStuntTime()
    {
        if (mpRoadRulesManager->mfStuntTime > 10.0f)
        {
            mpRoadRulesManager->mfStuntTime -= 10.0f;
        }
    }

    // Add 100 to the showtime score.
    void RoadRulesDebugComponent::AddCrashScore()
    {
        mpRoadRulesManager->miCrashScore += 100;
    }

    // The menu hands back the user-data registered in OnActivate, which is this component.
    void RoadRulesDebugComponent::DecreaseCurrentTimeCallback(void* lpData)
    {
        static_cast<RoadRulesDebugComponent*>(lpData)->DecreaseCurrentTime();
    }

    void RoadRulesDebugComponent::DecreaseCurrentStuntTimeCallback(void* lpData)
    {
        static_cast<RoadRulesDebugComponent*>(lpData)->DecreaseCurrentStuntTime();
    }

    void RoadRulesDebugComponent::AddCrashScoreCallback(void* lpData)
    {
        static_cast<RoadRulesDebugComponent*>(lpData)->AddCrashScore();
    }

    void RoadRulesDebugComponent::OnActivate()
    {
        RegisterVariable(&mbRenderInfo,  "Render info");
        RegisterVariable(&mbRenderTimes, "Render times");

        RegisterFunction(&RoadRulesDebugComponent::DecreaseCurrentTimeCallback,      this, "Decrease current time");
        RegisterFunction(&RoadRulesDebugComponent::DecreaseCurrentStuntTimeCallback, this, "Decrease current stunt time");
        RegisterFunction(&RoadRulesDebugComponent::AddCrashScoreCallback,            this, "Add crash score");
    }

    // Two independent panels.
    //
    // mbRenderInfo: the current road's debug name ("Invalid" with no current road) and, while a
    // time rule runs, "Road Rule TIME: <mfTime>s". Built in a StrStream over a 1 KB stack buffer.
    //
    // mbRenderTimes: a header row and one row per challenge road (all 64 slots; the inlined
    // StreetData accessors assert if a slot runs past the loaded road count). Each cell is built in
    // a SimpleStrStream constructed per row and Reset() between cells. The integer cells go
    // through the stream's own int formatting (decimal, or hex in the stream's hex modes).
    //
    // The Net cell prints liScore, which holds the Par score when the Par cell was valid and
    // 0x7FFFFFFF otherwise: the console never loads a score out of the Net entry, only its
    // valid bit.
    void RoadRulesDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        if (mbRenderInfo)
        {
            char              lacBuffer[1024];
            CgsDev::StrStream lInfoStream(lacBuffer, sizeof(lacBuffer));

            const char* lpcRoadName = "Invalid";
            if (mpRoadRulesManager->miLastRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX)
            {
                const BrnStreetData::StreetData* lpStreetData = mpRoadRulesManager->mpStreetManager->GetStreetData();
                lpcRoadName = lpStreetData->GetRoad(mpRoadRulesManager->miLastRoadIndex)->GetDebugName();
            }

            lInfoStream << lpcRoadName << " \n";
            lpRender->DrawText(lInfoStream.GetBuffer(), 50.0f, 32.0f, 30.0f, 0xFFFFFFC8u);
            lInfoStream.Reset();

            if (mpRoadRulesManager->maiChallengeRoadIndex[BrnStreetData::E_SCORE_TYPE_TIME] != BrnStreetData::KI_INVALID_ROAD_INDEX)
            {
                lInfoStream << "Road Rule TIME: " << mpRoadRulesManager->mfTime << "s \n";
                lpRender->DrawText(lInfoStream.GetBuffer(), 50.0f, 96.0f, 30.0f, 0xFFFFC8FFu);
                lInfoStream.Reset();
            }
        }

        if (mbRenderTimes)
        {
            lpRender->DrawText("Par",    KF_PAR_SCORES_X,    KF_SCORES_Y, 20.0f, 0xFFFFFFFFu);
            lpRender->DrawText("Player", KF_PLAYER_SCORES_X, KF_SCORES_Y, 20.0f, 0xFFFFFFFFu);
            lpRender->DrawText("Net",    KF_NET_SCORES_X,    KF_SCORES_Y, 20.0f, 0xFFFFFFFFu);

            for (s32 liRoad = 0; liRoad < KI_MAX_CHALLENGES; ++liRoad)
            {
                CgsDev::SimpleStrStream lCellStream;

                const BrnStreetData::ChallengeData* lpParScores =
                    mpRoadRulesManager->mpStreetManager->GetStreetData()->GetChallengeParScore(liRoad);
                const f32 lfRowY = KF_SCORES_Y + static_cast<f32>(liRoad + 2) * 14.0f;
                s32       liScore = 0x7FFFFFFF;

                // Row label: the road's debug name.
                lCellStream << mpRoadRulesManager->mpStreetManager->GetStreetData()->GetRoad(liRoad)->GetDebugName();
                lpRender->DrawText(lCellStream.GetBuffer(), KF_ROAD_NAME_X, lfRowY, 14.0f, 0xFFFFFFFFu);
                lCellStream.Reset();

                // Par: the compiled par score.
                if (lpParScores->ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    liScore = lpParScores->GetScore(BrnStreetData::E_SCORE_TYPE_TIME);
                    lCellStream << liScore;
                    lpRender->DrawText(lCellStream.GetBuffer(), KF_PAR_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu);
                }
                lCellStream.Reset();

                // Player: the local player's recorded score.
                const BrnStreetData::ChallengeData* lpPlayerScores =
                    mpRoadRulesManager->mpStreetManager->GetPlayerChallengeData(liRoad);
                if (lpPlayerScores->ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    lCellStream << lpPlayerScores->GetScore(BrnStreetData::E_SCORE_TYPE_TIME);
                    lpRender->DrawText(lCellStream.GetBuffer(), KF_PLAYER_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu);
                }
                lCellStream.Reset();

                // Net: gated on the online entry's valid bit, prints liScore (see above).
                const BrnStreetData::ChallengeData* lpNetScores =
                    mpRoadRulesManager->mpStreetManager->GetNetChallengeData(liRoad);
                if (lpNetScores->ContainsData(BrnStreetData::E_SCORE_TYPE_TIME))
                {
                    lCellStream << liScore;
                    lpRender->DrawText(lCellStream.GetBuffer(), KF_NET_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu);
                }
            }
        }
    }
}
