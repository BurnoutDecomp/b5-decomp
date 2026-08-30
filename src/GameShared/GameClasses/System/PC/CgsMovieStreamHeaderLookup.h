#ifndef CGS_SYSTEM_PC_MOVIESTREAMHEADERLOOKUP_H
#define CGS_SYSTEM_PC_MOVIESTREAMHEADERLOOKUP_H

#include "types.hpp"

// ============================================================================
//  CgsSystem::MovieStreamHeaderLookup -- the movie player's retained SNR lookup.
//
//  THE CONSOLE CHAIN (X360 ARTIST, asm-verified):
//    - BrnSound::Logic::Streaming::StreamingStateManager::Prepare @0x826EE680
//      loads "sound\streams\StreamHeaders.bundle" (+ language variants);
//    - RootSoundModule::RegistryLoad merges StreamsRegistry ContentSpecs into
//      the playback Registry (Module::AddRegistry @0x826C7BE8);
//    - a stream request resolves Registry::GetEntity<ContentSpec> by interned
//      NAME (CgsSound::Playback::Name::MakeHash), then ContentSpec::GetPathZone
//      (spec, 0) @0x826928C8 yields the gamedb URL (the spec path up to '|');
//    - ResourceRegistrar::GetResource @0x826B0B08 keys the StreamHeaders bundle
//      by CgsResource::ID::HashString(url) (= zlib crc32 of the LOWERCASED
//      string, table @dword_820F71F0) -> the GenericRwacWaveContent (SNR):
//      channels / sample rate / total samples / the PREFETCHED attack;
//    - path zone 1 (after '|') is the .SNS stream file under "SOUND\STREAMS\".
//
//  The reconstructed sound engine now resolves music, presentation and speech
//  through its live Registry/Content path. This helper is intentionally private
//  to the separate movie-audio leaf retained by Phase G4: the console movie
//  player owns that path outside the game sound engine.
// ============================================================================

namespace CgsSystem
{
    class MovieStreamHeaderLookup
    {
    public:
        // Resolve by the .SNS FILE name (zone 1, case-insensitive, no directory)
        // -- the movie-audio path derives the stream from the video name.
        static bool ResolveBySnsName(const char* lpacSnsFile,
                                     const u8** lppSnr, u32* lpuSnrLen);
    };
}

#endif // CGS_SYSTEM_PC_MOVIESTREAMHEADERLOOKUP_H
