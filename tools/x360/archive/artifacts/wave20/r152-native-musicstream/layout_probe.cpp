#include "GameSource/Sound/Global/BrnMusicEffect.h"
CGS_STATIC_ASSERT((sizeof(BrnSound::Logic::MusicStream) == 0x60), "MusicStream native size");
unsigned NativeMusicStreamSize() { return sizeof(BrnSound::Logic::MusicStream); }
