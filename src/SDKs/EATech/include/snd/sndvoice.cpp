#include "SDKs/EATech/include/snd/sndhal.h"
#include "SDKs/EATech/include/snd/sndsys.h"

namespace Snd9
{
namespace
{
    void ClearVoice(Hal::Channel& arChannel)
    {
        arChannel.mucVoiceState = 0;
        arChannel.mucVoiceGroup = 0;
        arChannel.mucGroupPrimary = 0;
        arChannel.muLastVoiceTick = guServiceTick;
    }
}
}

// ARTIST 0x82B72E70.
extern "C" void SNDVOICEI_free(s32 aiVoice)
{
    using namespace Snd9;
    Hal::Channel& lrVoice = Hal::gpChannels[aiVoice];
    if (lrVoice.mucVoiceGroup)
    {
        s32 liPrimary = -1;
        s32 liActive = 0;
        for (s32 i = 0; i < giVoiceCount; ++i)
        {
            const Hal::Channel& lrOther = Hal::gpChannels[i];
            if (lrOther.mucVoiceGroup == lrVoice.mucVoiceGroup &&
                lrOther.miVoiceHandle >= 0 && lrOther.mucVoiceState)
            {
                ++liActive;
                if (lrOther.mucGroupPrimary)
                    liPrimary = i;
            }
        }

        if (liActive != 1)
        {
            Hal::Channel& lrPrimary = Hal::gpChannels[liPrimary];
            if (lrPrimary.mucVoiceState == 2 && aiVoice != liPrimary && liActive == 2)
            {
                ClearVoice(lrVoice);
                ClearVoice(lrPrimary);
                return;
            }
            if (lrPrimary.mucVoiceState == 1 && aiVoice == liPrimary)
            {
                lrPrimary.mucVoiceState = 2;
                return;
            }
        }
    }
    ClearVoice(lrVoice);
}
