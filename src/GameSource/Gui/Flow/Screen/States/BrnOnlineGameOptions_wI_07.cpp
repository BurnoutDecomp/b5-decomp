// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 07: the store/highlight mirror pair over
// the create-match option lists.
//   StoreCreateGameOptions     @0x82492BD8  (asserts cpp:1684 / cpp:1777)
//   HighlightCreateGameOptions @0x82490740  (asserts cpp:1496 / cpp:1611)
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
