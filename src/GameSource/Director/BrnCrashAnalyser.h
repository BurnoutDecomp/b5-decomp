#ifndef GAMESOURCE_DIRECTOR_BRN_CRASH_ANALYSER_H
#define GAMESOURCE_DIRECTOR_BRN_CRASH_ANALYSER_H

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"   // EActiveRaceCarIndex

// ============================================================================
// GameSource/Director/BrnCrashAnalyser.h
//
// HOME (DWARF BrnCrashAnalyser.h:42 / :67) for BrnDirector::CrashAnalysis -- the eight-byte verdict
// on the player's crash that the shot selector and the hard-stop moment read -- and for
// BrnDirector::CrashAnalyser, the MainDirector sub-object (console +0x1245C) that produces it once
// per pre-scene pass (MainDirector::PreSceneQueryUpdate @0x8225BCDC).
//
// It replaces the "minimal slice" of CrashAnalysis BrnDirectorVehicleTracker.h carried, whose
// members were named from their consumers (mxFlags / mau4 / mbUseLeftSide); the DWARF names are
// below and the slice is retired.
//
// Layout (console offsets; parity is by NAMED member -- no pointer lives here, so the host layout
// is the console's): CrashAnalysis { u32 +0, bool +4, bool +5 } (8 bytes); CrashAnalyser
// { mAnalysis +0x0, mLastAnalysis +0x8, mAnalysisAtLastShotChange +0x10 } (0x18 bytes). The
// console reaches all three with word copies (Update @0x822092CC..E4, SetShotChanged inline in
// MainDirector::Update @0x82274F90..AC), which pins the eight-byte stride.
// ============================================================================

namespace BrnDirector
{
    // Class-keys as their homes declare them (BrnDirectorModuleIO.h:105 / BrnDirectorGameState.h:42):
    // MSVC mangles the key, so a `class` here would not bind to the `struct` definition.
    namespace DirectorIO { struct InputBuffer; }
    struct GameState;

    // DWARF BrnCrashAnalyser.h:42.
    struct CrashAnalysis
    {
        // DWARF :46. Inlined by CrashAnalyser::Update @0x82209570..0x82209578 (its only site):
        //     stw 0, 0(this) ; stb 0, 4(this) ; stb 1, 5(this)
        // -- note mbSuggestLeftOfLineOfAction is SET, not cleared.
        void Clear()
        {
            mxEventFlags                = 0;
            mbIsPlayerCrashing          = false;
            mbSuggestLeftOfLineOfAction = true;
        }

        u32  mxEventFlags;                  // :53  +0x0  AttribSys::Enums::CrashEvents bits
        bool mbIsPlayerCrashing;            // :54  +0x4
        bool mbSuggestLeftOfLineOfAction;   // :55  +0x5
    };

    // DWARF BrnCrashAnalyser.h:67.
    struct CrashAnalyser
    {
    public:
        // DWARF :70 / :73. NOT-CALLED on the console: no X360 or PS3 symbol, and neither
        // MainDirector::Construct @0x8225B448 nor MainDirector::Prepare @0x8224FB38 stores into the
        // analyser's span (+0x1245C..+0x12473) -- it starts from the module allocation's zero.
        // Declared for the DWARF shape only.
        void Construct();
        bool Prepare();

        // DWARF :79 / cpp:44. @0x82209290. Body in BrnCrashAnalyser.cpp.
        void Update(const DirectorIO::InputBuffer* lpInput, const GameState* lpGameState,
                    EActiveRaceCarIndex lePlayerCarIndex);

        // DWARF :82 / :85. Inlined everywhere (MainDirector::UpdateMoments publishes &mAnalysis).
        const CrashAnalysis& GetAnalysis() const { return mAnalysis; }
        const CrashAnalysis& GetAnalysisAtLastShotChange() const { return mAnalysisAtLastShotChange; }

        // DWARF :88 / cpp:246. PS3 @0x15048 (`lwz 8 / lwz 0xC ; stw 0x10 / stw 0x14`), inlined on the
        // X360 by MainDirector::Update @0x82274F90..0x82274FAC when the published camera is
        // E_FLAG_NEW_THIS_FRAME: the analysis the NEW shot was chosen against is the previous
        // frame's, i.e. mLastAnalysis.
        void SetShotChanged() { mAnalysisAtLastShotChange = mLastAnalysis; }

    private:
        CrashAnalysis mAnalysis;                    // :92  +0x00
        CrashAnalysis mLastAnalysis;                // :93  +0x08
        CrashAnalysis mAnalysisAtLastShotChange;    // :94  +0x10
    };
}

#endif // GAMESOURCE_DIRECTOR_BRN_CRASH_ANALYSER_H
