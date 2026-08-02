// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 07: the store/highlight mirror pair over
// the create-match option lists.
//   StoreCreateGameOptions     @0x82492BD8  (asserts cpp:1684 / cpp:1777)
//   HighlightCreateGameOptions @0x82490740  (asserts cpp:1496 / cpp:1611)
//
// MERGED DROP-IN FORM -- this is the exact file to move to
// b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions_wI_07.cpp once the
// spec's §H1/§H2 header changes land, PLUS the one extra header change §H does not list:
//
//   *** GuiCache::mbOnlineMatchRanked is PRIVATE. ***
//   BrnGuiCache.h exposes only the setter SetOnlineMatchRanked (line 453); there is no
//   getter and OnlineGameOptions is not a friend. Measured -- compiled against a probe
//   copy of the full §H1/§H2 headers this file still fails with
//     error C2248: "BrnGui::GuiCache::mbOnlineMatchRanked": Kein Zugriff auf private Member
//     (note: b5-decomp/src/GameSource/Gui/BrnGuiCache.h(757))
//   Adding the wave-H-style friendship makes the same probe compile clean:
//     // near line 44, beside the existing forward declarations:
//     struct OnlineGameOptions;      // friend of GuiCache (reads mbOnlineMatchRanked by name)
//     // in the private section, beside `friend struct OnlineGameRoomPlayerInfo;`:
//     friend struct OnlineGameOptions;
//   (An accessor `bool IsOnlineMatchRanked() const` would also work, but no DWARF
//   accessor row attests one, so it would be a fabricated surface. Flagging, not choosing.)
//
// The per-function parks with their full unblock banners are
//   scratchpad/waveI/parked/BrnOnlineGameOptions_07_StoreCreateGameOptions.cpp
//   scratchpad/waveI/parked/BrnOnlineGameOptions_07_HighlightCreateGameOptions.cpp
// PROVEN, not assumed: all three files compile clean (0 errors, 0 warnings) under the
// gate's own cl /c command line against a shadow copy of the headers carrying only the
// declarations those banners list -- scratchpad/waveI/probe07/ (gate.bat,
// gate_StoreCreateGameOptions.bat, gate_HighlightCreateGameOptions.bat, all exit 0).
//
// LINK NOTE for the conductor (invisible to cl /c): this partfile deliberately does NOT
// define the static tables it reads -- KAP_GAME_MODE_OPTION_DATA, KAE_RACE_MODE_OPTIONS,
// KAE_ROAD_RAGE_MODE_OPTIONS, KAE_BURNING_HOME_RUN_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS
// -- so the several wave-I partfiles that read them do not collide on the definition
// (spec §H1 puts the definitions in the consolidated .cpp). Callees undefined tree-wide:
// StoreGameMode / GetSelectedGameMode / GetOptionIndex / SetupGameModeOptions (other
// wave-I groups) and GetHighlightedOption (@0x8248EC98, a foreign ledger TU, `reviewed`
// but defined nowhere).
//
// ===================================================================================
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX -- the raw asm in
// .ida-exports/BURNOUT_X360_ARTIST.XEX/0x82492BD8.json and /0x82490740.json arbitrated
// over Hex-Rays throughout.
//
// The pair is one screen contract read in both directions. Store walks the current game
// mode's option-id list, asks the toggle group which row each id is on, reads that row's
// highlighted option and folds it back into mGameOptions. Highlight walks the same list
// and drives each row's highlight from the stored field. They cross-check each other: the
// list each mode picks in Highlight's switch agrees arm for arm with the list
// KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()] yields on the store side.
//
// Notes taken from the asm rather than Hex-Rays:
//  * Both bodies switch on `id - 8` through a 49-entry jump table (`cmplwi 0x30` then
//    `bctr`), so the arm ids below are the table cases + 8. Store's table and Highlight's
//    table have exactly the same populated arms.
//  * Store's list walk is PRE-tested (`lwz r10,0(r30) ; cmpwi 60 ; beq exit` at
//    0x82492C4C, before the first GetIndexFromId) -- an all-terminator list does nothing.
//    Highlight's is POST-tested (the `bne loc_824909B8` at 0x82490C5C is the only back
//    edge; the body is entered unconditionally from every switch arm), so its first entry
//    is always processed. That asymmetry is the console's and is kept -- hence the while
//    loop on one side and the do/while on the other.
//  * Store cases 22 and 23 are ONE arm serving two table slots (0x82492DBC, "jumptable
//    cases 14,15"): BOTH store GetOptionIndex + 1. Highlight reads them back differently
//    (22: miNumRounds - 1; 23: miNumRounds / 2 - 1, the `srawi r11,r11,1 ; addze` signed
//    halve at 0x82490AE4). So the even-rounds list does NOT round-trip through
//    miNumRounds on the console: store writes the row's sequence index + 1 where highlight
//    expects a round count. Reproduced exactly as shipped.
//  * Store's three boolean arms are the `addi -K ; cntlzw ; extrwi 1,26` idiom, i.e.
//    plain equality against the ON row (35 / 38 / 41 at 0x82492DD0 / 0x82492E04 /
//    0x82492E18). Highlight's mirrors are `lbzx ; cmplwi 0` picks between the ON and OFF
//    rows (35/36, 38/39, 41/42).
//  * Highlight's mode switch is a `mode - 10` jump table of 8 entries (`cmplwi 7 ; bgt
//    default` at 0x8249077C), so only modes 10..17 can reach a named arm; slot 6 (mode 16,
//    E_MODE_ONLINE_SHOWTIME) points at the default arm.
//  * Highlight's DEFAULT mode arm does NOT call SetupGameModeOptions -- it only takes the
//    race list and fires the assert (0x82490944..0x82490964). Every named arm calls it.
//    (The understand-phase note that SetupGameModeOptions runs on every arm is wrong
//    here; the jump-table arms settle it.)
//  * GetHighlightedOption's result is consumed as a 32-bit option id (`mr r4,r3` at
//    0x82492CD0 feeds every arm) -- the Hex-Rays `>> 32` is decompiler noise from the
//    u64 register view.
//  * The lpeGameModeOptions assert is non-fatal on the console: the body falls straight
//    through into the ranked store and the list load. CGS_ASSERT behaves the same way.
//
// The X360 member displacements (0x1100 mCreateGameToggles, 0xA278 meGameMode, 0xA284
// meBoostType, 0xA288 meVehicleChoice, 0xA28C miTimeLimit, 0xA290 miNumRounds, 0xA294
// miVehicleClass, 0xA298 miNumRunnerCrashes, 0xA29C..0xA29F the four bools, 0xA500
// mpGuiCache, 0x4B51 GuiCache::mbOnlineMatchRanked) are CONSOLE values and are
// deliberately not reproduced: every access here is by name, so the host's own layout
// applies. No floats in either body, so there is no NaN-polarity decision to make.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache (mbOnlineMatchRanked)

