#ifndef BRN_GUI_PFX_EVENTS_H
#define BRN_GUI_PFX_EVENTS_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"   // CgsGui::GuiEvent<N>
#include "GameSource/Gui/PFX/BrnGuiPFXHooks.h"        // BrnGui::KI_MAX_PFX_ID_LENGTH

// =============================================================================
// The GUI post-FX hook events (DecFIGS DWARF BrnGuiEventTypeDefs.h:5966..6049, field
// names; ARTIST ids and field ORDER from the readers -- the two builds disagree on the
// start event's order, and the shipped image decides):
//
//   495 GuiPFXHookEvent                 Director -> GUI   start a hook   (EffectsArbitrator::StartHook
//                                        @0x8250B618 reads the GUID at +0, the six maxima at
//                                        +4..+27, the name at +28; 64 bytes)
//   496 GuiPFXHookStopEvent             Director -> GUI   stop a hook    (GUID +0, name +4; 40 bytes)
//   497 GuiEvent<497>                   Director -> GUI   stop the menu hook (no payload)
//   498 GuiPFXStartBackgroundHookEvent  Director -> GUI   (GUID +0, name +4, maximum weight +40; 44 bytes)
//   499 GuiPFXStopBackgroundHookEvent   Director -> GUI   (GUID +0, name +4; 40 bytes)
//   500 GuiEvent<500>                   Director -> GUI   "enumerate your hooks" (no payload)
//   501 GuiPFXHookEnumeration           GUI -> Director   the reply: hook count + one name pointer
//                                        per hook (the PFXHook records start with their name,
//                                        so the console stores the hook pointers themselves)
//
// All are posted by BrnGameModule::BridgeDirectorToGui @0x823DD5C0 out of the director
// camera's CameraEffects requests, except 501 which EffectsArbitrator::EventUpdate posts
// back and BridgeGuiToDirector hands to EffectInterface::Update.
// =============================================================================
namespace BrnGui
{
    typedef char HookNameString[KI_MAX_PFX_ID_LENGTH + 1];   // BrnGuiPFXHooks.h:37 -- 33 bytes

    struct GuiPFXHookEvent : public CgsGui::GuiEvent<495>
    {
        u32            muGuid;                        // :5966  +0x00
        f32            mfMaximumBloomWeight;          // :5968  +0x04
        f32            mfMaximumVignetteWeight;       // :5969  +0x08
        f32            mfMaximumBlurWeight;           // :5970  +0x0C
        f32            mfMaximumDepthOfFieldWeight;   // :5971  +0x10
        f32            mfMaximum2DTintWeight;         // :5972  +0x14
        f32            mfMaximum3DTintWeight;         // :5973  +0x18
        HookNameString macName;                       // :5967  +0x1C
        u8             maPad3D[3];
    };

    struct GuiPFXHookStopEvent : public CgsGui::GuiEvent<496>
    {
        u32            muGuid;                        // :6012  +0x00
        HookNameString macName;                       // :6013  +0x04
        u8             maPad25[3];
    };

    struct GuiPFXStopMenuHookEvent : public CgsGui::GuiEvent<497> {};

    struct GuiPFXStartBackgroundHookEvent : public CgsGui::GuiEvent<498>
    {
        u32            muGuid;                        // :5989  +0x00
        HookNameString macName;                       // :5990  +0x04
        u8             maPad25[3];
        f32            mfMaximumWeight;               // :5991  +0x28
    };

    struct GuiPFXStopBackgroundHookEvent : public CgsGui::GuiEvent<499>
    {
        u32            muGuid;                        // :5997  +0x00
        HookNameString macName;                       // :5998  +0x04
        u8             maPad25[3];
    };

    struct GuiPFXHookEnumerationRequest : public CgsGui::GuiEvent<500> {};

    struct GuiPFXHookEnumeration : public CgsGui::GuiEvent<501>
    {
        s32         miHookNameCount;                  // :6048
        const char* mapHookNames[100];                // :6049  (widened: real pointers on PC)
    };
}

#endif
