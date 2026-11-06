#include <cstddef>
#include "GameSource/Sound/Global/BrnHUDEffect.h"

typedef BrnSound::Logic::HUDEffect HUDEffect;
typedef BrnSound::Logic::MusicStream MusicStream;
typedef BrnSound::Logic::HUDEffect::GameModeData GameModeData;
typedef CgsSound::Utils::Average<10, f32> SlipstreamAverage;

CGS_STATIC_ASSERT((sizeof(MusicStream) == 0x60), "native MusicStream extent");
CGS_STATIC_ASSERT((sizeof(CgsSound::Utils::Average<10, f32>) == 0x30),
                  "native slipstream Average extent");
CGS_STATIC_ASSERT((offsetof(SlipstreamAverage, maPoints) == 0x00),
                  "Average samples at +0x00");
CGS_STATIC_ASSERT((offsetof(SlipstreamAverage, muNextPoint) == 0x28),
                  "Average cursor at +0x28");
CGS_STATIC_ASSERT((offsetof(SlipstreamAverage, mfAverage) == 0x2C),
                  "Average mean at +0x2C");
CGS_STATIC_ASSERT((offsetof(HUDEffect, mMusicStream) == 0x3A0),
                  "HUDEffect MusicStream offset");
CGS_STATIC_ASSERT((offsetof(HUDEffect, mSlipstreamAverage) == 0x400),
                  "HUDEffect slipstream Average offset");
CGS_STATIC_ASSERT((offsetof(HUDEffect, mGameModeData) == 0x430),
                  "HUDEffect GameModeData offset");
CGS_STATIC_ASSERT((offsetof(HUDEffect, mSlipstreamAverage) ==
                   offsetof(HUDEffect, mMusicStream) + sizeof(MusicStream)),
                  "Average immediately follows MusicStream");
CGS_STATIC_ASSERT((offsetof(HUDEffect, mGameModeData) ==
                   offsetof(HUDEffect, mSlipstreamAverage) + sizeof(SlipstreamAverage)),
                  "GameModeData immediately follows Average");
CGS_STATIC_ASSERT((sizeof(GameModeData) == 0x70), "native GameModeData extent");
CGS_STATIC_ASSERT((sizeof(HUDEffect) == 0x4D8), "native HUDEffect allocation size");

