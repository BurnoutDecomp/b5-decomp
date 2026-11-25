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
// (the X360 image folds trivial bodies onto one shared address); that base call follows the member initialization.

#include "GameShared/GameClasses/Language/CgsLanguageManagerDebugComponent.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"                                // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"   // Debug2DImmediateRender, Vector2
#include "GameShared/GameClasses/Core/CgsAssert.h"                                             // CGS_ASSERT

namespace CgsLanguage
{
    namespace
    {
        const char KAC_ARTIST_FILE[] =
            "..\\..\\..\\GameShared\\GameClasses\\Language/CgsLanguageManagerDebugComponent.cpp";

        // HUD colours (X360 packed u32): white text, opaque-black full-screen backdrop.
        const u32 KU_TEXT_COLOUR       = 0xFFFFFFFFu;   // X360 -1
        const u32 KU_BACKGROUND_COLOUR = 0xFF000000u;   // X360 -16777216


    }

    void LanguageManagerDebugComponent::Construct( const LanguageManager* lpLanguageManager )
    {
        mbShowDebugComponent       = false;
        mbShowKeys                 = false;
        mbShowLocalisedTextAsStars = false;

        // Original strings, in digit-count order, from 0x820D15B0 and 0x820E6300.
        mapStars[0] = "*";
        mapStars[1] = "**";
        mapStars[2] = "***";
        mapStars[3] = "****";
        mapStars[4] = "********";

        CGS_ASSERT_AT(lpLanguageManager != 0, "lpLanguageManager", KAC_ARTIST_FILE, 114);
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
        CgsDev::DebugComponent::Construct();
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

        const f32 lfScale = mrTextSize;
        const f32 lfY = lrY;
        const f32 lfLabelWidth = lpRender->CalcTextWidth( lpcLabel, lfScale );
        lpRender->DrawText( lpcValue, lfX + lfLabelWidth, lfY, lfScale, KU_TEXT_COLOUR );

        lrY += mrTextSpacing * mrTextSize;
    }

    void LanguageManagerDebugComponent::RenderHUD( CgsDev::Debug2DImmediateRender* lpRender )
    {
        if ( !mbShowDebugComponent )
        {
            return;
        }

        CGS_ASSERT_AT(mpLanguageManager != 0, "mpLanguageManager", KAC_ARTIST_FILE, 223);

        const f32 lfX = mrPosX;
        f32 lfY = mrPosY;
        const Vector2 lv2Origin(0.0f, 0.0f);
        lpRender->DrawBox(lv2Origin, lpRender->GetVirtualScreenSize(), KU_BACKGROUND_COLOUR);

        char acValue[32];

        // The X360 force-terminates the last buffer byte after every format call (defensive against an
        // unterminated formatter); kept here per line.
        mpLanguageManager->FormatIntegerString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mnNumber1, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Integer= ", acValue, lfX, lfY );

        mpLanguageManager->FormatXoverYString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mnNumber1, mnNumber2, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "X / Y = ", acValue, lfX, lfY );

        mpLanguageManager->FormatPercentageString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mnNumber1, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Percentage = ", acValue, lfX, lfY );

        mpLanguageManager->FormatCurrencyString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mnCurrency, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Currency = ", acValue, lfX, lfY );

        mpLanguageManager->FormatDateString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mnDays, mnMonths, mnYears, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Date = ", acValue, lfX, lfY );

        mpLanguageManager->FormatHoursMinutesAndSecondsString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Hours and Minutes and Seconds = ", acValue, lfX, lfY );

        mpLanguageManager->FormatMinutesAndSecondsString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Minutes And Seconds = ", acValue, lfX, lfY );

        mpLanguageManager->FormatMinutesAndSecondsAndHundredsString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Minutes and Seconds and Hundredths = ", acValue, lfX, lfY );

        mpLanguageManager->FormatSecondsAndHundredsString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Seconds and Hundredths = ", acValue, lfX, lfY );

        mpLanguageManager->FormatSecondsString( reinterpret_cast<CgsUnicode::CgsUtf8*>(acValue), mrSeconds, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Time Seconds = ", acValue, lfX, lfY );

        mpLanguageManager->FormatSmallDistanceString( acValue, mrDistance, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Small Distance = ", acValue, lfX, lfY );

        mpLanguageManager->FormatLargeDistanceString( acValue, mrDistance, sizeof( acValue ) );
        acValue[31] = '\0';
        DrawText( lpRender, "Large Distance = ", acValue, lfX, lfY );

        mpLanguageManager->FormatAutoDistanceString(acValue, mrDistance, sizeof(acValue));
        acValue[31] = '\0';
        DrawText( lpRender, "Smallest = ", acValue, lfX, lfY );
    }
}
