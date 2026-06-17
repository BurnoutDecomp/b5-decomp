// b5-decomp/src/GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.cpp
//
// Bodies for the road-rules debug component reconstructed from BURNOUT_X360_ARTIST.XEX:
//   DecreaseCurrentTimeCallback       @ 0x823171F0
//   DecreaseCurrentStuntTimeCallback  @ 0x82317220
//   AddCrashScoreCallback             @ 0x82317250
//   OnActivate                        @ 0x823248E0
//   RenderHUD                         @ 0x82335350
//
// The component reaches directly into the (otherwise private) RoadRulesManager state; the
// BrnRoadRulesManager.h grow that lands with this slice materialises the touched members
// (mfTime/mfStuntTime/miCrashScore/maiChallengeRoadIndex) and grants `friend class
// RoadRulesDebugComponent`. The StreetManager accessors come from the committed
// BrnGameStateStreetManager.h home.

#include "GameSource/GameState/RoadRules/BrnRoadRulesDebugComponent.h"

#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"                              // RoadRulesManager (member reads)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"                       // BrnGameState::StreetManager
#include "GameSource/GameState/StreetData/BrnChallengeHighScoreEntry.h"                      // ChallengeHighScoreEntry (Net column)
#include "SharedClasses/StreetData/BrnStreetData.h"                                          // StreetData / Road / ChallengeParScoresEntry
#include "SharedClasses/StreetData/BrnChallengeData.h"                                       // ChallengeData (Player column)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h" // Debug2DImmediateRender, RGBA
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                 // StrStream / SimpleStrStream
#include "GameShared/GameClasses/Core/CgsAssert.h"                                           // CGS_ASSERT

namespace BrnGameState
{
    // ---- HUD layout constants (X360 .rdata) --------------------------------
    // The score-table column X positions + header Y, inlined as literals in the
    // X360 header-draw calls (250.0/300.0/350.0 @ Y 50.0).
    const f32 RoadRulesDebugComponent::KF_PAR_SCORES_X    = 250.0f;   // flt_82021390
    const f32 RoadRulesDebugComponent::KF_PLAYER_SCORES_X = 300.0f;   // flt_82020F08
    const f32 RoadRulesDebugComponent::KF_NET_SCORES_X    = 350.0f;   // flt_82021398
    const f32 RoadRulesDebugComponent::KF_SCORES_Y        =  50.0f;   // flt_820138DC
    // FLAGGED: the row-label (road index) column X is loaded from flt_820049E0 in the X360
    // binary (asm: `lfs f1, 0x49E0(r11)` with r11 = 0x82000000). The float's exact .rdata
    // bits are not recoverable from the available IDA exports (no raw .rdata dump); modelled
    // here as the natural left-of-Par column position. This is a screen-position layout
    // constant only -- it has no ABI/behaviour effect -- but the precise value is unverified.
    const f32 RoadRulesDebugComponent::KF_ROAD_NAME_X     = 200.0f;   // flt_820049E0 (FLAGGED value)

    namespace
    {
        // The X360 builds each score cell with CgsDev::StrStreamBase::AppendFormat using one of two
        // .rdata format strings selected by a 0..2 highlight counter (X360 v92):
        //   off_82F3193C = "%d"    (normal score)
        //   off_82F31944 = "0x%X"  (highlighted score)
        // v92 is reset to 0 at the top of every row and is NEVER incremented anywhere in RenderHUD,
        // so the selector is unconditionally 0 every iteration -> the "%d" path is the only one ever
        // taken. The highlight branch is therefore dead in this build; the cell append collapses to a
        // single decimal integer stream (value-identical to the live X360 path).
        void AppendScore( CgsDev::SimpleStrStream& lStream, s32 liScore )
        {
            lStream << liScore;   // "%d"
        }
    }

    // @ 0x823171F0. Static menu-action callback: the void* user-data IS the component (registered via
    // RegisterFunction(&DecreaseCurrentTimeCallback, this, "Decrease current time")). The X360 reads
    // *(*(data+12)+68) == lpThis->mpRoadRulesManager->mfTime and knocks 10s off it (floored at 10s).
    // The decompiler's `int result`/`return result` is the register-passed `this` artifact on a void
    // function; dropped.
    void RoadRulesDebugComponent::DecreaseCurrentTimeCallback( void* lpData )
    {
        RoadRulesDebugComponent* lpThis = static_cast<RoadRulesDebugComponent*>( lpData );

        // *(manager+68) == mfTime; clamp so it never drops below 10s.
        if ( lpThis->mpRoadRulesManager->mfTime > 10.0f )
        {
            lpThis->mpRoadRulesManager->mfTime -= 10.0f;
        }
    }

