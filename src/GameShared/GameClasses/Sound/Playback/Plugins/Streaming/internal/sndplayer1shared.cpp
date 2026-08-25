#include "types.hpp"

#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x8268CD20
//   (rw::audio::core::SndPlayer1_CgsStreamMod::AdvanceCurrentRequest)
//
// Advances the "current request" cursor around the request ring, then caches the
// new current request's parameters into the mCurrentRequest* fields -- but only if
// that request is still active (state is neither COMPLETE(4) nor FREE(0)). When
// the request is inactive, handle/sampleRate keep their previous values and only
// samplesPlayed/numSamples are cleared -- matching the asm exactly.
//
//   asm: lbz 0x181 (+1, wrap vs lbz 0x182) -> stb 0x181
//        stw 0, 0x158 ; stw 0, 0x15C
//        entry = this + lhz(0x17C) + current*32          (rotlwi r9,r9,5)
//        lbz entry+0x1E; ==4 || ==0 -> return
//        stw 0, 0x158
//        lfs/stfs entry+0xC  -> 0x150   (float request handle)
//        lfs/stfs entry+0x10 -> 0x154   (float sample rate)
//        lwz/stw entry+0x14 -> 0x15C   (numSamples)
//
// (2026-08-25, audio-faithfulness wave 2: the former TU-local member-less rival
// struct + raw byte-offset transliteration is retired; the single header home
// carries the PDB-named layout and this body reads it BY NAME. The console *32
// entry stride is sizeof(RequestInternal) over the runtime-seeded
// mRequestInternalOffset tail walk on the host.)

namespace rw { namespace audio { namespace core {

    SndPlayer1_CgsStreamMod* SndPlayer1_CgsStreamMod::AdvanceCurrentRequest()
    {
        // Bump + wrap the current-request cursor.
        u8 lu8Current = static_cast<u8>(mCurrentRequest + 1);
        mCurrentRequest = lu8Current;
        if (lu8Current == mMaxRequests)
        {
            lu8Current = 0;
            mCurrentRequest = 0;
        }

        mCurrentRequestSamplesPlayed = 0;
        mCurrentRequestNumSamples    = 0;

        // The RequestInternal ring lives in the object's tail allocation at the
        // runtime-seeded byte offset mRequestInternalOffset (console stride 32 ==
        // sizeof(RequestInternal) there; host stride is the host sizeof).
        const RequestInternal* lpEntry = reinterpret_cast<const RequestInternal*>(
            reinterpret_cast<char*>(this) + mRequestInternalOffset
            + lu8Current * sizeof(RequestInternal));

        if (lpEntry->state != E_REQUESTSTATE_COMPLETE &&
            lpEntry->state != E_REQUESTSTATE_FREE)
        {
            mCurrentRequestSamplesPlayed = 0;
            mCurrentRequestHandle        = lpEntry->requestHandle;   // lfs/stfs (float)
            mCurrentRequestSampleRate    = lpEntry->sampleRate;      // lfs/stfs (float)
            mCurrentRequestNumSamples    = lpEntry->numSamples;
        }
        return this;
    }

}}}
