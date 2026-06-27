#include "SDKs/EATech/include/Nicotine/SnapshotMixer.hpp"
#include <cstddef> // offsetof

// Compile-gate the (ARTIST-derived) SnapshotMixer layout.
static void SnapshotMixer_embed_check()
{
    (void)sizeof(Nicotine::SnapshotMixer);
    static_assert(offsetof(Nicotine::SnapshotMixer, mpOwner) < offsetof(Nicotine::SnapshotMixer, mpSnapshotHdr), "o1");
    static_assert(offsetof(Nicotine::SnapshotMixer, meState) < offsetof(Nicotine::SnapshotMixer, mpChannels), "o2");
    static_assert(offsetof(Nicotine::SnapshotMixer, miNumChannels) < offsetof(Nicotine::SnapshotMixer, miNumSnapshots), "o3");
}
