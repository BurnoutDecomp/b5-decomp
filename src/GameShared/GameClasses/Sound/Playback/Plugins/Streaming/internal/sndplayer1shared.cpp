#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268CD20
//   (rw::audio::core::SndPlayer1_CgsStreamMod::AdvanceCurrentRequest)
//
// Advances the "current request" cursor around the request ring, then caches the
// new current request's parameters into the mCurrentRequest* fields — but only if
// that request is still active (state is neither COMPLETE(4) nor FREE(0)).
// Behaviour-faithful to the X360 pseudocode (byte offsets):
//
//   current = (current + 1);  if (current == maxRequests) current = 0;   // wrap
//   mCurrentRequestSamplesPlayed = 0;   (+344)
//   mCurrentRequestNumSamples    = 0;   (+348)
//   entry = (char*)this + mRequestInternalOffset(+380) + current*32;
//   if (entry.state(+30) != 4 && entry.state != 0) {        // active
//       mCurrentRequestSamplesPlayed = 0;                   (+344)
//       mCurrentRequestHandle     = entry[+12];             (+336)
//       mCurrentRequestSampleRate = entry[+16];             (+340)
//       mCurrentRequestNumSamples = entry[+20];             (+348)
//   }
//   return this;
//
// When the request is inactive, handle/sampleRate (+336/+340) keep their previous
// values and only samplesPlayed/numSamples are cleared — matching the source.

namespace rw { namespace audio { namespace core {

    struct SndPlayer1_CgsStreamMod
    {
        SndPlayer1_CgsStreamMod* AdvanceCurrentRequest();
    };

    static const u32 KU_REQUEST_STRIDE        = 32;   // bytes per RequestInternal
    static const u8  KU_REQUESTSTATE_COMPLETE = 4;
    static const u8  KU_REQUESTSTATE_FREE     = 0;

    SndPlayer1_CgsStreamMod* SndPlayer1_CgsStreamMod::AdvanceCurrentRequest()
    {
        char* lpBase = reinterpret_cast<char*>(this);

        const u8 lu8MaxRequests = *reinterpret_cast<u8*>(lpBase + 386);
        u8 lu8Current = static_cast<u8>(*reinterpret_cast<u8*>(lpBase + 385) + 1);
        if (lu8Current == lu8MaxRequests)
            lu8Current = 0;
        *reinterpret_cast<u8*>(lpBase + 385) = lu8Current;

        *reinterpret_cast<s32*>(lpBase + 344) = 0;   // mCurrentRequestSamplesPlayed
        *reinterpret_cast<s32*>(lpBase + 348) = 0;   // mCurrentRequestNumSamples

        const u16 lu16InternalOffset = *reinterpret_cast<u16*>(lpBase + 380);
        char* lpEntry = lpBase + lu16InternalOffset + lu8Current * KU_REQUEST_STRIDE;

        const u8 lu8State = *reinterpret_cast<u8*>(lpEntry + 30);
        if (lu8State != KU_REQUESTSTATE_COMPLETE && lu8State != KU_REQUESTSTATE_FREE)
        {
            *reinterpret_cast<s32*>(lpBase + 344) = 0;                                  // samplesPlayed
            *reinterpret_cast<f32*>(lpBase + 336) = *reinterpret_cast<f32*>(lpEntry + 12); // handle
            *reinterpret_cast<f32*>(lpBase + 340) = *reinterpret_cast<f32*>(lpEntry + 16); // sampleRate
            *reinterpret_cast<s32*>(lpBase + 348) = *reinterpret_cast<s32*>(lpEntry + 20); // numSamples
        }
        return this;
    }

}}}
