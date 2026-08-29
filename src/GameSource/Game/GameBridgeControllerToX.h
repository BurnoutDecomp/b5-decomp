#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeControllerToX.h
//
// Support types for the BrnGame::BrnGameModule controller-bridge family
// (GameSource/Unity/../Game/GameBridgeControllerToX.cpp). Each per-frame bridge
// reads player-0's pad record (CgsInput::InputIO::PadOutputInformation, returned
// by CgsInput::InputIO::OutputBuffer::GetPadInfo BY NAME) and publishes it into a
// downstream subsystem's input buffer.
//
// HONEST PLACEHOLDERS (FLAGGED). Several downstream targets are not yet homed:
//   * CgsGui::GuiModule + its GUI-event payloads (GuiEventActiveUserIndex, ...,
//     GuiEventSetLanguage) -- the GUI module's AddGuiEvent<T> template family is
//     a whole un-reconstructed subsystem. Modelled here as named placeholder
//     payload structs + a templated AddGuiEvent member so the ToGui bridge can
//     reference every event BY NAME and store-for-store fill each payload.
//   * The debug-controller image (172 bytes) MapActionInfoToDebugController builds
//     and the world/director bridges memcpy into their buffers' DebugController /
//     ControllerInfo members -- modelled as a named POD (DebugControllerImage).
//   * The Director controller-info image (the v31 flag block + the debug-controller
//     image + 4 trailing axis floats) the ToDirector bridge passes to
//     DirectorIO::InputBuffer::SetControllerInfo(const void*) -- modelled as
//     DirectorControllerInfoImage.
//   * The global action-index tables (X360 unk_820352F0 / dword_82035330 region)
//     that map GUI/world action ids to pad ActionInfo slots -- modelled as the
//     named file-scope arrays gauiActionIndexTable / giNumGuiActionIndices in the .cpp.
// Promote each placeholder to its real home when that subsystem is reconstructed.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::ActionInfo (action slot read by name)
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"       // CgsGui::GuiEventControllerInput* / ActiveUserIndex / Axis / SetLanguage

#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"  // BrnGui::GuiEventToggleChangeCarMessage (id 540) -- the canonical home

#include <cstring>   // memcpy

namespace BrnGame
{
    // ---- debug-controller image (X360 172-byte block) -----------------------
    // MapActionInfoToDebugController copies the player's 22 ActionInfo slots into this image
    // (per-action: value @ +4*i, then three status bools at the +0x58/+0x6E/+0x84 sub-blocks).
    // The world bridge memcpy's it into BrnWorldIO::UpdateInputBuffer::GetDebugController(); the
    // director bridge embeds it inside DirectorControllerInfoImage. FLAG: field semantics beyond
    // the 22 {value,bool,bool,bool} action groups are not attested -- modelled as the raw 172B image
    // with the typed accessor surface MapActionInfoToDebugController writes through.
    struct DebugControllerImage
    {
        // 22 action groups. The X360 lays the per-action data out across three parallel sub-arrays
        // inside the 172-byte image: values (f32) at +0x00 stride 4, a "pressed" bool array at +0x58,
        // a "held"-ish bool array at +0x6E, and a third bool array at +0x84. Modelled with named
        // arrays at those exact offsets so the writes are by-name; total size pinned to 172.
        static const s32 KI_NUM_ACTIONS = 22;
        f32  mafValue[KI_NUM_ACTIONS];                 // +0x00 .. +0x58
        u8   mabFlagA[KI_NUM_ACTIONS];                 // +0x58 .. +0x6E
        u8   mabFlagB[KI_NUM_ACTIONS];                 // +0x6E .. +0x84
        u8   mabFlagC[KI_NUM_ACTIONS];                 // +0x84 .. +0x9A
        u8   maPad[172 - (KI_NUM_ACTIONS * (4 + 1 + 1 + 1))]; // close to 172 bytes
    };

