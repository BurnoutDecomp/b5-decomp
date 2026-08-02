// ===================================================================================
// BrnGui::OnlineGameOptions -- wave-I partfile 04: the create-game toggle builders.
//   SetupOptions                 @0x8248EB60  (the assert it fires is cpp:1387)
//   SetupCommonCreateGameOptions @0x82490598
//   SetupGameModeOptions         @0x82490630
//
//
// The committed leaf header BrnOnlineGameOptions.h is still the MINIMAL pre-wave version
// (the GetResourcesToLoad inline plus the two resource statics), and BrnCreateMatchOption.h
// still carries the placeholder `{ u32 muWord0; u32 muWord1; }` element with no EOption
// enum. The wave-I spec's §H1 class extension and §H2 element fix had not been applied when
// this partfile was written, and headers are frozen for implementers, so none of the three
// bodies can name maOptions / mpCommonOptions / miStartItem / mCreateGameToggles /
// KAP_GAME_MODE_OPTION_DATA / KI_MAX_CREATE_GAME_OPTIONS / CreateMatchOption::EOption.
// Measured with the compile gate, not assumed.
//
// The three complete bodies live at, each with a banner naming the exact declaration lines
// that unblock it:
// They concatenate into this file (single `namespace BrnGui { ... }`; the union of their
// include sets is BrnOnlineGameOptions.h + CgsAssert.h, and the two anonymous-namespace
// constants KI_MAX_TOGGLE_OPTIONS / KAC_EMPTY_STRING merge into one block) once §H1/§H2
// land. All three were VERIFIED to compile clean against §H1/§H2 applied to a scratch
// overlay copy of the two headers -- the real headers were not touched.
//
// ⭐ SPEC CORRECTION for the conductor -- wave-I spec trap 1 is OVERTURNED by the asm.
// The spec predicted that SetupOptions builds each 64-bit toggle id as a console pack of
// {name pointer << 32 | option} and that every consumer reads only the low word, so the
// host build should drop the pointer half. There is no pack to drop. The asm at
// 0x8248EBF8..0x8248EC14 is
//     lwz r10, 0(r3) ; extsw r11, r28 ; stw r10, 0(r26) ; std r11, 0(r27)
// -- the row's mpcName (r10) goes ONLY into the parallel const char* array, and the u64 id
// (r11) is the row's meOption sign-extended, nothing more. Hex-Rays fused the independent
// r10/r11 pair into one doubleword view (`HIDWORD(v16) = *Item; LODWORD(v16) = v12;`) and
// the spec inherited that reading. The same `extsw` shapes the SetSelectableId argument at
// 0x8248EC7C. No host-pointer-width accommodation is needed anywhere in this group.
//
// ⭐ SPEC TRAP 10 CONFIRMED. The blank-row SetupToggle argument order was read off the asm
// at 0x8249070C..0x82490728 rather than reasoned:
//     li r9,0 ; li r8,0 ; mr r7,""(unk_820046A7) ; li r6,1 ; li r5,0 ; mr r4,row
// == (liIndex = row, liNumOptions = 0, lbActive = 1, lpacText = "", lppacOptions = 0,
// lpu64Ids = 0) -- the corrected roles in BrnMenuToggleGroup.h, matching SetupOptions'
// populated call. The spec's prediction holds.
//
// LINK NOTE for the conductor: SetupGameModeOptions READS the static member
// KAP_GAME_MODE_OPTION_DATA. Its out-of-line DEFINITION is deliberately not written here --
// every wave-I implementer writes a different partfile and two definitions would collide.
// The same note is already on BrnOnlineGameOptions_wI_01.cpp.
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82490598.json, asm arbitrated over Hex-Rays).
//
// The five-row create-game toggle window is a scroll view over TWO option-id lists laid
// end to end: the common list (mpCommonOptions == KAE_COMMON_OPTIONS) followed by the
// current mode's list. miStartItem is the index of the first visible entry across that
// concatenation, so this half maps common entry i onto toggle row (i - miStartItem) and
// simply produces nothing once the window has scrolled past the common list entirely.
//
// HEX-RAYS MISREADING, arbitrated from the assembly: the call renders with one argument
// missing (`SetupOptions(v1, v6 - *v4)`). The asm loads r5 from the list before every `bl`
// (`lwzx r5, r30, r9` @0x824905F4 seeding the entry test, `lwzx r5, r11, r30` @0x8249061C
// re-seeding it in the loop tail), so the third argument is the option id being laid onto
// that row -- the very word the loop's terminator test just examined.
//
// X360-LITERAL AUDIT: the only console displacements this body touches are the two member
// bases (this+42236 = mpCommonOptions via `addis r28,r29,1 ; addi r28,r28,-0x5B04`, and
// this+42252 = miStartItem via `addi r27,r27,-0x5AF4`) and the option-list element stride 4
// (`slwi r30, r31, 2`), which here is the host's own `sizeof(CreateMatchOption::EOption)`
// array step. Both members are reached BY NAME, so no console number is reproduced as code.
// No floating-point comparison occurs, so there is no NaN-polarity hazard.
    // ================================================================================
    // SetupCommonCreateGameOptions @ 0x82490598
    //
    // Lay the visible part of the common option list onto the top of the toggle window.
    // ================================================================================
    void OnlineGameOptions::SetupCommonCreateGameOptions()
    {
        // Live length of the common list (walked to its terminator). The console peels the
        // first entry's test out of the loop; kept as written so an already-empty list
        // counts zero rather than walking off the front.
        s32 liNumCommonOptions = 0;
        if (mpCommonOptions[0] != CreateMatchOption::E_OPTION_TERMINATOR)
        {
            const CreateMatchOption::EOption* lpeCommonOption = mpCommonOptions;
            do
            {
                ++lpeCommonOption;
                ++liNumCommonOptions;
            }
            while (*lpeCommonOption != CreateMatchOption::E_OPTION_TERMINATOR);
        }

        // Nothing of the common list is on screen once the window has scrolled past it.
        if (miStartItem < liNumCommonOptions)
        {
            // Entry i lands on toggle row (i - miStartItem). The console re-reads
            // miStartItem from the object on every iteration (`lwz r11, 0(r27)`); it does
            // not change, so the subtraction is written directly.
            s32 liOptionIndex = miStartItem;
            while (mpCommonOptions[liOptionIndex] != CreateMatchOption::E_OPTION_TERMINATOR)
            {
                SetupOptions(liOptionIndex - miStartItem, mpCommonOptions[liOptionIndex]);
                ++liOptionIndex;
            }
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82490630.json, asm arbitrated over Hex-Rays).
//
// The mode half of the same scroll window SetupCommonCreateGameOptions fills the top of.
// The two option-id lists are laid end to end (common list, then
// KAP_GAME_MODE_OPTION_DATA[GetSelectedGameMode()]), and miStartItem is the index of the
// first visible entry across the concatenation. So either the window still shows part of
// the common list -- the mode list then starts at its own entry 0, on the row the common
// list stopped at -- or the window has scrolled past the common list entirely and row 0 is
// already a mode entry. `cmpw cr6, r9, r11 ; ble` @0x82490680 puts the equality case in the
// FIRST branch (miStartItem == liNumCommonOptions -> entry 0 on row 0, which the second
// branch would also produce).
//
// HEX-RAYS MISREADING, arbitrated from the assembly: the SetupOptions call renders with one
// argument missing (`SetupOptions(a1, v11)`). The asm loads r5 from the list before each
// `bl` (`lwzx r5, r11, r30` @0x824906B4 and @0x824906E8), so the third argument is the
// option id being laid onto that row.
//
// TRAP-10 VERIFICATION (the blank-row SetupToggle argument order), read off the asm at
// 0x8249070C..0x82490728 rather than reasoned:
//     li r9,0 ; li r8,0 ; mr r7,r30("") ; li r6,1 ; li r5,0 ; mr r4,r31(row) ; mr r3,r29
// i.e. (liIndex = row, liNumOptions = 0, lbActive = 1, lpacText = "", lppacOptions = 0,
// lpu64Ids = 0) -- exactly the corrected parameter roles in BrnMenuToggleGroup.h, and the
// same order SetupOptions' populated call uses. The prediction in spec trap 10 holds.
//
// X360-LITERAL AUDIT: the console displacements this body touches are member bases
// (this+42236 = mpCommonOptions via `ori r10,r11,0xA4FC ; lwzx r10,r26,r10`, this+42252 =
// miStartItem via `ori r10,r10,0xA50C`, this+4352 = mCreateGameToggles via
// `addi r29,r26,0x1100`) and the two element strides 4 (`slwi r27,r3,2` over the pointer
// table, `slwi r30,r10,2` over the id list), which are the host's own array steps here.
// Every member and the pointer table are reached BY NAME, so no console number is
// reproduced as code. The window height is the class's KI_MAX_CREATE_GAME_OPTIONS, not the
// bare 5 the console compares against (`cmpwi cr6, r31, 5`). No floating-point comparison
// occurs, so there is no NaN-polarity hazard.
    namespace
    {
        // unk_820046A7 -- the canonical empty string the blank-row SetupToggle passes as
        // its title text.
        const char KAC_EMPTY_STRING[] = "";
    }

    // ================================================================================
    // SetupGameModeOptions @ 0x82490630
    //
    // Lay the visible part of the current game mode's option list onto the rows the common
    // list left over, then blank whatever is still empty.
    // ================================================================================
    void OnlineGameOptions::SetupGameModeOptions()
    {
        const s32 liGameMode = GetSelectedGameMode();

        // The mode list is displayed immediately after the common list, so where it starts
        // in the window depends on the common list's length.
        s32 liNumCommonOptions = 0;
        for (const CreateMatchOption::EOption* lpeCommonOption = mpCommonOptions;
             *lpeCommonOption != CreateMatchOption::E_OPTION_TERMINATOR;
             ++lpeCommonOption)
        {
            ++liNumCommonOptions;
        }

        s32 liOptionIndex;   // first mode entry the window shows
        s32 liStartRow;      // toggle row that entry lands on
        if (miStartItem <= liNumCommonOptions)
        {
            // The window still shows some of the common list (or starts exactly where the
            // mode list does), so the mode list begins at its own first entry.
            liOptionIndex = 0;
            liStartRow    = liNumCommonOptions - miStartItem;
        }
        else
        {
            // The window has scrolled into the mode list; row 0 is already a mode entry.
            liOptionIndex = miStartItem - liNumCommonOptions;
            liStartRow    = 0;
        }

        const CreateMatchOption::EOption* lpeGameModeOptions =
            KAP_GAME_MODE_OPTION_DATA[liGameMode];

        s32 liNumEmitted = 0;
        if (lpeGameModeOptions[liOptionIndex] != CreateMatchOption::E_OPTION_TERMINATOR)
        {
            s32 liRow = liStartRow;
            do
            {
                // The window is only five rows tall; the console breaks out before the call
                // rather than before the terminator test, so liNumEmitted stops here too.
                if (liRow >= KI_MAX_CREATE_GAME_OPTIONS)
                {
                    break;
                }

                SetupOptions(liRow, lpeGameModeOptions[liOptionIndex]);

                ++liOptionIndex;
                ++liNumEmitted;
                ++liRow;
            }
            while (lpeGameModeOptions[liOptionIndex] != CreateMatchOption::E_OPTION_TERMINATOR);
        }

        // Blank every row the two lists did not reach: an empty toggle with no options at
        // all. The console recomputes the first blank row as (emitted + start row) rather
        // than reusing the loop cursor; the two agree because the cursor advanced in step.
        for (s32 liEmptyRow = liNumEmitted + liStartRow;
             liEmptyRow < KI_MAX_CREATE_GAME_OPTIONS;
             ++liEmptyRow)
        {
            mCreateGameToggles.SetupToggle(liEmptyRow, 0, true, KAC_EMPTY_STRING, 0, 0);
        }
    }

//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX
// (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x8248EB60.json, asm arbitrated over Hex-Rays).
//
// SetupOptions turns ONE option-group id into one populated row of the five-row create-game
// toggle window. maOptions is the {name, option} table the foreign BuildGameOptions
// @0x8248CA98 fills; a group is a title row whose meOption IS the group id, followed by its
// value rows, ended by an E_OPTION_TERMINATOR row.
//
// ⭐ HEX-RAYS MISREADING, arbitrated from the assembly -- and it overturns the wave-I spec's
// trap 1. The pseudocode renders each 64-bit toggle id as a pack of the row's name pointer
// into the high word and the option into the low word:
//     HIDWORD(v16) = *Item; LODWORD(v16) = v12;
// and the spec predicted a console pointer pack whose consumers read only the low word. The
// asm @0x8248EBF8..0x8248EC14 is:
//     lwz r10, 0(r3)      ; r10 = the row's mpcName
//     extsw r11, r28      ; r11 = the row's meOption, SIGN-EXTENDED to 64 bits
//     stw r10, 0(r26)     ; -> the const char* array  (r26 += 4)
//     std r11, 0(r27)     ; -> the u64 id array       (r27 += 8)
// The name goes ONLY into the parallel text array; the id IS the option value and nothing
// else. There is no pack, so no host-pointer-width accommodation is needed at all. Hex-Rays
// fused the independent r10/r11 pair into one doubleword view. The same `extsw` appears on
// the SetSelectableId argument @0x8248EC7C.
//
// X360-LITERAL AUDIT: the only console displacements this body touches are member bases
// (this+41632 = maOptions via `addis r29,r20,1 ; addi r29,r29,-0x5D60`, this+4352 =
// mCreateGameToggles via `addi r30,r20,0x1100`) and the two stack-buffer strides 4 and 8,
// which are the host's own `sizeof(const char*)` / `sizeof(u64)` array steps here. Both
// members are reached BY NAME, so no console number is reproduced as code. maOptions'
// element stride never appears -- Array<T,N>::GetItem does the host arithmetic. No
// floating-point comparison occurs, so there is no NaN-polarity hazard.
    namespace
    {
        // The stack option-name / option-id buffers this body builds are 1024 bytes and
        // 2048 bytes on the console (`[sp+50h]` 256 words, `[sp+450h]` 256 doublewords), and
        // the guard it fires is `cmpwi cr6, r31, 0x100`.
        const s32 KI_MAX_TOGGLE_OPTIONS = 256;
    }

    // ================================================================================
    // SetupOptions @ 0x8248EB60
    //
    // Populate toggle row liToggleIndex from the option group headed by leOption: the
    // group's title row in maOptions is the entry whose meOption == leOption, and the rows
    // that follow it up to the next terminator are the group's selectable values.
    // ================================================================================
    void OnlineGameOptions::SetupOptions(s32 liToggleIndex, CreateMatchOption::EOption leOption)
    {
        const char* lapcOptionNames[KI_MAX_TOGGLE_OPTIONS];
        u64         lau64OptionIds[KI_MAX_TOGGLE_OPTIONS];

        // Locate the title row. The console runs this scan with no terminator guard and no
        // assert -- BuildGameOptions always emits a title row for every id that can reach
        // here, so the search is guaranteed to hit.
        s32 liTitleIndex = -1;
        do
        {
            ++liTitleIndex;
        }
        while (maOptions.GetItem(static_cast<u32>(liTitleIndex)).meOption != leOption);

        // The toggle's label is the title row's own name string (`lwz r23, 0(r11)`).
        const char* lpcTitleName = maOptions.GetItem(static_cast<u32>(liTitleIndex)).mpcName;

        s32 liRowIndex   = liTitleIndex + 1;
        s32 liNumOptions = 0;

        CreateMatchOption::EOption leRowOption =
            maOptions.GetItem(static_cast<u32>(liRowIndex)).meOption;

        if (leRowOption != CreateMatchOption::E_OPTION_TERMINATOR)
        {
            do
            {
                const CreateMatchOption& lrRow = maOptions.GetItem(static_cast<u32>(liRowIndex));

                lapcOptionNames[liNumOptions] = lrRow.mpcName;
                // `extsw r11, r28 ; std r11, 0(r27)` -- the id is the row's option value,
                // sign-extended to 64 bits (see the banner note).
                lau64OptionIds[liNumOptions] = static_cast<u64>(static_cast<s64>(leRowOption));
                ++liNumOptions;

                // The console tests AFTER the pair of stores, so the guard is one row late.
                // It cannot be reached in practice -- maOptions holds at most 75 entries in
                // total -- and it is kept where the console put it.
                CGS_ASSERT(liNumOptions <= KI_MAX_TOGGLE_OPTIONS,
                           "Too many options specified");   // cpp:1387

                ++liRowIndex;
                leRowOption = maOptions.GetItem(static_cast<u32>(liRowIndex)).meOption;
            }
            while (leRowOption != CreateMatchOption::E_OPTION_TERMINATOR);

            if (liNumOptions > 0)
            {
                // r3 = this + 0x1100 (mCreateGameToggles); r4 = index, r5 = count, r6 = 1,
                // r7 = title text, r8 = the name array, r9 = the id array -- the corrected
                // parameter roles in BrnMenuToggleGroup.h.
                mCreateGameToggles.SetupToggle(liToggleIndex, liNumOptions, true, lpcTitleName,
                                               lapcOptionNames, lau64OptionIds);

                // `extsw r5, r22` -- the row's own selectable id is the group id.
                mCreateGameToggles.SetSelectableId(liToggleIndex,
                                                   static_cast<u64>(static_cast<s64>(leOption)));
            }
        }
    }
}
