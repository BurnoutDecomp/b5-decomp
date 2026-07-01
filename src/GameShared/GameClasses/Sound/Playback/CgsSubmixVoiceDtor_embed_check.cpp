// CgsSubmixVoiceDtor_embed_check.cpp
//
// Forces emission of the CgsSound::Playback::SubmixVoice deleting destructor (the
// X360 `vector deleting destructor' @ 0x826C7EF8) by taking a delete-expression on a
// SubmixVoice*. Compile-only gate: the Voice base ~Voice() is a DEFERRED keystone
// symbol resolved at the eventual link.

#include "GameShared/GameClasses/Sound/Playback/CgsSubmixVoice.h"

namespace
{
    void EmbedCheck(CgsSound::Playback::SubmixVoice* apVoice)
    {
        delete apVoice;
    }

    void (*gpfEmbedCheck)(CgsSound::Playback::SubmixVoice*) = &EmbedCheck;
}
