#ifndef GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULEIO_H
#define GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULEIO_H

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsIOBuffer.h"             // CgsModule::IOBuffer (both bases)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // VariableEventQueue<4096,16> (the request queue)
#include "GameShared/GameClasses/Containers/CgsArray.h"            // Array<T,3> (the freed-buffer list)
#include "GameShared/GameClasses/Sound/IO/CgsMessage.h"            // CgsSound::Io::QueueElement

// =============================================================================
// CgsSoundPlaybackModuleIO.h -- the playback Module's per-frame IO buffer pair
// (canonical DWARF home CgsSoundPlaybackModuleIO.h; phase B4 of the faithful
// audio-engine bring-up).
//
//   Io::InputBuffer  (DWARF h:64)  : IOBuffer            -- the base status byte
//     only (the X360 IOBufferStack::CreateIOBuffer<InputBuffer> @0x826C5B68
//     carves exactly 1 byte and runs the base Construct).
//   Io::OutputBuffer (DWARF h:100) : IOBuffer + the freed-stream-buffer list +
//     the resource request queue (the X360 CreateIOBuffer<OutputBuffer>
//     @0x826C5C40 carves 4132 bytes: status byte [+0, padded], the
//     Array<QueueElement,3> [+4 data / +16 count -- the host Array<T,N>
//     data-then-count layout matches], the VariableEventQueue<4096,16> [+20]).
//
// The DWARF spells the queue typedef through a ResourceRequestQueue<N> template
// (h:50; the Module's own deferred queue is ResourceRequestQueue<1024>, h:51) --
// on the host the underlying CgsModule::VariableEventQueue<N,16> is used
// directly (the X360 asm dispatches those symbols; no distinct
// ResourceRequestQueue code exists in the image).
// =============================================================================

namespace CgsSound
{
namespace Playback
{
namespace Module
{
namespace Io
{

// DWARF CgsSoundPlaybackModuleIO.h:64. The playback module's input buffer --
// carries no own payload in this build.
struct InputBuffer : public CgsModule::IOBuffer
{
    void Construct();   // h:69  (base Construct only -- the @0x826C5B68 carve site)
    void Destruct();    // h:73  (attested empty -- the DWARF cpp:69 dump shows no calls)
    void Clear();       // h:77  (attested empty -- the DWARF cpp:87 dump shows no calls)
};

// DWARF CgsSoundPlaybackModuleIO.h:100. The playback module's output buffer:
// the per-frame resource requests (drained from the module's deferred queue +
// the stream-service posts) and the stream-buffer-freed voice-ident list.
struct OutputBuffer : public CgsModule::IOBuffer
{
    typedef Array<CgsSound::Io::QueueElement, 3>  FreedBuffersArray;    // h:52
    typedef CgsModule::VariableEventQueue<4096, 16> ResourceRequestQueue; // h:50 (ResourceRequestQueue<4096>)

    void Construct();   // h:104  (@0x826C5C40 carve site: base + queue Construct/Clear + array Construct/Clear)
    bool Prepare();     // h:108
    bool Release();     // h:112
    void Destruct();    // h:116  (attested empty -- the DWARF cpp:158 dump shows no calls)
    void Clear();       // h:120  (queue Clear + freed-array Clear -- the Module::Update @0x826E9700 inline)

    ResourceRequestQueue&       GetResourceRequestQueue()       { return mRequestQueue; }        // h:125
    const ResourceRequestQueue& GetResourceRequestQueue() const { return mRequestQueue; }        // h:129
    const FreedBuffersArray&    GetStreamBuffersFreed() const   { return mStreamBuffersFreed; }  // h:135
    FreedBuffersArray&          GetStreamBuffersFreed()         { return mStreamBuffersFreed; }  // h:141

private:
    FreedBuffersArray    mStreamBuffersFreed;  // h:147  (X360 +4 data / +16 count)
    ResourceRequestQueue mRequestQueue;        // h:149  (X360 +20)
};

} // namespace Io
} // namespace Module
} // namespace Playback
} // namespace CgsSound

#endif // GAMESHARED_GAMECLASSES_SOUND_PLAYBACK_MODULE_CGSSOUNDPLAYBACKMODULEIO_H