namespace BrnGui
{
    namespace
    {
        // The two game-mode rows BuildGameOptions @0x8248CA98 emits that the wave-I option
        // enum leaves as unnamed gaps (spec §2: values 2 and 6 are not attested by a DWARF
        // enumerator). Their roles are fixed by StoreGameMode @0x82490C68, which maps
        // highlighted row 2 -> mode 11 and row 6 -> mode 13; HighlightCreateGameOptions is
        // that map read backwards. FLAG: role-derived names, not DWARF names.
        const CreateMatchOption::EOption KE_OPTION_GAME_MODE_ROAD_RAGE =
            static_cast<CreateMatchOption::EOption>(2);
        const CreateMatchOption::EOption KE_OPTION_GAME_MODE_BURNING_HOME_RUN =
            static_cast<CreateMatchOption::EOption>(6);
    }

    // @0x82492BD8 -- fold the create-match toggle rows back into mGameOptions.
    //
    // The toggle rows carry the option id as their selectable id, so the list entry IS the
    // lookup key: GetIndexFromId(-1) means this mode's list mentions an option that is not
    // on screen right now (the five-row scroll window), and that entry is skipped.
    void OnlineGameOptions::StoreCreateGameOptions()
    {
        StoreGameMode();

        const CreateMatchOption::EOption* lpeGameModeOptions =
            KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()];
        CGS_ASSERT(lpeGameModeOptions != 0, "lpeGameModeOptions");            // cpp:1684

