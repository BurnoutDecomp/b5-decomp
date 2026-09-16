#pragma once

// ===================================================================================
// BrnGui::CrashNavStats -- THE PAUSE MENU'S "STATS" TAB.
//
// ⭐⭐ WHY THIS HEADER GREW. The out-of-line bodies for this state landed in
// BrnCrashNavStats.cpp, but this header declared exactly ONE thing -- GetResourcesToLoad --
// so the .cpp defined members and methods the class did not have and the TU could not
// compile. It was therefore never added to tools/build/build_game_exe.bat, which is why
// `grep -c BrnCrashNavStats.cpp tools/build/build_game_exe.bat` read 0: the state IS
// registered by the screen flow (BrnScreenFlow.cpp:303, lapStates[10],
// Construct(CgsIDCompress("CN_STATS"))) and its code never ran. With only the do-nothing
// CgsGui::State base reachable, the tab registered for NO events, so it received none --
// the same header-only-shell shape CrashNavSettings and CrashNavOptions were fixed out of.
//
// SHAPE. DecFIGS DWARF (references/DecFIGS/dwarfdump/.../BrnCrashNavStats.h), gated on the
// X360 ledger. Guest offsets are MEASURED from the X360 bodies, and they close exactly:
//    +0x0038  meCurrentState     (OnEnter @0x824B5BE0: `stw r11, 0x38`)
//    +0x003C  mpGuiCache         (UpdateInitSetup @0x824CA970: `stw r10, 0x3C(r28)`)
//    +0x0040  maStatTextfields   (OnEnter: `addi r29, r31, 0x40`, loop stride 0x128)
//    +0x29E0  mbDataReceived     (OnEnter: `stb r11, 0x29E0`)
// 0x40 + 36 * sizeof(TextField)(0x128) == 0x29E0, so the array closes ON mbDataReceived and
// the field count is pinned at 36 by the layout as well as by the name table.
//
// ⛔ NOT IMPORTED FROM THE DWARF (declared there, ABSENT from the X360 ledger, i.e. folded
// by the X360 compiler): UpdateRunning (cpp:374) -- the RUNNING rung of Update @0x824D8318
// is inlined to "store 3 and break", with no call -- and HandleControllerInput (cpp:584),
// whose whole body is inlined into UpdatePermanent's event-6 arm. Per the project rule the
// X360 ledger decides what exists, so neither is declared here.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameSource/Gui/BrnGuiTextField.h"   // TextField (36 embedded by value)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class GuiCache;                    // pointer-only member
    struct GuiEventStatsResponse;      // HandleStatData's argument (BrnGuiEventStatsResponse.h)

    struct CrashNavStats : public CgsGui::State
    {
        // DWARF BrnCrashNavStats.h:137. Update @0x824D8318 dispatches 0..4 through a jump
        // table at 0x824D8350 and asserts above 4, so every enumerator below COUNT is live.
        enum EInternalScreenState
        {
            E_INTERNALSCREENSTATE_SETUP        = 0,
            E_INTERNALSCREENSTATE_LOADING      = 1,
            E_INTERNALSCREENSTATE_INITIALISING = 2,
            E_INTERNALSCREENSTATE_RUNNING      = 3,
            E_INTERNALSCREENSTATE_LEAVING      = 4,
            E_RACEINTERNALSTATE_COUNT          = 5,
        };

        // 36 apt text fields -- the width of the name table at .rdata 0x82F26DB0 AND the
        // width the +0x40 .. +0x29E0 member span allows. Both derivations agree.
        static const u32 KU_NUM_STAT_TEXTFIELDS = 36;

        virtual void OnEnter();     // @0x824B5BE0
        virtual void OnLeave();     // @0x824CA8E0
        virtual void Update();      // @0x824D8318

        // @0x82500008 -- hands the stats screen's static resource list to the loader
        // (X360: *r4 = &maResourcesToLoad; *r5 = muNumResourcesToLoad, count = 1).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        bool UpdateInitSetup();                                    // @0x824CA970
        bool UpdateLoading();                                      // @0x824CAAA0
        bool UpdateInitialising();                                 // @0x824B5C60
        void UpdatePermanent();                                    // @0x824C1690
        void HandleStatData(const GuiEventStatsResponse* lpStatsEvent);   // @0x824B5D18
        void HandleTriggers(const CgsModule::Event* lpEvent);      // @0x824B6370
        void SetExpectedAptComponents();                           // @0x824B6408

        static const s32                    maiEventToObserve[4];  // @0x820660CC (.rdata)
        static const s32                    miNumEventsObserved;   // == 4
        static const CgsGui::sResourceTuple maResourcesToLoad[];   // @0x82F26D88 (.rdata)
        static const u32                    muNumResourcesToLoad;  // @0x82F26D90 (.rdata) == 1
        static const char* const            KAPC_STAT_TEXTFIELD_NAMES[KU_NUM_STAT_TEXTFIELDS];

        EInternalScreenState meCurrentState;                       // +0x0038
        GuiCache*            mpGuiCache;                           // +0x003C
        TextField            maStatTextfields[KU_NUM_STAT_TEXTFIELDS];   // +0x0040
        bool                 mbDataReceived;                       // +0x29E0
    };
}
