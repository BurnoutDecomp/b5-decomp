#pragma once

#include "types.hpp"

// ===========================================================================
//  CgsSystem::GuiSoundPC -- PC playback of the GUI presentation sounds (the
//  menu move/accept blips).
//
//  FLAG PC-platform leaf (faithful data): the console consumer chain is
//  SoundLogicModule::ProcessGuiEvents @0x826ED6C8 -> CgsSound::Io::Message(6)
//  -> BrnSound::Logic::PresentationEffect::Notify @0x826FF840 -> Resolve
//  @0x82687990 over the Attrib::Gen::presentationactionlist data -> a splice
//  in the presentation Splicer bank played on a StereoWavVoiceSpec voice.
//  The sound-logic/message layer is an un-homed behavioural cluster, so this
//  leaf reproduces the OBSERVABLE from the REAL data set:
//   * SOUND\BURNOUTGLOBALDATA.BIN  (AttribSys vault; the presentationactionlist
//     parallel arrays keyed by CgsResource::ID::HashString of the trigger
//     strings -- the same table Resolve walks),
//   * SOUND\SPLICER\PRESENTATIONASSET.BUNDLE (the Splicer bank: splice TOC +
//     fully-resident SNR samples, decoded through the shared EA-XMA path).
//  Playback mixes additively over the music/movie stream through the
//  AudioOutputPC overlay fill (a 4-slot one-shot pool mirroring the effect's
//  aging-voice pool).
// ===========================================================================

namespace CgsSystem
{
    class GuiSoundPC
    {
    public:
        // Handle one GUI audio trigger. Key rule (the console trigger-resolve over
        // the REAL GuiAudioTriggerEvent record {component[32], s32 actionEnum,
        // label[32], movie[32]} @0x824F6350/0x8269E368): the string key is the
        // LABEL unless "uninitialised", then the COMPONENT name; the row must also
        // carry the matching PresentationAction enum. Params: lpacTypeName = string
        // key candidate A (label/trigger name), lpacActionName = the AS action
        // string (parsed to the enum when liChannel < 0), lpacAptName = candidate B
        // (component/movie name), liChannel = the action enum (or -1 to parse).
        // Lazily initialises the data set on first call.
        static void OnTrigger(const char* lpacTypeName, const char* lpacActionName,
                              const char* lpacAptName, s32 liChannel);

    private:
        static bool Initialise();
        static void FillStatic(s16* lpOut, int liFrames, void* lpUser);
    };
}
