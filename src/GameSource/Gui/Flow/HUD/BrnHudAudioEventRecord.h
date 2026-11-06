#pragma once

// GameSource/Gui/Flow/HUD/BrnHudAudioEventRecord.h
// The 40-byte OutputGuiEvent<BrnGui::GuiAudioEvent> wrapper record the HUD states post on the GUI
// out channel: header { sizeof(payload)=24, type=456, payload offset=16 }, then the 24-byte payload.
// BrnGui::GuiAudioEvent is still `u8 maPayload[12]` in BrnGuiDemangledEventTypes.h, so the leading
// words have no recovered names and are spelled by role.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"

namespace BrnGui
{
    struct alignas(8) HudAudioEventRecord
    {
        s32   miOutEventSize;     // +0x00 = 24
        s32   miOutEventType;     // +0x04 = 456
        s32   miOutEventOffset;   // +0x08 = 16
        s32   miHeaderPad;        // +0x0C (uninitialised on the console)
        s32   miAudioParam0;      // +0x10
        s32   miAudioParam1;      // +0x14
        s32   miAudioParam2;      // +0x18
        s32   miPayloadPad;       // +0x1C (uninitialised on the console)
        CgsID mAudioId;           // +0x20 = 0

        HudAudioEventRecord(s32 liParam0, s32 liParam1, s32 liParam2, CgsID lAudioId = 0)
            : miOutEventSize(24), miOutEventType(456), miOutEventOffset(16), miHeaderPad(0)
            , miAudioParam0(liParam0), miAudioParam1(liParam1), miAudioParam2(liParam2)
            , miPayloadPad(0), mAudioId(lAudioId)
        {
        }
    };

    // The pause cue both main HUD states post before their PAUSE: { 2, 0, -1 }, X360 @0x824807C0
    // (RaceMainHudState) and @0x82481178 (FBurnMainHudState).
    const s32 KI_HUD_AUDIO_PAUSE_PARAM0 = 2;
    const s32 KI_HUD_AUDIO_PAUSE_PARAM1 = 0;
    const s32 KI_HUD_AUDIO_PAUSE_PARAM2 = -1;
}