    // ---- director controller-info image (X360 0xAC=172... actually 0xE0=224 target) ----------
    // The ToDirector bridge builds a 16-byte flag block (v31) immediately followed by the 172-byte
    // debug-controller image (v34) and 4 trailing axis floats, then calls
    // DirectorIO::InputBuffer::SetControllerInfo(&image) which memcpy's 224 bytes. Modelled as the
    // matching POD so every store is by-name. FLAG: the flag bit assignments are named from the
    // bridge's source ActionInfo status bits; the 224-byte total matches SetControllerInfo's copy.
    struct DirectorControllerInfoImage
    {
        // 16-byte leading flag block (X360 v31[0..15]); each is a 1-byte controller-state flag.
        u8   mabFlags[16];        // +0x00 .. +0x10
        // 172-byte debug-controller image (X360 v34) -- the MapActionInfoToDebugController output,
        // with the 4 trailing axis floats (v35[39..42]) appended by the bridge before the memcpy.
        DebugControllerImage mDebugController; // +0x10 .. (172B)
        u8   maPadTo224[224 - 16 - 172];       // close to SetControllerInfo's 224-byte copy
    };

    // ---- world player-vehicle-controls image (X360 60-byte block) -----------
    // The ToWorld bridge fills a local controls block (steering/throttle/brake/handbrake/boost +
    // flags) then calls BrnWorldIO::UpdateInputBuffer::SetPlayerVehicleControls(&controls).
    // Modelled here as the named POD that SetPlayerVehicleControls(const PlayerVehicleControls*)
    // copies (60 bytes). FLAG: derived purely from the bridge's stores (no DWARF shape).
    struct WorldVehicleControlsImage
    {
        // Field offsets pinned store-for-store to X360 0x823CD890 (the &controls passed to
        // SetPlayerVehicleControls is var_140 = sp+0x60; offsets below are controls-relative).
        f32  mfAxis08;        // +0x00 (X360 var_140 = *(pad+0x08))
        f32  mfStickLX;       // +0x04 (X360 var_13C = *(pad+0x00), the left-stick X)
        f32  mfAxis0C;        // +0x08 (X360 var_138 = *(pad+0x0C))
        f32  mfStickLY;       // +0x0C (X360 var_134 = *(pad+0x04))
        f32  mafZeroed[4];    // +0x10 .. +0x1C (X360 var_130/12C/128/124 = 0)
        f32  mfAxis18;        // +0x20 (X360 var_120 = *(pad+0x18) / ShowtimeIntro 1.0)
        f32  mfAxis20;        // +0x24 (X360 var_11C = *(pad+0x20) / ShowtimeIntro 0.0)
        f32  mfAxis28;        // +0x28 (X360 var_118 = *(pad+0x28) / ShowtimeIntro 1.0)
        f32  mfSteeringCurved;// +0x2C (X360 var_114 = curve(stickX) / ShowtimeIntro steering)
        f32  mfDistance;      // +0x30 (X360 var_110 = action[55].value - action[54].value)
        u8   mabStatus[8];    // +0x34 .. +0x3C (X360 var_10C..var_105, eight action/state bits)
        // total = 0x3C = 60 bytes (SetPlayerVehicleControls's copy width)
    };

    // ========================================================================
    // The GUI event sink is the REAL CgsGui::GuiModule::AddGuiEvent<T> now
    // (GameShared/GameClasses/Gui/CgsGuiModule.h; instances @0x823DA8A8 etc.), pushing
    // onto the GUI module INPUT buffer's inbound queue (CgsGuiModuleIO::InputBuffer::
    // GetGuiEvents @0x8284F238). The payload types are the real CgsGui GuiEvent<N>
    // records (CgsGuiEventTypeDefs.h). The former placeholder sink is retired.
    // ========================================================================
}

// ⭐ FORK RETIRED 2026-08-29 (map-world wave). `BrnGui::GuiEventToggleChangeCarMessage` was
// re-defined here as `CgsGui::GuiEvent<540>` (a 12-byte record) while its canonical home,
// GameSource/Gui/BrnGuiDemangledEventTypes.h:277, already carried the X360-attested shape:
// id 540, SIZE 1 -- and AddGuiEvent<T> takes its payload width from sizeof(T), so the two
// definitions pushed records of different lengths. Two namespace-scope definitions of one
// class link silently; this one only surfaced when a TU finally co-included both headers.
// The canonical home is included below and the fork is gone; the note this file's sibling
// GameBridgeGameStateToX.cpp:57 already carried named exactly this.
