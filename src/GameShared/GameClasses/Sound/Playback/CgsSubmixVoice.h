#ifndef CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H
#define CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H

#include "types.hpp"

namespace rw { namespace audio { namespace core { class PlugIn; } } }

// ============================================================================
// CgsSubmixVoice.h  (MINIMAL home for the SubmixVoice destructor TU).
//
//   CgsSound::Playback::SubmixVoice::`vector deleting destructor'  @ 0x826C7EF8
//
// The X360 compiler-synthesised deleting destructor installs the SubmixVoice vtable,
// runs the Voice base dtor, then (on flag&1) frees. mpSubmix is a trivially-
// destructible raw PlugIn pointer, so the source ~SubmixVoice() body is empty.
//
// (2026-08-25, audio-faithfulness wave 4): the former TU-local minimal Object +
// Voice rivals are RETIRED -- the base chain comes from the REAL homes (the wave-3
// Object fold made CgsVoice.h's Content/Object graph the single definition), so
// SubmixVoice derives the real Voice below. Modelled BY NAME from DWARF
// CgsSubmixVoice.h:44/59/78 (host-width FLAG: pointer/refcount members widen; no
// absolute-offset static_assert).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"   // the REAL Voice (: public Object)

namespace CgsSound
{
namespace Playback
{

// CgsSubmixVoice.h:44/59/78 (DWARF). SubmixVoice : public Voice, adding a submix
// PlugIn pointer.
struct SubmixVoice : public Voice
{
    SubmixVoice(size_t auClientSize, Factory& arFactory,
                const VoiceSpec& arVoiceSpec, u32 au32Ident);
    virtual ~SubmixVoice();

    rw::audio::core::PlugIn* GetSubmix() const { return mpSubmix; }
    rw::audio::core::PlugIn** GetSubmixAddress() { return &mpSubmix; }

protected:
    rw::audio::core::PlugIn* mpSubmix;   // CgsSubmixVoice.h:78
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H
