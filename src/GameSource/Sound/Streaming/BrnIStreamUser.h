#ifndef BRN_SOUND_LOGIC_STREAMING_BRN_ISTREAM_USER_H
#define BRN_SOUND_LOGIC_STREAMING_BRN_ISTREAM_USER_H

#include "types.hpp"
#include "GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h"

// =============================================================================
// BrnSound::Logic::Streaming::IStreamUser  (minimal interface home)
//   DWARF home: GameSource/Sound/Streaming/BrnStreamingEffect.h:41-58 (rendered as a
//   single-vptr interface with three virtuals GetCreateParams / UpdateVoiceParams /
//   StreamStopped).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. IStreamUser is the streaming-user
// interface sub-object that streaming-aware effect leaves (SpeechEffect,
// PresentationEffect) multiply-inherit -- the X360 ctors of those leaves install its
// vptr as the third base sub-object (@ +0x38). It has NO committed home elsewhere in
// the tree; this MINIMAL home materialises only the interface SHAPE (a polymorphic
// base with a virtual destructor) so those leaves can name it as a base BY NAME and so
// the compiler produces the +0x38 sub-object vptr the ctors install. The three
// interface virtuals (GetCreateParams / UpdateVoiceParams / StreamStopped) and their
// bodies are DEFERRED to the full IStreamUser recon slice -- NOT fabricated here.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Streaming
{

// Minimal polymorphic interface base. The virtual destructor makes it a polymorphic
// sub-object (its own vptr), which is all the deriving leaves' ctors/dtors need to
// install/settle the +0x38 interface vptr. The three DWARF interface virtuals are
// DEFERRED.
struct IStreamUser
{
    IStreamUser() {}
    virtual const CgsSound::Logic::VoiceWrapper::CreateParams& GetCreateParams() const = 0;
    virtual void UpdateVoiceParams(CgsSound::Logic::VoiceWrapper& arVoice,
                                   f32 afGain, f32 afElapsedTime) = 0;
    virtual void StreamStopped() = 0;
};

} // namespace Streaming
} // namespace Logic
} // namespace BrnSound

#endif // BRN_SOUND_LOGIC_STREAMING_BRN_ISTREAM_USER_H
