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
// This MINIMAL home models the base chain (Object -> Voice -> SubmixVoice) with only
// the members/dtors the teardown needs: the full Voice/Object hierarchy is DEFERRED
// to the keystone TU. Modelled BY NAME from DWARF CgsSubmixVoice.h:44/59/78 (host-
// width FLAG: pointer/refcount members widen; no absolute-offset static_assert).
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// Minimal per-TU Object base (vptr + refcount + virtual dtor).
class Object
{
public:
    Object() : mu32RefCount(0) {}
    virtual ~Object() {}
protected:
    u32 mu32RefCount;
};

// Minimal per-TU Voice base. ~Voice is DECLARED (the SubmixVoice dtor chains through
// it); its body is DEFERRED to the Voice keystone TU.
class Voice : public Object
{
public:
    virtual ~Voice();
};

// CgsSubmixVoice.h:44/59/78 (DWARF). SubmixVoice : public Voice, adding a submix
// PlugIn pointer.
struct SubmixVoice : public Voice
{
    virtual ~SubmixVoice();

private:
    rw::audio::core::PlugIn* mpSubmix;   // CgsSubmixVoice.h:78
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_CGSSUBMIXVOICE_H
