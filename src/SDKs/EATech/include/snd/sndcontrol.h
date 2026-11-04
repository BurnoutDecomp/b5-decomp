#pragma once

#include "types.hpp"

extern "C" s32 iSNDcalcpitch(s32 aiVoice);
extern "C" s32 SNDPLATFORM_setpitch(s32 aiVoice);
extern "C" s32 iSNDcalcvol(s32 aiVoice);
extern "C" s32 SNDstop(s32 aiVoiceHandle);
