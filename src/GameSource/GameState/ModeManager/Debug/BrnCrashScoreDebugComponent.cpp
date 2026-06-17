#include "GameSource/GameState/ModeManager/Debug/BrnCrashScoreDebugComponent.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // CgsDev::Debug2DImmediateRender, RGBA, Vector2
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                       // CgsCore::SPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The "Showtime score" crash-mode debug overlay.
// RenderHUD/Update assert the scoring object is wired, OnActivate registers the two bool tweakables,
// and DisplayScores formats six live crash-scoring stats and draws them down the right of the HUD via
// the debug 2D immediate renderer (the X360 inlines CrashModeScoring's getters to direct member reads;
// five map onto the closed accessor set, the sixth -- "Highest jump" / mfHighestJump @ +0x320 -- has
// no accessor in the DWARF, so it is read directly as a friend, exactly as the X360 binary does).

namespace BrnGameState
{
    // File-scope debug-HUD text colour (X360 dword_82CDB850, referenced only here). Packed RGBA8;
    // the X360 stores a single constant word -- white for the readout text.
    static const CgsDev::RGBA KU_INFO_TEXT_COLOUR = 0xFFFFFFFFu;

    // @ 0x82312168 - assert the scoring object is wired (the base Update has nothing else to do here).
    void CrashScoreDebugComponent::Update()
    {
        CGS_ASSERT(mpCrashScoring != nullptr, "mpCrashScoring != NULL");
    }

    // @ 0x823121B0
    const char* CrashScoreDebugComponent::GetName() const
    {
        return "Showtime score";
    }

    // @ 0x823121C0 - register the two crash-score debug tweakables with the debug menu: the local
    // "Display score" flag (this+0x10) and the scoring object's "Infinite Crash Mode" flag.
    void CrashScoreDebugComponent::OnActivate()
    {
        RegisterVariable(&mbShowScoring, "Display score");
        RegisterVariable(&mpCrashScoring->mbInfiniteCrashMode, "Infinite Crash Mode (never ends)");
    }

    // @ 0x82312210 - format and draw the six live crash-scoring stats down the HUD. x is fixed at
    // 190; y steps 120/150/170/190/210/230; the first line is drawn at scale 30, the rest at 20.
    void CrashScoreDebugComponent::DisplayScores(CgsDev::Debug2DImmediateRender* lpRender)
    {
        char lacBuffer[64];

        // +0x308 mfDistanceTravelled (f32), printed as an integer metre count (X360 fctiwz -> %d).
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Distance = %dm",
                         static_cast<s32>(mpCrashScoring->GetDistanceTravelled()));
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 120.0f, 0.0f, 0.0f }, 30.0f, KU_INFO_TEXT_COLOUR, false);

        // +0x2F8 miNumCarsLeaped.
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Cars leaped = %d",
                         mpCrashScoring->GetNumCarsLeapt());
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 150.0f, 0.0f, 0.0f }, 20.0f, KU_INFO_TEXT_COLOUR, false);

        // +0x2E0 miCurrentComboCount.
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Current combo count = %d",
                         mpCrashScoring->GetCurrentComboCount());
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 170.0f, 0.0f, 0.0f }, 20.0f, KU_INFO_TEXT_COLOUR, false);

        // +0x2E4 miScoreMultiplier.
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Score multiplier = %d",
                         mpCrashScoring->GetScoreMultiplier());
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 190.0f, 0.0f, 0.0f }, 20.0f, KU_INFO_TEXT_COLOUR, false);

        // +0x31C mfLongestJumpAirTime (f32), printed as %.0f seconds.
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Longest air time = %.0f seconds",
                         static_cast<f64>(mpCrashScoring->GetBestAirTime()));
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 210.0f, 0.0f, 0.0f }, 20.0f, KU_INFO_TEXT_COLOUR, false);

        // +0x320 mfHighestJump (f32), printed as %.0f metres. CrashModeScoring exposes no accessor for
        // this member (its getter set is closed); the X360 reads the field directly inline, so this
        // component (a friend of CrashModeScoring) reads it directly too.
        CgsCore::SPrintf(lacBuffer, sizeof(lacBuffer), "Highest jump = %.0f metres",
                         static_cast<f64>(mpCrashScoring->mfHighestJump));
        lpRender->DrawText(lacBuffer, Vector2{ 190.0f, 230.0f, 0.0f, 0.0f }, 20.0f, KU_INFO_TEXT_COLOUR, false);
    }

    // @ 0x8231EB90 - assert the scoring object is wired, then draw the readout if "Display score" is on.
    void CrashScoreDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
    {
        CGS_ASSERT(mpCrashScoring != nullptr, "mpCrashScoring != NULL");
        if (mbShowScoring)
            DisplayScores(lpRender);
    }
}
