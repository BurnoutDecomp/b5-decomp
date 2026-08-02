#pragma once

// ===================================================================================
// BrnGui::CreateMatchOption  -- owning header for the create-match option array element
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnCreateMatchOption.h
//
// BrnGui::OnlineGameOptions builds an array of up to 75 create-match option entries. The
// container is the committed generic Array<T,N> (CgsArray.h) instantiated as
// Array<CreateMatchOption, 75>:
//   * the element stride is 8 bytes (X360 Append @0x82488B40 does `slwi r11, count, 3`
//     then copies TWO words: `stw [+0]`, `stw [+4]`),
//   * the live-element count word sits at this+0x258 == 8 * 75, i.e. immediately after the
//     75-element buffer (the committed Array<T,N> shape: elements then count),
//   * GetItem @0x82488DC8 returns `8*index + this`, i.e. &maElements[index].
//
// The 8-byte stride above is the CONSOLE stride: on the X360 both fields are 32-bit, so an
// entry is two words. The first word is a string POINTER, which widens on the x64 host, so
// the host entry is larger. That is fine and intended -- every access here is BY NAME and
// the Array<T,N> instantiation is sizeof-driven. Never reuse the console 8 for host
// arithmetic.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    // One create-match option entry: the display string and the option it selects.
    // DWARF BrnOnlineGameOptions.h:135/136 names the members mpcName/meOption, and
    // OnlineGameOptions::BuildGameOptions @0x8248CA98 appends {string-literal, enum} pairs.
    struct CreateMatchOption
    {
        // DWARF BrnOnlineGameOptions.h:55 (the DWARF spells the type name plural,
        // CreateMatchOptions; this home keeps the committed singular). These are the ARTIST
        // build's values: the Dec-07 DWARF enum runs one lower from VEHICLE_CHOICE upward
        // (merge-window drift). Decoded from BuildGameOptions @0x8248CA98 together with the
        // KAE_* option lists; the unattested slots are deliberately left unnamed rather
        // than invented.
        enum EOption
        {
            E_OPTION_GAME_MODE            = 0,
            E_OPTION_GAME_MODE_RACE       = 1,
            E_OPTION_GAME_MODE_STUNT      = 3,   // dev-flag-gated rows 3..5
            E_OPTION_GAME_MODE_STUNT_FREE_FOR_ALL = 4,
            E_OPTION_GAME_MODE_STUNT_COOP = 5,
            E_OPTION_VEHICLE_CHOICE       = 8,
            E_OPTION_VEHICLE_CHOICE_FREE  = 9,
            E_OPTION_VEHICLE_CHOICE_HOST  = 10,
            E_OPTION_VEHICLE_CLASS        = 11,  // class rows 12..17
            E_OPTION_ROUNDS               = 22,  // rounds rows 24..33 (ROUNDS_n == n + 23)
            E_OPTION_ROUNDS_EVEN          = 23,
            E_OPTION_INFINITE_BOOST       = 34,
            E_OPTION_INFINITE_BOOST_ON    = 35,
            E_OPTION_INFINITE_BOOST_OFF   = 36,
            E_OPTION_TRAFFIC              = 37,
            E_OPTION_TRAFFIC_ON           = 38,
            E_OPTION_TRAFFIC_OFF          = 39,
            E_OPTION_TRAFFIC_CHECKING     = 40,
            E_OPTION_TRAFFIC_CHECKING_ON  = 41,
            E_OPTION_TRAFFIC_CHECKING_OFF = 42,
            E_OPTION_BOOST_TYPE           = 43,  // boost rows 44..47
            E_OPTION_RUNNER_CRASH_LIMIT   = 49,  // rows 50..54
            E_OPTION_RUNNER_CRASH_LIMIT_NEVER = 55,
            E_OPTION_TIME_LIMIT           = 56,  // rows 57..59
            E_OPTION_TERMINATOR           = 60,  // every option-list walk stops here
            E_OPTION_COUNT                = 61,
        };

        const char* mpcName;   // console +0x00 (a 32-bit pointer there; widens on x64)
        EOption     meOption;  // console +0x04
    };
}
