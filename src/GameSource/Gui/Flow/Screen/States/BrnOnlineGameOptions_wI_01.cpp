// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 01: the option-math leaves.
//   GetSelectedGameMode @0x82485A88
//   GetNumberOptions    @0x82485B08
//   GetOptionIndex      @0x8248ED50
//
//
// The committed leaf header BrnOnlineGameOptions.h is still the MINIMAL pre-wave version
// (the GetResourcesToLoad inline plus the two resource statics), and BrnCreateMatchOption.h
// still carries the placeholder `{ u32 muWord0; u32 muWord1; }` element with no EOption
// enum. The wave-I spec's §H1 class extension and §H2 element fix had not been applied when
// this partfile was written, and headers are frozen for implementers, so none of the three
// bodies can name mGameOptions / maOptions / mpCommonOptions / KAP_GAME_MODE_OPTION_DATA /
// CreateMatchOption::EOption. Measured with the compile gate, not assumed.
//
// The three complete bodies live at, each with a banner naming the exact declaration lines
// that unblock it:
// They concatenate into this file (single `namespace BrnGui { ... }`, include set =
// BrnOnlineGameOptions.h + BrnCreateMatchOption.h) once §H1/§H2 land.
//
// LINK NOTE for the conductor: KAP_GAME_MODE_OPTION_DATA is a static member that
// GetNumberOptions reads. Its out-of-line DEFINITION is deliberately not written here --
// every wave-I implementer writes a different partfile and two definitions would collide.
// Dumped values (scratchpad/waveI/ogo_rodata.txt lines 99..104):
//   @0x82F26824 = { KAE_RACE_MODE_OPTIONS, KAE_ROAD_RAGE_MODE_OPTIONS,
//                   KAE_BURNING_HOME_RUN_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS,
//                   KAE_DEFAULT_MODE_OPTIONS, KAE_DEFAULT_MODE_OPTIONS }
// which cross-checks exactly against GetSelectedGameMode's mode -> slot mapping.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h"   // CreateMatchOption::EOption

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82485B08.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The GAME-MODE list is counted first (r8, 0x82485B18 `li r8, 0`) and the COMMON list
//    second (r10), and the return is `common + mode` (0x82485B80 `add r3, r10, r8`).
//  * Both loops are terminator-driven -- they stop on E_OPTION_TERMINATOR(60)
//    (`cmpwi cr6, r10, 0x3C`), never on a count -- and both are peeled do-while shapes
//    (the first element is tested before the counter moves), so an empty list contributes
//    0. Kept as two separate loops, matching the two the X360 emits.
//  * `lwzx r11, r9, 0xA4FC` == mpCommonOptions at CONSOLE offset 42236; not reproduced --
//    the member is reached by name so the host's own layout applies. Likewise the list
//    walk advances by `addi r11, r11, 4` == the console's 4-byte enum stride; on the host
//    the pointer increment is the enum's own sizeof, which is what `++lpe...` gives.
//  * No floats, so there is no NaN-polarity decision to make.
    // @0x82485B08 -- the total number of option groups the create-match page can scroll
    // through: the selected mode's list plus the always-present common list, each counted
    // up to its E_OPTION_TERMINATOR.
    s32 OnlineGameOptions::GetNumberOptions()
    {
        s32 liNumGameModeOptions = 0;
        for (const CreateMatchOption::EOption* lpeGameModeOption
                 = KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()];
             *lpeGameModeOption != CreateMatchOption::E_OPTION_TERMINATOR;
             ++lpeGameModeOption)
        {
            ++liNumGameModeOptions;
        }

        s32 liNumCommonOptions = 0;
        for (const CreateMatchOption::EOption* lpeCommonOption = mpCommonOptions;
             *lpeCommonOption != CreateMatchOption::E_OPTION_TERMINATOR;
             ++lpeCommonOption)
        {
            ++liNumCommonOptions;
        }

        return liNumCommonOptions + liNumGameModeOptions;
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248ED50.json, asm arbitrated over Hex-Rays).
//
// Notes taken from the asm rather than the pseudocode:
//  * The array base is built as `addis r30, r3, 1` / `addi r30, r30, -0x5D60`, i.e.
//    this + 65536 - 23904 == this + 41632 -- the CONSOLE offset of maOptions. It is
//    deliberately NOT reproduced: the member is reached by name so the host's own layout
//    applies. Likewise the element's option word at `4(r3)` is reached as `.meOption`,
//    and the 8-byte element stride inside GetItem is the container's own sizeof.
//  * The backward scan is guarded `i >= 0` BEFORE each fetch (0x8248EDA4 `cmpwi cr6, r31,
//    0` / `blt`, and 0x8248EDC8 / `bge` at the bottom), so index -1 is never handed to
//    GetItem. When no terminator precedes the row the scan falls out at i == -1 and the
//    result is j + 1 - 2, exactly what the X360 returns.
//  * The forward scan has NO terminator guard on the console -- the list layout
//    guarantees a hit and the X360 runs off the end otherwise. Kept as-is; the committed
//    Array<T,N>::GetItem does bounds-check against the live count, so on the host an
//    absent option fires the container's own "Array index out of bounds" assert instead
//    of walking into the count word. That is the tree's container contract, not an
//    invented guard.
//  * Both comparisons are integer (`cmpw` / `cmpwi ... 0x3C`); no floats in this
//    function, so there is no NaN-polarity decision to make.
    // @0x8248ED50 -- the position of leOption within its own option group. maOptions is
    // the flat {name, option} row list BuildGameOptions produced, laid out as repeated
    // groups of "terminator, group title, value rows...". So the row's index inside its
    // group is (its own index) - (the preceding terminator's index) - 2: the -1 steps off
    // the terminator and the second -1 steps off the group's title row.
    s32 OnlineGameOptions::GetOptionIndex(CreateMatchOption::EOption leOption)
    {
        s32 liIndex = 0;
        while (maOptions.GetItem(static_cast<u32>(liIndex)).meOption != leOption)
        {
            ++liIndex;
        }

        const s32 liOptionRow = liIndex;
        for (; liIndex >= 0; --liIndex)
        {
            if (maOptions.GetItem(static_cast<u32>(liIndex)).meOption
                    == CreateMatchOption::E_OPTION_TERMINATOR)
            {
                break;
            }
        }

        return liOptionRow - liIndex - 2;
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82485A88.json, asm arbitrated over Hex-Rays).
//
// The X360 body is a `value - 10` jump table over eight cases:
//     lwzx r11, r3, 0xA278 ; addi r11, r11, -0xA ; cmplwi cr6, r11, 7 ; bgt -> default
// so only mode values 10..17 can reach a table arm, and table entries 0 (mode 10),
// 5 (mode 15) and 6 (mode 16) point back at the default arm. Written as the source-level
// switch over the mode values, which lowers to exactly that table.
//
// 0xA278 == 41592 is the CONSOLE offset of mGameOptions.meGameMode and is deliberately
// NOT reproduced: the field is reached by name so the host's own layout applies.
// No floats in this function, so there is no NaN-polarity decision to make.
    // @0x82485A88 -- which KAP_GAME_MODE_OPTION_DATA list the edited match's mode uses.
    // The mapping is deliberately NOT the identity. MEASURED slot contents (the six
    // pointers dumped at 0x82F26824, cross-checked against the run lengths in rodata and
    // the DWARF array bounds):
    //   0 = KAE_RACE_MODE_OPTIONS            @0x8205F154, 5 entries {22, 8, 11, 37, 60}
    //   1 = KAE_ROAD_RAGE_MODE_OPTIONS       @0x8205F168, 6 entries
    //   2 = KAE_BURNING_HOME_RUN_MODE_OPTIONS@0x8205F180, 8 entries
    //   3, 4 and 5 all hold the SAME pointer @0x8205F1A0 -- one shared 5-entry fallback
    //     list (KAE_DEFAULT_MODE_OPTIONS), not three distinct per-mode lists.
    // So the arms below read: modes 10/15/16 -> the race list, 11 road rage -> the road-rage
    // list, 13 burning home run -> its own list, and 12 fugitive / 14 free burn / 17 -> the
    // shared fallback. The mode values are left as literals with their Dec-07 DWARF
    // enumerator names in comment: the
    // GsmIO::EGameModeType enum spells 17 as E_MODE_ONLINE_MODE_END == E_MODE_COUNT (a
    // sentinel, not a mode), so naming that arm would assert a mode the recovered enum
    // does not have. FLAG: the ARTIST build clearly treats 17 as a real online mode.
    s32 OnlineGameOptions::GetSelectedGameMode() const
    {
        switch (mGameOptions.meGameMode)
        {
            case 11:                     // E_MODE_ONLINE_ROAD_RAGE
                return 1;

            case 12:                     // E_MODE_ONLINE_FUGITIVE
                return 3;

            case 13:                     // E_MODE_ONLINE_BURNING_HOME_RUN
                return 2;

            case 14:                     // E_MODE_ONLINE_FREE_BURN
                return 4;

            case 17:                     // FLAG: DWARF E_MODE_ONLINE_MODE_END / E_MODE_COUNT
                return 5;

            default:                     // 10 race, 15 free-burn lobby, 16 showtime, ...
                return 0;
        }
    }
}
