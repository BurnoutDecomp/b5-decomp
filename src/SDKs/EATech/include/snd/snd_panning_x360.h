#pragma once
#include "types.hpp"
namespace Snd9 {
    extern f32 gafSpeakerAzimuths[6];
    extern s16 gasSpeakerConfig2[12], gasSpeakerConfig4[12], gasSpeakerConfig6[12];
    extern u8* gapAzimuthToSpeaker[6];
    extern u8* gpAzimuthToSpeakerStorage;
    extern u32 guServiceTick;
    extern u32 gauRandomSeeds[6];
    extern void* gpBankList;
    extern f32 gfSystemScale;
    extern u8 gucStartupFlag6A, gucServiceFlag65, gucServiceFlag66;
    extern void (*gpfnRestoreB0)(); // ARTIST 0x832778B0, written by SNDAEMSI_resolvemodulebank
    extern void (*gpfnRestoreB4)(); // ARTIST 0x832778B4
    extern void (*gpfnRestoreB8)(s32); // ARTIST 0x832778B8
    extern void (*gpfnRestoreBC)(); // ARTIST 0x832778BC
}
extern "C" void SNDI_spkrconfig();
extern "C" void SNDI_precalcaztospkrvol();
extern "C" void SNDI_freespkrtable();
extern "C" s32 SNDPLATFORM_init();
extern "C" s32 SNDPLATFORM_restore();
extern "C" s32 SNDSYSI_init();
extern "C" s32 SNDSYS_restore();