    // @ 0x82317220. Same shape as DecreaseCurrentTimeCallback but for the stunt-rule clock:
    // *(*(data+12)+88) == lpThis->mpRoadRulesManager->mfStuntTime, knocked down 10s, floored at 10s.
    void RoadRulesDebugComponent::DecreaseCurrentStuntTimeCallback( void* lpData )
    {
        RoadRulesDebugComponent* lpThis = static_cast<RoadRulesDebugComponent*>( lpData );

        // *(manager+88) == mfStuntTime.
        if ( lpThis->mpRoadRulesManager->mfStuntTime > 10.0f )
        {
            lpThis->mpRoadRulesManager->mfStuntTime -= 10.0f;
        }
    }

    // @ 0x82317250. Menu-action callback: bump the manager's current crash-rule score by 100.
    // *(*(data+12)+96) == lpThis->mpRoadRulesManager->miCrashScore += 100.
    void RoadRulesDebugComponent::AddCrashScoreCallback( void* lpData )
    {
        RoadRulesDebugComponent* lpThis = static_cast<RoadRulesDebugComponent*>( lpData );

        // *(manager+96) == miCrashScore.
        lpThis->mpRoadRulesManager->miCrashScore += 100;
    }

    // @ 0x823248E0. Called when the debug menu opens this component: register the two render-toggle
    // flags as bool variables and the three manual road-rule cheats as menu actions.
    //   sub_8282D800(this, this+16, name) == DebugComponent::RegisterVariable(bool*, const char*)
    //   sub_8282F720(this, cb, this, name) == DebugComponent::RegisterFunction(cb, void*, const char*)
    // The decompiler `return sub_8282F720(...)` is the void tail-call artifact (RegisterFunction is
    // void); dropped.
    void RoadRulesDebugComponent::OnActivate()
    {
        RegisterVariable( &mbRenderInfo,  "Render info" );
        RegisterVariable( &mbRenderTimes, "Render times" );

        RegisterFunction( &RoadRulesDebugComponent::DecreaseCurrentTimeCallback,      this, "Decrease current time" );
        RegisterFunction( &RoadRulesDebugComponent::DecreaseCurrentStuntTimeCallback, this, "Decrease current stunt time" );
        RegisterFunction( &RoadRulesDebugComponent::AddCrashScoreCallback,            this, "Add crash score" );
    }

