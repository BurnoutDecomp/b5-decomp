#ifndef SDKS_EATECH_SND_SNDO_H
#define SDKS_EATECH_SND_SNDO_H

#include "types.hpp"

// ============================================================================
// SDKs/EATech/include/snd/sndo.h  (DWARF home)
//
// EATech "Snd9" audio-middleware interfaces. MINIMAL reconstruction: only the
// polymorphic IAemsSamplePlayer interface (sndo.h:1421) that the game's
// CgsSound::Playback::AemsRWSamplePlayer derives from is modelled, plus its
// InputSelector enum (sndo.h:1429). This is the abstract base whose vtable shape the
// derived player's destructor and overrides depend on.
//
// FLAG (MINIMAL vendor surface): the full sndo.h SDK declares many more types
// (IAemsSamplePlayerFactory, AemsPlayerInputAccessor, SNDREQUESTSTATUS, Util, ...).
// Only IAemsSamplePlayer is homed here -- the slice the CgsSound AEMS player TUs
// need. The remainder is reconstructed when a TU that touches it is decompiled.
// ============================================================================

namespace Snd9
{
    // sndo.h:1421. Abstract sample-player interface a platform/middleware backend
    // implements. All hooks are pure virtual; the derived player overrides them.
    struct IAemsSamplePlayer
    {
        // sndo.h:1429. Per-input selector ids passed to SetInput.
        enum InputSelector
        {
            PLAYER_INPUT_PITCHMULT = 0,
            PLAYER_INPUT_TIMEMULT  = 1,
            PLAYER_INPUT_VOL       = 2,
            PLAYER_INPUT_AZIMUTH   = 3,
            PLAYER_INPUT_ELEVATION = 4,
            PLAYER_INPUT_FXWET0    = 5,
            PLAYER_INPUT_LOWPASS   = 6,
            PLAYER_INPUT_HIGHPASS  = 7,
            PLAYER_INPUT_DRYLEVEL  = 8,
            PLAYER_INPUT_USER_FIRST = 9,
            PLAYER_INPUT_USER_LAST  = 136,
        };

        // sndo.h:1491/1505/1518/1533/1549/1562 -- pure-virtual control surface.
        virtual void Release()                                  = 0;
        virtual void Pause()                                    = 0;
        virtual void Unpause()                                  = 0;
        virtual void SetInput(InputSelector aeSelector, int aiValue) = 0;
        virtual void SetAzimuth(int aiAzimuth, int* apLegacyAzimuths) = 0;
        virtual void GetOutputs(int aiNumOutputs, int* apValues) = 0;

    protected:
        // sndo.h:1566. Protected default ctor (subclass-only construction).
        IAemsSamplePlayer() {}
        // sndo.h:1567. Polymorphic base destructor.
        virtual ~IAemsSamplePlayer() {}
    };
} // namespace Snd9

#endif // SDKS_EATECH_SND_SNDO_H
