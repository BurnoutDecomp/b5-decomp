#ifndef CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H
#define CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H

#include "types.hpp"

namespace rw { namespace audio { namespace core { class PlugIn; } } }

// ARTIST inlines the SubmixVoice base lifetime into MasterVoice and retains
// the compiler-generated SubmixVoice deleting destructor for its vtable.

#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"   // the REAL Voice (: public Object)

namespace CgsSound
{
namespace Playback
{

// CgsSubmixVoice.h:29 (DecFIGS). ARTIST interns this shared default send once
// during startup; the definition is adjacent to the root sound allocator globals.
extern const Name K_DefaultSendName;

// CgsSubmixVoice.h:44/59/78 (DWARF). SubmixVoice : public Voice, adding a submix
// PlugIn pointer.
struct SubmixVoice : public Voice
{
    SubmixVoice(size_t auClientSize, Factory& arFactory,
                const VoiceSpec& arVoiceSpec, u32 au32Ident)
        : Voice(auClientSize, arFactory, arVoiceSpec, au32Ident), mpSubmix(0) {}
    virtual ~SubmixVoice() {}

    rw::audio::core::PlugIn* GetSubmix() const { return mpSubmix; }
    rw::audio::core::PlugIn** GetSubmixAddress() { return &mpSubmix; }

protected:
    rw::audio::core::PlugIn* mpSubmix;   // CgsSubmixVoice.h:78
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H
