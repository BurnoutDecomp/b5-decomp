// Bodies for the language-manager value-formatting debug HUD, reconstructed from BURNOUT_X360_ARTIST.XEX
// (Construct/RenderHUD verified against the disassembly, not just the pseudocode):
//   Construct   @ 0x82860A68
//   GetName     @ 0x82860A58
//   DrawText    @ 0x82860E70
//   RenderHUD   @ 0x82863930
//
// RenderHUD dumps one labelled line per LanguageManager formatter against a fixed set of sample inputs
// (1000 / a 1984-02-16 09:45 date / 0s / 1000m), so localisation output can be read straight off the
// screen. Each line formats into a shared 32-byte buffer, force-terminates it, then draws "<label>
// <value>" via the DrawText helper, which advances a running Y. The trailing folded
// `BaseCollisionGenerator::Destruct(this)` in Construct is the base CgsDev::DebugComponent::Construct()
// (the X360 image folds trivial bodies onto one shared address); it is emitted as a tail call here.

#include "GameShared/GameClasses/Language/CgsLanguageManagerDebugComponent.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"                                // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"   // Debug2DImmediateRender, Vector2
#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT

namespace CgsLanguage
{
    namespace
    {
        // HUD colours (X360 packed u32): white text, opaque-black full-screen backdrop.
        const u32 KU_TEXT_COLOUR       = 0xFFFFFFFFu;   // X360 -1
        const u32 KU_BACKGROUND_COLOUR = 0xFF000000u;   // X360 -16777216

        // The HUD picks the small vs large distance string when (scale * distance) drops below this
        // threshold (X360 flt_82001C98). The exact .rdata bits are not recoverable here; modelled as
        // the natural unit boundary 1.0 (the test is a magnitude check). FLAGGED: value unverified.
        const f32 KF_SMALLEST_DISTANCE_THRESHOLD = 1.0f;
    }

    void LanguageManagerDebugComponent::Construct( const LanguageManager* lpLanguageManager )
    {
        mbShowDebugComponent       = false;
        mbShowKeys                 = false;
        mbShowLocalisedTextAsStars = false;

        // Star strings indexed by GetStarString(digitCount). The longer entries are recovered .rdata
        // literals; the two shortest (X360 unk_820D15B0 / unk_820E6318) are not readable from the
        // available exports - FLAGGED: their exact contents are unverified, modelled as the natural
        // one/two-star strings that continue the sequence.
        mapStars[0] = "*";          // unk_820D15B0 (FLAGGED, unverified)
        mapStars[1] = "**";         // unk_820E6318 (FLAGGED, unverified)
        mapStars[2] = "***";
        mapStars[3] = "****";
        mapStars[4] = "********";

        CGS_ASSERT( lpLanguageManager != nullptr, "lpLanguageManager" );
        mpLanguageManager = lpLanguageManager;

        // Layout + text style.
        mrPosX        = 200.0f;
        mrPosY        = 200.0f;
        mrTextSpacing = 1.5f;
        mrTextSize    = 18.0f;

        // Fixed sample inputs the HUD formats every frame.
        mnNumber1  = 1000;
        mnNumber2  = 1000;
        mnCurrency = 1000;
        mnDays     = 16;
        mnMonths   = 2;
        mnYears    = 1984;
        mnHours    = 9;
        mnMinutes  = 45;
        mrSeconds  = 0.0f;
        mrDistance = 1000.0f;

        CgsDev::DebugComponent::Construct();   // base two-phase init (X360 tail call)
    }

    const char* LanguageManagerDebugComponent::GetName() const
    {
        return "LanguageManager";
    }

    void LanguageManagerDebugComponent::DrawText( CgsDev::Debug2DImmediateRender* lpRender,
                                                  const char* lpcLabel, const char* lpcValue,
                                                  f32 lfX, f32& lrY ) const
    {
        lpRender->DrawText( lpcLabel, lfX, lrY, mrTextSize, KU_TEXT_COLOUR );

        const f32 lfLabelWidth = lpRender->CalcTextWidth( lpcLabel, mrTextSize );
        lpRender->DrawText( lpcValue, lfX + lfLabelWidth, lrY, mrTextSize, KU_TEXT_COLOUR );

        lrY += mrTextSpacing * mrTextSize;
    }

    void LanguageManagerDebugComponent::RenderHUD( CgsDev::Debug2DImmediateRender* lpRender )
    {
        if ( !mbShowDebugComponent )
        {
            return;
        }

        CGS_ASSERT( mpLanguageManager != nullptr, "mpLanguageManager" );

        // Dim full-screen backdrop so the white text reads against any scene. The top-left is the
        // origin (X360 flt_82001CC0 == 0.0, the same .rdata float that zero-inits mrSeconds); the
        // bottom-right is the renderer's virtual screen size.
        Vector2 lv2Origin;
        lv2Origin.SetZero();
        lpRender->DrawBox( lv2Origin, lpRender->GetVirtualScreenSize(), KU_BACKGROUND_COLOUR );

        char acValue[32];
        f32  lfY = mrPosY;

        // The X360 force-terminates the last buffer byte after every format call (defensive against an
        // unterminated formatter); kept here per line.
        mpLanguageManager->FormatIntegerString( acValue, mnNumber1, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Integer= ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatXoverYString( acValue, mnNumber1, mnNumber2, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "X / Y = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatPercentageString( acValue, mnNumber1, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Percentage = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatCurrencyString( acValue, mnCurrency, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Currency = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatDateString( acValue, mnDays, mnMonths, mnYears, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Date = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatHoursMinutesAndSecondsString( acValue, mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Hours and Minutes and Seconds = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatMinutesAndSecondsString( acValue, mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Minutes And Seconds = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatMinutesAndSecondsAndHundredsString( acValue, mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Minutes and Seconds and Hundredths = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatSecondsAndHundredsString( acValue, mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Seconds and Hundredths = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatSecondsString( acValue, mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Seconds = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatSmallDistanceString( acValue, mrDistance, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Small Distance = ", acValue, mrPosX, lfY );

        mpLanguageManager->FormatLargeDistanceString( acValue, mrDistance, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Large Distance = ", acValue, mrPosX, lfY );

        // "Smallest": pick whichever distance string reads better for this magnitude.
        if ( mpLanguageManager->GetDistanceDisplayScale() * mrDistance < KF_SMALLEST_DISTANCE_THRESHOLD )
        {
            mpLanguageManager->FormatSmallDistanceString( acValue, mrDistance, sizeof( acValue ) );
        }
        else
        {
            mpLanguageManager->FormatLargeDistanceString( acValue, mrDistance, sizeof( acValue ) );
        }
        acValue[31] = '\0';
        DrawText( lpRender, "Smallest = ", acValue, mrPosX, lfY );
    }
}
