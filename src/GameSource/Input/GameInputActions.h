#pragma once

// ===================================================================================
// EGameInputActions -- owning header
//   b5-decomp/src/GameSource/Input/GameInputActions.h
//   DWARF home: references/DecFIGS/dwarfdump/GameSource/Input/GameInputActions.h:24
//
// THE action vocabulary of the whole input path. One id per logical control; the id IS
// the index into CgsInput::InputIO::PadOutputInformation::maActionInfo[] (8-byte
// {mfValue, muStatus} records at pad+0x18 on the X360 -- so action N's status word is at
// pad + 0x18 + 8*N + 4, e.g. action 45 -> +0x184).
//
// PRODUCERS (host control -> action id):
//   * console: CgsInput::InputPads::FillRawData @0x828E7350 walks gaDefaultGameInputMapping
//     (ActionMapping[34], DWARF GameSource/Input/DefaultGameInputMapping.h:26). ⚠️ That
//     table is NOT recovered -- no IDA export for the data symbol and none for
//     InputPads::Update @0x828F8690 -- so the console's control->action map is unattested.
//   * PC: CgsInput::InputPadsPC::UpdatePlayer0's KA_BINDINGS table
//     (GameShared/GameClasses/System/Input/PC/CgsInputPadsPC.cpp) is the one PC stand-in.
//
// CONSUMERS (action id -> game/GUI):
//   * driving: BrnGame::BrnGameModule::BridgeControllerToWorld @0x823CD890 reads actions
//     0/1/2/3/5/7/8/10/13 and 54/55 into BrnWorld::PlayerVehicleControls.
//   * GUI: BrnGame::BrnGameModule::BridgeControllerToGui @0x823E6B18 turns a fixed set of
//     ids into GuiEventControllerInput{Down,Pressed,Released} wire events. Only the ids in
//     its two recovered scan tables can ever reach a GUI screen:
//       gaiGuiActionIndexTable   (rodata @0x820352F0, 16 dwords)
//           { 49, 46, 50, 51, 52, 53, 54, 55, 56, 57, 45, 47, 48, 21, 59, 58 }
//       gaiGuiMenuAxisActionTable (rodata @0x82035330, 8 dwords, 0.8s delay / 0.1s repeat)
//           { 37, 38, 39, 40, 41, 42, 43, 44 }
//     -- i.e. the GUI-visible set is { 21, 37..59 }. Anything else is invisible to the GUI
//     no matter what the pad does.
//
// ⚠️ THE THREE NAMES THAT KEPT BEING GUESSED WRONG, stated once, here, from the DWARF:
//     45 GUI_START  is the START button (pause / press-start / driver details) -- NOT accept.
//     49 GUI_SELECT is accept (the A button).
//     50 GUI_CANCEL is back (the B button).
//   and the direction pair, settled against every real consumer (CrashNavOptions
//   @0x824DE418, CrashNavDriverDetails @0x824CF3B8, CrashNavProfile @0x824B7BB0,
//   CarSelectLivery @0x824D6D10, OnlineGameRoomPlayerInfo, BootLegal @0x82473978):
//     41 GUI_UP   == HighlightPrevious   42 GUI_DOWN == HighlightNext
//   The dpad family 37..40 is DISTINCT from the generic-nav family 41..44 -- proof:
//   BrnGui::CrashNavPanel::HandleControllerInput @0x824408E0, embedded inside the map,
//   reads ONLY 37..40 while the map's legend reads 43/44 in the same frame.
//
// The enum is at global scope in the DWARF (no namespace), so it is at global scope here.
// ===================================================================================

enum EGameInputActions
{
    // ---- driving (BridgeControllerToWorld) ----
    E_GAMEINPUTACTIONS_ACCELERATE                 = 0,
    E_GAMEINPUTACTIONS_BRAKE                      = 1,
    E_GAMEINPUTACTIONS_HANDBRAKE                  = 2,
    E_GAMEINPUTACTIONS_BOOST                      = 3,
    E_GAMEINPUTACTIONS_CRASHBREAKER               = 4,
    E_GAMEINPUTACTIONS_CHANGEVIEW                 = 5,
    E_GAMEINPUTACTIONS_LOOKBACK                   = 6,
    E_GAMEINPUTACTIONS_RESET                      = 7,
    E_GAMEINPUTACTIONS_START                      = 8,
    E_GAMEINPUTACTIONS_POWERSWERVE_L              = 9,
    E_GAMEINPUTACTIONS_POWERSWERVE_R              = 10,
    E_GAMEINPUTACTIONS_DIRTY_TRICK                = 11,
    E_GAMEINPUTACTIONS_SCREENSHOT                 = 12,
    E_GAMEINPUTACTIONS_HORN                       = 13,

