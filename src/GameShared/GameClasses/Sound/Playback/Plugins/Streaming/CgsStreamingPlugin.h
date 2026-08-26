#ifndef CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_CGSSTREAMINGPLUGIN_H
#define CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_CGSSTREAMINGPLUGIN_H

#include "types.hpp"
#include "GameShared/GameClasses/System/FileSystem/CgsReadStream.h"   // CgsFileSystem::ReadStream (the by-value stream handle)

// CgsSound::Playback::IStreamProvider -- the audio-streaming provider interface
// (canonical DWARF home CgsStreamingPlugin.h). The playback Module derives it as
// its FIRST interface base (console sub-object +0x228; the interface pointer the
// RWAC-stage publish `off_82FFBA0C = &module-interface` hands to the SndPlayer1
// side): the streaming plugin opens/closes its read streams through these two
// virtuals, which the Module services against its 3 owned stream buffers
// (DoOpenStream @0x826FA020 / DoCloseStream @0x826FA2B8).
//
// The ReadStream vocabulary is the resource layer's CgsFileSystem::ReadStream
// (the one-pointer by-value StreamDeviceDiskRead handle; the DWARF spells it via
// `using namespace CgsResource::ResourceIO` in the module TU).

namespace rw { namespace audio { namespace core { class PlugIn; } } }

namespace CgsSound
{
namespace Playback
{

struct IStreamProvider
{
    // DWARF CgsStreamingPlugin.h:30 -- the open request record a streaming
    // plugin fills in: the file to stream, the out-slot for the carve buffer the
    // module hands back, the requesting engine plug-in (the content lookup key),
    // and the two read priorities.
    struct StreamSpec
    {
        const char*                    mpFilename;        // :38
        void**                         mppvBuffer;        // :39
        const rw::audio::core::PlugIn* mpPlugin;          // :40
        s32                            mi32PriorityLow;   // :41
        s32                            mi32PriorityHigh;  // :42

        // :31 -- declared-only (its own ledger surface; call sites construct in
        // place on the SndPlayer1 side).
        StreamSpec();
    };

    // CgsStreamingPlugin.h:47 (vtable slot 0). Open a read stream per the spec;
    // returns the stream handle (the Module's override returns a pointer INTO
    // its stream-buffer record table), or null when no buffer is free.
    virtual CgsFileSystem::ReadStream* DoOpenStream(StreamSpec& lrSpec) = 0;

    // CgsStreamingPlugin.h:51 (vtable slot 1, the console `vtbl+4` dispatch).
    // Close a stream previously returned by DoOpenStream.
    virtual void DoCloseStream(const CgsFileSystem::ReadStream* lpReadStream) = 0;
};

} // namespace Playback
} // namespace CgsSound

#endif // CGS_SOUND_PLAYBACK_PLUGINS_STREAMING_CGSSTREAMINGPLUGIN_H