        // The ranked flag is not a toggle row -- it comes off the cache the online menu
        // set when the player picked ranked or unranked play.
        mGameOptions.mbRanked = mpGuiCache->mbOnlineMatchRanked;

        while (*lpeGameModeOptions != CreateMatchOption::E_OPTION_TERMINATOR)
        {
            const CreateMatchOption::EOption leOption = *lpeGameModeOptions;

            // The console sign-extends the id word into the 64-bit id argument (`extsw`).
            if (mCreateGameToggles.GetIndexFromId(static_cast<u64>(leOption)) != -1)
            {
                const CreateMatchOption::EOption leHighlighted = GetHighlightedOption(leOption);

                switch (leOption)
                {
                    case CreateMatchOption::E_OPTION_VEHICLE_CHOICE:            // 8
                        mGameOptions.meVehicleChoice = GetOptionIndex(leHighlighted);
                        break;

                    case CreateMatchOption::E_OPTION_VEHICLE_CLASS:             // 11
                        mGameOptions.miVehicleClass = GetOptionIndex(leHighlighted);
                        break;

                    // One shared arm for both rounds lists -- see the banner: the even
                    // list stores its sequence index + 1, not the round count.
                    case CreateMatchOption::E_OPTION_ROUNDS:                    // 22
                    case CreateMatchOption::E_OPTION_ROUNDS_EVEN:               // 23
                        mGameOptions.miNumRounds = GetOptionIndex(leHighlighted) + 1;
                        break;

                    case CreateMatchOption::E_OPTION_INFINITE_BOOST:            // 34
                        mGameOptions.mbInfiniteBoost =
                            (leHighlighted == CreateMatchOption::E_OPTION_INFINITE_BOOST_ON);
                        break;

                    case CreateMatchOption::E_OPTION_TRAFFIC:                   // 37
                        mGameOptions.mbTrafficOn =
                            (leHighlighted == CreateMatchOption::E_OPTION_TRAFFIC_ON);
                        break;

                    case CreateMatchOption::E_OPTION_TRAFFIC_CHECKING:          // 40
                        mGameOptions.mbTrafficCheckingOn =
                            (leHighlighted == CreateMatchOption::E_OPTION_TRAFFIC_CHECKING_ON);
                        break;

                    case CreateMatchOption::E_OPTION_BOOST_TYPE:                // 43
                        mGameOptions.meBoostType = GetOptionIndex(leHighlighted);
                        break;

                    case CreateMatchOption::E_OPTION_RUNNER_CRASH_LIMIT:        // 49
                        mGameOptions.miNumRunnerCrashes = GetOptionIndex(leHighlighted) + 1;
                        break;

                    case CreateMatchOption::E_OPTION_TIME_LIMIT:                // 56
                        mGameOptions.miTimeLimit = GetOptionIndex(leHighlighted);
                        break;

                    default:
                        CGS_ASSERT(false, "Unknown game mode option");          // cpp:1777
                        break;
                }
            }

            ++lpeGameModeOptions;
        }
    }

    // @0x82490740 -- drive every visible toggle row from the stored match options.
    //
    // Two halves: the game-mode row itself (which also selects WHICH option list the page
    // shows, and rebuilds the toggle rows for it), then that list's rows.
    void OnlineGameOptions::HighlightCreateGameOptions()
    {
        const s32 liGameModeRow = mCreateGameToggles.GetIndexFromId(
            static_cast<u64>(CreateMatchOption::E_OPTION_GAME_MODE));

        const CreateMatchOption::EOption* lpeGameModeOptions;

        // Mode values are GsmIO::EGameModeType; left as literals with the enumerator name
        // in comment, matching the sibling wave-I partfiles (the recovered enum spells 17
        // E_MODE_ONLINE_MODE_END == E_MODE_COUNT, a sentinel the ARTIST build nonetheless
        // uses as a real online mode).
        switch (mGameOptions.meGameMode)
        {
            case 10:                                    // E_MODE_ONLINE_RACE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOption::E_OPTION_GAME_MODE_RACE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                break;

            case 11:                                    // E_MODE_ONLINE_ROAD_RAGE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow, GetOptionIndex(KE_OPTION_GAME_MODE_ROAD_RAGE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_ROAD_RAGE_MODE_OPTIONS;
                break;

            case 12:                                    // E_MODE_ONLINE_FUGITIVE
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOption::E_OPTION_GAME_MODE_STUNT));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            case 13:                                    // E_MODE_ONLINE_BURNING_HOME_RUN
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow, GetOptionIndex(KE_OPTION_GAME_MODE_BURNING_HOME_RUN));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_BURNING_HOME_RUN_MODE_OPTIONS;
                break;

            case 14:                                    // E_MODE_ONLINE_FREE_BURN
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOption::E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            case 15:                                    // E_MODE_ONLINE_FREE_BURN_LOBBY
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOption::E_OPTION_GAME_MODE_RACE));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                break;

            case 17:                                    // FLAG: DWARF E_MODE_ONLINE_MODE_END
                if (liGameModeRow > -1)
                {
                    mCreateGameToggles.HighlightItem(
                        liGameModeRow,
                        GetOptionIndex(CreateMatchOption::E_OPTION_GAME_MODE_STUNT_COOP));
                }
                SetupGameModeOptions();
                lpeGameModeOptions = KAE_DEFAULT_MODE_OPTIONS;
                break;

            default:                                    // 16 E_MODE_ONLINE_SHOWTIME, ...
                // The list is taken BEFORE the assert fires, and this arm alone skips
                // SetupGameModeOptions -- so the rows below are highlighted against
                // whatever the group already holds.
                lpeGameModeOptions = KAE_RACE_MODE_OPTIONS;
                CGS_ASSERT(false, "Invalid game mode option");                  // cpp:1496
                break;
        }

        // Post-tested walk: the first entry is always processed (see the banner).
        do
        {
            const CreateMatchOption::EOption leOption = *lpeGameModeOptions;
            const s32 liRow = mCreateGameToggles.GetIndexFromId(static_cast<u64>(leOption));

            if (liRow != -1)
            {
                switch (leOption)
                {
                    case CreateMatchOption::E_OPTION_VEHICLE_CHOICE:            // 8
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.meVehicleChoice);
                        break;

                    case CreateMatchOption::E_OPTION_VEHICLE_CLASS:             // 11
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miVehicleClass);
                        break;

                    case CreateMatchOption::E_OPTION_ROUNDS:                    // 22
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miNumRounds - 1);
                        break;

                    case CreateMatchOption::E_OPTION_ROUNDS_EVEN:               // 23
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miNumRounds / 2 - 1);
                        break;

                    case CreateMatchOption::E_OPTION_INFINITE_BOOST:            // 34
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbInfiniteBoost
                                               ? CreateMatchOption::E_OPTION_INFINITE_BOOST_ON
                                               : CreateMatchOption::E_OPTION_INFINITE_BOOST_OFF));
                        break;

                    case CreateMatchOption::E_OPTION_TRAFFIC:                   // 37
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbTrafficOn
                                               ? CreateMatchOption::E_OPTION_TRAFFIC_ON
                                               : CreateMatchOption::E_OPTION_TRAFFIC_OFF));
                        break;

                    case CreateMatchOption::E_OPTION_TRAFFIC_CHECKING:          // 40
                        mCreateGameToggles.HighlightItem(
                            liRow,
                            GetOptionIndex(mGameOptions.mbTrafficCheckingOn
                                               ? CreateMatchOption::E_OPTION_TRAFFIC_CHECKING_ON
                                               : CreateMatchOption::E_OPTION_TRAFFIC_CHECKING_OFF));
                        break;

                    case CreateMatchOption::E_OPTION_BOOST_TYPE:                // 43
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.meBoostType);
                        break;

                    case CreateMatchOption::E_OPTION_RUNNER_CRASH_LIMIT:        // 49
                        mCreateGameToggles.HighlightItem(liRow,
                                                         mGameOptions.miNumRunnerCrashes - 1);
                        break;

                    case CreateMatchOption::E_OPTION_TIME_LIMIT:                // 56
                        mCreateGameToggles.HighlightItem(liRow, mGameOptions.miTimeLimit);
                        break;

                    default:
                        CGS_ASSERT(false, "Unknown game mode option");          // cpp:1611
                        break;
                }
            }

            ++lpeGameModeOptions;
        }
        while (*lpeGameModeOptions != CreateMatchOption::E_OPTION_TERMINATOR);
    }
}