    // @ 0x82335350. The road-rules debug HUD. Two independent panels gated by the two toggle flags:
    //
    //   mbRenderInfo (*(this+16)): top-left text. Line 1 = the current road's debug name ("Invalid"
    //     when the manager has no current road index; the road's name is read via
    //     mpStreetManager->GetStreetData()->GetRoad(miLastRoadIndex)->GetDebugName(); a null name
    //     falls back to "<NULLSTRING>"). Line 2, only when a challenge road is set
    //     (maiChallengeRoadIndex[0] != -1), prints "Road Rule TIME: <mfTime>s".
    //
    //   mbRenderTimes (*(this+17)): the three-column per-road score table (Par / Player / Net).
    //
    // Each label is built in a stack stream (a CgsDev::StrStream over a 1024-byte buffer for the info
    // panel, a CgsDev::SimpleStrStream per table cell) and flushed via the immediate-mode debug
    // renderer's DrawText. The X360 `MaybeDrawText(render, text, .., X, Y, scale, .., colour, ..)`
    // is CgsDev::Debug2DImmediateRender::DrawText(text, X, Y, scale, RGBA); the extra register args
    // are decompiler artifacts. Packed-u32 colours: 0xFFFFFFFF white (-1), 0xFFFFFFC8 (-56),
    // 0xFFFFC9FF (-14081).
    void RoadRulesDebugComponent::RenderHUD( CgsDev::Debug2DImmediateRender* lpRender )
    {
        // ---- Panel 1: current-road info ------------------------------------
        if ( mbRenderInfo )
        {
            char              lacBuffer[1024];
            CgsDev::StrStream lInfoStream( lacBuffer, sizeof( lacBuffer ) );
            lacBuffer[0] = 0;

            const char* lpcRoadName = "Invalid";
            if ( mpRoadRulesManager->miLastRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX )
            {
                const BrnStreetData::StreetData* lpStreetData = mpRoadRulesManager->mpStreetManager->GetStreetData();
                const BrnStreetData::Road*       lpRoad       = lpStreetData->GetRoad( mpRoadRulesManager->miLastRoadIndex );
                lpcRoadName = lpRoad->GetDebugName();
                if ( lpcRoadName == nullptr )
                {
                    lpcRoadName = "<NULLSTRING>";
                }
            }

            lInfoStream << lpcRoadName << " \n";
            lpRender->DrawText( lInfoStream.GetBuffer(), 50.0f, 32.0f, 30.0f, 0xFFFFFFC8u );
            lInfoStream.Reset();

            // Second line: active road-rule TIME, only while a challenge road is set.
            if ( mpRoadRulesManager->maiChallengeRoadIndex[0] != BrnStreetData::KI_INVALID_ROAD_INDEX )
            {
                lInfoStream << "Road Rule TIME: " << mpRoadRulesManager->mfTime << "s \n";
                lpRender->DrawText( lInfoStream.GetBuffer(), 50.0f, 96.0f, 30.0f, 0xFFFFC9FFu );
                lInfoStream.Reset();
            }
        }

        // ---- Panel 2: per-road Par / Player / Net high-score table ---------
        if ( mbRenderTimes )
        {
            // Column headers.
            lpRender->DrawText( "Par",    KF_PAR_SCORES_X,    KF_SCORES_Y, 20.0f, 0xFFFFFFFFu );
            lpRender->DrawText( "Player", KF_PLAYER_SCORES_X, KF_SCORES_Y, 20.0f, 0xFFFFFFFFu );
            lpRender->DrawText( "Net",    KF_NET_SCORES_X,    KF_SCORES_Y, 20.0f, 0xFFFFFFFFu );

            // X360 PARITY: the row loop is FIXED at 64 iterations. The index `liRoad` runs 0.. and the
            // player-table byte offset `liPlayerByteOff` steps +40 each row, breaking only when it
            // reaches 2560 (== 64 * 40). The X360 does NOT clamp to the live road count -- instead it
            // relies on the two inline StreetData::GetMemoryR bounds asserts inside the body
            // (BrnStreetData.h lines 718 / 621) to fire if a row index runs past miRoadCount. The
            // committed StreetData::GetRoad / GetChallengeParScore accessors carry exactly that
            // CGS_ASSERT( liIndex < miRoadCount && liIndex >= 0 ) guard, so iterating the fixed 64
            // rows reproduces the X360's assert-on-overrun behaviour faithfully.
            s32 liRoad         = 0;   // X360 v41 (row index)
            s32 liPlayerByteOff = 0;  // X360 v44 (player-table byte cursor, stride 40)
            while ( true )
            {
                CgsDev::SimpleStrStream lCellStream;
                const f32               lfRowY = ( static_cast<f32>( liRoad + 2 ) * 14.0f ) + 50.0f;

                const BrnStreetData::StreetData* lpStreetData = mpRoadRulesManager->mpStreetManager->GetStreetData();

                // Row label: the road index, drawn at the road-name column.
                lCellStream << liRoad;
                lpRender->DrawText( lCellStream.GetBuffer(), KF_ROAD_NAME_X, lfRowY, 14.0f, 0xFFFFFFFFu );
                lCellStream.Reset();

                // -- Par column: the StreetData compiled par challenge score for this road --------
                // The Par valid mask is the local u64 BrnStreetData::ChallengeParScoresEntry.mValidScores
                // (plain integer bitfield) -> `& 1` matches the X360 `ld; clrldi ,,63`.
                const BrnStreetData::ChallengeParScoresEntry* lpParScores = lpStreetData->GetChallengeParScore( liRoad );
                s32 liParScore = 0x7FFFFFFF;
                if ( lpParScores->mValidScores & 1 )
                {
                    liParScore = lpParScores->maScoreList[0];
                    AppendScore( lCellStream, liParScore );
                    lpRender->DrawText( lCellStream.GetBuffer(), KF_PAR_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu );
                }
                lCellStream.Reset();

                // -- Player column: the local-player high score (StreetManager player table) ------
                // The valid mask here is a CgsContainers::BitArray<2u> -> IsBitSet(0) (the X360 asserts
                // even bake "mValidScores.IsBitSet( leScoreType )").
                const BrnStreetData::ChallengeData* lpPlayerScore = mpRoadRulesManager->mpStreetManager->GetPlayerChallengeData( liRoad );
                if ( lpPlayerScore->mValidScores.IsBitSet( 0 ) )
                {
                    AppendScore( lCellStream, lpPlayerScore->mScoreList.maScores[0] );
                    lpRender->DrawText( lCellStream.GetBuffer(), KF_PLAYER_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu );
                }
                lCellStream.Reset();

                // -- Net column: the online high score (StreetManager ChallengeHighScoreEntry table) --
                // X360 quirk faithfully preserved: when the Net cell is valid the value DRAWN is the Par
                // score (X360 register r27/v49, last loaded from the Par cell), not a separate Net score.
                const BrnStreetData::ChallengeHighScoreEntry* lpNetScore = mpRoadRulesManager->mpStreetManager->GetNetChallengeData( liRoad );
                if ( lpNetScore->mValidScores.IsBitSet( 0 ) )
                {
                    AppendScore( lCellStream, liParScore );
                    lpRender->DrawText( lCellStream.GetBuffer(), KF_NET_SCORES_X, lfRowY, 14.0f, 0xFFFFFFFFu );
                }

                // X360 loop step: byte cursor += 40, index += 1; break once the cursor hits 2560.
                liPlayerByteOff += 40;
                ++liRoad;
                if ( liPlayerByteOff >= 2560 )
                {
                    break;
                }
            }
        }
    }
}
