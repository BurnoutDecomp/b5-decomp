#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82685A10
//   (CgsSound::Logic::VoicePoolBase::IsPlaying)
//
// Scans the voice array for any voice whose state is "playing" (state field is
// neither 0 == free nor 7 == stopped). Returns true on the first such voice.
// Behaviour-faithful to the X360 pseudocode:
//
//   count = this->muVoiceCount;          (*(this+12))
//   if (!count) return false;
//   state = &voices[0].state;            (*(this+8) + 72)
//   for (index = 0; ; ) {
//       if (*state != 7 && *state != 0) return true;   // playing
//       if (++index >= count) return false;
//       state += 23;                     // 23 dwords (92 bytes) per voice
//   }

namespace CgsSound
{
    namespace Logic
    {
        struct VoicePoolBase
        {
            u8        mPad[8];          // [0x00] opaque
            uintptr_t mpVoices;         // [0x08] base of the voice array
            u32       muVoiceCount;     // [0x0C] number of voices

            bool IsPlaying() const;
        };

        // Stride between voices, in 32-bit words; the state field sits 72 bytes in.
        static const u32 KU_VOICE_STRIDE_DWORDS = 23;
        static const u32 KU_VOICE_STATE_OFFSET  = 72;
        static const u32 KU_VOICE_STATE_STOPPED = 7;
        static const u32 KU_VOICE_STATE_FREE    = 0;

        bool VoicePoolBase::IsPlaying() const
        {
            const u32 luCount = muVoiceCount;
            if (luCount == 0)
                return false;

            const u32* lpState = reinterpret_cast<const u32*>(mpVoices + KU_VOICE_STATE_OFFSET);
            for (u32 luIndex = 0; ; )
            {
                if (*lpState != KU_VOICE_STATE_STOPPED && *lpState != KU_VOICE_STATE_FREE)
                    return true;
                if (++luIndex >= luCount)
                    return false;
                lpState += KU_VOICE_STRIDE_DWORDS;
            }
        }
    }
}
