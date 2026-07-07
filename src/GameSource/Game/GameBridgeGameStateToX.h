#pragma once
// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX.h
//
// Public declarations for the GameState->X bridge TU (DWARF home
// GameSource/Game/GameBridgeGameStateToX.cpp). Only the surface needed by the
// reconstructed function(s) in this batch is declared here; other bridge entry
// points are added by their owning batches.
// ============================================================================

#include "SharedClasses/Progression/BrnTrainingTypes.h"   // BrnProgression::ETrainingType
#include "GameShared/GameClasses/System/Input/CgsInputModuleIO.h" // CgsInput::InputIO::PostWorldInputBuffer

namespace BrnGameState { class GameStateModule; }

namespace BrnGameState
{
    // Un-homed game-state accessors reached by name from
    // BrnGameModule::BridgeGameStateToController (X360 0x823C0AE8: sub_823B9CD8(a3) returns the
    // bind-request queue; the unbind-request queue sits at +0x4C from it). Their canonical home
    // is the game-state IO; declared here so the bridge TU resolves them until that TU lands.
    const CgsInput::InputIO::PostWorldInputBuffer::BindRequestQueue*
        GetGameStateInputBindRequestQueue(GameStateModule* lpGameStateOutput);
    const CgsInput::InputIO::PostWorldInputBuffer::UnBindRequestQueue*
        GetGameStateInputUnbindRequestQueue(GameStateModule* lpGameStateOutput);

    // ---- takedown-event output-queue element + accessors ---------------------------------------
    // TranslateTakedownsToGuiEvents (X360 0x823E1C38) walks the OutputBuffer's TakedownEventOutput
    // queue (BrnGameState::GameStateModuleIO::TakedownEventOutputQueueType @ +0x4040), reading the
    // element count at +8 and copying each 40-byte record out via BrnGameState::TakedownEvent__.
    // Both are un-homed here (their canonical home is the game-state IO TU) and reached BY NAME; the
    // queue is passed opaquely (X360 treats it as a raw _DWORD*). FLAG: the 40-byte record's field
    // semantics beyond the compared race-car index are not attested -- modelled store-for-store from
    // the X360 copy (5 qwords) + the fields the bridge reads.
    struct TakedownEventOutputRecord
    {
        s32 miField00;        // +0x00 (X360 v23 -> GuiEvent +0x10)
        s32 miRaceCarIndex;   // +0x04 (X360 v24 -> GuiEvent +0x14; compared against the runner index)
        u64 mu64Field08;      // +0x08 (X360 v25 -> GuiEvent +0x00)
        u64 mu64Field10;      // +0x10 (X360 v26 -> GuiEvent +0x08)
        s32 miField18;        // +0x18 (X360 v27 -> GuiEvent +0x18)
        s32 miField1C;        // +0x1C (X360 v28 -> hard GuiEvent +0x20)
        s32 miField20;        // +0x20 (X360 v29 -> hard GuiEvent +0x1C)
        u8  mbField24;        // +0x24 (X360 v30 -> GuiEvent status byte 0)
        u8  mbPad25;          // +0x25
        u8  mbField26;        // +0x26 (X360 v31 -> GuiEvent status byte 1)
        u8  mbPad27;          // +0x27
    };

    s32 GetTakedownEventOutputCount(const void* lpTakedownQueue);
    const TakedownEventOutputRecord* GetTakedownEventOutputRecord(
        const void* lpTakedownQueue, s32 liIndex);
}

namespace BrnGui
{
    // ---- takedown GUI-event payloads -- UN-HOMED placeholder (FLAGGED) --------------------------
    // TranslateTakedownsToGuiEvents (X360 0x823E1C38) synthesises one of these per queued takedown
    // and pushes it through the CgsGui GUI module (this + 7252512). Both variants fill the same
    // stack image (X360 &v14); layouts are derived store-for-store from the branch stores. Replace
    // with the real BrnGui event types when the GUI-event family is reconstructed.
    struct GuiTakedownEvent
    {
        u64 mu64Field00;      // +0x00 (src +0x08)
        u64 mu64Field08;      // +0x08 (src +0x10)
        s32 miField10;        // +0x10 (src +0x00)
        s32 miRaceCarIndex;   // +0x14 (src +0x04)
        s32 miField18;        // +0x18 (src +0x18)
        s32 miField1C;        // +0x1C (src +0x20)
        s32 miField20;        // +0x20 (src +0x1C)
        u8  mbStatus24;       // +0x24 (src +0x24)
        u8  mbStatus25;       // +0x25 (src +0x26)
        u8  maPad[2];         // +0x26
    };
    struct GuiSoftTakedownEvent
    {
        u64 mu64Field00;      // +0x00 (src +0x08)
        u64 mu64Field08;      // +0x08 (src +0x10)
        s32 miField10;        // +0x10 (src +0x00)
        s32 miRaceCarIndex;   // +0x14 (src +0x04)
        s32 miField18;        // +0x18 (src +0x18)
        u8  mbStatus1C;       // +0x1C byte0 (src +0x24)
        u8  mbStatus1D;       // +0x1D byte1 (src +0x26)
        u8  maPad[2];         // +0x1E
    };
}

namespace BrnGame
{
    // @0x823AA3B8 -- map a training-tip enum to its GUI string-ID.
    // Returns "ERROR - UNKNOWN TRAINING TYPE" for the unused/gap indices.
    const char* ConvertTrainingTypeToStringId(BrnProgression::ETrainingType leTrainingType);

} // namespace BrnGame