    // ---- debug pad (only 21 DEBUG_RIGHT_BUTTON appears in the GUI scan table) ----
    E_GAMEINPUTACTIONS_DEBUG_UP_PAD               = 14,
    E_GAMEINPUTACTIONS_DEBUG_DOWN_PAD             = 15,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_PAD             = 16,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_PAD            = 17,
    E_GAMEINPUTACTIONS_DEBUG_UP_BUTTON            = 18,
    E_GAMEINPUTACTIONS_DEBUG_DOWN_BUTTON          = 19,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_BUTTON          = 20,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_BUTTON         = 21,
    E_GAMEINPUTACTIONS_DEBUG_LOWER_TRIGGER_LEFT   = 22,
    E_GAMEINPUTACTIONS_DEBUG_LOWER_TRIGGER_RIGHT  = 23,
    E_GAMEINPUTACTIONS_DEBUG_UPPER_TRIGGER_LEFT   = 24,
    E_GAMEINPUTACTIONS_DEBUG_UPPER_TRIGGER_RIGHT  = 25,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_STICK_UP        = 26,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_STICK_DOWN      = 27,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_STICK_LEFT      = 28,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_STICK_RIGHT     = 29,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_STICK_UP       = 30,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_STICK_DOWN     = 31,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_STICK_LEFT     = 32,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_STICK_RIGHT    = 33,
    E_GAMEINPUTACTIONS_DEBUG_LEFT_STICK_BUTTON    = 34,
    E_GAMEINPUTACTIONS_DEBUG_RIGHT_STICK_BUTTON   = 35,
    E_GAMEINPUTACTIONS_DEBUG_STEP                 = 36,

    // ---- GUI: the dpad family (CrashNavPanel's road-rule filter reads ONLY these) ----
    E_GAMEINPUTACTIONS_GUI_DPAD_UP                = 37,
    E_GAMEINPUTACTIONS_GUI_DPAD_DOWN              = 38,   // also PauseScreen's "TO_COLOUR"
    E_GAMEINPUTACTIONS_GUI_DPAD_LEFT              = 39,
    E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT             = 40,

    // ---- GUI: the generic navigation family ----
    E_GAMEINPUTACTIONS_GUI_UP                     = 41,   // HighlightPrevious
    E_GAMEINPUTACTIONS_GUI_DOWN                   = 42,   // HighlightNext
    E_GAMEINPUTACTIONS_GUI_LEFT                   = 43,   // HighlightPreviousItem / legend prev
    E_GAMEINPUTACTIONS_GUI_RIGHT                  = 44,   // HighlightNextItem   / legend next

    // ---- GUI: the face/system buttons ----
    E_GAMEINPUTACTIONS_GUI_START                  = 45,   // START  (pause / press-start / exit map)
    E_GAMEINPUTACTIONS_GUI_BACK                   = 46,   // BACK   (InGame: open the main map)
    E_GAMEINPUTACTIONS_GUI_LTHUMB                 = 47,
    E_GAMEINPUTACTIONS_GUI_RTHUMB                 = 48,   // language-cycle combo partner of 56
    E_GAMEINPUTACTIONS_GUI_SELECT                 = 49,   // A -- accept
    E_GAMEINPUTACTIONS_GUI_CANCEL                 = 50,   // B -- back
    E_GAMEINPUTACTIONS_GUI_OPTION0                = 51,
    E_GAMEINPUTACTIONS_GUI_OPTION1                = 52,
    E_GAMEINPUTACTIONS_GUI_OPTION2                = 53,
    E_GAMEINPUTACTIONS_GUI_LSHOULDER              = 54,   // tab left  ("TOGGLE_LEFT")
    E_GAMEINPUTACTIONS_GUI_RSHOULDER              = 55,   // tab right ("TOGGLE_RIGHT")
    E_GAMEINPUTACTIONS_GUI_LTRIGGER               = 56,   // map zoom
    E_GAMEINPUTACTIONS_GUI_RTRIGGER               = 57,   // inspect the hovered event icon
    E_GAMEINPUTACTIONS_GUI_EVENT_DETAILS          = 58,   // InGame::OpenEventMap -> "MAP_EVENT"
    E_GAMEINPUTACTIONS_GUI_CAR_LOG                = 59,

    E_GAMEINPUTACTIONS_SET_PLAYERSTATS_MAX        = 60,
    E_GAMEINPUTACTIONS_COUNT                      = 61,
    E_GAMEINPUTACTIONS_INVALID                    = -1
};
